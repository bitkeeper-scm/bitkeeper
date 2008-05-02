#include "bkd.h"
#include "range.h"

int
repogca_main(int ac, char **av)
{
	sccs	*s;
	delta	*d;
	FILE	*f;
	int	c, i, status;
	char	**urls, **nav;
	char	*dspec = ":REV:\n";
	char	buf[MAXKEY];

	while ((c = getopt(ac, av, "5d;k")) != -1) {
		switch (c) {
		    case 'd': dspec = optarg; break;
		    case 'k': dspec = ":KEY:\n"; break;
		    case '5': dspec = ":MD5KEY:\n"; break;
		    default:  system("bk help -s repogca"); return (1);
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
		for (i = optind; av[i]; i++) nav = addLine(nav, strdup(av[i]));
	} else {
		urls = parent_pullp();
		EACH (urls) nav = addLine(nav, urls[i]);
		freeLines(urls, 0);
	}
	addLine(nav, 0);	/* null term list */
	s = sccs_csetInit(SILENT);
	assert(s && HASGRAPH(s));
	sccs_findKeyDB(s, 0);

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
	for (d = s->table; d; d = d->next) {
		if ((d->type == 'D') && !(d->flags & D_RED)) {
			sccs_prsdelta(s, d, 0, dspec, stdout);
			sccs_free(s);
			return (0);
		}
	}
	sccs_free(s);
	return (1);
}
