#include "bkd.h"
#include "logging.h"

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


/*
 * Write the data to either the gzip channel or to 1.
 */
int
out(char *buf)
{
	return (outfd(1, buf));
}

int
outc(char c)
{
	return (writen(1, &c, 1));
}

int
outfd(int to, char *buf)
{
	return (writen(to, buf, strlen(buf)));
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
	char	*retval;

	va_start(ap, fmt);
	if (vasprintf(&retval, fmt, ap) < 0) retval = 0;
	va_end(ap);
	if (retval) {
		if (getenv("_BK_IN_BKD")) {
			out("ERROR-");
			out(retval);
		} else {
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
readn(int from, char *buf, int size)
{
	int	done;
	int	n;

	for (done = 0; done < size; ) {
		n = read(from, buf + done, size - done);
		if (n <= 0) {
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
 */
int
getline(int in, char *buf, int size)
{
	int	i = 0;
	int	c;
	static	int echo = -1;

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
 *     disconnect(r, 1);
 *     wait_eof(r, 0);
 *     disconnect(r, 2);
 *     remote_free(r);
 */
void
wait_eof(remote *r, int verbose)
{
	int	i;
	int	bytes = 0;
	char	buf[MAXLINE];

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
	unless (av[1]) return (1);
	usleep(atoi(av[1]));
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
	char	*cmd;
	FILE	*in;
	char	msgtmp[MAXPATH];
	char	buf[1024];
	char	**lines = 0;

	while ((c = getopt(ac, av, "cegGiowxf:n:p:t:Ty:")) != -1) {
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
		}
	}
	if (((file || prog) && av[optind]) ||
	    (!(file || prog) && !av[optind]) ||
	    (av[optind] && av[optind+1]) || (file && prog)) {
		system("bk help -s prompt");
err:		if (file == msgtmp) unlink(msgtmp);
		if (lines) freeLines(lines, free);
		exit(2);
	}
	if (prog) {
		assert(!file);
		bktmp(msgtmp, "prompt");
		file = msgtmp;
		cmd = aprintf("%s > '%s'", prog, file);

		/* For caching of the real pager */
		(void)pager();
		putenv("PAGER=cat");
		if (system(cmd)) goto err;
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
	if (getenv("BK_EVENT") && ((i = open(DEV_TTY, O_RDWR, 0)) >= 0)) {
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

int
bkd_connect(remote *r)
{
	assert((r->rfd == -1) && (r->wfd == -1));
	r->pid = bkd(r);
	if (r->trace) {
		fprintf(stderr,
		    "bkd_connect: r->rfd = %d, r->wfd = %d\n", r->rfd, r->wfd);
	}
	if (r->wfd >= 0) return (0);
	if (r->badhost) {
		fprintf(stderr, "Cannot resolve host '%s'.\n", r->host);
	} else if (r->badconnect) {
		fprintf(stderr, "Unable to connect to host '%s'.\n", r->host);
	} else {
		char	*rp = remote_unparse(r);

		perror(rp);
		free(rp);
	}
	return (-1);
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
			remote_perror(r, "write");
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
	char	*q, *hdr;
	int	no_extra = (extra == 0);

	assert(r->wfd >= 0);	/* we should be connected */
	m = mopen(file, "r");
	assert(m);
	assert(strneq(m->mmap, "putenv ", 7));
	len = m->size;

	if (r->type == ADDR_HTTP) extra += 5;	/* "quit\n" */
	q = secure_hashstr(m->mmap, len, makestring(KEY_BK_AUTH_HMAC));
	hdr = aprintf("putenv BK_AUTH_HMAC=%d|%d|%s\n", len, extra, q);
	free(q);
	rc = send_msg(r, hdr, strlen(hdr), len+extra);
	free(hdr);
	unless (rc) {
		if (writen(r->wfd, m->mmap, len) != len) {
			remote_perror(r, "write");
			rc = -1;
		}
		if (r->trace) {
			fprintf(stderr, "Sending...\n");
			fwrite(m->mmap, 1, len, stderr);
		}
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
			remote_perror(r, "sf_extra");
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
				if (r->errs) {
					lease_mkerror(r->errs, "http-err", buf);
				}
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
disconnect(remote *r, int how)
{
	assert((how >= 0) && (how <= 2));
	assert(!r->need_exdone);

	switch (how) {
	    case 0:	if (r->rfd == -1) break;
			if (r->isSocket) {
				assert(!r->rf);
				shutdown(r->rfd, 0);
			} else if (r->rf) {
				fclose(r->rf);
				r->rf = 0;
			} else {
				close(r->rfd);
			}
			r->rfd = -1;
			break;
	    case 1: 	if (r->wfd == -1) break;
			if (r->isSocket) {
				shutdown(r->wfd, 1);
			} else {
				close(r->wfd);
			}
			r->wfd = -1;
			break;
	    case 2:	if (r->rf) {
				fclose(r->rf);
				r->rf = 0;
			} else if (r->rfd >= 0) {
				close(r->rfd);
			}
			if ((r->wfd >= 0) && (r->wfd != r->rfd)) close(r->wfd);
			r->rfd = r->wfd = -1;
			break;
	}
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
	if (streq(p, "ERROR-bogus license key")) return (100);
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
			if (p && *p) fprintf(stderr, "%s: %s\n", url, p);
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

			getcwd(pwd, MAXPATH);
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
	char	*lic;
	project	*p = proj_init(".");
	char	buf[MAXLINE];

	assert(r->wfd >= 0);	/* we should be connected */
	/*
	 * Send any vars the user requested first so that they can't
	 * overwrite any of the standard variables.
	 */
	EACH(envVar) fprintf(f, "putenv %s\n", envVar[i]);

	if (r->host)
		fprintf(f, "putenv BK_VHOST=%s\n", r->host);
	fprintf(f, "putenv BK_REMOTE_PROTOCOL=%s\n", BKD_VERSION);

	fprintf(f, "putenv BK_VERSION=%s\n", bk_vers);
	fprintf(f, "putenv BK_UTC=%s\n", bk_utc);
	fprintf(f, "putenv BK_TIME_T=%s\n", bk_time);
	user = sccs_getuser();
	fprintf(f, "putenv BK_USER=%s\n", user);
	fprintf(f, "putenv _BK_USER=%s\n", user);	/* XXX remove in 3.0 */
	host = sccs_gethost();
	fprintf(f, "putenv _BK_HOST=%s\n", host);
	if (lic = licenses_accepted()) {
		fprintf(f, "putenv BK_ACCEPTED=%s\n", lic);
		free(lic);
	}
	fprintf(f, "putenv BK_REALUSER=%s\n", sccs_realuser());
	fprintf(f, "putenv BK_REALHOST=%s\n", sccs_realhost());
	fprintf(f, "putenv BK_PLATFORM=%s\n", platform());

	if (t = getenv("BKD_NESTED_LOCK")) {
		fprintf(f, "putenv 'BK_NESTED_LOCK=%s'\n", t);
	}

	unless (flags & SENDENV_NOREPO) {
		/*
		 * This network connection is not necessarily run from
		 * a repository, so don't send information about the
		 * current repository.  Clone is the primary example
		 * of this.
		 */
		assert(p);	/* We must be in a repo here */
		fprintf(f, "putenv BK_LEVEL=%d\n", getlevel());
		proj = proj_root(p);
		if (strchr(proj, ' ')) {
			fprintf(f, "putenv 'BK_ROOT=%s'\n", proj);
		} else {
			fprintf(f, "putenv BK_ROOT=%s\n", proj);
		}
		fprintf(f, "putenv BK_ROOTKEY=%s\n", proj_rootkey(p));
		if (repo = proj_repoID(0)) {
			if (strchr(repo, ' ')) {
				fprintf(f, "putenv 'BK_REPO_ID=%s'\n", repo);
			} else {
				fprintf(f, "putenv BK_REPO_ID=%s\n", repo);
			}
			if (bp_hasBAM()) fprintf(f, "putenv BK_BAM=YES\n");
			if (bp = bp_serverURL(buf)) {
				if (strchr(bp, ' ')) {
					fprintf(f,
					    "putenv 'BK_BAM_SERVER_URL=%s'\n",
					    bp);
				} else {
					fprintf(f,
					    "putenv BK_BAM_SERVER_URL=%s\n",
					    bp);
				}
			}
			unless (bp = bp_serverID(buf, 0)) {
				bp = proj_repoID(proj_product(0));
			}
			if (strchr(bp, ' ')) {
				fprintf(f,
				    "putenv 'BK_BAM_SERVER_ID=%s'\n", bp);
			} else {
				fprintf(f, "putenv BK_BAM_SERVER_ID=%s\n", bp);
			}
		}
	}
	unless (flags & SENDENV_NOLICENSE) {
		if (flags & SENDENV_NOREPO) {
			/*
			 * Send information on the current license, but
			 * don't fail if we can't find a license
			 */
			if (lic = lease_bkl(0, 0)) {
				fprintf(f, "putenv BK_LICENSE=%s\n", lic);
				free(lic);
			}
		} else {
			/* Require a license or die */
			fprintf(f, "putenv BK_LICENSE=%s\n", proj_bkl(0));
		}
	}
	/*
	 * Send comma separated list of client features so the bkd
	 * knows which outputs are supported.
	 *   lkey:1	use leasekey #1 to sign lease requests
	 *   BAM
	 *   pSFIO	send whole sfiles in SFIO attached to patches
	 *   mSFIO	will accept modes with sfio.
	 */
	fprintf(f, "putenv BK_FEATURES=lkey:1,BAMv2,SAMv1,mSFIO");
	unless (getenv("_BK_NO_PATCHSFIO")) fputs(",pSFIO", f);
	fputc('\n', f);
	unless (r->seed) bkd_seed(0, 0, &r->seed);
	fprintf(f, "putenv BK_SEED=%s\n", r->seed);
	if (p) proj_free(p);
}

int
getServerInfoBlock(remote *r)
{
	int	ret = 1; /* protocol error, never saw @END@ */
	int	gotseed = 0;
	int	i;
	char	*newseed;
	char	buf[4096];

	/* clear previous values for stuff that is set conditionally */
	putenv("BKD_BAM=");
	putenv("BKD_BAM_SERVER_URL=");
	putenv("BKD_PRODUCT_KEY=");
	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (streq(buf, "@END@")) {
			ret = 0; /* ok */
			break;
		}
		if (r->trace) fprintf(stderr, "Server info:%s\n", buf);

		if (strneq(buf, "ERROR-", 6)) {
			lease_printerr(buf+6);
			return (1);
		}
		if (strneq(buf, "PROTOCOL", 8)) {
			safe_putenv("BK_REMOTE_%s", buf);
		} else {
			safe_putenv("BKD_%s", buf);
			if (strneq(buf, "REPO_ID=", 8)) {
				cmdlog_addnote("rmts", buf+8);
			}
			if (strneq(buf, "SEED=", 5)) gotseed = 1;
		}
	}
	assert(r->seed);	/* I should have always sent a seed */
	if (gotseed) {
		i = bkd_seed(r->seed, getenv("BKD_SEED"), &newseed);
	} else {
		i = 0;
		newseed = 0;
	}
	safe_putenv("BKD_SEED_OK=%d", i);
	if (r->seed) free(r->seed);
	r->seed = newseed;
	return (ret);
}

/*
 *  Send server env from bkd to client
 */
int
sendServerInfoBlock(int is_rclone)
{
	char	*repoid, *rootkey, *p, *errs = 0;
	char	buf[MAXPATH];
	char	bp[MAXLINE];

	out("@SERVER INFO@\n");
	unless (is_rclone) {
		if (p = lease_bkl(0, &errs)) {
			free(p);
		} else {
			assert(errs);
			out("ERROR-");
			out(errs);
			out("\n");
			return (1);
		}
	}
        sprintf(buf, "PROTOCOL=%s\n", BKD_VERSION);	/* protocol version */
	out(buf);

        sprintf(buf, "VERSION=%s\n", bk_vers);		/* binary version   */
	out(buf);
        sprintf(buf, "UTC=%s\n", bk_utc);
	out(buf);
        sprintf(buf, "TIME_T=%s\n", bk_time);
	out(buf);

	/*
	 * When we are doing a rclone, there is no tree on the bkd side yet
	 * Do not try to get the level of the server tree.
	 */
	unless (is_rclone) {
        	sprintf(buf, "LEVEL=%d\n", getlevel());
		out(buf);
		out("LICTYPE=");
		out(eula_name());
		out("\n");
		if (bp_hasBAM()) out("BAM=YES\n");
		if (p = bp_serverURL(bp)) {
			sprintf(buf, "BAM_SERVER_URL=%s\n", p);
			out(buf);
		}
	}
	out("ROOT=");
	getcwd(buf, sizeof(buf));
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
	/*
	 * Return a comma seperated list of features supported by the bkd.
	 *   pull-r	pull -r is parsed correctly
	 *   BAMv2	support BAM operations (4.1.1 and later)
	 *   pSFIO	send whole sfiles in SFIO attached to patches
	 */
	out("\nFEATURES=pull-r,BAMv2,SAMv1");
	unless (getenv("_BK_NO_PATCHSFIO")) out(",pSFIO");

	if (repoid = proj_repoID(0)) {
		sprintf(buf, "\nREPO_ID=%s", repoid);
		out(buf);
		unless (p = bp_serverID(bp, 0)) p = repoid;
		sprintf(buf, "\nBAM_SERVER_ID=%s", p);
		out(buf);
	}
	if (rootkey = proj_rootkey(0)) {
		sprintf(buf, "\nROOTKEY=%s", rootkey);
		out(buf);
	}
	/* only send back a seed if we received one */
	if (p = getenv("BKD_SEED")) {
		out("\nSEED=");
		out(p);
	}
	if (p = getenv("BKD_NESTED_LOCK")) {
		out("\nNESTED_LOCK=");
		out(p);
	}
	if (proj_isComponent(0)) {
		sprintf(buf, "\nPRODUCT_KEY=%s", proj_rootkey(proj_product(0)));
		out(buf);
	}
	out("\n@END@\n");
	return (0);
}

private int
has_feature(char *bk, char *f)
{
	int	len;
	char	var[20];

	sprintf(var, "%s_FEATURES", bk);
	bk = getenv(var);
	len = strlen(f);
	while (bk) {
		if (strneq(bk, f, len)) {
			if (bk[len] == ',' || !bk[len]) return (1);
		}
		if (bk = strchr(bk, ',')) ++bk;
	}
	return (0);
}

int
bk_hasFeature(char *f)
{
	return (has_feature("BK", f));
}

int
bkd_hasFeature(char *f)
{
	return (has_feature("BKD", f));
}

/*
 * Generate a http response header from a bkd back to the client.
 */
void
http_hdr(void)
{
	static	int done = 0;

	if (done) return; /* do not send it twice */
	out("HTTP/1.0 200 OK\r\n");
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

int
isLocalHost(char *h)
{
	unless (h) return (0);
	return (streq("localhost", h) ||
	    streq("localhost.localdomain", h) || streq("127.0.0.1", h));
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
		unless (pg) pg = "bk more";
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

/*
 * Return true if no checked marker or if it is too old.
 */
int
full_check(void)
{
	struct	stat sb;
	time_t	now = time(0);
	time_t	window;

	unless (proj_configbool(0, "partial_check")) return (1);
	if (window = proj_configsize(0, "check_frequency")) {
		window *= DAY;
	} else {
		window = DAY;
	}
	if (window > 2*WEEK) window = 2*WEEK;
	return (lstat(CHECKED, &sb) || ((now - sb.st_mtime) > window));
}

/*
 * If they hand us a partial list use that if we can.
 * Otherwise do a full check.
 */
int
run_check(int quiet, char *flist, char *opts, int *did_partial)
{
	int	i, j, ret;
	char	buf[20];
	char	pwd[MAXPATH];

again:
	assert(!opts || (strlen(opts) < sizeof(buf)));
	unless (opts && *opts) opts = "--";
	unless (quiet) {
		getcwd(pwd, sizeof(pwd));
		fprintf(stderr, "Running consistency check in %s ...\n", pwd);
	}
	if (!flist || full_check()) {
		ret = sys("bk", "-r", "check", "-ac", opts, SYS);
		if (did_partial) *did_partial = 0;
	} else {
		ret = sysio(flist, 0, 0, "bk", "check", "-c", opts, "-", SYS);
		if (did_partial) *did_partial = 1;
	}
	ret = WIFEXITED(ret) ? WEXITSTATUS(ret) : 1;
	if (strchr(opts, 'f') && (ret == 2)) {
		for (i = j = 0; opts[i]; i++) {
			unless (opts[i] == 'f') buf[j++] = opts[i];
		}
		buf[j] = 0;
		opts = buf;
		goto again;
	}
	return (ret);
}


/*
 * Print progress bar.
 *
 * Use 65 columns for the progress bar.
 * %3u% |================================ \r
 */
void
progressbar(u64 n, u64 max, char *msg)
{
	static	int	last = 0;
	static	struct	timeval start;
	static	float	lastup = 0.0;
	float	elapsed;
	int	percent = max ? (n * 100) / max : 100;
	int	i, want;
	int	barlen = 65;
	char	*p;
	struct	timeval tv;

	if (percent > 100) percent = 100;
	if (percent < last) {		/* reset */
		last = 0;
		start.tv_sec = 0;
		lastup = 0.0;
	}
	unless ((percent > last) || (n == 0) || msg) return;
	gettimeofday(&tv, 0);
	unless (start.tv_sec) start = tv;
	elapsed =
	    (tv.tv_sec - start.tv_sec) + (tv.tv_usec - start.tv_usec) / 1.0e6;
	/* This wacky expression is to try and smooth the drawing */
	if (n && !msg &&
	    ((elapsed - lastup) < 0.25) && ((percent - last) <= 2)) {
		return;
	}
	last = percent;
	lastup = elapsed;

	fprintf(stderr, "%3u%% ", percent);
	if ((elapsed > 10.0) && (n < max)) {
		int	remain = elapsed * (((float)max/n) - 1.0);

		p = aprintf("%dm%02ds ", remain/60, remain%60);
		barlen -= strlen(p);
		fputs(p, stderr);
		free(p);
	}
	fputc('|', stderr);
	want = (percent * barlen) / 100;
	for (i = 1; i <= want; ++i) fputc('=', stderr);
	if (i <= barlen) fprintf(stderr, "%*s", barlen - i + 1, "");
	if (msg) {
		fprintf(stderr, "| %s\n", msg);
	} else {
		fprintf(stderr, "|\r");
	}
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
		unless (streq(buf2 + c - 9, "(deleted)")) continue;

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
			if (j == 20) ttyprintf("Waiting for %s\n", buf1);
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

u32
crc(char *str)
{
	return (adler32(0, str, strlen(str)));
}

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
		    !get(buf, SILENT|GET_EXPAND, "-")) {
			return (strdup(buf));
		}
	}
	if (proj_isComponent(0) && (root = proj_root(proj_product(0)))) {
		sprintf(buf, "%s/BitKeeper/etc/%s", root, base);
		if (exists(buf) ||
		    !get(buf, SILENT|GET_EXPAND, "-")) {
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
