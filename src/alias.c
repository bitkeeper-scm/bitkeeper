#include "sccs.h"
#include "nested.h"

/*
 * in case I want to try storing globs in the aliasdb: uncomment
 */
// #define CRAZY_GLOB	1

/* union of aliasCreate and aliasShow options */

typedef struct {
	char	**urls;		/* -@url: options for populate */
	char	*rev;
	hash	*seen;
	u32	quiet:1;	/* -q: quiet */
	u32	nocommit:1;	/* -C: don't commit in product after change */
	u32	showkeys:1;	/* -k: show keys instead of pathnames */
	u32	here:1;		/* -h: only fully present aliases */
	u32	missing:1;	/* -m: only aliases not present */
	u32	expand:1;	/* -e: expand list of aliases */
	u32	force:1;	/* -f: force remove components */
	u32	verbose:8;	/* -v: be verbose */
} aopts;

private	int	aliasCreate(char *cmd, aopts *opts, char **av);
private	int	aliasShow(char *cmd, aopts *opts, char **av);
private	int	dbAdd(hash *aliasdb, char *alias, char **aliases);
private	int	dbRemove(hash *aliasdb, char *alias, char **aliases);
private	int	dbWrite(nested *n, hash *aliasdb, char *comment, int commit);
private	int	dbChk(nested *n, hash *aliasdb);
private	int	dbShow(nested *n, hash *aliasdb, char *cwd, char **aliases,
		    aopts *o);
private	int	expand(nested *n, hash *db, hash *keys, hash *seen,
		    char *alias);
private	int	value(nested *n, hash *keys, char *alias);
private	hash	*dbLoad(nested *n, hash *aliasdb);
private	int	chkReserved(char *alias, int fix);
private	int	validName(char *name);
private	comp	*findDir(nested *n, char *cwd, char *dir);
private	char	*relGlob(char *alias, char *pre, char *cwd);

int	rmAlias = 0;

/*
 * aliasdb is a hash -- key is alias and value is string of aliases
 * alias is a name into the hash
 * aliases is a list of names, keys, and globs
 * c is a pointer to a component
 * n is a pointer to a nest
 *
 * functions have to do with writing the db or reading the db
 * writers: dbAdd, dbRemove, dbWrite
 * readers: dbShow, aliasdb_expand
 *
 */

/*
 * bk alias [-C] name comp [...]	// create a new alias
 * bk alias set [-C] name comp [...]	// create or replace an alias
 * bk alias add [-C] name comp [...]	// append to existing alias (-f create)
 * bk alias rm [-C]  name comp [...]	// remove comps from an alias
 * bk alias rm [-C]  name		// remove entire alias
 * bk alias [-rrev] [-hm]		// list all aliases
 * bk alias [-rrev] [-hm] [-k] name	// show the value of a db key
 * bk alias -e [-rrev] [-hm] [-k] name	// show the key or path expansion
 */
int
alias_main(int ac, char **av)	/* looks like bam.c:bam_main() */
{
	int	i, c, cmdn;
	int	islist = 0;	/* saw an option that only goes to list */
	int	isntlist = 0;	/* saw an option that never goes to list */
	aopts	opts = {0};
	struct {
		char	*verb;
		int	(*fcn)(char *cmd, aopts *opts, char **av);
	} cmds[] = {
		{"new", aliasCreate },
		{"set", aliasCreate },
		{"add", aliasCreate },
		{"rm", aliasCreate },
		{"list", aliasShow },
		{0, aliasShow}		/* default with no verb  */
	};
	for (cmdn = 0; cmds[cmdn].verb; cmdn++) {
		if (av[1] && streq(av[1], cmds[cmdn].verb)) {
			ac -= 1;
			av += 1;
			break;
		}
	}
	while ((c = getopt(ac, av, "@|Cefkhmr;qv", 0)) != -1) {
		switch (c) {
		    case '@':
			isntlist = 1;
			if (bk_urlArg(&opts.urls, optarg)) usage();
			break;
		    case 'C': isntlist = 1; opts.nocommit = 1; break;
		    case 'f': isntlist = 1; opts.force = 1; break;
		    case 'e': islist = 1; opts.expand = 1; break;
		    case 'k': islist = 1; opts.showkeys = 1; break;
		    case 'h': islist = 1; opts.here = 1; break;
		    case 'm': islist = 1; opts.missing = 1; break;
		    case 'r': islist = 1; opts.rev = optarg; break;
		    case 'q': opts.quiet = 1; break;
		    case 'v': opts.verbose += 1; break;
		    default: bk_badArg(c, av);
		}
	}
	/*
	 * if we have list options but are using one of the action verbs
	 * then give a usage error.
	 */
	if (!cmds[cmdn].verb || streq(cmds[cmdn].verb, "list")) {
		if (isntlist) usage();
	} else if (islist) {
		usage();
	}

	if (opts.here && opts.missing) {
		error("%s: here or missing but not both\n", prog);
		usage();
	}

	EACH(opts.urls) {
		char	*u = opts.urls[i];

		opts.urls[i] = parent_normalize(u);
		free(u);
	}

	/* now call the correct function */
	return (cmds[cmdn].fcn(cmds[cmdn].verb, &opts, av + optind));
}

/* used for new, set, add, rm */
private int
aliasCreate(char *cmd, aopts *opts, char **av)
{
	nested	*n = 0;
	comp	*cp;
	hash	*aliasdb = 0;
	char	*p, *alias = 0;
	char	*comment = 0;
	char	**aliases = 0, *nlid = 0;
	int	c, i;
	int	reserved = 0, rc = 1, needunedit = 0;
	int	ac = 0;
	popts	ops;

	if (proj_cd2product()) {
		error("%s: called in a non-product.\n", prog);
		goto err;
	}
	unless (alias = av[ac++]) {
		error("%s: no alias specified.\n", prog);
		usage();
	}
	rmAlias = (streq(cmd, "rm") && !av[ac]);
	// downcast reserved keys if not removing an alias
	unless (rmAlias) reserved = chkReserved(alias, 1);
	assert(reserved >= 0);
	unless (rmAlias || reserved || validName(alias)) usage();
	if (reserved && !streq(alias, "here")) {
		error("%s: reserved name \"%s\" may not be changed.\n",
		    prog, alias);
		usage();
	}
	bzero(&ops, sizeof(ops));
	ops.quiet = opts->quiet;
	ops.verbose = opts->verbose;

	/* lock */
	unless (nested_mine(0, getenv("_NESTED_LOCK"), 1)) {
		unless (nlid = nested_wrlock(0)) {
			error("%s", nested_errmsg());
			return (1);
		}
		safe_putenv("_NESTED_LOCK=%s", nlid);
	}

	/* get the nest */
	unless (n = nested_init(0, 0, 0, NESTED_PENDING)) goto err;
	unless (aliasdb = aliasdb_init(n, 0, n->tip, n->pending, 1)) goto err;

	/* get the list of aliases to add or remove */
	for (; (p = av[ac]); ++ac) {
		if (streq(p, "-") && !av[ac+1]) break; // last is '-'
		aliases = addLine(aliases, strdup(p));
	}
	/* if last is '-', then fetch params from stdin */
	if (p) {
		while (p = fgetline(stdin)) {
			aliases = addLine(aliases, strdup(p));
		}
	}

	/* replace vals like ./tcl with a rootkey; downcase reserved words */
	if (c = aliasdb_chkAliases(n, aliasdb, &aliases, start_cwd)) {
		error("%s: %d error%s processing aliases\n",
		    prog, c, (c > 1)?"s":"");
		goto err;
	}

	if (streq(alias, "here")) {
		if (streq(cmd, "rm")) {
			EACH(aliases) {
				unless (removeLine(n->here, aliases[i], free)) {
					// XXX bad message LMXXX
					error("%s: %s not in HERE\n",
					    prog, aliases[i]);
					goto err;
				}
			}
		} else {
			if (streq(cmd, "new") || streq(cmd, "set")) {
				freeLines(n->here, free);
				n->here = 0;
			}
			EACH(aliases) {
				n->here = addLine(n->here, strdup(aliases[i]));
			}
			uniqLines(n->here, free);
		}
		goto write;

	}
	if (streq(cmd, "new") || streq(cmd, "set")) {
		if (hash_fetchStr(aliasdb, alias)) {
			if (streq(cmd, "new")) {
				error("%s: %s exists, use set?\n", prog, alias);
				goto err;
			}
			if (dbRemove(aliasdb, alias, 0)) goto err;
			comment = aprintf("Replace alias %s", alias);
		} else {
			comment = aprintf("Create alias %s", alias);
		}
		if (dbAdd(aliasdb, alias, aliases)) goto err;
	} else if (streq(cmd, "add")) {
		unless (hash_fetchStr(aliasdb, alias)) {
			comment = aprintf("Create alias %s", alias);
		} else {
			comment = aprintf("Modify alias %s", alias);
		}
		if (dbAdd(aliasdb, alias, aliases)) goto err;
	} else if (streq(cmd, "rm")) {
		if (dbRemove(aliasdb, alias, aliases)) goto err;
		comment = aprintf("Delete alias %s", alias);
	} else {
		error("%s: unknown command %s", cmd ? cmd : "null");
		goto err;
	}
	/*
	 * Write aliases file now before we try populating so that
	 * consistancy checks will work when reading the file.
	 */
	(void)system("bk edit -q " ALIASES);
	hash_toFile(aliasdb, ALIASES);
	needunedit = 1;
write:
	/* see if the HERE still matches the present bits */
	if (aliasdb_tag(n, aliasdb, n->here)) goto err;
	EACH_STRUCT(n->comps, cp, i) {
		if (cp->present != cp->alias) {
			ops.runcheck = 1;
			ops.force = opts->force;
			if (nested_populate(n, opts->urls, &ops)) {
				goto err;
			}
			break;
		}
	}
	if (streq(alias, "here")) {
		nested_writeHere(n);
	} else {
		if (dbWrite(n, aliasdb, comment, !opts->nocommit)) goto err;
	}
	rc = 0;
err:
	if (nlid) {
		if (nested_unlock(0, nlid)) {
			error("%s", nested_errmsg());
			rc = 1;
		}
		free(nlid);
		putenv("_NESTED_LOCK=");
	}
	if (rc && needunedit) {
		/* revert any local edits to the aliases file */
		system("bk unedit -q " ALIASES);
	}
	nested_free(n);
	aliasdb_free(aliasdb);
	freeLines(aliases, free);
	if (comment) free(comment);
	return (rc);
}

private	int
aliasShow(char *cmd, aopts *opts, char **av)
{
	nested	*n = 0;
	hash	*aliasdb = 0;
	char	*p;
	char	*comment = 0;
	char	**aliases = 0;
	int	rc = 1;
	int	nflags;
	int	ac = 0;

	cmd = av[0];
	if (proj_cd2product()) {
		error("%s: called in a non-product.\n", prog);
		goto err;
	}
	/* similar to rm with no params, turn off checks for show with none */
	rmAlias = !av[ac];
	/* get the nest */
	nflags = (opts->rev ? 0 : NESTED_PENDING);
	unless (n = nested_init(0, opts->rev, 0, nflags)) goto err;
	unless (aliasdb = aliasdb_init(n, 0, n->tip, n->pending, 0)) goto err;

	/* slurp in a list of aliases to list */
	for (; (p = av[ac]); ++ac) {
		if (streq(p, "-") && !av[ac+1]) break; // last is '-'
		aliases = addLine(aliases, strdup(p));
	}
	/* if last is '-', then fetch params from stdin */
	if (p) {
		while (p = fgetline(stdin)) {
			aliases = addLine(aliases, strdup(p));
		}
	}

	rc = dbShow(n, aliasdb, start_cwd, aliases, opts);
err:
	nested_free(n);
	aliasdb_free(aliasdb);
	freeLines(aliases, free);
	if (comment) free(comment);
	return (rc);
}

hash	*
aliasdb_init(nested *n, project *p, char *rev, int pending, int no_diffs)
{
	hash	*aliasdb = 0;
	project	*prodroot;
	char	*path = 0;
	sccs	*s;
	char	*csetrev = 0;
	char	buf[MAXPATH];
	char	tmp[MAXPATH];

	/*
	 * XXX: if 'p' then assuming product -- error check it?
	 */
	assert(n);
	unless (p || (p = n->proj) || (p = proj_product(0))) {
		error("%s: aliasdb: not in a product\n", prog);
		return (0);
	}
	concat_path(buf, proj_root(p), ALIASES);
	path = name2sccs(buf);
	s = sccs_init(path, INIT_MUSTEXIST);
	free(path);
	if (!s && (prodroot = proj_isResync(p))) {
		/* if no aliases in RESYNC, use directory above */
		concat_path(buf, proj_root(prodroot), ALIASES);
		path = name2sccs(buf);
		if (s = sccs_init(path, INIT_MUSTEXIST)) {
			/* the sccs_get "-r@<rev>" will use RESYNC cset file */
			if (s->proj) proj_free(s->proj);
			s->proj = proj_init(proj_root(p));
		}
		free(path);
	}
	if (s) {
		if (pending) {
			assert(!rev);
		} else {
			csetrev = aprintf("@%s", rev ? rev : "+");
		}
		if (pending && HAS_GFILE(s)) {
			if (no_diffs && sccs_hasDiffs(s, SILENT, 1)) {
				error("%s: local edits in %s\n",
				    prog, ALIASES);
				sccs_free(s);
				return (0);
			}
			aliasdb = hash_fromFile(0, s->gfile);
		} else {
			bktmp(tmp, "aliasdb");
			if (sccs_get(s, csetrev, 0,0,0, SILENT|PRINT, tmp)) {
				error("%s: aliases get failed: rev %s\n",
				    prog, csetrev ? csetrev : "+");
				sccs_free(s);
				return (0);
			}
			aliasdb = hash_fromFile(0, tmp);
			unlink(tmp);
		}
		if (csetrev) free(csetrev);
		sccs_free(s);
	} else if (pending) {
		if (no_diffs && exists(buf)) {
			error("%s: local edits in %s\n", prog, ALIASES);
			return (0);
		}
		aliasdb = hash_fromFile(0, buf); /* may have gfile */
	}

	unless (aliasdb) aliasdb = hash_new(HASH_MEMHASH);

	if (dbChk(n, aliasdb)) {
		aliasdb_free(aliasdb);
		aliasdb = 0;
	}
	return (aliasdb);
}

void
aliasdb_free(hash *aliasdb)
{
	if (aliasdb) hash_free(aliasdb);
}

// ====== functions used to write the db =========

/*
 * Add aliases to alias 'alias' in the aliases db,
 * creating alias if it doesn't already exist
 * XXX: can't detect a null difference.
 */
private	int
dbAdd(hash *aliasdb, char *alias, char **aliases)
{
	char	*val = 0;
	char	**list = 0;
	int	i, rc = 1;

	if (strchr(alias, '-')) {
		error("%s: invalid alias name: %s\n", prog, alias);
		goto err;
	}
	if (val = hash_fetchStr(aliasdb, alias)) {
		list = splitLine(val, "\r\n", 0);
	}
	EACH(aliases) {
		if (streq(aliases[i], "here") || streq(aliases[i], "there")) {
			error("%s: not allowed as value: %s in key %s\n",
			    prog, aliases[i], alias);
			goto err;
		}
		list = addLine(list, strdup(aliases[i]));
	}
	uniqLines(list, free);
	val = joinLines("\n", list);
	unless (hash_storeStr(aliasdb, alias, val ? val : "")) {
		error("%s: failed to store aliases in %s: %s\n",
		    prog, alias, val);
		goto err;
	}
	rc = 0;
err:
	if (val) free(val);
	freeLines(list, free);
	return (rc);
}

/*
 * Remove aliases from an alias definition.
 * If aliases == 0, then the alias itself is removed.
 */
private	int
dbRemove(hash *aliasdb, char *alias, char **aliases)
{
	int	i, errors = 0, rc = 1;
	char	*p;
	char	**list = 0;

	unless (p = hash_fetchStr(aliasdb, alias)) {
		error("%s: no such alias \"%s\".\n"
		    "Check to see if listed in \"bk alias\".\n",
		    prog, alias);
		goto err;
	}
	if (aliases) {
		list = splitLine(p, "\r\n", 0);
		EACH (aliases) {
			unless (removeLine(list, aliases[i], free)) {
				error("%s: %s is not part of %s\n",
				    prog, aliases[i], alias);
				errors++;
			}
		}
		if (errors) {
			error("%s: %s not modified\n", prog, alias);
			goto err;
		}
		p = joinLines("\n", list);
		hash_storeStr(aliasdb, alias, p ? p : "");
		if (p) free(p);
	} else {
		hash_deleteStr(aliasdb, alias);
	}
	rc = 0;

err:
	freeLines(list, free);
	return (rc);
}

private	int
dbWrite(nested *n, hash *aliasdb, char *comment, int commit)
{
	int	ret = 0;
	char	*tmpfile;
	char	buf[MAXPATH];

	if (ret = dbChk(n, aliasdb)) return (ret);

	if (n->cset) sccs_close(n->cset);	/* win32 */

	(void)system("bk -P edit -q " ALIASES);
	hash_toFile(aliasdb, ALIASES);
	sprintf(buf, "bk -P clean -q %s", ALIASES);
	unless (ret = system(buf)) return (0); // if clean, okay
	sprintf(buf, "bk -P delta -aqy'%s' %s", comment, ALIASES);
	if (ret = system(buf)) return (ret);
	if (commit) {
		tmpfile = bktmp(0, "cmt");
		sprintf(buf,
		    "bk -P sfiles -pAC %s |"
		    "bk -P sccslog -A -f -nd:C: - >'%s'", ALIASES, tmpfile);
		if (ret = system(buf)) return (ret);
		sprintf(buf,
		    "bk -P sfiles -pC %s |"
		    "bk -P commit -qfY'%s' -", ALIASES, tmpfile);
		ret = system(buf);
		unlink(tmpfile);
		free(tmpfile);
	}
	return (ret);
}

/*
 * integrity check the db
 * XXX: Do we want to have a fix?
 */

private	int
dbChk(nested *n, hash *aliasdb)
{
	int	reserved;
	int	total = 0, errors = 0;
	char	*key;
	char	**aliases = 0;
	char	**comps;

	// disable checks if partial list or removing an alias
	// Used to have n->revs, but now the list is full
	if (rmAlias || !aliasdb) return (0);

#ifdef BREAK_UNDO
	// Check the HERE file entry
	if (errors = aliasdb_chkAliases(n, aliasdb, &n->here, 0)) {
		error("%s: 'HERE' file failed integrity check\n", prog);
		total += errors;
	}
#endif

	EACH_HASH(aliasdb) {
		key = aliasdb->kptr;
		if (reserved = chkReserved(key, 0)) {
			if (reserved < 0) {
				error("%s: bad case for key: %s\n",
				    prog, key);
				total++;
			} else {
				error("%s: illegal aliasdb key: %s\n",
				    prog, key);
				total++;
			}
		} else {
			unless (validName(key)) total++;
		}
		aliases = splitLine(aliasdb->vptr, "\r\n", 0);
		if (errors = aliasdb_chkAliases(n, aliasdb, &aliases, 0)) {
			error("%s: bad values for key: %s\n",
			    prog, key);
			total += errors;
		}
		freeLines(aliases, free);
		/* check for recursion */
		if (comps = aliasdb_expandOne(n, aliasdb, key)) {
			freeLines(comps, 0);
		} else {
			total++;
		}
	}
	if (total) {
		error("%s: %d error%s found while checking the aliasdb\n",
		    prog, total, (total > 1)?"s":"");
	}
	return (total);
}

// =================== functions to read the db ===============

/*
 * Given an aliasdb and a list of aliases, set c->alias on each component
 * in 'n' that is contained in those aliases.
 *
 * Prints error message and returns non-zero if aliases fail to expand.
 */
int
aliasdb_tag(nested *n, hash *aliasdb, char **aliases)
{
	comp	*c;
	char	**comps;
	int	i, j;

	assert(n);
	EACH_STRUCT(n->comps, c, i) c->alias = 0;
	EACH_INDEX(aliases, j) {
		unless (comps = aliasdb_expandOne(n, aliasdb, aliases[j])) {
			return (-1);
		}
		EACH_STRUCT(comps, c, i) {
			assert(!c->product);
			c->alias = 1;
		}
		freeLines(comps, 0);
	}
	n->product->alias = 1;
	n->alias = 1;
	return (0);
}

/*
 * Return a list of pointers to components -- a subset of n->comps
 * in the same order.  Or 0 if something was amiss.
 * (an alias that expands to nothing returns an empty but allocated lines array)
 */
char	**
aliasdb_expandOne(nested *n, hash *aliasdb, char *alias)
{
	int	i;
	comp	*c;
	char	**comps = 0;
	hash	*keys = 0, *seen = 0;

	assert(n && alias);
	/*
	 * this is a bit rude for now. Set param to 1 when tired
	 * of chasing down errors.  For now, it lets errors be seen.
	 */

	if (chkReserved(alias, 0) < 0) goto err;

	unless (aliasdb = dbLoad(n, aliasdb)) goto err;

	keys = hash_new(HASH_MEMHASH);
	seen = hash_new(HASH_MEMHASH);
	/* resursive expansion fills 'keys' hash with rootkeys */
	if (expand(n, aliasdb, keys, seen, alias)) {
		error("%s: expansion of alias %s failed\n", prog, alias);
		goto err;
	}
	/* output subset of n->comps in n->comps order */
	EACH_STRUCT(n->comps, c, i) {
		if (hash_fetchStr(keys, c->rootkey)) {
			assert(!c->product);
			comps = addLine(comps, c);
		}
	}
	unless (comps) comps = allocLines(2);	/* always return something */
err:
	if (seen) hash_free(seen);
	if (keys) hash_free(keys);
	return (comps);
}

private void
dbPrint(nested *n, hash *aliasdb, char **items, int indent, aopts *op)
{
	int	sawall, i, j;
	char	*val;
	char	**comps, **list;
	comp	*c;

	if (op->missing || op->here) {
		EACH(items) {
			val = items[i];
			sawall = 1;
			comps = aliasdb_expandOne(n, aliasdb, val);
			EACH_STRUCT(comps, c, j) {
				unless (c->present) {
					sawall = 0;
					break;
				}
			}
			freeLines(comps, 0);
			unless ((sawall && op->here) ||
			    (!sawall && op->missing)) {
				removeLineN(items, i, free);
				i--;
			}
		}
	}
	unless (op->showkeys) {
		EACH(items) {
			unless (isKey(items[i]) && strchr(items[i], '|')) {
				continue;
			}
			unless (c = nested_findKey(n, items[i])) {
				error("%s: no component: %s\n", prog, items[i]);
				continue;
			}
			free(items[i]);
			items[i] = aprintf("./%s", c->path);
		}
	}
	sortLines(items, 0);
	EACH(items) {
		for (j = 0; j < indent; j++) putchar('\t');
		printf("%s", items[i]);
		if ((indent >= op->verbose) ||
		    !(val = hash_fetchStr(aliasdb, items[i]))) {
			puts("");
			continue;
		}
		unless (hash_insertStr(op->seen, items[i], 0)) {
			/* mark that this key was already expanded */
			puts("*");
			continue;
		}
		puts(":");

		/* recurse */
		list = splitLine(val, "\r\n", 0);
		sortLines(list, 0);
		dbPrint(n, aliasdb, list, indent+1, op);
		freeLines(list, free);
	}
}


private	int
dbShow(nested *n, hash *aliasdb, char *cwd, char **aliases, aopts *op)
{
	char	**items = 0;
	char	*val, *alias;
	comp	*c;
	int	i, j, rc = 1;

	assert(aliasdb);
	if (op->expand) {
		if (aliases) {
			if (aliasdb_chkAliases(n, aliasdb, &aliases, cwd)) {
				goto err;
			}
			if (aliasdb_tag(n, aliasdb, aliases)) {
				goto err;
			}
		}

		EACH_STRUCT(n->comps, c, i) {
			if (c->product ||
			    (op->missing && c->present) ||
			    (op->here && !c->present)) {
				continue;
			}
			if (n->alias && !c->alias) continue;
			items = addLine(items,
			    op->showkeys
			    ?  strdup(c->rootkey)
			    : aprintf("./%s", c->path));
		}
		sortLines(items, 0);
		EACH(items) puts(items[i]);
		freeLines(items, free);
		return (0);
	}

	unless (aliases) {
		/* print all the alias keys */
		EACH_HASH(aliasdb) {
			items = addLine(items, strdup(aliasdb->kptr));
		}
		goto preprint;
	}

	if (op->missing || op->here) {
		if (aliasdb_chkAliases(n, aliasdb, &aliases, cwd)) {
			goto err;
		}
		EACH(aliases) items = addLine(items, strdup(aliases[i]));
		goto preprint;
	}

	/* list out the contents for a single key */
	unless (nLines(aliases) == 1) {
		error("%s: one alias at a time, or use -e\n", prog);
		goto err;
	}
	alias = aliases[1];	// firstLine(aliases);
	/*
	 * print the val entry from the db
	 */
	if (i = chkReserved(alias, 1)) {
		assert(i >= 0);
		if (streq(alias, "here")) {
			EACH_INDEX(n->here, j) {
				items = addLine(items, strdup(n->here[j]));
			}
			goto preprint;
		} else {
			error("%s: use -e when expanding "
			    "reserved alias; %s\n", prog, alias);
			goto err;
		}
	}
	if ((i == 0) && !validName(alias)) {
		error("If a path, glob, or key, "
		    "use -e when expanding\n");
		goto err;
	}
	val = hash_fetchStr(aliasdb, alias);
	unless (val) {
		error("%s: no alias: %s\n", prog, alias);
		goto err;
	}
	items = splitLine(val, "\r\n", 0);

preprint:
	op->seen = hash_new(HASH_MEMHASH);
	dbPrint(n, aliasdb, items, 0, op);
	hash_free(op->seen);
	rc = 0;

err:	freeLines(items, free);
	return (rc);
}

/*
 * Recursively expand a single alias into a hash of rootkeys
 */
private	int
expand(nested *n, hash *aliasdb, hash *keys, hash *seen, char *alias)
{
	char	*mval;
	char	**expansion = 0;
	comp	*c;
	int	i, inserted = 0, rc = 1;

	assert(n && aliasdb && keys && seen && alias);

	if (streq("here", alias) || streq("there", alias)) {
		error("%s: %s not allowed in an alias definition\n",
		    prog, alias);
		goto done;
	}

	if (streq("all", alias)) {
		EACH_STRUCT(n->comps, c, i) {
			if (c->product) continue;
			hash_insertStr(keys, c->rootkey, 0);
		}
		rc = 0;
		goto done;
	}

	unless (mval = hash_fetchStr(aliasdb, alias)) {
		rc = value(n, keys, alias);
		goto done;
	}

	unless (hash_insertStr(seen, alias, 0)) {
		error("%s: recursive alias definition '%s'.\n", prog, alias);
		goto done;
	}

	inserted = 1;

	expansion = splitLine(mval, "\r\n", 0);
	EACH(expansion) { /* build up keys */
		if (expand(n, aliasdb, keys, seen, expansion[i])) goto done;
	}
	rc = 0;
done:
	if (inserted) hash_deleteStr(seen, alias);
	freeLines(expansion, free);
	return (rc);
}

/*
 * Expand a value of the alias db into its keys, note that this doesn't handle
 * alias names. See expand() for that.
 */
private	int
value(nested *n, hash *keys, char *alias)
{
	if (isKey(alias)) {
		hash_insertStr(keys, alias, 0);
#ifdef	CRAZY_GLOB
	} else if (is_glob(alias)) {
		int	i;
		comp	*c;

		/* XXX: is this needed */
		if (strneq(alias, "./", 2)) alias += 2;

		EACH_STRUCT(n->comps, c, i) {
			if (c->product) continue;
			if (match_one(c->path, alias, 0)) {
				hash_insertStr(keys, c->rootkey, 0);
			}
		}
#endif
	} else {
		error("%s: value must be either a "
#ifdef	CRAZY_GLOB
		    "glob, " // add a comma after key below
#endif
		    "key or alias: %s\n", prog, alias);
		return (1);
	}
	return (0);
}

/*
 * Utilities from here on out
 */

private	hash	*
dbLoad(nested *n, hash *aliasdb)
{
	unless (aliasdb) {
		unless (n->aliasdb ||
		    (n->aliasdb = aliasdb_init(n, 0, n->tip, n->pending, 0))) {
			return (0);
		}
		aliasdb = n->aliasdb;
	}
	return (aliasdb);
}

/*
 * Given a list of aliases, verify they are all legal.
 * Return non-zero and print error messages if problems are
 * found.
 *
 * If 'cwd', then the aliases are expanded from the command line to
 * the 'standard' form.  Pathnames are mapped to rootkeys and globs
 * are expanded.
 */
int
aliasdb_chkAliases(nested *n, hash *aliasdb, char ***paliases, char *cwd)
{
	int	i, j, reserved, errors = 0, fix = (cwd != 0);
	comp	*c;
	char	*p, *alias, **aliases;
	char	**addkeys = 0, **globkeys = 0;

	unless (aliasdb || (aliasdb = dbLoad(n, 0))) {
		error("%s: cannot initial aliasdb\n", prog);
		return (1);
	}

	assert(paliases);
	aliases = *paliases;
	EACH(aliases) {
		alias = aliases[i];

		if (reserved = chkReserved(alias, fix)) {
			if (reserved < 0) {
				errors++; /* case problem */
			} else if (streq(alias, "here") ||
			    streq(alias, "there")) {
				/* 'here' will auto-expand */
				if (fix) {
					EACH_INDEX(n->here, j) {
						addkeys = addLine(
						    addkeys, n->here[j]);
					}
					removeLineN(aliases, i, free);
					i--;
				} else {
					error("%s: not allowed as value: %s\n",
					    prog, alias);
					errors++;
				}
			}
			continue;
		}
		/* see if alias is in the aliasdb and has a validName */
		if (hash_fetchStr(aliasdb, alias)) {
			unless (validName(alias)) errors++;
			continue;
		}

		if (isKey(alias)) {
			/* is it a normal rootkey? */
			if (strchr(alias, '|')) {
				/* that is in the nested collection? */
				if (c = nested_findKey(n, alias)) {
					if (c->product) goto root;
					continue;
				}

				/* rootkey but not found */
				error("%s: not a component rootkey: %s\n",
				    prog, alias);
				errors++;
				continue;
			}

			/* it is an md5 key */
			unless (fix) {
				error("%s: illegal md5key: %s\n", prog, alias);
				errors++;
				continue;
			}

			/* try replacing it with a rootkey */
			unless (c = nested_findMD5(n, alias)) {
				error("%s: not a component md5key: %s\n",
				    prog, alias);
				errors++;
				continue;
			}

			/* okay, we have a rootkey to replace it */
			debug((stderr, "%s was md5, now rootkey\n", alias));

			/* strip out product rootkeys */
root:			if (c->product) {
				unless (fix) {
					error(
					    "%s: list has product rookey: %s\n",
					    prog, alias);
					errors++;
					continue;
				}
				removeLineN(aliases, i, free);
				i--;
				continue;
			}
			unless (c->included) {
				error("%s: component not present: %s\n",
				    prog, c->path);
				errors++;
				continue;
			}
			free(alias);
			aliases[i] = strdup(c->rootkey);
			continue;
		}

		/* Is it a glob ? */
		if (p = is_glob(alias)) {
#ifndef	CRAZY_GLOB
			unless (fix) {
				error( "%s: glob not allowed: %s\n",
				    prog, alias);
				errors++;
				continue;
			}
			unless (alias = relGlob(alias, p, cwd)) {
				error( "%s: glob not in this repository: %s\n",
				    prog, aliases[i]);
				errors++;
				continue;
			}
			assert(!globkeys);
			EACH_STRUCT(n->comps, c, j) {
				if (c->product) continue;
				if (match_one(c->path, alias, 0)) {
					globkeys = addLine(
					    globkeys, c->rootkey);
				}
			}
			free(alias);
			alias = 0;
			unless (globkeys) {
				error("%s: %s does not match any components.\n",
				    prog, aliases[i]);
				errors++;
				continue;
			} else {
				EACH_INDEX(globkeys, j) {
					addkeys = addLine(
					    addkeys, globkeys[j]);
				}
				freeLines(globkeys, 0);
				globkeys = 0;
			}
			removeLineN(aliases, i, free);
			i--;
#endif
			continue;
		}

		/*
		 * if this is command line, is it a path which points
		 * to a repository?
		 */
		if (fix && (c = findDir(n, cwd, alias))) goto root;

		if (fix) {
			error("%s: %s must be either a glob, "
			    "key, alias, or component.\n", prog, alias);
		} else {
			error("%s: %s must be either a "
#ifdef	CRAZY_GLOB
			    "glob, " // add a comma after key below
#endif
			    "key or alias.\n", prog, alias);
		}
		errors++;
	}
	EACH(addkeys) {
		aliases = addLine(aliases, strdup(addkeys[i]));
	}
	freeLines(addkeys, 0);
	*paliases = aliases;
	return (errors);
}

private	int
chkReserved(char *alias, int fix)
{
	int	rc = 0;
	char	**wp, *w;
	char	*reserved[] = {
		"all", "here", "there",
		"new", "add", "rm", "set", "list", 0};

	for (wp = reserved; (w = *wp); wp++) {
		if (strieq(w, alias)) break;
	}
	if (w) {
		if (streq(w, alias)) {
			rc = 1;
		} else if (fix) {
			debug((stderr, "downcasting alias: %s\n", alias));
			assert(strlen(alias) == strlen(w));
			strcpy(alias, w);

			/* map there => here */
			//XXX causes problems with error messages
			//if (streq(alias, "there")) strcpy(alias, "here");
			rc = 1;
		} else {
			error("%s: alias not downcasted: %s\n", prog, alias);
			rc = -1;
		}
	}
	return (rc);
}

/*
 * Check that a alias name is valid
 * valid == /^[a-z][a-z0-9_\-]*$/i
 */
private	int
validName(char *name)
{
	char	*p;

	unless (isalpha(name[0])) {
err:		error("%s: invalid alias name: %s\n", prog, name);
		return (0);
	}
	for (p = name+1; *p; p++) {
		unless (isalnum(*p) || (*p == '_') || (*p == '-')) {
		    	goto err;
		}
	}
	return (1);
}

/*
 * Get the rootkey of a directory and verify
 * that it is indeed a component of this product
 */
private	comp	*
findDir(nested *n, char *cwd, char *dir)
{
	char	*p = 0;
	comp	*c = 0;

	/* cwd idiom lifted from bkd_client:nfs_parse() */
	if (cwd && !IsFullPath(dir)) {
		dir = p = aprintf("%s/%s", cwd, dir);
	}
	dir = proj_relpath(0, dir);
	if (p) free(p);
	c = nested_findDir(n, dir);
	free(dir);
	return (c);
}

/*
 * compute a glob relative to repo root.  Example:
 * alias = './path/to/FOO*'
 * pre = "/FOO*"
 * pre is points into alias to the last slash before the first glob char.
 * If no slash, then just the first char:
 * if alias is 'FOO*X/bar', then pre points to the F : "FOO*X/bar"
 */

private	char	*
relGlob(char *alias, char *pre, char *cwd)
{
	char	*t1 = 0, *t2 = 0, *ret = 0;

	assert(cwd);		/* proj_relpath will block cwd == "/" */
	if (*pre == '/') {
		/*
		 * if cwd = "/foo/bar"; alias = "src/FOO*"
		 * then make cwd = "/foo/bar/src"; alias = "FOO*"
		 */
		*pre = 0;
		t1 = aprintf("%s/%s", cwd, alias);
		cwd = t1;
		alias = pre+1;
		*pre = '/';
	}
	/* clean and make rel */
	unless (t2 = proj_relpath(0, cwd)) goto err;
	if (streq(t2, ".")) {
		ret = strdup(alias);
	} else {
		ret = aprintf("%s/%s", t2, alias);
	}
err:
	if (t1) free(t1);
	if (t2) free(t2);
	return (ret);
}
