/**
 * communicator.cc
 */

#include <iostream>
#include <thread>
#include <sys/types.h>		// required by select()
#include <unistd.h>		// required by select()
#include <sys/select.h>	// required by select()
#include <sys/ioctl.h>
#include "connection.hh"
#include "communicator.hh"
#include "component.hh"
#include "socketexception.hh"
#include "../config/config.hh"
#include "../common/garbagecollector.hh"
#include "../common/enums.hh"
#include "../common/debug.hh"
#include "../protocol/message.pb.h"
#include "../protocol/messagefactory.hh"

using namespace std;

// global variable defined in each component
extern ConfigLayer* configLayer;

// mutex
mutex outMessageQueueMutex;
mutex waitReplyMessageMapMutex;
mutex connectionMapMutex;

Communicator::Communicator() {

	// verify protobuf version
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// initialize variables
	_requestId.store(0);
	_connectionMap = {};
	_componentIdMap = {};
	_outMessageQueue = {};
	_waitReplyMessageMap = {};
	_maxFd = 0;

	// select timeout
	_timeoutSec = configLayer->getConfigInt("Communication>SelectTimeout>sec");
	_timeoutUsec = configLayer->getConfigInt(
			"Communication>SelectTimeout>usec");

	// chunk size
	_chunkSize = configLayer->getConfigInt("Communication>ChunkSize");

	_serverPort = configLayer->getConfigInt("Communication>ServerPort");

	debug("%s\n", "Communicator constructed");
}

Communicator::~Communicator() {
	debug("%s\n", "Communicator destructed");
}

void Communicator::createServerSocket() {

	// create a socket for accepting new peers
	if (!_serverSocket.create()) {
		throw SocketException("Could not create server socket.");
	}

	if (!_serverSocket.bind(_serverPort)) {
		throw SocketException("Could not bind to port.");
	}

	if (!_serverSocket.listen()) {
		throw SocketException("Could not listen to socket.");
	}

	debug("Server Port = %" PRIu16 " sockfd = %" PRIu32 "\n",
			_serverPort, _serverSocket.getSockfd());
}

/*
 * Runs in a while (1) loop
 * 1. Add serverSockfd to fd_set
 * 2. Add sockfd for all connections to fd_set
 * 3. Use select to monitor all fds
 * 4. If select returns:
 * 		serverSockfd is set : accept connection and save in map
 * 		other sockfd is set : receive a header
 * 		timeout: check if all connections are still alive
 */

void Communicator::waitForMessage() {

	char* buf; // message receive buffer
	map<uint32_t, Connection*>::iterator p; // connectionMap iterator

	int result; // return value for select
	fd_set sockfdSet; // fd_set for select
	struct timeval tv; // timeout for select

	const uint32_t serverSockfd = _serverSocket.getSockfd();

	// adjust _maxFd
	if (serverSockfd > _maxFd) {
		_maxFd = serverSockfd;
	}

	while (1) {

		// reset timeout
		tv.tv_sec = _timeoutSec;
		tv.tv_usec = _timeoutUsec;

		FD_ZERO(&sockfdSet);

		// add listen socket
		FD_SET(serverSockfd, &sockfdSet);

		// add all socket descriptors into sockfdSet
		{
			lock_guard<mutex> lk(connectionMapMutex);
			for (p = _connectionMap.begin(); p != _connectionMap.end(); p++) {
				FD_SET(p->second->getSockfd(), &sockfdSet);
			}
		}

		// invoke select
		result = select(_maxFd + 100, &sockfdSet, NULL, NULL, &tv);

		if (result < 0) {
			cerr << "select error" << endl;
			return;
		} else if (result == 0) {
			debug("%s\n", "select timeout");
		} else {
			debug("%s\n", "select returns");
		}

		// if there is a new connection
		if (FD_ISSET(_serverSocket.getSockfd(), &sockfdSet)) {

			// accept connection
			Connection* conn = new Connection();
			_serverSocket.accept(conn->getSocket());

			// add connection to _connectionMap
			{
				lock_guard<mutex> lk(connectionMapMutex);
				_connectionMap[conn->getSockfd()] = conn;
			}

			debug("New connection sockfd = %" PRIu32 "\n", conn->getSockfd());

			// adjust _maxFd
			if (conn->getSockfd() > _maxFd) {
				_maxFd = conn->getSockfd();
			}
		}

		// if there is data in existing connections
		{ // start critical session
			lock_guard<mutex> lk(connectionMapMutex);
			for (p = _connectionMap.begin(); p != _connectionMap.end(); p++) {
				// if socket has data available
				if (FD_ISSET(p->second->getSockfd(), &sockfdSet)) {

					// check if connection is lost
					int nbytes = 0;
					ioctl(p->second->getSockfd(), FIONREAD, &nbytes);
					if (nbytes == 0) {
						// disconnect and remove from _connectionMap
						delete p->second;
						_connectionMap.erase(p->first);
						continue;
					}

					// receive message into buffer, memory allocated in recvMessage
					buf = p->second->recvMessage();
					dispatch(buf, p->first);
				}
			}
		} // end critical session

	} // end while (1)
}

/**
 * 1. Set a requestId for a message if it is 0
 * 2. Push the message to _outMessageQueue
 * 3. If need to wait for reply, add the message to waitReplyMessageMap
 */

void Communicator::addMessage(Message* message, bool expectReply) {

	// if requestID == 0, generate a new one
	if (message->getMsgHeader().requestId == 0) {
		message->setRequestId(generateRequestId());
	}

	// add message to waitReplyMessageMap if needed
	if (expectReply) {
		message->setExpectReply(true);
		{
			const uint32_t requestId = message->getMsgHeader().requestId;
			lock_guard<mutex> lk(waitReplyMessageMapMutex);
			_waitReplyMessageMap[requestId] = message;
			debug("Message (ID: %" PRIu32 ") added to waitReplyMessageMap\n",
					requestId);
		}
	}

	// add message to outMessageQueue
	{
		lock_guard<mutex> lk(outMessageQueueMutex);
		_outMessageQueue.push_back(message); // must be at the end of function
	}

}

Message* Communicator::popWaitReplyMessage(uint32_t requestId) {

	lock_guard<mutex> lk(waitReplyMessageMapMutex);

	// check if message is in map
	if (_waitReplyMessageMap.count(requestId)) {

		// find message
		Message* message = _waitReplyMessageMap[requestId];

		// remove message from map
		_waitReplyMessageMap.erase(requestId);

		return message;
	}
	return NULL;
}

void Communicator::sendMessage() {

	// get config: polling interval
	const uint32_t pollingInterval = configLayer->getConfigInt(
			"Communication>SendPollingInterval");

	while (1) {

		// send all message in the outMessageQueue
		while (!isOutMessageQueueEmpty()) {

			// get and remove from queue
			Message* message;
			{
				lock_guard<mutex> lk(outMessageQueueMutex);
				message = _outMessageQueue.front();
				_outMessageQueue.pop_front();
			}

			const uint32_t sockfd = message->getSockfd();

			// handle disconnected component
			if (sockfd == (uint32_t) -1) {
				debug("Message (ID: %" PRIu32 ") disconnected, ignore\n",
						message->getMsgHeader().requestId);
				continue;
			}

			{
				lock_guard<mutex> lk(connectionMapMutex);
				if (!(_connectionMap.count(sockfd))) {
					debug("%s\n", "Connection not found!");
					exit(-1);
				}
				_connectionMap[sockfd]->sendMessage(message);
			}

			// debug

			message->printHeader();
			debug(
					"Message (ID: %" PRIu32 ") FD = %" PRIu32 " removed from queue\n",
					message->getMsgHeader().requestId, sockfd);

			// delete message if it is not waiting for reply
			if (!message->isExpectReply()) {
				debug("Deleting Message (Type = %d ID: %" PRIu32 ")\n",
						(int)message->getMsgHeader().protocolMsgType, message->getMsgHeader().requestId);
				delete message;
			}
		}

	}

	// in terms of 10^-6 seconds
	usleep(pollingInterval);
}

/**
 * 1. Connect to target component
 * 2. Add the connection to the corresponding map
 */

void Communicator::connectAndAdd(string ip, uint16_t port,
		ComponentType connectionType) {

	// Construct a Connection object and connect to component
	Connection* conn = new Connection();
	const uint32_t sockfd = conn->doConnect(ip, port, connectionType);

	// Save the connection into corresponding list
	{
		lock_guard<mutex> lk(connectionMapMutex);
		_connectionMap[sockfd] = conn;
	}

	// adjust _maxFd
	if (sockfd > _maxFd)
		_maxFd = sockfd;
}

/**
 * 1. Disconnect the connection
 * 2. Remove the connection from map
 * 3. Run the connection destructor
 */

void Communicator::disconnectAndRemove(uint32_t sockfd) {
	lock_guard<mutex> lk(connectionMapMutex);

	if (_connectionMap.count(sockfd)) {
		Connection* conn = _connectionMap[sockfd];
		delete conn;
		_connectionMap.erase(sockfd);
	} else {
		cerr << "Connection not found, cannot remove connection" << endl;
	}

}

uint32_t Communicator::getMdsSockfd() {
	// TODO: assume return first MDS
	map<uint32_t, Connection*>::iterator p;

	lock_guard<mutex> lk(connectionMapMutex);

	for (p = _connectionMap.begin(); p != _connectionMap.end(); p++) {
		if (p->second->getConnectionType() == MDS) {
			return p->second->getSockfd();
		}
	}

	return -1;
}

uint32_t Communicator::getMonitorSockfd() {
	// TODO: assume return first Monitor
	map<uint32_t, Connection*>::iterator p;

	lock_guard<mutex> lk(connectionMapMutex);

	for (p = _connectionMap.begin(); p != _connectionMap.end(); p++) {
		if (p->second->getConnectionType() == MONITOR) {
			return p->second->getSockfd();
		}
	}

	return -1;
}

uint32_t Communicator::getOsdSockfd() {
	// TODO: assume return first Osd
	map<uint32_t, Connection*>::iterator p;

	lock_guard<mutex> lk(connectionMapMutex);

	for (p = _connectionMap.begin(); p != _connectionMap.end(); p++) {
		if (p->second->getConnectionType() == OSD) {
			return p->second->getSockfd();
		}
	}

	return -1;
}

// static function
void Communicator::handleThread(Message* message) {
	message->handle();
}

// static function
void Communicator::sendThread(Communicator* communicator) {
	communicator->sendMessage();
}

/**
 * 1. Get the MsgHeader from the receive buffer
 * 2. Get the MsgType from the MsgHeader
 * 3. Use the MessageFactory to obtain a new Message object
 * 4. Fill in the socket descriptor into the Message
 * 5. message->parse() and fill in payload pointer
 * 6. start new thread for message->handle()
 */

void Communicator::dispatch(char* buf, uint32_t sockfd) {
	struct MsgHeader msgHeader;
	memcpy(&msgHeader, buf, sizeof(struct MsgHeader));

	const MsgType msgType = msgHeader.protocolMsgType;

	// delete after message is handled
	Message* message = MessageFactory::createMessage(this, msgType);

	message->setSockfd(sockfd);
	message->setRecvBuf(buf);
	message->parse(buf);

	// set payload pointer
	message->setPayload(
			buf + sizeof(struct MsgHeader) + msgHeader.protocolMsgSize);

	// debug
	message->printHeader();
	message->printProtocol();
	//message->printPayloadHex();

	thread t(handleThread, message);
	t.detach();
}

uint32_t Communicator::generateRequestId() {

	// increment _requestId

	_requestId.store(_requestId.load() + 1);
	return _requestId.load();

}

bool Communicator::isOutMessageQueueEmpty() {
	lock_guard<mutex> lk(outMessageQueueMutex);
	return _outMessageQueue.empty();
}

void Communicator::waitAndDelete(Message* message) {
	GarbageCollector::getInstance().addToDeleteList(message);
}

vector<struct Component> parseConfigFile(string componentType) {
	vector<struct Component> componentList;

	// get count
	const string componentCountQuery = "Components>" + componentType + ">count";
	const uint32_t componentCount = configLayer->getConfigInt(
			componentCountQuery.c_str());

	for (uint32_t i = 0; i < componentCount; i++) {
		const string idQuery = "Components>" + componentType + ">"
				+ componentType + to_string(i) + ">id";
		const string ipQuery = "Components>" + componentType + ">"
				+ componentType + to_string(i) + ">ip";
		const string portQuery = "Components>" + componentType + ">"
				+ componentType + to_string(i) + ">port";

		const uint32_t id = configLayer->getConfigInt(idQuery.c_str());
		const string ip = configLayer->getConfigString(ipQuery.c_str());
		const uint32_t port = configLayer->getConfigInt(portQuery.c_str());

		struct Component component;
		component.id = id;
		component.ip = ip;
		component.port = (uint16_t) port;

		componentList.push_back(component);
	}

	return componentList;
}

void Communicator::connectAllComponents() {

	// parse config file
	vector<Component> mdsList = parseConfigFile("MDS");
	vector<Component> osdList = parseConfigFile("OSD");
	vector<Component> monitorList = parseConfigFile("MONITOR");

	// debug: print all components
	cout << "========== MDS LIST ==========" << endl;
	for (uint32_t i = 0; i < mdsList.size(); i++) {
		cout << i << ": ID: " << mdsList[i].id << " IP: " << mdsList[i].ip
				<< ":" << mdsList[i].port << endl;
	}

	cout << "========== OSD LIST ==========" << endl;
	for (uint32_t i = 0; i < osdList.size(); i++) {
		cout << i << ": ID: " << osdList[i].id << " IP: " << osdList[i].ip
				<< ":" << osdList[i].port << endl;
	}

	cout << "========== MONITOR LIST ==========" << endl;
	for (uint32_t i = 0; i < monitorList.size(); i++) {
		cout << i << ": ID: " << monitorList[i].id << " IP: "
				<< monitorList[i].ip << ":" << monitorList[i].port << endl;
	}

	// connect to MDS

	// connect to OSD

	// connect to MONITOR

	// if not connected, make connection and save to map

}
