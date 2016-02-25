/*
 * Copyright 2000-2002,2004,2006-2010,2015-2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"

typedef	struct	mem {
	char	*data;		/* pointer to the full block */
	char	*avail;		/* pointer to the first free byte */
	int	left;		/* avail..end */
	struct	mem *next;
} mem_t;

private void	slurp(FILE *f, char ***linesp, mem_t **memp, int *np);
private	int	(*sortfcn)(const void *a, const void *b);
private	int	sortfield = 0;

private mem_t	*
moreMem(mem_t *m)
{
	mem_t	*n = new(mem_t);

	unless (n && (n->data = malloc(n->left = 4<<20))) return (0);
	n->avail = n->data;
	n->next = m;
	return (n);
}

/*
 * sort on the Nth field.  Split by whitespace only.
 */
private	int
field_sort(const void *a, const void *b)
{
	char	*na, *nb;
	int	i;

	na = *(char **)a;
	na += strspn(na, " \t");
	for (i = 1; i < sortfield; i++) {
		na += strcspn(na, " \t");
		na += strspn(na, " \t");
	}
	nb = *(char **)b;
	nb += strspn(nb, " \t");
	for (i = 1; i < sortfield; i++) {
		nb += strcspn(nb, " \t");
		nb += strspn(nb, " \t");
	}
	if (i = sortfcn(&na, &nb)) return (i);
	return (string_sort(a, b));
}

/*
 * A simple sort clone.  This is often used to sort keys in the ChangeSet
 * file so the allocations are sized to do that efficiently.
 */
int
sort_main(int ac, char **av)
{
	char	**lines = allocLines(50000);
	int	n = 0;
	int	uflag = 0, rflag = 0, nflag = 0, showcount = 0;
	int	i, c, count = 1;
	mem_t	*mem;
	FILE	*f;
	longopt	lopts[] = {
		{ "count", 300 },
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "k:nru", lopts)) != -1) {
		switch (c) {
		    case 'k': sortfield = atoi(optarg); break;
		    case 'n': nflag = 1; break;
		    case 'r': rflag = 1; break;
		    case 'u': uflag = 1; break;
		    case 300: showcount = 1; break; // --count
		    default: bk_badArg(c, av);
		}
	}

	unless (lines && (mem = moreMem(0))) {
		perror("malloc");
		exit(1);
	}
	if (av[optind]) {
		while (av[optind]) {
			unless (f = fopen(av[optind], "r")) {
				perror(av[optind]);
				exit(1);
			}
			slurp(f, &lines, &mem, &n);
			fclose(f);
			optind++;
		}
	} else {
		slurp(stdin, &lines, &mem, &n);
	}
	sortfcn = nflag ? number_sort : string_sort;
	qsort((void*)&lines[1], n, sizeof(char *),
	    (sortfield ? field_sort : sortfcn));
	if (rflag) reverseLines(lines);
	EACH(lines) {
		/* lookahead */
		if (uflag && ((i+1) <= nLines(lines)) &&
		    streq(lines[i], lines[i+1])) {
			count++;
			continue;
		}
		if (showcount) printf("%d ", count);
		puts(lines[i]);
		count = 1;
	}
	free(lines);
	while (mem) {
		mem_t	*memnext = mem->next;
		free(mem->data);
		free(mem);
		mem = memnext;
	}
	return (0);
}

private void
slurp(FILE *f, char ***linesp, mem_t **memp, int *np)
{
	char	**lines = *linesp;
	mem_t	*mem = *memp;
	char	*p;
	int	i;
	int	n = *np;
	char	buf[MAXKEY*2];

	while (fgets(buf, sizeof(buf), f)) {
		chomp(buf);
		i = strlen(buf) + 1;
		unless (mem->left > i) {
			unless (mem = moreMem(mem)) {
				perror("malloc");
				exit(1);
			}
		}
		p = mem->avail;
		strcpy(p, buf);
		mem->avail += i;
		mem->left -= i;
		lines = addLine(lines, p);
		n++;
	}
	*linesp = lines;
	*memp = mem;
	*np = n;
}
