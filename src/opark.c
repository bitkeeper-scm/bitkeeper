/*
 * Copyright 2000-2005,2013,2016 BitMover, Inc
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

private int listParkFile(void);
private int purgeParkFile(int id);

int
opark_main(int ac, char **av)
{
	char	parkfile[MAXPATH] = "", changedfile[MAXPATH] = "";
	char	*diffsopts, *comment = 0;
	int 	lflag  = 0, qflag = 0, purge = 0, c, status, try = 0;
	FILE	*f;

	while ((c = getopt(ac, av, "lp:qy:", 0)) != -1) {
		switch (c) {
		    case 'l':	lflag = 1; break;		/* doc 2.0 */
		    case 'p':	purge = atoi(optarg); break;	/* doc 2.0 */
		    case 'q':	qflag = 1; break;		/* doc 2.0 */
		    case 'y':	comment = optarg; break;	/* doc 2.0 */
		    default: bk_badArg(c, av);
		}
	}

	if (proj_cd2root()) {
		fprintf(stderr, "Can't find package root\n");
		return (0);
	}

	if (lflag) return (listParkFile());
	if (purge) return (purgeParkFile(purge));

	bktmp(changedfile);
	sysio(0, changedfile, 0, "bk", "gfiles", "-c", SYS);
	if (size(changedfile) <= 0) {
empty:		unless (qflag) printf("Nothing to park\n");
		if (parkfile[0]) unlink(parkfile);
		if (changedfile[0]) unlink(changedfile);
		return (0);
	}

	/*
	 * XXX TODO What do we do when user tries to park a binary file?
	 */

	do {
		sprintf(parkfile, "%s/parkfile-%d", BKTMP, ++try);
	} while (exists(parkfile));

	diffsopts = qflag ? "-u" : "-uv";
	status = sysio(changedfile, parkfile, 0,
					"bk", "diffs", diffsopts, "-", SYS);
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		fprintf(stderr, "bk park failed\n");
		unlink(changedfile);
		return (1);
	}
	if (size(parkfile) <= 0) goto empty;

	/*
	 * Store comment in comment file
	 */
	if (comment) {
		sprintf(parkfile, "%s/parkcomment-%d", BKTMP, try);
		f = fopen(parkfile, "w"); assert(f);
		fprintf(f, "%s\n", comment);
		fclose(f);
	}

	sysio(changedfile, 0, 0, "bk", "unedit", "-", SYS); /* careful */
	unless (qflag) fprintf(stderr, "Parked changes in %s.\n", parkfile);
	unlink(changedfile);
	return (0);
}

private int
listParkFile(void)
{
	char	**d;
	int	i, j;
	char	parkCommentFile[MAXPATH];

	unless (d = getdir(BKTMP)) return (0);
	EACH (d) {
		if ((strlen(d[i]) > 9) && strneq(d[i], "parkfile-", 9)) {
			printf("%s\n", d[i]);
			j = atoi(&(d[i][9]));
			sprintf(parkCommentFile, "%s/parkcomment-%d", BKTMP, j);
			if (exists(parkCommentFile)) {
				printf(
"=============================== Comment ==================================\n");
				cat(parkCommentFile);
				printf(
"==========================================================================\n");
			}
		}
	}
	freeLines(d, free);
	return (0);
}

private int
purgeParkFile(int id)
{
	char	parkfile[MAXPATH];

	sprintf(parkfile, "%s/parkfile-%d", BKTMP, id);
	if (unlink(parkfile)) {
		perror(parkfile);
		return (1);
	}
	printf("%s purged\n", parkfile);
	sprintf(parkfile, "%s/commentfile-%d", BKTMP, id);
	unlink(parkfile);
	return (0);
}

private int
do_unpark(int id)
{
	char	parkfile[MAXPATH], parkCommentFile[MAXPATH];
	int	rc;

	sprintf(parkfile, "%s/parkfile-%d", BKTMP, id);
	unless (exists(parkfile)) {
		fprintf(stderr, "%s does not exist\n", parkfile);
		return (1);
	}

	fprintf(stderr, "Unparking %s\n", parkfile);
	sprintf(parkCommentFile, "%s/parkcomment-%d", BKTMP, id);
	if (exists(parkCommentFile)) {
		printf(
"=============================== Comment ==================================\n");
			cat(parkCommentFile);
		printf(
"==========================================================================\n");
	}
	rc = sysio(parkfile, 0, 0, "bk", "patch", "-p1", "-g1", "-fu", SYS);
	if (rc) {
		fprintf(stderr, "Cannot unpark %s\n", parkfile);
		/* Do not unlink the parkfile,  user may want to re-try */
		return (1);
	}
	fprintf(stderr, "Unpark of %s is successful\n", parkfile);
	unlink(parkfile); /* careful */
	unlink(parkCommentFile); /* careful */
	return (0);
}

int
ounpark_main(int ac, char **av)
{
	char	**d;
	int	i, j, top = 0;

	if (proj_cd2root()) {
		fprintf(stderr, "Can't find package root\n");
		return (0);
	}
	if (av[1]) return (do_unpark(atoi(av[1]))); /* unpark parkefile-n */

	unless (d = getdir(BKTMP)) {
empty:		fprintf(stderr, "No parkfile found\n");
		return (0);
	}

	/*
	 * The parkfile list is a LIFO, last one parked got unparked first
	 */
	EACH (d) {
		if ((strlen(d[i]) > 9) && strneq(d[i], "parkfile-", 9)) {
			j = atoi(&(d[i][9]));
			if (j > top) top = j; /* get the highest number */
		}
	}
	freeLines(d, free);
	if (top == 0) goto empty;
	return (do_unpark(top));
}
