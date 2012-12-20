#include <sstream>
#include <iostream>
#include <algorithm>
#include <string.h>
#include "../common/debug.hh"
#include "coding.hh"
#include "raid1coding.hh"
#include "../common/memorypool.hh"

Raid1Coding::Raid1Coding() {

}

Raid1Coding::~Raid1Coding() {

}

vector<BlockData> Raid1Coding::encode(SegmentData segmentData, string setting) {

	const uint32_t raid1_n = getParameters(setting);
	vector<struct BlockData> blockDataList;

	debug("RAID1: Replication No = %" PRIu32 "\n", raid1_n);

	for (uint32_t i = 0; i < raid1_n; i++) {

		struct BlockData blockData;
		blockData.info.segmentId = segmentData.info.segmentId;
		blockData.info.blockId = i;
		blockData.info.blockSize = segmentData.info.segmentSize;

		// an optimization is to point the buf pointer to the same memory,
		// but it may create confusion when user wants to free the data

		blockData.buf = MemoryPool::getInstance().poolMalloc(
				segmentData.info.segmentSize);

		memcpy(blockData.buf, segmentData.buf, blockData.info.blockSize);

		blockDataList.push_back(blockData);
	}

	return blockDataList;
}

SegmentData Raid1Coding::decode(vector<BlockData> &blockDataList,
		symbol_list_t &symbolList, uint32_t segmentSize, string setting) {

	// for raid1, only use first required block to decode
	uint32_t blockId = symbolList[0].first;

	struct SegmentData segmentData;
	segmentData.info.segmentId = blockDataList[blockId].info.segmentId;
	segmentData.info.segmentSize = segmentSize;
	segmentData.buf = MemoryPool::getInstance().poolMalloc(
			segmentData.info.segmentSize);
	memcpy(segmentData.buf, blockDataList[blockId].buf,
			segmentData.info.segmentSize);

	return segmentData;
}

uint32_t Raid1Coding::getParameters(string setting) {
	uint32_t raid1_n;
	istringstream(setting) >> raid1_n;
	return raid1_n;
}

symbol_list_t Raid1Coding::getRequiredBlockSymbols(vector<bool> blockStatus,
		string setting) {

	// for Raid1 Coding, find the first running OSD
	vector<bool>::iterator it;
	it = find(blockStatus.begin(), blockStatus.end(), true);

	// not found (no OSD is running)
	if (it == blockStatus.end()) {
		return {};
	}

	// return the index
	uint32_t offset = it - blockStatus.begin();
	vector<uint32_t> symbols = {0};
	block_symbols_t blockSymbols = make_pair(offset, symbols);
	return {blockSymbols};
}

symbol_list_t Raid1Coding::getRepairBlockSymbols(vector<uint32_t> failedBlocks,
		vector<bool> blockStatus, string setting) {

	// for RAID-1, repair is same as normal download
	return getRequiredBlockSymbols(blockStatus, setting);
}

vector<BlockData> Raid1Coding::repairBlocks(vector<uint32_t> repairBlockIdList,
		vector<BlockData> &blockData, vector<uint32_t> &blockIdList,
		symbol_list_t &symbolList, uint32_t segmentSize, string setting) {

	// for raid1, only use first required block to decode
	vector<BlockData> repairedBlockDataList;
	repairedBlockDataList.reserve(repairBlockIdList.size());

	for (uint32_t blockId : repairBlockIdList) {
		struct BlockData repairedBlock;
		uint32_t blockSize = repairedBlock.info.blockSize;
		repairedBlock.info.segmentId = blockData[blockIdList[0]].info.segmentId;
		repairedBlock.info.blockId = blockId;
		repairedBlock.info.blockSize = blockSize;
		repairedBlock.buf = MemoryPool::getInstance().poolMalloc(blockSize);
		memcpy(repairedBlock.buf, blockData[blockIdList[0]].buf, blockSize);
		repairedBlockDataList.push_back(repairedBlock);
	}

	return repairedBlockDataList;
}
