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
	int	sort(const void *a, const void *b);
	mem_t	*memlist = calloc(sizeof(mem_t), 1);
	mem_t	*mem = memlist;

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
			assert((int)(long)lines[0] == (size*2));
			size *= 2;
			next++;
		} else {
			lines[next++] = p;
		}
		bytes += strlen(buf);
	}
	qsort((void*)&lines[1], next-1, sizeof(char *), sort);
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

int
sort(const void *a, const void *b)
{
	char	*l, *r;

	l = *(char**)a;
	r = *(char**)b;
	return (strcmp(l, r));
}
