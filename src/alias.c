#include "sccs.h"
#include "nested.h"

/*
 * in case I want to try storing globs in the aliasdb: uncomment
 */
// #define CRAZY_GLOB	1

private	int	aliasCreate(int ac, char **av);
private	int	aliasShow(int ac, char **av);
private	int	dbAdd(hash *aliasdb, char *alias, char **aliases);
private	int	dbRemove(hash *aliasdb, char *alias, char **aliases);
private	int	dbWrite(nested *n, hash *aliasdb, char *comment, int commit);
private	int	dbChk(nested *n, hash *aliasdb);
private	int	dbShow(nested *n, hash *aliasdb, char *cwd, char **aliases,
		    int showkeys, int showpaths);
private	int	expand(nested *n, hash *db, hash *keys, hash *seen,
		    char *alias);
private	int	value(nested *n, hash *keys, char *alias);
private	hash	*dbLoad(nested *n, hash *aliasdb);
private	int	chkReserved(char *alias, int fix);
private	int	validName(char *name);
private	comp	*findDir(nested *n, char *cwd, char *dir);

/* union of aliasCreate and aliasShow options */

typedef struct {
	char	*rev;
	u32	force:1;
	u32	quiet:1;
	u32	showkeys:1;
	u32	showpaths:1;
	u32	commit:1;
} opts;

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
 * create a new one or overwrite (w/ -f)
 * bk alias create [-Cfq] name comp [...]
 *
 * bk alias add [-Cfq] name comp [...]	// append to existing alias (-f create)
 * bk alias rm [-Cq]  name comp [...]	// remove comps from an alias
 * bk alias rm [-Cq]  name		// remove entire alias
 * bk alias list			// list all aliases
 * bk alias list name			// show the value of a db key
 * bk alias list -k | -p comp [...]	// show the key or path expansion
 */

int
alias_main(int ac, char **av)	/* looks like bam.c:bam_main() */
{
	int	c, i;
	struct {
		char	*name;
		int	(*fcn)(int ac, char **av);
	} cmds[] = {
		{"new", aliasCreate },
		{"set", aliasCreate },
		{"add", aliasCreate },
		{"rm", aliasCreate },
		{"show", aliasShow },
		{0, 0}
	};

	while ((c = getopt(ac, av, "")) != -1) {
		switch (c) {
		    default:
usage:			system("bk help -s alias");
			return (1);
		}
	}
	unless (av[optind]) goto usage;
	for (i = 0; cmds[i].name; i++) {
		if (streq(av[optind], cmds[i].name)) {
			ac -= optind;
			av += optind;
			getoptReset();
			return (cmds[i].fcn(ac, av));
		}
	}
	goto usage;
}

int
aliasCreate(int ac, char **av)	/* used for create, add, rm */
{
	nested	*n = 0;
	hash	*aliasdb = 0;
	char	*p, *alias = 0;
	char	*comment = 0;
	char	**aliases = 0;
	char	*cwd, *cmd;
	int	c;
	int	reserved = 0, rc = 1;
	int	nflags;
	opts	opts;

	memset(&opts, 0, sizeof(opts));
	opts.commit = 1;
	cwd = strdup(proj_cwd());
	cmd = av[0];
	if (streq(cmd, "set")) {
		cmd = "new";
		opts.force = 1;
	}
	if (proj_cd2product()) {
		error("%s: called in a non-product.\n", prog);
		goto err;
	}
	while ((c = getopt(ac, av, "Cfq")) != -1) {
		switch (c) {
		    case 'C': opts.commit = 0; break;
		    case 'f': opts.force = 1; break;
		    case 'q': opts.quiet = 1; break;
		    default:
usage:			sys("bk", "help", "-s", "alias", SYS);
			goto err;
		}
	}
	unless (av[optind]) {
		error("%s: no alias specified.\n", prog);
		goto usage;
	}
	alias = av[optind++];
	rmAlias = (streq(cmd, "rm") && !av[optind]);
	// downcast reserved keys if not removing an alias
	unless (rmAlias) reserved = chkReserved(alias, 1);
	assert(reserved >= 0);
	unless (rmAlias || reserved || validName(alias)) goto usage;
	if (reserved && !streq(alias, "default")) {
		error("%s: reserved name \"%s\" may not be changed.\n",
		    prog, alias);
		goto usage;
	}
	/* get the nest */
	nflags = NESTED_PENDING;
	unless (n = nested_init(0, 0, 0, nflags)) goto err;
	unless (aliasdb = aliasdb_init(n, 0, n->tip, n->pending)) goto err;

	/* get the list of aliases to add or remove */
	for (; (p = av[optind]); ++optind) {
		if (streq(p, "-") && !av[optind+1]) break; // last is '-'
		aliases = addLine(aliases, strdup(p));
	}
	/* if last is '-', then fetch params from stdin */
	if (p) {
		while (p = fgetline(stdin)) {
			aliases = addLine(aliases, strdup(p));
		}
	}

	/* replace vals like ./tcl with a rootkey; downcase reserved words */
	if (c = aliasdb_chkAliases(n, aliasdb, &aliases, cwd)) {
		error("%s: %d error%s processing aliases\n",
		    prog, c, (c > 1)?"s":"");
		goto err;
	}

	/*
	 * XXX
	 * The 'bk alias' command should not be allowed to change or delete
	 * any aliases that are currently in the BitKeeper/log/COMPONENTS
	 * file.  Otherwise the user can invalidate their repository.
	 */
	if (streq(cmd, "new")) {
		if (hash_fetchStr(aliasdb, alias)) {
			unless (opts.force) {
				error("%s: %s exists, use -f?\n", prog, alias);
				goto err;
			}
			if (dbRemove(aliasdb, alias, 0)) goto err;
			comment = aprintf("Replace alias %s", alias);
		} else {
			comment = aprintf("Create alias %s", alias);
		}
		if (dbAdd(aliasdb, alias, aliases)) goto err;
		if (dbWrite(n, aliasdb, comment, opts.commit)) goto err;
	}

	if (streq(cmd, "add")) {
		unless (hash_fetchStr(aliasdb, alias)) {
			unless (opts.force) {
				error("%s: %s does not exist.  "
				    "Use -f or bk alias create\n",
				    prog, alias);
				goto err;
			}
			comment = aprintf("Create alias %s", alias);
		} else {
			comment = aprintf("Modify alias %s", alias);
		}
		if (dbAdd(aliasdb, alias, aliases)) goto err;
		if (dbWrite(n, aliasdb, comment, opts.commit)) goto err;
	}

	if (streq(cmd, "rm")) {
		if (dbRemove(aliasdb, alias, aliases)) goto err;
		comment = aprintf("Delete alias %s", alias);
		if (dbWrite(n, aliasdb, comment, opts.commit)) goto err;
	}
	rc = 0;
err:
	free(cwd);
	nested_free(n);
	aliasdb_free(aliasdb);
	freeLines(aliases, free);
	if (comment) free(comment);
	return (rc);
}

private	int
aliasShow(int ac, char **av)
{
	nested	*n = 0;
	hash	*aliasdb = 0;
	char	*p;
	char	*comment = 0;
	char	**aliases = 0;
	char	*cwd;
	int	c;
	int	rc = 1;
	int	nflags;
	opts	opts;

	memset(&opts, 0, sizeof(opts));
	cwd = strdup(proj_cwd());
	if (proj_cd2product()) {
		error("%s: called in a non-product.\n", prog);
		goto err;
	}

	while ((c = getopt(ac, av, "kpqr;")) != -1) {
		switch (c) {
		    case 'k': opts.showkeys = 1; break;
		    case 'p': opts.showpaths = 1; break;
		    case 'q': opts.quiet = 1; break;
		    case 'r': opts.rev = optarg; break;
		    default:
usage:			sys("bk", "help", "-s", "alias", SYS);
			goto err;
		}
	}
	if (opts.showkeys && opts.showpaths) {
		error("%s: one of -k or -p but not both\n", prog);
		goto usage;
	}
	/* similar to rm with no params, turn off checks for show with none */
	rmAlias = !av[optind];
	/* get the nest */
	nflags = (opts.rev ? 0 : NESTED_PENDING);
	unless (n = nested_init(0, opts.rev, 0, nflags)) goto err;
	unless (aliasdb = aliasdb_init(n, 0, n->tip, n->pending)) goto err;

	/* slurp in a list of aliases to list */
	for (; (p = av[optind]); ++optind) {
		if (streq(p, "-") && !av[optind+1]) break; // last is '-'
		aliases = addLine(aliases, strdup(p));
	}
	/* if last is '-', then fetch params from stdin */
	if (p) {
		while (p = fgetline(stdin)) {
			aliases = addLine(aliases, strdup(p));
		}
	}

	rc = dbShow(n, aliasdb, cwd, aliases, opts.showkeys, opts.showpaths);
err:
	free(cwd);
	nested_free(n);
	aliasdb_free(aliasdb);
	freeLines(aliases, free);
	if (comment) free(comment);
	return (rc);
}

hash	*
aliasdb_init(nested *n, project *p, char *rev, int pending)
{
	hash	*aliasdb = 0;
	char	*path = 0;
	sccs	*s;
	char	*csetrev = 0;
	char	buf[MAXPATH];
	char	tmp[MAXPATH];

	/*
	 * XXX: if 'p' then assuming product -- error check it?
	 */
	unless (p || (p = proj_product(0))) {
		error("%s: aliasdb: not in a product\n", prog);
		return (0);
	}
	concat_path(buf, proj_root(p), ALIASES);
	path = name2sccs(buf);
	if (s = sccs_init(path, INIT_MUSTEXIST)) {
		if (pending) {
			assert(!rev);
		} else {
			csetrev = aprintf("@%s", rev ? rev : "+");
		}
		if (pending && HAS_GFILE(s)) {
			aliasdb = hash_fromFile(0, s->gfile);
		} else {
			bktmp(tmp, "aliasdb");
			if (!sccs_get(s, csetrev, 0,0,0, SILENT|PRINT, tmp)) {
				aliasdb = hash_fromFile(0, tmp);
			}
			unlink(tmp);
		}
		if (csetrev) free(csetrev);
		sccs_free(s);
	} else if (pending) {
		aliasdb = hash_fromFile(0, buf); /* may have gfile */
	}
	free(path);

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

	unless (nLines(aliases)) {
		// XXX normal and quiet mode coming...
		// error("%s: nothing to add\n", prog);
		goto err;
	}
	if (val = hash_fetchStr(aliasdb, alias)) {
		list = splitLine(val, "\r\n", 0);
	}
	EACH(aliases) {
		if (streq(alias, "here") || streq(alias, "there")) {
			error("%s: not allowed as value: %s\n",
			    prog, alias);
			goto err;
		}
		list = addLine(list, strdup(aliases[i]));
	}
	uniqLines(list, free);
	val = joinLines("\n", list);
	unless (hash_storeStr(aliasdb, alias, val)) {
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
		    "Check to see if listed in \"bk alias show\".\n",
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
	}
	if (errors) {
		error("%s: %s not modified\n", prog, alias);
		goto err;
	}
	if (nLines(list) > 0) {
		p = joinLines("\n", list);
		hash_storeStr(aliasdb, alias, p);
		free(p);
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

	EACH_HASH(aliasdb) {
		key = aliasdb->kptr;
		if (reserved = chkReserved(key, 0)) {
			if (reserved < 0) {
				error("%s: bad case for key: %s\n",
				    prog, key);
				total++;
			} else unless (streq(key, "default")) {
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

char	**
aliasdb_expand(nested *n, hash *aliasdb, char **aliases)
{
	comp	*c;
	char	**comps;
	int	i, j;

	assert(n);
	unless (aliases) return (0);
	EACH_STRUCT(n->comps, c) c->nlink = 0;
	EACH_INDEX(aliases, j) {
		unless (comps = aliasdb_expandOne(n, aliasdb, aliases[j])) {
			return (0);
		}
		EACH_STRUCT(comps, c) {
			assert(!c->product);
			c->nlink += 1; // deprecated
		}
		freeLines(comps, 0);
	}
	comps = 0;
	EACH_STRUCT(n->comps, c) {
		if (c->nlink) comps = addLine(comps, c);
	}
	unless (comps) comps = allocLines(2); /* always return something */
	n->alias = 1;
	return (comps);
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

	/*
	 * here and there can not be used inside an alias
	 */
	if (streq("here", alias) || streq("there", alias)) {
		EACH_STRUCT(n->comps, c) {
			if (c->product) continue;
			if (c->present) comps = addLine(comps, c);
		}
		return (comps);
	}

	keys = hash_new(HASH_MEMHASH);
	seen = hash_new(HASH_MEMHASH);
	/* resursive expansion fills 'keys' hash with rootkeys */
	if (expand(n, aliasdb, keys, seen, alias)) {
		error("%s: expansion of alias %s failed\n", prog, alias);
		goto err;
	}
	/* output subset of n->comps in n->comps order */
	EACH_STRUCT(n->comps, c) {
		if (hash_fetchStr(keys, c->rootkey)) {
			/* 
			 * I'd like to:  assert(!c->product);
			 * but clone sends the key over as part of
			 * a non empty COMPONENTS file. Until the
			 * full protocol is worked out, filtering them
			 * here.
			 */
			// assert(!c->product);
			if (c->product) continue;
			comps = addLine(comps, c);
		}
	}
	unless (comps) comps = allocLines(2);	/* always return something */
err:
	if (seen) hash_free(seen);
	if (keys) hash_free(keys);
	return (comps);
}

private	int
dbShow(nested *n, hash *aliasdb, char *cwd, char **aliases,
    int showkeys, int showpaths)
{
	char	**items = 0, **comps;
	char	*val, *alias;
	comp	*c;
	int	i, rc = 1;

	unless (aliases) {
		/* print all the alias keys */
		EACH_HASH(aliasdb) {
			items = addLine(items, strdup(aliasdb->kptr));
		}
		goto print;
	}

	/* if showing keys or paths, the expand any key or value item */
	if (showkeys || showpaths) {
		if (aliasdb_chkAliases(n, aliasdb, &aliases, cwd)) goto err;
		unless (comps = aliasdb_expand(n, aliasdb, aliases)) goto err;

		EACH_STRUCT(comps, c) {
			items = addLine(items,
			    strdup(showkeys ? c->rootkey : c->path));
		}
		freeLines(comps, 0);
		goto print;
	}

	/* list out the contents for a single key */
	unless (nLines(aliases) == 1) {
		error("%s: one alias at a time, or use -k or -p\n", prog);
		goto err;
	}
	alias = aliases[1];	// firstLine(aliases);
	/*
	 * print the val entry from the db; fake default if not there
	 */
	if (i = chkReserved(alias, 1)) {
		assert(i >= 0);
		unless (streq(alias, "default")) {
			error("%s: use -k or -p when expanding "
			    "reserved alias; %s\n", prog, alias);
			goto err;
		}
	}
	if ((i == 0) && !validName(alias)) {
		error("If a path, glob, or key, "
		    "use -k or -p when expanding\n");
		goto err;
	}
	val = hash_fetchStr(aliasdb, alias);
	if (!val && streq(alias, "default")) {
		val = "all";
	} else if (!val) {
		error("%s: no alias: %s\n", prog, alias);
		goto err;
	}
	items = splitLine(val, "\r\n", 0);

print:
	sortLines(items, 0);
	EACH(items) printf("%s\n", items[i]);
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
		EACH_STRUCT(n->comps, c) {
			if (c->product) continue;
			hash_insertStr(keys, c->rootkey, 0);
		}
		rc = 0;
		goto done;
	}

	unless (mval = hash_fetchStr(aliasdb, alias)) {
		if (streq("default", alias)) {
			rc = expand(n, aliasdb, keys, seen, "all");
		} else {
			rc = value(n, keys, alias);
		}
		goto done;
	}

	unless (hash_insertStr(seen, alias, 0)) {
		error("%s: recursive alias definition '%s'.\n", prog, alias);
		goto done;
	}

	inserted = 1;

	unless (expansion = splitLine(mval, "\r\n", 0)) {
		error("%s: no alias conent for '%s'.\n", prog, alias);
		goto done;
	}

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

		EACH_STRUCT(n->comps, c) {
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
		    (n->aliasdb = aliasdb_init(n, 0, n->tip, n->pending))) {
			return (0);
		}
		aliasdb = n->aliasdb;
	}
	return (aliasdb);
}

int
aliasdb_chkAliases(nested *n, hash *aliasdb, char ***paliases, char *cwd)
{
	int	i, reserved, errors = 0, fix = (cwd != 0);
	comp	*c;
	char	*alias, **aliases;
#ifndef	CRAZY_GLOB
	char	**addkeys = 0;
	int	j;
#endif

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
				if (fix) {
					addkeys = file2Lines(addkeys,
					    "BitKeeper/log/COMPONENTS");
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
			free(alias);
			aliases[i] = strdup(c->rootkey);
			continue;
		}

		/* Is it a glob ? */
		if (is_glob(alias)) {
#ifndef	CRAZY_GLOB
			unless (fix) {
				error( "%s: glob not allowed: %s\n",
				    prog, alias);
			}
			if (strneq(alias, "./", 2)) alias += 2;
			EACH_STRUCT_INDEX(n->comps, c, j) {
				if (c->product) continue;
				if (match_one(c->path, alias, 0)) {
					addkeys = addLine(addkeys,
					    strdup(c->rootkey));
				}
			}
			if (emptyLines(addkeys)) {
				error("%s: %s does not match any components.\n",
				    prog, alias);
				errors++;
			}
			removeLineN(aliases, i, free);
			i--;
#endif
			/* XXX: remap a relative glob to repo relative? */
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
	if (errors) {
		freeLines(addkeys, free);
	} else {
		EACH(addkeys) {
			aliases = addLine(aliases, addkeys[i]);
		}
		freeLines(addkeys, 0);
		*paliases = aliases;
	}
	return (errors);
}

private	int
chkReserved(char *alias, int fix)
{
	int	rc = 0;
	char	**wp, *w;
	char	*reserved[] = {"all", "default", "here", "there", 0};

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
 * valid == /^[a-z][-a-z0-9_+=]*$/i
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
		unless (isalnum(*p) ||
		    (*p == '_') || (*p == '+') || (*p == '-') || (*p == '=')) {
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
