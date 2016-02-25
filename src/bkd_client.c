/*
 * Copyright 2000-2011,2013,2016 BitMover, Inc
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

#include "bkd.h"

private remote	*file_parse(char *p);
private	remote	*nfs_parse(char *p, int flags);
private	remote	*url_parse(char *p, int default_port);
private	pid_t	bkd(remote *r);

/*
 * Turn either
 *	bk://host:port/pathname		<- running bkd form
 *	bk://user@host:port/pathname	<- rsh host -l user
 * or	user@host:pathname		<- rsh host bkd
 * into a struct remote.
 */
remote *
remote_parse(const char *url, u32 flags)
{
	char	*freeme = 0;
	static	int echo = -1;
	remote	*r;
	char	*p, *params;

	unless (url && *url) return (0);

	if (echo == -1) echo = getenv("BK_REMOTE_PARSE") != 0;

	freeme = p = strdup(url);
	if (params = strchr(p, '?')) *params++ = 0;
	if (strneq("bk://", p, 5)) {
		r = url_parse(p + 5, BK_PORT);
		if (r) {
			r->type = ADDR_BK;;
			if (r->user) r->loginshell = 1;
		}
	} else if (strneq("http://", p, 7)) {
		r = url_parse(p + 7, WEB_PORT);
		if (r) {
			r->type = ADDR_HTTP;
			r->httppath =
			    (flags & REMOTE_BKDURL)
			    ? "/cgi-bin/web_bkd" : r->path;
		}
	} else if (strneq("rsh://", p, 6)) {
		if (r = url_parse(p + 6, 0)) r->type = ADDR_RSH;
	} else if (strneq("ssh://", p, 6)) {
		if (r = url_parse(p + 6, 0)) r->type = ADDR_SSH;;
	} else {
		if (strneq("file://", p, 7)) {
			r = file_parse(p);
			if (r) r->type = ADDR_FILE;;
		} else if (strneq("file:/", p, 6)) {
			fprintf(stderr,
			    "\"file\" is an illegal host name.\n"
			    "Did you mean \"file://path\"?\n");
			r = NULL;
		} else {
			r = nfs_parse(p, flags);
		}
	}
	if (r) {
		if ((flags & REMOTE_ROOTKEY) && proj_isComponent(0)) {
			r->params = hash_new(HASH_MEMHASH);
			hash_storeStr(r->params, "ROOTKEY", proj_rootkey(0));
		}
		if (params) {
			unless (r->params) r->params = hash_new(HASH_MEMHASH);
			hash_fromStr(r->params, params);
			params[-1] = '?';
		}
	}
	/*
	 * Truncate ensemble roots which are like file:///build/freebsd-csets/.
	 */
	if (r && r->path && (p = strrchr(r->path, '/')) && streq(p, "/.")) {
		*p = 0;
	}
	if (echo && r) {
	    	fprintf(stderr, "RP[%s]->[%s]\n", p, remote_unparse(r));
		if (r->port) fprintf(stderr, "port=%u\n", r->port);
		if (r->user) fprintf(stderr, "user=%s\n", r->user);
		if (r->host) fprintf(stderr, "host=%s\n", r->host);
		if (r->path) fprintf(stderr, "path=%s\n", r->path);
	}
	if (freeme) free(freeme);
	if (r && getenv("BK_TRACE_REMOTE")) r->trace = 1;
	return (r);
}

int
remote_valid(char *url)
{
	remote	*r = remote_parse(url, 0);
	int	rc = 0;
	char	buf[MAXPATH];

	if (r) {
		rc = 1;
		if (r->notUrl) {
			concat_path(buf, r->path, BKROOT);
			unless (isdir(buf)) rc = 0;
		}
		remote_free(r);
	}
	return (rc);
}

/*
 * file:///full/path
 * Note: We do not support relative path in this format
 * due to ambiguouity with the host:/path format
 * We accept file://full/path
 * but treat it as a full path.
 */
private remote *
file_parse(char *p)
{
	remote	*r;

	r = new(remote);
	r->rfd = r->wfd = -1;
	p += 7;	/* skip file:// */
	if (isDriveColonPath(p)) { /* for win32 */
		r->path = strdup(p);
		return (r);
	}
	while (*p == '/') p++;
	r->path = aprintf("/%s", p);
	return (r);
}

/* [[user@]host:]path */
private	remote *
nfs_parse(char *p, int flags)
{
	remote	*r;
	char	*s;

	unless (*p) return (0);
	r = new(remote);
	r->rfd = r->wfd = -1;
	/* user@host:path */
	if (s = strchr(p, '@')) {
		*s = 0; r->user = strdup(p); p = s + 1; *s = '@';
	}
	/* just path, no host */
	unless (s = isHostColonPath(p)) {
		if (r->user) {
			p -= strlen(r->user) + 1;
			free(r->user);
			r->user = 0;
		}
		if (IsFullPath(p)) {
			r->path = strdup(p);
		} else if (flags & REMOTE_ROOTREL){
			r->path = strdup(proj_fullpath(0, p));
		} else {
			p = aprintf("%s/%s", start_cwd, p);
			r->path = fullname(p, 0);
			free(p);
		}
		r->type = ADDR_FILE;
		r->notUrl = 1;
		return (r);
	}
	r->type = ADDR_NFS;
	*s = 0; r->host = strdup(p); p = s + 1; *s = ':';
	unless (*p) p = ".";	/* we like having a path */

	r->path = strdup(p);
	return (r);
}

/*
 * host[:port]//path	(e.g. bitmover.com:80//full_path/to/repo)
 * host[:port]/path	(e.g. bitmover.com:80/relative_path/to/repo)
 * host[:port]:dospath	(e.g. bitmover.com:80:C:/full_path/to/repo)
 * user@host[//path]
 * user@host[/path]
 * user@host[:path]	(if path[0] is not a digit)
 * user@host[:port][:path]
 * host:port
 * host
 *
 * Note: If we are talking to a bkd on win32, there could be
 * 	 a colon in the path component
 */
private	remote *
url_parse(char *p, int default_port)
{
	remote	*r;
	char	*t = p;
	char	save;

	unless (*p) return (0);
	r = new(remote);
	r->rfd = r->wfd = -1;
	r->port = default_port;

	/* If there is a user, grab that */
	if (strchr(p, '@')) {
		/*
		 * Don't allow bogus crud in the user name.
		 * What I'm trying to catch here is a path that looks
		 * like a user name.
		 */
		for (t = p; *t != '@'; t++) {
			if (*t == '/') {
				goto not_a_user;
			}
		}
		*t = 0;
		r->user = strdup(p);
		*t = '@';
		p = t + 1;
	}
not_a_user:

	/*
	 * Windows supposedly uses _ in hostnames.
	 * (cf http://en.wikipedia.org/wiki/Hostname)
	 */
#define	ishost(c)	(isalnum(c) || (c == '-') || (c == '_') || (c == '.'))

	/* There has to be a host name */
	unless (ishost(*p)) {
error:		if (r->user) free(r->user);
		if (r->host) free(r->host);
		if (r->path) free(r->path);
		free(r);
		return (0);
	}

	for (t = p; ishost(*t); t++);
	save = *t;
	*t = 0;
	r->host = strdup(p);
	*t = save;
	p = t;

	/* There may be a port which must be all digits followed by [:/] */
	if ((*p == ':') && isdigit(p[1])) {
		for (t = p+1; isdigit(*t); t++);
		if ((*t == ':') || (*t == '/') || (*t == 0)) {
			r->port = atoi(++p);
			p = t;
		}
	}

	if (*p) {
		unless ((*p == ':') || (*p == '/')) goto error;
		if (p[1]) r->path = strdup(++p);
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
	char	*t, *ret;
	char	buf[MAXPATH*2];
	char	port[10];

	if ((r->type == ADDR_HTTP) || (r->type == ADDR_BK) ||
	    (r->type == ADDR_RSH) || (r->type == ADDR_SSH)) {
		switch (r->type) {
		    case ADDR_BK:	strcpy(buf, "bk://"); break;
		    case ADDR_HTTP:	strcpy(buf, "http://"); break;
		    case ADDR_RSH:	strcpy(buf, "rsh://"); break;
		    case ADDR_SSH:	strcpy(buf, "ssh://"); break;
		    default:		assert("unknown address type" == 0);
		}
		if (r->user) {
			strcat(buf, r->user);
			strcat(buf, "@");
		}
		assert(r->host);
		strcat(buf, r->host);
		if (r->port) {
			/*
			 * If default port, skip
			 */
			if (((r->type == ADDR_BK) && (r->port != BK_PORT))  ||
			    ((r->type == ADDR_SSH) && (r->port != SSH_PORT)) ||
			    ((r->type == ADDR_HTTP) && (r->port != WEB_PORT))) {
				strcat(buf, ":");
				sprintf(port, "%u", r->port);
				strcat(buf, port);
		    	}
		}
		if (r->path) {
			strcat(buf, "/");
			strcat(buf, r->path);
		}
		goto out;
	}
	if (r->user) {
		assert(r->host);
		sprintf(buf, "%s@%s:", r->user, r->host);
		if (r->path) strcat(buf, r->path);
		goto out;
	} else if (r->host) {
		sprintf(buf, "%s:", r->host);
		if (r->path) strcat(buf, r->path);
		goto out;
	}
	assert(r->path);
	if (IsFullPath(r->path)) {
		sprintf(buf, "file://%s", r->path);
	} else {
		/* if we get here, we got a relative path */
		strcpy(buf, r->path);
	}
out:
	if (r->params && (t = hash_toStr(r->params))) {
		ret = aprintf("%s?%s", buf, t);
		free(t);
	} else {
		ret = strdup(buf);
	}
	return (ret);
}

void
remote_free(remote *r)
{
	unless (r) return;
	if ((r->rfd != -1) || (r->wfd != -1)) {
		/*
		 * This shouldn't happen.  If we opened a connection to
		 * a remote bkd then we should disconnect before calling
		 * remote_free().
		 */
#if 0
		fprintf(stderr,
"remote_free(): freeing struct with open connection, disconnecting...\n");
#endif
		disconnect(r);
	}
	if (r->user) free(r->user);
	if (r->host) free(r->host);
	if (r->path) free(r->path);
	if (r->cred) free(r->cred);
	if (r->params) hash_free(r->params);
	free(r);
}

void
remote_print(remote *r, FILE *f)
{
	unless (r) return;
	fprintf(f, "R=%s ", p2str(r));
	if (r->user) fprintf(f, "USER=%s ", r->user);
	if (r->host) fprintf(f, "HOST=%s ", r->host);
	if (r->port) fprintf(f, "PORT=%u ", r->port);
	if (r->path) fprintf(f, "PATH=%s ", r->path);
	fprintf(f, "\n");
}

void
remote_error(remote *r, char *msg)
{
	char	*p;

	unless (r) return;
	p = remote_unparse(r);
	fprintf(stderr, "%s: %s: %s\n", p, prog, msg);
	free(p);
}

private pid_t
bkd_tcp_connect(remote *r)
{
	int	i;

	if (r->type == ADDR_HTTP) {
		http_connect(r);
	} else {
		i = tcp_connect(r->host, r->port);
		if (i < 0) {
			r->rfd = r->wfd = -1;
			if (i == -2) r->badhost = 1;
			if (i == -3) r->badconnect = 1;
		} else {
			r->rfd = r->wfd = i;
		}
	}
	r->isSocket = 1;
	return ((pid_t)0);
}

int
bkd_connect(remote *r, int opts)
{
	assert((r->rfd == -1) && (r->wfd == -1));
	r->pid = bkd(r);
	if (r->trace) {
		fprintf(stderr,
		    "bkd_connect: r->rfd = %d, r->wfd = %d\n", r->rfd, r->wfd);
	}
	if (r->wfd >= 0) return (0);
	unless (opts & SILENT) {
		if (r->badhost) {
			fprintf(stderr,
			    "Cannot resolve host '%s'.\n", r->host);
		} else if (r->badconnect) {
			fprintf(stderr,
			    "Unable to connect to host '%s'.\n", r->host);
		} else {
			char	*rp = remote_unparse(r);

			perror(rp);
			free(rp);
		}
	}
	return (-1);
}

/*
 * Return the pid of a connected to daemon with stdin/out put in fds[].
 * Stderr is left alone, we don't want to touch that - ssh needs it for
 * password prompts and other commands may use it for status.
 * 
 * **Win32 note: ssh does not work with BitKeeper when CYGWIN=tty is set,
 */
private pid_t
bkd(remote *r)
{
	char	*t, *freeme = 0, *freeme2 = 0;
	char	*remsh;
	char	*remopts;
	char	**bkrsh = 0;
	char	*rpath = 0;
	int	i, j;
	pid_t	p = -1;
	char	*cmd[100];
	char	port[6];

	/* default to user's compress level, but override later */
	r->gzip = r->gzip_in;

	if ((r->port == 1) && getenv("BK_REGRESSION")) {
		r->wfd = r->rfd = -1;
		r->badconnect = 1;
		return (0);
	}
	if (r->port && (r->type != ADDR_SSH) && !r->loginshell) {
		assert(r->host);
		return (bkd_tcp_connect(r));
	}
	if (r->host) {
		/* note that we are local, gzip/fastpatch care */
		if (isLocalHost(r->host) || streq(r->host, sccs_realhost())) {
			putenv("_BK_BKD_IS_LOCAL=1");
		}
		if (t = getenv("BK_RSH")) {
			/*
			 * Parse the command into words.
			 */
			bkrsh = shellSplit(t);
			i = 0;
			EACH_INDEX(bkrsh, j) cmd[i++] = bkrsh[j];
			unless (i) {
				fprintf(stderr, "error: BK_RSH empty\n");
				goto err;
			}
			i--; /* we do cmd[++i] = r->host below */
			if (streq(cmd[0], "rsh") && check_rsh(cmd[0])) {
				goto err;
			}
		} else {
			/* use rsh if told, or preferred or no ssh */
			remsh = "ssh";
			remopts = (r->gzip > 0) ? "-C" : 0;
			if ((r->type == ADDR_RSH) ||
			    ((r->type == ADDR_NFS) &&
			    (t = getenv("PREFER_RSH")) && streq(t, "YES")) ||
			    !(freeme = which("ssh"))) {
				remsh = "rsh";
#ifdef	hpux
				remsh = "remsh";
#endif
#ifdef	_SCO_XPG_VERS
				remsh = "rcmd";
#endif
				if (check_rsh(remsh)) return (-1);
				remopts = 0;
			}
			if (freeme) {
				free(freeme);
				freeme = 0;
			}
			cmd[i = 0] = remsh;
			if (remopts) { /* must be -C above */
				cmd[++i] = remopts;
				r->gzip = 0; /* don't compress extra */
			}
		}
		cmd[++i] = r->host;
		if (r->user) {
			cmd[++i] = "-l";
			cmd[++i] = r->user;
		}
		if ((r->type == ADDR_SSH) && r->port) {
			assert(sizeof(r->port) == 2);
			sprintf(port, "%u", r->port);
			cmd[++i] = "-p";
			cmd[++i] = port;
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
				freeme = aprintf("'PATH=%s'", t);
				cmd[++i] = "env";
				cmd[++i] = freeme;

				/*
				 * If we have a local BK_DOTBK pass it to the
				 * new subshell.  This is mostly so regressions
				 * don't pollute ~/.bk/repos
				 */
				if (t = getenv("BK_DOTBK")) {
					cmd[++i] = freeme2 =
					    aprintf("BK_DOTBK='%s'", t);
				}
			}
			/*
			 * Allow passing of the path to the remote bk path
			 * if we are using rsh or ssh.  In that case they
			 * have a login anyway so they have access to the
			 * machine.
			 * XXX - what about where the bkd is the login shell?
			 * I think in that case this is meaningless, correct?
			 * Isn't it the case that the command is just lost if
			 * we are already the bkd?
			 */
			if ((t = getenv("BK_REMOTEBIN")) &&
			    (r->type & (ADDR_NFS|ADDR_RSH|ADDR_SSH))) {
				rpath = aprintf("\"%s/bk\" bkd", t);
				cmd[++i] = rpath;
			} else {
				cmd[++i] = "bk bkd";
			}
			if (r->remote_cmd) cmd[++i] = "-U";
		} else if (streq(cmd[0], "rsh") || streq(cmd[0], "remsh")) {
			fprintf(stderr,
			    "Warning: rsh doesn't work with bkd loginshell\n");
		}
		cmd[++i] = 0;
	} else {
		putenv("_BK_BKD_IS_LOCAL=1");
		cmd[i=0] = "bk";
		cmd[++i] = "bkd";
		if (r->remote_cmd) cmd[++i] = "-U";
		cmd[++i] = 0;
    	}
	if (getenv("_BK_BKD_IS_LOCAL")) r->gzip = 0;
	if (getenv("BK_DEBUG")) {
		for (i = 0; cmd[i]; i++) {
			fprintf(stderr, "CMD[%d]=%s\n", i, cmd[i]);
		}
	}
	if ((p = spawnvpio(&(r->wfd), &(r->rfd), 0, cmd)) < 0) {
		fprintf(stderr, "%s: Command not found\n", cmd[0]);
	}
err:	if (freeme) free(freeme);
	if (freeme2) free(freeme2);
	if (rpath) free(rpath);
	if (bkrsh) freeLines(bkrsh, free);	/* if BK_RSH env var */
	return (p);
}
