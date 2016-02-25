/*
 * Copyright 2000-2001,2003,2005-2006,2009-2012,2015-2016 BitMover, Inc
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

private int annocat(char *file, int flags, int pipe);

int
cat_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	skip_bin = 0, rc = 0;
	int	pnames = getenv("BK_PRINT_EACH_NAME") != 0;
	int	c, gfile;
	int	aflags = 0;
	int	pipe = 0;

	while ((c = getopt(ac, av, "A:a:B", 0)) != -1) {
		switch (c) {
		    case 'A':
		    	pipe = 1;
			/* fall through */
		    case 'a':
			aflags = annotate_args(aflags, optarg);
			if (aflags == -1) usage();
			if (aflags) {
				if (c == 'A') aflags |= GET_ALIGN;
			} else {
				pipe = 0;	// -Anone
			}
			break;
		    case 'B': skip_bin = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	for (name = sfileFirst("cat", &av[optind], SF_NOCSET);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, INIT_NOCKSUM)) {
			rc |= 1;
			continue;
		}
		gfile = (access(s->gfile, R_OK) == 0);
		unless (gfile || HASGRAPH(s)) {
			rc |= 1;
			sccs_free(s);
			continue;
		}
		if (skip_bin && (BINARY(s) ||
		    (gfile && !HASGRAPH(s) && !ascii(s->gfile)))) {
			sccs_free(s);
			continue;
		}
		if (pnames) {
			printf("|FILE|%s|CRC|%lu\n", s->gfile,
			    adler32(0, s->gfile, strlen(s->gfile)));
			fflush(stdout);
		}
		if (gfile) {
			if (aflags) {
				rc |= annocat(s->gfile, aflags, pipe) ? 1 : 0;
			} else {
				rc |= cat(s->gfile) ? 1 : 0;
			}
			sccs_free(s);
			continue;
		}
		if (sccs_get(s, 0, 0, 0, 0, aflags|SILENT, 0, stdout)) {
			rc |= 1;
		}
		sccs_free(s);
	}
	if (sfileDone()) rc = 1;
	return (rc);
}

private int
annocat(char *file, int aflags, int pipe)
{
	FILE	*f;
	char	*p;
	char	*name = 0;
	int	lineno = 0;
	time_t	tt;
	char	fake[MAXPATH + 100];

	unless (f = fopen(file, "r")) return (-1);
	if (aflags & GET_MODNAME) {
		name = basenm(file);
	} else if (aflags & GET_RELPATH) {
		name = file;
	}
	fake[0] = 0;
	if (aflags & GET_PREFIXDATE) {
		tt = time(0);
		strftime(fake, sizeof(fake), "%Y/%m/%d\t", localtimez(&tt, 0));
	}
	if (aflags & GET_USER) strcat(fake, "?\t");
	if (aflags & GET_REVNUMS) strcat(fake, "?\t");
	if (aflags & GET_SERIAL) strcat(fake, "?\t");
	while (p = fgetline(f)) {
		if (name) {
			fputs(name, stdout);
			fputc('\t', stdout);
		}
		if (fake[0]) fputs(fake, stdout);
		if (aflags & GET_LINENUM) printf("%d\t", ++lineno);
		if (pipe) fputs("| ", stdout);
		puts(p);
	}
	fclose(f);
	return (0);
}
