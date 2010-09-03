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
 * 		if sfio includes files hardlinked on the source they will
 *		be hardlinked at dest
 * 	l - local - do not recurse to any BAM servers; typical usage is
 * 		when bk havekeys does -l then sfio wants -l
 *
 * When calling this for a remote BAM server, the command line should
 * look like this:
 *     # fetching data
 *     bk -q@URL -Lr -zo0 -Bstdin sfio -oBq [-l] - < keylist > sfio
 *     (read lock, compress keys not data, buffer input)
 *
 *	# sending data
 *	bk -q@URL -Lw -z0 sfio -iBq - < sfio
 *	(write lock, no compression)
 */
#include "system.h"
#ifdef SFIO_STANDALONE
#include "utils/sfio.h"
#define	scansize(x)	0
#else
#include "sccs.h"
#include "bam.h"
#include "progress.h"
#endif

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

private	void	optsFree(void);
private	int	sfio_out(void);
private int	out_file(char *file, struct stat *, off_t *byte_count,
		    int useDsum, u32 dsum);
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
private	void	print_status(char *file, u32 sz);
private int	sfio_in_Nway(int n);
private	void	perfile(char *path);

private	struct {
	u32	quiet:1;	/* suppress normal verbose output */
	u32	doModes:1;	/* vm1.4 - sends permissions */
	u32	echo:1;		/* echo files to stdout as they are written */
	u32	force:1;	/* overwrite existing files */
	u32	bp_tuple:1;	/* -B rkey dkey bphash path */
	u32	hardlinks:1;	/* save hardlinks */
	u32	key2path:1;	/* -K read keys on stdin */
	u32	sfile2gfile:1;	/* convert sfiles to gfiles when printing */
	u32	lclone:1;	/* lclone-mode, hardlink everything */
	u32	index:1;	/* -I generate index info sfio */
	u32	mark_no_dfiles:1;
	int	mode;		/* M_IN, M_OUT, M_LIST */
	char	**more;		/* additional list of files to send */
	char	*prefix;	/* dir prefix to put on the file listing */
	hash	*sent;		/* list of d.files we have set, for dups */
	int	recurse;	/* if set, try and find a server on cachemiss */
	char	**missing;	/* tuples we couldn't find here */
	char	*lastdir;	/* for -I, a place to remember dirs */
	u64	todo;		/* -b`psize` - bytes we think we are moving */
	u64	done;		/* file bytes we've moved so far */
	u32	nfiles;		/* -N%d - number of files we'll move */
	int	prevlen;	/* length of previously printed line */
#ifndef SFIO_STANDALONE
	ticker	*tick;		/* progress bar */
#endif
} *opts;

#define M_IN	1
#define M_OUT	2
#define M_LIST	3

int
sfio_main(int ac, char **av)
{
	int	c, rc;
	int	parallel = 0;
	longopt	lopts[] = {
		{ "mark-no-dfiles", 300 },
		{ 0, 0 }
	};

	opts = (void*)calloc(1, sizeof(*opts));
	opts->recurse = 1;
	opts->prefix = "";
	setmode(0, O_BINARY);
	while ((c = getopt(ac, av, "a;A;b;BefgHIij;KLlmN;opP;qr", lopts)) != -1) {
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
		    case 'g': opts->sfile2gfile = 1; break;	/* undoc */
		    case 'H': opts->hardlinks = 1; break;
		    case 'I':
			opts->index = 1;
			opts->quiet = 1;
			opts->mode = M_LIST;
			break;
		    case 'i': 					/* doc 2.0 */
			if (opts->mode) goto usage;
			opts->mode = M_IN;
			break;
		    case 'j':
#ifdef	PARALLEL_MAX
			if ((parallel = atoi(optarg)) <= 0) {
				parallel = -1;
			} else if (parallel > PARALLEL_MAX) {
				parallel = PARALLEL_MAX;	/* cap it */
			}
#endif
			break;
		    case 'K': opts->key2path = 1; break;
		    case 'L': opts->lclone = 1; break;
		    case 'l': opts->recurse = 0; break;
		    case 'N': opts->nfiles = atoi(optarg); break;
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
		    case 300:	/* --mark-no-dfiles */
			opts->mark_no_dfiles = 1;
			break;
		    default: bk_badArg(c, av);
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
		/*
		 * This code detects hardlinked files by comparing
		 * st_dev/st_inode and nt_stat() doesn't currently set
		 * these fields.  If nt_stat() was fixed then win32
		 * could include this mode too.
		 */
		fprintf(stderr, "%s: hardlink-mode not supported on Windows\n",
		    av[0]);
		return (1);
	}
#endif

#ifndef	SFIO_STANDALONE
	prog = opts->bp_tuple ? "BAM xfer" : "file xfer";
	if (opts->quiet) {
	} else if (opts->todo) {
		opts->tick = progress_start(PROGRESS_BAR, opts->todo);
	} else if (opts->nfiles) {
		opts->tick = progress_start(PROGRESS_BAR, opts->nfiles);
	}
#endif

	if (opts->mode == M_OUT) {
		rc = sfio_out();
	} else if (opts->mode == M_IN) {
		rc = (parallel > 0) ? sfio_in_Nway(parallel) : sfio_in(1);
	} else if (opts->mode == M_LIST) {
		rc = sfio_in(0);
	} else {
		goto usage;
	}

#ifndef	SFIO_STANDALONE
	if (opts->tick) progress_done(opts->tick, rc ? "FAILED" : "OK");
	if (opts->mark_no_dfiles) touch(NO_DFILE, 0666);
#endif
	optsFree();
	return (rc);

usage:
#ifndef SFIO_STANDALONE
	prog = 0;
#endif
	optsFree();
	usage();
}

private	void
optsFree(void)
{
	unless (opts) return;

	/*
	 * XXX: maybe leaking opts->tick as it can be freed but not
	 * cleared, and there's no interface to clear its internal
	 * memory such as tick->name, so choosing to leave it alone.
	 * Should only leak in the error case.
	 */
	if (opts->more) freeLines(opts->more, free);
	if (opts->sent) hash_free(opts->sent);
	if (opts->lastdir) free(opts->lastdir);
	free(opts);
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
#ifndef	SFIO_STANDALONE
	char	buf[MAXPATH];
	struct	stat sb;
	off_t	byte_count = 0;
	int	n;
	char	*gfile, *sfile, *t;
	hash	*links = 0;
	MDBM	*idDB = 0;
	char	ln[32];

	setmode(0, _O_TEXT); /* read file list in text mode */
	fputs(SFIO_VERS, stdout);

	if (opts->key2path) idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	if (opts->bp_tuple) opts->sent = hash_new(HASH_MEMHASH);
	if (opts->hardlinks) links = hash_new(HASH_MEMHASH);
	byte_count = 10;
	while (nextfile(buf)) {
		chomp(buf);
		if (opts->bp_tuple && strchr(buf, '|')) {
			if (n = out_bptuple(buf, &byte_count)) {
				send_eof(n);
				return (n);
			}
			continue;
		}
		if (opts->key2path) {
			unless (gfile = key2path(buf, idDB)) continue;
			sfile = name2sccs(gfile);
			strcpy(buf, sfile);
			free(sfile);
			free(gfile);
			if (lstat(buf, &sb)) continue;
		} else {
			if (lstat(buf, &sb)) {
				perror(buf);
				send_eof(SFIO_LSTAT);
				return (SFIO_LSTAT);
			}
			cleanPath(buf, buf);  /* avoid stuff like ./file */
		}
		byte_count += printf("%04d%s", (int)strlen(buf), buf);
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
		if (opts->lclone && !strneq(buf, "BitKeeper/log/", 14)) {
			/*
			 * In lclone-mode every file is a "hardlink" to
			 * the absolute path of the file in the original
			 * repository.  This sfio will only unpack on the
			 * same machine as it was created.
			 */
			t = fullname(buf, 0);
			n = out_hardlink(buf, &sb, &byte_count, t);
			free(t);
			if (n) {
				send_eof(n);
				return (n);
			}
			continue;
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
reg:			if (n = out_file(buf, &sb, &byte_count, 0, 0)) {
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
	save_byte_count(byte_count);
	if (opts->hardlinks) hash_free(links);
#endif
	return (0);
}

#ifndef	SFIO_STANDALONE

private int
out_symlink(char *file, struct stat *sp, off_t *byte_count)
{
	char	buf[MAXPATH];
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
	*byte_count += printf("SLNK%06u%s", n, buf);
	sum += adler32(sum, buf, n);
	*byte_count += printf("%010u", sum);
	assert(opts->doModes);
	*byte_count += printf("%03o", sp->st_mode & 0777);
	print_status(file, 0);
	return (0);
}

private int
out_hardlink(char *file, struct stat *sp, off_t *byte_count, char *linkMe)
{
	int	n;
	u32	sum = 0;


	n = strlen(linkMe);
	/*
	 * We have 10 chars into which we can encode a type.
	 * We know the pathname is <= 4 chars, so we encode the
	 * hardlink as "HLNK001024".
	 */
	*byte_count += printf("HLNK%06u%s", n, linkMe);
	sum += adler32(sum, linkMe, n);
	*byte_count += printf("%010u", sum);
	if (opts->doModes) *byte_count += printf("%03o", sp->st_mode & 0777);
	print_status(file, 0);
	return (0);
}

private int
out_file(char *file, struct stat *sp, off_t *byte_count, int useDsum, u32 dsum)
{
	char	buf[SFIO_BSIZ];
	int	fd = open(file, 0, 0);
	int	n, nread = 0;
	u32	sum = 0, sz = (u32)sp->st_size;

	if (fd == -1) {
		perror(file);
		return (SFIO_OPEN);
	}

	setmode(fd, _O_BINARY);
	*byte_count += printf("%010u", (unsigned int)sp->st_size);
	while ((n = readn(fd, buf, sizeof(buf))) > 0) {
		nread += n;
		opts->done += n;
		unless (useDsum) sum = adler32(sum, buf, n);
		if (fwrite(buf, 1, n, stdout) != n) {
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
	if (useDsum) sum = dsum;
	*byte_count += printf("%010u", sum);
	if (opts->doModes) *byte_count += printf("%03o", sp->st_mode & 0777);
	close(fd);
	print_status(file, sz);
	return (0);
}

/* adler32.md5sum rkey dkey */
private int
out_bptuple(char *keys, off_t *byte_count)
{
	char	*path, *fullpath, *p;
	int	n, sum;
	struct	stat sb;

	unless (fullpath = bp_lookupkeys(0, keys)) {
		if (opts->recurse) {
			opts->missing = addLine(opts->missing, strdup(keys));
			return (0);
		}
		fprintf(stderr, "lookupkeys(%s) failed\n", keys);
		return (SFIO_LOOKUP);
	}
	path = fullpath + strlen(proj_root(proj_product(0))) + 1;

	/* d.file */
	if (lstat(fullpath, &sb)) {
		perror(fullpath);
		return (SFIO_LSTAT);
	}
	*byte_count += printf("%04d%s", (int)strlen(keys), keys);
	unless (hash_insertStr(opts->sent, path, keys)) {
		n = out_hardlink(path, &sb, byte_count, opts->sent->vptr);
	} else {
		/*
		 * For BAM files we already know the expected checksum so
		 * we don't calculate it again.  If the file is corrupted it
		 * will be detected when being unpacked.
		 */
		if ((p = strrchr(path, '.')) && (p[1] == 'd') &&
		    !getenv("_BP_HASHCHARS")) {
			while (*p != '/') --p;
			sum = strtoul(p+1, 0, 16);
			n = out_file(fullpath, &sb, byte_count, 1, sum);
		} else {
			n = out_file(fullpath, &sb, byte_count, 0, 0);
		}
	}
	free(fullpath);
	return (n);
}

/*
 * Go fetch the missing BAM chunks.
 */
private void
missing(off_t *byte_count)
{
	char	*p, *tmpf;
	FILE	*f;
	int	i;
	char	buf[BUFSIZ];

	unless (bp_serverID(buf, 1)) {
err:		send_eof(SFIO_LOOKUP);
		return;
	}
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
	p = aprintf("bk -q@'%s' -Lr -Bstdin -zo0 sfio -qoB - < '%s'",
	    bp_serverURL(buf), tmpf);
	f = popen(p, "r");
	free(p);
	unless (f) goto err;
	send_eof(SFIO_MORE);
	while ((i = fread(buf, 1, sizeof(buf), f)) > 0) {
		*byte_count += fwrite(buf, 1, i, stdout);
	}
	pclose(f);	// If we error here we're just hosed.
	unlink(tmpf);
	free(tmpf);
	/* they should have sent EOF */
}
#endif	/* SFIO_STANDALONE */

/*
 * Send an eof by sending a 0 or less than 0 for the pathlen of the "next"
 * file.  Safe to do with old versions of sfio, they'll handle it properly.
 */
private void
send_eof(int error)
{
	/* negative lengths are exit codes */
	if (error > 0) error = -error;
	printf("%4d", error);
}

/*
 * sfio -i - produce a tree from an sfio on stdin
 * sfio -p - produce a listing of the files in the sfio and verify checksums
 *
 * Nota bene: changes to here should do the same change in sfio_in_Nway().
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
	if (fread(buf, 1, 10, stdin) != 10) {
		perror("fread");
		return (1);
	}
	if (strneq(buf, SFIO_NOMODE, 10)) {
		if (opts->doModes) {
			fprintf(stderr,
			    "sfio: modes requested but not sent.\n");
			return (1);
		}
	} else if (strneq(buf, SFIO_MODE, 10)) {
		opts->doModes = 1;
	} else {
		fprintf(stderr,
		    "Version mismatch [%s]<>[%s]\n", buf, SFIO_VERS);
		return (1);
	}
	for (;;) {
		n = fread(buf, 1, 4, stdin);
		if (n == 0) return (0);
		if (n != 4) {
			perror("fread");
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
			return (-len); /* we got a EOF */
		}
		if (len >= MAXPATH) {
			fprintf(stderr, "Bad length in sfio\n");
			return (1);
		}
		if (fread(buf, 1, len, stdin) != len) {
			perror("fread");
			return (1);
		}
		byte_count += len;
		buf[len] = 0;
		perfile(buf);
		if (fread(datalen, 1, 10, stdin) != 10) {
			perror("fread");
			return (1);
		}
		datalen[10] = 0;
		len = 0;
		if (opts->bp_tuple && strchr(buf, '|')) {
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
		if (opts->echo) printf("%s\n", buf);
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
	char	*p, *t;
	u32	sum = 0, sum2 = 0;
	char	file[MAXPATH];
	char	tmp[MAXPATH];

	if (strneq("HLNK00", datalen, 6)) {
		sscanf(&datalen[6], "%04d", &todo);
		/* keys linked to older keys we load into file */
		if (fread(file, 1, todo, stdin) != todo) return (1);
		file[todo] = 0;
		sum = adler32(0, file, todo);
		if (fread(tmp, 1, 10, stdin) != 10) {
			perror("chksum read");
			return (1);
		}
		tmp[10] = 0;
		sscanf(tmp, "%010u", &sum2);
		if (sum != sum2) {
			fprintf(stderr, "Checksum mismatch %u:%u for %s\n",
			    sum, sum2, keys);
			return (1);
		}
		if (opts->doModes) {
			if (fread(tmp, 1, 3, stdin) != 3) {
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
		p = strcpy(file, p); /* mdbm can't write pointers to itself */
	} else {

		sscanf(datalen, "%010d", &todo);

		bp_dataroot(0, file);
		t = file + strlen(file);

		/* extract to new file in BAM pool (adler32 is first in keys) */
		p = t + sprintf(t, "/%c%c/%.*s",
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
		p = t + 1; /* p points to %c%c/%s.%d in BAM/[<ROOTKEY>/] */
	}
	mdbm_store_str(proj_BAMindex(0, 1), keys, p, MDBM_REPLACE);
	bp_logUpdate(0, keys, p);
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
	if (fread(buf, 1, pathlen, stdin) != pathlen) return (1);
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
	if (fread(buf, 1, 10, stdin) != 10) {
		perror("chksum read");
		goto err;
	}
	buf[10] = 0;
	sscanf(buf, "%010u", &sum2);
	if (sum != sum2) {
		fprintf(stderr,
		    "Checksum mismatch %u:%u for %s\n", sum, sum2, file);
		goto err;
	}
	if (fread(buf, 1, 3, stdin) != 3) {
		perror("mode read");
		goto err;
	}
	buf[3] = 0;
	sscanf(buf, "%03o", &imode);
	mode = imode;
	if (extract) {
		/*
		 * This is trying to chmod what is pointed to, which
		 * is not what we want.
		chmod(file, mode & 0777);
		 */
	}
	print_status(file, 0);
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
	if (fread(buf, 1, pathlen, stdin) != pathlen) return (1);
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
	if (fread(buf, 1, 10, stdin) != 10) {
		perror("chksum read");
		goto err;
	}
	buf[10] = 0;
	sscanf(buf, "%010u", &sum2);
	if (sum != sum2) {
		fprintf(stderr,
		    "Checksum mismatch %u:%u for %s\n", sum, sum2, file);
		goto err;
	}
	if (opts->doModes) {
		if (fread(buf, 1, 3, stdin) != 3) {
			perror("mode read");
			goto err;
		}
#ifdef	WIN32
		{		/* mode not with inode on windows */
			u32	imode;

			buf[3] = 0;
			sscanf(buf, "%03o", &imode);
			chmod(file, imode & 0777);
		}
	} else {
		chmod(file, 0444);
#endif
	}
	print_status(file, 0);
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
	while ((n = fread(buf, 1, min(todo, sizeof(buf)), stdin)) > 0) {
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
done:	if (fread(buf, 1, 10, stdin) != 10) {
		perror("chksum read");
		goto err;
	}
	buf[10] = 0;
	sscanf(buf, "%010u", &sum2);
	if ((sum != sum2) && !getenv("_BK_ALLOW_BAD_CRC")) {
		fprintf(stderr,
		    "Checksum mismatch %u:%u for %s\n", sum, sum2, file);
		goto err;
	}
	if (opts->doModes) {
		unsigned int imode;  /* mode_t might be a short */
		if (fread(buf, 1, 3, stdin) != 3) {
			perror("mode read");
			goto err;
		}
		buf[3] = 0;
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
	print_status(file, sz);
	return (0);

err:
	if (extract) {
		close(fd);
		unlink(file);
	}
	return (1);
}

private void
print_status(char *file, u32 sz)
{
	static	int n;
	char	line[MAXPATH];

	if (opts->quiet) return;

#ifdef	SFIO_STANDALONE
	/* just do a simple print in standalone mode */
	fputs(file, stderr);
	fputc('\n', stderr);
#else
	if (opts->todo) {
// ttyprintf("todo=%llu done=%llu max=%llu\n", opts->todo, opts->done, opts->tick->max);
		progress(opts->tick, opts->done);
		return;
	} else if (opts->nfiles) {
		progress(opts->tick, ++n);
		return;
	}

	if (opts->sfile2gfile && (sccs_filetype(file) == 's')) {
		file = sccs2name(file);
		sprintf(line, "%s%s", opts->prefix, file);
		free(file);
	} else {
		sprintf(line, "%s%s", opts->prefix, file);
	}
	fprintf(stderr, "%s\n", line);
#endif
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

private int
sfio_in_Nway(int n)
{
	int	i, nticks = 0, rc = 1;
	int	cur = 0;
	FILE	**f;
	int	*sent;
	int	len;
	off_t	byte_count = 0;
	char	*cmd;
	char	datalen[11];
	char	buf[MAXPATH];
	char	data[BUFSIZ];

	f = calloc(n, sizeof(FILE *));
	sent = calloc(n, sizeof(int));
	cmd = aprintf("bk sfio -i%s",
		(opts->quiet || opts->todo || opts->nfiles)? "q" : "");
	for (i = 0; i < n; i++) {
		f[i] = popen(cmd, "w");
	}
	free(cmd);

header:
	bzero(buf, sizeof(buf));
	if (fread(buf, 1, 10, stdin) != 10) {
		perror("fread");
		goto out;
	}
	if (strneq(buf, SFIO_NOMODE, 10)) {
		if (opts->doModes) {
			fprintf(stderr,
			    "sfio: modes requested but not sent.\n");
			goto out;
		}
	} else if (strneq(buf, SFIO_MODE, 10)) {
		opts->doModes = 1;
	} else {
		fprintf(stderr,
		    "Version mismatch [%s]<>[%s]\n", buf, SFIO_VERS);
		goto out;
	}
	for (i = 0; i < n; i++) {
		sent[i] += fwrite(buf, 1, 10, f[i]);
	}
	for (;;) {
		i = fread(buf, 1, 4, stdin);
		if (i== 0) {
			rc = 0;
			goto out;
		}
		if (i != 4) {
			perror("fread");
			goto out;
		}
		byte_count += i;
		buf[4] = 0;
		len = 0;
		sscanf(buf, "%04d", &len);
		if (len <= 0) {
			if (len < 0) {
				if (-len == SFIO_MORE) {
					for (i = 0; i < n; i++) {
						fwrite(buf, 1, 4, f[i]);
					}
					goto header;
				}
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
			strcpy(buf, "0000");
			for (i = 0; i < n; i++) {
				fwrite(buf, 1, 4, f[i]);
			}
			rc = -len;
			goto out;
		}
		if (len >= MAXPATH) {
			fprintf(stderr, "Bad length in sfio\n");
			goto out;
		}
		sent[cur] += fwrite(buf, 1, 4, f[cur]);
		if (fread(buf, 1, len, stdin) != len) {
			perror("fread");
			goto out;
		}
		byte_count += len;
		buf[len] = 0;
		sent[cur] += fwrite(buf, 1, len, f[cur]);
		perfile(buf);
		if (fread(datalen, 1, 10, stdin) != 10) {
			perror("fread");
			goto out;
		}
		datalen[10] = 0;
		sent[cur] += fwrite(datalen, 1, 10, f[cur]);
		if (strneq("LNK00", datalen + 1, 5)) {
			/* match HLNK00 && SLNK00 */
			len = strtoul(datalen + 6, 0, 10);
		} else {
			len = strtoul(datalen, 0, 10);
		}
		len += 10;	/* checksum */
		if (opts->doModes) len += 3;
                /*
                 * Stdio will bypass buffering if the data to be
                 * written after the current buffer is empty is >= the
                 * size of a buffer.  We make that happen more often
                 * by flushing when a big file is about to be
                 * transferred.
                 */
		if (len >= 2*sizeof(data)) fflush(f[cur]);
		while (len > 0) {
			i = fread(data, 1, min(len, sizeof(data)), stdin);
			if (i <= 0) {
				perror("fread");
				goto out;
			}
			sent[cur] += fwrite(data, 1, i, f[cur]);
			len -= i;
		}
#ifndef SFIO_STANDALONE
		if (opts->todo) {
			progress(opts->tick, opts->done);
		} else if (opts->nfiles) {
			progress(opts->tick, ++nticks);
		}
#endif
		len = 0;
		/* select next subprocess to use */
		for (i = 0; i < n; i++) {
			if (sent[i] < sent[cur]) cur = i;
		}
		if (opts->echo) printf("%s\n", buf);
	}
#ifndef SFIO_STANDALONE
	save_byte_count(byte_count);
#endif
out:
	for (i = 0; i < n; i++) {
		if (cur = pclose(f[i])) {
			rc = WIFEXITED(cur) ? WEXITSTATUS(cur) : 17;
			fprintf(stderr, "process %d exited %d\n", i, rc);
		}
	}
	free(f);
	free(sent);
	return (rc);
}

private void
where(char *path)
{
	char	*dir = dirname_alloc(path);

	if (!opts->lastdir || !streq(opts->lastdir, dir)) {
		if (opts->lastdir) free(opts->lastdir);
		opts->lastdir = dir;
		printf("%s/\n", dir);
	} else {
		free(dir);
	}
	/* might be better to print size from last entry instead */
	printf("%s|%lx\n", basenm(path), ftell(stdin));
}

/*
 * Called as each filename is seen reading a sfio.
 */
private void
perfile(char *file)
{
	if (opts->mark_no_dfiles &&
	    (strneq(file, "SCCS/d.", 7) || strstr(file, "/SCCS/d."))) {
		opts->mark_no_dfiles = 0;
	}
	if (opts->index) where(file);
}

