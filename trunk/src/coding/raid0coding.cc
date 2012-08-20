#include <sstream>
#include <iostream>
#include "coding.hh"
#include "raid0coding.hh"
#include "../common/segmentdata.hh"
#include "../common/objectdata.hh"
#include "../common/memorypool.hh"

using namespace std;

Raid0Coding::Raid0Coding() {

}

Raid0Coding::~Raid0Coding() {

}

uint32_t roundTo(unsigned int value, unsigned int roundTo) {
	return ((value + roundTo - 1) / roundTo) * roundTo;
}

vector<struct SegmentData> Raid0Coding::encode(struct ObjectData objectData,
		string setting) {

	vector<struct SegmentData> segmentDataList;
	const uint32_t noOfStrips = getNoOfStrips(setting);

	// calculate size of each strip
	const uint32_t stripSize = roundTo(objectData.info.objectSize, noOfStrips)
			/ noOfStrips;

	for (uint32_t i = 0; i < noOfStrips; i++) {

		struct SegmentData segmentData;
		segmentData.info.objectId = objectData.info.objectId;
		segmentData.info.segmentId = i;

		if (i == noOfStrips - 1) { // last segment
			segmentData.info.segmentSize = objectData.info.objectSize
					- i * stripSize;

		} else {
			segmentData.info.segmentSize = stripSize;
		}

		// TODO: free
		segmentData.buf = MemoryPool::getInstance().poolMalloc(stripSize);

		char* bufPos = objectData.buf + i * stripSize;

		memcpy(segmentData.buf, bufPos, segmentData.info.segmentSize);

		segmentDataList.push_back(segmentData);
	}

	return segmentDataList;
}

struct ObjectData Raid0Coding::decode(vector<struct SegmentData> segmentData,
		string setting) {

	const uint32_t segmentCount = (uint32_t) segmentData.size();
	//const uint32_t noOfStrips = getNoOfStrips(setting);

	struct ObjectData objectData;

	// copy objectID from first segment
	objectData.info.objectId = segmentData[0].info.objectId;

	if (segmentCount == 1) {
		objectData.info.objectSize = segmentData[0].info.segmentSize;
	} else {
		// objectsize = first segment * (segmentCount - 1) + last segment
		objectData.info.objectSize = segmentData[0].info.segmentSize
				* (segmentCount - 1) + segmentData.end()->info.segmentSize;
	}

	objectData.buf = MemoryPool::getInstance().poolMalloc(
			objectData.info.objectSize);

	for (struct SegmentData segment : segmentData) {
		memcpy(objectData.buf, segment.buf, segment.info.segmentSize);
	}

	return objectData;
}

uint32_t Raid0Coding::getNoOfStrips(string setting) {
	uint32_t noOfStrips;
	istringstream(setting) >> noOfStrips;
	return noOfStrips;
}

/*
string Raid0Coding::generateSetting(int noOfStrips) {
	return to_string(noOfStrips);
}
*/

vector<uint32_t> Raid0Coding::getRequiredSegmentIds (string setting) {
	// for Raid0 Coding, require all segments for decode
	const uint32_t noOfStrips = getNoOfStrips(setting);
	vector <uint32_t> requiredSegments (noOfStrips);
	for (int i = 0; i < noOfStrips; i++) {
		requiredSegments[i] = i;
	}
	return requiredSegments;
}

void Raid0Coding::display() {

}
