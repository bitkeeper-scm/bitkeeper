#include "bkd.h"

void	listIt(sccs *s);
void	listrev(delta *d);

int
cmd_pull(int ac, char **av)
{
	static	char *cset[] = { "bk", "cset", "-m", "-", 0 };
	char	buf[4096];
	char	tmpfile[MAXPATH];
	char	csetfile[200] = CHANGESET;
	FILE	*f = 0;
	MDBM	*them = 0, *me = 0;
	int	list = 0, error = 0, bytes = 0;
	int	doit = 1, verbose = 1, first = 1;
	pid_t	pid;
	kvpair	kv;
	char	*t;
	int	c;
	sccs	*s;

	if (!exists("BitKeeper/etc")) {
		writen(1, "ERROR-Not at project root\n");
		exit(1);
	}

	unless (repository_rdlock() == 0) {
		writen(1, "ERROR-Can't get read lock on the repository.\n");
		exit(1);
	} else {
		writen(1, "OK-read lock granted\n");
	}

	while ((c = getopt(ac, av, "lnq")) != -1) {
		switch (c) {
		    case 'l': list = 1; verbose = 0; break;
		    case 'n': doit = 0; break;
		    case 'q': verbose = 0; break;
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
				writen(1,
				    "Different project, root key mismatch\n");
				goto out;
			}
			mdbm_store_str(me, t, "", 0);
			bytes += strlen(t) + 1;
		}
		first = 0;
	}
	pclose(f); f = 0;
	unless (bytes) {
		if (doit) writen(1, "OK-Nothing to send.\n");
		goto out;
	}
	writen(1, "OK-something to send.\n");
	if (doit) {
		if (verbose || list) writen(1, 
"OK--------------------- Sending the following csets ---------------------\n");
		gettemp(tmpfile, "push");
		f = fopen(tmpfile, "w");
	} else {
		if (verbose || list) writen(1,
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
			writen(1, kv.key.dptr);
		}
		bytes += kv.key.dsize;
		if (bytes >= 50) {
			if (verbose) writen(1, "\nOK-");
			bytes = 0;
		} else {
			if (verbose) writen(1, " ");
		}
	}
	if (verbose || list) {
		if (list) {
			listIt(s);
			sccs_free(s);
		} else {
			writen(1, "\n");
		}
		writen(1, 
"OK----------------------------------------------------------------------\n");
		writen(1, "OK-END\n");
	}
	unless (doit) goto out;

	fclose(f); f = 0;

	/*
	 * What I want is to run cset with stdin being the file.
	 */
	if (pid = fork()) {
		int	status;

		if (pid == -1) {
			writen(1, "ERROR-fork failed\n");
			goto out;
		}
		waitpid(pid, &status, 0);
		unlink(tmpfile);
		if (WIFEXITED(status)) {
			error = WEXITSTATUS(status);
			goto out;
		}
		OUT;
	} else {
		close(0);
		open(tmpfile, 0, 0);
		execvp(cset[0], cset);
	}

out:
	if (f) pclose(f);
	if (them) mdbm_close(them);
	if (me) mdbm_close(me);
	repository_rdunlock(0);
	writen(1, "OK-Unlocked\n");
	exit(error);
}

void
listIt(sccs *s)
{
	delta	*d;

	for (d = s->table; d; d = d->next) {
		if (d->flags & D_SET) listrev(d);
	}
}

void
listrev(delta *d)
{
	char	*t;
	int	i;
	char	buf[100];

	assert(d);
	writen(1, "OK-ChangeSet@");
	writen(1, d->rev);
	writen(1, ", ");
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
	writen(1, buf);
	if (d->zone) {
		writen(1, "-");
		writen(1, d->zone);
	}
	writen(1, ", ");
	writen(1, d->user);
	if (d->hostname) {
		writen(1, "@");
		writen(1, d->hostname);
	}
	writen(1, "\n");
	EACH(d->comments) {
		writen(1, "OK-  ");
		writen(1, d->comments[i]);
		writen(1, "\n");
    	}
	writen(1, "OK-\n");
}
