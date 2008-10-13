#include "system.h"
#include "sccs.h"
#include "logging.h"
#include "ensemble.h"
#include "range.h"

typedef struct aliases {
	sccs	*cset;		/* pointer to product's ChangeSet file' */
	int	free_cset;	/* whether we need to free the sccs * above */
	repos	*comps;		/* component list as of "+" */
	hash	*aliasDB;	/* aliases db from the product */
	hash	*seen;		/* auxiliary hash for recursive resolution */
} aliases;

private hash	*aliasdb_init(void);
private int	aliasdb_add(char *alias, char **components, int commit);
private int	aliasdb_rm(char *alias, char **components, int commit);
private	char	**aliasdb_show(char *alias);
private int	alias_validName(char *name);
private int	alias_expand(char *name, aliases *aliases, hash *keys);
private int	value_expand(char *name, aliases *mdb, hash *keys);
private void	aliases_free(aliases *mdb);
private int	dir_expand(char *dir, aliases *aliases, hash *keys);
private char	*dir2key(char *dir, aliases *mdb);
private void	error(const char *fmt, ...);
private int	finish(char *comment, hash *aliasDB, int commit);
private int	verifyKey(char *key, aliases *mdb);
extern char	*prog;

/*
 * bk alias add -A<alias> <comp> ...	// create/add to a alias
 * bk alias rm -A<alias> [<comp> ...]	// remove a comp|alias
 * bk alias list			// list all aliases
 */
int
alias_main(int ac, char **av)
{
	char	*command, *p, *alias = 0;
	char	**comps = 0;
	int	c, commit = 1, i;

	unless (av[1]) {
err:		system("bk help -s alias");
		return (1);
	}
	unless (proj_product(0)) {
		fprintf(stderr, "alias: called in a non-product.\n");
		return (1);
	}
	command = av[1];
	av++, ac--;
	while ((c = getopt(ac, av, "A;C")) != -1) {
		switch (c) {
		    case 'A': alias = optarg; break;
		    case 'C': commit = 0; break;
		}
	}
	/* handle list immediately since it's easy */
	if (streq(command, "list")) {
		hash	*aliasDB;

		unless (aliasDB = aliasdb_init()) {
			fprintf(stderr, "aliases: no aliases\n");
			return (1);
		}
		EACH_HASH(aliasDB) comps = addLine(comps, aliasDB->kptr);
		sortLines(comps, 0);
		EACH(comps) printf("%s\n", comps[i]);
		freeLines(comps, 0);
		hash_free(aliasDB);
		return (0);
	}
	unless (alias) {
		fprintf(stderr, "alias: no alias specified.\n");
		goto err;
	}
	if (streq(command, "show")) {
		char	**components = 0;

		unless (components = aliasdb_show(alias)) {
			fprintf(stderr,
			    "alias: alias '%s' does not exist.\n", alias);
			return (1);
		}
		EACH(components) printf("%s\n", components[i]);
		freeLines(components, free);
		return (0);
	}
	/* gather list of components */
	while (av[optind]) {
		/* "-" has to be last */
		if (streq(av[optind], "-")) {
			while (p = fgetline(stdin)) comps = addLine(comps, p);
			break;
		}
		comps = addLine(comps, strdup(av[optind++]));
	}
	if (streq(alias, "all")) {
		fprintf(stderr,
		    "alias: reserved name \"all\" may not be changed.\n");
		goto err;
	}
	if (streq(command, "add") || streq(command, "create")) {
		if (aliasdb_add(alias, comps, commit)) return (1);
	} else if (streq(command, "rm")) {
		if (aliasdb_rm(alias, comps, commit)) return (1);
	} else {
		goto err;
	}
	return (0);
}

/*
 * Given one or more alias names, and optionally a changeset sccs*,
 * return a hash of implied root keys.
 * All names are recursively expanded if the values have names in them.
 * XXX - always imply the product?
 * XXX - does not take a rev and that's probably wrong
 */
hash *
alias_list(char **names, sccs *cset)
{
	int	i, failed = 0;
	aliases	*mdb;		/* BitKeeper/etc/aliases */
	hash	*keys = 0;	/* resulting key expansion */
	char	buf[MAXKEY];

	unless (proj_isProduct(0)) {
		error("%s: alias expansion called in a non-product.\n", prog);
		return (0);
	}
	unless (names && names[1]) {
		error("%s: alias expansion with no names?\n", prog);
		return (0);
	}
	mdb = new(aliases);
	unless (mdb->aliasDB = aliasdb_init()) {
		error("%s: can't get %s\n", prog, buf);
		return (0);
	}
	keys = hash_new(HASH_MEMHASH);
	mdb->free_cset = (cset == 0);
	mdb->cset = cset;
	mdb->seen = hash_new(HASH_MEMHASH);
	EACH(names) {
		if (alias_expand(names[i], mdb, keys)) {
			failed = 1;
			break;
		}
	}
	aliases_free(mdb);
	if (failed) {
		hash_free(keys);
		keys = 0;
	}
	return (keys);
}

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

private hash *
aliasdb_init(void)
{
	char	buf[MAXPATH];

	concat_path(buf, proj_root(proj_product(0)), ALIASES);
	if (!exists(buf) && get(buf, SILENT, "-")) {
		error("%s: no alias file found\n", prog);
		return (0);
	}
	return (hash_fromFile(0, buf));
}

private char **
aliasdb_show(char *alias)
{
	aliases	*mdb;
	char	**result = 0;
	char	*val;

	mdb = new(aliases);
	unless (mdb->aliasDB = aliasdb_init()) goto out;
	unless (val = hash_fetchStr(mdb->aliasDB, alias)) goto out;
	result = splitLine(mdb->aliasDB->vptr, "\r\n", 0);
out:	aliases_free(mdb);
	return (result);
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
	mdb = new(aliases);
	unless (mdb->aliasDB = aliasdb_init()) {
		fprintf(stderr, "%s: failed to open alias db.\n", prog);
		goto err;
	}
	EACH (components) {
		if (isKey(components[i])) {
			unless (verifyKey(components[i], mdb)) {
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
	int	i, j, found, rc = 1;
	char	*p, *comment = 0;
	aliases	*mdb;
	char	**list = 0;

	mdb = new(aliases);
	unless (mdb->aliasDB = aliasdb_init()) goto err;
	unless (hash_fetchStr(mdb->aliasDB, alias)) {
		fprintf(stderr, "%s: no such alias %s\n", prog, alias);
		goto err;
	}
	unless (components) {
		/* delete the alias itself */
		hash_deleteStr(mdb->aliasDB, alias);
		comment = aprintf("Remove %s", alias);
		goto out;
	}
	list = splitLine(mdb->aliasDB->vptr, "\r\n", 0);
	EACH (components) {
		found = 0;
		EACH_INDEX(list, j) {
			if (streq(components[i], list[j])) {
				removeLineN(list, j, free);
				found = 1;
				break;	/* or j-- if dups are expected */
			}
		}
		unless (found) {
			fprintf(stderr, "%s: %s is not part of %s, "
			    "nothing modified.\n", prog, components[i], alias);
			goto err;

		}
	}
	switch (nLines(list)) {
	    case 0:
		comment = aprintf("Delete alias %s", alias);
		hash_deleteStr(mdb->aliasDB, alias);
		break;
	    case 1:
		comment = aprintf("Modify alias %s", alias);
		hash_storeStr(mdb->aliasDB, alias, list[1]);
		break;
	    default:
		comment = aprintf("Modify alias %s", alias);
		p = joinLines("\n", list);
		hash_storeStr(mdb->aliasDB, alias, p);
		free(p);
		break;
	}
out:	rc = finish(comment, mdb->aliasDB, commit);
err:	if (comment) free(comment);
	if (list) freeLines(list, free);
	aliases_free(mdb);
	return (rc);
}

/*
 * Check that a alias name is valid
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
		hash_storeStr(keys, name, "");
	} else if (strneq(name, "./", 2)) {
		unless (dir_expand(&name[2], mdb, keys)) {
			error("%s: no match for %s\n", prog, name);
			return (1);
		}
	} else if (streq(name, ".")) {
		/* just add the product */
		hash_storeStr(keys, proj_rootkey(proj_product(0)), "");
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

	if (streq("all", name)) {
		return (value_expand("./*", mdb, keys));
	}
	unless (mval = hash_fetchStr(mdb->aliasDB, name)) {
		if (streq("default", name)) {
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
	if (mdb->free_cset && mdb->cset) sccs_free(mdb->cset);
	if (mdb->comps) ensemble_free(mdb->comps);
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
	unless (mdb->comps) {
		eopts	opts;

		bzero(&opts, sizeof(eopts));
		opts.rev = "+";
		opts.sc = mdb->cset;
		mdb->comps = ensemble_list(opts);
	}
	EACH_REPO(mdb->comps) {
		if (match_one(mdb->comps->path, dir, 0)) {
			hash_storeStr(keys, mdb->comps->rootkey, "");
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
	eopts	opts;
	char	*p;

	assert(mdb);
	unless (mdb->comps) {
		bzero(&opts, sizeof(eopts));
		opts.rev = "+";
		opts.sc = mdb->cset;
		mdb->comps = ensemble_list(opts);
	}
	dir = proj_relpath(proj_product(0), dir);
	p = ensemble_dir2key(mdb->comps, dir);
	free(dir);
	unless (p) return (0);
	return (strdup(p));
}

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

	system("bk -P edit -q " ALIASES);
	concat_path(buf, proj_root(proj_product(0)), ALIASES);
	hash_toFile(aliasDB, buf);
	sprintf(buf, "bk -P delta -qy'%s' %s", comment, ALIASES);
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
 * Verify that a given key is indeed a component
 * of this product
 */
private int
verifyKey(char *key, aliases *mdb)
{
	eopts	opts;

	assert(mdb);
	unless (mdb->comps) {
		bzero(&opts, sizeof(eopts));
		opts.rev = "+";
		opts.sc = mdb->cset;
		mdb->comps = ensemble_list(opts);
	}
	return (ensemble_find(mdb->comps, key));
}
