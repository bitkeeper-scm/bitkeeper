#include "bkd.h"
#include "range.h"
#include "nested.h"

private struct {
	u32	one:1;		/* -1: stop after printing one entry */
	u32	all:1;		/* list all, including tags etc */
	u32	doSearch:1;	/* search the comments list */
	u32	showdups:1;	/* do not filter duplicates when multi-parent */
	u32	forwards:1;	/* like prs -f */
	u32	html:1;		/* do html style output */
	u32	keys:1;		/* just list the keys */
	u32	local:1;	/* want the new local csets */
	u32	newline:1;	/* add a newline after each record, like prs */
	u32	noempty:1;	/* do not list empty merge deltas */
	u32	nomerge:1;	/* do not list _any_ merge deltas */
	u32	remote:1;	/* want the new remote csets */
	u32	tagOnly:1;	/* only show items which are tagged */
	u32	timesort:1;	/* force sorting based on time, not dspec */
	u32	urls:1;		/* list each URL for local/remote */
	u32	verbose:1;	/* list the file checkin comments */
	u32	prodOnly:1;	/* limit output to just product repo */
	u32	doComp:1;	/* decend to components from product */
	u32	diffs:1;	/* show diffs with verbose mode */
	u32	tsearch:1;	/* pattern applies to tags instead of cmts */
	u32	BAM:1;		/* only include BAM files */
	u32	filt:1;		/* output limited by filenames */

	search	search;		/* -/pattern/[i] matches comments w/ pattern */
	char	*dspec;		/* override dspec */
	char	*begin;		/* $begin{....} */
	char	*end;		/* $end{....} */
	char	**users;	/* lines list of users to include */
	char	**notusers;	/* lines list of users to exclude */
	char	**inc;		/* list of globs for files to include */
	char	**exc;		/* list of globs for files to exclude */

	RANGE	rargs;
	FILE	*fmem;		/* in-mem output buffering */
	FILE	*fcset;		/* save cset for ordering */
} opts;

typedef struct slog {
	sccs	*s;
	delta	*d;
	char	*path;
} slog;

/* per-repo state */
struct rstate {
	MDBM	*idDB;
	MDBM	*goneDB;
	MDBM	*csetDB;
	MDBM	*graphDB;
};

private int	doit(int dash);
private int	want(sccs *s, delta *e);
private int	send_part1_msg(remote *r, char **av);
private int	send_end_msg(remote *r, char *msg);
private int	send_part2_msg(remote *r, char **av, char *key_list);
private int	changes_part1(remote *r, char **av, char *key_list);
private int	changes_part2(remote *r, char **av, char *key_list, int ret);
private int	_doit_remote(char **av, char *url);
private int	doit_remote(char **nav, char *url);
private int	doit_local(int nac, char **nav, char **urls);
private	int	cset(hash *state, sccs *cset, char *dkey, FILE *f, char *dspec);
private	MDBM	*loadcset(sccs *cset);
private	void	fileFilt(sccs *s, MDBM *csetDB);
private	int	prepSearch(char *str);

private	hash	*seen; /* list of keys seen already */
private	sccs	*s_cset;

int
changes_main(int ac, char **av)
{
	int	i, c;
	int	rc = 0, nac = 0;
	char	*nav[30];
	char	**urls = 0, **rurls = 0, **lurls = 0;
	char	*normal;
	char	*searchStr = 0;
	char	buf[MAXPATH];
	pid_t	pid = 0; /* pager */

	bzero(&opts, sizeof(opts));
	opts.showdups = opts.urls = opts.noempty = 1;

	nav[nac++] = "bk";
	nav[nac++] = "changes";
	/*
	 * XXX Warning: The 'changes' command can NOT use the -K
	 * option.  that is used internally by the bkd_changes part1 cmd.
	 */
	while ((c = getopt(ac, av, "1aBc;Dd;efhi;kLmnPqRr;tTu;U;Vv/;x;")) != -1)
	{
		unless (c == 'L' || c == 'R' || c == 'D') {
			if (optarg) {
				nav[nac++] = aprintf("-%c%s", c, optarg);
			} else {
				nav[nac++] = aprintf("-%c", c);
			}
		}
		switch (c) {
		    /*
		     * Note: do not add option 'K', it is reserved
		     * for internal use by bkd_changes.c, part1
		     */
		    case '1': opts.one = 1; break;
		    case 'a': opts.all = 1; opts.noempty = 0; break;
		    case 'B': opts.BAM = 1; break;
		    case 'c':
			if (range_addArg(&opts.rargs, optarg, 1)) goto usage;
			break;
		    case 'D': opts.urls = opts.showdups = 0; break;
		    case 'd': opts.dspec = strdup(optarg); break;
		    case 'e': opts.noempty = !opts.noempty; break;
		    case 'f': opts.forwards = 1; break;
		    case 'h': opts.html = 1; opts.urls = 0; break;
		    case 'i':
			opts.inc = addLine(opts.inc, strdup(optarg));
			break;
		    /* case 'K': reserved */
		    case 'k':
		    	opts.keys = opts.all = 1; opts.noempty = 0; /* -a */
			opts.urls = opts.showdups = 0;		    /* -D */
			break;
		    case 'm': opts.nomerge = 1; break;
		    case 'n': opts.newline = 1; break;
		    case 'P': opts.prodOnly = 1; break;
		    case 'q': opts.urls = 0; break;
		    case 't': opts.tagOnly = 1; break;		/* doc 2.0 */
		    case 'T': opts.timesort = 1; break;
		    case 'u':
			opts.users = addLine(opts.users, strdup(optarg));
			break;
		    case 'U':
			opts.notusers = addLine(opts.notusers, strdup(optarg));
			break;
		    case 'V': opts.doComp = 1; break;
		    case 'v':
		    	if (opts.verbose) opts.diffs = 1;
			opts.verbose =1 ;
			break;
		    case 'r':
			if (range_addArg(&opts.rargs, optarg, 0)) goto usage;
			break;
		    case 'x':
			opts.exc = addLine(opts.exc, strdup(optarg));
			break;
		    case '/': searchStr = optarg; break;
		    case 'L': opts.local = 1; break;
		    case 'R': opts.remote = 1; break;
		    default:
usage:			system("bk help -s changes");
			exit(1);
		}
		optarg = 0;
	}
	opts.filt = opts.BAM || opts.inc || opts.exc;

	/* ERROR check options */
	/* XXX: could have rev range limit output -- whose name space? */
	if ((opts.local || opts.remote) && opts.rargs.rstart) goto usage;

	if (proj_isProduct(0)) {
		if (opts.verbose && !opts.prodOnly) opts.doComp = 1;
		if (opts.filt && !opts.doComp && !opts.BAM) {
			opts.doComp = opts.prodOnly = 1;
		}
	}
	if (opts.keys && (opts.verbose||opts.html||opts.dspec)) goto usage;
	if (opts.html && opts.dspec) goto usage;
	if (opts.diffs && opts.dspec) goto usage;
	if (opts.html && opts.diffs) goto usage;

	if (opts.local || opts.remote || !av[optind] ||
	    (streq(av[optind], "-") && !av[optind + 1])) {
		unless (proj_root(0)) {
			fprintf(stderr, "bk: Cannot find package root.\n");
			return (1);
		}
	} else if (streq(av[optind], "-") && av[optind + 1]) {
		fprintf(stderr,
		    "changes: either '-' or URL list, but not both\n");
		return (1);
	}
	if (searchStr && prepSearch(searchStr)) goto usage;
	/* force a -a if -L or -R and no -a */
	if ((opts.local || opts.remote) && !opts.all) {
		nav[nac++] = strdup("-a");
		opts.all = 1;
		opts.noempty = 0;
	}
	nav[nac] = 0;	/* terminate list of args with -L and -R removed */

#if 0
	// NOT YET
	// Current thinking is that $include{filename} will read a file
	// as a dspec.
	if (opts.dspec && (opts.dspec[0] == '<')) {
		// XXX replace with $include
		if (streq(opts.dspec, "<-")) {
			if (av[optind] && streq(av[optind], "-")) {
				fprintf(stderr, "changes: "
				    "can't read dspec and revs from stdin.\n");
				return (1);
			}
			f = fmem_open();
			while ((i = fread(buf, 1, sizeof(buf), stdin)) > 0) {
				fwrite(buf, 1, i, f);
			}
			free(opts.dspec);
			opts.dspec = fmem_retbuf(f, 0);
			fclose(f);
		} else if (exists(opts.dspec+1)) {
			if (p = loadfile(opts.dspec+1, 0)) {
				free(opts.dspec);
				opts.dspec = p;
			}
		}
	}
#endif
	/*
	 * There are 5 major cases
	 * 1) bk changes -L (url_list | -)
	 * 2) bk changes -R (url_list | -)
	 * 3) bk changes [-]
	 * 4) bk changes url_list
	 * 5) bk changes -L -R (url_list | -)
	 * Note: the dash in cases 1, 2, and 5 is a url list,
	 *       the dash in case 3 is a key list.
	 */
	if (opts.local || opts.remote) {
		if (av[optind] && streq("-", av[optind])) {
			/*
			 * bk changes -[LR] -
			 * get url list from stdin
			 */
			while (fnext(buf, stdin)) {
				chomp(buf);
				normal = parent_normalize(buf);
				lurls = addLine(lurls, normal);
			}
		} else if (av[optind]) {
			while (av[optind]) {
				normal = parent_normalize(av[optind++]);
				lurls = addLine(lurls, normal);
			}
		} else {
			if (opts.local) lurls = parent_pushp();
			if (opts.remote) rurls = parent_pullp();
			unless (lurls || rurls) {
				getMsg("missing_parent", 0, 0, stderr);
				goto usage;
			}
		}
		unless (lurls || rurls) goto usage;
		unless (rurls) rurls = lurls;
		seen = hash_new(HASH_MEMHASH);
		pid = mkpager();
		putenv("BK_PAGER=cat");
		proj_cd2root();
		s_cset = sccs_csetInit(SILENT|INIT_NOCKSUM);
	}
	if (opts.local) {
		if (rc = doit_local(nac, nav, lurls)) goto out;
	}
	if (opts.remote) {
		EACH(rurls) {
			if (opts.urls) {
				printf("==== changes -R %s ====\n", rurls[i]);
				fflush(stdout);
			}
			rc |= doit_remote(nav, rurls[i]);
		}
	}
	if (opts.local || opts.remote) goto out;

	if (av[optind] && !streq(av[optind], "-")) {
		pid_t	pid2;

		/* bk changes url [url ...] */
		while (av[optind]) {
			normal = parent_normalize(av[optind++]);
			urls = addLine(urls, normal);
		}
		pid2 = mkpager();
		putenv("BK_PAGER=cat");
		EACH(urls) {
			if (opts.urls) {
				printf("==== changes %s ====\n", urls[i]);
				fflush(stdout);
			}
			rc |= doit_remote(nav, urls[i]);
		}
		if (pid2 > 0) {
			fclose(stdout);
			waitpid(pid2, 0, 0);
		}
	} else {
		if (proj_cd2root()) {
			fprintf(stderr, "Can't find package root\n");
			exit(1);
		}
		s_cset = sccs_csetInit(SILENT|INIT_NOCKSUM);
		unless (av[optind]) {
			rc = doit(0); /* bk changes */
		} else {
			assert(streq(av[optind], "-"));
			/* get key list from stdin */
			rc = doit(1); /* bk changes - */
		}
	}

	/*
	 * clean up
	 */
out:	if (s_cset) sccs_free(s_cset);
	if (pid > 0)  {
		fclose(stdout);
		waitpid(pid, 0, 0);
	}
	if (opts.dspec) free(opts.dspec);
	if (opts.begin) free(opts.begin);
	if (opts.end) free(opts.end);
	if (seen) hash_free(seen);
	for (c = 2; c < nac; c++) free(nav[c]);
	freeLines(urls, free);
	if (rurls != lurls) freeLines(rurls, free);
	freeLines(lurls, free);
	return (rc);
}

private	int
prepSearch(char *str)
{
	char	*p;

	/*
	 * XXX: note this does not support \/ in search pattern
	 * and neither does search_parse which starts with strchr(.., '/')
	 */
	if ((p = strchr(str, '/')) && (p = strchr(p, 't'))) {
		opts.tsearch = 1;
		opts.tagOnly = 1;
		/* eat it */
		while (*p = *(p+1)) p++;
	} else {
		opts.doSearch = 1;
	}
	opts.search = search_parse(str);
	return (opts.search.pattern == 0);
}

private int
_doit_local(char **nav, char *url)
{
	FILE	*f = 0;
	int	status;
	int	rc = 0;
	FILE	*p;
	remote	*r;
	char	buf[MAXKEY];
	char	tmpf[MAXPATH];

	/*
	 * What we get here is: bk synckey -l url | bk changes opts -
	 */
	if (opts.showdups) {
		f = popenvp(nav, "w");
		assert(f);
	}

	r = remote_parse(url, REMOTE_BKDURL | REMOTE_ROOTKEY);
	assert(r);

	bktmp(tmpf, 0);
	p = fopen(tmpf, "w");
	rc = synckeys(r, s_cset, PK_LKEY, p);
	fclose(p);
	remote_free(r);
	p = fopen(tmpf, "r");
	assert(p);
	while (fnext(buf, p)) {
		if (opts.showdups) {
			fputs(buf, f);
		} else {
			int	*v;

			unless (v = hash_fetchStr(seen, buf)) {
				v = hash_insert(seen,
				    buf, strlen(buf) + 1, 0, sizeof(int));
			}
			*v += 1;
		}
	}
	fclose(p);
	unlink(tmpf);
	if (opts.showdups) {
		status = pclose(f);
		unless (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) rc=1;
	}
	return (rc);
}

private int
doit_local(int nac, char **nav, char **urls)
{
	FILE	*f;
	char	*p;
	int	status, i;
	int	ac = nac, rc = 0;
	int	all = 0;

	nav[ac++] = strdup("-");
	assert(ac < 30);
	nav[ac] = 0;
	EACH(urls) {
		if (opts.urls) {
			if ((p = getenv("BK_STATUS")) &&
			    streq(p, "LOCAL_WORK")) {
			    	printf("#### Not updating "
				    "due to the following local work:\n");
			} else {
				printf("==== changes -L %s ====\n", urls[i]);
			}
			fflush(stdout);
		}
		if (rc = _doit_local(nav, urls[i])) goto done;
		all++;
	}
	unless (opts.showdups) {
		/* Note: for Local case, we are not filtering dups per se.
		 * We are only listing changes that are unique to this repo.
		 * That means only list output that has been seen in all
		 * of the _doit_local cases above.
		 *
		 * Optimize: see if we are going to list anything
		 * and skip starting a process if we aren't.
		 */
		f = popenvp(nav, "w");
		assert(f);
		EACH_HASH(seen) {
			if (*(int *)seen->vptr == all) fputs(seen->kptr, f);
		}
		status = pclose(f);
		unless (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) rc=1;
	}
done:
	while(ac > nac) free(nav[--ac]);
	nav[nac] = 0;
	return (rc);
}

private int
doit(int dash)
{
	char	cmd[MAXKEY];
	char	*spec, *specf;
	pid_t	pid;
	sccs	*s = 0;
	delta	*e, *dstart, *dstop;
	int	rc = 1;
	hash	*state;
	struct	rstate *rstate;
	char	**keys;
	kvpair	kv;
	int	flags;

	unless (opts.dspec) {
		if (opts.html) {
			spec = opts.verbose ?
			    "dspec-changes-hv" :
			    "dspec-changes-h";
		} else {
			spec = opts.diffs ?
			    "dspec-changes-vv" :
			    "dspec-changes";
		}
		specf = bk_searchFile(spec);
		TRACE("Reading dspec from %s", specf ? specf : "(not found)");
		unless (specf && (opts.dspec = loadfile(specf, 0))) {
			fprintf(stderr,
			    "changes: cant find %s/%s\n", bin, spec);
			exit(1);
		}
		free(specf);
	}
	dspec_collapse(&opts.dspec, &opts.begin, &opts.end);
	s = s_cset;
	unless (s && HASGRAPH(s)) {
		system("bk help -s changes");
		exit(1);
	}
	if (opts.rargs.rstart) {
		unless (opts.rargs.rstop) opts.noempty = 0;
		if (range_process("changes", s, RANGE_SET, &opts.rargs)) {
			goto next;
		}
		for (e = s->rstop; e; e = e->next) {
			if ((e->flags & D_SET) && !want(s, e)) {
				e->flags &= ~D_SET;
			}
			if (e == s->rstart) break;
		}
		if (opts.all) range_markMeta(s);
	} else if (dash) {
		while (fgets(cmd, sizeof(cmd), stdin)) {
			/* ignore blank lines and comments */
			if ((*cmd == '#') || (*cmd == '\n')) continue;
			chomp(cmd);
			e = sccs_findrev(s, cmd);
			unless (e) {
				fprintf(stderr,
				    "changes: can't find key: %s\n", cmd);
				goto next;
			}
			/* maintain the rstart->rstop range */
			if (!s->rstart || (e->serial < s->rstart->serial)) {
				s->rstart = e;
			}
			if (!s->rstop || (e->serial > s->rstop->serial)) {
				s->rstop = e;
			}
#ifdef	CRAZY_WOW
			/* this has cause more problems than solved
			 * in that customers would have a tag difference
			 * and up would pop a delta being different.
			 * Wayne forced -a on -L as a way to address
			 * this confusion.  In marking this as not compiled
			 * in, I pulled the forced -a as well.  People can
			 * choose what they want to see: only deltas.
			 */
			while (!opts.all && (e->type == 'R')) {
				e = e->parent;
				assert(e);
			}
#endif
			if (want(s, e)) e->flags |= D_SET;
		}
	} else {
		s->rstop = s->table;
		for (e = s->table; e; e = e->next) {
			if (want(s, e)) e->flags |= D_SET;
		}
	}
	/*
	 * What we want is: this process | pager
	 */
	pid = mkpager();

	state = hash_new(HASH_MEMHASH);
	/*
	 * If we are doing filtering in a nested environment then we
	 * need to create a fmem FILE* to be used to buffer output
	 * before we know if it will be printed.
	 */
	if (opts.doComp && opts.filt) {
		opts.fmem = fmem_open();
	}
	if (opts.doComp || opts.verbose) opts.fcset = fmem_open();
	/* capture the comments, for the csets we care about */
	dstart = dstop = 0;
	for (e = s->rstop; e; e = e->next) {
		if (e->flags & D_SET) {
			unless (dstart) dstart = e;
			dstop = e;
			comments_load(s, e);
		}
		if (e == s->rstart) break;
	}
	if (opts.forwards) {
		e = dstop;
		dstop = dstart;
		dstart = e;
	}
	flags = PRS_FORCE;
	if (opts.newline) flags |= PRS_LF;
	if (opts.begin && dstart) {
		sccs_prsdelta(s, dstart, flags, opts.begin, stdout);
	}
	cset(state, s, 0, stdout, opts.dspec);
	if (opts.end && dstop) {
		sccs_prsdelta(s, dstop, flags, opts.end, stdout);
	}
	if (opts.fcset) fclose(opts.fcset);
	if (opts.fmem) fclose(opts.fmem);
	EACH_HASH(state) {
		rstate = state->vptr;
		EACH_KV(rstate->graphDB) {
			memcpy(&s, kv.val.dptr, sizeof (sccs *));
			if (s) sccs_free(s);
		}
		mdbm_close(rstate->graphDB);
		mdbm_close(rstate->idDB);
		mdbm_close(rstate->goneDB);
		EACH_KV(rstate->csetDB) {
			memcpy(&keys, kv.val.dptr, sizeof (char **));
			freeLines(keys, free);
		}
		mdbm_close(rstate->csetDB);
	}
	hash_free(state);
	if (pid > 0) {
		fclose(stdout);
		waitpid(pid, 0, 0);
	}
	rc = 0;
next:
	return (rc);
}

private	void
fileFilt(sccs *s, MDBM *csetDB)
{
	delta	*d;
	datum	k, v;

	/* Unset any csets that don't contain files from -i and -x */
	for (d = s->table; d; d = d->next) {
		unless (d->flags & D_SET) continue;
		/* if cset changed nothing, keep it if not filtering by inc */
		if (!(opts.inc || opts.BAM) && !d->added) continue;
		k.dptr = d->rev;
		k.dsize = strlen(d->rev);
		v = mdbm_fetch(csetDB, k);
		unless (v.dptr) d->flags &= ~D_SET;
	}
}

private int
delta_sort(const void *a, const void *b)
{
	slog	*d1, *d2;
	int	cmp;
	char	key1[MAXKEY], key2[MAXKEY];

	d1 = *((slog**)a);
	d2 = *((slog**)b);

	if (d1->s == d2->s) {
		/* comparing deltas of the same sfiles, time order */
		cmp = (d2->d->serial - d1->d->serial);
		if (opts.forwards) cmp *= -1;
		return (cmp);
	} else {
		/* comparing different sfiles */
		if (opts.timesort && (cmp = d2->d->date - d1->d->date)) {
			if (opts.forwards) cmp *= -1;
			return (cmp);
		}
		/* compare latest pathnames */
		if (cmp = strcmp(d1->path, d2->path)) {
			return (cmp);
		}
		/*
		 * XXX: To get here, two sfiles have to exist and have had the
		 * same name at the same time.  I'm tempted to suggest
		 * putting an assert() here but all that would do is point
		 * to the crazy repo somewhere which had two files in the
		 * same spot, then gone'd the file, then brought it back later.
		 * Instead, I'll just leave this comment :)
		 *
		 * sort ties by rootkeys
		 */
		sccs_sdelta(d1->s, sccs_ino(d1->s), key1);
		sccs_sdelta(d2->s, sccs_ino(d2->s), key2);
		return (keycmp(key1, key2));
	}
}

private	void
dumplog(char **list, delta *cset, char *dspec, int flags, FILE *f)
{
	slog	*ll;
	int	i;
	char	*comppath = 0;
	

	if (strrchr(cset->pathname, '/')) {
		comppath = strdup(cset->pathname);
		csetChomp(comppath);
	}

	sortLines(list, delta_sort);

	/*
	 * Print the sorted list
	 */
	EACH(list) {
		ll = (slog *)list[i];
		if (ll->s->prs_indentC) ll->s->comppath = comppath;
		sccs_prsdelta(ll->s, ll->d, flags, dspec, f);
		ll->s->comppath = 0;
		free(ll);
	}
	freeLines(list, 0);
	free(comppath);
}

/*
 * Cache the sccs struct to avoid re-initing the same sfile
 */
private sccs *
sccs_keyinitAndCache(project *proj, char *key, int flags, MDBM *idDB, MDBM *graphDB)
{
	datum	k, v;
	sccs	*s;
	delta	*d;
	char	*path, *here;
	project	*prod;

	k.dptr = key;
	k.dsize = strlen(key);
	v = mdbm_fetch(graphDB, k);
	if (v.dptr) { /* cache hit */
		memcpy(&s, v.dptr, sizeof (sccs *));
		return (s);
	}
	s = sccs_keyinit(proj, key, flags|INIT_NOWARN, idDB);

	/*
	 * When running in a nested product's RESYNC tree we we can
	 * descend to the fake component's directories and won't be
	 * able to find files in those components.
	 * (ex: 'bk changes -v' in post-commit trigger after merge)
	 * In this case the resolve of components have already been
	 * completed so we just look for the already resolved file
	 * in the original tree.
	 * NOTE: nothing here is optimized
	 */
	if (!s && proj_isComponent(proj) &&
	    (prod = proj_isResync(proj_product(proj)))) {
		here = strdup(proj_cwd());
		chdir(proj_root(prod));
		idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
		if (path = key2path(proj_rootkey(proj), idDB)) {
			mdbm_close(idDB);
			proj = proj_init(path);
			chdir(path);
			free(path);
			idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
			chdir(proj_root(prod));
			s = sccs_keyinit(proj, key, flags|INIT_NOWARN, idDB);
			proj_free(proj);
		}
		mdbm_close(idDB);
		chdir(here);
		free(here);
	}
	v.dptr = (void *) &s;
	v.dsize = sizeof (sccs *);
	if (mdbm_store(graphDB, k, v, MDBM_INSERT)) { /* cache the new entry */
		perror("sccs_keyinitAndCache");
	}
	if (s) {
		/* capture the comments */
		for (d = s->table; d; d = d->next) comments_load(s, d);
		sccs_close(s); /* we don't need the delta body */
		if (opts.doComp) s->prs_indentC = 1;
	}
	return (s);
}

/*
 * Given a "top" delta "d", this function computes ChangeSet boundaries
 * It collect all the deltas inside a changeset and stuff them to "list".
 */
private char **
collectDelta(sccs *s, delta *d, char **list)
{
	slog	*ll;
	char	*path = 0;	/* The most recent :DPN: for this file */

	/*
	 * Walk all deltas included in this cset and capture the
	 * changes output.
	 */
	range_cset(s, d);
	for (d = s->rstop; d; d = d->next) {
		if (d->flags & D_SET) {
			/* add delta to list */
			ll = new(slog);
			ll->s = s;
			ll->d = d;
			unless (path) path = d->pathname;
			ll->path = path;
			list = addLine(list, ll);
			d->flags &= ~D_SET;	/* done using it */
		}
		if (d == s->rstart) break;
	}
	return (list);
}

private void
saveKey(MDBM *db, char *rev, char **keylist)
{
	datum	k, v;

	k.dptr = rev;
	k.dsize = strlen(rev);
	v.dptr = (char *) &keylist;
	v.dsize = sizeof (keylist);
	if (mdbm_store(db, k, v, MDBM_INSERT)) perror("savekey");
}

/*
 * Load a db of rev key_list pair
 * key is rev
 * val is a list of entries in "root_key delta_key" format.
 */
private MDBM *
loadcset(sccs *cset)
{
	char	*rev = NULL;
	char	**keylist = 0, **cweave;
	char	*keypath, *pipe;
	char	*p, *t;
	delta	*d;
	MDBM	*db;
	int	i;
	ser_t	ser;
	char	*pathp;
	char	path[MAXPATH];

	/*
	 * Get a list of csets marked D_SET
	 */
	sccs_open(cset, 0);
	if ((cweave = cset_mkList(cset)) == (char **)-1) return (0);
	sccs_close(cset);

	if (t = proj_comppath(cset->proj)) {
		strcpy(path, t);
		pathp = path + strlen(path);
		*pathp++ = '/';
	} else {
		pathp = path;
	}
	db = mdbm_mem();
	EACH(cweave) {
		p = strchr(cweave[i], '\t');
		assert(p);
		*p++ = 0;
		if (opts.filt) {
			keypath = separator(p);
			keypath = strchr(keypath, '|');
			assert(keypath);
			keypath++;
			pipe = strchr(keypath, '|');
			assert(pipe);
			/* if -V , traverse all components... */
			unless (opts.doComp && (pipe - keypath >= 10) &&
			    strneq(&pipe[-10], "/ChangeSet", 10)) {
				if (opts.BAM) {
					/* skip unless rootkey =~ /^B:/ */
					t = separator(p);
					assert(t);
					while (*t != '|') --t;
					unless (strneq(t, "|B:", 3)) continue;
				}
				if (opts.inc || opts.exc) {
					strncpy(pathp, keypath, pipe - keypath);
					pathp[pipe-keypath] = 0;
					if (opts.inc &&
					    !match_globs(path, opts.inc, 0)) {
						continue;
					}
					if (opts.exc &&
					    match_globs(path, opts.exc, 0)) {
						continue;
					}
				}
			}
		}
		ser = atoi(cweave[i]);
		d = sfind(cset, ser);
		assert(d);
		if (!rev) {
			rev = strdup(d->rev);
			assert(keylist == NULL);
		} else if (rev && !streq(rev, d->rev)) {
			saveKey(db, rev, keylist);
			free(rev);
			rev = strdup(d->rev);
			keylist = 0;
		}
		keylist = addLine(keylist, strdup(p));
	}
	freeLines(cweave, free);

	if (rev) {
		saveKey(db, rev, keylist);
		free(rev);
	}
	if (opts.filt) fileFilt(cset, db);
	return (db);
}

/*
 * generate output for all csets in 'sc'
 *   if no dkey, marked with then D_SET
 *   if dkey, then all the component csets that are in same product
 *   cset as the component cset delta key: dkey
 * returns true if any output printed (may all be filtered)
 */
private int
cset(hash *state, sccs *sc, char *dkey, FILE *f, char *dspec)
{
	int	flags = PRS_FORCE; /* skip checks in sccs_prsdelta(), no D_SET*/
	int	iflags = INIT_NOCKSUM;
	int	i, j, found;
	char	**keys, **csets = 0;
	char	**list;
	sccs	*s;
	delta	*d, *e;
	char	*rkey;
	datum	k, v;
	char	**complist;
	int	rc = 0;
	FILE	*fsave = 0;
	char	*buf;
	size_t	len;
	struct	rstate	*rstate;

	assert(dspec);
	if (opts.newline) flags |= PRS_LF; /* for sccs_prsdelta() */
	if (opts.all) flags |= PRS_ALL;	   /* force s->prs_all */

	/* Create an empty rstate if it doesn't already exist */
	hash_insert(state, &sc->proj, sizeof(project *),
	    0, sizeof(struct rstate));
	rstate = state->vptr;	/* ptr to current rstate */

	unless (rstate->idDB) {
		buf = strdup(proj_cwd());
		/* need to be in component for loadDB() to work */
		chdir(proj_root(sc->proj));

		unless (rstate->idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
			perror("idcache");
			exit(1);
		}
		rstate->goneDB = loadDB(GONE, 0, DB_KEYSONLY|DB_NODUPS);
		chdir(buf);
		rstate->graphDB = mdbm_mem();
		if (dkey) {
			/*
			 * Cache the component graph as though the
			 * product graph selected all (non optimal,
			 * but safe).  XXX: this is doing an 'and'
			 * with -u and -/re/ patterns: the product
			 * cset && the component cset need to pass
			 * the want() test.  Is anding useful?
			 */
			for (e = sc->table; e; e = e->next) {
				if (want(sc, e)) e->flags |= D_SET;
			}
		}
		/*
		 * loads all file key pairs for csets marked D_SET
		 * Has the side-effect of unD_SETing when testing BAM||inc||exc
		 */
		rstate->csetDB = loadcset(sc);
		assert(rstate->csetDB);
		if (dkey) {
			for (e = sc->table; e; e = e->next) {
				e->flags &= ~D_SET;
			}
		}
	}
	if (dkey) {
		d = sccs_findKey(sc, dkey);
		assert(d);
		range_cset(sc, d);
	}

	/*
	 * Collect the cset in a list
	 * 'e' will have delta records from the product perspective
	 * of this cset file. 'd' will have delta records from the
	 * repo the cset file is in. If the repo is the product repo,
	 * these will be the same.
	 */
	for (e = sc->rstop; e; e = e->next) {
		if (e->flags & D_SET) {
			if (!dkey || want(sc, e)) csets = addLine(csets, e);
		}
		if (e == sc->rstart) break;
	}
	if (opts.forwards) reverseLines(csets);

	/*
	 * Walk the ordered cset list and dump the file deltas contain in
	 * each cset. The file deltas are also sorted on the fly in dumplog().
	 */
	EACH_INDEX(csets, j) {
		e = (delta *)csets[j];

		if (fflush(f)) break;		/* abort when stdout is closed */
		if (opts.doComp || opts.verbose) {
			ftrunc(opts.fcset, 0);
			if (opts.keys) {
				sccs_pdelta(sc, e, opts.fcset);
				fputc('\n', opts.fcset);
			} else {
				sccs_prsdelta(sc, e, flags, dspec, opts.fcset);
			}
		} else {
			if (opts.keys) {
				sccs_pdelta(sc, e, f);
				fputc('\n', f);
			} else {
				sccs_prsdelta(sc, e, flags, dspec, f);
			}
			continue;
		}

		/* get key list */
		k.dptr = e->rev;
		k.dsize = strlen(e->rev);
		v = mdbm_fetch(rstate->csetDB, k);
		keys = 0;
		if (v.dptr) memcpy(&keys, v.dptr, v.dsize);
		mdbm_delete(rstate->csetDB, k);

		list = 0;
		found = 0;
		complist = 0;
		EACH_INDEX(keys, i) {
			rkey = keys[i];
			dkey = separator(rkey);
			assert(dkey);
			*dkey++ = 0;
			/*
			 * bk changes -ifoo - print this cset if it
			 * changes foo even if foo is gone.
			 * XXX: Any good reason to change that behavior?
			 */
			unless (opts.doComp && strstr(dkey, "/ChangeSet|")) {
				found = 1;
			}
			s = sccs_keyinitAndCache(sc->proj, rkey, iflags,
			    rstate->idDB, rstate->graphDB);
			unless (s) {
				unless (gone(rkey, rstate->goneDB)) {
					fprintf(stderr,
					     "Cannot sccs_init(), key = %s\n",
					     rkey);
				}
				continue;
			}
			if (CSET(s) && !proj_isComponent(s->proj)) continue;
			unless (d = sccs_findKey(s, dkey)) {
				if (gone(dkey, rstate->goneDB)) continue;
				if (gone(rkey, rstate->goneDB)) continue;
				fprintf(stderr,
				    "changes: in file %s, there is a "
				    "missing delta\n\t%s\n"
				    "Please run 'bk -r check -vac' to check "
				    "for this and any other problems\n",
				    s->gfile, dkey);
				continue;
			}
			if (opts.doComp && CSET(s)) {
				/* save components */
				complist = addLine(complist, int2p(i));
				continue;
			}
			if (opts.prodOnly && CSET(s)) continue;
			/*
			 * CollectDelta() compute cset boundaries,
			 * when this function returns, "list" will contain
			 * all member deltas/dspec in "s" for this cset
			 */
			if (opts.verbose) list = collectDelta(s, d, list);
		}
		/*
		 * if we have sub-components then we must be in the
		 * product and if fmem is set then we must be
		 * filtering in doComp verbose mode.  If so we don't know
		 * if this product cset should be printed or not, so we
		 * write it to the fmem file until we know
		 */
		if (!found && complist && opts.fmem) {
			fsave = f;
			ftrunc(opts.fmem, 0);
			f = opts.fmem;
		}
		if (!opts.filt ||
		    (keys && (!opts.prodOnly || proj_isProduct(sc->proj)))) {
			/* write cset data saved above */
			buf = fmem_getbuf(opts.fcset, &len);
			fwrite(buf, 1, len, f);
		}
		if (found) rc = 1; /* Remember we printed output */
		/* sort file deltas, print it, then free it */
		dumplog(list, e, dspec, flags, f);

		/*
		 * Foreach component delta found mark them with D_SET
		 * and recursively call cset() with the new cset
		 */
		EACH(complist) {
			rkey = keys[p2int(complist[i])];
			s = sccs_keyinitAndCache(sc->proj, rkey, iflags,
			    rstate->idDB, rstate->graphDB);

			dkey = rkey + strlen(rkey) + 1;
			assert(dkey);

			/* call cset() recursively */
			if (cset(state, s, dkey, f, dspec) && fsave) {
				/*
				 * we generated output so flush the saved data
				 */
				buf = fmem_getbuf(f, &len);
				f = fsave;
				fsave = 0;
				fwrite(buf, 1, len, f);
			}
		}
		freeLines(complist, 0);
		/* reduce mem foot print, could be huge */
		freeLines(keys, free);
		if (fsave) {
			/* No output generated, restore normal output */
			f = fsave;
			fsave = 0;
		}
	}

	/*
	 * All done, clean up
	 * The above loop may break out prematurely if pager exit
	 * We need to account for it.
	 */
	freeLines(csets, 0);

	/* clear marks */
	for (e = sc->rstop; e; e = e->next) e->flags &= ~D_SET;

	return (rc);
}

private int
want(sccs *s, delta *e)
{
	char	*p;
	int	i, match;
	symbol	*sym;

	unless (opts.all || (e->type == 'D')) return (0);
	if (opts.tagOnly) {
		unless (e->flags & D_SYMBOLS) return (0);
		if (opts.tsearch) {
			match = 0;
			for (sym = s->symbols; sym; sym = sym->next) {
				unless (sym->d == e) continue;
				if (search_either(sym->symname, opts.search)) {
					match = 1;
					break;
				}
			}
			unless (match) return (0);
		}
	}
	if (opts.notusers) {
		if (p = strchr(e->user, '/')) *p = 0;
		match = 0;
		EACH(opts.notusers) match |= streq(opts.notusers[i], e->user);
		if (p) *p = '/';
		if (match) return (0);
	}
	if (opts.users) {
		if (p = strchr(e->user, '/')) *p = 0;
		match = 0;
		EACH(opts.users) match |= streq(opts.users[i], e->user);
		if (p) *p = '/';
		unless (match) return (0);
	}
	if (opts.nomerge && e->merge) return (0);
	if (opts.noempty && e->merge && !e->added && !(e->flags & D_SYMBOLS)) {
	    	return (0);
	}
	if (opts.doSearch) {
		int	i;

		EACH_COMMENT(s, e) {
			if (search_either(e->cmnts[i], opts.search)) {
				return (1);
			}
		}
		return (0);
	}
	return (1);
}


private int
send_part1_msg(remote *r, char **av)
{
	int	rc = 0, i, extra = 0;
	FILE 	*f;
	char	*cmdf, *probef = 0;	/* tmpfiles */
	char	buf[MAXLINE];

	cmdf = bktmp(0, "changes");
	f = fopen(cmdf, "w");
	assert(f);
	sendEnv(f, 0, r, ((opts.remote || opts.local) ? 0 : SENDENV_NOREPO));
	add_cd_command(f, r);
	fprintf(f, "chg_part1");
	if (opts.remote) {
		/*
		 * When doing remote, the -v -t -r options are passed in
		 * part2 of this command
		 */
		fputs(" -K", f); /* this enables the key sync code path */
	} else {
		/* Use the -L/-R cleaned options */
		for (i = 1; av[i]; i++) fprintf(f, " %s", av[i]);
	}
	fputs("\n", f);
	fclose(f);

	if (opts.remote) {
		probef = bktmp(0, 0);
		if (f = fopen(probef, "wb")) {
			rc = probekey(s_cset, 0, 0, f);
			fclose(f);
			extra = size(probef);
		} else {
			rc = 1;
		}
	}
	unless (rc) rc = send_file(r, cmdf, extra);
	unlink(cmdf);
	free(cmdf);
	if (probef) {
		f = fopen(probef, "rb");
		while ((i = fread(buf, 1, sizeof(buf), f)) > 0) {
			writen(r->wfd, buf, i);
		}
		fclose(f);
		unlink(probef);
		free(probef);
	}
	return (rc);
}

private int
send_end_msg(remote *r, char *msg)
{
	char	msgfile[MAXPATH];
	FILE	*f;
	int	rc;

	bktmp(msgfile, "changes_end");
	f = fopen(msgfile, "w");
	assert(f);
	sendEnv(f, 0, r, ((opts.remote || opts.local) ? 0 : SENDENV_NOREPO));

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in part 1
	 */
	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "chg_part2\n");
	fclose(f);

	rc = send_file(r, msgfile, strlen(msg));
	writen(r->wfd, msg, strlen(msg));
	unlink(msgfile);
	return (rc);
}

private int
send_part2_msg(remote *r, char **av, char *key_list)
{
	int	rc, i;
	char	msgfile[MAXPATH], buf[MAXLINE];
	FILE	*f;

	bktmp(msgfile, "changes_msg");
	f = fopen(msgfile, "w");
	assert(f);
	sendEnv(f, 0, r, ((opts.remote || opts.local) ? 0 : SENDENV_NOREPO));

	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "chg_part2");
	/* Use the -L/-R cleaned options */
	for (i = 1; av[i]; i++) fprintf(f, " %s", av[i]);
	fputs("\n", f);
	fclose(f);

	rc = send_file(r, msgfile, size(key_list) + 17);
	unlink(msgfile);
	f = fopen(key_list, "rt");
	assert(f);
	writen(r->wfd, "@KEY LIST@\n", 11);
	while (fnext(buf, f)) {
		writen(r->wfd, buf, strlen(buf));
		chomp(buf);
		/* mark the seen key, so we can skip it on next repo */
		unless (opts.showdups) hash_storeStr(seen, buf, "");
	}
	writen(r->wfd, "@END@\n", 6);
	fclose(f);
	return (rc);
}

/*
 * TODO: this could be merged with synckeys() in synckeys.c
 */
private int
changes_part1(remote *r, char **av, char *key_list)
{
	int	flags, fd, rc, rcsets = 0, rtags = 0;
	char	buf[MAXPATH];

	if (bkd_connect(r)) return (-1);
	if (send_part1_msg(r, av)) return (-1);
	if (r->rfd < 0) return (-1);

	unless (r->rf) r->rf = fdopen(r->rfd, "r");
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if ((rc = remote_lock_fail(buf, 1))) {
		return (rc); /* -2 means locked */
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfoBlock(r)) {
			fprintf(stderr, "changes: premature disconnect?\n");
			return (-1);
		}
		getline2(r, buf, sizeof(buf));
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		return (-1);
	}
	if (get_ok(r, buf, 1)) return (-1);

	if (opts.remote == 0) {
		getline2(r, buf, sizeof(buf));
		unless (streq("@CHANGES INFO@", buf)) {
			return (0); /* protocol error */
		}
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (streq("@END@", buf)) break;

			/*
			 * Check for write error, in case
			 * our pager terminate early
			 */
			if (buf[0] == BKD_DATA) {
				if (writen(1, &buf[1], strlen(buf) - 1) < 0) break;
				if (write(1, "\n", 1) < 0) break;
			}
		}
		return (0);
	}

	/*
	 * What we want is: "remote => bk _prunekey => keylist"
	 */
	bktmp(key_list, "keylist");
	fd = open(key_list, O_CREAT|O_WRONLY, 0644);
	flags = PK_REVPREFIX|PK_RKEY;
	rc = prunekey(s_cset, r, seen, fd, flags, 0, NULL, &rcsets, &rtags);
	if (rc < 0) {
		switch (rc) {
		    case -2:
			getMsg("unrelated_repos", 0, '=', stderr);
			break;
		    case -3:
			getMsg("no_repo", 0, '=', stderr);
			break;
		}
		close(fd);
		disconnect(r, 2);
		return (-1);
	}
	close(fd);
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	return (rcsets + rtags);
}

private int
changes_part2(remote *r, char **av, char *key_list, int ret)
{
	int	rc = 0;
	int	rc_lock;
	char	buf[MAXLINE];

	if ((r->type == ADDR_HTTP) && bkd_connect(r)) {
		return (1);
	}

	unless (r->rf) r->rf = fdopen(r->rfd, "r");
	if (ret == 0){
		send_end_msg(r, "@NOTHING TO SEND@\n");
		/* No handshake?? */
		goto done;
	}
	send_part2_msg(r, av, key_list);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);

	getline2(r, buf, sizeof(buf));
	if (rc_lock = remote_lock_fail(buf, 0)) {
		rc = rc_lock;
		goto done;
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfoBlock(r)) {
			rc = -1; /* protocol error */
			goto done;
		}
	}

	getline2(r, buf, sizeof(buf));
	unless (streq("@CHANGES INFO@", buf)) {
		rc = -1; /* protocol error */
		goto done;
	}
	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (streq("@END@", buf)) break;
		if (buf[0] == BKD_DATA) {
			if (writen(1, &buf[1], strlen(buf) - 1) < 0) break;
			write(1, "\n", 1);
		}
	}

done:	unlink(key_list);
	disconnect(r, 1);
	wait_eof(r, 0);
	return (rc);
}

private int
_doit_remote(char **av, char *url)
{
	char 	key_list[MAXPATH] = "";
	char	*tmp;
	int	rc;
	remote	*r;

	r = remote_parse(url, REMOTE_BKDURL | REMOTE_ROOTKEY);
	unless (r) {
		fprintf(stderr, "invalid url: %s\n", url);
		return (1);
	}

	/* Quote the dspec for the other side */
	for (rc = 0; av[rc]; ++rc) {
		unless (strneq("-d", av[rc], 2)) continue;
		tmp = aprintf("'-d%s'", &av[rc][2]);
		free(av[rc]);
		av[rc] = tmp;
	}
	rc = changes_part1(r, av, key_list);
	if (rc >= 0 && opts.remote) {
		rc = changes_part2(r, av, key_list, rc);
	}
	disconnect(r, 2);
	remote_free(r);
	if (key_list[0]) unlink(key_list);
	return (rc);
}

private int
doit_remote(char **nav, char *url)
{
	int	rc;
	int	i = 0;

	for (i = 1; i <= 5; i++) {
		rc = _doit_remote(&nav[1], url);
		if (rc != -2) break; /* -2 means locked */
		if (getenv("BK_REGRESSION")) break;
		fprintf(stderr,
		    "changes: remote locked, trying again...\n");
		sleep(i * 2);
	}
	if (rc == -2) fprintf(stderr, "changes: giving up on remote lock.\n");
	return (rc ? 1 : 0);
}
