/*
 * sfio - SCCS file transfer, kind of like cpio
 *
 * This:
 *	bk sfiles | bk sfio -o | (cd /some/place/else && sfio -i)
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
#include "sccs.h"
#include "zlib/zlib.h"
#undef	unlink		/* I know the files are writable, I created them */
WHATSTR("@(#)%K%");

#define	SFIO_BSIZ	4096
#define	COMPAT_NOMODE	"SFIO v 1.2"	/* must be 10 bytes exactly */
#define	COMPAT_MODE	"SFIO v 1.3"	/* must be 10 bytes exactly */
#define	SFIO_NOMODE	"SFIO v 1.4"	/* must be 10 bytes exactly */
#define	SFIO_MODE	"SFIO vm1.4"	/* must be 10 bytes exactly */
#define	SFIO_VERS	(compat ? \
			    (doModes ? COMPAT_MODE : COMPAT_NOMODE) : \
			    (doModes ? SFIO_MODE : SFIO_NOMODE))

private	int	sfio_out();
private int	out_file(char *file, struct stat *, off_t *byte_count);
private int	out_file_compat(char *file, struct stat *, off_t *byte_count);
private int	out_link(char *file, struct stat *, off_t *byte_count);
private	int	sfio_in(int extract);
private int	in_link(char *file, int todo, int extract);
private int	in_file(char *file, int todo, int extract);
private int	in_file_compat(char *file, int todo, int extract);
private	int	mkfile(char *file);
extern	void	platformSpecificInit(char *name);

private	int	quiet;
private	int	doModes;
private	int	compat;		/* internal use only compat option */
private int	(*out_reg)();
private int	(*in_reg)();

#define M_IN	1
#define M_OUT	2
#define M_LIST	3

int
sfio_main(int ac, char **av)
{
	int	c, mode = 0;

	platformSpecificInit(NULL);
	if (ac == 2 && streq(av[1], "--help")) {
		system("bk help sfio");
		return (0);
	}
	in_reg = in_file;
	out_reg = out_file;
	while ((c = getopt(ac, av, "cimopq")) != -1) {
		switch (c) {
		    case 'c':	/* undoc */
			compat = 1;
			in_reg = in_file_compat;
			out_reg = out_file_compat;
			break;
		    case 'i': if (mode) goto usage; mode = M_IN;   break; /* doc 2.0 */
		    case 'o': if (mode) goto usage; mode = M_OUT;  break; /* doc 2.0 */
		    case 'p': if (mode) goto usage; mode = M_LIST; break; /* doc 2.0 */
		    case 'm': doModes = 1; break; 	/* doc 2.0 */
		    case 'q': quiet = 1; break; 	/* doc 2.0 */
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

usage:	system("bk help -s sfio");
	return (1);
}

/*
 * sfio -o - produce an sfio file on stdout from a list on stdin.
 */
int
sfio_out()
{
	char	buf[MAXPATH];
	char	len[5];
	struct	stat sb;
	off_t	byte_count = 0;
	int	n;

	setmode(0, _O_TEXT); /* read file list in text mode */
	writen(1, SFIO_VERS, 10);
	byte_count = 10;
	while (fnext(buf, stdin)) {
		chop(buf);
		n = strlen(buf);
		sprintf(len, "%04d", n);
		writen(1, len, 4);
		writen(1, buf, n);
		byte_count += (n + 4);
		if (lstat(buf, &sb)) return (1);
		if (S_ISLNK(sb.st_mode)) {
			unless (doModes) {
				fprintf(stderr,
				"Warning: symlink %s sent as regular file.\n",
				buf);
				stat(buf, &sb);
				goto reg;
			}
			if (out_link(buf, &sb, &byte_count)) return (1);
		} else if (S_ISREG(sb.st_mode)) {
reg:			if (out_reg(buf, &sb, &byte_count)) return (1);
		} else {
			fprintf(stderr, "unknown file type %s ignored\n",  buf);
		}
	}
	save_byte_count(byte_count);
	return (0);
}

private int
out_link(char *file, struct stat *sp, off_t *byte_count)
{
	char	buf[MAXPATH];
	char	len[11];
	int	n;
	u32	sum = 0;

	n = readlink(file, buf, sizeof(buf));
	if (n == -1) {
		perror(file);
		return (1);
	}
	buf[n] = 0;	/* paranoid */
	/*
	 * We have 10 chars into which we can encode a type.
	 * We know the pathname is <= 4 chars, so we encode the
	 * symlink as "SLNK001024".
	 */
	sprintf(len, "SLNK%06u", (unsigned int)n);
	*byte_count += writen(1, len, 10);
	*byte_count += writen(1, buf, n);
	sum += adler32(sum, buf, n);
	sprintf(buf, "%010u", sum);
	*byte_count = writen(1, buf, 10);
	assert(doModes);
	sprintf(buf, "%03o", sp->st_mode & 0777);
	*byte_count = writen(1, buf, 3);
	return (0);
}

private int
out_file(char *file, struct stat *sp, off_t *byte_count)
{
	char	buf[SFIO_BSIZ];
	char	len[11];
	int	fd = open(file, 0, 0);
	int	n, nread = 0;
	u32	sum = 0;

	if (fd == -1) {
		perror(file);
		return (1);
	}
	sprintf(len, "%010u", (unsigned int)sp->st_size);
	n = writen(1, len, 10);
	*byte_count += n;
	while ((n = readn(fd, buf, sizeof(buf))) > 0) {
		nread += n;
		sum = adler32(sum, buf, n);
		if (writen(1, buf, n) != n) {
			perror(file);
			return (1);
		}
		*byte_count += n;
	}
	if (nread != sp->st_size) {
		fprintf(stderr, "Size mismatch on %s %u:%u\n",
		    file, nread, (unsigned int)sp->st_size);
		return (1);
	}
	sprintf(buf, "%010u", sum);
	n = writen(1, buf, 10);
	*byte_count += n;
	if (doModes) {
		sprintf(buf, "%03o", sp->st_mode & 0777);
		n = writen(1, buf, 3);
		*byte_count += n;
	}
	close(fd);
	return (0);
}

/*
 * Remove this in 3.0 or when we stop getting 1.x config messages.
 */
private int
out_file_compat(char *file, struct stat *sp, off_t *byte_count)
{
	char	buf[1024];
	char	len[11];
	int	fd = open(file, 0, 0);
	int	n, nread = 0;
	u32	sum = 0;

	if (fd == -1) {
		perror(file);
		return (1);
	}
	sprintf(len, "%010u", (unsigned int)sp->st_size);
	n = writen(1, len, 10);
	*byte_count += n;
	while ((n = readn(fd, buf, sizeof(buf))) > 0) {
		nread += n;
		sum += adler32(sum, buf, n);
		if (writen(1, buf, n) != n) {
			perror(file);
			return (1);
		}
		*byte_count += n;
	}
	if (nread != sp->st_size) {
		fprintf(stderr, "Size mismatch on %s %u:%u\n",
		    file, nread, (unsigned int)sp->st_size);
		return (1);
	}
	sprintf(buf, "%010u", sum);
	n = writen(1, buf, 10);
	*byte_count += n;
	if (doModes) {
		sprintf(buf, "%03o", sp->st_mode & 0777);
		n = writen(1, buf, 3);
		*byte_count += n;
	}
	close(fd);
	return (0);
}

/*
 * sfio -i - produce a tree from an sfio on stdin
 * sfio -p - produce a listing of the files in the sfio and verify checksums
 */
int
sfio_in(int extract)
{
	char	buf[MAXPATH];
	char	datalen[11];
	int	len;
	int	n;
	off_t	byte_count = 0;

	bzero(buf, sizeof(buf));
	if (readn(0, buf, 10) != 10) {
		perror("read");
		return (1);
	}
	unless (strneq(buf, SFIO_VERS, 10)) {
		fprintf(stderr,
		    "Version mismatch [%s]<>[%s]\n", buf, SFIO_VERS);
		return (1);
	}
	for (;;) {
		n = readn(0, buf, 4);
		if (n == 0) return (0);
		if (n != 4) {
			perror("read");
			return (1);
		}
		byte_count += n;
		buf[5] = 0;
		len = 0;
		sscanf(buf, "%04d", &len);
		if(len == 0) return (0); /* we got a EOF */
		if (len <= 0 || len >= MAXPATH) {
			fprintf(stderr, "Bad length in sfio\n");
			return (1);
		}
		if (readn(0, buf, len) != len) {
			perror("read");
			return (1);
		}
		byte_count += len;
		buf[len] = 0;
		if (readn(0, datalen, 10) != 10) {
			perror("read");
			return (1);
		}
		datalen[10] = 0;
		len = 0;
		if (strneq("SLNK00", datalen, 6)) {
			sscanf(&datalen[6], "%04d", &len);
			if (in_link(buf, len, extract)) return (1);
		} else {
			sscanf(datalen, "%010d", &len);
			if (in_reg(buf, len, extract)) return (1);
		}
	}
	save_byte_count(byte_count);
}

private int
in_link(char *file, int pathlen, int extract)
{
	char	buf[MAXPATH];
	mode_t	mode = 0;
	u32	sum = 0, sum2 = 0;
	unsigned int imode;  /* mode_t might be a short */

	unless (pathlen) {
		fprintf(stderr, "symlink with 0 length?\n");
		return (1);
	}
	if (readn(0, buf, pathlen) != pathlen) return (1);
	buf[pathlen] = 0;
	sum = adler32(0, buf, pathlen);
	if (extract) {
		if (symlink(buf, file)) {
			mkdirf(file);
			if (symlink(buf, file)) {
				perror(file);
				return (1);
			}
		}
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
	if (readn(0, buf, 3) != 3) {
		perror("mode read");
		goto err;
	}
	sscanf(buf, "%03o", &imode);
	mode = imode;
	if (extract) {
		/*
		 * This is trying to chmod what is pointed to, which
		 * is not what we want.
		chmod(file, mode & 0777);
		 */
	}
	unless (quiet) fprintf(stderr, "%s\n", file);
	return (0);

err:	
	if (extract) unlink(file);
	return (1);
}

private int
in_file(char *file, int todo, int extract)
{
	char	buf[SFIO_BSIZ];
	int	n;
	int	fd = -1;
	mode_t	mode = 0;
	u32	sum = 0, sum2 = 0;

	if (extract) {
		fd = mkfile(file);
		/* do NOT jump to err, it unlinks and the file may exist */
		if (fd == -1) return (1);
	}
	/* Don't try to read zero bytes.  */
	unless (todo) goto done;
	while ((n = readn(0, buf, min(todo, sizeof(buf)))) > 0) {
		todo -= n;
		sum = adler32(sum, buf, n);
		unless (extract) continue;
		/*
		 * This write statement accounts for 57% sfio's execution
		 * time on NT. Replacing it with stream mode I/0 did not help
		 * much.  Another 35% is consumed by the mkdir() call in
		 * mkfile(). File I/O on NT is 4X slower than Linux.
		 */
		if (writen(fd, buf, n) != n) {
			perror(file);
			goto err;
		}
	}
	if (todo) {
		fprintf(stderr, "Premature EOF on %s\n", file);
		goto err;
	}
done:	if (readn(0, buf, 10) != 10) {
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
		if (doModes) {
			chmod(file, mode & 0777);
		} else {
			chmod(file, 0444);
		}
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

/*
 * Remove this in 3.0 or when we stop getting 1.x config messages.
 */
private int
in_file_compat(char *file, int todo, int extract)
{
	char	buf[1024];
	int	n;
	int	fd = -1;
	mode_t	mode = 0;
	u32	sum = 0, sum2 = 0;

	if (extract) {
		fd = mkfile(file);
		/* do NOT jump to err, it unlinks and the file may exist */
		if (fd == -1) return (1);
	}
	/* Don't try to read zero bytes.  */
	unless (todo) goto done;
	while ((n = readn(0, buf, min(todo, sizeof(buf)))) > 0) {
		todo -= n;
		sum += adler32(sum, buf, n);
		unless (extract) continue;
		/*
		 * This write statement accounts for 57% sfio's execution
		 * time on NT. Replacing it with stream mode I/0 did not help
		 * much.  Another 35% is consumed by the mkdir() call in
		 * mkfile(). File I/O on NT is 4X slower than Linux.
		 */
		if (writen(fd, buf, n) != n) {
			perror(file);
			goto err;
		}
	}
	if (todo) {
		fprintf(stderr, "Premature EOF on %s\n", file);
		goto err;
	}
done:	if (readn(0, buf, 10) != 10) {
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
		if (doModes) {
			chmod(file, mode & 0777);
		} else {
			chmod(file, 0444);
		}
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
