#include "bkd.h"

private	remote	*nfs_parse(char *p);
private	remote	*url_parse(char *p);
extern	char	cmdlog_buffer[];

/*
 * Turn either
 *	bk://host:port/pathname		<- running bkd form
 *	bk://user@host:port/pathname	<- rsh host -l user
 * or	user@host:pathname		<- rsh host bkd -e
 * into a struct remote.
 * If nothing is passed in, use `bk parent`.
 */
remote *
remote_parse(char *p, int is_clone)
{
	char	buf[MAXPATH+256];
	static	int echo = -1;
	int	append = 0;
	remote	*r;

	if (echo == -1) echo = getenv("BK_REMOTE_PARSE") != 0;

	unless (p && *p) {
		unless (getParent(buf, sizeof buf)) return (0);
		p = buf;
		append = 1;
	}
	unless (p) return (0);
	if (strneq("bk://", p, 5)) {
		r = url_parse(p + 5);
	} else if (strneq("http://", p, 7)) {
		r = url_parse(p + 7);
		if (r) r->httpd = 1;
	} else {
		if (!is_clone && (bk_mode() == BK_BASIC)) {
			fprintf(stderr,
				"Non-url address detected: %s\n", upgrade_msg);
			r = NULL;
		} else {
			r = nfs_parse(p);
		}
	}
	if (r && append && cmdlog_buffer[0]) {
		char	*rem = remote_unparse(r);

		strcat(cmdlog_buffer, " ");
		strcat(cmdlog_buffer, rem);
		free(rem);
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
	r->rfd = r->wfd = -1;
	/* user@host:path */
	if (s = strchr(p, '@')) {
		*s = 0; r->user = strdup(p); p = s + 1; *s = '@';
	}
	/* just path, no host */
	unless (s = isHostColonPath(p)) {
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

/*
 * host[:port]/path or
 * user@host[:path] or
 * host:port
 * host
 *
 * Note: If we are talking to a bkd on win32, there could be
 * 	 a colon in the path component
 */
private	remote *
url_parse(char *p)
{
	remote	*r;
	char	*s;

	unless (*p) return (0);
	new(r);
	r->rfd = r->wfd = -1;
	if (s = strchr(p, '@')) {		/* user@host[:path] */
		r->loginshell = 1;
		*s = 0; r->user = strdup(p); p = s + 1; *s = '@';
		if (s = strchr(p, ':')) {
			*s = 0; r->host = strdup(p); p = s + 1; *s = ':';
			r->path = strdup(p);
		} else {
			r->host = strdup(p);
		}
	} else if ((s = strchr(p, ':')) && isdigit(s[1])) { /* host:port[/path] */
		*s = 0; r->host = strdup(p); p = s + 1; *s = ':';
		r->port = atoi(p);
		/*
		 * Skip over port number to get to the path part.
		 * Note that the path may be in drive:path format if the URL
		 * destinaltion is a Win32 box
		 */
		while (isdigit(*p)) p++;
		if (*p == ':') p++; /* skip optional colon */
		if (*p) r->path = strdup(p);
	} else if (s = strchr(p, '/')) {	/* host/path */
		*s = 0;
		r->port = BK_PORT;
		r->host = strdup(p);
		*s = '/';
		r->path = strdup(s);
	} else { 				/* host */
		r->port = BK_PORT;
		r->host = strdup(p);
		r->path = 0;
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

	if (r->port || r->loginshell) {
		strcpy(buf, r->httpd ? "http://" : "bk://");
		if (r->user) {
			strcat(buf, r->user);
			strcat(buf, "@");
		}
		assert(r->host);
		strcat(buf, r->host);
		if (r->port) {
			if (r->port != BK_PORT) {
				strcat(buf, ":");
				sprintf(port, "%u", r->port);
				strcat(buf, port);
		    	}
		}
		if (r->path) {
			unless(r->path[0] == '/') strcat(buf, ":");
			strcat(buf, r->path);
		}
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



/*
 * Return the pid of a connected to daemon with stdin/out put in fds[].
 * Stderr is left alone, we don't want to touch that - ssh needs it for
 * password prompts and other commands may use it for status.
 */
pid_t
bkd(int compress, remote *r)
{
	char	*t, *freeme = 0;
	char	*remsh = "ssh";
	char	*remopts = compress ? "-C" : 0;
	char	*cmd[100];
	int	i;
	pid_t	p;
	int	findprog(char *);

	if (r->port) {
		assert(r->host);
		return (bkd_tcp_connect(r));
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
			if (check_rsh(remsh)) return (-1);
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
		unless (r->loginshell) {
			if (streq(r->host, "localhost")
			    && (t = getenv("PATH"))) {
				freeme = malloc(strlen(t) + 20);
				sprintf(freeme, "PATH=%s", t);
				cmd[++i] = "env";
				cmd[++i] = freeme;
			}
			cmd[++i] = "bk bkd -e";
		} else if (streq(cmd[0], "rsh") || streq(cmd[0], "remsh")) {
			fprintf(stderr,
			    "Warning: rsh doesn't work with bkd loginshell\n");
		}
		cmd[++i] = 0;
	} else {
		cmd[0] = "bk";
		cmd[1] = "bkd";
		cmd[2] = "-e";
		cmd[3] = 0;
    	}
	if (getenv("BK_DEBUG")) {
		for (i = 0; cmd[i]; i++) {
			fprintf(stderr, "CMD[%d]=%s\n", i, cmd[i]);
		}
	}

	signal(SIGCHLD, SIG_DFL);
	p = spawnvp_rwPipe(cmd, &(r->rfd), &(r->wfd), BIG_PIPE);
	if (freeme) free(freeme);
	return (p);
}
