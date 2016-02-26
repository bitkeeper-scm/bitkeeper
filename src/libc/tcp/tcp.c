/*
 * Copyright 1999-2007,2011 BitMover, Inc
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

/*
 * tcp_lib.c - routines for managing TCP connections.
 */

#include "system.h"

/*
 * Get a TCP socket, bind it, figure out the port,
 * and advertise the port as program "prog".
 */
int
tcp_server(char *addr, int port, int quiet)
{
	int	sock;
	struct	sockaddr_in s;

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		if (!quiet) perror("socket server");
		return (-1);
	}
	memset((void*)&s, 0, sizeof(s));
	s.sin_family = AF_INET;
	s.sin_port = htons(SOCK_PORT_CAST port);

	/* inet_aton would be a better choice because -1 is indeed
	 * a valid IP address (255.255.255.255), but inet_addr exists
	 * on windows and inet_aton does not.
	 */
	if (addr) s.sin_addr.s_addr = inet_addr(addr);
	if (s.sin_addr.s_addr == -1) {
		if (!quiet) fprintf(stderr, "%s: invalid IP address\n", addr);
		return (-1);
	}
	if (port) tcp_reuse(sock);
	tcp_keepalive(sock);
	if (bind(sock, (struct sockaddr*)&s, sizeof(s)) < 0) {
		if (!quiet) perror("bind");
		return (-2);
	}
	if (listen(sock, 100) < 0) {
		if (!quiet) perror("listen");
		return (-3);
	}
	return (sock);
}

/*
 * Accept a connection and return it
 */
int
tcp_accept(int sock)
{
	struct	sockaddr_in s;
	int	newsock;
	int	namelen;

	namelen = sizeof(s);
	memset((void*)&s, 0, namelen);

retry:
	if ((newsock = accept(sock, (struct sockaddr*)&s, &namelen)) < 0) {
		if (errno == EINTR)
			goto retry;
		perror("accept");
		return (-1);
	}
	tcp_keepalive(newsock);
	return (newsock);
}

/*
 * Connect to the TCP socket advertised as "port" on "host" and
 * return the connected socket.
 */
int
tcp_connect(char *host, int port)
{
	struct	hostent *h;
	struct	sockaddr_in s;
	int	sock;
	char	*freeme;

	if (getenv("_BK_TCP_CONNECT_FAIL") && !streq(host, "127.0.0.1")) {
		return (-3);
	}
	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket connect");
		return (-1);
	}
	if (freeme = strrchr(host, ':')) {
		host = strdup(host);
		freeme = strrchr(host, ':');
		*freeme++ = 0;
		port = atoi(freeme);
		freeme = host;
	}
	if (!(h = gethostbyname(host))) {
		if (freeme) free(freeme);
		closesocket(sock);
		return (-2);
	}
	memset((void *) &s, 0, sizeof(s));
	s.sin_family = AF_INET;
	memmove((void *)&s.sin_addr, (void*)h->h_addr, h->h_length);
	s.sin_port = SOCK_PORT_CAST htons(SOCK_PORT_CAST port);
	if (connect(sock, (struct sockaddr*)&s, sizeof(s)) < 0) {
		//fprintf(stderr, "connect(%s:%u) failed.\n", host, port);
		closesocket(sock);
		if (freeme) free(freeme);
		return (-3);
	}
	tcp_keepalive(sock);
	if (freeme) free(freeme);
	return (sock);
}

int
sockport(int s)
{
	int	namelen;
	struct sockaddr_in sin;

	namelen = sizeof(sin);
	if (getsockname(s, (struct sockaddr *)&sin, &namelen) < 0) {
		perror("getsockname");
		return(-1);
	}
	return ((int)ntohs(sin.sin_port));
}

char *
peeraddr(int s)
{
	int	namelen;
	struct sockaddr_in sin;

	namelen = sizeof(sin);
	if (getpeername(s, (struct sockaddr *)&sin, &namelen) < 0) {
		perror("getpeername");
		return(0);
	}
	return (inet_ntoa(SOCK_ADDR_CAST sin.sin_addr));
}

char *
sockaddr(int s)
{
	int	namelen;
	struct sockaddr_in sin;

	namelen = sizeof(sin);
	if (getsockname(s, (struct sockaddr *)&sin, &namelen) < 0) {
		perror("getsockname");
		return(0);
	}
	return (inet_ntoa(SOCK_ADDR_CAST sin.sin_addr));
}

int
issock(int s)
{
        int rc, t = 1;

        rc = getsockopt(s, SOL_SOCKET, SO_TYPE, SOCK_OPT_CAST &t, &t);
        if (rc) return (0);
        return (1);
}

int
isLocalHost(char *h)
{
	unless (h) return (0);
	return (streq("localhost", h) ||
	    streq("localhost.localdomain", h) ||
	    strneq("127.", h, 4)); /* localhost == 127.0.0.0/8 */
}


char	*
hostaddr(char *host)
{
	struct	hostent *h;
	struct	in_addr a;
#ifdef	h_addr
	int	i;
#endif

	if (!(h = gethostbyname(host))) return (0);
#ifndef	h_addr
	memmove(&a, h->h_addr, h->h_length);
	return (inet_ntoa(a));
#else
	for (i = 0; h->h_addr_list[i]; i++) {
		char	*p;

		memmove(&a, h->h_addr_list[i], h->h_length);
		p = inet_ntoa(a);
		unless (isLocalHost(p)) return (p);
	}
	return (inet_ntoa(a));	// shrug, have to do something.
#endif
}

/*
 * Emulate socketpair.
 */
int
tcp_pair(int fds[2])
{
	int	fd;

	if ((fd = tcp_server(0, 0, 0)) == -1) {
		perror("tcp_server");
		fds[0] = fds[1] = -1;
		return (-1);
	}
	fds[0] = fd;
	if ((fd = tcp_connect("127.0.0.1", sockport(fds[0]))) == -1) {
		perror("tcp_connect");
		closesocket(fd);
		fds[0] = fds[1] = -1;
		return (-1);
	}
	fds[1] = fd;
	if ((fd = tcp_accept(fds[0])) == -1) {
		perror("tcp_accept");
		closesocket(fds[0]);
		closesocket(fds[1]);
		fds[0] = fds[1] = -1;
		return (-1);
	}
	closesocket(fds[0]);
	fds[0] = fd;
	return (0);
}

void
tcp_ndelay(int sock, int val)
{
#ifdef	TCP_NODELAY
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
	    SOCK_OPT_CAST  &val, sizeof(val))) {
		fprintf(stderr, "TCP_NODELAY failed %s(%d)\n",
		    strerror(errno), errno);
	}
#endif
}

void
tcp_reuse(int sock)
{
#ifdef	SO_REUSEADDR
	int	one = 1;

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, SOCK_OPT_CAST &one, sizeof(one))) {
		fprintf(stderr, "SO_REUSEADDR failed %s(%d)\n",
		    strerror(errno), errno);
	}
#endif
}

void
tcp_keepalive(int sock)
{
#ifdef	SO_KEEPALIVE
	int	one = 1;

	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, SOCK_OPT_CAST &one, sizeof(one))) {
		perror("SO_KEEPALIVE");
	}
#endif
}
