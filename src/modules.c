#include "system.h"
#include "sccs.h"
#include "logging.h"
#include "ensemble.h"
#include "range.h"

/*
 * Given one or more module names, and optionally a changeset sccs*,
 * return a hash of implied root keys.
 * All names must be
 * - in the on disk hash
 * - a key
 * - a pathname w/ trailing /
 * or nothing is returned.
 * All names are recursively expanded if the values have names in them.
 * XXX - only one level deep
 * XXX - always imply the product?
 */
hash *
module_list(char **names, sccs *cset)
{
	int	i, j;
	int	free_cset = 0;
	hash	*modules = 0;	/* BitKeeper/etc/modules */
	hash	*seen = 0;	/* all key|dir|names we've seen */
	hash	*results = 0;	/* hash of results */
	hash	*tmp = 0;	/* used for recursion */
	char	**dirs = 0;	/* any dirs we've found and need to expand */
	char	**keys;		/* used to split the hash value */
	char	**more = 0;	/* used to hold module names found in value */
	char	*p;
	kvpair	kv;		/* walks the cset hash */
	static	int depth = 0;	/* limits to one level of recursion */
	char	buf[MAXKEY];

	unless (proj_isProduct(0)) {
		fprintf(stderr, "module_list called in a non-product.\n");
		return (0);
	}
	unless (names && names[1]) {
		fprintf(stderr, "module_list with no names?\n");
		return (0);
	}

	/*
	 * Go load the entire modules DB into a hash.  We'll pick through
	 * that using names.
	 */
	concat_path(buf, proj_root(proj_product(0)), "BitKeeper/etc/modules");
	unless (exists(buf)) sys("bk", "get", "-q", buf, SYS);
	modules = hash_fromFile(0, buf);

	/*
	 * Foreach name,
	 * if it is a directory, save that aside.
	 * if it is a key, stuff that into expanded.
	 * if it is in the hash, split the value into keys and stuff those
	 * into expanded.
	 * if none of the above, error.
	 */
	seen = hash_new(HASH_MEMHASH);
	results = hash_new(HASH_MEMHASH);
	EACH(names) {
		if (hash_fetchStr(seen, names[i])) {
			fprintf(stderr,
			    "modules: duplicate name %s\n", names[i]);
			goto err;
		}
		hash_storeStr(seen, names[i], "");
		if ((p = strrchr(names[i], '/')) && !p[1]) {
			dirs = addLine(dirs, names[i]);
		} else if (isKey(names[i])) {
			hash_storeStr(results, names[i], "");
		} else if (p = hash_fetchStr(modules, names[i])) {
			keys = splitLine(p, "\r\n", 0);
			EACH_INDEX(keys, j) {
				if ((p = strrchr(keys[j], '/')) && !p[1]) {
					dirs = addLine(dirs, keys[j]);
				} else if (isKey(keys[j])) {
					hash_storeStr(results, keys[j], "");
				} else {
					more = addLine(more, strdup(keys[j]));
				}
			}
			freeLines(keys, free);
		} else {
			fprintf(stderr, "modules: bad name %s\n", names[i]);
err:			hash_free(modules);
			hash_free(results);
			hash_free(seen);
			freeLines(dirs, 0);
			freeLines(more, free);
			return (0);
		}
	}

	if (more) {
		if (depth > 5) {
			fprintf(stderr,
			    "modules: too many levels of recursion.\n");
			goto err;
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
	EACH_KV(cset->mdbm) {
		unless (componentKey(kv.val.dptr)) continue;
		EACH(dirs) {
			sprintf(buf, "|%s", dirs[i]);
			/*
			 * lm@bitmover.com|gdb/ChangeSet|20080311222908|21117
			 * so even though it may be bk/lm there is no | before
			 * the user name and the other fields can't contain /
			 * And dirs[i] has a trailing / so that's all good.
			 */
			if (strstr(kv.val.dptr, buf)) {
				hash_storeStr(results, kv.key.dptr, "");
				break;
			}
	    	}
	}

	if (free_cset) sccs_free(cset);
done:	hash_free(modules);
	hash_free(seen);
	freeLines(dirs, 0);
	return (results);
}

int
module_main(int ac, char **av)
{
	hash	*h;
	char	**names = 0;
	char	*p;
	MDBM	*idDB = loadDB(IDCACHE, 0, DB_IDCACHE);

	while (av[1]) {
		names = addLine(names, av[1]);
		av++;
	}
	unless (h = module_list(names, 0)) exit(1);
	EACH_HASH(h) {
		p = key2path(h->kptr, idDB);
		csetChomp(p);
		printf("%s\n", p);
		free(p);
	}
	hash_free(h);
	freeLines(names, 0);
	mdbm_close(idDB);
	exit(0);
}
