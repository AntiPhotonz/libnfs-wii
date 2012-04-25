/*
	libnfs.c
	Simple functionality for mounting and unmounting of NFS-based connections.

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

#include <sys/iosupport.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "nfs_net.h"
#include "nfs_dir.h"
#include "nfs_file.h"
#include "portmap.h"

uint16_t _nfs_clientport = 600;
int32_t _nfs_buffer_size = 8192;
int16_t _nfs_portmapper_port = 111;

static const devoptab_t dotab_nfs = {
	"nfs",
	sizeof (NFS_FILE_STRUCT),
	_NFS_open_r,
	_NFS_close_r,
	_NFS_write_r,
	_NFS_read_r,
	_NFS_seek_r,
	NULL, // No fstat
	_NFS_stat_r,
	NULL, //_NFS_link_r, // TODO
	_NFS_unlink_r,
	_NFS_chdir_r,
	_NFS_rename_r,
	_NFS_mkdir_r,
	sizeof (NFS_DIR_STATE_STRUCT),
	_NFS_diropen_r,
	_NFS_dirreset_r,
	_NFS_dirnext_r,
	_NFS_dirclose_r,
	NULL, //_NFS_statvfs_r,
	NULL, //_NFS_ftruncate_r,
	NULL, //_NFS_fsync_r,
	NULL,	/* Device data */
	NULL,
	NULL
};

bool nfsMount(const char *name, const char *ipAddress, const char *mountdir)
{
	NFSMOUNT *nfsmount = NULL;
	devoptab_t* devops;

	if (!name || strlen(name) > 8 || !mountdir) return false;

	char devname[10];
	sprintf(devname, "%s:", name);

	if(FindDevice(devname) >= 0)
		return true;

	int32_t struclen = sizeof(devoptab_t) + strlen(name) + 1;
	devops = _NFS_mem_allocate(struclen);
	if (!devops) {
		return false;
	}
	memset(devops, 0, struclen);

        nfsmount = _NFS_mem_allocate(sizeof(NFSMOUNT));
        if (!nfsmount) return NULL;
	memset(nfsmount, 0, sizeof(NFSMOUNT));
	nfsmount->socket = -1;

	udp_init(nfsmount, ipAddress, _nfs_clientport++);

	// Use the space allocated at the end of the devoptab struct for storing the name
	char *nameCopy = (char*)(devops+1);

	if (rpc_mount(nfsmount, mountdir) != 0) goto error;

	// Add an entry for this device to the devoptab table
	memcpy (devops, &dotab_nfs, sizeof(dotab_nfs));
	strncpy(nameCopy, name, strlen(name));
	devops->name = nameCopy;
	devops->deviceData = nfsmount;

	AddDevice(devops);

	goto finish;
error:
	_NFS_mem_free(devops);
	if (!nfsmount) _NFS_mem_free(nfsmount);
	_nfs_clientport--;
	return false;
finish:
	return true;
}

void nfsUnmount(const char *name)
{
	NFSMOUNT *nfsmount;
	devoptab_t* devops;

	if (!name) return;

	devops = (devoptab_t *) GetDeviceOpTab(name);
	if (!devops) return;

	// Perform a quick check to make sure we're dealing with a libnfs controlled network location
	if (devops->open_r != dotab_nfs.open_r) {
		return;
	}

	nfsmount = (NFSMOUNT*)devops->deviceData;
	rpc_unmount(nfsmount);

	// Clear the buffer
	if (nfsmount->buffer) _NFS_mem_free(nfsmount->buffer);
	nfsmount->buffer = NULL;
	nfsmount->bufferlen = 0;

	_NFS_mem_free(nfsmount);
	_NFS_mem_free(devops);

	RemoveDevice(name);
}
