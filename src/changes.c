#include "bkd.h"
#include "range.h"

private struct {
	search	search;		/* -/pattern/[i] matches comments w/ pattern */
	u32	doSearch:1;	/* search the comments list */
	u32	forwards:1;	/* like prs -f */
	char	*date;		/* list this range of dates */
	char	*dspec;		/* override dspec */
	u32	all:1;		/* list all, including tags etc */
	u32	noempty:1;	/* do not list empty merge deltas */
	u32	html:1;		/* do html style output */
	u32	keys:1;		/* just list the keys */
	u32	local:1;	/* want the new local csets */
	u32	nomerge:1;	/* do not list _any_ merge deltas */
	u32	newline:1;	/* add a newline after each record, like prs */
	char	*rev;		/* list this rev or range of revs */
	u32	remote:1;	/* want the new remote csets */
	u32	remote2:1;	/* bk changes url */
	u32	tagOnly:1;	/* only show items which are tagged */
	char	*user;		/* only from this user */
	u32	others:1;	/* -U<user> everyone except <user> */
	u32	verbose:1;	/* list the file checkin comments */

	/* not opts */
	FILE	*f;		/* global for recursion */
	sccs	*s;		/* global for recursion */
	char	*spec;		/* global for recursion */
} opts;

typedef struct slog {
	time_t	date;
	time_t	dateFudge;
	char	*log;
	delta	*delta;
	struct	slog *next;
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
private int	doit_local(int nac, char **nav, char *url);
private void	cset(sccs *s, FILE *f, char *dspec);

private	MDBM	*seen; /* key list */

int
changes_main(int ac, char **av)
{
	int	c;
	int	rc = 0, nac = 0;
	char	*nav[30];
	char	*url = NULL, *freeme = NULL;
	char	buf[MAXPATH];
	pid_t	pid = 0; /* pager */

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help changes");
		return (1);
	}

	bzero(&opts, sizeof(opts));
	opts.noempty = 1;
	nav[nac++] = "bk";
	nav[nac++] = "changes";
	while ((c = getopt(ac, av, "ac;d;efhkLmnRr|tu;U;v/;")) != -1) {
		unless (c == 'L' || c == 'R') {
			if (optarg) {
				nav[nac++] = aprintf("-%c%s", c, optarg);
			} else {
				nav[nac++] = aprintf("-%c", c);
			}
		}
		switch (c) {
		    /*
		     * Note: do not add option 'K', it is reserved
		     * for internal use
		     */
		    case 'a': opts.all = 1; opts.noempty = 0; break;
		    case 'c': opts.date = optarg; break;
		    case 'd': opts.dspec = optarg; break;
		    case 'e': opts.noempty = !opts.noempty; break;
		    case 'f': opts.forwards = 1; break;
		    case 'h': opts.html = 1; break;
		    case 'k': opts.keys = opts.all = 1; opts.noempty = 0; break;
		    case 'm': opts.nomerge = 1; break;
		    case 'n': opts.newline = 1; break;
		    case 't': opts.tagOnly = 1; break;		/* doc 2.0 */
		    case 'U': opts.others = 1;
		    	/* fall through to u */
		    case 'u': opts.user = optarg; break;
		    case 'v': opts.verbose = 1; break;		/* doc 2.0 */
		    case 'r': opts.rev = optarg; break;		/* doc 2.0 */
		    case '/': opts.search = searchParse(optarg);
			      opts.doSearch = 1;
			      break;
		    case 'L': opts.local = 1; break;
		    case 'R': opts.remote = 1; break;
		    default:
usage:			system("bk help -s changes");
			exit(1);
		}
		optarg = 0;
	}
	nav[nac] = 0;
	if ((opts.local || opts.remote) && opts.rev) goto usage;
	if (opts.keys && (opts.verbose||opts.html||opts.dspec)) goto usage;

	if (!opts.local && !opts.remote &&
		av[optind] && !streq("-", av[optind])) {
		opts.remote2 = 1;	/* bk changes url */
	}

	if (sccs_cd2root(0, 0)) {
		if (!opts.remote2) {
			fprintf(stderr, "Can't find package root\n");
			exit(1);
		}
		/* otherwise we don't really care */
	}

	/*
	 * There are 4 major cases
	 * 1) bk changes -L url
	 * 2) bk changes -R (url_list | -)
	 * 3) bk changes url
	 * 4) bk changes [-]
	 * Note: the dash in case "2" is a url list,
	 *       the dash in case "4" is a key list.
	 */
	if (opts.local) {
		/* bk changes -L url */
		unless (url = av[optind]) {
			unless (url = freeme = getParent()) {
				fprintf(stderr, "No repository specified?!\n");
out:				for (c = 2; c < nac; c++) free(nav[c]);
				if (freeme) free(freeme);
				if (seen) mdbm_close(seen);
				return (1);
			}
		}
		rc = doit_local(nac, nav, url);
	} else if (opts.remote) {
		pid = mkpager();
		seen = mdbm_mem();
		if (av[optind] && streq("-", av[optind])) {
			/*
			 * bk changes -R -
			 * get url list from stdin
			 */
			while (fnext(buf, stdin)) {
				chomp(buf);
				rc |= doit_remote(nav, buf);
			}
		} else if (av[optind] == NULL) {
			/* bk changes -R */
			unless (url = freeme = getParent()) {
				fprintf(stderr, "No repository specified?!\n");
				if (pid > 0)  {
					fclose(stdout);
					waitpid(pid, 0, 0);
				}
				goto out;
			}
			rc = doit_remote(nav, url);
		} else {
			/* bk changes -R url_list */
			while (av[optind]) {
				rc |= doit_remote(nav, av[optind++]);
			}
		}
		mdbm_close(seen);
		if (pid > 0)  {
			fclose(stdout);
			waitpid(pid, 0, 0);
		}
	} else if (opts.remote2) {
		/* bk changes url */
		rc = doit_remote(nav, av[optind]);
	} else {
		if (!av[optind]) {
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
	for (c = 2; c < nac; c++) free(nav[c]);
	if (freeme) free(freeme);
	return (rc);
}

private int
doit_local(int nac, char **nav, char *url)
{
	int	wfd, fd1, status;
	pid_t	pid;

	if (opts.remote) {
		fprintf(stderr, "warning: -R option ignored\n");
	}

	/*
	 * What we want is: bk synckey -lk url | bk changes opts -
	 */
	nav[nac++] = strdup("-");
	assert(nac < 30);
	nav[nac] = 0;
	pid = spawnvp_wPipe(nav, &wfd, 0);

	/*
	 * Send "bk synckey" stdout into the pipe
	 */
	fd1 = dup(1); close(1);
	dup2(wfd, 1); close(wfd);
	sys("bk", "synckeys", "-lk", url, SYS);
	close(1); dup2(fd1, 1); /* restore fd1, just in case */
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) return (WEXITSTATUS(status));
	return (1); /* interrupted */
}

private int
pdelta(delta *e)
{
	if (feof(opts.f)) return (1); /* for early pager exit */
	unless (e->flags & D_SET) return(0);
	if (opts.keys) {
		sccs_pdelta(opts.s, e, opts.f);
		fputc('\n', opts.f);
	} else {
		if (opts.all || (e->type == 'D')) {
			int	flags = opts.all ? PRS_ALL : 0;

			sccs_prsdelta(opts.s, e, flags, opts.spec, opts.f);
			if (opts.newline) fputc('\n', opts.f);
		}
	}
	fflush(opts.f);
	return (0);
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
#define	DSPEC	":DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\n$each(:C:){  (:C:)\n}$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}\n"
#define	HSPEC	"<tr bgcolor=lightblue><td font size=4>" \
		"&nbsp;:Dy:-:Dm:-:Dd: :Th:::Tm:&nbsp;&nbsp;" \
		":P:@:HT:&nbsp;&nbsp;:I:</td></tr>\n" \
		"$if(:TAG:){<tr bgcolor=yellow><td>&nbsp;Tag: :TAG:" \
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
		"$if(:TAG:){<tr bgcolor=yellow><td>&nbsp;Tag: :TAG:" \
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
	char	s_cset[] = CHANGESET;
	char	*spec;
	pid_t	pid;
	sccs	*s;
	delta	*e;
	int	noisy = 0;
	RANGE_DECL;

	if (opts.dspec && !opts.html) {
		spec = opts.dspec;
	} else if (opts.html) {
		spec = opts.verbose ? HSPECV : HSPEC;
	} else {
		spec = DSPEC;
	}
	s = sccs_init(s_cset, SILENT|INIT_NOCKSUM|INIT_SAVEPROJ, bk_proj);
	assert(s && HASGRAPH(s));
	if (opts.rev || opts.date) {
		if (opts.rev) {
			r[0] = notnull(opts.rev);
			things += tokens(r[0]);
			if (things == 1) opts.noempty = 0;
		} else {
			d[0] = notnull(opts.date);
			things += tokens(d[0]);
		}
		rd = 1;
		RANGE("changes", s, opts.all ? 2 : 1, noisy)
		unless (SET(s)) {
			for (e = s->rstop; e; e = e->next) {
				if (want(s, e)) e->flags |= D_SET;
				if (e == s->rstart) break;
			}
		} else {
			s->state &= ~S_SET;
		}
	} else if (dash) {
		while (fgets(cmd, sizeof(cmd), stdin)) {
			/* ignore blank lines and comments */
			if ((*cmd == '#') || (*cmd == '\n')) continue;
			chomp(cmd);
			e = sccs_getrev(s, cmd, 0, 0);
			unless (e) {
				fprintf(stderr,
				    "changes: can't find key: %s\n", cmd);
				sccs_free(s);
				return (1);
			}
			while (!opts.all && (e->type == 'R')) {
				e = e->parent;
				assert(e);
			}
			if (want(s, e)) e->flags |= D_SET;
		}
	} else {
		for (e = s->table; e; e = e->next) {
			if (want(s, e)) e->flags |= D_SET;
		}
	}
	//assert(!SET(s));

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
	s->xflags |= X_YEAR4;
	if (opts.verbose) {
		cset(s, stderr, spec);
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
	sccs_free(s);
	if (pid > 0) {
		fclose(stdout);
		waitpid(pid, 0, 0);
	}
	return (0);

next:	return (1);
}

/*
 * Note that these two are identical except for the d1/d2 assignment.
 */
private int
compar(const void *a, const void *b)
{
        register        slog *d1, *d2;

        d1 = *((slog**)a);
        d2 = *((slog**)b);
	if (d1->date == d2->date) {
        	return (d2->dateFudge - d1->dateFudge);
	}
        return (d2->date - d1->date);
}

private int
forwards(const void *a, const void *b)
{
        register        slog *d1, *d2;

        d1 = *((slog**)b);
        d2 = *((slog**)a);
	if (d1->date == d2->date) {
        	return (d2->dateFudge - d1->dateFudge);
	}
        return (d2->date - d1->date);
}

private slog *
dumplog(slog *list, int *n)
{
	slog	*ll;
	slog 	**sorted;	
	int	i = *n;
	int	indent = 2;
	char 	*p, *q;

	if (i == 0) return (NULL);
	assert(i > 0);

	/*
	 * Stuff the list in a array so we can sort it
	 */
	sorted = malloc(i * sizeof(sorted));
	for (ll = list; ll; ll = ll->next) sorted[--i] = ll;
	assert(i == 0);
	qsort(sorted, *n, sizeof(sorted), opts.forwards ? forwards : compar);

	/*
	 * Print the sorted list
	 */
	for (i = 0; i < *n; ++i) {
		ll = sorted[i];
		p = ll->log;

		/* indent each non-empty line */
		while (p) {
			q = strchr(p, '\n');
			/* do not indent empty line */
			if (indent && (q > p)) printf("%*s", indent, "");
			if (q) {
				*q++ = '\0';
				printf("%s\n", p);
			} else {
				printf("%s", p);
			}
			p = q;
		}
		if (opts.newline) fputc('\n', stdout);

		free(ll->log);
		free(ll);
	}

	/*
	 * Reset the list
	 */
	free(sorted);
	*n = 0;
	return (NULL);
}

/*
 * Cache the sccs struct to avoid re-initing the same sfile
 */
private sccs *
sccs_keyinitAndCache(char *key, int flags,
    project *proj, MDBM *idDB, MDBM *graphDB, MDBM *goneDB)
{
	static	int	rebuilt = 0;
	datum	k, v;
	sccs	*s;

	k.dptr = key;
	k.dsize = strlen(key);
	v = mdbm_fetch(graphDB, k);
	if (v.dptr) { /* cache hit */
		memcpy(&s, v.dptr, sizeof (sccs *));
		return (s);
	}
 retry:
	s = sccs_keyinit(key, flags, proj, idDB);
	unless (s || gone(key, goneDB)) {
		unless (rebuilt) {
			mdbm_close(idDB);
			if (sccs_reCache(1)) {
				fprintf(stderr,
				    "changes: cannot build %s\n",
				    IDCACHE);
			}
			unless (idDB =
			    loadDB(IDCACHE,
				0, DB_KEYFORMAT|DB_NODUPS)) {
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
	if (s) sccs_close(s); /* we don't need the delta body */
	return (s);
}

/*
 * Given a "top" delta "d", this function computes ChangeSet boundaries
 */
private slog *
collectDelta(sccs *s, delta *d, slog *list, char *dspec, int *n, char *dbuf)
{
	slog	*ll;
	delta	*e;

	/*
	 * Walk d->parent and d->merge recursively to find cset boundaries
	 * and collect the deltas/dspec-output along the way
	 */
	do {
		d->flags |= D_SET;
		dbuf[0] = '\0';
		sccs_prsbuf(s, d, PRS_ALL, dspec, dbuf);
		/* add delta to list */
		ll = calloc(sizeof (slog), 1);
		ll->log = strdup(dbuf);
		ll->date = NOFUDGE(d);
		ll->dateFudge = d->dateFudge;
		ll->next = list;
		list = ll;
		(*n)++;
		d->flags &= ~D_SET;

		if (d->merge) {
			e = sfind(s, d->merge);
			assert(e);
			unless (e->flags & D_CSET) {
				list = collectDelta(s, e, list, dspec, n, dbuf);
			}
		}
		
		d =  d->parent;
	} while (d && !(d->flags & D_CSET));
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
	char	tmp[MAXPATH], buf[2 * MAXKEY + 100];
	char	*rev = NULL;
	char	**keylist = NULL;
	char	*p;
	FILE	*f;
	MDBM	*db;

	bktmp(tmp, "loadcset");
	sccs_cat(cset, SILENT|GET_NOHASH|GET_REVNUMS|PRINT, tmp);
	f = fopen(tmp, "rt");

	db = mdbm_mem();
	while (fnext(buf, f)) {
		
		chomp(buf);
		p = strchr(buf, '\t');
		assert(p);
		*p++ = 0;
		if (!rev) {
			rev = strdup(buf);
			assert(keylist == NULL);
		} else if (rev && !streq(rev, buf)) {
			saveKey(db, rev, keylist);
			free(rev);
			rev = strdup(buf);
			keylist = NULL;
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
cset(sccs *cset, FILE *f, char *dspec)
{
	int	flags = PRS_ALL, iflags = INIT_NOCKSUM|INIT_SAVEPROJ;
	int 	i, j, m = 0, n = 0;
	char	*dbuf;
	char	**keys;
	slog	*list = 0, *ll, *ee;
	slog 	**sorted;	
	delta	*e;
	kvpair	kv;
	datum	k, v;
	MDBM 	*idDB, *goneDB, *graphDB, *csetDB;
	project	*proj = bk_proj;

	assert(dspec);
	if (opts.newline) flags |= PRS_LF; /* for sccs_prsdelta() */

	/*
	 * Init idDB, goneDB, graphDB and csetDB
	 */
	unless (idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
		perror("idcache");
		exit(1);
	}
	goneDB = NULL;
	if (exists(SGONE)) {
		char tmp_gone[MAXPATH];

		bktmp(tmp_gone, "change_gone");
		sysio(0, tmp_gone, 0, "bk", "get", "-kpsC", GONE, SYS);
		goneDB = loadDB(tmp_gone, 0, DB_KEYSONLY|DB_NODUPS);
		unlink(tmp_gone);
	}
	graphDB = mdbm_mem();
	csetDB = loadcset(cset);
	sccs_close(cset);

	/*
	 * Collect the cset in a list
	 */
	for (e = cset->table; e; e = e->next) {
		unless (e->flags & D_SET) continue;
		ll = calloc(sizeof (slog), 1);
		ll->date = NOFUDGE(e);
		ll->dateFudge = e->dateFudge;
		ll->delta = e;
		ll->next = list;
		list = ll;
		m++;
	}

	/*
	 * Stuff the cset list in a array so we can sort it
	 */
	if (m == 0) return; /* no work needed */
	sorted = malloc(m * sizeof(sorted));
	for (i = m, ll = list; ll; ll = ll->next) sorted[--i] = ll;
	assert(i == 0);
	qsort(sorted, m, sizeof(sorted), opts.forwards ? forwards : compar);

	dbuf = malloc(4096); /* XXX This should match VSIZE in slib.c */
	assert(dbuf);	     /* life will be better when we go to the */
			     /* 3.0 tree where prs vbuf is 	      */
			     /* dynamically sized		      */


	/*
	 * Walk the sorted cset list and dump the file deltas contain in
	 * each cset. The file deltas are also sorted on the fly in dumplog().
	 */
	list = NULL;
	for (j = 0; j < m; ++j) {
		ee = sorted[j];
		sorted[j] = NULL;

		/* print cset dspec */
		e = ee->delta;
		sccs_prsdelta(cset, e, flags, dspec, stdout);

		/* get key list */
		k.dptr = e->rev;
		k.dsize = strlen(e->rev);
		v = mdbm_fetch(csetDB, k);
		unless (v.dptr) continue; /* merge cset are usually empty */
		memcpy(&keys, v.dptr, v.dsize);
		mdbm_delete(csetDB, k);

		EACH (keys) {
			sccs	*s;
			delta	*d;
			char	*dkey;

			dkey = separator(keys[i]);
			assert(dkey);
			*dkey++ = 0;
			s = sccs_keyinitAndCache(
				keys[i], iflags, proj, idDB, graphDB, goneDB);
			unless (s) continue;
			d = sccs_findKey(s, dkey);
			assert(d);

			/*
			 * CollectDelta() compute cset boundaries,
			 * when this function returns, "list" will contain
			 * all member deltas/dspec in "s" for this cset
			 */
			list = 	collectDelta(s, d, list, dspec, &n, dbuf);
		}
		freeLines(keys, free); /* reduce mem foot print, could be huge */
		free(ee);
		list = dumplog(list, &n); /* sort file dspec and print it */
		if (fflush(stdout)) break;
	}

	/*
	 * All done, clean up
	 * The above loop may break out prematurely if pager exit
	 * We need to account for it.
	 */
	for (i = 0; i < m; ++i) {
		ee = sorted[i];
		if (ee) free(ee);
	}
	free(sorted);

	EACH_KV(graphDB) {
		sccs	*s;

		memcpy(&s, kv.val.dptr, sizeof (sccs *));
		if (s) sccs_free(s);
	}

	EACH_KV(csetDB) {
		memcpy(&keys, kv.val.dptr, sizeof (char **));
		freeLines(keys, free);
	}

	mdbm_close(graphDB);
	mdbm_close(idDB);
	mdbm_close(goneDB);
	mdbm_close(csetDB);
	free(dbuf);
	return;
}

private int
want(sccs *s, delta *e)
{
	unless (opts.all || (e->type == 'D')) return (0);
	if (opts.tagOnly && !(e->flags & D_SYMBOLS)) return (0);
	if (opts.others) {
		if (streq(opts.user, e->user)) return (0);
	} else if (opts.user && !streq(opts.user, e->user)) {
		return (0);
	}
	if (opts.nomerge && e->merge) return (0);
	if (opts.noempty && e->merge && !e->added && !(e->flags & D_SYMBOLS)) {
	    	return (0);
	}
	if (opts.doSearch) {
		int	i;

		EACH(e->comments) {
			if (searchMatch(e->comments[i], opts.search)) {
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
	char	*cmd, buf[MAXPATH];
	int	rc, i;
	FILE 	*f;

	bktmp(buf, "changes");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, NULL, r, !opts.remote);
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
		cmd = aprintf("bk _probekey  >> %s", buf);
		system(cmd);
		free(cmd);
	}

	rc = send_file(r, buf, 0, 0);
	unlink(buf);
	return (rc);
}

private int
send_end_msg(remote *r, char *msg)
{
	char	msgfile[MAXPATH];
	FILE	*f;
	int	rc;

	bktmp(msgfile, "changes_end");
	f = fopen(msgfile, "wb");
	assert(f);
	sendEnv(f, NULL, r, !opts.remote);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in part 1
	 */
	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
	fprintf(f, "chg_part2");
	fputs("\n", f);

	fputs(msg, f);
	fclose(f);

	rc = send_file(r, msgfile, 0, 0);
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
	f = fopen(msgfile, "wb");
	assert(f);
	sendEnv(f, NULL, r, !opts.remote);

	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
	fprintf(f, "chg_part2");
	/* Use the -L/-R cleaned options */
	for (i = 1; av[i]; i++) fprintf(f, " %s", av[i]);
	fputs("\n", f);
	fclose(f);

	rc = send_file(r, msgfile, size(key_list) + 17, 0);
	unlink(msgfile);
	f = fopen(key_list, "rt");
	assert(f);
	write_blk(r, "@KEY LIST@\n", 11);
	while (fnext(buf, f)) {
		write_blk(r, buf, strlen(buf));
		chomp(buf);
		/* mark the seen key, so we can skip it on next repo */
		mdbm_store_str(seen, buf, "", MDBM_INSERT);
	}
	write_blk(r, "@END@\n", 6);
	fclose(f);
	return (rc);
}

/*
 * TODO: this could be merged with synckeys() in synckeys.c
 */
private int
changes_part1(remote *r, char **av, char *key_list)
{
	char	buf[MAXPATH], s_cset[] = CHANGESET;
	int	flags, fd, rc, rcsets = 0, rtags = 0;
	sccs	*s;

	if (bkd_connect(r, 0, 1)) return (-1);
	send_part1_msg(r, av);
	if (r->rfd < 0) return (-1);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if ((rc = remote_lock_fail(buf, 1))) {
		return (rc); /* -2 means locked */
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
		getline2(r, buf, sizeof(buf));
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		return (-1);
	}
	if (get_ok(r, buf, 1)) return (-1);

	if (opts.remote == 0) {
		pid_t	pid;

		getline2(r, buf, sizeof(buf));
		unless (streq("@CHANGES INFO@", buf)) {
			return (0); /* protocal error */
		}
		pid = mkpager();
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (streq("@END@", buf)) break;

			/*
			 * Check for write error, in case
			 * our pager terminate early
			 */
			if (write(1, &buf[1], strlen(buf) - 1) < 0) break;
			if (write(1, "\n", 1) < 0) break;
		}
		fclose(stdout);
		if (pid > 0) waitpid(pid, 0, 0);
		return (0);
	}

	/*
	 * What we want is: "remote => bk _prunekey => keylist"
	 */
	bktmp(key_list, "keylist");
	fd = open(key_list, O_CREAT|O_WRONLY, 0644);
	s = sccs_init(s_cset, 0, 0);
	flags = PK_REVPREFIX|PK_RKEY;
	rc = prunekey(s, r, seen, fd, flags, 0, NULL, &rcsets, &rtags);
	if (rc < 0) {
		switch (rc) {
		    case -2:
			printf(
"You are trying to sync to an unrelated package. The root keys for the\n\
ChangeSet file do not match.  Please check the pathnames and try again.\n");
			close(fd);
			sccs_free(s);
			return (1); /* needed to force bkd unlock */
		    case -3:
			printf("You are syncing to an empty directory\n");
			sccs_free(s);
			return (1); /* empty dir */
			break;
		}
		close(fd);
		disconnect(r, 2);
		sccs_free(s);
		return (-1);
	}
	close(fd);
	sccs_free(s);
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
		getServerInfoBlock(r);
	}

	getline2(r, buf, sizeof(buf));
	unless (streq("@CHANGES INFO@", buf)) {
		rc = -1; /* protocal error */
		goto done;
	}
	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (streq("@END@", buf)) break;
		if (write(1, &buf[1], strlen(buf) - 1) < 0) {
			break;
		}
		write(1, "\n", 1);
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

	loadNetLib();
	if (opts.remote) has_proj("changes");
	r = remote_parse(url, 1);
	unless (r) {
		fprintf(stderr, "invalid url: %s\n", url);
		return (1);
	}

	if (opts.remote && opts.rev) {
		fprintf(stderr, "warning: -r option ignored\n");
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
	remote_free(r);
	if (key_list[0]) unlink(key_list);
	return (rc);
}

private int
doit_remote(char **nav, char *url)
{
	int	rc;
	int	i = 0;

	for (;;) {
		rc = _doit_remote(&nav[1], url);
		if (rc != -2) break; /* -2 means locked */
		fprintf(stderr,
		    "changes: remote locked, trying again...\n");
		sleep(min((i++ * 2), 10)); /* auto back off */
	}
	return (rc);
}
