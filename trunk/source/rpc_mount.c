/*
 rpc_mount.c for libnfs

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

#include <string.h>
#include <sys/iosupport.h>

#include "common.h"
#include "rpc.h"
#include "rpc_mount.h"
#include "nfs_net.h"
#include "portmap.h"
#include "lock.h"

extern int _nfs_buffer_size;

s32 rpc_domount(NFSMOUNT *nfsmount, s32 procedure, s32 mountport, const char *mountdir)
{
	int headerSize = rpc_create_header(nfsmount, PROGRAM_MOUNT, 3, procedure, AUTH_UNIX);

	int offset = headerSize;
	offset += rpc_write_string(nfsmount, offset, mountdir);

	s32 ret = udp_sendrecv(nfsmount, offset, mountport);
	if (ret < 0)
	{
		return ret;
	}

	s32 rpc_header_length = 0;
	if (rpc_parse_header(nfsmount, &rpc_header_length) != 0)
	{
		return -10;
	}

	if (procedure == PROCEDURE_UNMOUNT) return 0; // The RPC header status is 0, unmount will not supply additional information

	int32_t status;
	rpc_read_int(nfsmount, rpc_header_length, &status);
	if (status == 0) // Status of the mount call
	{
		return rpc_header_length;
	}
	return -11;
}

void rpc_fsinfo(NFSMOUNT *nfsmount)
{
	int headerSize = rpc_create_header(nfsmount, PROGRAM_NFS, 3, PROCEDURE_FSINFO, AUTH_UNIX);

	int offset = headerSize;
	offset += rpc_write_fhandle(nfsmount, offset, &nfsmount->handle);

	s32 ret = udp_sendrecv(nfsmount, offset, nfsmount->nfs_port);
	if (ret < 0) return;

	int32_t rpc_header_length = 0;
	if (rpc_parse_header(nfsmount, &rpc_header_length) == 0)
	{
		offset = rpc_header_length;
		int32_t status;
		offset += rpc_read_int(nfsmount, offset, &status);
		if (status == 0)
		{
			// Did we receive the object attributes entry (which is useless anyway :P)
			offset += rpc_read_int(nfsmount, offset, &status);
			if (status) {
				struct stat attr = {0};
				offset += rpc_read_stat(nfsmount, offset, &attr);
			}
			offset += rpc_read_int(nfsmount, offset, &nfsmount->rtmax);
			offset += rpc_read_int(nfsmount, offset, &nfsmount->rtpref);
			offset += rpc_read_int(nfsmount, offset, &nfsmount->rtmult);
			offset += rpc_read_int(nfsmount, offset, &nfsmount->wtmax);
			offset += rpc_read_int(nfsmount, offset, &nfsmount->wtpref);
			offset += rpc_read_int(nfsmount, offset, &nfsmount->wtmult);
			offset += rpc_read_int(nfsmount, offset, &nfsmount->dtpref);
		}
	}
}

NFSMOUNT * rpc_mount(const char *mountdir)
{
	NFSMOUNT *nfsmount = _NFS_mem_allocate(sizeof(NFSMOUNT));
	if (!nfsmount) return NULL;

	memset(nfsmount, 0, sizeof(NFSMOUNT));

	// Initialize the NFS lock
	_NFS_lock_init(&nfsmount->lock);

	_NFS_lock(&nfsmount->lock);

	// Allocate the buffer, which will be used for sending and retrieving
	nfsmount->buffer = _NFS_mem_allocate(_nfs_buffer_size);
	nfsmount->bufferlen = _nfs_buffer_size;
	if (!nfsmount->buffer) goto error;

	if (portmap_find_mount_port(nfsmount) != 0) goto error;
	if (portmap_find_nfs_port(nfsmount) != 0) goto error;

	s32 ret = rpc_domount(nfsmount, PROCEDURE_MOUNT, nfsmount->mount_port, mountdir); // ret will contain the length of the header
	if (ret > 0)
	{
		// First, get the length of the handle
		int offset = ret + 4; // 4 bytes to skip the status field, that was already checked in the rpc_domount
		offset += rpc_read_fhandle(nfsmount, offset, &nfsmount->handle);

		// Copy mountdir into nfsmount
		nfsmount->mountdir = _NFS_mem_allocate(strlen(mountdir) + 1);
		memset(nfsmount->mountdir, 0, strlen(mountdir) + 1);
		strncpy(nfsmount->mountdir, mountdir, strlen(mountdir));

		// Do a call to FSINFO at this point
		rpc_fsinfo(nfsmount);

		_NFS_unlock(&nfsmount->lock);
		return nfsmount;
	}
error:
	_NFS_unlock(&nfsmount->lock);
	_NFS_lock_deinit(&nfsmount->lock);
	_NFS_mem_free(nfsmount);
	return NULL; // Negative return value
}

void rpc_unmount(NFSMOUNT *nfsmount)
{
	_NFS_lock(&nfsmount->lock);

	rpc_domount(nfsmount, PROCEDURE_UNMOUNT, nfsmount->mount_port, nfsmount->mountdir);

	// Always clear, even on unclean unmount. Leave the unclean unmount the problem of the server
	
	// Set mount_port to 0 and clear mountdir
	_NFS_mem_free(nfsmount->mountdir);
	nfsmount->mountdir = NULL;
	nfsmount->mount_port = 0;
	nfsmount->nfs_port = 0;

	fhandle3_free(&nfsmount->handle);
	fhandle3_free(&nfsmount->curdir);

	if (nfsmount->curdirname) _NFS_mem_free(nfsmount->curdirname);

	_NFS_unlock(&nfsmount->lock);
	_NFS_lock_deinit(&nfsmount->lock);
}

NFSMOUNT *_NFS_get_NfsMountFromPath(const char *path)
{
	const devoptab_t *devops;

	devops = GetDeviceOpTab (path);

	if (!devops) {
		return NULL;
	}

	return (NFSMOUNT*)devops->deviceData;
}

void fhandle3_copy(fhandle3 *dest, fhandle3 *src)
{
	dest->len = src->len;
	dest->val = _NFS_mem_allocate(src->len);
	memcpy(dest->val, src->val, dest->len);
}

void fhandle3_cleancopy(fhandle3 *dest, fhandle3 *src)
{
	if (dest->val != NULL) _NFS_mem_free(dest->val);
	fhandle3_copy(dest, src);
}

void fhandle3_free(fhandle3 *handle)
{
	if (handle->val) _NFS_mem_free(handle->val);
	memset(handle, 0, sizeof(fhandle3));
}
