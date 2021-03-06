#include "../common/define.hh"

#include <fuse.h>
#include <errno.h>
#include <unordered_map>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <sys/stat.h>

#include "client.hh"
#include "client_communicator.hh"
//#include "filedatamodule.hh"
#include "filedatacache.hh"

#include "../common/metadata.hh"
#include "../common/garbagecollector.hh"
#include "../common/debug.hh"

#include "../coding/raid1coding.hh"

#include "../config/config.hh"

Client* client;

ConfigLayer* configLayer;

ClientCommunicator* _clientCommunicator;

string _fuseFolder = "./fusedir";

CodingScheme codingScheme = RAID1_CODING;
string codingSetting = Raid1Coding::generateSetting(1);

uint32_t _clientId = 51000;
#ifdef FUSE_READ_AHEAD
uint32_t _readAhead = 5;
#endif

mutex fileInfoCacheMutex;
mutex _segmentProcessingMutex;
mutex _readAheadCountMutex;
mutex _segmentReadCacheAccessTimeMutex;

unordered_map<uint32_t, FileDataCache*> _fileDataCache;
//unordered_map<uint64_t, unique_lock<mutex> > _segmentProcessing;
unordered_map<uint32_t, struct FileMetaData> _fileInfoCache;
unordered_map<string, uint32_t> _fileIdCache;
unordered_map<uint32_t, uint32_t> _readAheadCount;
unordered_map<uint32_t, vector<bool> > _segmentProcessing;

list<uint64_t> _segmentReadCacheAccessTime;
unordered_map<uint64_t, bool> _segmentReadCacheBitMap;
uint32_t _readCacheLimit = 20;

thread garbageCollectionThread;
thread receiveThread;
thread sendThread;

#ifdef PARALLEL_TRANSFER

#include "../../lib/threadpool/threadpool.hpp"
boost::threadpool::pool _tp;
boost::threadpool::pool _writetp;
uint32_t _writePoolLimit = 20;

void addSegmentReadCache(uint64_t segmentId) {
	lock_guard<mutex> lk(_segmentReadCacheAccessTimeMutex);
	if( _segmentReadCacheBitMap.count(segmentId) == 0) {
		_segmentReadCacheBitMap[segmentId] = true;
		if( _segmentReadCacheAccessTime.size() == _readCacheLimit) {
			uint64_t segmentToClose = _segmentReadCacheAccessTime.front();
			_segmentReadCacheAccessTime.pop_front();
			client->getStorageModule()->closeSegment(segmentToClose);
		}
		_segmentReadCacheAccessTime.push_back(segmentId);	
	} else {
		// TODO: Update LRU
	}
}

void startDownloadThread(uint32_t clientId, uint32_t sockfd, uint64_t segmentId, uint32_t fileId, uint32_t segmentIndex){
	client->getSegment(clientId, sockfd, segmentId);
	debug("Segment ID = %" PRIu64 " finished download\n", segmentId);
	_segmentProcessing[fileId][segmentIndex] = false;
	addSegmentReadCache(segmentId);
}
#endif

int ncvfs_error(const char *str) {
	int ret = -errno;
	printf("    ERROR %s: %s\n", str, strerror(errno));
	return ret;
}

struct FileMetaData getAndCacheFileInfo(string filePath) {
	struct FileMetaData fileMetaData;
	uint32_t fileId;
	lock_guard<mutex> lk(fileInfoCacheMutex);
	if (_fileIdCache.count(filePath) == 0) {
//	if (_fileInfoCache.count(fileId) == 0) { // if file not found in cache
		fileMetaData = _clientCommunicator->getFileInfo(_clientId, filePath);
		if(fileMetaData._fileType == NOTFOUND)
			return fileMetaData;
		_fileIdCache[filePath] = fileMetaData._id;
		_fileInfoCache[fileMetaData._id] = fileMetaData;
		fileId = fileMetaData._id;
	} else {
		fileId = _fileIdCache[filePath];
		debug("File Info Cache Found %s [%" PRIu32 "]\n",
				filePath.c_str(), fileId);
	}
	return _fileInfoCache[fileId];
}

struct FileMetaData getAndCacheFileInfo(uint32_t fileId) {
	struct FileMetaData fileMetaData;
	lock_guard<mutex> lk(fileInfoCacheMutex);
	if (_fileInfoCache.count(fileId) == 0) {
		fileMetaData = _clientCommunicator->getFileInfo(_clientId, fileId);
		if(fileMetaData._fileType == NOTFOUND)
			return fileMetaData;
		_fileIdCache[fileMetaData._path] = fileId;
		_fileInfoCache[fileId] = fileMetaData;
	}
	debug("File Info Cache Found [%" PRIu32 "]\n", fileId);
	return _fileInfoCache[fileId];
}

void startGarbageCollectionThread() {
	GarbageCollector::getInstance().start();
}

void* ncvfs_init(struct fuse_conn_info *conn) {
	debug_cyan ("%s\n", "implemented");
	_clientCommunicator->createServerSocket();

	// 1. Garbage Collection Thread
	garbageCollectionThread = thread(startGarbageCollectionThread);

	// 2. Receive Thread
	receiveThread = thread(&Communicator::waitForMessage, _clientCommunicator);

	// 3. Send Thread
#ifdef USE_MULTIPLE_QUEUE
#else
	sendThread = thread(&Communicator::sendMessage, _clientCommunicator);
#endif

	_clientCommunicator->setId(client->getClientId());
	_clientCommunicator->setComponentType(CLIENT);

	//_clientCommunicator->connectAllComponents();
	_clientCommunicator->connectToMds();
	_clientCommunicator->connectToMonitor();
	_clientCommunicator->getOsdListAndConnect();

#ifdef FUSE_READ_AHEAD
	_readAhead = configLayer->getConfigInt("Fuse>ReadAhead");
#endif

#ifdef PARALLEL_TRANSFER
	uint32_t _numClientThreads = configLayer->getConfigInt(
			"Communication>NumClientThreads");
	_tp.size_controller().resize(_numClientThreads);
	_writetp.size_controller().resize(_numClientThreads);
	_writePoolLimit = configLayer->getConfigInt("Fuse>WritePoolLimit");
	_readCacheLimit = configLayer->getConfigInt("Fuse>ReadCacheLimit");
#endif
	return NULL;
}

static int ncvfs_getattr(const char *path, struct stat *stbuf) {
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	//debug ("fpath = %s\n", fpath);
	printf("%s\n",fpath);

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();
		return 0;
	}

	retstat = lstat(fpath, stbuf);
	if (retstat != -1) { // if file is found in fuseFolder
		uint64_t fileId = 0;
		struct FileMetaData fileMetaData;
		//
		// Load File ID
		FILE* fp = fopen(fpath,"r");
		int ret = fscanf(fp,"%" PRIu64, &fileId);
		fclose(fp);
		if(ret > 0)
			fileMetaData = getAndCacheFileInfo(fileId);
		else
			// Newly Created File
			fileMetaData = getAndCacheFileInfo(path);

		if (fileMetaData._id == 0) { // should not happen?
			_fileIdCache.erase(path);
			_fileInfoCache.erase(fileMetaData._id);
			return -ENOENT;
		} else {
			stbuf->st_size = fileMetaData._size;
		}
	} else {
		perror("ERROR: lstat");
		return -ENOENT;
	}

	return retstat;

	/*
	 struct FileMetaData fileMetaData = getAndCacheFileInfo(path);
	 if (fileMetaData._id == 0) {
	 _fileIdCache.erase(path);
	 _fileInfoCache.erase(fileMetaData._id);
	 return -ENOENT;
	 }

	 stbuf->st_mode = S_IFREG | 0644;
	 stbuf->st_nlink = 1;
	 stbuf->st_uid = getuid();
	 stbuf->st_gid = getgid();
	 stbuf->st_size = fileMetaData._size;
	 stbuf->st_blocks = 0;
	 stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

	 return 0;
	 */
}

static int ncvfs_open(const char *path, struct fuse_file_info *fi) {
	debug_cyan ("%s\n", "implemented");
//	struct FileMetaData fileMetaData = getAndCacheFileInfo(path);
	
	// Load File ID
	const char* fpath = (_fuseFolder + string(path)).c_str();
	FILE* fp = fopen(fpath,"r");
	int ret = fscanf(fp,"%" PRIu64, &(fi->fh));
	if (ret < 0) {
		perror("fscanf()");
		exit(-1);
	}
	fclose(fp);

	struct FileMetaData fileMetaData = getAndCacheFileInfo(fi->fh);

	debug("Open File %s with ID %" PRIu64 "\n",path,fi->fh);
	vector<bool> segmentProcessing (fileMetaData._segmentList.size(),false);
	_segmentProcessing[fi->fh] = segmentProcessing;
	_readAheadCount[fi->fh] = 0;

	return 0;
}

static int ncvfs_create(const char * path, mode_t mode,
		struct fuse_file_info *fi) {
	debug_cyan ("%s\n", "implemented");
	
	const char* fpath = (_fuseFolder + string(path)).c_str();
	creat(fpath, mode);

	uint32_t segmentCount = configLayer->getConfigInt(
			"Fuse>PreallocateSegmentNumber");
	uint32_t segmentSize = configLayer->getConfigInt("Storage>SegmentSize")
			* 1024;
	struct FileMetaData fileMetaData = _clientCommunicator->uploadFile(
			_clientId, path, 0, segmentCount);
	fileMetaData._fileType = NORMAL;
	_fileIdCache[path] = fileMetaData._id;
	_fileInfoCache[fileMetaData._id] = fileMetaData;
	_fileDataCache[fileMetaData._id] = new FileDataCache(fileMetaData,
			segmentSize);
	fi->fh = fileMetaData._id;
	return 0;
}

static int ncvfs_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi) {
	debug_cyan ("%s\n", "implemented");
	struct FileMetaData fileMetaData = getAndCacheFileInfo(fi->fh);

	if (offset >= (off_t)fileMetaData._size)
		return 0;
	if ((offset + size) > fileMetaData._size){
		size = fileMetaData._size - offset;
	}

	ClientStorageModule* storageModule = client->getStorageModule();

	uint64_t segmentSize = storageModule->getSegmentSize();
	uint32_t startSegmentNum = offset / segmentSize;
	uint32_t endSegmentNum = (offset + size) / segmentSize;
	if (((offset + size) % segmentSize) == 0)
		--endSegmentNum;
	uint64_t byteWritten = 0;
	for (uint32_t i = startSegmentNum; i <= endSegmentNum; ++i) {
		uint64_t segmentId = fileMetaData._segmentList[i];
		uint32_t componentId = fileMetaData._primaryList[i];
		uint32_t sockfd = _clientCommunicator->getSockfdFromId(componentId);

		struct SegmentTransferCache segmentCache;
		while(_segmentProcessing[fi->fh][i]);
		segmentCache = client->getSegment(_clientId, sockfd, segmentId);

		addSegmentReadCache(segmentId);

		uint64_t copySize = min(segmentSize, size - byteWritten);
		copySize = min(copySize, (i + 1) * segmentSize - (offset + byteWritten));
		uint64_t segmentOffset = (offset + byteWritten) - i * segmentSize;
		memcpy(buf + byteWritten, segmentCache.buf + segmentOffset, copySize);
		byteWritten += copySize;
		//storageModule->closeSegment(segmentId);
	}

#ifdef FUSE_READ_AHEAD
	for(uint32_t j = _readAheadCount[fi->fh]; j <= endSegmentNum + _readAhead; ++j){
		if( j >= fileMetaData._segmentList.size())
			break;
		if( j <= _readAheadCount[fi->fh])
			continue;
		_readAheadCount[fi->fh] = j;
		uint64_t segmentId = fileMetaData._segmentList[j];
		uint32_t componentId = fileMetaData._primaryList[j];
		uint32_t sockfd = _clientCommunicator->getSockfdFromId(componentId);
		_tp.schedule(boost::bind(startDownloadThread, _clientId, sockfd, segmentId, fi->fh, j));
		_segmentProcessing[fi->fh][j] = true;
	}
#endif
	return byteWritten;
}

static int ncvfs_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi) {
	debug_cyan ("%s\n", "implemented");
	(void) buf;
	(void) offset;
	(void) fi;

//	if(strcmp(path, "/") != 0)
//		return -ENOENT;
	
	if (_fileDataCache.count(fi->fh) == 0) {
		struct FileMetaData fileMetaData = _fileInfoCache[fi->fh];
		uint32_t segmentSize = configLayer->getConfigInt("Storage>SegmentSize") * 1024;
		_fileDataCache[fi->fh] = new FileDataCache(fileMetaData, segmentSize);
	}

	_fileDataCache[fi->fh]->write(buf, size, offset);

	return size;
}

static int ncvfs_release(const char* path, struct fuse_file_info *fi) {
	debug_cyan ("%s\n", "implemented");
	debug("Release %s [%" PRIu32 "]\n", path, (uint32_t)fi->fh);

	// Save File ID
	const char* fpath = (_fuseFolder + string(path)).c_str();
	FILE* fp = fopen(fpath,"w");
	fprintf(fp,"%" PRIu64, fi->fh);
	fclose(fp);

	ClientStorageModule* storageModule = client->getStorageModule();
	if(_fileDataCache.count(fi->fh)){
		delete _fileDataCache[fi->fh];
		_fileDataCache.erase(fi->fh);
	}
	struct FileMetaData fileMetaData= _fileInfoCache[fi->fh];
	for(uint32_t i = 0; i < fileMetaData._segmentList.size(); ++i)
		storageModule->closeSegment(fileMetaData._segmentList[i]);
	_fileInfoCache.erase(fi->fh);
	_fileIdCache.erase(path);
	return 0;
}

void ncvfs_destroy(void* userdata) {
	debug_cyan ("%s\n", "implemented");
	//garbageCollectionThread.join();
	//receiveThread.join();
	//sendThread.join();
	return;
}

int ncvfs_chmod(const char *path, mode_t mode) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	retstat = chmod(fpath, mode);
	if (retstat < 0)
		retstat = ncvfs_error("ncvfs_chmod chmod");

	return retstat;
}

// not required
int ncvfs_chown(const char *path, uid_t uid, gid_t gid) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	retstat = chown(fpath, uid, gid);
	if (retstat < 0)
		retstat = ncvfs_error("ncvfs_chown chown");

	return retstat;
}

// not required
int ncvfs_utime(const char *path, struct utimbuf *ubuf) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	retstat = utime(fpath, ubuf);
	if (retstat < 0)
		retstat = ncvfs_error("ncvfs_utime utime");

	return retstat;
}

int ncvfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	DIR *dp;
	struct dirent *de;

	// once again, no need for fullpath -- but note that I need to cast fi->fh
	dp = (DIR *) (uintptr_t) fi->fh;

	// Every directory contains at least two entries: . and ..  If my
	// first call to the system readdir() returns NULL I've got an
	// error; near as I can tell, that's the only condition under
	// which I can get an error from readdir()
	de = readdir(dp);
	if (de == 0)
		return -errno;

	// This will copy the entire directory into the buffer.  The loop exits
	// when either the system readdir() returns NULL, or filler()
	// returns something non-zero.  The first case just means I've
	// read the whole directory; the second means the buffer is full.
	do {
		if (filler(buf, de->d_name, NULL, 0) != 0)
			return -ENOMEM;
	} while ((de = readdir(dp)) != NULL);

	return retstat;
}

// not required
int ncvfs_readlink(const char *path, char *link, size_t size) {
	debug_cyan("%s\n", "not implemented");
	return 0;
}

// not required
int ncvfs_mknod(const char *path, mode_t mode, dev_t dev) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	// On Linux this could just be 'mknod(path, mode, rdev)' but this
	//  is more portable
	if (S_ISREG(mode)) {
		retstat = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (retstat < 0)
			retstat = ncvfs_error("ncvfs_mknod open");
		else {
			retstat = close(retstat);
			if (retstat < 0)
				retstat = ncvfs_error("ncvfs_mknod close");
		}
	} else if (S_ISFIFO(mode)) {
		retstat = mkfifo(fpath, mode);
		if (retstat < 0)
			retstat = ncvfs_error("ncvfs_mknod mkfifo");
	} else {
		retstat = mknod(fpath, mode, dev);
		if (retstat < 0)
			retstat = ncvfs_error("ncvfs_mknod mknod");
	}

	return retstat;
}

int ncvfs_mkdir(const char *path, mode_t mode) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	retstat = mkdir(fpath, mode);
	if (retstat < 0)
		retstat = ncvfs_error("ncfs_mkdir mkdir");

	return retstat;
}

int ncvfs_unlink(const char *path) {
	debug_cyan("%s\n", "not implemented");
	const char* fpath = (_fuseFolder + string(path)).c_str();
	uint64_t fileId = 0;
	FILE* fp = fopen(fpath,"r");
	int ret = fscanf(fp,"%" PRIu64, &fileId);
	if (ret < 0) {
		return 0;
	}
	client->deleteFileRequest(path,fileId);
	remove (fpath);
	// TODO: Clear File Meta Data Cache
	return 0;
}

int ncvfs_rmdir(const char *path) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	retstat = rmdir(fpath);
	if (retstat < 0)
		retstat = ncvfs_error("ncfs_rmdir rmdir");

	return retstat;

}

/*
// not required
int ncvfs_symlink(const char *path, const char *link) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();
	const char* flink = (_fuseFolder + string(link)).c_str();
	//const char* bpath = ("/" + string(path)).c_str();

	fprintf(stderr, "%s->%s\n%s->%s\n", path, fpath, link, flink);

	retstat = symlink(fpath, flink);
//	retstat = symlink(path, flink);
	if (retstat < 0)
		retstat = ncvfs_error("ncvfs_symlink symlink");

	return retstat;

}
*/

int ncvfs_rename(const char *path, const char *newpath) {
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	string fpath = (_fuseFolder + string(path));
	string fnewpath = (_fuseFolder + string(newpath));

	retstat = rename(fpath.c_str(), fnewpath.c_str());
	debug("%s %s\n",fpath.c_str(), fnewpath.c_str());
	if (retstat < 0)
		retstat = ncvfs_error("ncfs_rename rename\n");

	_fileIdCache.erase(path);

	client->renameFileRequest(path, newpath);
	return retstat;
}

// not required
int ncvfs_link(const char *path, const char *newpath) {
	debug_cyan("%s\n", "not implemented");
	return 0;

}

int ncvfs_truncate(const char *path, off_t newsize) {
	debug_cyan ("%s\n", "implemented");

	uint64_t fileId = 0;

	// Load File ID
	const char* fpath = (_fuseFolder + string(path)).c_str();
	FILE* fp = fopen(fpath,"r");
	int ret = fscanf(fp,"%" PRIu64, &fileId);
	if (ret < 0) {
		perror("fscanf()");
		exit(-1);
	}
	fclose(fp);

	//struct FileMetaData fileMetaData = getAndCacheFileInfo(fileId);
	client->truncateFileRequest(fileId);
	return 0;

	/*
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	retstat = truncate(fpath, newsize);
	if (retstat < 0)
		ncvfs_error("ncvfs_truncate truncate");

	return retstat;
	*/

}

// not required
int ncvfs_statfs(const char *path, struct statvfs *statv) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	// get stats for underlying filesystem
	retstat = statvfs(fpath, statv);
	if (retstat < 0)
		retstat = ncvfs_error("ncvfs_statfs statvfs");

	return retstat;

}

// not required
int ncvfs_flush(const char *path, struct fuse_file_info *fi) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	if(_fileDataCache.count(fi->fh)) {
		_fileDataCache[fi->fh]->flush();
	}
	int retstat = 0;

	return retstat;
}

// not strictly required
int ncvfs_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");

	// TODO: return 0 for now, need contact MDS
	return 0;

	/*
	int retstat = 0;

	if (datasync)
		retstat = fdatasync(fi->fh);
	else
		retstat = fsync(fi->fh);

	if (retstat < 0)
		ncvfs_error("ncvfs_fsync fsync");

	return retstat;
	*/
}

// not required
int ncvfs_setxattr(const char *path, const char *name, const char *value,
		size_t size, int flags) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	retstat = lsetxattr(fpath, name, value, size, flags);
	if (retstat < 0)
		retstat = ncvfs_error("ncvfs_setxattr lsetxattr");

	return retstat;
}

// not required
int ncvfs_getxattr(const char *path, const char *name, char *value,
		size_t size) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	retstat = lgetxattr(fpath, name, value, size);
	if (retstat < 0)
		retstat = ncvfs_error("ncvfs_getxattr lgetxattr");

	return retstat;
}

// not required
int ncvfs_listxattr(const char *path, char *list, size_t size) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	retstat = llistxattr(fpath, list, size);
	if (retstat < 0)
		retstat = ncvfs_error("ncvfs_listxattr llistxattr");

	return retstat;
}

// not required
int ncvfs_removexattr(const char *path, const char *name) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	retstat = lremovexattr(fpath, name);
	if (retstat < 0)
		retstat = ncvfs_error("ncvfs_removexattr lrmovexattr");

	return retstat;
}

int ncvfs_opendir(const char *path, struct fuse_file_info *fi) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	DIR *dp;
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	dp = opendir(fpath);
	if (dp == NULL)
		retstat = ncvfs_error("ncvfs_opendir opendir");

	fi->fh = (intptr_t) dp;

	return retstat;

}

int ncvfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset) {
	debug_cyan("%s\n", "not implemented");
	return 0;
}

int ncvfs_releasedir(const char *path, struct fuse_file_info *fi) {
	//debug_cyan ("%s\n", "not implemented");
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;

	closedir((DIR *) (uintptr_t) fi->fh);

	return retstat;
}

// not strictly required
int ncvfs_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi) {
	debug_cyan("%s\n", "not implemented");
	// TODO: return 0 for now, need contact MDS
	return 0;
}

int ncvfs_access(const char *path, int mask) {
	debug_cyan ("%s\n", "implemented");
	int retstat = 0;
	const char* fpath = (_fuseFolder + string(path)).c_str();

	retstat = access(fpath, mask);

	if (retstat < 0)
		retstat = ncvfs_error("ncfs_access access");

	return retstat;
}

int ncvfs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi) {
	debug_cyan ("%s\n", "implemented");

	client->truncateFileRequest(fi->fh);
	return 0;

	/*
	int retstat = 0;

	retstat = ftruncate(fi->fh, offset);
	if (retstat < 0)
		retstat = ncvfs_error("ncvfs_ftruncate ftruncate");

	return retstat;
	*/
}

int ncvfs_fgetattr(const char *path, struct stat *statbuf,
		struct fuse_file_info *fi) {
	debug_cyan("%s\n", "not implemented");
	return 0;
}

struct ncvfs_fuse_operations: fuse_operations {
	ncvfs_fuse_operations() {
		init = ncvfs_init;
		destroy = ncvfs_destroy;
		getattr = ncvfs_getattr;
//		fgetattr = ncvfs_fgetattr;
		access = ncvfs_access;
		readlink = ncvfs_readlink; // not required
		opendir = ncvfs_opendir;
		readdir = ncvfs_readdir;
		mknod = ncvfs_mknod; // not required
		mkdir = ncvfs_mkdir;
		unlink = ncvfs_unlink;
		rmdir = ncvfs_rmdir;
//		symlink = ncvfs_symlink; // not required and not implemented
		rename = ncvfs_rename;
		link = ncvfs_link; // not required
		chmod = ncvfs_chmod;
		chown = ncvfs_chown; // not required
		truncate = ncvfs_truncate;
		ftruncate = ncvfs_ftruncate;
		utime = ncvfs_utime; // not required
		open = ncvfs_open;
		read = ncvfs_read;
		write = ncvfs_write;
		statfs = ncvfs_statfs; // not required
		release = ncvfs_release;
		releasedir = ncvfs_releasedir;
		fsync = ncvfs_fsync; // not strictly required
		fsyncdir = ncvfs_fsyncdir; // not strictly required
		flush = ncvfs_flush; // not required
		setxattr = ncvfs_setxattr; // not required
//		getxattr = ncvfs_getxattr; // not required
		listxattr = ncvfs_listxattr; // not required
		removexattr = ncvfs_removexattr; // not required
		create = ncvfs_create;

//		flag_nullpath_ok = 0;				// accept NULL path and use fi->fh
	}
};

static struct ncvfs_fuse_operations ncvfs_oper;

int main(int argc, char *argv[]) {
	configLayer = new ConfigLayer("clientconfig.xml");
	client = new Client(_clientId);
	//fileDataModule = new FileDataModule();
	_clientCommunicator = client->getCommunicator();
	return fuse_main(argc, argv, &ncvfs_oper, NULL);
}
