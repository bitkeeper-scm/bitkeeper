/* Copyright (c) 2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private jmp_buf	jmp;
private	handler	abort_lock(int dummy) { longjmp(jmp, 1); }

/*
 * lock - repository level locking
 */
int
lock_main(int ac, char **av)
{
	int	c;
	int	what = 0, silent = 0;
	pid_t	pid;
	char	*thisHost;

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help lock");
		return (0);
	}
	while ((c = getopt(ac, av, "lqrsw")) != -1) {
		switch (c) {
		    case 'q': /* fall thru */	/* doc 2.0 */
		    case 's': silent = 1; break;	/* undoc 2.0 */
		    case 'l':	/* doc 2.0 */
		    case 'r':	/* doc 2.0 */
		    case 'w':	/* doc 2.0 */
			unless (what) {
				what = c;
				break;
			}
			/* fall through */
		    default:
usage:			system("bk help -s lock");
			return (1);
		}
	}
	unless (what) what = 'l';
	if (av[optind]) chdir(av[optind]);
	sccs_cd2root(0, 0);
	pid = getpid();
	thisHost = sccs_gethost();
	if (setjmp(jmp)) {
		if (what == 'r') repository_rdunlock(0);
		if (what == 'w') repository_wrunlock(0);
		exit(0);
	}
	(void)sig_catch(abort_lock);
	switch (what) {
	    case 'r':	/* read lock the repository */
		if (repository_rdlock()) {
			fprintf(stderr, "read lock failed.\n");
			repository_lockers(0);
			exit(1);
		}
		/* make ourselves go away after the lock is gone */
		do {
			usleep(500000);
		} while (repository_locker('r', pid, thisHost));
		exit(0);
	    
	    case 'w':
		if (repository_wrlock()) {
			fprintf(stderr, "write lock failed.\n");
			repository_lockers(0);
			exit(1);
		}
		/* make ourselves go away after the lock is gone */
		do {
			usleep(500000);
		} while (repository_locker('w', pid, thisHost));
		exit(0);

	    case 'l':
		unless (silent) repository_lockers(0);
		if (repository_locked(0)) exit(1);
		unless (silent) {
			fprintf(stderr, "No active lock in repository\n");
		}
		exit(0);

	    default: /* we should never get here */
		goto usage;
	}
}

void
repo_main()
{
	fprintf(stderr, "The repo command has been replaced.\n");
	fprintf(stderr, "To lock use bk lock.\n");
	fprintf(stderr, "To unlock use bk unlock.\n");
	exit(1);
}
