/* Copyright (c) 1999 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
#include "zlib.h"

#include "comments.c"
#include "host.c"
#include "user.c"

WHATSTR("@(#)%K%");

char	*cset_help = "\n\
usage: cset [opts]\n\n\
    -c		like -m, except generate only ChangeSet diffs\n\
    -d<range>	do unified diffs for the range\n\
    -C		clear and remark all ChangeSet boundries\n\
    -i		Create a new change set history rooted at <root>\n\
    -l<range>	List each rev in range as file:rev,rev,rev (set format)\n\
    -m<range>	Generate a patch of the changes in <range>\n\
    -M<range>	Mark the files included in the range of csets\n\
    -p		print the list of deltas being added to the cset\n\
    -r<range>	List the filenames:rev..rev which match <range>\n\
    		ChangeSet is not included in the listing\n\
    -R<range>	Like -r but start on rev farther back (for diffs)\n\
    		ChangeSet is not included in the listing\n\
    -s		Run silently\n\
    -S<sym>	Set <sym> to be a symbolic tag for this revision\n\
    -t<rev>	List the filenames:revisions of the repository as of rev\n\
    -y<msg>	Sets the changeset comment to <msg>\n\
    -Y<file>	Sets the changeset comment to the contents of <file>\n\
    \nRanges of revisions may be specified with the -l or the -r options.\n\
    -l1.3..1.5 does 1.3, 1.4, and 1.5\n\n\
    Useful idioms:\n\t\
    bk cset -Ralpha..beta | bk diffs -\n\t\
    bk cset -ralpha..beta | bk sccslog -\n\n";

typedef	struct cset {
	/* bits */
	int	mixed;		/* if set, then both long and short keys */
	int	csetOnly;	/* if set, do logging ChangeSet */
	int	makepatch;	/* if set, act like makepatch */
	int	listeach;	/* if set, like -r except list revs 1/line */
	int	mark;		/* act like csetmark used to act */
	int	doDiffs;	/* prefix with unified diffs */
	int	force;		/* if set, then force past errors */
	int	remark;		/* clear & redo all the ChangeSet marks */
	int	dash;
	int	newlod;

	/* numbers */
	int	verbose;
	int	range;		/* if set, list file:rev..rev */
				/* if set to 2, then list parent..rev */
	int	ndeltas;
	int	nfiles;
	pid_t	pid;		/* adler32 process id */
	/* hashes */
	MDBM	*tot;
	MDBM	*base;
} cset_t;

int	csetCreate(sccs *cset, int flags, char *sym, int newlod);
int	csetInit(sccs *cset, int flags, char *text);
void	csetlist(cset_t *cs, sccs *cset);
void	csetList(sccs *cset, char *rev, int ignoreDeleted);
int	marklist(char *file, int newlod, MDBM *tot, MDBM *base);
void	csetDeltas(cset_t *cs, sccs *sc, delta *start, delta *d);
void	dump(MDBM *db, FILE *out);
int	sameState(const char *file, const struct stat *sb);
void	lock(char *);
void	unlock(char *);
delta	*mkChangeSet(sccs *cset, FILE *diffs);
void	explodeKey(char *key, char *parts[4]);
char	*file2str(char *f);
void	doRange(cset_t *cs, sccs *sc);
void	doSet(sccs *sc);
void	doMarks(cset_t *cs, sccs *sc);
void	doDiff(sccs *sc, int kind);
void	sccs_patch(sccs *, cset_t *);
void	cset_exit(int n);

private void	mklod(sccs *s, delta *start, delta *stop);

FILE	*id_cache;
MDBM	*idDB = 0;
char	csetFile[] = CHANGESET; /* for win32, need writable	*/
				/* buffer for name convertion	*/

cset_t	cs1;		/* an easy way to create a local that is zeroed */
char	*spin = "|/-\\";

/*
 * cset.c - changeset command
 */
int
main(int ac, char **av)
{
	sccs	*cset;
	int	flags = 0;
	int	c, list = 0;
	char	*sym = 0, *text = 0;
	int	cFile = 0, ignoreDeleted = 0;
	char	allRevs[6] = "1.0..";
	int	newlod = 0;
	RANGE_DECL;

	platformSpecificInit(NULL);
	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "%s", cset_help);
		return (1);
	}
	if (streq(av[0], "makepatch")) cs1.makepatch++;

	while ((c = getopt(ac, av, "c|Cd|Dfi|l|Lm|M|pqr|R|sS;t;vy|Y|")) != -1) {
		switch (c) {
		    case 'D': ignoreDeleted++; break;
		    case 'i':
			flags |= DELTA_EMPTY|NEWFILE;
			text = optarg;
			break;
		    case 'f': cs1.force++; break;
		    case 'R':
			cs1.range++;
			/* fall through */
		    case 'r':
		    	cs1.range++;
		    	/* fall through */
		    case 'l':
			if (c == 'l') cs1.listeach++;
		    	/* fall through */
		    case 'd':
			if (c == 'd') cs1.doDiffs++;
		    	/* fall through */
		    case 'c':
			if (c == 'c') {
				cs1.csetOnly++;
				cs1.makepatch++;
			}
			/* fall through */
		    case 'M':
			if (c == 'M') cs1.mark++;
			/* fall through */
		    case 'm':
			if (c == 'm') cs1.makepatch++;
		    	list |= 1;
			if (optarg) {
				r[rd++] = notnull(optarg);
				things += tokens(notnull(optarg));
			}
			break;
		    case 'C':
			/* XXX - this stomps on everyone else */
		    	list |= 1;
			cs1.mark++;
			cs1.remark++;
			cs1.force++;
			r[0] = allRevs;
			rd = 1;
			things = tokens(notnull(optarg));
			break;
		    case 't':
			list |= 2;
			r[rd++] = optarg;
			things += tokens(optarg);
			break;
		    case 'p': flags |= PRINT; break;
		    case 'q':
		    case 's': flags |= SILENT; break;
		    case 'v': cs1.verbose++; break;
		    case 'y':
			comment = optarg;
			gotComment = 1;
			flags |= DELTA_DONTASK;
			break;
		    case 'Y':
			comment = file2str(optarg);
			flags |= DELTA_DONTASK;
			gotComment = 1;
			cFile++;
			break;
		    case 'L': newlod++; break; /* XXX: someday with sym */
		    case 'S': sym = optarg; break;

		    default:
			goto usage;
		}
	}

	if (cs1.doDiffs && cs1.csetOnly) {
		fprintf(stderr, "Warning: ignoring -d option\n");
		cs1.doDiffs = 0;
	}
	if (list == 3) {
		fprintf(stderr,
		    "%s: -l and -t are mutually exclusive\n", av[0]);
		goto usage;
	}
	if ((things > 1) && (list != 1)) {
		fprintf(stderr, "%s: only one rev allowed with -t\n", av[0]);
		goto usage;
	}
	if (av[optind] && streq(av[optind], "-")) {
		optind++;
		cs1.dash++;
	}
	if (av[optind]) {
		unless (isdir(av[optind])) {
			if (flags & NEWFILE) {
				char	path[MAXPATH];

				sprintf(path, "mkdir -p %s", av[optind]);
				system(path);
			}
		}
		if (chdir(av[optind])) {
			perror(av[optind]);
			return (1);
		}
	} else if (flags & NEWFILE) {
		fprintf(stderr, "cset: must specify project root.\n");
		return (1);
	} else if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "cset: can not find project root.\n");
		return (1);
	}
	cset = sccs_init(csetFile, flags, 0);
	if (!cset) return (101);
	cs1.mixed = !(cset->state & S_KEY2);

	/*
	 * If we are initializing, then go create the file.
	 * XXX - descriptive text.
	 */
	if (flags & NEWFILE) {
		if (sym) {
			fprintf(stderr, "cset: no symbols allowed with -i.\n");
			cset_exit(1);
		}
		return (csetInit(cset, flags, text));
	}

	if (list) {
#ifdef  ANSIC
		signal(SIGINT, SIG_DFL);
#else
		sig(UNCATCH, SIGINT);
		sig(UNBLOCK, SIGINT);
#endif
	}

	if (list && (things < 1) && !cs1.dash) {
		fprintf(stderr, "cset: must specify a revision.\n");
		sccs_free(cset);
		purify_list();
		cset_exit(1);
	}
	switch (list) {
	    case 1:
		RANGE("cset", cset, cs1.dash ? 0 : 2, 1);
		csetlist(&cs1, cset);
next:		sccs_free(cset);
		if (cFile) free(comment);
		purify_list();
		return (0);
	    case 2:
	    	csetList(cset, r[0], ignoreDeleted);
		sccs_free(cset);
		if (cFile) free(comment);
		purify_list();
		return (0);
	}

	/*
	 * Otherwise, go figure out if we have anything to add to the
	 * changeset file.
	 * XXX - should allow them to pick and choose for multiple
	 * changesets from one pending file.
	 */
	c = csetCreate(cset, flags, sym, newlod);
	if (cFile) free(comment);
	purify_list();
	return (c);
}

void
cset_exit(int n)
{
	if (cs1.pid) {
		fprintf(stderr, "cset: failed to wait for adler32\n");
		waitpid(cs1.pid, 0, 0);
	}
	fflush(stdout);
	_exit(n);
}

/*
 * Spin off a subprocess and rejigger stdout to feed into its stdin.
 * The subprocess will run a checksum over the text of the patch
 * (everything from "# Patch vers:\t0.6" on down) and append a trailer
 * line.
 *
 */
pid_t
spawn_checksum_child(void)
{
	int	p[2], fd0, rc;
	pid_t	pid;
	char	cmd[MAXPATH];
	char	*av[2] = {cmd, 0};

	/* the strange syntax is to hide the call from purify */
	if ((pipe)(p)) {
		perror("pipe");
		return -1;
	}

	/* save fd 0*/
	fd0 = (dup)(0);
	assert(fd0 > 0);


	/* for Child.
	 * Replace stdin with the read end of the pipe.
	 */
	sprintf(cmd, "%s%s", getenv("BK_BIN"), ADLER32);
	rc = (close)(0);
	if (rc == -1) perror("close");
	assert(rc != -1);
	rc = (dup2)(p[0], 0);
	if (rc == -1) perror("dup2");
	assert(rc != -1);

	/* Now go do the real work... */
	pid = spawnvp_ex(_P_NOWAIT, cmd, av );
	if (pid == -1) return -1;

	/*
	 * for Parent
	 * restore fd0
	 * set stdout to write end of the pipe
	 */
	rc = (dup2)(fd0, 0); /* restore stdin */
	assert(rc != -1);
	(close)(1);
	rc = (dup2)(p[1], 1);
	assert(rc != -1);
	(close)(p[0]);
	(close)(p[1]);
	return pid;
}

int
csetInit(sccs *cset, int flags, char *text)
{
	delta	*d = 0;

	/*
	 * Create BitKeeper root.
	 */
	sccs_mkroot(".");


	 // awc to lm:
	 // should we make sure there are no changeset file
	 //in the subdirectory before we proceed ?
	unless (streq(basenm(CHANGESET), basenm(cset->sfile))) {
		fprintf(stderr, "cset: must be -i ChangeSet\n");
		cset_exit(1);
	}
	if (flags & DELTA_DONTASK) unless (d = getComments(d)) goto intr;
	unless(d = getHostName(d)) goto intr;
	unless(d = getUserName(d)) goto intr;
	d->sym = strdup(KEY_FORMAT2);
	cset->state |= S_CSET|S_KEY2;
	if (text) {
		FILE    *desc; 
		char    dbuf[200];

		desc = fopen(text, "rt"); /* must be text mode */
		if (!desc) {
			fprintf(stderr, "admin: can't open %s\n", text);
			goto error;
		}
		assert(cset->text == 0);
		while (fgets(dbuf, sizeof(dbuf), desc)) {
			cset->text = addLine(cset->text, strnonldup(dbuf));
		}
		fclose(desc);
	}
	if (sccs_delta(cset, flags, d, 0, 0) == -1) {
intr:		sccs_whynot("cset", cset);
error:		sccs_free(cset);
		sfileDone();
		commentsDone(saved);
		hostDone();
		userDone();
		purify_list();
		return (1);
	}
	close(creat(IDCACHE, GROUP_MODE));
	sccs_free(cset);
	commentsDone(saved);
	hostDone();
	userDone();
	sfileDone();
	purify_list();
	return (0);
}

void
csetList(sccs *cset, char *rev, int ignoreDeleted)
{
	MDBM	*idDB;				/* db{fileId} = pathname */
	MDBM	*goneDB;
	kvpair	kv;
	char	*t;
	sccs	*sc;
	delta	*d;
	int  	doneFullRebuild = 0;

	if (csetIds(cset, rev)) {
		fprintf(stderr,
		    "Can't find changeset %s in %s\n", rev, cset->sfile);
		cset_exit(1);
	}

	unless (idDB = loadDB(IDCACHE, 0, DB_NODUPS)) {
		if (system("bk sfiles -r")) {
			fprintf(stderr, "cset: can not build %s\n", IDCACHE);
			cset_exit(1);
		}
		doneFullRebuild = 1;
		unless (idDB = loadDB(IDCACHE, 0, DB_NODUPS)) {
			perror("idcache");
			cset_exit(1);
		}
	}
	goneDB = loadDB(GONE, 0, DB_KEYSONLY|DB_NODUPS);
	for (kv = mdbm_first(cset->mdbm);
	    kv.key.dsize != 0; kv = mdbm_next(cset->mdbm)) {
		t = kv.key.dptr;
		unless (sc = sccs_keyinit(t, INIT_NOCKSUM, idDB)) {
			if (gone(t, goneDB)) continue;
			fprintf(stderr, "cset: init of %s failed\n", t);
			cset_exit(1);
		}
		unless (d = sccs_findKey(sc, kv.val.dptr)) {
			fprintf(stderr,
			    "cset: can't find delta '%s' in %s\n",
			    kv.val.dptr, sc->sfile);
			cset_exit(1);
		}
		if (ignoreDeleted) {
			char *p;
			int len;
			/*
			 * filter out deleted file
			 */
			assert(d->pathname);
			p = strrchr(d->pathname, '/');
			p = p ? &p[1] : d->pathname;
			len = strlen(p);
			if ((len >= 6) && (strncmp(p, ".del-", 5) == 0)) {
				sccs_free(sc);
				continue;
			}
		}
		printf("%s%c%s\n", sc->gfile, BK_FS, d->rev);
		sccs_free(sc);
	}
	mdbm_close(idDB);
}

/*
 * Do whatever it is they want.
 */
void
doit(cset_t *cs, sccs *sc)
{
	static	int first = 1;

	unless (sc) {
		if (cs->doDiffs && cs->makepatch) printf("\n");
		first = 1;
		return;
	}
	cs->nfiles++;
	if (cs->doDiffs) {
		doDiff(sc, DF_UNIFIED);
	} else if (cs->makepatch) {
		if (!cs->csetOnly || (sc->state & S_CSET)) {
			sccs_patch(sc, cs);
		}
	} else if (cs->mark) {
		doMarks(cs, sc);
	} else if (cs->range && !cs->listeach) {
		doRange(cs, sc);
	} else {
		doSet(sc);
	}
}

void
header(sccs *cset, int diffs)
{
	char	*dspec =
		"$each(:FD:){# Project:\t(:FD:)}\n# ChangeSet ID: :LONGKEY:";
	int	save = cset->state;
	time_t	t = time(0);
	char	pwd[MAXPATH];

	if (diffs) {
		printf("\
# This is a BitKeeper patch.  What follows are the unified diffs for the\n\
# set of deltas contained in the patch.  The rest of the patch, the part\n\
# that BitKeeper cares about, is below these diffs.\n");
	}
	sccs_prsdelta(cset, cset->tree, 0, dspec, stdout);
	printf("# User:\t\t%s\n", getuser());
	printf("# Host:\t\t%s\n", sccs_gethost() ? sccs_gethost() : "?");
	getcwd(pwd, sizeof(pwd));
	printf("# Root:\t\t%s\n", pwd);
	printf("# Date:\t\t%s", ctime(&t));
	cset->state = save;
}

/*
 * Depending on what we are doing, either
 * mark all deltas in this cset, or
 * just mark the cset boundry.
 */
void
markThisCset(cset_t *cs, sccs *s, delta *d)
{
	if (cs->mark) {
		d->flags |= D_SET;
		return;
	}
	do {
		d->flags |= D_SET;
		if (d->merge) {
			delta	*e = sfind(s, d->merge);

			assert(e);
			unless (e->flags & D_CSET) markThisCset(cs, s, e);
		}
		d = d->parent;
	} while (d && !(d->flags & D_CSET));
}

/*
 * Return true if the two keys describe the same file.
 * If we are in KEY_FORMAT2 it's easy, they match or they don't.
 * Otherwise we'll try short versions.
 */
int
sameFile(cset_t *cs, char *key1, char *key2)
{
	char	*a, *b;
	int	ret;

	if (streq(key1, key2)) return (1);
	unless (cs->mixed) return (0);
	if (a = sccs_iskeylong(key1)) *a = 0;
	if (b = sccs_iskeylong(key2)) *b = 0;
	ret = streq(key1, key2);
	if (a) *a = '|';
	if (b) *b = '|';
	return (ret);
}

int
doKey(cset_t *cs, char *key, char *val)
{
	static	MDBM *idDB;
	static	int doneFullRebuild;
	static	int doneFullRemark;
	static	sccs *sc;
	static	char *lastkey;
	delta	*d;

	/*
	 * Cleanup code, called to reset state.
	 */
	unless (key) {
		if (idDB) {
			mdbm_close(idDB);
			idDB = 0;
		}
		if (lastkey) {
			free(lastkey);
			lastkey = 0;
		}
		if (sc) {
			doit(cs, sc);
			sccs_free(sc);
			sc = 0;
		}
		doneFullRebuild = 0;
		doneFullRemark = 0;
		doit(cs, 0);
		return (0);
	}

	/*
	 * If we have a match, just mark the delta and return,
	 * we'll finish later.
	 *
	 * With long/short keys mixed, we have to be a little careful here.
	 */
	if (lastkey && sameFile(cs, lastkey, key)) {
		unless (d = sccs_findKey(sc, val)) {
			return (cs->force ? 0 : -1);
		}
		markThisCset(cs, sc, d);
		return (0);
	}

	/*
	 * This would be later - do the last file and clean up.
	 */
	if (sc) {
		doit(cs, sc);
		sccs_free(sc);
		free(lastkey);
		sc = 0;
		lastkey = 0;
	}

	/*
	 * Set up the new file.
	 */
	unless (idDB || (idDB = loadDB(IDCACHE, 0, DB_NODUPS))) {
		perror("idcache");
	}
	lastkey = strdup(key);
retry:	sc = sccs_keyinit(lastkey, INIT_NOCKSUM, idDB);
	unless (sc) {
		/* cache miss, rebuild cache */
		unless (doneFullRebuild) {
			mdbm_close(idDB);
			if (cs->verbose) fputs("Rebuilding caches...\n", stderr);
			if (system("bk sfiles -r")) {
				fprintf(stderr,
				    "cset: can not build %s\n", IDCACHE);
			}
			doneFullRebuild = 1;
			unless (idDB = loadDB(IDCACHE, 0, DB_NODUPS)) {
				perror("idcache");
			}
			goto retry;
		}
		if (cs->force < 2) {
			fprintf(stderr,
			    "cset: missing id %s, sfile removed?\n", key);
		}
		free(lastkey);
		lastkey = 0;
		return (cs->force ? 0 : -1);
	}

	/*
 	 * Unless we are here to mark the file, if it isn't marked,
	 * go do that first.  We skip ChangeSet files because the code
	 * path which does the work also seems to skip them (because it
	 * is used for other operations which don't want them).
	 */
	if (!(sc->state & S_CSET) && !(sc->state & S_CSETMARKED) && !cs->mark) {
		char	buf[MAXPATH];

		if (doneFullRemark) {
			fprintf(stderr,
			    "cset: missing cset metadata in %s\n",
			    sc->sfile);
			return (-1);
		}
		fprintf(stderr,
		    "cset: %s has no ChangeSet marks\n\n", sc->sfile);
		fputs(
"\nBitKeeper has found a file which is missing some metadata.  That metadata\n\
is being automatically generated and added to all files.  If your repository\n\
is large, this is going to take a while - it has to rewrite each file.\n\
This is a one time event to upgrade this repository to the latest format.\n\
Please stand by.\n\n", stderr);
		sprintf(buf, "bk cset -M1.0.. %s", cs->verbose ? "-v" : "");
		system(buf);
		doneFullRemark++;
		goto retry;
	}
	unless (d = sccs_findKey(sc, val)) {
		fprintf(stderr,
		    "cset: failed to find %s in %s\n", val, sc->sfile);
		return (-1);
	}
	markThisCset(cs, sc, d);
	return (0);
}

/*
 * Mark the deltas listed in the diff file.  Ignore first line.
 * XXX: change from 0a0 format to I0 0 format
 */
int
marklist(char *file, int newlod, MDBM *tot, MDBM *base)
{
	char	*t;
	FILE	*list;
	char	buf[MAXPATH*2];
	cset_t	cs;

	bzero(&cs, sizeof(cs));
	cs.mark++;
	cs.newlod = newlod;
	cs.tot = tot;
	cs.base = base;

	unless (list = fopen(file, "r")) {
		perror(file);
		return (-1);
	}

	/* eat the first line ... */
	unless (fnext(buf, list)) {
		fprintf(stderr, "cset: marking new list: empty file\n");
		fclose(list);
		return (-1);
	}
	/*
	 * Now do the real data.
	 * XXX: fix when replace 0a0 with I0 0.  &buf[2] => buf
	 */
	while (fnext(buf, list)) {
		chop(buf);
		for (t = &buf[2]; *t != ' '; t++);
		*t++ = 0;
		if (doKey(&cs, &buf[2], t)) {
			fclose(list);
			return (-1);
		}
	}
	doKey(&cs, 0, 0);
	fclose(list);
	return (0);
}

/*
 * List all the revisions which make up a range of changesets.
 * The list is sorted for performance.
 */
void
csetlist(cset_t *cs, sccs *cset)
{
	char	*t;
	FILE	*list;
	char	buf[MAXPATH*2];
	char	cat[30], csort[30];
	char	*csetid;
	int	status;
	delta	*d;
	MDBM	*goneDB = 0;

	if (cs->dash) {
		while(fgets(buf, sizeof(buf), stdin)) {
			chop(buf);
			unless (d = findrev(cset, buf)) {
				fprintf(stderr,
				    "cset: no rev like %s in %s\n",
				    buf, cset->gfile);
				cset_exit(1);
			}
			d->flags |= D_SET;
		}
	}

	/* Save away the cset id */
	sccs_sdelta(cset, sccs_ino(cset), buf);
	csetid = strdup(buf);

	gettemp(cat, "catZ");
	gettemp(csort, "csort");
	unless (cs->csetOnly) {
		/*
		 * Get the list of key tuples in a sorted file.
		 */
		if (sccs_cat(cset, GET_NOHASH|PRINT, cat)) {
			sccs_whynot("cset", cset);
			unlink(cat);
			goto fail;
		}
		sprintf(buf, "sort < %s > %s", cat, csort);
		if (system(buf)) {
			unlink(cat);
			goto fail;
		}
		chmod(csort, TMP_MODE);		/* in case we don't unlink */
		unlink(cat);
		if (cs->verbose > 5) {
			sprintf(buf, "cat %s", csort);
			system(buf);
		}
		goneDB = loadDB(GONE, 0, DB_KEYSONLY|DB_NODUPS);
	} else {
		close(creat(csort, TMP_MODE));
	}
	unlink(cat);
	unless (list = fopen(csort, "r")) {
		perror(buf);
		goto fail;
	}

	/* checksum the output */
	if (cs->makepatch) {
		cs->pid = spawn_checksum_child();
		if (cs->pid == -1) goto fail;
	}
	if (cs->makepatch || cs->doDiffs) header(cset, cs->doDiffs);
again:	/* doDiffs can make it two pass */
	if (!cs->doDiffs && cs->makepatch) {
		printf("%s", PATCH_CURRENT);
	}

	sccs_close(cset); /* for win32 */
	/*
	 * Do the ChangeSet deltas first, takepatch needs it to be so.
	 */
	for (d = cset->table; d; d = d->next) {
		if ((d->flags & D_SET) && (d->type == 'D')) {
			sccs_sdelta(cset, d, buf);
			if (doKey(cs, csetid, buf)) goto fail;
		}
	}

	/*
	 * Now do the real data.
	 */
	while (fnext(buf, list)) {
		chop(buf);
		for (t = buf; *t != ' '; t++);
		*t++ = 0;
		if (sameFile(cs, csetid, buf)) continue;
		if (gone(buf, goneDB)) continue;
		if (doKey(cs, buf, t)) goto fail;
	}
	if (cs->doDiffs && cs->makepatch) {
		doKey(cs, 0, 0);
		cs->doDiffs = 0;
		rewind(list);
		goto again;
	}
	fclose(list);
	doKey(cs, 0, 0);
	if (cs->verbose && cs->makepatch) {
		fprintf(stderr,
		    "makepatch: patch contains %d revisions from %d files\n",
		    cs->ndeltas, cs->nfiles);
	} else if (cs->verbose && cs->mark && cs->ndeltas) {
		fprintf(stderr,
		    "cset: marked %d revisions in %d files\n",
		    cs->ndeltas, cs->nfiles);
	}
	if (cs->makepatch) {
		char eot[3] = { '\004', '\n', 0 };

		fputs(eot, stdout);	/* send  EOF indicator */
		fclose(stdout);
		if (waitpid(cs->pid, &status, 0) != cs->pid) {
			perror("waitpid");
		}
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			fprintf(stderr,
		  "makepatch: checksum process exited abnormally, status %d\n",
				status);
		}
		cs->pid = 0;
	}
	unlink(csort);
	free(csetid);
	if (goneDB) mdbm_close(goneDB);
	return;

fail:
	if (cs->makepatch) {
		char eot[3] = { '\004', '\n', 0 };

		fputs(eot, stdout);	/* send  EOF indicator */
		fclose(stdout);
	}
	unlink(csort);
	free(csetid);
	if (goneDB) mdbm_close(goneDB);
	cset_exit(1);
}

/*
 * Spit out the diffs.
 */
void
doDiff(sccs *sc, int kind)
{
	delta	*d, *e = 0;

	if (sc->state & S_CSET) return;	/* no changeset diffs */
	for (d = sc->table; d; d = d->next) {
		if (d->flags & D_SET) {
			e = d;
		} else if (e) {
			break;
		}
	}
	for (d = sc->table; d && !(d->flags & D_SET); d = d->next);
	if (!d) return;
	unless (e->parent) {
		printf("--- New file ---\n+++ %s\t%s\n",
		    sc->gfile, sc->tree->sdate);
		sccs_get(sc, 0, 0, 0, 0, PRINT|SILENT, "-");
		printf("\n");
		return;
	}
	e = e->parent;
	if (e == d) return;
	sccs_diffs(sc, e->rev, d->rev, 0, kind, stdout);
}

/*
 * Print the oldest..youngest
 * XXX - does not make sure that they are both on the trunk.
 */
void
doRange(cset_t *cs, sccs *sc)
{
	delta	*d, *e = 0;

	if (sc->state & S_CSET) return;
	for (d = sc->table; d; d = d->next) {
		if (d->flags & D_SET) e = d;
	}
	unless (e) return;
	if ((cs->range == 2) && e->parent) e = e->parent;
	printf("%s%c%s..", sc->gfile, BK_FS, e->rev);
	for (d = sc->table; d; d = d->next) {
		if (d->flags & D_SET) {
			printf("%s\n", d->rev);
			return;
		}
	}
}

private void
promote(sccs *s, delta *d, u16 lod, u16 level)
{
	char	newrev[MAXPATH];

	sprintf(newrev, "%u.%u", lod, level);
	if (d->rev) free(d->rev);
	d->rev = strdup(newrev);

	d->r[0] = lod;
	d->r[1] = level;
	d->r[2] = 0;
	d->r[3] = 0;
}

private void
mklod(sccs *s, delta *start, delta *stop)
{
	u16	lod;
	u16	level;
	delta	*t;

	unless(lod = sccs_nextlod(s)) {
		/* XXX: out of LODS?? */
		fprintf(stderr, "cset: ran out of LODs\n");
		cset_exit(1);
	}

	level = 0;
	for (t = start; t; t = t->kid) {
		level++;
		promote(s, t, lod, level);
		if (t == stop) break;
	}
}

private void
mkNewlod(cset_t *cs, sccs *s, delta *d)
{
	delta	*t;

	if (cs->tot) {	/* promote to new lod if hasn't happened yet */
		char	key[MAXPATH];
		char	*base, *tot;

		assert(cs->base);

		/* XXX: had the key, lost the key, regen the key */
		sccs_sdelta(s, sccs_ino(s), key);
		base = mdbm_fetch_str(cs->base, key);
		tot = mdbm_fetch_str(cs->tot, key);
		unless (base && base[0] && tot && tot[0] && streq(base, tot))
			return;
	}

	/* back down to root of cset */
	for (t = d; t; t = t->parent) {
		if (!t->parent || t->parent->flags & D_CSET) break;
	}

	assert(t);

	/* special exceptions: do not promote 1.0 or x.1 */
	if ((t->r[0] == 1 && !t->r[1]) || (t->r[1] == 1 && !t->r[2])) {
		return;
	}
	
	/* XXX: what does it mean to have merges on these deltas?? */
	mklod(s, t, d);
}

void
doMarks(cset_t *cs, sccs *s)
{
	delta	*d;
	int	did = 0;

	/*
	 * Throw away the existing marks if we are rebuilding.
	 */
	if (cs->remark) {
		for (d = s->table; d; d = d->next) d->flags &= ~D_CSET;
	}

	for (d = s->table; d; d = d->next) {
		if (d->flags & D_SET) {
			if (cs->force || !(d->flags & D_CSET)) {
				if (cs->verbose > 2) {
					fprintf(stderr,
					    "Mark %s%c%s\n", s->gfile, BK_FS, d->rev);
				}
				d->flags |= D_CSET;
				if (cs->newlod) mkNewlod(cs, s, d);
				cs->ndeltas++;
				did++;
			}
		}
	}
	if (did || !(s->state & S_CSETMARKED)) {
		s->state |= S_CSETMARKED;
		sccs_admin(s, 0, NEWCKSUM, 0, 0, 0, 0, 0, 0, 0, 0);
		if ((cs->verbose > 1) && did) {
			fprintf(stderr,
			    "Marked %d csets in %s\n", did, s->gfile);
		} else if (cs->verbose > 1) {
			fprintf(stderr,
			    "Set CSETMARKED flag in %s\n", s->sfile);
		}
	}
}

void
doSet(sccs *sc)
{
	delta	*d;
	int	first = 1;

	printf("%s:", sc->gfile);
	for (d = sc->table; d; d = d->next) {
		if (d->flags & D_SET) {
			if (first) {
				first = 0;
			} else {
				printf(",");
			}
			printf("%s", d->rev);
		}
	}
	printf("\n");
}

/*
 * Print out everything leading from start to d, not including start.
 */
void
csetDeltas(cset_t *cs, sccs *sc, delta *start, delta *d)
{
	int	i;

	unless (d) return;
	debug((stderr, "cD(%s, %s)\n", sc->gfile, d->rev));
	if ((d == start) || (d->flags & D_SET)) return;
	d->flags |= D_SET;
	csetDeltas(cs, sc, start, d->parent);
	/*
	 * We don't need the merge pointer, it is part of the include list.
	 * if (d->merge) csetDeltas(sc, start, sfind(sc, d->merge));
	 */
	EACH(d->include) {
		delta	*e = sfind(sc, d->include[i]);

		csetDeltas(cs, sc, e->parent, e);
	}
	// XXX - fixme - removed deltas not done.
	// Is this an issue?  I think makepatch handles them.
	unless (cs->makepatch || cs->range) {
		if (d->type == 'D') printf("%s%c%s\n", sc->gfile, BK_FS, d->rev);
	}
}

/*
 * Add a delta to the ChangeSet.
 *
 * XXX - this could check to make sure we are not adding 1.3 to a cset LOD
 * which already has 1.5 from the same file.
 */
private void
add(FILE *diffs, char *buf)
{
	sccs	*s;
	char	*rev = 0;	/* lint */
	delta	*d;

	unless ((chop(buf) == '\n') && (rev = strrchr(buf, BK_FS))) {
		fprintf(stderr, "cset: bad file:rev format: %s\n", buf);
		system("bk clean -u ChangeSet");
		cset_exit(1);
	}
	*rev++ = 0;
	unless (s = sccs_init(buf, INIT_NOCKSUM|SILENT, 0)) {
		fprintf(stderr, "cset: can't init %s\n", buf);
		system("bk clean -u ChangeSet");
		cset_exit(1);
	}
	if (s->state & S_CSET) {
		sccs_free(s);
		return;
	}
	unless (d = sccs_getrev(s, rev, 0, 0)) {
		fprintf(stderr, "cset: can't find %s in %s\n", rev, buf);
		system("bk clean -u ChangeSet");
		cset_exit(1);
	}
	sccs_sdelta(s, sccs_ino(s), buf);
	fprintf(diffs, "> %s ", buf);
	sccs_sdelta(s, d, buf);
	fprintf(diffs, "%s\n", buf);
	sccs_free(s);
}

/*
 * Read file:rev from stdin and apply those to the changeset db.
 * Edit the ChangeSet file and add the new stuff to that file and
 * leave the file sorted.
 * Close the cset sccs* when done.
 */
delta	*
mkChangeSet(sccs *cset, FILE *diffs)
{
	delta	*d;
	char	buf[MAXPATH];

	/*
	 * Edit the ChangeSet file - we need it edited to modify it as well
	 * as load in the current state.
	 * If the edit flag is off, then make sure the file is already edited.
	 */
	unless ((cset->state & (S_SFILE|S_PFILE)) == (S_SFILE|S_PFILE)) {
		int flags = GET_EDIT|GET_SKIPGET|SILENT;
		if (sccs_get(cset, 0, 0, 0, 0, flags, "-")) {
			unless (BEEN_WARNED(cset)) {
				fprintf(stderr,
				    "cset: get -eg of ChangeSet failed\n");
				cset_exit(1);
			}
		}
	}
	d = sccs_dInit(0, 'D', cset, 0);
	/*
	 * XXX we need to insist d->hostname is non-null here,
	 * otherwise it will inherit hostname from its ancestor
	 * which will cause cset -i/-L to fail since
	 * the signiture do not match
	 */
	assert(d->hostname && d->hostname[0]);

	fprintf(diffs, "0a0\n"); /* fake diff header */

	/*
	 * Read each file:rev from stdin and add that to the cset.
	 * add() will ignore the ChangeSet entry itself.
	 */
	while (fgets(buf, sizeof(buf), stdin)) {
		add(diffs, buf);
	}

#ifdef CRAZY_WOW
	/*
	 * Adjust the date of the new rev, scripts can make this be in the
	 * same second.  It's OK that we adjust it here, we are going to use
	 * this delta * as part of the checkin on this changeset.
	 */
	if (d->date <= cset->table->date) {
		d->dateFudge = (cset->table->date - d->date) + 1;
		d->date += d->dateFudge;
	}
	/* Add ChangeSet entry */
	sccs_sdelta(cset, sccs_ino(cset), buf);
	fprintf(diffs, "> %s", buf);
	sccs_sdelta(cset, d, buf);
	fprintf(diffs, " %s\n", buf);
#endif
	return (d);
}

void
dump(MDBM *db, FILE *out)
{
	kvpair	kv;

	for (kv = mdbm_first(db); kv.key.dsize; kv = mdbm_next(db)) {
		fprintf(out, "%s %s\n", kv.key.dptr, kv.val.dptr);
	}
}

void
updateIdCacheEntry(sccs *sc, const char *filename)
{
	char	*path;
	char	buf[MAXPATH*2];

	/* update the id cache */
	sccs_sdelta(sc, sccs_ino(sc), buf);
	if (sc->tree->pathname && streq(sc->tree->pathname, sc->gfile)) {
		path = sc->tree->pathname;
	} else {
		path = sc->gfile;
	}
	fprintf(id_cache, "%s %s\n", buf, path);
}

#ifndef	PROFILE
/*
 * XXX TODO
 * the locking code need to handle intr
 * It need to leave the lock in a clean state after a interrupt
 *
 * should hanlde lock fail with re-try
 */
void
lock(char *lockName)
{
	int	i;

	unless ((i = open(lockName, O_CREAT|O_EXCL, GROUP_MODE)) > 0) {
		fprintf(stderr, "cset: can't lock %s\n", lockName);
		cset_exit(1);
	}
	close(i);
}

void
unlock(char *lockName)
{
	if (unlink(lockName)) {
		fprintf(stderr, "unlock: lockname=%s\n", lockName);
		perror("unlink:");
	}
}
#endif


/* There are two ways a new LOD can be created:
 * + The user asks for this changeset to be the first on a new lod
 * + The current workspace is based on a specific changeset as opposed
 *   to a LOD: make new changeset start new LOD.
 */

int
csetCreate(sccs *cset, int flags, char *sym, int newlod)
{
	delta	*d;
	int	error = 0;
	MMAP	*diffs;
	FILE	*fdiffs;
	MDBM	*totdb = 0;
	MDBM	*basedb = 0;
	char	filename[30];

	gettemp(filename, "cdif");
	unless (fdiffs = fopen(filename, "w+")) {
		perror(filename);
		sccs_free(cset);
		cset_exit(1);
	}

	d = mkChangeSet(cset, fdiffs); /* write change set to diffs */

	fclose(fdiffs);
	unless (diffs = mopen(filename)) {
		perror(filename);
		sccs_free(cset);
		unlink(filename);
		cset_exit(1);
	}

	if (sym) d->sym = strdup(sym);
	d->flags |= D_CSET;	/* XXX: longrun, don't tag cset file */

	if (newlod) {
		delta	*t;
		unless(t = findrev(cset, 0)) {
			fprintf(stderr, "find tot on cset before new cset\n");
			error = -1;
			goto out;
		}
		unless(streq(t->rev, "1.0")) mklod(cset, d, d);
	}

	/*
	 * Make /dev/tty where we get input.
	 */
#undef	close
#undef	open
	close(0);
	open(DEV_TTY, 0, 0);
	if (flags & DELTA_DONTASK) d = getComments(d);
	if (sccs_delta(cset, flags, d, 0, diffs) == -1) {
		sccs_whynot("cset", cset);
		error = -1;
		goto out;
	}

	/* XXX: can do a re-init?  There is a new delta */
	sccs_free(cset);
	unless (cset = sccs_init(csetFile, flags, 0)) {
		perror("init");
		error = -1;
		goto out;
	}

	/* XXX: check 'd' for mem leak?  It should be free as part of
	 * sccs_free(cset)
	 */
	unless(d = findrev(cset, 0)) {
		perror("find tot");
		error = -1;
		goto out;
	}

	newlod = 0;
	if (d->r[0] > 1) {	/* lod process only if not base lod */
		newlod = 1;
		if (d->r[1] != 1) {	/* not new lod, but maybe for some */
			delta	*lodone;

			for (lodone = d; lodone; lodone = lodone->parent) {
				if (lodone->r[1] == 1) break;
			}
			unless (lodone) {
				fprintf(stderr, "cset: no .1 on LOD %u",
					d->r[0]);
				error = -1;
				goto out;
			}
			unless (lodone->parent) {
				fprintf(stderr, "cset: no .0 on LOD %u",
					d->r[0]);
				error = -1;
				goto out;
			}

			assert(d->parent->rev);
			if (csetIds(cset, d->parent->rev)) {
				error = -1;
				goto out;
			}
			totdb = cset->mdbm;
			cset->mdbm = 0;

			assert(lodone->parent->rev);
			if (csetIds(cset, lodone->parent->rev)) {
				error = -1;
				goto out;
			}
			basedb = cset->mdbm;
			cset->mdbm = 0;
		}
	}

	if (marklist(filename, newlod, totdb, basedb)) {
		error = -1;
		goto out;
	}

out:	sccs_free(cset);
	if (totdb) mdbm_close(totdb);
	if (basedb) mdbm_close(basedb);
	unlink(filename);
	commentsDone(saved);
	return (error);
}

char	*
file2str(char *f)
{
	struct	stat sb;
	int	fd = open(f, O_RDONLY, 0);
	char	*s;

	if ((fd == -1) || (fstat(fd, &sb) == -1) || (sb.st_size == 0)) {
		fprintf(stderr, "Can't get comments from %s\n", f);
		if (fd != -1) close(fd);
		return (0);
	}
	s = malloc(sb.st_size + 1);
	if (!s) {
		perror("malloc");
		close(fd);
		return (0);
	}
	read(fd, s, sb.st_size);
	s[sb.st_size] = 0;
	close(fd);
	return (s);
}

/*
 * All the deltas we want are marked so print them out.
 * Note: takepatch depends on table order so don't change that.
 */
void
sccs_patch(sccs *s, cset_t *cs)
{
	delta	*d;
	int	deltas = 0;
	int	i, n, newfile;
	delta	**list;

	if (cs->verbose>1) fprintf(stderr, "makepatch: %s ", s->gfile);
	n = sccs_markMeta(s);

	/*
	 * Build a list of the deltas we're sending
	 * Clear the D_SET flag because we need to be able to do one at
	 * a time when sending the cset diffs.
	 */
	list = calloc(n, sizeof(delta*));
	newfile = s->tree->flags & D_SET;
	for (i = 0, d = s->table; d; d = d->next) {
		if (d->flags & D_SET) {
			assert(i < n);
			list[i++] = d;
			d->flags &= ~D_SET;
		}
	}

	/*
	 * For each file, spit out file seperators when the filename
	 * changes.
	 * Spit out the root rev so we can find if it has moved.
	 */
	for (i = n - 1; i >= 0; i--) {
		d = list[i];
		assert(d);
		if (cs->verbose > 2) fprintf(stderr, "%s ", d->rev);
		if (cs->verbose == 2) fprintf(stderr, "%c\b", spin[deltas % 4]);
		if (i == n - 1) {
			delta	*top = list[0];

			unless (top->pathname) {
				fprintf(stderr, "\n%s%c%s has no path\n",
				    s->gfile, BK_FS, d->rev);
				cset_exit(1);
			}
			printf("== %s ==\n", top->pathname);
			if (newfile) {
				printf("New file: %s\n", d->pathname);
				sccs_perfile(s, stdout);
			}
			s->rstop = s->rstart = s->tree;
			sccs_pdelta(s, s->tree, stdout);
			printf("\n");
		}

		/*
		 * For each file, also eject the parent of the rev.
		 */
		if (d->parent) {
			sccs_pdelta(s, d->parent, stdout);
			printf("\n");
		}
		s->rstop = s->rstart = d;
		sccs_prs(s, PRS_PATCH|SILENT, 0, NULL, stdout);
		printf("\n");
		if (d->type == 'D') {
			if (s->state & S_CSET) {
				if (d->added) {
					sccs_getdiffs(s,
					    d->rev, GET_HASHDIFFS, "-");
				}
			} else {
				sccs_getdiffs(s, d->rev, GET_BKDIFFS, "-");
			}
		}
		printf("\n");
		deltas++;
	}
	if (cs->verbose == 2) {
		fprintf(stderr, "%d revisions\n", deltas);
	} else if (cs->verbose > 1) {
		fprintf(stderr, "\n");
	}
	cs->ndeltas += deltas;
	if (list) free(list);
}
