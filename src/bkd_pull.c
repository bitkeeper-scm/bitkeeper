#include "bkd.h"

/*
 * Input:
 *	key
 *	key
 *	...
 *	@END@
 * Output:
 *	patch of anything not in the list
 *
 * Returns - this never returns, always causes the daemon to exit.
 */
int
cmd_pull(int ac, char **av, int in, int out, int err)
{
	static	char *cset[] = { "bk", "cset", "-m", "-", 0 };
	char	buf[4096];
	char	tmpfile[MAXPATH];
	FILE	*f = 0;
	MDBM	*them = 0, *me = 0;
	char	*t;
	int	error = 0;
	int	c, bytes = 0;
	int	doit = 1;
	int	verbose = 1;
	pid_t	pid;
	int	first = 1;
	kvpair	kv;

	if (!exists("BitKeeper/etc")) {
		writen(err, "Not at project root\n");
		exit(1);
	}

	if (exists("RESYNC")) {
		writen(err, "Project is locked for update.\n");
		exit(1);
	}

	while ((c = getopt(ac, av, "nq")) != -1) {
		switch (c) {
		    case 'n': doit = 0; break;
		    case 'q': verbose = 0; break;
	    	}
	}
	
	/*
	 * Get the remote keys
	 */
#define	OUT	{ error = 1; goto out; }
	unless (them = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) OUT;
	while (getline(in, buf, sizeof(buf)) > 0) {
		if (streq("@END@", buf)) break;
		mdbm_store_str(them, buf, "", 0);
	}
	
	/*
	 * Get the local keys
	 */
	unless (f = popen("bk prs -bhad':KEY: :REV:' ChangeSet", "r")) {
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
				writen(out,
				    "Different project, root key mismatch\n");
				fprintf(stderr, "ERROR\n");
				goto out;
			}
			mdbm_store_str(me, t, "", 0);
			bytes += strlen(t) + 1;
		}
		first = 0;
	}
	pclose(f); f = 0;
	unless (bytes) {
		if (doit) writen(out, "Nothing to resync.\n");
		goto out;
	}
	if (doit) {
		if (verbose) writen(err, 
"--------------------- Sending the following csets ---------------------\n");
		gettemp(tmpfile, "push");
		f = fopen(tmpfile, "w");
	} else {
		if (verbose) writen(err,
"-------------------- Would send the following csets -------------------\n");
	}
	bytes = 0;
	for (kv = mdbm_first(me); kv.key.dsize != 0; kv = mdbm_next(me)) {
		if (doit) fprintf(f, "%s\n", kv.key.dptr);
		if (verbose) writen(err, kv.key.dptr);
		bytes += kv.key.dsize;
		if (bytes >= 50) {
			if (verbose) writen(err, "\n");
			bytes = 0;
		} else {
			if (verbose) writen(err, " ");
		}
	}
	if (verbose) writen(err, 
"\n-----------------------------------------------------------------------\n");
	unless (doit) exit(0);
	fclose(f); f = 0;

	/*
	 * What I want is to run cset with stdin being the file.
	 */
	if (pid = fork()) {
		int	status;

		if (pid == -1) {
			perror("fork");
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
	exit(error);
}
