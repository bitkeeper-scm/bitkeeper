#include "system.h"
#include "sccs.h"

extern char *editor, *pager, *bin;
private	int is_command(char *file);

int
help_main(int ac,  char **av)
{
	char	buf[MAXLINE];
	char	help_out[MAXPATH];
	int	c, i = 0, use_pager = 1;
	FILE	*f;

	if (ac == 1) {
		sprintf(buf, "bk gethelp help | %s", pager);
		system(buf);
		return (0);
	}
	while ((c = getopt(ac, av, "p")) != -1) {
		switch (c) {
		case 'p':	use_pager = 0; /* disable pager */ 
				break;
		defaults:	fprintf(stderr,
					"usage: bk help [-p] [topic]\n");
				return (1);
				break;
		}
	}
	sprintf(help_out, "%s/bk_help%d", TMP_PATH, getpid());
	while (av[++i]) {
		sprintf(buf, "bk gethelp help_%s %s > %s",
							av[i], bin, help_out);
		if (system(buf) != 0) {
			if (is_command(av[i])) {
				f = fopen(help_out, "ab");
				fprintf(f,
	"		-------------- %s help ---------------\n\n", av[i]);
				fclose(f);
				sprintf(buf, "bk %s --help >> %s 2>&1",
							    av[i], help_out);
				system(buf);
			} else {
				f = fopen(help_out, "ab");
				fprintf(f, "No help for %s, check spelling.\n",
									av[i]);
				fclose(f);
			}
		}
	}
	if (use_pager) {
		sprintf(buf, "%s %s", pager, help_out);
		system(buf);
	} else {
		f = fopen(help_out, "rt");
		while (fgets(buf, sizeof(buf), f)) {
			fputs(buf, stdout);
		}
		fclose(f);
	
	}
	unlink(help_out);
	return (0);
}

private	int
is_command(char *cmd)
{
	int i;
	extern  struct command cmdtbl[]; /* see bkmain.c */

	for (i = 0; cmdtbl[i].name; i++) {
		if (streq(cmdtbl[i].name, cmd)) return 1;
	}
	return 0;
}
