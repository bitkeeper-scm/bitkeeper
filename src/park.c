/*
 * Copyright (c) 2000, Andrew Chang
 */
#include "system.h"
#include "sccs.h"

private int listParkFile();
private int purgeParkFile(int id);

int
park_main(int ac, char **av)
{
	char	parkfile[MAXPATH], changedfile[MAXPATH];
	char	*diffsopts, *comment = 0;
	int 	lflag  = 0, qflag = 0, purge = 0, c, status, try = 0;
	FILE	*f;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help park");
		return (0);
	}

	while ((c = getopt(ac, av, "lp:qy:")) != -1) {
		switch (c) {
		    case 'l':	lflag = 1; break;
		    case 'p':	purge = atoi(optarg); break;
		    case 'q':	qflag = 1; break;
		    case 'y':	comment = optarg; break;
		    default: 	system("bk help -s park");
				return (1);
		}
	}

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "Can't find package root\n");
		return (0);
	}

	if (lflag) return (listParkFile());
	if (purge) return (purgeParkFile(purge));

	bktemp(changedfile);
	sysio(0, changedfile, 0, "bk", "sfiles", "-c", SYS);
	if (size(changedfile) == 0) {
		unless (qflag) printf("Nothing to park\n");
		unlink(changedfile);
		return (0);
	}

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
	assert(size(parkfile) > 0);

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
	rc = sysio(parkfile, 0, 0, "bk", "patch", "-p1", "-g1", "-u", SYS);
	if (rc) {
		fprintf(stderr, "Cannot unpark %s\n", parkfile);
		/* Do not unlink the parkfile,  user may want to re-try */
		return (1);
	}
	fprintf(stderr, "Unpark of %s is successful\n", parkfile);
	unlink(parkfile); /* careful */
	unlink(parkCommentFile); /* careful */
}

int
unpark_main(int ac, char **av)
{
	struct	dirent *e;
	DIR	*dh;
	int	c, rc, i, n = 0, top = 0, purge = 0;
	char	parkfile[MAXPATH], parkCommentFile[MAXPATH];

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help unpark");
		return (0);
	}

	if (av[1]) return (do_unpark(atoi(av[1]))); /* unpark parkefile-n */

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "Can't find package root\n");
		return (0);
	}

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
