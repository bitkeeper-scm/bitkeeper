/* Copyright (c) 1999 L.W.McVoy */
#include "sccs.h"
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
usage: takepatch [-cFiv] [-f file]\n\n\
    -c		do not accept conflicts with this patch\n\
    -F		(fast) do rebuild id cache when creating files\n\
    -f<file>	take the patch from <file> and do not save it\n\
    -i		initial patch, create a new repository\n\
    -v		verbose level, more is more verbose, -vv is suggested.\n\n";

#define	CLEAN_RESYNC	1
#define	CLEAN_PENDING	2

delta	*getRecord(FILE *f);
int	extractPatch(char *name, FILE *p, int flags);
int	extractDelta(char *name, sccs *s, int newFile, FILE *f, int flags);
int	applyPatch(char *local, char *remote, int flags, sccs *perfile);
int	getLocals(sccs *s, delta *d, char *name);
void	insertPatch(patch *p);
void	initProject(void);
FILE	*init(FILE *p, int flags);
int	mkdirp(char *file);
int	fileCopy(char *from, char *to);

int	echo = 0;
int	line;
int	no;
patch	*patchList = 0;
int	files, remote, conflicts;
int	newProject;
MDBM	*idDB;
delta	*gca;		/* The oldest parent found in the patch */
int	nfound;
int	noConflicts;	/* if set, abort on conflicts */
char	pendingFile[MAXPATH];
FILE	*input;
int	fast;		/* skip idcache rebuilds for new files */

int
main(int ac, char **av)
{
	char	buf[MAXLINE];
	FILE	*p;
	int	c;
	int	flags = SILENT;
	char	*t;

	input = stdin;
	debug_main(av);
	while ((c = getopt(ac, av, "cFf:iv")) != -1) {
		switch (c) {
		    case 'c': noConflicts++; break;
		    case 'F': fast++; break;
		    case 'f': input = fopen(optarg, "rt"); break;
		    case 'i': newProject++; break;
		    case 'v': echo++; flags &= ~SILENT; break;
		    default: goto usage;
		}
	}
	if (av[optind]) {
usage:		fprintf(stderr, takepatch_help);
		return (1);
	}

	p = init(input, flags);

	/*
	 * Find a file and go do it.
	 */
	while (fnext(buf, p)) {
		++line;
		if (echo>3) fprintf(stderr, "%s", buf);
		unless (strncmp(buf, "== ", 3) == 0) {
			if (echo > 6) fprintf(stderr, "skipping: %s", buf);
			continue;
		}
		unless ((t = strrchr(buf, ' ')) && streq(t, " ==\n")) {
			fprintf(stderr, "Bad patch: %s", buf);
			cleanup(CLEAN_RESYNC);
		}
		*t = 0;
		extractPatch(&buf[3], p, flags);
	}
	fclose(p);
	purify_list();
	if (echo) {
		fprintf(stderr,
		    "takepatch: %d new revision%s, %d conflicts in %d files\n",
		    remote, remote == 1 ? "" : "s", conflicts, files);
	}
	unless (remote) {
		cleanup(CLEAN_RESYNC | CLEAN_PENDING);
	}
	exit(0);
}

delta *
getRecord(FILE *f)
{
	int	e = 0;
	delta	*d = sccs_getInit(0, 0, f, 1, &e, 0);

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
int
extractPatch(char *name, FILE *p, int flags)
{
	delta	*tmp;
	sccs	*s = 0;
	sccs	*perfile = 0;
	int	newFile = 0;
	char	*gfile;
	static	int rebuilt = 0;	/* static - do it once only */
	char	buf[1200];

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
	files++;
	fnext(buf, p); chop(buf);
	line++;
	name = name2sccs(name);
	if (strneq("New file: ", buf, 10)) {
		newFile = 1;
		perfile = sccs_getperfile(p, &line);
		fnext(buf, p);
		chop(buf);
		line++;
	}
	if (newProject && !newFile) notfirst();

	if (echo>3) fprintf(stderr, "%s\n", buf);
again:	s = sccs_keyinit(buf, NOCKSUM, idDB);
	/*
	 * Unless it is a brand new workspace, or a new file,
	 * rebuild the id cache if look up failed.
	 *
	 * XXX - we need some test cases in the regression scripts for this.
	 * a) move the file and make sure it finds it
	 * b) move the file and send a new file over and make sure it finds
	 *    it.
	 */
	unless (s || newProject || (newFile && fast)) {
		unless (rebuilt++) {
			rebuild_id(buf);
			goto again;
		}
		unless (newFile) {
			fprintf(stderr,
			   "takepatch: can't find key '%s' in id cache\n", buf);
			cleanup(CLEAN_RESYNC);
		}
	}

	/*
	 * They may have sent us a patch from 1.1, so the patch looks like a
	 * new file.  But if we have a match, we want to use it.
	 */
	if (s) {
		if (newFile && (echo > 3)) {
			fprintf(stderr,
			    "takepatch: new file %s already exists.\n", name);
			newFile = 0;
		}
		if (echo > 6) {
			fprintf(stderr, "takepatch: file %s found.\n",
			s->sfile);
		}
		if (IS_EDITED(s)) {
			fprintf(stderr,
			    "takepatch: %s is edited\n", name);
			cleanup(CLEAN_RESYNC);
		}
		if (s->state & S_PFILE) {
			fprintf(stderr, 
			    "takepatch: %s is locked.\n", s->sfile);
			exit(1);
		}
		unless (tmp = sccs_findKey(s, buf)) {
			fprintf(stderr,
			    "takepatch: can't find root delta '%s' in %s\n",
			    buf, name);
			exit(1);
		}
		unless (s->tree == tmp) {
			fprintf(stderr,
		    	"takepatch: root deltas do not match in %s\n", name);
			exit(1);
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
		if (streq(name, "SCCS/s.ChangeSet")) {
			/*
			 * If we are about to create a new ChangeSet file,
			 * this better be the very first and only
			 * ChangeSet file.
			 */
			assert(!exists("SCCS/s.ChangeSet"));
		}
		if (echo > 2) {
			fprintf(stderr,
			    "takepatch: new file %s\n", buf);
		}
	}
	gca = 0;
	nfound = 0;
	while (extractDelta(name, s, newFile, p, flags)) {
		if (newFile) newFile = 2;
	}
	remote += nfound;
	gfile = sccs2name(name);
	if (echo>1) {
		fprintf(stderr, "takepatch: %3d new in %s ", nfound, gfile);
		if (echo != 2) fprintf(stderr, "\n");
	}
	if (patchList && gca) {
		if (getLocals(s, gca, name) && noConflicts) noconflicts();
	}
	applyPatch(s ? s->sfile : 0, name, flags, perfile);
	if (echo == 2) fprintf(stderr, " \n");
	if (perfile) free(perfile);
	if (s) sccs_free(s);
	free(gfile);
	free(name);
	return (0);
}

int
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
 * Extract one delta from the patch file.
 * Deltas end on the first blank line.
 */
int
extractDelta(char *name, sccs *s, int newFile, FILE *f, int flags)
{
	FILE	*t;
	delta	*d, *parent = 0, *tmp;
	char	tmpf[MAXPATH];
	char	buf[MAXLINE];
	char	*pid = 0;
	long	off;
	int	c;
	int	skip = 0;
	patch	*p;

	if (newFile == 1) goto delta;

	fnext(buf, f); chop(buf); line++;
	if (echo>3) fprintf(stderr, "%s\n", buf);
	pid = strdup(buf);
	/*
	 * This code assumes that the patch order is 1.1..1.100
	 */
	if (parent = sccs_findKey(s, buf)) {
		unless (gca) {
			gca = parent;
			/*
			 * This could be the continuation of something which
			 * is a branch in this repository (but the trunk in
			 * the other repository).  So work you way back up
			 * until you are on the trunk.
			 * LOD-XXX - this will need to get reworked for LODs.
			 */
			while (gca->r[2]) gca = gca->parent;
		}
	}

	/* go get the delta table entry for this delta */
delta:	off = ftell(f);
	d = getRecord(f);
	sccs_sdelta(buf, d);
	if (tmp = sccs_findKey(s, buf)) {
		if (echo > 2) {
			fprintf(stderr,
			    "takepatch: delta %s already in %s, skipping it.\n",
			    tmp->rev, s->sfile);
		}
		skip++;
	} else {
		no++;
	}
	fseek(f, off, 0);
	if (skip) {
		free(pid);
		/* Eat metadata */
		while (fnext(buf, f) && !streq("\n", buf)) line++;
		line++;
		/* Eat diffs */
		while (fnext(buf, f) && !streq("\n", buf)) line++;
		line++;
	} else {
		sprintf(tmpf, "RESYNC/BitKeeper/tmp/%03d-init", no);
		unless (t = fopen(tmpf, "w")) {
			perror(tmpf);
			exit(0);
		}
		while (fnext(buf, f) && !streq("\n", buf)) {
			line++;
			if (echo>3) fprintf(stderr, "%s", buf);
			if (fputs(buf, t) <= 0) {
				perror("fputs on init file");
				cleanup(CLEAN_RESYNC);
			}
		}
		line++;
		if (echo>4) fprintf(stderr, "\n");
		fclose(t);
		p = calloc(1, sizeof(patch));
		p->flags = PATCH_REMOTE;
		p->pid = pid;
		sccs_sdelta(buf, d);
		p->me = strdup(buf);
		p->initFile = strdup(tmpf);
		p->localFile = s ? strdup(s->sfile) : 0;
		sprintf(tmpf, "RESYNC/%s", name);
		p->resyncFile = strdup(tmpf);
		sprintf(tmpf, "RESYNC/BitKeeper/tmp/%03d-diffs", no);
		p->diffFile = strdup(tmpf);
		p->order = parent == d ? 0 : d->date;
		if (echo>5) fprintf(stderr, "REM: %s %s %u\n", d->rev, p->me, p->order);
		unless (t = fopen(tmpf, "w")) {
			perror(tmpf);
			exit(0);
		}
		c = line;
		while (fnext(buf, f) && !streq("\n", buf)) {
			if (fputs(buf, t) <= 0) {
				perror("fputs on diffs file");
				cleanup(CLEAN_RESYNC);
			}
			line++;
			if (echo>4) fprintf(stderr, "%s", buf);
		}
		if (d->flags & D_META) {
			p->flags |= PATCH_META;
			assert(c == line);
		}
		line++;
		if (echo>4) fprintf(stderr, "\n");
		fclose(t);
		nfound++;
		insertPatch(p);
	}
	sccs_freetree(d);
	if ((c = getc(f)) != EOF) {
		ungetc(c, f);
		return (c != '=');
	}
	return (0);
}

noconflicts()
{
	fprintf(stderr,
"takepatch was instructed not to accept conflicts into this tree.\n\
Please resync in the opposite direction and then reapply this patch.\n");
	cleanup(CLEAN_PENDING|CLEAN_RESYNC);
}

notfirst()
{
	fprintf(stderr,
"takepatch: when creating a project, as you are currently doing, you have\n\
to resync from version 1.0 forward.  Please try again, with a command like\n\
\n\
\tbk resync -r1.0.. from to\n\
or\n\
\tbk send 1.0.. user@host.com\n\
\n\
takepatch has not cleaned up your destination, you need to do that.\n");
	cleanup(CLEAN_RESYNC|CLEAN_PENDING);
}

void
ahead(char *pid, char *sfile)
{
	fprintf(stderr,
	    "takepatch: can't find parent ID\n\t%s\n\tin %s\n", pid, sfile);
	fprintf(stderr,
"This patch is ahead of your tree, you need to get an earlier patch first.\n\
Look at your tree with a ``bk changes'' and do the same on the other tree,\n\
and get a patch that is based on a common ancestor.\n");
	cleanup(CLEAN_RESYNC|CLEAN_PENDING);
}

/*
 * If the destination file does not exist, just apply the patches to create
 * the new file.
 * If the file does exist, copy it, rip out the old stuff, and then apply
 * the list.
 */
int
applyPatch(char *localPath, char *remotePath, int flags, sccs *perfile)
{
	patch	*p = patchList;
	FILE	*iF, *dF;
	sccs	*s = 0;
	delta	*d;
	int	newflags;
	char	*getuser(), *now();
	static	char *spin = "|/-\\";
	char	*gcaPath = 0;
	int	n = 0;

	unless (p) return (0);
	if (echo == 2) fprintf(stderr, "%c\b", spin[n++ % 4]);
	if (echo > 6) {
		fprintf(stderr, "L=%s\nR=%s\nP=%s\nM=%s\n",
		    p->localFile, p->resyncFile, p->pid, p->me);
	}
	unless (localPath) {
		mkdirp(p->resyncFile);
		goto apply;
	}
	if (fileCopy(localPath, p->resyncFile)) {
		perror("cp");
		cleanup(CLEAN_RESYNC);
	}
	unless (s = sccs_init(p->resyncFile, NOCKSUM|flags)) {
		fprintf(stderr, "takepatch: can't open %s\n", p->resyncFile);
		cleanup(CLEAN_RESYNC);
	}
	if (!s->tree) {
		if (!(s->state & S_SFILE)) {
			fprintf(stderr,
			    "takepatch: no s.file %s\n", p->resyncFile);
		} else {
			perror(s->sfile);
		}
		cleanup(CLEAN_RESYNC);
	}
	unless (gca) goto apply;
	/*
	 * Note that gca is NOT a valid pointer into the sccs tree "s".
	 */
	assert(gca);
	assert(gca->rev);
	assert(gca->pathname);
	if (echo > 5) fprintf(stderr, "rmdel %s from %s\n", gca->rev, s->sfile);
	if (d = sccs_next(s, sccs_getrev(s, gca->rev, 0, 0))) {
		if (sccs_rmdel(s, d, 1, SILENT)) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "rmdel of %s failed.\n", p->resyncFile);
			}
			cleanup(CLEAN_RESYNC);
		}
	}
	sccs_free(s);
	/* sccs_restart does not rebuild the graph and we just pruned it,
	 * so do a hard restart.
	 */
	unless (s = sccs_init(p->resyncFile, NOCKSUM|flags)) {
		fprintf(stderr,
		    "takepatch: can't open %s\n", p->resyncFile);
		cleanup(CLEAN_RESYNC);
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
				if (sccs_meta(s, d, p->initFile)) {
					perror("meta");
					cleanup(CLEAN_RESYNC);
				}
			} else {
				newflags = (echo > 5) ?
				    NOCKSUM|SKIPGET|EDIT :
				    NOCKSUM|SILENT|SKIPGET|EDIT;
				/* CSTYLED */
				if (sccs_get(s, d->rev, 0,0,0, newflags, "-")) {
				    	perror("get");
					cleanup(CLEAN_RESYNC);
				}
				if (echo > 6) {
					char	buf[MAXPATH];

					sprintf(buf,
			"echo --- %s ---; cat %s; echo --- %s ---; cat %s", 
			p->initFile, p->initFile, p->diffFile, p->diffFile);
					system(buf);
				}
				iF = fopen(p->initFile, "r");
				dF = fopen(p->diffFile, "r");
				newflags = (echo > 2) ?
				    NOCKSUM|FORCE|PATCH :
				    NOCKSUM|FORCE|PATCH|SILENT;
				if (sccs_delta(s, newflags, 0, iF, dF)) {
					perror("delta");
					cleanup(CLEAN_RESYNC);
				}
				if (s->state & S_BAD_DSUM) {
					cleanup(CLEAN_RESYNC);
				}
				fclose(iF);	/* dF done by delta() */
			}
		} else {
			assert(s == 0);
			unless (s = sccs_init(p->resyncFile, NEWFILE)) {
				fprintf(stderr,
				    "takepatch: can't create %s\n",
				    p->resyncFile);
				cleanup(CLEAN_RESYNC);
			}
			if (perfile) sccscopy(s, perfile);
			iF = fopen(p->initFile, "r");
			dF = fopen(p->diffFile, "r");
			d = 0;
			newflags = (echo > 2) ?
			    NOCKSUM|NEWFILE|FORCE|PATCH :
			    NOCKSUM|NEWFILE|FORCE|PATCH|SILENT;
			if (sccs_delta(s, newflags, d, iF, dF)) {
				perror("delta");
				cleanup(CLEAN_RESYNC);
			}
			if (s->state & S_BAD_DSUM) cleanup(CLEAN_RESYNC);
			fclose(iF);	/* dF done by delta() */
			sccs_free(s);
			s = sccs_init(p->resyncFile, NOCKSUM);
		}
		p = p->next;
	}

	sccs_free(s);
	s = sccs_init(patchList->resyncFile, 0);
	assert(s);
	gcaPath = gca ? name2sccs(gca->pathname) : 0;
	unless (localPath) {
		/* must be new file */
		conflicts += sccs_resolveFile(s, 0, 0, 0);
	} else if (streq(localPath, remotePath)) {
		/* no name changes, life is good */
		conflicts += sccs_resolveFile(s, 0, 0, 0);
	} else {
		debug((stderr, "L=%s\nR=%s\nG=%s (%s)\n",
		    localPath, remotePath, gcaPath, gca ? gca->rev : ""));
		/* local != remote */
		assert(gcaPath);
		conflicts +=
		    sccs_resolveFile(s, localPath, gcaPath, remotePath);
	}
	sccs_free(s);
	for (p = patchList; p; ) {
		patch	*next = p->next;

		unlink(p->initFile);
		free(p->initFile);
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
	patchList = 0;
	no = 0;
	line = 0;
	if (gcaPath) free(gcaPath);
	return (0);
}

/*
 * Include up to but not including gca in the list.
 */
int
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
		sprintf(tmpf, "RESYNC/BitKeeper/tmp/%03d-init", ++no);
		unless (t = fopen(tmpf, "w")) {
			perror(tmpf);
			exit(0);
		}
		s->rstart = s->rstop = d;
		sccs_restart(s);
		sccs_prs(s, PATCH|SILENT, 0, NULL, t);
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
		sprintf(tmpf, "RESYNC/BitKeeper/tmp/%03d-diffs", no);
		unless (d->flags & D_META) {
			p->diffFile = strdup(tmpf);
			sccs_restart(s);
			if (sccs_getdiffs(s, d->rev, 0, tmpf)) {
				fprintf(stderr, "unable to create diffs");
				cleanup(CLEAN_RESYNC);
			}
		} else {
			p->flags |= PATCH_META;
		}
		unless (d->date || streq("70/01/01 00:00:00", d->sdate)) {
			assert(d->date);
		}
		sccs_sdelta(tmpf, d->parent);
		p->pid = strdup(tmpf);
		sccs_sdelta(tmpf, d);
		p->me = strdup(tmpf);
		p->order = d->date;
		if (echo>5) {
			fprintf(stderr,
			    "LOCAL: %s %s %u\n", d->rev, p->me, p->order);
		}
		insertPatch(p);
		nfound++;
		n++;
	}
	return (n);
}

/*
 * Insert the delta in the list in sorted time order.
 */
void
insertPatch(patch *p)
{
	patch	*t;

	if (!patchList || (patchList->order > p->order)) {
		p->next = patchList;
		patchList = p;
		return;
	}
	/*
	 * We know that t is pointing to a node that is older than us.
	 */
	for (t = patchList; t->next; t = t->next) {
		if (t->next->order > p->order) {
			p->next = t->next;
			t->next = p;
			return;
		}
	}

	/*
	 * There is no next field and we know that t->order is < date.
	 */
	assert(t->order <= p->order);
	t->next = p;
}

/*
 * Create enough stuff that the tools can find the project root.
 */
void
initProject()
{
	unless (emptyDir(".")) {
		fprintf(stderr,
		    "takepatch: -i can only be used in an empty directory\n");
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
FILE *
init(FILE *p, int flags)
{
	char	buf[MAXPATH];
	char	file[MAXPATH];
	int	i;
	int	started = 0;
	FILE	*f, *g;

	if (newProject) {
		initProject();
		goto tree;
	}

	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "takepatch: can't find project root.\n");
		exit(1);
	}

tree:
	/*
	 * See if we can lock the tree.
	 */
	if ((mkdir("RESYNC", 0775) == -1) && (access("RESYNC", F_OK) == 0)) {
		unless ((f = fopen("RESYNC/BitKeeper/tmp/pid", "r")) &&
		    fnext(buf, f)) {
			fprintf(stderr, "takepatch: RESYNC dir exists\n");
			exit(1);
		}
		fclose(f);
		chop(buf);
		f = fopen("RESYNC/BitKeeper/tmp/patch", "r");
		if (fnext(file, f)) {
			chop(file);
			fprintf(stderr,
			    "takepatch: RESYNC dir locked by %s for patch %s\n",
			    buf, file);
		} else {
			fprintf(stderr,
			    "takepatch: RESYNC dir locked by %s\n", buf);
		}
		exit(1);
	}
	unless (mkdir("RESYNC/SCCS", 0775) == 0) {
		perror("mkdir");
		exit(1);
	}
	sccs_mkroot("RESYNC");
	unless (f = fopen("RESYNC/BitKeeper/tmp/pid", "w")) {
		perror("RESYNC/BitKeeper/tmp/pid");
		exit(1);
	}
	fprintf(f, "%d\n", getpid());
	fclose(f);

	if (p == stdin) {
		/*
		 * Save the patch in the pending dir
		 * and record we're working on it.
		 */
		if (!isdir("PENDING") && (mkdir("PENDING", 0775) == -1)) {
			perror("PENDING");
			exit(1);
		}
		for (i = 1; ; i++) {				/* CSTYLED */
			struct	tm *tm;
			time_t	now = time(0);

			tm = localtime(&now);
			strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
			sprintf(pendingFile, "PENDING/%s.%02d", buf, i);
			if (exists(pendingFile)) continue;
			if (f = fopen(pendingFile, "w+")) {
				break;
			}
			if (i > 100) {
				fprintf(stderr,
				    "takepatch: too many patches.\n");
				exit(1);
			}
		}
		unless (g = fopen("RESYNC/BitKeeper/tmp/patch", "w")) {
			perror("RESYNC/BitKeeper/tmp/patch");
			exit(1);
		}
		fprintf(g, "%s\n", pendingFile);
		fclose(g);

		/*
		 * Save patch first, making sure it is on disk.
		 */
		while (fnext(buf, p)) {
			if (!started && streq(buf, PATCH_VERSION)) started = 1;
			if (started) {
				if (fputs(buf, f) <= 0) {
					perror("fputs on patch");
					cleanup(CLEAN_PENDING|CLEAN_RESYNC);
				}
			} else {
				if (echo > 4) {
					fprintf(stderr, "Discard: %s", buf);
				}
			}
		}
		unless (started) {
			fprintf(stderr,
"takepatch: nothing to do in patch, which probably means a patch version\n\
mismatch.  You need to make sure that the software generating the patch is\n
the same as the software accepting the patch.  We were looking for\n\
%s", PATCH_VERSION);
		}
		if (fflush(f) || fsync(fileno(f))) {
			perror("fsync on patch");
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
		unless (flags & SILENT) {
			fprintf(stderr,
			    "takepatch: saved patch in %s\n", pendingFile);
		}
		fseek(f, 0, 0);
	} else {
		f = p;
	}
	fnext(buf, f);		/* skip version number */

	if (newProject) {
		unless (idDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) {
			perror("mdbm_open");
			exit(1);
		}
		return (f);
	}

	unless (idDB = loadDB("SCCS/x.id_cache", 0)) {
		perror("SCCS/x.id_cache");
		exit(1);
	}
	return (f);
}

/*
 * Creat the directory for the destination file.
 */
int
mkdirp(char *file)
{
	char	*s = strrchr(file, '/');
	char	*t;
	char	buf[MAXPATH];

	strcpy(buf, file);	/* for !writable string constants */
	unless (s = strrchr(buf, '/')) return (0);
	*s = 0;
	for (t = buf; t < s; ) {
		if (t > buf) *t++ = '/';
		if (t < s) {
			while ((*t != '/') && (t < s)) t++;
			*t = 0;
		}
		mkdir(buf, 0775);
	}
	*s = '/';
	return (0);
}

int
fileCopy(char *from, char *to)
{
	char	buf[8192];
	int	n, from_fd, to_fd;
	struct	stat sb;

	mkdirp(to);
	if ((from_fd = open(from, 0, 0)) == -1) {
		perror(from);
		cleanup(CLEAN_RESYNC);
	}
	if (fstat(from_fd, &sb) == -1) {
		perror(from);
		cleanup(CLEAN_RESYNC);
	}
	if ((to_fd = creat(to, sb.st_mode & 0777)) == -1) {
		perror(to);
		cleanup(CLEAN_RESYNC);
	}
	while ((n = read(from_fd, buf, sizeof(buf))) > 0) {
		if (write(to_fd, buf, n) != n) {
			perror(to);
			cleanup(CLEAN_RESYNC);
		}
	}
	close(from_fd);
	close(to_fd);
	return (0);
}

rebuild_id(char *id)
{
	fprintf(stderr, "takepatch: miss in idcache for %s, rebuilding...", id);
	system("bk sfiles -r");
	if (idDB) mdbm_close(idDB);
	unless (idDB = loadDB("SCCS/x.id_cache", 0)) {
		perror("SCCS/x.id_cache");
		exit(1);
	}
	fprintf(stderr, "done\n");
}

cleanup(int what)
{
	if (what & CLEAN_RESYNC) {
		assert(exists("RESYNC"));
		system("/bin/rm -rf RESYNC");
	} else {
		fprintf(stderr, "takepatch: RESYNC directory left intact.\n");
	}
	if (input != stdin) exit(1);
	if (what & CLEAN_PENDING) {
		assert(exists("PENDING"));
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
	exit(1);
}
