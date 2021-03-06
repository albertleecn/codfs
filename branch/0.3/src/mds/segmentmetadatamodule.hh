#ifndef __SEGMENT_METADATA_MODULE_HH__
#define __SEGMENT_METADATA_MODULE_HH__

#include <stdint.h>
#include <vector>

#include "configmetadatamodule.hh"

#include "../storage/mongodb.hh"

#include "../common/metadata.hh"

class SegmentMetaDataModule {
public:
	/**
	 * @brief	Default Constructor
	 *
	 * @param	configMetaDataModule	Configuration Meta Data Module
	 */
	SegmentMetaDataModule(ConfigMetaDataModule* configMetaDataModule);

	/**
	 * @brief	Save Segment Info
	 *
	 * @param	segmentId	ID of the Segment
	 * @param	segmentInfo	Info of the Segment
	 */
	void saveSegmentInfo(uint64_t segmentId, struct SegmentMetaData segmentInfo);

	/**
	 * @brief	Read Segment Info
	 *
	 * @param	segmentId	ID of the Segment
	 *
	 * @return	Info of the Segment
	 */
	struct SegmentMetaData readSegmentInfo(uint64_t segmentId);

	/**
	 * @brief	Save Node List of a Segment
	 *
	 * @param	segmentId	ID of the Segment
	 * @param	segmentNodeList	List of Node ID
	 */
	void saveNodeList(uint64_t segmentId,
			const vector<uint32_t> &segmentNodeList);

	/**
	 * @brief	Read Node List of a Segment
	 *
	 * @param	segmentId	ID of the Segment
	 *
	 * @return	List of Node ID
	 */
	vector<uint32_t> readNodeList(uint64_t segmentId);

	/**
	 * @brief Find all segments owned by the osd
	 * @param osdId Osd ID
	 * @return list of segmentId
	 */
	vector<uint64_t> findOsdSegments(uint32_t osdId);

	/**
	 * @brief Find all the segments owned by the osd as primary
	 * @param osdId Osd ID
	 * @return list of segmentId
	 */

	vector<uint64_t> findOsdPrimarySegments(uint32_t osdId);

	/**
	 * @brief	Set Primary of a Segment
	 *
	 * @param	segmentId	ID of the Segment
	 * @param	primaryOsdId	ID of the Primary
	 */
	void setPrimary(uint64_t segmentId, uint32_t primary);

	/**
	 * @brief	Get Primary of a Segment
	 *
	 * @param	segmentId	ID of the Segment
	 *
	 * @return	ID of the Primary
	 */
	uint32_t getPrimary(uint64_t segmentId);

	/**
	 * @brief	Generate a New Segment ID
	 *
	 * @return	File ID
	 */
	uint64_t generateSegmentId();
private:
	/// Collection
	string _collection;

	/// Configuration Meta Data Module
	ConfigMetaDataModule* _configMetaDataModule;

	/// Underlying Meta Data Storage
	MongoDB* _segmentMetaDataStorage;

	//SegmentMetaDataCache *_segmentMetaDataCache;
};
#endif
