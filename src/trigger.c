/*
 * Copyright (c) 2000, David Parsons & Larry McVoy & Andrew Chang
 */

#include "bkd.h"
private int localTrigger(char **);
private int remotePreTrigger(char **);
private int remotePostTrigger(char **);

int put_trigger_env(char *where, char *v, char *value);


/*
 * set up the trigger environment
 */
private void
trigger_env(char *what)
{
	char buf[2*MAXPATH];
	char *direction;

	if (strneq(what, "incoming", 8))
		direction = "_INCOMING";
	else if (strneq(what, "outgoing", 8))
		direction = "_OUTGOING";
	else
		direction = "";

	putroot(direction);
	put_trigger_env(direction, "HOST", sccs_gethost());
	put_trigger_env(direction, "USER", sccs_getuser());
	put_trigger_env(direction, "TIME_T", bk_time);
	put_trigger_env(direction, "UTC", bk_utc);
	put_trigger_env(direction, "VERSION", bk_vers);
}


/*
 * trigger:  Fire triggers before and/or after repository level commands.
 *
 * Note: Caller must make sure we are at the package root before
 * calling this function.
 */
int
trigger(char **av, char *when)
{
	char	buf[MAXPATH], tbuf[MAXPATH];
	char	*what, *t, **triggers = 0;
	char	*triggerDir = "BitKeeper/triggers";
	int	len, resync, rc = 0;
	struct	dirent *e;
	DIR	*dh;

	t = av[0];
	if (strneq(t, "remote pull", 11)) {
		what = "outgoing";
		putenv("BK_EVENT=outgoing pull");
	} else if (strneq(t, "push", 4)) {
		what = "outgoing";
		putenv("BK_EVENT=outgoing push");
	} else if (strneq(t, "remote clone", 12)) {
		what = "outgoing";
		putenv("BK_EVENT=outgoing clone");
	} else if (strneq(t, "remote push", 11)) {
		what = "incoming";
		putenv("BK_EVENT=incoming push");
	} else if (streq(t, "resolve") || streq(t, "remote resolve")) {
		what = "resolve";
		putenv("BK_EVENT=resolve");
	} else if (strneq(t, "clone", 5)) {
		/* XXX - can this happen?  Where's the trigger? */
		what = "incoming";
		putenv("BK_EVENT=incoming clone");
	} else if (strneq(t, "pull", 4)) {
		what = "incoming";
		putenv("BK_EVENT=incoming pull");
	} else if (strneq(t, "commit", 6)) {
		what = "commit";
		putenv("BK_EVENT=commit");
	} else if (strneq(t, "remote log push", 15)) {
		/*
		 * logs triggers are global over all logging trees
		 */
		unless (logRoot) return (0);
		triggerDir = tbuf;
		concat_path(triggerDir, logRoot, "triggers");
		what = "incoming-log";
		putenv("BK_EVENT=incoming log");
	} else {
		fprintf(stderr,
			"Warning: Unknown trigger event: %s, ignored\n", av[0]);
		return (0);
	}

	unless (isdir(triggerDir)) return (0);
	sys("bk", "get", "-q", triggerDir, SYS);

	/* Run the incoming trigger in the RESYNC dir if there is one.  */
	resync = (triggerDir != tbuf) && streq(what, "resolve");
	if (resync) {
		assert(isdir(ROOT2RESYNC));
	    	chdir(ROOT2RESYNC);
		triggerDir = RESYNC2ROOT "/BitKeeper/triggers";
	}

	/*
	 * Find all the trigger scripts associated with this event.
	 * XXX TODO need to think about the spilt root case
	 */
	sprintf(buf, "BK_TRIGGER=%s-%s", when, what);
	putenv((strdup)(buf));
	sprintf(buf, "%s-%s", when, what);
	len = strlen(buf);
	dh = opendir(triggerDir);
	assert(dh);
	while (e = readdir(dh)) {
		if ((strlen(e->d_name) >= len) &&
		    strneq(e->d_name, buf, len)) {
			char	file[MAXPATH];

			sprintf(file, "%s/%s",  triggerDir, e->d_name);
			triggers = addLine(triggers, strdup(file));
		}
	}
	closedir(dh);
	unless (triggers) {
		if (resync) chdir(RESYNC2ROOT);
		return (0);
	}

	/*
	 * Stuff some more useful crud in the environment.
	 */
	trigger_env(what);

	/*
	 * Sort it and run the triggers
	 */
	sortLines(triggers);
	unless (strneq(av[0], "remote ", 7))  {
		rc = localTrigger(triggers);
	} else if (streq(when, "pre")) {
		rc = remotePreTrigger(triggers);
	} else 	{
		rc = remotePostTrigger(triggers);
	}
	freeLines(triggers);
	if (resync) chdir(RESYNC2ROOT);
	return (rc);
}

private int
runit(char *file, char *output)
{
	int	status, rc, j = 0, fd1, fd;
	char	*my_av[10];

	if (output) {
		fd1 = dup(1); close(1); 
		fd = open(output, O_CREAT|O_TRUNC|O_WRONLY, 0666);
		assert(fd == 1);
	}

	my_av[j++] = file;
	my_av[j] = 0;
	status = spawnvp_ex(_P_WAIT, my_av[0], my_av);
	if (output) {
		close(1);
		dup2(fd1, 1);
		close(fd1);
	}
	if (WIFEXITED(status)) {
		rc = WEXITSTATUS(status);
	} else {
		rc = 100;
	}
	return (rc);
}

/*
 * Both pre and post client triggers are handled here
 */
private int
localTrigger(char **triggers)
{
	int	i, rc = 0;

	/*
	 * BK/basic does not support client side trigger
	 */
	if ((bk_mode() == BK_BASIC)) return (0);

	EACH(triggers) {
		unless (runable(triggers[i])) continue;
		rc = runit(triggers[i], 0);
		if (rc) break;
	}
	return (rc);
}

private int
remotePreTrigger(char **triggers)
{
	int	i, rc = 0;
	char	output[MAXPATH], buf[MAXLINE];
	FILE	*f;

	/*
	 * BK/basic only support bkd trigger on master repository
	 */
	if ((bk_mode() == BK_BASIC) && !exists(BKMASTER)) return (0);

	gettemp(output, "trigger");
	fputs("@TRIGGER INFO@\n", stdout);
	EACH(triggers) {
		unless (runable(triggers[i])) continue;
		rc = runit(triggers[i], output);
		f = fopen(output, "rt");
		assert(f);
		while (fnext(buf, f)) {
			printf("%c%s", BKD_DATA, buf);
		}
		fclose(f);
		if (rc) break;
	}
	printf("%c%d\n", BKD_RC, rc);
	fputs("@END@\n", stdout);
	fflush(stdout);
	unlink(output);
	return (rc);
}
/*
 * This function is called by client side to process the TRIGGER INFO block
 * sent by run_bkd_trigger() above
 */
int
getTriggerInfoBlock(remote *r, int verbose)
{
	char	buf[4096];
	int	i =0, rc = 0;

	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (buf[0] == BKD_DATA) {
			unless (i++) {
				if (verbose) fprintf(stderr,
"------------------------- Remote trigger message --------------------------\n"
				);
			}
			if (verbose) fprintf(stderr, "%s\n", &buf[1]);
		} else if (buf[0] == BKD_RC) {
			rc = atoi(&buf[1]);
			if (rc && r->trace) {
				fprintf(stderr, "trigger failed rc=%d\n", rc);
			}
		}
		if (streq(buf, "@END@")) break;
	}
	if (i && verbose) {
		fprintf(stderr, 
"---------------------------------------------------------------------------\n"
		);
	}
	return (rc);
}

private int
remotePostTrigger(char **triggers)
{
	int	fd1, i, rc = 0;

	/*
	 * BK/basic only support bkd trigger on master repository
	 */
	if ((bk_mode() == BK_BASIC) && !exists(BKMASTER)) return (0);

	/*
	 * Process post trigger for remote client
	 *
	 * Post trigger output is not sent to remote client
	 * So redirect output to stderr
	 */
	fflush(stdout);
	fd1 = dup(1); assert(fd1 > 0);
	if (dup2(2, 1) < 0) perror("trigger: dup2");
	EACH(triggers) {
		unless (runable(triggers[i])) continue;
		rc = runit(triggers[i], 0);
		if (rc) break;
	}
	dup2(fd1, 1); close(fd1);
	return (rc);
}
