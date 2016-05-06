/*
 * Copyright 1999-2016 BitMover, Inc
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
#include "progress.h"
#include "nested.h"
#include "cfg.h"
#ifdef WIN32
#include "Winbase.h"
#endif

bkdopts	Opts;	/* has to be declared here, other people use this code */

int
saveStdin(char *tmpfile)
{
	int	fd, n;
	char	buf[BUFSIZ];

	if ((fd = open(tmpfile, O_CREAT|O_TRUNC|O_WRONLY, 0664)) == -1) {
		perror(tmpfile);
		return (-1);
	}
	while ((n = read(0, buf, sizeof(buf))) > 0) {
		if (write(fd, buf, n) != n) {
			perror(tmpfile);
			close(fd);
			return (-1);
		}
	}
	close(fd);
	return (0);
}

int
out(char *buf)
{
	return (writen(1, buf, strlen(buf)));
}

int
outc(char c)
{
	return (writen(1, &c, 1));
}

/*
 * read a character at a time from stdin on the bkd.
 * returns EOF(-1) for EOF or ERROR!
 */
int
bkd_getc(void)
{
	u8	c;
	int	ret;

	if (Opts.use_stdio) {
		/*
		 * The goal here is to just consume the current stdio buffer
		 * and then refer back to character at a time IO.
		 * This is currently necessary because pull_part2 uses stdio
		 * for prunekeys, but part3 using direct IO for bk-remote
		 * to unpack BAM data.
		 * Yes, assume internals knowledge of our stdio library.
		 */
		if (stdin->_r > 0) {
			ret = getc(stdin);
			if (stdin->_r == 0) Opts.use_stdio = 0;
			return (ret);
		}
		Opts.use_stdio = 0;
	}
	if (read(0, &c, 1) != 1) return (EOF);
	return ((int)c);
}

void
error(const char *fmt, ...)
{
	va_list	ap;
	char	*p, *retval;

	va_start(ap, fmt);
	if (vasprintf(&retval, fmt, ap) < 0) retval = 0;
	va_end(ap);
	if (retval) {
		if ((p = getenv("_BK_IN_BKD")) && !streq(p, "QUIET")) {
			out("ERROR-");
			out(retval);
		} else if (p == 0) {
			fputs(retval, stderr);
		}
		free(retval);
	}
}

int
writen(int to, void *buf, int size)
{
	int	done, n, retry = 5;

	for (done = 0; done < size; ) {
		n = write(to, (u8 *)buf + done, size - done);
		if ((n == -1) && ((errno == EINTR) || (errno == EAGAIN))) {
			usleep(10000);
			unless (--retry) {
				perror("write");
				exit(1);
			}
			continue;
		} else if (n <= 0) {
			break;
		}
		done += n;
	}
	return (done);
}

int
fd2file(int from, char *to)
{
	int	fd = creat(to, 0664);
	int	n;
	char	buf[BUFSIZ];

	if (fd == -1) {
		perror(to);
		return (-1);
	}
	while ((n = read(from, buf, sizeof(buf))) > 0) writen(fd, buf, n);
	return (close(fd));
}

/*
 * Currently very lame because it does 1 byte at a time I/O
 * Usually used in bkd.
 * XXX:
 * If first parameter gets used, change its name.
 * If there is some massive cleanup, drop the first parameter.
 */
int
getline(int unused, char *buf, int size)
{
	int	i = 0;
	int	c;
	static	int echo = -1;

	assert(unused == 0);
	if (echo == -1) {
		echo = getenv("BK_GETLINE") != 0;
		if (getenv("BK_GETCHAR")) echo = 2;
	}
	buf[0] = 0;
	size--;
	unless (size) return (-3);
	for (;;) {
		if ((c = bkd_getc()) != EOF) {
			if (echo == 2) fprintf(stderr, "[%c]\n", c);
			if (((buf[i] = (char)c) == '\n') || (c == '\r')) {
				buf[i] = 0;
				if (echo) {
					fprintf(stderr,
					    "%u [%s]\n", getpid(), buf);
				}
				return (i + 1);	/* we did read a newline */
			}
			if (++i == size) {
				buf[i] = 0;
				return (-2);
			}
		} else {
			buf[i] = 0;
			if (echo) {
				fprintf(stderr,
				    "%u [%s] (unterminated)\n", getpid(), buf);
			}
			return (-1);
		}
	}
}

int
read_blk(remote *r, char *buf, int len)
{
	if (r->rf) {
		return (fread(buf, 1, len, r->rf));
	} else {
		return (read(r->rfd, buf, len));
	}
}

/*
 * We have completed our transactions with a remote bkd and closed our
 * write filehandle, now we want to wait until the remote bkd has
 * finished so we can be sure all locks are freed.  We might have to
 * read garbage data from the pipe in order to get to the EOF.  Since
 * we might be attached to a random port that will never close we
 * shouldn't wait forever.  For now, we wait until 2 Megs of junk has
 * been read.
 *
 * This function should appear in code like this:
 *     wait_eof(r, 0);
 *     disconnect(r);
 *     remote_free(r);
 */
void
wait_eof(remote *r, int verbose)
{
	int	i;
	int	bytes = 0;
	char	buf[MAXLINE];

	if (r->wfd != -1) {
		if (r->isSocket) {
			shutdown(r->wfd, 1);
		} else {
			close(r->wfd);
		}
		r->wfd = -1;
	}
	if (verbose) fprintf(stderr, "Waiting for remote to disconnect\n");
	while (bytes < 2 * 1024 * 1024) {
		i = read_blk(r, buf, sizeof(buf) - 1);
		if (i <= 0) {
			if (verbose) fprintf(stderr, "Remote Disconnected\n");
			return;
		}
		bytes += i;
	}
	if (verbose) {
		fprintf(stderr,
		    "wait_eof: Got %d unexpected byte(s) from remote\n",
		    bytes);
		buf[i] = 0;
		fprintf(stderr, "buf=\"%s\"\n", buf);
	}
}

/*
 * 
 * Get a line : Unlke the getline() above, '\r' by itself is _not_ considered
 * as a line terminator, `\r' is striped if it is followed by the `\n`
 * character.
 * This version uses the read_blk() instead of read() interface
 * On WIN32, read() does not work on a socket.
 * TODO: This function should be merged with the getline() function above
 */
int
getline2(remote *r, char *buf, int size)
{
	int	ret, i = 0;
	char	c;
	static	int echo = -1;

	if (echo == -1) {
		echo = getenv("BK_GETLINE") != 0;
		if (getenv("BK_GETCHAR")) echo = 2;
	}
	buf[0] = 0;
	size--;
	unless (size) return (-3);
	for (;;) {
		switch (ret = read_blk(r, &c, 1)) {
		    case 1:
			if (echo == 2) fprintf(stderr, "[%c]\n", c);
			if (((buf[i] = c) == '\n')) {
				if ((i > 0) && (buf[i-1] == '\r')) i--;
				buf[i] = 0;
				if (echo) {
					fprintf(stderr,
					    "%u [%s]\n", getpid(), buf);
				}
				return (i + 1);	/* we did read a newline */
			}
			if (++i == size) {
				buf[i] = 0;
				return (-2);
			}
			break;

		    default:
			buf[i] = 0;
			if (echo) {
				if (ret) perror("getline2");
				fprintf(stderr, "[%s]=%d\n", buf, ret);
			}
			return (-1);
		}
	}
}

private	int	caught;
private	void	abort_prompt(int dummy) { caught++; }

/*
 * Prompt the user and get an answer.
 * The buffer has to be MAXPATH bytes long.
 */
int
prompt(char *msg, char *buf)
{
	int	ret;

	caught = 0;
	sig_catch(abort_prompt);

	write(2, msg, strlen(msg));
	write(2, " ", 1);
	ret = getline(0, buf, MAXPATH) > 1;
	if (caught) {
		fprintf(stderr, "\n(interrupted)\n");
		assert(!ret);
	}
	sig_restore();
	return (ret);
}

int
confirm(char *msg)
{
	char	*p, buf[100];
	int	gotsome;

	caught = 0;
	sig_catch(abort_prompt);

	write(2, msg, strlen(msg));
	write(2, " (y/n) ", 7);
	gotsome = getline(0, buf, sizeof(buf)) > 1;
	if (caught) {
		fprintf(stderr, "\n(interrupted)\n");
		assert(!gotsome);
	}
	sig_restore();
	unless (gotsome) return (0);
	for (p = buf; *p && isspace(*p); p++);
	return ((*p == 'y') || (*p == 'Y'));
}

int
usleep_main(int ac, char **av)
{
	u32	left;

	unless (av[1]) return (1);
	left = atoi(av[1]);
	while (left > 999999) {
		usleep(999999);
		left -= 999999;
	}
	if (left) usleep(left);
	return (0);
}

int
tmpdir_main(int ac, char **av)
{
	printf("%s\n", TMP_PATH);
	return (0);
}

/*
 * Usage: bk prompt [-n NO] [-y YES] [-t TITLE] msg | -f FILE | -p program
 */
int
prompt_main(int ac, char **av)
{
	int	i, c, ret, ask = 1, nogui = 0, gui = 0;
	char	*prog = 0, *file = 0, *no = "NO", *yes = "OK", *title = 0;
	char	*type = 0;
	char	*cmd, *p;
	FILE	*in;
	char	msgtmp[MAXPATH];
	char	buf[1024];
	char	**lines = 0;

	while ((c = getopt(ac, av, "cegGiowxf:n:p:t:Ty:", 0)) != -1) {
		switch (c) {
		    case 'c': ask = 0; break;
		    case 'e': type = "-E"; break;
		    case 'G': gui = 1; break;
		    case 'i': type = "-I"; break;
		    case 'w': type = "-W"; break;
		    case 'x': /* ignored, see notice() for why */ break;
		    case 'f': file = optarg; break;
		    case 'n': no = optarg; break;
		    case 'o': no = 0; break;
		    case 'p': prog = optarg; break;
		    case 't': title = optarg; break;	/* Only for GUI */
		    case 'T': /* text, supported option */
		    case 'g': /* old, do not doc */
		    	nogui = 1; break;
		    case 'y': yes = optarg; break;
		    default: bk_badArg(c, av);
		}
	}
	if (((file || prog) && av[optind]) ||
	    (!(file || prog) && !av[optind]) ||
	    (av[optind] && av[optind+1]) || (file && prog) ||
	    (av[optind] && !av[optind][0])) {
		if (file == msgtmp) unlink(msgtmp);
		usage();
	}
	if (prog) {
		assert(!file);
		bktmp(msgtmp);
		file = msgtmp;
		cmd = aprintf("%s > '%s'", prog, file);

		/* For caching of the real pager */
		(void)pager();
		putenv("PAGER=cat");
		if (system(cmd)) {
err:			if (file == msgtmp) unlink(msgtmp);
			if (lines) freeLines(lines, free);
			exit(2);
		}
		free(cmd);
	}
	if ((gui || gui_useDisplay()) && !nogui) {
		char	*nav[19];

		nav[i=0] = "bk";
		nav[++i] = "msgtool";
		if (title) {
			nav[++i] = "-T";
			nav[++i] = title;
		}
		if (no) {
			nav[++i] = "-N";
			nav[++i] = no;
		}
		assert(yes);
		nav[++i] = "-Y";
		nav[++i] = yes;
		if (type) nav[++i] = type;
		if (file) {
			nav[++i] = "-F";
			nav[++i] = file;
		} else {
			nav[++i] = av[optind];
		}
		nav[++i] = 0;
		assert(i < sizeof(nav)/sizeof(char*));
		ret = spawnvp(_P_WAIT, nav[0], nav);
		if (prog) unlink(msgtmp);
		if (WIFEXITED(ret)) exit(WEXITSTATUS(ret));
		exit(2);
	}

	/* in trigger and in text mode */
	if ((p = getenv("BK_EVENT")) &&
	    ((i = open(DEV_TTY, O_RDWR, 0)) >= 0)) {
		dup2(i, 0);
		dup2(i, 1);
		dup2(i, 2);
		if (i > 2) close(i);
	}

	if (type) {
		int	j, len, half;

		switch (type[1]) {
		    case 'x': type = 0; break;
		    case 'E': type = "Error"; break;
		    case 'I': type = "Info"; break;
		    case 'W': type = "Warning"; break;
		}
		len = strlen(type) + 4;
		half = (76 - len) / 2;
		for (j = i = 0; i < half; ++i) buf[j++] = '=';
		buf[j++] = ' ';
		buf[j++] = ' ';
		for (i = 0; type[i]; buf[j++] = type[i++]);
		buf[j++] = ' ';
		buf[j++] = ' ';
		if (len & 1) buf[j++] = ' ';
		for (i = 0; i < half; ++i) buf[j++] = '=';
		buf[j++] = '\n';
		buf[j++] = 0;
		lines = addLine(lines, strdup(buf));
	}
	if (file) {
		unless (in = fopen(file, "r")) goto err;
		while (fnext(buf, in)) {
			lines = addLine(lines, strdup(buf));
		}
		fclose(in);
	} else if (streq(av[optind], "-")) {
		goto err;
	} else {
		lines = addLine(lines, strdup(av[optind]));
		lines = addLine(lines, strdup("\n"));
	}
	if (type) {
		for (i = 0; i < 76; ++i) buf[i] = '=';
		buf[i++] = '\n';
		buf[i++] = 0;
		lines = addLine(lines, strdup(buf));
	}
	if (nLines(lines) <= 24) {
		EACH(lines) {
			fprintf(stderr, "%s", lines[i]);
		}
		fflush(stderr); /* for win32 */
	} else {
		FILE	*out;

		signal(SIGPIPE, SIG_IGN);
		sprintf(buf, "%s 1>&2", pager());
		out = popen(buf, "w");
		EACH(lines) {
			fprintf(out, "%s", lines[i]);
		}
		pclose(out);
	}
	if (lines) freeLines(lines, free);
	if (prog) unlink(msgtmp);
	/* No exit status if no prompt */
	unless (ask) exit(0);
	exit(confirm(yes ? yes : "OK") ? 0 : 1);
}

/*
 * Return true if spath is the real ChangeSet file.
 * Must be light weight, it is called from sccs_init().
 * It assumes ChangeSet and BitKeeper/etc share the same parent dir.
 * XXX Moving the ChangeSet file will invalidate this code!
 * Handles the following cases:
 * a) spath = not a ChangeSet path	# common case
 * b) spath = SCCS/s.ChangeSet		# common case
 * c) spath = XSCCS/s.ChangeSet		# should not happen
 * d) spath = /SCCS/s.ChangeSet		# repo root is "/"
 * e) spath = X/SCCS/s.ChangeSet	# X is "." or other prefix
 */
int
isCsetFile(char *spath)
{
	char	*q;
	char	buf[MAXPATH];

	unless (spath) return (0);

	/* find "SCCS/s.ChangeSet" suffix */
	unless (q = strrchr(spath, '/')) return (0);
	q -= 4;
	unless ((q >= spath) && patheq(q, CHANGESET)) return (0);
								/* case ' a' */

	if (q == spath) return (isdir(BKROOT));			/* case ' b' */
	--q;
	if (*q != '/') return (0);				/* case ' c' */
	if (q == spath) return (0);				/* case ' d' */

	/* test if pathname contains BKROOT */
	sprintf(buf, "%.*s/" BKROOT, (int)(q - spath), spath);
	return (isdir(buf));					/* case ' e' */
}

private int
send_msg(remote *r, char *msg, int mlen, int extra)
{
	assert(r->wfd != -1);
	if (r->type == ADDR_HTTP) {
		if (http_send(r, msg, mlen, extra, "send")) {
			fprintf(stderr, "http_send failed\n");
			return (-1);
		}
	} else {
		if (writen(r->wfd, msg, mlen) != mlen) {
			remote_error(r, "write to remote failed");
			return (-1);
		}
	}
	return (0);
}

/*
 * send commands to bkd
 *
 * if extra>0 then that many more bytes are to be included in this
 * connection and will be send to r->wfd next.
 * In this case send_file_extra_done() must be called after this
 * extra data is sent.
 */
int
send_file(remote *r, char *file, int extra)
{
	MMAP	*m;
	int	rc, len;
	int	no_extra = (extra == 0);

	assert(r->wfd >= 0);	/* we should be connected */
	m = mopen(file, "r");
	assert(m);
	assert(strneq(m->mmap, "putenv ", 7));
	len = m->size;

	if (r->type == ADDR_HTTP) extra += 5;	/* "quit\n" */
	rc = send_msg(r, m->mmap, len, extra);
	if (r->trace) {
		fprintf(stderr, "Sending...\n");
		fwrite(m->mmap, 1, len, stderr);
	}
	mclose(m);
	unless (rc) {
		r->need_exdone = 1;
		if (no_extra) rc = send_file_extra_done(r);
	}
	return (rc);
}

/*
 * When extra data is sent after send_file() this call marks the end
 * of the extra data.
 */
int
send_file_extra_done(remote *r)
{
	int	rc = 0;

	assert(r->need_exdone);
	r->need_exdone = 0;
	if (r->type == ADDR_HTTP) {
		if (writen(r->wfd, "quit\n", 5) != 5) {
			remote_error(r, "write to remote failed");
			rc = -1;
		}
		if (r->trace) {
			fprintf(stderr, "quit\n");
		}
	}
	return (rc);
}

/*
 * Skip to the end of a http header.  This is called in both the bkd
 * and the client so it processes both request and response headers.
 */
int
skip_http_hdr(remote *r)
{
	char	*p;
	int	line = 0;
	char	buf[1024];

	r->contentlen = -1;

	while (getline2(r, buf, sizeof(buf)) >= 0) {
		if ((line == 0) && strneq(buf, "HTTP/", 5)) {
			/* detect response header and handle errs */
			unless (p = strchr(buf, ' ')) return (-1);
			unless (atoi(p+1) == 200) {
				return (-1);
			}
		}
		if (p = strchr(buf, ':')) {
			*p++ = 0;
			while (isspace(*p)) ++p;
			if (strieq(buf, "Content-length")) {
				r->contentlen = atoi(p);
			}
		}
		if (buf[0] == 0) return (0); /*ok */
		++line;
	}
	return (-1); /* failed */
}

void
disconnect(remote *r)
{
	assert(!r->need_exdone);
	if (r->isSocket && (r->wfd != -1)) {
		shutdown(r->wfd, 2);
	}
	if (r->rf) {
		fclose(r->rf);
		r->rf = 0;
	} else if (r->rfd >= 0) {
		close(r->rfd);
	}
	if ((r->wfd >= 0) && (r->wfd != r->rfd)) close(r->wfd);
	r->rfd = r->wfd = -1;

	if ((r->pid > 0) && (r->rfd == -1) && (r->wfd == -1)) {
		/*
		 * We spawned a bkd in the background to handle this
		 * connection. When it get here it SHOULD be finished
		 * already, but just in case we close the connections
		 * and wait for that process to exit.
		 */
		waitpid(r->pid, 0, 0);
		r->pid = 0;
	}
}


// XXX verbose is always==1 other than in lconfig where stderr is
//      redirected to /dev/null.  Perhaps it should be removed.
int
get_ok(remote *r, char *read_ahead, int verbose)
{
	int 	i, ret;
	char	buf[512], *p, *url;

	if (read_ahead) {
		p = read_ahead;
	} else {
		ret = getline2(r, buf, sizeof(buf));
		if (ret <= 0) {
			/* ssh/rsh login failure will give us EOF */
			//if (verbose) fprintf(stderr, "get_ok: Got EOF.\n");
			return (1); /* failed */
		}
		p = buf;
	}

	if (streq(p, "@OK@")) return (0); /* ok */
	if (strneq(p, "ERROR-unable to update BAM server", 33)) {
		if (verbose) fprintf(stderr, "%s\n", p);
		return (-1000);
	}
	if (verbose) {
		i = 0;
		url = remote_unparse(r);
		if (p && *p && strneq(p, "ERROR-", 6)) p += 6;
		if (p && *p) fprintf(stderr, "%s: %s\n", url, p);
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (streq(buf, "@END@")) break;
			p = buf;
			if (p && *p && strneq(p, "ERROR-", 6)) p += 6;

			// The exiting message just muddies the water.
			if (p && *p && !streq(p, "exiting")) {
				fprintf(stderr, "%s: %s\n", url, p);
			}
			/*
			 * 20 lines of error message should be enough
			 */
			if (i++ > 20) break;
		}
		free(url);
	}
	return (1); /* failed */
}

void
add_cd_command(FILE *f, remote *r)
{
	char	*t;
	char	*rootkey = 0;
	char	buf[MAXPATH];

	if (r->params) rootkey = hash_fetchStr(r->params, "ROOTKEY");
	unless (r->path || rootkey) return;
	buf[0] = 0;
	if (r->path) strcpy(buf, r->path);
	if (rootkey) sprintf(buf + strlen(buf), "|%s", rootkey);
	t = shellquote(buf);
	fprintf(f, "cd %s\n", t);
	free(t);
}

void
putroot(char *where)
{
	char	*root = proj_root(0);

	if (root) {
		if (streq(root, ".")) {
			char	pwd[MAXPATH];

			strcpy(pwd, proj_cwd());
			safe_putenv("%s_ROOT=%s", where, pwd);
		} else {
			safe_putenv("%s_ROOT=%s", where, root);
		}
	}
}

/*
 * Send env variables to remote bkd.
 */
void
sendEnv(FILE *f, char **envVar, remote *r, u32 flags)
{
	int	i;
	char	*user, *host, *repo, *proj, *bp;
	char	*t;
	project	*p = proj_init(".");
	project	*prod = p;	/* product if we have one */
	char	buf[MAXLINE];

	assert(r->wfd >= 0);	/* we should be connected */
	/*
	 * Send any vars the user requested first so that they can't
	 * overwrite any of the standard variables.
	 */
	EACH(envVar) fprintf(f, "putenv %s\n", envVar[i]);

	if (r->host) fprintf(f, "putenv BK_VHOST=%s\n", r->host);
	fprintf(f, "putenv BK_REMOTE_PROTOCOL=%s\n", BKD_VERSION);
	fprintf(f, "putenv BK_VERSION=%s\n", bk_vers);
	fprintf(f, "putenv BK_UTC=%s\n", bk_utc);
	fprintf(f, "putenv BK_TIME_T=%s\n", bk_time);
	user = sccs_getuser();
	fprintf(f, "putenv BK_USER=%s\n", user);
	fprintf(f, "putenv _BK_USER=%s\n", user);	/* XXX remove in 3.0 */
	host = sccs_gethost();
	fprintf(f, "putenv _BK_HOST=%s\n", host);
	fprintf(f, "putenv BK_REALUSER=%s\n", sccs_realuser());
	fprintf(f, "putenv BK_REALHOST=%s\n", sccs_realhost());
	fprintf(f, "putenv BK_PLATFORM=%s\n", platform());

	/*
	 * If we are saving a BKD_NESTED_LOCK from
	 * getServerInfo() then send it back to the back here.
	 * If we ever talk to two different bkd's from the same bk
	 * process then we need to clear BKD_NESTED_LOCK so a new lock
	 * can be acquired.
	 */
	if (t = getenv("BKD_NESTED_LOCK")) {
		fprintf(f, "putenv 'BK_NESTED_LOCK=%s'\n", t);
	}
	/*
	 * If we're doing a port, send the rootkey
	 */
	if (t = getenv("BK_PORT_ROOTKEY")) {
		fprintf(f, "putenv 'BK_PORT_ROOTKEY=%s'\n", t);
	}
	if (getenv("_BK_PROGRESS_MULTI")) {
		fprintf(f, "putenv _BK_PROGRESS_MULTI=YES\n");
	}

	if (flags & SENDENV_NOREPO) {
		/* remember the local repo wasnt part of the connection */
		r->noLocalRepo = 1;
	} else {
		/*
		 * This network connection is not necessarily run from
		 * a repository, so don't send information about the
		 * current repository.  Clone is the primary example
		 * of this.
		 */
		assert(p);	/* We must be in a repo here */
		fprintf(f, "putenv BK_LEVEL=%d\n", getlevel());
		proj = proj_root(p);
		fprintf(f, "putenv 'BK_ROOT=%s'\n", proj);
		fprintf(f, "putenv BK_REPOTYPE=");	// no newline here
		/* Match :REPOTYPE: */
		if (proj_isProduct(p)) {
			fprintf(f, "product\n");
		} else if (proj_isComponent(p)) {
			prod = proj_product(p);
			fprintf(f, "component\n");
		} else {
			fprintf(f, "standalone\n");
		}
		fprintf(f, "putenv 'BK_ROOTKEY=%s'\n", proj_rootkey(p));
		unless (streq(proj_rootkey(p), proj_syncroot(p))) {
			fprintf(f, "putenv 'BK_SYNCROOT=%s'\n", proj_syncroot(p));
		}
		if (repo = proj_repoID(prod)) {
			fprintf(f, "putenv 'BK_REPO_ID=%s'\n", repo);
			if (bp_hasBAM()) fprintf(f, "putenv BK_BAM=YES\n");
			if (bp = bp_serverURL(buf)) {
				fprintf(f, "putenv 'BK_BAM_SERVER_URL=%s'\n",bp);
			}
			unless (bp = bp_serverID(buf, 0)) {
				bp = repo;
			}
			fprintf(f, "putenv 'BK_BAM_SERVER_ID=%s'\n", bp);
		}
		if ((flags & SENDENV_FORCEREMAP) ||
		    !(proj_hasOldSCCS(0) || (flags & SENDENV_FORCENOREMAP))) {
			fprintf(f, "putenv BK_REMAP=1\n");
		}
	}
	if (t = getenv("_BK_TESTFEAT")) {
		t = strdup(t);
	} else {
		u32	bits = features_list(); /* all supported features */
#if 0
		unless (flags & SENDENV_NOREPO) {
			/*
			 * prevent old bk's from including sfio in patches
			 * bk-5.x  pSFIO feature (now gone)
			 * bk-6.x  BKFILE feature
			 * bk-7.x  BKMERGE feature
			 */
			bits &= ~(FEAT_BKFILE|FEAT_BKMERGE);
		}
#endif
		t = features_fromBits(bits);
	}
	fprintf(f, "putenv BK_FEATURES=%s\n", t);
	free(t);
	unless (flags & SENDENV_NOREPO) {
		u32	bits = features_bits(0);

		t = features_fromBits(bits);
		fprintf(f, "putenv BK_FEATURES_USED=%s\n", t);
		free(t);

		if (t = getenv("_BK_TEST_REQUIRED")) {
			t = strdup(t);
		} else {
			/* remove local-only features */
			bits &= ~(FEAT_REMAP|FEAT_SCANDIRS);

			/* only clone needs these since the patch format
			 * can encode them
			 */
			unless (flags & SENDENV_SENDFMT) {
				bits &= ~FEAT_FILEFORMAT;
			}
			t = features_fromBits(bits);
		}
		fprintf(f, "putenv BK_FEATURES_REQUIRED=%s\n", t);
		free(t);
	}
	if (p) proj_free(p);
}

/*
 * Process the @SERVER INFO@ block from the bkd which may contain an
 * ERROR- message that should be expanded for the user.
 */
int
getServerInfo(remote *r, hash *bkdEnv)
{
	int	ret = 1; /* protocol error, never saw @END@ */
	char	*p, *key;
	char	buf[4096];

	unless (bkdEnv) {
		/* clear previous values for stuff that is set conditionally */
		putenv("BKD_BAM=");
		putenv("BKD_BAM_SERVER_URL=");
		putenv("BKD_PRODUCT_ROOTKEY=");
		putenv("BKD_REMAP=");
	}
	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (streq(buf, "@END@")) {
			ret = 0; /* ok */
			break;
		}
		if (r->trace) fprintf(stderr, "Server info:%s\n", buf);

		if (strneq(buf, "ERROR-bk_missing_feature ", 25)) {
			getMsg("bk_missing_feature", buf+25, '=', stderr);
			return (1);
		} else if (strneq(buf, "ERROR-bkd_missing_feature ", 26)) {
			getMsg("bkd_missing_feature", buf+26, '=', stderr);
			return (1);
		} else if (strneq(buf, "ERROR-", 6)) {
			char	*tmpf = bktmp(0);
			FILE	*f = fopen(tmpf, "w");

			unless (getMsgP(buf+6, 0, 0, 0, f)) {
				getMsgP("other-error", buf+6, 0, 0, f);
			}
			fclose(f);
			sys("bk", "prompt", "-eocf", tmpf, SYS);
			unlink(tmpf);
			free(tmpf);
			return (1);
		}
		if (strneq(buf, "PROTOCOL", 8)) {
			if (bkdEnv) {
				p = strchr(buf, '=');
				assert(p);
				*p++ = 0;
				key = aprintf("BK_REMOTE_%s", buf);
				hash_storeStrStr(bkdEnv, key, p);
				free(key);
			} else {
				safe_putenv("BK_REMOTE_%s", buf);
			}
		} else {
			if (bkdEnv) {
				p = strchr(buf, '=');
				assert(p);
				*p++ = 0;
				key = aprintf("BKD_%s", buf);
				hash_storeStrStr(bkdEnv, key, p);
				free(key);
				*(--p) = '=';
			} else {
				safe_putenv("BKD_%s", buf);
			}
			if (strneq(buf, "REPO_ID=", 8)) {
				cmdlog_addnote("rmts", buf+8);
			}
		}
	}
	if (ret) {
		fprintf(stderr, "%s: premature disconnect\n", prog);
	} else if (features_bkdCheck(0, r->noLocalRepo)) {
		/* cleanup stale nested lock */
		if ((bkdEnv && hash_fetchStr(bkdEnv, "BKD_NESTED_LOCK")) ||
		    getenv("BKD_NESTED_LOCK")) {
			FILE	*f;
			char	buf[MAXPATH];

			if (r->type == ADDR_HTTP) {
				disconnect(r);
				bkd_connect(r, 0);
			}
			bktmp(buf);
			f = fopen(buf, "w");
			assert(f);
			sendEnv(f, 0, r, SENDENV_NOREPO);
			if (r->type == ADDR_HTTP) add_cd_command(f, r);
			fprintf(f, "nested abort\n");
			fclose(f);
			if (send_file(r, buf, 0)) return (1);
			unlink(buf);
			while (getline2(r, buf, sizeof(buf)) > 0) {
				if (streq(buf, "@OK@")) break;
			}
		}
		ret = 1;
	}
	return (ret);
}


/*
 *  Send server env from bkd to client
 */
int
sendServerInfo(u32 cmdlog_flags)
{
	char	*repoid, *rootkey, *p;
	project	*prod = 0;	/* product project* (if we have a product) */
	int	no_repo = (cmdlog_flags & CMD_NOREPO);
	u32	bits;
	char	buf[MAXPATH];
	char	bp[MAXLINE];

	unless (no_repo || isdir(BKROOT)) no_repo = 1;

	out("@SERVER INFO@\n");
	if (features_bkdCheck(1, no_repo)) {
		return (1);
	}
        sprintf(buf, "PROTOCOL=%s\n", BKD_VERSION);	/* protocol version */
	out(buf);

        sprintf(buf, "VERSION=%s\n", bk_vers);		/* binary version   */
	out(buf);
        sprintf(buf, "UTC=%s\n", bk_utc);
	out(buf);
        sprintf(buf, "TIME_T=%s\n", bk_time);
	out(buf);

	unless (no_repo) {
		/*
		 * Some comands don't have a tree on the remote
		 * side. These include, but are not limited to:
		 * rclone, kill, etc.
		 */
		sprintf(buf, "LEVEL=%d\n", getlevel());
		out(buf);
		if (bp_hasBAM()) out("BAM=YES\n");
		if (p = bp_serverURL(bp)) {
			sprintf(buf, "BAM_SERVER_URL=%s\n", p);
			out(buf);
		}
		if (rootkey = proj_rootkey(0)) {
			sprintf(buf, "ROOTKEY=%s\n", rootkey);
			out(buf);
			p = proj_syncroot(0);
			unless (streq(rootkey, p)) {
				sprintf(buf, "SYNCROOT=%s\n", p);
				out(buf);
			}
		}
		/* Match :REPOTYPE: */
		if (proj_isComponent(0)) {
			prod = proj_product(0);
			sprintf(buf, "REPOTYPE=component\nPRODUCT_ROOTKEY=%s\n"
			    "COMPONENT_PATH=%s\n",
			    proj_rootkey(prod),
			    proj_relpath(prod, proj_root(0)));
			out(buf);
		} else if (proj_isProduct(0)) {
			out("REPOTYPE=product\n");
			if (nested_isPortal(0)) {
				out("PORTAL=1\n");
			}
			if (nested_isGate(0)) {
				out("GATE=1\n");
			}
		} else {
			out("REPOTYPE=standalone\n");
		}
		if (repoid = proj_repoID(prod)) {
			sprintf(buf, "REPO_ID=%s\n", repoid);
			out(buf);
			unless (p = bp_serverID(bp, 0)) p = repoid;
			sprintf(buf, "BAM_SERVER_ID=%s\n", p);
			out(buf);
		}
		unless (proj_hasOldSCCS(0)) out("REMAP=1\n");
		sprintf(buf, "NFILES=%u\n", repo_nfiles(0,0));
		out(buf);
		sprintf(buf,
		    "CLONE_DEFAULT=%s\n", cfg_str(0, CFG_CLONE_DEFAULT));
		out(buf);
		sprintf(buf, "TIP_MD5=%s\n", proj_tipmd5key(0));
		out(buf);
		sprintf(buf, "TIP_KEY=%s\n", proj_tipkey(0));
		out(buf);
		sprintf(buf, "TIP_REV=%s\n", proj_tiprev(0));
	}
	out("ROOT=");
	strcpy(buf, proj_cwd());
	out(buf);
	out("\nUSER=");
	out(sccs_getuser());
	out("\nHOST=");
	out(sccs_gethost());
	out("\nREALUSER=");
	out(sccs_realuser());
	out("\nREALHOST=");
	out(sccs_realhost());
	out("\nPLATFORM=");
	out(platform());
	if (p = getenv("_BKD_TESTFEAT")) {
		p = strdup(p);
	} else {
		u32	bits = features_list(); /* all supported features */
#if 0
		unless (no_repo) {
			/* prevent old bk's from including sfio in patches */
			bits &= ~(FEAT_BKFILE|FEAT_BKMERGE);
		}
#endif
		p = features_fromBits(bits);
	}
	out("\nFEATURES=");
	out(p);
	free(p);
	bits = features_bits(0);
	out("\nFEATURES_USED=");
	p = features_fromBits(bits);
	out(p);
	free(p);
	if (p = getenv("_BKD_TEST_REQUIRED")) {
		p = strdup(p);
	} else {
		/* remove local-only features */
		bits &= ~(FEAT_REMAP|FEAT_SCANDIRS);
		bits &= ~FEAT_FILEFORMAT;
		p = features_fromBits(bits);
	}
	out("\nFEATURES_REQUIRED=");
	out(p);
	free(p);

	/* send local nested lock to BKD_NESTED_LOCK on client */
	if (p = getenv("_BK_NESTED_LOCK")) {
		out("\nNESTED_LOCK=");
		out(p);
	}
	out("\n@END@\n");
	return (0);
}

/*
 * Generate a http response header from a bkd back to the client.
 */
void
http_hdr(void)
{
	static	int done = 0;

	if (done) return; /* do not send it twice */
	if (getenv("GATEWAY_INTERFACE")) {
		/* looks like a CGI context */
		out("Status: 200 OK\r\n");
	} else {
		out("HTTP/1.0 200 OK\r\n");
	}
	out("Server: BitKeeper daemon ");
	out(bk_vers);
	out("\r\n");
	out("Cache-Control: no-cache\r\n");	/* for http 1.1 */
	out("Pragma: no-cache\r\n");		/* for http 1.0 */
	out("Content-type: text/plain\r\n");
	out("\r\n");				/* end of header */
	done = 1;
}

/*
 * Drain error message:
 * Theses are the possibilities:
 * 1) remote is running a version 1.2 (i.e. old) bkd
 * 2) remote is not running a bkd.
 * 3) remote is running a current bkd, but have some type of config/path error
 * 4) remote is running a current bkd, but some command have been disabled
 *
 * XXX This function could be simplied if we don't need to detect old bkd.
 */
void
drainErrorMsg(remote *r, char *buf, int bsize)
{
	int	bkd_msg = 0, i;
	char	**lines = 0;

	lines = addLine(lines, strdup(buf));
	if (strneq("ERROR-BAD CMD: putenv", buf, 21)) {
		bkd_msg = 1;
		while (getline2(r, buf, bsize) > 0) {
			lines = addLine(lines, strdup(buf));
			if (strneq("ERROR-BAD CMD: putenv", buf, 21)) continue;
			break;
		}
	}

	while (1) {
		if (strneq("ERROR-BAD CMD: pull_part1", buf, 25)) {
			getMsg("no_pull_part1", 0, 0, stderr);
			break;
		}
		if (strneq("ERROR-BAD CMD: @END", buf, 19)) break; /*for push*/
		/*
		 * Comment the following code out, it is causing problem
		 * when we have case 4.
		 * And I don't remember why I need it in the first place.
		 *
		 * if (strneq("ERROR-BAD CMD:", buf, 14)) goto next;
		 */
		if (streq("OK-root OK", buf)) goto next;
		if (streq("ERROR-exiting", buf)) exit(1);
		if (!strneq("ERROR-", buf, 6)) {
			fprintf(stderr,
				"drainErrorMsg: Unexpected response:\n");
			EACH (lines) fprintf(stderr, "%s\n", lines[i]);
		} else {
			fprintf(stderr, "%s\n", buf);
		}
		break;
next:		if (getline2(r, buf, bsize) <= 0) break;
		lines = addLine(lines, strdup(buf));
	}

	if (bkd_msg) getMsg("upgrade_remote", 0, 0, stderr);
	freeLines(lines, free);
	return;
}

/* strdup a line upto the next \n, strip trailing \r\n */
char *
strnonldup(char *s)
{
	char	*p, *ret;

	if (p = strchr(s, '\n')) {
		ret = malloc(p - s + 1);
		p = ret;
		while (*s != '\n') *p++ = *s++;
		*p-- = 0;
		while ((p > ret) && (*p == '\r')) *p-- = 0;
	} else {
		ret = strdup(s);
	}
	return (ret);
}

int
remote_lock_fail(char *buf, int verbose)
{
	if (streq(buf, LOCK_WR_BUSY) || streq(buf, LOCK_RD_BUSY)) {
		if (verbose) fprintf(stderr, "%s\n", buf);
		return (-2);
	}
	if (streq(buf, LOCK_PERM) || streq(buf, LOCK_UNKNOWN)) {
		if (verbose) fprintf(stderr, "%s\n", buf);
		return (-1);
	}
	return (0);
}

char *
strdup_tochar(const char *s, int c)
{
	char	*p;
	char	*ret;

	if (p = strchr(s, c)) {
		ret = malloc(p - s + 1);
		p = ret;
		while (*s != c) *p++ = *s++;
		*p = 0;
	} else {
		ret = strdup(s);
	}
	return (ret);
}

char	*
savefile(char *dir, char *prefix, char *pathname)
{
	int	i, fd;
	struct	tm *tm;
	time_t	now = time(0);
	char	path[MAXPATH];
	char	*p;

	/*
	 * Save the file in the passed in dir.
	 */
	if (!isdir(dir) && (mkdir(dir, 0777) == -1)) return (0);

	/* Force this group writable */
	(void)chmod(dir, 0775);
	if (access(dir, W_OK)) return (0);

	p = path;
	p += sprintf(p, "%s/", dir);
	if (prefix) p += sprintf(p, "%s", prefix);
	tm = localtimez(&now, 0);
	p += strftime(p, 20, "%Y-%m-%d", tm);

	for (i = 1; i < 500000; i++) {
		sprintf(p, ".%02d", i);
		fd = open(path, O_CREAT|O_EXCL|O_WRONLY, 0666);
		if (fd == -1) {
			if (errno == EEXIST) continue;	/* name taken */
			return (0);
		}
		if (close(fd)) return (0);
		if (pathname) {
			strcpy(pathname, path);
			return (pathname);
		} else {
			return (strdup(path));
		}
	}
	assert(0);
	/* not reached */
	return (0);
}

void
has_proj(char *who)
{
	if (proj_root(0)) return;
	fprintf(stderr, "%s: cannot find package root\n", who);
	exit(1);
}

int
spawn_cmd(int flag, char **av)
{
	int ret;

	ret = spawnvp(flag, av[0], av); 
	if (WIFSIGNALED(ret)) {
		unless (WTERMSIG(ret) == SIGPIPE) {
			fprintf(stderr,
			    "%s died from %d\n", av[0], WTERMSIG(ret));
		}
		return (127);
	} else unless (WIFEXITED(ret)) {
		fprintf(stderr, "bk: cannot spawn %s\n", av[0]);
		return (127);
	}
	return (WEXITSTATUS(ret));
}

char *
pager(void)
{
	char	*pagers[3] = {"more", "less", 0};
	char	*cmd = 0;
	char	*path, *oldpath;
	char	**words;
	int	i;
	char	buf[MAXPATH];
	static	char	*pg;

	if (pg) return (pg); /* already cached */

	/* restore user's path environment so we pick up their pager */
	path = strdup(getenv("PATH"));
	if (oldpath = getenv("BK_OLDPATH")) safe_putenv("PATH=%s", oldpath);

	if ((pg = getenv("BK_PAGER")) || (pg = getenv("PAGER"))) {
		/* $PAGER might be "less -E", i.e., multiple words */
		words = shellSplit(pg);
		pg = 0;
		if (words && words[1] && (cmd = which(words[1]))) {
			free(words[1]);
			words[1] = cmd;
			/* not perfect, Wayne would prefer shellJoin() */
			pg = joinLines(" ", words);
		}
		freeLines(words, free);
	}
	unless (pg) {
		for (i = 0; pagers[i]; i++) {
			if (cmd = which(pagers[i])) {
				sprintf(buf, "\"%s\"", cmd);
				free(cmd);
				pg = strdup(buf);
				break;
			}
		}
		unless (pg) pg = "cat";
	}
	if (oldpath) safe_putenv("PATH=%s", path);
	if (path) free(path);	/* hard to imagine we don't have one but.. */
	return (pg);
}

#define	MAXARGS	100
/*
 * Set up pager and connect it to our stdout
 */
pid_t
mkpager(void)
{
	int	pfd;
	pid_t	pid;
	char	*pager_av[MAXARGS];
	char	*cmd;
	char	*pg = pager();

	/* win32 treats "nul" as a tty, in this case we don't care */
	unless (isatty(1)) return (0);

	/* "cat" is a no-op pager used in bkd */
	if (streq("cat", basenm(pg))) return (0);

	fflush(stdout);
	signal(SIGPIPE, SIG_IGN);
	cmd = strdup(pg); /* line2av stomp */
	line2av(cmd, pager_av); /* some user uses "less -X -E" */
	pid = spawnvpio(&pfd, 0, 0, pager_av);
	dup2(pfd, 1);
	close(pfd);
	free(cmd);
	return (pid);
}

/*
 * Convert a command line to a av[] vector
 */
void
line2av(char *cmd, char **av)
{
	char	*p, *q, *s;
	int	i = 0;
#define	isQuote(q) (strchr("\"\'", *q) && q[-1] != '\\')
#define	isDelim(c) isspace(c)

	p = cmd;
	while (isspace(*p)) p++;
	while (*p) {
		av[i++] = p;
		if (i >= MAXARGS) {
			av[0] = 0;
			return;
		}
		s = q = p;
		while (*q && !isDelim(*q)) {
			if (*q == '\\') {
				q++;
				*s++ = *q++;
			} else if (isQuote(q)) {
				q++; /* strip begin quote */
				while (!isQuote(q)) {
					*s++ = *q++;
				}
				q++; /* strip end quote */
			} else {
				*s++ = *q++;
			}
		}
		if (*q == 0) {
			*s = 0;
			break;
		}
		*s = 0;
		p = ++q;
		while (isspace(*p)) p++;
	}
	av[i] = 0;
	return;
}

/*
 * Return true if there any symlinks or .. components of the path.
 * Nota bene: we do not check the last component, that is typically
 * anno/../bk-2.0.x/Makefile@+ stuff.
 */
int
unsafe_path(char *s)
{
	char	buf[MAXPATH];

	strcpy(buf, s);
	unless (s = strrchr(buf, '/')) return (0);
	for (;;) {
		/* no .. components */
		if (streq(s, "/..")) return (1);
		*s = 0;
		/* we've chopped the last component, it must be a dir */
		unless (isdir(buf)) return (1); /* this calls lstat() */
		unless (s = strrchr(buf, '/')) {
			/* might have started with ../someplace */
			return (streq(buf, ".."));
		}
	}
	/*NOTREACHED*/
}

int
cset_needRepack(void)
{
	off_t	heapsize;

	/*
	 * If the non-packed portion of the ChangeSet heap
	 * (2.ChangeSet) has grown to be at least a quarter of the
	 * size of the packed portion (1.ChangeSet) then we probably
	 * should do a full check and a repack even if the timestamps
	 * suggest a full check is not required yet.  The thinking is
	 * that the performance impact of not repacking is starting to
	 * matter more than the check time.
	 * We call bin_needHeapRepack() just to make sure the repack
	 * isn't blocked by one of the other conditions.
	 */
	if ((heapsize = size("SCCS/1.ChangeSet")) &&
	   (size("SCCS/2.ChangeSet") > heapsize/4)) {
		sccs	*cset = sccs_csetInit(0);
		int	repack;

		repack = bin_needHeapRepack(cset);
		sccs_free(cset);
		if (repack) return (1);
	}
	return (0);
}

/*
 * Return true if no checked marker or if it is too old.
 */
int
full_check(void)
{
	FILE	*f;
	char	*t;
	time_t	now = time(0);
	time_t	window;
	time_t	checkt = 0;	/* time of last full check */

	unless (cfg_bool(0, CFG_PARTIAL_CHECK)) return (1);
	if (window = cfg_int(0, CFG_CHECK_FREQUENCY)) {
		window *= DAY;
	} else {
		window = WEEK;
	}
	if (window > 2*WEEK) window = 2*WEEK;
	if (f = fopen(CHECKED, "r")) {
		if (t = fgetline(f)) checkt = strtoul(t, 0, 10);
		fclose(f);
	}
	if ((now - checkt) > window) return (1);
	if (cset_needRepack()) return (1);

	return (0);		/* no full check is required */
}

/*
 * If they hand us a partial list use that if we can.
 * Otherwise do a full check.
 *
 * quiet and verbose arguments together control how check
 * is to run WRT verbosity:
 *
 *  Q	V	RESULT
 *  0	0	progress bar
 *  0	1	very verbose
 *  1	0	silent
 *  1	1	silent
 *
 */
int
run_check(int quiet, int verbose, char **flist, char *opts, int *did_partial)
{
	int	i, j, ret;
	char	*cmd, *verbosity = "";
	char	buf[20];
	char	pwd[MAXPATH];
	FILE	*p;

again:
	assert(!opts || (strlen(opts) < sizeof(buf)));
	unless (opts && *opts) opts = "--";
	if (verbose) {
		strcpy(pwd, proj_cwd());
		fprintf(stderr, "Running consistency check in %s ...\n", pwd);
		unless (quiet) verbosity = "-vv";
	} else {
		unless (quiet) verbosity = "-v";
	}
	progress_pauseDelayed();
	if (!flist || full_check()) {
		ret = systemf("bk -?BK_NO_REPO_LOCK=YES -r check -ac %s %s",
		    verbosity, opts);
		if (did_partial) *did_partial = 0;
	} else {
		/* For possible progress bar, pass in # files. */
		cmd = aprintf("bk -?BK_NO_REPO_LOCK=YES "
		    "check -c -N%d %s '%s' -",
		    nLines(flist), verbosity, opts);
		p = popen(cmd, "w");
		free(cmd);
		EACH(flist) fprintf(p, "%s\n", flist[i]);
		ret = pclose(p);
		if (did_partial) *did_partial = 1;
	}
	progress_resumeDelayed();
	ret = WIFEXITED(ret) ? WEXITSTATUS(ret) : 1;
	if (strchr(opts, 'f') && (ret == 2)) {
		for (i = j = 0; opts[i]; i++) {
			unless (opts[i] == 'f') buf[j++] = opts[i];
		}
		buf[j] = 0;
		opts = buf;
		/*
		 * After a partial check needs an auto-fix, require a
		 * full check
		 */
		flist = 0;
		goto again;
	}
	return (ret);
}

/*
 * Make sure stdin and stdout are both open to to file handles.
 * If not tie them to /dev/null to match assumption in the rest of bk.
 * This tends to be a problem in cgi environments.
 */
void
reserveStdFds(void)
{
	int	fd;


#ifdef WIN32
	closeBadFds();
#endif
	/* reserve stdin */
	if ((fd = open(DEVNULL_RD, O_RDONLY, 0)) != 0) close(fd);
	if (fd <= 2) {
		/* reserve stdout & stderr */
		if ((fd = open(DEVNULL_WR, O_WRONLY, 0)) > 2) close(fd);
		if (fd == 1) {
			if ((fd = open(DEVNULL_WR, O_WRONLY, 0)) > 2) close(fd);
		}
	}
}

#ifndef	NOPROC

int
checking_rmdir(char *dir)
{
	int	rc;

	rc = (rmdir)(dir);
	rmdir_findprocs();
	return (rc);
}

#define	WAIT	60	/* /2 to get seconds */

void
rmdir_findprocs(void)
{
	char	**d;
	int	i, j, c;
	char	buf1[MAXLINE], buf2[MAXLINE];

	unless (getenv("BK_REGRESSION")) return;
	unless (bin) return;	/* platforminit failures can't run this */
	d = getdir("/proc");
	EACH (d) {
		unless (isdigit(d[i][0])) continue;
		sprintf(buf1, "/proc/%s/exe", d[i]);
		if ((c = readlink(buf1, buf2, sizeof(buf2))) < 0) continue;
		buf2[c] = 0;
		unless (strneq(buf2, bin, strlen(bin))) continue;
		sprintf(buf1, "/proc/%s/cwd", d[i]);
		if ((c = readlink(buf1, buf2, sizeof(buf2))) < 0) continue;
		buf2[c] = 0;
		/* next unless /.\(deleted\/)$/; */
		unless ((c > 9) && streq(buf2 + c - 9, "(deleted)")) continue;

		/* can't block on myself... */
		if (atoi(d[i]) == getpid()) {
			/* we are exiting the bkd so this is ok */
			if (getenv("_BK_IN_BKD")) continue;

			/*
			 * Can't dump core if we are in a deleted directory...
			 */
			chdir(getenv("BK_REGRESSION"));
			assert(0);
		}

		/* give them a chance to go away */
		for (j = 0; j < WAIT; ++j) {
			usleep(500000);
			if ((c = readlink(buf1, buf2, sizeof(buf2))) < 0) break;
			if ((j == 20) && getenv("_BK_DEBUG_BG")) {
				ttyprintf("Waiting for %s\n", buf1);
			}
		}
		/* we know they are gone if we broke out early */
		if (j < WAIT) continue;
		
		buf2[c] = 0;
		unless (streq(buf2 + c - 9, "(deleted)")) continue;
		buf2[c - 10] = 0;
		ttyprintf("proc %s is in dir %s which has been deleted\n",
		    d[i], buf2);
		sprintf(buf2, "/bin/ls -l /proc/%s > /dev/tty", d[i]);
		system(buf2);
		sprintf(buf2,
		    "/usr/bin/od -c /proc/%s/cmdline > /dev/tty", d[i]);
		system(buf2);
		assert(0);
	}
	freeLines(d, free);
}

#endif

int
shellSplit_test_main(int ac, char **av)
{
	int	i;
	char	**lines;

	unless (av[1] && !av[2]) return (1);
	lines = shellSplit(av[1]);
	EACH (lines) {
		printf("%d: (%s)\n", i, lines[i]);
	}
	return (0);
}

/*
 * Portable way to print a pointer.  Results are returned in a static buffer
 * and you may use up to N of these in one call to printf() or whatever
 * before you start stomping on yourself.
 */
#if GCC_VERSION > 40600
#pragma	GCC diagnostic push
#endif
#pragma	GCC diagnostic ignored "-Wpointer-to-int-cast"
char *
p2str(void *p)
{
#define N	10
#define PTRLEN	((sizeof(void *) / 4) + 3)
	static	char bufs[N][PTRLEN];
	static	int w = 0;
	char	*b;

	w = (w + 1) % N;
	b = bufs[w];
	if (sizeof(void *) == sizeof(int)) {
		int	n = sizeof(int) * 2;

		sprintf(b, "0x%.*x", n, p2int(p));
	} else if (sizeof(void *) == sizeof(u64)) {
		u64	a = (u64)p;
		u32	top, bot;

		top = (u32)(a >> 32);
		bot = a & 0xffffffff;
		sprintf(b, "0x%8x%8x", top, bot);
	} else {
		assert("Pointers are too big" == 0);
	}
	return (b);
#undef N
}
#if GCC_VERSION > 40600
#pragma	GCC diagnostic pop
#endif

char	*
psize(u64 size)
{
	static	char	p64buf[10][20];
	static	int	n;
	double	d = size;
	char	*tags = "BKMGTPE";
	int	t = 0;
	char	*s = p64buf[n++];

	if (n == 10) n = 0;
	while (d >= 999.0) t++, d /= 1024.0;
	if (t == 0) {
		sprintf(s, "%.0f", d); /* bytes, not character */
	} else if (d < 9.995) {
		sprintf(s, "%.2f%c", d, tags[t]); /* x.yyK */
	} else if (d < 99.95) {
		sprintf(s, "%.1f%c", d, tags[t]); /* xx.yK */
	} else {
		sprintf(s, "%.0f%c", d, tags[t]); /* xxxK */
	}
	return (s);
}

u64
scansize(char *size)
{
        float	f;
	u64	sz = 0;
	char    *p;

	unless (size && isdigit(*size)) return (sz);
	sscanf(size, "%f", &f);
	for (p = size; isdigit(*p) || (*p == '.'); p++);
	unless (*p) return ((u64)f);
	f *= 1024;	/* turn 12.2 into roughly 12200 */
	sz = (u64)f;
	switch (*p) {
	    case 'M': sz <<= 10; break;
	    case 'G': sz <<= 20; break;
	    case 'T': sz <<= 30; break;
	    case 'P': sz <<= 40; break;
	    case 'E': sz <<= 50; break;
	}
	return (sz);
}

/*
 * Allow override of the gone file so Scott McPeak at Coverity can try
 * out his subsetting idea (per group gone file settings).
 */
char *
goneFile(void)
{
	static	char *gone;

	unless (gone) {
		unless (gone = getenv("BK_GONE")) gone = "BitKeeper/etc/gone";
	}
	return (gone);
}

char *
sgoneFile(void)
{
	static	char *sgone;

	unless (sgone) sgone = name2sccs(goneFile());
	return (sgone);
}

/*
 * search for a file in the "usual" places:
 *	<repo>/BitKeeper/etc
 *	<product>/BitKeeper/etc
 *	$HOME/.bk
 *	/etc/BitKeeper/etc
 *	$BIN
 *
 * and return the full path to the first file found.
 */
char *
bk_searchFile(char *base)
{
	char	*root;
	char	buf[MAXPATH];

	if (root = proj_root(0)) {
		sprintf(buf, "%s/BitKeeper/etc/%s", root, base);
		if (exists(buf) ||
		    !get(buf, SILENT|GET_EXPAND)) {
			return (strdup(buf));
		}
	}
	if (proj_isComponent(0) && (root = proj_root(proj_product(0)))) {
		sprintf(buf, "%s/BitKeeper/etc/%s", root, base);
		if (exists(buf) ||
		    !get(buf, SILENT|GET_EXPAND)) {
			return (strdup(buf));
		}
	}
	concat_path(buf, getDotBk(), base);
	if (exists(buf)) return (strdup(buf));
	sprintf(buf, "%s/BitKeeper/etc/%s", globalroot(), base);
	if (exists(buf)) return (strdup(buf));
	concat_path(buf, bin, base);
	if (exists(buf)) return (strdup(buf));
	return (0);
}


/*
 * Parse a -@<url> argument in getopt() switch
 *
 * used with getopt(ac, av, "@|...", 0)
 *
 * Behavior:
 *	-@	# add my parents to list
 *	-@URL	# add URL to list
 *	-@@FILE	# add filename to list (1 url per line)
 *
 * Returns non-zero on error and prints message to stderr
 */
int
bk_urlArg(char ***urls, char *arg)
{
	char	**list = 0;

	if (arg && (arg[0] == '@')) {
		unless (list = file2Lines(0, arg+1)) {
			perror(arg+1);
			return (1);
		}
		*urls = catLines(*urls, list);
	} else if (arg && *arg) {
		*urls = addLine(*urls, strdup(arg));
	} else {
		unless (list = parent_allp()) {
			fprintf(stderr, "%s: -@ failed as "
			    "repository has no parent\n",
			    prog);
			return (1);
		}
		*urls = catLines(*urls, list);
	}
	if (list) freeLines(list, free);
	return (0);
}

/*
 * Add to getopt() lines in bk as:
 *
 * default: bk_badArg(c, av);
 */
void
bk_badArg(int c, char **av)
{
	if (getenv("_BK_IN_BKD")) fprintf(stderr, "ERROR-");
	if (c == GETOPT_ERR) {
		if (optopt) {
			fprintf(stderr, "bad option -%c\n", optopt);
		} else {
			/* unknown --long option */
			fprintf(stderr, "bad option %s\n", av[optind-1]);
		}
	} else if (c == GETOPT_NOARG) {
		if (optopt) {
			fprintf(stderr, "-%c missing argument\n", optopt);
		} else {
			/* unknown --long option */
			fprintf(stderr, "%s missing argument\n", av[optind-1]);
		}
	} else {
		/* we shouldn't see these */
		if (isprint(c)) {
			fprintf(stderr, "bad option -%c\n", c);
		} else {
			fprintf(stderr, "bad option %d\n", c);
		}
	}
	usage();
}

/*
 * Given the --standalone option moves to the root we are tracking
 * and returns true if operating on whole nested collection.
 */
int
bk_nested2root(int standalone)
{
	if ((!standalone && proj_isComponent(0) && proj_cd2product()) ||
	    proj_cd2root()) {
		fprintf(stderr, "%s: cannot find package root.\n", prog);
		exit(1);
	}
	return (!standalone && proj_isProduct(0));
}

/*
 * Use after getopt() to have a command line that will repoduce the
 * same set of options.
 */
char **
bk_saveArg(char **nav, char **av, int c)
{
	char	*buf;

	if (c > 32 && c < 127) {
		/* normal short option */
		buf = aprintf("-%c%s", c, optarg?optarg:"");
		nav = addLine(nav, buf);
	} else if (c > GETOPT_NOARG) {
		/* long option */
		if (optarg && (optarg == av[optind-1])) {
			/* --long with-arg */
			nav = addLine(nav, strdup(av[optind-2]));
		}
		nav = addLine(nav, strdup(av[optind-1]));
	} else {
		/* getopt error */
		assert((c == -1) || (c == GETOPT_ERR) || (c == GETOPT_NOARG));
	}
	return (nav);
}

/*
 * print usage message to stderr and exit.
 * Use 3 since some command use 1 as a special meaning. ex: diff
 *
 * This assumes functions do option parsing.  They should at least
 * have done this:
 *   while ((c = getopt(ac, av, "", 0)) != -1) bk_badArg(c, av);
 *
 */
void
usage(void)
{
	unless (prog) prog = "bk";
	if (systemf("bk gethelp -s '%s' 1>&2", prog)) {
		if (optind) {	/* only if getopt was used */
			sys("bk", prog, "--usage", SYS);
		} else {
			fprintf(stderr, "usage: %s\n", prog);
		}
	}
	exit(3);
}

/*
 * Override bk config in this process by setting BK_CONFIG in the
 * environment.
 */
void
bk_setConfig(char *key, char *val)
{
	char	*p = getenv("BK_CONFIG");

	if (p) {
		safe_putenv("BK_CONFIG=%s;%s:%s!", p, key, val);
	} else {
		safe_putenv("BK_CONFIG=%s:%s!", key, val);
	}
}

/*
 * Return the number of cpus
 */
int
cpus(void)
{
#ifdef WIN32
	SYSTEM_INFO info;

	GetSystemInfo(&info);
	return (info.dwNumberOfProcessors);
#elif defined(_SC_NPROCESSORS_CONF)
	int	n = sysconf(_SC_NPROCESSORS_CONF);

	return (n);
#else
	return (1);
#endif
}

int
cpus_main(int ac, char **av)
{
	printf("%d\n", cpus());
	return (0);
}

/*
 * Return reasonable values for parallel sfio/checkouts/etc.
 *
 * 'flags' is used to pass modifiers based on contect
 *         Current:
 *             WRITER this process is mostly writing new data to disk
 */
int
parallel(char *path, int flags)
{
	int	p;
	char	*t;
	int	val = 1;

#ifdef	WIN32
	return (val);
#endif
	if ((t = cfg_str(0, CFG_PARALLEL)) && isdigit(*t)) {
		val = atoi(t);
		return (min(val, PARALLEL_MAX));
	}
	p = fstype(path);
	if (p == FS_NFS) {
		/*
		 * A good NFS server likes parallel accesses to
		 * reducely latency. It has less to do with the client
		 * so we hardcode a value.
		 * XXX shouldn't writes be a tad higher than reads?
		 */
		val = 8;
	} else if (p == FS_SSD) {
		/*
		 * SSDs are happy to handle lots of parallel requests
		 * so we mostly just want to keep the CPUs busy.
		 */
		val = cpus();
	} else {
		/*
		 * assert((p == FS_DISK) || (p == FS_UNKOWN));
		 * When writing to a local spinning disk it is good to
		 * have a couple outstanding writes to allow the
		 * kernel to reorder them.  For reads we go in order
		 * to prevent thrashing.
		 */
		val = (flags & WRITER) ? 3 : 1;
	}
	return (val);
}

int
parallel_main(int ac, char **av)
{
	u32	flags = READER;
	int	c;

	while ((c = getopt(ac, av, "w", 0)) != -1) {
		switch (c) {
		    case 'w': flags = WRITER; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();

	printf("%d\n", parallel(".", flags));
	return (0);
}
/*
 * Given a number encoded as a bitfield and a list of names for each
 * bit position return a malloc'ed strict with comma separated names
 * for each bit set.
 *
 * ex:
 *    str = formatBits(37,
 *	 32, "foo", 16, "bar", 8, "baz", 4, "bing", 2, "bang", 1, "bap", 0);
 *   returns "foo,bing,bap"
 */
char *
formatBits(u32 bits, ...)
{
	u32	mask;
	char	*name;
	char	**out = 0;
	char	*extra = 0;
	char	*ret;
	va_list	ap;

	va_start(ap, bits);
	while (mask = va_arg(ap, u32)) {
		unless (bits) break;
		name = va_arg(ap, char *);
		if (bits & mask) {
			out = addLine(out, name);
			bits &= ~mask;
		}
	}
	va_end(ap);

	if (bits) out = addLine(out, (extra = aprintf("0x%x", bits)));
	ret = joinLines(",", out);
	freeLines(out, 0);
	if (extra) free(extra);
	return (ret);
}

int
bk_gzipLevel(void)
{
	return (cfg_int(0, CFG_BKD_GZIP));
}

void
notice(char *key, char *arg, char *type)
{
	char	*gm;

	assert(key);
	if (arg) {
		gm = aprintf("bk getmsg %s '%s'", key, arg);
	} else {
		gm = aprintf("bk getmsg %s", key);
	}
	unless (type) type = "-x";
	sys("bk", "prompt", type, "-t", key, "-ocp", gm, SYS);

	/* So we can have both GUI and command line results for regressions */
	if (getenv("BK_GUI") && getenv("_BK_PROMPT")) {
		sys("bk", "prompt", "-gocp", gm, SYS);
	}
}
