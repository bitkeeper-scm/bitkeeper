1c\
/* config.h for compiling `patch' with DJGPP for MS-DOS and MS-Windows.\
   Please keep this file as similar as possible to ../../config.h\
   to simplify maintenance later.  */\
\
/* This does most of the work; the rest of this file defines only those\
   symbols that <sys/config.h> doesn't define correctly.  */\
#include <sys/config.h>

s/#undef HAVE_DONE_WORKING_MALLOC_CHECK/#define HAVE_DONE_WORKING_MALLOC_CHECK 1/
s/#undef HAVE_DONE_WORKING_REALLOC_CHECK/#define HAVE_DONE_WORKING_REALLOC_CHECK 1/
s/#undef HAVE_LONG_FILE_NAMES/#define HAVE_LONG_FILE_NAMES 1/
s/#undef HAVE_MEMCMP/#define HAVE_MEMCMP 1/
s/#undef HAVE_MKTEMP/#define HAVE_MKTEMP 1/
s/#undef HAVE_PATHCONF/#define HAVE_PATHCONF 1/
s/#undef HAVE_RAISE/#define HAVE_RAISE 1/
s/#undef HAVE_SIGPROCMASK/#define HAVE_SIGPROCMASK 1/
s/#undef HAVE_STRUCT_UTIMBUF/#define HAVE_STRUCT_UTIMBUF 1/
s/#undef HAVE_UTIME_H/#define HAVE_UTIME_H 1/
s/#undef HAVE_VPRINTF/#define HAVE_VPRINTF 1/
s/#undef PROTOTYPES/#define PROTOTYPES 1/

s,#undef.*,/* & */,

$a\
/* DGJPP-specific definitions */\
\
#define chdir chdir_safer\
int chdir_safer (char const *);\
\
#define FILESYSTEM_PREFIX_LEN(f) ((f)[0] && (f)[1] == ':' ? 2 : 0)\
#define ISSLASH(c) ((c) == '/'  ||  (c) == '\\\\')\
\
#define HAVE_DOS_FILE_NAMES 1\
\
#define HAVE_SETMODE 1\
#ifdef WIN32\
# define setmode _setmode\
#endif\
\
#define TMPDIR "c:"
