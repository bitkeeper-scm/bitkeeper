#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int	ascii(int);

/* Look for files containing binary data that BitKeeper cannot handle.
 * This consists of (a) NULs, (b) \n followed by \001, or (c) a line
 * longer than 4000 characters (internal buffers are 4096 bytes, and
 * we want to leave a bit of space for safety).
 */

int
main(int ac, char **av)
{
	if (ac != 2) {
		fprintf(stderr, "usage: %s filename\n", av[0]);
	}
	return (ascii(open(av[1], 0)));
}

int
ascii(int fd)
{
	char	buf[8192];
	int	n, i;
	int	len = 0, beginning = 1;

	if (fd == -1) return (2);
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		for (i = 0; i < n; ++i) {
			switch (buf[i])	{
			    case '\0':	 return (1);
			    case '\n':	 beginning = 1; len = 0; break;
			    case '\001': if (beginning) return (1);
				/* FALLTHRU */
			    default:	 beginning = 0;
			}
			if (++len > 4000) return (1);
		}
	}
	return (0);
}
