#ifndef __RDP_CODING_HH__
#define __RDP_CODING_HH__

#include "coding.hh"
#include "evenoddcoding.hh"

class RDPCoding: public EvenOddCoding {
public:

	RDPCoding();
	~RDPCoding();

	vector<BlockData> encode(struct SegmentData segmentData, string setting);

	block_list_t getRepairBlockSymbols(vector<uint32_t> failedBlocks,
			vector<bool> blockStatus, uint32_t segmentSize, string setting);

	vector<BlockData> repairBlocks(vector<uint32_t> repairBlockIdList,
			vector<BlockData> &blockData, block_list_t &symbolList,
			uint32_t segmentSize, string setting);

protected:
	virtual uint32_t getBlockSize(uint32_t segmentSize, uint32_t k) {
		return roundTo(segmentSize, k * k) / k;
	}

	virtual uint32_t getSymbolSize(uint32_t blockSize, uint32_t k) {
		return blockSize / k;
	}

	virtual char** repairDataBlocks(vector<BlockData> &blockDataList,
			block_list_t &symbolList, uint32_t segmentSize, string setting);
};

#endif
