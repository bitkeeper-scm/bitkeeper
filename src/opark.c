/*
 * Copyright (c) 2000, Andrew Chang
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

	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr,
		    "Park is deprecated interface, please do not use it\n");
		return (0);
	}

	while ((c = getopt(ac, av, "lp:qy:")) != -1) {
		switch (c) {
		    case 'l':	lflag = 1; break;		/* doc 2.0 */
		    case 'p':	purge = atoi(optarg); break;	/* doc 2.0 */
		    case 'q':	qflag = 1; break;		/* doc 2.0 */
		    case 'y':	comment = optarg; break;	/* doc 2.0 */
		    default: 	fprintf(stderr,
			   "Usage: bk park [-l] [-q] [-p num] [ -y comment]\n");
				return (1);
		}
	}

	if (proj_cd2root()) {
		fprintf(stderr, "Can't find package root\n");
		return (0);
	}

	if (lflag) return (listParkFile());
	if (purge) return (purgeParkFile(purge));

	bktmp(changedfile, 0);
	sysio(0, changedfile, 0, "bk", "sfiles", "-c", SYS);
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
		f = fopen(parkfile, "wb"); assert(f);
		fprintf(f, "%s\n", comment);
		fclose(f);
	}

	sysio(changedfile, 0, 0, "bk", "unedit", "-", SYS); /* careful */
	unless (qflag) fprintf(stderr, "Parked changes in %s.\n", parkfile);
	unlink(changedfile);
	return (0);
}

private int
listParkFile()
{
	struct	dirent *e;
	DIR	*dh;
	int	i;
	char	parkCommentFile[MAXPATH];

	dh = opendir(BKTMP);
	unless (dh) return (0);
	while ((e = readdir(dh)) != NULL) {
		if ((strlen(e->d_name) > 9) &&
		    strneq(e->d_name, "parkfile-", 9)) {
			printf("%s\n", e->d_name);
			i = atoi(&(e->d_name[9]));
			sprintf(parkCommentFile, "%s/parkcomment-%d", BKTMP, i);
			if (exists(parkCommentFile)) {
				printf(
"=============================== Comment ==================================\n");
				cat(parkCommentFile);
				printf(
"==========================================================================\n");
			}
		}
	}
	closedir(dh);
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
	struct	dirent *e;
	DIR	*dh;
	int	i, top = 0;

	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr,
		    "Unpark is deprecated interface, please do not use it\n");
		return (0);
	}


	if (proj_cd2root()) {
		fprintf(stderr, "Can't find package root\n");
		return (0);
	}

	if (av[1]) return (do_unpark(atoi(av[1]))); /* unpark parkefile-n */

	dh = opendir(BKTMP);
	unless (dh) {
empty:		fprintf(stderr, "No parkfile found\n");
		return (0);
	}

	/*
	 * The parkfile list is a LIFO, last one parked got unprak first
	 */
	while ((e = readdir(dh)) != NULL) {
		if ((strlen(e->d_name) > 9) &&
		    strneq(e->d_name, "parkfile-", 9)) {
			i = atoi(&(e->d_name[9]));
			if (i > top) top = i; /* get the highest number */
		}
	}
	closedir(dh);
	if (top == 0) goto empty;

	return (do_unpark(top));
}
