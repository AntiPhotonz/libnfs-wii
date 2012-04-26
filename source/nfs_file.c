
/*
 nfs_file.c for libnfs

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
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "rpc.h"
#include "rpc_mount.h"
#include "nfs_net.h"
#include "nfs_dir.h"
#include "nfs_file.h"

void _NFS_copy_stat(struct stat *dest, struct stat *src)
{
	memcpy(dest, src, sizeof(struct stat));
}

int32_t _NFS_stat_from_handle(struct _reent *r, NFSMOUNT *nfsmount, fhandle3 *handle, struct stat *st)
{
	int32_t headerSize = rpc_create_header(nfsmount, PROGRAM_NFS, 3, PROCEDURE_GETATTR, AUTH_UNIX);

	// Write a dir entry, first write the handle
	uint32_t offset = headerSize;
	offset += rpc_write_fhandle(nfsmount, offset, handle);

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

	rpc_read_int(nfsmount, rpc_header_length, &r->_errno);

	if (r->_errno == 0) {
		rpc_read_stat(nfsmount, rpc_header_length + 4, st);
	}

	_NFS_unlock(&nfsmount->lock);

	return r->_errno == 0 ? 0 : -1;
}

int32_t _NFS_open_r (struct _reent *r, void *fileStruct, const char *path, int32_t flags, int32_t mode)
{
	NFSMOUNT *nfsmount = _NFS_get_NfsMountFromPath(path);
	if (nfsmount == NULL) {
		r->_errno = ENODEV;
		return -1;
	}

	// Check if we are doing a Write on readonly partition
	if (((flags & O_WRONLY) || (flags & O_RDWR)) && nfsmount->readonly) {
		r->_errno = EROFS;
		return -1;
	}

	_NFS_lock(&nfsmount->lock);

	NFS_FILE_STRUCT* file = (NFS_FILE_STRUCT*) fileStruct;
	memset(file, 0, sizeof(NFS_FILE_STRUCT));
	file->nfsmount = nfsmount;

	// Some modes
	// O_RDONLY -> only allow read, block write calls
	// O_WRONLY -> only allow write, block read
	// O_RDWR -> allow both read and write
	file->read = ((mode & O_RDONLY) | (mode & O_RDWR)) != 0 ? 1 :0;
	file->write = ((mode & O_WRONLY) | (mode & O_RDWR)) != 0 ? 1 :0;
	file->append = (flags & O_CREAT) == O_CREAT && (flags & O_TRUNC) == 0; 

	fhandle3 baseDir = {0};
	char *filename = _NFS_get_dir_handle(r, nfsmount, path, &baseDir);
	if (filename == NULL) {
		_NFS_unlock(&nfsmount->lock);
		r->_errno = EACCES;
		return -1;
	}

	// First, get the attributes of the file
	int32_t exists = 1;
	struct stat attr = {0};

	// Do a lookup on the file, and a GETATTR call after that
	r->_errno = _NFS_do_lookup(file->nfsmount, &baseDir, filename, &attr, &file->handle);
	if (r->_errno != 0) {
		if (r->_errno != NFS3ERR_NOENT) {
			return -1;
		}
		r->_errno = exists = 0;
	}

	//If the file exists at this point, we're done
	if (file->handle.len > 0) {
		if ((flags & O_EXCL) == O_EXCL && (flags & O_CREAT) == O_CREAT) {
			_NFS_unlock(&file->nfsmount->lock);
			r->_errno = EEXIST;
			return -1;
		}
		file->size = (int32_t) attr.st_size;
		file->currentPosition = ((flags & O_APPEND) == O_APPEND) ? file->size : 0;
		if ((flags & O_TRUNC) == 0) { // If we need truncation, we need a create call
			_NFS_unlock(&file->nfsmount->lock);
			return 0;
		}
	}

	// Some flags here
	// O_CREAT -> Create if it doesn't exists
	// O_EXCL & O_CREAT -> Fail if the file exists, but don't overwrite!
	// O_TRUNK -> Truncate, so call a create, and overwrite the existing file
	// Which means:
	// - if ONLY O_CREAT is set call CREATE with GUARDED, but don't FAIL if EEXISTS is returned
	// - if O_EXCL is set, call CREATE with GUARDED, and FAIL if EEXISTS is returned
	// - if O_TRUNC is set, always call CREATE with UNGUARDED
	// All other scenarios, don't call CREAT, just do a lookup for the handle
	int32_t create_mode = -1;
	if ((flags == O_CREAT) || (flags & O_EXCL) == O_EXCL)
		create_mode = CREATE_GUARDED;
	if ((flags & O_TRUNC) == O_TRUNC)
		create_mode = CREATE_UNCHECKED;

	int32_t headerSize = rpc_create_header(nfsmount, PROGRAM_NFS, 3, create_mode == -1 ? PROCEDURE_LOOKUP : PROCEDURE_CREATE, AUTH_UNIX);

	// Write a dir entry, first write the handle
	uint32_t offset = headerSize;
	offset += rpc_write_fhandle(nfsmount, offset, &baseDir);
	offset += rpc_write_string(nfsmount, offset, filename);

	if (create_mode != -1) {
		// Do a create call
		file->isnew = 1;
		offset += rpc_write_int(nfsmount, offset, create_mode);

		sattr3 attr = {0};
		attr.setmode = 1;
		attr.mode = mode;
		attr.setuid = 1;
		attr.uid = nfsmount->uid;
		attr.setgid = 1;
		attr.gid = nfsmount->gid;
		attr.setsize = 1;
		attr.setatime = TIME_SERVER;
		attr.setmtime = TIME_SERVER;

		offset += rpc_write_sattr(nfsmount, offset, &attr);
	}

	int32_t ret = udp_sendrecv(nfsmount, offset, nfsmount->nfs_port);
	if (ret < 0) {
		_NFS_unlock(&nfsmount->lock);
		return ret;
	}

	int32_t rpc_header_length = 0;
	ret = rpc_parse_header(nfsmount, &rpc_header_length);
	if (ret < 0) {
		_NFS_unlock(&nfsmount->lock);
		return -1;
	}

	offset = rpc_header_length;
	offset += rpc_read_int(nfsmount, offset, &r->_errno);

	if (r->_errno == 0) {
		int intVal;
		offset += rpc_read_int(nfsmount, offset, &intVal);
		if (intVal) offset += rpc_read_fhandle(nfsmount, offset, &file->handle);

		offset += rpc_read_int(nfsmount, offset, &intVal);
		if (intVal) {
			struct stat attr;
			offset += rpc_read_stat(nfsmount, offset, &attr);
		
			file->size = attr.st_size;
		}
	}

	if (create_mode == -1 && (flags & O_APPEND) == O_APPEND) {
		file->currentPosition = file->size;
	}

	_NFS_unlock(&nfsmount->lock);

	return r->_errno == 0 ? 0 : -1;
}

int32_t _NFS_close_r (struct _reent *r, int32_t fd)
{
	NFS_FILE_STRUCT *file = (NFS_FILE_STRUCT *) fd;

	_NFS_lock(&file->nfsmount->lock);

	if (file->shouldcommit == 1) {
		// Do a COMMIT message
		int32_t headerSize = rpc_create_header(file->nfsmount, PROGRAM_NFS, 3, PROCEDURE_COMMIT, AUTH_UNIX);

		// Write a dir entry, first write the handle
		uint32_t offset = headerSize;
		offset += rpc_write_fhandle(file->nfsmount, offset, &file->handle);
		offset += rpc_write_long(file->nfsmount, offset, 0);
		offset += rpc_write_int(file->nfsmount, offset, 0);

		int32_t ret = udp_sendrecv(file->nfsmount, offset, file->nfsmount->nfs_port);
		if (ret < 0) {
			_NFS_unlock(&file->nfsmount->lock);
			return ret;
		}

		int32_t rpc_header_length = 0;
		ret = rpc_parse_header(file->nfsmount, &rpc_header_length);
		if (ret < 0) {
			_NFS_unlock(&file->nfsmount->lock);
			return -1;
		}

		int intVal;
		rpc_read_int(file->nfsmount, offset, &intVal);
		if (intVal != 0) {
			_NFS_unlock(&file->nfsmount->lock);
			return intVal;
		}
	}
	mutex_t lock = file->nfsmount->lock;

	memset(file, 0, sizeof(NFS_FILE_STRUCT));
	_NFS_unlock(&lock);

	return 0;
}

ssize_t _NFS_write_r (struct _reent *r, int32_t fd, const char *ptr, size_t len)
{
	NFS_FILE_STRUCT *file = (NFS_FILE_STRUCT *) fd;

	if (file == NULL || !file->write) {
		r->_errno = EBADF;
		return -1;
	}

	_NFS_lock(&file->nfsmount->lock);
	file->shouldcommit = 1;

	int32_t block_len = file->nfsmount->wtpref;
	// Calculate the best block_len, as specified by the server
	if (block_len > file->nfsmount->bufferlen) block_len = ((file->nfsmount->bufferlen - 1)/file->nfsmount->wtmult) * file->nfsmount->wtmult;

	int32_t amount_of_data_written = 0;
	int64_t verifier = 0;
	do
	{
		// Amount of data to read
		int32_t current_block = block_len;
		if (len - amount_of_data_written < current_block) current_block = len - amount_of_data_written;

		int32_t headerSize = rpc_create_header(file->nfsmount, PROGRAM_NFS, 3, PROCEDURE_WRITE, AUTH_UNIX);

		uint32_t offset = headerSize;
		offset += rpc_write_fhandle(file->nfsmount, offset, &file->handle);
		offset += rpc_write_long(file->nfsmount, offset, file->currentPosition);

		#if defined (__wii__)
		current_block = 4096 - (offset + 12); // I'll have to write less bytes (12 for the other header bytes, which we still have to write), max 4096
		if (len - amount_of_data_written < current_block) current_block = len - amount_of_data_written;
		#endif

		offset += rpc_write_int(file->nfsmount, offset, current_block);
		offset += rpc_write_int(file->nfsmount, offset, WRITE_UNSTABLE);		
		offset += rpc_write_block(file->nfsmount, offset, (void *) ptr + amount_of_data_written, current_block);

		int32_t ret = udp_sendrecv(file->nfsmount, offset, file->nfsmount->nfs_port);
		if (ret < 0) {
			_NFS_unlock(&file->nfsmount->lock);
			return ret;
		}

		int32_t rpc_header_length = 0;
		ret = rpc_parse_header(file->nfsmount, &rpc_header_length);
		if (ret < 0) {
			_NFS_unlock(&file->nfsmount->lock);
			return ret;
		}

		int32_t intVal, count;
		offset = rpc_header_length;
		offset += rpc_read_int(file->nfsmount, offset, &intVal);
		if (intVal != 0) {
			r->_errno = intVal;
			return -1;
		}

		// We will receive the weak cache consistency first, ignore it
		offset += rpc_read_int(file->nfsmount, offset, &intVal);
		if (intVal) offset += sizeof(object_attributes); // skip "before" object attributes
		offset += rpc_read_int(file->nfsmount, offset, &intVal);
		if (intVal) offset += sizeof(object_attributes); // skip "after" object attributes

		offset += rpc_read_int(file->nfsmount, offset, &count);
		offset += rpc_read_int(file->nfsmount, offset, &intVal); // Committed, which may be everything, since we use UNSAFE write (it must be intVal >= WRITE_MODE)

		int64_t newVerifier;
		offset += rpc_read_long(file->nfsmount, offset, &newVerifier);
		if (verifier == 0) verifier = newVerifier;
		else if (verifier != newVerifier) { // Server state changed between calls, that cannot be allowed!
			r->_errno=EIO;
			return -1;
		}

		amount_of_data_written += count;
		file->currentPosition += count;

		if (intVal) {
			break;
		}
	} while (amount_of_data_written < len);

	_NFS_unlock(&file->nfsmount->lock);

	return amount_of_data_written;
}

ssize_t _NFS_read_r (struct _reent *r, int32_t fd, char *ptr, size_t len)
{
	// We don't cache reads, since it'll cost more memory, just don't do random reads :)
	NFS_FILE_STRUCT *file = (NFS_FILE_STRUCT *) fd;
	_NFS_lock(&file->nfsmount->lock);

	int32_t block_len = file->nfsmount->rtpref;
	// Calculate the best block_len, as specified by the server
	if (block_len > file->nfsmount->bufferlen) block_len = ((file->nfsmount->bufferlen - 1)/file->nfsmount->rtmult) * file->nfsmount->rtmult;

	#if defined (__wii__)
	// Fake a block_len for now
	block_len =	8192 - 128; // This works for the wii, but 8192 bytes seems to be the max message size which is retrieved (128 is the max header for a READ reply)
	#endif

	int64_t amount_of_data_read = 0;
	do
	{
		// Amount of data to read
		int32_t current_block = block_len;
		if (len - amount_of_data_read < current_block) current_block = len - amount_of_data_read;

		int32_t headerSize = rpc_create_header(file->nfsmount, PROGRAM_NFS, 3, PROCEDURE_READ, AUTH_UNIX);

		uint32_t offset = headerSize;
		offset += rpc_write_fhandle(file->nfsmount, offset, &file->handle);
		offset += rpc_write_long(file->nfsmount, offset, file->currentPosition);
		offset += rpc_write_int(file->nfsmount, offset, current_block);

		int32_t ret = udp_sendrecv(file->nfsmount, offset, file->nfsmount->nfs_port);
		if (ret < 0) {
			_NFS_unlock(&file->nfsmount->lock);
			return ret;
		}

		int32_t rpc_header_length = 0;
		ret = rpc_parse_header(file->nfsmount, &rpc_header_length);
		if (ret < 0) {
			_NFS_unlock(&file->nfsmount->lock);
			return ret;
		}

		int32_t intVal, count;
		offset = rpc_header_length;
		offset += rpc_read_int(file->nfsmount, offset, &intVal);
		if (intVal != 0) {
			r->_errno = intVal;
			return -1;
		}

		offset += rpc_read_int(file->nfsmount, offset, &intVal);
		if (intVal) {
			offset += sizeof(object_attributes); // Let's skip the attributes for now
		}
		offset += rpc_read_int(file->nfsmount, offset, &count);
		offset += rpc_read_int(file->nfsmount, offset, &intVal); // EOF?

		// First, copy all data
		offset += 4; // Again a count? Weird...
		memcpy(ptr + amount_of_data_read, file->nfsmount->buffer + offset, count);

		amount_of_data_read += count;
		file->currentPosition += count;

		if (intVal) {
			break;
		}
	} while (amount_of_data_read < len);

	_NFS_unlock(&file->nfsmount->lock);

	return amount_of_data_read;
}

off_t _NFS_seek_r (struct _reent *r, int32_t fd, off_t pos, int32_t dir)
{
	NFS_FILE_STRUCT *file = (NFS_FILE_STRUCT *) fd;
	if (file->isnew && dir == SEEK_END) {
		r->_errno = EINVAL;
		return -1;
	}
	
	_NFS_lock(&file->nfsmount->lock);

	off_t newPos;

	switch(dir)
	{
		case SEEK_SET:
			newPos = pos;
			break;
		case SEEK_CUR:
			newPos = (off_t) file->currentPosition + pos;
			break;
		case SEEK_END:
			newPos = (off_t) file->size + pos;
			break;
		default:
			_NFS_unlock(&file->nfsmount->lock);
			r->_errno = EINVAL;
			return -1;
	}
	file->currentPosition = (uint32_t) newPos;
	_NFS_unlock(&file->nfsmount->lock);
	return (off_t) file->currentPosition;
}

int32_t _NFS_stat_r (struct _reent *r, const char *path, struct stat *st)
{
	NFSMOUNT *nfsmount = _NFS_get_NfsMountFromPath(path);
	if (nfsmount == NULL) {
		r->_errno = ENODEV;
		return -1;
	}

	_NFS_lock(&nfsmount->lock);

	// First, find the handle
	fhandle3 handle;
	if (_NFS_get_handle(r, nfsmount, path, NULL, &handle, 0) < 0) {
		_NFS_unlock(&nfsmount->lock);
		return -1;
	}

	return _NFS_stat_from_handle(r, nfsmount, &handle, st);
}
/*
int32_t _NFS_link_r (struct _reent *r, const char *existing, const char *newLink)
{

	return -1;
}
*/
int32_t _NFS_unlink_r (struct _reent *r, const char *name)
{
	NFSMOUNT *nfsmount = _NFS_get_NfsMountFromPath(name);
	if (nfsmount == NULL) {
		r->_errno = ENODEV;
		return -1;
	}
	if (nfsmount->readonly) {
		r->_errno = EROFS;
		return -1;
	}

	_NFS_lock(&nfsmount->lock);

	fhandle3 baseDir = {0};
	char *entryToDelete = _NFS_get_dir_handle(r, nfsmount, name, &baseDir);
	if (entryToDelete == NULL) {
		_NFS_unlock(&nfsmount->lock);
		return -1;
	}

	// Find out if the entry is a file or a directory
	struct stat attr;
	if (_NFS_stat_r(r, name, &attr) != 0) {
		return -1;
	}

	// If this is a directory, use procedure RMDIR, otherwise REMOVE
	int32_t headerSize = rpc_create_header(nfsmount, PROGRAM_NFS, 3, S_ISDIR(attr.st_mode) ? PROCEDURE_RMDIR : PROCEDURE_REMOVE, AUTH_UNIX);

	// Write a dir entry, first write the handle
	uint32_t offset = headerSize;
	offset += rpc_write_fhandle(nfsmount, offset, &baseDir);
	offset += rpc_write_string(nfsmount, offset, entryToDelete);

	int32_t ret = udp_sendrecv(nfsmount, offset, nfsmount->nfs_port);
	if (ret < 0) return ret;

	int32_t rpc_header_length = 0;
	ret = rpc_parse_header(nfsmount, &rpc_header_length);
	if (ret < 0) {
		return ret;
	}

	rpc_read_int(nfsmount, rpc_header_length, &r->_errno);

	_NFS_unlock(&nfsmount->lock);

	return r->_errno == 0 ? 0 : -1;
}

// This method can be optimized, by checking the directories before renaming
// If the directories are the same, then you don't have to retrieve the handle 
// for the new directory
int32_t _NFS_rename_r (struct _reent *r, const char *oldName, const char *newName)
{
	NFSMOUNT *nfsmount = _NFS_get_NfsMountFromPath(oldName);
	if (nfsmount == NULL) {
		r->_errno = ENODEV;
		return -1;
	}
	if (nfsmount->readonly) {
		r->_errno = EROFS;
		return -1;
	}

	_NFS_lock(&nfsmount->lock);

	fhandle3 oldDir = {0}, newDir = {0};
	char *oldFile = _NFS_get_dir_handle(r, nfsmount, oldName, &oldDir);
	if (oldFile == NULL) {
		_NFS_unlock(&nfsmount->lock);
		return -1;
	}
	char *newFile = _NFS_get_dir_handle(r, nfsmount, newName, &newDir);
	if (newFile == NULL) {
		_NFS_unlock(&nfsmount->lock);
		return -1;
	}

	// Create the MKDIR message
	int32_t headerSize = rpc_create_header(nfsmount, PROGRAM_NFS, 3, PROCEDURE_RENAME, AUTH_UNIX);

	// Write a dir entry, first write the handle
	uint32_t offset = headerSize;
	offset += rpc_write_fhandle(nfsmount, offset, &oldDir);
	offset += rpc_write_string(nfsmount, offset, oldFile);
	offset += rpc_write_fhandle(nfsmount, offset, &newDir);
	offset += rpc_write_string(nfsmount, offset, newFile);

	int32_t ret = udp_sendrecv(nfsmount, offset, nfsmount->nfs_port);
	if (ret < 0) return ret;

	int32_t rpc_header_length = 0;
	ret = rpc_parse_header(nfsmount, &rpc_header_length);
	if (ret < 0) {
		return ret;
	}

	rpc_read_int(nfsmount, rpc_header_length, &r->_errno);

	_NFS_unlock(&nfsmount->lock);

	return r->_errno == 0 ? 0 : -1;
}

void _NFS_copy_stat_from_attributes(struct stat *dest, object_attributes *attr)
{
	memset(dest, 0, sizeof(struct stat));
	dest->st_ino = attr->fileid;
	dest->st_mode = attr->mode;
	dest->st_nlink = attr->nlink;
	dest->st_uid = attr->uid;
	dest->st_gid = attr->gid;
	dest->st_rdev = attr->rdev;
	if (sizeof(size_t) == 4)
	{
		dest->st_size = attr->size_u != 0 ? -1 : attr->size_l;
	} else {
		dest->st_size = ((int64_t) attr->size_u << 32) | attr->size_u;
	}
	dest->st_atime = attr->atime;
	dest->st_mtime = attr->mtime;
	dest->st_ctime = attr->ctime;
}
