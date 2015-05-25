/* $OpenBSD: netcat.c,v 1.103 2011/10/04 08:34:34 fgsch Exp $ */
/*
 * Copyright (c) 2001 Eric Jackson <ericj@monkey.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Re-written nc(1) for OpenBSD. Original implementation by
 * *Hobbit* <hobbit@avian.org>.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

int SoundIP::getstream()
{
	struct addrinfo hints;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = portmode_udp ? SOCK_DGRAM : SOCK_STREAM;
	hints.ai_protocol = portmode_udp ? IPPROTO_UDP : IPPROTO_TCP;

	if (portmode_udp) {
		hints.ai_flags = AI_PASSIVE;
		udp_connected = false;
		stream = remote_connect(NULL, m_port, hints);
	} else
		stream = remote_connect(m_host, m_port, hints);
	return stream;
}

void SoundIP::closestream()
{
	if (stream >= 0)
		close(stream);
	stream = -1;
}

/*
 * remote_connect()
 * Returns a socket connected to a remote host. Properly binds to a local
 * port or source address if needed. Returns -1 on failure.
 */
int
SoundIP::remote_connect(const char *host, const char *port, struct addrinfo hints)
{
	struct addrinfo *res, *res0;
	int s, x, ret;

	if ( getaddrinfo(host, port, &hints, &res) )
		return -1;

	res0 = res;
	do {
		if ((s = socket(res0->ai_family, res0->ai_socktype,
		    res0->ai_protocol)) < 0)
			continue;

		if (portmode_udp) {
			x = 1;
			ret = setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &x, sizeof(x));
			if (ret > -1)
				if (bind(s, (struct sockaddr *)res0->ai_addr, res0->ai_addrlen) == 0)
					break;
		} else {
			if (timeout_connect(s, res0->ai_addr, res0->ai_addrlen) == 0)
				break;
		}

		close(s);
		s = -1;
	} while ((res0 = res0->ai_next) != NULL);

	freeaddrinfo(res);

	return (s);
}

int
SoundIP::timeout_connect(int s, const struct sockaddr *name, socklen_t namelen)
{
	struct pollfd pfd;
	socklen_t optlen;
	int ret, optval;

	if ((ret = connect(s, name, namelen)) != 0 && errno == EINPROGRESS) {
		pfd.fd = s;
		pfd.events = POLLOUT;
		if ((ret = poll(&pfd, 1, -1)) == 1) {
			optlen = sizeof(optval);
			if ((ret = getsockopt(s, SOL_SOCKET, SO_ERROR,
			    &optval, &optlen)) == 0) {
				ret = optval == 0 ? 0 : -1;
			}
		} else
			return -1;
	}

	return (ret);
}

/*
 * readport()
 * Loop that polls on the network file descriptor
 */
int
SoundIP::readstream(uint8_t *buffer)
{
	struct pollfd pfd;
	int n, plen, rv;
	struct  sockaddr_storage z;
	socklen_t len = sizeof(z);

	plen = 4096;

	if (stream < 0)
		return 0;

	if (portmode_udp) {
		rv = recvfrom(stream, buffer, plen, MSG_PEEK | MSG_DONTWAIT, (struct sockaddr *)&z, &len);
		if (rv < 0)
			return 0;
		//rv = connect(stream, (struct sockaddr *)&z, len);
		//udp_connected = true;
		return ( read(stream, buffer, plen) );
	}

	/* Setup Network FD */
	pfd.fd = stream;
	pfd.events = POLLIN;

	n = poll(&pfd, 1, -1);
	if (n  <= 0) {
		closestream();
		return n;
	}

	if (pfd.revents & POLLIN) {
		n = read(stream, buffer, plen);
		if (n == 0) {
			closestream();
		}
	} else
		n = 0;
	return n;
}
