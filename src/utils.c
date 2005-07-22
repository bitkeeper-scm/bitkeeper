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

#ifndef WIN32
int
cat(char *file)
{
	MMAP	*m = mopen(file, "r");

	unless (m) return (-1);
	unless (write(1, m->mmap, m->size) == m->size) {
		mclose(m);
		return (-1);
	}
	mclose(m);
	return (0);
}
#else
/*
 * We need a win32 version beacuse win32 write interface cannot
 * handle large buffer, do _not_ change this code unless you tested it 
 * on win32. I coded ths once before and someone removed it. - awc
 *
 * XXX TODO move this to the port directory.
 */
int
cat(char *file)
{
	MMAP	*m = mopen(file, "r");
	char	*p;
	int	n;

	unless (m) return (-1);

	p = m->mmap;
	n = m->size;
	while (n) {
		if (n >=  MAXLINE) {
			write(1, p, MAXLINE);
			n -= MAXLINE;
			p += MAXLINE;
		} else {
			write(1, p, n);
			n = 0;
			p = 0;
		}
	};
	mclose(m);
	return (0);
}
#endif

char *
loadfile(char *file, int *size)
{
	FILE	*f;
	struct	stat	statbuf;
	char	*ret;
	int	len;

	f = fopen(file, "r");
	unless (f) return (0);

	if (fstat(fileno(f), &statbuf)) {
 err:		fclose(f);
		return (0);
	}
	len = statbuf.st_size;
	ret = malloc(len+1);
	unless (ret) goto err;
	fread(ret, 1, len, f);
	fclose(f);
	ret[len] = 0;

	if (size) *size = len;
	return (ret);
}

int
touch(char *file, int mode)
{
	int	fh = open(file, O_CREAT|O_EXCL|O_WRONLY, mode);

	if (fh < 0) return (fh);
	return (close(fh));
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

int
in(char *buf, int n)
{
	return (readn(0, buf, n));
}

int
writen(int to, void *buf, int size)
{
	int	done;
	int	n;

	for (done = 0; done < size; ) {
		n = write(to, buf + done, size - done);
		if ((n == -1) && ((errno == EINTR) || (errno == EAGAIN))) {
			usleep(10000);
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

/*
 * Currently very lame because it does 1 byte at a time I/O
 */
int
getline(int in, char *buf, int size)
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
		switch (ret = read(in, &c, 1)) {
		    case 1:
			if (echo == 2) fprintf(stderr, "[%c]\n", c);
			if (((buf[i] = c) == '\n') || (c == '\r')) {
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
				perror("getline");
				fprintf(stderr, "[%s]=%d\n", buf, ret);
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

int
write_blk(remote *r, char *buf, int len)
{
	return (writen(r->wfd, buf, len));
}

/*
 * We have completed our transactions with a remote bkd and closed our
 * write filehandle, now we want to wait until the remote bkd has
 * finished so we can be sure all locks are freed.  We might have to
 * read garbage data from the pipe in order to get to the EOF.  Since
 * we might be attached to a random port that will never close we
 * shouldn't wait forever.  For now, we wait until 2 Megs of junk has
 * been read.
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
				perror("getline2");
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

	flush_fd0(); /* for Win/98 and Win/ME */
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

	flush_fd0(); /* for Win/98 and Win/Me */
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

	while ((c = getopt(ac, av, "cegGiowxf:n:p:t:y:")) != -1) {
		switch (c) {
		    case 'c': ask = 0; break;
		    case 'e': type = "-E"; break;
		    case 'g': nogui = 1; break;
		    case 'G': gui = 1; break;
		    case 'i': type = "-I"; break;
		    case 'w': type = "-W"; break;
		    case 'x': /* ignored, see notice() for why */ break;
		    case 'f': file = optarg; break;
		    case 'n': no = optarg; break;
		    case 'o': no = 0; break;
		    case 'p': prog = optarg; break;
		    case 't': title = optarg; break;	/* Only for GUI */
		    case 'y': yes = optarg; break;
		}
	}
	if (((file || prog) && av[optind]) ||
	    (!(file || prog) && !av[optind]) ||
	    (av[optind] && av[optind+1]) || (file && prog)) {
err:		system("bk help -s prompt");
		if (file == msgtmp) unlink(msgtmp);
		if (lines) freeLines(lines, free);
		exit(1);
	}
	if (prog) {
		assert(!file);
		bktmp(msgtmp, "prompt");
		file = msgtmp;
		cmd = aprintf("%s > %s", prog, file);

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
 * Return an MDBM with all the keys from the ChangeSet file
 * after subtracting the keys from the "not" database.
 * The result are stored as db{key} = rev.
 */
MDBM	*
csetDiff(MDBM *not,  int wantTag)
{
	char	buf[MAXKEY], s_cset[MAXPATH] = CHANGESET;
	sccs	*s;
	delta	*d;
	MDBM	*db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	int	n = 0;
		
	unless (s = sccs_init(s_cset, INIT_NOCKSUM)) {
		mdbm_close(db);
		return (0);
	}
	for (d = s->table; d; d = d->next) {
		if (!wantTag && (d->type == 'R')) continue;
		sccs_sdelta(s, d, buf);
		unless (not && mdbm_fetch_str(not, buf)) {
			mdbm_store_str(db, buf, d->rev, 0);
			n++;
		}
	}
	sccs_free(s);
	if (n) return (db);
	mdbm_close(db);
	return (0);
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
	sprintf(buf, "%.*s/" BKROOT, q - spath, spath);
	return (isdir(buf));					/* case ' e' */
}


int
bkd_connect(remote *r, int compress, int verbose)
{
	assert((r->rfd == -1) && (r->wfd == -1));
	r->pid = bkd(compress, r);
	if (r->trace) {
		fprintf(stderr,
		    "bkd_connect: r->rfd = %d, r->wfd = %d\n", r->rfd, r->wfd);
	}
	if (r->wfd < 0) {
		if (verbose) {
			if (r->badhost) {
				fprintf(stderr,
					"Cannot resolve host %s\n", r->host);
			} else {
				char	*rp = remote_unparse(r);
				perror(rp);
				free(rp);
			}
		}
		return (-1);
	}
	return (0);
}



private int
send_msg(remote *r, char *msg, int mlen, int extra)
{
	char	*cgi = WEB_BKD_CGI;

	assert(r->wfd != -1);
	if (r->type == ADDR_HTTP) {
		if (r->path && strneq(r->path, "cgi-bin/", 8)) {
			cgi = r->path + 8;
		}
		if (http_send(r, msg, mlen, extra, "send", cgi)) {
			fprintf(stderr, "http_send failed\n");
			return (-1);
		}
	} else {
		if (write_blk(r, msg, mlen) != mlen) {
			perror("send_msg");
			fprintf(stderr, "r->wfd = %d errno = %d\n",
			    r->wfd, errno);
			return (-1);
		}
	}
	return (0);
}


int
send_file(remote *r, char *file, int extra)
{
	MMAP	*m;
	int	rc, len;
	char	*q, *hdr;

	m = mopen(file, "r");
	assert(m);
	assert(strneq(m->mmap, "putenv ", 7));
	len = m->size;

	q = secure_hashstr(m->mmap, len, "11ef64c95df9b6227c5654b8894c8f00");
	hdr = aprintf("putenv BK_AUTH_HMAC=%d|%d|%s\n", len, extra, q);
	free(q);
	rc = send_msg(r, hdr, strlen(hdr), len+extra);
	free(hdr);
	unless (rc) if (write_blk(r, m->mmap, len) != len) rc = -1;
	mclose(m);
	return (rc);
}

int
skip_http_hdr(remote *r)
{
	char	buf[1024];

	r->contentlen = -1;

	while (getline2(r, buf, sizeof(buf)) >= 0) {
		sscanf(buf, "Content-Length: %d", &r->contentlen);
		if (buf[0] == 0) return (0); /*ok */
	}
	return (-1); /* failed */
}

void
disconnect(remote *r, int how)
{
	assert((how >= 0) && (how <= 2));

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
	if (r->pid && (r->rfd == -1) && (r->wfd == -1)) {
		/*
		 * We spawned a bkd in the background to handle this
		 * connection. When it get here it SHOULD be finished
		 * already, but just in case we close the connections
		 * and wait for that process to exit.
		 */
		if (getenv("BK_SHOWPROC")) {
			fprintf(stderr,
			    "disconnect(): pid %u waiting for %u...\n",
			    getpid(), r->pid);
		}
		waitpid(r->pid, 0, 0);
		r->pid = 0;
	}
}


int
get_ok(remote *r, char *read_ahead, int verbose)
{
	int 	i, ret;
	char	buf[512], *p;

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
	if (verbose) {
		i = 0;
		if (p && *p) fprintf(stderr, "remote: %s\n", p);
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (buf[0]) fprintf(stderr, "remote: %s\n", buf);
			if (streq(buf, "@END@")) break;
			/*
			 * 20 lines of error message should be enough
			 */
			if (i++ > 20) break;
		}
	}
	return (1); /* failed */
}

/*
 * Return the repo_id if there is one.
 * Assumes we are at the root of the repository.
 * Caller frees.
 */
char *
repo_id(void)
{
	char	*root = proj_root(0);
	char	*file;
	char	*repoid;

	unless (root) return (0);
	file = aprintf("%s/" REPO_ID, root);
	repoid = loadfile(file, 0);
	free(file);
	unless (repoid) return (0);
	chomp(repoid);
	return (repoid);
}

char *
rootkey(char *buf)
{
	char	s_cset[] = CHANGESET;
	sccs	*s;

	s = sccs_init(s_cset, INIT_NOCKSUM);
	assert(s);
	sccs_sdelta(s, sccs_ino(s), buf);
	sccs_free(s);
	return (buf);
}

void
add_cd_command(FILE *f, remote *r)
{
	int	needQuote = 0;
	char	key[MAXKEY];

	/*
	 * XXX TODO need to handle embeded quote in pathname
	 */
	if (strchr(r->path, ' ')) needQuote = 1;
	if (streq(r->path, "///LOG_ROOT///")) {
		rootkey(key);
		if (strchr(key, ' ')) needQuote = 1;
		if (needQuote) {
			fprintf(f, "cd \"%s%s\"\n", r->path, key);
		} else {
			fprintf(f, "cd %s%s\n", r->path, key);
		}
	} else {
		if (needQuote) {
			fprintf(f, "cd \"%s\"\n", r->path);
		} else {
			fprintf(f, "cd %s\n", r->path);
		}
	}
}

void
put_trigger_env(char *prefix, char *v, char *value)
{
	unless (value) value = "";
	safe_putenv("%s_%s=%s", prefix, v, value);
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
 * Send env varibale to remote bkd.
 *
 * NOTE: When editing this function be sure to make the same changes in
 *       clone.c:out_trigger()
 */
void
sendEnv(FILE *f, char **envVar, remote *r, u32 flags)
{
	int	i;
	char	*user, *host, *repo;
	char	*lic;
	project	*p = proj_init(".");

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
	if (repo = repo_id()) {
		fprintf(f, "putenv BK_REPO_ID=%s\n", repo);
		free(repo);
	}
	if (lic = licenses_accepted()) {
		fprintf(f, "putenv BK_ACCEPTED=%s\n", lic);
		free(lic);
	}
	fprintf(f, "putenv BK_REALUSER=%s\n", sccs_realuser());
	fprintf(f, "putenv BK_REALHOST=%s\n", sccs_realhost());
	fprintf(f, "putenv BK_PLATFORM=%s\n", platform());

	unless (flags & SENDENV_NOREPO) {
		/*
		 * This network connection is not necessarly run from
		 * a repository, so don't send information about the
		 * current repository.  Clone is the primary example
		 * of this.
		 */
		assert(p);	/* We must be in a repo here */
		fprintf(f, "putenv BK_LEVEL=%d\n", getlevel());
		fprintf(f, "putenv BK_ROOT=%s\n", proj_root(p));
	}
	unless (flags & SENDENV_NOLICENSE) {
		/*
		 * Send information on the current license.
		 * We might be outside a repository so suppress any failures.
		 */
		lease_checking(0);
		fprintf(f, "putenv BK_LICENSE=%s\n", proj_license(p));
		lease_checking(1);
	}
	/*
	 * Send comma seperated list of client features so the bkd
	 * knows which outputs are supported.
	 *   lkey:1	use leasekey #1 to sign lease requests
	 */
	fprintf(f, "putenv BK_FEATURES=lkey:1\n");
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

	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (streq(buf, "@END@")) {
			ret = 0; /* ok */
			break;
		}
		if (r->trace) fprintf(stderr, "Server info:%s\n", buf);
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
 *
 * NOTE: When editing this function be sure to make the same changes in
 *       clone.c:in_trigger()
 */
void
sendServerInfoBlock(int is_rclone)
{
	char	buf[MAXPATH];
	char	*repoid, *p;

	out("@SERVER INFO@\n");
        sprintf(buf, "PROTOCOL=%s\n", BKD_VERSION);	/* protocol version */
	out(buf);

        sprintf(buf, "VERSION=%s\n", bk_vers);		/* binary version   */
	out(buf);
        sprintf(buf, "UTC=%s\n", bk_utc);
	out(buf);
        sprintf(buf, "TIME_T=%s\n", bk_time);
	out(buf);

	/*
	 * When we are doing a rclone, there is no tree in the bkd sode yet
	 * Do not try to get the level of the server tree.
	 */
	unless (is_rclone) {
        	sprintf(buf, "LEVEL=%d\n", getlevel());
		out(buf);
		out("LICTYPE=");
		out(eula_name());
		out("\n");
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
	if (repoid = repo_id()) {
		sprintf(buf, "\nREPO_ID=%s", repoid);
		out(buf);
	}
	/*
	 * Return a comma seperated list of features supported by the bkd.
	 *   pull-r    pull -r is parsed corrently
	 */
	out("\nFEATURES=pull-r");

	/* only send back a seed if we received one */
	if (p = getenv("BKD_SEED")) {
		out("\nSEED=");
		out(p);
	}
	out("\n@END@\n");
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

void
http_hdr(void)
{
	static	int done = 0;

	if (done) return; /* do not send it twice */
	if (getenv("BKD_DAEMON")) {
		out("HTTP/1.0 200 OK\r\n");
		out("Server: BitKeeper daemon ");
		out(bk_vers);
		out("\r\n");
	}
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

/*
 * This function works like sprintf(), except it return a
 * malloc'ed buffer which caller should free when done
 */
char *
aprintf(char *fmt, ...)
{
	va_list	ptr;
	int	rc;
	char	*buf;
	int	size = strlen(fmt) + 64;

	while (1) {
		buf = malloc(size);
		va_start(ptr, fmt);
		rc = vsnprintf(buf, size, fmt, ptr);
		va_end(ptr);
		if (rc >= 0 && rc < size - 1) break;
		free(buf);
		if (rc < 0 || rc == size - 1) {
			/*
			 * Older C libraries return -1 to indicate
			 * the buffer was too small.
			 *
			 * On IRIX, it truncates and returns size-1.
			 * We can't assume that that is OK, even
			 * though that might be a perfect fit.  We
			 * always bump up the size and try again.
			 * This can rarely lead to an extra alloc that
			 * we didn't need, but that's tough.
			 */
			size *= 2;
		} else {
			/* In C99 the number of characters needed 
			 * is always returned. 
			 */
			size = rc + 2;	/* extra byte for IRIX */
		}
	}
	return (buf); /* caller should free */
}

/*
 * Print a message on /dev/tty
 */
void
ttyprintf(char *fmt, ...)
{
	FILE	*f = fopen(DEV_TTY, "w");
	va_list	ptr;

	unless (f) f = stderr;
	va_start(ptr, fmt);
	vfprintf(f, fmt, ptr);
	va_end(ptr);
	if (f != stderr) fclose(f);
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
			if (errno == ENOENT &&
			    (realmkdir(dir, 0777) == 0)) {
				/* dir missing, applyall race? */
				continue;
			}
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
	static	char	*pg;
	int	i;

	if (pg) return (pg); /* already cached */

	unless (pg = getenv("BK_PAGER")) pg = getenv("PAGER");

	/* env can be PAGER="less -E" */
	if (pg) {
		char	**cmds = shellSplit(pg);

		unless (cmds && cmds[1] && which(cmds[1], 0, 1)) pg = 0;
		freeLines(cmds, free);
	}
	if (pg) return (strdup(pg));	/* don't trust env to not change */

	for (i = 0; pagers[i]; i++) {
		if (which(pagers[i], 0, 1)) {
			pg = pagers[i];
			return (pg);
		}
	}
	return (pg = "bk more");
}

#define	MAXARGS	100
/*
 * Set up pager and connect it to our stdout
 */
pid_t
mkpager()
{
	int	pfd;
	pid_t	pid;
	char	*pager_av[MAXARGS];
	char	*cmd;
	char	*pg = pager();

	/* win32 treats "nul" as a tty, in this case we don't care */
	unless (isatty(1)) return (0);

	/* "cat" is a no-op pager used in bkd */
	if (streq("cat", pg)) return (0);

	fflush(stdout);
	signal(SIGPIPE, SIG_IGN);
	cmd = strdup(pg); /* line2av stomp */
	line2av(cmd, pager_av); /* some user uses "less -X -E" */
	pid = spawnvp_wPipe(pager_av, &pfd, 0);
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

#define	STALE	DAY

/*
 * If they hand us a partial list use that if we can.
 * Otherwise do a full check.
 */
int
run_check(char *partial, int fix, int quiet)
{
	int	ret;
	struct	stat sb;
	time_t	now = time(0);
	char	*fixopt;
	char	*opts = quiet ? "-ac" : "-acv";

 again:
	fixopt = (fix ? "-f" : "--");
	if (!partial || 
	    fast_lstat(CHECKED, &sb) || ((now - sb.st_mtime) > STALE)) {
		ret = sys("bk", "-r", "check", opts, fixopt, SYS);
	} else {
		ret = sysio(partial, 0, 0, "bk", "check", fixopt, "-", SYS);
	}
	ret = WIFEXITED(ret) ? WEXITSTATUS(ret) : 1;
	if (fix && ret == 2) {
		fix = 0;
		goto again;
	}
	return (ret);
}

#undef	isatty

int
myisatty(int fd)
{
	int	ret;
	char	*p;
	char	buf[16];

	if (getenv("_BK_IN_BKD") && !getenv("_BK_BKD_IS_LOCAL")) return (0);

	sprintf(buf, "BK_ISATTY%d", fd);
	if (p = getenv(buf)) {
		ret = atoi(p);
	} else if (getenv("BK_NOTTY")) {
		ret = 0;
	} else {
		ret = isatty(fd);
	}
	return (ret);
}

#define	isatty	myisatty

/*
 * Print progress bar.
 *
 * Use 65 columns for the progress bar.
 * %3u% |================================ \r
 */
void
progressbar(int n, int max, char *msg)
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
	unless ((percent > last) || msg) return;
	gettimeofday(&tv, 0);
	unless (start.tv_sec) start = tv;
	elapsed =
	    (tv.tv_sec - start.tv_sec) + (tv.tv_usec - start.tv_usec) / 1.0e6;
	/* This wacky expression is to try and smooth the drawing */
	if (!msg && ((elapsed - lastup) < 0.25) && ((percent - last) <= 2)) {
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
	int	fd0, fd1;

#ifdef WIN32
	closeBadFds();
#endif
	if ((fd0 = open(DEVNULL_RD, O_RDONLY, 0)) == 0) {
do1:		if ((fd1 = open(DEVNULL_WR, O_WRONLY, 0)) != 1) {
			close(fd1);
		}
	} else {
		close(fd0);
		if (fd0 == 1) {
			goto do1;
		}
	}
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

		sprintf(b, "0x%.*x", n, (unsigned)p);
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
