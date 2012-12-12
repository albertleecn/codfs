#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <iterator>
#include "../config/config.hh"
#include "../common/debug.hh"
#include "selectionmodule.hh"

using namespace std;

extern ConfigLayer* configLayer;

#ifdef RR_DISTRIBUTE
mutex RRprimaryMutex;
uint32_t RRprimaryCount;
mutex RRsecondaryMutex;
uint32_t RRsecondaryCount;

#endif

SelectionModule::SelectionModule(map<uint32_t, struct OsdStat>& mapRef):
	_osdStatMap(mapRef) { 
#ifdef RR_DISTRIBUTE
	RRprimaryCount = 0;
	RRsecondaryCount = 0;
#endif
}

vector<uint32_t> SelectionModule::ChoosePrimary(uint32_t numOfObjs){
	//Just random choose primary
	vector<uint32_t> primaryList;
	vector<uint32_t> allOnlineList;
	{
		lock_guard<mutex> lk(osdStatMapMutex);
		for(auto& entry: _osdStatMap) {
			if (entry.second.osdHealth == ONLINE) {
				// choose this as a primary
				allOnlineList.push_back(entry.first);
			}
		}
	}
	uint32_t n = allOnlineList.size();
	set<uint32_t> selected;
	while (numOfObjs) {
#ifdef RR_DISTRIBUTE
		RRprimaryMutex.lock();
		int idx = RRprimaryCount;
		RRprimaryCount = (RRprimaryCount + 1) % n;
		RRprimaryMutex.unlock();
#else
		int idx = rand() % n;
#endif
		if (selected.size() == n || 
			selected.find(allOnlineList[idx]) == selected.end()) {
			primaryList.push_back(allOnlineList[idx]);
			selected.insert(allOnlineList[idx]);
			numOfObjs --;
		}
	}
	return primaryList;
}

vector<struct BlockLocation> SelectionModule::ChooseSecondary(uint32_t numOfSegs, 
	uint32_t primaryId) {

	vector<struct BlockLocation> secondaryList;

	// Push the primary osd as the first secondary
	struct BlockLocation primary;
	primary.osdId = primaryId;
	primary.blockId = 0;
	secondaryList.push_back(primary);

	vector<uint32_t> allOnlineList;
	{
		lock_guard<mutex> lk(osdStatMapMutex);
		for(auto& entry: _osdStatMap) {
			if (entry.second.osdHealth == ONLINE) {
				// choose this as a primary
				allOnlineList.push_back(entry.first);
			}
		}
	}
	set<uint32_t> selected;
	selected.insert(primaryId);
	uint32_t n = allOnlineList.size();
	numOfSegs--;
	while (numOfSegs) {
#ifdef RR_DISTRIBUTE
		RRsecondaryMutex.lock();
		int idx = RRsecondaryCount;
		RRsecondaryCount = (RRsecondaryCount + 1) % n;
		RRsecondaryMutex.unlock();
#else
		int idx = rand() % n;
#endif
		if (selected.size() == n || 
			selected.find(allOnlineList[idx]) == selected.end()) {
			struct BlockLocation tmp;
			tmp.osdId = allOnlineList[idx];
			tmp.blockId = 0;
			secondaryList.push_back(tmp);
			selected.insert(allOnlineList[idx]);
			numOfSegs--;
		}
	}
	return secondaryList;
}
