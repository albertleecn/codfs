/**
 * getsecondarylistreply.hh
 */

#ifndef __GET_SECONDARY_LIST_REPLY_HH__
#define __GET_SECONDARY_LIST_REPLY_HH__

#include <vector>
#include <string>
#include "../message.hh"

#include "../../common/enums.hh"
#include "../../common/metadata.hh"
#include "../../common/segmentlocation.hh"

using namespace std;

/**
 * Extends the Message class
 * Request to list files in a directory from MDS
 */

class GetSecondaryListReplyMsg: public Message {
public:

	/**
	 * Default Constructor
	 *
	 * @param	communicator	Communicator the Message belongs to
	 */

	GetSecondaryListReplyMsg (Communicator* communicator);

	/**
	 * Constructor - Save parameters in private variables
	 *
	 * @param	communicator	Communicator the Message belongs to
	 * @param	numOfObjs	number of Objects
	 * @param	mdsSockfd	Socket descriptor
	 */

	GetSecondaryListReplyMsg (Communicator* communicator, uint32_t requestId, uint32_t sockfd, vector<struct SegmentLocation> secondaryList);

	/**
	 * Copy values in private variables to protocol message
	 * Serialize protocol message and copy to private variable
	 */

	void prepareProtocolMsg ();

	/**
	 * Override
	 * Parse message from raw buffer
	 * @param buf Raw buffer storing header + protocol + payload
	 */

	void parse (char* buf);

	/**
	 * Override
	 * Execute the corresponding Processor
	 */

	void doHandle ();

	/**
	 * Override
	 * DEBUG: print protocol message
	 */

	void printProtocol ();


private:
	vector<struct SegmentLocation> _secondaryList;
};

#endif
