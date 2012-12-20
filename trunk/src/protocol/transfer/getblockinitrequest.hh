#ifndef __GETBLOCKINITREQUEST_HH__
#define __GETBLOCKINITREQUEST_HH__

#include "../message.hh"

using namespace std;

class GetBlockInitRequestMsg: public Message {
public:

	GetBlockInitRequestMsg(Communicator* communicator);

	GetBlockInitRequestMsg(Communicator* communicator, uint32_t osdSockfd,
			uint64_t segmentId, uint32_t blockId);

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

	/*
	void setBlockSize(uint32_t blockSize);
	uint32_t getBlockSize();
	void setChunkCount(uint32_t chunkCount);
	uint32_t getChunkCount();
	*/

private:
	uint64_t _segmentId;
	uint32_t _blockId;
};

#endif