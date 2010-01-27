#include "bkd.h"
#include "range.h"

int
repogca_main(int ac, char **av)
{
	sccs	*s;
	delta	*d, *p, *lastd;
	FILE	*f;
	int	c, i, status;
	int	all = 0, rc = 1;
	char	**urls, **nav;
	char	*dspec = ":JOIN::REV:\n$end{\\n}";
	char	*begin = 0, *end = 0;
	char	buf[MAXKEY];

	while ((c = getopt(ac, av, "a5d;k", 0)) != -1) {
		switch (c) {
		    case 'a': all = 1; break;
		    case 'd': dspec = optarg; break;
		    case 'k': dspec = ":KEY:\\n"; break;
		    case '5': dspec = ":MD5KEY:\\n"; break;
		    default: bk_badArg(c, av);
		}
	}
	unless (proj_root(0)) {
		fprintf(stderr, "repogca: must be run in a repository\n");
		exit(1);
	}
	nav = addLine(0, strdup("bk"));
	nav = addLine(nav, strdup("changes"));
	nav = addLine(nav, strdup("-L"));
	nav = addLine(nav, strdup("-end:KEY:"));

	if (av[optind]) {
		for (i = optind; av[i]; i++) {
			unless (remote_valid(av[i])) {
				fprintf(stderr,
				    "repogca: invalid url %s\n", av[i]);
				exit(1);
			}
			nav = addLine(nav, strdup(av[i]));
		}
	} else {
		urls = parent_pullp();
		EACH (urls) nav = addLine(nav, urls[i]);
		freeLines(urls, 0);
	}
	nav = addLine(nav, 0);	/* null term list */
	s = sccs_csetInit(SILENT);
	assert(s && HASGRAPH(s));

	f = popenvp(nav + 1, "r");
	freeLines(nav, free);
	while (fnext(buf, f)) {
		if (strneq(buf, "==== ", 5)) continue;
		chop(buf);
		d = sccs_findKey(s, buf);
		assert(d);
		d->flags |= D_RED;
	}
	status = pclose(f);
	unless (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) {
		sccs_free(s);
		fprintf(stderr, "repogca: connection to parent failed\n");
		return (2);
	}
	dspec = strdup(dspec);
	dspec_collapse(&dspec, &begin, &end);
	lastd = s->table;
	for (d = s->table; d; d = NEXT(d)) {
		if ((d->type == 'D') && !(d->flags & (D_RED|D_BLUE))) {
			if (begin) {
				sccs_prsdelta(s, d, 0, begin, stdout);
				free(begin);
				begin = 0;
			}
			lastd = d;
			sccs_prsdelta(s, d, 0, dspec, stdout);
			rc = 0;
			unless (all) break;
			d->flags |= D_BLUE;
		}
		if (d->flags & D_BLUE) {
			if (p = PARENT(s, d)) p->flags |= D_BLUE;
			if (p = MERGE(s, d)) p->flags |= D_BLUE;
		}
	}
	if (end) {
		sccs_prsdelta(s, lastd, 0, end, stdout);
		free(end);
	}
	free(dspec);
	sccs_free(s);
	return (rc);
}
