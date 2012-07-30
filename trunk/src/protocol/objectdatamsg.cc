#include "objectdatamsg.hh"
#include "../common/debug.hh"
#include "../protocol/message.pb.h"
#include "../common/enums.hh"
#include "../osd/osd.hh"
#include "../client/client.hh"

#ifdef COMPILE_FOR_OSD
	extern Osd* osd;
#endif

#ifdef COMPILE_FOR_CLIENT
	extern Client* client;
#endif

ObjectDataMsg::ObjectDataMsg(Communicator* communicator) :
		Message(communicator) {

}

ObjectDataMsg::ObjectDataMsg(Communicator* communicator, uint32_t osdSockfd,
		uint64_t objectId, uint64_t offset, uint32_t length) :
		Message(communicator) {

	_sockfd = osdSockfd;
	_objectId = objectId;
	_offset = offset;
	_length = length;
}

void ObjectDataMsg::prepareProtocolMsg() {
	string serializedString;

	ncvfs::ObjectDataPro objectDataPro;
	objectDataPro.set_objectid(_objectId);
	objectDataPro.set_offset(_offset);
	objectDataPro.set_length(_length);

	if (!objectDataPro.SerializeToString(&serializedString)) {
		cerr << "Failed to write string." << endl;
		return;
	}

	setProtocolSize(serializedString.length());
	setProtocolType(OBJECT_DATA);
	setProtocolMsg(serializedString);

}

void ObjectDataMsg::parse(char* buf) {

	memcpy(&_msgHeader, buf, sizeof(struct MsgHeader));

	ncvfs::ObjectDataPro objectDataPro;
	objectDataPro.ParseFromArray(buf + sizeof(struct MsgHeader),
			_msgHeader.protocolMsgSize);

	_objectId = objectDataPro.objectid();
	_offset = objectDataPro.offset();
	_length = objectDataPro.length();

}

void ObjectDataMsg::handle() {
#ifdef COMPILE_FOR_OSD
	osd->putObjectDataProcessor(_sockfd, _objectId, _offset, _length, _payload);
#endif
}

void ObjectDataMsg::printProtocol() {
	cout << "[OBJECT_DATA] objectID = " << _objectId << " Offset = " << _offset << " Length = " << _length
			<< endl;
}