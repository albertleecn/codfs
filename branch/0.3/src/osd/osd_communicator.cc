/**
 * osd_communicator.cc
 */

#include <iostream>
#include <cstdio>
#include "osd.hh"
#include "osd_communicator.hh"
#include "../common/enums.hh"
#include "../common/memorypool.hh"
#include "../common/debug.hh"
#include "../common/blockdata.hh"
#include "../common/segmentdata.hh"
#include "../common/metadata.hh"
#include "../protocol/metadata/uploadsegmentack.hh"
#include "../protocol/metadata/listdirectoryrequest.hh"
#include "../protocol/metadata/getsegmentinforequest.hh"
#include "../protocol/metadata/cachesegmentreply.hh"
#include "../protocol/transfer/putsegmentinitreply.hh"
#include "../protocol/transfer/getblockinitrequest.hh"
#include "../protocol/transfer/putblockinitrequest.hh"
#include "../protocol/transfer/putblockinitreply.hh"
#include "../protocol/transfer/segmenttransferendreply.hh"
#include "../protocol/transfer/blocktransferendrequest.hh"
#include "../protocol/transfer/blocktransferendreply.hh"
#include "../protocol/transfer/blockdatamsg.hh"
#include "../protocol/transfer/recoveryblockdatamsg.hh"
#include "../protocol/nodelist/getsecondarylistrequest.hh"
#include "../protocol/status/osdstartupmsg.hh"
#include "../protocol/status/getosdstatusrequestmsg.hh"
#include "../protocol/status/repairsegmentinfomsg.hh"

using namespace std;

extern Osd* osd;

/**
 * Constructor
 */

OsdCommunicator::OsdCommunicator() {

}

/**
 * Destructor
 */

OsdCommunicator::~OsdCommunicator() {

}

void OsdCommunicator::replyPutSegmentInit(uint32_t requestId,
		uint32_t connectionId, uint64_t segmentId) {

	PutSegmentInitReplyMsg* putSegmentInitReplyMsg = new PutSegmentInitReplyMsg(
			this, requestId, connectionId, segmentId);
	putSegmentInitReplyMsg->prepareProtocolMsg();

	addMessage(putSegmentInitReplyMsg);
}

void OsdCommunicator::replyPutBlockInit(uint32_t requestId,
		uint32_t connectionId, uint64_t segmentId, uint32_t blockId) {

	PutBlockInitReplyMsg* putBlockInitReplyMsg = new PutBlockInitReplyMsg(this,
			requestId, connectionId, segmentId, blockId);
	putBlockInitReplyMsg->prepareProtocolMsg();

	addMessage(putBlockInitReplyMsg);
}

void OsdCommunicator::replyPutSegmentEnd(uint32_t requestId,
		uint32_t connectionId, uint64_t segmentId) {

	SegmentTransferEndReplyMsg* putSegmentEndReplyMsg =
			new SegmentTransferEndReplyMsg(this, requestId, connectionId,
					segmentId);
	putSegmentEndReplyMsg->prepareProtocolMsg();

	addMessage(putSegmentEndReplyMsg);
}

void OsdCommunicator::replyCacheSegment(uint32_t requestId,
		uint32_t connectionId, uint64_t segmentId) {

	CacheSegmentReplyMsg* cacheSegmentReplyMsg = new CacheSegmentReplyMsg(this,
			requestId, connectionId, segmentId);
	cacheSegmentReplyMsg->prepareProtocolMsg();

	addMessage(cacheSegmentReplyMsg);
}

void OsdCommunicator::replyPutBlockEnd(uint32_t requestId,
		uint32_t connectionId, uint64_t segmentId, uint32_t blockId,
		uint32_t waitOnRequestId) {

	uint32_t msgRequestId = 0;
	if (waitOnRequestId == 0) {
		msgRequestId = requestId;
	} else {
		msgRequestId = waitOnRequestId;
	}

	BlockTransferEndReplyMsg* blockTransferEndReplyMsg =
			new BlockTransferEndReplyMsg(this, msgRequestId, connectionId,
					segmentId, blockId);
	blockTransferEndReplyMsg->prepareProtocolMsg();

	addMessage(blockTransferEndReplyMsg);

}

uint32_t OsdCommunicator::reportOsdFailure(uint32_t osdId) {
	return 0;
}

uint32_t OsdCommunicator::sendBlock(uint32_t sockfd,
		struct BlockData blockData) {

	uint64_t segmentId = blockData.info.segmentId;
	uint32_t blockId = blockData.info.blockId;
	uint32_t length = blockData.info.blockSize;
	char* buf = blockData.buf;
	const uint32_t chunkCount = ((length - 1) / _chunkSize) + 1;

	// step 1: send init message, wait for ack
	debug("Put Block Init to FD = %" PRIu32 "\n", sockfd);
	putBlockInit(sockfd, segmentId, blockId, length, chunkCount);
	debug("Put Block Init ACK-ed from FD = %" PRIu32 "\n", sockfd);

#ifdef SERIALIZE_DATA_QUEUE
	lockDataQueue(sockfd);
#endif
	// step 2: send data

	uint64_t byteToSend = 0;
	uint64_t byteProcessed = 0;
	uint64_t byteRemaining = length;

	while (byteProcessed < length) {

		if (byteRemaining > _chunkSize) {
			byteToSend = _chunkSize;
		} else {
			byteToSend = byteRemaining;
		}

		putBlockData(sockfd, segmentId, blockId, buf, byteProcessed,
				byteToSend);
		byteProcessed += byteToSend;
		byteRemaining -= byteToSend;

	}
#ifdef SERIALIZE_DATA_QUEUE
	unlockDataQueue(sockfd);
#endif

	// Step 3: Send End message

	putBlockEnd(sockfd, segmentId, blockId);

	cout << "Put Block ID = " << segmentId << "." << blockId << " Finished"
			<< endl;

	return 0;
}

uint32_t OsdCommunicator::sendRecoveryBlock(uint32_t requestId, uint32_t sockfd,
		struct BlockData blockData) {

	uint64_t segmentId = blockData.info.segmentId;
	uint32_t blockId = blockData.info.blockId;
	uint32_t length = blockData.info.blockSize;
	char* buf = blockData.buf;

	uint32_t newRequestId = generateRequestId();

	RecoveryBlockDataMsg* recoveryBlockDataMsg = new RecoveryBlockDataMsg(this,
			requestId, sockfd, segmentId, blockId, length, newRequestId);

	recoveryBlockDataMsg->prepareProtocolMsg();
	recoveryBlockDataMsg->preparePayload(buf, length);

	addMessage(recoveryBlockDataMsg, true, newRequestId);
	MessageStatus status = recoveryBlockDataMsg->waitForStatusChange();

	if (status == READY) {
		debug("sendRecoveryBlock wait on %" PRIu32 "READY\n", newRequestId);
		return 0;
	}

	return 0;
}

void OsdCommunicator::getBlockRequest(uint32_t osdId, uint64_t segmentId,
		uint32_t blockId, vector<offset_length_t> symbols) {

	uint32_t dstSockfd = getSockfdFromId(osdId);
	GetBlockInitRequestMsg* getBlockInitRequestMsg = new GetBlockInitRequestMsg(
			this, dstSockfd, segmentId, blockId, symbols);
	getBlockInitRequestMsg->prepareProtocolMsg();

	addMessage(getBlockInitRequestMsg, false);

}

BlockData OsdCommunicator::getRecoveryBlock(uint32_t osdId, uint64_t segmentId,
		uint32_t blockId, vector<offset_length_t> symbols) {

	uint32_t dstSockfd = getSockfdFromId(osdId);
	GetBlockInitRequestMsg* getRecoveryBlockRequestMsg =
			new GetBlockInitRequestMsg(this, dstSockfd, segmentId, blockId,
					symbols, true); // is recovery
	getRecoveryBlockRequestMsg->prepareProtocolMsg();

	addMessage(getRecoveryBlockRequestMsg, true);
	MessageStatus status = getRecoveryBlockRequestMsg->waitForStatusChange();

	if (status == READY) {
		return getRecoveryBlockRequestMsg->getRecoveryBlockData();
	}

	debug_error(
			"ERROR: getRecoveryBlockData segmentId = %" PRIu64 " blockId = %" PRIu32 "\n",
			segmentId, blockId);
	return {};

}

vector<struct BlockLocation> OsdCommunicator::getOsdListRequest(
		uint64_t segmentId, ComponentType dstComponent, uint32_t blockCount,
		uint32_t primaryId, uint64_t blockSize) {

	GetSecondaryListRequestMsg* getSecondaryListRequestMsg =
			new GetSecondaryListRequestMsg(this, getMonitorSockfd(), blockCount,
					primaryId, blockSize);
	getSecondaryListRequestMsg->prepareProtocolMsg();

	addMessage(getSecondaryListRequestMsg, true);
	MessageStatus status = getSecondaryListRequestMsg->waitForStatusChange();

	if (status == READY) {
		vector<struct BlockLocation> osdList =
				getSecondaryListRequestMsg->getSecondaryList();
		return osdList;
	}

	return {};
}

vector<bool> OsdCommunicator::getOsdStatusRequest(vector<uint32_t> osdIdList) {

	GetOsdStatusRequestMsg* getOsdStatusRequestMsg = new GetOsdStatusRequestMsg(
			this, getMonitorSockfd(), osdIdList);
	getOsdStatusRequestMsg->prepareProtocolMsg();

	addMessage(getOsdStatusRequestMsg, true);
	MessageStatus status = getOsdStatusRequestMsg->waitForStatusChange();

	if (status == READY) {
		vector<bool> osdStatusList = getOsdStatusRequestMsg->getOsdStatus();
		return osdStatusList;
	}

	return {};
}

uint32_t OsdCommunicator::sendBlockAck(uint64_t segmentId, uint32_t blockId,
		ComponentType dstComponent) {
	return 0;
}

//
// PRIVATE FUNCTIONS
//

void OsdCommunicator::putBlockInit(uint32_t sockfd, uint64_t segmentId,
		uint32_t blockId, uint32_t length, uint32_t chunkCount) {

	// Step 1 of the upload process

	PutBlockInitRequestMsg* putBlockInitRequestMsg = new PutBlockInitRequestMsg(
			this, sockfd, segmentId, blockId, length, chunkCount);

	putBlockInitRequestMsg->prepareProtocolMsg();
	addMessage(putBlockInitRequestMsg, true);

	MessageStatus status = putBlockInitRequestMsg->waitForStatusChange();
	if (status == READY) {
		waitAndDelete(putBlockInitRequestMsg);
		return;
	} else {
		debug_error("Put Block Init Failed %" PRIu64 ".%" PRIu32 "\n",
				segmentId, blockId);
		exit(-1);
	}

}

void OsdCommunicator::putBlockData(uint32_t sockfd, uint64_t segmentId,
		uint32_t blockId, char* buf, uint64_t offset, uint32_t length) {

	// Step 2 of the upload process
	BlockDataMsg* blockDataMsg = new BlockDataMsg(this, sockfd, segmentId,
			blockId, offset, length);

	blockDataMsg->prepareProtocolMsg();
	blockDataMsg->preparePayload(buf + offset, length);

	addMessage(blockDataMsg, false);
}

void OsdCommunicator::putBlockEnd(uint32_t sockfd, uint64_t segmentId,
		uint32_t blockId) {

	// Step 3 of the upload process

	BlockTransferEndRequestMsg* blockTransferEndRequestMsg =
			new BlockTransferEndRequestMsg(this, sockfd, segmentId, blockId);

	blockTransferEndRequestMsg->prepareProtocolMsg();
	addMessage(blockTransferEndRequestMsg, true);

	MessageStatus status = blockTransferEndRequestMsg->waitForStatusChange();
	if (status == READY) {
		waitAndDelete(blockTransferEndRequestMsg);
		return;
	} else {
		debug_error("Block Transfer End Failed %" PRIu64 ".%" PRIu32 "\n",
				segmentId, blockId);
		exit(-1);
	}
}

void OsdCommunicator::segmentUploadAck(uint64_t segmentId, uint32_t segmentSize,
		CodingScheme codingScheme, string codingSetting,
		vector<uint32_t> nodeList, string checksum) {
	uint32_t mdsSockFd = getMdsSockfd();

	UploadSegmentAckMsg* uploadSegmentAckMsg = new UploadSegmentAckMsg(this,
			mdsSockFd, segmentId, segmentSize, codingScheme, codingSetting,
			nodeList, checksum);

	uploadSegmentAckMsg->prepareProtocolMsg();
	addMessage(uploadSegmentAckMsg, false);

	/*
	 addMessage(segmentUploadAckRequestMsg, true);

	 MessageStatus status = segmentUploadAckRequestMsg->waitForStatusChange();
	 if(status == READY) {
	 waitAndDelete(segmentUploadAckMsg);
	 return ;
	 } else {
	 debug_error("Segment Upload Ack Failed [%" PRIu64 "]\n", segmentId);
	 exit(-1);
	 }
	 */
}

// DOWNLOAD

struct SegmentTransferOsdInfo OsdCommunicator::getSegmentInfoRequest(
		uint64_t segmentId) {

	struct SegmentTransferOsdInfo segmentInfo = { };
	uint32_t mdsSockFd = getMdsSockfd();

	GetSegmentInfoRequestMsg* getSegmentInfoRequestMsg =
			new GetSegmentInfoRequestMsg(this, mdsSockFd, segmentId);
	getSegmentInfoRequestMsg->prepareProtocolMsg();
	addMessage(getSegmentInfoRequestMsg, true);

	MessageStatus status = getSegmentInfoRequestMsg->waitForStatusChange();
	if (status == READY) {
		segmentInfo._id = segmentId;
		segmentInfo._size = getSegmentInfoRequestMsg->getSegmentSize();
		segmentInfo._codingScheme = getSegmentInfoRequestMsg->getCodingScheme();
		segmentInfo._codingSetting =
				getSegmentInfoRequestMsg->getCodingSetting();
		segmentInfo._checksum = getSegmentInfoRequestMsg->getChecksum();
		segmentInfo._osdList = getSegmentInfoRequestMsg->getNodeList();
		waitAndDelete(getSegmentInfoRequestMsg);
	} else {
		debug("Get Segment Info Request Failed %" PRIu64 "\n", segmentId);
		exit(-1);
	}

	return segmentInfo;
}

void OsdCommunicator::registerToMonitor(uint32_t ip, uint16_t port) {
	OsdStartupMsg* startupMsg = new OsdStartupMsg(this, getMonitorSockfd(),
			osd->getOsdId(), osd->getFreespace(), osd->getCpuLoadavg(0), ip,
			port);
	startupMsg->prepareProtocolMsg();
	addMessage(startupMsg);
}

void OsdCommunicator::repairBlockAck(uint64_t segmentId,
		vector<uint32_t> repairBlockList, vector<uint32_t> repairBlockOsdList) {

	RepairSegmentInfoMsg * repairSegmentInfoMsg = new RepairSegmentInfoMsg(this,
			getMdsSockfd(), segmentId, repairBlockList, repairBlockOsdList);
	repairSegmentInfoMsg->prepareProtocolMsg();
	addMessage(repairSegmentInfoMsg);
}
