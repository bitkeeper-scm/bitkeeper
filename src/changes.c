#include "bkd.h"
#include "range.h"

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
	u32	diffs:1;	/* show diffs with verbose mode */
	u32	tsearch:1;	/* pattern applies to tags instead of cmts */
	u32	BAM:1;		/* only include BAM files */

	search	search;		/* -/pattern/[i] matches comments w/ pattern */
	char	*dspec;		/* override dspec */
	char	**users;	/* lines list of users to include */
	char	**notusers;	/* lines list of users to exclude */
	char	**inc;		/* list of globs for files to include */
	char	**exc;		/* list of globs for files to exclude */

	/* not opts */
	FILE	*f;		/* global for recursion */
	sccs	*s;		/* global for recursion */
	char	*spec;		/* global for recursion */

	RANGE	rargs;
} opts;

typedef struct slog {
	time_t	date;
	char	*gfile;
	char	*log;
	delta	*delta;
	ser_t	serial;
} slog;

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
private	void	cset(sccs *cset, MDBM *csetDB, FILE *f, char *dspec);
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
	while ((c = getopt(ac, av, "1aBc;Dd;efhi;kLmnqRr;tTu;U;v/;x;")) != -1) {
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
		    case 'd': opts.dspec = optarg; break;
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
		    case 'q': opts.urls = 0; break;
		    case 't': opts.tagOnly = 1; break;		/* doc 2.0 */
		    case 'T': opts.timesort = 1; break;
		    case 'u':
			opts.users = addLine(opts.users, strdup(optarg));
			break;
		    case 'U':
			opts.notusers = addLine(opts.notusers, strdup(optarg));
			break;
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

	/* ERROR check options */
	/* XXX: could have rev range limit output -- whose name space? */
	if ((opts.local || opts.remote) && opts.rargs.rstart) goto usage;

	if (opts.keys && (opts.verbose||opts.html||opts.dspec)) goto usage;
	if (opts.html && opts.dspec) goto usage;

	if (opts.local || opts.remote || (av[optind] == 0)) {
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

	r = remote_parse(url, REMOTE_BKDURL);
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
	int	status, i;
	int	ac = nac, rc = 0;
	int	all = 0;

	nav[ac++] = strdup("-");
	assert(ac < 30);
	nav[ac] = 0;
	EACH(urls) {
		if (opts.urls) {
			printf("==== changes -L %s ====\n", urls[i]);
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
pdelta(delta *e)
{
	unless (e->flags & D_SET) return(0);
	if (opts.keys) {
		sccs_pdelta(opts.s, e, opts.f);
		fputc('\n', opts.f);
	} else {
		if (opts.all || (e->type == 'D')) {
			int	flags = opts.all ? PRS_ALL : 0;

			if (opts.newline) flags |= PRS_LF;
			sccs_prsdelta(opts.s, e, flags, opts.spec, opts.f);
		}
	}
	return (fflush(opts.f) || opts.one);
}

private int
recurse(delta *d)
{
	if (d->next) {
		if (recurse(d->next)) return (1);
	}
	return (pdelta(d));
}

/*
 * XXX May need to change the @ to BK_FS in the following dspec
 */
#define	DSPEC	"$unless(:CHANGESET:){  }" \
		":DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:} " \
		"+:LI: -:LD:\n" \
		"$each(:C:){$unless(:CHANGESET:){  }  (:C:)\n}" \
		"$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}" \
		"$if(:MERGE:){$unless(:CHANGESET:){  }  MERGE: " \
		":MPARENT:\n}\n"
#define	VSPEC	"$if(:CHANGESET:){\n#### :DPN: ####\n}" \
		"$else{\n==== :DPN: ====\n}" \
		":Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:} " \
		"$unless(:CHANGESET:){+:LI: -:LD:}" \
		"\n" \
		"$each(:C:){  (:C:)\n}" \
		"$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}" \
		"$unless(:CHANGESET:){:DIFFS_UP:}"
#define	HSPEC	"<tr bgcolor=lightblue><td font size=4>" \
		"&nbsp;:Dy:-:Dm:-:Dd: :Th:::Tm:&nbsp;&nbsp;" \
		":P:@:HT:&nbsp;&nbsp;:I:</td></tr>\n" \
		"$if(:TAG:){<tr bgcolor=yellow><td>" \
		"$each(:SYMBOL:){&nbsp;TAG: (:SYMBOL:)<br>\n}" \
		"</td></tr>\n}" \
		"<tr bgcolor=white><td>" \
		"$each(:C:){&nbsp;(:C:)<br>\n}</td></tr>\n"
#define	HSPECV	"<tr bgcolor=" \
		"$if(:TYPE:=BitKeeper|ChangeSet){lightblue>}" \
		"$if(:TYPE:=BitKeeper){#f0f0f0>}" \
		"<td font size=4>&nbsp;" \
		"$if(:TYPE:=BitKeeper|ChangeSet){" \
		":Dy:-:Dm:-:Dd: :Th:::Tm:&nbsp;&nbsp;" \
		":P:@:HT:&nbsp;&nbsp;:I:}" \
		"$if(:TYPE:=BitKeeper){&nbsp;:DPN: :I:}" \
		"</td></tr>\n" \
		"$if(:TAG:){<tr bgcolor=yellow><td>" \
		"$each(:SYMBOL:){&nbsp;TAG: (:SYMBOL:)<br>\n}" \
		"</td></tr>\n}" \
		"<tr bgcolor=white><td>" \
		"$each(:C:){&nbsp;" \
		"$if(:TYPE:=BitKeeper){&nbsp;&nbsp;&nbsp;&nbsp;}" \
		"(:C:)<br>\n}" \
		"$unless(:C:){" \
		"$if(:TYPE:=BitKeeper){&nbsp;&nbsp;&nbsp;&nbsp;}" \
		"&lt;no comments&gt;}" \
		"</td></tr>\n"

private int
doit(int dash)
{
	char	cmd[MAXKEY];
	char	*spec;
	pid_t	pid;
	sccs	*s = 0;
	delta	*e;
	int	rc = 1;
	MDBM	*csetDB = 0;

	if (opts.dspec && !opts.html) {
		spec = opts.dspec;
	} else if (opts.html) {
		spec = opts.verbose ? HSPECV : HSPEC;
	} else {
		spec = opts.diffs ? VSPEC : DSPEC;
	}
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
		for (e = s->table; e; e = e->next) {
			if (want(s, e)) e->flags |= D_SET;
		}
	}
	//assert(!SET(s));
	if (opts.verbose || opts.inc || opts.exc || opts.BAM) {
		/* loads all file key pairs for csets marked D_SET */
		csetDB = loadcset(s);
	}
	if (opts.inc || opts.exc || opts.BAM) fileFilt(s, csetDB);

	/*
	 * What we want is: this process | pager
	 */
	pid = mkpager();

	if (opts.html) {
		fputs("<html><body bgcolor=white>\n"
		    "<table align=center bgcolor=black cellspacing=0 "
		    "border=0 cellpadding=0><tr><td>\n"
		    "<table width=100% cellspacing=1 border=0 cellpadding=1>"
		    "<tr><td>\n", stdout);
		fflush(stdout);
	}
	if (opts.verbose) {
		cset(s, csetDB, stdout, spec);
	} else {
		opts.f = stdout;
		opts.s = s;
		opts.spec = spec;
		if (opts.forwards) {
			recurse(s->table);
		} else {
			for (e = s->table; e; e = e->next) {
				if (pdelta(e)) break;
			}
		}
	}
	if (opts.html) {
		fprintf(stdout, "</td></tr></table></table></body></html>\n");
	}
	if (pid > 0) {
		fclose(stdout);
		waitpid(pid, 0, 0);
	}
	rc = 0;
next:
	if (csetDB) {
		kvpair	kv;
		char	**keys;

		EACH_KV(csetDB) {
			memcpy(&keys, kv.val.dptr, sizeof (char **));
			freeLines(keys, free);
		}
		mdbm_close(csetDB);
	}
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
/*
 * Note that the next two are identical except for
 * the date and serial subtraction orders is flipped
 */

private int
dateback(const void *a, const void *b)
{
	slog	*d1, *d2;
	int	cmp;

	d1 = *((slog**)a);
	d2 = *((slog**)b);
	if (cmp = (d2->date - d1->date)) return (cmp);
	if (cmp = strcmp(d1->gfile, d2->gfile)) return (cmp);
	return (d2->serial - d1->serial);
}

private int
dateforw(const void *a, const void *b)
{
	slog	*d1, *d2;
	int	cmp;

	d1 = *((slog**)a);
	d2 = *((slog**)b);
	if (cmp = (d1->date - d2->date)) return (cmp);
	if (cmp = strcmp(d1->gfile, d2->gfile)) return (cmp);
	return (d1->serial - d2->serial);
}

/*
 * Mostly same as dateback/dateforw - but swap gfile and date check
 */
private int
strback(const void *a, const void *b)
{
	slog	*d1, *d2;
	int	cmp;

	d1 = *((slog**)a);
	d2 = *((slog**)b);
	if (cmp = strcmp(d1->gfile, d2->gfile)) return (cmp);
	if (cmp = (d2->date - d1->date)) return (cmp);
	return (d2->serial - d1->serial);
}

/* mostly same as dateforw - swap gfile and time stamp check */
private int
strforw(const void *a, const void *b)
{
	slog	*d1, *d2;
	int	cmp;

	d1 = *((slog**)a);
	d2 = *((slog**)b);
	if (cmp = strcmp(d1->gfile, d2->gfile)) return (cmp);
	if (cmp = (d1->date - d2->date)) return (cmp);
	return (d1->serial - d2->serial);
}

private	void
dumplog(char **list, FILE *f)
{
	slog	*ll;
	int	i;

	if (opts.timesort) {
		sortLines(list, opts.forwards ? dateforw : dateback);
	} else {
		sortLines(list, opts.forwards ? strforw : strback);
	}

	/*
	 * Print the sorted list
	 */
	EACH(list) {
		ll = (slog *)list[i];
		fprintf(f, "%s", ll->log);
		free(ll->log);
		free(ll->gfile);
		free(ll);
	}

	freeLines(list, 0);
	return;
}

/*
 * Cache the sccs struct to avoid re-initing the same sfile
 */
private sccs *
sccs_keyinitAndCache(char *key,
	int	flags, MDBM **idDB, MDBM *graphDB, MDBM *goneDB)
{
	static	int	rebuilt = 0;
	datum	k, v;
	sccs	*s;
	delta	*d;

	k.dptr = key;
	k.dsize = strlen(key);
	v = mdbm_fetch(graphDB, k);
	if (v.dptr) { /* cache hit */
		memcpy(&s, v.dptr, sizeof (sccs *));
		return (s);
	}
 retry:
	s = sccs_keyinit(key, flags|INIT_NOWARN, *idDB);
	unless (s || gone(key, goneDB)) {
		unless (rebuilt) {
			mdbm_close(*idDB);
			if (sccs_reCache(1)) {
				fprintf(stderr,
				    "changes: cannot build %s\n",
				    IDCACHE);
			}
			unless (*idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
				perror("idcache");
			}
			rebuilt = 1;
			goto retry;
		}
		fprintf(stderr, "Cannot sccs_init(), key = %s\n", key);
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
	}
	return (s);
}

/*
 * Given a "top" delta "d", this function computes ChangeSet boundaries
 * It collect all the deltas inside a changeset and stuff them to "list".
 */
private char **
collectDelta(sccs *s, delta *d, char **list, char *dspec, int flags)
{
	slog	*ll;
	delta	*e;

	/*
	 * Walk d->parent and d->merge recursively to find cset boundaries
	 * and collect the deltas/dspec-output along the way
	 */
	do {
		d->flags |= D_SET;

		/* add delta to list */
		ll = calloc(sizeof (slog), 1);
		ll->log = sccs_prsbuf(s, d, flags, dspec);
		ll->date = d->date;
		ll->gfile = strdup(s->gfile);
		ll->serial = d->serial;
		list = addLine(list, ll);

		if (d->merge) {
			e = sfind(s, d->merge);
			assert(e);
			unless (e->flags & (D_SET|D_CSET)) {
				list = collectDelta(s, e, list, dspec, flags);
			}
		}
		d = d->parent;
	} while (d && !(d->flags & (D_SET|D_CSET)));
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
	char	**keylist = 0;
	char	*keypath, *pipe;
	char	*p, *t;
	FILE	*f;
	MDBM	*db;
	char	tmp[MAXPATH], buf[2 * MAXKEY + 100], path[MAXPATH];

	bktmp(tmp, "loadcset");
	sccs_cat(cset, SILENT|GET_NOHASH|GET_REVNUMS|PRINT, tmp);
	f = fopen(tmp, "rt");

	db = mdbm_mem();
	while (fnext(buf, f)) {
		chomp(buf);
		p = strchr(buf, '\t');
		assert(p);
		*p++ = 0;
		if (opts.BAM) {
			/* skip unless rootkey =~ /^B:/ */
			t = separator(p);
			assert(t);
			while (*t != '|') --t;
			unless (strneq(t, "|B:", 3)) continue;
		}
		if (opts.inc || opts.exc) {
			keypath = separator(p);
			keypath = strchr(keypath, '|');
			assert(keypath);
			keypath++;
			pipe = strchr(keypath, '|');
			assert(pipe);
			path[0] = 0;
			strncat(path, keypath, pipe - keypath);
			if (opts.inc && 
			    !match_globs(path, opts.inc, 0)) {
				continue;
			}
			if (opts.exc && match_globs(path, opts.exc, 0)){
				continue;
			}
		}
		if (!rev) {
			rev = strdup(buf);
			assert(keylist == NULL);
		} else if (rev && !streq(rev, buf)) {
			saveKey(db, rev, keylist);
			free(rev);
			rev = strdup(buf);
			keylist = 0;
		}
		keylist = addLine(keylist, strdup(p));
	}
	fclose(f);

	if (rev) {
		saveKey(db, rev, keylist);
		free(rev);
	}
	
	unlink(tmp);
	return (db);
}

private void
cset(sccs *cset, MDBM *csetDB, FILE *f, char *dspec)
{
	int	flags = opts.all ? PRS_ALL : 0;
	int	iflags = INIT_NOCKSUM;
	int 	i, j;
	char	**keys, **csets = 0;
	char	**list;
	delta	*e;
	kvpair	kv;
	datum	k, v;
	MDBM 	*idDB, *goneDB, *graphDB;

	assert(dspec);
	if (opts.newline) flags |= PRS_LF; /* for sccs_prsdelta() */

	/*
	 * Init idDB, goneDB, graphDB and csetDB
	 */
	unless (idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
		perror("idcache");
		exit(1);
	}
	goneDB = loadDB(GONE, 0, DB_KEYSONLY|DB_NODUPS);
	graphDB = mdbm_mem();

	/*
	 * Collect the cset in a list
	 */
	for (e = cset->table; e; e = e->next) {
		unless (e->flags & D_SET) continue;
		csets = addLine(csets, e);
	}

	if (opts.forwards) reverseLines(csets);

	/*
	 * Walk the ordered cset list and dump the file deltas contain in
	 * each cset. The file deltas are also sorted on the fly in dumplog().
	 */

	EACH_INDEX(csets, j) {
		e = (delta *)csets[j];

		sccs_prsdelta(cset, e, flags, dspec, f);
		/* get key list */
		k.dptr = e->rev;
		k.dsize = strlen(e->rev);
		v = mdbm_fetch(csetDB, k);
		unless (v.dptr) continue;	/* no files */
		
		memcpy(&keys, v.dptr, v.dsize);
		mdbm_delete(csetDB, k);

		list = 0;
		EACH_INDEX(keys, i) {
			sccs	*s;
			delta	*d;
			char	*dkey;

			dkey = separator(keys[i]);
			assert(dkey);
			*dkey++ = 0;
			s = sccs_keyinitAndCache(
				keys[i], iflags, &idDB, graphDB, goneDB);
			unless (s && !CSET(s)) continue;
			if (mdbm_fetch_str(goneDB, dkey)) continue;
			d = sccs_findKey(s, dkey);
			assert(d);

			/*
			 * CollectDelta() compute cset boundaries,
			 * when this function returns, "list" will contain
			 * all member deltas/dspec in "s" for this cset
			 */
			list = collectDelta(s, d, list, dspec, flags);
		}
		/* reduce mem foot print, could be huge */
		freeLines(keys, free);

		/* sort file dspec, print it, then free it */
		dumplog(list, f);
		if (fflush(f)) break;
	}

	/*
	 * All done, clean up
	 * The above loop may break out prematurely if pager exit
	 * We need to account for it.
	 */
	freeLines(csets, 0);

	EACH_KV(graphDB) {
		sccs	*s;

		memcpy(&s, kv.val.dptr, sizeof (sccs *));
		if (s) sccs_free(s);
	}

	mdbm_close(graphDB);
	mdbm_close(idDB);
	mdbm_close(goneDB);
	return;
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
	if (r->path) add_cd_command(f, r);
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
			rc = probekey(s_cset, 0, f);
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
	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
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

	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
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

	if (bkd_connect(r, 0, 1)) return (-1);
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

	if ((r->type == ADDR_HTTP) && bkd_connect(r, 0, 0)) {
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

	r = remote_parse(url, REMOTE_BKDURL);
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
	} else {
		disconnect(r, 1);
	}
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
