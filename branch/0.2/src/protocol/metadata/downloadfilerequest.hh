/**
 * downloadfilerequest.hh
 */

#ifndef __DOWNLOAD_FILE_REQUEST_HH__
#define __DOWNLOAD_FILE_REQUEST_HH__

#include <string>
#include <vector>
#include <future>

#include "../message.hh"

#include "../../common/enums.hh"
#include "../../common/metadata.hh"

using namespace std;

/**
 * Extends the Message class
 * Requset to upload file
 */

class DownloadFileRequestMsg: public Message {
public:

	/**
	 * Default Constructor
	 *
	 * @param	communicator	Communicator the Message belongs to
	 */

	DownloadFileRequestMsg(Communicator* communicator);

	/**
	 * Constructor - Save parameters in private variables
	 *
	 * @param	communicator	Communicator the Message belongs to
	 * @param	clientId	Client ID
	 * @param	fileId		File ID
	 */

	DownloadFileRequestMsg(Communicator* communicator, uint32_t mdsSockfd,
			uint32_t clientId, uint32_t fileId);

	/**
	 * Constructor - Save parameters in private variables
	 *
	 * @param	communicator	Communicator the Message belongs to
	 * @param	clientId	Client ID
	 * @param	filePath	File Path
	 */

	DownloadFileRequestMsg(Communicator* communicator, uint32_t mdsSockfd,
			uint32_t clientId, string filePath);

	/**
	 * Copy values in private variables to protocol message
	 * Serialize protocol message and copy to private variable
	 */

	void prepareProtocolMsg();

	/**
	 * Override
	 * Parse message from raw buffer
	 * @param buf Raw buffer storing header + protocol + payload
	 */

	void parse(char* buf);

	/**
	 * Override
	 * Execute the corresponding Processor
	 */

	void doHandle();

	/**
	 * Override
	 * DEBUG: print protocol message
	 */

	void printProtocol();

	void setObjectList(vector<uint64_t> objectList);
	void setPrimaryList(vector<uint32_t> primaryList);
	vector<uint64_t> getObjectList();
	vector<uint32_t> getPrimaryList();
	void setFileId (uint32_t fileId);
	uint32_t getFileId ();
	void setFilePath (string filePath);
	string getFilePath ();
	void setSize (uint64_t size);
	uint32_t getSize();

private:
	uint32_t _clientId;
	uint32_t _fileId;
	uint64_t _size;
	string _filePath;
	vector<uint64_t> _objectList;
	vector<uint32_t> _primaryList;
};

#endif
