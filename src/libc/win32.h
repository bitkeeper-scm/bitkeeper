/*
 * Copyright 1999-2002,2004-2010,2016 BitMover, Inc
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


#ifndef _BK_WIN32_H
#define _BK_WIN32_H

#define ANSIC


#include <winsock2.h>
#include <objbase.h>
#include <windows.h>
#include <io.h>
#include <stdarg.h>	/* for _vsnprintf */
#include <time.h>
#include <share.h>	/* for locks */
#include <errno.h>
#include <string.h>
#include <direct.h>
#include <malloc.h>
#include <process.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include "win32/sys/wait.h"
#include "win32/misc.h"
#include "win32/stat.h"
#include "win32/mman.h"
#include "win32/times.h"
#include "win32/dirent.h"
#include "win32/utsname.h"
#include "win32/re_map_decl.h"
#include "win32/re_map.h"
#include "win32/w32sock.h"

/* undefine the symbol that is in conflict */
/* with bitkeeper main source              */
#undef isascii
#undef min
#undef max
#undef OUT


#define	PATH_DELIM	';'
#define	EDITOR		"vim"
#define	DEVNULL_WR	"nul"		/* do not use this for read operation */
#define DEVNULL_RD	getNull()
#define	DEV_TTY		"con"
#define ROOT_USER	"Administrator"
#define	TMP_PATH	nt_tmpdir()
#define	IsFullPath(f)	nt_is_full_path_name(f)
#define	strieq(a, b)	!strcasecmp(a, b)
#define	pathneq(a, b, n) !strncasecmp(a, b, n)
#define	mixedCasePath()	0

#define	gethostbyname(h)	(nt_loadWinSock(), gethostbyname(h))
#define localName2bkName(x, y) 	nt2bmfname(x, y)
#define	sameuser(a, b)	(!strcasecmp(a, b))
#define	fast_getcwd(b, l)	nt_getcwd(b, l)
#define	reserved(f)	Reserved(f)
#define	mkpipe(p, size)	pipe(p, size)

#define	win32()		1
#define	macosx()	0
#define	NOPROC
#define ELOOP		40

/* this sets errno as a side-effect */
#define	GetLastError()	bk_GetLastError()

DWORD	bk_GetLastError(void);

/* tcp/tcp.c */
#define SOCK_ADDR_CAST
#define SOCK_OPT_CAST (char *)
#define SOCK_PORT_CAST (unsigned short)

typedef int uid_t;

#define safeCloseHandle(h) \
	unless(CloseHandle(h)) \
		fprintf(stderr, "CloseHandle(%x) failed, error = %ld\n", \
		    (int)h, GetLastError())


/* utils/fileinfo.c */
void	closeBadFds(void);
int	Reserved(char *basename);

/* win32/wapi_intf.c */

#define	WIN32_NOISY	0x00000001	/* print messages while retrying */
#define	WIN32_RETRY	0x00000002	/* when an operation fails, retry */

int	win_supported(void);
char	*win_verstr(void);
int	win32flags_get(void);
void	win32flags_set(int flags);
void	win32flags_clear(int flags);
void	win32_retry(int times);

/* win32/reg.c */
int	reg_broadcast(char *key, int timeout);
void	*reg_get(char *key, char *value, long *len);
int	reg_set(char *key, char *value, DWORD type, void *data, long len);
DWORD	reg_type(char *key, char *value);
char	*reg_typestr(DWORD type);
int	reg_delete(char *key, char *value);
char	**reg_keys(char *key);
char	**reg_values(char *key);

/* win32/runas.c */
int	runas(char *cmd, char *param, int async);
#endif
