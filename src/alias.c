#include "sccs.h"
#include "nested.h"

typedef struct {
	repos	*comps;		/* component list as of rev */
	hash	*aliasDB;	/* aliases db from the product */
	hash	*seen;		/* auxiliary hash for recursive resolution */
} aliases;

private aliases	*aliases_init(sccs *cset, char *rev, u32 flags);
private hash	*aliasdb_init(char *rev, u32 flags);
private int	aliasdb_add(char *alias, char **components, int commit);
private int	aliasdb_rm(char *alias, char **components, int commit);
private	char	**aliasdb_show(char *alias, int present, int paths);
private int	alias_validName(char *name);
private int	alias_expand(char *name, aliases *aliases, hash *keys);
private int	value_expand(char *name, aliases *mdb, hash *keys);
private void	aliases_free(aliases *mdb);
private int	dir_expand(char *dir, aliases *aliases, hash *keys);
private char	*dir2key(char *dir, aliases *mdb);
private void	error(const char *fmt, ...);
private int	finish(char *comment, hash *aliasDB, int commit);
private	int	do_list(char *alias, int here, int paths);
extern char	*prog;

/*
 * bk alias [-Cfq] name comp [...]	// create a new one or overwrite (w/ -f)
 * bk alias [-Cq] -a name comp [...]	// append to existing alias
 * bk alias [-Cq] -r name comp [...]	// remove comps from an alias
 * bk alias [-Cq] -r name		// remove entire alias
 * bk alias				// list all aliases
 * bk alias [-k] name			// show expansion, as paths, 1 per line
 */
int
alias_main(int ac, char **av)
{
	char	*p, *alias = 0;
	char	**comps = 0, **tmp = 0;
	int	c;
	int	create = 0, append = 0, force = 0, rm = 0, list = 0;
	int	commit = 1, paths = 1, here = 0, rc = 1;

	unless (start_cwd) start_cwd = strdup(proj_cwd());
	if (proj_cd2product()) {
		fprintf(stderr, "alias: called in a non-product.\n");
		goto err;
	}
	while ((c = getopt(ac, av, "aCfhkr")) != -1) {
		switch (c) {
		    case 'a': append = 1; break;
		    case 'C': commit = 0; break;
		    case 'f': force = 1; break;
		    case 'k': paths = 0; break;
		    case 'h': here = 1; break;
		    case 'r': rm = 1; break;
		    default:
usage:			sys("bk", "help", "-s", "alias", SYS);
			goto err;
		}
	}
	if (av[optind]) alias = av[optind++];

	if (append && rm) goto usage;
	if (!append && !rm) {
		if (alias && av[optind]) {
			create = 1;
		} else {
			list = 1;
		}
	}
	if (append || rm || create) {
		unless (alias) {
			fprintf(stderr, "alias: no alias specified.\n");
			goto usage;
		}
		if ((strcasecmp(alias, "all") == 0) ||
		    (strcasecmp(alias, "here") == 0) ||
		    (strcasecmp(alias, "there") == 0)) {
			fprintf(stderr,
			    "alias: reserved name \"%s\" may not be changed.\n",
			    alias);
			goto usage;
		}
	} else {
		assert(list);
		if (av[optind]) goto usage;
	}

	/* handle list immediately since it's easy */
	if (list) return (do_list(alias, here, paths));

	/* the rest of the commands can have a list of components */
	for (; av[optind]; ++optind) {
		if (streq(av[optind], "-") && !av[optind+1]) {
			while (p = fgetline(stdin)) {
				comps = addLine(comps, strdup(p));
			}
		} else {
			comps = addLine(comps, strdup(av[optind]));
		}
	}

	/*
	 * Don't overwrite unless they told us that's what they wanted.
	 */
	if (create) {
		if ((tmp = aliasdb_show(alias, 0, 0)) && (tmp != INVALID)) {
			freeLines(tmp, free);
			unless (force) {
				fprintf(stderr,
				    "alias: %s exists, use -f?\n", alias);
				goto err;
			}
			/* XXX: save and restore? Atomic replace? */
			if (aliasdb_rm(alias, 0, 0)) goto err;
		}
		if (aliasdb_add(alias, comps, commit)) goto err;
	}

	/*
	 * aliasdb_add means append if existing
	 */
	if (append) {
		unless (force ||
		    ((tmp = aliasdb_show(alias, 0, 0)) && (tmp != INVALID))) {
			fprintf(stderr, "alias: %s does not exist.\n", alias);
			goto err;
		}
		freeLines(tmp, free);
		if (aliasdb_add(alias, comps, commit)) goto err;
	}

	if (rm) {
		if (aliasdb_rm(alias, comps, commit)) goto err;
	}
	rc = 0;
err:
	freeLines(comps, free);
	return (rc);
}

private aliases *
aliases_init(sccs *cset, char *rev, u32 flags)
{
	aliases	*mdb;
	eopts	opts = {0};

	mdb = new(aliases);
	unless (mdb->aliasDB = aliasdb_init(rev, flags)) {
		error("%s: could not initialize aliases (rev = %s)\n",
		    prog, rev ? rev : "none");
		free(mdb);
		return (0);
	}
	mdb->seen = hash_new(HASH_MEMHASH);
	opts.rev = rev;
	opts.sc = cset;
	opts.pending = ((flags & ALIAS_PENDING) != 0);
	mdb->comps = nested_list(opts);
	return (mdb);
}

/*
 * Given one or more alias names, and optionally a changeset sccs*,
 * return a hash of implied root keys.
 * All names are recursively expanded if the values have names in them.
 *
 * If 'present' is given, the alias expansion will fail if any
 * component in the expanded aliases is not present. The error message
 * will say which alias expansion failed.
 */
hash *
alias_hash(char **names, sccs *cset, char *rev, u32 flags)
{
	int	i, failed = 0;
	aliases	*mdb;		/* BitKeeper/etc/aliases */
	hash	*k, *keys = 0;	/* resulting key expansion */

	unless (proj_isProduct(0)) {
		error("%s: alias expansion called in a non-product.\n", prog);
		return (0);
	}
	if (emptyLines(names)) {
		error("%s: alias expansion with no names?\n", prog);
		return (0);
	}
	unless (mdb = aliases_init(cset, rev, flags)) return (0);

	keys = hash_new(HASH_MEMHASH);

	EACH(names) {
		k = hash_new(HASH_MEMHASH);
		if (alias_expand(names[i], mdb, k)) {
			failed = 1;
			hash_free(k);
			continue;
		}
		EACH_HASH(k) {
			char	*rk = (char*)k->kptr;
			int	here = *(u8*)k->vptr;

			unless (nested_find(mdb->comps, rk)) {
				TRACE("WTF? %s", rk);
			}
			if ((flags & ALIAS_HERE) && !here) {
				error("%s: error expanding alias '%s' because "
				    "%s is not present\n", prog,
				    names[i], mdb->comps->path);
				failed = 1;
				continue;
			}
			TRACE("%s", mdb->comps->path);
			hash_storeStr(keys, rk, "");
		}
		hash_free(k);
	}
	aliases_free(mdb);
	if (failed) {
		hash_free(keys);
		keys = 0;
		return (0);
	}
	return (keys);
}

/*
 * Take a single alias and return the md5 of the sorted expansion.
 */
char *
alias_md5(char *name, sccs *cset, char *rev, u32 flags)
{
	char	**names = 0;
	char	*joined, *md5;
	hash	*h;

	if (isKey(name)) return (hashstr(name, strlen(name)));
	names = addLine(0, name);
	h = alias_hash(names, cset, rev, flags);
	freeLines(names, 0);
	names = 0;
	EACH_HASH(h) names = addLine(names, h->kptr);
	sortLines(names, 0);
	joined = joinLines("\n", names);
	freeLines(names, 0);
	hash_free(h);
	md5 = hashstr(joined, strlen(joined));
	free(joined);
	return (md5);
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

private	int
do_list(char *alias, int here, int paths)
{
	hash	*h;
	aliases	*mdb;
	char	**comps = 0;
	int	i, rc = 1;

	if (alias) {
		unless (comps = aliasdb_show(alias, here, paths)) {
			fprintf(stderr,
			    "alias: '%s' not found\n", alias);
			goto err;
		}
		if (comps == INVALID) goto err;
	} else if (here) {
		unless (comps =
		    file2Lines(0, "BitKeeper/log/COMPONENTS")) {
			fprintf(stderr, "alias: no local aliases\n");
			goto err;
		}
		unless (h =
		    alias_hash(comps, 0, 0, (ALIAS_HERE|ALIAS_PENDING))) {
			goto done;
		}
		hash_free(h);
	} else {
		unless (mdb = aliases_init(0, 0, ALIAS_PENDING)) {
			fprintf(stderr, "alias: no aliases\n");
			goto err;
		}
		EACH_HASH(mdb->aliasDB) {
			comps = addLine(comps,
			    strdup(mdb->aliasDB->kptr));
		}
		aliases_free(mdb);
	}
	sortLines(comps, 0);
	EACH(comps) printf("%s\n", comps[i]);
done:	rc = 0;
err:	freeLines(comps, free);
	return (rc);
}

private hash *
aliasdb_init(nested *n)
{
	hash	*ret = 0;
	char	*p;
	sccs	*s;
	char	*csetrev = 0;
	char	buf[MAXPATH];
	char	tmp[MAXPATH];

	concat_path(buf, proj_root(proj_product(0)), ALIASES);
	p = name2sccs(buf);
	if (s = sccs_init(p, INIT_MUSTEXIST)) {
		if (n->pending) {
			assert(!rev);
		} else {
			csetrev = aprintf("@%s", n->rev ? n->rev : "+");
		}
		if (n->pending && HAS_GFILE(s)) {
			ret = hash_fromFile(0, s->gfile);
		} else {
			bktmp(tmp, "aliasdb");
			if (!sccs_get(s, csetrev, 0,0,0, SILENT|PRINT, tmp)) {
				ret = hash_fromFile(0, tmp);
			}
			unlink(tmp);
		}
		if (csetrev) free(csetrev);
		sccs_free(s);
	} else if (flags & ALIAS_PENDING) {
		ret = hash_fromFile(0, buf); /* may have gfile */
	}
	free(p);
	return (ret);
}

private char **
aliasdb_show(char *alias, int present, int paths)
{
	nest	*n = 0;
	comp	*c;
	char	**list = 0;

	n = nest_init(0, 0, 0, NESTED_PENDING);
	list = addLine(0, alias);
	// XXX: handle errors
	(void)nest_filterAlias(n, list);
	freeLines(list, 0);
	list = 0;

	EACH_STRUCT(n->comps, c) {
		unless (c->nlink) continue;
		if (present && !c->present) {
			freeLines(result, free);
			list = INVALID;
			goto out;
		}
		if (paths) {
			list = addLines(list, strdup(c->path));
		} else {
			list = addLines(list, strdup(c->rootkey));
		}
	}

out:	nested_free(n);
	return (list);
}

/*
 * Add components to alias 'alias' in the aliases db,
 * creating alias if it doesn't already exist
 */
private int
aliasdb_add(char *alias, char **components, int commit)
{
	int	i;
	int	rc = 1, errors = 0;
	char	*comment = 0, *p = 0;
	char	**list = 0, **val = 0;
	char	*key;
	aliases	*mdb;

	unless (alias_validName(alias)) {
		fprintf(stderr, "%s: invalid alias name %s.\n", prog, alias);
		return (1);
	}
	unless (mdb = aliases_init(0, 0, ALIAS_PENDING)) return (1);
	EACH (components) {
		if (isKey(components[i])) {
			unless (nested_find(mdb->comps, components[i])) {
				fprintf(stderr, "%s: not a component key: "
				    "'%s'\n", prog, components[i]);
				errors++;
				continue;
			}
			list = addLine(list, strdup(components[i]));
		} else if (hash_fetchStr(mdb->aliasDB, components[i])) {
			/* alias name, just store it */
			list = addLine(list, strdup(components[i]));
		} else if (key = dir2key(components[i], mdb)) {
			list = addLine(list, key);
		} else if (is_glob(components[i])) {
			/* glob, just store it */
			/* XXX - should allow escapes */
			list = addLine(list, strdup(components[i]));
		} else {
			fprintf(stderr, "%s: %s must be either a glob, key, "
			    "alias, or component.\n", prog, components[i]);
			errors++;
			continue;
		}
	}
	if (errors) {
		fprintf(stderr, "%s: %d error%s processing aliases\n",
		    prog, errors, (errors > 1)?"s":"");
		goto err;
	}
	unless (nLines(list)) {
		fprintf(stderr, "%s: nothing to add\n", prog);
		goto err;
	}
	unless (hash_fetchStr(mdb->aliasDB, alias)) {
		comment = aprintf("Create alias %s", alias);
	} else {
		comment = aprintf("Modify alias %s", alias);
		val = splitLine(mdb->aliasDB->vptr, "\r\n", 0);
	}
	EACH (list) val = addLine(val, strdup(list[i]));
	uniqLines(val, free);
	p = joinLines("\n", val);
	hash_storeStr(mdb->aliasDB, alias, p);
	rc = finish(comment, mdb->aliasDB, commit);
err:	aliases_free(mdb);
	if (comment) free(comment);
	if (list) freeLines(list, free);
	return (rc);
}

/*
 * Remove components from alias. If components == 0
 * then the alias itself is removed.
 */
private int
aliasdb_rm(char *alias, char **components, int commit)
{
	int	i, rc = 1;
	char	*p, *comment = 0;
	aliases	*mdb;
	char	**list = 0;

	unless (mdb = aliases_init(0, 0, ALIAS_PENDING)) return (1);
	unless (hash_fetchStr(mdb->aliasDB, alias)) {
		fprintf(stderr, "%s: no such alias %s\n", prog, alias);
		goto err;
	}
	if (components) {
		list = splitLine(mdb->aliasDB->vptr, "\r\n", 0);
		EACH (components) {
			unless (removeLine(list, components[i], free)) {
				fprintf(stderr, "%s: %s is not part of %s, "
				    "nothing modified.\n",
				    prog, components[i], alias);
				goto err;
			}
		}
	} else {
		/* no components deletes alias */
		list = 0;
	}
	if (nLines(list) > 0) {
		comment = aprintf("Modify alias %s", alias);
		p = joinLines("\n", list);
		hash_storeStr(mdb->aliasDB, alias, p);
		free(p);
	} else {
		comment = aprintf("Delete alias %s", alias);
		hash_deleteStr(mdb->aliasDB, alias);
	}
	rc = finish(comment, mdb->aliasDB, commit);
err:	if (comment) free(comment);
	if (list) freeLines(list, free);
	aliases_free(mdb);
	return (rc);
}

/*
 * Check that a alias name is valid
 * valid == /^[a-z][-a-z0-9_+=]*$/i
 */
private int
alias_validName(char *name)
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
 * Expand a value of the alias db into its keys, note that this doesn't handle
 * alias names. See alias_expand for that.
 */
private int
value_expand(char *name, aliases *mdb, hash *keys)
{
	if (isKey(name)) {
		unless (nested_find(mdb->comps, name)) {
			error("%s: key '%s' is not a component\n", prog, name);
			return (1);
		}
		hash_store(keys, name, strlen(name)+1,
		    &mdb->comps->present, sizeof(mdb->comps->present));
	} else if (strneq(name, "./", 2)) {
		unless (dir_expand(&name[2], mdb, keys)) {
			error("%s: no match for %s\n", prog, name);
			return (1);
		}
	} else if (streq(name, ".")) {
		char	*p;
		u8	u;

		p = proj_rootkey(proj_product(0));
		u = 1;
		/* just add the product (which is always present)*/
		hash_store(keys, p, strlen(p)+1, &u, sizeof(u));
	} else {
		error("%s: could not find alias '%s'\n", prog, name);
		return (1);
	}
	return (0);
}

/*
 * Expand a single alias into its keys and put them
 * in the keys hash.
 */
private int
alias_expand(char *name, aliases *mdb, hash *keys)
{
	char	*mval;
	char	**expansion = 0;
	int	i, rc = 1;

	if (strcasecmp("all", name) == 0) {
		EACH_REPO(mdb->comps) {
			hash_store(keys,
			    mdb->comps->rootkey, strlen(mdb->comps->rootkey)+1,
			    &mdb->comps->present, sizeof(mdb->comps->present));
		}
		return (0);
	}
	if ((strcasecmp("here", name)==0) || (strcasecmp("there", name)==0)) {
		EACH_REPO(mdb->comps) {
			if (mdb->comps->present) {
				hash_store(keys,
				    mdb->comps->rootkey,
				    strlen(mdb->comps->rootkey)+1,
				    &mdb->comps->present,
				    sizeof(mdb->comps->present));
			}
		}
		return (0);
	}
	unless (mval = hash_fetchStr(mdb->aliasDB, name)) {
		if (strcasecmp("default", name) == 0) {
			return (alias_expand("all", mdb, keys));
		}
		return (value_expand(name, mdb, keys));
		/* not a alias name */
	}
	unless (hash_insertStr(mdb->seen, name, "")) {
		error("%s: recursive alias definition '%s'.\n",
		    prog, name);
		return (1);
	}
	unless (expansion = splitLine(mval, "\r\n", 0)) goto out;
	EACH(expansion) {
		if (alias_expand(expansion[i], mdb, keys)) goto out;
	}
	rc = 0;
out:	hash_deleteStr(mdb->seen, name);
	if (expansion) freeLines(expansion, free);
	return (rc);
}

private void
aliases_free(aliases *mdb)
{
	assert(mdb);
	if (mdb->comps) nested_free(mdb->comps);
	if (mdb->aliasDB) hash_free(mdb->aliasDB);
	if (mdb->seen) hash_free(mdb->seen);
	free(mdb);
}
/*
 * Get a list of keys that match a dir glob.
 */
private int
dir_expand(char *dir, aliases *mdb, hash *keys)
{
	int	rc = 0;

	EACH_REPO(mdb->comps) {
		if (match_one(mdb->comps->path, dir, 0)) {
			hash_store(keys,
			    mdb->comps->rootkey, strlen(mdb->comps->rootkey)+1,
			    &mdb->comps->present,
			    sizeof(mdb->comps->present));
			rc = 1;
		}
	}
	return (rc);
}

/*
 * Get the rootkey of a directory and verify
 * that it is indeed a component of this product
 */
private char *
dir2key(char *dir, aliases *mdb)
{
	char	*p = 0;

	/* start_cwd idiom lifted from bkd_client:nfs_parse() */
	if (start_cwd && !IsFullPath(dir)) {
		dir = p = aprintf("%s/%s", start_cwd, dir);
	}
	dir = proj_relpath(0, dir);
	if (p) free(p);
	p = nested_dir2key(mdb->comps, dir);
	free(dir);
	unless (p) return (0);
	return (strdup(p));
}

/* XXX should be exported, moved, and given a more unique name */
private void
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

private int
finish(char *comment, hash *aliasDB, int commit)
{
	int	ret = 0;
	char	*tmpfile;
	char	buf[MAXPATH];

	(void)system("bk -P edit -q " ALIASES);
	concat_path(buf, proj_root(proj_product(0)), ALIASES);
	hash_toFile(aliasDB, buf);
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
