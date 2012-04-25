/*
 nfs_dir.c for libnfs

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
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "nfs_net.h"
#include "rpc.h"
#include "nfs_dir.h"
#include "nfs_file.h"
#include "lock.h"

int32_t _NFS_do_lookup(NFSMOUNT *nfsmount, fhandle3 *parentHandle, const char *dir, struct stat *attr, fhandle3 *handle)
{
	int32_t headerSize = rpc_create_header(nfsmount, PROGRAM_NFS, 3, PROCEDURE_LOOKUP, AUTH_UNIX);

	// Write a dir entry, first write the handle
	uint32_t offset = headerSize;
	offset += rpc_write_fhandle(nfsmount, offset, parentHandle);
	offset += rpc_write_string(nfsmount, offset, dir);

	// Do a call
	int32_t ret = udp_sendrecv(nfsmount, offset, nfsmount->nfs_port);
	if (ret < 0) return ret;

	int32_t rpc_header_length = 0;
	if (rpc_parse_header(nfsmount, &rpc_header_length) != 0)
	{
		return -10;
	}

	// First, check the status
	int32_t intVal;
	offset = rpc_header_length;
	offset += rpc_read_int(nfsmount, offset, &intVal); // Status
	if (intVal != 0) return intVal;

	// Extract the handle from the return message
	offset += rpc_read_fhandle(nfsmount, offset, handle);

	offset += rpc_read_int(nfsmount, offset, &intVal); // Has object attributes
	if (intVal == 1 && attr != NULL) // And if we want them, ofc
	{
		rpc_read_stat(nfsmount, offset, attr); // Read object attributes
	}
	return 0;
}

int32_t _NFS_get_handle(struct _reent *r, NFSMOUNT *nfsmount, const char *path, char *pathEnd, fhandle3 *handle, int32_t only_directories)
{
	// First, check if the requested directory is by any chance the current directory
	if (nfsmount->curdirname != NULL && strncmp(nfsmount->curdirname, path, strlen(path)) == 0) {
		fhandle3_copy(handle, &nfsmount->curdir);
		return 0;
	}

	int32_t relative_dir = 1;

	// We should do lookups on all dirs in the path, to obtain the directory handles
	if (strchr(path, ':') != NULL) // Remove the partition name
	{
		relative_dir = 0;
		path = strchr(path, ':') + 1;
	}
	if (strchr (path, ':') != NULL) {
		r->_errno = EINVAL;
		return -1;
	}
	if (path[0] == '/') // Remove the starting slash, I don't want slashes when communication with the remote end
		path = path + 1;

	// Are we requesting the root directory? We already have that handle
	if (strlen(path) == 0) {
		fhandle3_copy(handle, &nfsmount->handle);
		return 0;
	}

	// Copy the source handle to the handle
	fhandle3 *src = relative_dir == 1 && nfsmount->curdir.val != NULL ? &nfsmount->curdir : &nfsmount->handle;
	fhandle3_copy(handle, src);

	// Allocate a new string, since we'll use strtok (and strtok changes strings)
	int32_t str = pathEnd != NULL ? pathEnd - path + 1 : strlen(path) + 1;
	char *input = (char *) _NFS_mem_allocate(str);
	memset(input, 0, sizeof(input));
	strncpy(input, path, str);
	char *dir = strtok(input, "/");

	struct stat obj_attr = {0};

	while (dir != NULL) {
		// Do a lookup on this dir
		fhandle3 newHandle = {0};

		// This method will allocate newHandle, we can free handle after using it
		int32_t ret = _NFS_do_lookup(nfsmount, handle, dir, &obj_attr, &newHandle);

		// Free the old handle
		fhandle3_free(handle);
		if (ret != 0 || (only_directories && !S_ISDIR(obj_attr.st_mode)))
		{
			fhandle3_free(&newHandle);
			_NFS_mem_free(input);
			r->_errno = ENOENT;
			return -1;
		}

		memcpy((void *) handle, (void *) &newHandle, sizeof(fhandle3));

		// Move the pointer to the start of the next dir
		dir = strtok(NULL, "/");
	}
	_NFS_mem_free(input);
	return 0;
}

int32_t _NFS_get_childs(struct _reent *r, NFSMOUNT *nfsmount, NFS_DIR_STATE_STRUCT *dir)
{
	return 0;
}

DIR_ITER * _NFS_diropen_r(struct _reent *r, DIR_ITER *dirState, const char *path)
{
	NFS_DIR_STATE_STRUCT *state = (NFS_DIR_STATE_STRUCT *) dirState->dirStruct;
	memset(state, 0, sizeof(NFS_DIR_STATE_STRUCT));
	state->nfsmount = _NFS_get_NfsMountFromPath(path);
	if (state->nfsmount == NULL) {
		r->_errno = ENODEV;
		return NULL;
	}

	_NFS_lock(&state->nfsmount->lock);

	// Allocate a new handle, since we need to retrieve the subdirectories one by one
	fhandle3 handle = {0};
	int32_t ret = _NFS_get_handle(r, state->nfsmount, path, NULL, &handle, 1);

	if (ret == 0) memcpy((void *) &state->handle, (void *) &handle, sizeof(fhandle3));

	_NFS_unlock(&state->nfsmount->lock);

	return dirState;
}

int32_t _NFS_dirclose_r(struct _reent *r, DIR_ITER *dirState)
{
	int32_t i;

	// Free memory for this directory
	NFS_DIR_STATE_STRUCT *state = (NFS_DIR_STATE_STRUCT *) dirState->dirStruct;
	_NFS_lock(&state->nfsmount->lock);

	fhandle3_free(&state->handle);
	if (state->childs != NULL)
	{
		// Free all childs
		for (i = 0; i < state->numchilds; i++)
		{
			fhandle3_free(&state->childs[i].handle);
			if (state->childs[i].name) _NFS_mem_free(state->childs[i].name);
		}
		_NFS_mem_free(state->childs);
	}

	state->childs = NULL;
	state->numchilds = 0;

	_NFS_unlock(&state->nfsmount->lock);

	return 0;
}

int32_t _NFS_chdir_r (struct _reent *r, const char *path)
{
	NFSMOUNT *nfsmount = _NFS_get_NfsMountFromPath(path);
	if (nfsmount == NULL) {
		r->_errno = ENODEV;
		return -1;
	}

	_NFS_lock(&nfsmount->lock);

	// Allocate a new handle, since we need to retrieve the subdirectories one by one
	fhandle3 handle = {0};
	int32_t ret = _NFS_get_handle(r, nfsmount, path, NULL, &handle, 1);

	if (ret == 0)
	{
		// Clear the curdir handle
		fhandle3_free(&nfsmount->curdir);
		memcpy((void *) &nfsmount->curdir, (void *) &handle, sizeof(fhandle3));
		nfsmount->curdirname = _NFS_mem_reallocate(nfsmount->curdirname, strlen(path) + 1);
		memset(nfsmount->curdirname, 0, strlen(path) + 1);
		strncpy(nfsmount->curdirname, path, strlen(path));
	}
	_NFS_unlock(&nfsmount->lock);

	return ret;
}

int32_t _NFS_dirreset_r (struct _reent *r, DIR_ITER *dirState)
{
	NFS_DIR_STATE_STRUCT *state = (NFS_DIR_STATE_STRUCT *) dirState->dirStruct;
	_NFS_lock(&state->nfsmount->lock);
	state->current_child = 0;
	_NFS_unlock(&state->nfsmount->lock);
	
	return 0;
}

extern int32_t _nfs_buffer_size;

// Does a single call to readdirplus, meaning that you get some entries, but not always all
// The starting point32_t will be defined by the cookie property of the state
int32_t _NFS_readdirplus_single(NFSMOUNT *nfsmount, NFS_DIR_STATE_STRUCT *state)
{
	int32_t headerSize = rpc_create_header(nfsmount, PROGRAM_NFS, 3, PROCEDURE_READDIRPLUS, AUTH_UNIX);

	// Write a dir entry, first write the handle
	uint32_t offset = headerSize;
	offset += rpc_write_fhandle(nfsmount, offset, &state->handle);
	offset += rpc_write_long(nfsmount, offset, state->cookie);
	offset += rpc_write_long(nfsmount, offset, 0); // Verifier data
	offset += rpc_write_int(nfsmount, offset, 0); // Dir count

	int32_t max_len = nfsmount->dtpref;
	if (max_len == 0 || max_len > nfsmount->bufferlen) max_len = nfsmount->bufferlen - offset;
	offset += rpc_write_int(nfsmount, offset, max_len); // Max size of message

	int32_t ret = udp_sendrecv(nfsmount, offset, nfsmount->nfs_port);
	if (ret < 0) return ret;

	int32_t rpc_header_length = 0;
	ret = rpc_parse_header(nfsmount, &rpc_header_length);
	if (ret < 0) {
		return ret;
	}

	int32_t boolval = 0;

	// Read the entries
	offset = rpc_header_length;
	offset += rpc_read_int(nfsmount, offset, &boolval);
	if (boolval != 0) // NFS Status
	{
		return -1; // Invalid status
	}
	offset += rpc_read_int(nfsmount, offset, &boolval);
	if (boolval == 1) { // Does the data contain dir_attributes
		offset += rpc_read_objectattr(nfsmount, offset, NULL); // I don't need this information
	}
	offset += 8; // 8 bytes for the verifier data (which is null and has a length 0)

	NFS_DIR_CHILD child;

	while (true) {
		offset += rpc_read_int(nfsmount, offset, &boolval); // Has entry value
		if (boolval == 0) {
			break; // No more values found, we need to check for EOF after this while
		}

		memset(&child, 0, sizeof(NFS_DIR_CHILD));

		// Read entry
		offset += 8; // Skip fileid
		offset += rpc_read_string(nfsmount, offset, &child.name); // Extract the name
		offset += rpc_read_long(nfsmount, offset, (long long *) &state->cookie); // Extract the cookie (required for the consequent call, it tells the server where to continue)
		offset += rpc_read_int(nfsmount, offset, &boolval); // Has attributes
		if (boolval == 1) {
			offset += rpc_read_stat(nfsmount, offset, &child.stat); // Read the attributes as stat
		}
		offset += rpc_read_int(nfsmount, offset, &boolval); // Has file handle
		if (boolval == 1) {
			offset += rpc_read_fhandle(nfsmount, offset, &child.handle); // Read the file handle

			// I need a handle in order to be able to do something with this child, so I'll add the child here to the list
			if (state->handle.len == child.handle.len) {
				if (memcmp(&state->handle, &child.handle, state->handle.len + 4) == 0) {
					continue; // This handle is the same as the parents handle, ignore it
				}
			}


			// Increase buffer
			state->numchilds++;
			state->childs = _NFS_mem_reallocate(state->childs, state->numchilds * sizeof(NFS_DIR_CHILD));
			NFS_DIR_CHILD *newChild = &state->childs[state->numchilds - 1];
			memcpy(newChild, &child, sizeof(NFS_DIR_CHILD));
		}
	}
	offset += rpc_read_int(nfsmount, offset, &state->is_completed); // Read the EOF marker (if 0, then you need subsequent replies)
	return 0;
}

// This method is not really used, but it can be used to download all children of a given directory
int32_t _NFS_readdirplus_all(NFSMOUNT *nfsmount, NFS_DIR_STATE_STRUCT *state)
{
	int32_t ret = 0;
	while(!state->is_completed) {
		if ((ret = _NFS_readdirplus_single(nfsmount, state)) < 0) {
			return ret;
		}
	}
	return 0;
}

int32_t _NFS_dirnext_r (struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat)
{
	NFS_DIR_STATE_STRUCT *state = (NFS_DIR_STATE_STRUCT *) dirState->dirStruct;
	_NFS_lock(&state->nfsmount->lock);

	// You've requested a child that's not there, if we've not completed downloading, download the next "batch" of childs
	if (state->current_child >= state->numchilds && !state->is_completed) {
		if (_NFS_readdirplus_single(state->nfsmount, state)) {
			_NFS_unlock(&state->nfsmount->lock);
			r->_errno = ENOENT;
			return -1;
		}
	}

	// We already downloaded the whole directory, and we don't have any more childs.
	if (state->current_child >= state->numchilds && state->is_completed) { 
		_NFS_unlock(&state->nfsmount->lock);
		r->_errno = ENOENT;
		return -1;
	}

	strncpy(filename, state->childs[state->current_child].name, MAX_FILENAME_LENGTH);
	if (filestat != NULL) {
		memcpy(filestat, &state->childs[state->current_child].stat, sizeof(struct stat));
	}

	state->current_child++;
	_NFS_unlock(&state->nfsmount->lock);
	
	return 0;
}

char *_NFS_get_dir_handle(struct _reent *r, NFSMOUNT *nfsmount, const char *path, fhandle3 *handle)
{
	char *lastPart = strrchr(path, '/');
	if (lastPart == NULL) {
		// No path was specified
		if (nfsmount->curdir.val == NULL) {
			r->_errno = ENOTDIR;
			return NULL;
		}
		handle = &nfsmount->curdir;
		lastPart = (char *) path;
	} else {
		// Get stuff from here
		if (_NFS_get_handle(r, nfsmount, path, lastPart, handle, 1) < 0) {
			r->_errno = ENOTDIR;
			return NULL;
		}
		// Move the lastPart past the last seperator
		lastPart += 1;
	}
	return lastPart;
}

int32_t _NFS_mkdir_r (struct _reent *r, const char *path, int32_t mode)
{
	NFSMOUNT *nfsmount = _NFS_get_NfsMountFromPath(path);
	if (nfsmount == NULL) {
		r->_errno = ENODEV;
		return -1;
	}
	if (nfsmount->readonly) {
		r->_errno = EROFS;
		return -1;
	}

	_NFS_lock(&nfsmount->lock);

	fhandle3 parentHandle = {0};

	// Get the directory it has to go in
	char *lastPart = _NFS_get_dir_handle(r, nfsmount, path, &parentHandle);
	if (lastPart == NULL) {
		_NFS_unlock(&nfsmount->lock);
		return -1;
	}

	// Create the MKDIR message
	int32_t headerSize = rpc_create_header(nfsmount, PROGRAM_NFS, 3, PROCEDURE_MKDIR, AUTH_UNIX);

	// Write a dir entry, first write the handle
	uint32_t offset = headerSize;
	offset += rpc_write_fhandle(nfsmount, offset, &parentHandle);
	offset += rpc_write_string(nfsmount, offset, lastPart);

	sattr3 attr = {0};
	attr.setmode = 1;
	attr.mode = mode;
	attr.setuid = 1;
	attr.uid = nfsmount->uid;
	attr.setgid = 1;
	attr.gid = nfsmount->gid;
	attr.setsize = 0;
	attr.setatime = TIME_SERVER;
	attr.setmtime = TIME_SERVER;

	offset += rpc_write_sattr(nfsmount, offset, &attr);

	int32_t ret = udp_sendrecv(nfsmount, offset, nfsmount->nfs_port);
	if (ret < 0) {
		_NFS_unlock(&nfsmount->lock);
		return ret;
	}

	int32_t rpc_header_length = 0;
	ret = rpc_parse_header(nfsmount, &rpc_header_length);
	if (ret < 0) {
		_NFS_unlock(&nfsmount->lock);
		return ret;
	}

	// I have to do something here? Not sure ;)
	rpc_read_int(nfsmount, rpc_header_length, &r->_errno);

	_NFS_unlock(&nfsmount->lock);

	return r->_errno == 0 ? 0 : -1;
}
