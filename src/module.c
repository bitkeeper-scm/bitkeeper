#include "system.h"
#include "sccs.h"
#include "logging.h"
#include "ensemble.h"
#include "range.h"

typedef struct modules {
	sccs	*cset;		/* pointer to product's ChangeSet file' */
	int	free_cset;	/* whether we need to free the sccs * above */
	repos	*comps;		/* component list as of "+" */
	hash	*moduleDB;	/* modules db from the product */
	hash	*seen;		/* auxiliary hash for recursive resolution */
} modules;

private hash	*moduledb_init(void);
private int	moduledb_add(char *module, char **components, int commit);
private int	moduledb_rm(char *module, char **components, int commit);
private	char	**moduledb_show(char *module);
private int	module_validName(char *name);
private int	module_expand(char *name, modules *modules, hash *keys);
private int	value_expand(char *name, modules *mdb, hash *keys);
private void	modules_free(modules *mdb);
private int	dir_expand(char *dir, modules *modules, hash *keys);
private char	*dir2key(char *dir, modules *mdb);
private void	error(const char *fmt, ...);
private int	finish(char *comment, hash *moduleDB, int commit);
private int	verifyKey(char *key, modules *mdb);
extern char	*prog;

/*
 * bk module add -M<module> <comp> ...	// create/add to a module
 * bk module rm -M<module> [<comp> ...]	// remove a comp|module
 * bk module list			// list all modules
 */
int
module_main(int ac, char **av)
{
	char	*command, *p, *module = 0;
	char	**comps = 0;
	int	c, commit = 1, i;

	unless (av[1]) {
err:		system("bk help -s module");
		return (1);
	}
	if (proj_cd2product()) {
		fprintf(stderr, "module: called in a non-product.\n");
		return (1);
	}
	command = av[1];
	av++, ac--;
	while ((c = getopt(ac, av, "CM;")) != -1) {
		switch (c) {
		    case 'C': commit = 0; break;
		    case 'M': module = optarg; break;
		}
	}
	/* handle list immediately since it's easy */
	if (streq(command, "list")) {
		hash	*moduleDB;

		unless (moduleDB = moduledb_init()) {
			fprintf(stderr, "modules: no modules\n");
			return (1);
		}
		EACH_HASH(moduleDB) comps = addLine(comps, moduleDB->kptr);
		sortLines(comps, 0);
		EACH(comps) printf("%s\n", comps[i]);
		freeLines(comps, 0);
		hash_free(moduleDB);
		return (0);
	}
	unless (module) {
		fprintf(stderr, "module: no module specified.\n");
		goto err;
	}
	if (streq(command, "show")) {
		char	**components = 0;

		unless (components = moduledb_show(module)) {
			fprintf(stderr,
			    "module: module '%s' does not exist.\n", module);
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
	if (streq(module, "all")) {
		fprintf(stderr,
		    "module: reserved name \"all\" may not be changed.\n");
		goto err;
	}
	if (streq(command, "add") || streq(command, "create")) {
		if (moduledb_add(module, comps, commit)) return (1);
	} else if (streq(command, "rm")) {
		if (moduledb_rm(module, comps, commit)) return (1);
	} else {
		goto err;
	}
	return (0);
}

/*
 * Given one or more module names, and optionally a changeset sccs*,
 * return a hash of implied root keys.
 * All names are recursively expanded if the values have names in them.
 * XXX - always imply the product?
 * XXX - does not take a rev and that's probably wrong
 */
hash *
module_list(char **names, sccs *cset)
{
	int	i, failed = 0;
	modules	*mdb;		/* BitKeeper/etc/modules */
	hash	*keys = 0;	/* resulting key expansion */
	char	buf[MAXKEY];

	unless (proj_isProduct(0)) {
		error("%s: module expansion called in a non-product.\n", prog);
		return (0);
	}
	unless (names && names[1]) {
		error("%s: module expansion with no names?\n", prog);
		return (0);
	}
	mdb = new(modules);
	unless (mdb->moduleDB = moduledb_init()) {
		error("%s: can't get %s\n", prog, buf);
		return (0);
	}
	keys = hash_new(HASH_MEMHASH);
	mdb->free_cset = (cset == 0);
	mdb->cset = cset;
	mdb->seen = hash_new(HASH_MEMHASH);
	EACH(names) {
		if (module_expand(names[i], mdb, keys)) {
			failed = 1;
			break;
		}
	}
	modules_free(mdb);
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
moduledb_init(void)
{
	char	buf[MAXPATH];

	concat_path(buf, proj_root(proj_product(0)), MODULES);
	if (!exists(buf) && get(buf, SILENT, "-")) {
		error("%s: no module file found\n", prog);
		return (0);
	}
	return (hash_fromFile(0, buf));
}

private char **
moduledb_show(char *module)
{
	modules	*mdb;
	char	**result = 0;
	char	*val;

	mdb = new(modules);
	unless (mdb->moduleDB = moduledb_init()) goto out;
	unless (val = hash_fetchStr(mdb->moduleDB, module)) goto out;
	result = splitLine(mdb->moduleDB->vptr, "\r\n", 0);
out:	modules_free(mdb);
	return (result);
}

/*
 * Add components to module 'module' in the modules db,
 * creating module if it doesn't already exist
 */
private int
moduledb_add(char *module, char **components, int commit)
{
	int	i;
	int	rc = 1, errors = 0;
	char	*comment = 0, *p = 0;
	char	**list = 0, **val = 0;
	char	*key;
	modules	*mdb;

	unless (module_validName(module)) {
		fprintf(stderr, "%s: invalid module name %s.\n", prog, module);
		return (1);
	}
	mdb = new(modules);
	unless (mdb->moduleDB = moduledb_init()) goto err;
	EACH (components) {
		if (isKey(components[i])) {
			unless (verifyKey(components[i], mdb)) {
				fprintf(stderr, "%s: not a component key: "
				    "'%s'\n", prog, components[i]);
				errors++;
				continue;
			}
			list = addLine(list, strdup(components[i]));
		} else if (hash_fetchStr(mdb->moduleDB, components[i])) {
			/* module name, just store it */
			list = addLine(list, strdup(components[i]));
		} else if (key = dir2key(components[i], mdb)) {
			list = addLine(list, key);
		} else if (is_glob(components[i])) {
			/* glob, just store it */
			/* XXX - should allow escapes */
			list = addLine(list, strdup(components[i]));
		} else {
			fprintf(stderr, "%s: %s must be either a glob, key, "
			    "module, or component.\n", prog, components[i]);
			errors++;
			continue;
		}
	}
	if (errors) {
		fprintf(stderr, "%s: %d error%s processing modules\n",
		    prog, errors, (errors > 1)?"s":"");
		goto err;
	}
	unless (nLines(list)) {
		fprintf(stderr, "%s: nothing to add\n", prog);
		goto err;
	}
	unless (hash_fetchStr(mdb->moduleDB, module)) {
		comment = aprintf("Create module %s", module);
	} else {
		comment = aprintf("Modify module %s", module);
		val = splitLine(mdb->moduleDB->vptr, "\r\n", 0);
	}
	EACH (list) val = addLine(val, strdup(list[i]));
	uniqLines(val, free);
	p = joinLines("\n", val);
	hash_storeStr(mdb->moduleDB, module, p);
	rc = finish(comment, mdb->moduleDB, commit);
err:	modules_free(mdb);
	if (comment) free(comment);
	if (list) freeLines(list, free);
	return (rc);
}

/*
 * Remove components from module. If components == 0
 * then the module itself is removed.
 */
private int
moduledb_rm(char *module, char **components, int commit)
{
	int	i, j, found, rc = 1;
	char	*p, *comment = 0;
	modules	*mdb;
	char	**list = 0;

	mdb = new(modules);
	unless (mdb->moduleDB = moduledb_init()) goto err;
	unless (hash_fetchStr(mdb->moduleDB, module)) {
		fprintf(stderr, "%s: no such module %s\n", prog, module);
		goto err;
	}
	unless (components) {
		/* delete the module itself */
		hash_deleteStr(mdb->moduleDB, module);
		comment = aprintf("Remove %s", module);
		goto out;
	}
	list = splitLine(mdb->moduleDB->vptr, "\r\n", 0);
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
			    "nothing modified.\n", prog, components[i], module);
			goto err;

		}
	}
	switch (nLines(list)) {
	    case 0:
		comment = aprintf("Delete module %s", module);
		hash_deleteStr(mdb->moduleDB, module);
		break;
	    case 1:
		comment = aprintf("Modify module %s", module);
		hash_storeStr(mdb->moduleDB, module, list[1]);
		break;
	    default:
		comment = aprintf("Modify module %s", module);
		p = joinLines("\n", list);
		hash_storeStr(mdb->moduleDB, module, p);
		free(p);
		break;
	}
out:	rc = finish(comment, mdb->moduleDB, commit);
err:	if (comment) free(comment);
	if (list) freeLines(list, free);
	modules_free(mdb);
	return (rc);
}

/*
 * Check that a module name is valid
 */
private int
module_validName(char *name)
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
 * Expand a value of the module db into its keys, note that this doesn't handle
 * module names. See module_expand for that.
 */
private int
value_expand(char *name, modules *mdb, hash *keys)
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
	}  else {
		error("%s: could not find module '%s'\n", prog, name);
		return (1);
	}
	return (0);
}

/*
 * Expand a single module into its keys and put them
 * in the keys hash.
 */
private int
module_expand(char *name, modules *mdb, hash *keys)
{
	char	*mval;
	char	**expansion = 0;
	int	i, rc = 1;

	unless (mval = hash_fetchStr(mdb->moduleDB, name)) {
		/* not a module name */
		return (value_expand(name, mdb, keys));
	}
	unless (hash_insertStr(mdb->seen, name, "")) {
		error("%s: recursive module definition '%s'.\n",
		    prog, name);
		return (1);
	}
	unless (expansion = splitLine(mval, "\r\n", 0)) goto out;
	EACH(expansion) {
		if (module_expand(expansion[i], mdb, keys)) goto out;
	}
	rc = 0;
out:	hash_deleteStr(mdb->seen, name);
	if (expansion) freeLines(expansion, free);
	return (rc);
}

private void
modules_free(modules *mdb)
{
	assert(mdb);
	if (mdb->free_cset && mdb->cset) sccs_free(mdb->cset);
	if (mdb->comps) ensemble_free(mdb->comps);
	if (mdb->moduleDB) hash_free(mdb->moduleDB);
	if (mdb->seen) hash_free(mdb->seen);
	free(mdb);
}
/*
 * Get a list of keys that match a dir glob.
 */
private int
dir_expand(char *dir, modules *mdb, hash *keys)
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
dir2key(char *dir, modules *mdb)
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
	if (strneq(dir, "./", 2)) dir += 2;
	unless (p = ensemble_dir2key(mdb->comps, dir)) return (0);
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
finish(char *comment, hash *moduleDB, int commit)
{
	int	ret = 0;
	char	buf[MAXPATH];

	system("bk edit -q " MODULES);
	hash_toFile(moduleDB, MODULES);
	sprintf(buf, "bk delta -qy'%s' %s", comment, MODULES);
	if (ret = system(buf)) return (ret);
	if (commit) {
		sprintf(buf,
		    "bk sfiles -pAC %s |"
		    "bk sccslog -A -f -nd:C: - >BitKeeper/tmp/cmt%u",
		    MODULES, getpid());
		if (ret = system(buf)) return (ret);
		sprintf(buf,
		    "bk sfiles -pC %s |"
		    "bk commit -qfYBitKeeper/tmp/cmt%u -", MODULES, getpid());
		ret = system(buf);
		sprintf(buf, "BitKeeper/tmp/cmt%u", getpid());
		unlink(buf);
	}
	return (ret);
}

/*
 * Verify that a given key is indeed a component
 * of this product
 */
private int
verifyKey(char *key, modules *mdb)
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
