#include "system.h"
#include "sccs.h"
#include "logging.h"
#include "ensemble.h"
#include "range.h"

private int	finish(char *comment, hash *moduleDB, int commit);
char		*rootkey(char *path);
private void	error(const char *fmt, ...);
extern	char	*prog;

/*
 * Given one or more module names, and optionally a changeset sccs*,
 * return a hash of implied root keys.
 * All names must be
 * - in the on disk hash
 * - a key
 * - a pathname w/ trailing /
 * or nothing is returned.
 * All names are recursively expanded if the values have names in them.
 * XXX - always imply the product?
 * XXX - does not take a rev and that's probably wrong
 */
hash *
module_list(char **names, sccs *cset)
{
	int	i, j;
	int	free_cset = 0;
	hash	*modules = 0;	/* BitKeeper/etc/modules */
	hash	*seen = 0;	/* all key|dir|names we've seen */
	hash	*results = 0;	/* hash of results */
	hash	*expanded = 0;	/* record of which ones expanded */
	hash	*tmp = 0;	/* used for recursion */
	char	**dirs = 0;	/* any dirs we've found and need to expand */
	char	**keys;		/* used to split the hash value */
	char	**more = 0;	/* used to hold module names found in value */
	char	*p, *e;
	char	*name;		/* used to hold a de-escaped copy */
	kvpair	kv;		/* walks the cset hash */
	static	int depth = 0;	/* limits to one level of recursion */
	char	buf[MAXKEY];

	unless (proj_isProduct(0)) {
		error("%s: module expansion called in a non-product.\n", prog);
		return (0);
	}
	unless (names && names[1]) {
		error("%s: module expansion with no names?\n");
		return (0);
	}

	/*
	 * Go load the entire modules DB into a hash.  
	 */
	concat_path(buf, proj_root(proj_product(0)), MODULES);
	if (!exists(buf) && get(buf, SILENT, "-")) {
		error("%s: can't get %s\n", prog, buf);
		return (0);
	}
	modules = hash_fromFile(0, buf);

	/*
	 * Foreach name,
	 * if it is in the hash, split the value into keys and stuff those
	 * into results.
	 * if it is a directory, save that aside.
	 * if it is a key, stuff that into results.
	 * if none of the above, error.
	 */
	seen = hash_new(HASH_MEMHASH);
	results = hash_new(HASH_MEMHASH);
	EACH(names) {
		if (hash_fetchStr(seen, names[i])) {
			error("%s: duplicate module name %s\n", prog, names[i]);
			goto err;
		}
		hash_storeStr(seen, names[i], "");
		if (p = hash_fetchStr(modules, names[i])) {
			keys = splitLine(p, "\r\n", 0);
			EACH_INDEX(keys, j) {
				if (hash_fetchStr(modules, keys[j])) {
					more = addLine(more, strdup(keys[j]));
				} else if (isKey(keys[j])) {
					hash_storeStr(results, keys[j], "");
				} else {
					dirs = addLine(dirs, strdup(keys[j]));
				}
			}
			freeLines(keys, free);
		} else if (isKey(names[i])) {
			hash_storeStr(results, names[i], "");
		} else {
			dirs = addLine(dirs, strdup(names[i]));
		}
	}

	if (more) {
		if (depth > 5) {
			p = joinLines(" ", names);
			error("%s: too many levels of "
			    "recursion expanding module[s]: %s\n", prog, p);
			free(p);
err:			hash_free(modules);
			hash_free(results);
			hash_free(seen);
			if (expanded) hash_free(expanded);
			freeLines(dirs, free);
			freeLines(more, free);
			return (0);
		}
		depth++;
		tmp = module_list(more, cset);
		depth--;
		unless (tmp) goto err;
		EACH_HASH(tmp) hash_storeStr(results, tmp->kptr, "");
		hash_free(tmp);
		freeLines(more, free);
		more = 0;
	}

	unless (dirs) goto done;

	unless (cset) {
		concat_path(buf, proj_root(proj_product(0)), CHANGESET);
		cset = sccs_init(buf, INIT_NOCKSUM|INIT_NOSTAT);
		free_cset = 1;
	}
	assert(CSET(cset) && proj_isProduct(cset->proj));
	sccs_get(cset, "+", 0, 0, 0, SILENT|GET_HASHONLY, 0);

	expanded = hash_new(HASH_MEMHASH);
	/*
	 * N*M alg.  lm3di
	 */
	EACH_KV(cset->mdbm) {
		unless (componentKey(kv.val.dptr)) continue;
		EACH(dirs) {
			/*
			 * kv.val.dptr =
			 * lm@bitmover.com|gdb/ChangeSet|20080311222908|21117
			 * dirs[i] =
			 * .
			 * ./something
			 * ./glob_pattern
			 */
			if (streq(dirs[i], ".")) {
				hash_storeStr(results, proj_rootkey(0), "");
				hash_storeStr(expanded, dirs[i], "");
				continue;
			}

			/* The expanded loop below will flag this. */
			unless (strneq(dirs[i], "./", 2)) break;

			p = strchr(kv.val.dptr, '|');
			assert(p);
			p++;
			e = strchr(p, '|');
			assert(e);
			while ((--e > p) && (*e != '/'));
			*e = 0;
			unless (streq(dirs[i]+2, p) ||
			    match_one(p, dirs[i]+2, 0)){
				*e = '/';
				continue;
			}
			hash_storeStr(results, kv.key.dptr, "");
			hash_storeStr(expanded, dirs[i], "");
			break;
	    	}
	}
	if (free_cset && cset) sccs_free(cset);
	j = 0;
	EACH(dirs) {
		unless (hash_fetchStr(expanded, dirs[i])) {
			j++;
			error("%s: no match for module %s\n", prog, dirs[i]);
		}
	}
	if (j) goto err;
done:	hash_free(modules);
	hash_free(seen);
	if (expanded) hash_free(expanded);
	freeLines(dirs, free);
	return (results);
}

/*
 * bk module add -M<module> <comp> ...	// create/add to a module
 * bk module rm -M<module> [<comp> ...]	// remove a comp|module
 * bk module list			// list all modules
 */
int
module_main(int ac, char **av)
{
	int	i, j, found;
	int	ret = 1, commit = 1;
	int	add = 0, rm = 0;
	char	**names = 0;
	char	**comps = 0;
	char	*key = 0, *p;
	hash	*h = 0, *moduleDB = 0;
	MDBM	*idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	char	buf[MAXPATH];

	unless (av[1]) {
err:		system("bk help -s module");
out:		mdbm_close(idDB);
		freeLines(names, free);
		freeLines(comps, free);
		if (h) hash_free(h);
		if (moduleDB) hash_free(moduleDB);
		return (ret);
	}
	if (proj_cd2product()) {
		fprintf(stderr, "module called in a non-product.\n");
		goto err;
	}
	if (!exists(MODULES) && get(MODULES, SILENT, "-")) {
		fprintf(stderr, "module: unable to get modules db\n");
		goto err;
	}
	moduleDB = hash_fromFile(0, MODULES);

	if (streq(av[1], "add")) add++;
	if (streq(av[1], "create")) add++;
	if (streq(av[1], "rm")) rm++;
	if (streq(av[1], "list")) {
		EACH_HASH(moduleDB) names = addLine(names, moduleDB->kptr);
		sortLines(names, 0);
		EACH(names) printf("%s\n", names[i]);
		freeLines(names, 0);
		names = 0;
		ret = 0;
		goto out;
	}

	unless (add || rm) goto err;
	av++, ac--;

	while ((j = getopt(ac, av, "CM;")) != -1) {
		switch (j) {
		    case 'C': commit = 0; break;
		    case 'M': key = optarg; break;
	    	}
	}

	unless (key) {
		fprintf(stderr, "module: no module specified.\n");
		goto err;
	}

	while (av[optind]) comps = addLine(comps, strdup(av[optind++]));

	/*
	 * Both add and rm want these converted.
	 */
	EACH(comps) {
		/* dir/ or path/to/repo/root */
		if (isdir(comps[i])) {
			p = strrchr(comps[i], '/');
			if (p && (p[1] == 0)) {
				names = addLine(names, strdup(comps[i]));
				continue;
			}
			unless (isComponent(comps[i])) {
				fprintf(stderr,
				    "module: %s is not a component\n",
				    comps[i]);
				goto err;
			}
			p = rootkey(comps[i]);
			assert(p);
			names = addLine(names, p);
			continue;
		}

		/* rootkey */
		if (isKey(comps[i])) {
			p = key2path(comps[i], idDB);
			unless (isComponent(p)) {
				fprintf(stderr,
				    "module: %s is not a component\n",
				    comps[i]);
				free(p);
				goto err;
			}
			names = addLine(names, p);
			continue;
		}
		
		/* must be symbolic name */
		if (add) {
			/* check to see if it is a bogus dir or not in db */
			p = strrchr(comps[i], '/');
			if (p && (p[1] == 0)) {
				p = "directory";
			} else unless (hash_fetchStr(moduleDB, comps[i])) {
				p = "module";
			} else {
				p = 0;
			}
			if (p ) {
				fprintf(stderr,
				    "module: %s is not a %s\n", comps[i], p);
				goto err;
			}
		}
		names = addLine(names, strdup(comps[i]));
	}
	if (comps) {
		freeLines(comps, free);
		comps = names;
		names = 0;
	}

	if (p = hash_fetchStr(moduleDB, key)) {
		names = splitLine(moduleDB->vptr, "\r\n", 0);
	}

	if (rm) {
		unless (p) {
			fprintf(stderr, "module %s doesn't exist\n", key);
			goto err;
		}
		if (comps) {
			EACH(comps) {
				found = 0;
				EACH_INDEX(names, j) {
					if (streq(comps[i], names[j])) {
						removeLineN(names, j, free);
						found = 1;
						break;
					}
				}
				unless (found) {
					fprintf(stderr,
					    "module: %s is not part of %s, "
					    "nothing modified.\n",
					    comps[i], key);
					goto err;
				}
			}
			freeLines(comps, free);
			comps = names;
			names = 0;
			if (nLines(comps) == 0) {
				freeLines(comps, free);
				comps = 0;
			}
		}

		sprintf(buf, "Modify module %s", key);
		switch (nLines(comps)) {
		    case 0:
			sprintf(buf, "Delete module %s", key);
			hash_deleteStr(moduleDB, key);
			break;
		    case 1:
			hash_storeStr(moduleDB, key, comps[1]);
			break;
		    default:
			p = joinLines("\n", comps);
			strcat(p, "\n");		// join chomps
			hash_storeStr(moduleDB, key, p);
			free(p);
			break;
		}
		ret = finish(buf, moduleDB, commit);
		goto out;
	}

	/* add case */
	unless (comps) goto err;
	j = nLines(comps);
	EACH(names) comps = addLine(comps, names[i]);
	freeLines(names, 0);
	names = 0;
	uniqLines(comps, free);
	if (hash_fetchStr(moduleDB, key)) {
		if (nLines(comps) == j) {
			fprintf(stderr, "Nothing to add.\n");
			goto err;
		}
		sprintf(buf, "Modify module %s", key);
	} else {
		sprintf(buf, "Create module %s", key);
	}
	switch (nLines(comps)) {
	    case 1:
		hash_storeStr(moduleDB, key, comps[1]);
		break;
	    default:
		p = joinLines("\n", comps);
		strcat(p, "\n");		// join chomps
		hash_storeStr(moduleDB, key, p);
		free(p);
		break;
	}
	ret = finish(buf, moduleDB, commit);
	goto out;
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

char	*
rootkey(char *path)
{
	project	*p = proj_init(path);
	char	*key;

	assert(p);
	key = strdup(proj_rootkey(p));
	proj_free(p);
	return (key);
}

private int
finish(char *comment, hash *moduleDB, int commit)
{
	int	ret;
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
	return (0);
}
