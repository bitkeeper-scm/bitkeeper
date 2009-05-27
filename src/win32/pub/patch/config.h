#include <stddef.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <io.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/utime.h>
#include <winsock2.h>
#include "misc.h"
#include "stat.h"
#include "times.h"
#include "dirent.h"
#include "re_map_decl.h"
#include "re_map.h"
#include "fslayer/win_remap.h"

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
#define HAVE_STRUCT_UTIMBUF	1
#define HAVE_DONE_WORKING_MALLOC_CHECK 1
#define HAVE_DONE_WORKING_REALLOC_CHECK 1
#define	CLOSEDIR_VOID	1
#define	HAVE_TMPNAM	1
#define	RETSIGTYPE	void
#define	NULL_DEVICE	"nul"

#define PARAMS(args)	args
#define popen(c, m)	safe_popen(c, m)
#define pclose(f)	safe_pclose(f)
#define	strncasecmp(a, b, n) strnicmp(a, b, n)

#define malloc(a)	rpl_malloc(a)
#define realloc(p, n)	rpl_realloc(p, n)
#define	mkdir(f, m)	nt_mkdir(f)

#define HAVE_MKTEMP	1
#define HAVE_VPRINTF	1
#define	HAVE_STRERROR	1
