/*
 * Copyright 2012,2015-2016 BitMover, Inc
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

#include "sccs.h"

private	int	unrm(char *file, char type, void *data);

private	char	**deletes = 0;

int
unrm_main(int ac, char **av)
{
	int	c, i;
	int	force = 0;
	int	quiet = 0;
	int	ret = 0;
	char	**data;
	char	*file, *fileexp;
	char	buf[MAXPATH];

	while ((c = getopt(ac, av, "fq", 0)) != -1) {
		switch (c) {
		    case 'f': force = 1; break;
		    case 'q': quiet = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind] && !av[optind+1]) usage();

	unless (proj_root(0)) {
		fprintf(stderr, "unrm: must be run from a repository.\n");
		return (1);
	}
	file = proj_relpath(0, av[optind]);
	fileexp = aprintf("|%s|", file);
	proj_cd2root();
	walksfiles("BitKeeper/deleted", unrm, file);

	unless (deletes) {
		unless (quiet) {
			printf(
			    "------------------\n"
			    "No matching files.\n"
			    "------------------\n");
		}
		return (2);
	}
	/*
	 * If there is only one match, and it is a exact match[1], don't
	 * ask for confirmation.
	 *
	 * [1] exact match is implied if the file arg has a slash; without
	 *     the slash, it's just a basenm *except* in the case where
	 *     the file we are looking for is in the root of the repository
	 */
	if ((nLines(deletes) == 1) &&
	    (strchr(file, '/') || strstr(deletes[1], fileexp))) force = 1;

	/*
	 * Note to reviewer: the first field is fixed length unsigned hex
	 * of (-date); therefore, an ascii sort will put the newest first.
	 */
	sortLines(deletes, 0);

	if (force && (nLines(deletes) > 1) && !quiet) {
		printf("%d possible files found, choosing newest\n",
		    nLines(deletes));
	}
	EACH(deletes) {
		data = splitLine(deletes[i], "|", 0);

		unless (force) {
			sprintf(buf, "bk prs -r%s -hnd'"
			    "-------- Match %d of %d -----------------------\n"
			    "File:        :GFILE:\n"
			    "Deleted on:  :D: :T::TZ: by :USER:@:HOST:\n'"
			    " '%s'",
			    data[3], i, nLines(deletes), data[2]);
			system(buf);
			printf("Top delta before it was deleted:\n");
			fflush(stdout);
			sprintf(buf, "-r%s", data[5]);
			putenv("BK_PAGER=cat");
			sys("bk", "log", "-h", buf, data[2], SYS);
			printf("Undelete this file back to \"%s\"? (y|n|q)>",
			    data[4]);
			fflush(stdout);
			unless (fnext(buf, stdin)) strcpy(buf, "n");
			switch(tolower(buf[0])) {
			    case 'y': break;
			    case 'q':
				freeLines(data, free);
				goto done;
			    default: {
				printf("File skipped.\n\n");
				fflush(stdout);
				freeLines(data, free);
				continue;
			    }
			}
		}
		fflush(stdout);

		if (sys("bk", "mv", "-uf", data[2], data[4], SYS)) {
			fprintf(stderr, "NOT moving \"%s\" -> \"%s\"\n",
			    data[2], data[4]);
			ret = 1;
		} else unless (quiet) {
			printf("Moving \"%s\" -> \"%s\"\n", data[2], data[4]);
		}
		freeLines(data, free);
		break;
	}
done:	freeLines(deletes, free);
	free(file); free(fileexp);
	return (ret);
}


private int
unrm(char *file, char type, void *data)
{
	char	*path = data, *drev = 0;
	sccs	*s;
	ser_t	d;

	s = sccs_init(file, SILENT);
	unless (s && HASGRAPH(s)) {
		fprintf(stderr, "unrm: can't open %s skipping...\n", file);
		if (s) sccs_free(s);
		return (0);
	}
	for (d = TABLE(s); d >= TREE(s); --d) {
		if (strneq(PATHNAME(s, d), "BitKeeper/deleted/", 18)) {
			drev = REV(s, d);
			continue;
		}
		if (strchr(path, '/')) {
			/* exact match */
			unless (streq(PATHNAME(s, d), path)) break;
		} else {
			/* basename match */
			unless (streq(basenm(PATHNAME(s, d)), path)) break;
		}
		/* found a match */
		deletes = addLine(deletes,
		    aprintf("%08x|%s|%s|%s|%s",
			-(u32)DATE(s, d), file, drev,
			PATHNAME(s, d), REV(s, d)));
		break;
	}
	sccs_free(s);
	return (0);
}
