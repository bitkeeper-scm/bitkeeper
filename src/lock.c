/* Copyright (c) 2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"

#ifndef  WIN32
#define	HANDLE	int
#endif

private	int	openIt(char *file, HANDLE *h);
private	void	closeIt(HANDLE h);

private	int	caught;
private	void	abort_lock(int dummy) { caught++; }

/*
 * lock - repository level locking
 */
int
lock_main(int ac, char **av)
{
	int	nsock, c, uslp = 1000;
	int	what = 0, silent = 0, keepOpen = 0, tcp = 0;
	char	*file = 0;
	HANDLE	h = 0;
	pid_t	pid;

	while ((c = getopt(ac, av, "f;klqrstwLU")) != -1) {
		switch (c) {
		    case 'q': /* fall thru */			/* doc 2.0 */
		    case 's': silent = 1; break;		/* undoc 2.0 */
		    case 't': tcp = 1; break;
		    case 'k': keepOpen = 1; break;		/* undoc */
		    case 'f':
			if (file) goto usage;
		    	file = optarg;
			break;
		    /* One of .. or fall through to error */
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
	unless (!file || !what || (what == 'U') || (what == 'L')) {
		goto usage;
	}
	unless (what) what = (file) ? 'f' : 'l';
	if (av[optind]) chdir(av[optind]);
	unless (file) {
		if (proj_cd2root() < 0) {
			fprintf(stderr, "lock: Not in a repository\n");
			exit(2);
		}
	}
	pid = getpid();
	sig_catch(abort_lock);
	if (tcp) {
		tcp = tcp_server(0, 0, 1);
		printf("127.0.0.1:%d\n", sockport(tcp));
		fflush(stdout);
	}
	switch (what) {
	    case 'f':	/* lock using the specified file */
		unless (file) exit(1);
		if (keepOpen) {
			if (openIt(file, &h)) exit(1);
		} else if (sccs_lockfile(file, -1, 1)) {
			perror(file);
			exit(1);
		}
		/* make ourselves go when told */
		if (tcp) {
			nsock = tcp_accept(tcp);
			if (keepOpen) {
				closeIt(h);
			} else {
				sccs_unlockfile(file);
			}
			chdir("/");
			write(nsock, &c, 1);
			closesocket(nsock);
			closesocket(tcp);
			exit(0);
		}
		do {
			usleep(500000);
		} while (sccs_mylock(file) && !caught);
		if (keepOpen) {
			closeIt(h);
		} else {
			sccs_unlockfile(file);
		}
		exit(0);

	    case 'r':	/* read lock the repository */
		if (repository_rdlock(0)) {
			fprintf(stderr, "read lock failed.\n");
			repository_lockers(0);
			exit(1);
		}
		/* make ourselves go when told */
		if (tcp) {
			nsock = tcp_accept(tcp);
			repository_rdunlock(0, 0);
			chdir("/");
			write(nsock, &c, 1);
			closesocket(nsock);
			closesocket(tcp);
			exit(0);
		}
		/* make ourselves go away after the lock is gone */
		do {
			usleep(500000);
		} while (repository_mine(0, 'r') && !caught);
		if (caught) repository_rdunlock(0, 0);
		exit(0);
	    
	    case 'w':	/* write lock the repository */
		if (repository_wrlock(0)) {
			fprintf(stderr, "write lock failed.\n");
			repository_lockers(0);
			exit(1);
		}
		/* make ourselves go when told */
		if (tcp) {
			nsock = tcp_accept(tcp);
			repository_wrunlock(0, 0);
			chdir("/");
			write(nsock, &c, 1);
			closesocket(nsock);
			closesocket(tcp);
			exit(0);
		}
		/* make ourselves go away after the lock is gone */
		do {
			usleep(500000);
		} while (repository_mine(0, 'w') && !caught);
		repository_wrunlock(0, 0);
		exit(0);

	    case 'l':	/* list lockers / exit status */
		unless (silent) repository_lockers(0);
		while (repository_locked(0)) {
			/* if silent, delay at most one second (1/2 + 1/4...) */
			if (!silent || (uslp > 500000)) exit (1);
			usleep(uslp);
			uslp <<= 1;
		}
		unless (silent) {
			fprintf(stderr, "No active lock in repository\n");
		}
		exit(0);
	    
	    case 'L':	/* wait for the file|repository to become locked */
		while ((file && !exists(file)) ||
		    (!file && !repository_locked(0)) && !caught) {
			usleep(uslp);
			if (uslp < 1000000) uslp <<= 1;
		}
		exit(0);

	    case 'U':	/* wait for the file|repository to become unlocked */
		while ((file && exists(file)) ||
		    (!file && repository_locked(0)) && !caught) {
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

private	int
openIt(char *file, HANDLE *h)
{
#ifdef  WIN32
	*h = CreateFile(file, GENERIC_READ | GENERIC_WRITE,
	    0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (*h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Failed to open %s", file);
		return (1);
	}
#else
	if ((*h = creat(file, 0777)) < 0) {
		fprintf(stderr, "Failed to open %s", file);
		return (1);
	}
#endif
	return (0);
}

private	void
closeIt(HANDLE h)
{
#ifdef  WIN32
	CloseHandle(h);
#else
	close(h);
#endif
}
