#include "bkd.h"

private	void	listIt(sccs *s);
private	void	listrev(delta *d);
private int	uncompressed(char *tmpfile);
private int	compressed(int gzip, char *tmpfile);

private	char	*cset[] = { "bk", "cset", "-m", "-", 0 };

int
cmd_pull(int ac, char **av)
{
	char	buf[4096];
	char	tmpfile[MAXPATH];
	char	csetfile[200] = CHANGESET;
	FILE	*f = 0;
	MDBM	*them = 0, *me = 0;
	int	list = 0, error = 0, bytes = 0;
	int	doit = 1, verbose = 1, first = 1;
	int	gzip = 0;
	kvpair	kv;
	char	*t;
	int	c;
	sccs	*s;

	if (!exists("BitKeeper/etc")) {
		out("ERROR-Not at project root\n");
		exit(1);
	}

	unless (repository_rdlock() == 0) {
		out("ERROR-Can't get read lock on the repository.\n");
		exit(1);
	} else {
		out("OK-read lock granted\n");
	}

	while ((c = getopt(ac, av, "lnqz|")) != -1) {
		switch (c) {
		    case 'l': list = 1; verbose = 0; break;
		    case 'n': doit = 0; break;
		    case 'q': verbose = 0; break;
		    case 'z': 
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
	    	}
	}
	
	/*
	 * Get the remote keys
	 */
#define	OUT	{ error = 1; goto out; }
	unless (them = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) OUT;
	while (getline(0, buf, sizeof(buf)) > 0) {
		if (streq("@ABORT@", buf)) OUT;
		if (streq("@END@", buf)) break;
		mdbm_store_str(them, buf, "", 0);
	}

	/*
	 * Get the local keys
	 */
	unless (f = popen("bk prs -r1.0.. -bhad':KEY: :REV:' ChangeSet", "r")) {
		error = -1;
		goto out;
	}
	unless (me = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) OUT;
	while (fnext(buf, f)) {
		chop(buf);
		unless (t = strchr(buf, ' ')) OUT;
		*t++ = 0;
		unless (mdbm_fetch_str(them, buf)) {
			if (first) {
				// XXX
				out("Different project, root key mismatch\n");
				goto out;
			}
			mdbm_store_str(me, t, "", 0);
			bytes += strlen(t) + 1;
		}
		first = 0;
	}
	pclose(f); f = 0;
	unless (bytes) {
		if (doit) out("OK-Nothing to send.\n");
		goto out;
	}
	out("OK-something to send.\n");
	if (doit) {
		if (verbose || list) out(
"OK--------------------- Sending the following csets ---------------------\n");
		gettemp(tmpfile, "push");
		f = fopen(tmpfile, "w");
	} else {
		if (verbose || list) out(
"OK-------------------- Would send the following csets -------------------\n");
	}
	if (list) {
		s = sccs_init(csetfile, INIT_NOCKSUM, 0);
		assert(s);
	}
	bytes = 0;
	for (kv = mdbm_first(me); kv.key.dsize != 0; kv = mdbm_next(me)) {
		if (doit) fprintf(f, "%s\n", kv.key.dptr);
		if (list) {
			delta	*d = sccs_getrev(s, kv.key.dptr, 0, 0);

			d->flags |= D_SET;
		} else if (verbose) {
			unless (bytes) {
				out("OK-");
			} else {
				out(" ");
			}
			out(kv.key.dptr);
		}
		bytes += kv.key.dsize;
		if (bytes >= 50) {
			if (verbose) out("\n");
			bytes = 0;
		}
	}
	if (verbose || list) {
		if (list) {
			listIt(s);
			sccs_free(s);
		} else {
			out("\n");
		}
		out(
"OK----------------------------------------------------------------------\n");
		out("OK-END\n");
	}
	unless (doit) goto out;

	fclose(f); f = 0;

	if (gzip) {
		error = compressed(gzip, tmpfile);
	} else {
		error = uncompressed(tmpfile);
	}

out:
	if (f) pclose(f);
	if (them) mdbm_close(them);
	if (me) mdbm_close(me);
	repository_rdunlock(0);
	out("OK-Unlocked\n");
	exit(error);
}

private int
uncompressed(char *tmpfile)
{
	pid_t	pid;

	/*
	 * What I want is to run cset with stdin being the file.
	 */
	if (pid = fork()) {
		int	status;

		if (pid == -1) {
			repository_rdunlock(0);
			out("ERROR-fork failed\n");
			return (1);
		}
		waitpid(pid, &status, 0);
		unlink(tmpfile);
		if (WIFEXITED(status)) {
			return (WEXITSTATUS(status));
		}
		return (100);
	} else {
		close(0);
		open(tmpfile, 0, 0);
		execvp(cset[0], cset);
		exit(1);
	}
}

private int
compressed(int gzip, char *tmpfile)
{
	pid_t	pid;
	int	n;
	int	p[2];
	char	buf[8192];

	if (pipe(p) == -1) {
err:		repository_rdunlock(0);
		exit(1);
	}

	pid = fork();
	if (pid == -1) {
		out("ERROR-fork failed\n");
		goto err;
	} else if (pid) {
		int	status;

		close(p[1]);
		gzip_init(gzip);
		while ((n = read(p[0], buf, sizeof(buf))) > 0) {
			gzip2fd(buf, n, 1);
		}
		gzip_done();
		waitpid(pid, &status, 0);
		unlink(tmpfile);
		if (WIFEXITED(status)) {
			return (WEXITSTATUS(status));
		}
		return (100);
	} else {
		close(0);
		open(tmpfile, 0, 0);
		close(1); dup(p[1]); close(p[1]);
		close(p[0]);
		execvp(cset[0], cset);
		exit(1);
	}
}

private void
listIt(sccs *s)
{
	delta	*d;

	for (d = s->table; d; d = d->next) {
		if (d->flags & D_SET) listrev(d);
	}
}

private void
listrev(delta *d)
{
	char	*t;
	int	i;
	char	buf[100];

	assert(d);
	out("OK-ChangeSet@");
	out(d->rev);
	out(", ");
	if (atoi(d->sdate) <= 68) {
		strcpy(buf, "20");
		strcat(buf, d->sdate);
	} else if (atoi(d->sdate) > 99) {	/* must be 4 digit years */
		strcpy(buf, d->sdate);
	} else {
		strcpy(buf, "19");
		strcat(buf, d->sdate);
	}
	for (t = buf; *t != '/'; t++); *t++ = '-';
	for ( ; *t != '/'; t++); *t = '-';
	out(buf);
	if (d->zone) {
		out("-");
		out(d->zone);
	}
	out(", ");
	out(d->user);
	if (d->hostname) {
		out("@");
		out(d->hostname);
	}
	out("\n");
	EACH(d->comments) {
		out("OK-  ");
		out(d->comments[i]);
		out("\n");
    	}
	out("OK-\n");
}
