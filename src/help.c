#include "system.h"
#include "sccs.h"

int
help_main(int ac,  char **av)
{
	FILE	*f, *f1;
	pid_t	pid = 0;
	int	c, i = 0, use_pager = 1;
	char	*opt = 0, *synopsis = "";
	char	*file = 0;
	char 	*new_av[2] = {"help", 0 };
	char	buf[MAXLINE];
	char	out[MAXPATH];

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
	bktmp(out, "help");
	if (av[i=optind] && 
	    (streq(av[i], "bkl") ||
	    streq(av[i], "bkcl") || streq(av[i], "license"))) {
		sysio(0, out, 0, "bk", "_eula", "-S", SYS);
		goto print;
	}
	if (av[i=optind] && !strcasecmp(av[i], "release-notes")) {
		sprintf(buf, "%s/RELEASE-NOTES", bin);
		fileCopy(buf, out);
		goto print;
	}
	unless (av[optind]) {
		av = new_av;
		optind = -0;
	}
	if (opt) {
		for (i = optind; av[i]; i++) {
			if (file) {
				sprintf(buf,
				    "bk helpsearch -f%s -%s %s >> '%s'",
				    file, opt, av[i], out);
			} else {
				sprintf(buf,
				    "bk helpsearch -%s %s >> '%s'",
				    opt, av[i], out);
			}
			system(buf);
		}
		goto print;
	}
	for (i = optind; av[i]; i++) {
		if (file) {
			sprintf(buf,
			    "bk gethelp %s -f%s %s %s >> '%s'",
			     		synopsis, file, av[i], bin, out);
		} else {
			sprintf(buf, "bk gethelp %s %s %s >> '%s'",
					synopsis, av[i], bin, out);
		}
		if (system(buf) != 0) {
			sprintf(buf, "bk getmsg -= %s >> '%s'", av[i], out);
			if (system(buf) != 0) {
				f = fopen(out, "a");
				fprintf(f,
				    "No help for %s, check spelling.\n", av[i]);
				fclose(f);
			}
		}
	}
print:
	if (use_pager) pid = mkpager();
	f = fopen(out, "rt");
	f1 = (*synopsis) ? stderr : stdout;
	while (fgets(buf, sizeof(buf), f)) {
		fputs(buf, f1);
		if (fflush(f1)) break;	/* so the pager can quit */
	}
	fclose(f);
	unlink(out);
	if (pid > 0) {
		fclose(f1);
		waitpid(pid, 0, 0);
	}
	return (0);
}
