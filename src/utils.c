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

	if (echo == -1) echo = getenv("BK_GETLINE") != 0;
	buf[0] = 0;
	size--;
	unless (size) return (-3);
	for (;;) {
		switch (ret = read(in, &c, 1)) {
		    case 1:
			if ((buf[i] = c) == '\n') {
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
