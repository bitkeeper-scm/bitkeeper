#include "sccs.h"

private	int	components_citool(char **av);

int
components_main(int ac, char **av)
{
	int	c;
	int	citool = 0;
	int	rc;
	char	**nav = 0;

	// XXX hack for compat with old 'bk components add'
	if (av[1] && streq(av[1], "add")) return (populate_main(ac-1, av+1));

	while ((c = getopt(ac, av, "chkm", 0)) != -1) {
		switch (c) {
		    case 'c': citool = 1; break;
		    case 'h': nav = addLine(nav, "-h"); break;
		    case 'k': nav = addLine(nav, "-k"); break;
		    case 'm': nav = addLine(nav, "-m"); break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	if (citool) {
		nav = unshiftLine(nav, "alias");
		nav = unshiftLine(nav, "bk");
	}
	nav = addLine(nav, "-e");
	nav = addLine(nav, "ALL");
	nav = addLine(nav, 0);

	if (citool) {
		rc = components_citool(nav);
	} else {
		getoptReset();
		rc = alias_main(nLines(nav), nav);
	}
	freeLines(nav, 0);
	return (rc);
}

private int
components_citool(char **av)
{
	FILE	*f;
	char	*t;
	char	*first = 0;
	int	status;

	unless (f = popenvp(av+1, "r")) return (1);

	if (proj_isComponent(0)) {
		first = strdup(proj_comppath(0));
		puts(first);
	}
	(void)proj_cd2product();

	puts(".");
	while (t = fgetline(f)) {
		if (strneq(t, "./", 2)) t += 2;
		if (first && streq(first, t)) {
			free(first);
			first = 0;
			continue;
		}
		puts(t);
	}
	status = pclose(f);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) return (1);
	return (0);
}
