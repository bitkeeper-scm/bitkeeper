#include "../bkd.h"

private void	argv_save(int ac, char **av, char **nav, int j);

void
reap(int sig)
{
/* There is no need to reap processes on Windows */
#ifndef WIN32
	while (waitpid((pid_t)-1, 0, WNOHANG) > 0);
	signal(SIGCHLD, reap);
#endif
}

void
bkd_server(int ac, char **av)
{
	int	i, j, port, killsock, startsock, sock, nsock, maxfd;
	char	*p;
	FILE	*f;
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
		/* XXX bind 127.0.0.1 in 3.3.x */
		if ((startsock = tcp_server(0, 0)) < 0) {
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
		spawnvp(_P_DETACH, nav[0], nav);

		/* wait for bkd to start */
		if ((nsock = tcp_accept(startsock)) >= 0) closesocket(nsock);
		closesocket(startsock);
		exit(nsock >= 0 ? 0 : 1);
	}

	/*
	 * We need to make sure that sock is not 0/1/2 because we stomp
	 * on that below.
	 * When we're called via spawn w/ DETACH, we have no fds.
	 */
	reserveStdFds();

	port = atoi(getenv("BKD_PORT"));
	sock = tcp_server(port, 0);
	if (sock < 0) exit(2);	/* regressions count on 2 */
	assert(sock > 2);
	make_fd_uninheritable(sock);

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
	signal(SIGCHLD, reap);
	signal(SIGPIPE, SIG_IGN);
	if (Opts.alarm) {
		signal(SIGALRM, exit);
		alarm(Opts.alarm);
	}
	if (Opts.kill_ok) {
		/* XXX bind 127.0.0.1 in 3.3.x */
		if ((killsock = tcp_server(0, 0)) < 0) {
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
		buf[0] = 'K';
		write(nsock, buf, 1);
		closesocket(nsock);
	}
	closesocket(killsock);
}

extern	char	*bkd_getopt;

private void
argv_save(int ac, char **av, char **nav, int j)
{
	int	c;
	char	*p;

	/*
	 * Parse the av[] to decide which one we should pass down stream
	 */
	getoptReset();
	while ((c = getopt(ac, av, bkd_getopt)) != -1) {
		/*
		 * skip all options which don't make sense for a short lived bkd
		 */
		if (strchr("cdeEgpPRStu", c)) continue;
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
