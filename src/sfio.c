/*
 * sfio - SCCS file transfer, kind of like cpio
 *
 * This:
 *	sfiles | sfio -o | (cd /some/place/else && sfio -i)
 * will move a bunch of files.  When it is done, all the sfiles are owned
 * by uid/gid of the caller, and are 0444 mode.
 *
 * File format is
 *	char	vers[10];
 *	char	pathlen[4];
 *	char	path[atoi(pathlen)];
 *	char	datalen[10];
 *	char	data[atoi(datalen)];
 *	char	adler32[10];
 *	char	mode[3];	(VERSMODE only)
 * repeats, except for the version number.
 */
#include "system.h"
#include <zlib.h>	/* for adler32 */
#undef	unlink		/* I know the files are writable, I created them */
WHATSTR("@(#)%K%");

#define	SFIO_VERSNOMODE	"SFIO v 1.2"	/* must be 10 bytes exactly */
#define	SFIO_VERSMODE	"SFIO v 1.3"	/* must be 10 bytes exactly */
#define	SFIO_VERS	(doModes ? SFIO_VERSMODE : SFIO_VERSNOMODE)
#define	u32		unsigned int

int	sfio_out(void);
int	out(char *file);
char	chop(register char *s);
int	sfio_in(int extract);
int	in(char *file, int todo, int extract);
int	mkfile(char *file);
int	mkdirp(char *dir);
int	mkdirf(char *file);
int	isdir(char *s);
int	readn(int from, char *buf, int size);
int	writen(int from, char *buf, int size);
extern	void platformSpecificInit(char *name);

static const char help[] = "\
usage:	sfio [-q] [-m] -i | -p < archive\n\
or:	sfio [-q] [-m] -o < filelist\n\
\n\
  -i	extract archive\n\
  -m	transfer file modes (default not to do so)\n\
  -o	create archive\n\
  -p	list contents of archive\n\
  -q	quiet mode\n";

static	int quiet;
static	int doModes;

#define M_IN	1
#define M_OUT	2
#define M_LIST	3

int
main(int ac, char **av)
{
	int	c, mode = 0;

	platformSpecificInit(NULL);
	if (ac == 2 && streq(av[1], "--help")) goto usage;
	while ((c = getopt(ac, av, "imopqs")) != -1) {
		switch (c) {
		    case 'i': if (mode) goto usage; mode = M_IN;   break;
		    case 'o': if (mode) goto usage; mode = M_OUT;  break;
		    case 'p': if (mode) goto usage; mode = M_LIST; break;
		    case 'm': doModes = 1; break;
		    case 'q': quiet = 1; break;
		default:
			goto usage;
		}
	}
	/* ignore "-" it makes bk -r sfio -o work */
	if (av[optind] && streq(av[optind], "-")) optind++;
	if (optind != ac) goto usage;

	if      (mode == M_OUT)  return (sfio_out());
	else if (mode == M_IN)   return (sfio_in(1));
	else if (mode == M_LIST) return (sfio_in(0));

usage:	fputs(help, stderr);
	return (1);
}

int
sfio_out()
{
	char	buf[1024];
	char	len[5];

#ifdef WIN32
	setmode(0, _O_TEXT); /* read file list in text mode */
#endif
	writen(1, SFIO_VERS, 10);
	while (fnext(buf, stdin)) {
		chop(buf);
		sprintf(len, "%04d", strlen(buf));
		writen(1, len, 4);
		writen(1, buf, strlen(buf));
		if (out(buf)) return (1);
	}
	return (0);
}

int
out(char *file)
{
	char	buf[1024];
	int	fd = open(file, 0, 0);
	struct	stat sb;
	char	len[11];
	int	n, nread = 0;
	u32	sum = 0;

	if ((fd == -1) || fstat(fd, &sb)) {
		perror(file);
		return (1);
	}
	sprintf(len, "%010u", (unsigned int)sb.st_size);
	writen(1, len, 10);
	while ((n = readn(fd, buf, sizeof(buf))) > 0) {
		nread += n;
		sum += adler32(sum, buf, n);
		if (writen(1, buf, n) != n) return (1);
	}
	if (nread != sb.st_size) {
		fprintf(stderr, "Size mismatch on %s %u:%u\n",
		    file, nread, (unsigned int)sb.st_size);
		return (1);
	}
	sprintf(buf, "%010u", sum);
	writen(1, buf, 10);
	if (doModes) {
		sprintf(buf, "%03o", sb.st_mode & 0777);
		writen(1, buf, 3);
	}
	close(fd);
	return (0);
}

char
chop(register char *s)
{
	char	c;

	unless (s && *s) return (0);
	while (*s++);
	c = s[-2];
	s[-2] = 0;
	return (c);
}

int
sfio_in(int extract)
{
	char	buf[1024];
	char	datalen[11];
	int	len;
	int	n;

	if (readn(0, buf, 10) != 10) {
		perror("read");
		return (1);
	}
	unless (strneq(buf, SFIO_VERS, 10)) {
		fprintf(stderr, "Version mismatch\n");
		return (1);
	}
	for (;;) {
		n = readn(0, buf, 4);
		if (n == 0) return (0);
		if (n != 4) {
			perror("read");
			return (1);
		}
		buf[5] = 0;
		len = 0;
		sscanf(buf, "%04d", &len);
		if (len <= 0 || len > 1023) {
			fprintf(stderr, "Bad length in sfio\n");
			return (1);
		}
		if (readn(0, buf, len) != len) {
			perror("read");
			return (1);
		}
		buf[len] = 0;
		if (readn(0, datalen, 10) != 10) {
			perror("read");
			return (1);
		}
		datalen[10] = 0;
		len = 0;
		sscanf(datalen, "%010d", &len);
		if (in(buf, len, extract)) return (1);
	}
}

int
in(char *file, int todo, int extract)
{
	char	buf[1024];
	int	n;
	int	fd = -1;
	mode_t	mode = 0;
	u32	sum = 0, sum2 = 0;

	unless (todo) {
		fprintf(stderr, "Empty file: %s\n", file);
		return (1);
	}
	if (extract) {
		fd = mkfile(file);
		/* do NOT jump to err, it unlinks and the file may exist */
		if (fd == -1) return (1);
	}
	while ((n = readn(0, buf, min(todo, sizeof(buf)))) > 0) {
		todo -= n;
		sum += adler32(sum, buf, n);
		unless (extract) continue;
		if (writen(fd, buf, n) != n) goto err;
	}
	if (todo) {
		fprintf(stderr, "Premature EOF on %s\n", file);
		goto err;
	}
	if (readn(0, buf, 10) != 10) {
		perror("chksum read");
		goto err;
	}
	sscanf(buf, "%010u", &sum2);
	if (sum != sum2) {
		fprintf(stderr,
		    "Checksum mismatch %u:%u for %s\n", sum, sum2, file);
		goto err;
	}
	if (doModes) {
		unsigned int imode;  /* mode_t might be a short */
		if (readn(0, buf, 3) != 3) {
			perror("mode read");
			goto err;
		}
		sscanf(buf, "%03o", &imode);
		mode = imode;
	}
	if (extract) {
		close(fd);
		if (doModes) chmod(file, mode & 0777);
	}
	unless (quiet) fprintf(stderr, "%s\n", file);
	return (0);

err:	
	if (extract) {
		close(fd);
		unlink(file);
	}
	return (1);
}

int
mkfile(char *file)
{
	int	fd;
	int	first = 1;

	if (access(file, F_OK) == 0) {
		fprintf(stderr, "%s exists\n", file);
		return (-1);
	}
again:	fd = open(file, O_CREAT|O_EXCL|O_WRONLY, 0666);
	if (fd != -1) return (fd);
	if (errno == EEXIST) {
		perror(file);
		return (-1);
	}
	if (first) {
		mkdirf(file);
		first = 0;
		goto again;
	}
	return (-1);
}

/*
 * Given a pathname, make the directory.
 */
int
mkdirp(char *dir)
{
	char	*t;

	if ((mkdir(dir, 0777) == 0) || (errno == EEXIST)) return (0);
	for (t = dir; *t; t++) {
		unless (*t == '/') continue;
		*t = 0;
		mkdir(dir, 0777);
		*t = '/';
	}
	return (mkdir(dir, 0777));
}

/*
 * given a pathname, create the dirname if it doesn't exist.
 */
int
mkdirf(char *file)
{
	char	*s;
	int	ret;

	unless (s = strrchr(file, '/')) return (0);
	*s = 0;
	if (isdir(file)) {
		*s = '/';
		return (0);
	}
	ret = mkdirp(file);
	*s = '/';
	return (ret);
}

int
isdir(char *s)
{
	struct	stat sbuf;

	if (lstat(s, &sbuf) == -1) return 0;
	return (S_ISDIR(sbuf.st_mode));
}

int
readn(int from, char *buf, int size)
{
	int	done;
	int	n;

	for (done = 0; done < size; ) {
		n = read(from, buf + done, size - done);
		if (n <= 0) {
			break;
		}
		done += n;
	}
	return (done);
}

int
writen(int from, char *buf, int size)
{
	int	done;
	int	n;

	for (done = 0; done < size; ) {
		n = write(from, buf + done, size - done);
		if (n <= 0) {
			break;
		}
		done += n;
	}
	return (done);
}
