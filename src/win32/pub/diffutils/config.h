#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <../../../win32.h>

#undef	SHORT_FILE_NAMES 
#undef	INHIBIT_STRING_HEADER
#define	HAVE_SYS_TYPES_H 1
#define	STDC_HEADERS	1
#define	HAVE_MALLOC_H	1
#define	HAVE_ALLOCA_H	0
#define	HAVE_STRING_H	1
#define	HAVE_FCNTL_H	1
#define	HAVE_TIME_H	1
#define	HAVE_SETMODE	1
#define	HAVE_STDLIB_H	1
#define	HAVE_FORK	0
#define	HAVE_DIRENT_H	1
#define	HAVE_SYS_WAIT_H	1
#define	CLOSEDIR_VOID	1
#define	HAVE_TMPNAM	1
#define	RETSIGTYPE	void
#define	same_file(a, b)	(-1)

