/*
 * messagefactory.cc
 */

#include "../common/debug.hh"
#include "../common/enums.hh"
#include "message.hh"
#include "listdirectoryrequest.hh"
#include "listdirectoryreply.hh"
#include "uploadfilerequest.hh"
#include "uploadfilereply.hh"
#include "putobjectinitrequest.hh"
#include "putobjectinitreply.hh"
#include "putobjectendrequest.hh"
#include "putobjectendreply.hh"
#include "putsegmentinitrequest.hh"
#include "putsegmentinitreply.hh"
#include "putsegmentendrequest.hh"
#include "putsegmentendreply.hh"
#include "objectdatamsg.hh"
#include "segmentdatamsg.hh"
#include "messagefactory.hh"
#include "handshakerequest.hh"
#include "handshakereply.hh"
#include "osdstartupmsg.hh"
#include "osdshutdownmsg.hh"
#include "osdstatupdaterequestmsg.hh"
#include "osdstatupdatereplymsg.hh"
#include "uploadobjectack.hh"
#include "getprimarylistrequest.hh"
#include "getprimarylistreply.hh"

MessageFactory::MessageFactory() {

}

MessageFactory::~MessageFactory() {

}

Message* MessageFactory::createMessage(Communicator* communicator,
		MsgType messageType) {
	switch (messageType) {
	case (HANDSHAKE_REQUEST):
		return new HandshakeRequestMsg(communicator);
		break;
	case (HANDSHAKE_REPLY):
		return new HandshakeReplyMsg(communicator);
		break;
	case (LIST_DIRECTORY_REQUEST):
		return new ListDirectoryRequestMsg(communicator);
		break;
	case (LIST_DIRECTORY_REPLY):
		return new ListDirectoryReplyMsg(communicator);
		break;
	case (UPLOAD_FILE_REQUEST):
		return new UploadFileRequestMsg(communicator);
		break;
	case (UPLOAD_FILE_REPLY):
		return new UploadFileReplyMsg(communicator);
		break;
	case (PUT_OBJECT_INIT_REQUEST):
		return new PutObjectInitRequestMsg(communicator);
		break;
	case (PUT_OBJECT_INIT_REPLY):
		return new PutObjectInitReplyMsg(communicator);
		break;
	case (PUT_OBJECT_END_REQUEST):
		return new PutObjectEndRequestMsg(communicator);
		break;
	case (PUT_OBJECT_END_REPLY):
		return new PutObjectEndReplyMsg(communicator);
		break;
	case (PUT_SEGMENT_INIT_REQUEST):
		return new PutSegmentInitRequestMsg(communicator);
		break;
	case (PUT_SEGMENT_INIT_REPLY):
		return new PutSegmentInitReplyMsg(communicator);
		break;
	case (PUT_SEGMENT_END_REQUEST):
		return new PutSegmentEndRequestMsg(communicator);
		break;
	case (PUT_SEGMENT_END_REPLY):
		return new PutSegmentEndReplyMsg(communicator);
		break;
	case (OBJECT_DATA):
		return new ObjectDataMsg(communicator);
		break;
	case (SEGMENT_DATA):
		return new SegmentDataMsg(communicator);
		break;
	case (UPLOAD_OBJECT_ACK):
		return new UploadObjectAckMsg(communicator);
		break;
	case (OSD_STARTUP):
		return new OsdStartupMsg(communicator);
		break;
	case (OSD_SHUTDOWN):
		return new OsdShutdownMsg(communicator);
		break;
	case (OSDSTAT_UPDATE_REQUEST):
		return new OsdStatUpdateRequestMsg(communicator);
		break;
	case (OSDSTAT_UPDATE_REPLY):
		return new OsdStatUpdateReplyMsg(communicator);
		break;
	case (GET_PRIMARY_LIST_REQUEST):
		return new GetPrimaryListRequestMsg(communicator);
		break;
	case (GET_PRIMARY_LIST_REPLY):
		return new GetPrimaryListReplyMsg(communicator);
		break;
	default:
		debug("%s\n", "Invalid message type");
		break;
	}
	return NULL;
}
