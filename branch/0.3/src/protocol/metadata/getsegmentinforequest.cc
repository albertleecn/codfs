#include <iostream>
using namespace std;
#include "getsegmentinforequest.hh"
#include "../../common/debug.hh"
#include "../../protocol/message.pb.h"
#include "../../common/enums.hh"
#include "../../common/memorypool.hh"

#ifdef COMPILE_FOR_MDS
#include "../../mds/mds.hh"
extern Mds* mds;
#endif

GetSegmentInfoRequestMsg::GetSegmentInfoRequestMsg(Communicator* communicator) :
		Message(communicator) {

}

GetSegmentInfoRequestMsg::GetSegmentInfoRequestMsg(Communicator* communicator,
		uint32_t dstSockfd, uint64_t segmentId) :
		Message(communicator) {

	_sockfd = dstSockfd;
	_segmentId = segmentId;
}

void GetSegmentInfoRequestMsg::prepareProtocolMsg() {
	string serializedString;

	ncvfs::GetSegmentInfoRequestPro getSegmentInfoRequestPro;
	getSegmentInfoRequestPro.set_segmentid(_segmentId);

	if (!getSegmentInfoRequestPro.SerializeToString(&serializedString)) {
		cerr << "Failed to write string." << endl;
		return;
	}

	setProtocolSize(serializedString.length());
	setProtocolType(GET_SEGMENT_INFO_REQUEST);
	setProtocolMsg(serializedString);

}

void GetSegmentInfoRequestMsg::parse(char* buf) {

	memcpy(&_msgHeader, buf, sizeof(struct MsgHeader));

	ncvfs::GetSegmentInfoRequestPro getSegmentInfoRequestPro;
	getSegmentInfoRequestPro.ParseFromArray(buf + sizeof(struct MsgHeader),
			_msgHeader.protocolMsgSize);

	_segmentId = getSegmentInfoRequestPro.segmentid();

}

void GetSegmentInfoRequestMsg::doHandle() {
#ifdef COMPILE_FOR_MDS
	mds->getSegmentInfoProcessor (_msgHeader.requestId, _sockfd, _segmentId);
#endif
}

void GetSegmentInfoRequestMsg::printProtocol() {
	debug("[GET_SEGMENT_INFO_REQUEST] Segment ID = %" PRIu64 "\n", _segmentId);
}

vector<uint32_t> GetSegmentInfoRequestMsg::getNodeList() {
	return _nodeList;
}

void GetSegmentInfoRequestMsg::setNodeList(vector<uint32_t> nodeList) {
	_nodeList = nodeList;
}

CodingScheme GetSegmentInfoRequestMsg::getCodingScheme() {
	return _codingScheme;
}

void GetSegmentInfoRequestMsg::setCodingScheme(CodingScheme codingScheme) {
	_codingScheme = codingScheme;
}

string GetSegmentInfoRequestMsg::getCodingSetting() {
	return _codingSetting;
}

void GetSegmentInfoRequestMsg::setCodingSetting(string codingSetting) {
	_codingSetting = codingSetting;
}

uint64_t GetSegmentInfoRequestMsg::getSegmentSize() {
	return _segmentSize;
}

void GetSegmentInfoRequestMsg::setSegmentSize(uint32_t segmentSize) {
	_segmentSize = segmentSize;
}

string GetSegmentInfoRequestMsg::getChecksum() {
	return _checksum;
}

void GetSegmentInfoRequestMsg::setChecksum(string checksum) {
	_checksum = checksum;
}
