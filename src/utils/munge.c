/*
 * This program wants two arguments, the installer executable and the
 * tarball (or whatever) that the installer wants to manage.  It
 * stuffs them into a binary which, when run, runs the first giving it
 * the second as an argument.
 *
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
#include "../system.h"

#define	uchar	unsigned char
#define	OUTPUT	"_data.c"
#define	OBJ	"_data.o"

/*
 * Layout in _data.c
 *	unsigned int	sfio_size;
 *	unsigned char	sfio_data[installer_size];
 *	unsigned int	data_size;
 *	unsigned char	data_data[data_size];
 */
uchar	init[] = { 255, 6, 1, 2, 3, 4, 255, 3, 9, 62, 255, 10, 4, 61, 255, 0 };

int
writen(int fd, char *buf)
{
	return (write(fd, buf, strlen(buf)));
}

off_t
setup(int out, char *prog, int fd)
{
	
	struct	stat sb;
	uchar	buf[1024];
	int	i;
	off_t	size;

	if (fstat(fd, &sb) == -1) {
		perror("fstat");
		return (1);
	}
	size = sb.st_size;

	sprintf(buf,
	    "unsigned int %s_size = %lu;\n", prog, (unsigned long)size);
	writen(out, buf);
	sprintf(buf,
	    "unsigned char %s_data[%lu] = {\n", prog, (unsigned long)size);
	writen(out, buf);
	/*
	 * Accoording lm, to keep hpux happy, we need stuff some non-zero
	 * (random) value here.
	 */
	for (i = 0; init[i]; i++) {
		sprintf(buf, "\t%u,\n", (int)init[i]);
		writen(out, buf);
	}
	writen(out, "};\n");
	return (size);
}

uchar	*
install(uchar *map, uchar *start, off_t size, int fd)
{
	uchar	*p, *end;
	uchar	buf[81920];
	int	i, n;

	end = map + size;
	for (p = start; p < end; p++) {
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
		if ((p + n) > end) {
			fprintf(stderr,
			    "Error, writting outside mmap region\n");
			exit(1);
		}
		bcopy(buf, p, n);
		write(1, ".", 1);
		p += n;
	}
	write(1, "\n", 1);
	writen(1, "done copying data\n");
	return (p);
}

int
main(int ac, char **av)
{
	int	sfio, data, out;
	uchar	buf[1024];
	char	*cc;
	char	*map;
	uchar	*p;
	struct	stat asb;
	off_t	sf_size, d_size, a_size;

	if (ac != 3) {
		fprintf(stderr,
		    "usage: %s sfio data\n", av[0]);
		return (1);
	}

	if ((sfio = open(av[1], O_RDONLY, 0)) < 0) {
		perror(av[1]);
		return (1);
	}
	setmode(sfio, _O_BINARY);

	if ((data = open(av[2], O_RDONLY, 0)) < 0) {
		perror(av[2]);
		return (1);
	}
	setmode(data, _O_BINARY);

	if ((out = open(OUTPUT, O_CREAT|O_WRONLY|O_TRUNC, 0666)) < 0) {
		perror(OUTPUT);
		return (1);
	}
	setmode(out, _O_BINARY);
	sf_size = setup(out, "sfio", sfio);
	d_size = setup(out, "data", data);
	close(out);

#ifdef WIN32
	sprintf(buf,
	    "cl -c -W0 -O2 -G3 -Og -Oi -Oy -DWIN32 -D_WIN32 "
	    "-D_MT -D_DLL -MD %s -Fo%s", OUTPUT, OBJ); 
#else
	if (!(cc = getenv("CC"))) cc = "cc";
	sprintf(buf, "%s -c %s", cc, OUTPUT);
#endif
	system(buf);
	out = open(OBJ, 2, 0);
	setmode(out, _O_BINARY);
	if (fstat(out, &asb) == -1) {
		perror("fstat of object file");
		return (1);
	}
	if (asb.st_size <= (sf_size + d_size)) {
		fprintf(stderr, "%s is not big enough\n", OBJ);
		return (1);
	}
	map = mmap(0, asb.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, out, 0);
	if (!map) {
		perror("mmap");
		return (1);
	}
	p = install((uchar *)map, (uchar *)map, asb.st_size, sfio);
	install((uchar *)map, p, asb.st_size, data);
	munmap(map, asb.st_size);
	write(1, "done.\n", 6);
	return (0);
}
