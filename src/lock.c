/* Copyright (c) 2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "lib_tcp.h"

private	int	caught;
private	void	abort_lock(int dummy) { caught++; }

/*
 * lock - repository level locking
 */
int
lock_main(int ac, char **av)
{
	int	nsock, c, uslp = 1000;
	int	what = 0, silent = 0, tcp = 0;
	char	*file = 0;
	pid_t	pid;

	while ((c = getopt(ac, av, "f;lqrstwLU")) != -1) {
		switch (c) {
		    case 'q': /* fall thru */			/* doc 2.0 */
		    case 's': silent = 1; break;		/* undoc 2.0 */
		    case 'f': file = optarg;
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
		    case 't': tcp = 1; break;
		    default:
usage:			system("bk help -s lock");
			return (1);
		}
	}
	unless (what) what = 'l';
	if (av[optind]) chdir(av[optind]);
	proj_cd2root();
	pid = getpid();
	sig_catch(abort_lock);
	if (tcp) {
		tcp = tcp_server(0, 1);
		printf("127.0.0.1:%d\n", sockport(tcp));
		fflush(stdout);
	}
	switch (what) {
	    case 'f':	/* lock using the specified file */
		unless (file) exit(1);
	    	if (sccs_lockfile(file, -1, 1)) {
			perror(file);
			exit(1);
		}
		/* make ourselves go when told */
		if (tcp) {
			nsock = tcp_accept(tcp);
			sccs_unlockfile(file);
			chdir("/");
			write(nsock, &c, 1);
			closesocket(nsock);
			closesocket(tcp);
			exit(0);
		}
		do {
			usleep(500000);
		} while (sccs_mylock(file) && !caught);
		sccs_unlockfile(file);
		exit(0);

	    case 'r':	/* read lock the repository */
		if (repository_rdlock()) {
			fprintf(stderr, "read lock failed.\n");
			repository_lockers(0);
			exit(1);
		}
		/* make ourselves go when told */
		if (tcp) {
			nsock = tcp_accept(tcp);
			repository_rdunlock(0);
			chdir("/");
			write(nsock, &c, 1);
			closesocket(nsock);
			closesocket(tcp);
			exit(0);
		}
		/* make ourselves go away after the lock is gone */
		do {
			usleep(500000);
		} while (repository_mine('r') && !caught);
		if (caught) repository_rdunlock(0);
		exit(0);
	    
	    case 'w':	/* write lock the repository */
		if (repository_wrlock()) {
			fprintf(stderr, "write lock failed.\n");
			repository_lockers(0);
			exit(1);
		}
		/* make ourselves go when told */
		if (tcp) {
			nsock = tcp_accept(tcp);
			repository_wrunlock(0);
			chdir("/");
			write(nsock, &c, 1);
			closesocket(nsock);
			closesocket(tcp);
			exit(0);
		}
		/* make ourselves go away after the lock is gone */
		do {
			usleep(500000);
		} while (repository_mine('w') && !caught);
		repository_wrunlock(0);
		exit(0);

	    case 'l':	/* list lockers / exit status */
		unless (silent) repository_lockers(0);
		if (repository_locked(0)) exit(1);
		unless (silent) {
			fprintf(stderr, "No active lock in repository\n");
		}
		exit(0);
	    
	    case 'L':	/* wait for the file|repository to become locked */
		while ((file && !exists(file)) ||
		    (!file && !repository_locked(0))) {
			usleep(uslp);
			if (uslp < 1000000) uslp <<= 1;
		}
		exit(0);

	    case 'U':	/* wait for the file|repository to become unlocked */
		while ((file && exists(file)) ||
		    (!file && repository_locked(0))) {
			usleep(uslp);
			if (uslp < 1000000) uslp <<= 1;
		}
		exit(0);

	    default: /* we should never get here */
		goto usage;
	}
}

int
repo_main(int ac, char **av)
{
	fprintf(stderr, "The repo command has been replaced.\n");
	fprintf(stderr, "To lock use bk lock.\n");
	fprintf(stderr, "To unlock use bk unlock.\n");
	return (1);
}
