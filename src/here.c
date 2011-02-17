#include "sccs.h"
#include "nested.h"

private int	here_check_main(int ac, char **av);
private int	here_where_main(int ac, char **av);

int
here_main(int ac, char **av)
{
	int	i;
	int	rc;
	char	**nav = 0;

	unless (proj_isEnsemble(0)) {
		fprintf(stderr, "%s: must be in a nested repository\n", prog);
		return (1);
	}
	if (!av[1] || streq(av[1], "list") || (av[1][0] == '-')) {
		/* alias for 'bk alias <opts> HERE' */
		i = (av[1] && streq(av[1], "list")) ? 2 : 1;
		nav = addLine(nav, "alias");
		/* copy options */
		for (; av[i] && (av[i][0] == '-') && av[i][1]; i++) {
			if (streq(av[i], "--")) {
				i++;
				break;
			}
			nav = addLine(nav, strdup(av[i]));
		}
		nav = addLine(nav, "here");
		nav = addLine(nav, 0);
		getoptReset();
		rc = alias_main(nLines(nav), nav+1);
		freeLines(nav, 0);
		return (rc);

	} else if (streq(av[1], "add") ||
	    streq(av[1], "rm") ||
	    streq(av[1], "set")) {
		/* alias for 'bk alias CMD OPTS here ARGS' */
		nav = addLine(nav, strdup("alias"));
		nav = addLine(nav, strdup(av[1])); /* add|rm|set */
		/* copy options */
		for (i = 2; av[i] && (av[i][0] == '-') && av[i][1]; i++) {
			if (streq(av[i], "--")) {
				i++;
				break;
			}
			nav = addLine(nav, strdup(av[i]));
		}
		nav = addLine(nav, strdup("here"));
		/* copy args */
		for (; av[i]; i++) nav = addLine(nav, strdup(av[i]));
		nav = addLine(nav, 0);
		getoptReset();
		rc = alias_main(nLines(nav), nav+1);
		freeLines(nav, free);
		return (rc);

	} else if (streq(av[1], "check")) {
		return (here_check_main(ac-1, av+1));

	} else if (streq(av[1], "where")) {
		return (here_where_main(ac-1, av+1));

	} else if (streq(av[1], "missing")) {
		/* alias for 'bk alias -m' */
		if (av[2]) usage();
		nav = addLine(nav, "alias");
		nav = addLine(nav, "here");
		nav = addLine(nav, "-m");
		nav = addLine(nav, 0);
		getoptReset();
		rc = alias_main(nLines(nav), nav+1);
		freeLines(nav, 0);
		return (rc);

	} else {
		usage();
	}
	/* not reached */
	abort();
}

private int
here_check_main(int ac, char **av)
{
	int	i, c, rc;
	u32	flags = 0;
	nested	*n;
	char	**urls = 0;
	longopt	lopts[] = {
		{ "superset", 300 },
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "@|cq", lopts)) != -1) {
		switch (c) {
		    case '@': if (bk_urlArg(&urls, optarg)) return (1); break;
		    case 'c': flags |= URLLIST_TRIM_NOCONNECT; break;
		    case 'q': flags |= SILENT; break;
		    case 300: flags |= URLLIST_SUPERSET; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	EACH(urls) {
		char	*u = urls[i];

		urls[i] = parent_normalize(u);
		free(u);
	}
	proj_cd2product();
	n = nested_init(0, 0, 0, NESTED_PENDING);
	assert(n);
	rc = urllist_check(n, flags, urls);
	nested_free(n);
	return (rc);
}

private int
here_where_main(int ac, char **av)
{
	int	c;
	char	*p;
	int	allurls = 0, allcomps = 0, flags = SILENT|URLLIST_NOERRORS;
	longopt	lopts[] = {
		{ "all", 310 },		/* all comps, all urls */
		{ "allcomps", 320 },	/* all comps not just missing */
		{ "allurls", 330},	/* all urls for a comp */
		{ 0, 0}
	};

	while ((c = getopt(ac, av, "v", lopts)) != -1) {
		switch (c) {
		    case 'v': flags = 0; break;
		    case 310: allurls = 1; allcomps = 1; break;
		    case 320: allcomps = 1; break;
		    case 330: allurls = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	proj_cd2product();
	unless (av[optind]) {
		urllist_dump(0, allurls, allcomps, flags);
	} else if (streq(av[optind], "rm")) {
		/* Perhaps _rm? or change the verb? */
		if (av[optind+1]) usage();
		unlink(NESTED_URLLIST);
	} else {
		for ( ; av[optind]; optind++) {
			p = av[optind];
			if (strneq(p, "./", 2)) p += 2;
			urllist_dump(p, allurls, allcomps, flags);
		}
	}
	return (0);

}

int
populate_main(int ac, char **av)
{
	char	**nav = 0;
	int	rc;

	av++;		/* skip [0]=populate */
	nav = addLine(nav, "here");
	nav = addLine(nav, "add");
	while (*av) nav = addLine(nav, *av++);
	nav = addLine(nav, 0);
	rc = here_main(ac+1, nav+1);
	freeLines(nav, 0);
	return (rc);
}

int
unpopulate_main(int ac, char **av)
{
	char	**nav = 0;
	int	rc;

	av++;		/* skip [0]=unpopulate */
	nav = addLine(nav, "here");
	nav = addLine(nav, "rm");
	while (*av) nav = addLine(nav, *av++);
	nav = addLine(nav, 0);
	rc = here_main(ac+1, nav+1);
	freeLines(nav, 0);
	return (rc);
}
