#include "sccs.h"
#include "nested.h"

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

private	int	dbAdd(hash *aliasdb, char *alias, char **aliases);
private	int	dbRemove(hash *aliasdb, char *alias, char **aliases);
private	int	dbWrite(hash *aliasdb, char *comment, int commit);
private	int	dbShow(nested *n, hash *aliasdb, char *cwd, char *alias,
		    int showkeys, int showpaths);
private	int	expand(nested *n, hash *db, hash *keys, hash *seen,
		    char *alias);
private	int	value(nested *n, hash *keys, char *alias);
private	int	chkReserved(char *alias, int fix);
private	int	validName(char *name);
private	comp	*findDir(nested *n, char *cwd, char *dir);
private	void	error(const char *fmt, ...);

extern char	*prog;

int	rmAlias = 0;

/*
 * bk alias [-Cfq] name comp [...]	// create a new one or overwrite (w/ -f)
 * bk alias [-Cq] -a name comp [...]	// append to existing alias
 * bk alias [-Cq] -x name comp [...]	// remove comps from an alias
 * bk alias [-Cq] -x name		// remove entire alias
 * bk alias				// list all aliases
 * bk alias [-kp] name			// show expansion, 1 per line
 */
int
alias_main(int ac, char **av)
{
	nested	*n = 0;
	hash	*aliasdb = 0;
	char	*p, *alias = 0;
	char	*rev = 0;
	char	*comment = 0;
	char	**aliases = 0;
	char	*cwd;
	int	c;
	int	reserved = 0, rc = 1;
	/* options */
	int	create = 0, append = 0, force = 0, rm = 0, list = 0;
	int	quiet = 0, commit = 1, showkeys = 0, showpaths = 0;

	cwd = strdup(proj_cwd());
	if (proj_cd2product()) {
		fprintf(stderr, "%s: called in a non-product.\n", prog);
		goto err;
	}
	while ((c = getopt(ac, av, "aCfkpqr;x")) != -1) {
		switch (c) {
		    case 'a': append = 1; break;
		    case 'C': commit = 0; break;
		    case 'f': force = 1; break;
		    case 'k': showkeys = 1; break;
		    case 'p': showpaths = 1; break;
		    case 'q': quiet = 1; break;
		    case 'r': rev = optarg; break;
		    case 'x': rm = 1; break;
		    default:
usage:			sys("bk", "help", "-s", "alias", SYS);
			goto err;
		}
	}
	/*
	 * Check options
	 */
	if (append && rm) goto usage;
	if (showkeys && showpaths) {
		fprintf(stderr, "%s: one of -k or -p but not both\n", prog);
		goto usage;
	}
	if (av[optind]) {
		alias = av[optind++];
		rmAlias = (rm && !av[optind]); // delete the whole alias
		// downcast reserved keys if not removing an alias
		unless (rmAlias) reserved = chkReserved(alias, 1);
		assert(reserved >= 0);
	}
	if (!append && !rm) {
		if (alias && av[optind]) {
			create = 1;
		} else {
			list = 1;
		}
	}
	if (append || rm || create) {
		unless (alias) {
			fprintf(stderr, "%s: no alias specified.\n", prog);
			goto usage;
		}
		unless (rmAlias || reserved || validName(alias)) {
			fprintf(stderr,
			    "%s: invalid alias name %s.\n", prog, alias);
			goto usage;
		}
		if (rev) {
			fprintf(stderr,
			    "%s: no -r when altering an alias\n", prog);
			goto usage;
		}
		if (showkeys || showpaths) {
			fprintf(stderr,
			    "%s: no -k or -p when altering an alias\n", prog);
			goto usage;
		}
		if (reserved && !streq(alias, "default")) {
			fprintf(stderr,
			    "%s: reserved name \"%s\" may not be changed.\n",
			    prog, alias);
			goto usage;
		}
	} else {
		assert(list && !av[optind]);
	}
	/*
	 * Done checking options.
	 */
	unless (n = nested_init(0, rev, 0, rev ? 0 : NESTED_PENDING)) goto err;
	unless (aliasdb = aliasdb_init(n, 0, rev, rev ? 0 : 1)) goto err;

	/* handle list immediately since it's easy */
	if (list) {
		rc = dbShow(n, aliasdb, cwd, alias, showkeys, showpaths);
		goto err;	// really, goto cleanup and leave with rc
	}

	/*
	 * the rest of the commands can have a list of args that map
	 * to aliases. For example, if user is in src/gui/tcltk and
	 * passes in arg './tcl', then it will get mapped to rootkey
	 * of the tcl repo.
	 */
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
	if (c = aliasdb_chkAliases(n, aliasdb, aliases, cwd, 1)) {
		fprintf(stderr, "%s: %d error%s processing aliases\n",
		    prog, c, (c > 1)?"s":"");
		goto err;
	}

	/*
	 * Don't overwrite unless they told us that's what they wanted.
	 */
	if (create) {
		if (hash_fetchStr(aliasdb, alias)) {
			unless (force) {
				fprintf(stderr,
				    "%s: %s exists, use -f?\n", prog, alias);
				goto err;
			}
			if (dbRemove(aliasdb, alias, 0)) goto err;
			comment = aprintf("Replace alias %s", alias);
		} else {
			comment = aprintf("Create alias %s", alias);
		}
		if (dbAdd(aliasdb, alias, aliases)) goto err;
		if (dbWrite(aliasdb, comment, commit)) goto err;
	}

	/*
	 * aliasdb_add means append if existing
	 */
	if (append) {
		unless (hash_fetchStr(aliasdb, alias)) {
			unless (force) {
				fprintf(stderr,
				    "%s: %s does not exist.\n", prog, alias);
				goto err;
			}
			comment = aprintf("Create alias %s", alias);
		} else {
			comment = aprintf("Modify alias %s", alias);
		}
		if (dbAdd(aliasdb, alias, aliases)) goto err;
		if (dbWrite(aliasdb, comment, commit)) goto err;
	}

	if (rm) {
		if (dbRemove(aliasdb, alias, aliases)) goto err;
		comment = aprintf("Delete alias %s", alias);
		if (dbWrite(aliasdb, comment, commit)) goto err;
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

hash	*
aliasdb_init(nested *n, project *p, char *rev, int pending)
{
	hash	*aliasdb = 0;
	int	total = 0, errors = 0;
	int	reserved;
	char	*path = 0;
	char	**aliases = 0;
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

	/*
	 * this is aliasdb_chkDb(), in case we need a separate entry
	 */
	if (rmAlias) goto done;	// disable checks if removing an alias
	EACH_HASH(aliasdb) {
		if (reserved = chkReserved(aliasdb->kptr, 0)) {
			if (reserved < 0) {
				fprintf(stderr,
				    "%s: bad case for key: %s\n",
				    prog, aliasdb->kptr);
				total++;
			} else unless (streq(aliasdb->kptr, "default")) {
				fprintf(stderr,
				    "%s: illegal aliasdb key: %s\n",
				    prog, aliasdb->kptr);
				total++;
			}
		} else {
			unless (validName(aliasdb->kptr)) total++;
		}
		aliases = splitLine(aliasdb->vptr, "\r\n", 0);
		if (errors = aliasdb_chkAliases(n, aliasdb, aliases, 0, 0)) {
			fprintf(stderr,
			    "%s: bad values for key: %s\n",
			    prog, aliasdb->kptr);
			total += errors;
		}
		freeLines(aliases, free);
	}
	if (total) {
		fprintf(stderr, "%s: %d error%s initializing aliasdb\n",
		    prog, total, (total > 1)?"s":"");
		hash_free(aliasdb);
		aliasdb = 0;
	}
done:
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
		fprintf(stderr, "%s: nothing to add\n", prog);
		goto err;
	}
	if (val = hash_fetchStr(aliasdb, alias)) {
		list = splitLine(val, "\r\n", 0);
	}
	EACH(aliases) {
		list = addLine(list, strdup(aliases[i]));
	}
	uniqLines(list, free);
	val = joinLines("\n", list);
	unless (hash_storeStr(aliasdb, alias, val)) {
		fprintf(stderr,
		    "%s: failed to store aliases in %s: %s\n",
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
		fprintf(stderr, "%s: no such alias %s\n", prog, alias);
		goto err;
	}
	if (aliases) {
		list = splitLine(p, "\r\n", 0);
		EACH (aliases) {
			unless (removeLine(list, aliases[i], free)) {
				fprintf(stderr, "%s: %s is not part of %s\n",
				    prog, aliases[i], alias);
				errors++;
			}
		}
	}
	if (errors) {
		fprintf(stderr, "%s: %s not modified\n", prog, alias);
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
dbWrite(hash *aliasdb, char *comment, int commit)
{
	int	ret = 0;
	char	*tmpfile;
	char	buf[MAXPATH];

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
	if (chkReserved(alias, 0) < 0) goto err;

	unless (aliasdb) {
		unless (n->aliasdb ||
		    (n->aliasdb = aliasdb_init(n, 0, n->tip, n->pending))) {
			goto err;
		}
		aliasdb = n->aliasdb;
	}

	/*
	 * here and there can not be used inside an alias
	 */
	if (streq("here", alias) || streq("there", alias)) {
		EACH_STRUCT(n->comps, c) {
			if (c->present) comps = addLine(comps, c);
		}
		return (comps);
	}

	keys = hash_new(HASH_MEMHASH);
	seen = hash_new(HASH_MEMHASH);
	/* resursive expansion fills 'keys' hash with rootkeys */
	if (expand(n, aliasdb, keys, seen, alias)) {
		fprintf(stderr,
		    "%s: expansion of alias %s failed\n", prog, alias);
		goto err;
	}
	/* output subset of n->comps in n->comps order */
	EACH_STRUCT(n->comps, c) {
		if (hash_fetchStr(keys, c->rootkey)) {
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
dbShow(nested *n, hash *aliasdb, char *cwd, char *alias,
    int showkeys, int showpaths)
{
	char	**items = 0, **comps;
	char	*val;
	comp	*c;
	int	i, rc = 1;

	unless (alias) {
		/*
		 * print all the alias keys
		 * XXX: list reserved words?
		 */
		EACH_HASH(aliasdb) {
			items = addLine(items, strdup(aliasdb->kptr));
		}
		goto print;
	}

	unless (showkeys || showpaths) {
		/*
		 * print the val entry from the db; fake default if not there
		 */
		val = hash_fetchStr(aliasdb, alias);
		if (!val && streq(alias, "default")) {
			val = "all";
		} else if (!val) {
			fprintf(stderr, "%s: no alias: %s\n", prog, alias);
			goto err;
		}
		items = splitLine(val, "\r\n", 0);
		goto print;
	}

	/* if showing keys or paths, the expand any key or value item */

	if (i = chkReserved(alias, 1)) {
		/* 
		 * This is here to allow keys like 'here' which are
		 * not allowed in the value string checked in the else block
		 */
		assert(i > 0);	// when fixing case, no errors can happen
		val = strdup(alias);
	} else {
		/* use lines arry to fix up a single item */
		items = addLine(0, strdup(alias));
		if (aliasdb_chkAliases(n, aliasdb, items, cwd, 1)) goto err;
		val = items[1];	/* firstLine(items) */
		freeLines(items, 0);
		items = 0;
	}
	comps = aliasdb_expandOne(n, aliasdb, val);
	free(val);
	unless (comps) goto err;

	EACH_STRUCT(comps, c) {
		items = addLine(items,
		    strdup(showkeys ? c->rootkey : c->path));
	}
	freeLines(comps, 0);

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
	comp	*c;
	int	i;

	if (isKey(alias)) {
		hash_insertStr(keys, alias, 0);
	} else if (is_glob(alias)) {
		/* XXX: is this needed */
		if (streq(alias, "./")) alias += 2;

		EACH_STRUCT(n->comps, c) {
			if (match_one(c->path, alias, 0)) {
				hash_insertStr(keys, c->rootkey, 0);
			}
		}
	} else {
		fprintf(stderr,
		    "%s: alias value problem with: %s\n", prog, alias);
		return (1);
	}
	return (0);
}

/*
 * Utilities from here on out
 */

int
aliasdb_chkAliases(nested *n, hash *aliasdb, char **aliases,
    char *cwd, int fix)
{
	int	i, reserved, errors = 0;
	comp	*c;
	char	*alias;

	EACH(aliases) {
		alias = aliases[i];

		if (reserved = chkReserved(alias, fix)) {
			if (reserved < 0) {
				/* case problem */
				errors++;
			}
			else if (streq(alias, "here") ||
			    streq(alias, "there")) {
				fprintf(stderr,
				    "%s: not allowed as value: %s\n",
				    prog, alias);
				errors++;
			}
			continue;
		}
		/* see if alias is in the aliasdb and has a validName */
		if (hash_fetchStr(aliasdb, alias)) {
			unless (validName(alias)) {
				fprintf(stderr,
				    "%s: invalid alias name %s\n",
				    prog, alias);
				errors++;
			}
			continue;
		}

		if (isKey(alias)) {
			/* is it a normal rootkey? */
			if (strchr(alias, '|')) {
				/* that is in the nested collection? */
				if (nested_findKey(n, alias)) continue;

				/* rootkey but not found */
				fprintf(stderr,
				    "%s: not a component rootkey: %s\n",
				    prog, alias);
				errors++;
				continue;
			}

			/* it is an md5 key */
			unless (fix) {
				fprintf(stderr,
				    "%s: illegal md5key: %s\n", prog, alias);
				errors++;
				continue;
			}

			/* try replacing it with a rootkey */
			unless (c = nested_findMD5(n, alias)) {
				fprintf(stderr,
				    "%s: not a component md5key: %s\n",
				    prog, alias);
				errors++;
				continue;
			}

			/* okay, we have a rootkey to replace it */
			debug((stderr, "%s was md5, now rootkey\n", alias));
			free(alias);
			aliases[i] = strdup(c->rootkey);
			continue;
		} 

		/* Is it a glob ? */
		if (is_glob(alias)) {
			/* XXX: remap a relative glob to repo relative? */
			continue;
		}

		/*
		 * if this is command line, is it a path which points
		 * to a repository?
		 */
		if (fix && (c = findDir(n, cwd, alias))) {
			free(alias);
			aliases[i] = strdup(c->rootkey);
			continue;
		}

		fprintf(stderr, "%s: %s must be either a glob, key, "
		    "or alias.\n", prog, alias);
		errors++;
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
			fprintf(stderr,
			    "%s: alias not downcasted: %s\n", prog, alias);
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
	int	valid = 1;

	unless (isalpha(name[0])) return (0);
	for (p = name+1; *p; p++) {
		unless (isalnum(*p) ||
		    (*p == '_') || (*p == '+') || (*p == '-') || (*p == '=')) {
			valid = 0;
		}
	}
	return (valid);
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
 * Return true if the pathname is a is a component relative to
 * the cwd.  Only present components will be detected.
 * We could not if a path is a missing component, but this function
 * won't tell you that.
 */
int
isComponent(char *path)
{
	int	ret;
	project	*p = 0;
	char	buf[MAXPATH];

	sprintf(buf, "%s/%s", path, BKROOT);
	ret = exists(buf) && (p = proj_init(buf)) && proj_isComponent(p);
	if (p) proj_free(p);
	return (ret);
}

/* XXX should be exported, moved, and given a more unique name */
private	void
error(const char *fmt, ...)
{
	va_list	ap;
	char	*retval;

	va_start(ap, fmt);
	if (vasprintf(&retval, fmt, ap) < 0) retval = 0;
	va_end(ap);
	if (retval) {
		if (getenv("_BK_IN_BKD")) {
			out("ERROR-");
			out(retval);
		} else {
			fputs(retval, stderr);
		}
		free(retval);
	}
}
