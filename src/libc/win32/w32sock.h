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

#ifndef	NO_SOCKET_REMAPS

#define	socket			nt_socket
#define	accept			nt_accept
#define	bind			nt_bind
#define	send			nt_send
#define	sendto			nt_sendto
#define	recv			nt_recv
#define	recvfrom		nt_recvfrom
#define	listen			nt_listen
#define	connect			nt_connect
#define	getpeername		nt_getpeername
#define	getsockname		nt_getsockname
#define	setsockopt		nt_setsockopt
#define	closesocket		nt_closesocket
#define	select			nt_select
#define	shutdown		nt_shutdown

#endif

int	nt_socket(int af, int type, int protocol);
int	nt_accept(int s, struct sockaddr *addr, int *addrlen);
int	nt_bind(int s, const struct sockaddr *addr, int addrlen);
int	nt_connect(int s, const struct sockaddr *addr, int addrlen);
int	nt_send(int s, const char *buf, int len, int flags);
int	nt_sendto(int s, const char *buf, int len, int flags,
		const struct sockaddr *dst, int dstlen);
int	nt_recv(int s, char *buf, int len, int flags);
int	nt_recvfrom(int s, char *buf, int len, int flags,
		struct sockaddr *src, int *srclen);
int	nt_listen(int s, int backlog);
int	nt_getpeername(int s, struct sockaddr *addr, int *addlen);
int	nt_getsockname(int s, struct sockaddr *addr, int *addrlen);
int	nt_setsockopt(int s, int l, int oname, const char *oval, int olen);
int	nt_closesocket(int s);
int	nt_select(int n, fd_set *rfds, fd_set *wfds, fd_set *efds,
		struct timeval *t);
int	nt_shutdown(int s, int how);

