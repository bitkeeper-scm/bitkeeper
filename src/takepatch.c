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
char *takepatch_help = "usage: takepatch [-v]\n";

int	echo = 0;
int	line;
int	no;
patch	*patchList = 0;
FILE	*init(FILE *, int);
delta	*getRecord(FILE *f);
int	extractPatch(FILE *p, int flags);
int	applyPatch();
int	extractDelta(char *name, sccs *s, int nf, FILE *f, int flags);
int	getLocals(sccs *s, delta *d, char *name);
void	insertPatch(patch *p);
int	fileCopy(char *from, char *to);
int	files, local, remote;
int	newProject;
MDBM	*idDB;
delta	*gca;
int	nfound;

int
main(int ac, char **av)
{
	char	buf[1024];
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
		    "takepatch: %d remote, %d local revisions in %d files\n",
		    remote, local, files);
	}
	exit(0);
}

delta *
getRecord(FILE *f)
{
	int	e = 0;
	delta	*d = sccs_getInit(0, f, 1, &e, 0);
	int	c;

	if (!d || e) {
		fprintf(stderr,
		    "takepatch: bad delta record near line %d\n", line);
		exit(1);
	}
	d->date = sccs_date2time(d->sdate, d->zone, 0);
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
	long	off;
	datum	k, v;
	char	*name, *t;
	int	newFile = 0;
	char	*gfile;
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
	 * D 1.1 99/02/23 00:29:01-08:00 lm@lm.bitmover.com +128 -0
	 * etc.
	 */
	files++;
	fnext(buf, p); chop(buf);
	line++;
	if (echo>3) fprintf(stderr, "%s\n", buf);
	if (strneq("New file: ", buf, 10)) newFile = 1;
	k.dptr = buf;
	k.dsize = strlen(buf) + 1;
	v = mdbm_fetch(idDB, k);
	unless (v.dptr) {
		unless (newFile) {
			fprintf(stderr,
			    "takepatch: can't find key '%s' in id cache\n", buf);
			exit(1);
		}
	}
	unless (newFile) {
		name = name2sccs(v.dptr);
		unless ((s = sccs_init(name, flags)) && HAS_SFILE(s)) {
			fprintf(stderr,
			    "takepatch: can't find file '%s'\n", name);
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
		name = name2sccs(&buf[10]);
		s = sccs_init(name, NEWFILE);
		unless (s) perror(name);
		assert(s);
	}
	gca = 0;
	nfound = 0;
	while (extractDelta(name, s, newFile, p, flags)) {
		if (newFile) newFile = 2;
	}
	gfile = sccs2name(name);
	if (patchList && gca) {
		int	rem = nfound;

		remote += nfound;
		nfound = 0;
		getLocals(s, gca->kid, name);
		if (echo>1) {
			fprintf(stderr,
			    "takepatch: %d remote, %d local revisions in %s\n",
			    rem, nfound, gfile);
		}
		local += nfound;
	} else {
		if (echo>1) {
			fprintf(stderr,
			    "takepatch: %d remote revisions for %s\n",
			    nfound, gfile);
		}
		remote += nfound;
	}
	free(gfile);
	sccs_free(s);
	applyPatch(flags);
	free(name);
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
	char	tmpf[1024];
	char	buf[1024];
	char	*pid = 0;
	long	off;
	int	c;
	int	skip = 0;
	patch	*p;

	if (newFile == 1) goto delta;

	fnext(buf, f); chop(buf); line++;
	if (echo>3) fprintf(stderr, "%s\n", buf);
	pid = strdup(buf);
	unless (gca) {
		unless (parent = sccs_findKey(s, buf)) {
			unless (newFile) {
				fprintf(stderr,
				    "takepatch: can't find parent %s in %s ",
				    buf, s->sfile);
				fprintf(stderr,
				    "near line %d in patch\n", line);
				exit(1);
			}
		}
		gca = parent;
	}

	/* go get the delta table entry for this delta */
delta:	off = ftell(f);
	d = getRecord(f);
	sccs_sdelta(buf, d);
	if (tmp = sccs_findKey(s, buf)) {
		fprintf(stderr,
		    "takepatch: delta %s already in %s, skipping it.\n",
		    tmp->rev, s->sfile);
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
		p->order = parent == d ? 0 : d->date + d->dateFudge;
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

char *
findLeaf(sccs *s, int flag)
{
	patch	*p, *r = 0;
	delta	*d, *e;

	for (p = patchList, r = 0; p; p = p->next) {
		if (p->flags & flag) r = p;
	}
	unless (r) return ("none");
	e = sccs_findKey(s, r->me);
	assert(e);
	return (e->rev);
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
	FILE	*f;
	FILE	*iF, *dF;
	sccs	*s = 0;
	delta	*d;
	char	*t, *leftRev = 0, *rightRev = 0, *gcaRev = "none";
	int	newflags;
	char	*getuser(), *now();

	unless (p) return (0);
	unless (gca) {
		mkdirp(p->resyncFile);
		goto apply;
	}
	if (fileCopy(p->localFile, p->resyncFile)) {
		perror("cp");
		exit(1);
	}
	unless (s = sccs_init(p->resyncFile, flags)) {
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
	assert(gca);
	if (sccs_rmdel(s, gca->rev, 1, 0)) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr, "rmdel of %s failed.\n", p->resyncFile);
		}
		exit(1);
	}
	gcaRev = strdup(gca->rev);
	sccs_free(s);
	/* sccs_restart does not rebuild the graph and we just pruned it,
	 * so do a hard restart.
	 */
	unless (s = sccs_init(p->resyncFile, flags)) {
		fprintf(stderr,
		    "takepatch: can't open %s\n", p->resyncFile);
		exit(1);
	}
apply:
	p = patchList;
	while (p) {
		char	cmd[2048];

		if (p->pid) {
			unless (d = sccs_findKey(s, p->pid)) {
				fprintf(stderr,
				    "takepatch: can't find %s in %s\n",
				    p->pid, s->sfile);
				exit(1);
			}
			unless (sccs_restart(s)) { perror("restart"); exit(1); }
			if (p->flags & PATCH_META) {
				if (sccs_meta(s, d, p->initFile)) {
					perror("meta");
					exit(1);
				}
			} else {
				newflags = NOCKSUM|SILENT|SKIPGET|EDIT;
				if (sccs_get(s, d->rev, 0, 0, newflags, "-")) {
					perror("get");
					exit(1);
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
				fclose(iF);	/* dF done by delta() */
			}
		} else {
			assert(s == 0);
			unless (s = sccs_init(p->resyncFile, NEWFILE)) {
				fprintf(stderr,
				    "takepatch: can't create %s\n",
				    p->resyncFile);
				exit(1);
			}
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
			fclose(iF);	/* dF done by delta() */
			sccs_free(s);
			s = sccs_init(p->resyncFile, 0);
		}
		p = p->next;
	}

	/*
	 * Create resolve file.
	 * XXX - this is also where we would handle pathnames, symbols, etc.
	 */
	sccs_free(s);
	s = sccs_init(patchList->resyncFile, 0);
	assert(s);
	leftRev = findLeaf(s, PATCH_REMOTE);
	rightRev = findLeaf(s, PATCH_LOCAL);
	t = strrchr(patchList->resyncFile, '/');
	assert(t);
	assert(t[-1] == 'S');
	assert(t[1] == 's');
	t[1] = 'r';
	f = fopen(patchList->resyncFile, "w");
	fprintf(f, "merge deltas %s %s %s %s %s\n",
	    leftRev, gcaRev, rightRev, getuser(), now());
	fclose(f);
	sccs_free(s);
	for (p = patchList; p; ) {
		patch	*next = p->next;

		unlink(p->initFile);
		free(p->initFile);
		unlink(p->diffFile);
		free(p->diffFile);
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
 * Given a parent node, include all descendants of that node in the list.
 */
int
getLocals(sccs *s, delta *d, char *name)
{
	FILE	*t;
	patch	*p;
	static	char tmpf[1024];	/* don't allocate on stack */

	if (!d) return (0);

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
	p->diffFile = strdup(tmpf);
	sccs_restart(s);
	sccs_getdiffs(s, d->rev, 0, tmpf);
	d->date = sccs_date2time(d->sdate, d->zone, 0);
	sccs_sdelta(tmpf, d->parent);
	p->pid = strdup(tmpf);
	sccs_sdelta(tmpf, d);
	p->me = strdup(tmpf);
	p->order = d->date;
	insertPatch(p);
	nfound++;
	getLocals(s, d->kid, name);
	getLocals(s, d->siblings, name);
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
 * Go find the change set file and do this relative to that.
 * Create the RESYNC dir or bail out if it exists.
 * Put our pid in that dir so that we can figure out if
 * we are still here.
 */
FILE *
init(FILE *p, int flags)
{
	char	buf[1024];
	char	file[1024];
	ino_t	slash = 0;
	struct	stat sb;
	int	i, j;
	FILE	*f, *g;

	if (newProject) goto tree;

	if (stat("/", &sb)) {
		perror("stat of /");
		exit(-1);
	}
	slash = sb.st_ino;

	/*
	 * Now work backwards up the tree until we find a ChangeSet or /
	 */
	for (i = 0; ; i++) {				/* CSTYLED */
		buf[0] = 0;
		for (j = 0; j < i; ++j) strcat(buf, "../");
		sprintf(file, "%s%s", buf, CHANGESET);
		if (access(file, R_OK) == 0) {
			chdir(buf);
			break;
		}
		if (stat(buf[0] ? buf : ".", &sb) == -1) {
			perror(buf);
			exit(1);
		}
		if (sb.st_ino == slash) {
			fprintf(stderr,
			    "takepatch: No %s found, can't apply patch.\n",
			    CHANGESET);
			exit(1);
		}
	}

	unless (flags & SILENT) {
		fprintf(stderr, "takepatch: using %s as project root\n", buf);
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
	unless (mkdir("RESYNC/BitKeeper", 0775) == 0) {
		perror("mkdir");
		exit(1);
	}
	unless (mkdir("RESYNC/BitKeeper/tmp", 0775) == 0) {
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
		sprintf(file, "PENDING/%s.%02d", buf, i);
		if (f = fopen(file, "w+")) {
			break;
		}
		if (i > 10) {
			fprintf(stderr, "Unknown error, can't save patch.\n");
			exit(1);
		}
		sleep(1);
	}
	unless (g = fopen("RESYNC/BitKeeper/tmp/patch", "w")) {
		perror("RESYNC/BitKeeper/tmp/patch");
		exit(1);
	}
	unless (flags & SILENT) {
		fprintf(stderr, "takepatch: saving patch in %s\n", file);
	}
	fprintf(g, "%s\n", file);
	fclose(g);

	/*
	 * Save patch first, making sure it is on disk.
	 */
	while (fnext(buf, p)) {
		fputs(buf, f);
	}
	fflush(f);
	fsync(fileno(f));
	fseek(f, 0, 0);

	unless (idDB = mdbm_open(NULL, 0, 0, 4096)) {
		perror("mdbm_open");
		exit(1);
	}
	mdbm_pre_split(idDB, 1<<10);
	if (newProject) return (f);

	/*
	 * Make sure the caches are there.
	 */
	unless (exists("SCCS/x.id_cache") && exists("SCCS/x.pending_cache")) {
		verbose((stderr, "takepatch: rebuilding name cache.\n"));
		system("sfiles -r");
	}

	/*
	 * Build the in memory name cache.
	 */
	unless (g = fopen("SCCS/x.id_cache", "r")) {
		perror("SCCS/x.id_cache");
		exit(1);
	}
	while (fnext(buf, g)) {
		char	*s = strchr(buf, ' ');
		datum	k, v;

		/* Ignore comments */
		if (buf[0] == '#') continue;
		if (!s) {
			fprintf(stderr, "takepatch: corrupted name cache\n");
			fprintf(stderr, "takepatch: '%s'\n", buf);
			exit(1);
		}
		*s++ = 0;
		k.dptr = buf;
		k.dsize = strlen(buf) + 1;
		chop(s);
		v.dptr = s;
		v.dsize = strlen(s) + 1;
		if (mdbm_store(idDB, k, v, MDBM_INSERT)) {
			fprintf(stderr, "Duplicate key in name cache\n");
			exit(1);
		}
	}
	fclose(g);
	return (f);
}

/*
 * Creat the directory for the destination file.
 */
int
mkdirp(char *file)
{
	char	*s = strrchr(file, '/');
	char	buf[1024];

	if (!s) return (0);
	*s = 0;
	sprintf(buf, "mkdir -p %s\n", file);
	system(buf);
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
