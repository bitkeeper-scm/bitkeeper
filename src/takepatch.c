/* Copyright (c) 1999-2000 L.W.McVoy */
#include "sccs.h"
#include "logging.h"
#include "bam.h"
#include "nested.h"
#include <time.h>
#include "range.h"

/*
 * takepatch - apply a BitKeeper patch file
 *
 * XXX - should take an option directory name or read the environment for
 * a directory.
 *
 * Patch format:
 *
 * == filename ==
 * key_of_root_delta
 * key_of_parent_delta
 *
 * D 1.159 99/02/06 16:57:48 lm 160/55/6481
 * ....
 *
 * diffs
 *
 * Repeats.
 */
#define	CLEAN_RESYNC	1	/* blow away the RESYNC dir */
#define	CLEAN_PENDING	2	/* blow away the PENDING dir */
#define	CLEAN_OK	4	/* but exit 0 anyway */
#define	SHOUT() fputs("\n=================================== "\
		    "ERROR ====================================\n", stderr);
#define	SHOUT2() fputs("======================================="\
		    "=======================================\n", stderr);
#define	NOTICE() fputs("------------------------------------"\
		    "---------------------------------------\n", stderr);

private	delta	*getRecord(MMAP *f);
private void	errorMsg(char *msg, char *arg1, char *arg2);
private	int	extractPatch(char *name, MMAP *p);
private	int	extractDelta(char *name, sccs *s, int newFile, MMAP *f, int*);
private	int	applyPatch(char *local, sccs *perfile);
private	int	applyCsetPatch(sccs *s, int *nfound, sccs *perfile);
private	int	getLocals(sccs *s, delta *d, char *name);
private	void	insertPatch(patch *p);
private	void	reversePatch(void);
private	void	initProject(void);
private	MMAP	*init(char *file);
private	void	cleanup(int what);
private	void	freePatchList(void);
private	void	fileCopy2(char *from, char *to);
private	void	badpath(sccs *s, delta *tot);
private	int	skipPatch(MMAP *p);
private	void	getChangeSet(void);
private	void	loadskips(void);
private int	sfio(MMAP *m);

private	int	echo = 0;	/* verbose level, more means more diagnostics */
private	int	mkpatch = 0;	/* act like makepatch verbose */
private	int	line;		/* line number in the patch file */
private	int	fileNum;	/* counter for the Nth init/diff file */
private	patch	*patchList = 0;	/* patches for a file, list len == fileNum */
private	int	conflicts;	/* number of conflicts over all files */
private	int	newProject;	/* command line opt to create a new repo */
private	int	saveDirs;	/* save directories even if errors */
private	MDBM	*idDB;		/* key to pathname db, set by init or rebuilt */
private	MDBM	*goneDB;	/* key to gone database */
private	delta	*tableGCA;	/* predecessor to the oldest delta found
				 * in the patch */
private	int	noConflicts;	/* if set, abort on conflicts */
private	char	pendingFile[MAXPATH];
private	char	*input;		/* input file name,
				 * either "-" or a patch file */
private	int	encoding;	/* encoding before we started */
private	char	*spin = "|/-\\";
private	char	*comments;	/* -y'comments', pass to resolve. */
private	char	**errfiles;	/* files had errors during apply */
private	char	**edited;	/* files that were in modified state */
private	int	collapsedups;	/* allow csets in BitKeeper/etc/collapsed */
private	int	Fast;		/* Fast patch mode */

/*
 * Structure for keys we skip when incoming, used for old LOD keys that
 * we do not want in the open logging tree.
 */
typedef	struct s {
	char	*rkey;		/* file inode */
	char	**dkeys;	/* lines array of delta keys */
	struct	s *next;	/* next file */
} skips;
private	skips	*skiplist;

int
takepatch_main(int ac, char **av)
{
	FILE 	*f;
	char	*buf;
	MMAP	*p;
	int	c;
	int	files = 0;
	char	*t;
	int	error = 0;
	int	remote = 0;	/* remote csets */
	int	resolve = 0;
	int	textOnly = 0;

	setmode(0, O_BINARY); /* for win32 */
	input = "-";
	while ((c = getopt(ac, av, "acDFf:iLmqsStTvy;")) != -1) {
		switch (c) {
		    case 'q':					/* undoc 2.0 */
		    case 's':					/* undoc 2.0 */
			/* undoc, Ignored for option consistency.  */
			break;
		    case 'a': resolve++; break;			/* doc 2.0 */
		    case 'c': noConflicts++; break;		/* doc 2.0 */
		    case 'D': collapsedups++; break;
		    case 'F': break;				/* obsolete */
		    case 'f':					/* doc 2.0 */
			    input = optarg;
			    break;
		    case 'i': newProject++; break;		/* doc 2.0 */
		    case 'm': mkpatch++; break;			/* doc 2.0 */
		    case 'S': saveDirs++; break;		/* doc 2.0 */
		    case 'T': /* -T is preferred, remove -t in 5.0 */
		    case 't': textOnly++; break;		/* doc 2.0 */
		    case 'v': echo++; break;			/* doc 2.0 */
		    case 'y': comments = optarg; break;
		    default: goto usage;
		}
	}
	if (getenv("TAKEPATCH_SAVEDIRS")) saveDirs++;
	if ((t = getenv("BK_NOTTY")) && *t && (echo == 3)) echo = 2;
	if (av[optind]) {
usage:		system("bk help -s takepatch");
		return (1);
	}
	p = init(input);
	if (newProject) putenv("_BK_NEWPROJECT=YES");
	if (sane(0, 0)) exit(1);

	if (newProject) {
		unless (idDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) {
			perror("mdbm_open");
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
	} else {

		/* OK if this returns NULL */
		goneDB = loadDB(GONE, 0, DB_KEYSONLY|DB_NODUPS);

		loadskips();

		unless (idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
			perror("SCCS/x.id_cache");
			exit(1);
		}
	}

	/*
	 * Find a file and go do it.
	 */
	while (buf = mnext(p)) {
		char	*b;
		int	rc;

		++line;
		unless (strncmp(buf, "== ", 3) == 0) {
			if (echo > 7) {
				fprintf(stderr, "skipping: %*s",
				    (int)(strchr(buf, '\n') - buf), buf);
			}
			continue;
		}

		/*
		 * If we see the SFIO header then consider the patch ended.
		 * We'll fall through to sfio to unpack.
		 */
		if (strneq(buf, "== @SFIO@ ==\n", 13)) {
			if (rc = sfio(p)) error = -1;
			break;
		}
		/* we need our own storage , extractPatch calls mkline */
		b = strdup(mkline(buf));
		if (echo>4) fprintf(stderr, "%s\n", b);
		unless ((t = strrchr(b, ' ')) && streq(t, " ==")) {
			SHOUT();
			fprintf(stderr, "Bad patch: %s\n", b);
			cleanup(CLEAN_RESYNC);
		}
		*t = 0;
		files++;
		rc = extractPatch(&b[3], p);
		free(b);
		if (rc < 0) {
			error = rc;
			continue;
		}
		remote += rc;
	}
	mclose(p);
	if (idDB) { mdbm_close(idDB); idDB = 0; }
	if (goneDB) { mdbm_close(goneDB); goneDB = 0; }
	if (error < 0) {
		/* XXX: Save?  Purge? */
		cleanup(CLEAN_RESYNC);
	}
	if (echo) {
		files = 0;
		if (f = popen("bk sfiles RESYNC", "r")) {
			while ((c = getc(f)) > 0) if (c == '\n') ++files;
			pclose(f);
		}

		fprintf(stderr,
		    "takepatch: %d new changeset%s, %d conflicts in %d files\n",
		    remote, remote == 1 ? "" : "s", conflicts, files);
	}

	/* save byte count for logs */
	f = fopen("BitKeeper/log/byte_count", "w");
	if (f) {
		fprintf(f, "%lu\n", (unsigned long)size(pendingFile));
		fclose(f);
	}

	unless (remote) {
		/*
		 * The patch didn't contain any new csets and so we don't
		 * need to run resolve.
		 * We should still run the post resolve trigger.
		 */
		if (resolve) {
			putenv("BK_STATUS=REDUNDANT");
			trigger("resolve", "post");
		}
		cleanup(CLEAN_RESYNC | CLEAN_PENDING | CLEAN_OK);
	}

	/*
	 * The ideas here are to (a) automerge any hash-like files which
	 * we maintain, and (b) converge on the oldest inode for a
	 * particular file.  The converge code will make sure all of the
	 * inodes are present.
	 */
	if (conflicts) converge_hash_files();

	/*
	 * There are instances (contrived) where the ChangeSet
	 * file will not be in the RESYNC tree.  Make sure that
	 * it is.  This will prevent resolve from failing and
	 * hopefully those scary support emails.
	 */
	getChangeSet();

	if (resolve) {
		char 	*resolve[] = {"bk", "resolve", 0, 0, 0, 0, 0, 0};
		int 	i;

		if (echo) {
			fprintf(stderr,
			    "Running resolve to apply new work...\n");
		}
		i = 1;
		unless (echo) resolve[++i] = "-q";
		if (textOnly) resolve[++i] = "-t";
		if (noConflicts) resolve[++i] = "-c";
		if (comments) resolve[++i] = aprintf("-y%s", comments);
		i = spawnvp(_P_WAIT, resolve[0], resolve);
		unless (WIFEXITED(i)) return (-1);
		error = WEXITSTATUS(i);
	}
	exit(error);
}

private void
getChangeSet(void)
{
	if (exists(CHANGESET)) {
		unless (exists(ROOT2RESYNC "/" CHANGESET)) {
			fileCopy(CHANGESET, ROOT2RESYNC "/" CHANGESET);
		}
    	}
}

private hash *
loadCollapsed(void)
{
	FILE	*f;
	hash	*db = 0;
	char	*p;
	char	buf[MAXLINE];

	if (collapsedups) return (0);
	f = popen("bk annotate -ar " COLLAPSED, "r");
	while (fnext(buf, f)) {
		chomp(buf);
		unless (db) db = hash_new(HASH_MEMHASH);
		/* ignore dups, we don't care */
		p = strchr(buf, '\t');
		assert(p);
		*p++ = 0;
		hash_insertStr(db, p, buf);
	}
	pclose(f);
	return (db);
}

private	delta *
getRecord(MMAP *f)
{
	int	e = 0;
	delta	*d = sccs_getInit(0, 0, f, 1, &e, 0, 0);

	if (!d || e) {
		fprintf(stderr,
		    "takepatch: bad delta record near line %d\n", line);
		exit(1);
	}
	unless (d->date || streq("70/01/01 00:00:00", d->sdate)) {
		assert(d->date);
	}
	return (d);
}

private void
shout(void)
{
	static	int	shouted = 0;

	if (shouted) return; /* already done shouting */
	SHOUT();
	shouted = 1;
}

/*
 * Extract a contiguous set of deltas for a single file from the patch file.
 * "name" is the cset name, i.e., the most recent name in the csets being
 * sent; it might be different than the local name.
 */
private	int
extractPatch(char *name, MMAP *p)
{
	delta	*tmp;
	sccs	*s = 0;
	sccs	*perfile = 0;
	int	newFile = 0;
	int	reallyNew = 0;
	int	cset;
	char	*gfile = 0;
	int	nfound = 0, rc;
	char	*t;

	/*
	 * Patch format for continuing file:
	 * == filename ==
	 * lm||19970518232929
	 * lm@lm.bitmover.com|sccs/slib.c|19990130201757
	 * D 1.159 99/02/06 16:57:48-08:00 lm@lm.bitmover.com +160 -55
	 * etc.
	 *
	 * Patch format for new file:
	 * == filename ==
	 * New file: filename as of creation time
	 * perfile information (encoding, etc)
	 *
	 * lm||19970518232929
	 * D 1.1 99/02/23 00:29:01-08:00 lm@lm.bitmover.com +128 -0
	 * etc.
	 *
	 * Tue Mar 28 2000
	 * == filename ==
	 * Grafted file: filename as of creation time
	 * {same as above, no perfile.}
	 */
	t = mkline(mnext(p));
	line++;
	name = name2sccs(name);
	if (strneq("New file: ", t, 10)) {
		reallyNew = newFile = 1;
		perfile = sccs_getperfile(p, &line);
		t = mkline(mnext(p));
		line++;
	}
	if (strneq("Grafted file: ", t, 14)) {
		t = mkline(mnext(p));
		line++;
	}
	if (newProject && !newFile) errorMsg("tp_notfirst", 0, 0);

	if (echo>4) fprintf(stderr, "%s\n", t);
	s = sccs_keyinit(0, t, SILENT, idDB);
	if (s && !s->cksumok) goto error;
	/*
	 * Unless it is a brand new workspace, or a new file,
	 * rebuild the id cache if look up failed.
	 *
	 * If the file is gone, it's an error to get updates to that file.
	 *
	 * XXX - we need some test cases in the regression scripts for this.
	 * a) move the file and make sure it finds it
	 * b) move the file and send a new file over and make sure it finds
	 *    it.
	 */
	unless (s || newProject || newFile) {
		if (gone(t, goneDB)) {
			if (getenv("BK_GONE_OK")) {
				skipPatch(p);
				return (0);
			} else {
				errorMsg("tp_gone_error", t, 0);
			}
		}
		SHOUT();
		fprintf(stderr,
		    "takepatch: can't find key '%s' in id cache\n", t);
error:		if (perfile) sccs_free(perfile);
		if (gfile) free(gfile);
		if (s) sccs_free(s);
		/* we don't free name, we pass it to errfiles */
		errfiles = addLine(errfiles, name);
		return (-1);
	}

	/*
	 * They may have sent us a patch from 1.0, so the patch looks like a
	 * new file.  But if we have a match, we want to use it.
	 */
	if (s) {
		reallyNew = 0;
		if (newFile && (echo > 4)) {
			fprintf(stderr,
			    "takepatch: new file %s already exists.\n", name);
		}
		if (echo > 7) {
			fprintf(stderr, "takepatch: file %s found.\n",
			s->sfile);
		}
		if (EDITED(s)) {
			int cleanflags = SILENT|CLEAN_SHUTUP|CLEAN_CHECKONLY;

			if (sccs_clean(s, cleanflags)) {
				edited = addLine(edited, sccs2name(s->sfile));
				sccs_free(s);
				s = 0;
				goto error;
			} else {
				sccs_restart(s);
			}
		} else if (s->state & S_PFILE) {
			if (unlink(s->pfile)) {
				fprintf(stderr,
				    "takepatch: unlink(%s): %s\n",
				    s->pfile, strerror(errno));
				goto error;
			}
			s->state &= ~S_PFILE; 
		}
		tmp = sccs_top(s);
		unless (CSET(s)) {
			unless (sccs_patheq(tmp->pathname, s->gfile)) {
				badpath(s, tmp);
				goto error;
			}
		}
		unless (tmp = sccs_findKey(s, t)) {
			shout();
			fprintf(stderr,
			    "takepatch: can't find root delta '%s' in %s\n",
			    t, name);
			goto error;
		}
		unless (sccs_ino(s) == tmp) {
			shout();
			fprintf(stderr,
			    "takepatch: root deltas do not match in %s\n",
			    name);
			goto error;
		}
	} else {	/* create a new file */
		/*
		 * ChangeSet is special, don't rename it,
		 * because "resolve" expects to find a ChangeSet
		 * file in its normal location.
		 * ChangeSet is never renamed, so
		 * it is safe to assume there is no
		 * name conflict for the ChangeSet file.
		 */
		if (streq(name, "SCCS/s.ChangeSet") &&
		    exists("SCCS/s.ChangeSet")) {
			errorMsg("tp_changeset_exists", 0, 0);
		}
		if (echo > 3) {
			fprintf(stderr,
			    "takepatch: new file %s\n", t);
		}
	}
	tableGCA = 0;
	if (s) encoding = s->encoding;
	while (extractDelta(name, s, newFile, p, &nfound)) {
		if (newFile) newFile = 2;
	}
	gfile = sccs2name(name);
	if (echo>1) {
		fprintf(stderr, "Updating %s", gfile);
		if (echo == 3) fputc(' ', stderr);
	}
	cset = s ? CSET(s) : streq(name, CHANGESET);
	if (Fast || cset) {
		rc = applyCsetPatch(s, &nfound, perfile);
		s = 0;		/* applyCsetPatch calls sccs_free */
		unless (cset) nfound = 0;	/* only count csets */
	} else {
		if (patchList && tableGCA) getLocals(s, tableGCA, name);
		rc = applyPatch(s ? s->sfile : 0, perfile);
		if (rc < 0) errfiles = addLine(errfiles, strdup(name));
		nfound = 0;	/* only count csets */
	}
	if (echo == 3) fputc(' ', stderr);
	if (echo > 1) fputc('\n', stderr);
	if (perfile) sccs_free(perfile);
	if (streq(gfile, "BitKeeper/etc/config")) {
		/*
		 * If we have rewritten the config file we need to
		 * flush the cache of config data in the project
		 * struct.
		 */
		proj_reset(0);
	}
	free(gfile);
	free(name);
	if (s) sccs_free(s);
	if (rc < 0) {
		cleanup(CLEAN_RESYNC);
		return (rc);
	}
	return (nfound);
}

private	int
sccscopy(sccs *to, sccs *from)
{
	unless (to && from) return (-1);
	to->state |= from->state;
	unless (to->defbranch) {
		to->defbranch = from->defbranch;
		from->defbranch = 0;
	}
	to->encoding = from->encoding;
	unless (to->text) {
		to->text = from->text;
		from->text = 0;
	}
	return (0);
}

#define SKIPKEYS	"BitKeeper/etc/skipkeys"

/*
 * Read in the BitKeeper/etc/skipkeys file
 * Format is alphabetized:
 * rootkey1 deltakey1
 * rootkey1 deltakey2
 * rootkey2 deltakey1
 * rootkey2 deltakey2
 */
private void
loadskips(void)
{
	FILE	*f;
	char	buf[2 * MAXKEY];
	char	*dkey;
	skips	*s = 0;

	f = popen("bk cat " SKIPKEYS, "r");
	while (fnext(buf, f)) {
		chomp(buf);
		if (buf[0] == '#') continue;
		unless (dkey = separator(buf)) {
			fprintf(stderr, "Garbage in skipfiles: %s\n", buf);
			continue;
		}
		*dkey++ = '\0';
		unless (s && streq(buf, s->rkey)) {
			s = new(skips);
			assert(s);
			s->next = skiplist;
			skiplist = s;
			s->rkey = strdup(buf);
		}
		assert(s);
		s->dkeys = addLine(s->dkeys, strdup(dkey));
	}
	pclose(f);
}

private int
skipkey(sccs *s, char *dkey)
{
	char	rkey[MAXKEY];
	skips	*sk;
	int	i;

	unless (s && skiplist) return (0);
	sccs_sdelta(s, sccs_ino(s), rkey);
	for (sk = skiplist; sk; sk = sk->next) {
		unless (streq(sk->rkey, rkey)) continue;
		EACH(sk->dkeys) {
			if (streq(sk->dkeys[i], dkey)) {
				return (1);
			}
		}
		return (0);
	}
	return (0);
}

/*
 * Extract one delta from the patch file.
 * Deltas end on the first blank line.
 */
private	int
extractDelta(char *name, sccs *s, int newFile, MMAP *f, int *np)
{
	delta	*d, *parent = 0, *tmp;
	char	buf[MAXPATH];
	char	*b;
	char	*pid = 0;
	long	off;
	int	c;
	int	skip = 0;
	int	ignore = 0;
	patch	*p;

	if (newFile == 1) goto delta1;

	b = mkline(mnext(f)); line++;
	if (echo>4) fprintf(stderr, "%s\n", b);
	if (strneq(b, "# Patch checksum=", 17)) return 0;
	pid = strdup(b);
	/*
	 * This code assumes that the delta table order is time sorted.
	 * We stash away the parent of the earliest delta as a "GCA".
	 */
	if (parent = sccs_findKey(s, b)) {
		if (!tableGCA || (tableGCA->date >= parent->date)) {
			tableGCA = parent;
		}
	}

	/* go get the delta table entry for this delta */
delta1:	off = mtell(f);
	d = getRecord(f);
	sccs_sdelta(s, d, buf);
	if (Fast) {
		if (skipkey(s, buf)) ignore = 1;
		goto save;
	}
	/*
	 * 11/29/02 - we fill in dangling deltas by pretending they are
	 * incoming deltas which we do not already have.  In the patch
	 * they are not going to be dangling so the flag is clear;
	 * applying the patch will clear the flag and the code paths
	 * are all happier this way.
	 */
	if ((tmp = sccs_findKey(s, buf)) && !tmp->dangling) {
		if (echo > 3) {
			fprintf(stderr,
			    "takepatch: delta %s already in %s, skipping it.\n",
			    tmp->rev, s->sfile);
		}
		skip++;
	} else if (skipkey(s, buf)) {
		if (echo>6) {
			fprintf(stderr,
			    "takepatch: skipping marked delta in %s\n",
			    s->sfile);
		}
		skip++;
	} else {
		fileNum++;
	}
save:	mseekto(f, off);
	if (skip) {
		free(pid);
		/* Eat metadata */
		while ((b = mnext(f)) && (*b != '\n')) line++;
		line++;
		/* Eat diffs */
		while ((b = mnext(f)) && (*b != '\n')) line++;
		line++;
	} else {
		char	*start, *stop;

		start = f->where; stop = start;
		while ((b = mnext(f)) && (*b != '\n')) {
			stop = f->where;
			line++;
			if (echo>4) fprintf(stderr, "%.*s", linelen(b), b);
		}
		line++;
		if (echo>5) fprintf(stderr, "\n");
		p = new(patch);
		p->remote = 1;
		p->pid = pid;
		sccs_sdelta(s, d, buf);
		p->me = strdup(buf);
		p->initMmap = ignore ? 0 : mrange(start, stop, "b");
		p->localFile = s ? strdup(s->sfile) : 0;
		sprintf(buf, "RESYNC/%s", name);
		p->resyncFile = strdup(buf);
		p->order = parent == d ? 0 : d->date;
		if (echo>6) fprintf(stderr, "REM: %s %s %lu\n",
				    d->rev, p->me, p->order);
		c = line;
		start = f->where; stop = start;
		while ((b = mnext(f)) && (*b != '\n')) {
			stop = f->where;
			line++;
			if (echo>5) fprintf(stderr, "%.*s", linelen(b), b);
		}
		if (Fast) {
			/* header and diff are not connected */
			if (d->flags & D_META) p->meta = 1;
			if (start != stop) {
				p->diffMmap = mrange(start, stop, "b");
			}
		} else if (d->flags & D_META) {
			p->meta = 1;
			assert(c == line);
		} else {
			p->diffMmap = mrange(start, stop, "b");
		}
		line++;
		if (echo>5) fprintf(stderr, "\n");
		(*np)++;
		insertPatch(p);
	}
	sccs_freetree(d);
	if ((c = mpeekc(f)) != EOF) {
		return (c != '=');
	}
	return (0);
}

/*
 * Skip to the next file start.
 * Deltas end on the first blank line.
 */
private	int
skipPatch(MMAP *p)
{
	char	*b;
	int	c;

	do {
		b = mnext(p); line++;
		if (strneq(b, "# Patch checksum=", 17)) return 0;
		/* Eat metadata */
		while ((b = mnext(p)) && (*b != '\n')) line++;
		line++;
		/* Eat diffs */
		while ((b = mnext(p)) && (*b != '\n')) line++;
		line++;
		if ((c = mpeekc(p)) == EOF) return (0);
	} while (c != '=');
	return (0);
}

private void
errorMsg(char *msg, char *arg1, char *arg2)
{
	SHOUT();
	getMsg2(msg, arg1, arg2, 0, stderr);
    	cleanup(CLEAN_RESYNC|CLEAN_PENDING);
}

private	void
badpath(sccs *s, delta *tot)
{
	SHOUT();
	getMsg2("tp_badpath", s->gfile, tot->pathname, 0, stderr);
}

private	void
badXsum(int a, int b)
{
	SHOUT();
	if (echo > 3) {
		char	*p = aprintf(" (%x != %x)", a, b);

		getMsg("tp_badXsum", p, 0, stderr);
		free(p);
	} else {
		getMsg("tp_badXsum", "", 0, stderr);
	}
	cleanup(CLEAN_PENDING|CLEAN_RESYNC);
	/* XXX - should clean up everything if this was takepatch -i */
}

/*
 * Most of the code in this function is copied from applyPatch
 * We may want to merge the two function later.
 *
 * This tries within a reasonable amount of effort to write the
 * SCCS file only once.  This can be done because the sccs struct
 * is in memory and can be fiddled with carefully.  The weaving
 * of new information is done when we are done fiddling with the sccs
 * file and write out the table, then weave the new deltas into the
 * body.  While this is doable for the general weave case, it is
 * very doable for the ChangeSet file (or any hash file) because
 * the weave is an ordered set of blocks, youngest to oldest.
 * So by building a table of blocks to weave, as well as a way to
 * know how to increment the serial number of existing blocks, it
 * is possible to only write the file once.
 */
private	int
applyCsetPatch(sccs *s, int *nfound, sccs *perfile)
{
	patch	*p;
	MMAP	*iF;
	MMAP	*dF;
	delta	*d = 0;
	delta	*top = 0;
	delta	*remote_tagtip = 0;
	int	n = 0;
	int	c;
	int	psize = *nfound;
	int	confThisFile;
	int	port = 0;
	FILE	*csets = 0, *f;
	hash	*cdb;
	char	csets_in[MAXPATH];
	char	buf[MAXKEY];

	reversePatch();
	unless (p = patchList) return (0);

	if (echo == 3) fprintf(stderr, "%c\b", spin[n++ % 4]);
	if (echo > 7) {
		fprintf(stderr, "L=%s\nR=%s\nP=%s\nM=%s\n",
		    p->localFile, p->resyncFile, p->pid, p->me);
	}
	/*
	 * XXX: could this be done in cset_write()? As in, do not create
	 * until it is needed?  This seems to work but may be excess
	 */
	if (mkdirf(p->resyncFile) == -1) {
		if (errno == EINVAL) {
			getMsg("reserved_name",
			    p->resyncFile, '=', stderr);
			return (-1);
		}
	}
	if (s) {
		/* arrange for this sccs* to write into RESYNC */
		free(s->sfile);
		s->sfile = strdup(p->resyncFile);
		free(s->gfile);
		s->gfile = sccs2name(s->sfile);
		free(s->pfile);
		s->pfile = strdup(sccs_Xfile(s, 'p'));
		free(s->zfile);
		s->zfile = strdup(sccs_Xfile(s, 'z'));
		s->state &= ~(S_PFILE|S_ZFILE|S_GFILE);
		/* NOTE: we leave S_SFILE set, but no sfile there */

		unless (CSET(s)) top = sccs_top(s);
	} else {
		unless (s = sccs_init(p->resyncFile, NEWFILE|SILENT)) {
			SHOUT();
			fprintf(stderr,
			    "takepatch: can't create %s\n", p->resyncFile);
			goto err;
		}
		s->bitkeeper = 1;
		if (perfile) sccscopy(s, perfile);
	}
	assert(s);
	cweave_init(s, psize);
	*nfound = 0;
	while (p) {
		if (echo == 3) fprintf(stderr, "%c\b", spin[n % 4]);
		n++;
		if (echo>9) {
			fprintf(stderr, "PID: %s\nME:  %s\n",
			    p->pid ? p->pid : "none", p->me);
		}
		/*
		 * iF = 0 if skipkey found it; so its parent may have also
		 * been skipkey'd and so not here; don't look up parent
		 * without iF being set (as well as parent pointer p->pid).
		 * p->pid won't be set for 1.0 delta.
		 */
		dF = p->diffMmap;
		iF = p->initMmap;
		d = 0;	/* in this case, parent */
		if (iF && p->pid) {
			unless (d = sccs_findKey(s, p->pid)) {
				if ((echo == 2) || (echo == 3)) {
					fprintf(stderr, " \n");
				}
				errorMsg("tp_ahead", p->pid, s->sfile);
				/*NOTREACHED*/
			}
		}
		if (echo>9) {
			fprintf(stderr, "Child of %s", d ? d->rev : "none");
			if (p->meta) {
				fprintf(stderr, " meta\n");
			} else {
				fprintf(stderr, " data\n");
			}
		}
		/* passing in d = parent, setting d = new or existing */
		if (d = cset_insert(s, iF, dF, d, Fast)) {
			(*nfound)++;
			p->d = d;
			if (d->flags & D_REMOTE) {
				if (d->symGraph) remote_tagtip = d;
			}
		}
		p = p->next;
	}
	unless (*nfound) {
		if (HAS_SFILE(s)) {
			sccs_close(s);
			unlink(s->sfile);
			sccs_rmEmptyDirs(s->sfile);
		}
		goto done;
	}
	/*
	 * pull -r can propagate a non-tip tag element as the tip.
	 * We have to mark it here before writing the file out.
	 */
	if (remote_tagtip && !remote_tagtip->symLeaf) {
		assert(CSET(s));
		remote_tagtip->symLeaf = 1;
		debug((stderr,
		    "takepatch: adding leaf to tag delta %s (serial %d)\n",
		    remote_tagtip->rev, remote_tagtip->serial));
	}
	if (CSET(s) && (echo == 3)) fprintf(stderr, "\b, ");
	if (cset_write(s, (echo == 3), Fast)) {
		SHOUT();
		fprintf(stderr, "takepatch: can't update %s\n", s->sfile);
		goto err;
	}
	s = sccs_restart(s);
	assert(s);

	unless (CSET(s)) goto markup;

	if (cdb = loadCollapsed()) {
		for (p = patchList; p; p = p->next) {
			unless ((d = p->d) && (d->flags & D_REMOTE)) continue;
			sccs_md5delta(s, d, buf);
			if (hash_fetchStr(cdb, buf)) {
				/* find cset that added that entry */
				sprintf(buf, "bk -R r2c -r%s " COLLAPSED,
				    (char *)cdb->vptr);
				f = popen(buf, "r");
				fnext(buf, f);
				chomp(buf);
				pclose(f);
				errorMsg("takepatch-collapsed", cdb->kptr, buf);
				hash_free(cdb);
				goto err;
			}
		}
		hash_free(cdb);
	}
	/* examine incoming csets and looks for updates to components with
	 * pending deltas
	 */
	if (proj_isProduct(0)) {
		for (p = patchList; p; p = p->next) {
			char	*t, *s;
			char	key[MAXKEY];

			unless (p->diffMmap) continue;
			while (t = mnext(p->diffMmap)) {
				unless (*t == '>') continue;
				t += 2;
				s = separator(t);
				strncpy(key, t, s-t);
				key[s-t] = 0;

				unless (changesetKey(key)) continue;

				t = key2path(key, idDB);
				if (sccs_isPending(t)) {
					dirname(t); /* strip /ChangeSet */
					getMsg("tp_uncommitted",
					    t, 0, stderr);
					free(t);
					goto err;
				}
				free(t);
			}
			mseekto(p->diffMmap, 0);
		}
	}
	sprintf(csets_in, "%s/%s", ROOT2RESYNC, CSETS_IN);
	csets = fopen(csets_in, "w");
	assert(csets);
	for (p = patchList; p; p = p->next) {
		unless ((d = p->d) && (d->flags & D_REMOTE)) continue;
		fprintf(csets, "%s\n", p->me);
	}
	fclose(csets);
markup:
	/*
	 * D_REMOTE used in sccs_resolveFiles()
	 * D_SET used in cset_resum()
	 */
	for (d = 0, p = patchList; p; p = p->next) {
		/*
		 * In fastpatch, diffMmap is not related to initMmap, so
		 * just clear all of them.  And mclose works if 0 passed in.
		 */
		mclose(p->diffMmap); /* win32: must mclose after cset_write */
		unless ((d = p->d) && (d->flags & D_REMOTE)) continue;
		d->flags |= D_SET; /* for resum() */
		unless (TAG(d)) port = !(d->flags & D_CSET);
	}
	if (top && (top->dangling || !(top->flags & D_CSET))) {
		delta	*a, *b;

		if (top->dangling && sccs_findtips(s, &a, &b)) {
			fprintf(stderr, "takepatch: monotonic file %s "
			    "has dangling deltas\n", s->sfile);
			goto err;
		}
		if (!(top->flags & D_CSET) && sccs_isleaf(s, top)) {
			/* uncommitted error for dangling is backward compat */
			SHOUT();
			getMsg("tp_uncommitted", s->gfile, 0, stderr);
			goto err;
		}
	}
	/*
	 * Make a new changeset node in resolve if no new node created
	 */
	s->state |= S_SET;
	if (CSET(s)) {
		if (echo == 3) fprintf(stderr, "\b, ");
		if (cset_resum(s, 0, 0, echo == 3, 1)) {
			getMsg("takepatch-chksum", 0, '=', stderr);
			goto err;
		}
	} else if (!BAM(s)) {
		for (d = s->table; d; d = d->next) {
			unless ((d->flags & D_SET) && !TAG(d)) continue;
			c = sccs_resum(s, d, 0, 0);
			if (c & 2) {
				getMsg("takepatch-chksum", 0, '=', stderr);
				goto err;
			}
		}
	}

	if ((confThisFile = sccs_resolveFiles(s)) < 0) goto err;
	/* Signal resolve to add a null cset for ChangeSet path fixup. */
	if (port && exists(ROOT2RESYNC "/SCCS/m.ChangeSet")) {
		touch(ROOT2RESYNC "/SCCS/n.ChangeSet", 0664);
	}
	if (!confThisFile && (s->state & S_CSET) && 
	    sccs_admin(s, 0, SILENT|ADMIN_BK, 0, 0, 0, 0, 0, 0, 0)) {
	    	confThisFile++;
		/* yeah, the count is slightly off if there were conflicts */
	}
	conflicts += confThisFile;
	if (BAM(s) && !bp_hasBAM()) {
		/* this shouldn't be needed... */
		if (touch(BAM_MARKER, 0664)) perror(BAM_MARKER);
	}
done:	sccs_free(s);
	s = 0;
	if (noConflicts && conflicts) errorMsg("tp_noconflicts", 0, 0);
	freePatchList();
	patchList = 0;
	fileNum = 0;
	return (0);
err:
	if (s) sccs_free(s);
	return (-1);
}

/* 
 * an existing file that was in the patch but didn't
 * get any deltas.  Usually an error, but we should
 * handle this better.
 */
private int
noupdates(char *localPath)
{
	char    *resync = aprintf("RESYNC/%s", localPath);
	int	rc = 0, i = 0;
	sccs	*s;
	delta	*d;
	FILE	*f;
	char	*p, buf[MAXKEY*2], key[MAXKEY];
	
	while (exists(resync)) {
		free(resync);
		resync = aprintf("RESYNC/BitKeeper/RENAMES/s.%d", i++);
	}
	fileCopy2(localPath, resync);

	/* No changeset, no marks for you, dude */
	unless (exists(ROOT2RESYNC "/SCCS/s.ChangeSet")) goto out;

	/*
	 * bk fix -c can leave files that are pending but then a pull
	 * puts back the changes.  Make sure that if we have pending
	 * deltas we are either filling that back in or error with a
	 * pending message.
	 */
	s = sccs_init(resync, INIT_NOCKSUM);
	sccs_sdelta(s, sccs_ino(s), key);
	f = popen("bk annotate -R -h " ROOT2RESYNC "/ChangeSet", "r");
	while (fnext(buf, f)) {
		chomp(buf);
		p = separator(buf);
		assert(p);
		*p++ = 0;
		unless (streq(key, buf)) continue;
		d = sccs_findKey(s, p);
		assert(d);
		if (d->flags & D_CSET) continue;
		if (echo > 4) fprintf(stderr,"MARK(%s|%s)\n", s->gfile, d->rev);
		d->flags |= D_CSET;
	}
	pclose(f);
	unless (sccs_top(s)->flags & D_CSET) {
		SHOUT();
		getMsg("tp_uncommitted",
		    s->gfile + strlen(ROOT2RESYNC) + 1, 0, stderr);
		rc = -1;
	} else {
		sccs_newchksum(s);
	}
	sccs_free(s);
out:	free(resync);
	return (rc);
}

/*
 * If the destination file does not exist, just apply the patches to create
 * the new file.
 * If the file does exist, copy it, rip out the old stuff, and then apply
 * the list.
 */
private	int
applyPatch(char *localPath, sccs *perfile)
{
	patch	*p;
	MMAP	*iF;
	MMAP	*dF;
	sccs	*s = 0;
	delta	*d = 0;
	int	newflags;
	int	pending = 0;
	int	n = 0;
	int	confThisFile;
	int	flags = echo ? 0 : SILENT;

	reversePatch();
	p = patchList;
	if (!p && localPath) return (noupdates(localPath));

	if (echo == 3) fprintf(stderr, "%c\b", spin[n++ % 4]);
	if (echo > 7) {
		fprintf(stderr, "L=%s\nR=%s\nP=%s\nM=%s\n",
		    p->localFile, p->resyncFile, p->pid, p->me);
	}
	unless (localPath) {
		if (mkdirf(p->resyncFile) == -1) {
			if (errno == EINVAL) {
				getMsg("reserved_name",
				    p->resyncFile, '=', stderr);
				return (-1);
			}
		}
		goto apply;
	}
	fileCopy2(localPath, p->resyncFile);
	unless (s = sccs_init(p->resyncFile, INIT_NOCKSUM|flags)) {
		SHOUT();
		fprintf(stderr, "takepatch: can't open %s\n", p->resyncFile);
		return -1;
	}
	unless (HASGRAPH(s)) {
		SHOUT();
		if (!(s->state & S_SFILE)) {
			fprintf(stderr,
			    "takepatch: no s.file %s\n", p->resyncFile);
		} else {
			perror(s->sfile);
		}
		return -1;
	}
	/* convert to uncompressed, it's faster, we saved the mode above */
	if (s->encoding & E_GZIP) s = sccs_unzip(s);
	unless (s) return (-1);
	unless (tableGCA) goto apply;
	/*
	 * Note that tableGCA is NOT a valid pointer into the sccs tree "s".
	 */
	assert(tableGCA);
	assert(tableGCA->rev);
	assert(tableGCA->pathname);
	if (echo > 6) {
		fprintf(stderr,
		    "stripdel %s from %s\n", tableGCA->rev, s->sfile);
	}
	if (d = sccs_next(s, sccs_findrev(s, tableGCA->rev))) {
		delta	*e;

		for (e = s->table; e; e = NEXT(e)) {
			e->flags |= D_SET|D_GONE;
		    	if (echo>7) {
				char	k[MAXKEY];

				sccs_sdelta(s, e, k);
				fprintf(stderr, "STRIP %s/%u/%c %s\n",
				    e->rev, e->serial, e->type, k);
			}
			if (e == d) break;
		}
		if (sccs_stripdel(s, "takepatch")) {
			SHOUT();
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "stripdel of %s failed.\n", p->resyncFile);
			}
			return -1;
		}
	}
	sccs_free(s);
	/* sccs_restart does not rebuild the graph and we just pruned it,
	 * so do a hard restart.
	 */
	unless (s = sccs_init(p->resyncFile, INIT_NOCKSUM|flags)) {
		SHOUT();
		fprintf(stderr,
		    "takepatch: can't open %s\n", p->resyncFile);
		return -1;
	}
apply:

	p = patchList;
	if (p && !p->pid) {
		/* initial file create */
		if (echo == 3) fprintf(stderr, "%c\b", spin[n % 4]);
		n++;
		assert(s == 0);
		unless (s = sccs_init(p->resyncFile, NEWFILE|SILENT)) {
			SHOUT();
			fprintf(stderr,
			    "takepatch: can't create %s\n",
			    p->resyncFile);
			return -1;
		}
		if (perfile) {
			sccscopy(s, perfile);
			/*
			 * For takepatch performance turn off
			 * compression when we are in takepatch.
			 *
			 * Note: Since this is a new file from remote,
			 * there is no local setting. We save the
			 * compression setting of the remote file and
			 * use that as the new local file when when
			 * takepatch is done.
			 */
			encoding = s->encoding; /* save for later */
			s->encoding &= ~E_GZIP;
		}
		if (p->initFile) {
			iF = mopen(p->initFile, "b");
		} else {
			iF = p->initMmap;
			p->initMmap = 0;
		}
		if (p->diffFile) {
			dF = mopen(p->diffFile, "b");
		} else {
			dF = p->diffMmap;
			p->diffMmap = 0;
		}
		d = 0;
		newflags = NEWFILE|DELTA_FORCE|DELTA_PATCH|DELTA_NOPENDING;
		if (echo <= 3) newflags |= SILENT;
		if (sccs_delta(s, newflags, d, iF, dF, 0)) {
			unless (s->io_error) perror("delta");
			return -1;
		}
		if (s->bad_dsum || s->io_error) return (-1);
		mclose(iF);	/* dF done by delta() */
		sccs_free(s);
		s = sccs_init(p->resyncFile, INIT_NOCKSUM|SILENT);
		if (p = p->next) s->mem_out = 1;
	}
	/* enable in-memory mode, if more than one patch */
	if (p && p->next) s->mem_out = 1;
	while (p) {
		assert(p->pid);
		if (echo == 3) fprintf(stderr, "%c\b", spin[n % 4]);
		n++;
		assert(s);
		if (echo > 9) {
			fprintf(stderr,
			    "------------- %s delta ---------\n"
			    "PID: %s\nME:  %s\n",
			    p->local ? "local" : "remote",
			    p->pid, p->me);
		}
		unless (d = sccs_findKey(s, p->pid)) {
			if ((echo == 2) || (echo == 3)) {
				fprintf(stderr, " \n");
			}
			errorMsg("tp_ahead", p->pid, s->sfile);
			/*NOTREACHED*/
		}
		unless (sccs_restart(s)) { perror("restart"); exit(1); }
		if (echo > 9) fprintf(stderr, "Child of %s\n", d->rev);
		assert(!p->meta); /* this is not the cset file */
		newflags = GET_FORCE|GET_SKIPGET|GET_EDIT;
		unless (echo > 6) newflags |= SILENT;
		if (sccs_get(s, d->rev, 0, 0, 0, newflags, "-")) {
			perror("get");
			return (-1);
		}
		if (p->initFile) {
			iF = mopen(p->initFile, "b");
		} else {
			iF = p->initMmap;
			p->initMmap = 0;
		}
		if (p->diffFile) {
			dF = mopen(p->diffFile, "b");
		} else {
			dF = p->diffMmap;
			p->diffMmap = 0;
		}
		newflags = DELTA_FORCE|DELTA_PATCH|DELTA_NOPENDING;
		if (echo <= 3) newflags |= SILENT;
		if (sccs_delta(s, newflags, 0, iF, dF, 0)) {
			unless (s->io_error) perror("delta");
			return (-1);
		}
		if (s->bad_dsum || s->io_error) return -1;
		mclose(iF);
		p = p->next;
	}
	sccs_free(s);
	s = sccs_init(patchList->resyncFile, SILENT);
	assert(s);

	/*
	 * Honor gzip on all files.
	 */
	if (encoding & E_GZIP) s = sccs_gzip(s);
	for (d = 0, p = patchList; p; p = p->next) {
		assert(p->me);
		d = sccs_findKey(s, p->me);
		/*
		 * XXX - this is probably an incomplete fix.
		 * The problem was that we got a patch with a meta delta
		 * with no content in it and delta_table() tossed it out.
		 * So when we go looking for it, we don't find it.
		 * What is not being checked here is if the delta was
		 * indeed empty.
		 */
		if (!d && p->meta) continue;
		assert(d);
		if (p->remote) d->flags |= D_REMOTE;
		if (s->state & S_CSET) continue;
		if (sccs_isleaf(s, d) && !(d->flags & D_CSET)) pending++;
	}
	if (pending) {
		sccs_free(s);
		SHOUT();
		getMsg("tp_uncommitted", localPath, 0, stderr);
		return (-1);
	}
	if ((confThisFile = sccs_resolveFiles(s)) < 0) {
		sccs_free(s);
		return (-1);
	}
	if (!confThisFile && (s->state & S_CSET) && 
	    sccs_admin(s, 0, SILENT|ADMIN_BK, 0, 0, 0, 0, 0, 0, 0)) {
	    	confThisFile++;
		/* yeah, the count is slightly off if there were conflicts */
	}
	conflicts += confThisFile;
	if (BAM(s) && !bp_hasBAM()) {
		/* this shouldn't be needed... */
		if (touch(BAM_MARKER, 0664)) perror(BAM_MARKER);
	}
	sccs_free(s);
	if (noConflicts && conflicts) errorMsg("tp_noconflicts", 0, 0);
	freePatchList();
	patchList = 0;
	fileNum = 0;
	return (0);
}

/*
 * Include up to but not including tableGCA in the list.
 */
private	int
getLocals(sccs *s, delta *g, char *name)
{
	FILE	*t;
	patch	*p;
	delta	*d;
	int	n = 0;
	static	char tmpf[MAXPATH];	/* don't allocate on stack */

	if (echo > 6) {
		fprintf(stderr, "getlocals(%s, %s, %s)\n",
		    s->gfile, g->rev, name);
	}
	for (d = s->table; d != g; d = NEXT(d)) {
		/*
		 * Silently discard removed deltas, we don't support them.
		 */
		if ((d->type == 'R') && !(d->flags & D_META)) continue;

		/*
		 * If we are dangling, don't insert the local delta if
		 * it is already in the patch list.  In applyPatch()
		 * we'll undangle the delta.
		 */
		if (d->dangling) {
			char	key[MAXPATH];

			sccs_sdelta(s, d, key);
			for (p = patchList; p; p = p->next) {
				if (streq(key, p->me)) break;
			}
			if (p) continue;
		}
			
		assert(d);
		sprintf(tmpf, "RESYNC/BitKeeper/tmp/%03d-init", ++fileNum);
		unless (t = fopen(tmpf, "w")) {
			perror(tmpf);
			exit(1);
		}
		sccs_restart(s);
		s->rstart = s->rstop = d;
		sccs_prs(s, PRS_PATCH|SILENT, 0, NULL, t);
		if (ferror(t)) {
			perror("error on init file");
			cleanup(CLEAN_RESYNC);
		}
		fclose(t);

		p = new(patch);
		p->local = 1;
		p->initFile = strdup(tmpf);
		p->localFile = strdup(name);
		sprintf(tmpf, "RESYNC/%s", name);
		p->resyncFile = strdup(tmpf);
		sprintf(tmpf, "RESYNC/BitKeeper/tmp/%03d-diffs", fileNum);
		unless (d->flags & D_META) {
			int	dflag;

			p->diffFile = strdup(tmpf);
			sccs_restart(s);
			dflag =
			    (s->state & S_CSET) ? GET_HASHDIFFS : GET_BKDIFFS;
			if (sccs_getdiffs(s, d->rev, dflag, tmpf)) {
				SHOUT();
				fprintf(stderr, "unable to create diffs");
				cleanup(CLEAN_RESYNC);
			}
		} else {
			p->meta = 1;
		}
		unless (d->date || streq("70/01/01 00:00:00", d->sdate)) {
			assert(d->date);
		}
		sccs_sdelta(s, PARENT(s, d), tmpf);
		p->pid = strdup(tmpf);
		sccs_sdelta(s, d, tmpf);
		p->me = strdup(tmpf);
		p->order = d->date;
		if (echo>6) {
			fprintf(stderr,
			    "LOCAL: %s %s %lu\n", d->rev, p->me, p->order);
		}
		insertPatch(p);
		n++;
	}
	return (n);
}


/*
 * Return true if 'a' is earlier than 'b'
 */
private	int
earlier(patch *a, patch *b)
{
	int ret;

	if (a->order < b->order) return 1;
	if (a->order > b->order) return 0;
	ret = strcmp(a->me, b->me);
	if (ret < 0)   return 1;
	if (ret > 0)   return 0;
	assert("Can't figure out the order of deltas\n" == 0);
	return (-1); /* shut off VC++ comipler warning */
}

/*
 * Insert the delta in the list in sorted time order.
 * The global patchList points at the newest/youngest.
 */
private	void
insertPatch(patch *p)
{
	patch	*t;

	if (!patchList || earlier(patchList, p)) {
		p->next = patchList;
		patchList = p;
		return;
	}
	/*
	 * We know that t is pointing to a node that is younger than us.
	 */
	for (t = patchList; t->next; t = t->next) {
		if (earlier(t->next, p)) {
			p->next = t->next;
			t->next = p;
			return;
		}
	}

	/*
	 * There is no next field and we know that t->order is > date.
	 */
	assert(earlier(p, t));
	t->next = p;
}

/*
 * Reverse order to optimize reading
 * patchList will be left pointing at the oldest delta in the patch
 */
private	void
reversePatch(void)
{
	/* t - temp; f - forward; p - previous */
	patch	*t, *f, *p;

	if (!patchList) return;
	for (p = 0, t = patchList; t; t = f) {
		f = t->next;
		t->next = p;
		p = t;
	}
	assert(p);
	patchList = p;
}

private	void
freePatchList(void)
{
	patch	*p;

	for (p = patchList; p; ) {
		patch	*next = p->next;

		if (p->initFile) {
			unlink(p->initFile);
			free(p->initFile);
		}
		if (p->diffFile) {
			unlink(p->diffFile);
			free(p->diffFile);
		}
		if (p->localFile) free(p->localFile);
		if (p->initMmap) free(p->initMmap);
		free(p->resyncFile);
		if (p->pid) free(p->pid);
		if (p->me) free(p->me);
		free(p);
		p = next;
	}
}

/*
 * Create enough stuff that the tools can find the project root.
 */
private	void
initProject(void)
{
	unless (emptyDir(".")) {
		SHOUT();
		fprintf(stderr,
		    "takepatch: -i can only be used in an empty directory\n");
		SHOUT2();
		/*
		 * We MUST exit here.  It is an invariant that if we are not
		 * empty we abort.  See cleanup().
		 */
		exit(1);
	}
	sccs_mkroot(".");
}

private void
resync_lock(void)
{
	FILE	*f;

	/*
	 * See if we can lock the tree.
	 * We assume that a higher level program called repository_wrlock(),
	 * we're just doing the RESYNC part.
	 *
	 * Note: bk's mkdir will pass if RESYNC already exists so we need
	 * to test that separately.
	 */
	if ((isdir("RESYNC") && !nested_mine(0, getenv("_NESTED_LOCK"), 1))
	    || mkdir("RESYNC", 0777)) {
		fprintf(stderr, "takepatch: cannot create RESYNC dir.\n");
		repository_lockers(0);
		cleanup(0);
	}
	unless (mkdir("RESYNC/BitKeeper", 0777) == 0) {
		SHOUT();
		perror("mkdir");
		cleanup(CLEAN_RESYNC);
	}
	sccs_mkroot("RESYNC");
	unless (f = fopen("RESYNC/BitKeeper/tmp/pid", "w")) {
		SHOUT();
		perror("RESYNC/BitKeeper/tmp/pid");
		cleanup(CLEAN_RESYNC);
	}
	fprintf(f, "%u\n", getpid());
	fclose(f);
}

/*
 * Next line is patch checksum, then SFIO data follows.
 */
private int
sfio(MMAP *m)
{
	char	*t;
	FILE	*f = 0;
	sccs	*s = 0, *sr = 0;
	delta	*d;
	size_t	left, n;
	char	*flist;
	int	rc = -1, rlen;
	char	buf[MAXLINE];
	char	key[MAXKEY];

	unless ((t = mnext(m)) && strneq(t-1, "\n\n# Patch checksum=", 19)) {
		return (-1);
	}

	flist = bktmp(0, "sfio_filelist");
	sprintf(buf, "bk sfio -eigB %s - > '%s'",
	    (echo > 1) ? "-P'Updating '" : "-q", flist);

	fflush(stdout);
	chdir(ROOT2RESYNC);
	f = popen(buf, "w");
	chdir(RESYNC2ROOT);
	unless (f) {
		perror(prog);
		goto err;
	};
	t = mnext(m);
	t = strchr(t, '\n') + 1;	/* will not have \n if SFIO errored */
	assert(strneq(t, "SFIO ", 5));
	left = m->end - t;
	do {
		unless (n = fwrite(t, 1, left, f)) break;
		left -= n;
		t += n;
	} while (left);
	if (pclose(f)) {
		fprintf(stderr, "takepatch: BAM sfio -i failed.\n");
		f = 0;
		goto err;
	}
	/* process each file unpacked */
	unless (f = fopen(flist, "r")) {
		perror(prog);
		goto err;
	}
	strcpy(buf, ROOT2RESYNC "/");
	t = buf + strlen(buf);
	rlen = sizeof(buf) - strlen(buf); /* space in buf after RESYNC/ */
	while (fgets(t, rlen, f)) {
		chomp(t);
		if (strchr(t, '|')) continue; /* skip BAM keys */

		unless (sr = sccs_init(buf,
			SILENT|INIT_NOCKSUM|INIT_MUSTEXIST)) {
			fprintf(stderr, "takepatch: can't open %s\n", buf);
			goto err;
		}
		sccs_sdelta(sr, sccs_ino(sr), key); /* rootkey */

		/* Check the original version of this file */
		unless (s = sccs_keyinit(0, key, INIT_NOCKSUM, idDB)) {
			/* must be new file? */
			sccs_free(sr);
			sr = 0;
			continue;
		}
		assert(!CSET(s));  /* some more logic is needed for cset */

		/* local diffs are bad */
		if (sccs_clean(s, SILENT|CLEAN_SHUTUP|CLEAN_CHECKONLY)) {
			edited = addLine(edited, sccs2name(s->sfile));
			goto err;
		}
		if (s->table->dangling) {
			fprintf(stderr, "takepatch: monotonic file %s "
			    "has dangling deltas\n", s->sfile);
			goto err;
		}
		sccs_sdelta(s, s->table, key); /* local tipkey */

		unless (d = sccs_findKey(sr, key)) {
			/*
			 * I can't find local tipkey in resync file.
			 * This might be because it is a pending delta, if
			 * so complain about that, otherwise just complain.
			 * Note, it is OK to have a pending delta that
			 * has been committed in the resync version, the old
			 * code used to ignore these duplicate deltas.
			 */

			/* because of pending deltas? */
			unless (s->table->flags & D_CSET) {
				SHOUT();
				getMsg("tp_uncommitted", s->sfile, 0, stderr);
			} else {
				fprintf(stderr,
				    "takepatch: key '%s' not found "
				    "in sfile %s\n", key, buf);
			}
			goto err;
		}
		/* mark remote-only deltas */
		range_walkrevs(sr, d, 0, 0, 0,
		    walkrevs_setFlags, (void*)D_REMOTE);
		/*
		 * techically, d->flags |= D_LOCAL, but D_LOCAL goes away
		 * in /home/bk/bk and the way resolveFiles is written, it
		 * does the right thing with or without D_LOCAL.
		 */
		d->flags |= D_LOCAL;
		if (sccs_resolveFiles(sr) < 0) goto err;
		sccs_free(s);
		sccs_free(sr);
		s = sr = 0;
	}
	rc = 0;

err:	if (f) fclose(f);
	unlink(flist);
	free(flist);
	if (s) sccs_free(s);
	if (sr) sccs_free(sr);
	return (rc);
}

/*
 * Go find the change set file and do this relative to that.
 * Create the RESYNC dir or bail out if it exists.
 * Put our pid in that dir so that we can figure out if
 * we are still here.
 *
 * This function creates patches in the PENDING directory when the
 * patches are read from stdin.
 */
private	MMAP	*
init(char *inputFile)
{
	char	buf[BUFSIZ];		/* used ONLY for input I/O */
	char	*root, *t;
	int	i, len;
	FILE	*f, *g;
	MMAP	*m = 0;
	uLong	sumC = 0, sumR = 0;
	int	sfiopatch = 0;
	struct	{
		u32	newline:1;	/* current line is a newline */
		u32	preamble_nl:1;	/* previous line was a newline */
		u32	preamble:1;	/* looking for patch version */
		u32	version:1;	/* previous line was Patch vers: ... */
		u32	type:1;		/* looking for patch type */
		u32	versionblank:1;	/* previous line was \n after vers */
		u32	filename:1;	/* previous line was == file == */
		u32	first:1;	/* previous line was == file == */
		u32	perfile:1;	/* previous line was perfile stuff */
		u32	perblank:1;	/* previous line was per blank */
		u32	metadata:1;	/* previous line was meta data block */
		u32	metaline:1;	/* previous line was 48x- */
		u32	metablank:1;	/* previous line was \n after meta */
		u32	diffs:1;	/* previous line was diffs */
		u32	diffsblank:1;	/* previous line was \n after diffs */
		u32	sfiopatch:1;	/* previous line was SFIO marker */
	}	st;
	int	line = 0, j = 0;
	char	*note;
	char	incoming[MAXPATH];

	bzero(&st, sizeof(st));
	st.preamble = 1;

	/*
	 * If we are reading from stdin and we get ERROR/Nothing,
	 * then bail out before creating any state.
	 */
	if (streq(inputFile, "-")) {
		if (fnext(buf, stdin)) {
			if (streq(buf, "ERROR\n")) exit(1);
			if (streq(buf, "OK-Nothing to resync.\n")) {
				if (echo) fputs(buf, stderr);
				exit(0);
			}
		} else {
			errorMsg("tp_nothingtodo", 0, 0);
		}
	}

	if (newProject) {
		initProject();
	} else {
		root = proj_root(0);
		if (!root && emptyDir(".")) {
			/* If we are invoked in an empty directory,
			 * assume they meant -i.
			 */
			if (echo > 0) {
				fputs("takepatch: creating new repository.\n",
				      stderr);
			}
			initProject();
			newProject = 1;
		} else if (proj_cd2root()) {
			SHOUT();
			fputs("takepatch: can't find package root.\n", stderr);
			SHOUT2();
			exit(1);
		}
	}
	if (streq(inputFile, "-")) {
		if (echo && mkpatch) fprintf(stderr, "Receiving patch ... ");
		/*
		 * Save the patch in the pending dir
		 * and record we're working on it.  We use a .incoming
		 * file and then rename it later.
		 */
		unless (savefile("PENDING", ".incoming", pendingFile)) {
			SHOUT();
			perror("PENDING");
			cleanup(CLEAN_RESYNC);
		}
		f = fopen(pendingFile, "wb+");
		assert(f);
		resync_lock();
		for (;;) {
			st.newline = streq("\n", buf);
			if (echo > 10) {
				fprintf(stderr, "ST: ");
				if (st.newline) fprintf(stderr, "nl ");
				if (st.preamble_nl) fprintf(stderr, "p_nl ");
				if (st.preamble) fprintf(stderr, "p ");
				if (st.version) fprintf(stderr, "v ");
				if (st.versionblank) fprintf(stderr, "vb ");
				if (st.filename) fprintf(stderr, "f ");
				if (st.first) fprintf(stderr, "1 ");
				if (st.perfile) fprintf(stderr, "pf ");
				if (st.perblank) fprintf(stderr, "pb ");
				if (st.metadata) fprintf(stderr, "m ");
				if (st.metaline) fprintf(stderr, "ml ");
				if (st.metablank) fprintf(stderr, "mb ");
				if (st.diffs) fprintf(stderr, "d ");
				if (st.diffsblank) fprintf(stderr, "db ");
				if (st.sfiopatch) fprintf(stderr, "sfio ");
			}
			if (echo > 7) fprintf(stderr, "P: %s", buf);
	
			if (st.preamble) {
				if (st.newline) {
					st.preamble_nl = 1;
				}
				if (st.preamble_nl) {
					if (streq(buf, PATCH_CURRENT) ||
					    (Fast = streq(buf, PATCH_FAST))) {
						st.type = 1;
						st.preamble = 0;
						st.preamble_nl = 0;
					}
				}
			} else if (st.type) {
				if (streq(buf, PATCH_REGULAR)) {
					st.type = 0;
					st.version = 1;
				} else {
					fprintf(stderr, "Expected type\n");
					goto error;
				}
			} else if (st.version) {
				if (st.newline) {
					st.version = 0;
					st.versionblank = 1;
				} else {	/* false alarm */
					rewind(f);
					st.preamble = 1;
					st.version = 0;
				}
			} else if (st.versionblank) {
				if (strneq("== ", buf, 3)) {
					st.versionblank = 0;
					st.filename = 1;
					st.first = 1;
				} else {
					fprintf(stderr, "Expected '== f =='\n");
error:					fprintf(stderr, "GOT: %s", buf);
					cleanup(CLEAN_PENDING|CLEAN_RESYNC);
				}
			} else if (st.filename) {
				if (st.newline) {
					fprintf(stderr, "Expected metadata\n");
					goto error;
				} else if (strneq(buf, "New file: ", 10)) {
					st.perfile = 1;
				} else {
					st.metadata = 1;
				}
				st.filename = 0;
			} else if (st.perfile) {
				if (st.newline) {
					st.perfile = 0;
					st.perblank = 1;
				}
			} else if (st.perblank) {
				if (st.newline) {
					fprintf(stderr, "Expected metadata\n");
					goto error;
				}
				st.metadata = 1;
				st.perblank = 0;
			} else if (st.metadata) {
#define	DIVIDER	"------------------------------------------------\n"
				if (st.newline) {
					fprintf(stderr, "Expected metadata\n");
					goto error;
				} else if (streq(buf, DIVIDER)) {
					st.metadata = 0;
					st.metaline = 1;
				}
			} else if (st.metaline) {
				if (st.newline) {
					st.metaline = 0;
					st.metablank = 1;
				} else {
					fprintf(stderr, "Expected metablank\n");
					goto error;
				}
			} else if (st.metablank) {
				st.metablank = 0;
				if (st.newline) {	/* no diffs */
					st.diffsblank = 1;
				} else {
					st.diffs = 1;
				}
			} else if (st.diffs || st.sfiopatch) {
				if (st.newline) {
					st.sfiopatch = st.diffs = 0;
					st.diffsblank = 1;
				} else if (st.sfiopatch) {
					fprintf(stderr,
					    "Expected newline after @SFIO@\n");
					goto error;
				}
			} else if (st.diffsblank) {
				if (strneq("== @SFIO@ ==", buf, 12)) {
					sfiopatch = st.sfiopatch = 1;
					st.diffsblank = 0;
				} else if (strneq("== ", buf, 3)) {
					st.diffsblank = 0;
					st.filename = 1;
					st.first = 0;
				} else if (strneq(buf,
				    "# Patch checksum=", 17)) {
					sumR = strtoul(buf+17, 0, 16);
					assert(sumR != 0);
					fputs(buf, f);
					break;
				} else if (st.newline) {
					fprintf(stderr,
					    "Expected '== f ==' or key\n");
				    	goto error;
				} else {
					st.diffsblank = 0;
					st.metadata = 1;
				}
			}

			unless (st.preamble) {
				for (;;) {
					if (fputs(buf, f) == EOF) {
						perror("fputs on patch");
						cleanup(
						    CLEAN_PENDING|CLEAN_RESYNC);
					}
					len = strlen(buf);
					sumC = adler32(sumC, buf, len);
					if (strchr(buf, '\n')) break;
					unless (fnext(buf, stdin)) {
						perror("fnext");
						goto missing;
					}
				}
			}

			if (st.metablank && (echo == 3) && mkpatch) {
				fprintf(stderr, "%c\b", spin[j % 4]);
				j++;
			}
			unless (fnext(buf, stdin)) {
				perror("fnext");
				goto missing;
			}
		}
		if (st.preamble) errorMsg("tp_nothingtodo", 0, 0);
		if (sfiopatch) {
			while (len = fread(buf, 1, sizeof(buf), stdin)) {
				fwrite(buf, 1, len, f);
			}
		}
		if (fclose(f)) {
			perror("fclose on patch");
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
		strcpy(incoming, pendingFile);
		unless (savefile("PENDING", 0, pendingFile)) {
			SHOUT();
			perror("PENDING");
			cleanup(CLEAN_RESYNC);
		}
		note = aprintf("%u", (u32)size(incoming));
		cmdlog_addnote("psize", note);
		free(note);
		rename(incoming, pendingFile);
		if (echo) {
			if (mkpatch) fprintf(stderr, "done.\n");
			NOTICE();
			fprintf(stderr,
			    "takepatch: saved entire patch in %s\n",
			    pendingFile);
			NOTICE();
		}
		unless (m = mopen(pendingFile, "b")) {
			perror(pendingFile);
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
		unless (g = fopen("RESYNC/BitKeeper/tmp/patch", "w")) {
			perror("RESYNC/BitKeeper/tmp/patch");
			exit(1);
		}
		fprintf(g, "%s\n", pendingFile);
		fclose(g);
	} else {
		resync_lock();
		unless (m = mopen(inputFile, "b")) {
			perror(inputFile);
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
		unless (g = fopen("RESYNC/BitKeeper/tmp/patch", "w")) {
			perror("RESYNC/BitKeeper/tmp/patch");
			exit(1);
		}
		fprintf(g, "%s\n", inputFile);
		fclose(g);

		i = 0;
		while (t = mnext(m)) {
			if (strneq(t, PATCH_CURRENT, strsz(PATCH_CURRENT)) ||
			    (Fast = strneq(t, PATCH_FAST, strsz(PATCH_FAST)))) {
				len = linelen(t);
				sumC = adler32(sumC, t, len);
				t = mnext(m);
				i++;
				break;
			}
		}
		unless (i) errorMsg("tp_noversline", input, 0);
		do {
			len = linelen(t);
			sumC = adler32(sumC, t, len);
			unless (t = mnext(m)) break;
		} while (!strneq(t-2, "\n\n# Patch checksum=", 19));
		unless (t && strneq(t-2, "\n\n# Patch checksum=", 19)) {
			goto missing;
		}
		t = mkline(t);
		sumR = strtoul(t+17, 0, 16);
		assert(sumR != 0);
	}

	unless (sumR) {
missing:	
		SHOUT();
		fputs("takepatch: missing checksum line in patch, aborting.\n",
		      stderr);
		cleanup(CLEAN_PENDING|CLEAN_RESYNC);
	}
	if (sumR != sumC) badXsum(sumR, sumC);

	mseekto(m, 0);
	mnext(m);		/* skip version number */
	line = 1;

	return (m);
}

private	void
fileCopy2(char *from, char *to)
{
	if (fileCopy(from, to)) cleanup(CLEAN_RESYNC);
}

sccs *
sccs_unzip(sccs *s)
{
	s = sccs_restart(s);
	if (sccs_admin(s, 0,
	    SILENT|ADMIN_FORCE|NEWCKSUM, "none", 0, 0, 0, 0, 0, 0)) {
		return (0);
	}
	s = sccs_restart(s);
	return (s);
}

sccs *
sccs_gzip(sccs *s)
{
	s = sccs_restart(s);
	if (sccs_admin(s,
	    0, ADMIN_FORCE|NEWCKSUM, "gzip", 0, 0, 0, 0, 0, 0)) {
		sccs_whynot("admin", s);
	}
	s = sccs_restart(s);
	return (s);
}

private	void
cleanup(int what)
{
	int	i, rc = 1;

	if (patchList) freePatchList();
	if (idDB) mdbm_close(idDB);
	if (goneDB) mdbm_close(goneDB);
	if (saveDirs) {
		fprintf(stderr, "takepatch: neither directory removed.\n");
		goto done;
	}
	for (i = 3; i < 20; ++i) close(i);
	if (what & CLEAN_RESYNC) {
		rmtree(ROOT2RESYNC);
	} else {
		fprintf(stderr, "takepatch: RESYNC directory left intact.\n");
	}
	unless (streq(input, "-")) goto done;
	if (what & CLEAN_PENDING) {
		unlink(pendingFile);
		if (emptyDir("PENDING")) {
			rmdir("PENDING");
		} else {
			fprintf(stderr,
			    "takepatch: other patches left in PENDING\n");
		}
	} else {
		if (exists(pendingFile)) {
			fprintf(stderr, "takepatch: patch left in %s\n",
			    pendingFile);
		}
	}
done:
	if (what & CLEAN_OK) {
		rc = 0;
	} else {
		if (edited) {
			shout();
			fprintf(stderr,
			    "The following files are modified locally and "
			    "in the patch:\n");
			EACH(edited) fprintf(stderr, "\t%s\n", edited[i]);
			fprintf(stderr,
			    "For more information run \"bk help tp1\"\n");
		}
		SHOUT2();
		if (errfiles) {
			fprintf(stderr,
			    "Errors during update of the following files:\n");
			EACH(errfiles) fprintf(stderr, "%s\n", errfiles[i]);
			SHOUT2();
		}
	}
	exit(rc);
}
