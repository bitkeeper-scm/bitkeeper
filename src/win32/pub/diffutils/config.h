#undef SHORT_FILE_NAMES 
#undef INHIBIT_STRING_HEADER
#define STDC_HEADERS 1
#define HAVE_MALLOC_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_ALLOCA_H 0
#define HAVE_STRING_H 1
#define HAVE_FCNTL_H 1
#define HAVE_TIME_H 1
#define HAVE_SETMODE 1
#define HAVE_STDLIB_H 1
#define HAVE_FORK 0
#define HAVE_DIRENT_H 1
#define HAVE_WAIT_H 1
#define CLOSEDIR_VOID 1

#define same_file(a, b) (-1)


/* stuff for sdiff */
#include <process.h>
#include <io.h>
#define popen(a,b)	_popen(a, b)
#define pclose(a)	_pclose(a)
#define system(a)	nt_system(a)
#define RETSIGTYPE void
#define HAVE_TMPNAM 1
//#define HAVE_SIGACTION 

#define SYSTEM_QUOTE_ARG(q, a) \
  { \
    *(q)++ = '\"'; \
    for (;  *(a);  *(q)++ = *(a)++) \
      if (*(a) == '\"') \
        { \
          *(q)++ = '\"'; \
          *(q)++ = '\\'; \
          *(q)++ = '\"'; \
        } \
    *(q)++ = '\"'; \
  }
