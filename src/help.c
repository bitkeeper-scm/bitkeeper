#include "system.h"
#include "sccs.h"

extern char *editor, *pager, *bin;
private	int is_command(char *file);

int
help_main(int ac,  char **av)
{
	char	buf[MAXLINE];
	char	out[MAXPATH];
	int	c, i = 0, use_pager = 1;
	char	*opt = 0;
	FILE	*f;
	char	*file = 0;

	if (ac == 1) {
		sprintf(buf, "bk help help | %s", pager);
		system(buf);
		return (0);
	}
	while ((c = getopt(ac, av, "af:kp")) != -1) {
		switch (c) {
		    case 'a': opt = "al"; break;
		    case 'f': file = optarg; break;
		    case 'k': opt = "l"; break;		/* like man -k */
		    case 'p': use_pager = 0; break;
		    defaults:
			fprintf(stderr,
			    "usage: bk help [-akp] [-f<helptxt>] [topic]\n");
			return (1);
		}
	}
	sprintf(out, "%s/bk_help%d", TMP_PATH, getpid());
	unlink(out);
	if (opt) {
		for (av[i = optind]; av[i]; i++) {
			if (file) {
				sprintf(buf,
				    "bk helpsearch -f%s -%s %s >> %s",
				    file, opt, av[i], out);
			} else {
				sprintf(buf,
				    "bk helpsearch -%s %s >> %s",
				    opt, av[i], out);
			}
			system(buf);
		}
		goto print;
	}
	for (av[i = optind]; av[i]; i++) {
		sprintf(buf,
		    "bk gethelp %s %s >> %s", av[i], bin, out);
		if (file) {
			sprintf(buf,
			    "bk gethelp -f%s %s %s >> %s",
			    file, av[i], bin, out);
		} else {
			sprintf(buf, "bk gethelp %s %s >> %s", av[i], bin, out);
		}
		if (system(buf) != 0) {
			if (is_command(av[optind])) {
				f = fopen(out, "ab");
				fprintf(f,
	"		-------------- %s help ---------------\n\n", av[i]);
				fclose(f);
				sprintf(buf,
				    "bk %s --help >> %s 2>&1", av[i], out);
				system(buf);
			} else {
				f = fopen(out, "ab");
				fprintf(f,
				    "No help for %s, check spelling.\n", av[i]);
				fclose(f);
			}
		}
	}
print:
	if (use_pager) {
		sprintf(buf, "%s %s", pager, out);
		system(buf);
	} else {
		f = fopen(out, "rt");
		while (fgets(buf, sizeof(buf), f)) {
			fputs(buf, stdout);
		}
		fclose(f);
	
	}
	unlink(out);
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
