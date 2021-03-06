#ifndef __MDS_COMMUNICATOR_HH__
#define __MDS_COMMUNICATOR_HH__

#include "../common/hotness.hh"
#include "../common/metadata.hh"
#include "../communicator/communicator.hh"
#include "../common/segmentlocation.hh"

#include <stdint.h>
#include <vector>
using namespace std;

class MdsCommunicator: public Communicator {
public:

	MdsCommunicator();

	void display();

	/**
	 * @brief Request OSD to cache extra copies
	 * @param segmentId Segment ID
	 * @param req HotnessRequest
	 * @param osdList OSD list
	 * @return actual number of request made for cache
	 */

	uint32_t requestCache(uint64_t segmentId, HotnessRequest req, vector<uint32_t> osdList);

	/**
	 * @brief	Reply Segment and Primary List to Client
	 *
	 * @param	requestId	Request ID
	 * @param	connectionId	Connection ID
	 * @param	fileId	File ID
	 * @param	segmentList	Segment List
	 * @param	primaryList	Primary List
	 */
	void replySegmentandPrimaryList(uint32_t requestId, uint32_t connectionId,
			uint32_t fileId, vector<uint64_t> segmentList,
			vector<uint32_t> primaryList);

	/**
	 * @brief	Reply Download Information to Client
	 *
	 * @param	requestId	Request ID
	 * @param	connectionId	Connection ID
	 * @param	fileId	File ID
	 * @param	filePath	File Path
	 * @param	fileSize	Size of the File
	 * @param	fileType	File Type
	 * @param	checksum	Checksum of the File
	 * @param	segmentList	Segment List
	 * @param	primaryList	Primary List
	 */
	void replyDownloadInfo(uint32_t requestId, uint32_t connectionId,
			uint32_t fileId, string filePath, uint64_t fileSize,
			const FileType& fileType, string checksum,
			vector<uint64_t> segmentList, vector<uint32_t> primaryList);

	/**
	 * @brief	Reply Delete File
	 * 
	 * @param	requestId	Request ID
	 * @param	connectionId	Connection ID
	 * @param	fileId	File Id
	 */
	void replyDeleteFile(uint32_t requestId, uint32_t connectionId,
			uint32_t fileId);

	/**
	 * @brief	Reply Rename File
	 *
	 * @param	requestId	Request ID
	 * @param	connectionId	Connection ID
	 * @param	fileId	File Id
	 * @param	path	File Path
	 */
	void replyRenameFile(uint32_t requestId, uint32_t connectionId, uint32_t fileId, const string& path);

	/**
	 * @brief	Reply Segment ID List
	 *
	 * @param	requestId	Request ID
	 * @param	connectionId	Connection ID
	 * @param	segmentList	Segment ID List
	 * @parma	primaryList	Primary List
	 */
	void replySegmentIdList(uint32_t requestId, uint32_t connectionId,
			vector<uint64_t> segmentList, vector<uint32_t> primaryList);

	/**
	 * @brief	Reply Segment Information to Osd
	 *
	 * @param	requestId	Request ID
	 * @param	connectionId	Connection ID
	 * @param	segmentId	ID of the Segment
	 * @param 	segmentSize	Segment Size
	 * @param	nodeList	Node List
	 * @param	codingScheme	Coding Scheme for the file
	 * @param 	codingSetting	Coding Scheme Setting
	 */
	void replySegmentInfo(uint32_t requestId, uint32_t connectionId,
			uint64_t segmentId, uint32_t segmentSize, vector<uint32_t> nodeList,
			CodingScheme codingScheme, string codingSetting);

	/**
	 * @brief	Reply Save Segment List Request
	 *
	 * @param	requestId	Request ID
	 * @param	connectionId	Connection ID
	 * @param	fileId	File ID
	 */
	void replySaveSegmentList(uint32_t rquestId, uint32_t connectionId,
			uint32_t fileId);

	/**
	 * @brief	Reply With Folder Data
	 *
	 * @param	requestId	Request ID
	 * @param	connectionId	Connection ID
	 * @param	path	Path to the folder
	 * @param	folderData	Folder Data
	 */
	void replyFolderData(uint32_t requestId, uint32_t connectionId, string path,
			vector<FileMetaData> folderData);

	/**
	 * @brief	Reply the Recovery Information (Segment List and Associated Node List
	 * 
	 * @param	requestId	Request ID
	 * @param	connectionId	Connection ID
	 * @param	osdId		ID of the Osd to be Recovered
	 * @param	segmentLocationList	List of the Location of Segment
	 */

	void replyRecoveryTrigger(uint32_t requestId, uint32_t connectionId,
			vector<SegmentLocation> segmentLocationList);

	// Request to Other Nodes

	/**
	 * @brief	Report Failure to Monitor
	 * 
	 * @param	osdId	ID of the Failed Osd
	 * @param	reason	Reason of the Failure
	 */
	void reportFailure(uint32_t osdId, FailureReason reason);

	/**
	 * @brief	Ask Monitor for Primary List
	 *
	 * @param	numOfObjs	Number of Segments
	 */
	vector<uint32_t> askPrimaryList(uint32_t numOfObjs);

	vector<uint32_t> getPrimaryList(uint32_t sockfd, uint32_t numOfObjs);

	/**
	 * Request the status of OSD from MONITOR
	 * @param osdIdList List of OSD ID to request
	 * @return boolean array storing OSD health
	 */

	vector<bool> getOsdStatusRequest(vector<uint32_t> osdIdList);

    void replyUploadSegmentAck(uint32_t requestId, uint32_t connectionId, uint64_t segmentId);
private:
};
#endif
