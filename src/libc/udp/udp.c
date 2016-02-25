/*
 * Copyright 2015-2016 BitMover, Inc
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
 * udp_lib.c - routines for managing UDP connections.
 */

#include "system.h"

/*
 * Get a UDP socket
 */
int
udp_server(char *addr, int port, int quiet)
{
	int	sock;
	struct	sockaddr_in s;

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		unless (quiet) perror("socket udp server");
		return (-1);
	}
	memset((void*)&s, 0, sizeof(s));
	s.sin_family = AF_INET;
	s.sin_port = htons(port);
	if (addr) s.sin_addr.s_addr = inet_addr(addr);
	if (s.sin_addr.s_addr == -1) {
		unless (quiet) {
			fprintf(stderr, "%s: invalid IP address\n", addr);
		}
		return (-1);
	}
	if (bind(sock, (struct sockaddr*)&s, sizeof(s)) < 0) {
		unless (quiet) perror("bind");
		return (-2);
	}
	return (sock);
}

/*
 * Return a conected UDP socket. What 'connected' means for UDP is
 * that the receiving side has been associated with the client socket
 * so that you cannot use sendto() on it. Only send() or
 * write(). Ditto for recvfrom(), can't use it. Use recv() or read().
 */
int
udp_connect(char *host, int port)
{
	struct	hostent *h;
	struct	sockaddr_in s;
	int	sock;
	char	*freeme = 0;

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket");
		return (-1);
	}
	if (strchr(host, ':')) {
		assert(!port);
		host = strdup(host);
		freeme = strrchr(host, ':');
		*freeme++ = 0;
		port = atoi(freeme);
		freeme = host;
	}
	unless (h = gethostbyname(host)) {
		free(freeme);
		closesocket(sock);
		return (-2);
	}
	memset((void *)&s, 0, sizeof(s));
	s.sin_family = AF_INET;
	memmove((void *)&s.sin_addr, (void *)h->h_addr, h->h_length);
	s.sin_port = SOCK_PORT_CAST htons(SOCK_PORT_CAST port);
	if (connect(sock, (struct sockaddr*)&s, sizeof(s)) < 0) {
		closesocket(sock);
		free(freeme);
		return (-3);
	}
	free(freeme);
	return (sock);
}

/*
 * Return a file descriptor if the udp socket is readable, -1
 * otherwise.  Timeout after 'sec' have elapsed.
 *
 * Not really udp specific so in theory it could go in some other
 * file.
 */
int
readable(int fd, int sec)
{
	fd_set	rset;
	struct	timeval tv = {0};
	int	rc;

	FD_ZERO(&rset);
	FD_SET(fd, &rset);

	tv.tv_sec = sec;
	tv.tv_usec = 0;

	rc = select(fd + 1, &rset, 0, 0, &tv);
	return ((rc > 0) ? FD_ISSET(fd, &rset) : rc);
}
