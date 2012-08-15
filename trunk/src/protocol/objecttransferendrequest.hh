#ifndef __OBJECT_TRANSFER_END_REQUEST_HH__
#define __OBJECT_TRANSFER_END_REQUEST_HH__

#include "message.hh"

using namespace std;

/**
 * Extends the Message class
 * Initiate an object upload
 */

class ObjectTransferEndRequestMsg: public Message {
public:

	ObjectTransferEndRequestMsg(Communicator* communicator);

	ObjectTransferEndRequestMsg(Communicator* communicator, uint32_t osdSockfd,
			uint64_t objectId);

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

private:
	uint64_t _objectId;
};

#endif