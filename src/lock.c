/* Copyright (c) 2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private jmp_buf	jmp;
private	void abort_lock(int dummy) { longjmp(jmp, 1); }

/*
 * lock - repository level locking
 */
int
lock_main(int ac, char **av)
{
	int	c, uslp = 1000;
	int	what = 0, silent = 0;
	pid_t	pid;

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help lock");
		return (0);
	}
	while ((c = getopt(ac, av, "lqrswLU")) != -1) {
		switch (c) {
		    case 'q': /* fall thru */			/* doc 2.0 */
		    case 's': silent = 1; break;		/* undoc 2.0 */
		    case 'l':					/* doc 2.0 */
		    case 'r':					/* doc 2.0 */
		    case 'w':					/* doc 2.0 */
		    case 'L':
		    case 'U':
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
		} while (repository_mine('r'));
		exit(0);
	    
	    case 'w':	/* write lock the repository */
		if (repository_wrlock()) {
			fprintf(stderr, "write lock failed.\n");
			repository_lockers(0);
			exit(1);
		}
		/* make ourselves go away after the lock is gone */
		do {
			usleep(500000);
		} while (repository_mine('w'));
		exit(0);

	    case 'l':	/* list lockers / exit status */
		unless (silent) repository_lockers(0);
		if (repository_locked(0)) exit(1);
		unless (silent) {
			fprintf(stderr, "No active lock in repository\n");
		}
		exit(0);
	    
	    case 'L':	/* wait for the repository to become locked */
		while (!repository_locked(0)) {
			usleep(uslp);
			if (uslp < 1000000) uslp <<= 1;
		}
		exit(0);

	    case 'U':	/* wait for the repository to become unlocked */
		while (repository_locked(0)) {
			usleep(uslp);
			if (uslp < 1000000) uslp <<= 1;
		}
		exit(0);

	    default: /* we should never get here */
		goto usage;
	}
}

void
repo_main(int ac, char **av)
{
	fprintf(stderr, "The repo command has been replaced.\n");
	fprintf(stderr, "To lock use bk lock.\n");
	fprintf(stderr, "To unlock use bk unlock.\n");
	exit(1);
}
