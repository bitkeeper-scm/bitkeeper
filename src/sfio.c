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
#include "zlib/zlib.h"
#ifdef SFIO_STANDALONE
#include "utils/sfio.h"
#else
#include "sccs.h"
#include "binpool.h"
#endif

#undef	unlink		/* I know the files are writable, I created them */
#define	SFIO_BSIZ	4096
#define	SFIO_NOMODE	"SFIO v 1.4"	/* must be 10 bytes exactly */
#define	SFIO_MODE	"SFIO vm1.4"	/* must be 10 bytes exactly */
#define	SFIO_VERS	(opts->doModes ? SFIO_MODE : SFIO_NOMODE)
/* error returns, don't use 1, that's generic */
#define	SFIO_LSTAT	2
#define	SFIO_READLINK	3
#define	SFIO_OPEN	4
#define	SFIO_SIZE	5
#define	SFIO_LOOKUP	6

private	int	sfio_out(void);
private int	out_file(char *file, struct stat *, off_t *byte_count);
private int	out_bptuple(char *tuple, off_t *byte_count);
private int	out_link(char *file, struct stat *, off_t *byte_count);
private	int	sfio_in(int extract);
private int	in_link(char *file, int todo, int extract);
private int	in_file(char *file, int todo, int extract);
private	int	mkfile(char *file);
private	void	send_eof(int status);

struct {
	u32	quiet:1;	/* suppress normal verbose output */
	u32	doModes:1;	/* vm1.4 - sends permissions */
	u32	echo:1;		/* echo files to stdout as they are written */
	u32	force:1;	/* overwrite existing files */
	u32	bp_tuple:1;	/* -Bk rkey dkey bphash path */
	int	mode;		/* M_IN, M_OUT, M_LIST */
	char	newline;	/* -r makes it \r instead of \n */
	char	**more;		/* additional list of files to send */
	MDBM	*sent;		/* list of d.files we have set, for dups */
	MDBM	*attribs;	/* attributes for binpool */
} *opts;

#define M_IN	1
#define M_OUT	2
#define M_LIST	3

int
sfio_main(int ac, char **av)
{
	int	c;

	opts = (void*)calloc(1, sizeof(*opts));
	opts->newline = '\n';
	setmode(0, O_BINARY);
	while ((c = getopt(ac, av, "a;A;B;efimopqr")) != -1) {
		switch (c) {
		    case 'a':
			opts->more = addLine(opts->more, strdup(optarg));
			break;
		    case 'A':
			opts->more = file2Lines(opts->more, optarg);
			break;
		    case 'B':
			while (*optarg) {
				if (*optarg== 'k') opts->bp_tuple = 1;
				optarg++;
			}
			break;
		    case 'e': opts->echo = 1; break;		/* doc 2.3 */
		    case 'f': opts->force = 1; break;		/* doc */
		    case 'i': 					/* doc 2.0 */
			if (opts->mode) goto usage;
			opts->mode = M_IN;
			break;
		    case 'o': 					/* doc 2.0 */
			if (opts->mode) goto usage;
			opts->mode = M_OUT;
			break;
		    case 'p': 					/* doc 2.0 */
			if (opts->mode) goto usage;
			opts->mode = M_LIST;
			break; 
		    case 'm': opts->doModes = 1; break; 	/* doc 2.0 */
		    case 'q': opts->quiet = 1; break; 		/* doc 2.0 */
		    case 'r': opts->newline = '\r'; break;
		    default:
			goto usage;
		}
	}
	/* ignore "-" it makes bk -r sfio -o work */
	if (av[optind] && streq(av[optind], "-")) optind++;
	if (optind != ac) goto usage;
	if (opts->more && (opts->mode != M_OUT)) goto usage;

	if (opts->mode == M_OUT)       return (sfio_out());
	else if (opts->mode == M_IN)   return (sfio_in(1));
	else if (opts->mode == M_LIST) return (sfio_in(0));

usage:	system("bk help -s sfio");
	free(opts);
	return (1);
}

private char *
nextfile(char *buf)
{
	static	int eof = 0;
	static	int i = 1;

	unless (eof) {
		if (fgets(buf, MAXPATH, stdin)) return (buf);
		eof = 1;
	}
	unless (opts->more && (i <= nLines(opts->more))) return (0);
	sprintf(buf, "%s\n", opts->more[i++]);
	return (buf);
}

/*
 * sfio -o - produce an sfio file on stdout from a list on stdin.
 */
int
sfio_out(void)
{
	char	buf[MAXPATH];
	char	len[5];
	struct	stat sb;
	off_t	byte_count = 0;
	int	n;

	setmode(0, _O_TEXT); /* read file list in text mode */
	writen(1, SFIO_VERS, 10);
	if (opts->bp_tuple) {
		opts->sent = mdbm_mem();
		opts->attribs = mdbm_mem();
	}
	byte_count = 10;
	while (nextfile(buf)) {
		chomp(buf);
		unless (opts->quiet) fprintf(stderr, "%s\n", buf);
		if (opts->bp_tuple) {
			if (n = out_bptuple(buf, &byte_count)) {
				send_eof(n);
				return (n);
			}
			continue;
		}
		if (lstat(buf, &sb)) {
			perror(buf);
			send_eof(SFIO_LSTAT);
			return (SFIO_LSTAT);
		}
		n = strlen(buf);
		sprintf(len, "%04d", n);
		writen(1, len, 4);
		writen(1, buf, n);
		byte_count += (n + 4);
		if (S_ISLNK(sb.st_mode)) {
			unless (opts->doModes) {
				fprintf(stderr,
				"Warning: symlink %s sent as regular file.\n",
				buf);
				stat(buf, &sb);
				goto reg;
			}
			if (n = out_link(buf, &sb, &byte_count)) {
				send_eof(n);
				return (n);
			}
		} else if (S_ISREG(sb.st_mode)) {
reg:			if (n = out_file(buf, &sb, &byte_count)) {
				send_eof(n);
				return (n);
			}
		} else {
			fprintf(stderr, "unknown file %s ignored\n", buf);
			// No error?
		}
	}
#ifndef SFIO_STANDALONE
	save_byte_count(byte_count);
	if (opts->bp_tuple) {
		FILE	*f;
		kvpair	kv;

		sprintf(len, "%04d", 6);
		writen(1, len, 4);
		writen(1, "|ATTR|", 6);
		byte_count += (6 + 4);
		bktmp(buf, 0);
		f = fopen(buf, "w");
		EACH_KV(opts->attribs) {
			fprintf(f, "%s %s\n", kv.key.dptr, kv.val.dptr);
		}
		fclose(f);
		if (lstat(buf, &sb)) {
			perror(buf);
			send_eof(SFIO_LSTAT);
			return (SFIO_LSTAT);
		}
		out_file(buf, &sb, &byte_count);
		unlink(buf);
	}
#endif
	if (opts->more) freeLines(opts->more, free);
	if (opts->sent) mdbm_close(opts->sent);
	if (opts->attribs) mdbm_close(opts->attribs);
	free(opts);
	send_eof(0);
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
		perror("readlink");
		return (SFIO_READLINK);
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
	assert(opts->doModes);
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
	int	n, nread = 0, dosum = 1;
	u32	sum = 0;
	char	*p;

	if (fd == -1) {
		perror(file);
		return (SFIO_OPEN);
	}

	/*
	 * For binpool files we already know the expected checksum so
	 * we don't calculate it again.  If the file is corrupted it
	 * will be detected when being unpacked.
	 */
	if (opts->bp_tuple && strneq(file, "BitKeeper/binpool/", 18) &&
	    (p = strrchr(file, '.')) && (p[1] == 'd') &&
	    !getenv("_BP_HASHCHARS")) {
		p = strrchr(file, '/');
		sum = strtoul(p+1, 0, 16);
		dosum = 0;
	}

	setmode(fd, _O_BINARY);
	sprintf(len, "%010u", (unsigned int)sp->st_size);
	n = writen(1, len, 10);
	*byte_count += n;
	while ((n = readn(fd, buf, sizeof(buf))) > 0) {
		nread += n;
		if (dosum) sum = adler32(sum, buf, n);
		if (writen(1, buf, n) != n) {
			perror(file);
			close(fd);
			return (1);
		}
		*byte_count += n;
	}
	if (nread != sp->st_size) {
		fprintf(stderr, "Size mismatch on %s %u:%u\n\n", 
		    file, nread, (unsigned int)sp->st_size);
		close(fd);
		return (SFIO_SIZE);
	}
	sprintf(buf, "%010u", sum);
	n = writen(1, buf, 10);
	*byte_count += n;
	if (opts->doModes) {
		sprintf(buf, "%03o", sp->st_mode & 0777);
		n = writen(1, buf, 3);
		*byte_count += n;
	}
	close(fd);
	return (0);
}

/* rkey dkey adler32 path */
private int
out_bptuple(char *tuple, off_t *byte_count)
{
#ifndef SFIO_STANDALONE
	char	*keys, *path, *dfile, *freeme;
	int	n;
	struct	stat sb;
	char	buf[MAXPATH];

// ttyprintf("TUPL %s\n", tuple);
	keys = tuple;
	path = strchr(tuple, ' ');
	assert(path);
	if (path = strchr(path+1, ' ')) *path++ = 0;

	unless (freeme = bp_lookupkeys(0, keys)) {
// ttyprintf("MISS %s\n", tuple);
		fprintf(stderr, "lookupkeys(%s) = %s\n",
		    keys, sys_errlist[errno]);
		return (SFIO_LOOKUP);
	}
	dfile = freeme + strlen(proj_root(0)) + 1;
	mdbm_store_str(opts->attribs, keys, dfile, MDBM_INSERT);
	if (mdbm_store_str(opts->sent, dfile, keys, MDBM_INSERT) &&
	    (errno == EEXIST)) {
// ttyprintf("DUP  %s\n", dfile);
		free(freeme);
		return (0);
	}
	/* d.file */
	if (lstat(dfile, &sb)) {
		perror(dfile);
		return (SFIO_LSTAT);
	}
	/* allow them to override path if they want (for stuff repair|get) */
	unless (path) path = dfile;
	n = strlen(path);
	sprintf(buf, "%04d", n);
	writen(1, buf, 4);
	writen(1, path, n);
	*byte_count += (n + 4);
// ttyprintf("DATA %s\n", path);
	n = out_file(path, &sb, byte_count);

	free(freeme);
	return (n);
#else
	fprintf(stderr, "Unsupported.\n");
	return (1);
#endif
}

/*
 * Send an eof by sending a 0 or less than 0 for the pathlen of the "next"
 * file.  Safe to do with old versions of sfio, they'll handle it properly.
 */
private void
send_eof(int error)
{
	char	buf[10];

// ttyprintf("DONE %d\n", error);
	/* negative lengths are exit codes */
	if (error > 0) error = -error;
	sprintf(buf, "%4d", error);
	assert(strlen(buf) == 4);
	writen(1, buf, 4);
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
		if (len <= 0) {
			if (len < 0) {
				fprintf(stderr, "Incomplete archive: ");
				switch (-len) {
				    case SFIO_LSTAT:
					fprintf(stderr, "lstat failed\n");
					break;
				    case SFIO_READLINK:
					fprintf(stderr, "readlink failed\n");
					break;
				    case SFIO_OPEN:
					fprintf(stderr, "open failed\n");
					break;
				    case SFIO_SIZE:
					fprintf(stderr, "file changed size\n");
					break;
				    case SFIO_LOOKUP:
					fprintf(stderr,
					    "binpool lookup failed\n");
					break;
				    default:
					fprintf(stderr,
					    "unknown error %d\n", -len);
				    	break;
				}
			}
			return (-len); /* we got a EOF */
		}
		if (len >= MAXPATH) {
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
#ifndef SFIO_STANDALONE
	save_byte_count(byte_count);
#endif
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
			if (opts->force) unlink(file);
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
	unless (opts->quiet) fprintf(stderr, "%s%c", file, opts->newline);
	if (opts->echo) printf("%s\n", file);
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
	if ((sum != sum2) && !getenv("_BK_ALLOW_BAD_CRC")) {
		fprintf(stderr,
		    "Checksum mismatch %u:%u for %s\n", sum, sum2, file);
		goto err;
	}
	if (opts->doModes) {
		unsigned int imode;  /* mode_t might be a short */
		if (readn(0, buf, 3) != 3) {
			perror("mode read");
			goto err;
		}
		sscanf(buf, "%03o", &imode);
		mode = imode;
	}
	if (exists) {
		if (adler32_file(file) == sum) {
			unless (opts->quiet) {
				fprintf(stderr,
				    "%s existed but contents match, skipped.\n",
				    file);
			}
		} else {
			fprintf(stderr, "%s already exists.\n", file);
			return (1);
		}
	} else if (extract) {
		close(fd);
		if (opts->doModes) {
			chmod(file, mode & 0777);
		} else {
			chmod(file, 0444);
		}
	}
	unless (opts->quiet) fprintf(stderr, "%s%c", file, opts->newline);
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
bad_name:	getMsg("reserved_name", file, '=', stderr);
		errno = EINVAL;
		return (-1);
	}

#ifdef WIN32
	/*
	 * Check trailing dot, this only happen when we clone
	 * from Unix to Windows
	 */
	if (file[strlen(file) -1 ] == '.') {
		getMsg("win_trailing_dot", file, '=', stderr);
	}
#endif

	if (access(file, F_OK) == 0) {
#ifndef SFIO_STANDALONE
		char	realname[MAXPATH];

		getRealName(file, NULL, realname);
		unless (streq(file, realname)) {
			getMsg2("case_conflict", file, realname, '=', stderr);
			errno = EINVAL;
		} else {
			if (opts->force) {
				unlink(file);
				goto again;
			}
			errno = EEXIST;
		}
#endif
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
