#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "mmap.h"

#define	u8	unsigned char

static	int	ascii(char *file);
extern	MMAP	*mopen(char *, char *);

/* Look for files containing binary data that BitKeeper cannot handle.
 * This consists of (a) NULs, (b) \n followed by \001, or (c) a line
 * longer than 4000 characters (internal buffers are 4096 bytes, and
 * we want to leave a bit of space for safety).
 */

int
isascii_main(int ac, char **av)
{
	if (ac != 2) {
		fprintf(stderr, "usage: %s filename\n", av[0]);
	}
	return (ascii(av[1]));
}

static int
ascii(char *file)
{
	MMAP	*m = mopen(file, "b");
	u8	*p, *end;
	int	beginning = 1;

	if (!m) return (2);
	for (p = (u8*)m->where, end = (u8*)m->end; p < end; p++) {
		switch (*p) {
		    case '\0':	
			mclose(m);
			return (1);
		    case '\n':	
			beginning = 1;
			break;
		    case '\001':
			if (beginning) {
				mclose(m);
				return (1);
			}
			/* FALLTHRU */
		    default:
			beginning = 0;
		}
	}
	mclose(m);
	return (0);
}
