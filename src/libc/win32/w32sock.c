/*
 * Copyright 2005-2006,2015-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <process.h>
#include <assert.h>
#include <time.h>
#include <utime.h>
#include <signal.h>
#include <ctype.h>
#include <mswsock.h>

#define	NO_SOCKET_REMAPS
#include "w32sock.h"

void
nt_loadWinSock(void)
{
	WORD	version;
	WSADATA	wsaData;
	int	iSockOpt = SO_SYNCHRONOUS_NONALERT;
	static	int	winsock_loaded = 0;

	if (winsock_loaded) return;

	version = MAKEWORD(2, 2);
	/* make sure we get version 1.1 or higher */
	if (WSAStartup(version, &wsaData)) {
		fprintf(stderr, "Failed to load WinSock\n");
		exit(1);
	}
	/* Enable the use of sockets as filehandles */
	setsockopt(INVALID_SOCKET, SOL_SOCKET, SO_OPENTYPE,
	    (char *)&iSockOpt, sizeof(iSockOpt));

	winsock_loaded = 1;
}

#define	OPEN_SOCK(x)	_open_osfhandle((x),_O_RDWR|_O_BINARY)
#define TO_SOCK(x)	_get_osfhandle(x)
#define SOCK_TEST(x, y)	if((x) == (y)) errno = WSAGetLastError()
#define SOCK_TEST_ERROR(x) SOCK_TEST(x, SOCKET_ERROR)

int
nt_socket(int af, int type, int protocol)
{
	SOCKET	s;

	nt_loadWinSock();
	if ((s = socket(af, type, protocol)) == INVALID_SOCKET) {
		errno = WSAGetLastError();
	} else {
		s = OPEN_SOCK(s);
	}
	return (s);
}

int
nt_accept(int s, struct sockaddr *addr, int *addrlen)
{
    SOCKET	r;

    SOCK_TEST((r = accept(TO_SOCK(s), addr, addrlen)), INVALID_SOCKET);
    return (OPEN_SOCK(r));
}

int
nt_bind(int s, const struct sockaddr *addr, int addrlen)
{
	int	r;

	SOCK_TEST_ERROR(r = bind(TO_SOCK(s), addr, addrlen));
	return (r);
}

int
nt_connect(int s, const struct sockaddr *addr, int addrlen)
{
	int	r;

	SOCK_TEST_ERROR(r = connect(TO_SOCK(s), addr, addrlen));
	return (r);
}

int
nt_getpeername(int s, struct sockaddr *addr, int *addrlen)
{
	int	r;

	SOCK_TEST_ERROR(r = getpeername(TO_SOCK(s), addr, addrlen));
	return (r);
}

int
nt_getsockname(int s, struct sockaddr *addr, int *addrlen)
{
	int	r;

	SOCK_TEST_ERROR(r = getsockname(TO_SOCK(s), addr, addrlen));
	return (r);
}

int
nt_setsockopt(int s, int level, int optname, const char *optval, int optlen)
{
	int	r;

	SOCK_TEST_ERROR(r = setsockopt(TO_SOCK(s), level, optname,
	    optval, optlen));
	return (r);
}

int
nt_send(int s, const char *buf, int len, int flags)
{
	int	r;

	SOCK_TEST_ERROR(r = send(TO_SOCK(s), buf, len, flags));
	return (r);
}

int
nt_sendto(int s, const char *buf, int len, int flags,
	const struct sockaddr *dst, int dstlen)
{
	int	r;

	SOCK_TEST_ERROR(r = sendto(TO_SOCK(s), buf, len, flags, dst, dstlen));
	return (r);
}

int
nt_recv(int s, char *buf, int len, int flags)
{
	int	r;

	SOCK_TEST_ERROR(r = recv(TO_SOCK(s), buf, len, flags));
	return (r);
}

int
nt_recvfrom(int s, char *buf, int len, int flags,
	struct sockaddr *src, int *srclen)
{
	int	r;

	SOCK_TEST_ERROR(r = recvfrom(TO_SOCK(s), buf, len, flags, src, srclen));
	return (r);
}

int
nt_listen(int s, int backlog)
{
	int	r;

	SOCK_TEST_ERROR(r = listen(TO_SOCK(s), backlog));
	return (r);
}

int
nt_closesocket(int s)
{
	int	sock = TO_SOCK(s);

	close(s);
	return (closesocket(sock));
}

int
nt_shutdown(int s, int how)
{
	int	r;

	SOCK_TEST_ERROR(r = shutdown(TO_SOCK(s), how));
	return (r);
}

/*
 * Our own wrapper for winsock's select() function.  On Windows, select can
 * _only_ be used for sockets.  So we translate the filenames to socket
 * handles and then call the real select.
 */

static void
fdset2sock(fd_set *fds, int **map)
{
	int	i;
	int	n, o;
	int	*p = *map;

	for (i = 0; i < fds->fd_count; i++) {
		o = fds->fd_array[i];
		n = TO_SOCK(o);
		*p++ = n;
		*p++ = o;
		fds->fd_array[i] = n;
	}
	*map = p;
}

static void
sockset2fd(fd_set *fds, int *map)
{
	int	i, j;

	for (i = 0; i < fds->fd_count; i++) {
		for (j = 0; map[j]; j += 2) {
			if (map[j] == fds->fd_array[i]) {
				fds->fd_array[i] = map[j+1];
				break;
			}
		}
		assert(map[j]);
	}
}

int
nt_select(int n, fd_set *rfds, fd_set *wfds, fd_set *efds, struct timeval *t)
{
	int	map[2*64];
	int	*mapp = map;
	int	rc;

	if (rfds) fdset2sock(rfds, &mapp);
	if (wfds) fdset2sock(wfds, &mapp);
	if (efds) fdset2sock(efds, &mapp);

	assert((mapp - map) < sizeof(map)/sizeof(int));
	*mapp = 0;

	rc = select(n, rfds, wfds, efds, t);
	
	if (rfds) sockset2fd(rfds, map);
	if (wfds) sockset2fd(wfds, map);
	if (efds) sockset2fd(efds, map);
	
	return (rc);
}
