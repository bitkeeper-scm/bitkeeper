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
	int	c, use_pager = 1;
	char	*opt = 0, *synopsis = "", *p;
	char	*topic;
	char	*file = 0;
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

	bktmp(out);
	if (!(topic = av[optind]) || av[optind+1]) {
		topic = "help";
	}
	assert(topic);
	if (strieq(topic, "release-notes")) {
		sprintf(buf, "%s/RELEASE-NOTES", bin);
		fileCopy(buf, out);
	} else if (opt) {
		if (file) {
			sprintf(buf,
			    "bk helpsearch -f'%s' -%s '%s' >> '%s'",
			    file, opt, topic, out);
		} else {
			sprintf(buf,
			    "bk helpsearch -%s '%s' >> '%s'",
			    opt, topic, out);
		}
		system(buf);
	} else {
		upgrade_maybeNag(out);
		p = aprintf("bk-%s", topic);
		if (file) {
			sprintf(buf, "bk gethelp %s -f'%s' '%s' '%s' >> '%s'",
			    synopsis, file, topic, bin, out);
		} else if (which(p)) {
			sprintf(buf, "bk %s --help", topic);
		} else {
			sprintf(buf, "bk gethelp %s '%s' '%s' >> '%s'",
			    synopsis, topic, bin, out);
		}
		if (system(buf) != 0) {
			sprintf(buf, "bk getmsg -= '%s' >> '%s'",
			    topic, out);
			if (system(buf) != 0) {
				f = fopen(out, "a");
				fprintf(f,
				    "No help for %s, check spelling.\n",
				    topic);
				fclose(f);
			}
		}
		free(p);
	}
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
