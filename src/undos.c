#include "system.h"
#include "sccs.h"

private	void	undos(char *s);
extern	void	platformSpecificInit(char *);
int	auto_new_line = 1;

int
undos_main(int ac, char **av)
{
	FILE	*f;
	char	buf[1024];
	int 	c;

	if (ac == 2 && !strcmp("--help", av[1])) {
		system("bk help undos");
		return (0);
	}

	platformSpecificInit(NULL);
 	while ((c = getopt(ac, av, "n")) != -1) { 
		switch (c) {
		    case 'n': auto_new_line = 0; break;
		    default:
			system("bk help -s undos");
			return (1);
		}
	}
	while (av[optind]) {
		undos(av[optind++]);
	}
	return (0);
}

private void
undos(char *file)
{
	MMAP	*m = mopen(file, "r");
	char	*p;
	u32	sz;

	unless (m) {
		perror(file);
	}
	for (p = m->where, sz = m->size; sz--; p++) {
		unless (*p == '\r') {
			putchar(*p);
			continue;
		}
		if (p[1] == '\n') continue;
		if (auto_new_line) putchar('\n');
	}
	mclose(m);
}
