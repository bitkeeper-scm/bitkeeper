/* Copyright (c) 1999 L.W.McVoy */
#include "sccs.h"
WHATSTR("%W%");

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
char *takepatch_help = "usage: takepatch [-v] [-i]\n";

#define	CLEAN_RESYNC	1
#define	CLEAN_PENDING	2

delta	*getRecord(FILE *f);
int	extractPatch(FILE *p, int flags);
int	extractDelta(char *name, sccs *s, int newFile, FILE *f, int flags);
int	applyPatch(int flags);
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
delta	*gca;		/* only gets set if there are conflicts */
int	nfound;
char	pendingFile[MAXPATH];

int
main(int ac, char **av)
{
	char	buf[MAXLINE];
	FILE	*p;
	int	c;
	int	flags = SILENT;

	debug_main(av);
	while ((c = getopt(ac, av, "iv")) != -1) {
		switch (c) {
		    case 'i': newProject++; break;
		    case 'v': echo++; flags &= ~SILENT; break;
		    default: goto usage;
		}
	}
	if (av[optind]) {
usage:		fprintf(stderr, takepatch_help);
		return (1);
	}

	p = init(stdin, flags);

	/*
	 * Find a file and go do it.
	 */
	while (fnext(buf, p)) {
		++line;
		if (echo>3) fprintf(stderr, "%s", buf);
		unless (strncmp(buf, "== ", 3) == 0) {
			fprintf(stderr, "skipping: %s", buf);
			continue;
		}
		extractPatch(p, flags);
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
 */
int
extractPatch(FILE *p, int flags)
{
	delta	*tmp;
	sccs	*s = 0;
	sccs	*perfile = 0;
	datum	k, v;
	char	*name;
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
	if (strneq("New file: ", buf, 10)) {
		newFile = 1;
		name = name2sccs(&buf[10]);
		perfile = sccs_getperfile(p, &line);
		fnext(buf, p);
		chop(buf);
		line++;
	}
	if (echo>3) fprintf(stderr, "%s\n", buf);
	k.dptr = buf;
	k.dsize = strlen(buf) + 1;
again:	v = mdbm_fetch(idDB, k);
	unless (v.dptr || newFile) {
		unless (rebuilt++) {
			rebuild_id();
			goto again;
		}
		fprintf(stderr,
		    "takepatch: can't find key '%s' in id cache\n", buf);
		cleanup(CLEAN_RESYNC);
	}

	/*
	 * They may have sent us a patch from 1.1, so the patch looks like a
	 * new file.  But if we have a match, we want to use it.
	 */
	if (v.dptr) {
		name = name2sccs(v.dptr);
		unless ((s = sccs_init(name, flags)) && HAS_SFILE(s)) {
			fprintf(stderr,
			    "takepatch: can't find file '%s'\n", name);
			exit(1);
		}
		if (newFile && (echo > 3)) {
			fprintf(stderr,
			    "takepatch: new file %s already exists.\n", name);
		}
		if (IS_EDITED(s)) {
			fprintf(stderr,
			    "takepatch: %s is edited\n", name);
			cleanup(CLEAN_RESYNC);
		}
		if (s->state & PFILE) {
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
		s = sccs_init(name, NEWFILE);
		unless (s) perror(name);
		assert(s);
		if (echo > 2) {
			fprintf(stderr,
			    "takepatch: new file %s\n", name);
		}
		if (perfile) {
			sccscopy(s, perfile);
			if (perfile->defbranch) free(perfile->defbranch);
			if (perfile->text) freeLines(perfile->text);
			free(perfile);
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
		fprintf(stderr,
		    "takepatch: %d new revision%s in %s ",
		    nfound, nfound == 1 ? "" : "s", gfile);
		if (echo != 2) fprintf(stderr, "\n");
	}
	if (patchList && gca) getLocals(s, gca, name);
	applyPatch(flags);
	if (echo == 2) fprintf(stderr, " \n");
	sccs_free(s);
	free(gfile);
	free(name);
	return (0);
}

sccscopy(sccs *to, sccs *from)
{
	unless (to && from) return;
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
	if (parent = sccs_findKey(s, buf)) {
		/*
		 * This weird bit of code will handle out of order deltas
		 * in the patch.
		 */
		if (gca) {
			for (d = parent; d; d = d->parent) {
				if (d == gca) break;
			}
			/* if not found, this is higher so use it */
			if (!d) gca = parent;
		} else {
			gca = parent;
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
			fputs(buf, t);
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
		p->localFile = strdup(name);
		sprintf(tmpf, "RESYNC/%s", name);
		p->resyncFile = strdup(tmpf);
		sprintf(tmpf, "RESYNC/BitKeeper/tmp/%03d-diffs", no);
		p->diffFile = strdup(tmpf);
		p->order = parent == d ? 0 : d->date;
		if (newFile) p->init = s;
		if (echo>5) fprintf(stderr, "REM: %s %s %u\n", d->rev, p->me, p->order);
		unless (t = fopen(tmpf, "w")) {
			perror(tmpf);
			exit(0);
		}
		c = line;
		while (fnext(buf, f) && !streq("\n", buf)) {
			fputs(buf, t);
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
 * XXX - the new file stuff doesn't work yet and when it does, it needs to
 * set the bitkeeper flag.
 * If the file does exist, copy it, rip out the old stuff, and then apply
 * the list.
 */
int
applyPatch(int flags)
{
	patch	*p = patchList;
	patch	*p2;
	FILE	*iF, *dF;
	sccs	*s = 0;
	delta	*d;
	int	newflags;
	char	*getuser(), *now();
	char	*localPath = 0, *remotePath = 0;
	static	char *spin = "|/-\\";
	int	n = 0;

	unless (p) return (0);
	if (echo == 2) fprintf(stderr, "%c\b", spin[n++ % 4]);
	unless (p->localFile && exists(p->localFile)) {
		mkdirp(p->resyncFile);
		goto apply;
	}
	if (fileCopy(p->localFile, p->resyncFile)) {
		perror("cp");
		exit(1);
	}
	unless (s = sccs_init(p->resyncFile, NOCKSUM|flags)) {
		fprintf(stderr, "takepatch: can't open %s\n", p->resyncFile);
		exit(1);
	}
	if (!s->tree) {
		if (!(s->state & SFILE)) {
			fprintf(stderr,
			    "takepatch: no s.file %s\n", p->resyncFile);
		} else {
			perror(s->sfile);
		}
		exit(1);
	}
	unless (gca) goto apply;
	assert(gca);
	assert(gca->rev);
	assert(gca->pathname);
	if (echo > 5) fprintf(stderr, "rmdel %s from %s\n", gca->rev, s->sfile);
	if (sccs_rmdel(s, gca->rev, 1, 0)) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr, "rmdel of %s failed.\n", p->resyncFile);
		}
		exit(1);
	}
	sccs_free(s);
	/* sccs_restart does not rebuild the graph and we just pruned it,
	 * so do a hard restart.
	 */
	unless (s = sccs_init(p->resyncFile, NOCKSUM|flags)) {
		fprintf(stderr,
		    "takepatch: can't open %s\n", p->resyncFile);
		exit(1);
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
			if (p->flags & PATCH_META) {
				if (sccs_meta(s, d, p->initFile)) {
					perror("meta");
					exit(1);
				}
			} else {
				newflags = NOCKSUM|SILENT|SKIPGET|EDIT;
				/* CSTYLED */
				if (sccs_get(s, d->rev, 0,0,0, newflags, "-")) {
					perror("get");
					exit(1);
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
				    NOCKSUM|FORCE|BRANCHOK|PATCH :
				    NOCKSUM|FORCE|BRANCHOK|PATCH|SILENT;
				if (sccs_delta(s, newflags, 0, iF, dF)) {
					perror("delta");
					exit(1);
				}
				if (s->state & BAD_DSUM) cleanup(CLEAN_RESYNC);
				fclose(iF);	/* dF done by delta() */
			}
		} else {
			assert(s == 0);
			assert(p->init);
			unless (s = sccs_init(p->resyncFile, NEWFILE)) {
				fprintf(stderr,
				    "takepatch: can't create %s\n",
				    p->resyncFile);
				exit(1);
			}
			sccscopy(s, p->init);
			iF = fopen(p->initFile, "r");
			dF = fopen(p->diffFile, "r");
			d = 0;
			newflags = (echo > 2) ?
			    NOCKSUM|NEWFILE|FORCE|BRANCHOK|PATCH :
			    NOCKSUM|NEWFILE|FORCE|BRANCHOK|PATCH|SILENT;
			if (newProject &&
			    streq("RESYNC/SCCS/s.ChangeSet", p->resyncFile)) {
				d = calloc(1, sizeof(*d));
				d->rev = strdup("1.0");
				s->state |= ONE_ZERO;
			}
			if (sccs_delta(s, newflags, d, iF, dF)) {
				perror("delta");
				exit(1);
			}
			if (s->state & BAD_DSUM) cleanup(CLEAN_RESYNC);
			fclose(iF);	/* dF done by delta() */
			sccs_free(s);
			s = sccs_init(p->resyncFile, NOCKSUM);
		}
		p = p->next;
	}

	sccs_free(s);
	s = sccs_init(patchList->resyncFile, 0);
	assert(s);
	for (p2 = 0, p = patchList; p; p = p->next) {
		if (p->flags & PATCH_LOCAL) p2 = p;
	}
	if (p2) {
		assert(p2->me);
		d = sccs_findKey(s, p2->me);
	} else {
		if (gca) {
			d = gca;
		} else {
			d = sccs_findKey(s, patchList->me);
		}
	}
	assert(d);
	localPath = d->pathname;
	for (p2 = 0, p = patchList; p; p = p->next) {
		if (p->flags & PATCH_REMOTE) p2 = p;
	}
	assert(p2 && p2->me);
	d = sccs_findKey(s, p2->me);
	assert(d);
	remotePath = d->pathname;
	unless(streq(localPath, remotePath)) {
		conflicts += sccs_resolveFile(s, localPath,
			gca ? gca->pathname : localPath, remotePath);
	} else {
		conflicts += sccs_resolveFile(s, 0, 0, 0);
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
		free(p->localFile);
		free(p->resyncFile);
		if (p->pid) free(p->pid);
		if (p->me) free(p->me);
		free(p);
		p = next;
	}
	patchList = 0;
	no = 0;
	line = 0;
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
		sccs_prs(s, PATCH|SILENT, NULL, t);
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
			sccs_getdiffs(s, d->rev, 0, tmpf);
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
	}
	return (0);
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
	if (mkdir("BitKeeper", 0775) || mkdir("BitKeeper/etc", 0775)) {
		perror("mkdir");
		exit(1);
	}
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
	int	i, j;
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
	unless (mkdir("RESYNC/BitKeeper", 0775) == 0) {
		perror("mkdir");
		exit(1);
	}
	unless (mkdir("RESYNC/BitKeeper/tmp", 0775) == 0) {
		perror("mkdir");
		exit(1);
	}
	unless (mkdir("RESYNC/BitKeeper/etc", 0775) == 0) {
		perror("mkdir");
		exit(1);
	}
	unless (f = fopen("RESYNC/BitKeeper/tmp/pid", "w")) {
		perror("RESYNC/BitKeeper/tmp/pid");
		exit(1);
	}
	fprintf(f, "%d\n", getpid());
	fclose(f);

	/*
	 * Save the patch in the pending dir and record we're working on it.
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
			fprintf(stderr, "takepatch: too many patches.\n");
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
			fputs(buf, f);
		} else {
			printf("Discard: %s", buf);
		}
	}
	fflush(f);
	fsync(fileno(f));
	unless (flags & SILENT) {
		fprintf(stderr, "takepatch: saved patch in %s\n", pendingFile);
	}
	fseek(f, 0, 0);
	fnext(buf, f);		/* skip version number */

	if (newProject) {
		unless (idDB = mdbm_open(NULL, 0, 0, 4096)) {
			perror("mdbm_open");
			exit(1);
		}
		mdbm_pre_split(idDB, 1<<10);
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
	char	buf[MAXPATH];

	if (!s) return (0);
	*s = 0;
	unless (isdir(file)) {
		sprintf(buf, "mkdir -p %s\n", file);
		system(buf);
	}
	*s = '/';
	return (0);
}

int
fileCopy(char *from, char *to)
{
	char	*s = malloc(strlen(from) + strlen(to) + 20);
	char	*t;

	t = rindex(to, '/');
	assert(t);
	*t = 0;
	sprintf(s, "mkdir -p %s", to);
	system(s);
	*t = '/';
	sprintf(s, "cp -p %s %s", from, to);
	system(s);
	free(s);
	return (0);
}

rebuild_id()
{
	fprintf(stderr, "takepatch: miss in idcache, rebuilding...\n");
	system("bk sfiles -r");
	if (idDB) mdbm_close(idDB);
	unless (idDB = loadDB("SCCS/x.id_cache", 0)) {
		perror("SCCS/x.id_cache");
		exit(1);
	}
}

cleanup(int what)
{
	if (what & CLEAN_RESYNC) {
		assert(exists("RESYNC"));
		system("/bin/rm -rf RESYNC");
	} else {
		fprintf(stderr, "takepatch: RESYNC directory left intact.\n");
	}
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
