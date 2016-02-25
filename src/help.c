/*
 * Copyright 2000-2002,2004-2006,2010,2015-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
	char	**ALL = 0;
	char	buf[MAXLINE];
	char	out[MAXPATH];

	while ((c = getopt(ac, av, "af:kps", 0)) != -1) {
		switch (c) {
		    case 'a': opt = "al"; break;		/* doc 2.0 */
		    case 'f': file = optarg; break;		/* doc 2.0 */
		    case 'k': opt = "l"; break;	/* like man -k *//* doc 2.0 */
		    case 's': synopsis = "-s";			/* undoc 2.0 */
			      /* fall thru */
		    case 'p': use_pager = 0; break;		/* doc 2.0 */
		    default: bk_badArg(c, av);
		}
	}

	/* Needs to match what is in man/man2help/help2sum.pl */
	ALL = addLine(ALL, "All");
	ALL = addLine(ALL, "topic");
	ALL = addLine(ALL, "topics");
	ALL = addLine(ALL, "command");
	ALL = addLine(ALL, "commands");
	ALL = addLine(ALL, "Utility");

	bktmp(out);
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
				    "bk helpsearch -f'%s' -%s '%s' >> '%s'",
				    file, opt, av[i], out);
			} else {
				sprintf(buf,
				    "bk helpsearch -%s '%s' >> '%s'",
				    opt, av[i], out);
			}
			system(buf);
		}
		goto print;
	}
	upgrade_maybeNag(out);
	for (i = optind; av[i]; i++) {
		if (file) {
			sprintf(buf,
			    "bk gethelp %s -f'%s' '%s' '%s' >> '%s'",
			     		synopsis, file, av[i], bin, out);
		} else {
			sprintf(buf, "bk gethelp %s '%s' '%s' >> '%s'",
					synopsis, av[i], bin, out);
		}
		if (system(buf) != 0) {
			sprintf(buf, "bk getmsg -= '%s' >> '%s'", av[i], out);
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
