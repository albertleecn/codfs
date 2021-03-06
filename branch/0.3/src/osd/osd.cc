/**
 * osd.cc
 */

#include <thread>
#include <vector>
#include <openssl/md5.h>
#include "osd.hh"
#include "../common/blocklocation.hh"
#include "../common/debug.hh"
#include "../common/define.hh"
#include "../common/metadata.hh"
#include "../common/convertor.hh"
#include "../config/config.hh"
#include "../protocol/status/osdstartupmsg.hh"
#include "../protocol/status/osdshutdownmsg.hh"
#include "../protocol/status/osdstatupdatereplymsg.hh"
#include "../protocol/status/newosdregistermsg.hh"
#include "../protocol/transfer/getblockinitrequest.hh"

// for random srand() time() rand() getloadavg()
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/statvfs.h>

// Global Variables
extern Osd* osd;
extern ConfigLayer* configLayer;
mutex segmentRequestCountMutex;
mutex recoveryMutex;

#include <atomic>

#ifdef PARALLEL_TRANSFER

#include "../../lib/threadpool/threadpool.hpp"
boost::threadpool::pool _blocktp;
boost::threadpool::pool _recoverytp;
#endif

#ifdef TIME_POINT
#include <chrono>
using namespace std;
typedef chrono::high_resolution_clock Clock;
typedef chrono::milliseconds milliseconds;
mutex timeMutex;
double lockSegmentCountMutexTime = 0;
double getSegmentInfoTime = 0;
double getOSDStatusTime = 0;
double getBlockTime = 0;
double decodeSegmentTime = 0;
double sendSegmentTime = 0;
double cacheSegmentTime = 0;
#endif

Osd::Osd(uint32_t selfId) {

	configLayer = new ConfigLayer("osdconfig.xml", "common.xml");
	_storageModule = new StorageModule();
	_osdCommunicator = new OsdCommunicator();
	_codingModule = new CodingModule();
	_osdId = selfId;

	srand(time(NULL)); //random test

#ifdef PARALLEL_TRANSFER
	uint32_t _numThreads = configLayer->getConfigInt("ThreadPool>NumThreads");
	_blocktp.size_controller().resize(_numThreads);
	_recoverytp.size_controller().resize(RECOVERY_THREADS);
#endif

}

Osd::~Osd() {
	//delete _blockLocationCache;
	delete _storageModule;
	delete _osdCommunicator;
}

void Osd::cacheSegment(uint64_t segmentId, SegmentData segmentData) {
	// cache segmentData
	struct SegmentTransferCache segmentTransferCache;
	segmentTransferCache.buf = segmentData.buf;
	segmentTransferCache.length = segmentData.info.segmentSize;

	if (!_storageModule->isSegmentCached(segmentId)) {
		_storageModule->putSegmentToDiskCache(segmentId, segmentTransferCache);
	}
}

void Osd::freeSegment(uint64_t segmentId, SegmentData segmentData) {
	// free segmentData
	debug("free segment %" PRIu64 "\n", segmentId);
	MemoryPool::getInstance().poolFree(segmentData.buf);
	debug("segment %" PRIu64 "free-d\n", segmentId);

	_segmentDataMap.erase(segmentId);
}

/**
 * Send the segment to the target
 */

void Osd::getSegmentRequestProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t segmentId, bool localRetrieve) {

	if (localRetrieve) {
		debug_yellow("Local retrieve for segment ID = %" PRIu64 "\n",
				segmentId);
	}

#ifdef TIME_POINT
	Clock::time_point t0 = Clock::now();
	Clock::time_point t1 = Clock::now();
	Clock::time_point t2 = Clock::now();
	Clock::time_point t3 = Clock::now();
	Clock::time_point t4 = Clock::now();
	Clock::time_point t5 = Clock::now();
	Clock::time_point t6 = Clock::now();
	Clock::time_point t7 = Clock::now();
#endif
	segmentRequestCountMutex.lock();
	if (!_segmentRequestCount.count(segmentId)) {
		_segmentRequestCount.set(segmentId, 1);
		mutex* tempMutex = new mutex();
		_segmentDownloadMutex.set(segmentId, tempMutex);
		_segmentDataMap.set(segmentId, { });
		_isSegmentDownloaded.set(segmentId, false);
	} else {
		_segmentRequestCount.increment(segmentId);
	}
	struct SegmentData& segmentData = _segmentDataMap.get(segmentId);
	segmentRequestCountMutex.unlock();
#ifdef TIME_POINT
	t1 = Clock::now();
#endif

	{
		lock_guard<mutex> lk(*(_segmentDownloadMutex.get(segmentId)));

		if (!_isSegmentDownloaded.get(segmentId)) {

			if (_storageModule->isSegmentCached(segmentId)) {
				// case 1: if segment exists in cache
				debug("Segment ID = %" PRIu64 " exists in cache", segmentId);

				// for local retrieve, no need to get it
				if (!localRetrieve) {
					segmentData = _storageModule->getSegmentFromDiskCache(
							segmentId);
				}

			} else {
				// case 2: if segment does not exist in cache

				// TODO: check if osd list exists in cache

				// 1. ask MDS to get segment information

				SegmentTransferOsdInfo segmentInfo =
						_osdCommunicator->getSegmentInfoRequest(segmentId);
#ifdef TIME_POINT
				t2 = Clock::now();
#endif
				const CodingScheme codingScheme = segmentInfo._codingScheme;
				const string codingSetting = segmentInfo._codingSetting;
				const uint32_t segmentSize = segmentInfo._size;

				// bool array to store osdStatus
				vector<bool> blockStatus =
						_osdCommunicator->getOsdStatusRequest(
								segmentInfo._osdList);

#ifdef TIME_POINT
				t3 = Clock::now();
#endif
				// check which blocks are needed to request
				uint32_t totalNumOfBlocks = segmentInfo._osdList.size();
				block_list_t requiredBlockSymbols =
						_codingModule->getRequiredBlockSymbols(codingScheme,
								blockStatus, segmentSize, codingSetting);

				// error in finding required Blocks (not enough blocks to rebuild segment)
				if (requiredBlockSymbols.size() == 0) {
					debug_error(
							"Not enough blocks available to rebuild Segment ID %" PRIu64 "\n",
							segmentId);
					exit(-1);
				}

				// 2. initialize list and count

				const uint32_t blockCount = requiredBlockSymbols.size();

				_downloadBlockRemaining.set(segmentId, blockCount);
				_receivedBlockData.set(segmentId,
						vector<struct BlockData>(totalNumOfBlocks));
				vector<struct BlockData>& blockDataList =
						_receivedBlockData.get(segmentId);

				debug("PendingBlockCount = %" PRIu32 "\n", blockCount);

				// 3. request blocks
				// case 1: load from disk
				// case 2: request from OSD
				// case 3: already requested

				for (auto blockSymbols : requiredBlockSymbols) {

					uint32_t i = blockSymbols.first;

					uint32_t osdId = segmentInfo._osdList[i];

					if (osdId == _osdId) {
						// read block from disk
						struct BlockData blockData = _storageModule->readBlock(
								segmentId, i, blockSymbols.second);

						// blockDataList reserved space for "all blocks"
						// only fill in data for "required blocks"
						blockDataList[i] = blockData;

						_downloadBlockRemaining.decrement(segmentId);
						debug(
								"Read from local block for Segment ID = %" PRIu64 " Block ID = %" PRIu32 "\n",
								segmentId, i);

					} else {
						// request block from other OSD
						debug("sending request for block %" PRIu32 "\n", i);
						_osdCommunicator->getBlockRequest(osdId, segmentId, i,
								blockSymbols.second);
					}
				}

				// 4. wait until all blocks have arrived

				while (1) {
					if (_downloadBlockRemaining.get(segmentId) == 0) {
#ifdef TIME_POINT
						t4 = Clock::now();
#endif

						// 5. decode blocks

						debug(
								"[DOWNLOAD] Start Decoding with %d scheme and settings = %s\n",
								(int)codingScheme, codingSetting.c_str());
						segmentData = _codingModule->decodeBlockToSegment(
								codingScheme, blockDataList,
								requiredBlockSymbols, segmentSize,
								codingSetting);

						// clean up block data
						_downloadBlockRemaining.erase(segmentId);

						for (auto blockSymbols : requiredBlockSymbols) {
							uint32_t i = blockSymbols.first;
							debug(
									"%" PRIu32 " free block %" PRIu32 " addr = %p\n",
									i, blockDataList[i].info.blockId, blockDataList[i].buf);
							MemoryPool::getInstance().poolFree(
									blockDataList[i].buf);
							debug("%" PRIu32 " block %" PRIu32 " free-d\n",
									i, blockDataList[i].info.blockId);
						}
						_receivedBlockData.erase(segmentId);
#ifdef TIME_PONT
						t5 = Clock::now();
#endif

						break;
					} else {
						usleep(50000); // 0.01s
					}
				}
			}

			debug("%s\n", "[DOWNLOAD] Send Segment");

			_isSegmentDownloaded.set(segmentId, true);
		}
	}

	// 5. send segment if not localRetrieve
	if (!localRetrieve) {
		_osdCommunicator->sendSegment(_osdId, sockfd, segmentData);
	}

#ifdef TIME_POINT
	t6 = Clock::now();
#endif

	// 6. cache and free
	{
		lock_guard<mutex> lk(segmentRequestCountMutex);
		_segmentRequestCount.decrement(segmentId);
		if (_segmentRequestCount.get(segmentId) == 0) {
			_segmentRequestCount.erase(segmentId);

			cacheSegment(segmentId, segmentData);
			freeSegment(segmentId, segmentData);

			debug("%s\n", "[DOWNLOAD] Cleanup completed");
		}
	}

	// send reply to MDS for localRetrieve
	if (localRetrieve) {
		_osdCommunicator->replyCacheSegment(requestId, sockfd, segmentId);
	}

#ifdef TIME_POINT
	t7 = Clock::now();
#endif

#ifdef TIME_POINT
	timeMutex.lock();
	lockSegmentCountMutexTime += chrono::duration_cast < milliseconds > (t1 - t0).count();
	getSegmentInfoTime += chrono::duration_cast < milliseconds > (t2 - t1).count();
	getOSDStatusTime += chrono::duration_cast < milliseconds > (t3 - t2).count();
	getBlockTime += chrono::duration_cast < milliseconds > (t4 - t3).count();
	decodeSegmentTime += chrono::duration_cast < milliseconds > (t5 - t4).count();
	sendSegmentTime += chrono::duration_cast < milliseconds > (t6 - t5).count();
	if (!localRetrieve) {
		cacheSegmentTime += chrono::duration_cast < milliseconds > (t7 - t6).count();
	}
	timeMutex.unlock();
#endif

}

void Osd::getBlockRequestProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t segmentId, uint32_t blockId, vector<offset_length_t> symbols) {
	struct BlockData blockData = _storageModule->readBlock(segmentId, blockId,
			symbols);
	_osdCommunicator->sendBlock(sockfd, blockData);
	MemoryPool::getInstance().poolFree(blockData.buf);
	debug("Block ID = %" PRIu32 " free-d\n", blockId);
}

void Osd::getRecoveryBlockProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t segmentId, uint32_t blockId, vector<offset_length_t> symbols) {
	struct BlockData blockData = _storageModule->readBlock(segmentId, blockId,
			symbols);

	// will block until reply is received
	_osdCommunicator->sendRecoveryBlock(requestId, sockfd, blockData);

	MemoryPool::getInstance().poolFree(blockData.buf);
}

void Osd::retrieveRecoveryBlock(uint32_t requestId, uint32_t osdId,
		uint64_t segmentId, uint32_t blockId,
		vector<offset_length_t> &offsetLength, BlockData &repairedBlock) {

	if (osdId == _osdId) {
		// read block from disk
		repairedBlock = _storageModule->readBlock(segmentId, blockId,
				offsetLength);
	} else {
		repairedBlock = _osdCommunicator->getRecoveryBlock(osdId, segmentId,
				blockId, offsetLength);
		debug_cyan("[RECOVERY] Collected Symbols for Block %" PRIu32 "\n",
				blockId);
	}

	_recoverytpRequestCount.decrement(requestId);
}

void Osd::recoveryBlockDataProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t segmentId, uint32_t blockId, uint32_t length, char* buf,
		uint32_t waitOnRequestId) {

	// copy data
	BlockData blockData;
	blockData.info.segmentId = segmentId;
	blockData.info.blockSize = length;
	blockData.info.blockId = blockId;
	blockData.buf = MemoryPool::getInstance().poolMalloc(length);
	memcpy(blockData.buf, buf, length);

	// attach data to request
	GetBlockInitRequestMsg* getBlockInitRequestMsg =
			(GetBlockInitRequestMsg*) _osdCommunicator->popWaitReplyMessage(
					requestId);
	getBlockInitRequestMsg->setRecoveryBlockData(blockData);
	getBlockInitRequestMsg->setStatus(READY);

	debug_yellow(
			"Recovery block for segment %" PRIu64 " ,Block %" PRIu32 ", requestId %" PRIu32 " arrived\n",
			segmentId, blockId, requestId);

	// send reply
	_osdCommunicator->replyPutBlockEnd(requestId, sockfd, segmentId, blockId,
			waitOnRequestId);

}

void Osd::putSegmentInitProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t segmentId, uint32_t length, uint32_t chunkCount,
		CodingScheme codingScheme, string setting, string checksum) {

	struct CodingSetting codingSetting;
	codingSetting.codingScheme = codingScheme;
	codingSetting.setting = setting;

	// initialize chunkCount value
	_pendingSegmentChunk.set(segmentId, chunkCount);

	// save coding setting
	_codingSettingMap.set(segmentId, codingSetting);

	// create segment and cache
	_storageModule->createSegmentTransferCache(segmentId, length);
	_osdCommunicator->replyPutSegmentInit(requestId, sockfd, segmentId);

	// save md5 to map
	_checksumMap.set(segmentId, checksum);

}

void Osd::distributeBlock(uint64_t segmentId, const struct BlockData& blockData,
		const struct BlockLocation& blockLocation, uint32_t blocktpRequestId) {
	debug("Distribute Block %" PRIu64 ".%" PRIu32 " to %" PRIu32 "\n",
			segmentId, blockData.info.blockId, blockLocation.osdId);
	// if destination is myself
	if (blockLocation.osdId == _osdId) {
		_storageModule->createBlock(segmentId, blockData.info.blockId,
				blockData.info.blockSize);
		_storageModule->writeBlock(segmentId, blockData.info.blockId,
				blockData.buf, 0, blockData.info.blockSize);
		_storageModule->flushBlock(segmentId, blockData.info.blockId);
	} else {
		uint32_t dstSockfd = _osdCommunicator->getSockfdFromId(
				blockLocation.osdId);
		_osdCommunicator->sendBlock(dstSockfd, blockData);
	}

	// free memory
	MemoryPool::getInstance().poolFree(blockData.buf);

	if (blocktpRequestId != 0) {
		_blocktpRequestCount.decrement(blocktpRequestId);
	}

	debug(
			"Completed Distributing Block %" PRIu64 ".%" PRIu32 " to %" PRIu32 "\n",
			segmentId, blockData.info.blockId, blockLocation.osdId);

}

void Osd::putSegmentEndProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t segmentId) {

	// TODO: check integrity of segment received
	while (1) {

		if (_pendingSegmentChunk.get(segmentId) == 0) {
			// if all chunks have arrived
			struct SegmentTransferCache segmentCache =
					_storageModule->getSegmentTransferCache(segmentId);

			// compute md5 checksum
			unsigned char checksum[MD5_DIGEST_LENGTH];
			MD5((unsigned char*) segmentCache.buf, segmentCache.length,
					checksum);
			debug_cyan("md5 of segment ID %" PRIu64 " = %s\n",
					segmentId, md5ToHex(checksum).c_str());

			// compare md5 with saved one
			if (_checksumMap.get(segmentId) != md5ToHex(checksum)) {
				debug_error("MD5 of Segment ID = %" PRIu64 " mismatch!\n",
						segmentId);
				exit(-1);
			} else {
				debug("MD5 of Segment ID = %" PRIu64 " match\n", segmentId);
				_checksumMap.erase(segmentId);
			}

			// perform coding
			struct CodingSetting codingSetting = _codingSettingMap.get(
					segmentId);
			_codingSettingMap.erase(segmentId);

			debug("Coding Scheme = %d setting = %s\n",
					(int) codingSetting.codingScheme, codingSetting.setting.c_str());

			vector<struct BlockData> blockDataList =
					_codingModule->encodeSegmentToBlock(
							codingSetting.codingScheme, segmentId,
							segmentCache.buf, segmentCache.length,
							codingSetting.setting);

			// request secondary OSD list
			vector<struct BlockLocation> blockLocationList =
					_osdCommunicator->getOsdListRequest(segmentId, MONITOR,
							blockDataList.size(), _osdId,
							blockDataList[0].info.blockSize);

			vector<uint32_t> nodeList;
			uint32_t i = 0;

			_blocktpRequestCount.set(requestId, blockDataList.size());

			for (const auto blockData : blockDataList) {

#ifdef PARALLEL_TRANSFER
				debug("Thread Pool Status %d/%d/%d\n",
						(int)_blocktp.active(), (int)_blocktp.pending(), (int)_blocktp.size());
				_blocktp.schedule(
						boost::bind(&Osd::distributeBlock, this, segmentId,
								blockData, blockLocationList[i], requestId));
#else
				distributeBlock(segmentId, blockData,blockLocationList[i]);
#endif

				nodeList.push_back(blockLocationList[i].osdId);

				i++;
			}
#ifdef PARALLEL_TRANSFER
			// block until all blocks retrieved
			while (_blocktpRequestCount.get(requestId) > 0) {
				usleep(10000);
			}
#endif
			_blocktpRequestCount.erase(requestId);
			_pendingSegmentChunk.erase(segmentId);

			// Acknowledge MDS for Segment Upload Completed
			_osdCommunicator->segmentUploadAck(segmentId, segmentCache.length,
					codingSetting.codingScheme, codingSetting.setting, nodeList,
					md5ToHex(checksum));

			cout << "Segment " << segmentId << " uploaded" << endl;

			// if all chunks have arrived, send ack
			_osdCommunicator->replyPutSegmentEnd(requestId, sockfd, segmentId);

			// after ack, write segment cache to disk (and close file)
			_storageModule->putSegmentToDiskCache(segmentId, segmentCache);

			// close file and free cache
			_storageModule->closeSegmentTransferCache(segmentId);

			break;
		} else {
			usleep(10000); // sleep 0.01s
		}

	}

}

void Osd::putBlockEndProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t segmentId, uint32_t blockId) {

	// TODO: check integrity of block received
	const string blockKey = to_string(segmentId) + "." + to_string(blockId);

	bool isDownload = _downloadBlockRemaining.count(segmentId);

	while (1) {

		if (_pendingBlockChunk.get(blockKey) == 0) {

			if (!isDownload) {
				// close file and free cache
				_storageModule->flushBlock(segmentId, blockId);
			} else {
				_downloadBlockRemaining.decrement(segmentId);
				debug("all chunks for block %" PRIu32 "is received\n", blockId);
			}

			// remove from map
			_pendingBlockChunk.erase(blockKey);

		}
		// if all chunks have arrived, send ack
		if (!_pendingBlockChunk.count(blockKey)) {
			_osdCommunicator->replyPutBlockEnd(requestId, sockfd, segmentId,
					blockId);
			break;
		} else {
			usleep(10000); // sleep 0.01s
		}

	}

}

uint32_t Osd::putSegmentDataProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t segmentId, uint64_t offset, uint32_t length, char* buf) {

	uint32_t byteWritten;
	byteWritten = _storageModule->writeSegmentTransferCache(segmentId, buf,
			offset, length);

	_pendingSegmentChunk.decrement(segmentId);

	return byteWritten;
}

void Osd::putBlockInitProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t segmentId, uint32_t blockId, uint32_t length,
		uint32_t chunkCount) {

	const string blockKey = to_string(segmentId) + "." + to_string(blockId);
	bool isDownload = _downloadBlockRemaining.count(segmentId);

	debug(
			"[PUT_BLOCK_INIT] Segment ID = %" PRIu64 ", Block ID = %" PRIu32 ", Length = %" PRIu32 ", Count = %" PRIu32 "isDownload = %d\n",
			segmentId, blockId, length, chunkCount, isDownload);

	_pendingBlockChunk.set(blockKey, chunkCount);

	if (isDownload) {
		struct BlockData& blockData = _receivedBlockData.get(segmentId)[blockId];

		blockData.info.segmentId = segmentId;
		blockData.info.blockId = blockId;
		blockData.info.blockSize = length;
		blockData.buf = MemoryPool::getInstance().poolMalloc(length);
	} else {
		// create file
		_storageModule->createBlock(segmentId, blockId, length);
	}

	_osdCommunicator->replyPutBlockInit(requestId, sockfd, segmentId, blockId);

}

uint32_t Osd::putBlockDataProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t segmentId, uint32_t blockId, uint32_t offset, uint32_t length,
		char* buf) {

	const string blockKey = to_string(segmentId) + "." + to_string(blockId);

	uint32_t byteWritten = 0;
	bool isDownload = _downloadBlockRemaining.count(segmentId);

	// if the block received is for download process
	if (isDownload) {
		struct BlockData& blockData = _receivedBlockData.get(segmentId)[blockId];

		debug("offset = %" PRIu32 ", length = %" PRIu32 "\n", offset, length);
		memcpy(blockData.buf + offset, buf, length);
	} else {
		byteWritten = _storageModule->writeBlock(segmentId, blockId, buf,
				offset, length);
	}

	_pendingBlockChunk.decrement(blockKey);

	/*
	 // if all chunks have arrived
	 if (_pendingBlockChunk.get(blockKey) == 0) {

	 if (!isDownload) {
	 // close file and free cache
	 _storageModule->flushBlock(segmentId, blockId);
	 } else {
	 _downloadBlockRemaining.decrement(segmentId);
	 debug("all chunks for block %" PRIu32 "is received\n", blockId);
	 }

	 // remove from map
	 _pendingBlockChunk.erase(blockKey);

	 }
	 */
	return byteWritten;
}

void Osd::repairSegmentInfoProcessor(uint32_t requestId, uint32_t sockfd,
		uint64_t segmentId, vector<uint32_t> repairBlockList,
		vector<uint32_t> repairBlockOsdList) {

	string repairBlockOsdListString;
	for (uint32_t osd : repairBlockOsdList) {
		repairBlockOsdListString += to_string(osd) + " ";
	}
	debug_yellow(
			"Repair OSD List repairBlockList size = %zu Osd List size = %zu: %s\n",
			repairBlockList.size(), repairBlockOsdList.size(), repairBlockOsdListString.c_str());

//    lock_guard<mutex> lk(recoveryMutex);

	// get coding information from MDS
	SegmentTransferOsdInfo segmentInfo =
			_osdCommunicator->getSegmentInfoRequest(segmentId);

	const CodingScheme codingScheme = segmentInfo._codingScheme;
	const string codingSetting = segmentInfo._codingSetting;
	const uint32_t segmentSize = segmentInfo._size;

	debug_cyan("[RECOVERY] Coding Scheme = %d setting = %s\n",
			(int) codingScheme, codingSetting.c_str());

	// get block status
	vector<bool> blockStatus = _osdCommunicator->getOsdStatusRequest(
			segmentInfo._osdList);

	// obtain required blockSymbols for repair
	block_list_t blockSymbols = _codingModule->getRepairBlockSymbols(
			codingScheme, repairBlockList, blockStatus, segmentSize,
			codingSetting);

	// obtain blocks from other OSD
	vector<BlockData> repairBlockData(
			_codingModule->getNumberOfBlocks(codingScheme, codingSetting));

	// initialize map for tracking recovery
	_recoverytpRequestCount.set(requestId, blockSymbols.size());

	for (auto block : blockSymbols) {

		uint32_t blockId = block.first;
		uint32_t osdId = segmentInfo._osdList[blockId];

		vector<offset_length_t> offsetLength = block.second;

		debug_cyan(
				"[RECOVERY] Need to obtain %zu symbols in block %" PRIu32 " from OSD %" PRIu32 "\n",
				offsetLength.size(), blockId, osdId);

		_recoverytp.schedule(
				boost::bind(&Osd::retrieveRecoveryBlock, this, requestId, osdId,
						segmentId, blockId, offsetLength,
						boost::ref(repairBlockData[blockId])));

	}

	// block until all recovery blocks retrieved
	while (_recoverytpRequestCount.get(requestId) > 0) {
		usleep(10000);
	}
	_recoverytpRequestCount.erase(requestId);

	debug_cyan("[RECOVERY] Performing Repair for Segment %" PRIu64 "\n",
			segmentId);

	// perform repair
	vector<BlockData> repairedBlocks = _codingModule->repairBlocks(codingScheme,
			repairBlockList, repairBlockData, blockSymbols, segmentSize,
			codingSetting);

	debug_cyan(
			"[RECOVERY] Distributing repaired blocks for segment %" PRIu64 "\n",
			segmentId);

	uint32_t j = 0;
	for (auto repairedBlock : repairedBlocks) {
		BlockLocation blockLocation;
		blockLocation.blockId = repairedBlock.info.blockId;
		blockLocation.osdId = repairBlockOsdList[j];
		distributeBlock(segmentId, repairedBlock, blockLocation); // free-d here
		j++;
	}

	// cleanup
	for (auto block : blockSymbols) {
		uint32_t blockId = block.first;
		MemoryPool::getInstance().poolFree(repairBlockData[blockId].buf);
	}

	// TODO: repairBlockOsd fails at this point?
	// send success message to MDS
	_osdCommunicator->repairBlockAck(segmentId, repairBlockList,
			repairBlockOsdList);
}

void Osd::OsdStatUpdateRequestProcessor(uint32_t requestId, uint32_t sockfd) {
	OsdStatUpdateReplyMsg* replyMsg = new OsdStatUpdateReplyMsg(
			_osdCommunicator, sockfd, _osdId, getFreespace(), getCpuLoadavg(2));
	replyMsg->prepareProtocolMsg();
	_osdCommunicator->addMessage(replyMsg);
}

void Osd::NewOsdRegisterProcessor(uint32_t requestId, uint32_t sockfd,
		uint32_t osdId, uint32_t osdIp, uint32_t osdPort) {
	if (_osdId > osdId) {
		// Do connect
		_osdCommunicator->connectToOsd(osdIp, osdPort);
	}
}

void Osd::OnlineOsdListProcessor(uint32_t requestId, uint32_t sockfd,
		vector<struct OnlineOsd>& onlineOsdList) {

	for (uint32_t i = 0; i < onlineOsdList.size(); ++i) {
		if (_osdId > onlineOsdList[i].osdId) {
			// Do connect
			_osdCommunicator->connectToOsd(onlineOsdList[i].osdIp,
					onlineOsdList[i].osdPort);

		}
	}
}

uint32_t Osd::getCpuLoadavg(int idx) {
	double load[3];
	int ret = getloadavg(load, 3);
	if (ret < idx) {
		return (INF);
	} else {
		return ((uint32_t) (load[idx] * 100));
	}
}

uint32_t Osd::getFreespace() {
	struct statvfs64 fiData;
	if ((statvfs64(DISK_PATH, &fiData)) < 0) {
		printf("Failed to stat %s:\n", DISK_PATH);
		return 0;
	} else {
//		printf("Disk %s: \n", DISK_PATH);
//		printf("\tblock size: %u\n", fiData.f_bsize);
//		printf("\ttotal no blocks: %i\n", fiData.f_blocks);
//		printf("\tfree blocks: %i\n", fiData.f_bfree);
		return ((uint32_t) (fiData.f_bsize * fiData.f_bfree / 1024 / 1024));
	}
}

OsdCommunicator* Osd::getCommunicator() {
	return _osdCommunicator;
}

StorageModule* Osd::getStorageModule() {
	return _storageModule;
}

uint32_t Osd::getOsdId() {
	return _osdId;
}

/*
 void Osd::setOsdListStatus(vector<bool> &secondaryOsdStatus) {
 for (auto osdStatus : secondaryOsdStatus) {
 osdStatus = true;
 }

 // failure simulation
 //secondaryOsdStatus[0] = false;
 //secondaryOsdStatus[1] = false;
 }
 */
