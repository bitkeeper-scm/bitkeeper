#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
/*
 * Calculate the same as is used in BitKeeper.
 *
 * %W% %@%
 */
int
main(int ac, char **av)
{
	int	sum, fd, i;
	int	doit(int);

	if (ac == 1) {
		sum = doit(0);
		printf("%d\n", sum);
	} else for (i = 1; i < ac; ++i) {
		fd = open(av[i], 0);
		if (fd == -1) {
			perror(av[i]);
		} else {
			sum = doit(fd);
			close(fd);
			printf("%-20s %d\n", av[i], sum);
		}
	}
	exit(0);
}

int
doit(int fd)
{
	unsigned char buf[16<<10];
	register unsigned char *p;
	register int i;
	unsigned short sum = 0;

	while ((i = read(fd, buf, sizeof(buf))) > 0) {
		for (p = buf; i--; sum += *p++);
	}
	return ((int)sum);
}
