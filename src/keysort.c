#include "system.h"
#include "sccs.h"

typedef	struct	mem {
	char	*data;		/* pointer to the full block */
	char	*avail;		/* pointer to the first free byte */
	int	left;		/* avail..end */
	struct	mem *next;
} mem_t;

mem_t	*
moreMem(mem_t *m)
{
	mem_t	*n = calloc(sizeof(mem_t), 1);

	unless (n && (n->data = malloc(n->left = 4<<20))) return (0);
	n->avail = n->data;
	while (m->next) m = m->next;
	m->next = n;
	return (n);
}

/*
 * Sort the keys in the changeset file.
 * All we are trying to do is to group the same files together.
 */
int
keysort_main(int ac, char **av)
{
	char	buf[MAXKEY*2];
	char	*p;
	int	size = 50000;
	char	**lines = calloc(size, sizeof(char *));
	int	next = 1;
	int	bytes = 0;
	int	i;
	int	string_sort(const void *a, const void *b);
	mem_t	*memlist = calloc(sizeof(mem_t), 1);
	mem_t	*mem = memlist;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help keysort");
		return (0);
	}

	unless (lines && mem) {
		perror("malloc");
		exit(1);
	}
	unless (mem->data = malloc(mem->left = 4<<20)) {
		perror("malloc");
		exit(1);
	}
	mem->avail = mem->data;
	lines[0] = (char *)(long)(size);
	while (fgets(buf, sizeof(buf), stdin)) {
		i = strlen(buf) + 1;
		unless (mem->left > i) {
			unless (mem = moreMem(memlist)) {
				perror("malloc");
				exit(1);
			}
		}
		p = mem->avail;
		strcpy(p, buf);
		mem->avail += i;
		mem->left -= i;
		if (next == size) {
			lines = addLine(lines, p);
			size = (int)(long)lines[0];
			next++;
		} else {
			lines[next++] = p;
		}
		bytes += strlen(buf);
	}
	qsort((void*)&lines[1], next-1, sizeof(char *), string_sort);
	EACH(lines) fputs(lines[i], stdout);
	free(lines);
	for (mem = memlist; mem; ) {
		memlist = mem->next;
		free(mem->data);
		free(mem);
		mem = memlist;
	}
	return (0);
}

/*
 * Sort stdin in a way that is consistent across all platforms (avoids locale).
 */
int
sort_main(int ac, char **av)
{
	char	buf[MAXKEY*2];
	char	*last = 0, **lines = 0;
	int	i, c, uflag = 0;
	size_t	n = 0;
	int	string_sort(const void *a, const void *b);

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help sort");
		return (0);
	}

	while ((c =getopt(ac, av, "u")) != -1) {
		switch (c) {
		    case 'u': uflag = 1; break;
		    default:
			system("bk help -s sort");
		}
	}

	setmode(0, _O_TEXT);  /* no-op on unix */
	while (fgets(buf, sizeof(buf), stdin)) {
		chop(buf);
		lines = addLine(lines, strdup(buf));
		n++;
	}
	qsort((void*)&lines[1], n, sizeof(char *), string_sort);
	EACH(lines) {
		if (uflag && last && streq(last, lines[i])) continue;
		fprintf(stdout, "%s\n", lines[i]);
		last = lines[i];
	}
	free(lines);
	return (0);
}

int
string_sort(const void *a, const void *b)
{
	char	*l, *r;

	l = *(char**)a;
	r = *(char**)b;
	return (strcmp(l, r));
}
