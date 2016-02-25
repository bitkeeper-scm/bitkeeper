/*
 * Copyright 1999-2011,2015-2016 BitMover, Inc
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

/* %W% Copyright (c) 1999 Zack Weinberg */
#ifndef _BK_UNIX_H
#define _BK_UNIX_H
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pwd.h>
#include <dirent.h>
#include <netdb.h>
#include <unistd.h>
#include <utime.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <resolv.h>
#ifdef _AIX
#include <sys/select.h>
#endif

/*
 * Local config starts here.
 */
#define	PATH_DELIM	':'
#define EDITOR		"vi"
#define	DEVNULL_WR	"/dev/null" /* for write */
#define	DEVNULL_RD	"/dev/null" /* for read */
#define DEV_TTY		"/dev/tty"
#define	ROOT_USER	"root"
#define	TMP_PATH	"/tmp"
#define	IsFullPath(f)	((f)[0] == '/')
#define	strieq(a, b)	!strcasecmp(a, b)
#define pathneq(a, b, n) strneq(a, b, n)
#define sameuser(a,b)	(!strcmp(a, b))
#define	fileBusy(f)	(0) 
#define	mixedCasePath()	1
#define reserved(f)	(0)		/* No reserved names on unix */

#define GetShortPathName(p, q, s) 	/* Unlike win32, 
					 * Unix has no short path name
					 */
/* These functions are a "no-op" on unix */
#define localName2bkName(x, y)		(void)1
#define	make_fd_uninheritable(fd)  fcntl(fd, F_SETFD, 1)
#define	mkpipe(p, size)	pipe(p)
#define	setmode(a, b)

#define	closesocket(i)	close(i)
#define	linkcount(a, b)	(b)->st_nlink

#define	win32()		0
#define	win_supported()	1


#ifdef	__APPLE__
#define	macosx()	1
#else
#define	macosx()	0
#endif

/* tcp/tcp.c */
#define SOCK_ADDR_CAST (struct in_addr)
#define SOCK_OPT_CAST
#define SOCK_PORT_CAST

#endif
