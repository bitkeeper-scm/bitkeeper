/*
 * Copyright 1997-2000,2005-2007,2015-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * what - look for SCCS what strings.
 */
#include "system.h"
#include "sccs.h"

private	int	print_id(char *file);

int
what_main(int ac, char **av)
{
	char	*name, *gfile;
	int	c, rc = 0;

	while ((c = getopt(ac, av, "", 0)) != -1) {
		switch (c) {
		    default: bk_badArg(c, av);
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
