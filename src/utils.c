#include "bkd.h"

bkdopts	Opts;	/* has to be declared here, other people use this code */

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

	f = fopen(file, "rb");
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
writen(int to, char *buf, int size)
{
	int	done;
	int	n;

	for (done = 0; done < size; ) {
		n = write(to, buf + done, size - done);
		if (n <= 0) {
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

/*
 * We need this becuase on win32, read() does not work on socket
 */
int
read_blk(remote *r, char *buf, int len)
{
	if (r->isSocket) {
		return (recv(r->rfd, buf, len, 0));
	} else {
		return (read(r->rfd, buf, len));
	}
}

/*
 * We need this becuase on win32, write() does not work on socket
 */
int
write_blk(remote *r, char *buf, int len)
{
	if (r->isSocket) {
		return (send(r->wfd, buf, len, 0));
	} else {
		return (write(r->wfd, buf, len));
	}
}

void
wait_eof(remote *r, int verbose)
{
	int	i;
	char	buf[MAXLINE];

	if (verbose) fprintf(stderr, "Waiting for remote to disconnect\n");
	i = read_blk(r, buf, sizeof(buf) - 1);
	if (i <= 0) {
		if (verbose) fprintf(stderr, "Remote Disconnected\n");
		return;
	}

	if (verbose) {
		fprintf(stderr,
			"wait_eof: Got %d unexpected byte(s) from remote\n", i);
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
	fflush(stdout);

	flush_fd0(); /* for Win/98 and Win/ME */
	write(1, msg, strlen(msg));
	write(1, " ", 1);
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
	char	buf[100];
	int	gotsome;

	caught = 0;
	sig_catch(abort_prompt);
	fflush(stdout);

	flush_fd0(); /* for Win/98 and Win/Me */
	write(1, msg, strlen(msg));
	write(1, " (y/n) ", 7);
	gotsome = getline(0, buf, sizeof(buf)) > 1;
	if (caught) {
		fprintf(stderr, "\n(interrupted)\n");
		assert(!gotsome);
	}
	sig_restore();
	return (gotsome && ((buf[0] == 'y') || (buf[0] == 'Y')));
}

/*
 * Usage: bk prompt [-n NO] [-y YES] [-t TITLE] msg | -f FILE | -p program
 */
int
prompt_main(int ac, char **av)
{
	int	i, c, ret, ask = 1, nogui = 0;
	char	*prog = 0, *file = 0, *no = "NO", *yes = "OK", *title = 0;
	char	*type = 0;
	char	*cmd;
	FILE	*in, *out = 0;
	char	msgtmp[MAXPATH];
	extern	char *pager;

	while ((c = getopt(ac, av, "cegiowxf:n:p:t:y:")) != -1) {
		switch (c) {
		    case 'c': ask = 0; break;
		    case 'e': type = "-E"; break;
		    case 'g': nogui = 1; break;
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
		if (out) pclose(out);
		exit(1);
	}
	if (prog) {
		assert(!file);
		gettemp(msgtmp, "prompt");
		file = msgtmp;
		cmd = aprintf("%s > %s", prog, file);
		putenv("PAGER=cat");
		if (system(cmd)) goto err;
		free(cmd);
	}
	if (getenv("BK_GUI") && !nogui) {
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
		ret = spawnvp_ex(_P_WAIT, nav[0], nav);
		if (prog) unlink(msgtmp);
		if (WIFEXITED(ret)) exit(WEXITSTATUS(ret));
		exit(2);
	}

	out = popen(pager, "w");
	if (type) {
		int	len, half;

		switch (type[1]) {
		    case 'x': type = 0; break;
		    case 'E': type = "Error"; break;
		    case 'I': type = "Info"; break;
		    case 'W': type = "Warning"; break;
		}
		len = strlen(type) + 4;
		half = (76 - len) / 2;
		for (i = 0; i < half; ++i) fputc('=', out);
		fputc(' ', out);
		fputc(' ', out);
		fputs(type, out);
		fputc(' ', out);
		fputc(' ', out);
		if (len & 1) fputc(' ', out);
		for (i = 0; i < half; ++i) fputc('=', out);
		fputc('\n', out);
		fflush(out);
	}
	if (file) {
		char	buf[MAXLINE];

		unless (in = fopen(file, "r")) goto err;
		while (fnext(buf, in)) {
			fputs(buf, out);
		}
		fclose(in);
	} else if (streq(av[optind], "-")) {
		goto err;
	} else {
		fputs(av[optind], out);
		fputc('\n', out);
	}
	if (type) {
		for (i = 0; i < 76; ++i) fputc('=', out);
		fputc('\n', out);
	}
	pclose(out);
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
		
	unless (s = sccs_init(s_cset, INIT_NOCKSUM, 0)) {
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



int 
send_msg(remote *r, char *msg, int mlen, int extra, int compress)
{
	assert(r->wfd != -1);
	if (r->type == ADDR_HTTP) {
		if (http_send(r, msg, mlen, extra, "BitKeeper", WEB_BKD_CGI)) {
			fprintf(stderr, "http_send failed\n");
			return (-1);
		}
	} else {
		if (write_blk(r, msg, mlen) != mlen) {
			perror("send_msg");
			return (-1);
		}
	}
	return 0;
}


int
send_file(remote *r, char *file, int extra, int gzip)
{
	int	fd, rc, len = size(file);
	char	*p = malloc(len);

	assert(p);
	fd = open(file, O_RDONLY, 0);
	assert(fd >= 0);
	if (read(fd, p, len) != len) {
		perror(file);
		return (-1);
	}
	close(fd);
	rc = send_msg(r, p,  len, extra, gzip);
	free (p);
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
	    case 0:	if (r->rfd == -1) return;
			if (r->isSocket) {
				shutdown(r->rfd, 0);
			} else {
				close(r->rfd);
				r->rfd = -1;
			}
			break;
	    case 1: 	if (r->wfd == -1) return;
			if (r->isSocket) {
				shutdown(r->wfd, 1);
			} else {
				close(r->wfd);
				r->wfd = -1;
			}
			break;
	    case 2:	if (r->rfd >= 0) close(r->rfd);
			if (r->wfd >= 0) close(r->wfd);
			r->rfd = r->wfd = -1;
			break;
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
	char	*root = sccs_root(0);
	char	*file;
	char	*repoid;

	unless (root) return (0);
	file = aprintf("%s/" REPO_ID, root);
	free(root);
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

	s = sccs_init(s_cset, INIT_NOCKSUM, 0);
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
	safe_putenv("%s_%s=%s", prefix, v, value);
}

void
putroot(char *where)
{
	char	*root = sccs_root(0);

	if (root) {
		if (streq(root, ".")) {
			char	pwd[MAXPATH];

			getcwd(pwd, MAXPATH);
			safe_putenv("%s_ROOT=%s", where, pwd);
		} else {
			safe_putenv("%s_ROOT=%s", where, root);
		}
		free(root);
	}
}

/*
 * Send env varibale to remote bkd.
 */
void
sendEnv(FILE *f, char **envVar, remote *r, int isClone)
{
	int	i;
	char	*root, *user, *host, *repo;

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

	/*
	 * We have no Package root when we clone, so skip root related variables
	 * This is important when we have nested repository. Otherwise, we may
	 * incorrectly pick up info in the enclosing tree. Jack Moffitt's
	 * icecast repository exposed this problem.
	 */
	unless (isClone) {
		fprintf(f, "putenv BK_LEVEL=%d\n", getlevel());

		root = sccs_root(0);
		if (root) {
			if (streq(root, ".")) {
				char	pwd[MAXPATH];

				getcwd(pwd, MAXPATH);
				fprintf(f, "putenv BK_ROOT=%s\n", pwd);
			} else {
				fprintf(f, "putenv BK_ROOT=%s\n", root);
			}
			free(root);
		}
	}

	EACH(envVar) {
		fprintf(f, "putenv %s\n", envVar[i]);
	}
}

int
getServerInfoBlock(remote *r)
{
	char	buf[4096];

	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (streq(buf, "@END@")) return (0); /* ok */
		if (r->trace) fprintf(stderr, "Server info:%s\n", buf);
		if (strneq(buf, "PROTOCOL", 8)) {
			safe_putenv("BK_REMOTE_%s", buf);
		} else {
			safe_putenv("BKD_%s", buf);
			if (strneq(buf, "REPO_ID=", 8)) {
				cmdlog_addnote("rmts", buf+8);
			}
		}
	}
	return (1); /* protocol error, never saw @END@ */
}

void
sendServerInfoBlock(int is_rclone)
{
	char	buf[MAXPATH];
	char	*repoid;

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
	}
	out("ROOT=");
	getcwd(buf, sizeof(buf));
	out(buf);
	out("\nUSER=");
	out(sccs_getuser());
	out("\nHOST=");
	out(sccs_gethost());
	if (repoid = repo_id()) {
		sprintf(buf, "\nREPO_ID=%s", repoid);
		out(buf);
	}
	out("\n@END@\n");
} 

void
http_hdr(int full)
{
	static	int done = 0;
	
	if (done) return; /* do not send it twice */
	if (full) {
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

void
flush2remote(remote *r)
{
	if (r->isSocket) {
		flushSocket(r->wfd);
	} else {
		flush_fd(r->wfd); /* this is a no-op on Unix */
	}
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
			fprintf(stderr,
			    "Remote dose not understand \"pull_part1\""
			    "command\n"
			    "There are two possibilities:\n"
			    "a) Remote bkd has disabled \"pull\" command.\n"
			    "b) We are talking to a old 1.x bkd.\n");
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

	if (bkd_msg) {
		fprintf(stderr,
			"Remote seems to be running a older BitKeeper release\n"
			"Try \"bk opush\", \"bk opull\" or \"bk oclone\"\n");
	}
	freeLines(lines, free);
	exit(1);
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
 * Return the number of files in a repository.
 */
int
nFiles(void)
{
        FILE    *f;
        int     i = 0;
        char    buf[ 2 * MAXKEY + 10];
	char	*p = getenv("_BK_NFILES");
 
        f = popen("bk -R get -qkp ChangeSet", "r");
        assert(f);
        while (fnext(buf, f)) {
                i++;
        }
        pclose(f);
	/* regressions: allow it to be adjusted up only */
	if (p && (atoi(p) > i)) i = atoi(p);
        return (i);
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
			    ((mkdir)(dir, 0777) == 0)) {
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
	if (bk_proj && bk_proj->root) return;
	fprintf(stderr, "%s: cannot find package root\n", who);
	exit(1);
}

int
spawn_cmd(int flag, char **av)
{
	int ret;

	ret = spawnvp_ex(flag, av[0], av); 
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



#ifndef WIN32
/*
 * The semantics of this interface is that it must return a NON-NULL list
 * even if the list is empty.  The NULL return value is reserved for errors.
 * This removes all duplicates, ".", and "..".
 * It also checks for updates to the dir and retries if it sees one.
 */
char	**
getdir(char *dir)
{
	char	**lines = 0;
	DIR	*d;
	struct	dirent   *e;
	struct  stat sb1, sb2;
	int	i;

again:	if (lstat(dir, &sb1)) {
		if (errno == ENOENT) return (NULL);
		perror(dir);
		return(NULL);
	}
	if ((d = opendir(dir)) == NULL)  {
		perror(dir);
		return(NULL);
	}
	lines = addLine(lines, strdup("f"));
	assert(streq("f", lines[1]));
	removeLineN(lines, 1, free);
	while (e = readdir(d)) {
		unless (streq(e->d_name, ".") || streq(e->d_name, "..")) {
			lines = addLine(lines, strdup(e->d_name));
		}
	}
	closedir(d);
	if (lstat(dir, &sb2)) {
		perror(dir);
		freeLines(lines, free);
		return(NULL);
	}
	if ((sb1.st_mtime != sb2.st_mtime) || 
	    (sb1.st_size != sb2.st_size)) {
		freeLines(lines, free);
		lines = 0;
		goto again;
	}
	sortLines(lines, 0);

	/* Remove duplicate files that can result on some filesystems.  */
	EACH(lines) {
		while ((i > 1) && streq(lines[i-1], lines[i])) {
			removeLineN(lines, i, free);
		}
	}
	return (lines);
}
#else
char	**
getdir(char *dir)
{
	struct  _finddata_t found_file;
	char	*file = found_file.name;
	char	**lines = 0;
	char	buf[MAXPATH];
	long	dh;

	bm2ntfname(dir, buf);
	strcat(buf, "\\*.*");
	if ((dh =  _findfirst(buf, &found_file)) == -1L) {
		if (errno == ENOENT) return (NULL);
		perror(dir);
		return (NULL);
	}
	lines = addLine(lines, strdup("f"));
	assert(streq("f", lines[1]));
	removeLineN(lines, 1, free);

	do {
		unless (streq(file, ".") || streq(file, "..")) {
			localName2bkName(file, file);
			lines = addLine(lines, strdup(file));
		}
	} while (_findnext(dh, &found_file) == 0);
	_findclose(dh);

#if	0
	sortLines(lines);

	/* Remove duplicate files that can result on some filesystems.  */
	EACH(lines) {
		while ((i > 1) && streq(lines[i-1], lines[i])) {
			removeLineN(lines, i);
	}
#endif
	return (lines);
	
}
#endif

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
	extern 	char *pager;

	/* "cat" is a no-op pager used in bkd */
	if (streq("cat", pager)) return (0);

	fflush(stdout);
	signal(SIGPIPE, SIG_IGN);
	cmd = strdup(pager); /* line2av stomp */
	line2av(cmd, pager_av); /* win32 pager is "less -E" */
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
		unless (isdir(buf)) return (1); /* this call lstat() */
		unless (s = strrchr(buf, '/')) {
			/* might have started with ../someplace */
			return (streq(buf, ".."));
		}
	}
	/*NOTREACHED*/
}

#define	STALE	(24*60*60)

/*
 * If they hand us a partial list use that if we can.
 * Otherwise do a full check.
 */
int
run_check(char *partial, int fix)
{
	int	ret;
	struct	stat sb;
	time_t	now = time(0);
	char	*fixopt;

 again:
	fixopt = (fix ? "-f" : "--");
	if (!partial || stat(CHECKED, &sb) || ((now - sb.st_mtime) > STALE)) {
		ret = sys("bk", "-r", "check", "-ac", fixopt, SYS);
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
