/**
 * osd.hh
 */

#ifndef __OSD_HH__
#define __OSD_HH__
#include <stdint.h>
#include <vector>
#include "osd_communicator.hh"
#include "storagemodule.hh"
#include "codingmodule.hh"
#include "../common/metadata.hh"
#include "../common/segmentdata.hh"
#include "../common/blockdata.hh"
#include "../common/blocklocation.hh"
#include "../common/onlineosd.hh"
#include "../protocol/message.hh"
#include "../datastructure/concurrentmap.hh"

/**
 * Central class of OSD
 * All functions of OSD are invoked here
 * Segments and Blocks can be divided into trunks for transportation
 */

/**
 * Message Functions
 *
 * UPLOAD
 * 1. putSegmentProcessor
 * 2. segmentTrunkProcessor 	-> getOsdListRequest (MONITOR)
 * 									-> osdListProcessor
 * 							-> sendBlockToOsd
 * 3. (other OSD) putBlockProcessor
 * 4. (other OSD) blockTrunkProcessor
 * 5. sendBlockAck (PRIMARY OSD, CLIENT, MDS)
 *
 * DOWNLOAD
 * 1. getSegmentProcessor 	-> getOsdListRequest (MDS)
 * 									-> osdListProcessor
 * 2. getBlockRequest
 * 			-> (other OSD) getBlockProcessor
 * 			-> (other OSD) sendBlockToOsd
 * 			-> putBlockProcessor
 * 			-> blockTrunkProcessor
 * 	3. sendSegmentToClient
 */

class Osd {
public:

	/**
	 * Constructor
	 */

	Osd(uint32_t selfId);

	/**
	 * Destructor
	 */

	~Osd();

	/**
	 * Action when an OSD list is received
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param segmentId 	Segment ID
	 * @param osdList 	Secondary OSD List
	 * @return Length of list if success, -1 if failure
	 */

	uint32_t osdListProcessor(uint32_t requestId, uint32_t sockfd,
			uint64_t segmentId, vector<struct BlockLocation> osdList);

	// DOWNLOAD

	/**
	 * Action when a getSegmentRequest is received
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param segmentId 	ID of the segment to send
	 * @param localRetrieve (Optional) Retrieve the segment and store in cache
	 */

	void getSegmentRequestProcessor(uint32_t requestId, uint32_t sockfd,
			uint64_t segmentId, bool localRetrieve = false);

	/**
	 * Action when a getBlockRequest is received
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param segmentId Segment ID
	 * @param blockId Block ID
	 * @param symbols List of symbols to retrieve
	 */

	void getBlockRequestProcessor(uint32_t requestId, uint32_t sockfd,
			uint64_t segmentId, uint32_t blockId,
			vector<offset_length_t> symbols);

	/**
	 * Action when a getRecoveryBlockRequest is received
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param segmentId Segment ID
	 * @param blockId Block ID
	 * @param symbols List of symbols to retrieve
	 */

	void getRecoveryBlockProcessor(uint32_t requestId, uint32_t sockfd,
			uint64_t segmentId, uint32_t blockId,
			vector<offset_length_t> symbols);

	/**
	 * Action when a recoveryBlockData is received
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param segmentId Segment ID
	 * @param blockId Block ID
	 * @param length Length of recovery block data
	 * @param buf Pointer to buffer
	 * @param waitOnRequestId new request ID that this message should wait for reply
	 */

	void recoveryBlockDataProcessor(uint32_t requestId, uint32_t sockfd,
			uint64_t segmentId, uint32_t blockId, uint32_t length, char* buf,
			uint32_t waitOnRequestId);

	/**
	 * Get a recovery block from another OSD
	 * @param requestId Request ID
	 * @param osdId ID of the OSD to retrieve from
	 * @param segmentId Segment ID
	 * @param blockId Block ID
	 * @param offsetLength List of <offset, length> in the target block
	 * @param repairedBlock Referenced memory which the recovered block is stored
	 */
	void retrieveRecoveryBlock(uint32_t requestId, uint32_t osdId,
			uint64_t segmentId, uint32_t blockId,
			vector<offset_length_t> &offsetLength, BlockData &repairedBlock);

	/**
	 * Action when a put segment request is received
	 * A number of trunks are expected to receive afterwards
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param segmentId Segment ID
	 * @param length Segment size, equals the total length of all the trunks
	 * @param chunkCount number of chunks that will be received
	 * @param codingScheme Coding Scheme for the segment
	 * @param setting Coding setting for the segment
	 * @param checksum Checksum of the segment
	 */

	void putSegmentInitProcessor(uint32_t requestId, uint32_t sockfd,
			uint64_t segmentId, uint32_t length, uint32_t chunkCount,
			CodingScheme codingScheme, string setting, string checksum);

	/**
	 * Action when a put segment end is received
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param segmentId Segment ID
	 */

	void putSegmentEndProcessor(uint32_t requestId, uint32_t sockfd,
			uint64_t segmentId);

	/**
	 * Action when an segment trunk is received
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param segmentId Segment ID
	 * @param offset Offset of the trunk in the segment
	 * @param length Length of trunk
	 * @param buf Pointer to buffer
	 * @return Length of trunk if success, -1 if failure
	 */

	uint32_t putSegmentDataProcessor(uint32_t requestId, uint32_t sockfd,
			uint64_t segmentId, uint64_t offset, uint32_t length, char* buf);

	/**
	 * Action when a putBlockInitRequest is received
	 * A number of trunks are expected to be received afterwards
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param segmentId Segment ID
	 * @param blockId Block ID
	 * @param length Block size, equals the total length of all the trunks
	 * @param chunkCount No of trunks to receive
	 */

	void putBlockInitProcessor(uint32_t requestId, uint32_t sockfd,
			uint64_t segmentId, uint32_t blockId, uint32_t length,
			uint32_t chunkCount);

	/**
	 * Distribute Blocks to OSD
	 *
	 * @param segmentId	Segment Id
	 * @param blockData	Data Block
	 * @param blockLocation Location of Block
	 * @param blocktpRequestId The requestId of the message in the thread pool
	 */
	void distributeBlock(uint64_t segmentId, const struct BlockData& blockData,
			const struct BlockLocation& blockLocation,
			uint32_t blocktpRequestId = 0);

	/**
	 * Action when a block trunk is received
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param segmentId Segment ID
	 * @param blockId Block ID
	 * @param offset Offset of the trunk in the block
	 * @param length Length of trunk
	 * @param buf Pointer to buffer
	 * @return Length of trunk if success, -1 if failure
	 */

	uint32_t putBlockDataProcessor(uint32_t requestId, uint32_t sockfd,
			uint64_t segmentId, uint32_t blockId, uint32_t offset,
			uint32_t length, char* buf);

	/**
	 * Action when a put block end is received
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param segmentId Segment ID
	 * @param blockId Block ID
	 */

	void putBlockEndProcessor(uint32_t requestId, uint32_t sockfd,
			uint64_t segmentId, uint32_t blockId);

	/**
	 * Action when a recovery request is received
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param segmentId Segment ID
	 * @param repairBlockId Block ID to be repaired
	 * @param repairBlockOsd New OSDs to store the repaired blocks
	 */

	void repairSegmentInfoProcessor(uint32_t requestId, uint32_t sockfd,
			uint64_t segmentId, vector<uint32_t> repairBlockId,
			vector<uint32_t> repairBlockOsd);

	/**
	 * Action when a monitor requests a status update 
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 */
	void OsdStatUpdateRequestProcessor(uint32_t requestId, uint32_t sockfd);

	/**
	 * Action when a monitor tells a new osd is startup,to connect it if my id > its id 
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param osdId the id of the newly startup osd
	 * @param osdIp the ip addr of the newly startup osd
	 * @param osdPort the port of the newly startup osd
	 */
	void NewOsdRegisterProcessor(uint32_t requestId, uint32_t sockfd,
			uint32_t osdId, uint32_t osdIp, uint32_t osdPort);

	/**
	 * Action when a monitor flush the online list to a osd 
	 * @param requestId Request ID
	 * @param sockfd Socket descriptor of message source
	 * @param onlineOsdList list reference contains all the online osd with its<ip, port, id>
	 */
	void OnlineOsdListProcessor(uint32_t requestId, uint32_t sockfd,
			vector<struct OnlineOsd>& onlineOsdList);

	// getters

	/** 
	 * To get the current cpu load average in last 15 mins. If error, return
	 * infinity INF = (1<<29)
	 * @param idx 0 to get last one minute, 1 to get last 5 mins, 2 to get last 15 mins
	 * @return loading*100 to cast into integer
	 */
	uint32_t getCpuLoadavg(int idx);

	/**
	 * To get the free space of the current disk in MB
	 * @return free space in MB, if error, return 0
	 */
	uint32_t getFreespace();

	/**
	 * Get a reference of OSDCommunicator
	 * @return Pointer to OSD communication module
	 */

	OsdCommunicator* getCommunicator();

	/**
	 * Get a reference of StorageModule
	 * @return Pointer to OSD storage module
	 */

	StorageModule* getStorageModule();

	/**
	 * Get a reference of OSD Cache
	 * @return Pointer to OSD block location cache
	 */
	//BlockLocationCache* getBlockLocationCache();
	/**
	 * Get the ID
	 * @return OSD ID
	 */

	uint32_t getOsdId();

	/**
	 * If block is not requested, return false and set status to true
	 * If block is requested, return true
	 * @param segmentId Segment ID
	 * @param blockId Block ID
	 * @return is block requested
	 */

	bool isBlockRequested(uint64_t segmentId, uint32_t blockId);

private:

	/**
	 * Set the bool array containing the status of the OSDs holding the blocks
	 * @param osdListStatus Bool array representing OSD status (true = up, false = down)
	 */

	//	void setOsdListStatus (vector<bool> &secondaryOsdStatus);
	/**
	 * Retrieve a block from the storage
	 * @param segmentId ID of the segment that the block is belonged to
	 * @param blockId Target Block ID
	 * @return BlockData structure
	 */

	struct BlockData getBlockFromStroage(uint64_t segmentId, uint32_t blockId);

	/**
	 * Save a block to storage
	 * @param blockData a BlockData structure
	 * @return Length of block if success, -1 if failure
	 */

	uint32_t saveBlockToStorage(BlockData blockData);

	/**
	 * Perform degraded read of an segment
	 * @param segmentId ID of the segment to read
	 * @return an SegmentData structure
	 */

	struct SegmentData degradedRead(uint64_t segmentId);

	/**
	 * Cache segment to disk cache
	 * @param segmentId Segment ID
	 * @param segmentData Segment Data structure
	 */

	void cacheSegment(uint64_t segmentId, SegmentData segmentData);

	/**
	 * Free segmentData
	 * @param segmentId Segment ID
	 * @param segmentData Segment Data structure
	 */

	void freeSegment(uint64_t segmentId, SegmentData segmentData);

	/**
	 * Stores the list of OSDs that store a certain block
	 */

	//BlockLocationCache* _blockLocationCache;
	/**
	 * Handles communication with other components
	 */

	OsdCommunicator* _osdCommunicator;

	/**
	 * Handles the storage layer
	 */

	StorageModule* _storageModule;

	/**
	 * Handles coding and decoding
	 */

	CodingModule* _codingModule;

	//	Coding _cunit; // encode & decode done here
	uint32_t _osdId;

	// upload
	ConcurrentMap<uint64_t, uint32_t> _pendingSegmentChunk;
	ConcurrentMap<uint64_t, struct CodingSetting> _codingSettingMap;
	ConcurrentMap<uint64_t, string> _checksumMap;

	// download
	ConcurrentMap<uint32_t, uint32_t> _blocktpRequestCount;

	ConcurrentMap<uint64_t, vector<struct BlockData>> _receivedBlockData;
	ConcurrentMap<uint64_t, uint32_t> _downloadBlockRemaining;
	ConcurrentMap<uint64_t, uint32_t> _segmentRequestCount;
	ConcurrentMap<uint64_t, mutex*> _segmentDownloadMutex;
	ConcurrentMap<uint64_t, SegmentData> _segmentDataMap;
	ConcurrentMap<uint64_t, bool> _isSegmentDownloaded;

	// recovery
	ConcurrentMap<uint32_t, uint32_t> _recoverytpRequestCount;

	// upload / download
	ConcurrentMap<string, uint32_t> _pendingBlockChunk;
};
#endif
