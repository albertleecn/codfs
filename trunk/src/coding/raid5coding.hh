#ifndef __RAID5CODING_HH__
#define __RAID5CODING_HH__

#include "coding.hh"

class Raid5Coding: public Coding {
public:

	Raid5Coding();
	~Raid5Coding();

	vector<BlockData> encode(struct SegmentData segmentData, string setting);

	SegmentData decode(vector<BlockData> &blockDataList,
			symbol_list_t &symbolList, uint32_t segmentSize, string setting);

	symbol_list_t getRequiredBlockSymbols(vector<bool> blockStatus,
			string setting);

	symbol_list_t getRepairBlockSymbols(vector<uint32_t> failedBlocks,
			vector<bool> blockStatus, string setting);

	vector<BlockData> repairBlocks(vector<uint32_t> repairBlockIdList,
			vector<BlockData> &blockData, vector<uint32_t> &blockIdList,
			symbol_list_t &symbolList, uint32_t segmentSize, string setting);

	static string generateSetting(int raid5_n) {
		return to_string(raid5_n);
	}

private:
	uint32_t getParameters(string setting);
};

#endif
