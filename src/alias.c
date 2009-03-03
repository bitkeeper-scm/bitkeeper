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
private	int	value(nested *n, hash *keys, char *atom);
private	char	*chkAlias(nested *n, hash *aliasdb, char *cwd,
		    int fix, char *alias);
private	char	*chkAtom(nested *n, char *cwd, int fix, char *atom);
private	int	validName(char *name);
private	comp	*findDir(nested *n, char *cwd, char *dir);
private	void	error(const char *fmt, ...);

extern char	*prog;

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
	int	c, errors;
	int	reserved = 0, rc = 1;
	/* options */
	int	create = 0, append = 0, force = 0, rm = 0, list = 0;
	int	commit = 1, showkeys = 0, showpaths = 0;

	cwd = strdup(proj_cwd());
	if (proj_cd2product()) {
		fprintf(stderr, "alias: called in a non-product.\n");
		goto err;
	}
	while ((c = getopt(ac, av, "aCfkprx")) != -1) {
		switch (c) {
		    case 'a': append = 1; break;
		    case 'C': commit = 0; break;
		    case 'f': force = 1; break;
		    case 'k': showkeys = 1; break;
		    case 'p': showpaths = 1; break;
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
		if (strieq(alias, "all") ||
		    strieq(alias, "here") ||
		    strieq(alias, "there")) {
		    	reserved = 1;
		}
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
		if (reserved) {
			fprintf(stderr,
			    "%s: reserved name \"%s\" may not be changed.\n",
			    prog, alias);
			goto usage;
		}
	} else {
		assert(list && !av[optind]);
		if (reserved && !showpaths && !showkeys) {
			fprintf(stderr,
			    "%s: Use options -k or -p to list contents "
			    "of reserved alias name \"%s\" .\n",
			    prog, alias);
			goto usage;
		}
	}
	unless (list && alias && (showkeys || showpaths)) {
		unless (validName(alias)) {
			fprintf(stderr,
			    "%s: invalid alias name %s.\n", prog, alias);
			goto usage;
		}
	}
	/*
	 * Done checking options.
	 */
	unless (n = nested_init(0, rev, 0, rev ? 0 : NESTED_PENDING)) goto err;
	unless (aliasdb = aliasdb_init(0, rev, rev ? 0 : 1)) goto err;

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
	errors = 0;
	for (; (p = av[optind]); ++optind) {
		if (streq(p, "-") && !av[optind+1]) break; // last is '-'

		unless (p = chkAlias(n, aliasdb, cwd, 1, p)) {
			errors++;
		} else {
			aliases = addLine(aliases, strdup(p));
		}
	}
	/* if last is '-', then fetch params from stdin */
	if (p) {
		while (p = fgetline(stdin)) {
			unless (p = chkAlias(n, aliasdb, cwd, 1, p)) {
				errors++;
			} else {
				aliases = addLine(aliases, strdup(p));
			}
		}
	}
	if (errors) {
		fprintf(stderr, "%s: %d error%s processing aliases\n",
		    prog, errors, (errors > 1)?"s":"");
		goto err;
	}

	/*
	 * Don't overwrite unless they told us that's what they wanted.
	 */
	if (create) {
		if (hash_fetchStr(aliasdb, alias)) {
			unless (force) {
				fprintf(stderr,
				    "alias: %s exists, use -f?\n", alias);
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
aliasdb_init(project *p, char *rev, int pending)
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
		return (INVALID);
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
	EACH(aliases) list = addLine(list, strdup(aliases[i]));
	uniqLines(list, free);
	val = joinLines("\n", list);
	if (hash_storeStr(aliasdb, alias, val)) {
		fprintf(stderr,
		    "%s: failed to store aliases in %s\n", prog, alias);
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
	unless (aliasdb) {
		unless (n->aliasdb ||
		    (n->aliasdb = aliasdb_init(0, n->tip, n->pending))) {
			goto err;
		}
		aliasdb = n->aliasdb;
	}
	/*
	 * here and there can not be used inside an alias
	 */
	if (strieq("here", alias) || strieq("there", alias)) {
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

	if (alias) {
		unless (showkeys || showpaths) {
			unless (val = hash_fetchStr(aliasdb, alias)) {
				fprintf(stderr, "%s: no alias %s\n", alias);
				goto err;
			}
			items = splitLine(val, "\r\n", 0);
		} else {
			comps = aliasdb_expandOne(n, aliasdb, alias);
			if (comps == INVALID) goto err;
			EACH_STRUCT(comps, c) {
				items = addLine(items,
				    strdup(showkeys ? c->rootkey : c->path));
			}
			freeLines(comps, 0);
		}
	} else {
		EACH_HASH(aliasdb) {
			items = addLine(items, strdup(aliasdb->kptr));
		}
	}
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
	int	i, rc = 0;

	assert(n && aliasdb && keys && seen && alias);

	if (strieq("here", alias) || strieq("there", alias)) {
		error("%s: %s not allowed in an alias definition\n",
		    prog, alias);
		rc = 1;
	} else if (strieq("all", alias)) {
		EACH_STRUCT(n->comps, c) {
			hash_insertStr(keys, c->rootkey, 0);
		}
	} else unless (mval = hash_fetchStr(aliasdb, alias)) {
		if (strieq("default", alias)) {
			rc = expand(n, aliasdb, keys, seen, "all");
		} else {
			rc = value(n, keys, alias);
		}
	} else unless (hash_insertStr(seen, alias, 0)) {
		error("%s: recursive alias definition '%s'.\n", prog, alias);
		rc = 1;
	} else unless (expansion = splitLine(mval, "\r\n", 0)) {
		error("%s: no alias conent for '%s'.\n", prog, alias);
		rc = 1;
	} else {
		EACH(expansion) { /* build up keys */
			if (expand(n, aliasdb, keys, seen, expansion[i])) {
				rc = 1;
			}
		}
	}

	hash_deleteStr(seen, alias);
	freeLines(expansion, free);
	return (rc);
}

/*
 * Expand a value of the alias db into its keys, note that this doesn't handle
 * alias names. See expand() for that.
 */
private	int
value(nested *n, hash *keys, char *atom)
{
	comp	*c;
	int	i;

	unless (atom = chkAtom(n, 0, 0, atom)) return (1);

	if (isKey(atom)) {
		hash_insertStr(keys, atom, 0);
	} else {
		/* XXX: is this needed */
		if (streq(atom, "./")) atom += 2;

		EACH_STRUCT(n->comps, c) {
			if (match_one(c->path, atom, 0)) {
				hash_insertStr(keys, c->rootkey, 0);
			}
		}
	}
	return (0);
}

/*
 * Utilities from here on out
 */

private	char	*
chkAlias(nested *n, hash *aliasdb, char *cwd, int fix, char *alias)
{
	if (strieq(alias, "default") ||
	    strieq(alias, "all") ||
	    hash_fetchStr(aliasdb, alias)) {
		if (validName(alias)) return (alias);
		fprintf(stderr, "%s: invalid alias name %s\n", prog, alias);
		return(0);
	}
	return (chkAtom(n, cwd, fix, alias));
}

/*
 * aliases are made up of aliases and atoms
 * This checks atoms. If the atom is okay, return 0
 * If atom maps to something else (rootkey), return strdup of key
 */

private	char	*
chkAtom(nested *n, char *cwd, int fix, char *atom)
{
	comp	*c;

	if (isKey(atom)) {
		unless (strchr(atom, '|')) {
			unless (fix) {
				fprintf(stderr,
				    "%s: illegal md5key atom: '%s'\n",
				    prog, atom);
				atom = 0;
			} else unless (c = nested_findMD5(n, atom)) {
				fprintf(stderr,
				    "%s: not a component md5key: '%s'\n",
				    prog, atom);
				atom = 0;
			} else {
				debug((stderr,
				    "%s was md5, now rootkey\n", atom));
				atom = c->rootkey;
			}
		} else unless (nested_findKey(n, atom)) {
			fprintf(stderr,
			    "%s: not a component key: '%s'\n", prog, atom);
			atom = 0;
		}
	} else if (is_glob(atom)) {
		/* glob is okay */
	} else if (fix && (c = findDir(n, cwd, atom))) {
		/* directory is a repo, return key; map '.' to product */
		atom = c->rootkey;
	} else {
		fprintf(stderr, "%s: %s must be either a glob, key, "
		    "or alias.\n", prog, atom);
		atom = 0;
	}
	return (atom);
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
