/*
 * Copyright 2012-2013,2015-2016 BitMover, Inc
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
#include "sfio.h"

private	int	sfio_in(int extract);
private int	in_link(char *file, int todo, int extract);
private int	in_file(char *file, int todo, int extract);
private	int	mkfile(char *file);

private	int	quiet;
private	int	doModes;
private	int	echo;		/* echo files to stdout as they are written */
private	int	force;		/* overwrite existing files */

#define M_IN	1
#define M_OUT	2
#define M_LIST	3

int
main(int ac, char **av)
{
	int	c, mode = 0;

	setmode(0, O_BINARY);
	setbuf(stdout, 0);
	while ((c = getopt(ac, av, "efimopq", 0)) != -1) {
		switch (c) {
		    case 'e': echo = 1; break;			/* doc 2.3 */
		    case 'f': force = 1; break;			/* doc */
		    case 'i': 					/* doc 2.0 */
			if (mode) goto usage; mode = M_IN;   break;
		    case 'o': 					/* doc 2.0 */
			if (mode) goto usage; mode = M_OUT;  break;
		    case 'p': 					/* doc 2.0 */
			if (mode) goto usage; mode = M_LIST; break; 
		    case 'm': doModes = 1; break; 		/* doc 2.0 */
		    case 'q': quiet = 1; break; 		/* doc 2.0 */
		    default:
			goto usage;
		}
	}
	/* ignore "-" it makes bk -r sfio -o work */
	if (av[optind] && streq(av[optind], "-")) optind++;
	if (optind != ac) goto usage;

	if      (mode == M_OUT)  {
		fprintf(stderr, "sfio: -o not supported\n");
		exit(1);
	}
	if      (mode == M_IN)   return (sfio_in(1));
	else if (mode == M_LIST) return (sfio_in(0));

usage:	fprintf(stderr, "usage: bk sfio [-qm] -i < <archive.sfio>\n");
	return (1);
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
	unless (strneq(buf, SFIO_VERS(doModes), 10)) {
		fprintf(stderr,
		    "Version mismatch [%s]<>[%s]\n", buf, SFIO_VERS(doModes));
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
			if (in_file(buf, len, extract)) return (1);
		}
	}
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
			if (force) unlink(file);
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
	unless (quiet) printf("%s\n", file);
	if (echo) printf("%s\n", file);
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
	int	exists = 0;
	mode_t	mode = 0;
	u32	sum = 0, sum2 = 0;

	if (extract) {
		fd = mkfile(file);
		/* do NOT jump to err, it unlinks and the file may exist */
		if (fd == -1) {
			if ((errno != EEXIST) || (size(file) != todo)) {
				perror(file);
				return (1);
			}
			exists = 1;
		}
	}
	/* Don't try to read zero bytes.  */
	unless (todo) goto done;
	while ((n = readn(0, buf, min(todo, sizeof(buf)))) > 0) {
		todo -= n;
		sum = adler32(sum, buf, n);
		if (exists || !extract) continue;
		/*
		 * This write statement accounts for 57% sfio's execution
		 * time on NT. Replacing it with stream mode I/0 did not help
		 * much.  Another 35% is consumed by the mkdir() call in
		 * mkfile(). File I/O on NT is 4X slower than Linux.
		 */
		if (write(fd, buf, n) != n) {
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
	if (exists) {
		fprintf(stderr, "%s already exists.\n", file);
		return (1);
	} else if (extract) {
		close(fd);
		if (doModes) {
			chmod(file, mode & 0777);
		} else {
			chmod(file, 0444);
		}
	}
	unless (quiet) printf("%s\n", file);
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

	if (reserved(basenm(file))) {
bad_name:	fprintf(stderr, "sfio: reserved filename '%s'\n", file);
		errno = EINVAL;
		return (-1);
	}

#ifdef WIN32
	/*
	 * Check trailing dot, this only happen when we clone
	 * from Unix to Windows
	 */
	if (file[strlen(file) -1 ] == '.') goto bad_name;
#endif

	if (access(file, F_OK) == 0) {
		return (-1);
	}
again:	fd = open(file, O_CREAT|O_EXCL|O_WRONLY, 0666);
	setmode(fd, _O_BINARY);
	if (fd != -1) return (fd);
	if (errno == EINVAL) goto bad_name;
	if (errno == EEXIST) {
		perror(file);
		return (-1);
	}
	if (first) {
		if (mkdirf(file)) {
			fputs("\n", stderr);
			perror(file);
			if (errno == EINVAL) goto bad_name;
		}
		first = 0;
		goto again;
	}
	return (-1);
}
