#ifndef DELTALOCATION_HH_
#define DELTALOCATION_HH_

struct DeltaLocation {
	uint32_t blockId;
	uint32_t deltaId;
	bool isReserveSpace;
	std::vector<offset_length_t> offsetLength;
};

#endif
