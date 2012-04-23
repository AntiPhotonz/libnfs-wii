/*
 portmap.c for libnfs

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
#include "nfs_net.h"
#include "portmap.h"
#include "rpc.h"

#define PROTO_UDP 17

extern uint16_t _nfs_portmapper_port;

uint16_t portmap_find_port(NFSMOUNT *nfsmount, uint16_t *port, u32 program)
{
	int headerSize = rpc_create_header(nfsmount, PROGRAM_PORTMAP, 2, PROCEDURE_GETPORT, AUTH_NULL);

	uint32_t *buf = (uint32_t *) (nfsmount->buffer + headerSize);
	buf[0] = program;
	buf[1] = 3;
	buf[2] = PROTO_UDP;
	buf[3] = 0;

	int32_t ret = udp_sendrecv(nfsmount, headerSize + 16, _nfs_portmapper_port); // Portmapper listens on port 111
	if (ret < 0)
	{
		return -1;
	}

	int32_t rpc_header_length = 0;
	if (rpc_parse_header(nfsmount, &rpc_header_length) != 0)
	{
		return -2;
	}
	*port = *(uint32_t *) (nfsmount->buffer + rpc_header_length);
	return 0;
}

uint16_t portmap_find_mount_port(NFSMOUNT *nfsmount)
{
	return portmap_find_port(nfsmount, &nfsmount->mount_port, PROGRAM_MOUNT);
}

uint16_t portmap_find_nfs_port(NFSMOUNT *nfsmount)
{
	return portmap_find_port(nfsmount, &nfsmount->nfs_port, PROGRAM_NFS);
}
