#include "system.h"
#include "sccs.h"

int
preference_main(int ac, char **av)
{
	kvpair	kv;
	MDBM	*db;
	char	**list = 0;
	char	*l;
	int	i;

	if (ac == 2) {
		fputs(user_preference(av[1]), stdout);
		fputs("\n", stdout);
		return (0);
	}
	if (ac != 1) return (1);
	user_preference("xxx");
	unless (bk_proj && (db = bk_proj->config)) return (1);
	for (kv = mdbm_first(db); kv.key.dsize; kv = mdbm_next(db)) {
		l = aprintf("%s: %s", kv.key.dptr, kv.val.dptr);
		list = addLine(list, l);
	}
	sortLines(list);
	EACH(list) {
		printf("%s\n", list[i]);
	}
	freeLines(list);
	return (0);
}


char *
user_preference(char *what)
{
	char *p;

	unless (bk_proj) return "";
	unless (bk_proj->config) {
		unless (bk_proj->root) return "";
		bk_proj->config = loadConfig(bk_proj->root);
		unless (bk_proj->config) return "";
	}
	p = mdbm_fetch_str(bk_proj->config, what);
	unless (p) p = "";
	return (p);
}
