#include "mds_communicator.hh"

#include "../protocol/listdirectoryreply.hh"
#include "../protocol/uploadfilereply.hh"


/**
 * @brief	Reply With Folder Data
 */
void MdsCommunicator::replyFolderData(uint32_t requestId, uint32_t connectionId, string path, vector<FileMetaData> folderData)
{
	ListDirectoryReplyMsg* listDirectoryReplyMsg = new ListDirectoryReplyMsg(this,requestId, connectionId, path, folderData);
	listDirectoryReplyMsg->prepareProtocolMsg();
	
	addMessage(listDirectoryReplyMsg);
	return ;
}


vector<uint32_t> MdsCommunicator::askPrimaryList(uint32_t numOfObjs)
{
	vector<uint32_t> primaryList;
	for(uint32_t i = 0; i < numOfObjs; ++i)
		primaryList.push_back(52000 + (i % 2));
	return primaryList;
}


void MdsCommunicator::display()
{
	return ;
}


void MdsCommunicator::replyNodeList(uint32_t requestId, uint32_t connectionId, uint64_t objectId, vector<uint32_t>nodeList)
{
	return ;
}


void MdsCommunicator::replyObjectandPrimaryList(uint32_t requestId, uint32_t connectionId, uint32_t fileId, vector<uint64_t> objectList, vector<uint32_t> primaryList, unsigned char* checksum)
{
	// checksum NULL for upload
	if(checksum == NULL) {
		UploadFileReplyMsg* uploadFileReplyMsg = new UploadFileReplyMsg(this, requestId, connectionId, fileId, objectList, primaryList);
		uploadFileReplyMsg->prepareProtocolMsg();

		addMessage(uploadFileReplyMsg);
	}
	return ;
}


void MdsCommunicator::replyPrimary(uint32_t requestId, uint32_t connectionId, uint64_t objectId, uint32_t osdId)
{
	return ;
}


void MdsCommunicator::replyRecoveryInfo(uint32_t requestId, uint32_t connectionId, uint32_t osdId, vector<uint64_t> objectList, vector<uint32_t> primaryList, vector< vector<uint32_t> > objectNodeList)
{
	return ;
}


void MdsCommunicator::reportFailure(uint32_t osdId, FailureReason reason)
{
	return ;
}

