/*
 * messagefactory.cc
 */

#include "../common/debug.hh"
#include "../common/enums.hh"
#include "message.hh"
#include "listdirectoryrequest.hh"
#include "listdirectoryreply.hh"
#include "putobjectinitrequest.hh"
#include "putobjectinitreply.hh"
#include "putobjectendrequest.hh"
#include "putobjectendreply.hh"
#include "objectdatamsg.hh"
#include "segmentdatamsg.hh"
#include "messagefactory.hh"

MessageFactory::MessageFactory() {

}

MessageFactory::~MessageFactory() {

}

Message* MessageFactory::createMessage(Communicator* communicator,
		MsgType messageType) {
	switch (messageType) {
	case (LIST_DIRECTORY_REQUEST):
		return new ListDirectoryRequestMsg(communicator);
		break;
	case (LIST_DIRECTORY_REPLY):
		return new ListDirectoryReplyMsg(communicator);
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
	case (OBJECT_DATA):
		return new ObjectDataMsg(communicator);
		break;
	case (SEGMENT_DATA):
		return new SegmentDataMsg(communicator);
		break;

	default:
		debug("%s\n", "Invalid message type");
		break;
	}
	return NULL;
}
