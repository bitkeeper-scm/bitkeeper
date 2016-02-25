/*
 * Copyright 2008-2009,0 BitMover, Inc
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

#ifndef	_SYSTEM_H
#define	_SYSTEM_H

/*  XXX: THIS MUST GO */
#ifdef	__cplusplus
typedef	basic_string<TCHAR> tstring;
extern "C" {
#endif

typedef	unsigned short mode_t;
typedef	unsigned uint32;
typedef	unsigned char u8;

#define	WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <ShellAPI.h>
#include <shlobj.h>

/* this is missing in VS headers */
#define	S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define	S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)
#define	S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define	S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define	S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)

#include "lines.h"
#include "hash.h"

#define	BK_EXTERNAL	0x00000001	// file is outside a bk repository
#define	BK_PROJROOT	0x00000002	// folder is a BK project root
#define	BK_EXTRA	0x00000004	// file is an extra inside a bk repo
#define	BK_READONLY	0x00000008	// file is in readonly state
#define	BK_EDITED	0x00000010	// file is in edited state
#define	BK_MODIFIED	0x00000020	// file has been modified
#define	BK_DIR		0x00000040	// a directory in a repo
#define	BK_FILE		0x00000080	// a file in a repo, regardless of state
#define	BK_IGNORED	0x00000100	// file is ignored
#define	BK_BACKGROUND	0x00000200	// user clicked on background
#define	BK_MULTIFILE	0x00000400	// multiple files selected
#define	BK_MULTIDIR	0x00000800	// multiple directories selected
#define	BK_INVALID	0x00001000	// invalid selection (e.g. Start Menu)

#define	SEL_BACKGROUND(m)	(((m)->flags & BK_BACKGROUND) == BK_BACKGROUND)
#define	SEL_HASDIRS(m)		(((m)->flags & BK_DIR) == BK_DIR)
#define	SEL_HASPROJROOT(m)	(((m)->flags & BK_PROJROOT) == BK_PROJROOT)
#define	SEL_HASFILES(m)		(((m)->flags & BK_FILE) == BK_FILE)
#define	SEL_HASEXTRAS(m)	(((m)->flags & BK_EXTRA) == BK_EXTRA)
#define	SEL_HASIGNORED(m)	(((m)->flags & BK_IGNORED) == BK_IGNORED)
#define	SEL_HASRO(m)		(((m)->flags & BK_READONLY) == BK_READONLY)
#define	SEL_HASEDITED(m)	(((m)->flags & BK_EDITED) == BK_EDITED)
#define	SEL_HASMODIFIED(m)	(((m)->flags & BK_MODIFIED) == BK_MODIFIED)
#define	SEL_MULTIFILE(m)	(((m)->flags & BK_MULTIFILE) == BK_MULTIFILE)
#define	SEL_MULTIDIR(m)		(((m)->flags & BK_MULTIDIR) == BK_MULTIDIR)
/* some utility ones */
#define	SEL_MIXED(m)		(SEL_HASDIRS(m) && SEL_HASFILES(m))
#define	SEL_SINGLEFILE(m)	(SEL_HASFILES(m) && \
				!SEL_HASDIRS(m) && !SEL_MULTIFILE(m))
#define	SEL_SINGLEDIR(m)	(SEL_HASDIRS(m) && \
				!SEL_HASFILES(m) && !SEL_MULTIDIR(m))
#define	SEL_INVALID(m)		(((m)->flags & BK_INVALID) == BK_INVALID)
#define	SEL_PROJROOT(m)		((SEL_SINGLEDIR(m) || SEL_BACKGROUND(m)) && \
				SEL_HASPROJROOT(m))

// LMXXX - remove before final release
#define	NETWORK_ENABLED	0x00000001	/* shellx enabled for remote drives */
#define	LOCAL_ENABLED	0x00000002	/* shellx enabled for local drives */

#define	MAXPATH	1024
#define	BKROOT	"BitKeeper/etc"
#define	PARENT	"BitKeeper/log/parent"
#define	CONFIG	"BitKeeper/etc/config"
#define	REGKEY	"HKEY_LOCAL_MACHINE\\Software\\bitmover\\bitkeeper"
#define MAILSLOTKEY "HKEY_CURRENT_USER\\Software\\bitmover\\bitkeeper\\shellx\\mailslots"

#define	unless(e) if (!(e))

#define	streq(a, b)		!strcmp(a, b)
#define	strieq(a, b)		!stricmp(a, b)
#define	strneq(a, b, n)		!strncmp(a, b, n)
#define	strnieq(a, b, n)	!strnicmp(a, b, n)
#define	index(s, c)		strchr(s, c)
#define	rindex(s, c)		strrchr(s, c)

#define	fnext(buf, in)		fgets(buf, sizeof(buf), in)

#define	bzero(buf, n)		ZeroMemory(buf, n)

#define	nt2bmfname(ofn, nfn)    switch_char(ofn, nfn, '\\', '/')
#define	isDriveColonPath(p)	(isalpha((p)[0]) && ((p)[1] == ':'))

#define	stat(f, sb)		nt_stat(f, sb)

/* cache.c */
#define	MAXCACHE 5		/* how many dirs we cache */

char*	cmd2buf(char *cmd, char *args);

/* util.c */
extern char	*bkdir;
extern char	*bkexe;
extern hash	*icons;
extern int	shellx;

void	BkExec(char *cmd, char *dir, int flags);
HICON	GetIcon(char *icon);
int	isdir(char *s);
int	exists(char *s);
char*	aprintf(char *fmt, ...);
char*	switch_char(const char *ofn, char *nfn, char ochar, char nchar);
void	chomp(char *s);
char*	rootDirectory(char *dir);
char*	getRepoParent(char *pathName);
int	isBkRoot(const char *pathName);
HANDLE	MakeReadPipe(char *cmd, const char *args,  HANDLE *hProc);
HANDLE	CreateChildProcess(char *cmd, const char *args, HANDLE hPipe);
unsigned long	ClosePipe(HANDLE hPipe, HANDLE hProc);
void	concat_path(char *buf, char *first, char *second);
int	patheq(char *a, char *b);
int	pathneq(char *a, char *b, size_t len);
char*	dirname(char *path);
char*	basename(char *path);
int	isDrive(char *path);
int	validDrive(char *path);
char*	dosify(char *buf);

#define	TRACE(format, ...)			\
	trace_msg(__FILE__, __LINE__, __FUNCTION__, format, __VA_ARGS__);

void	trace_msg(char *file, int line,
    const char *function, char *format, ...);

/* nt_stat.c */
int	nt_stat(const char *file, struct stat *sb);
int	linkcount(char *file, struct stat *sb);
int	nt_utime(const char *file, const struct utimbuf *ut);

/* cache.c */
#define	MAILSLOT	"\\\\.\\mailslot\\bitmover\\shellx"
#define	MAILSLOT_DELAY	850	/* 850 ms seems to be what
				 * explorer.exe waits for the FS to
				 * settle down before doing the
				 * callbacks. We mimick that.a  */
extern	HANDLE	cache_mutex;
extern	int	cache_shutdown;
int	cache_fileStatus(char *file, struct stat *sb);
DWORD WINAPI cache_mailslot(LPVOID data);

/* reg.c */
void *	reg_get(char *key, char *value, long *len);
int	reg_set(char *key, char *value, DWORD type, void *data, long len);
int	reg_delete(char *key, char *value);

#ifdef	__cplusplus
}
#endif
#endif	/* _SYSTEM_H */
