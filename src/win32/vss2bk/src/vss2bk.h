/*
 * Copyright 2002-2004,0 BitMover, Inc
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

/* Copyright (c) 2002 BitMover Inc */
#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <io.h>
#include <fcntl.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

#define	MAXPATH	1024
#define	MAXLINE 4096
#define	BKROOT	"BitKeeper/etc"
#define	SYS	(char*)0, 0xdeadbeef
#define	PATH_DELIM      ';'
#define	private static
#define	unless(e)       if (!(e))
#define popen(x, y)	_popen(x, y)
#define pclose(x)	_pclose(x)
#define	vsnprintf(a, b, c, d)  _vsnprintf(a, b, c, d)
#define	rindex(s, c)    strrchr(s, c)
#define fnext(buf, in)  fgets(buf, sizeof(buf), in)
#define	streq(a,b)      !strcmp(a,b)
#define	strneq(a,b,n)   !strncmp(a,b,n)
#define pathneq(a, b, n) !strnicmp(a, b, n)
#define localName2bkName(x, y)   _switch_char(x, y, '\\', '/')
#define bm2ntfname(ofn, nfn) _switch_char(ofn, nfn, '/', '\\')
#define nt2bmfname(ofn, nfn) _switch_char(ofn, nfn, '\\', '/')
#define	bcopy(s,d,l)	memmove(d,s,l)
#define	bzero(s,l)	memset(s,0,l)

#define	VSS_EOF		"**EOF**"
#define	REC_EOF		0
#define	REC_VERSION	1
#define	REC_MISC	2

#ifdef  DEBUG
#       define  debug(x)        fprintf x
#else
#       define  debug(x)
#endif

typedef struct {
	char	debug:1;
	char	skipBadVersion:1;
	int	skipSubPath;
	char    *login;
} opts;

void chomp(char *s);
char *getTimeZone(void);
char *aprintf(char *fmt, ...);
char *prog2path(char *);
char *name2sccs(char *);
char *dirname(char *);
char *_switch_char(const char *, char *, char, char);
char **convertTree(opts *, char *, int *);
int getopt(int, char **, char *);
int sys(char *p, ...);
int bktemp(char *);
int exists(char *s);
int getReg(HKEY , char *, char *, char *, long *);
int mkstemp(char *);
int mkdirf(char *);
int executable(char *);
int sccs_filetype(char *);
off_t	size(char *);


extern int	optind;		/* next arg in argv we process */
extern char	*optarg;	/* argument to an option */
extern char	*av[200];

int	verbose;
char	*ss_prog;

typedef unsigned int u32;

typedef struct meta {
	char	*file;
	int	ver;
	char	*user;
	char	*date;
	char	*time;
	char	**comment;
	char	*label;
	struct	meta *next;
} meta;
