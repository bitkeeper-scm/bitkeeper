#include "bkd.h"
#include "range.h"

int
repogca_main(int ac, char **av)
{
	char	*parent;
	sccs	*s;
	delta	*d;
	FILE	*f;
	char	buf[MAXKEY];
	char	s_cset[] = CHANGESET;

	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "repogca: must be run in a repository\n");
		exit(1);
	}
	parent = av[1] ? av[1] : getParent();
	s = sccs_init(s_cset, SILENT, 0);
	assert(s && s->tree);
	sprintf(buf, "bk changes -L -end:KEY: %s", parent);
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
