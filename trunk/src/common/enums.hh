#ifndef __ENUMS_HH__
#define __ENUMS_HH__

enum ComponentType {
	CLIENT, OSD, MDS, MONITOR
};

enum FailureReason {
	UNREACHABLE, DISKFAILURE, OBJECTLOST
};

enum MessageStatus {
	WAITING, READY, TIMEOUT
};

enum MsgType {
	DEFAULT,
	LIST_DIRECTORY_REQUEST,
	LIST_DIRECTORY_REPLY,
	PUT_OBJECT_INIT_REQUEST,
	PUT_OBJECT_INIT_REPLY,
	PUT_OBJECT_END,
	PUT_SEGMENT_INIT,
	PUT_SEGMENT_END,
	OBJECT_DATA,
	SEGMENT_DATA
};

enum StorageType {
	MONGODB, MYSQL
};

#endif
