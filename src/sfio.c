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
 *	char	mode[3];
 * repeats, except for the version number.
 */
#include "system.h"
WHATSTR("@(#)%K%");
#define	SFIO_VERS	"SFIO v 1.0"

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

int
main(int ac, char **av)
{
	if (ac != 2) {
usage:		fprintf(stderr, "sfiles | sfio -o\nor\nsfio -i < archive\n");
		fprintf(stderr, "or\nsfio -p < archive\n");
		return (1);
	}
	if (streq("-o", av[1])) return (sfio_out());
	if (streq("-i", av[1])) return (sfio_in(1));
	if (streq("-p", av[1])) return (sfio_in(0));
	goto usage;
}

int
sfio_out()
{
	char	buf[1024];
	char	len[5];

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

	if ((fd == -1) || fstat(fd, &sb)) {
		perror(file);
		return (1);
	}
	sprintf(len, "%010u", (unsigned int)sb.st_size);
	writen(1, len, 10);
	while ((n = readn(fd, buf, sizeof(buf))) > 0) {
		nread += n;
		if (writen(1, buf, n) != n) return (1);
	}
	if (nread != sb.st_size) {
		fprintf(stderr, "Size mismatch on %s %u:%u\n",
		    file, nread, (unsigned int)sb.st_size);
		return (1);
	}
	sprintf(buf, "%03o", sb.st_mode & 0777);
	writen(1, buf, 3);
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
	int	mode;

	unless (todo) {
		fprintf(stderr, "Empty file: %s\n", file);
		return (1);
	}
	if (extract) {
		fd = mkfile(file);
		if (fd == -1) return (1);
	}
	while ((n = readn(0, buf, min(todo, sizeof(buf)))) > 0) {
		todo -= n;
		unless (extract) continue;
		if (writen(fd, buf, n) != n) return (1);
	}
	if (todo) {
		fprintf(stderr, "Premature EOF on %s\n", file);
		return (1);
	}
	if (readn(0, buf, 3) != 3) {
		perror("mode read");
		return (1);
	}
	sscanf(buf, "%03o", &mode);
	if (extract) {
		close(fd);
		chmod(file, mode & 0777);
	}
	fprintf(stderr, "%s\n", file);
	return (0);
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
again:	fd = open(file, O_CREAT|O_EXCL|O_WRONLY, 0600);
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

	if ((mkdir(dir, 0775) == 0) || (errno == EEXIST)) return (0);
	for (t = dir; *t; t++) {
		unless (*t == '/') continue;
		*t = 0;
		mkdir(dir, 0775);
		*t = '/';
	}
	return (mkdir(dir, 0775));
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
