/*
 rpc.h for libnfs

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

#ifndef _RPC_H_
#define _RPC_H_

#include <gctypes.h>
#include "rpc_mount.h"
#include "nfs_file.h"

#define PROCEDURE_MOUNT			1
#define PROCEDURE_UNMOUNT		3

#define PROCEDURE_GETPORT		3

#define PROCEDURE_GETATTR		1
#define PROCEDURE_SETATTR		2
#define PROCEDURE_LOOKUP		3
#define PROCEDURE_ACCESS		4
#define PROCEDURE_EXPORT		5
#define PROCEDURE_READ			6
#define PROCEDURE_WRITE			7
#define PROCEDURE_CREATE		8
#define PROCEDURE_MKDIR			9
#define PROCEDURE_SYMLINK		10
#define PROCEDURE_MKNOD			11
#define PROCEDURE_REMOVE		12
#define PROCEDURE_RMDIR			13
#define PROCEDURE_RENAME		14
#define PROCEDURE_LINK			15
#define PROCEDURE_READDIR		16
#define PROCEDURE_READDIRPLUS	17
#define PROCEDURE_FSSTAT		18
#define PROCEDURE_FSINFO		19
#define PROCEDURE_PATHCONF		20
#define PROCEDURE_COMMIT		21

#define PROGRAM_PORTMAP			100000
#define PROGRAM_NFS				100003
#define PROGRAM_MOUNT			100005

#define AUTH_NULL				0
#define AUTH_UNIX				1

#define TIME_DONT_CHANGE		0
#define TIME_SERVER				1
#define TIME_CLIENT				2

#define WRITE_UNSTABLE			0
#define WRITE_DATA_SYNC			1
#define WRITE_FILE_SYNC			2

#define NFS3_OK					0
#define NFS3ERR_PERM			1
#define NFS3ERR_NOENT			2
#define NFS3ERR_IO				5
#define NFS3ERR_NXIO			6
#define NFS3ERR_ACCES			13
#define NFS3ERR_EXIST			17
#define NFS3ERR_XDEV			18
#define NFS3ERR_NODEV			19
#define NFS3ERR_NOTDIR			20
#define NFS3ERR_ISDIR			21
#define NFS3ERR_INVAL			22
#define NFS3ERR_FBIG			27
#define NFS3ERR_NOSPC			28
#define NFS3ERR_ROFS			30
#define NFS3ERR_MLINK			31
#define NFS3ERR_NAMETOOLONG		63
#define NFS3ERR_NOTEMPTY		66
#define NFS3ERR_DQUOT			69
#define NFS3ERR_STALE			70
#define NFS3ERR_REMOTE			71
#define NFS3ERR_BADHANDLE		10001
#define NFS3ERR_NOT_SYNC		10002
#define NFS3ERR_BAD_COOKIE		10003
#define NFS3ERR_NOTSUPP			10004
#define NFS3ERR_TOOSMALL		10005
#define NFS3ERR_SERVERFAULT		10006
#define NFS3ERR_BADTYPE			10007
#define NFS3ERR_JUKEBOX			10008

int32_t rpc_create_header(NFSMOUNT *nfsmount, int32_t program, int32_t program_version, int32_t procedure, int32_t auth);
int32_t rpc_parse_header(NFSMOUNT *nfsmount, int32_t *rpc_header_length);

int32_t rpc_write_string(NFSMOUNT *nfsmount, int32_t offset, const char *str);
int32_t rpc_write_fhandle(NFSMOUNT *nfsmount, int32_t offset, fhandle3 *handle);

int32_t rpc_write_sattr(NFSMOUNT *nfsmount, int32_t offset, sattr3 *attr);

int32_t rpc_write_long(NFSMOUNT *nfsmount, int32_t offset, int64_t value);
int32_t rpc_write_int(NFSMOUNT *nfsmount, int32_t offset, int32_t value);

int32_t rpc_write_block(NFSMOUNT *nfsmount, int32_t offset, void *buf, int32_t len);

int32_t rpc_read_fhandle(NFSMOUNT *nfsmount, int32_t offset, fhandle3 *handle);
int32_t rpc_read_objectattr(NFSMOUNT *nfsmount, int32_t offset, object_attributes *attr);
int32_t rpc_read_stat(NFSMOUNT *nfsmount, int32_t offset, struct stat *stat);
int32_t rpc_read_sattr(NFSMOUNT *nfsmount, int32_t offset, sattr3 *attr);

int32_t rpc_read_string(NFSMOUNT *nfsmount, int32_t offset, char **str);

int32_t rpc_read_long(NFSMOUNT *nfsmount, int32_t offset, int64_t *val);
int32_t rpc_read_int(NFSMOUNT *nfsmount, int32_t offset, int32_t *val);

#endif // _RPC_H_
