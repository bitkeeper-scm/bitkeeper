#include "system.h"
#include "sccs.h"

private	void	undos(char *s);
private void	undos_stdin(void);
int	auto_new_line = 1;

int
undos_main(int ac, char **av)
{
	int 	c;

	if (ac == 2 && !strcmp("--help", av[1])) {
		system("bk help undos");
		return (0);
	}

 	while ((c = getopt(ac, av, "n")) != -1) { 
		switch (c) {
		    case 'n': auto_new_line = 0; break;		/* doc 2.0 */
		    default:
			system("bk help -s undos");
			return (1);
		}
	}
	unless (av[optind]) {
		undos_stdin();
		return (0);
	}
	while (av[optind]) {
		undos(av[optind++]);
	}
	return (0);
}

/*
 * Both of these routines strip out \r's.
 * If you have "text\r More text" and you have auto_new_line on then
 * it does a s|\r|\n| otherwise it does a s|\r||.
 */
private void
undos_stdin()
{
	int	c;

	while ((c = getchar()) != EOF) {
		unless (c == '\r') {
			putchar(c);
			continue;
		}
again:		switch (c = getchar()) {
		    case EOF:
			if (auto_new_line) putchar('\n');
			return;
		    case '\n':
		    	putchar(c);
			break;
		    case '\r':
			if (auto_new_line) putchar('\n');
			goto again;
		    default:
		    	putchar(c);
			break;
		}
	}
}

private void
undos(char *file)
{
	MMAP	*m = mopen(file, "r");
	char	*p;
	u32	sz;

	unless (m) {
		/* perror(file);  mopen already printed the error */
		exit(1);
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
