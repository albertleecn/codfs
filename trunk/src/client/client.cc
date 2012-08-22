#include <iostream>
#include <cstdio>
#include <thread>
#include <iomanip>
#include <chrono>
#include "client.hh"
#include "client_storagemodule.hh"
#include "../config/config.hh"
#include "../common/debug.hh"
#include "../common/objectdata.hh"
#include "../common/segmentdata.hh"

using namespace std;

//#define PARALLEL_TRANSFER

mutex pendingObjectChunkMutex;

extern Client* client;
extern ConfigLayer* configLayer;

Client::Client() {
	_clientCommunicator = new ClientCommunicator();
	_storageModule = new ClientStorageModule();

	_clientId = configLayer->getConfigInt("Clientid");

}

/**
 * @brief	Get the Client Communicator
 *
 * @return	Pointer to the Client Communicator Module
 */
ClientCommunicator* Client::getCommunicator() {
	return _clientCommunicator;
}

ClientStorageModule* Client::getStorageModule() {
	return _storageModule;
}

#ifdef PARALLEL_TRANSFER

void startUploadThread(uint32_t clientId, uint32_t sockfd,
		struct ObjectData objectData, CodingScheme codingScheme,
		string codingSetting) {
	client->getCommunicator()->sendObject(clientId, sockfd, objectData,
			codingScheme, codingSetting);
	MemoryPool::getInstance().poolFree(objectData.buf);
}

void startDownloadThread(uint32_t clientId, uint32_t sockfd,
		uint64_t objectId, uint64_t offset, FILE* filePtr, string dstPath) {
	client->getCommunicator()->getObjectAndWriteFile(clientId, sockfd, objectId, offset, filePtr, dstPath);
	debug ("Object ID = %" PRIu64 " finished download\n", objectId);
}

#endif

uint32_t Client::uploadFileRequest(string path, CodingScheme codingScheme,
		string codingSetting) {

	// start timer
	typedef chrono::high_resolution_clock Clock;
	typedef chrono::milliseconds milliseconds;
	Clock::time_point t0 = Clock::now();

	const uint32_t objectCount = _storageModule->getObjectCount(path);
	const uint64_t fileSize = _storageModule->getFilesize(path);

	debug("Object Count of %s: %" PRIu32 "\n", path.c_str(), objectCount);

	struct FileMetaData fileMetaData = _clientCommunicator->uploadFile(
			_clientId, path, fileSize, objectCount, codingScheme,
			codingSetting);

	debug("File ID %" PRIu32 "\n", fileMetaData._id);

	for (uint32_t i = 0; i < fileMetaData._primaryList.size(); ++i) {
		debug("%" PRIu64 "[%" PRIu32 "]\n",
				fileMetaData._objectList[i], fileMetaData._primaryList[i]);
	}

#ifdef PARALLEL_TRANSFER
	thread uploadThread[objectCount];
#endif

	for (uint32_t i = 0; i < objectCount; ++i) {
		struct ObjectData objectData = _storageModule->readObjectFromFile(path,
				i);
		uint32_t primary = fileMetaData._primaryList[i];
		debug("Send to Primary [%" PRIu32 "]\n", primary);
		uint32_t dstOsdSockfd = _clientCommunicator->getSockfdFromId(primary);
		objectData.info.objectId = fileMetaData._objectList[i];

#ifdef PARALLEL_TRANSFER
		uploadThread[i] = thread(startUploadThread, _clientId, dstOsdSockfd,
				objectData, codingScheme, codingSetting);
#else
		_clientCommunicator->sendObject(_clientId, dstOsdSockfd, objectData, codingScheme, codingSetting);
		MemoryPool::getInstance().poolFree(objectData.buf);
#endif
	}

#ifdef PARALLEL_TRANSFER
	// wait for every thread to finish
	for (uint32_t i = 0; i < objectCount; i++) {
		uploadThread[i].join();
	}
#endif

	debug("Upload %s Done [%" PRIu32 "]\n", path.c_str(), fileMetaData._id);

	// Time and Rate calculation (in seconds)
	Clock::time_point t1 = Clock::now();
	milliseconds ms = chrono::duration_cast < milliseconds > (t1 - t0);
	double duration = ms.count() / 1024.0;
	double fileSizeMb = fileSize / 1048576.0;
	double rate = fileSizeMb / duration;

	cout << fixed;
	cout << setprecision(2);
	cout << fileSizeMb << " MB transferred in " << duration << " secs, Rate = "
			<< rate << " MB/s" << endl;

	return fileMetaData._id;
}


void Client::downloadFileRequest(uint32_t fileId, string dstPath) {

	const uint64_t objectSize = _storageModule->getObjectSize();

	// start timer
	typedef chrono::high_resolution_clock Clock;
	typedef chrono::milliseconds milliseconds;
	Clock::time_point t0 = Clock::now();

	// 1. Get file infomation from MDS
	struct FileMetaData fileMetaData = _clientCommunicator->downloadFile(
			_clientId, fileId);
	debug("File ID %" PRIu32 ", Size = %" PRIu64 "\n",
			fileMetaData._id, fileMetaData._size);

	// open file
	FILE* filePtr = _storageModule->createAndOpenFile(dstPath);

	// 2. Download file from OSD

#ifdef PARALLEL_TRANSFER
	const uint32_t objectCount = fileMetaData._objectList.size();
	thread downloadThread[objectCount];
#endif

	uint32_t i = 0;
	for (uint64_t objectId : fileMetaData._objectList) {
		uint32_t dstComponentId = fileMetaData._primaryList[i];
		uint32_t dstSockfd = _clientCommunicator->getSockfdFromId(
				dstComponentId);
		const uint64_t offset = objectSize * i;
#ifdef PARALLEL_TRANSFER
		downloadThread[i] = thread(startDownloadThread, _clientId, dstSockfd,
				objectId, offset, filePtr, dstPath);
#else
		_clientCommunicator->getObjectAndWriteFile(_clientId, dstSockfd, objectId, offset,filePtr, dstPath);
#endif

		i++;
	}

#ifdef PARALLEL_TRANSFER
	for (uint32_t i = 0; i < objectCount; i++) {
		downloadThread[i].join();
	}
#endif

	// write to file

//	i = 0;
//	for (uint64_t objectId : fileMetaData._objectList) {
//		struct ObjectCache objectCache = _storageModule->getObjectCache(
//				objectId);
//		_storageModule->writeFile(filePtr, dstPath, objectCache.buf, offset,
//				objectCache.length);
//		debug(
//				"Write Object ID: %" PRIu64 " Offset: %" PRIu64 " Length: %" PRIu64 " to %s\n",
//				objectId, offset, objectCache.length, dstPath.c_str());
//		_storageModule->closeObject(objectId);
//		i++;
//	}

	_storageModule->closeFile(filePtr);

	// Time and Rate calculation (in seconds)
	Clock::time_point t1 = Clock::now();
	milliseconds ms = chrono::duration_cast < milliseconds > (t1 - t0);
	double duration = ms.count() / 1024.0;
	double fileSizeMb = fileMetaData._size / 1048576.0;
	double rate = fileSizeMb / duration;

	cout << fixed;
	cout << setprecision(2);
	cout << fileSizeMb << " MB transferred in " << duration << " secs, Rate = "
			<< rate << " MB/s" << endl;
}

void Client::putObjectInitProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t objectId, uint32_t length, uint32_t chunkCount) {

	// initialize chunkCount value
	{
		lock_guard<mutex> lk(pendingObjectChunkMutex);
		_pendingObjectChunk[objectId] = chunkCount;
		debug("Init Chunkcount = %" PRIu32 "\n", chunkCount);
	}

	// create object and cache
	_storageModule->createObjectCache(objectId, length);
	_clientCommunicator->replyPutObjectInit(requestId, sockfd, objectId);

}

void Client::putObjectEndProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t objectId) {

	// TODO: check integrity of object received
	bool chunkRemaining = false;

	while (1) {

		{
			lock_guard<mutex> lk(pendingObjectChunkMutex);
			chunkRemaining = (bool) _pendingObjectChunk[objectId];
		}

		if (!chunkRemaining) {
			// if all chunks have arrived, send ack
			_pendingObjectChunk.erase(objectId);
			//_storageModule->closeObject(objectId); // cannot close here
			_clientCommunicator->replyPutObjectEnd(requestId, sockfd, objectId);
			break;
		} else {
			usleep(100000); // sleep 0.1s
		}

	}
}

uint32_t Client::ObjectDataProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t objectId, uint64_t offset, uint32_t length, char* buf) {

	uint32_t byteWritten;
	byteWritten = _storageModule->writeObjectCache(objectId, buf, offset,
			length);
	{
		lock_guard<mutex> lk(pendingObjectChunkMutex);
		// update pendingObjectChunk value
		_pendingObjectChunk[objectId]--;
	}
	return byteWritten;
}

void Client::setPendingChunkCount(uint64_t objectId, int chunkCount) {
	lock_guard<mutex> lk(pendingObjectChunkMutex);
	_pendingObjectChunk[objectId] = chunkCount;
}

int Client::getPendingChunkCount(uint64_t objectId) {
	lock_guard<mutex> lk(pendingObjectChunkMutex);
	return _pendingObjectChunk[objectId];
}

uint32_t Client::getClientId() {
	return _clientId;
}
