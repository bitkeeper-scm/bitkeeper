#include "sccs.h"

int
restore_main(int ac,  char **av)
{
	exit (restore_backup(av[1], 0));
}

int
restore_backup(char *backup, int overwrite)
{
	unless (backup) usage();
	unless (access(backup, R_OK) == 0) {
		fprintf(stderr, "restore: unable read backup %s\n", backup);
		return (1);
	}
	if (sysio(backup, 0, 0,
		"bk", "sfio", "-im", (overwrite ? "-f" : "--"), SYS)){
		getMsg("restore_failed", backup, '!', stderr);
		return (1);
	}
	getMsg("restore_checking", 0, 0, stderr);
	if (sys("bk", "-r", "check", "-ac", SYS)) {
		getMsg("restore_check_failed", backup, '=', stderr);
		return (1);
	}
	fprintf(stderr, "check passed.\n");
	return (0);
}
