/*
 * what - look for SCCS what strings.
 *
 * Copyright (c) 1997 by Larry McVoy; All rights reserved.
 *
 */
#include "sccs.h"

WHATSTR("%W%");

int
main(int ac, char **av)
{
	int	i;
	int	doit(char *file);

	debug_main(av);
	for (i = 1; i < ac; ++i) {
		doit(av[i]);
	}
	return (0);
}

int
doit(char *file)
{
	int	fd;
	struct	stat sbuf;
	char	*save, *p, *end;

	if ((fd = open(file, 0, 0)) == -1) {
		perror(file);
		return (-1);
	}
	if (fstat(fd, &sbuf) == -1) {
		perror("fstat");
		close(fd);
		return (-1);
	}
	if (!S_ISREG(sbuf.st_mode) || (sbuf.st_size == 0)) {
		close(fd);
		return (-1);
	}
	save = p = mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	end = p + sbuf.st_size;
	close(fd);
	if ((int)p == -1) {
		perror("mmap");
		return (-1);
	}
	printf("%s:\n", file);
	while (p < end - 4) {
		if (p[0] == '@' && p[1] == '(' && p[2] == '#' && p[3] == ')') {
			putchar('\t');
			p += 4;
			while (p < end) {
				if (*p == '\n' || *p == '"' || *p == 0) break;
				putchar(*p);
				p++;
			}
			putchar('\n');
		} else {
			p++;
		}
	}
	munmap(save, sbuf.st_size);
	return (0);
}
