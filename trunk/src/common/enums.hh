#ifndef __ENUMS_HH__
#define __ENUMS_HH__

enum ComponentType {
	CLIENT = 1, OSD = 2, MDS = 3, MONITOR = 4 // numbers match message.proto
};

enum FailureReason {
	UNREACHABLE, DISKFAILURE, OBJECTLOST
};

enum MessageStatus {
	WAITING, READY, TIMEOUT
};

enum FileType {
	NOTFOUND = 1, NORMAL = 2, FOLDER = 3
};

enum ObjectDataStatus {
	NEW, UNFETCHED, CLEAN, DIRTY
};

enum MsgType {
	DEFAULT,

	// HANDSHAKE
	HANDSHAKE_REQUEST,
	HANDSHAKE_REPLY,

	// METADATA
	LIST_DIRECTORY_REQUEST,
	LIST_DIRECTORY_REPLY,
	UPLOAD_FILE_REQUEST,
	UPLOAD_FILE_REPLY,
	DELETE_FILE_REQUEST,
	DELETE_FILE_REPLY,
	UPLOAD_OBJECT_ACK,
	GET_OBJECT_ID_LIST_REQUEST,
	GET_OBJECT_ID_LIST_REPLY,
	DOWNLOAD_FILE_REQUEST,
	DOWNLOAD_FILE_REPLY,
	GET_OBJECT_INFO_REQUEST,
	GET_OBJECT_INFO_REPLY,
	SAVE_OBJECT_LIST_REQUEST,
	SAVE_OBJECT_LIST_REPLY,
	SET_FILE_SIZE_REQUEST,

	// TRANSFER
	PUT_OBJECT_INIT_REQUEST,
	PUT_OBJECT_INIT_REPLY,
	OBJECT_TRANSFER_END_REQUEST,
	OBJECT_TRANSFER_END_REPLY,
	PUT_SEGMENT_INIT_REQUEST,
	PUT_SEGMENT_INIT_REPLY,
	SEGMENT_TRANSFER_END_REQUEST,
	SEGMENT_TRANSFER_END_REPLY,
	OBJECT_DATA,
	SEGMENT_DATA,
	GET_OBJECT_REQUEST,
	GET_SEGMENT_INIT_REQUEST,

	// STATUS
	OSD_STARTUP,
	OSD_SHUTDOWN,
	OSDSTAT_UPDATE_REQUEST,
	OSDSTAT_UPDATE_REPLY,

	// NODE_LIST
	GET_PRIMARY_LIST_REQUEST,
	GET_PRIMARY_LIST_REPLY,
	GET_SECONDARY_LIST_REQUEST,
	GET_SECONDARY_LIST_REPLY,
	GET_OSD_LIST_REQUEST,
	GET_OSD_LIST_REPLY,

	// Newly Added
	NEW_OSD_REGISTER,
	ONLINE_OSD_LIST,

	// Newly Added For degraded read
	GET_OSD_STATUS_REQUEST,
	GET_OSD_STATUS_REPLY,
	SWITCH_PRIMARY_OSD_REQUEST,
	SWITCH_PRIMARY_OSD_REPLY,

	// RECOVERY
	REPAIR_OBJECT_INFO,

	// END
	MSGTYPE_END
};

enum StorageType {
	MONGODB, MYSQL
};

enum CodingScheme {
	DEFAULT_CODING = 15,
	RAID0_CODING = 1,
	RAID1_CODING = 2,
	RAID5_CODING = 3,
	RS_CODING = 4
};

#endif
