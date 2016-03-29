/*
 * Copyright 2000-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bkd.h"
#include "range.h"
#include "nested.h"

private struct {
	u32	all:1;		/* list all, including tags etc */
	u32	doSearch:1;	/* search the comments list */
	u32	showdups:1;	/* do not filter duplicates when multi-parent */
	u32	forwards:1;	/* like prs -f */
	u32	html:1;		/* do html style output */
	u32	json:1;		/* do json output */
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
	u32	printComp:1;	/* whether to force-print the component csets */
	u32	diffs:1;	/* show diffs with verbose mode */
	u32	tsearch:1;	/* pattern applies to tags instead of cmts */
	u32	BAM:1;		/* only include BAM files */
	u32	filt:1;		/* output filtered by filenames */
	u32	sel:1;		/* output limited by filenames */
	u32	chgurl:1;	/* running 'bk changes URL', ignore local */
	u32	noMeta:1;	/* auto or --no-meta */
	u32	standalone:1;	/* --standalone: treat comps as standalone */
	u32	sameComp:1;	/* --same-component */
	u32	sparseOk:1;	/* --sparse-ok */
	u32	filter:1;	/* --filter */
	u32	startAfter:1;	/* --start-after */
	u32	endBefore:1;	/* --end-before */
	u32	around:1;	/* --around */

	u32	num;		/* -%u: stop after printing n entries */
	search	search;		/* -/pattern/[i] matches comments w/ pattern */
	char	*dspec;		/* override dspec */
	char	**begin;	/* $begin{....} */
	char	*end;		/* $end{....} */
	char	**users;	/* lines list of users to include */
	char	**notusers;	/* lines list of users to exclude */
	char	**incS;		/* list of globs for files to include */
	char	**excS;		/* list of globs for files to exclude */
	char	**incF;		/* list of globs for files to filter in */
	char	**excF;		/* list of globs for files to filter out */
	char	*dspecfile;	/* name the file where the dspec is */
	/* Pagination  (these are useful when combined with opts.num (limit) */
	char	*pagekey;

	RANGE	rargs;
	u32	rflags;		/* range flags */
} opts;

typedef struct slog {
	sccs	*s;
	ser_t	d;
	char	*path;
} slog;

/* per-repo state */
struct rstate {
	MDBM	*idDB;
	MDBM	*goneDB;
	hash	*csetDB;	/* serial => struct cstate */
	MDBM	*graphDB;
};

typedef	struct {
	char	**keylist;	/* list of keys from the weave (filtered) */
	u32	hasFiles:1;	/* does it have files? */
	u32	hasIncFiles:1;	/* does this cset have included files */
	u32	hasExcFiles:1;	/* does it have excluded files? */
} cstate;

private int	doit(int dash);
private int	want(sccs *s, ser_t e);
private int	send_part1_msg(remote *r, char **av);
private int	send_end_msg(remote *r, char *msg);
private int	send_part2_msg(remote *r, char **av, char *key_list);
private int	changes_part1(remote *r, char **av, char *key_list);
private int	changes_part2(remote *r, char **av, char *key_list, int ret);
private int	_doit_remote(char **av, char *url);
private int	doit_remote(char **nav, char *url);
private int	doit_local(char ***nav, char **urls);
private	int	cset(hash *state, sccs *cset, char *dkey, char *pkey, FILE *f,
		    char *dspec);
private	hash	*loadcset(sccs *cset);
private	int	prepSearch(char *str);
private	int	skipPath(char *path, int *incp, int *excp);
private	int	printCset(int hasInc, int hasExc);
private	void	checkPresent(sccs *s, char **inc, char **exc);

private	hash	*seen; /* list of keys seen already */
private	sccs	*s_cset;

int
changes_main(int ac, char **av)
{
	int	i, c;
	int	rc = 0;
	char	**nav = 0;
	char	**urls = 0, **rurls = 0, **lurls = 0;
	char	*normal;
	char	*searchStr = 0;
	char	buf[MAXPATH];
	pid_t	pid = 0; /* pager */
	longopt	lopts[] = {
		{ "no-meta", 300 },		/* don't show meta files */
						/* seems more like --no attr */
		{ "html", 301 },		/* old -h */
		{ "same-component", 302 },	/* undocumented?  LMXXX */
		{ "dspecf;", 303 },		/* let user pass in dspec */
		{ "filter", 310 },		/* -i/-x filter not select */
						/* undocumented on purpose */
		{ "json", 315 },		/* output json */
		{ "limit:", 318},		/* how many csets to output */
		{ "sparse-ok", 320 },		/* don't error on non-present */
		{ "standalone", 'S' },		/* treat comps as standalone */
		{ "start-after:", 325},		/* start after key */
		{ "end-before:", 326},		/* end list before key */
		{ "around-key:", 327},		/* list is around key */
		{ "lattice", 330 },		/* range is a lattice */
		{ "longest", 340 },		/* longest path */
		{ "dspecbegin;", 350 },		/* dspec $begin clause */
		{ 0, 0 }
	};

	bzero(&opts, sizeof(opts));
	opts.showdups = opts.urls = opts.noempty = 1;

	nav = addLine(nav, strdup("bk"));
	nav = addLine(nav, strdup("changes"));
	/*
	 * XXX Warning: The 'changes' command can NOT use the -K
	 * option.  that is used internally by the bkd_changes part1 cmd.
	 */
	while ((c =
	    getopt(ac, av, "0123456789aBc;Dd;efi;kLmnPqRr;StTu;U;Vv/;x;",
		lopts)) != -1) {
		unless (c == 'L' || c == 'R' || c == 'D' || c == 302) {
			nav = bk_saveArg(nav, av, c);
		}
		switch (c) {
		    /*
		     * Note: do not add option 'K', it is reserved
		     * for internal use by bkd_changes.c, part1
		     */
		    case '0': case '1': case '2': case '3': case '4':
		    case '5': case '6': case '7': case '8': case '9':
			opts.num = opts.num * 10 + (c - '0');
			break;
		    case 'a': opts.all = 1; opts.noempty = 0; break;
		    case 'B': opts.BAM = 1; break;
		    case 'c':
			if (range_addArg(&opts.rargs, optarg, 1)) usage();
			break;
		    case 'D': opts.urls = opts.showdups = 0; break;
		    case 'd':
			if (opts.dspec) usage();
			opts.dspec = strdup(optarg);
			break;
		    case 'e': opts.noempty = !opts.noempty; break;
		    case 'f': opts.forwards = 1; break;
		    case 301: opts.html = 1; opts.urls = 0; break;
		    case 'i':
			opts.incS = addLine(opts.incS, strdup(optarg));
			break;
		    /* case 'K': reserved */
		    case 'k':
		    	opts.keys = 1;
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
		    case 'V': opts.doComp = opts.printComp = 1; break;
		    case 'v':
			if (opts.verbose) opts.diffs = 1;
			opts.verbose = 1;
			break;
		    case 'r':
			if (range_addArg(&opts.rargs, optarg, 0)) usage();
			break;
		    case 'x':
			opts.excS = addLine(opts.excS, strdup(optarg));
			break;
		    case '/': searchStr = optarg; break;
		    case 'L': opts.local = 1; break;
		    case 'R': opts.remote = 1; break;
		    case 'S':
			opts.standalone = 1;
			opts.prodOnly = 1;
			break;
		    case 300: /* --no-meta */
		    	opts.noMeta = 1;
			break;
		    case 302: /* --same-component */
			opts.sameComp = 1;
			break;
		    case 303: /* --dspecf */
			opts.dspecfile = optarg;
			break;
		    case 310:	/* --filter */
			opts.filter = 1;
			break;
		    case 315: /* --json */
			opts.json = 1;
			break;
		    case 318:	/* --limit */
			opts.num = strtol(optarg, 0, 10);
			break;
		    case 320:	/* --sparse-ok */
			opts.sparseOk = 1;
			break;
		    case 325:	/* --start-after */
			if (opts.pagekey) usage();
			opts.startAfter = 1;
			opts.pagekey = optarg;
			break;
		    case 326:	/* --end_before */
			if (opts.pagekey) usage();
			opts.endBefore = 1;
			opts.pagekey = optarg;
			break;
		    case 327:	/* --around-key */
			if (opts.pagekey) usage();
			opts.around = 1;
			opts.pagekey = optarg;
			break;
		    case 330: /* --lattice */
			if (opts.rflags) bk_badArg(c, av);
			opts.rflags = RANGE_LATTICE;
			break;
		    case 340: /* --longest */
			if (opts.rflags) bk_badArg(c, av);
			opts.rflags = RANGE_LONGEST;
			break;
		    case 350: /* --dspecbegin */
			opts.begin = addLine(opts.begin, strdup(optarg));
			break;
		    default: bk_badArg(c, av);
		}
	}
	unless (opts.rflags) opts.rflags = RANGE_SET;
	if (opts.dspecfile) {
		if (opts.dspec) {
			fprintf(stderr,
			    "changes: cannot combine dspec and dspecfile\n");
			return (1);
		}
		unless (opts.dspec = loadfile(opts.dspecfile, 0)) {
			fprintf(stderr,
			    "changes: cannot load file \"%s\"\n",
			    opts.dspecfile);
			return (1);
		}
	}

	if (opts.filter) {
		opts.incF = opts.incS;
		opts.excF = opts.excS;
		opts.incS = opts.excS = 0;
	}

	opts.filt = opts.BAM || opts.incF || opts.excF;
	opts.sel = opts.incS || opts.excS;

	/* ERROR check options */
	/* XXX: could have rev range limit output -- whose name space? */
	if ((opts.local || opts.remote) && opts.rargs.rstart) usage();
	if (proj_isEnsemble(0)) {
		if (opts.verbose && !opts.prodOnly) opts.doComp = 1;
		if ((opts.filt || opts.sel) && !opts.doComp && !opts.BAM) {
			opts.doComp = opts.prodOnly = 1;
		}
	}
	/* only one option that specifies a dspec */
	i = 0;
	if (opts.keys) ++i;
	if (opts.html) ++i;
	if (opts.json) ++i;
	if (opts.diffs) ++i;
	if (opts.dspec) ++i;
	if (i > 1) usage();
	/* and -k can't be used with -v */
	if (opts.keys && opts.verbose) usage();

	/* can't do around without picking a number of csets */
	if (opts.around && !opts.num) usage();

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

	if (searchStr && prepSearch(searchStr)) usage();

	/* terminate list of args with -L and -R removed */
	nav = addLine(nav, 0);

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
			f = fmem();
			while ((i = fread(buf, 1, sizeof(buf), stdin)) > 0) {
				fwrite(buf, 1, i, f);
			}
			free(opts.dspec);
			opts.dspec = fmem_close(f, 0);
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
				rc = 1;
				goto out;
			}
		}
		unless (lurls || rurls) usage();
		unless (rurls) rurls = lurls;
		seen = hash_new(HASH_MEMHASH);
		pid = mkpager();
		putenv("BK_PAGER=cat");
		if (proj_isComponent(0) && !opts.standalone) {
			proj_cd2product();
		} else {
			proj_cd2root();
		}
		s_cset = sccs_csetInit(SILENT|INIT_NOCKSUM|INIT_MUSTEXIST);
		unless (s_cset) {
			fprintf(stderr, "changes: missing ChangeSet file\n");
			rc = 1;
			goto out;
		}
	}
	if (opts.local) {
		if (rc = doit_local(&nav, lurls)) goto out;
	}
	if (opts.remote) {
		char	*cpath = 0;

		if (proj_isComponent(0)) {
			/*
			 * Ideally we want the remote path, but if we can't
			 * get it then the local path will do.
			 */
			if (cpath=getenv("BKD_COMPONENT_PATH")){
				cpath = strdup(cpath);
			} else {
				cpath = proj_relpath(proj_product(0),
				    proj_root(0));
			}
		}
		EACH(rurls) {
			if (opts.urls) {
				if (cpath) {
					printf("==== changes -R %s/%s ====\n",
					    rurls[i], cpath);
				} else {
					printf("==== changes -R %s ====\n",
					    rurls[i]);
				}
				fflush(stdout);
			}
			rc |= doit_remote(nav, rurls[i]);
		}
		FREE(cpath);
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
		unless (opts.sameComp) {
			/*
			 * Tell changes not to add ROOTKEY=proj_rootkey() to
			 * the URL when talking to the remote bkd.
			 * This is used for 'bk changes URL' so the CWD
			 * doesn't change the output.
			 *
			 * --same-component can be used to keep this behavior
			 * this is used in collapse
			 */
			opts.chgurl = 1; /* ignore local repo */
		}
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
		if (proj_isComponent(0) && !opts.standalone) {
			proj_cd2product();
		} else {
			if (proj_cd2root()) {
				fprintf(stderr, "Can't find package root\n");
				exit(1);
			}
		}
		s_cset = sccs_csetInit(SILENT|INIT_NOCKSUM|INIT_MUSTEXIST);
		unless (s_cset) {
			fprintf(stderr, "changes: missing ChangeSet file\n");
			rc = 1;
			goto out;
		}
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
	freeLines(opts.begin, free);
	if (opts.end) free(opts.end);
	if (seen) hash_free(seen);
	freeLines(nav, free);
	freeLines(urls, free);
	if (rurls != lurls) freeLines(rurls, free);
	freeLines(lurls, free);
	return (rc);
}

/*
 * Make sure the glob patterns don't match missing components
 */
private	void
checkPresent(sccs *s, char **inc, char **exc)
{
	int	i, j, k;
	int	errors = 0;
	char	**list[2] = {inc, exc};
	int	missing = 0;
	nested	*n;
	comp	*c;

	unless (proj_isProduct(s->proj)) return;
	unless (n = nested_init(s, 0, 0, NESTED_PENDING)) {
		fprintf(stderr, "nested_init() failed\n");
		exit(1);
	}
	/* check for globs that don't have dir */
	EACH_STRUCT(n->comps, c, k) {
		if (C_PRESENT(c)) continue;
		for (i = 0; i < 2; i++) {
			EACH_INDEX(list[i], j) {
				if (!streq("ChangeSet", basenm(list[i][j]))
				    && paths_overlap(c->path, list[i][j])) {
					fprintf(stderr,
					    "%s is not present and "
					    "-%c%s could match files in it.\n",
					    c->path,
					    i ? 'x' : 'i', list[i][j]);
					errors = 1;
				}
				if (is_glob(list[i][j])) missing++;
			}
		}
	}
	nested_free(n);
	if (missing) {
		for (i = 0; i < 2; i++) {
			EACH_INDEX(list[i], j) {
				unless (strchr(list[i][j], '/')) {
					fprintf(stderr,
					    "There are missing components "
					    "where -%c%s could match files.\n",
					    i ? 'x' : 'i', list[i][j]);
					errors = 1;
				}
			}
		}
	}
	if (errors) {
		fprintf(stderr, "Use --sparse-ok if you want to ignore "
		    "non-present components.\n");
		exit(1);
	}
}

private	int
prepSearch(char *str)
{
	char	*p;

	if ((p = strrchr(str, '/')) && (p = strchr(p, 't'))) {
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
	 * What we get here is: bk synckey -l -S url | bk changes opts -
	 */
	if (opts.showdups) {
		f = popenvp(nav + 1, "w");
		assert(f);
	}

	r = remote_parse(url, REMOTE_BKDURL | REMOTE_ROOTKEY);
	assert(r);

	bktmp(tmpf);
	p = fopen(tmpf, "w");
	rc = synckeys(r, s_cset, SK_LKEY|SK_SYNCROOT, p);
	fclose(p);
	remote_free(r);
	p = fopen(tmpf, "r");
	assert(p);
	while (fnext(buf, p)) {
		if (opts.showdups) {
			fputs(buf, f);
		} else {
			int	*v;

			unless (v = hash_fetchStrMem(seen, buf)) {
				v = hash_insertStrMem(seen,
				    buf, 0, sizeof(int));
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
doit_local(char ***nav, char **urls)
{
	FILE	*f;
	char	*p, *cpath = 0;
	int	status, i;
	int	rc = 0;
	int	all = 0;

	*nav = addLine(*nav, strdup("-"));
	*nav = addLine(*nav, 0);
	if (proj_isComponent(0)) {
		cpath = proj_relpath(proj_product(0), proj_root(0));
	}
	EACH(urls) {
		if (opts.urls) {
			if ((p = getenv("BK_STATUS")) &&
			    streq(p, "LOCAL_WORK")) {
				printf("#### Not updating "
				    "due to the following local work:\n");
			} else if (cpath) {
				printf("==== changes -L %s/%s ====\n",
				    urls[i], cpath);
			} else {
				printf("==== changes -L %s ====\n", urls[i]);
			}
			fflush(stdout);
		}
		if (rc = _doit_local(*nav, urls[i])) goto done;
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
		f = popenvp((*nav) + 1, "w");
		assert(f);
		EACH_HASH(seen) {
			if (*(int *)seen->vptr == all) fputs(seen->kptr, f);
		}
		status = pclose(f);
		unless (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) rc=1;
	}
done:
	FREE(cpath);
	p = popLine(*nav); /* remove '-' from above */
	free(p);
	*nav = addLine(*nav, 0);
	return (rc);
}


private int
doit(int dash)
{
	char	cmd[MAXKEY];
	char	*spec, *specf;
	pid_t	pid;
	sccs	*s = 0;
	ser_t	e, dstart, dstop;
	int	i, rc = 1;
	hash	*state;
	struct	rstate *rstate;
	kvpair	kv;
	int	flags;

	unless (opts.dspec) {
		if (opts.html) {
			spec = opts.verbose ?
			    "dspec-changes-hv" :
			    "dspec-changes-h";
		} else if (opts.json) {
			spec = opts.verbose ?
			    "dspec-changes-json-v" :
			    "dspec-changes-json";
		} else {
			if (!opts.diffs && getenv("BK_4X_DSPEC_COMPAT")) {
				spec = "dspec-changes-4.0";
			} else {
				spec = opts.diffs ?
				    "dspec-changes-vv" :
				    "dspec-changes";
			}
		}
		specf = bk_searchFile(spec);
		T_DEBUG("Reading dspec from %s", specf ? specf : "(not found)");
		unless (specf && (opts.dspec = loadfile(specf, 0))) {
			fprintf(stderr,
			    "changes: cant find %s/%s\n", bin, spec);
			exit(1);
		}
		free(specf);
	}
	spec = 0;
	dspec_collapse(&opts.dspec, &spec, &opts.end);
	if (spec) opts.begin = addLine(opts.begin, spec);
	s = s_cset;
	unless (s && HASGRAPH(s)) {
		system("bk help -s changes");
		exit(1);
	}
	if ((opts.filt || opts.sel) && !opts.sparseOk &&
	    proj_isProduct(s->proj)) {
		/* checkPresent() exits on failure */
		if (opts.filter) {
			checkPresent(s, opts.incF, opts.excF);
		} else {
			checkPresent(s, opts.incS, opts.excS);
		}
	}
	if (opts.rargs.rstart ||
	    (opts.rflags & (RANGE_LATTICE|RANGE_LONGEST))) {
		/* if a list, then don't skip empty as they listed it */
		if (!opts.rargs.rstop &&
	    	    !(opts.rflags & (RANGE_LATTICE|RANGE_LONGEST))) {
			opts.noempty = 0;
		}
		if (range_process("changes", s, opts.rflags, &opts.rargs)) {
			goto next;
		}
		for (e = s->rstop; e >= TREE(s); e--) {
			if ((FLAGS(s, e) & D_SET) && !want(s, e)) {
				FLAGS(s, e) &= ~D_SET;
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
			if (!s->rstart || (e < s->rstart)) {
				s->rstart = e;
			}
			if (!s->rstop || (e > s->rstop)) {
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
			if (want(s, e)) FLAGS(s, e) |= D_SET;
		}
	} else {
		s->rstop = TABLE(s);
		for (e = TABLE(s); e >= TREE(s); e--) {
			if (want(s, e)) FLAGS(s, e) |= D_SET;
		}
	}
	/*
	 * What we want is: this process | pager
	 */
	pid = mkpager();

	state = hash_new(HASH_MEMHASH);
	/* capture the comments, for the csets we care about */
	dstart = dstop = 0;
	for (e = s->rstop; e >= TREE(s); e--) {
		if (FLAGS(s, e) & D_SET) {
			unless (dstart) dstart = e;
			dstop = e;
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
	/*
	 * $begin/$end node. When the range is empty then dstart = dend = 0.
	 * So in this case any keywords in $begin/$end will get passed with d=0
	 * and so not expand.
	 * Otherwise d is set to the first (or last) match
	 */
	EACH(opts.begin) sccs_prsdelta(s, dstart, flags, opts.begin[i], stdout);
	cset(state, s, 0, 0, stdout, opts.dspec);
	if (opts.end) sccs_prsdelta(s, dstop, flags, opts.end, stdout);
	EACH_HASH(state) {
		rstate = state->vptr;
		EACH_KV(rstate->graphDB) {
			memcpy(&s, kv.val.dptr, sizeof (sccs *));
			if (s) sccs_free(s);
		}
		mdbm_close(rstate->graphDB);
		mdbm_close(rstate->idDB);
		mdbm_close(rstate->goneDB);
		EACH_HASH(rstate->csetDB) {
			cstate	*cs = rstate->csetDB->vptr;

			if (cs->keylist) freeLines(cs->keylist, free);
		}
		hash_free(rstate->csetDB);
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
		cmp = (d2->d - d1->d);
		if (opts.forwards) cmp *= -1;
		return (cmp);
	} else {
		/* comparing different sfiles */
		if (opts.timesort &&
		    (cmp = DATE(d2->s, d2->d) - DATE(d1->s, d1->d))) {
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
dumplog(char **list, sccs *sc, ser_t cset, char *dspec, int flags, FILE *f)
{
	slog	*ll;
	int	i;
	char	*comppath = 0;

	if (strrchr(PATHNAME(sc, cset), '/')) {
		comppath = strdup(PATHNAME(sc, cset));
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
 * Given a "top" delta "d", this function computes ChangeSet boundaries
 * It collect all the deltas inside a changeset and stuff them to "list".
 */
private char **
collectDelta(sccs *s, ser_t d, char **list)
{
	slog	*ll;
	char	*path = 0;	/* The most recent :DPN: for this file */

	/*
	 * Walk all deltas included in this cset and capture the
	 * changes output.
	 */
	range_cset(s, d);
	for (d = s->rstop; d >= TREE(s); d--) {
		if (FLAGS(s, d) & D_SET) {
			/* add delta to list */
			ll = new(slog);
			ll->s = s;
			ll->d = d;
			unless (path) path = PATHNAME(s, d);
			ll->path = path;
			list = addLine(list, ll);
			FLAGS(s, d) &= ~D_SET;	/* done using it */
		}
		if (d == s->rstart) break;
	}
	return (list);
}

/*
 * Load a db of rev key_list pair
 * key is rev
 * val is a list of entries in "root_key delta_key" format.
 */
private hash *
loadcset(sccs *cset)
{
	char	**keylist = 0;
	char	*keypath, *pipe;
	char	*t;
	u32	rkoff, dkoff;
	char	*rkey, *dkey;
	hash	*db;
	int	files = 0, inc = 0, exc = 0;
	ser_t	d = 0;
	ser_t	last;
	char	*pathp;
	char	path[MAXPATH];

	/* respect it if they set it on the command line */
	unless (opts.noMeta) opts.noMeta = !opts.all && !opts.incF;

	/* but it has no meaning unless we are -v */
	unless (opts.verbose) opts.noMeta = 0;

	/*
	 * Get a list of csets marked D_SET
	 */
	if (!opts.standalone && (t = proj_comppath(cset->proj))) {
		strcpy(path, t);
		pathp = path + strlen(path);
		*pathp++ = '/';
	} else {
		pathp = path;
	}
	db = hash_new(HASH_MEMHASH);
	sccs_rdweaveInit(cset);
	cset_firstPair(cset, cset->rstop);
	last = 0;
	while (d = cset_rdweavePair(cset, RWP_DSET, &rkoff, &dkoff)) {
		unless (dkoff) continue; /* last key */
		if (d < cset->rstart) break;
		rkey = HEAP(cset, rkoff);
		dkey = HEAP(cset, dkoff);
		if (d != last) {
			if (keylist) {
				cstate	cs = { keylist, files, inc, exc };

				unless (hash_insert(db, &last, sizeof(last),
					&cs, sizeof(cs))) {
					perror("db");
				}
				files = 0;
				inc = exc = 0;
				keylist = 0;
			} else if (opts.filt) {
				/*
				 * We are filtering, so unmark any
				 * csets where none of the files
				 * were selected.
				 *
				 * If cset changed nothing,
				 * keep it if not filtering by
				 * inc
				 */
				if ((opts.incF || opts.BAM) ||
				    ADDED(cset, last)) {
					FLAGS(cset, last) &= ~D_SET;
				}
			}
			last = d;
		}
		if (opts.filt || opts.sel || opts.noMeta) {
			if (opts.BAM && !weave_isBAM(cset, rkoff)) continue;
			unless (weave_iscomp(cset, rkoff)) {
				keypath = strchr(dkey, '|');
				assert(keypath);
				keypath++;
				pipe = strchr(keypath, '|');
				assert(pipe);
				strncpy(pathp, keypath, pipe - keypath);
				pathp[pipe-keypath] = 0;
				/*
				 * path == full path from product root
				 * pathp == path from comp root
				 * (same for standalone)
				 */
				if (skipPath(path, &inc, &exc)) continue;
				if (opts.noMeta && sccs_metafile(pathp)) {
					continue;
				}
				files = 1;
			}
		}
		keylist = addLine(keylist, aprintf("%s %s", rkey, dkey));
	}
	if (keylist) {
		cstate	cs = { keylist, files, inc, exc };

		unless (hash_insert(db, &last, sizeof(last),
			&cs, sizeof(cs))) {
			perror("db");
		}
	}
	sccs_rdweaveDone(cset);
	sccs_close(cset);
	return (db);
}

/*
 * Return true if a given pathname should be skipped?
 *
 * Also optionally set *incp && *excp if the path matches in
 * the include or exclude lists.
 *
 * The input can be a pathname or a deltakey.
 */
private int
skipPath(char *key, int *incp, int *excp)
{
	int	inc = 0, exc = 0;
	char	*path;

	path = isKey(key) ? key2path(key, 0, 0, 0) : key;
	if (opts.incF && match_globs(path, opts.incF, 0)) {
		inc = 1;
	}
	if (opts.incS && match_globs(path, opts.incS, 0)) {
		if (incp) *incp = 1;
	}
	if (opts.excF && match_globs(path, opts.excF, 0)) {
		exc = 1;
	}
	if (opts.excS && match_globs(path, opts.excS, 0)) {
		if (excp) *excp = 1;
	}
	if (key != path) free(path);
	return ((opts.incF && !inc) || exc);  // skip this path?
}

private int
printCset(int hasInc, int hasExc)
{
	/*
	 * See 'bk help changes' for an English explanation
	 * of this logic.
	 *
	 * Karnaugh map of solution:
	 *	A = opts.incS;		B = opts.excS;
	 *	C = has Included;	D = has Excluded;
	 *
	 * C can't happen if !A so those values are don't care, same for D & B
	 *
	 *	        AB
	 *	    00 01 11 10
	 *	 00  1  1  0  0
	 *  CD	 01  x  0  0  x
	 *	 11  x  x  0  x
	 *	 10  x  x  1  1
	 *
	 * http://courseware.ee.calpoly.edu/~rsandige/KarnaughExplorer.html
	 *
	 * Solution: F(ABCD) = C !D + !A !D
	 */
	return ((hasInc && !hasExc) || (!opts.incS && !hasExc));
}

/*
 * generate output for all csets in 'sc'
 *   if no dkey, marked with then D_SET
 *   if dkey, then all the component csets that are in same product
 *   cset as the component cset delta key: dkey
 * returns true if any output printed (may all be filtered)
 */
private int
cset(hash *state, sccs *sc, char *compKey, char *pkey, FILE *f, char *dspec)
{
	int	flags = PRS_FORCE; /* skip checks in sccs_prsdelta(), no D_SET*/
	int	iflags = INIT_NOCKSUM;
	int	n, cret, foundComps;
	int	hasComps, hasIncComps, hasExcComps;
	int	printProduct, printComponent;
	char	*dkey;
	int	i, j;
	cstate	*cs = 0;
	char	**keys = 0;
	ser_t	*csets = 0;
	char	**list;
	sccs	*s;
	ser_t	d, e;
	char	*rkey;
	char	**complist;
	int	rc = 0;
	FILE	*fcset = 0, *fsave = 0;
	char	*buf;
	size_t	len;
	ser_t	ser;
	struct	rstate	*rstate;
	char	key[MAXKEY];

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
		rstate->goneDB = loadDB(GONE, 0, DB_GONE);
		chdir(buf);
		rstate->graphDB = mdbm_mem();
		if (compKey) {
			/*
			 * Cache the component graph as though the
			 * product graph selected all (non optimal,
			 * but safe).  XXX: this is doing an 'and'
			 * with -u and -/re/ patterns: the product
			 * cset && the component cset need to pass
			 * the want() test.  Is anding useful?
			 * Set the range to be used in loadcset();
			 */
			sc->rstop = 0;
			sc->rstart = 0;
			for (e = TABLE(sc); e >= TREE(sc); e--) {
				if (want(sc, e)) {
					FLAGS(sc, e) |= D_SET;
					unless (sc->rstop) sc->rstop = e;
					sc->rstart = e;
				}
			}
		}
		/*
		 * loads all file key pairs for csets marked D_SET
		 * Has the side-effect of unD_SETing when testing BAM||inc||exc
		 *
		 * Careful: loadcset walks the cset data, so only do
		 * it if it is needed: verbose, components, and BAM|inc|exc.
		 *
		 * Note that noMeta doesn't count as a filt, but it's
		 * filtering from the print list doesn't seems to impact
		 * logic such as empty merge node detection.  If it did,
		 * we'd need to walk the weave by default because of noMeta
		 * and empty merge node removal are on by default.
		 */
		if (opts.doComp || opts.verbose || opts.filt || opts.sel) {
			rstate->csetDB = loadcset(sc);
			assert(rstate->csetDB);
		}
		if (compKey) {
			for (e = sc->rstop; e >= sc->rstart; e--) {
				FLAGS(sc, e) &= ~D_SET;
			}
		}
	}
	if (compKey) {
		d = sccs_findKey(sc, compKey);
		assert(d && pkey);
		poly_range(sc, d, pkey);	/* sets rstop and rstart */
	}

	/*
	 * Collect the cset in a list
	 * 'e' will have delta records from the product perspective
	 * of this cset file. 'd' will have delta records from the
	 * repo the cset file is in. If the repo is the product repo,
	 * these will be the same.
	 */
	for (e = sc->rstop; e >= TREE(s); e--) {
		if (FLAGS(sc, e) & D_SET) {
			FLAGS(sc, e) &= ~D_SET;
			if (!compKey || want(sc, e)) addArray(&csets, &e);
		}
		if (e == sc->rstart) break;
	}
	if (opts.forwards) reverseArray(csets);

	/*
	 * If --start_after, --end_before, or --around are passed, we
	 * filter the list of changes to match the requirements.
	 */
	if (!compKey && opts.pagekey) {
		ser_t	d;
		ser_t	*result = 0;
		int	n = nLines(csets);
		int	s, e;
		int	num = opts.num ? opts.num : n;

		unless (d = sccs_findKey(sc, opts.pagekey)) {
			fprintf(stderr,
			    "%s: Key %s not found\n", prog, opts.pagekey);
			exit(1);
		}
		for (i = 1; i < n; i++) if (csets[i] == d) break;

		/*
		 * If 'd' is not in the list of csets we take it as
		 * unmatched criteria, like searching for -/foo/ when
		 * foo is not in any cset.
		 */
		unless (i < n) goto out;

		s = i+1;
		if (opts.endBefore) {
			s -= num+1;
		} else if (opts.around) {
			s -= num/2;
		}
		e = s + num;
		if (s < 1) s = 1;
		if (e > n) e = n+1;
		growArray(&result, e-s);
		memcpy(&result[1], &csets[s], (e-s)*sizeof(ser_t));

		free(csets);
		csets = result;
	}

	/*
	 * Walk the ordered cset list and dump the file deltas contain in
	 * each cset. The file deltas are also sorted on the fly in dumplog().
	 */
	n = 0;
	EACH_INDEX(csets, j) {
		e = csets[j];

		if (ferror(f)) break;	/* abort when stdout is closed */
		if (opts.num && (n >= opts.num)) goto out;
		fcset = fmem();
		if (opts.keys) {
			sccs_pdelta(sc, e, fcset);
			fputc('\n', fcset);
		} else {
			sccs_prsdelta(sc, e, flags, dspec, fcset);
		}
		unless (opts.doComp || opts.verbose || opts.filt || opts.sel) {
			buf = fmem_peek(fcset, &len);
			fwrite(buf, 1, len, f);
			fclose(fcset);
			unless (compKey) n++;
			continue;
		}

		/* get key list */
		ser = e;
		if (cs = hash_fetch(rstate->csetDB, &ser, sizeof(ser))) {
			keys = cs->keylist;
		} else {
			keys = 0;
		}

		list = 0;
		complist = 0;
		hasComps = hasIncComps = hasExcComps = 0;
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
			if (componentKey(dkey)) {
				unless (skipPath(dkey,
					&hasIncComps, &hasExcComps)) {
					hasComps = 1;
				}
			}
			s = sccs_keyinitAndCache(sc->proj, rkey, iflags,
			    rstate->graphDB, rstate->idDB);
			unless (s) {
				unless (gone(rkey, rstate->goneDB)) {
					fprintf(stderr,
					     "Cannot sccs_init(), key = %s\n",
					     rkey);
				}
				continue;
			}
			if (opts.doComp) s->prs_indentC = 1;
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
		printProduct = (!compKey &&
		    ((hasComps &&
			printCset(hasIncComps, hasExcComps)) ||
			(cs && cs->hasFiles &&
			    printCset(cs->hasIncFiles, cs->hasExcFiles))));
		printComponent = (compKey &&
		    ((opts.verbose && !skipPath(compKey, 0, 0)) ||
			((opts.printComp || opts.verbose)  &&
			    (cs && cs->hasFiles &&
				printCset(cs->hasIncFiles, cs->hasExcFiles)))));
		if ((!opts.filt && !opts.sel) ||
		    (compKey && opts.printComp) ||
		    printProduct || printComponent) {
			/* write cset data saved above */
			buf = fmem_peek(fcset, &len);
			fwrite(buf, 1, len, f);
			unless (compKey) n++;
			fclose(fcset);
			fcset = 0;
		} else {
			/*
			 * BAM doesn't print the ChangeSet
			 * information, just the files.
			 *
			 * And if we are processing a component
			 * and decided above to NOT print the
			 * ChangeSet, then toss it.
			 */
			fsave = f;
			if (opts.BAM || compKey) {
				f = fmem();
			} else {
				f = fcset;
			}
		}
		if (cs && cs->hasFiles &&
		    printCset(cs->hasIncFiles, cs->hasExcFiles)) {
			rc = 1; /* Remember we printed output */
		}
		/* sort file deltas, print it, then free it */
		dumplog(list, sc, e, dspec, flags, f);

		/*
		 * Foreach component delta found mark them with D_SET
		 * and recursively call cset() with the new cset
		 */
		if (complist) sccs_sdelta(sc, e, key);
		foundComps = 0;
		EACH(complist) {
			rkey = keys[p2int(complist[i])];
			s = sccs_keyinitAndCache(sc->proj, rkey, iflags,
			    rstate->graphDB, rstate->idDB);
			if (s && opts.doComp) s->prs_indentC = 1;

			dkey = rkey + strlen(rkey) + 1;
			assert(dkey);

			/* call cset() recursively */
			if (opts.excF && match_globs(key2path(dkey, 0, 0, 0),
				opts.excF, 0)) continue;
			cret = cset(state, s, dkey, key, f, dspec);
			foundComps |= cret;
			if (cret && fsave) {
				/*
				 * we generated output so flush the saved data
				 */
				n++;
				buf = fmem_peek(f, &len);
				f = fsave;
				fsave = 0;
				fwrite(buf, 1, len, f);
			}
		}
		freeLines(complist, 0);
		/* reduce mem footprint, could be huge */
		if (cs) {
			if (cs->keylist) freeLines(cs->keylist, free);
			hash_delete(rstate->csetDB, &ser, sizeof(ser));
		}
		if (fsave) {
			if (compKey && rc) {
				buf = fmem_peek(f, &len);
				fwrite(buf, 1, len, fsave);
			}
			f = fsave;
			fsave = 0;
		}
		if (fcset) fclose(fcset);
	}

	/*
	 * All done, clean up
	 * The above loop may break out prematurely if pager exit
	 * We need to account for it.
	 */
out:	free(csets);
	return (rc);
}

private int
want(sccs *s, ser_t e)
{
	char	*p, *t, old;
	int	i, match;
	symbol	*sym;

	unless (opts.all || !TAG(s, e)) return (0);
	if (opts.tagOnly) {
		match = 0;
		sym = 0;
		while (sym =
		    sccs_walkTags(sym, s, e, 0, opts.all)) {
			if (opts.tsearch &&
			    !search_either(SYMNAME(s, sym), opts.search)) {
				continue;
			}
			match = 1;
			break;
		}
		unless (match) return (0);
	}
	if (opts.notusers) {
		char	*u = USER(s, e);

		if (p = strchr(u, '/')) *p = 0;
		match = 0;
		EACH(opts.notusers) {
			match |= streq(opts.notusers[i], u);
		}
		if (p) *p = '/';
		if (match) return (0);
	}
	if (opts.users) {
		char	*u = USER(s, e);

		if (p = strchr(u, '/')) *p = 0;
		match = 0;
		EACH(opts.users) match |= streq(opts.users[i], u);
		if (p) *p = '/';
		unless (match) return (0);
	}
	if (opts.nomerge && (MERGE(s, e) || MTAG(s, e))) return (0);
	if (opts.noempty && MERGE(s, e) && !ADDED(s, e) && !(FLAGS(s, e) & D_SYMBOLS)) {
	    	return (0);
	}
	if (opts.doSearch) {
		t = COMMENTS(s, e);
		while (p = eachline(&t, &i)) {
			old = p[i];
			p[i] = 0;
			if (search_either(p, opts.search)) {
				p[i] = old;
				return (1);
			}
			p[i] = old;
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

	cmdf = bktmp(0);
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
		/* make -S easy to find in bkd_changes.c (MUST be first arg) */
		if (opts.standalone) fprintf(f, " -S");

		/* Use the -L/-R cleaned options; skip over "bk" "changes" */
		EACH_START(3, av, i) fprintf(f, " %s", av[i]);
	}
	fputs("\n", f);
	fclose(f);

	if (opts.remote) {
		probef = bktmp(0);
		if (f = fopen(probef, "wb")) {
			rc = probekey(s_cset, 0, SK_SYNCROOT, f);
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
		unless (rc) {
			f = fopen(probef, "rb");
			while ((i = fread(buf, 1, sizeof(buf), f)) > 0) {
				writen(r->wfd, buf, i);
			}
			fclose(f);
			rc = send_file_extra_done(r);
		}
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

	bktmp(msgfile);
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
	send_file_extra_done(r);
	return (rc);
}

private int
send_part2_msg(remote *r, char **av, char *key_list)
{
	int	rc, i;
	char	msgfile[MAXPATH], buf[MAXLINE];
	FILE	*f;

	bktmp(msgfile);
	f = fopen(msgfile, "w");
	assert(f);
	sendEnv(f, 0, r, ((opts.remote || opts.local) ? 0 : SENDENV_NOREPO));

	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "chg_part2");
	/* Use the -L/-R cleaned options; skip over "bk" "changes" */
	EACH_START(3, av, i) fprintf(f, " %s", av[i]);
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
	fclose(f);
	writen(r->wfd, "@END@\n", 6);
	send_file_extra_done(r);
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

	if (bkd_connect(r, 0)) return (-1);
	if (send_part1_msg(r, av)) return (-1);
	if (r->rfd < 0) return (-1);

	unless (r->rf) r->rf = fdopen(r->rfd, "r");
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if ((rc = remote_lock_fail(buf, 1))) {
		return (rc); /* -2 means locked */
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r, 0)) return (-1);
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
	bktmp(key_list);
	fd = open(key_list, O_CREAT|O_WRONLY, 0644);
	flags = SK_REVPREFIX|SK_RKEY|SK_SYNCROOT;
	rc = prunekey(s_cset, r, seen, fd, flags, NULL, &rcsets, &rtags);
	if (rc < 0) {
		switch (rc) {
		    case -2:
			getMsg("unrelated_repos",
			    "synchronize with", '=', stderr);
			break;
		    case -3:
			getMsg("no_repo", 0, '=', stderr);
			break;
		}
		close(fd);
		disconnect(r);
		return (-1);
	}
	close(fd);
	if (r->type == ADDR_HTTP) disconnect(r);
	return (rcsets + rtags);
}

private int
changes_part2(remote *r, char **av, char *key_list, int ret)
{
	int	rc = 0;
	int	rc_lock;
	char	buf[MAXLINE];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, 0)) {
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
		if (getServerInfo(r, 0)) {
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
	wait_eof(r, 0);
	disconnect(r);
	return (rc);
}

private int
_doit_remote(char **nav, char *url)
{
	char 	key_list[MAXPATH] = "";
	char	*tmp;
	int	rc, i;
	u32	flags = REMOTE_BKDURL;
	remote	*r;

	unless (opts.chgurl) flags |= REMOTE_ROOTKEY;
	r = remote_parse(url, flags);
	unless (r) {
		fprintf(stderr, "invalid url: %s\n", url);
		return (1);
	}

	/* Quote args for the other side */
	EACH(nav) {
		tmp = shellquote(nav[i]);
		free(nav[i]);
		nav[i] = tmp;
	}
	rc = changes_part1(r, nav, key_list);
	if (rc >= 0 && opts.remote) {
		rc = changes_part2(r, nav, key_list, rc);
	}
	disconnect(r);
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
		rc = _doit_remote(nav, url);
		if (rc != -2) break; /* -2 means locked */
		if (getenv("BK_REGRESSION")) break;
		fprintf(stderr,
		    "changes: remote locked, trying again...\n");
		sleep(i * 2);
	}
	if (rc == -2) fprintf(stderr, "changes: giving up on remote lock.\n");
	return (rc ? 1 : 0);
}
