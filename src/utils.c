#include "bkd.h"

bkdopts	Opts;	/* has to be declared here, other people use this code */

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
					    "%d [%s]\n", getpid(), buf);
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
	fprintf(stderr,
		"wait_eof: Got %d unexpectied byte(s) from remote\n", i);
	buf[i] = 0;
	fprintf(stderr, "buf=\"%s\"\n", buf);
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
					    "%d [%s]\n", getpid(), buf);
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

private jmp_buf	jmp;
private	void	(*handler)(int);
private	void	abort_prompt() { longjmp(jmp, 1); }

/*
 * Prompt the user and get an answer.
 * The buffer has to be MAXPATH bytes long.
 */
int
prompt(char *msg, char *buf)
{
	if (setjmp(jmp)) {
		fprintf(stderr, "\n(interrupted)\n");
		signal(SIGINT, handler);
		return (0);
	}
	handler = signal(SIGINT, abort_prompt);
	write(2, msg, strlen(msg));
	write(2, " ", 1);
	if (getline(0, buf, MAXPATH) > 1) {
		signal(SIGINT, handler);
		return (1);
	}
	signal(SIGINT, handler);
	return (0);
}

int
confirm(char *msg)
{
	char	buf[100];

	if (setjmp(jmp)) {
		fprintf(stderr, "\n(interrupted)\n");
		signal(SIGINT, handler);
		return (0);
	}
	fflush(stdout);
	write(1, msg, strlen(msg));
	write(1, " (y/n) ", 7);
	if (getline(0, buf, sizeof(buf)) <= 1) {
		signal(SIGINT, handler);
		return (0);
	}
	signal(SIGINT, handler);
	return ((buf[0] == 'y') || (buf[0] == 'Y'));
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
	bkd(compress, r);
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
	if (r->httpd) {
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
	char	*p = (char *) malloc(len);

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

	while (getline2(r, buf, sizeof(buf)) >= 0) {
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
	char	buf[200], *p;

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


private char *
rootkey()
{
	static char key[MAXKEY] = "";
	char	s_cset[] = CHANGESET;
	sccs	*s;

	s = sccs_init(s_cset, INIT_NOCKSUM, 0);
	assert(s);
	sccs_sdelta(s, sccs_ino(s), key);
	sccs_free(s);
	return (key);
}

void
add_cd_command(FILE *f, remote *r)
{
	int	needQuote = 0;
	char	*k;

	/*
	 * XXX TODO need to handle embeded quote in pathname
	 */
	if (strchr(r->path, ' ')) needQuote = 1;
	if (streq(r->path, "///LOG_ROOT///")) {
		k = rootkey(); assert(k);
		if (strchr(k, ' ')) needQuote = 1;
		if (needQuote) {
			fprintf(f, "cd \"%s%s\"\n", r->path, rootkey());
		} else {
			fprintf(f, "cd %s%s\n", r->path, rootkey());
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
put_trigger_env(char *where, char *v, char *value)
{
	char *env;
	char *buf;
	char *e;

	env = aprintf("BK%s_%s", where, v);
	if ((e = getenv(env)) && streq(e, value)) return;
	buf = aprintf("%s=%s", env, value);
	putenv(strdup(buf));
	free(env); free(buf);
}

void
putroot(char *where)
{
	char	*root = sccs_root(0);
	char	*e, *buf, *env;

	env = aprintf("BK%s_ROOT", where);

	if (root) {
		if (streq(root, ".")) {
			char	pwd[MAXPATH];

			getcwd(pwd, MAXPATH);
			if ((e = getenv(env)) && streq(e, pwd)) {
				return;
			}
			buf = aprintf("%s=%s", env, pwd);
		} else {
			if ((e = getenv(env)) && streq(e, root)) {
				return;
			}
			buf = aprintf("%s=%s", env, root);
		}
		putenv(buf);
		buf = 0; /* paranoid */
		free(root);
	}
	free(env);
}

void
sendEnv(FILE *f, char **envVar, char *direction)
{
	int	i;
	char	*root;

	fprintf(f, "putenv BK_REMOTE_PROTOCOL=%s\n", BKD_VERSION);

	fprintf(f, "putenv BK%s_VERSION=%s\n", direction, bk_vers);
	fprintf(f, "putenv BK%s_UTC=%s\n", direction, bk_utc);
	fprintf(f, "putenv BK%s_TIME_T=%s\n", direction, bk_time);
	fprintf(f, "putenv BK%s_USER=%s\n", direction, sccs_getuser());
	fprintf(f, "putenv BK%s_HOST=%s\n", direction, sccs_gethost());
	fprintf(f, "putenv BK%s_LEVEL=%d\n", direction, getlevel());
	root = sccs_root(0);
	if (root) {
		if (streq(root, ".")) {
			char	pwd[MAXPATH];

			getcwd(pwd, MAXPATH);
			fprintf(f, "putenv BK%s_ROOT=%s\n", direction, pwd);
		} else {
			fprintf(f, "putenv BK%s_ROOT=%s\n", direction, root);
		}
		free(root);
	}
	EACH(envVar) {
		fprintf(f, "putenv %s\n", envVar[i]);
	}
}

int
getServerInfoBlock(remote *r, char *direction)
{
	char	*p, buf[4096];
	int	len;

	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (streq(buf, "@END@")) return (0); /* ok */
		if (r->trace) fprintf(stderr, "Server info:%s\n", buf);
		len = strlen(buf); 
		/*
		 * 11 is the length of longest prefix + null termination byte
	 	 * Note: This memory is de-allocated at exit
		 */
		p = (char *) malloc(len + 11 + strlen(direction)); assert(p); 
		if (strneq(buf, "PROTOCOL", 8)) {
			sprintf(p, "BK_REMOTE_%s", buf);
		} else {
			sprintf(p, "BK%s_%s", direction, buf);
		}
		putenv(p);
	}
	return (1); /* protocol error */
}

void
sendServerInfoBlock()
{
	char	buf[MAXPATH];

	out("@SERVER INFO@\n");
        sprintf(buf, "PROTOCOL=%s\n", BKD_VERSION);	/* protocol version */
	out(buf);
        sprintf(buf, "VERSION=%s\n", bk_vers);		/* binary version   */
	out(buf);
        sprintf(buf, "UTC=%s\n", bk_utc);
	out(buf);
        sprintf(buf, "TIME_T=%s\n", bk_time);
	out(buf);
        sprintf(buf, "LEVEL=%d\n", getlevel());
	out(buf);
	out("ROOT=");
	getcwd(buf, sizeof(buf));
	out(buf);
	out("\nUSER=");
	out(sccs_getuser());
	out("\nHOST=");
	out(sccs_gethost());
	out("\n@END@\n");
} 

void
http_hdr()
{
	static done = 0;
	
	if (done) return; /* do not send it twice */
	out("Cache-Control: no-cache\n");	/* for http 1.1 */
	out("Pragma: no-cache\n");		/* for http 1.0 */
	out("Content-type: text/plain\n\n"); 
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
 * Drain non-standard message:
 * There are two possibilities:
 * 1) remote is running a version 1.2 (i.e. old) bkd
 * 2) remote is not running a bkd.
 */
void
drainNonStandardMsg(remote *r, char *buf, int bsize)
{
	int bkd_msg = 0;

	if (strneq("ERROR-BAD CMD: putenv", buf, 21)) {
		bkd_msg = 1;
		while (getline2(r, buf, bsize) > 0) {
			if (strneq("ERROR-BAD CMD: putenv", buf, 21)) continue;
			break;
		}
	}

	do {
		if (strneq("ERROR-BAD CMD: pull_part1", buf, 25)) break;
		if (strneq("ERROR-BAD CMD: @END", buf, 19)) break; /*for push*/
		if (strneq("ERROR-BAD CMD:", buf, 14)) continue;
		if (streq("OK-root OK", buf)) continue;
		if (streq("ERROR-exiting", buf)) exit(1);
		fprintf(stderr,
			"drainNonStandardMsg: Unexpected response: %s\n", buf);
		break;
	} while (getline2(r, buf, bsize) > 0);

	if (bkd_msg) {
		fprintf(stderr,
			"Remote seems to be running a older BitKeeper release\n"
			"Try \"bk opush\", \"bk opull\" or \"bk oclone\"\n");
	}
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
 * Return ture if reposirory have less than BK_MAX_FILES file
 */
int
smallTree(int threshold)
{
        FILE    *f;
        int     i = 0;
        char    buf[ 2 * MAXKEY + 10];
 
        f = popen("bk -R get -qkp ChangeSet", "r");
        assert(f);
        while (fnext(buf, f)) {
                if (++i > threshold) {
                        pclose(f);
                        return (0);
                }
        }
        pclose(f);
        return (1);
}         

/*
 * This function works like sprintf(), except it return a
 * malloc'ed buffer which caller should free when done
 */
char *
aprintf(char *fmt, ...)
{
	va_list	ptr;
	int	rc, size = 512;
	char	*buf = malloc(size);

	va_start(ptr, fmt);

	rc = vsnprintf(buf, size, fmt, ptr);
	while ((rc == -1) || (rc >= size)) {
		if (rc == -1) size *= 2;
		if (rc >= size) size = rc + 1;
		free(buf);
		buf = (char *) malloc(size);
		assert(buf);
		rc = vsnprintf(buf, size, fmt, ptr);
	}
	va_end(ptr);
	return (buf); /* caller should free */
}

int
isLocalHost(char *h)
{
	unless (h) return (0);
	return(streq("localhost", h) || streq("127.0.0.1", h));
}
