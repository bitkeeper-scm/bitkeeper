#include "../system.h"
#include "../sccs.h"

void
bkd_reap(pid_t resync, int r_pipe, int w_pipe)
{
	close(w_pipe);
	close(r_pipe);
#ifndef WIN32
	if (resync > 0) {
		/*
		 * win32 does not support the WNOHANG options
		 * there alos not need to reap child process on win32
		 */
		int	i;

		/* give it a bit for the protocol to close */
		for (i = 0; i < 20; ++i) {
			if (waitpid(resync, 0, WNOHANG) == resync) return;
			usleep(10000);
		}
		kill(resync, SIGTERM);
		for (i = 0; i < 20; ++i) {
			if (waitpid(resync, 0, WNOHANG) == resync) return;
			usleep(10000);
		}
		kill(resync, SIGKILL);
		waitpid(resync, 0, 0);
	}
#endif
}







#ifndef WIN32
pid_t
bkd_tcp_connect(remote *r)
{
	int i;
	if (r->httpd) {
		http_connect(r, WEB_BKD_CGI);
	} else {
		i = tcp_connect(r->host, r->port);
		if (i < 0) {
			r->rfd = r->wfd = -1;
			if (i == -2) r->badhost = 1;
		} else {
			r->rfd = r->wfd = i;
		}
	}
	r->isSocket = 1;
	return ((pid_t)0);
}
#else

pid_t
tcp_pipe(remote *r)
{
	char	port[50], pipe_size[50];
	char	*av[9] = {"bk", "_socket2pipe"};
	int	i = 2;

	sprintf(port, "%d", r->port);
	sprintf(pipe_size, "%d", BIG_PIPE);
	if (r->trace) av[i++] = "-d";
	if (r->httpd) av[i++] = "-h";
	av[i++] = "-p";
	av[i++] = pipe_size;
	av[i++] = r->host;
	av[i++] = port;
	av[i] = 0;
	return spawnvp_rwPipe(av, &(r->rfd), &(r->wfd), BIG_PIPE);
}

pid_t
bkd_tcp_connect(remote *r)
{
	pid_t	p;
		p = tcp_pipe(r);
		if (p == ((pid_t) -1)) {
			fprintf(stderr, "cannot create socket_helper\n");
			return (-1);
		}
		r->isSocket = 0;
		return (p);
}
#endif






#ifndef WIN32
check_rsh(char *remsh)
{
	/*
	 * rsh is bundled with most Unix system
	 * so we skip the check
	 */
	return (0);
}
#else
check_rsh(char *remsh)
{
	char *t;

	if (!(t = prog2path(remsh)) ||
	    strstr(t, "system32/rsh")) {
		fprintf(stderr, "Cannot find %s.\n", remsh);
		fprintf(stderr,
"=========================================================================\n\
The programs rsh/ssh are not bundled with the BitKeeper distribution.\n\
The recommended way for transfering BitKeeper files on Windows is via\n\
the bkd daemon. (If you have a bkd daemon configured on the remote host,\n\
try \"bk push/pull bk://HOST:PORT\".), If you prefer to transfer BitKeeper\n\
files via a rsh/ssh connection, you can install the rsh/ssh programs\n\
seperately. Please Note that the rsh command bundled with Windows NT is\n\
not compatible with Unix rshd.\n\
=========================================================================\n");
		return (-1);
	}
	return (0);
}
#endif
