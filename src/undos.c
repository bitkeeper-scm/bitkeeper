#include "system.h"
#include "sccs.h"

private	void	undos(char *s);
private void	doit(FILE *f, int auto_new_line);
extern	void	platformSpecificInit(char *);

int
undos_main(int ac, char **av)
{
	FILE	*f;
	char	buf[1024];
	int 	fd, c;
	int	auto_new_line = 1;

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

	unless (av[optind]) {
		doit(stdin, auto_new_line);
	} else {
		while (av[optind]) {
			f = fopen(av[optind], "rb");
			unless (f) {
				perror(av[optind]);
				exit(1);
			}
			doit(f, auto_new_line);
			fclose(f);
			optind++;
		}
	}
	return (0);

}

/* kill the newline and the \r */
private	void
undos(register char *s)
{
	static char last = '\0';
	if (!s[0]) return;
	/*
	 * This code is strange because we need to
	 * handle lines longer than the 1K buffer size
	 */
	if ((last == '\r') && (s[0] == '\n')) return;
	while (s[1]) s++;
	last = s[0];
	if (last == '\r') *s-- = 0;
	if ((s[-1] == '\r') && (s[0] == '\n')) {
		s[-1] = '\n';
		s[0] = '\0';
		return;
	}
}

private void
doit(FILE *f, int auto_new_line)
{
	char	buf[1024];

	buf[0] = 0;
	while (fgets(buf, sizeof(buf), f)) {
		undos(buf);
		fputs(buf, stdout);
	}
	if (auto_new_line && !strchr(buf, '\n')) fputc('\n', stdout);
}
