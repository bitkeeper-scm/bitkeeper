#include "bkd.h"

private	remote	*nfs_parse(char *p);
private	remote	*url_parse(char *p);

/*
 * Turn either
 *	bk://user@host:port/pathname
 * or	user@host:pathname
 * into a struct remote.
 * If nothing is passed in, use `bk parent`.
 */
remote *
remote_parse(char *p)
{
	char	buf[MAXPATH+256];
	static	echo = -1;
	remote	*r;

	if (echo == -1) echo = getenv("BK_REMOTE_PARSE") != 0;

	unless (p) {
		FILE	*f = popen("bk parent", "r");

		if (fgets(buf, sizeof(buf), f)) {
			if (strneq(buf, "Must specify parent", 19)) {
				fclose(f);
				return (0);
			}
				     /* 123456789012345678901 */
			assert(strncmp("Parent repository is ", buf, 21) == 0);
			p = &buf[21];
			chop(p);
		}
		pclose(f);
	}
	unless (p) return (0);
	if (strneq("bk://", p, 5)) {
		r = url_parse(p + 5);
	} else {
		r = nfs_parse(p);
	}
	if (echo && r) fprintf(stderr, "RP[%s]->[%s]\n", p, remote_unparse(r));
	return (r);
}

/* [[user@]host:]path */
private	remote *
nfs_parse(char *p)
{
	remote	*r;
	char	*s;
	
	unless (*p) return (0);
	new(r);
	/* user@host:path */
	if (s = strchr(p, '@')) {
		*s = 0; r->user = strdup(p); p = s + 1; *s = '@';
	}
	/* just path */
#ifdef WIN32
	/* Account for Dos path e.g c:/path */
	unless ((s = strchr(p, ':')) && (s != &p[1])) {
#else
	unless (s = strchr(p, ':')) {
#endif
		if (r->user) {
			remote_free(r);
			return (0);
		}
		r->path = strdup(fullname(p, 0));
		return (r);
	}
	*s = 0; r->host = strdup(p); p = s + 1; *s = ':';
	unless (*p) p = ".";	/* we like having a path */

	if (streq(r->host, sccs_gethost()) || streq(r->host, "localhost") ||
	    streq(r->host, "localhost.localdomain")) {
		    r->path = strdup(fullname(p, 0));
	} else {
		r->path = strdup(p);
	}
	return (r);
}

/* host[:port]/path or host:port */
private	remote *
url_parse(char *p)
{
	remote	*r;
	char	*s;

	unless (*p) return (0);
	new(r);
	if (s = strchr(p, ':')) {		/* host:port[/path] */
		*s = 0; r->host = strdup(p); p = s + 1; *s = ':';
		r->port = atoi(p);
		p = strchr(p, '/');
		if (p) r->path = strdup(p);
	} else if (s = strchr(p, '/')) {	/* host/path */
		*s = 0;
		r->port = BK_PORT;
		r->host = strdup(p);
		*s = '/';
		r->path = strdup(s);
	} else {
		return (0);
	}
	return (r);
}

/*
 * Do the opposite of remote_parse().
 *	bk://user@host:port/pathname
 * or	user@host:pathname
 */
char	*
remote_unparse(remote *r)
{
	char	buf[MAXPATH*2];
	char	port[10];

	if (r->port) {
		strcpy(buf, "bk://");
		if (r->user) {
			strcat(buf, r->user);
			strcat(buf, "@");
		}
		assert(r->host);
		strcat(buf, r->host);
		if (r->port != BK_PORT) {
			strcat(buf, ":");
			sprintf(port, "%u", r->port);
			strcat(buf, port);
		}
		if (r->path) strcat(buf, r->path);
		return (strdup(buf));
	}
	if (r->user) {
		assert(r->host);
		sprintf(buf, "%s@%s:", r->user, r->host);
		if (r->path) strcat(buf, r->path);
		return (strdup(buf));
	} else if (r->host) {
		sprintf(buf, "%s:", r->host);
		if (r->path) strcat(buf, r->path);
		return (strdup(buf));
	}
	assert(r->path);
	return (strdup(r->path));
}

void
remote_free(remote *r)
{
	unless (r) return;
	if (r->user) free(r->user);
	if (r->host) free(r->host);
	if (r->path) free(r->path);
	free(r);
}

void
remote_print(remote *r, FILE *f)
{
	unless (r) return;
	fprintf(f, "R=0x%x ", (unsigned int)r);
	if (r->user) fprintf(f, "USER=%s ", r->user);
	if (r->host) fprintf(f, "HOST=%s ", r->host);
	if (r->port) fprintf(f, "PORT=%u ", r->port);
	if (r->path) fprintf(f, "PATH=%s ", r->path);
	fprintf(f, "\n");
}

#ifdef WIN32
pid_t
tcp_pipe(char *host, int port, int *r_pipe, int *w_pipe)
{
	char *av[4];
	char pbuf[50];
		
	sprintf(pbuf, "%d", port);
	av[0] = "socket_helper";
	av[1] = host;
	av[2] = pbuf;
	av[3] = 0;
	return spawnvp_rwPipe(av, r_pipe, w_pipe);
}
#endif


/*
 * Return the pid of a connected to daemon with stdin/out put in fds[].
 * Stderr is left alone, we don't want to touch that - ssh needs it for
 * password prompts and other commands may use it for status.
 */
pid_t
bkd(int compress, remote *r, int *r_pipe, int *w_pipe)
{
	char	*t, *freeme = 0;
	char	*remsh = "ssh";
	char	*remopts = compress ? "-C" : 0;
	char	*cmd[100];
	int	i;
	pid_t	p;
	int	wpipe[2];
	int	rpipe[2];
	int	findprog(char *);
	int	fd0, fd1;

	if (r->port) {
		assert(r->host);
#ifdef WIN32
		p = tcp_pipe(r->host, r->port, r_pipe, w_pipe);
		if (p == ((pid_t) -1)) {
			fprintf(stderr, "can not create socket_helper\n");
			return (-1);
		}
#else
		*r_pipe = *w_pipe = tcp_connect(r->host, r->port);
#endif
		return ((pid_t)0);
	}
	t = sccs_gethost();
	if (r->host && (!t || !streq(t, r->host))) { 
		if (((t = getenv("PREFER_RSH")) && streq(t, "YES")) ||
		    !findprog("ssh")) {
#ifdef	hpux
			remsh = "remsh";
#else
			remsh = "rsh";
#endif
#ifdef WIN32
			if (!(t = prog2path(remsh)) ||
			    strstr(t, "system32/rsh")) {
				fprintf(stderr, "Can not find %s.\n", remsh);
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
#endif
			remopts = 0;
		}
		if (t = getenv("BK_RSH")) {
			/*
			 * Parse the command into words.
			 * XXX - This does not respect quotes.
			 */
			cmd[i = 0] = t;
			do {
				while (*t && !isspace(*t)) t++;
				if (isspace(*t)) {
					*t++ = 0; 
					while (*t && isspace(*t)) t++;
					if (*t && !isspace(*t)) cmd[++i] = t;
				}
			} while (*t);
		} else {
			cmd[i = 0] = remsh;
			if (remopts) cmd[++i] = remopts;
		}
		cmd[++i] = r->host;
		if (r->user) {
			cmd[++i] = "-l";
			cmd[++i] = r->user;
		}

		/*
		 * This cute hack should work if you have a sh or a csh.
		 * Thanks to David Sharnoff for this idea.
		 *
		 * It doesn't do this except in the localhost case because
		 * the paths may not be the same on both hosts.
		 */
		if (streq(r->host, "localhost") && (t = getenv("PATH"))) {
			freeme = malloc(strlen(t) + 20);
			sprintf(freeme, "PATH=%s", t);
			cmd[++i] = "env";
			cmd[++i] = freeme;
		}
		cmd[++i] = "bk bkd -e";
		cmd[++i] = 0;
	} else {
		cmd[0] = "bk";
		cmd[1] = "bkd";
		cmd[2] = "-e";
		cmd[3] = 0;
    	}
	if (pipe(wpipe) == -1) return ((pid_t)-1);
	if (pipe(rpipe) == -1) return ((pid_t)-1);
	fd0 = dup(0); close(0);
	fd1 = dup(1); close(1);
	dup2(wpipe[0], 0); close(wpipe[0]);
	dup2(rpipe[1], 1); close(rpipe[1]);
	make_fd_uninheritable(wpipe[1]);
	make_fd_uninheritable(rpipe[0]);
#ifndef WIN32
	signal(SIGCHLD, SIG_DFL);
#endif
	p = spawnvp_ex(_P_NOWAIT, cmd[0], cmd);
	/*
	 * for parent: restore fd0 fd1
	 */
	close(0); dup2(fd0, 0);
	close(1); dup2(fd1, 1);

	*w_pipe = wpipe[1];
	*r_pipe = rpipe[0];
	if (freeme) free(freeme);
	return (p);
}

void
bkd_reap(pid_t resync, int r_pipe, int w_pipe)
{
	close(w_pipe);
	close(r_pipe);
	if (resync > 0) {
	/*
	 * win32 does not support the WNOHANG options
	 */
#ifndef WIN32
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
#endif
		waitpid(resync, 0, 0);
	}
}
