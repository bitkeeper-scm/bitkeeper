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
	char	*p;
	MDBM	*db;

	unless (bk_proj) return "";
	db = proj_config(bk_proj);
	unless (db) return ("");
	p = mdbm_fetch_str(db, what);
	unless (p) p = "";
	return (p);
}

int
do_checkout(sccs *s)
{
	MDBM	*config = proj_config(s->proj);
	int	getFlags = 0;
	char	*co;

	unless (config) return (0);

	if ((co = mdbm_fetch_str(config, "checkout"))) {
		if (strieq(co, "get")) getFlags = GET_EXPAND;
		if (strieq(co, "edit")) getFlags = GET_EDIT;
	}
	if (getFlags) {
		s = sccs_restart(s);
		unless (s) return (-1);
		if (sccs_get(s, 0, 0, 0, 0, SILENT|getFlags, "-")) {
			return (-1);
		}
	}
	return (0);
}
