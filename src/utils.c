#include "bkd.h"

bkdopts	Opts;	/* has to be declared here, other people use this code */

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
	int	sigs = sigcaught(SIGINT);
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
				if (echo) fprintf(stderr, "[%s]\n", buf);
				return (i + 1);	/* we did read a newline */
			}
			if (++i == size) {
				buf[i] = 0;
				return (-2);
			}
			break;

		    default:
			unless (errno == EINTR) {	/* for !SIGINT */
err:				buf[i] = 0;
				if (echo) {
					perror("getline");
					fprintf(stderr, "[%s]=%d\n", buf, ret);
				}
				return (-1);
			} else if (sigs != sigcaught(SIGINT)) {
				goto err;
			}
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
 * On WIN32, read() does not work a socket.
 * TODO: This function should be merged with the getline() function above
 */
int
getline2(remote *r, char *buf, int size)
{
	int	ret, i = 0;
	char	c;
	int	sigs = sigcaught(SIGINT);
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
				if (echo) fprintf(stderr, "[%s]\n", buf);
				return (i + 1);	/* we did read a newline */
			}
			if (++i == size) {
				buf[i] = 0;
				return (-2);
			}
			break;

		    default:
			unless (errno == EINTR) {	/* for !SIGINT */
err:				buf[i] = 0;
				if (echo) {
					perror("getline");
					fprintf(stderr, "[%s]=%d\n", buf, ret);
				}
				return (-1);
			} else if (sigs != sigcaught(SIGINT)) {
				goto err;
			}
		}
	}
}

/*
 * Prompt the user and get an answer.
 * The buffer has to be MAXPATH bytes long.
 */
int
prompt(char *msg, char *buf)
{
	write(2, msg, strlen(msg));
	write(2, " ", 1);
	if (getline(0, buf, MAXPATH) > 1) return (1);
	return (0);
}

int
confirm(char *msg)
{
	char	buf[100];

	fflush(stdout);
	write(1, msg, strlen(msg));
	write(1, " (y/n) ", 7);
	if (getline(0, buf, sizeof(buf)) <= 1) return (0);
	return ((buf[0] == 'y') || (buf[0] == 'Y'));
}

/*
 * Return an MDBM with all the keys from the ChangeSet file
 * as db{key} = rev.
 */
MDBM	*
csetKeys(MDBM *not)
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
send_msg(remote *r, char *msg, int mlen, int extra, int compress)
{
	if (r->httpd) {
		assert((r->rfd == -1) && (r->wfd == -1));
		bkd(compress, r);
		if ((r->wfd < 0) && r->trace) {
			fprintf(stderr,
				"send_msg: cannot connect to %s:%d\n",
				r->host, r->port);
			return (-1);
		}
		http_send(r, msg, mlen, extra, "BitKeeper", WEB_BKD_CGI);
	} else {
		if (r->wfd == -1) bkd(compress, r);
		if (write_blk(r, msg, mlen) != mlen) {
			if (r->trace) perror("send_msg");
			return (-1);
		}
	}
	return 0;
}

int 
skip_http_hdr(remote *r)
{
	char	buf[1024];
	int	i =0;

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
			}
			r->rfd = -1;
			break;
	    case 1: 	if (r->wfd == -1) return;
			if (r->isSocket) {
				shutdown(r->wfd, 1);
			} else {
				close(r->wfd);
			}
			r->wfd = -1;
			break;
	    case 2:	if (r->rfd == -1) return;
			if (r->isSocket) {
				shutdown(r->rfd, 2);
			} else {
				close(r->rfd);
				close(r->wfd);
			}
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
			if (verbose) fprintf(stderr, "get_ok: Got EOF.\n");
			return (1); /* failed */
		}
		p = buf;
	}

	if (streq(p, "@OK@")) return (0); /* ok */
	if (strneq(p, "ERROR-BAD CMD:", 14)) {
		fprintf(stderr,
			"Remote seems to be running a older BitKeeper release\n"
			"Try \"bk opush\", \"bk opull\" or \"bk oclone\"\n");
			return (1);
	}
	if (verbose) {
		i = 0;
		fprintf(stderr, "remote: %s\n", p);
		while (getline2(r, buf, sizeof(buf)) > 0) {
			fprintf(stderr, "remote: %s\n", buf);
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
	if (streq(r->path, "///LOG_ROOT///")) {
		fprintf(f, "cd %s%s\n", r->path, rootkey());
	} else {
		fprintf(f, "cd %s\n", r->path);
	}
}

void
sendEnv(FILE *f, char **envVar)
{
	int i;

	fprintf(f, "putenv BK_CLIENT_PROTOCOL=%s\n", BKD_VERSION);
	fprintf(f, "putenv BK_CLIENT_RELEASE=%s\n", BK_RELEASE);
	fprintf(f, "putenv BK_CLIENT_USER=%s\n", sccs_getuser());
	fprintf(f, "putenv BK_CLIENT_HOST=%s\n", sccs_gethost());
	EACH(envVar) {
		fprintf(f, "putenv %s\n", envVar[i]);
	}
}

int
getServerInfoBlock(remote *r)
{
	char	*p, buf[4096];
	int	len;

	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (streq(buf, "@END@")) return (0); /* ok */
		if (r->trace) fprintf(stderr, "Server info:%s\n", buf);
		len = strlen(buf); 
		/*
		 * 11 is the length of prefix + null termination byte
	 	 * Note: This memory is de-allocated at exit
		 */
		p = (char *) malloc(len + 11); assert(p); 
		sprintf(p, "BK_SERVER_%s", buf);
		putenv(p);
	}
	return (1); /* protocol error */
}

void
sendServerInfoBlock()
{
	char buf[100];

	out("@SERVER INFO@\n");
        sprintf(buf, "PROROCOL=%s\n", BKD_VERSION);	/* protocol version */
	out(buf);
        sprintf(buf, "RELEASE=%s\n", BK_RELEASE);	/* binary version   */
	out(buf);
	out("@END@\n");
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
