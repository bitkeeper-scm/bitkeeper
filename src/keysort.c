#include "system.h"
#include "sccs.h"

/*
 * Sort the keys in the changeset file.
 * All we are trying to do is to group the same files together.
 */
keysort_main(int ac, char **av)
{
	char	buf[MAXKEY*2];
	int	size = 10000;
	char	**lines = calloc(size, sizeof(char *));
	int	next = 1;
	int	bytes = 0;
	int	i;
	int	sort(const void *a, const void *b);

	unless (lines) {
		perror("malloc");
		exit(1);
	}
	lines[0] = (char *)(long)(size);
	while (fgets(buf, sizeof(buf), stdin)) {
		if (next == size) {
			lines = addLine(lines, strdup(buf));
			assert((int)(long)lines[0] == (size*2));
			size *= 2;
		} else {
			lines[next++] = strdup(buf);
		}
		bytes += strlen(buf);
	}
	qsort((void*)&lines[1], next-1, sizeof(char *), sort);
	EACH(lines) fputs(lines[i], stdout);
	return (0);
}

int
sort(const void *a, const void *b)
{
	char	*l, *r;

	l = *(char**)a;
	r = *(char**)b;
	return (strcmp(l, r));
}
