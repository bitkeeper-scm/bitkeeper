#include "bkd.h"
#include "range.h"

int
repogca_main(int ac, char **av)
{
	char	*parent;
	sccs	*s;
	delta	*d;
	FILE	*f;
	char	**urls;
	char	buf[MAXKEY];
	char	s_cset[] = CHANGESET;

	if (proj_cd2root()) {
		fprintf(stderr, "repogca: must be run in a repository\n");
		exit(1);
	}
	if (av[1]) {
		parent = av[1];
	} else {
		urls = parent_pullp();
		parent = popLine(urls);
		freeLines(urls, free);
	}
	s = sccs_init(s_cset, SILENT);
	assert(s && HASGRAPH(s));
	sprintf(buf, "bk changes -L -end:REV: %s", parent);
	unless (f = popen(buf, "r")) {
		perror(buf);
		exit(1);
	}
	while (fnext(buf, f)) {
		chop(buf);
		d = sccs_findrev(s, buf);
		assert(d);
		d->flags |= D_RED;
	}
	pclose(f);
	for (d = s->table; d; d = d->next) {
		if ((d->type == 'D') && !(d->flags & D_RED)) {
			printf("%s\n", d->rev);
			sccs_free(s);
			exit(0);
		}
	}
	exit(1);
}
