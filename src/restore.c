#include "sccs.h"

int
restore_main(int ac,  char **av)
{
	return (restore_backup(av[1], 0));
}

int
restore_backup(char *backup, int overwrite)
{
	char	*tmpfile;
	int	rc = 0;

	unless (backup) usage();
	unless (access(backup, R_OK) == 0) {
		fprintf(stderr, "restore: unable read backup %s\n", backup);
		return (1);
	}
	if (sysio(backup, 0, 0,
		"bk", "sfio", "-q", "-im", (overwrite ? "-f" : "--"), SYS)){
		getMsg("restore_failed", backup, '!', stderr);
		return (1);
	}
	getMsg("restore_checking", 0, 0, stderr);
	tmpfile = bktmp(0, 0);
	if (rc = systemf("bk -?BK_NO_REPO_LOCK=YES -r check -ac >'%s' 2>&1",
	    tmpfile)) {
		if (proj_isProduct(0) && isdir("RESYNC")) {
			fprintf(stderr, "\nThere were errors during check.\n");
			getMsg("pull_in_progress", 0, 0, stderr);
		} else {
			char	*msg = loadfile(tmpfile, 0);

			fputs(msg, stderr);
			FREE(msg);
			getMsg("restore_check_failed", backup, '=', stderr);
		}
	} else {
		fprintf(stderr, "check passed.\n");
	}
	unlink(tmpfile);
	FREE(tmpfile);
	return (rc);
}
