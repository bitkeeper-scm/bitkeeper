/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang
 */

#include "bkd.h"
private int localTrigger(char *, char *, char **);
private int remotePreTrigger(char *, char *, char **);
private int remotePostTrigger(char *, char *, char **);

int put_trigger_env(char *where, char *v, char *value);


/*
 * set up the trigger environment
 */
private void
trigger_env(char *prefix, char *event, char *what)
{
	char	buf[100];

	if (streq("BK", prefix)) {
		put_trigger_env("BK", "SIDE", "client");
	} else {
		put_trigger_env("BK", "SIDE", "server");
		put_trigger_env("BK", "HOST", getenv("_BK_HOST"));
		put_trigger_env("BK", "USER", getenv("_BK_USER"));
	}
	put_trigger_env(prefix, "HOST", sccs_gethost());
	put_trigger_env(prefix, "USER", sccs_getuser());
	put_trigger_env("BK", "EVENT", event);
	putroot(prefix);
	put_trigger_env(prefix, "TIME_T", bk_time);
	put_trigger_env(prefix, "UTC", bk_utc);
	put_trigger_env(prefix, "VERSION", bk_vers);
	sprintf(buf, "%d", getlevel());
	put_trigger_env(prefix, "LEVEL", buf);
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
	char	*event = 0;
	int	len, resolve, rc = 0;
	struct	dirent *e;
	DIR	*dh;

	if (getenv("BK_NO_TRIGGERS")) return (0);

	t = av[0];
	if (strneq(t, "remote pull", 11)) {
		what = "outgoing";
		event = "outgoing pull";
	} else if (strneq(t, "push", 4)) {
		what = "outgoing";
		event = "outgoing push";
	} else if (strneq(t, "remote clone", 12)) {
		what = "outgoing";
		event = "outgoing clone";
	} else if (strneq(t, "remote push", 11)) {
		what = "incoming";
		event = "incoming push";
	} else if (streq(t, "resolve") || streq(t, "remote resolve")) {
		what = event = "resolve";
	} else if (strneq(t, "clone", 5)) {
		/* XXX - can this happen?  Where's the trigger? */
		what = "incoming";
		event = "incoming clone";
	} else if (strneq(t, "pull", 4)) {
		what = "incoming";
		event = "incoming pull";
	} else if (strneq(t, "apply", 5) || strneq(t, "remote apply", 12)) {
		what = event = "apply";
		sprintf(tbuf, "%s/%s", RESYNC2ROOT, triggerDir);
		triggerDir = tbuf;
	} else if (strneq(t, "commit", 6)) {
		what = event = "commit";
	} else if (strneq(t, "remote log push", 15)) {
		/*
		 * logs triggers are global over all logging trees
		 */
		unless (logRoot) return (0);
		triggerDir = tbuf;
		concat_path(triggerDir, logRoot, "triggers");
		what = "incoming-log";
		event = "incoming log";
	} else {
		fprintf(stderr,
			"Warning: Unknown trigger event: %s, ignored\n", av[0]);
		return (0);
	}

	unless (isdir(triggerDir)) return (0);
	sys("bk", "get", "-q", triggerDir, SYS);

	/* Run the incoming trigger in the RESYNC dir if there is one.  */
	resolve = (triggerDir != tbuf) && streq(what, "resolve");
	if (resolve) {
		assert(isdir(ROOT2RESYNC));
	    	chdir(ROOT2RESYNC);
		triggerDir = RESYNC2ROOT "/BitKeeper/triggers";
	}

	/*
	 * Find all the trigger scripts associated with this event.
	 * XXX TODO need to think about the split root case
	 */
	sprintf(buf, "%s-%s", when, what);
	len = strlen(buf);
	unless (dh = opendir(triggerDir)) {
		if (resolve) chdir(RESYNC2ROOT);
		return (0);
	}
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
		if (resolve) chdir(RESYNC2ROOT);
		return (0);
	}

	/*
	 * Sort it and run the triggers
	 */
	unless (getenv("BK_STATUS")) putenv("BK_STATUS=UNKNOWN");
	sortLines(triggers);
	unless (strneq(av[0], "remote ", 7))  {
		rc = localTrigger(event, what, triggers);
	} else if (streq(when, "pre")) {
		rc = remotePreTrigger(event, what, triggers);
	} else 	{
		rc = remotePostTrigger(event, what, triggers);
	}
	freeLines(triggers);
	if (resolve) chdir(RESYNC2ROOT);
	return (rc);
}

private int
runit(char *file, char *output)
{
	int	status, rc, j = 0, fd1, fd;
	char	*my_av[10];
	char	trigger[MAXPATH];

	sprintf(trigger, "BK_TRIGGER=%s", basenm(file));
	putenv(trigger);	/* OK to not dup, transitory */
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
localTrigger(char *event, char *what, char **triggers)
{
	int	i, rc = 0;

	/*
	 * BK/basic does not support client side triggers.
	 */
	if ((bk_mode() == BK_BASIC)) return (0);

	trigger_env("BK", event, what);

	EACH(triggers) {
		unless (runable(triggers[i])) continue;
		rc = runit(triggers[i], 0);
		if (rc) break;
	}
	return (rc);
}

private int
remotePreTrigger(char *event, char *what, char **triggers)
{
	int	i, rc = 0;
	char	output[MAXPATH], buf[MAXLINE];
	FILE	*f;

	/*
	 * BK/basic only supports triggers in master repository.
	 */
	if ((bk_mode() == BK_BASIC) && !exists(BKMASTER)) return (0);

	trigger_env("BKD", event, what);

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
remotePostTrigger(char *event, char *what, char **triggers)
{
	int	fd1, i, rc = 0;

	/*
	 * BK/basic only supports triggers in master repository.
	 */
	if ((bk_mode() == BK_BASIC) && !exists(BKMASTER)) return (0);

	trigger_env("BKD", event, what);
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
