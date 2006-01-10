/* %W% Copyright (c) 1999 Andrew Chang */
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

/*
 * We need the GetLongPathName function.  We use the NewAPIs.h header
 * from the Windows SDK to include a emulated version on old win98. 
 */

#define	WANT_GETLONGPATHNAME_WRAPPER
#define	WANT_GETFILEATTRIBUTESEX_WRAPPER

#include "win32/NewAPIs.h"

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
#define	patheq(a, b)	!strcasecmp(a, b)/* WIN98 path is case insensitive */
#define	pathneq(a, b, n) !strncasecmp(a, b, n)
#define wishConsoleVisible() (0)
#define	mixedCasePath()	0

#define	gethostbyname(h)	(nt_loadWinSock(), gethostbyname(h))
#define localName2bkName(x, y) 	nt2bmfname(x, y)
#define	sameuser(a, b)	(!strcasecmp(a, b))
#define	fast_getcwd(b, l)	nt_getcwd(b, l)
#define	reserved(f)	Reserved(f)
#define	mkpipe(p, size)	pipe(p, size)
#define	realmkdir(d, m)	nt_mkdir(d)
#define	mkdir(d, m)	smartMkdir(d, m)

#define	win32()		1
#define	macosx()	0
#define	NOPROC
#define ELOOP		40

typedef int uid_t;

int	Reserved(char *basename);
void	closeBadFds(void);
int	win_supported(void);

#endif
