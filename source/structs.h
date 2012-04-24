/*
 structs.h for libnfs

 Copyright (c) 2012 r-win

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _STRUCTS_H_
#define _STRUCTS_H_

#include <sys/iosupport.h>
#include <sys/stat.h>
#include <gccore.h>
#include <network.h>

typedef struct {
	int len;
	void *val;
} fhandle3;

typedef struct {
	uint32_t setmode;
	uint32_t mode;
	uint32_t setuid;
	uint32_t uid;
	uint32_t setgid;
	uint32_t gid;
	uint32_t setsize;
	uint64_t size;
	uint32_t setatime;
	uint32_t setmtime;
}  __attribute((packed)) sattr3;

typedef struct {
	void *buffer;
	uint32_t bufferlen;
	fhandle3 handle;
	fhandle3 curdir;
	char *curdirname;
	uint16_t mount_port;
	uint16_t nfs_port;
	char *mountdir;
	mutex_t lock;

	// Socket information
	uint32_t xid;
	int32_t socket;
	struct sockaddr_in remote;

	// FS info
	int32_t rtmax; // The max size for a READ request
	int32_t rtpref; // The preferred size for a READ request
	int32_t rtmult; // The suggested multiple for a READ request
	int32_t wtmax; // The max size for a WRITE request
	int32_t wtpref; // The preferred size for a WRITE request
	int32_t wtmult; // The suggested multiple for a WRITE request
	int32_t dtpref; // The preferred size for a READDIR request
} NFSMOUNT;

typedef struct {
	fhandle3 handle;
	struct stat stat;
	char *name;
} NFS_DIR_CHILD;

typedef struct {
	NFSMOUNT *nfsmount;

	// Dir handle
	fhandle3 handle;	// The filehandle for the directory
	long long cookie;
	int is_completed;

	// File handles
	int current_child; // For the dirnext and dirreset methods
	uint32_t numchilds; // The amount of childs
	NFS_DIR_CHILD *childs;
} NFS_DIR_STATE_STRUCT;

typedef struct {
	int32_t type;
	int32_t mode;
	int32_t nlink;
	int32_t uid;
	int32_t gid;
	uint32_t size_u;
	uint32_t size_l;
	int64_t used;
	int64_t rdev;
	int64_t fsid;
	int64_t fileid;
	uint32_t atime;
	uint32_t atime_nsec;
	uint32_t mtime;
	uint32_t mtime_nsec;
	uint32_t ctime;
	uint32_t ctime_nsec;
} __attribute__((packed)) object_attributes;

typedef struct {
	NFSMOUNT *nfsmount;

	// Dir handle
	fhandle3 handle;	// The filehandle for the file
	uint32_t size;     // The size of the file
	uint32_t currentPosition;
	int8_t isnew;
	int8_t read;
	int8_t write;
	int8_t append;
	int8_t shouldcommit;
} NFS_FILE_STRUCT;

#endif //_STRUCTS_H_
