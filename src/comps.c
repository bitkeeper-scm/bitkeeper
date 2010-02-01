#include "sccs.h"

private	int	comps_citool(char **av);

int
comps_main(int ac, char **av)
{
	int	c;
	int	citool = 0;
	int	rc;
	char	**nav = 0;
	longopt	lopts[] = {
		{ "here", 'h' },
		{ "missing", 'm' },
		{ 0, 0 }
	};

	nav = addLine(nav, "alias");
	while ((c = getopt(ac, av, "chkm", lopts)) != -1) {
		switch (c) {
		    case 'c': citool = 1; break;
		    case 'h': nav = addLine(nav, "-h"); break;
		    case 'k': nav = addLine(nav, "-k"); break;
		    case 'm': nav = addLine(nav, "-m"); break;
		    default: bk_badArg(c, av);
		}
	}
	nav = addLine(nav, "-e");
	nav = addLine(nav, "ALL");
	nav = addLine(nav, 0);

	if (av[optind]) usage();
	if (citool) {
		nav = unshiftLine(nav, "bk");
		nav = addLine(nav, 0);	/* guarantee array termination */
		rc = comps_citool(nav+1);
	} else {
		getoptReset();
		rc = alias_main(nLines(nav), nav+1);
	}
	freeLines(nav, 0);
	return (rc);
}

int
components_main(int ac, char **av)
{
	fprintf(stderr, "bk components: this command is deprecated\n");

	unless (av[1] && streq(av[1], "add")) return (1);

	getoptReset();
	return (here_main(ac, av));
}

private int
comps_citool(char **av)
{
	FILE	*f;
	char	*t;
	char	*first = 0;
	int	status;

	unless (f = popenvp(av, "r")) return (1);

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
