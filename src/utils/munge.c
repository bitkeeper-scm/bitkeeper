/*
 * Copyright 1999-2000,2004-2007,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This program wants two arguments, the installer executable and the
 * tarball (or whatever) that the installer wants to manage.  It
 * stuffs them into a binary which, when run, runs the first giving it
 * the second as an argument.
 *
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
#include "system.h"

#define	OUTPUT	"_data.c"
#define	OBJ	"_data.o"

/* unique sequence of bytes */
#define	MARKER	"2ccd0fc04e3abb1a1e5af1e3da51d0fa574d2c4f584b39bc954e7b76cefea"

/*
 * Layout in _data.c
 *	unsigned int	sfio_size;
 *	unsigned char	sfio_data[installer_size];
 *	unsigned int	data_size;
 *	unsigned char	data_data[data_size];
 */

private void
setup(FILE *f, char *prog, off_t size)
{
	int	i;

	fprintf(f, "unsigned int %s_size = %lu;\n", prog, (unsigned long)size);
	fprintf(f, "unsigned char %s_data[%lu] = {\n",
	    prog, (unsigned long)size);

	/*
	 * According lm, to keep hpux happy, we need stuff some non-zero
	 * (random) value here.
	 */
	for (i = 0; i < sizeof(MARKER); i++) {
		fprintf(f, "\t%u,\n", MARKER[i]);
	}
	fprintf(f, "};\n");
}

private u8	*
install(u8 *map, u8 *start, off_t size, int fd)
{
	u8	*p, *end;
	u8	buf[64<<10];
	int	n;

	end = map + size;
	if (p = memmem(start, end - start,
		MARKER, sizeof(MARKER))) {
		printf("Found array offset %lu bytes.\n", p - map);
	} else {
		printf("Did not find array\n");
		exit(1);
	}
	printf("Inserting data ");
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		if ((p + n) > end) {
			fprintf(stderr, "Error, writing outside mmap region\n");
			exit(1);
		}
		memcpy(p, buf, n);
		printf(".");
		p += n;
	}
	printf(" done.\n");
	return (p);
}

int
main(int ac, char **av)
{
	int	sfio, data, out;
	u8	buf[1024];
	char	*cc;
	char	*map;
	u8	*p;
	FILE	*f;
	off_t	sf_size, d_size, out_size;

#ifdef	WIN32
	_fmode = _O_BINARY;
#endif

	if (ac != 3) {
		fprintf(stderr,
		    "usage: %s sfio data\n", av[0]);
		return (1);
	}
	setbuf(stdout, 0);

	if ((sfio = open(av[1], O_RDONLY, 0)) < 0) {
		perror(av[1]);
		return (1);
	}
	sf_size = fsize(sfio);
	setmode(sfio, _O_BINARY);

	if ((data = open(av[2], O_RDONLY, 0)) < 0) {
		perror(av[2]);
		return (1);
	}
	d_size = fsize(data);
	setmode(data, _O_BINARY);

	unless (f = fopen(OUTPUT, "wb")) {
		perror(OUTPUT);
		return (1);
	}
	setup(f, "sfio", sf_size);
	setup(f, "data", d_size);
	fclose(f);

	if (!(cc = getenv("CC"))) cc = "cc";
	sprintf(buf, "%s -c %s", cc, OUTPUT);
	system(buf);
	out = open(OBJ, 2, 0);
	setmode(out, _O_BINARY);
	out_size = fsize(out);
	if (out_size <= (sf_size + d_size)) {
		fprintf(stderr, "%s is not big enough\n", OBJ);
		return (1);
	}
	map = mmap(0, out_size, PROT_READ|PROT_WRITE, MAP_SHARED, out, 0);
	if (!map) {
		perror("mmap");
		return (1);
	}
	p = install((u8 *)map, (u8 *)map, out_size, sfio);
	p = install((u8 *)map, p, out_size, data);
	munmap(map, out_size);
	return (0);
}
