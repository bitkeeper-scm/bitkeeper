#include "bkd.h"

private	void	listIt(sccs *s);
private	void	listrev(delta *d);
private int	uncompressed(char *tmpfile);
private int	compressed(int gzip, char *tmpfile);
extern	MDBM	*csetKeys(MDBM *);

private	char	*cset[] = { "bk", "cset", "-m", "-", 0 };

int
cmd_pull(int ac, char **av)
{
	char	buf[4096];
	char	csetfile[200] = CHANGESET;
	FILE	*f = 0;
	MDBM	*them = 0, *me = 0;
	int	list = 0, error = 0, bytes = 0;
	int	doit = 1, verbose = 1;
	int	gzip = 0;
	kvpair	kv;
	int	c;
	sccs	*s = 0;

	if (!exists("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		return (1);
	}
	if ((bk_mode() == BK_BASIC) && !exists("BitKeeper/etc/.master")) {
		out("ERROR-bkd std cannot access non-master repository\n");
		return (1);
	}

	out("OK-read lock granted\n");

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
	 * Get the set of keys not present in them.
	 */
	unless (me = csetKeys(them)) {
		putenv("BK_OUTGOING=NOTHING");
		out("OK-Nothing to send.\n");
		goto out;
	}
	out("OK-something to send.\n");
	if (doit) {
		if (verbose || list) out(
"OK--------------------- Sending the following csets ---------------------\n");
		f = fopen(CSETS_OUT, "w");
		unless (f) {
			perror(CSETS_OUT);
			OUT;
		}
	} else {
		putenv("BK_OUTGOING=DRYRUN");
		if (verbose || list) out(
"OK-------------------- Would send the following csets -------------------\n");
	}
	if (list) {
		s = sccs_init(csetfile, INIT_NOCKSUM, 0);
		assert(s);
	}
	bytes = 0;
	for (kv = mdbm_first(me); kv.key.dsize != 0; kv = mdbm_next(me)) {
		if (doit) fprintf(f, "%s\n", kv.val.dptr);
		if (list) {
			delta	*d = sccs_getrev(s, kv.val.dptr, 0, 0);

			d->flags |= D_SET;
		} else if (verbose) {
			unless (bytes) {
				out("OK-");
			} else {
				out(" ");
			}
			out(kv.val.dptr);
		}
		bytes += kv.val.dsize;
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
	chmod(CSETS_OUT, 0666);

	if (gzip) {
		error = compressed(gzip, CSETS_OUT);
	} else {
		error = uncompressed(CSETS_OUT);
	}

out:
	if (them) mdbm_close(them);
	if (me) mdbm_close(me);
	return (error);
}

private int
uncompressed(char *csets_out)
{
	int	fd0, fd;
	int	status;
	pid_t 	pid;

	/*
	 * What I want is to run cset with stdin being the file.
	 */

	fd0 = dup(0); close(0);
	fd = open(csets_out, 0,  0);
	assert(fd == 0);
	pid = spawnvp_ex(_P_NOWAIT, cset[0], cset);
	if (pid == -1) {
		out("ERROR-fork failed\n");
		return (1);
	}
	close(0); dup2(fd0, 0); close(fd0);
	waitpid(pid,  &status, 0);
	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	}
	return (100);
}

//XXX this code path have no regression test
private int
compressed(int gzip, char *csets_out)
{
	pid_t	pid;
	int	fd0, fd, n;
	int	rfd, status;
	char	buf[8192];

#ifndef WIN32
	signal(SIGCHLD, SIG_DFL);
#endif
	fd0 = dup(0); close(0);
	fd = open(csets_out, 0,  0);
	pid = spawnvp_rPipe(cset, &rfd);
	if (pid == -1) {
		out("ERROR-fork failed\n");
		return (1);
	}
	close(0); dup2(fd0, 0); close(fd0);
	gzip_init(gzip);
	while ((n = read(rfd, buf, sizeof(buf))) > 0) {
		gzip2fd(buf, n, 1);
	}
	gzip_done();
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	}
	return (100);
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
