/**
 * codingmodule.hh
 */

#ifndef __CODINGMODULE_HH__
#define __CODINGMODULE_HH__
#include <map>
#include <vector>
#include <stdint.h>
#include "../coding/coding.hh"
#include "../common/segmentdata.hh"
#include "../common/objectdata.hh"
#include "../common/enums.hh"

struct CodingSetting {
	CodingScheme codingScheme;
	string setting;
};

class CodingModule {
public:

	CodingModule();

	/**
	 * Encode an object to a list of segments
	 * @param objectData ObjectData structure
	 * @param setting for the coding scheme
	 * @return A list of SegmentData structure
	 */

	vector<struct SegmentData> encodeObjectToSegment(CodingScheme codingScheme,
			struct ObjectData objectData, string setting);

	/**
	 * Encode an object to a list of segments
	 * @param objectId Object ID
	 * @param buf Pointer to buffer holding object data
	 * @param length length of the object
	 * @return A list of SegmentData structure
	 */

	vector<struct SegmentData> encodeObjectToSegment(CodingScheme codingScheme,
			uint64_t objectId, char* buf, uint64_t length, string setting);

	/**
	 * Decode a list of segments into an object
	 * @param objectId Destination object ID
	 * @param segmentData a list of SegmentData structure
	 * @param requiredSegments IDs of segments that are required to do decode
	 * @return an ObjectData structure
	 */

	struct ObjectData decodeSegmentToObject(CodingScheme codingScheme,
			uint64_t objectId, vector<struct SegmentData> segmentData,
			vector<uint32_t> requiredSegments, string setting);

	/**
	 * Get the list of segments required to do decode
	 * @param codingScheme Coding Scheme
	 * @param setting Coding Setting
	 * @param secondaryOsdStatus a bool array containing the status of the OSD
	 * @return list of segment ID
	 */

	vector<uint32_t> getRequiredSegmentIds(CodingScheme codingScheme,
			string setting, vector<bool> secondaryOsdStatus);

	/**
	 * Get the number of segments that the scheme uses
	 * @param codingScheme Coding Scheme
	 * @param setting Coding Setting
	 * @return number of segments
	 */

	uint32_t getNumberOfSegments(CodingScheme codingScheme, string setting);

	/**
	 * Get the Coding object according to the codingScheme specified
	 * @param codingScheme Type of coding scheme
	 * @return The Coding object
	 */

	Coding* getCoding(CodingScheme codingScheme);

	/**
	 * Retrieve a segment from the storage module
	 * @param objectId ID of the object that the segment is belonged to
	 * @param segmentId ID of the segment to retrieve
	 * @return a SegmentData structure
	 */

private:
//	Coding* _coding;
	map<CodingScheme, Coding*> _codingWorker;
};

#endif
