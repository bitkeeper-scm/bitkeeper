#include "system.h"
#include "sccs.h"

int
park_main(int ac, char **av)
{
	char	parkfile[MAXPATH], changedfile[MAXPATH];
	int 	status, try;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help park");
		return (0);
	}

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "Cannot find package root\n");
		return (0);
	}

	
	bktemp(changedfile);
	sysio(0, changedfile, 0, "bk", "sfiles", "-c", SYS);
	if (size(changedfile) == 0) {
		printf( "Nothing to park\n");
		unlink(changedfile);
		return (0);
	}

	for (try = 1; ; try++) {
		sprintf(parkfile, "BitKeeper/tmp/parkfile-%d", try);
		unless (exists(parkfile)) break;
	}

	status = sysio(changedfile, parkfile, 0,
					"bk", "diffs", "-uv", "-", SYS);
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		fprintf(stderr, "bk park failed\n");
		unlink(changedfile);
		return (1);
	}
	assert(size(parkfile) > 0);
	sysio(changedfile, 0, 0, "bk", "unedit", "-", SYS); /* careful */
	fprintf(stderr, "Parked changes in %s.\n", parkfile);
	unlink(changedfile);
	return (0);
}

int
unpark_main(int ac, char **av)
{
	struct	dirent *e;
	DIR	*dh;
	int	rc, i, top = 0;
	char	parkfile[MAXPATH];

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help unpark");
		return (0);
	}

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "Cannot find package root\n");
		return (0);
	}
	
	dh = opendir(BKTMP);
	unless (dh) {
empty:		fprintf(stderr, "No parkfile found\n");
		return (0);
	}
	/*
	 * The parkfile list  is a LIFO, last one parked got unprak first 
	 */
	while ((e = readdir(dh)) != NULL) {
		if ((strlen(e->d_name) > 9) &&
		    strneq(e->d_name, "parkfile-", 9)) {
			i = atoi(&(e->d_name[9]));
			if (i > 0) top = i;
		}
	}
	closedir(dh);
	if (top == 0) goto empty;

	sprintf(parkfile, "%s/parkfile-%d", BKTMP, top);
	fprintf(stderr, "Unparking %s\n", parkfile);
	rc = sysio(parkfile, 0, 0, "bk", "patch", "-p1", "-g1", "-u", SYS);
	if (rc) {
		fprintf(stderr, "Cannot unpark %s\n", parkfile);
		/* Do not unlink the parkfile,  user may want to re-try */
		return (1);
	} 
	fprintf(stderr, "Unpark of %s is successful\n", parkfile);
	unlink(parkfile);
	return (0);
}
