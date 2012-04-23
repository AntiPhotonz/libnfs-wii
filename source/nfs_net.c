/*
 nfs_net.c for libnfs

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
#include <network.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "nfs_net.h"

#define IOS_O_NONBLOCK 0x04

static int32_t sock = -1;
static struct sockaddr_in remote;
int32_t udp_retries = 2;

int32_t udp_init(const char *server, uint16_t clientport)
{
	struct sockaddr_in client;
	
	sock = net_init();

	if (sock < 0)
	{
		return sock;
	}

	uint32_t  ip = net_gethostip();
	struct in_addr addr;
	addr.s_addr = ip;

	sock = net_socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		return -2;
	}

	memset(&remote, 0, sizeof(struct sockaddr));
	remote.sin_family = AF_INET;
	remote.sin_addr.s_addr = inet_addr((char *) server);

	memset(&client, 0, sizeof(struct sockaddr));
	client.sin_family = AF_INET;
	client.sin_port = clientport;
	client.sin_addr.s_addr = INADDR_ANY;

	int32_t ret = net_bind(sock, (struct sockaddr*) &client, sizeof(struct sockaddr));
	if (ret < 0)
	{
		return -4;
	}

	int32_t flags = net_fcntl(sock, F_GETFL, 0);
	net_fcntl(sock, F_SETFL, flags | IOS_O_NONBLOCK); 

	return 0;
}

int32_t udp_sendrecv(NFSMOUNT *nfsmount, uint32_t sendbuflen, uint16_t port)
{
	int32_t ret = -1;
	remote.sin_port = port;

	uint32_t length = sizeof(struct sockaddr);
	struct sockaddr from = {0};

	ret = net_connect(sock, (struct sockaddr*)&remote, sizeof(struct sockaddr));
	if (ret < 0)
	{
		return -3;
	}

	int32_t retr = 0;
	while (retr < udp_retries)
	{
		// We should resend after 500 ms
		ret = net_sendto(sock, nfsmount->buffer, sendbuflen, 0, (struct sockaddr *) &remote, sizeof(struct sockaddr));
		if (ret < 0)
		{
			return -1;
		}

		int32_t rec = 0;
		while (rec < 1000)
		{
			ret = net_recvfrom(sock, nfsmount->buffer, nfsmount->bufferlen, 0, (struct sockaddr *) &from, &length);
			if (ret > 0)
			{
				return ret; // Message received!
			}
			usleep(500);
			rec++;
		}
		retr++;
	}

	// No message
	return -2;
}
