/*
 * what - look for SCCS what strings.
 *
 * Copyright (c) 1997 by Larry McVoy; All rights reserved.
 *
 */
#include "system.h"
#include "sccs.h"

private	int	print_id(char *file);

int
what_main(int ac, char **av)
{
	char	*name, *gfile;
	int	c, rc = 0;

	while ((c = getopt(ac, av, "")) != -1) {
		switch (c) {
		    default:
			system("bk help -s what");
			return (1);
		}
	}

	for (name = sfileFirst("what", &av[optind], 0);
	    name; name = sfileNext()) {
		gfile = sccs2name(name);
		if (print_id(gfile)) rc = 1;
		free(gfile);
	}
	sfileDone();
	return (rc);
}

private int
print_id(char *file)
{
	int	dotab;
	char	*p;
	MMAP	*m;

	unless (m = mopen(file, "b")) return (-1);
	if ((m->fd == -1) && !m->size) {
		fprintf(stderr, "what: %s not a file\n", file);
		return (-1);
	}

	printf("%s:\n", file);
	p = m->mmap;
	while (p < (m->end - 4)) { /* at least 5 chars left */
		if ((p[0] == '@') && (p[1] == '(') &&
		    (p[2] == '#') && (p[3] == ')')) {
			dotab = 1;
			p += 4;
			while (p < m->end) {
				/* list from ATT what.c */
				if ((*p == 0) || (*p == '\n') || (*p == '"') \
				    || (*p == '\\') || (*p == '>')) {
					break;
				}
				if (dotab && isspace(*p)) {
					p++;
					continue;
				}
				if (dotab) {
					putchar('\t');
					dotab = 0;
				}
				putchar(*p);
				p++;
			}
			/* we can have a null entry above so dotab is set */
			unless (dotab) putchar('\n');
		} else {
			p++;
		}
	}
	mclose(m);
	return (0);
}
