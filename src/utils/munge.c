/*
 * This program generates a C file which is the data in an array named
 * progam_data, which is sized program_size.
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

uchar	init[] = { 255, 6, 1, 2, 3, 4, 255, 3, 9, 62, 255, 10, 4, 61, 255, 0 };

int
writen(int fd, char *buf)
{
	return (write(fd, buf, strlen(buf)));
}

main(int ac, char **av)
{
	int	in, out;
	uchar	buf[81920];
	int	n, i;
	char	*cc;
	char	*map;
	uchar	*p;
	struct	stat sb, sb2;

	if (ac != 2) {
		fprintf(stderr, "usage: %s input\n", av[0]);
		exit(1);
	}
	if (!(in = open(av[1], O_RDONLY))) {
		perror(av[1]);
		exit(1);
	}
	if (!(out = open(OUTPUT, O_CREAT|O_WRONLY, 0666))) {
		perror(OUTPUT);
		exit(1);
	}
	if (fstat(in, &sb) == -1) {
		perror("fstat");
		exit(1);
	}
	sprintf(buf, "unsigned int program_size = %lu;\n",
	    (long unsigned)sb.st_size);
	writen(out, buf);
	sprintf(buf, "unsigned char program_data[%lu] = {\n",
	    (long unsigned)sb.st_size);
	writen(out, buf);
	for (i = 0; init[i]; i++) {
		sprintf(buf, "\t%u,\n", (int)init[i]);
		writen(out, buf);
	}
	writen(out, "};\n");
	close(out);
	if (!(cc = getenv("CC"))) cc = "cc";
	sprintf(buf, "%s -c %s", cc, OUTPUT);
	system(buf);
	out = open(OBJ, 2);
	if (fstat(out, &sb2) == -1) {
		perror("fstat of object file");
		exit(1);
	}
	if (sb2.st_size <= sb.st_size) {
		fprintf(stderr, "%s is not big enough\n", OBJ);
		exit(1);
	}
	map = mmap(0, sb2.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, out, 0);
	if (!map) {
		perror("mmap");
		exit(1);
	}
	for (p = (uchar*)map; p < (uchar*)(map + sb2.st_size); p++) {
		if (*p != init[0]) continue;
		for (i = 1; init[i]; i++) {
			if (p[i] != init[i]) break;
		}
		if (!init[i]) {
		    	printf("Found array at %u bytes into the file.\n",
			    p - (uchar *)map);
			goto fill;
		}
	}
	fprintf(stderr, "Did not find array\n");
	exit(1);

fill:	writen(1, "Copying ");
	while ((n = read(in, buf, sizeof(buf))) > 0) {
		bcopy(buf, p, n);
		write(1, ".", 1);
		p += n;
	}
	munmap(map, sb2.st_size);
	write(1, " done.\n", 7);
	exit(0);
}
