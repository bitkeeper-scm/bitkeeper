#ifndef	SFIO_BSIZ
#define SFIO_BSIZ       (16<<10)
#define SFIO_NOMODE     "SFIO v 1.4"    /* must be 10 bytes exactly */
#define SFIO_MODE       "SFIO vm1.4"    /* must be 10 bytes exactly */
#define SFIO_VERS(m)    (m ? SFIO_MODE : SFIO_NOMODE)

/* error returns, don't use 1, that's generic */
#define SFIO_LSTAT      2
#define SFIO_READLINK   3
#define SFIO_OPEN       4
#define SFIO_SIZE       5
#define SFIO_LOOKUP     6
#define SFIO_MORE       7       /* another sfio follows */

#define M_IN    1
#define M_OUT   2
#define M_LIST  3
#endif
