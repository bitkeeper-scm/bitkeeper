/*
sccs_resolveFile() - the gcapath is s->gfile.  And the error check is that
left||right->path == s->gfile.
*/

/* Copyright (c) 1999 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "zlib/zlib.h"
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
char *takepatch_help = "\n\
usage: takepatch [-acFimStv] [-f file]\n\n\
    -a		apply the changes (call resolve)\n\
    -c		do not accept conflicts with this patch\n\
    -F		(fast) do rebuild id cache when creating files\n\
    -f<file>	take the patch from <file> and do not save it\n\
    -i		initial patch, create a new repository\n\
    -m		list deltas as they are read in the patch\n\
    -S		save RESYNC and or PENDING directories even if errors\n\
    -t		run in text only mode, do not talk to X11.\n\
    -v		verbose level, more is more verbose, -vv is suggested.\n\n";

#define	CLEAN_RESYNC	1	/* blow away the RESYNC dir */
#define	CLEAN_PENDING	2	/* blow away the PENDING dir */
#define	SHOUT() \
	fputs("\n==================== ERROR =======================\n", stderr);
#define	SHOUT2() \
	fputs("==================================================\n\n", stderr);
#define	NOTICE() \
	fputs("------------------------------------------------------\n",\
	stderr);

private	delta	*getRecord(MMAP *f);
private	int	extractPatch(char *name, MMAP *p, int flags, int fast, project *proj);
private	int	extractDelta(char *name, sccs *s, int newFile, MMAP *f, int, int*);
private	int	applyPatch(char *local, int flags, sccs *perfile, project *proj);
private	int	getLocals(sccs *s, delta *d, char *name);
private	void	insertPatch(patch *p);
private	void	initProject(void);
private	MMAP	*init(char *file, int flags, project **p);
private	void	rebuild_id(char *id);
private	void	cleanup(int what);
private	void	changesetExists(void);
private	void	notfirst(void);
private	void	goneError(char *key);
private	void	freePatchList();
private	void	fileCopy2(char *from, char *to);
private	void	badpath(sccs *s, delta *tot);

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
private	char	*spin = "|/-\\";

int
takepatch_main(int ac, char **av)
{
	char	*buf;
	MMAP	*p;
	int	c;
	int	flags = SILENT;
	int	files = 0;
	char	*t;
	int	error = 0;
	int	remote = 0;
	int	resolve = 0;
	project	*proj = 0;
	int	textOnly = 0;
	int	fast = 0;	/* undocumented switch for scripts,
				 * skips cache rebuilds on file creates */

	platformSpecificInit(NULL);
	input = "-";
	debug_main(av);
	while ((c = getopt(ac, av, "acFf:imqsStv")) != -1) {
		switch (c) {
		    case 'q':
		    case 's':
			/* Ignored for option consistency.  */
			break;
		    case 'a': resolve++; break;
		    case 'c': noConflicts++; break;
		    case 'F': fast++; break;
		    case 'f':
			    input = optarg;
			    break;
		    case 'i': newProject++; break;
		    case 'm': mkpatch++; break;
		    case 'S': saveDirs++; break;
		    case 't': textOnly++; break;
		    case 'v': echo++; flags &= ~SILENT; break;
		    default: goto usage;
		}
	}
	if (av[optind]) {
usage:		fprintf(stderr, takepatch_help);
		return (1);
	}

	p = init(input, flags, &proj);

	/*
	 * Find a file and go do it.
	 */
	while (buf = mnext(p)) {
		char	*b;
		int	rc;

		++line;
		/* we need our own storage , extractPatch calls mkline */
		b = strdup(mkline(buf));
		if (echo>3) fprintf(stderr, "%s\n", b);
		unless (strncmp(b, "== ", 3) == 0) {
			if (echo > 6) {
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
		rc = extractPatch(&b[3], p, flags, fast, proj);
		free(b);
		if (rc < 0) {
			error = rc;
			break;
		}
		remote += rc;
	}
	mclose(p);
	proj_free(proj);
	if (idDB) { mdbm_close(idDB); idDB = 0; }
	if (goneDB) { mdbm_close(goneDB); goneDB = 0; }
	if (error < 0) {
		/* XXX: Save?  Purge? */
		cleanup(0);
	}
	purify_list();
	if (echo) {
		fprintf(stderr,
		    "takepatch: %d new revision%s, %d conflicts in %d files\n",
		    remote, remote == 1 ? "" : "s", conflicts, files);
	}
	unless (remote) {
		cleanup(CLEAN_RESYNC | CLEAN_PENDING);
	}
	if (resolve) {
		if (echo) {
			fprintf(stderr,
			    "Running resolve to apply new work...\n");
		}
		if (textOnly) {
			system(echo ? "bk resolve -t" : "bk resolve -qt");
		} else {
			system(echo ? "bk resolve" : "bk resolve -q");
		}
	}
	exit(0);
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

/*
 * Extract a contiguous set of deltas for a single file from the patch file.
 * "name" is the cset name, i.e., the most recent name in the csets being
 * sent; it might be different than the local name.
 */
private	int
extractPatch(char *name, MMAP *p, int flags, int fast, project *proj)
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
	if (newProject && !newFile) notfirst();

	if (echo>3) fprintf(stderr, "%s\n", t);
again:	s = sccs_keyinit(t, INIT_NOCKSUM|INIT_SAVEPROJ, proj, idDB);
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
	unless (s || newProject || (newFile && fast)) {
		if (gone(t, goneDB)) goneError(t);
		unless (rebuilt++) {
			rebuild_id(t);
			goto again;
		}
		unless (newFile) {
			SHOUT();
			fprintf(stderr,
			   "takepatch: can't find key '%s' in id cache\n", t);
cleanup:		if (perfile) sccs_free(perfile);
			if (gfile) free(gfile);
			if (s) sccs_free(s);
			free(name);
			cleanup(CLEAN_RESYNC);
		}
	}

	/*
	 * They may have sent us a patch from 1.1, so the patch looks like a
	 * new file.  But if we have a match, we want to use it.
	 */
	if (s) {
		reallyNew = 0;
		if (newFile && (echo > 3)) {
			fprintf(stderr,
			    "takepatch: new file %s already exists.\n", name);
		}
		if (echo > 6) {
			fprintf(stderr, "takepatch: file %s found.\n",
			s->sfile);
		}
		if (IS_EDITED(s)) {
			if (sccs_clean(s, SILENT)) {
				SHOUT();
				fprintf(stderr,
				    "takepatch: %s is edited and modified.\n",
				    name);
				fprintf(stderr,
			    "takepatch: will not overwrite modified files.\n");
				goto cleanup;
			} else {
				sccs_restart(s);
			}
		}
		if (s->state & S_PFILE) {
			SHOUT();
			fprintf(stderr,
			    "takepatch: %s is locked w/o gfile?\n", s->sfile);
			goto cleanup;
		}
		tmp = sccs_getrev(s, "+", 0, 0);
		assert(tmp);
		unless (streq(tmp->pathname, s->gfile)) {
			badpath(s, tmp);
			goto cleanup;
		}
		unless (tmp = sccs_findKey(s, t)) {
			SHOUT();
			fprintf(stderr,
			    "takepatch: can't find root delta '%s' in %s\n",
			    t, name);
			goto cleanup;
		}
		unless (s->tree == tmp) {
			SHOUT();
			fprintf(stderr,
			    "takepatch: root deltas do not match in %s\n",
			    name);
			goto cleanup;
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
		if (echo > 2) {
			fprintf(stderr,
			    "takepatch: new file %s\n", t);
		}
	}
	tableGCA = 0;
	while (extractDelta(name, s, newFile, p, flags, &nfound)) {
		if (newFile) newFile = 2;
	}
	gfile = sccs2name(name);
	if (echo>1) {
		fprintf(stderr, "Applying %3d revisions to %s%s ",
		    nfound, reallyNew ? "new file " : "", gfile);
		if (echo != 2) fprintf(stderr, "\n");
	}
	if (patchList && tableGCA) getLocals(s, tableGCA, name);
	rc = applyPatch(s ? s->sfile : 0, flags, perfile, proj);
	if (echo == 2) fprintf(stderr, " \n");
	if (perfile) sccs_free(perfile);
	free(gfile);
	free(name);
	if (s) {
		s->proj = 0;
		sccs_free(s);
	}
	if (rc < 0) return (rc);
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
	unless (to->random || !from->random) {
		to->random = from->random;
		from->random = 0;
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
	if (echo>3) fprintf(stderr, "%s\n", b);
	if (strneq(b, "# Patch checksum=", 17)) return 0;
	pid = strdup(b);
	/*
	 * This code assumes that the patch order is 1.1..1.100
	 * We stash away the parent of the earliest delta as a "GCA".
	 */
	if ((parent = sccs_findKey(s, b)) && !tableGCA) tableGCA = parent;

	/* go get the delta table entry for this delta */
delta1:	off = mtell(f);
	d = getRecord(f);
	sccs_sdelta(s, d, buf);
	if (tmp = sccs_findKey(s, buf)) {
		if (echo > 2) {
			fprintf(stderr,
			    "takepatch: delta %s already in %s, skipping it.\n",
			    tmp->rev, s->sfile);
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
			if (echo>3) fprintf(stderr, "%.*s", linelen(b), b);
		}
		line++;
		if (echo>4) fprintf(stderr, "\n");
		p = calloc(1, sizeof(patch));
		p->flags = PATCH_REMOTE;
		p->pid = pid;
		sccs_sdelta(s, d, buf);
		p->me = strdup(buf);
		p->initMmap = mrange(start, stop, "b");
		p->localFile = s ? strdup(s->sfile) : 0;
		sprintf(buf, "RESYNC/%s", name);
		p->resyncFile = strdup(buf);
		p->order = parent == d ? 0 : d->date;
		if (echo>5) fprintf(stderr, "REM: %s %s %lu\n",
				    d->rev, p->me, p->order);
		c = line;
		start = f->where; stop = start;
		while ((b = mnext(f)) && (*b != '\n')) {
			stop = f->where;
			line++;
			if (echo>4) fprintf(stderr, "%.*s", linelen(b), b);
		}
		if (d->flags & D_META) {
			p->flags |= PATCH_META;
			assert(c == line);
		} else {
			p->diffMmap = mrange(start, stop, "b");
		}
		line++;
		if (echo>4) fprintf(stderr, "\n");
		(*np)++;
		insertPatch(p);
	}
	sccs_freetree(d);
	if ((c = mpeekc(f)) != EOF) {
		return (c != '=');
	}
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
    bk prs -hr1.0 -d:KEY: ChangeSet\n\n", stderr);
    	cleanup(CLEAN_RESYNC|CLEAN_PENDING);
}

private	void
goneError(char *buf)
{
	SHOUT();
	fprintf(stderr,
"File %s\n\
is marked as gone in this repository and therefor can not accept updates.\n\
The fact that you are getting updates indicates that the file is not gone\n\
in the other repository and could be restored in this repository.\n\
Contact BitMover for assistance, we'll have a tool to do this soon.\n", buf);
	cleanup(CLEAN_PENDING|CLEAN_RESYNC);
}

private	void
noconflicts(void)
{
	SHOUT();
	fputs(
"takepatch was instructed not to accept conflicts into this tree.\n\
Please make sure all pending deltas are comitted in this tree,\n\
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
	if (exists("RESYNC")) {
		cleanup(CLEAN_PENDING|CLEAN_RESYNC);
	} else {
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
	if (echo > 2) fprintf(stderr,  " (%x != %x)", a, b);
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

private	void
oldformat(void)
{
	fputs("takepatch: warning: patch is in obsolete format\n", stderr);
}

private	void
setlod(sccs *s, delta *d, int branch)
{
	u16	maxlod;
	u8	def[MAXPATH];

	if (s->defbranch) free(s->defbranch);
	s->defbranch=0;

	/* if revision, set it and leave */
	unless (branch) {
		assert(d->rev);
		s->defbranch = strdup(d->rev);
		return;
	}

	/* if branch, not needed if latest */
	maxlod = sccs_nextlod(s);
	maxlod--;
	if (maxlod == d->r[0]) return;

	sprintf(def, "%d", d->r[0]);
	s->defbranch = strdup(def);
	sccs_admin(s, 0, NEWCKSUM, 0, 0, 0, 0, 0, 0, 0, 0);
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
	patch	*p = patchList;
	MMAP	*iF;
	MMAP	*dF;
	sccs	*s = 0;
	delta	*d = 0;
	int	newflags;
	char	*now();
	int	n = 0;
	char	lodkey[MAXPATH];
	int	lodbranch = 1;	/* true if LOD is branch; false if revision */
	int	confThisFile;
#define	CSETS	"RESYNC/BitKeeper/etc/csets"
	FILE	*csets = 0;

	unless (p) return (0);
	lodkey[0] = 0;
	if (echo == 2) fprintf(stderr, "%c\b", spin[n++ % 4]);
	if (echo > 6) {
		fprintf(stderr, "L=%s\nR=%s\nP=%s\nM=%s\n",
		    p->localFile, p->resyncFile, p->pid, p->me);
	}
	unless (localPath) {
		mkdirf(p->resyncFile);
		goto apply;
	}
	fileCopy2(localPath, p->resyncFile);
	unless (s = sccs_init(p->resyncFile, INIT_NOCKSUM|flags, proj)) {
		SHOUT();
		fprintf(stderr, "takepatch: can't open %s\n", p->resyncFile);
		return -1;
	}
	if (!s->tree) {
		SHOUT();
		if (!(s->state & S_SFILE)) {
			fprintf(stderr,
			    "takepatch: no s.file %s\n", p->resyncFile);
		} else {
			perror(s->sfile);
		}
		return -1;
	}
	/* save current LOD setting, as it might change */
	if (d = findrev(s, "")) {
		sccs_sdelta(s, d, lodkey);
		if (s->defbranch) {
			u8	*ptr;
			/* XXX: handle symbols? */
			lodbranch = 1;
			for (ptr = s->defbranch; *ptr; ptr++) {
				unless (*ptr == '.') continue;
				lodbranch = 1 - lodbranch;
			}
		}
	}
	unless (tableGCA) goto apply;
	/*
	 * Note that tableGCA is NOT a valid pointer into the sccs tree "s".
	 */
	assert(tableGCA);
	assert(tableGCA->rev);
	assert(tableGCA->pathname);
	if (echo > 5) {
		fprintf(stderr,
		    "stripdel %s from %s\n", tableGCA->rev, s->sfile);
	}
	if (d = sccs_next(s, sccs_getrev(s, tableGCA->rev, 0, 0))) {
		delta	*e;

		for (e = s->table; e; e = e->next) {
			e->flags |= D_SET|D_GONE;
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
		if (echo == 2) fprintf(stderr, "%c\b", spin[n++ % 4]);
		if (p->pid) {
			assert(s);
			unless (d = sccs_findKey(s, p->pid)) {
				if (echo == 2) fprintf(stderr, " \n");
				ahead(p->pid, s->sfile);
			}
			unless (sccs_restart(s)) { perror("restart"); exit(1); }
			if (echo>8) fprintf(stderr, "Child of %s\n", d->rev);
			if (p->flags & PATCH_META) {
				MMAP	*m = p->initMmap;

				unless (m) m = mopen(p->initFile, "b");
				if (sccs_meta(s, d, m)) {
					perror("meta");
					return -1;
				}
			} else {
				newflags = (echo > 5) ?
				    GET_SKIPGET|GET_EDIT :
				    SILENT|GET_SKIPGET|GET_EDIT;
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
				newflags = (echo > 2) ?
				    DELTA_FORCE|DELTA_PATCH :
				    DELTA_FORCE|DELTA_PATCH|SILENT;
				if (sccs_delta(s, newflags, 0, iF, dF, 0)) {
					perror("delta");
					return -1;
				}
				if (s->state & S_BAD_DSUM) {
					return -1;
				}
				mclose(iF);
				if ((s->state & S_CSET) && 
				    !(p->flags & PATCH_LOCAL))  {
					static	int first = 1;
					delta	*d = sccs_findKey(s, p->me);

					assert(d);
					unless (csets) {
						csets = fopen(CSETS, "w");
						assert(csets);
					}
					unless (first) fprintf(csets, ",");
					first = 0;
					fprintf(csets, "%s", d->rev);
				}
			}
		} else {
			assert(s == 0);
			unless (s = sccs_init(p->resyncFile, NEWFILE, proj)) {
				SHOUT();
				fprintf(stderr,
				    "takepatch: can't create %s\n",
				    p->resyncFile);
				return -1;
			}
			if (perfile) sccscopy(s, perfile);
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
			newflags = (echo > 2) ?
			    NEWFILE|DELTA_FORCE|DELTA_PATCH :
			    NEWFILE|DELTA_FORCE|DELTA_PATCH|SILENT;
			if (sccs_delta(s, newflags, d, iF, dF, 0)) {
				perror("delta");
				return -1;
			}
			if (s->state & S_BAD_DSUM) cleanup(CLEAN_RESYNC);
			mclose(iF);	/* dF done by delta() */
			s->proj = 0; sccs_free(s);
			s = sccs_init(p->resyncFile, INIT_NOCKSUM, proj);
		}
		p = p->next;
	}

	if (csets) {
		fprintf(csets, "\n");
		fclose(csets);
	}
	s->proj = 0; sccs_free(s);
	s = sccs_init(patchList->resyncFile, 0, proj);
	assert(s);
	if (lodkey[0]) { /* restore LOD setting */
		unless (d = sccs_findKey(s, lodkey)) {
			fprintf(stderr, "takepatch: can't find lod key %s\n",
				lodkey);
			return (-1);
		}
		setlod(s, d, lodbranch);
	}
	for (d = 0, p = patchList; p; p = p->next) {
		assert(p->me);
		d = sccs_findKey(s, p->me);
		assert(d);
		d->flags |= (p->flags & PATCH_LOCAL) ? D_LOCAL : D_REMOTE;
	}
	if ((confThisFile = sccs_resolveFiles(s)) < 0) {
		s->proj = 0; sccs_free(s);
		return (-1);
	}

	/* Conflicts in ChangeSet don't count.  */
	if (confThisFile &&
	    !streq(s->sfile + strlen(s->sfile) - 9, "ChangeSet")) {
		conflicts += confThisFile;
	}
	if (confThisFile && !(s->state & S_CSET)) {
		assert(d);
		unless (d->flags & D_CSET) {
			fprintf(stderr, "No csetmark on %s\n", d->rev);
			s->proj = 0; sccs_free(s);
			uncommitted(localPath);
			return -1;
		}
	}
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

	if (echo > 5) {
		fprintf(stderr, "getlocals(%s, %s, %s)\n",
		    s->gfile, g->rev, name);
	}
	for (d = s->table; d != g; d = d->next) {
		assert(d);
		sprintf(tmpf, "RESYNC/BitKeeper/tmp/%03d-init", ++fileNum);
		unless (t = fopen(tmpf, "wb")) {
			perror(tmpf);
			exit(1);
		}
		s->rstart = s->rstop = d;
		sccs_restart(s);
		sccs_prs(s, PRS_PATCH|SILENT, 0, NULL, t);
		if (ferror(t)) {
			perror("error on init file");
			cleanup(CLEAN_RESYNC);
		}
		fclose(t);
		p = calloc(1, sizeof(patch));
		p->flags = PATCH_LOCAL;
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
			p->flags |= PATCH_META;
		}
		unless (d->date || streq("70/01/01 00:00:00", d->sdate)) {
			assert(d->date);
		}
		sccs_sdelta(s, d->parent, tmpf);
		p->pid = strdup(tmpf);
		sccs_sdelta(s, d, tmpf);
		p->me = strdup(tmpf);
		p->order = d->date;
		if (echo>5) {
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
 */
private	void
insertPatch(patch *p)
{
	patch	*t;

	if (!patchList || earlier(p, patchList)) {
		p->next = patchList;
		patchList = p;
		return;
	}
	/*
	 * We know that t is pointing to a node that is older than us.
	 */
	for (t = patchList; t->next; t = t->next) {
		if (earlier(p, t->next)) {
			p->next = t->next;
			t->next = p;
			return;
		}
	}

	/*
	 * There is no next field and we know that t->order is < date.
	 */
	assert(earlier(t, p));
	t->next = p;
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

/*
 * Go find the change set file and do this relative to that.
 * Create the RESYNC dir or bail out if it exists.
 * Put our pid in that dir so that we can figure out if
 * we are still here.
 */
private	MMAP	*
init(char *inputFile, int flags, project **pp)
{
	char	buf[MAXPATH];		/* used ONLY for input I/O */
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
	int	line = 0, first = 1, j = 0;

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

	/*
	 * See if we can lock the tree.
	 * We assume that a higher level program called repository_wrlock(),
	 * we're just doing the RESYNC part.
	 */
	if (mkdir("RESYNC", 0777)) {
		fprintf(stderr, "takepatch: can not create RESYNC dir.\n");
		repository_lockers(p);
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
	fprintf(f, "%d\n", getpid());
	fclose(f);

	if (streq(inputFile, "-")) {
		/*
		 * Save the patch in the pending dir
		 * and record we're working on it.
		 */
		if (!isdir("PENDING") && (mkdir("PENDING", 0777) == -1)) {
			SHOUT();
			perror("PENDING");
			cleanup(CLEAN_RESYNC);
		}
		/* Force this group writable */
		(void)chmod("PENDING", 0775);
		for (i = 1; ; i++) {				/* CSTYLED */
			struct	tm *tm;
			time_t	now = time(0);
			char	buf2[100];

			tm = localtime(&now);
			strftime(buf2, sizeof(buf2), "%Y-%m-%d", tm);
			sprintf(pendingFile, "PENDING/%s.%02d", buf2, i);
			if (exists(pendingFile)) continue;
			if (f = fopen(pendingFile, "wb+")) {
				break;
			} else {
				SHOUT();
				perror(pendingFile);
				fputs("Check permissions on PENDING\n", stderr);
				cleanup(CLEAN_RESYNC);
			}
			if (i > 100) {
				SHOUT();
				fprintf(stderr,
				    "takepatch: too many patches.\n");
				cleanup(CLEAN_RESYNC);
			}
		}
		unless (g = fopen("RESYNC/BitKeeper/tmp/patch", "wb")) {
			perror("RESYNC/BitKeeper/tmp/patch");
			exit(1);
		}
		fprintf(g, "%s\n", pendingFile);
		fclose(g);

		for (;;) {
			st.newline = streq("\n", buf);
			if (echo > 9) {
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
			if (echo > 6) fprintf(stderr, "P: %s", buf);
	
			if (st.preamble) {
				if (st.newline) {
					st.preamble_nl = 1;
				}
				if (st.preamble_nl &&
				    streq(buf, PATCH_CURRENT)) {
					st.version = 1;
					st.preamble = 0;
					st.preamble_nl = 0;
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
				if (fputs(buf, f) == EOF) {
					perror("fputs on patch");
					cleanup(CLEAN_PENDING|CLEAN_RESYNC);
				}
				len = strlen(buf);
				sumC = adler32(sumC, buf, len);
			}

			unless (mkpatch) {
				unless (fnext(buf, stdin)) goto missing;
				continue;
			}

			/*
			 * Status.
			 */
			if (st.filename) {
				char	*t = strchr(&buf[3], ' ');

				*t = 0;
				unless (st.first) {
					fprintf(stderr, "\b: %d deltas\n", j);
					j = 0;
				} else {
					st.first = 0;
				}
				fprintf(stderr, "%s ", &buf[3]);
			}

			if (st.metablank) {
				fprintf(stderr, "%c\b", spin[j++ % 4]);
			}

			if (st.preamble && echo > 4) {
				fprintf(stderr, "Discard: %s", buf);
			}

			unless (fnext(buf, stdin)) goto missing;
		}
		if (st.preamble) nothingtodo();
		if (mkpatch) fprintf(stderr, "\b: %d deltas\n", j);
		if (fclose(f)) {
			perror("fclose on patch");
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
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
	} else {
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
				i++;
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

	if (!sumR) {
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

	if (newProject) {
		unless (idDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) {
			perror("mdbm_open");
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
		return (m);
	}

	/* OK if this returns NULL */
	goneDB = loadDB(GONE, 0, DB_KEYSONLY|DB_NODUPS);

	unless (idDB = loadDB(IDCACHE, 0, DB_NODUPS)) {
		perror("SCCS/x.id_cache");
		exit(1);
	}
	return (m);
}

private	void
fileCopy2(char *from, char *to)
{
	if (fileCopy(from, to)) cleanup(CLEAN_RESYNC);
}

private	void
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
"takepatch: miss in idcache while looking for\n\t     \"%s\",\n\
\t     rebuilding (this can take a while)...", id);
	}
	sccs_reCache();
	if (idDB) mdbm_close(idDB);
	unless (idDB = loadDB(IDCACHE, 0, DB_NODUPS)) {
		perror("SCCS/x.id_cache");
		exit(1);
	}
	if (echo > 0) {
		*s = '|';
		fprintf(stderr, "done\n");
	}
}

private	void
cleanup(int what)
{
	if (patchList) freePatchList();
	if (idDB) mdbm_close(idDB);
	if (goneDB) mdbm_close(goneDB);
	purify_list(); /* win32 note: if we get here, all fd must be closed */
	if (saveDirs) {
		fprintf(stderr, "takepatch: neither directory removed.\n");
		SHOUT2();
		exit(1);
	}
	if (what & CLEAN_RESYNC) {
		char cmd[1024];
		assert(exists("RESYNC"));
		sprintf(cmd, "%s -rf RESYNC", RM);
		system(cmd);
	} else {
		fprintf(stderr, "takepatch: RESYNC directory left intact.\n");
	}
	unless (streq(input, "-")) {
		SHOUT2();
		exit(1);
	}
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
	SHOUT2();
	exit(1);
}
