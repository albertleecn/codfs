#ifndef __SEGMENTDATA_HH__
#define __SEGMENTDATA_HH__

#include <string>
#include <stdint.h>
#include "../common/enums.hh"

using namespace std;

struct SegmentInfo {
	uint64_t objectId;
	uint32_t segmentId;
	uint32_t segmentSize;
	string segmentPath;
//	CodingScheme codingScheme;
//	uint32_t offsetInObject;
};

struct SegmentData {
	struct SegmentInfo info;
	char* buf;
};

#endif
