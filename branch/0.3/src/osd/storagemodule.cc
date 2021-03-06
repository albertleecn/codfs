/*
 * storagemodule.cc
 */

#include <boost/lexical_cast.hpp>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <sys/file.h>
#include <fcntl.h> /* Definition of AT_* constants */
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "storagemodule.hh"
#include "../common/debug.hh"
#include "../common/define.hh"
#include "../common/convertor.hh"

// global variable defined in each component
extern ConfigLayer* configLayer;

mutex fileMutex;
mutex openedFileMutex;
mutex transferCacheMutex;
mutex diskCacheMutex;
mutex diskSpaceMutex;

StorageModule::StorageModule() {
	_openedFile = new FileLruCache <string, FILE*> (MAX_OPEN_FILES);
	_segmentTransferCache = {};
	_segmentFolder = configLayer->getConfigString("Storage>SegmentCacheLocation");
	_blockFolder = configLayer->getConfigString("Storage>BlockLocation");

	// create folder if not exist
	struct stat st;
	if(stat(_segmentFolder.c_str(),&st) != 0) {
		debug ("%s does not exist, make directory automatically\n", _segmentFolder.c_str());
		if (mkdir (_segmentFolder.c_str(), S_IRWXU | S_IRGRP | S_IROTH) < 0) {
			perror ("mkdir");
			exit (-1);
		}
	}
	if(stat(_blockFolder.c_str(),&st) != 0) {
		debug ("%s does not exist, make directory automatically\n", _blockFolder.c_str());
		if (mkdir (_blockFolder.c_str(), S_IRWXU | S_IRGRP | S_IROTH) < 0) {
			perror ("mkdir");
			exit (-1);
		}
	}

	// Unit in StorageModule: Bytes
	_maxSegmentCache = stringToByte(configLayer->getConfigString("Storage>SegmentCacheCapacity"));
	_maxBlockCapacity = stringToByte(configLayer->getConfigString("Storage>BlockCapacity"));

	cout << "=== STORAGE ===" << endl;
	cout << "Segment Cache Location = " << _segmentFolder << " Size = "
	<< formatSize(_maxSegmentCache) << endl;
	cout << "Block Storage Location = " << _blockFolder << " Size = "
	<< formatSize(_maxBlockCapacity) << endl;
	cout << "===============" << endl;

	initializeStorageStatus();
}

StorageModule::~StorageModule() {

}

void StorageModule::initializeStorageStatus() {

	//
	// initialize segments
	//

	_freeSegmentSpace = _maxSegmentCache;
	_freeBlockSpace = _maxBlockCapacity;
	_currentSegmentUsage = 0;
	_currentBlockUsage = 0;

	struct dirent* dent;
	DIR* srcdir;

	srcdir = opendir(_segmentFolder.c_str());
	if (srcdir == NULL) {
		perror("opendir");
		exit(-1);
	}

	cout << "===== List of Files =====" << endl;

	while ((dent = readdir(srcdir)) != NULL) {
		struct stat st;

		if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
			continue;

		if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
			perror(dent->d_name);
			continue;
		}

		// save file info
		struct SegmentDiskCache segmentDiskCache;
		uint64_t segmentId = boost::lexical_cast<uint64_t>(dent->d_name);
		segmentDiskCache.length = st.st_size;
		segmentDiskCache.lastAccessedTime = st.st_atim;
		segmentDiskCache.filepath = _segmentFolder + dent->d_name;

		{
			lock_guard<mutex> lk(diskCacheMutex);
			_segmentDiskCacheMap.set(segmentId, segmentDiskCache);
			_segmentCacheQueue.push_back(segmentId);
		}

		_freeSegmentSpace -= st.st_size;
		_currentSegmentUsage += st.st_size;

		cout << "ID: " << segmentId << "\tLength: " << segmentDiskCache.length
				<< "\t Modified: " << segmentDiskCache.lastAccessedTime.tv_sec
				<< endl;

	}
	closedir(srcdir);

	cout << "=======================" << endl;

	cout << "Segment Cache Usage: " << formatSize(_currentSegmentUsage) << "/"
			<< formatSize(_maxSegmentCache) << endl;

	//
	// initialize blocks
	//

	srcdir = opendir(_blockFolder.c_str());
	if (srcdir == NULL) {
		perror("opendir");
		exit(-1);
	}

	while ((dent = readdir(srcdir)) != NULL) {
		struct stat st;

		if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
			continue;

		if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
			perror(dent->d_name);
			continue;
		}

		// save file info
		_freeBlockSpace -= st.st_size;
		_currentBlockUsage += st.st_size;
	}
	closedir(srcdir);

	cout << "Block Storage Usage: " << formatSize(_currentBlockUsage) << "/"
			<< formatSize(_maxBlockCapacity) << endl;

}

uint64_t StorageModule::getFilesize(string filepath) {

	ifstream in(filepath, ifstream::in | ifstream::binary);

	if (!in) {
		debug_error("ERROR: Cannot open file: %s\n", filepath.c_str());
		perror("ifstream");
		exit(-1);
	}

	// check filesize
	in.seekg(0, std::ifstream::end);
	uint64_t filesize = in.tellg();

	in.close();

	return filesize;

}

void StorageModule::createSegmentTransferCache(uint64_t segmentId, uint32_t length) {

	// create cache
	struct SegmentTransferCache segmentTransferCache;
	segmentTransferCache.length = length;
	segmentTransferCache.buf = MemoryPool::getInstance().poolMalloc(length);

	// save cache to map
	{
		lock_guard<mutex> lk(transferCacheMutex);
		_segmentTransferCache[segmentId] = segmentTransferCache;
	}

	debug("Segment created ID = %" PRIu64 " Length = %" PRIu32 "\n",
			segmentId, length);
}

void StorageModule::createSegmentDiskCache(uint64_t segmentId, uint32_t length) {
	const string filepath = generateSegmentPath(segmentId, _segmentFolder);
	createFile(filepath);

	// write info
	writeSegmentInfo(segmentId, length, filepath);
}

void StorageModule::createBlock(uint64_t segmentId, uint32_t blockId,
		uint32_t length) {

	const string filepath = generateBlockPath(segmentId, blockId, _blockFolder);
	createFile(filepath);

	debug(
			"Block created ObjID = %" PRIu64 " BlockID = %" PRIu32 " Length = %" PRIu32 " Path = %s\n",
			segmentId, blockId, length, filepath.c_str());
}

bool StorageModule::isSegmentCached(uint64_t segmentId) {

#ifndef USE_SEGMENT_CACHE
	return false;
#endif

	lock_guard<mutex> lk(diskCacheMutex);
	if (_segmentDiskCacheMap.count(segmentId)) {

		_segmentDiskCacheMap.get(segmentId).lastAccessedTime = {time(NULL), 0};

		_segmentCacheQueue.remove(segmentId);
		_segmentCacheQueue.push_back(segmentId);

		return true;
	}
	return false;
}

/**
 * Default length = 0 (read whole segment)
 */

struct SegmentData StorageModule::readSegment(uint64_t segmentId,
		uint64_t offsetInSegment, uint32_t length) {

	struct SegmentData segmentData;
	segmentData.info = readSegmentInfo(segmentId);

	// check num of bytes to read
	// if length = 0, read whole segment
	uint32_t byteToRead;
	if (length == 0) {
		byteToRead = segmentData.info.segmentSize;
	} else {
		byteToRead = length;
	}

	// TODO: check maximum malloc size
	// poolFree in osd_communicator::sendSegment
	segmentData.buf = MemoryPool::getInstance().poolMalloc(byteToRead);

	readFile(segmentData.info.segmentPath, segmentData.buf, offsetInSegment,
			byteToRead);

	debug(
			"Segment ID = %" PRIu64 " read %" PRIu32 " bytes at offset %" PRIu64 "\n",
			segmentId, byteToRead, offsetInSegment);

	return segmentData;

}

struct BlockData StorageModule::readBlock(uint64_t segmentId,
		uint32_t blockId, uint64_t offsetInBlock, uint32_t length) {

	struct BlockData blockData;
	string blockPath = generateBlockPath(segmentId, blockId,
			_blockFolder);
	blockData.info.segmentId = segmentId;
	blockData.info.blockId = blockId;

	// check num of bytes to read
	// if length = 0, read whole block
	uint32_t byteToRead;
	if (length == 0) {
		const uint32_t filesize = getFilesize(blockPath);
		byteToRead = filesize;
	} else {
		byteToRead = length;
	}

	blockData.info.blockSize = byteToRead;

	blockData.buf = MemoryPool::getInstance().poolMalloc(byteToRead);

	readFile(blockPath, blockData.buf, offsetInBlock, byteToRead);

	debug(
			"Segment ID = %" PRIu64 " Block ID = %" PRIu32 " read %" PRIu32 " bytes at offset %" PRIu64 "\n",
			segmentId, blockId, byteToRead, offsetInBlock);

	return blockData;
}

struct BlockData StorageModule::readBlock(uint64_t segmentId,
		uint32_t blockId, vector<offset_length_t> symbols) {

	uint32_t combinedLength = 0;
	for (auto offsetLengthPair : symbols) {
		combinedLength += offsetLengthPair.second;
	}

	struct BlockData blockData;
	string blockPath = generateBlockPath(segmentId, blockId,
			_blockFolder);
	blockData.info.segmentId = segmentId;
	blockData.info.blockId = blockId;
	blockData.info.blockSize = combinedLength;
	blockData.buf = MemoryPool::getInstance().poolMalloc(combinedLength);

	uint32_t blockOffset = 0;
	for (auto offsetLengthPair : symbols) {
		uint32_t offset = offsetLengthPair.first;
		uint32_t length = offsetLengthPair.second;
        blockOffset += offset;
		readFile(blockPath, blockData.buf, blockOffset, length);
	}

	debug(
			"Segment ID = %" PRIu64 " Block ID = %" PRIu32 " read %zu symbols\n",
			segmentId, blockId, symbols.size());

	return blockData;

}

uint32_t StorageModule::writeSegmentTransferCache(uint64_t segmentId, char* buf,
		uint64_t offsetInSegment, uint32_t length) {

	char* recvCache;

	{
		lock_guard<mutex> lk(transferCacheMutex);
		if (!_segmentTransferCache.count(segmentId)) {
			debug_error("Cannot find cache for segment %" PRIu64 "\n", segmentId);
			exit(-1);
		}
		recvCache = _segmentTransferCache[segmentId].buf;
	}

	memcpy(recvCache + offsetInSegment, buf, length);

	return length;
}

uint32_t StorageModule::writeSegmentDiskCache(uint64_t segmentId, char* buf,
		uint64_t offsetInSegment, uint32_t length) {

	uint32_t byteWritten;

	string filepath = generateSegmentPath(segmentId, _segmentFolder);
	byteWritten = writeFile(filepath, buf, offsetInSegment, length);

	debug(
			"Segment ID = %" PRIu64 " write %" PRIu32 " bytes at offset %" PRIu64 "\n",
			segmentId, byteWritten, offsetInSegment);

	return byteWritten;
}

uint32_t StorageModule::writeBlock(uint64_t segmentId, uint32_t blockId,
		char* buf, uint64_t offsetInBlock, uint32_t length) {

	uint32_t byteWritten = 0;

	string filepath = generateBlockPath(segmentId, blockId, _blockFolder);
	byteWritten = writeFile(filepath, buf, offsetInBlock, length);

	debug(
			"Segment ID = %" PRIu64 " Block ID = %" PRIu32 " write %" PRIu32 " bytes at offset %" PRIu64 "\n",
			segmentId, blockId, byteWritten, offsetInBlock);

	updateBlockFreespace(length);
//	closeFile(filepath);

	return byteWritten;
}

void StorageModule::closeSegmentTransferCache(uint64_t segmentId) {
	// close cache
	struct SegmentTransferCache segmentCache = getSegmentTransferCache(segmentId);
	MemoryPool::getInstance().poolFree(segmentCache.buf);

	{
		lock_guard<mutex> lk(transferCacheMutex);
		_segmentTransferCache.erase(segmentId);
	}

	debug("Segment Cache ID = %" PRIu64 " closed\n", segmentId);
}

void StorageModule::flushSegmentDiskCache(uint64_t segmentId) {

	FILE* filePtr = NULL;
	try {
		string filepath = generateSegmentPath(segmentId, _segmentFolder);
		filePtr = _openedFile->get (filepath);
		fflush (filePtr);
		fsync (fileno(filePtr));
	} catch (out_of_range& oor) { // file pointer not found in cache
		return;	// file already closed by cache, do nothing
	}
	
}

void StorageModule::flushBlock(uint64_t segmentId, uint32_t blockId) {

	FILE* filePtr = NULL;
	try {
		string filepath = generateBlockPath(segmentId, blockId, _blockFolder);
		filePtr = _openedFile->get (filepath);
		fflush (filePtr);
		fsync (fileno(filePtr));
	} catch (out_of_range& oor) { // file pointer not found in cache
		return;	// file already closed by cache, do nothing
	}
}

//
// PRIVATE METHODS
//

void StorageModule::writeSegmentInfo(uint64_t segmentId, uint32_t segmentSize,
		string filepath) {

	// TODO: Database to be implemented

}

struct SegmentInfo StorageModule::readSegmentInfo(uint64_t segmentId) {
	// TODO: Database to be implemented
	struct SegmentInfo segmentInfo;

	segmentInfo.segmentId = segmentId;
	segmentInfo.segmentPath = generateSegmentPath(segmentId, _segmentFolder);
	segmentInfo.segmentSize = getFilesize(segmentInfo.segmentPath);

	return segmentInfo;
}

uint32_t StorageModule::readFile(string filepath, char* buf, uint64_t offset,
		uint32_t length) {

	debug("Read File :%s\n", filepath.c_str());

	// lock file access function
	lock_guard<mutex> lk(fileMutex);

	FILE* file = openFile(filepath);

	if (file == NULL) { // cannot open file
		debug("%s\n", "Cannot read");
		perror("open");
		exit(-1);
	}

	// Read file contents into buffer
	uint32_t byteRead = pread(fileno(file), buf, length, offset);

	if (byteRead != length) {
		debug_error("ERROR: Length = %" PRIu32 ", byteRead = %" PRIu32 "\n",
				length, byteRead);
		perror("pread()");
		exit(-1);
	}

	return byteRead;
}

uint32_t StorageModule::writeFile(string filepath, char* buf, uint64_t offset,
		uint32_t length) {

	// lock file access function
	lock_guard<mutex> lk(fileMutex);

	FILE* file = openFile(filepath);

	if (file == NULL) { // cannot open file
		debug("%s\n", "Cannot write");
		perror("open");
		exit(-1);
	}

	// Write file contents from buffer
	uint32_t byteWritten = pwrite(fileno(file), buf, length, offset);

	if (byteWritten != length) {
		debug_error("ERROR: Length = %d, byteWritten = %d\n", length, byteWritten);
		exit(-1);
	}

	return byteWritten;
}

string StorageModule::generateSegmentPath(uint64_t segmentId,
		string segmentFolder) {

	// append a '/' if not present
	if (segmentFolder[segmentFolder.length() - 1] != '/') {
		segmentFolder.append("/");
	}

	return segmentFolder + to_string(segmentId);
}

string StorageModule::generateBlockPath(uint64_t segmentId, uint32_t blockId,
		string blockFolder) {

	// append a '/' if not present
	if (blockFolder[blockFolder.length() - 1] != '/') {
		blockFolder.append("/");
	}

	return blockFolder + to_string(segmentId) + "." + to_string(blockId);
}

/**
 * Create and open a new file
 */

FILE* StorageModule::createFile(string filepath) {

	// open file for read/write
	// create new if not exist
	FILE* filePtr;
	filePtr = fopen(filepath.c_str(), "wb+");

	if (filePtr == NULL) {
		debug("%s\n", "Unable to create file!");
		return NULL;
	}

	// set buffer to zero to avoid memory leak
	//setvbuf(filePtr, NULL, _IONBF, 0);

	// add file pointer to map
	_openedFile->insert(filepath, filePtr);

	/*
	openedFileMutex.lock();
	_openedFile[filepath] = filePtr;
	openedFileMutex.unlock();
	*/

	return filePtr;
}

/**
 * Open an existing file, return pointer directly if file is already open
 */

FILE* StorageModule::openFile(string filepath) {

	FILE* filePtr = NULL;
	try {
		filePtr = _openedFile->get (filepath);
	} catch (out_of_range& oor) { // file pointer not found in cache
		filePtr = fopen(filepath.c_str(), "rb+");

		if (filePtr == NULL) {
			debug("Unable to open file at %s\n", filepath.c_str());
			perror("fopen()");
			return NULL;
		}

		// set buffer to zero to avoid memory leak
		setvbuf(filePtr, NULL, _IONBF, 0);

		// add file pointer to map
		_openedFile->insert(filepath, filePtr);
	}

	return filePtr;
}

/**
 * Close file, remove from map
 */

struct SegmentTransferCache StorageModule::getSegmentTransferCache(uint64_t segmentId) {
	lock_guard<mutex> lk(transferCacheMutex);
	if (!_segmentTransferCache.count(segmentId)) {
		debug_error("Segment cache not found %" PRIu64 "\n", segmentId);
		exit(-1);
	}
	return _segmentTransferCache[segmentId];
}

void StorageModule::setMaxBlockCapacity(uint32_t max_block) {
	_maxBlockCapacity = max_block;
}

void StorageModule::setMaxSegmentCache(uint32_t max_segment) {
	_maxSegmentCache = max_segment;
}

uint32_t StorageModule::getMaxBlockCapacity() {
	return _maxBlockCapacity;
}

uint32_t StorageModule::getMaxSegmentCache() {
	return _maxSegmentCache;
}

bool StorageModule::verifyBlockSpace(uint32_t size) {
	return size <= _freeBlockSpace ? true : false;
}

bool StorageModule::verifySegmentSpace(uint32_t size) {
	return size <= _freeSegmentSpace ? true : false;
}

void StorageModule::updateBlockFreespace(uint32_t new_block_size) {
	uint32_t update_space = new_block_size;
	if (verifyBlockSpace(update_space)) {
		_currentBlockUsage += update_space;
		_freeBlockSpace -= update_space;
	} else {
		perror("block free space not enough.\n");
	}

}

uint32_t StorageModule::getCurrentBlockCapacity() {
	return _currentBlockUsage;
}

uint32_t StorageModule::getCurrentSegmentCache() {
	return _currentSegmentUsage;
}

uint32_t StorageModule::getFreeBlockSpace() {
	return _freeBlockSpace;
}

uint32_t StorageModule::getFreeSegmentSpace() {
	return _freeSegmentSpace;
}

int32_t StorageModule::spareSegmentSpace(uint32_t newSegmentSize) {

	if (_maxSegmentCache < newSegmentSize) {
		return -1; // error: segment size larger than cache
	}

	while (_freeSegmentSpace < newSegmentSize) {
		uint64_t segmentId = 0;
		struct SegmentDiskCache segmentCache;

		{
			lock_guard<mutex> lk(diskCacheMutex);
			segmentId = _segmentCacheQueue.front();
			segmentCache = _segmentDiskCacheMap.get(segmentId);
		}

		remove(segmentCache.filepath.c_str());

		// update size
		_freeSegmentSpace += segmentCache.length;
		_currentSegmentUsage -= segmentCache.length;

		// remove from queue and map
		{
			lock_guard<mutex> lk(diskCacheMutex);
			_segmentCacheQueue.remove(segmentId);
			_segmentDiskCacheMap.erase(segmentId);
		}
	}

	return 0;

}

void StorageModule::putSegmentToDiskCache(uint64_t segmentId,
		SegmentTransferCache segmentCache) {

	debug("Before saving segment ID = %" PRIu64 ", cache = %s\n",
			segmentId, formatSize(_freeSegmentSpace).c_str());

	uint32_t segmentSize = segmentCache.length;

	{

		lock_guard<mutex> lk(diskSpaceMutex);

		if (!verifySegmentSpace(segmentSize)) {
			// clear cache if space is not available
			//updateSegmentFreespace(spareSegmentSpace(segmentSize));
			if (spareSegmentSpace(segmentSize) == -1) {
				debug(
						"Not enough space to cache segment! Segment Size = %" PRIu32 "\n",
						segmentSize);
				return;
			}
		}

	}

	debug("Spare space for saving segment ID = %" PRIu64 ", cache = %s\n",
			segmentId, formatSize(_freeSegmentSpace).c_str());

	// write cache to disk
	const string filepath = generateSegmentPath(segmentId, _segmentFolder);
	createFile(filepath);

#ifdef USE_SEGMENT_CACHE
	uint64_t byteWritten = writeSegmentDiskCache(segmentId, segmentCache.buf, 0,
			segmentCache.length);
#else
	uint64_t byteWritten = segmentCache.length;
#endif

	if (byteWritten != segmentCache.length) {
		perror("Cannot saveSegmentToDisk");
		exit(-1);
	}
	flushSegmentDiskCache(segmentId);

	_currentSegmentUsage += segmentSize;
	_freeSegmentSpace -= segmentSize;

	// save cache to map
	struct SegmentDiskCache segmentDiskCache;
	segmentDiskCache.filepath = generateSegmentPath(segmentId, _segmentFolder);
	segmentDiskCache.length = segmentCache.length;
	segmentDiskCache.lastAccessedTime = {time(NULL), 0}; // set to current time

	{
		lock_guard<mutex> lk(diskCacheMutex);
		_segmentDiskCacheMap.set(segmentId, segmentDiskCache);
		_segmentCacheQueue.push_back(segmentId);
	}

	debug("After saving segment ID = %" PRIu64 ", cache = %s\n",
			segmentId, formatSize(_freeSegmentSpace).c_str());

}

struct SegmentData StorageModule::getSegmentFromDiskCache(uint64_t segmentId) {
	struct SegmentData segmentData;
	struct SegmentDiskCache segmentDiskCache;
	{
		lock_guard<mutex> lk(diskCacheMutex);
		segmentDiskCache = _segmentDiskCacheMap.get(segmentId);
	}
	segmentData = readSegment(segmentId, 0, segmentDiskCache.length);
	return segmentData;
}

void StorageModule::clearSegmentDiskCache() {

	for (auto segment : _segmentCacheQueue) {
		remove(string(_segmentFolder + to_string(segment)).c_str());
	}

	{
		lock_guard<mutex> lk(diskCacheMutex);
		_segmentCacheQueue.clear();
		_segmentDiskCacheMap.clear();
	}

	_freeSegmentSpace = _maxSegmentCache;
	_currentSegmentUsage = 0;
}
