/*
sccs_resolveFile() - the gcapath is s->gfile.  Add the error check is that
left||right->path == s->gfile.
*/

/* Copyright (c) 1999-2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "logging.h"
#include "zlib/zlib.h"
#include <time.h>
WHATSTR("@(#)%K%");

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
		    "=======================================\n\n", stderr);
#define	NOTICE() fputs("------------------------------------"\
		    "---------------------------------------\n", stderr);

private	delta	*getRecord(MMAP *f);
private	int	extractPatch(char *name, MMAP *p, int flags, project *proj);
private	int	extractDelta(char *name, sccs *s, int newFile, MMAP *f, int, int*);
private	int	applyPatch(char *local, int flags, sccs *perfile, project *p);
private	int	applyCsetPatch(char *localPath, int nfound, int flags,
						sccs *perfile, project *proj);
private	int	getLocals(sccs *s, delta *d, char *name);
private	void	insertPatch(patch *p);
private	void	reversePatch(void);
private	void	initProject(void);
private	MMAP	*init(char *file, int flags, project **p);
private	int	rebuild_id(char *id);
private	void	cleanup(int what);
private	void	changesetExists(void);
private	void	notfirst(void);
private	void	goneError(char *key);
private	void	freePatchList(void);
private	void	fileCopy2(char *from, char *to);
private	void	badpath(sccs *s, delta *tot);
private void	merge(char *gfile);
private	int	skipPatch(MMAP *p);
private	void	getConfig(void);
private	void	getGone(int isLogPatch);
private void	metaUnion(void);
private void	metaUnionFile(char *file, char *cmd);
private void	metaUnionResyncFile(char *from, char *to);
private	void	loadskips(void);

private	int	isLogPatch = 0;	/* is a logging patch */
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
private	int	compat;		/* we are eating a compat patch, fail on tags */

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
	int	flags = SILENT;
	int	files = 0;
	char	*t, *q;
	int	error = 0;
	int	remote = 0;
	int	resolve = 0;
	project	*proj = 0;
	int	textOnly = 0;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help takepatch");
		return (0);
	}

	setmode(0, O_BINARY); /* for win32 */
	input = "-";
	debug_main(av);
	while ((c = getopt(ac, av, "acFf:iLmqsStv")) != -1) {
		switch (c) {
		    case 'q':					/* undoc 2.0 */
		    case 's':					/* undoc 2.0 */
			/* undoc, Ignored for option consistency.  */
			break;
		    case 'a': resolve++; break;			/* doc 2.0 */
		    case 'c': noConflicts++; break;		/* doc 2.0 */
		    case 'F': break;				/* obsolete */
		    case 'f':					/* doc 2.0 */
			    input = optarg;
			    break;
		    case 'i': newProject++; break;		/* doc 2.0 */
		    case 'L': isLogPatch = 1; break;
		    case 'm': mkpatch++; break;			/* doc 2.0 */
		    case 'S': saveDirs++; break;		/* doc 2.0 */
		    case 't': textOnly++; break;		/* doc 2.0 */
		    case 'v': echo++; flags &= ~SILENT; break;	/* doc 2.0 */
		    default: goto usage;
		}
	}
	if (getenv("TAKEPATCH_SAVEDIRS")) saveDirs++;
	if (av[optind]) {
usage:		system("bk help -s takepatch");
		return (1);
	}

	p = init(input, flags, &proj);

	if (streq(input, "-") && isLogPatch) {
		if (newProject) {
			/*
			 * We will handle creating new logging repos
			 * directly, so we won't be needing the .env
			 * file.
			 */
			char	*env = aprintf("%s.env", pendingFile);
			unlink(env);
			free(env);
		} else {
			char	*applyall[] = {"bk", "_applyall", 0};

			mclose(p);
			mdbm_close(goneDB);
			mdbm_close(idDB);

			spawnvp_ex(_P_NOWAIT, "bk", applyall);
			return(0);
		}
	}

	if (newProject) {
		unless (idDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) {
			perror("mdbm_open");
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
	} else {
		/*
		 * before loading up special dbs, update gfile is
		 * logging tree
		 */
		if (isLogPatch) metaUnion();

		/* OK if this returns NULL */
		goneDB = loadDB(GONE, 0, DB_KEYSONLY|DB_NODUPS);

		loadskips();

		unless (idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
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
		/* we need our own storage , extractPatch calls mkline */
		b = strdup(mkline(buf));
		if (echo>4) fprintf(stderr, "%s\n", b);
		unless (strncmp(b, "== ", 3) == 0) {
			if (echo > 7) {
				fprintf(stderr, "skipping: %s\n", b);
			}
			free(b);
			continue;
		}
		unless ((t = strrchr(b, ' ')) && streq(t, " ==")) {
			SHOUT();
			fprintf(stderr, "Bad patch: %s\n", b);
			cleanup(CLEAN_RESYNC);
		}
		*t = 0;
		files++;
		rc = extractPatch(&b[3], p, flags, proj);
		free(b);
		if (rc < 0) {
			error = rc;
			continue;
		}
		remote += rc;
	}
	mclose(p);
	proj_free(proj);
	if (idDB) { mdbm_close(idDB); idDB = 0; }
	if (goneDB) { mdbm_close(goneDB); goneDB = 0; }
	if (error < 0) {
		/* XXX: Save?  Purge? */
		cleanup(CLEAN_RESYNC);
	}
	if (echo) {
		fprintf(stderr,
		    "takepatch: %d new revision%s, %d conflicts in %d files\n",
		    remote, remote == 1 ? "" : "s", conflicts, files);
	}

	/* save byte count for logs */
	f = fopen("BitKeeper/log/byte_count", "w");
	if (f) {
		fprintf(f, "%lu\n", size(pendingFile));
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

	getConfig();

	/*
	 * The ideas here are to (a) automerge any hash-like files which
	 * we maintain, and (b) converge on the oldest inode for a
	 * particular file.  The converge code will make sure all of the
	 * inodes are present.
	 */
	if (conflicts && !isLogPatch) {
		char key[MAXKEY], gfile[MAXPATH];
		chdir(ROOT2RESYNC);
		f = popen("bk sfiles BitKeeper/etc BitKeeper/deleted | "
			  "bk prs -r+ -hd':ROOTKEY:\n:GFILE:\n' -", "r");
		assert(f);
		while (fnext(key, f))  {
			q = strchr(key, '|') + 1;
			t = strchr(q, '|'); *t = 0;
			fnext(gfile, f);
			unless (streq(q, "BitKeeper/etc/gone") ||
				streq(q, "BitKeeper/etc/ignore") ||
				streq(q, "BitKeeper/etc/skipkeys") ||
				streq(q, "BitKeeper/etc/logging_ok")) {
				continue;
			}
			chop(gfile);
			merge(gfile);
		}
		pclose(f);
		system("bk _converge -R");
		chdir(RESYNC2ROOT);
	}

	getGone(isLogPatch); /* 
		    * We need the Gone file even for no conflict case
		    * Because user may have deleted the sfile in the
		    * local tree,
		    */

	if (resolve) {
		char 	*resolve[7] = {"bk", "resolve", "-q", 0, 0, 0, 0};
		int 	i;

		if (echo) {
			fprintf(stderr,
			    "Running resolve to apply new work...\n");
		}
		i = 2;
		if (!echo) resolve[++i] = "-q";
		if (textOnly) resolve[++i] = "-t";
		if (noConflicts) resolve[++i] = "-c";
		i = spawnvp_ex(_P_WAIT, resolve[0], resolve);
		unless (WIFEXITED(i)) return (-1);
		error = WEXITSTATUS(i);
	}
	exit(error);
}

/*
 * It's probably an error to move the config file, but just in case
 * they did, if there is no config file, use the contents of the
 * enclosing repository.
 */
private void
getConfig(void)
{
	chdir(ROOT2RESYNC);
	unless (exists("BitKeeper/etc/SCCS/s.config")) {
		assert(exists(RESYNC2ROOT "/BitKeeper/etc/SCCS/s.config"));
		system("bk get -kqp " RESYNC2ROOT
		    "/BitKeeper/etc/SCCS/s.config > BitKeeper/etc/config");
	}
	chdir(RESYNC2ROOT);
}

private void
getGone(int isLogPatch)
{
	if (isLogPatch) {
		chdir(ROOT2RESYNC);
		metaUnionFile(GONE, "bk meta_union gone");
		chdir(RESYNC2ROOT);
		metaUnionResyncFile(GONE, "RESYNC/" GONE);
	}
	/* XXX - if this is edited, we don't get those changes */
	if (exists("BitKeeper/etc/SCCS/s.gone")) {
		unless (exists("RESYNC/BitKeeper/etc/SCCS/s.gone")) {
			system("cp BitKeeper/etc/SCCS/s.gone "
			    "RESYNC/BitKeeper/etc/SCCS/s.gone");
		} 
    	}
}

/*
 * Automerge any updates before converging the inodes.
 */
private void
merge(char *gfile)
{
	char	*s, l[200], g[200], r[200];
	char	*sfile = name2sccs(gfile);
	char	*rfile = name2sccs(gfile);
	char	*mfile = name2sccs(gfile);
	char	*t, buf[MAXPATH];

	t = strrchr(rfile, '/'), t[1] = 'r';
	t = strrchr(mfile, '/'), t[1] = 'm';
	unlink(mfile);
	free(mfile);
	if (exists(rfile)) {
		FILE	*f;

		/*
		 * Both remote and local have updated the file.
		 * We automerge here, saves trouble later.
		 */
		f = fopen(rfile, "r");
		fscanf(f, "merge deltas %s %s %s", l, g, r);
		fclose(f);
		s = strchr(l, '.'); s++;
		s = strchr(s, '.');
#define	TMP	"BitKeeper/tmp/CONTENTS"
		sprintf(buf, "bk get -eqgM%s %s", s ? l : r, gfile);
		system(buf);
		sprintf(buf, "bk get -qpr%s %s > %s", l, gfile, TMP);
		system(buf);
		sprintf(buf, "bk get -qpr%s %s >> %s", r, gfile, TMP);
		system(buf);
		sprintf(buf, "bk _sort -u < %s > %s", TMP, gfile);
		system(buf);
		sprintf(buf, "bk ci -qdPyauto-union %s", gfile);
		system(buf);
		unlink(rfile);
	} /* else remote update only */
	free(sfile);
	free(rfile);
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
extractPatch(char *name, MMAP *p, int flags, project *proj)
{
	delta	*tmp;
	sccs	*s = 0;
	sccs	*perfile = 0;
	int	newFile = 0;
	int	reallyNew = 0;
	char	*gfile = 0;
	int	nfound = 0, rc;
	static	int rebuilt = 0;	/* static - do it once only */
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
		if (isLogPatch && perfile && perfile->defbranch) {
			free(perfile->defbranch);
			perfile->defbranch = 0;
		}
		t = mkline(mnext(p));
		line++;
	}
	if (strneq("Grafted file: ", t, 14)) {
		t = mkline(mnext(p));
		line++;
	}
	if (newProject && !newFile) notfirst();

	if (echo>4) fprintf(stderr, "%s\n", t);
again:	s = sccs_keyinit(t, SILENT|INIT_NOCKSUM|INIT_SAVEPROJ, proj, idDB);
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
			if (isLogPatch || getenv("BK_GONE_OK")) {
				goto skip;
			} else {
				goneError(t);
			}
		}
		unless (rebuilt++) {
			if (rebuild_id(t)) {
				fprintf(stderr,
				    "ID cache problem causes abort.\n");
				if (perfile) sccs_free(perfile);
				if (gfile) free(gfile);
				if (s) sccs_free(s);
				free(name);
				cleanup(CLEAN_RESYNC);
			}
			goto again;
		}
		if (!newFile && isLogPatch) {
	skip:		skipPatch(p);
			return (0);
		}
		unless (newFile) {
			SHOUT();
			fprintf(stderr,
			   "takepatch: can't find key '%s' in id cache\n", t);
error:			if (perfile) sccs_free(perfile);
			if (gfile) free(gfile);
			if (s) sccs_free(s);
			free(name);
			return (-1);
		}
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
		if (IS_EDITED(s)) {
			int cleanflags = SILENT|CLEAN_SHUTUP|CLEAN_CHECKONLY;

			if (isLogPatch) cleanflags |= CLEAN_SKIPPATH;
			if (sccs_clean(s, cleanflags)) {
				shout();
				fprintf(stderr,
				    "takepatch: %s is edited and modified; "
				    "unsafe to overwrite.\n",
				    name);
				goto error;
			} else {
				sccs_restart(s);
			}
		} else if (s->state & S_PFILE) {
			shout();
			fprintf(stderr,
			    "takepatch: %s is locked w/o writeable gfile?\n",
			    s->sfile);
			goto error;
		}
		sccs_setpathname(s);
		unless (isLogPatch || streq(s->spathname, s->sfile)) {
			tmp = sccs_top(s);
			badpath(s, tmp);
			goto error;
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
			changesetExists();
		}
		if (echo > 3) {
			fprintf(stderr,
			    "takepatch: new file %s\n", t);
		}
	}
	tableGCA = 0;
	encoding = s ? s->encoding : 0;
	while (extractDelta(name, s, newFile, p, flags, &nfound)) {
		if (newFile) newFile = 2;
	}
	gfile = sccs2name(name);
	if (echo>1) {
		fprintf(stderr, "Applying %3d revisions to %s%s ",
		    nfound, reallyNew ? "new file " : "", gfile);
		if ((echo != 2) && (echo != 3)) fprintf(stderr, "\n");
	}
	if ((s && CSET(s)) || (!s && streq(name, CHANGESET))) {
		rc = applyCsetPatch(s ? s->sfile : 0 ,
						nfound, flags, perfile, proj);
	} else {
		if (patchList && tableGCA) getLocals(s, tableGCA, name);
		rc = applyPatch(s ? s->sfile : 0, flags, perfile, proj);
	}
	if (echo == 2) fprintf(stderr, "\n");
	if (echo == 3) fprintf(stderr, " \n");
	if (perfile) sccs_free(perfile);
	free(gfile);
	free(name);
	if (s) {
		s->proj = 0;
		sccs_free(s);
	}
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
/*
 * MetaUnion is a set of functions to aid the logging tree
 * to have a useful gfile of 'gone' and 'skipkeys' available
 * at the right time.  skipkeys is only used by takepatch,
 * and so needs to be correct before takepatch starts.
 * gone is used by checking and needs to be correct in
 * both the takepatch and resolve side of things.
 * there was already a function getGone() to do the setup
 * of the gone file for resolve.
 *
 * In resolve, modifications where to the pass4_apply function
 * to get a good copy of the files out of the resync directory
 * but because of clean() being called a number of times on a
 * file, it couldn't really be put into place.  At the end of
 * pass4_apply(), a call is made into here (metaUnionResync2())
 * to move the files into place.
 *
 * the whole code path, with skipkeys and all is a hack to help
 * out in switching lod design and make the logging tree work better.
 *
 * This used to also do 'skipkeys', but was that was removed because
 * new non 1.x deltas still caused problems in logging tree.
 *
 */
#define METAUNIONHEAD  "# Automatically generated by BK.  Do not edit.\n"
private void
metaUnionFile(char *file, char *cmd)
{
	FILE	*f, *w;
	char	buf[2 * MAXKEY];
	int	ok;

	ok = 0;
	if (exists(file)) {
		f = fopen(file, "rt");
		assert(f);
		if (fnext(buf, f)) {
			if (buf[0] == '#') ok = 1;
		}
		fclose(f);
	}
	unless (ok) {
		unlink(file);
		w = fopen(file, "wb");
		assert(w);
		fputs(METAUNIONHEAD, w);
		f = popen(cmd, "r");
		while (fnext(buf, f)) {
			if (buf[0] == '#') continue;
			fputs(buf, w);
		}
		fclose(w);
		fclose(f);
		chmod(file, 0444);
	}
}

#define SKIPKEYS	"BitKeeper/etc/skipkeys"

private void
metaUnion(void)
{
	metaUnionFile(GONE, "bk meta_union gone");
}

private void
metaUnionResyncFile(char *from, char *to)
{
	char	temp[MAXPATH];
	char	buf[MAXLINE];
	FILE	*f, *w;

	unless (exists(from)) return;

	unless (exists(to)) {
		sprintf(buf, "cp %s %s", from, to);
		system(buf);
		chmod(to, 0444);
		return;
	}

	sprintf(temp, "%s.cp", to);
	unlink(temp);
	sprintf(buf, "cat %s %s | bk sort -u", from, to);
	w = fopen(temp, "wb");
	f = popen(buf, "r");
	fputs(METAUNIONHEAD, w);
	while (fnext(buf, f)) {
		if (buf[0] == '#') continue;
		fputs(buf, w);
	}
	fclose(f);
	fclose(w);
	chmod(temp, 0444);
	unlink(to);
	sprintf(buf, "cp %s %s", temp, to);
	system(buf);
}

private void
metaUnionCopy(char *file)
{
	char	temp[MAXPATH];

	sprintf(temp, "%s.cp", file);
	unless (exists(temp)) return;
	unlink(file);
	rename(temp, file);
}

void
metaUnionResync1(void)
{
	metaUnion();
	chdir(RESYNC2ROOT);
	metaUnionResyncFile("RESYNC/" GONE, GONE);
	chdir(ROOT2RESYNC);
}

void
metaUnionResync2(void)
{
	metaUnionCopy(GONE);
}

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

	if (isLogPatch) return;
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
			s = calloc(1, sizeof(*s));
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

	unless (s && skiplist && !isLogPatch) return (0);
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
extractDelta(char *name, sccs *s, int newFile, MMAP *f, int flags, int *np)
{
	delta	*d, *parent = 0, *tmp;
	char	buf[MAXPATH];
	char	*b;
	char	*pid = 0;
	long	off;
	int	c;
	int	skip = 0;
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
	if (tmp = sccs_findKey(s, buf)) {
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
	mseekto(f, off);
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
		p = calloc(1, sizeof(patch));
		p->remote = 1;
		p->pid = pid;
		sccs_sdelta(s, d, buf);
		p->me = strdup(buf);
		p->initMmap = mrange(start, stop, "b");
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
		if (d->flags & D_META) {
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

private	void
changesetExists(void)
{
	SHOUT();
	fputs(
"You are trying to create a ChangeSet file in a repository which already has\n\
one.  This usually means you are trying to apply a patch intended for a\n\
different repository.  You can find the correct repository by running the\n\
following command at the top of each repository until you get a match with\n\
the changeset ID at the top of the patch:\n\
    bk prs -hr1.0 -d':KEY:\\n' ChangeSet\n\n", stderr);
    	cleanup(CLEAN_RESYNC|CLEAN_PENDING);
}

private	void
goneError(char *buf)
{
	SHOUT();
	fprintf(stderr,
"File %s\n\
is marked as gone in this repository and therefor cannot accept updates.\n\
The fact that you are getting updates indicates that the file is not gone\n\
in the other repository and could be restored in this repository.\n\
if you want to \"un-gone\" the file(s) using the s.file from a remote\n\
repository, try \"bk repair <remote repository>\"\n", buf);
	cleanup(CLEAN_PENDING|CLEAN_RESYNC);
}

private	void
noconflicts(void)
{
	SHOUT();
	fputs(
"takepatch was instructed not to accept conflicts into this tree.\n\
Please make sure all pending deltas are committed in this tree,\n\
resync in the opposite direction and then reapply this patch.\n",
stderr);
	cleanup(CLEAN_PENDING|CLEAN_RESYNC);
}

private	void
notfirst(void)
{
	SHOUT();
	fputs(
"takepatch: when creating a package, as you are currently doing, you have\n\
to resync from version 1.0 forward.  Please try again, with a command like\n\
\n\
\tbk resync -r1.0.. from to\n\
or\n\
\tbk send -r1.0.. user@host.com\n\
\n\
takepatch has not cleaned up your destination, you need to do that.\n",
stderr);
	cleanup(CLEAN_RESYNC|CLEAN_PENDING);
}

private	void
ahead(char *pid, char *sfile)
{
	SHOUT();
	fprintf(stderr,
	    "takepatch: can't find parent ID\n\t%s\n\tin %s\n", pid, sfile);
	fputs(
"This patch is ahead of your tree, you need to get an earlier patch first.\n\
Look at your tree with a ``bk changes'' and do the same on the other tree,\n\
and get a patch that is based on a common ancestor.\n", stderr);
	cleanup(CLEAN_RESYNC|CLEAN_PENDING);
}

private	void
badpath(sccs *s, delta *tot)
{
	SHOUT();
	fprintf(stderr, "takepatch: file %s%c%s has %s as recorded pathname\n",
	    s->gfile, BK_FS, tot->rev, tot->pathname);
	fputs(
"This file is not where BitKeeper thinks it should be.  If the file is in\n\
what you consider to be the right place, update it's name with the following\n\
command:\n\tbk names <filename>\n\n\
and retry the patch.  The patch has been saved in the PENDING directory\n",
stderr);
}

private	void
nothingtodo(void)
{
	SHOUT();
	fprintf(stderr,
"takepatch: nothing to do in patch, which probably means a patch version\n\
mismatch.  You need to make sure that the software generating the patch is\n\
the same as the software accepting the patch.  We were looking for\n\
%s", PATCH_CURRENT);
	if (exists("RESYNC") && exists("PENDING")) {
		cleanup(CLEAN_PENDING|CLEAN_RESYNC);
	} else if (exists("PENDING")) {
		cleanup(CLEAN_PENDING);
	}
}

private	void
noversline(char *name)
{
	SHOUT();
	fprintf(stderr,
"takepatch: the version line on %s is missing or does not match the\n\
format understood by this program.\n\
You need to make sure that the software generating the patch is\n\
the same as the software accepting the patch.  We were looking for\n\
%s", name, PATCH_CURRENT);
	cleanup(CLEAN_PENDING|CLEAN_RESYNC);
}

private	void
badXsum(int a, int b)
{
	SHOUT();
	fputs("takepatch: patch checksum is invalid", stderr);
	if (echo > 3) fprintf(stderr,  " (%x != %x)", a, b);
	fputs(".\nThe patch was probably corrupted in transit,\n", stderr);
	fputs("sometimes mailers do this.\n", stderr);
	fputs("Please get a new copy and try again.\n", stderr);
	cleanup(CLEAN_PENDING|CLEAN_RESYNC);
	/* XXX - should clean up everything if this was takepatch -i */
}

private	void
uncommitted(char *file)
{
	SHOUT();
	fprintf(stderr,
"takepatch: %s has uncommitted changes\n\
Please commit pending changes with `bk commit' and reapply the patch.\n",
		file);
}

/*
 * Make sure user files have no content
 * This function is called we process a logging patch
 */
private int
chkEmpty(sccs *s, MMAP *dF)
{
	char 	*p, root[MAXPATH];
	int	len;

	
	unless (dF && (dF->size > 0)) return (0);
	if (s->state & S_CSET) return (0);

	/*
	 * Two special case:
	 * a) s->tree is null. This happen when we get a brand new file.
	 * b) In some old repository, (e.g the BitKeeper tree), 
	 *    pathname is not recorded in some old delta; 
	 *    i.e. s->tree->patname is null. This should not happen in new tree.
	 */
	if (s->tree && s->tree->pathname) {
		strcpy(root, "BitKeeper/");
		p = s->tree->pathname; /* we want the root path */
	} else {
		strcpy(root, "RESYNC/BitKeeper/");
		assert(s->sfile);
		p = s->sfile;
	}

	len = strlen(root);
	if ((strlen(p) > len) && !strneq(p, root, len)) {
		fprintf(stderr,
		    "Logging patch should not have source content\n");
		return (1); /* failed */
	}
	return (0); /* ok */
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
applyCsetPatch(char *localPath,
			int nfound, int flags, sccs *perfile, project *proj)
{
	patch	*p;
	MMAP	*iF;
	MMAP	*dF;
	sccs	*s = 0;
	delta	*d = 0;
	int	n = 0;
	int	confThisFile;
	FILE	*csets = 0;
	char	csets_in[MAXPATH];

	reversePatch();
	unless (p = patchList) return (0);

	if (echo == 3) fprintf(stderr, "%c\b", spin[n++ % 4]);
	if (echo > 7) {
		fprintf(stderr, "L=%s\nR=%s\nP=%s\nM=%s\n",
		    p->localFile, p->resyncFile, p->pid, p->me);
	}
	unless (localPath) {
		if (mkdirf(p->resyncFile)) {
			perror(p->resyncFile);
		}
		goto apply;
	}
	/* NOTE: the "goto" above .. skip the next block if no ChangeSet */

	fileCopy2(localPath, p->resyncFile);
	/* Open up the cset file */
	unless (s = sccs_init(p->resyncFile, INIT_NOCKSUM|flags, proj)) {
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
	if (isLogPatch) {
		unless (LOGS_ONLY(s)) {
			fprintf(stderr,
	        		"takepatch: can't apply a logging "
				"patch to a regular file %s\n",
				p->resyncFile);
			sccs_free(s);
			return -1;
		}
	} else {
		if (LOGS_ONLY(s)) {
			fprintf(stderr,
	        		"takepatch: can't apply a regular "
				"patch to a logging file %s\n",
				p->resyncFile);
			sccs_free(s);
			return -1;
		}
	}
apply:
	p = patchList;
	if (p && p->pid) cweave_init(s, nfound);
	while (p) {
		if (echo == 3) fprintf(stderr, "%c\b", spin[n % 4]);
		n++;
		if (p->pid) {
			assert(s);
			if (echo>9) {
				fprintf(stderr,
				    "PID: %s\nME:  %s\n", p->pid, p->me);
			}
			unless (d = sccs_findKey(s, p->pid)) {
				if ((echo == 2) || (echo == 3)) {
					fprintf(stderr, " \n");
				}
				ahead(p->pid, s->sfile);
				/* Can not reach */
			}
			if (isLogPatch) {
				s->state |= S_FORCELOGGING;
				s->xflags |= X_LOGS_ONLY;
			}
			if (echo>9) {
				fprintf(stderr, "Child of %s", d->rev);
				if (p->meta) {
					fprintf(stderr, " meta\n");
				} else {
					fprintf(stderr, " data\n");
				}
			}
			iF = p->initMmap;
			dF = p->diffMmap;
			if (isLogPatch && chkEmpty(s, dF)) return -1;
			d = cset_insert(s, iF, dF, p->pid);
		} else {
			assert(s == 0);
			unless (s =
			    sccs_init(p->resyncFile, NEWFILE|SILENT, proj)) {
				SHOUT();
				fprintf(stderr,
				    "takepatch: can't create %s\n",
				    p->resyncFile);
				return -1;
			}
			if (perfile) {
				sccscopy(s, perfile);
				/*
				 * For takepatch performance
				 * turn off compression when we are in 
				 * takepatch.
				 * 
				 * Note: Since this is a new file from remote,
				 * there is no local setting. We save the 
				 * compression setting of the remote file
				 * and use that as the new local file when
				 * when takepatch is done.
				 */
			}
			iF = p->initMmap;
			dF = p->diffMmap;
			if (isLogPatch) {
				s->state |= S_FORCELOGGING;
				s->xflags |= X_LOGS_ONLY;
				if (chkEmpty(s, dF)) return -1;
			}
			cweave_init(s, nfound);
			d = cset_insert(s, iF, dF, p->pid);
			s->bitkeeper = 1;
		}
		/* LOD logging tree fix: All on LOD 1, renumber() will fix */
		if (isLogPatch && d && !streq(d->rev, "1.0")
		    && !streq(d->rev, "1.1")) {
			free(d->rev);
			d->rev = strdup("1.2");
			d->r[0] = 1;
			d->r[1] = 2;
			d->r[2] = 0;
			d->r[3] = 0;
		}
		p = p->next;
	}
	if (cset_write(s)) {
		SHOUT();
		fprintf(stderr, "takepatch: can't write %s\n", p->resyncFile);
		return -1;
	}

	s->proj = 0; sccs_free(s);
	/*
	 * Fix up d->rev, there is probaply a better way to do this.
	 * XXX: not only renumbers, but collapses inheritance on
	 * items brought in from patch.  Could call inherit.
	 * For now, leave at this and watch performance.
	 */
	sys("bk", "renumber", "-q", patchList->resyncFile, SYS);

	s = sccs_init(patchList->resyncFile, SILENT, proj);
	assert(s && s->tree);

	unless (sccs_findKeyDB(s, 0)) {
		assert("takepatch: could not build findKeyDB" == 0);
	}

	p = patchList;
	sprintf(csets_in, "%s/%s", ROOT2RESYNC, CSETS_IN);
	csets = fopen(csets_in, "w");
	assert(csets);
	while (p) {
		delta	*d;

		d = sccs_findKey(s, p->me);
		assert(d);
		unless (p->meta) fprintf(csets, "%s\n", d->rev);
		/*
		 * XXX: mclose take a null arg?
		 * meta doesn't have a diff block
		 */
		mclose(p->diffMmap); /* win32: must mclose after cset_write */
		p = p->next;
	}
	fclose(csets);

	/*
	 * XXX What does this code do ??
	 * this code seems to be setup the D_LOCAL ANd D_REMOTE flags 
	 * used in sccs_resolveFiles()
	 */
	for (d = s->table; d; d = d->next) d->flags |= D_LOCAL;
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
		d->flags |= p->local ? D_LOCAL : D_REMOTE;
	}

	if ((confThisFile = sccs_resolveFiles(s)) < 0) {
		s->proj = 0; sccs_free(s);
		return (-1);
	}
	if (!confThisFile && (s->state & S_CSET) && 
	    sccs_admin(s, 0, SILENT|ADMIN_BK, 0, 0, 0, 0, 0, 0, 0, 0)) {
	    	confThisFile++;
		/* yeah, the count is slightly off if there were conflicts */
	}
	conflicts += confThisFile;
	s->proj = 0; sccs_free(s);
	if (noConflicts && conflicts) noconflicts();
	freePatchList();
	patchList = 0;
	fileNum = 0;
	return (0);
}

/*
 * If the destination file does not exist, just apply the patches to create
 * the new file.
 * If the file does exist, copy it, rip out the old stuff, and then apply
 * the list.
 */
private	int
applyPatch(char *localPath, int flags, sccs *perfile, project *proj)
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
	FILE	*csets = 0;

	reversePatch();
	p = patchList;
	if (!p && localPath) {
		/* 
		 * an existing file that was in the patch but didn't
		 * get any deltas.  Usually an error, but we should
		 * handle this better.
		 */
		char    *resync = aprintf("RESYNC/%s", localPath);
		int	i = 0;
		
		while (exists(resync)) {
                  	free(resync);
			resync = aprintf("RESYNC/BitKeeper/RENAMES/s.%d", i++);
		}
		fileCopy2(localPath, resync);
		free(resync);
		return (0);
	}
	if (echo == 3) fprintf(stderr, "%c\b", spin[n++ % 4]);
	if (echo > 7) {
		fprintf(stderr, "L=%s\nR=%s\nP=%s\nM=%s\n",
		    p->localFile, p->resyncFile, p->pid, p->me);
	}
	unless (localPath) {
		if (mkdirf(p->resyncFile) == -1) {
			if (errno == EINVAL) {
				getMsg(
				    "reserved", p->resyncFile, 0, '=', stderr);
				return (-1);
			}
		}
		goto apply;
	}
	fileCopy2(localPath, p->resyncFile);
	unless (s = sccs_init(p->resyncFile, INIT_NOCKSUM|flags, proj)) {
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
	if (isLogPatch) {
		unless (LOGS_ONLY(s)) {
			fprintf(stderr,
	        		"takepatch: can't apply a logging "
				"patch to a regular file %s\n",
				p->resyncFile);
			sccs_free(s);
			return -1;
		}
	} else {
		if (LOGS_ONLY(s)) {
			fprintf(stderr,
	        		"takepatch: can't apply a regular "
				"patch to a logging file %s\n",
				p->resyncFile);
			sccs_free(s);
			return -1;
		}
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
	if (d = sccs_next(s, sccs_getrev(s, tableGCA->rev, 0, 0))) {
		delta	*e;

		for (e = s->table; e; e = e->next) {
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
	s->proj = 0; sccs_free(s);
	/* sccs_restart does not rebuild the graph and we just pruned it,
	 * so do a hard restart.
	 */
	unless (s = sccs_init(p->resyncFile, INIT_NOCKSUM|flags, proj)) {
		SHOUT();
		fprintf(stderr,
		    "takepatch: can't open %s\n", p->resyncFile);
		return -1;
	}
apply:

	p = patchList;
	while (p) {
		if (echo == 3) fprintf(stderr, "%c\b", spin[n % 4]);
		n++;
		if (p->pid) {
			assert(s);
			if (echo>9) {
				fprintf(stderr,
				    "PID: %s\nME:  %s\n", p->pid, p->me);
			}
			unless (d = sccs_findKey(s, p->pid)) {
				if ((echo == 2) || (echo == 3)) {
					fprintf(stderr, " \n");
				}
				ahead(p->pid, s->sfile);
			}
			unless (sccs_restart(s)) { perror("restart"); exit(1); }
			if (isLogPatch) {
				s->state |= S_FORCELOGGING;
				s->xflags |= X_LOGS_ONLY;
			}
			if (echo>9) {
				fprintf(stderr, "Child of %s", d->rev);
				if (p->meta) {
					fprintf(stderr, " meta\n");
				} else {
					fprintf(stderr, " data\n");
				}
			}
			if (p->meta) {
				MMAP	*m = p->initMmap;

				unless (m) m = mopen(p->initFile, "b");
				if (sccs_meta(s, d, m, 0)) {
					unless (s->io_error) perror("meta");
					return -1;
				}
			} else {
				newflags = GET_FORCE|GET_SKIPGET|GET_EDIT;
				unless (echo > 6) newflags |= SILENT;
				/* CSTYLED */
				if (sccs_get(s, d->rev, 0,0,0, newflags, "-")) {
				    	perror("get");
					return -1;
				}
				sccs_restart(s);
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
				if (isLogPatch && chkEmpty(s, dF)) return -1;
				newflags = 
				    DELTA_FORCE|DELTA_PATCH|DELTA_NOPENDING;
				if (echo <= 3) newflags |= SILENT;
				if (sccs_delta(s, newflags, 0, iF, dF, 0)) {
					unless (s->io_error) perror("delta");
					return -1;
				}
				if (s->bad_dsum || s->io_error) return -1;
				mclose(iF);
				if ((s->state & S_CSET) && !p->local) {
					delta	*d = sccs_findKey(s, p->me);

					assert(d);
					unless (csets) {
						char csets_in[MAXPATH];

						sprintf(csets_in, "%s/%s",
							ROOT2RESYNC, CSETS_IN);
						csets = fopen(csets_in, "w");
						assert(csets);
					}
					fprintf(csets, "%s\n", d->rev);
				}
			}
		} else {
			assert(s == 0);
			unless (s =
			    sccs_init(p->resyncFile, NEWFILE|SILENT, proj)) {
				SHOUT();
				fprintf(stderr,
				    "takepatch: can't create %s\n",
				    p->resyncFile);
				return -1;
			}
			if (perfile) {
				sccscopy(s, perfile);
				/*
				 * For takepatch performance
				 * turn off compression when we are in 
				 * takepatch.
				 * 
				 * Note: Since this is a new file from remote,
				 * there is no local setting. We save the 
				 * compression setting of the remote file
				 * and use that as the new local file when
				 * when takepatch is done.
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
			if (isLogPatch) {
				s->state |= S_FORCELOGGING;
				s->xflags |= X_LOGS_ONLY;
				if (chkEmpty(s, dF)) return -1;
			}
			d = 0;
			newflags = 
			    NEWFILE|DELTA_FORCE|DELTA_PATCH|DELTA_NOPENDING;
			if (echo <= 3) newflags |= SILENT;
			if (sccs_delta(s, newflags, d, iF, dF, 0)) {
				unless (s->io_error) perror("delta");
				return -1;
			}
			if (s->bad_dsum || s->io_error) return (-1);
			mclose(iF);	/* dF done by delta() */
			s->proj = 0; sccs_free(s);
			s = sccs_init(p->resyncFile, INIT_NOCKSUM|SILENT, proj);
		}
		p = p->next;
	}

	if (csets) {
		fclose(csets);
	}
	s->proj = 0; sccs_free(s);
	s = sccs_init(patchList->resyncFile, SILENT, proj);
	assert(s);
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
		d->flags |= p->local ? D_LOCAL : D_REMOTE;
		if (s->state & S_CSET) continue;
		if (sccs_isleaf(s, d) && !(d->flags & D_CSET)) pending++;
	}
	if (pending) {
		s->proj = 0; sccs_free(s);
		uncommitted(localPath);
		return -1;
	}
	if ((confThisFile = sccs_resolveFiles(s)) < 0) {
		s->proj = 0; sccs_free(s);
		return (-1);
	}
	if (!confThisFile && (s->state & S_CSET) && 
	    sccs_admin(s, 0, SILENT|ADMIN_BK, 0, 0, 0, 0, 0, 0, 0, 0)) {
	    	confThisFile++;
		/* yeah, the count is slightly off if there were conflicts */
	}
	conflicts += confThisFile;
	s->proj = 0; sccs_free(s);
	if (noConflicts && conflicts) noconflicts();
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
	for (d = s->table; d != g; d = d->next) {
		/*
		 * Silently discard removed deltas, we don't support them.
		 */
		if ((d->type == 'R') && !(d->flags & D_META)) continue;

		assert(d);
		sprintf(tmpf, "RESYNC/BitKeeper/tmp/%03d-init", ++fileNum);
		unless (t = fopen(tmpf, "wb")) {
			perror(tmpf);
			exit(1);
		}
		sccs_restart(s);
		s->rstart = s->rstop = d;
		sccs_prs(s, PRS_PATCH|PRS_LOGMARK|SILENT, 0, NULL, t);
		if (ferror(t)) {
			perror("error on init file");
			cleanup(CLEAN_RESYNC);
		}
		fclose(t);

		p = calloc(1, sizeof(patch));
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
		sccs_sdelta(s, d->parent, tmpf);
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
freePatchList()
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
initProject()
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
	 * Note: we need the real mkdir, not the so called smart one, we need
	 * to fail if it exists.
	 */
	if ((mkdir)("RESYNC", 0777)) {
		fprintf(stderr, "takepatch: cannot create RESYNC dir.\n");
		repository_lockers(0);
		cleanup(0);
	}
	unless (mkdir("RESYNC/SCCS", 0777) == 0) {
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
 * Go find the change set file and do this relative to that.
 * Create the RESYNC dir or bail out if it exists.
 * Put our pid in that dir so that we can figure out if
 * we are still here.
 *
 * This function creates patches in the PENDING directory when the
 * patches are read from stdin.  On a logging tree, these patches are
 * written and then processed by a seperate applyall process.  So
 * logging processes need to be very careful to not touch any state
 * that might effect the other takepatch that might be running in the
 * background from a previous patch.
 */
private	MMAP	*
init(char *inputFile, int flags, project **pp)
{
	char	buf[BUFSIZ];		/* used ONLY for input I/O */
	char	*root, *t;
	int	i, len;
	FILE	*f, *g;
	MMAP	*m = 0;
	uLong	sumC = 0, sumR = 0;
	project	*p = 0;
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
				if (echo) fprintf(stderr, buf);
				exit(0);
			}
		} else {
			nothingtodo();
		}
	}

	if (newProject) {
		initProject();
		new(p);
		p->root = strdup(ROOT2RESYNC);
		*pp = p;
	} else {
		root = sccs_root(0);
		if (!root && emptyDir(".")) {
			/* If we are invoked in an empty directory,
			 * assume they meant -i.
			 */
			if (echo > 0) {
				fputs("takepatch: creating new repository.\n",
				      stderr);
			}
			initProject();
			new(p);
			p->root = strdup(ROOT2RESYNC);
			*pp = p;
			newProject = 1;
		} else if (sccs_cd2root(0, root)) {
			SHOUT();
			fputs("takepatch: can't find package root.\n", stderr);
			SHOUT2();
			exit(1);
		} else {
			char	*tmp = root;

			root = malloc(strlen(root) + 8);
			sprintf(root, "%s/%s", tmp, ROOT2RESYNC);
			new(p);
			p->root = root;
			*pp = p;
			free(tmp);
		}
	}
	if (exists(LOG_TREE)) isLogPatch = 1;

	if (streq(inputFile, "-")) {
		/*
		 * Save the patch in the pending dir
		 * and record we're working on it.  We use a .incoming
		 * file and then rename it later so that a background
		 * applyall process on the logging server doesn't try
		 * to process a partial patch.
		 */
		unless (savefile("PENDING", ".incoming", pendingFile)) {
			SHOUT();
			perror("PENDING");
			cleanup(CLEAN_RESYNC);
		}
		f = fopen(pendingFile, "wb+");
		assert(f);
		/*
		 * If we are in in the openlogging tree, bkd_push has dropped
		 * the write lock to allow multiple incoming patches at the
		 * same time.  We don't care about multiple tips so it's OK,
		 * but it only works for logging trees.
		 * So we don't take the RESYNC lock here, we do it later.
		 */
		unless (isLogPatch) resync_lock();
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
			}
			if (echo > 7) fprintf(stderr, "P: %s", buf);
	
			if (st.preamble) {
				if (st.newline) {
					st.preamble_nl = 1;
				}
				if (st.preamble_nl) {
					if (streq(buf, PATCH_COMPAT)) {
						compat = 1;
						st.version = 1;
						st.preamble = 0;
						st.preamble_nl = 0;
					} else if (streq(buf, PATCH_CURRENT)) {
						st.type = 1;
						st.preamble = 0;
						st.preamble_nl = 0;
					}
				}
			} else if (st.type) {
				if (streq(buf, PATCH_REGULAR)) {
					st.type = 0;
					st.version = 1;
					isLogPatch = 0;
				} else if (streq(buf, PATCH_LOGGING)) {
					st.type = 0;
					st.version = 1;
					isLogPatch = 1;
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
				if (compat && strneq("s ", buf, 2)) {
				    	fprintf(stderr,
"\ntakepatch: will not accept tags in compatibility mode, please upgrade.\n");
					goto error;
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
			} else if (st.diffs) {
				if (st.newline) {
					st.diffs = 0;
					st.diffsblank = 1;
				}
			} else if (st.diffsblank) {
				if (strneq("== ", buf, 3)) {
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

			unless (mkpatch) {
				unless (fnext(buf, stdin)) {
					perror("fnext");
					goto missing;
				}
				continue;
			}

			/*
			 * Status.
			 */
			if (st.filename) {
				char	*t = strchr(&buf[3], ' ');

				*t = 0;
				unless (st.first) {
					if (echo == 3) fprintf(stderr, "\b");
					verbose((stderr, ": %d deltas\n", j));
					j = 0;
				} else {
					st.first = 0;
				}
				verbose((stderr, "%s", &buf[3]));
				if (echo == 3) fprintf(stderr, " ");
			}

			if (st.metablank && (echo == 3) && mkpatch) {
				fprintf(stderr, "%c\b", spin[j % 4]);
				j++;
			}

			if (st.preamble && echo > 5) {
				fprintf(stderr, "Discard: %s", buf);
			}

			unless (fnext(buf, stdin)) {
				perror("fnext");
				goto missing;
			}
		}
		if (st.preamble) nothingtodo();
		if (mkpatch) {
			if (echo == 3) fprintf(stderr, "\b");
			verbose((stderr, ": %d deltas\n", j));
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
		if (isLogPatch) saveEnviroment(pendingFile);
		note = aprintf("psize=%u", size(incoming));
		cmdlog_addnote(note);
		free(note);
		rename(incoming, pendingFile);
		unless (flags & SILENT) {
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
		if (isLogPatch && newProject) {
			resync_lock();
			assert(exists("BitKeeper/etc"));
			close(creat(LOG_TREE, 0666));
			t = ROOT2RESYNC "/" LOG_TREE;
			mkdirp(ROOT2RESYNC "/BitKeeper/etc");
			close(creat(t, 0666));
		}
		unless (isLogPatch && !newProject) {
			unless (g = fopen("RESYNC/BitKeeper/tmp/patch", "wb")) {
				perror("RESYNC/BitKeeper/tmp/patch");
				exit(1);
			}
			fprintf(g, "%s\n", pendingFile);
			fclose(g);
		}
	} else {
		resync_lock();
		unless (m = mopen(inputFile, "b")) {
			perror(inputFile);
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
		unless (g = fopen("RESYNC/BitKeeper/tmp/patch", "wb")) {
			perror("RESYNC/BitKeeper/tmp/patch");
			exit(1);
		}
		fprintf(g, "%s\n", inputFile);
		fclose(g);

		i = 0;
		while (t = mnext(m)) {
			if (strneq(t, PATCH_CURRENT, strsz(PATCH_CURRENT))) {
				len = linelen(t);
				sumC = adler32(sumC, t, len);
				t = mnext(m);
				if (strneq(t, PATCH_LOGGING, len)) {
					isLogPatch = 1;
					assert(exists("BitKeeper/etc"));
					close(creat(LOG_TREE, 0666));
					mkdirp(ROOT2RESYNC "/BitKeeper/etc");
					close(creat(ROOT2RESYNC
					    "/" LOG_TREE, 0666));
				}
				i++;
				break;
			}
			if (strneq(t, PATCH_COMPAT, strsz(PATCH_COMPAT))) {
				i++;
				compat=1;
				break;
			}
		}
		unless (i) noversline(input);
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
		/* truncated log patches may not create RESYNC */
		cleanup(CLEAN_PENDING|(isdir(ROOT2RESYNC) ? CLEAN_RESYNC : 0));
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
	    SILENT|ADMIN_FORCE|NEWCKSUM, 0, "none", 0, 0, 0, 0, 0, 0)) {
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
	    0, ADMIN_FORCE|NEWCKSUM, 0, "gzip", 0, 0, 0, 0, 0, 0)) {
		sccs_whynot("admin", s);
	}
	s = sccs_restart(s);
	return (s);
}

private	int
rebuild_id(char *id)
{
	char	*s = 0;

	if (echo > 0) {

		s = strchr(id, '|');
		assert(s);
		s = strchr(++s, '|');
		assert(s);
		s = strchr(++s, '|');
		assert(s);
		*s = 0;
		fprintf(stderr,
"Rebuilding idcache - looking for the following key (may take a moment):\n"
"    \"%s\"\n", id);
	}
	if (sccs_reCache(1)) return (1);
	if (idDB) mdbm_close(idDB);
	unless (idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
		perror("SCCS/x.id_cache");
		return (1);
	}
	if (echo > 0) {
		*s = '|';
		fprintf(stderr, "done\n");
	}
	return (0);
}

private	void
cleanup(int what)
{
	int	rc = 1;

	if (patchList) freePatchList();
	if (idDB) mdbm_close(idDB);
	if (goneDB) mdbm_close(goneDB);
	if (saveDirs) {
		fprintf(stderr, "takepatch: neither directory removed.\n");
		goto done;
	}
	if (what & CLEAN_RESYNC) {
		char cmd[1024];
		assert(exists("RESYNC"));
		sprintf(cmd, "%s -rf RESYNC", RM);
		system(cmd);
	} else {
		fprintf(stderr, "takepatch: RESYNC directory left intact.\n");
	}
	unless (streq(input, "-")) goto done;
	if (what & CLEAN_PENDING) {
		assert(exists("PENDING"));
		assert(pendingFile);
		unlink(pendingFile);
		if (rmdir("PENDING")) {
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
	SHOUT2();
	if (what & CLEAN_OK) rc = 0;
	exit(rc);
}
