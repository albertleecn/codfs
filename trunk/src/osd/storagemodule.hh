#ifndef __STORAGEMODULE_HH__
#define __STORAGEMODULE_HH__

#include <string>
#include <stdint.h>
#include <map>
#include <stdio.h>
#include "../config/config.hh"
#include "../common/memorypool.hh"
#include "../common/segmentdata.hh"
#include "../common/objectdata.hh"
using namespace std;

/**
 * For caching an object in memory during upload/download
 */

struct ObjectCache {
	uint64_t length;
	char* buf;
};

class StorageModule {
public:

	/**
	 * Constructor
	 */

	StorageModule();

	/**
	 * Destructor
	 */

	~StorageModule();

	/**
	 * Check if the object exists in the storage
	 * @param objectId Object ID
	 * @return true if object exists, false otherwise
	 */

	bool isObjectExist(uint64_t objectId);

	/**
	 * Create and open the file for storing the object on disk
	 * Also creates an ObjectCache for downloading the object
	 * @param objectId Object ID
	 * @param length Length of object
	 */

	void createObject(uint64_t objectId, uint32_t length);

	/**
	 * Create and open the file for storing the segment on disk
	 * @param objectId Object ID
	 * @param segmentId Segment ID
	 * @param length Length of segment
	 */

	void createSegment(uint64_t objectId, uint32_t segmentId, uint32_t length);

	/**
	 * Read a part of an object from the storage
	 * @param objectId ObjectID
	 * @param offsetInObject Number of bytes to skip (default 0)
	 * @param length Number of bytes to read (read whole object if 0)
	 * @return ObjectData structure
	 */

	struct ObjectData readObject(uint64_t objectId, uint64_t offsetInObject = 0,
			uint32_t length = 0);

	/**
	 * Read a part of a segment from the storage
	 * @param objectId Object ID
	 * @param segmentId Segment ID
	 * @param offsetInSegment Number of bytes to skip (default 0)
	 * @param length Number of bytes to read (read whole segment if 0)
	 * @return SegmentData structure
	 */

	struct SegmentData readSegment(uint64_t objectId, uint32_t segmentId,
			uint64_t offsetInSegment = 0, uint32_t length = 0);

	/**
	 * Write a partial object to the storage
	 * @param objectId Object ID
	 * @param buf Pointer to buffer containing data
	 * @param offsetInObject Offset of the trunk in the object
	 * @param length Number of bytes of the trunk
	 * @return Number of bytes written
	 */

	uint32_t writeObject(uint64_t objectId, char* buf, uint64_t offsetInObject,
			uint32_t length);

	/**
	 * Write a buffer to the ObjectCache of the object
	 * @param objectId Object ID
	 * @param buf Pointer to buffer
	 * @param offsetInObject No of bytes to skip in the cache
	 * @param length No of bytes to write to the cache
	 * @return No of bytes written
	 */

	uint32_t writeObjectCache(uint64_t objectId, char* buf,
			uint64_t offsetInObject, uint32_t length);

	/**
	 * Write a partial Segment ID to the storage
	 * @param objectId Object ID
	 * @param segmentId Segment ID
	 * @param buf Pointer to buffer containing data
	 * @param offsetInObject Offset of the trunk in the segment
	 * @param length Number of bytes of the trunk
	 * @return Number of bytes written
	 */

	uint32_t writeSegment(uint64_t objectId, uint32_t segmentId, char* buf,
			uint64_t offsetInSegment, uint32_t length);

	/**
	 * Create and prepare an object in the storage before writing
	 * @param objectId Object ID
	 * @param length Number of bytes the object will take
	 */

	FILE* createAndOpenObject(uint64_t objectId, uint32_t length);

	/**
	 * Create an prepare a segment in the storage before writing
	 * @param objectId Object ID
	 * @param segmentId Segment ID
	 * @param length Number of bytes the segment will take
	 */

	FILE* createAndOpenSegment(uint64_t objectId, uint32_t segmentId,
			uint32_t length);

	/**
	 * Close the object after the transfer is finished
	 * @param objectId Object ID
	 */

	void closeObject(uint64_t objectId);

	/**
	 * Close the segment after the transfer is finished
	 * @param objectId Object ID
	 * @param segmentId Segment ID
	 */

	void closeSegment(uint64_t objectId, uint32_t segmentId);

	// getters
	uint32_t getCapacity();
	uint32_t getFreespace();
	struct ObjectCache getObjectCache(uint64_t objectId);

	void setMaxSegmentCapacity(uint32_t max_segment);
	void setMaxObjectCache(uint32_t max_object);

	uint32_t getMaxSegmentCapacity();
	uint32_t getMaxObjectCache();

	void updateCurrentSegmentCapacity(uint32_t new_segment_size, uint32_t count);
	void updateCurrentObjectCache(uint32_t new_object_size, uint32_t count);

	uint32_t getCurrentSegmentCapacity();
	uint32_t getCurrentObjectCache();

	uint32_t getFreeSegmentSpace();
	uint32_t getFreeObjectSpace();

	bool verifySegmentSpace(uint32_t size);
	bool verifyObjectSpace(uint32_t size);


private:

	/**
	 * Write the information about an object to the database
	 * @param objectId Object ID
	 * @param objectSize Number of bytes the object takes
	 * @param filepath Location of the object in the filesystem
	 */

	void writeObjectInfo(uint64_t objectId, uint32_t objectSize,
			string filepath);

	/**
	 * Read the information about an object from the database
	 * @param objectId Object ID
	 * @return ObjectInfo structure
	 */

	struct ObjectInfo readObjectInfo(uint64_t objectId);

	/**
	 * Write the information about a segment to the database
	 * @param objectId Object ID
	 * @param segmentId Segment ID
	 * @param segmentSize Number of bytes the segment takes
	 * @param filepath Location of the segment in the filesystem
	 */

	void writeSegmentInfo(uint64_t objectId, uint32_t segmentId,
			uint32_t segmentSize, string filepath);

	/**
	 * Read the information about a segment form the database
	 * @param objectId Object ID
	 * @param segmentId Segment ID
	 * @return SegmentInfo structure
	 */

//	struct SegmentInfo readSegmentInfo(uint64_t objectId, uint32_t segmentId);

	/**
	 * Open a file and read data to buffer
	 * @param filepath Path of the file in the storage
	 * @param buf Pointer to destination buffer (already malloc-ed)
	 * @param offset Offset in the file
	 * @param length Length to read
	 * @return Number of bytes read
	 */

	uint32_t readFile(string filepath, char* buf, uint64_t offset,
			uint32_t length);

	/**
	 * Open a file and write data from buffer
	 * @param filepath Path of the file in the storage
	 * @param buf Pointer to source buffer
	 * @param offset Offset in the file
	 * @param length Length to write
	 * @return Number of bytes written
	 */

	uint32_t writeFile(string filepath, char* buf, uint64_t offset,
			uint32_t length);

	/**
	 * Return the object path given Object ID
	 * @param objectId Object ID
	 * @param objectFolder Location where objects are stored
	 * @return filepath of the object in the filesystem
	 */

	string generateObjectPath(uint64_t objectId, string objectFolder);

	/**
	 * Return the segment path given Object ID and Segment ID
	 * @param objectId Object ID
	 * @param segmentId Segment ID
	 * @param segmentFolder Location where segments are stored
	 * @return filepath of the segment in the filesystem
	 */

	string generateSegmentPath(uint64_t objectId, uint32_t segmentId,
			string segmentFolder);

	/**
	 * Create a file on disk and open it
	 * @param filepath Path of the file on storage
	 * @return Pointer to the opened file
	 */

	FILE* createFile(string filepath);

	/**
	 * Retrieve the opened file pointer if file is already open
	 * Open the file on disk if file is not already open
	 * @param filepath Path to the file on disk
	 * @return Pointer to the opened file
	 */

	FILE* openFile(string filepath);

	/**
	 * Close the file and remove it from _openedFile map
	 * @param filepath Path to the file on disk
	 */

	void closeFile(string filepath);

	/**
	 * Open the file and finds the size of it
	 * @param filepath Path to the file on disk
	 * @return Size of the file
	 */
	uint64_t getFilesize(string filepath);

	uint32_t _capacity; // total capacity of the node
	uint32_t _freespace; // remaining capacity of the node
	map<string, FILE*> _openedFile;
	map<uint64_t, struct ObjectCache> _objectCache;
	string _objectFolder;
	string _segmentFolder;

	uint32_t _maxSegmentCapacity;
	uint32_t _maxObjectCache;
	uint32_t _currentSegment;
	uint32_t _currentObject;
	uint32_t _freeSegmentSpace;
	uint32_t _freeObjectSpace;
};

#endif
