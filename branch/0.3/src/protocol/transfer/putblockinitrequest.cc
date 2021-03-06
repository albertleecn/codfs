#include "putblockinitrequest.hh"
#include "../../common/debug.hh"
#include "../../protocol/message.pb.h"
#include "../../common/enums.hh"

#ifdef COMPILE_FOR_OSD
#include "../../osd/osd.hh"
extern Osd* osd;
#endif

PutBlockInitRequestMsg::PutBlockInitRequestMsg(Communicator* communicator) :
		Message(communicator) {

}

PutBlockInitRequestMsg::PutBlockInitRequestMsg(Communicator* communicator,
		uint32_t osdSockfd, uint64_t segmentId, uint32_t blockId, uint32_t blockSize, uint32_t chunkCount) :
		Message(communicator) {

	_sockfd = osdSockfd;
	_segmentId = segmentId;
	_blockId = blockId;
	_blockSize = blockSize;
	_chunkCount = chunkCount;
	
}

void PutBlockInitRequestMsg::prepareProtocolMsg() {
	string serializedString;
	ncvfs::PutBlockInitRequestPro putBlockInitRequestPro;
	putBlockInitRequestPro.set_segmentid(_segmentId);
	putBlockInitRequestPro.set_blockid(_blockId);
	putBlockInitRequestPro.set_blocksize(_blockSize);
	putBlockInitRequestPro.set_chunkcount(_chunkCount);

	if (!putBlockInitRequestPro.SerializeToString(&serializedString)) {
		cerr << "Failed to write string." << endl;
		return;
	}

	setProtocolSize(serializedString.length());
	setProtocolType(PUT_BLOCK_INIT_REQUEST);
	setProtocolMsg(serializedString);

}

void PutBlockInitRequestMsg::parse(char* buf) {

	memcpy(&_msgHeader, buf, sizeof(struct MsgHeader));

	ncvfs::PutBlockInitRequestPro putBlockInitRequestPro;
	putBlockInitRequestPro.ParseFromArray(buf + sizeof(struct MsgHeader),
			_msgHeader.protocolMsgSize);

	_segmentId = putBlockInitRequestPro.segmentid();
	_blockId = putBlockInitRequestPro.blockid();
	_blockSize = putBlockInitRequestPro.blocksize();
	_chunkCount = putBlockInitRequestPro.chunkcount();

}

void PutBlockInitRequestMsg::doHandle() {
#ifdef COMPILE_FOR_OSD
	debug("[PUT_BLOCK_INIT] Segment ID = %" PRIu64 ", Block ID = %" PRIu32 ", Length = %" PRIu32 ", Count = %" PRIu32 "\n",
			_segmentId, _blockId, _blockSize, _chunkCount);
	osd->putBlockInitProcessor (_msgHeader.requestId, _sockfd, _segmentId, _blockId, _blockSize, _chunkCount);
#endif
}

void PutBlockInitRequestMsg::printProtocol() {
	debug("[PUT_BLOCK_INIT] Segment ID = %" PRIu64 ", Block ID = %" PRIu32 ", Length = %" PRIu32 ", Count = %" PRIu32 "\n",
			_segmentId, _blockId, _blockSize, _chunkCount);
}
