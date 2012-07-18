/**
 * listdirectoryreply.cc
 */

#include <iostream>

#include "listdirectoryreply.hh"

#include "../protocol/message.pb.h"
#include "../common/enums.hh"

ListDirectoryReplyMessage::ListDirectoryReplyMessage(uint32_t requestId, uint32_t sockfd, string path, vector<FileMetaData> folderData)
{
	_requestId = requestId;
	_path = path;
	_folderData = folderData;
	_sockfd = sockfd;
}

/**
 * @brief	Copy values in private variables to protocol message
 * Serialize protocol message and copy to private variable
 */
void ListDirectoryReplyMessage::prepareProtocolMsg() {
	string serializedString;

	ncvfs::ListDirectoryReply listDirectoryReply;

	vector<FileMetaData>::iterator it;

	for (it = _folderData.begin(); it < _folderData.end(); ++it)
	{
		ncvfs::FileInfo* fileInfo = listDirectoryReply.add_fileinfo();
		fileInfo->set_fileid((*it)._id);
		fileInfo->set_filesize((*it)._size);
		fileInfo->set_filename((*it)._path);
	}

	if (!listDirectoryReply.SerializeToString(&serializedString)) {
		cerr << "Failed to write string." << endl;
		return ;
	}

	setProtocolSize(serializedString.length());
	setProtocolType(LIST_DIRECTORY_REPLY);
	setProtocolMsg(serializedString);

	return ;
}