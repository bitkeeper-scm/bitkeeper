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
 *
 * opts not appearing in 'bk help sfio': a;A;b;BHrR;
 * 	a <fileName> - add this file name to the list of things to pack
 * 		NOTE: filename could be a bptuple if option is set
 * 	A <fileName> - file with each line being like '-a' option
 * 	b <size-ish> - look for wacky bk size string (suffixes)
 * 		used in reading and writing for printing
 * 		uses some rounding so may go above 100%? or not get there?
 * 	B - interpret file names as bptuple names
 * 	H - hardlinks - sfio encodes hardlink info
 * 		XXX can't find any usage
 * 	l - local - do not recurse to any BAM servers; typical usage is
 * 		when bk havekeys does -l then sfio wants -l
 * 	r - use \r to terminate output trace - XXX: no padding to erase
 */
#include "system.h"
#ifdef SFIO_STANDALONE
#include "utils/sfio.h"
#define	scansize(x)	0
#else
#include "sccs.h"
#include "bam.h"
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
#define	SFIO_MORE	7	/* another sfio follows */

#define	NEWLINE()	if ((opts->newline == '\r') && !opts->quiet) \
				fputc('\n', stderr)

private	int	sfio_out(void);
private int	out_file(char *file, struct stat *, off_t *byte_count);
private int	out_bptuple(char *tuple, off_t *byte_count);
private int	out_symlink(char *file, struct stat *, off_t *byte_count);
private int	out_hardlink(char *file,
		    struct stat *, off_t *byte_count, char *linkMe);
private	int	sfio_in(int extract);
private int	in_bptuple(char *file, char *datalen, int extract);
private int	in_symlink(char *file, int todo, int extract);
private int	in_hardlink(char *file, int todo, int extract);
private int	in_file(char *file, u32 todo, int extract);
private	int	mkfile(char *file);
private	void	send_eof(int status);
private void	missing(off_t *byte_count);

struct {
	u32	quiet:1;	/* suppress normal verbose output */
	u32	doModes:1;	/* vm1.4 - sends permissions */
	u32	echo:1;		/* echo files to stdout as they are written */
	u32	force:1;	/* overwrite existing files */
	u32	bp_tuple:1;	/* -B rkey dkey bphash path */
	u32	hardlinks:1;	/* save hardlinks */
	u32	key2path:1;	/* -K read keys on stdin */
	int	mode;		/* M_IN, M_OUT, M_LIST */
	char	newline;	/* -r makes it \r instead of \n */
	char	**more;		/* additional list of files to send */
	char	*prefix;	/* dir prefix to put on the file listing */
	hash	*sent;		/* list of d.files we have set, for dups */
	int	recurse;	/* if set, try and find a server on cachemiss */
	char	**missing;	/* tuples we couldn't find here */
	u64	todo;		/* -b`psize` - bytes we think we are moving */
	u64	done;		/* file bytes we've moved so far */
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
	opts->recurse = 1;
	opts->prefix = "";
	setmode(0, O_BINARY);
	while ((c = getopt(ac, av, "a;A;b;BefHiKlmopP;qr")) != -1) {
		switch (c) {
		    case 'a':
			opts->more = addLine(opts->more, strdup(optarg));
			break;
		    case 'A':
			opts->more = file2Lines(opts->more, optarg);
			break;
		    case 'b':
		    	opts->todo = scansize(optarg);
			break;
		    case 'B': opts->bp_tuple = 1; break;
		    case 'e': opts->echo = 1; break;		/* doc 2.3 */
		    case 'f': opts->force = 1; break;		/* doc */
		    case 'H': opts->hardlinks = 1; break;
		    case 'i': 					/* doc 2.0 */
			if (opts->mode) goto usage;
			opts->mode = M_IN;
			break;
		    case 'K': opts->key2path = 1; break;
		    case 'l': opts->recurse = 0; break;
		    case 'o': 					/* doc 2.0 */
			if (opts->mode) goto usage;
			opts->mode = M_OUT;
			break;
		    case 'p': 					/* doc 2.0 */
			if (opts->mode) goto usage;
			opts->mode = M_LIST;
			break; 
		    case 'P': opts->prefix = optarg; break;
		    case 'm': opts->doModes = 1; break; 	/* doc 2.0 */
		    case 'q': opts->quiet = 1; break; 		/* doc 2.0 */
		    case 'r': opts->newline = '\r'; break;
		    default:
			goto usage;
		}
	}
	if (getenv("BK_NO_SFIO_PREFIX")) opts->prefix = "";	// just in case

	/*
	 * If we're being executed remotely don't let them grab everything.
	 */
	if (getenv("_BK_VIA_REMOTE") && !(opts->bp_tuple || opts->key2path)) {
		fprintf(stderr, "Remote is limited to keys.\n");
		exit(1);
	}
	/* ignore "-" it makes bk -r sfio -o work */
	if (av[optind] && streq(av[optind], "-")) optind++;
	if (optind != ac) goto usage;
	if ((opts->more || opts->key2path) && (opts->mode != M_OUT)) {
		goto usage;
	}
#ifdef	WIN32
	if (opts->hardlinks) {
		fprintf(stderr, "%s: hardlink-mode not supported on Windows\n",
		    av[0]);
		return (1);
	}
#endif
#ifndef	SFIO_STANDALONE
	if (opts->bp_tuple) {
		if (proj_cd2root()) {
			fprintf(stderr, "%s: not in repository\n", av[0]);
			return (1);
		}
		if (proj_isResync(0)) chdir(RESYNC2ROOT);
	}
#endif
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
	char	*gfile, *sfile;
	hash	*links = 0;
	MDBM	*idDB = 0;
	char	ln[32];

	setmode(0, _O_TEXT); /* read file list in text mode */
	writen(1, SFIO_VERS, 10);

	if (opts->key2path) idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	if (opts->bp_tuple) opts->sent = hash_new(HASH_MEMHASH);
	if (opts->hardlinks) links = hash_new(HASH_MEMHASH);
	byte_count = 10;
	while (nextfile(buf)) {
		chomp(buf);
		if (opts->bp_tuple) {
			if (n = out_bptuple(buf, &byte_count)) {
				send_eof(n);
				return (n);
			}
			continue;
		}
		if (opts->key2path) {
			unless (gfile = key2path(buf, idDB)) {
				continue;
			}
			sfile = name2sccs(gfile);
			strcpy(buf, sfile);
			free(gfile);
			free(sfile);
			if (lstat(buf, &sb)) continue;
		} else {
			if (lstat(buf, &sb)) {
				perror(buf);
				send_eof(SFIO_LSTAT);
				return (SFIO_LSTAT);
			}
		}
		n = strlen(buf);
		sprintf(len, "%04d", n);
		writen(1, len, 4);
		writen(1, buf, n);
		byte_count += (n + 4);
		if (opts->hardlinks) {
			sprintf(ln, "%x %x", (u32)sb.st_dev, (u32)sb.st_ino);
			unless (hash_insertStr(links, ln, buf)) {
				n = out_hardlink(buf, &sb, &byte_count,
				    links->vptr);
				if (n) {
					hash_free(links);
					send_eof(n);
					return (n);
				}
				continue;
			}
		}
		if (S_ISLNK(sb.st_mode)) {
			unless (opts->doModes) {
				fprintf(stderr,
				"Warning: symlink %s sent as regular file.\n",
				buf);
				stat(buf, &sb);
				goto reg;
			}
			if (n = out_symlink(buf, &sb, &byte_count)) {
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
	if (opts->missing) {
		missing(&byte_count);
		freeLines(opts->missing, free);
	} else {
		send_eof(0);
	}
#ifndef SFIO_STANDALONE
	save_byte_count(byte_count);
#endif
	if (opts->more) freeLines(opts->more, free);
	if (opts->sent) hash_free(opts->sent);
	if (opts->hardlinks) hash_free(links);
	NEWLINE();
	free(opts);
	return (0);
}

private int
out_symlink(char *file, struct stat *sp, off_t *byte_count)
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
	*byte_count += writen(1, buf, 10);
	assert(opts->doModes);
	sprintf(buf, "%03o", sp->st_mode & 0777);
	*byte_count += writen(1, buf, 3);
	unless (opts->quiet) {
		fprintf(stderr, "%s%s%c", opts->prefix, file, opts->newline);
	}
	return (0);
}

private int
out_hardlink(char *file, struct stat *sp, off_t *byte_count, char *linkMe)
{
	char	buf[MAXPATH];
	char	len[11];
	int	n;
	u32	sum = 0;


	n = strlen(linkMe);
	/*
	 * We have 10 chars into which we can encode a type.
	 * We know the pathname is <= 4 chars, so we encode the
	 * hardlink as "HLNK001024".
	 */
	sprintf(len, "HLNK%06u", (unsigned int)n);
	*byte_count += writen(1, len, 10);
	*byte_count += writen(1, linkMe, n);
	sum += adler32(sum, linkMe, n);
	sprintf(buf, "%010u", sum);
	*byte_count += writen(1, buf, 10);
	if (opts->doModes) {
		sprintf(buf, "%03o", sp->st_mode & 0777);
		*byte_count += writen(1, buf, 3);
	}
	unless (opts->quiet) {
		fprintf(stderr, "%s%s%c", opts->prefix, file, opts->newline);
	}
	return (0);
}

private int
out_file(char *file, struct stat *sp, off_t *byte_count)
{
	char	buf[SFIO_BSIZ];
	char	len[11];
	int	fd = open(file, 0, 0);
	int	n, nread = 0, dosum = 1;
	u32	sum = 0, sz = (u32)sp->st_size;
	char	*p;

	if (fd == -1) {
		perror(file);
		return (SFIO_OPEN);
	}

	/*
	 * For BAM files we already know the expected checksum so
	 * we don't calculate it again.  If the file is corrupted it
	 * will be detected when being unpacked.
	 */
	if (opts->bp_tuple && (p = strrchr(file, '.')) && (p[1] == 'd') &&
	    !getenv("_BP_HASHCHARS")) {
		while (*p != '/') --p;
		sum = strtoul(p+1, 0, 16);
		dosum = 0;
	}

	setmode(fd, _O_BINARY);
	sprintf(len, "%010u", (unsigned int)sp->st_size);
	n = writen(1, len, 10);
	*byte_count += n;
	while ((n = readn(fd, buf, sizeof(buf))) > 0) {
		nread += n;
		opts->done += n;
		if (dosum) sum = adler32(sum, buf, n);
		if (writen(1, buf, n) != n) {
			if ((errno != EPIPE) || getenv("BK_SHOWPROC")) {
				perror(file);
			}
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
#ifndef	SFIO_STANDALONE
	unless (opts->quiet) {
		if (opts->newline == '\r') {
			if (opts->todo) {
				sprintf(buf, "%s (%s of %s)",
				    file, psize(opts->done), psize(opts->todo));
			} else {
				sprintf(buf, "%s (+%s = %s)", 
				    file, psize(sz), psize(opts->done));
			}
			fprintf(stderr, "%-72s\r", buf);
		} else {
			fprintf(stderr, "%s%s\n", opts->prefix, file);
		}
	}
#endif
	return (0);
}

/* adler32.md5sum rkey dkey */
private int
out_bptuple(char *keys, off_t *byte_count)
{
#ifndef SFIO_STANDALONE
	char	*path, *freeme;
	int	n;
	struct	stat sb;
	char	buf[MAXPATH];

	unless (freeme = bp_lookupkeys(0, keys)) {
		if (opts->recurse) {
			opts->missing = addLine(opts->missing, strdup(keys));
			return (0);
		}
		fprintf(stderr, "lookupkeys(%s) failed\n", keys);
		return (SFIO_LOOKUP);
	}
	path = freeme + strlen(proj_root(0)) + 1;

	/* d.file */
	if (lstat(path, &sb)) {
		perror(path);
		return (SFIO_LSTAT);
	}
	n = strlen(keys);
	sprintf(buf, "%04d", n);
	writen(1, buf, 4);
	writen(1, keys, n);
	*byte_count += (n + 4);
	unless (hash_insertStr(opts->sent, path, keys)) {
		n = out_hardlink(path, &sb, byte_count, opts->sent->vptr);
	} else {
		n = out_file(path, &sb, byte_count);
	}
	free(freeme);
	return (n);
#else
	fprintf(stderr, "Unsupported.\n");
	return (1);
#endif
}

/*
 * Go fetch the missing BAM chunks.
 */
private void
missing(off_t *byte_count)
{
#ifndef	SFIO_STANDALONE
	char	*p, *tmpf;
	FILE	*f;
	int	i;
	char	buf[BUFSIZ];

	if (bp_serverID(&p) || (p == 0)) {
err:		send_eof(SFIO_LOOKUP);
		return;
	}
	free(p);
	tmpf = bktmp(0, "bp_missing");
	unless (f = fopen(tmpf, "w")) {
		perror(tmpf);
		free(tmpf);
		goto err;
	}
	EACH(opts->missing) fprintf(f, "%s\n", opts->missing[i]);
	fclose(f);
	/*
	 * XXX - this will fail miserably on loops or locks.
	 */
	p = aprintf("bk -q@'%s' -Lr -Bstdin sfio -qoB - < '%s'",
	    proj_configval(0, "BAM_server"), tmpf);
	f = popen(p, "r");
	free(p);
	unless (f) goto err;
	send_eof(SFIO_MORE);
	while ((i = fread(buf, 1, sizeof(buf), f)) > 0) {
		writen(1, buf, i);
		*byte_count += i;
	}
	pclose(f);	// If we error here we're just hosed.
	unlink(tmpf);
	free(tmpf);
	/* they should have sent EOF */
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
	int	len, n;
	u32	ulen;
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
		buf[4] = 0;
		len = 0;
		sscanf(buf, "%04d", &len);
		if (len <= 0) {
			if (len < 0) {
				if (-len == SFIO_MORE) {
					return (sfio_in(extract));
				}
				NEWLINE();
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
					fprintf(stderr, "BAM lookup failed\n");
					break;
				    default:
					fprintf(stderr,
					    "unknown error %d\n", -len);
				    	break;
				}
			}
			NEWLINE();
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
		if (opts->bp_tuple) {
			if (in_bptuple(buf, datalen, extract)) return (1);
		} else if (strneq("SLNK00", datalen, 6)) {
			sscanf(&datalen[6], "%04d", &len);
			if (in_symlink(buf, len, extract)) return (1);
		} else if (strneq("HLNK00", datalen, 6)) {
			sscanf(&datalen[6], "%04d", &len);
			if (in_hardlink(buf, len, extract)) return (1);
		} else {
			sscanf(datalen, "%010u", &ulen);
			if (in_file(buf, ulen, extract)) return (1);
		}
	}
#ifndef SFIO_STANDALONE
	save_byte_count(byte_count);
#endif
}

private int
in_bptuple(char *keys, char *datalen, int extract)
{
#ifndef	SFIO_STANDALONE
	int	todo, i, j;
	char	*p;
	u32	sum = 0, sum2 = 0;
	char	file[MAXPATH];
	char	tmp[MAXPATH];

	if (strneq("HLNK00", datalen, 6)) {
		sscanf(&datalen[6], "%04d", &todo);
		/* keys linked to older keys we load into file */
		if (readn(0, file, todo) != todo) return (1);
		file[todo] = 0;
		sum = adler32(0, file, todo);
		if (readn(0, tmp, 10) != 10) {
			perror("chksum read");
			return (1);
		}
		sscanf(tmp, "%010u", &sum2);
		if (sum != sum2) {
			fprintf(stderr, "Checksum mismatch %u:%u for %s\n",
			    sum, sum2, keys);
			return (1);
		}
		if (opts->doModes) {
			if (readn(0, tmp, 3) != 3) {
				perror("mode read");
				return (1);
			}
		}
		/* find bp file used by the linked keys and use that */
		unless (p = mdbm_fetch_str(proj_BAMindex(0, 0), file)) {
			fprintf(stderr, "sfio: hardlink to %s failed.\n",
			    file);
			return (1);
		}
	} else {
		sscanf(datalen, "%010d", &todo);

		/* extract to new file in BAM pool (adler32 is first in keys) */
		p = file + sprintf(file, BAM_ROOT "/%c%c/%.*s",
		    keys[0], keys[1], 8, keys);
		/* find unused entry */
		for (i = 1; ; i++) {
			sprintf(p, ".d%d", i);
			unless (exists(file)) break;
		}
		if (in_file(file, todo, extract)) return (1);
		/* see if we should collapse with other files */
		strcpy(tmp, file);
		for (j = 1; j <= i; j++) {
			sprintf(p, ".d%d", j);
			if (j == i) break;
			if ((size(file) == todo) && sameFiles(file, tmp)) {
				/* we already have a copy of this file */
				unlink(tmp);
				break;
			}
		}
		/* file is now the right name for this data */
		p = strchr(file, '/'); /* skip BitKeeper/BAM/ */
		p = strchr(p+1, '/') + 1;
	}
	mdbm_store_str(proj_BAMindex(0, 1), keys, p, MDBM_REPLACE);
	bp_logUpdate(keys, p);
	return (0);
#else
	fprintf(stderr, "Unsupported.\n");
	return (1);
#endif
}

private int
in_symlink(char *file, int pathlen, int extract)
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
	unless (opts->quiet) {
		fprintf(stderr, "%s%s%c", opts->prefix, file, opts->newline);
	}
	if (opts->echo) printf("%s\n", file);
	return (0);

err:	
	if (extract) unlink(file);
	return (1);
}

private int
in_hardlink(char *file, int pathlen, int extract)
{
	char	buf[MAXPATH];
	u32	sum = 0, sum2 = 0;

	assert(pathlen > 0);
	if (readn(0, buf, pathlen) != pathlen) return (1);
	buf[pathlen] = 0;
	sum = adler32(0, buf, pathlen);
	if (extract) {
		if (link(buf, file)) {
			mkdirf(file);
			if (opts->force) unlink(file);
			if (link(buf, file)) {
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
	if (opts->doModes) {
		if (readn(0, buf, 3) != 3) {
			perror("mode read");
			goto err;
		}
	}
	unless (opts->quiet) {
		fprintf(stderr, "%s%s%c", opts->prefix, file, opts->newline);
	}
	if (opts->echo) printf("%s\n", file);
	return (0);

err:
	if (extract) unlink(file);
	return (1);
}

private int
in_file(char *file, u32 todo, int extract)
{
	char	buf[SFIO_BSIZ];
	int	n;
	int	fd = -1;
	int	exists = 0;
	mode_t	mode = 0;
	u32	sum = 0, sum2 = 0, sz = todo;

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
		opts->done += n;
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
			return (0);
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
#ifndef	SFIO_STANDALONE
	unless (opts->quiet) {
		if (opts->newline == '\r') {
			if (opts->todo) {
				sprintf(buf, "%s (%s of %s)",
				    file, psize(opts->done), psize(opts->todo));
			} else {
				sprintf(buf, "%s (+%s = %s)", 
				    file, psize(sz), psize(opts->done));
			}
			fprintf(stderr, "%-72s\r", buf);
		} else {
			fprintf(stderr, "%s%s\n", opts->prefix, file);
		}
	}
#endif
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
