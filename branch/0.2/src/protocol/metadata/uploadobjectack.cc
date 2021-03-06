#include <iostream>

#include "uploadobjectack.hh"

#include "../../protocol/message.pb.h"
#include "../../common/enums.hh"
#include "../../common/memorypool.hh"
#include "../../common/debug.hh"

#ifdef COMPILE_FOR_MDS
#include "../../mds/mds.hh"
extern Mds* mds;
#endif

UploadObjectAckMsg::UploadObjectAckMsg(Communicator* communicator) :
		Message(communicator) {
}

UploadObjectAckMsg::UploadObjectAckMsg(Communicator* communicator,
		uint32_t sockfd, uint64_t objectId, CodingScheme codingScheme,
		string codingSetting, vector<uint32_t> nodeList, string checksum) :
		Message(communicator) {
	_sockfd = sockfd;
	_objectId = objectId;
	_codingScheme = codingScheme;
	_codingSetting = codingSetting;
	_nodeList = nodeList;
	_checksum = checksum;
}

void UploadObjectAckMsg::prepareProtocolMsg() {
	string serializedString;

	ncvfs::UploadObjectAckPro uploadObjectAckPro;

	uploadObjectAckPro.set_objectid((long long int) _objectId);
	uploadObjectAckPro.set_codingscheme(
			(ncvfs::PutObjectInitRequestPro_CodingScheme) _codingScheme);
	uploadObjectAckPro.set_codingsetting(_codingSetting);
	uploadObjectAckPro.set_checksum(_checksum);

	vector<uint32_t>::iterator it;

	for (it = _nodeList.begin(); it < _nodeList.end(); ++it) {
		uploadObjectAckPro.add_nodelist(*it);
	}

	if (!uploadObjectAckPro.SerializeToString(&serializedString)) {
		cerr << "Failed to write string." << endl;
		return;
	}

	setProtocolSize(serializedString.length());
	setProtocolType(UPLOAD_OBJECT_ACK);
	setProtocolMsg(serializedString);

	return;
}

void UploadObjectAckMsg::parse(char* buf) {
	memcpy(&_msgHeader, buf, sizeof(struct MsgHeader));

	ncvfs::UploadObjectAckPro uploadObjectAckPro;
	uploadObjectAckPro.ParseFromArray(buf + sizeof(struct MsgHeader),
			_msgHeader.protocolMsgSize);

	_objectId = uploadObjectAckPro.objectid();
	_codingScheme = (CodingScheme) uploadObjectAckPro.codingscheme();
	_codingSetting = uploadObjectAckPro.codingsetting();
	_checksum = uploadObjectAckPro.checksum();

	for (int i = 0; i < uploadObjectAckPro.nodelist_size(); ++i) {
		_nodeList.push_back(uploadObjectAckPro.nodelist(i));
	}

	return;
}

void UploadObjectAckMsg::doHandle() {
#ifdef COMPILE_FOR_MDS
	mds->uploadObjectAckProcessor(_msgHeader.requestId, _sockfd, _objectId, _codingScheme, _codingSetting, _nodeList, _checksum);
#endif
}

void UploadObjectAckMsg::printProtocol() {
	debug("[UPLOAD_OBJECT_ACK] Object ID = %" PRIu64 "\n", _objectId);
}
