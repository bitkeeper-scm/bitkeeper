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
	char	*opt = 0, *synopsis = "";
	FILE	*f, *f1;
	char	*file = 0;
	char 	*new_av[2] = {"help", 0 };

	if (ac == 2 && streq("--help", av[1])) {
		sprintf(buf, "bk help help | %s", pager);
		system(buf);
		return (0);
	}

	while ((c = getopt(ac, av, "af:kps")) != -1) {
		switch (c) {
		    case 'a': opt = "al"; break;		/* doc 2.0 */
		    case 'f': file = optarg; break;		/* doc 2.0 */
		    case 'k': opt = "l"; break;	/* like man -k *//* doc 2.0 */
		    case 's': synopsis = "-s";			/* undoc 2.0 */
			      /* fall thru */
		    case 'p': use_pager = 0; break;		/* doc 2.0 */
		    default:
			system("bk help -s help");
			return (1);
		}
	}
	sprintf(out, "%s/bk_help%d", TMP_PATH, getpid());
	unlink(out);
	unless (av[optind]) {
		av = new_av;
		optind = -0;
	}
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
		if (file) {
			sprintf(buf,
			    "bk gethelp %s -f%s %s %s >> %s",
			     		synopsis, file, av[i], bin, out);
		} else {
			sprintf(buf, "bk gethelp %s %s %s >> %s",
					synopsis, av[i], bin, out);
		}
		if (system(buf) != 0) {
			f = fopen(out, "ab");
			fprintf(f, "No help for %s, check spelling.\n", av[i]);
			fclose(f);
		}
	}
print:
	if (use_pager) {
		sprintf(buf, "%s %s", pager, out);
		system(buf);
	} else {
		f = fopen(out, "rt");
		f1 = (*synopsis) ? stderr : stdout;
		while (fgets(buf, sizeof(buf), f)) {
			fputs(buf, f1);
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
