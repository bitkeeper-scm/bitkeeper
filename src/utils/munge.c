/*
 * This program wants two arguments, the installer executable and the
 * tarball (or whatever) that the installer wants to manage.  It
 * stuffs them into a binary which, when run, runs the first giving it
 * the second as an argument.
 *
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../unix.h"

#define	uchar	unsigned char
#define	OUTPUT	"_data.c"
#define	OBJ	"_data.o"

/*
 * Layout in _data.c
 *	unsigned int	installer_size;
 *	unsigned char	installer_data[installer_size];
 *	unsigned int	data_size;
 *	unsigned char	data_data[data_size];
 */
uchar	init[] = { 255, 6, 1, 2, 3, 4, 255, 3, 9, 62, 255, 10, 4, 61, 255, 0 };

int
writen(int fd, char *buf)
{
	return (write(fd, buf, strlen(buf)));
}

void
setup(int out, char *prog, off_t size)
{
	uchar	buf[1024];
	int	i;

	sprintf(buf,
	    "unsigned int %s_size = %lu;\n", prog, (unsigned long)size);
	writen(out, buf);
	sprintf(buf,
	    "unsigned char %s_data[%lu] = {\n", prog, (unsigned long)size);
	writen(out, buf);
	for (i = 0; init[i]; i++) {
		sprintf(buf, "\t%u,\n", (int)init[i]);
		writen(out, buf);
	}
	writen(out, "};\n");
}

uchar	*
install(uchar *map, off_t size, int fd)
{
	uchar	*p;
	uchar	buf[81920];
	int	i, n;

	for (p = map; p < (map + size); p++) {
		if (*p != init[0]) continue;
		for (i = 1; init[i]; i++) {
			if (p[i] != init[i]) break;
		}
		if (!init[i]) {
		    	printf("Found array offset %u bytes.\n", p - map);
			goto fill;
		}
	}
	fprintf(stderr, "Did not find array\n");
	exit(1);

fill:	writen(1, "copying data ");
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		bcopy(buf, p, n);
		write(1, ".", 1);
		p += n;
	}
	write(1, "\n", 1);
	return (p);
}

int
main(int ac, char **av)
{
	int	installer, data, out;
	uchar	buf[1024];
	char	*cc;
	char	*map;
	uchar	*p;
	struct	stat isb, dsb, asb;

	if (ac != 3) {
		fprintf(stderr, "usage: %s installer data\n", av[0]);
		return (1);
	}
	if (!(installer = open(av[1], O_RDONLY))) {
		perror(av[1]);
		return (1);
	}
	if (!(data = open(av[2], O_RDONLY))) {
		perror(av[1]);
		return (1);
	}
	if (!(out = open(OUTPUT, O_CREAT|O_WRONLY|O_TRUNC, 0666))) {
		perror(OUTPUT);
		return (1);
	}
	if (fstat(installer, &isb) == -1) {
		perror("fstat");
		return (1);
	}
	setup(out, "installer", isb.st_size);
	if (fstat(data, &dsb) == -1) {
		perror("fstat");
		return (1);
	}
	setup(out, "data", dsb.st_size);
	close(out);
	if (!(cc = getenv("CC"))) cc = "cc";
	sprintf(buf, "%s -c %s", cc, OUTPUT);
	system(buf);
	out = open(OBJ, 2);
	if (fstat(out, &asb) == -1) {
		perror("fstat of object file");
		return (1);
	}
	if (asb.st_size <= (isb.st_size + dsb.st_size)) {
		fprintf(stderr, "%s is not big enough\n", OBJ);
		return (1);
	}
	map = mmap(0, asb.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, out, 0);
	if (!map) {
		perror("mmap");
		return (1);
	}
	p = install((uchar *)map, asb.st_size, installer);
	install(p, asb.st_size, data);
	munmap(map, asb.st_size);
	write(1, "done.\n", 6);
	return (0);
}
