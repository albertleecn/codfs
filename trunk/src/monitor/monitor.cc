#include <cstdio>
#include "monitor.hh"
#include "../config/config.hh"
#include "../common/garbagecollector.hh"

ConfigLayer* configLayer;

Monitor* monitor;

mutex osdStatMapMutex;

/*  Monitor default constructor
 */
Monitor::Monitor() {
	_osdStatMap = {};

	configLayer = new ConfigLayer("monitorconfig.xml");
	_selectionModule = new SelectionModule(_osdStatMap);
	_recoveryModule = new RecoveryModule(_osdStatMap);
	_statModule = new StatModule(_osdStatMap);
	_monitorCommunicator = new MonitorCommunicator();
	_monitorId = configLayer->getConfigInt("MonitorId");
}

/*	Monitor default desctructor
 */
Monitor::~Monitor() {
	delete _selectionModule;
	delete _recoveryModule;
	delete _statModule;
	delete _monitorCommunicator;
}

MonitorCommunicator* Monitor::getCommunicator() {
	return _monitorCommunicator;
}

StatModule* Monitor::getStatModule() {
	return _statModule;
}

RecoveryModule* Monitor::getRecoveryModule() {
	return _recoveryModule;
}

uint32_t Monitor::getMonitorId() {
	return _monitorId;
}

void Monitor::OsdStartupProcessor(uint32_t requestId, uint32_t sockfd,
	uint32_t osdId, uint32_t capacity, uint32_t loading, uint32_t ip, 
	uint16_t port ) {
	_statModule->setStatById (osdId, sockfd, capacity, loading, ONLINE, ip, port);
}

void Monitor::OsdStatUpdateReplyProcessor(uint32_t requestId, uint32_t sockfd,
	uint32_t osdId, uint32_t capacity, uint32_t loading) {
	_statModule->setStatById (osdId, sockfd, capacity, loading, ONLINE);
}

void Monitor::OsdShutdownProcessor(uint32_t requestId, uint32_t sockfd, 
	uint32_t osdId) {
	_statModule->removeStatById (osdId);
}

void Monitor::getPrimaryListProcessor (uint32_t requestId, uint32_t sockfd, uint32_t numOfObjs){
	vector<uint32_t> primaryList;
	primaryList = _selectionModule->ChoosePrimary(numOfObjs);
	_monitorCommunicator->replyPrimaryList(requestId, sockfd, primaryList);
	return;
}


void startGarbageCollectionThread() {
	GarbageCollector::getInstance().start();
}

void startSendThread(Communicator* communicator) {
	communicator->sendMessage();
}

void startReceiveThread(Communicator* communicator) {
	communicator->waitForMessage();
}

void startUpdateThread(Communicator* communicator, StatModule* statmodule) {
	statmodule->updateOsdStatMap(communicator);
}

void startRecoveryThread(Communicator* communicator, RecoveryModule* recoverymodule) {
	recoverymodule->failureDetection(100,10);
}

int main (void) {
	
	printf ("MONITOR\n");

	monitor = new Monitor();
	MonitorCommunicator* communicator = monitor->getCommunicator();
	StatModule* statmodule = monitor->getStatModule();
	RecoveryModule* recoverymodule = monitor->getRecoveryModule();

	// set up communicator
	communicator->setId(monitor->getMonitorId());
	communicator->setComponentType(MONITOR);

	communicator->createServerSocket();

	// 1. Garbage Collection Thread
	thread garbageCollectionThread(startGarbageCollectionThread);

	// 2. Receive Thread
	thread receiveThread(startReceiveThread, communicator);

	// 3. Send Thread
	thread sendThread(startSendThread, communicator);

	// 4. Update Thread
	thread updateThread(startUpdateThread, communicator, statmodule);
	
	// 5. Recovery Thread
	thread recoveryThread(startRecoveryThread, communicator, recoverymodule);

	// threads join
	garbageCollectionThread.join();
	receiveThread.join();
	sendThread.join();
	updateThread.join();
	recoveryThread.join();
	//clean up
	delete configLayer;
	delete monitor;

	return 0;
}
