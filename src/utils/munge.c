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

main(int ac, char **av)
{
	int	in, out;
	uchar	buf[81920];
	char	num[10];
	int	size = 0, n, i;
	char	*t, *cc;
	char	*map;
	char	*p;
	struct	stat sb, sb2;

	if (ac != 2) {
		fprintf(stderr, "usage: %s input\n");
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
	sprintf(buf, "unsigned int program_size = %u;\n", sb.st_size);
	writen(out, buf);
	sprintf(buf, "unsigned int program_before = 0x%x;\n",
	    (unsigned int)0xdeadbeef);
	writen(out, buf);
	sprintf(buf,
	    "unsigned char program_data[%u] = { 0, 1, 2, 3, 4, 3, 9, 62 };\n",
	    sb.st_size);
	writen(out, buf);
	sprintf(buf, "unsigned int program_after = 0x%x;\n", 
	    (unsigned int)0xdeadbeef);
	writen(out, buf);
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
	for (p = map; p < (map + sb2.st_size); p++) {
		int	*i = (int *)p;

		if (*i == 0xdeadbeef) {
			printf("Found marker at offset %u\n", p - map);
			goto array;
		}
	}
	fprintf(stderr, "Did not find marker\n");
	exit(1);

array:	for (p = p + 4; p < (map + sb2.st_size); p++) {
		if (p[0] == 0 &&
		    p[1] == 1 &&
		    p[2] == 2 &&
		    p[3] == 3 &&
		    p[4] == 4 &&
		    p[5] == 3 &&
		    p[6] == 9 &&
		    p[7] == 62) {
		    	printf("Found array at offset %u\n", p - map);
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
	write(1, "Done\n", 5);
	exit(0);
}

writen(int fd, char *buf)
{
	return (write(fd, buf, strlen(buf)));
}
