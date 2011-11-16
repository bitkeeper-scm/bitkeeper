#include "bkd.h"
#include "range.h"

int
repogca_main(int ac, char **av)
{
	int	c, i;
	int	rc = 1;
	char	**urls = 0;
	char	*dspec = 0;
	int	flags = 0;
	longopt	lopts[] = {
		{ "dspecf;", 310 },

		/* aliases */
		{ "standalone", 'S' }, /* treat comp as standalone */
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "a5d;kS", lopts)) != -1) {
		switch (c) {
		    case 'a': flags |= RGCA_ALL; break;
		    case 'd':
			if (dspec) usage();
			dspec = strdup(optarg);
			break;
		    case 'k':
			if (dspec) usage();
			dspec = strdup(":KEY:\\n");
			break;
		    case '5':
			if (dspec) usage();
			dspec = strdup(":MD5KEY:\\n");
			break;
		    case 'S': flags |= RGCA_STANDALONE; break;
		    case 310: // --dspecf=FILE
			if (dspec) usage();
			unless (dspec = loadfile(optarg, 0)) {
				fprintf(stderr, "%s: unable to read '%s'\n",
				    prog, optarg);
				return (1);
			}
			break;
		    default: bk_badArg(c, av);
		}
	}
	for (i = optind; av[i]; i++) urls = addLine(urls, av[i]);
	rc = repogca(urls, dspec, flags, stdout);
	FREE(dspec);
	freeLines(urls, 0);
	return (rc);
}

int
repogca(char **urls, char *dspec, u32 flags, FILE *out)
{
	sccs	*s = 0;
	ser_t	d, p, lastd;
	FILE	*f;
	int	i, status;
	int	rc = 1;
	char	**nav;
	char	*key;
	char	*begin = 0, *end = 0;
	char	buf[MAXPATH];

	if (dspec) dspec = strdup(dspec);
	nav = addLine(0, strdup("bk"));
	nav = addLine(nav, strdup("changes"));
	if (flags & RGCA_STANDALONE) nav = addLine(nav, strdup("-S"));
	nav = addLine(nav, strdup("-qL"));
	nav = addLine(nav, strdup("-end:KEY:"));

	if (urls) {
		EACH(urls) {
			unless (remote_valid(urls[i])) {
				verbose((stderr,
					"%s: invalid url %s\n", prog, urls[i]));
				goto out;
			}
			nav = addLine(nav, strdup(urls[i]));
		}
	} else {
		urls = parent_pullp();
		EACH (urls) nav = addLine(nav, urls[i]);
		freeLines(urls, 0);
	}
	nav = addLine(nav, 0);	/* null term list */

	unless (proj_root(0)) {
		verbose((stderr, "%s: must be run in a repository\n", prog));
		goto out;
	}
	if (proj_isComponent(0) && !(flags & RGCA_STANDALONE)) {
		strcpy(buf, proj_root(proj_product(0)));
	} else {
		strcpy(buf, proj_root(0));
	}
	concat_path(buf, buf, CHANGESET);
	s = sccs_init(buf, SILENT);
	assert(s && HASGRAPH(s));

	f = popenvp(nav + 1, "r");
	while (key = fgetline(f)) {
		d = sccs_findKey(s, key);
		assert(d);
		FLAGS(s, d) |= D_RED;
	}
	status = pclose(f);
	unless (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) {
		verbose((stderr, "%s: connection to parent failed\n", prog));
		rc = 2;
		goto out;
	}
	unless (dspec) dspec = strdup("#dv2\n:JOIN::REV:\n$end{\\n}");
	dspec_collapse(&dspec, &begin, &end);
	lastd = TABLE(s);
	for (d = TABLE(s); d >= TREE(s); d--) {
		if (!TAG(s, d) && !(FLAGS(s, d) & (D_RED|D_BLUE))) {
			if (begin) {
				sccs_prsdelta(s, d, 0, begin, out);
				free(begin);
				begin = 0;
			}
			lastd = d;
			sccs_prsdelta(s, d, 0, dspec, out);
			rc = 0;
			unless (flags & RGCA_ALL) break;
			FLAGS(s, d) |= D_BLUE;
		}
		if (FLAGS(s, d) & D_BLUE) {
			if (p = PARENT(s, d)) FLAGS(s, p) |= D_BLUE;
			if (p = MERGE(s, d)) FLAGS(s, p) |= D_BLUE;
		}
	}
	if (end) {
		sccs_prsdelta(s, lastd, 0, end, out);
		free(end);
	}
out:	FREE(dspec);
	freeLines(nav, free);
	if (s) sccs_free(s);
	return (rc);
}
