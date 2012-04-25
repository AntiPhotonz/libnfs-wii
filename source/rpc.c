/*
 rpc.c for libnfs

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

#include <gctypes.h>
#include <sys/time.h>
#include <network.h>
#include <string.h>
#include "rpc.h"

#define TYPE_CALL 0
#define TYPE_REPLY 1

#define SUCCESS 0

int32_t rpc_create_header(NFSMOUNT *nfsmount, int32_t program, int32_t program_version, int32_t procedure, int32_t auth)
{
	if (nfsmount->bufferlen < 40) return -1;
	if (auth != AUTH_NULL && auth != AUTH_UNIX) return -2;

	uint32_t *buf = (u32 *) nfsmount->buffer;
	memset(nfsmount->buffer, 0, nfsmount->bufferlen);
	buf[0] = ++nfsmount->xid;		// Transmission Id
	buf[1] = TYPE_CALL;			// Message type (always call)
	buf[2] = 2;				// RPC Version (make it 2)
	buf[3] = program;			// Program to be called
	buf[4] = program_version;		// Programversion to be called
	buf[5] = procedure;			// Procedure to execute on external program
	buf[6] = auth;				// Authentication type

	if (auth == AUTH_NULL)
	{
		// Leave this for comments only
		buf[7] = 0;			// Length of auth header = 0
		buf[8] = 0;			// Verifier is not used with NFS, set to NULL
		buf[9] = 0;			// Length of verifier header = 0

		return 40; // Size is 40 bytes, 10 entries of 4 bytes
	}
	else
	{
		struct timeval tv;
		gettimeofday(&tv,NULL);

		// Post an IP or something, you can just post everything, it won't be checked
		uint32_t ip = net_gethostip();
		struct in_addr addr;
		addr.s_addr = ip;
		char *ipAddr = inet_ntoa(addr);
		
		uint32_t length = strlen(ipAddr);

		buf[8] = tv.tv_sec;						// Timestamp
		buf[9] = length;						// Length of the machine name 
		strncpy((char *) &buf[10], ipAddr, length); // The machine name
		
		unsigned char fillBytes = length % 4 == 0 ? 0 : 4 - (length % 4); // Align the name length to 4 bytes

		int32_t offset = ((length + fillBytes) / sizeof(u32)) + 10;
		buf[offset] = nfsmount->uid;				// UID
		buf[offset + 1] = nfsmount->gid;			// GID
		buf[offset + 2] = 0;					// Amount of GIDS in the gidlist, followed by (optionally) the gids themselves
		buf[7] = 8 + length + fillBytes + 12;	// Length of the authentication header

		// Buffers for Verifier can be empty, buffer is filled until 9 + length + fillBytes + 12, plus UID, GID and GIDlist
		buf[offset + 3] = 0;					// Verifier is not used with NFS, set to NULL
		buf[offset + 4] = 0;					// Length of verifier header = 0
		return 60 + length + fillBytes;
	}
}

int32_t rpc_is_expected_message(NFSMOUNT *nfsmount)
{
	uint32_t xid;
	rpc_read_int(nfsmount, 0, (int32_t *) &xid);

	return xid == nfsmount->xid ? 1 : 0;
}

int32_t rpc_parse_header(NFSMOUNT *nfsmount, int32_t *rpc_header_length)
{
	uint32_t *buf = (uint32_t *) nfsmount->buffer;

	*rpc_header_length = 24; // Fixed length of the reply header, since we don't have verifier data

	// buf[0] -> contains sequenceId = xid
	if (buf[1] != TYPE_REPLY) return -1;
	if (buf[2] != SUCCESS) return -2;
	// buf[3] -> contains verifier, which is NULL
	// buf[4] -> contains verifier length, which will be 0

	return (s32) buf[5]; // Contains the accepted bit
}

int32_t rpc_write_string(NFSMOUNT *nfsmount, int32_t offset, const char *str)
{
	int32_t len = strlen(str);
	*(uint32_t *) (nfsmount->buffer + offset) = len;
	strncpy((char *) (nfsmount->buffer + offset + 4), str, len);
	if (len % 4 != 0) len += (4 - (len % 4)); // Fill the text up to 4 bytes
	return len + 4;
}

int32_t rpc_write_fhandle(NFSMOUNT *nfsmount, int32_t offset, fhandle3 *handle)
{
	*((int32_t *) (nfsmount->buffer + offset)) = handle->len;
	memcpy((void *) (nfsmount->buffer + offset + 4), handle->val, handle->len);
	return handle->len + 4;
}

int32_t rpc_write_sattr(NFSMOUNT *nfsmount, int32_t offset, sattr3 *attr)
{
	int32_t len = 0;
	len += rpc_write_int(nfsmount, offset + len, attr->setmode);
	if (attr->setmode) len += rpc_write_int(nfsmount, offset + len, attr->mode);
	len += rpc_write_int(nfsmount, offset + len, attr->setuid);
	if (attr->setuid) len += rpc_write_int(nfsmount, offset + len, attr->uid);
	len += rpc_write_int(nfsmount, offset + len, attr->setgid);
	if (attr->setgid) len += rpc_write_int(nfsmount, offset + len, attr->gid);
	len += rpc_write_int(nfsmount, offset + len, attr->setsize);
	if (attr->setsize) len += rpc_write_long(nfsmount, offset + len, attr->size);
	len += rpc_write_int(nfsmount, offset + len, attr->setatime);
	len += rpc_write_int(nfsmount, offset + len, attr->setmtime);
	return len;
}

int32_t rpc_write_long(NFSMOUNT *nfsmount, int32_t offset, int64_t value)
{
	*((int32_t *) (nfsmount->buffer + offset)) = (int32_t)((value >> 32) & 0xFFFFFFFF);
	*((int32_t *) (nfsmount->buffer + offset + 4)) = (int32_t)(value & 0xFFFFFFFF);
	return sizeof(int64_t);
}

int32_t rpc_write_int(NFSMOUNT *nfsmount, int32_t offset, int32_t value)
{
	*((int32_t *) (nfsmount->buffer + offset)) = value;
	return sizeof(int);
}

int32_t rpc_write_block(NFSMOUNT *nfsmount, int32_t offset, void *buf, int32_t len)
{
	offset += rpc_write_int(nfsmount, offset, len);
	memcpy(nfsmount->buffer + offset, buf, len);
	return len + 4;
}

int32_t rpc_read_fhandle(NFSMOUNT *nfsmount, int32_t offset, fhandle3 *handle)
{
	handle->len = *(u32 *) (nfsmount->buffer + offset);
	handle->val = _NFS_mem_allocate(handle->len);
	memcpy(handle->val, nfsmount->buffer + offset + 4, handle->len);

	return handle->len + 4;
}

int32_t rpc_read_objectattr(NFSMOUNT *nfsmount, int32_t offset, object_attributes *attr)
{
	if (attr != NULL) memcpy(attr, nfsmount->buffer + offset, sizeof(object_attributes));
	return sizeof(object_attributes);
}

int32_t rpc_read_stat(NFSMOUNT *nfsmount, int32_t offset, struct stat *stat)
{
	object_attributes *attr = (object_attributes *) (nfsmount->buffer + offset);
	_NFS_copy_stat_from_attributes(stat, attr);
	return sizeof(object_attributes);
}

int32_t rpc_read_sattr(NFSMOUNT *nfsmount, int32_t offset, sattr3 *attr)
{
	int32_t len = 0;
	len += rpc_read_int(nfsmount, offset + len, (int32_t *) &attr->setmode);
	if (attr->setmode) len += rpc_read_int(nfsmount, offset + len, (int32_t *) &attr->mode);
	len += rpc_read_int(nfsmount, offset + len, (int32_t *) &attr->setuid);
	if (attr->setuid) len += rpc_read_int(nfsmount, offset + len, (int32_t *) &attr->uid);
	len += rpc_read_int(nfsmount, offset + len, (int32_t *) &attr->setgid);
	if (attr->setgid) len += rpc_read_int(nfsmount, offset + len, (int32_t *) &attr->gid);
	len += rpc_read_int(nfsmount, offset + len, (int32_t *) &attr->setsize);
	if (attr->setsize) len += rpc_read_long(nfsmount, offset + len, (int64_t *) &attr->size);
	len += rpc_read_int(nfsmount, offset + len, (int32_t *) &attr->setatime);
	len += rpc_read_int(nfsmount, offset + len, (int32_t *) &attr->setmtime);
	return len;
}

int32_t rpc_read_string(NFSMOUNT *nfsmount, int32_t offset, char **str)
{
	int32_t len = *((int32_t *) (nfsmount->buffer + offset));
	*str = _NFS_mem_allocate(len + 1);
	memset(*str, 0, len + 1);
	strncpy(*str, nfsmount->buffer + offset + 4, len);
	return 4 + len + (len % 4 == 0 ? 0 : 4 - (len % 4));
}

int32_t rpc_read_long(NFSMOUNT *nfsmount, int32_t offset, int64_t *val)
{
	*val = *((int64_t *) (nfsmount->buffer + offset));
	return sizeof(int64_t);
}

int32_t rpc_read_int(NFSMOUNT *nfsmount, int32_t offset, int32_t *val)
{
	*val = *((int32_t *) (nfsmount->buffer + offset));
	return sizeof(int);
}
