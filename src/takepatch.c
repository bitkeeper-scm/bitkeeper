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
delta	*gca;
int	nfound;
char	pendingFile[1024];

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
		    "takepatch: %d new revision%s, %d conflicts in %d files\n",
		    remote, remote > 1 ? "s" : "", conflicts, files);
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
	datum	k, v;
	char	*name;
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
			    "takepatch: can't find key '%s' in id cache\n",
			    buf);
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
	remote += nfound;
	gfile = sccs2name(name);
	if (echo>1) {
		fprintf(stderr,
		    "takepatch: %d new revision%s in %s ",
		    nfound, nfound > 1 ? "s" : "", gfile);
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
		p->order = parent == d ? 0 : d->date;
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
	unless (s = sccs_init(p->resyncFile, flags)) {
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
				fprintf(stderr,
				    "takepatch: can't find PID %s in %s\n",
				    p->pid, s->sfile);
				if (echo>6) {
					char	buf[1024];

					sprintf(buf,
					    "echo --- %s ---; cat %s", 
					    p->initFile, p->initFile);
					system(buf);
				}
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
				/* CSTYLED */
				if (sccs_get(s, d->rev, 0,0,0, newflags, "-")) {
					perror("get");
					exit(1);
				}
				if (echo > 6) {
					char	buf[1024];

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
	static	char tmpf[1024];	/* don't allocate on stack */

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
	char	buf[1024];
	char	file[1024];
	ino_t	slash = 0;
	struct	stat sb;
	int	i, j;
	FILE	*f, *g;

	if (newProject) {
		initProject();
		goto tree;
	}

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
		if (!buf[0]) getcwd(buf, sizeof(buf));
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
		if (f = fopen(pendingFile, "w+")) {
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
		fprintf(stderr, "takepatch: saving patch in %s\n", pendingFile);
	}
	fprintf(g, "%s\n", pendingFile);
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
		system("/bin/rm -rf PENDING");
	} else {
		if (exists(pendingFile)) {
			fprintf(stderr, "takepatch: patch left in %s\n",
			    pendingFile);
		}
	}
	exit(1);
}
