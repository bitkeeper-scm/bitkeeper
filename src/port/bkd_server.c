/*
 * Copyright 2000-2005,2008-2011,2016 BitMover, Inc
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

#include "../bkd.h"

private	void	argv_save(int ac, char **av, char **nav, int j);

private	void
reap(int sig)
{
	while (waitpid((pid_t)-1, 0, WNOHANG) > 0);
	signal(SIGCHLD, reap);
}

void
bkd_server(int ac, char **av)
{
	int	i, j, port, killsock, startsock, sock, nsock, maxfd, tries;
	char	*addr;
	char	*p;
	FILE	*f;
	pid_t	pid = 0;
	fd_set	fds;
	struct timeval delay;
	char	buf[4];
	char	*nav[100];

	putenv("BKD_DAEMON=1");		/* for safe_cd code */

	/*
	 * If we're a standard daemon respawn ourselves in background and
	 * we are done.
	 */
	unless (Opts.foreground) {
		if ((startsock = tcp_server("127.0.0.1", 0, 0)) < 0) {
			fprintf(stderr, "bkd: failed to start startsock.\n");
			exit(1);
		}
		safe_putenv("_STARTSOCK=%d", sockport(startsock));
		i = 0;
		nav[i++] = "bk";
		nav[i++] = "bkd";
		nav[i++] = "-D";
		j = 1;
		while (nav[i++] = av[j++]);
		assert(i < 100);
		/*
		 * On Linux I have see the child be a zombie once in while.
		 * Our guess is that the port we want has gotten used
		 * between the time we checked in bkd.c and now.
		 * We do a few tries to see if we can get it and then give up.
		 */
		tries = getenv("BK_REGRESSION") ? 5 : 1;
		while (tries > 0) {
			unless (pid) {
				pid = spawnvp(_P_DETACH, nav[0], nav);
				if (pid == (pid_t)-1) {
					perror("bk bkd -D");
					exit(1);
				}
			}

			FD_ZERO(&fds);
			FD_SET(startsock, &fds);
			delay.tv_sec = 10;
			delay.tv_usec = 0;
			if (select(startsock+1, &fds, 0, 0, &delay) < 0) {
				goto next;
			}
			reap(0);
			if (kill(pid, 0) != 0) {
				pid = 0;
				goto next;
			}
			unless (FD_ISSET(startsock, &fds)) goto next;

			/* should be started */
			if ((nsock = tcp_accept(startsock)) >= 0) {
				closesocket(nsock);
				closesocket(startsock);
				if (nsock >= 0) {
					exit(0);
				} else {
					break;
				}
			}
next:			--tries;
			usleep(10000);
		}
		fprintf(stderr, "Failed to start background BKD\n");
		exit(1);
	}

	/*
	 * We need to make sure that sock is not 0/1/2 because we stomp
	 * on that below.
	 * When we're called via spawn w/ DETACH, we have no fds.
	 */
	reserveStdFds();

	addr = getenv("_BKD_ADDR");
	port = atoi(getenv("_BKD_PORT"));
	sock = tcp_server(addr, port, 0);
	if (sock < 0) exit(2);	/* regressions count on 2 */
	assert(sock > 2);
	make_fd_uninheritable(sock);
	safe_putenv("_BKD_PORT=%d", sockport(sock));

	i = 0;
	nav[i++] = "bk";
	nav[i++] = "bkd";
	argv_save(ac, av, nav, i);

	/*
	 * Don't create pidfile if we are running a win32 service,
	 * killing the bkd will not uninstall the service.
	 */
	if (!getenv("BKD_SERVICE") &&
	    Opts.pidfile && (f = fopen(Opts.pidfile, "w"))) {
		fprintf(f, "%u\n", getpid());
		fclose(f);
	}
	if (Opts.portfile && (f = fopen(Opts.portfile, "w"))) {
		fprintf(f, "%d\n", sockport(sock));
		fclose(f);
	}
	signal(SIGCHLD, reap);
	signal(SIGPIPE, SIG_IGN);
	if (Opts.alarm) {
		signal(SIGALRM, exit);
		alarm(Opts.alarm);
	}
	if (Opts.kill_ok) {
		if ((killsock = tcp_server("127.0.0.1", 0, 0)) < 0) {
			fprintf(stderr, "bkd: failed to start killsock.\n");
			exit(1);
		}
		safe_putenv("_KILLSOCK=%d\n", sockport(killsock));
	} else {
		killsock = -1;
	}

	maxfd = (sock > killsock) ? sock : killsock;

	/* bkd started ... */
	if (p = getenv("_STARTSOCK")) {
		if ((nsock = tcp_connect("127.0.0.1", atoi(p))) >= 0) {
			closesocket(nsock);
		}
	}

	/*
	 * We want only sock and possibly killsock open so we don't hang
	 * the client.  Leave stderr alone.
	 */
	close(0);
	close(1);
	for (;;) {
		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		if (Opts.kill_ok) FD_SET(killsock, &fds);
		delay.tv_sec = 60;
		delay.tv_usec = 0;
		if (select(maxfd+1, &fds, 0, 0, &delay) < 0) continue;
		if (Opts.kill_ok && FD_ISSET(killsock, &fds)) {
			break; /* stop server */
		}
		unless (FD_ISSET(sock, &fds)) continue;
		if ((nsock = tcp_accept(sock)) < 0) continue;
		assert(nsock == 0);
		unless (p = peeraddr(nsock)) p = "unknown";
		safe_putenv("BKD_PEER=%s", p);
		/*
		 * Make sure all the I/O goes to/from the socket
		 */
		dup2(nsock, 1);
		signal(SIGCHLD, SIG_DFL);	/* restore signals */
		spawnvp(_P_NOWAIT, "bk", nav);
		close(0);
		close(1);

		/* reap 'em if you got 'em */
		reap(0);
	}
	/* reap 'em if you got 'em */
	reap(0);

	closesocket(sock);

	/* confirm death */
	chdir("/");
	if ((nsock = tcp_accept(killsock)) >= 0) {
		chdir("/");
		buf[0] = 'K';
		write(nsock, buf, 1);
		closesocket(nsock);
	}
	closesocket(killsock);
}

extern	char	*bkd_getopt;

private	void
argv_save(int ac, char **av, char **nav, int j)
{
	int	c;
	char	*p;

	/*
	 * Parse the av[] to decide which one we should pass down stream
	 */
	getoptReset();
	while ((c = getopt(ac, av, bkd_getopt, 0)) != -1) {
		/*
		 * skip all options which don't make sense for a short lived bkd
		 */
		if (strchr("acdeEgpPRtu", c)) continue;
		if (p = strchr(bkd_getopt, c)) {
			if ((p[1] == ':') || (p[1] == '|')) {
				p = optarg ? optarg : "";
			} else {
				p = "";
			}
			nav[j++] = aprintf("-%c%s", c, p);
		}
	}
	nav[j] = 0;
}
