#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif
#define	streq(a,b)	!strcmp(a,b)

/*
 * Calculate the same checksum as is used in BitKeeper.
 *
 * %W% %@%
 */
int
chksum_main(int ac, char **av)
{
	int	sum, fd, i;
	int	do_chksum(int, int);
	int	off = 0;

#ifdef WIN32
	setmode(1, _O_BINARY);
#endif
	if (av[1] && streq(av[1], "--help")) {
		fprintf(stderr, "usage: chksum [-o offset] [file]\n");
		exit(1);
	}

	if (av[1] && (strcmp(av[1], "-o") == 0)) {
		off = atoi(av[2]);
		av += 2;
		ac -= 2;
	}
	if (ac == 1) {
		sum = do_chksum(0, off);
		printf("%d\n", sum);
	} else for (i = 1; i < ac; ++i) {
		fd = open(av[i], 0);
		if (fd == -1) {
			perror(av[i]);
		} else {
			sum = do_chksum(fd, off);
			close(fd);
			printf("%-20s %d\n", av[i], sum);
		}
	}
	exit(0);
}

int
do_chksum(int fd, int off)
{
	unsigned char buf[16<<10];
	register unsigned char *p;
	register int i;
	unsigned short sum = 0;

	while (off--) {
		if (read(fd, buf, 1) != 1) exit(1);
	}
	while ((i = read(fd, buf, sizeof(buf))) > 0) {
		for (p = buf; i--; sum += *p++);
	}
	return ((int)sum);
}
