#include <signal.h>

#include <iostream>
#include <cstdio>
#include <thread>
#include <ctime>

#include "client.hh"
#include "client_storagemodule.hh"
#include "../common/garbagecollector.hh"
#include "../common/debug.hh"
#include "../config/config.hh"
#include "../common/objectdata.hh"
#include "../common/segmentdata.hh"

#include <sys/stat.h>

using namespace std;

void sighandler(int signum) {
	if (signum == SIGINT)
		exit(42);
}

/// Client Object
Client* client;

/// Config Layer
ConfigLayer* configLayer;

Client::Client() {
	_clientCommunicator = new ClientCommunicator();
	_storageModule = new ClientStorageModule();

	// TODO: HARDCODE CLIENT ID
	_clientId = 1;

}

/**
 * @brief	Get the Client Communicator
 *
 * @return	Pointer to the Client Communicator Module
 */
ClientCommunicator* Client::getCommunicator() {
	return _clientCommunicator;
}

uint32_t Client::uploadFileRequest(string path) {
	uint32_t objectCount = _storageModule->getObjectCount(path);

	struct stat tempFileStat;
	stat(path.c_str(), &tempFileStat);

	uint64_t fileSize = tempFileStat.st_size;

	debug("Object Count of %s: %" PRIu32 "\n", path.c_str(), objectCount);

	struct FileMetaData fileMetaData = _clientCommunicator->uploadFile(
			_clientId, path, fileSize, objectCount);

	debug("File ID %" PRIu32 "\n", fileMetaData._id);

	debug("%s\n", "====================");
	for (uint32_t i = 0; i < fileMetaData._primaryList.size(); ++i) {
		debug("%" PRIu64 "[%" PRIu32 "]\n",
				fileMetaData._objectList[i], fileMetaData._primaryList[i]);
	}

	return fileMetaData._id;
}

/**
 * 1. Divide the file into fixed size objects
 * 2. For each object, contact MDS to obtain objectId and dstOsdID
 * 3. Call uploadObjectRequest()
 */

uint32_t Client::sendFileRequest(string filepath) {

	// start timer
	const clock_t start = clock();

	const uint32_t objectCount = _storageModule->getObjectCount(filepath);

	// TODO: obtain a list of OSD for upload from MDS

	debug("Object Count of %s is %" PRIu32 "\n", filepath.c_str(), objectCount);

	for (uint32_t i = 0; i < objectCount; ++i) {
		struct ObjectData objectData = _storageModule->readObjectFromFile(
				filepath, i);

		// TODO: HARDCODE FOR NOW!
		uint32_t dstOsdSockfd = _clientCommunicator->getOsdSockfd();
		objectData.info.objectId = i;

		_clientCommunicator->putObject(_clientId, dstOsdSockfd, objectData);
	}

	// end timer
	const clock_t end = clock();
	const double duration = (end - start) / (double) CLOCKS_PER_SEC;
	const uint64_t filesize = _storageModule->getFilesize(filepath) / 1024 / 1024;
	const double rate = filesize / duration;

	cout << "start = " << start << " end = " << end << "CLOCKS_PER_SEC = " << CLOCKS_PER_SEC << endl;

	cout << filesize << " MB transferred in " << duration << " secs, Rate = "
			<< rate << " MB/s" << endl;

	return 0;
}

/*
 uint32_t Client::downloadFileRequest(string dstPath) {
 return 0;
 }
 */

void Client::downloadFileRequest(uint32_t fileId, string dstPath) {
	// to be implemented
}

void startGarbageCollectionThread() {
	GarbageCollector::getInstance().start();
}

void startSendThread() {
	client->getCommunicator()->sendMessage();
}

void startReceiveThread(Communicator* communicator) {
	// wait for message
	communicator->waitForMessage();

}

int main(void) {

	signal(SIGINT, sighandler);

	configLayer = new ConfigLayer("clientconfig.xml");
	client = new Client();
	ClientCommunicator* communicator = client->getCommunicator();

	// start server
	communicator->createServerSocket();

	/*
	 const int segmentNumber = configLayer->getConfigInt("Coding>SegmentNumber");
	 debug("Segment Number = %d\n", segmentNumber);
	 */

	// connect to MDS
	//	communicator->connectToMds();
	// connect to OSD
	communicator->connectToOsd();

	// 1. Garbage Collection Thread
	thread garbageCollectionThread(startGarbageCollectionThread);

	// 2. Receive Thread
	thread receiveThread(startReceiveThread, communicator);

	// 3. Send Thread
	thread sendThread(startSendThread);

	////////////////////// TEST FUNCTIONS ////////////////////////////

	// TEST PUT OBJECT
	client->sendFileRequest("./testfile");
	//client->uploadFileRequest("./testfile");

	/*

	 // TEST LIST FOLDER
	 vector<FileMetaData> folderData;
	 folderData = communicator->listFolderData(1, ".");

	 // TODO: when to delete listFolderDataRequest and listFolderDataReply?

	 vector<FileMetaData>::iterator it;
	 for (it = folderData.begin(); it < folderData.end(); ++it) {
	 debug("name: %s size: %d\n", ((*it)._path).c_str(), (int)(*it)._size);
	 }
	 */

	garbageCollectionThread.join();
	receiveThread.join();
	sendThread.join();

	return 0;
}

