#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int	ascii(int);

int
main(int ac, char **av)
{
	if (ac != 2) {
		fprintf(stderr, "usage: %s filename\n", av[0]);
	}
	return (ascii(open(av[1], 0)) == 0);
}

inline int
isAscii(int c)
{
	if (c & 0x60) return (1);
	return (c == '\f') ||
	    (c == '\n') || (c == '\b') || (c == '\r') || (c == '\t');
}

int
ascii(int fd)
{
	char	buf[8192];
	int	n, i;

	if (fd == -1) return (0);
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		for (i = 0; i < n; ++i) {
			if (!isAscii(buf[i])) {
				close(fd);
				return (0);
			}
		}
	}
	return (1);
}

