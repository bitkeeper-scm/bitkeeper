#include "bkd.h"

remote	*nfs_parse(char *p);
remote	*url_parse(char *p);

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
remote *
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
	unless (s = strchr(p, ':')) {
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
remote *
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
		sprintf(buf, "%s@%s", r->user, r->host);
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

/*
 * Return the pid of a connected to daemon with stdin/out put in fds[].
 * Stderr is left alone, we don't want to touch that - ssh needs it for
 * password prompts and other commands may use it for status.
 */
pid_t
bkd(int compress, remote *r, int *sock)
{
	char	*t;
	char	*remsh = "ssh";
	char	*remopts = compress ? "-C" : 0;
	char	*cmd[100], bkd_path[MAXPATH];
	int	i;
	pid_t	p;
	int	inout[2];
	int	findprog(char *);
	extern	char *bin;

	if (r->port) {
		assert(r->host);
		*sock = tcp_connect(r->host, r->port);
		return ((pid_t)0);
	}
	if (tcp_pair(inout) == -1) return ((pid_t)-1);
	t = sccs_gethost();
	if (r->host && (!t || !streq(t, r->host))) { 
		if (((t = getenv("PREFER_RSH")) && streq(t, "YES")) ||
		    !findprog("ssh")) {
#ifdef	hpux
			remsh = "remsh";
#else
			remsh = "rsh";
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
					*t = 0; 
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
		/* for regression tests */
		if (bin && streq(r->host, "localhost")) {
			/*
			 * Pick up the bkd in the $BK_BIN directory
			 * In regression test $BK_BIN is set to the
			 * test directory.
			 */
			sprintf(bkd_path, "%sbkd -e", bin);
			cmd[++i] = bkd_path;
		} else {
			/* pick up the bkd in the installed directory */
			cmd[++i] = "bk bkd -e";
		}
		cmd[++i] = 0;
	} else {
		cmd[0] = "bk";
		cmd[1] = "bkd";
		cmd[2] = "-e";
		cmd[3] = 0;
    	}
	switch (p = fork()) {
	    case -1: perror("fork"); return (-1);
	    case 0:
		close(0);
		dup(inout[1]);
		close(1);
		dup(inout[1]);
	    	execvp(cmd[0], cmd);
		exit(1);
	    default:
		close(inout[1]);
		*sock = inout[0];
	    	return (p);
    	}
}

void
bkd_reap(pid_t resync, int sock)
{
	int	i;

	close(sock);
	if (resync) {
		kill(resync, SIGTERM);
		for (i = 0; i < 100; ++i) {
			if (waitpid(resync, 0, WNOHANG)) break;
			usleep(10000);
		}
		kill(resync, SIGKILL);
		waitpid(resync, 0, 0);
	}
}
