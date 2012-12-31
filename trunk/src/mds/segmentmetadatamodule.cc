#include "segmentmetadatamodule.hh"

#include "../config/config.hh"

#include "../common/debug.hh"

#include "../storage/mongodb.hh"

extern ConfigLayer *configLayer;

using namespace mongo;

/**
 * @brief	Default Constructor
 */
SegmentMetaDataModule::SegmentMetaDataModule(
		ConfigMetaDataModule* configMetaDataModule) {
	_configMetaDataModule = configMetaDataModule;

	_collection = "Segment Meta Data";

	_segmentMetaDataStorage = new MongoDB();
	_segmentMetaDataStorage->connect();
	_segmentMetaDataStorage->setCollection(_collection);
}

/**
 * @brief	Save Segment Info
 */
void SegmentMetaDataModule::saveSegmentInfo(uint64_t segmentId,
		struct SegmentMetaData segmentInfo) {
	vector<uint32_t>::const_iterator it;
	BSONArrayBuilder arrb;
	for (it = segmentInfo._nodeList.begin(); it < segmentInfo._nodeList.end();
			++it) {
		arrb.append(*it);
	}
	BSONArray arr = arrb.arr();
	BSONObj querySegment = BSON ("id" << (long long int)segmentId);
	BSONObj insertSegment = BSON ("id" << (long long int)segmentId
			<< "primary" << segmentInfo._primary
			<< "checksum" << segmentInfo._checksum
			<< "size" << segmentInfo._size
			<< "codingScheme" << (int)segmentInfo._codingScheme
			<< "codingSetting" << segmentInfo._codingSetting
			<< "nodeList" << arr);
	_segmentMetaDataStorage->update(querySegment, insertSegment);
	//saveNodeList(segmentId, segmentInfo._nodeList);
	return;
}

/**
 * @brief	Read Segment Info
 *
 * @param	segmentId	ID of the Segment
 *
 * @return	Info of the Segment
 */
struct SegmentMetaData SegmentMetaDataModule::readSegmentInfo(uint64_t segmentId) {
	BSONObj querySegment = BSON ("id" << (long long int)segmentId);
	BSONObj result = _segmentMetaDataStorage->readOne(querySegment);
	struct SegmentMetaData segmentMetaData;
	segmentMetaData._id = segmentId;
	BSONForEach(it, result.getObjectField("nodeList")) {
		segmentMetaData._nodeList.push_back((uint32_t) it.numberInt());
	}
	segmentMetaData._primary = (uint32_t) result.getField("primary").numberInt();
	segmentMetaData._checksum = result.getField("checksum").str();
	segmentMetaData._size = (uint32_t) result.getField("size").numberInt();
	segmentMetaData._codingScheme = (CodingScheme) result.getField(
			"codingScheme").numberInt();
	segmentMetaData._codingSetting = result.getField("codingSetting").str();

	return segmentMetaData;
}

/**
 * @brief	Save Node List of a Segment
 */
void SegmentMetaDataModule::saveNodeList(uint64_t segmentId,
		const vector<uint32_t> &segmentNodeList) {
	debug("Save Node List For %" PRIu64 "\n", segmentId);
	vector<uint32_t>::const_iterator it;
	BSONObj querySegment = BSON ("id" << (long long int)segmentId);
	BSONArrayBuilder arrb;
	for (it = segmentNodeList.begin(); it < segmentNodeList.end(); ++it) {
		debug(" - %" PRIu32 "\n" , *it);
		arrb.append(*it);
	}
	debug("%s", "\n");
	BSONArray arr = arrb.arr();
	BSONObj updateSegment = BSON ("$set" << BSON ("nodeList" << arr));
	_segmentMetaDataStorage->update(querySegment, updateSegment);
	return;
}

/**
 * @brief	Read Node List of a Segment
 */
vector<uint32_t> SegmentMetaDataModule::readNodeList(uint64_t segmentId) {
	vector<uint32_t> nodeList;
	BSONObj querySegment = BSON ("id" << (long long int)segmentId);
	BSONObj result = _segmentMetaDataStorage->readOne(querySegment);
	BSONForEach(it, result.getObjectField("nodeList")) {
		nodeList.push_back((uint32_t) it.numberInt());
	}
	return nodeList;
}

/**
 * @brief	Set Primary of a Segment
 */
void SegmentMetaDataModule::setPrimary(uint64_t segmentId, uint32_t primary) {
	BSONObj querySegment = BSON ("id" << (long long int) segmentId);
	BSONObj updateSegment = BSON ("$set" << BSON ("primary" << primary));
	_segmentMetaDataStorage->update(querySegment, updateSegment);

	return;
}

/**
 * @brief	Get Primary of a Segment
 */
uint32_t SegmentMetaDataModule::getPrimary(uint64_t segmentId) {
	BSONObj querySegment = BSON ("id" << (long long int) segmentId);
	BSONObj temp = _segmentMetaDataStorage->readOne(querySegment);
	return (uint32_t) temp.getField("primary").Int();
}

/**
 * @brief	Generate a New Segment ID
 */
uint64_t SegmentMetaDataModule::generateSegmentId() {

	//return _configMetaDataModule->getAndInc("segmentId");
	return rand();
}

vector<uint64_t> SegmentMetaDataModule::findOsdSegments(uint32_t osdId) {
	vector<uint64_t> segmentList;
	BSONObj querySegment = BSON ("nodeList" << (int) osdId);
	vector<BSONObj> result = _segmentMetaDataStorage->read(querySegment);
	for (auto bson : result) {
		uint64_t id = (uint64_t)bson.getField("id").numberLong();
		segmentList.push_back(id);
	}
	return segmentList;
}

vector<uint64_t> SegmentMetaDataModule::findOsdPrimarySegments(uint32_t osdId) {
	vector<uint64_t> segmentList;
	BSONObj querySegment = BSON ("primary" << (int) osdId);
	vector<BSONObj> result = _segmentMetaDataStorage->read(querySegment);
	for (auto bson : result) {
		uint64_t id = (uint64_t)bson.getField("id").numberLong();
		segmentList.push_back(id);
	}
	return segmentList;
}
