#include "system.h"
#include "sccs.h"

extern char *editor, *pager, *bin;
private	int is_command(char *file);

help_main(int ac,  char **av)
{
	char	buf[MAXLINE];
	char	help_out[MAXPATH];
	int	i = 0;
	FILE	*f;

	platformInit();
	if (ac == 1) {
		sprintf(buf, "%sbk gethelp help | %s", bin, pager);
		system(buf);
		return (0);
	}
	sprintf(help_out, "%s/bk_help%d", TMP_PATH, getpid());
	while (av[++i]) {
		sprintf(buf, "%sbk gethelp help_%s %s > %s",
						bin, av[i], bin, help_out);
		if (system(buf) != 0) {
			if (is_command(av[i])) {
				f = fopen(help_out, "ab");
				fprintf(f,
	"		-------------- %s help ---------------\n\n", av[i]);
				fclose(f);
				sprintf(buf, "%sbk %s --help >> %s 2>&1",
							bin, av[i], help_out);
				system(buf);
			} else {
				f = fopen(help_out, "ab");
				fprintf(f, "No help for %s, check spelling.\n",
									av[i]);
				fclose(f);
			}
		}
	}
	sprintf(buf, "%s %s", pager, help_out);
	system(buf);
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
