#include "bkd.h"

void	bkd_server(void);
void	do_cmds(void);
int	findcmd(int ac, char **av);
int	getav(int *acp, char ***avp);
void	log_cmd(int i, int ac, char **av);
void	readonly(void);
void	reap(int sig);

int
main(int ac, char **av)
{
	int	c;

	while ((c = getopt(ac, av, "deil|p;r")) != -1) {
		switch (c) {
		    case 'd': Opts.daemon = 1; break;
		    case 'e': Opts.errors_exit = 1; break;
		    case 'i': Opts.interactive = 1; break;
		    case 'l':
			Opts.log = optarg ? fopen(optarg, "a") : stderr;
			break;
		    case 'p': Opts.port = atoi(optarg); break;
		    case 'r': Opts.readonly = 1; break;
	    	}
	}
	if (Opts.readonly) readonly();
	putenv("PAGER=cat");
	if (Opts.daemon) {
		bkd_server();
		exit(1);
		/* NOTREACHED */
	} else {
		do_cmds();
		return (0);
	}
}

void
reap(int sig)
{
	while (waitpid((pid_t)-1, 0, WNOHANG) > 0);
	signal(SIGCHLD, reap);
}

void
bkd_server()
{
	int	sock = tcp_server(Opts.port ? Opts.port : BK_PORT);

	signal(SIGCHLD, reap);
	while (1) {
		int	n = tcp_accept(sock);

		if (fork()) {
		    	close(n);
			/* reap 'em if you got 'em */
			waitpid((pid_t)-1, 0, WNOHANG);
			continue;
		}

		if (Opts.log) {
			struct	sockaddr_in sin;
			int	len = sizeof(sin);

			if (getpeername(n, (struct sockaddr*)&sin, &len)) {
				strcpy(Opts.remote, "unknown");
			} else {
				strcpy(Opts.remote, inet_ntoa(sin.sin_addr));
			}
		}
		/*
		 * Make sure all the I/O goes to/from the socket
		 */
		close(0); dup(n);
		close(1); dup(n);
		close(n);
		do_cmds();
		exit(0);
	}
}

void
do_cmds()
{
	int	ac;
	char	**av;
	int	i;

	while (getav(&ac, &av)) {
		getoptReset();
		if ((i = findcmd(ac, av)) != -1) {
			if (Opts.log) log_cmd(i, ac, av);
			if (cmds[i].cmd(ac, av) != 0) {
				if (Opts.interactive) {
					out("ERROR-CMD FAILED\n");
				}
				if (Opts.errors_exit) {
					out("ERROR-exiting\n");
					exit(1);
				}
			}
		} else if (av[0]) {
			if (Opts.interactive) out("ERROR-BAD CMD: ");
			if (Opts.interactive) out(av[0]);
			if (Opts.interactive) out(", Try help\n");
		} else if (Opts.interactive) {
			out("ERROR-Try help\n");
		}
	}
}

void
log_cmd(int i, int ac, char **av)
{
	time_t	t;
	struct	tm *tp;

	time(&t);
	tp = localtime(&t);
	fprintf(Opts.log, "%s %.24s ", Opts.remote, asctime(tp));
	for (i = 0; i < ac; ++i) {
		fprintf(Opts.log, "%s ", av[i]);
	}
	fprintf(Opts.log, "\n");
}

/* remove all write commands from the cmds array */
void
readonly()
{
	struct	cmd c[100];
	int	i, j;

	for (i = 0; cmds[i].name; i++);
	assert(i < 99);
	for (i = j = 0; cmds[i].name; i++) {
		if (cmds[i].readonly) c[j++] = cmds[i];
	}
	for (i = 0; i < j; i++) {
		cmds[i] = c[i];
	}
	cmds[i].name = 0;
	cmds[i].cmd = 0;
}

int
findcmd(int ac, char **av)
{
	int	i;

	if (ac == 0) return (-1);
	for (i = 0; cmds[i].name; ++i) {
		if (strcmp(av[0], cmds[i].name) == 0) return (i);
	}
	return (-1);
}

int
getav(int *acp, char ***avp)
{
	static	char buf[2500];		/* room for two keys */
	static	char *av[50];
	int	i, inspace = 1;
	int	ac;

	if (Opts.interactive) out("BK> ");
	for (ac = i = 0; in(&buf[i], 1) == 1; i++) {
		if (buf[i] == '\n') {
			buf[i] = 0;
			av[ac] = 0;
			*acp = ac;
			*avp = av;
			return (1);
		}
		if (isspace(buf[i])) {
			buf[i] = 0;
			inspace = 1;
		} else if (inspace) {
			av[ac++] = &buf[i];
			inspace = 0;
		}
	}
	return (0);
}
