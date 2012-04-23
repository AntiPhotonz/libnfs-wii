/*
 nfs_dir.h for libnfs

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

#ifndef _NFS_DIR_
#define _NFS_DIR_

#include <sys/iosupport.h>
#include "rpc_mount.h"
#include "nfs_file.h"

int32_t _NFS_get_handle(struct _reent *r, NFSMOUNT *nfsmount, const char *path, char *pathEnd, fhandle3 *handle, int32_t only_directories);
char *_NFS_get_dir_handle(struct _reent *r, NFSMOUNT *nfsmount, const char *path, fhandle3 *handle);
int32_t _NFS_do_lookup(NFSMOUNT *nfsmount, fhandle3 *parentHandle, const char *dir, struct stat *attr, fhandle3 *handle);

DIR_ITER * _NFS_diropen_r(struct _reent *r, DIR_ITER *dirState, const char *path);
int32_t _NFS_dirreset_r (struct _reent *r, DIR_ITER *dirState);
int32_t _NFS_dirnext_r (struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat);
int32_t _NFS_dirclose_r(struct _reent *r, DIR_ITER *dirState);

int32_t _NFS_chdir_r (struct _reent *r, const char *path);
int32_t _NFS_mkdir_r (struct _reent *r, const char *path, int32_t mode);

#endif //_NFS_DIR_
