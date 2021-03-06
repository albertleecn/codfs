#include "putsegmentinitrequest.hh"
#include "../../common/debug.hh"
#include "../../protocol/message.pb.h"
#include "../../common/enums.hh"

#ifdef COMPILE_FOR_OSD
#include "../../osd/osd.hh"
extern Osd* osd;
#endif

PutSegmentInitRequestMsg::PutSegmentInitRequestMsg(Communicator* communicator) :
		Message(communicator) {

}

PutSegmentInitRequestMsg::PutSegmentInitRequestMsg(Communicator* communicator,
		uint32_t osdSockfd, uint64_t objectId, uint32_t segmentId, uint32_t segmentSize, uint32_t chunkCount) :
		Message(communicator) {

	_sockfd = osdSockfd;
	_objectId = objectId;
	_segmentId = segmentId;
	_segmentSize = segmentSize;
	_chunkCount = chunkCount;
	
}

void PutSegmentInitRequestMsg::prepareProtocolMsg() {
	string serializedString;
	ncvfs::PutSegmentInitRequestPro putSegmentInitRequestPro;
	putSegmentInitRequestPro.set_objectid(_objectId);
	putSegmentInitRequestPro.set_segmentid(_segmentId);
	putSegmentInitRequestPro.set_segmentsize(_segmentSize);
	putSegmentInitRequestPro.set_chunkcount(_chunkCount);

	if (!putSegmentInitRequestPro.SerializeToString(&serializedString)) {
		cerr << "Failed to write string." << endl;
		return;
	}

	setProtocolSize(serializedString.length());
	setProtocolType(PUT_SEGMENT_INIT_REQUEST);
	setProtocolMsg(serializedString);

}

void PutSegmentInitRequestMsg::parse(char* buf) {

	memcpy(&_msgHeader, buf, sizeof(struct MsgHeader));

	ncvfs::PutSegmentInitRequestPro putSegmentInitRequestPro;
	putSegmentInitRequestPro.ParseFromArray(buf + sizeof(struct MsgHeader),
			_msgHeader.protocolMsgSize);

	_objectId = putSegmentInitRequestPro.objectid();
	_segmentId = putSegmentInitRequestPro.segmentid();
	_segmentSize = putSegmentInitRequestPro.segmentsize();
	_chunkCount = putSegmentInitRequestPro.chunkcount();

}

void PutSegmentInitRequestMsg::doHandle() {
#ifdef COMPILE_FOR_OSD
	debug("[PUT_SEGMENT_INIT] Object ID = %" PRIu64 ", Segment ID = %" PRIu32 ", Length = %" PRIu32 ", Count = %" PRIu32 "\n",
			_objectId, _segmentId, _segmentSize, _chunkCount);
	osd->putSegmentInitProcessor (_msgHeader.requestId, _sockfd, _objectId, _segmentId, _segmentSize, _chunkCount);
#endif
}

void PutSegmentInitRequestMsg::printProtocol() {
	debug("[PUT_SEGMENT_INIT] Object ID = %" PRIu64 ", Segment ID = %" PRIu32 ", Length = %" PRIu32 ", Count = %" PRIu32 "\n",
			_objectId, _segmentId, _segmentSize, _chunkCount);
}
