#include "bkd.h"

typedef struct {
	u32	interactive:1;		/* show prompts, etc */
	u32	errors_exit:1;		/* exit on any error */
	u32	daemon:1;		/* listen for TCP connections */
	u32	readonly:1;		/* do read only commands exclusively */
	FILE	*log;			/* if set, log commands to here */
	u16	port;			/* listen on this port */
	char	remote[16];		/* a.b.c.d */
} opts;

void	do_cmds(opts opts, int in, int out);
int	findcmd(opts opts, int ac, char **av);
int	getav(opts opts, int in, int out, int *acp, char ***avp);

int
main(int ac, char **av)
{
	int	c;
	opts	opts;

	bzero(&opts, sizeof(opts));
	while ((c = getopt(ac, av, "deil|p;r")) != -1) {
		switch (c) {
		    case 'd': opts.daemon = 1; break;
		    case 'e': opts.errors_exit = 1; break;
		    case 'i': opts.interactive = 1; break;
		    case 'l':
			opts.log = optarg ? fopen(optarg, "a") : stderr;
			break;
		    case 'p': opts.port = atoi(optarg); break;
		    case 'r': opts.readonly = 1; break;
	    	}
	}
	if (opts.readonly) readonly();
	putenv("PAGER=cat");
	if (opts.daemon) {
		bkd_server(opts);
		/* NOTREACHED */
	} else {
		do_cmds(opts, 0, 1);
		return (0);
	}
}

void
reap(int sig)
{
	while (waitpid((pid_t)-1, 0, WNOHANG) > 0);
	signal(SIGCHLD, reap);
}

bkd_server(opts opts)
{
	int	sock = tcp_server(opts.port ? opts.port : BK_PORT);

	signal(SIGCHLD, reap);
	while (1) {
		int	n = tcp_accept(sock);

		if (opts.log) {
			struct	sockaddr_in sin;
			int	len = sizeof(sin);

			if (getpeername(n, (struct sockaddr*)&sin, &len)) {
				strcpy(opts.remote, "unknown");
			} else {
				strcpy(opts.remote, inet_ntoa(sin.sin_addr));
			}
		}

		switch (fork()) {
		    case 0:
			do_cmds(opts, n, n);
			exit(0);
		    default:
		    	close(n);
			/* reap 'em if you got 'em */
			waitpid((pid_t)-1, 0, WNOHANG);
		}
	}
}

void
drain(int fd)
{
	int	c;

	while (read(fd, &c, 1) == 1);
}

void
do_cmds(opts opts, int in, int out)
{
	int	ac;
	char	**av;
	int	i;

	while (getav(opts, in, out, &ac, &av)) {
		getoptReset();
		if ((i = findcmd(opts, ac, av)) != -1) {
			if (opts.log) log_cmd(opts, i, ac, av);
			if (cmds[i].cmd(ac, av, in, out) != 0) {
				if (opts.interactive) {
					writen(out, "ERROR-CMD FAILED\n");
				}
				if (opts.errors_exit) {
					writen(out, "ERROR-exiting\n");
					drain(in);
					exit(1);
				}
			}
		} else if (av[0]) {
			if (opts.interactive) writen(out, "ERROR-BAD CMD: ");
			if (opts.interactive) writen(out, av[0]);
			if (opts.interactive) writen(out, ", Try help\n");
		} else {
			if (opts.interactive) writen(out, "ERROR-Try help\n");
		}
	}
}

log_cmd(opts opts, int i, int ac, char **av)
{
	time_t	t;
	struct	tm tm, *tp;

	time(&t);
	tp = localtime(&t);
	fprintf(opts.log, "%s %.24s ", opts.remote, asctime(tp));
	for (i = 0; i < ac; ++i) {
		fprintf(opts.log, "%s ", av[i]);
	}
	fprintf(opts.log, "\n");
}

/* remove all write commands from the cmds array */
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
findcmd(opts opts, int ac, char **av)
{
	int	i;

	if (ac == 0) return (-1);
	for (i = 0; cmds[i].name; ++i) {
		if (strcmp(av[0], cmds[i].name) == 0) return (i);
	}
	return (-1);
}

int
getav(opts opts, int in, int out, int *acp, char ***avp)
{
	static	char buf[2500];		/* room for two keys */
	static	char *av[50];
	int	i, inspace = 1;
	int	ac;

	if (opts.interactive) write(out, "BK> ", 4);
	for (ac = i = 0; read(in, &buf[i], 1) == 1; i++) {
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
