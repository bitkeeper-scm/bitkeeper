#include "system.h"
#include "sccs.h"

int
preference_main(int ac, char **av)
{
	unless (ac == 2) return (1);
	fputs(user_preference(av[1]), stdout);
	fputs("\n", stdout);
	return (0);
}


char *
user_preference(char *what)
{
	char *p;

	unless (bk_proj) return "";
	unless (bk_proj->config) {
		unless (bk_proj->root) return "";
		bk_proj->config = loadConfig(bk_proj->root, 0);
		unless (bk_proj->config) return "";
	}
	p = mdbm_fetch_str(bk_proj->config, what);
	unless (p) p = "";
	return (p);
}
