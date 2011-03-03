#include "sccs.h"

private	int	comps_citool(char **av, int haveAliases);

int
comps_main(int ac, char **av)
{
	int	c;
	int	citool = 0, haveAliases = 0;
	int	rc;
	char	**nav = 0;
	char	**aliases = 0;
	longopt	lopts[] = {
		{ "here", 'h' },
		{ "missing", 'm' },
		{ 0, 0 }
	};

	nav = addLine(nav, "alias");
	while ((c = getopt(ac, av, "chkms;", lopts)) != -1) {
		switch (c) {
		    case 'c': citool = 1; break;
		    case 'h': nav = addLine(nav, "-h"); break;
		    case 'k': nav = addLine(nav, "-k"); break;
		    case 'm': nav = addLine(nav, "-m"); break;
		    case 's':
			      haveAliases = 1;
			      aliases = addLine(aliases, optarg);
			      break;
		    default: bk_badArg(c, av);
		}
	}
	unless (aliases) {
		aliases = addLine(aliases, "ALL");
		unless (citool) aliases = addLine(aliases, "^PRODUCT");
	}
	nav = addLine(nav, "-e");
	nav = catLines(nav, aliases);
	freeLines(aliases, 0);
	aliases = 0;
	nav = addLine(nav, 0);

	if (av[optind]) usage();
	if (citool) {
		nav = unshiftLine(nav, "bk");
		nav = addLine(nav, 0);	/* guarantee array termination */
		rc = comps_citool(nav+1, haveAliases);
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
comps_citool(char **av, int haveAliases)
{
	FILE	*f;
	char	*t;
	char	*first = 0;
	int	status;

	unless (f = popenvp(av, "r")) return (1);

	if (!haveAliases && proj_isComponent(0)) {
		first = strdup(proj_comppath(0));
		puts(first);
	}
	(void)proj_cd2product();

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
