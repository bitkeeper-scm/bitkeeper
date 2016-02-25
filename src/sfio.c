/*
 * Copyright 1999-2016 BitMover, Inc
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
#include "sccs.h"
#include "bam.h"
#include "progress.h"
#include "sfio.h"

private	void	optsFree(void);
private	int	sfio_out(void);
private int	out_file(char *file, struct stat *, off_t *byte_count,
		    int useDsum, u32 dsum);
private int	out_xfile(char *file, int type, off_t *byte_count);
private int	out_bptuple(char *tuple, off_t *byte_count);
private int	out_symlink(char *file, struct stat *, off_t *byte_count);
private int	out_hardlink(char *file,
		    struct stat *, off_t *byte_count, char *linkMe);
private	int	sfio_in(int extract, int justone);
private int	in_bptuple(char *file, char *datalen, int extract);
private int	in_symlink(char *file, int todo, int extract);
private int	in_hardlink(char *file, int todo, int extract);
private int	in_xfile(char *file, int type, u32 todo, int extract);
private int	in_file(char *file, u32 todo, int extract);
private	int	mkfile(char *file);
private	void	send_eof(int status);
private void	missing(off_t *byte_count);
private	void	print_status(char *file, u32 sz);
private int	sfio_in_Nway(int n);
private	void	perfile(char *path);

private	struct {
	u32	quiet:1;	/* suppress normal verbose output */
	u32	verbose:1;	/* clone -v set this */
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
	u32	checkout:1;	/* --checkout: do sccs checkouts in parallel */
	u32	seen_config:1;	/* Have we unpacked BitKeeper/etc/config yet? */
	u32	takepatch:1;	/* called from takepatch */
	u32	clone:1;	/* --clone: use 'bk _sfiles_clone' */
	u32	parents:1;	/* --parents */
	u32	doubleKeys:1;	/* bam recurse is sending double len keys */
	u32	raw:1;		/* operate on raw files without magic */
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
	u32	compat;		/* send sfile in compat form */
	ticker	*tick;		/* progress bar */
	hash	*dirs;		/* mkdir() list */
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
		{ "checkout", 310 },
		{ "clone", 315 },
		{ "mark-no-dfiles", 320 },
		{ "Nway", 330 },
		{ "parents", 335 },
		{ "raw", 337 },		// operate on raw files without magic
		{ "takepatch", 340 },
		{ 0, 0 }
	};

	opts = (void*)calloc(1, sizeof(*opts));
	opts->recurse = 1;
	opts->prefix = "";
	setmode(0, O_BINARY);
	while ((c = getopt(ac, av, "2a;A;b;BCefgHIij;KLlmN;opP;qrv", lopts)) != -1) {
		switch (c) {
		    case '2': break;	/* eat arg used by sfiles_clone */
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
		    case 'C': opts->compat = 1; break;		/* undoc */
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
		    case 'v': opts->verbose = 1; break;
		    case 310:	/* --checkout */
		    	opts->checkout = 1;
			break;
		    case 315:	/* --clone */
			opts->clone = 1;
			break;
		    case 320:	/* --mark-no-dfiles */
			opts->mark_no_dfiles = 1;
			break;
		    case 330:	/* --Nway */
			opts->seen_config = 1; /* don't wait for config file */
			break;
		    case 335:	/* --parents */
			opts->parents = 1;
			break;
		    case 337:	/* --raw */
			opts->raw = 1;
			break;
		    case 340:	/* --takepatch */
			opts->takepatch = 1;
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

	prog = (opts->bp_tuple && !opts->takepatch) ? "BAM xfer" : "file xfer";
	if (opts->quiet) {
	} else if (opts->todo) {
		opts->tick = progress_start(PROGRESS_BAR, opts->todo);
	} else unless (opts->verbose) {
		opts->tick = progress_start(PROGRESS_BAR, opts->nfiles);
	}
	if (opts->mode == M_OUT) {
		rc = sfio_out();
	} else if (opts->mode == M_IN) {
		if (parallel > 1) {
			rc = sfio_in(1, 1); /* unpack BitKeeper/etc */
			if (rc == SFIO_MORE) {
				rc = sfio_in_Nway(parallel); /* do the rest in parallel */
			}
		} else {
			rc = sfio_in(1, 0);
		}
	} else if (opts->mode == M_LIST) {
		rc = sfio_in(0, 0);
	} else {
		goto usage;
	}

	if (opts->tick) progress_done(opts->tick, rc ? "FAILED" : "OK");
	if (opts->mark_no_dfiles) touch(NO_DFILE, 0666);
	optsFree();
	return (rc);

usage:
	prog = "sfio";
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
	if (opts->dirs) hash_free(opts->dirs);
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
	opts->key2path = 0;
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
	struct	stat sb;
	off_t	byte_count = 0;
	int	n;
	pid_t	clone_pid = 0;
	int	status;
	char	*gfile, *sfile, *t;
	hash	*links = 0;
	MDBM	*idDB = 0;
	MDBM	*goneDB = 0;
	int	fout;
	int	xfile;
	char	**av = 0;
	char	ln[32];

	if (opts->clone) {
		av = addLine(av, "bk");
		av = addLine(av, "_sfiles_clone");
		if (opts->lclone) {
			av = addLine(av, "-L");
		} else {
			av = addLine(av, "--cold");
		}
		if (opts->doModes) av = addLine(av, "-m2");
		if (opts->parents) av = addLine(av, "-p");
		av = addLine(av, 0);
		clone_pid = spawnvpio(0, &fout, 0, av+1);
		dup2(fout, 0);
		close(fout);
		freeLines(av, 0);
	}

	setmode(0, _O_TEXT); /* read file list in text mode */
	fputs(SFIO_VERS(opts->doModes), stdout);

	if (opts->key2path) {
		idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
		goneDB = loadDB(GONE, 0, DB_GONE);
	}
	if (opts->bp_tuple) opts->sent = hash_new(HASH_MEMHASH);
	if (opts->hardlinks) links = hash_new(HASH_MEMHASH);
	byte_count = 10;
	while (nextfile(buf)) {
		chomp(buf);
		if (opts->compat && 
		    (streq(buf, CHANGESET_H1) || streq(buf, CHANGESET_H2))) {
			continue;
		}
		if (streq(buf, "||")) {
			/*
			 * Insert a sfio separator and skip this file.
			 */
			send_eof(SFIO_MORE);
			fputs(SFIO_VERS(opts->doModes), stdout);
			continue;
		}
		if (opts->bp_tuple && strchr(buf, '|')) {
			if (n = out_bptuple(buf, &byte_count)) {
				send_eof(n);
				return (n);
			}
			continue;
		}
		xfile = opts->raw ? 0 : is_xfile(buf);
		if (opts->key2path) {
			unless (gfile = key2path(buf, idDB, goneDB, 0)) {
				continue;
			}
			sfile = name2sccs(gfile);
			strcpy(buf, sfile);
			free(sfile);
			free(gfile);
			if (lstat(buf, &sb)) continue;
			if (!opts->compat && streq(buf, CHANGESET)) {
				if (exists(CHANGESET_H1)) {
					opts->more = addLine(opts->more,
					    strdup(CHANGESET_H1));
				}
				if (exists(CHANGESET_H2)) {
					opts->more = addLine(opts->more,
					    strdup(CHANGESET_H2));
				}
			}
		} else {
			if (!xfile && lstat(buf, &sb)) {
				perror(buf);
				send_eof(SFIO_LSTAT);
				return (SFIO_LSTAT);
			}
			cleanPath(buf, buf);  /* avoid stuff like ./file */
		}
		byte_count += printf("%04d%s", (int)strlen(buf), buf);
		if (opts->hardlinks && !xfile) {
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
		if (opts->lclone && !xfile &&
		    !strneq(buf, "BitKeeper/log/", 14)) {
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
		if (xfile) {
			if (n = out_xfile(buf, xfile, &byte_count)) {
				send_eof(n);
				return (n);
			}
		} else if (S_ISLNK(sb.st_mode)) {
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
	} else if (opts->clone) {
		waitpid(clone_pid, &status, 0);
		close(0);
		send_eof(status ? SFIO_SFILES : 0);
	} else {
		send_eof(0);
	}
	save_byte_count(byte_count);
	if (opts->hardlinks) hash_free(links);
	mdbm_close(idDB);
	mdbm_close(goneDB);
	return (0);
}

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
out_xfile(char *file, int type, off_t *byte_count)
{
	u32	sum, sz;
	char	*data;

	data = xfile_fetch(file, type);
	unless (data) {
		perror(file);
		return (SFIO_OPEN);
	}
	sz = strlen(data);
	*byte_count += printf("%010u", sz);
	if (fwrite(data, 1, sz, stdout) != sz) {
		if ((errno != EPIPE) || getenv("BK_SHOWPROC")) {
			perror(file);
		}
		free(data);
		return (1);
	}
	sum = adler32(0, data, sz);
	free(data);
	*byte_count += sz;
	*byte_count += printf("%010u", sum);
	if (opts->doModes) *byte_count += printf("%03o", 0664);
	print_status(file, sz);
	return (0);
}

private int
out_file(char *file, struct stat *sp, off_t *byte_count, int useDsum, u32 dsum)
{
	char	buf[SFIO_BSIZ];
	int	fd;
	int	n, nread = 0;
	u32	sum = 0, sz = (u32)sp->st_size;

	if (opts->compat && (sccs_filetype(file) == 's')) {
		size_t	len;
		sccs	*s;
		char	*data;

		assert(!useDsum);
		unless (s = sccs_init(file, INIT_MUSTEXIST)) {
			perror(file);
			return (SFIO_OPEN);
		}
		data = sccs_scat(s, SCAT_SCCS, &len);
		sz = (u32)len;
		*byte_count += printf("%010u", sz);
		sum = adler32(sum, data, sz);
		if (fwrite(data, 1, len, stdout) != len) {
			free(data);
			if ((errno != EPIPE) || getenv("BK_SHOWPROC")) {
				perror(file);
			}
			sccs_free(s);
			return (1);
		}
		free(data);
		*byte_count += len;
		sccs_free(s);
	} else {
		if ((fd = open(file, 0, 0)) == -1) {
			perror(file);
			return (SFIO_OPEN);
		}
		*byte_count += printf("%010u", sz);
		setmode(fd, _O_BINARY);
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
		if (nread != sz) {
			fprintf(stderr, "Size mismatch on %s %u:%u\n\n", 
			    file, nread, (unsigned int)sp->st_size);
			close(fd);
			return (SFIO_SIZE);
		}
		close(fd);
	}
	if (useDsum) sum = dsum;
	*byte_count += printf("%010u", sum);
	if (opts->doModes) *byte_count += printf("%03o", sp->st_mode & 0777);
	print_status(file, sz);
	return (0);
}

/* adler32.md5sum rkey dkey */
private int
out_bptuple(char *keys, off_t *byte_count)
{
	char	*path, *fullpath, *p, *newkeys = keys;
	int	n, sum;
	struct	stat sb;

	/*
	 * see if keys has 2 sets of keys, to cover copying
	 * :BAMHASH: <old-dkey> <old-md5root> :BAMHASH: <new-dkey> <new-md5root>
	 */
	if ((p = strchr(keys, ' ')) &&
	    (p = separator(p+1)) && (p = strchr(p+1, ' '))) {
		*p = 0;
		newkeys = p + 1;
	}
	unless (fullpath = bp_lookupkeys(0, keys)) {
		if (opts->recurse) {
			if (p) {
				opts->doubleKeys = 1;
				*p = ' ';
			}
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
	*byte_count += printf("%04d%s", (int)strlen(newkeys), newkeys);
	unless (hash_insertStr(opts->sent, path, newkeys)) {
		n = out_hardlink(path, &sb, byte_count, opts->sent->vptr);
	} else {
		/*
		 * For BAM files we already know the expected checksum so
		 * we don't calculate it again.  If the file is corrupted it
		 * will be detected when being unpacked.
		 */
		if (opts->lclone) {
			/* Wacky options, but -H is for internal linking */
			n = out_hardlink(path, &sb, byte_count, fullpath);
		} else if ((p = strrchr(path, '.')) && (p[1] == 'd') &&
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
	if (opts->doubleKeys) {
		/* bk mv BAMsrc BAMdest - bring remote files to BAMsrc */
		p = aprintf("bk -q@'%s' -Lr -Bstdin -zo0 sfio -qoB -" 
		    " | bk sfio -qiB -",
		    bp_serverURL(buf));
		f = popen(p, "w");
		free(p);
		unless (f) goto err;
		EACH(opts->missing) {
			/* Just the lower part of the key; could be old bk */
			if ((p = strchr(opts->missing[i], ' ')) &&
			    (p = separator(p+1)) && (p = strchr(p+1, ' '))) {
				*p = 0;
			}
			fprintf(f, "%s\n", opts->missing[i]);
			if (p) *p = ' ';
		}
		if (pclose(f)) goto err;
	}
	tmpf = bktmp(0);
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
	if (opts->doubleKeys) {
		/* files are local, so just do the local copy */
		p = aprintf("bk sfio -qoB%s - < '%s'",
		    opts->lclone ?  "L" : "", tmpf);
	} else {
		p = aprintf("bk -q@'%s' -Lr -Bstdin -zo0 sfio -qoB - < '%s'",
		    bp_serverURL(buf), tmpf);
	}
	f = popen(p, "r");
	free(p);
	unless (f) goto err;
	send_eof(SFIO_MORE);
	while ((i = fread(buf, 1, sizeof(buf), f)) > 0) {
		*byte_count += fwrite(buf, 1, i, stdout);
	}
	i = pclose(f);	// If we error here we're just hosed.
	unlink(tmpf);
	free(tmpf);
	if (i) goto err;
	/* they should have sent EOF */
}

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
sfio_in(int extract, int justone)
{
	int	len, n, i;
	int	fail = 0, nfiles = 0;
	u32	ulen;
	off_t	byte_count = 0;
	FILE	*co = 0;
	char	**save = 0;
	char	*p;
	int	xfile;
	char	buf[MAXPATH];
	char	datalen[11];

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
		    "Version mismatch [%s]<>[%s]\n",
		    buf, SFIO_VERS(opts->doModes));
		return (1);
	}
	if (p = getenv("_BK_SFIO_FAIL")) fail = atoi(p);
	for (;;) {
		n = fread(buf, 1, 4, stdin);
		if (n == 0) {
			len = 0;
			goto eof;
		}
		if (n != 4) {
			perror("fread");
			return (1);
		}
		byte_count += n;
		buf[4] = 0;
		len = 0;
		sscanf(buf, "%04d", &len);
		if (len <= 0) {
eof:			if (co) {
				assert(!save);
				if (pclose(co)) {
					fprintf(stderr,
					    "checkout: checkouts failed\n");
					return (1);
				}
			}
			if (len < 0) {
				if (-len == SFIO_MORE) {
					if (justone) return (SFIO_MORE);
					return (sfio_in(extract, 0));
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
				    case SFIO_SFILES:
					fprintf(stderr,
					    "sfiles returned errror\n");
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
		xfile = opts->raw ? 0 : is_xfile(buf);
		if (fread(datalen, 1, 10, stdin) != 10) {
			perror("fread");
			return (1);
		}
		if (++nfiles == fail) {
			perror("dieing for regressions");
			exit(1);
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
		} else if (xfile) {
			sscanf(datalen, "%010u", &ulen);
			if (in_xfile(buf, xfile, ulen, extract)) return (1);
		} else {
			sscanf(datalen, "%010u", &ulen);
			if (in_file(buf, ulen, extract)) return (1);
		}
		if (extract && opts->checkout) {
			if ((!strneq("BitKeeper/", buf, 10) ||
				strneq("BitKeeper/triggers/", buf,  19)) &&
			    !streq(CHANGESET, buf)) {
				/* Skip the d.files and other crud */
			    	p = strrchr(buf, '/');
				assert(p);
				unless ((p[1] == 's') && (p[2] == '.')) {
					continue;
				}
dump:				if (opts->seen_config && !co) {
					/*
					 * defer spawning checkout
					 * until the first user file
					 * in checkout mode
					 */
					if (proj_checkout(0) & (CO_GET|CO_EDIT)){
						co = popen(
			     "bk checkout -Tq --skip-bam -", "w");
						assert(co);
						setlinebuf(co);
						EACH(save) {
							fprintf(co,
							    "%s\n", save[i]);
						}
					} else {
						opts->checkout = 0;
					}
					freeLines(save, free);
					save = 0;
				}
				/*
				 * Add code to buffer up file names
				 * until BitKeeper/etc/config has been
				 * seen so that we will get the
				 * correct checkout modes This is only
				 * needed when cloning from an old
				 * bkd.
				 */
				if (co) {
					fprintf(co, "%s\n", buf);
				} else if (!opts->seen_config) {
					save = addLine(save, strdup(buf));
				}
			} else if (streq("BitKeeper/etc/SCCS/s.config", buf)) {
				opts->seen_config = 1;
				// In case config is the last file, flush
				if (save && (p = popLine(save))) {
					strcpy(buf, p);
					goto dump;
				}
			}
		}
		if (opts->echo) printf("%s\n", buf);
	}
	assert(0);
}

private int
in_bptuple(char *keys, char *datalen, int extract)
{
	int	todo, i, j;
	char	*p, *t;
	u32	sum = 0, sum2 = 0;
	char	file[MAXPATH];
	int	hardlink = 0;
	char	tmp[MAXPATH];

	if (strneq("HLNK00", datalen, 6)) {
		hardlink = 1;
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
		unless (strchr(file, '|')) {
			strcpy(tmp, file);
			goto addfile;
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
addfile:
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
		if (hardlink) {
			if (fileLink(tmp, file)) return (1);
			todo = size(file);
		} else {
			if (in_file(file, todo, extract)) return (1);
		}
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
}

private int
in_symlink(char *file, int pathlen, int extract)
{
	char	buf[MAXPATH];
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
		if (fileLink(buf, file)) {
			perror(file);
			return (1);
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
in_xfile(char *file, int type, u32 todo, int extract)
{
	char	*data;
	int	rc = 1;
	u32	sum, sum2;
	char	*dir;
	char	buf[MAXLINE];

	data = malloc(todo+1);
	if (todo && (fread(data, 1, todo, stdin) != todo)) {
		fprintf(stderr, "Premature EOF of %s\n", file);
		goto err;
		free(data);
		return (1);
	}
	sum = adler32(0, data, todo);
	data[todo] = 0;

	if (fread(buf, 1, 10, stdin) != 10) {
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
		if (fread(buf, 1, 3, stdin) != 3) {
			perror("mode read");
			goto err;
		}
	}
	if (extract) {
		strcpy(buf, file);
		dir = dirname(buf); /* strip x.file */
		dir = dirname(dir); /* strip SCCS */
		unless (opts->dirs) opts->dirs = hash_new(HASH_MEMHASH);
		if (hash_insertStrSet(opts->dirs, dir)) mkdirp(dir);
		if (xfile_store(file, type, data)) {
			perror(file);
			goto err;
		}
	}
	rc = 0;
err:	free(data);
	return (rc);
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

	if (opts->todo) {
// ttyprintf("todo=%llu done=%llu max=%llu\n", opts->todo, opts->done, opts->tick->max);
		progress(opts->tick, opts->done);
		return;
	} else if (opts->tick) {
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
}

int
mkfile(char *file)
{
	int	fd;
	char	*dir;
	char	buf[MAXPATH];

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
	strcpy(buf, file);
	dir = dirname(buf);
	unless (opts->dirs) opts->dirs = hash_new(HASH_MEMHASH);
	if (hash_insertStrSet(opts->dirs, dir)) {
		if (mkdirp(dir) && (errno != EEXIST)) {
			fputs("\n", stderr);
			perror(file);
			if (errno == EINVAL) goto bad_name;
		}
	}
	fd = open(file, O_CREAT|O_EXCL|O_WRONLY, 0666);
	if (fd >= 0) {
		setmode(fd, _O_BINARY);
		return (fd);
	}
	if (errno == EINVAL) goto bad_name;
	if (errno == EEXIST) {
		char	realname[MAXPATH];

		getRealName(file, NULL, realname);
		unless (streq(file, realname)) {
			getMsg2("case_conflict", file, realname, '=', stderr);
			errno = EINVAL;
		} else {
			errno = EEXIST;	/* restore errno */
		}
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
	cmd = aprintf("bk sfio -i%s%s --Nway %s",
	    (opts->quiet || opts->todo || opts->tick)? "q" : "",
	    opts->verbose ? "v" : "",
	    opts->checkout ? "--checkout" : "");
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
		    "Version mismatch [%s]<>[%s]\n",
		    buf, SFIO_VERS(opts->doModes));
		goto out;
	}
	for (i = 0; i < n; i++) {
		fwrite(buf, 1, 10, f[i]);
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
				    case SFIO_SFILES:
					fprintf(stderr,
					    "sfiles returned errror\n");
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
		fwrite(buf, 1, 4, f[cur]);
		if (fread(buf, 1, len, stdin) != len) {
			perror("fread");
			goto out;
		}
		byte_count += len;
		buf[len] = 0;
		fwrite(buf, 1, len, f[cur]);
		perfile(buf);
		if (fread(datalen, 1, 10, stdin) != 10) {
			perror("fread");
			goto out;
		}
		datalen[10] = 0;
		fwrite(datalen, 1, 10, f[cur]);
		if (strneq("LNK00", datalen + 1, 5)) {
			/* match HLNK00 && SLNK00 */
			len = strtoul(datalen + 6, 0, 10);
		} else {
			len = strtoul(datalen, 0, 10);
		}
		len += 10;	/* checksum */
		if (opts->doModes) len += 3;
		/*
		 * Let's run sent[] in units of milliseconds.
		 *
		 * Let's assume that we're going burn 10 milliseconds/create.
		 * We do one create for the file, if in edit, another for the
		 * p.file, so we are at 20 milliseconds/file with no data.
		 *
		 * Let's assume that we get 25MB/sec over the network.  That's
		 * roughly 25K/millisecond.
		 */
		sent[cur] += 20;
		sent[cur] += len / (25<<10);
		while (len > 0) {
			i = fread(data, 1, min(len, sizeof(data)), stdin);
			if (i <= 0) {
				perror("fread");
				goto out;
			}
			fwrite(data, 1, i, f[cur]);
			len -= i;
		}
		if (opts->todo) {
			progress(opts->tick, opts->done);
		} else if (opts->tick) {
			progress(opts->tick, ++nticks);
		}
		fflush(f[cur]);
		len = 0;
		/* select next subprocess to use */
		for (i = 0; i < n; i++) {
			if (sent[i] < sent[cur]) cur = i;
		}
		if (opts->echo) printf("%s\n", buf);
	}
	save_byte_count(byte_count);
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

