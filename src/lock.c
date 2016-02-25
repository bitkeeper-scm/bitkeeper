/*
 * Copyright 2000-2003,2005-2007,2009-2013,2015-2016 BitMover, Inc
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
#include "nested.h"

#ifndef  WIN32
#define	HANDLE	int
#endif

private	int	openIt(char *file, HANDLE *h);
private	void	closeIt(HANDLE h);

private	int	caught;
private	void	abort_lock(int dummy) { caught++; }
private	int	do_async(int ac, char **av);
private	void	tcpHandshake(int lockclient, int tcp);

/*
 * lock - repository level locking
 */
int
lock_main(int ac, char **av)
{
	int	nsock, c, uslp = 1000;
	int	what = 0, silent = 0, keepOpen = 0, tcp = 0;
	int	lockclient = -1;
	int	printStale = 0;
	int	standalone = 0;
	int	foundLocks = 0, nested = 0;
	int	locked, rc;
	char	*file = 0, *nlid = 0, *pidfile = 0;
	HANDLE	h = 0;
	longopt	lopts[] = {
		{ "name:", 310 }, /* set the name of the lock */
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "f;klP;qRrSstwWLUv", lopts)) != -1) {
		switch (c) {
		    case 'P': pidfile = strdup(optarg);
		    case 'q': /* fall thru */			/* doc 2.0 */
		    case 's': silent = 1; break;		/* undoc 2.0 */
		    case 't': tcp = 1; break;
		    case 'k': keepOpen = 1; break;		/* undoc */
		    case 'f':
			if (file) usage();
			file = optarg;
			break;
		    case 'S': standalone = 1; break;
		    case 'v': printStale = 1; break;
		    case 310: /* --name */
			prog = optarg;
			break;
		    /* One of .. or fall through to error */
		    case 'l':					/* doc 2.0 */
		    case 'r':					/* doc 2.0 */
		    case 'R':					/* doc 2.0 */
		    case 'w':					/* doc 2.0 */
		    case 'W':					/* doc 2.0 */
		    case 'L':
		    case 'U':
			unless (what) {
				what = c;
				break;
			}
			/* fall through */
		    default: bk_badArg(c, av);
		}
	}
	unless (!file || !what || (what == 'U') || (what == 'L')) {
		usage();
	}
	unless (what) what = (file) ? 'f' : 'l';
	if (av[optind]) {
		if (chdir(av[optind])) {
			fprintf(stderr, "%s: no such directory: %s\n",
			    prog, av[optind]);
			return (2);
		}
	}
	unless (file) {
		if (proj_cd2root() < 0) {
			fprintf(stderr, "lock: Not in a repository\n");
			exit(2);
		}
	}
	sig_catch(abort_lock);
	if (tcp) {
		char	*clienturl;

		unless (clienturl = getenv("_BK_LOCK_CLIENT")) {
			return (do_async(ac, av));
		}
		fclose(stdout);	/* let foreground run stdout */
		/* handle pidfile right away */
		if (pidfile && !Fprintf(pidfile, "%d", getpid())) {
			perror(pidfile);
			exit(1);
		}
		if ((lockclient = tcp_connect(clienturl, 0)) < 0) {
			fprintf(stderr,
			    "Failed to connect to %s\n", clienturl);
			exit(1);
		}
		if ((tcp = tcp_server(0, 0, 1)) < 0) {
			perror("lock");
			exit(1);
		}
		putenv("_BK_LOCK_CLIENT=");
	}
	nested = !standalone && proj_isEnsemble(0);
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
			tcpHandshake(lockclient, tcp);
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
		nlid = 0;
		if (nested) {
			unless (nlid = nested_rdlock(0)) {
				fprintf(stderr, "nested read lock failed.\n%s\n",
				    nested_errmsg());
				return (1);
			}
		} else if (repository_rdlock(0)) {
			fprintf(stderr, "read lock failed.\n");
			repository_lockers(0);
			return (1);
		}
		/* make ourselves go when told */
		if (tcp) {
			tcpHandshake(lockclient, tcp);
			nsock = tcp_accept(tcp);
			if (getenv("_BK_LEAVE_LOCKED")) {
			} else if (nested && nested_unlock(0, nlid)) {
				fprintf(stderr, "nested unlock failed:\n%s\n",
				    nested_errmsg());
				return (1);
			} else {
				repository_rdunlock(0, 0);
			}
			chdir("/");
			write(nsock, &c, 1);
			closesocket(nsock);
			closesocket(tcp);
			return (0);
		}
		/* make ourselves go away after the lock is gone */
		do {
			usleep(500000);
			locked = nested
				? nested_mine(0, nlid, 0)
				: repository_mine(0, 'r');
		} while (locked && !caught);
		rc = 0;
		if (caught) {
			rc = nested
				? nested_unlock(0, nlid)
				: repository_rdunlock(0, 0);
		}
		return (rc);
	    case 'R':	/* nested_rdlock() the repository */
		unless (nlid = nested_rdlock(0)) {
			fprintf(stderr, "nested read lock failed.\n%s\n",
			    nested_errmsg());
			exit (1);
		}
		if (tcp) {
			tcpHandshake(lockclient, tcp);
			nsock = tcp_accept(tcp);
			if ((getenv("_BK_LEAVE_LOCKED") == 0) && nested_unlock(0, nlid)) {
				fprintf(stderr, "nested unlock failed:\n%s\n",
				    nested_errmsg());
				exit (1);
			}
			chdir("/");
			write(nsock, &c, 1);
			closesocket(nsock);
			closesocket(tcp);
			exit(0);
		}
		do {
			usleep(500000);
		} while (nested_mine(0, nlid, 0) && !caught);
		if (caught) {
			if ((getenv("_BK_LEAVE_LOCKED") == 0) && nested_unlock(0, nlid)) {
				fprintf(stderr, "nested unlock failed:\n%s\n",
				    nested_errmsg());
				exit (1);
			}
		}
		exit (0);
	    case 'w':	/* write lock the repository */
		putenv("_BK_LOCK_INTERACTIVE=1");
		nlid = 0;
		if (nested) {
			unless (nlid = nested_wrlock(0)) {
				fprintf(stderr, "nested write lock failed.\n%s\n",
				    nested_errmsg());
				return (1);
			}
		} else if (repository_wrlock(0)) {
			fprintf(stderr, "write lock failed.\n");
			repository_lockers(0);
			exit(1);
		}
		/* make ourselves go when told */
		if (tcp) {
			tcpHandshake(lockclient, tcp);
			nsock = tcp_accept(tcp);
			if (getenv("_BK_LEAVE_LOCKED")) {
			} else if (nested && nested_unlock(0, nlid)) {
				fprintf(stderr, "nested unlock failed:\n%s\n",
				    nested_errmsg());
				return (1);
			} else {
				repository_wrunlock(0, 0);
			}
			chdir("/");
			write(nsock, &c, 1);
			closesocket(nsock);
			closesocket(tcp);
			exit(0);
		}
		/* make ourselves go away after the lock is gone */
		do {
			usleep(500000);
			locked = nested
				? nested_mine(0, nlid, 0)
				: repository_mine(0, 'w');
		} while (locked && !caught);
		rc = nested
			? nested_unlock(0, nlid)
			: repository_wrunlock(0, 0);
		return (rc);

	    case 'W':	/* nested_wrlock() the repository */
		putenv("_BK_LOCK_INTERACTIVE=1");
		unless (nlid = nested_wrlock(0)) {
			fprintf(stderr, "nested write lock failed.\n%s\n",
			    nested_errmsg());
			exit (1);
		}
		if (tcp) {
			tcpHandshake(lockclient, tcp);
			nsock = tcp_accept(tcp);
			if ((getenv("_BK_LEAVE_LOCKED") == 0) && nested_unlock(0, nlid)) {
				fprintf(stderr, "nested unlock failed:\n%s\n",
				    nested_errmsg());
				exit (1);
			}
			chdir("/");
			write(nsock, &c, 1);
			closesocket(nsock);
			closesocket(tcp);
			exit(0);
		}
		do {
			usleep(500000);
		} while (nested_mine(0, nlid, 0) && !caught);
		if (caught) {
			if ((getenv("_BK_LEAVE_LOCKED") == 0) && nested_unlock(0, nlid)) {
				fprintf(stderr, "nested unlock failed:\n%s\n",
				    nested_errmsg());
				exit (1);
			}
		}
		exit (0);
	    case 'l':	/* list lockers / exit status */
		unless (silent) {
			if (nested) {
				/*
				 * If we are printing stale locks, don't
				 * remove them.
				 */
				foundLocks = nested_printLockers(0,
				    printStale, !printStale, stderr);
			}
			unless (foundLocks) {
				foundLocks += repository_lockers(0);
			}
		}
		while (repository_locked(0)) {
			/* if silent, delay at most one second (1/2 + 1/4...) */
			if (!silent || (uslp > 500000)) exit (1);
			usleep(uslp);
			uslp <<= 1;
		}
		if (!silent && !foundLocks) {
			fprintf(stderr, "No active lock in repository\n");
		}
		exit(0);
	    case 'L':	/* wait for the file|repository to become locked */
		while ((file && !exists(file)) ||
		    (!file && !repository_locked(0)) && !caught) {
			usleep(uslp);
			if (uslp < 1000000) uslp <<= 1;
		}
		if (!file && proj_isProduct(0)) {
			char	**locks = 0;
			/* wait for a nested lock */
			while (!(locks = nested_lockers(0, 0, 0))) {
				usleep(uslp);
				if (uslp < 1000000) uslp <<= 1;
			}
			freeLines(locks, freeNlock);
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
		usage();
	}
}

#define	MSGSIZE	512
/*
 * Run the bk lock command in a background process, but hang on until
 * after the lock has been grabbed.  Then return "host:port\n"
 */

private int
do_async(int ac, char **av)
{
	int	sock1 = -1, sock2 = -1;
	int	i;
	int	rc = -1;
	char	**nav;
	char	buf[MSGSIZE];

	nav = addLine(0, "bk");
	for (i = 0; i < ac; i++) {
		nav = addLine(nav, av[i]);
	}
	nav = addLine(nav, 0);

	if ((sock1 = tcp_server(0, 0, 1)) < 0) goto err;
	safe_putenv("_BK_LOCK_CLIENT=127.0.0.1:%d", sockport(sock1));

	if (spawnvp(P_NOWAIT, "bk", &nav[1]) < 0) {
		fprintf(stderr, "Cannot spawn bk\n");
		goto err;
	}

	if ((sock2 = tcp_accept(sock1)) < 0) goto err;
	if ((i = read(sock2, buf, sizeof(buf))) < 0) {
		perror("readsock2");
		goto err;
	}
	/*
	 * if lock failed, the background will print out a message to
	 * stderr and close the sock with no data written.  Take a 0 length
	 * message as an error.
	 */
	unless (i) goto err;
	fwrite(buf, 1, i, stdout);
	fflush(stdout);
	rc = 0;
err:
	freeLines(nav, 0);
	unless (sock2 < 0) closesocket(sock2);
	unless (sock1 < 0) closesocket(sock1);
	return (rc);
}

private	void
tcpHandshake(int lockclient, int tcp)
{
	char	buf[MSGSIZE];

	sprintf(buf, "127.0.0.1:%d\n", sockport(tcp));
	write(lockclient, buf, strlen(buf));
	closesocket(lockclient);
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
