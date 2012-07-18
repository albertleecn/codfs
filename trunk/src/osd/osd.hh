/**
 * osd.hh
 */

#ifndef __OSD_HH__
#define __OSD_HH__
#include <stdint.h>
#include <vector>
#include "../common/metadata.hh"
#include "../protocol/message.hh"
#include "osd_communicator.hh"
#include "objectdata.hh"
#include "segmentdata.hh"
#include "segmentlocationcache.hh"
#include "storagemodule.hh"

/**
 * Central class of OSD
 * All functions of OSD are invoked here
 * Objects and Segments can be divided into trunks for transportation
 */

class Osd {
public:

	/**
	 * Constructor
	 */

	Osd();

	/**
	 * Destructor
	 */
	~Osd();

	/**
	 * Action when an OSD list is received
	 * @param objectId 	Object ID
	 * @param osdList 	Secondary OSD List
	 * @return Length of list if success, -1 if failure
	 */

	uint32_t osdListHandler(uint64_t objectId, list<uint32_t> osdList);

	/**
	 * Action when a getObjectRequest is received
	 * @param objectId 	ID of the object to send
	 * @param sockfd	Socket Descriptor of the destination
	 * @return 0 if success, -1 if failure
	 */

	uint32_t getObjectHandler(uint64_t objectId, uint32_t sockfd);

	/**
	 * Action when a getSegmentRequest is received
	 * @param objectId 	ID of Object that the segment is belonged to
	 * @param segmentId ID of the segment to send
	 * @return 0 if success, -1 if failure
	 */

	uint32_t getSegmentHandler(uint64_t objectId, uint32_t segmentId);

	/**
	 * Action when an object trunk is received
	 * @param objectId Object ID
	 * @param offset Offset of the trunk in the object
	 * @param length Length of trunk
	 * @param buf Pointer to buffer
	 * @return Length of trunk if success, -1 if failure
	 */

	uint32_t objectTrunkHandler(uint64_t objectId, uint32_t offset,
			uint32_t length, char* buf);

	/**
	 * Action when a segment trunk is received
	 * @param objectId Object ID
	 * @param segmentId Segment ID
	 * @param offset Offset of the trunk in the segment
	 * @param length Length of trunk
	 * @param buf Pointer to buffer
	 * @return Length of trunk if success, -1 if failure
	 */

	uint32_t segmentTrunkHandler(uint64_t objectId, uint32_t segmentId,
			uint32_t offset, uint32_t length, vector<unsigned char> buf);

	/**
	 * Action when a recovery request is received
	 * @return 0 if success, -1 if failure
	 */

	uint32_t recoveryHandler();

	// getters

	/**
	 * Get a reference of OSDCommunicator
	 * @return Pointer to OSD communication module
	 */

	OsdCommunicator* getOsdCommunicator();

	/**
	 * Get a reference of OSD Cache
	 * @return Pointer to OSD segment location cache
	 */
	SegmentLocationCache* getSegmentLocationCache();

private:

	/**
	 * Encode an object to a list of segments
	 * @param objectData
	 * @return A list of SegmentData structure
	 */

	list<struct SegmentData> encodeObjectToSegment(
			struct ObjectData objectData);

	/**
	 * Decode a list of segments into an object
	 * @param objectId Destination object ID
	 * @param segmentData a list of SegmentData structure
	 * @return an ObjectData structure
	 */

	struct ObjectData decodeSegmentToObject(uint64_t objectId,
			list<struct SegmentData> segmentData);

	/**
	 * Send a request to get a segment to other OSD
	 * @param objectId ID of the object that the segment is belonged to
	 * @param segmentId
	 * @return 0 if success, -1 if failure
	 */

	uint32_t getSegmentRequest(uint64_t objectId, uint32_t segmentId);

	/**
	 * Send a request to get the secondary OSD list of an object from MDS/Monitor
	 * @param objectId Object ID for query
	 * @return 0 if success, -1 if failure
	 */

	uint32_t getSecOsdListRequest(uint64_t objectId);

	/**
	 * Retrieve a segment from the storage module
	 * @param objectId ID of the object that the segment is belonged to
	 * @param segmentId ID of the segment to retrieve
	 * @return a SegmentData structure
	 */

	struct SegmentData getSegmentFromStroage(uint64_t objectId,
			uint32_t segmentId);

	/**
	 * Send a segment to another OSD
	 * @param segmentData a SegmentData structure
	 * @param osdId ID of the destination OSD
	 * @return 0 if success, -1 if failure
	 */

	uint32_t sendSegmentToOsd(struct SegmentData segmentData, uint32_t osdId);

	/**
	 * Send an object to a client
	 * @param objectData an objectData structure
	 * @param clientId ID of the destination client
	 * @return 0 if success, -1 if failure
	 */

	uint32_t sendObjectToClient(struct ObjectData objectData,
			uint32_t clientId);

	/**
	 * Save a segment to storage
	 * @param segmentData a SegmentData structure
	 * @return Length of segment if success, -1 if failure
	 */

	uint32_t saveSegmentToStorage(SegmentData segmentData);

	/**
	 * Perform degraded read of an object
	 * @param objectId ID of the object to read
	 * @return an ObjectData structure
	 */

	struct ObjectData degradedRead(uint64_t objectId);

	/**
	 * Send a failure report to MDS / Monitor
	 * @param osdId ID of the OSD that failed
	 * @return 0 if success, -1 if failure
	 */

	uint32_t reportOsdFailure(uint32_t osdId);

	/**
	 * Stores the list of OSDs that store a certain segment
	 */

	SegmentLocationCache* _segmentLocationCache;

	/**
	 * Handles communication with other components
	 */

	OsdCommunicator* _osdCommunicator;

	/**
	 * Handles the storage layer
	 */

	StorageModule* _storageModule;

//	Coding _cunit; // encode & decode done here
//	OsdInfo _info;

};
#endif
