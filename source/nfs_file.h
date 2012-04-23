/*
 nfs_file.h for libnfs

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

#ifndef _NFS_FILE_
#define _NFS_FILE_

#include <sys/stat.h>
#include "common.h"

#define CREATE_UNCHECKED	0
#define CREATE_GUARDED		1
#define CREATE_EXCLUSIVE	2

int32_t _NFS_open_r (struct _reent *r, void *fileStruct, const char *path, int32_t flags, int32_t mode);
int32_t _NFS_close_r (struct _reent *r, int32_t fd);
ssize_t _NFS_write_r (struct _reent *r,int32_t fd, const char *ptr, size_t len);
ssize_t _NFS_read_r (struct _reent *r, int32_t fd, char *ptr, size_t len);
off_t _NFS_seek_r (struct _reent *r, int32_t fd, off_t pos, int32_t dir);
int32_t _NFS_stat_r (struct _reent *r, const char *path, struct stat *st);
//int32_t _NFS_link_r (struct _reent *r, const char *existing, const char *newLink);
int32_t _NFS_unlink_r (struct _reent *r, const char *name);
int32_t _NFS_rename_r (struct _reent *r, const char *oldName, const char *newName);

void _NFS_copy_stat_from_attributes(struct stat *dest, object_attributes *attr);
void _NFS_copy_stat(struct stat *dest, struct stat *src);

#endif // _NFS_FILE_
