/*
 * Copyright (c) 2000, David Parsons & Larry McVoy & Andrew Chang
 */

#include "bkd.h"
private int run_client_trigger(char **, char *, char *, char *);
private int run_bkd_pre_trigger(char **, char *, char *, char *);
private int run_bkd_post_trigger(char **, char *, char *, char *);

/*
 * trigger:  Fire triggers before and/or after repository level commands.
 *
 * Note: Caller must make sure we are at the package root before
 * calling this function.
 */
int
trigger(char **av, char *when, int status)
{
	char	*what, *var, *t, **triggers = 0, buf[MAXPATH*4], file[MAXPATH];
	char	*triggerDir = TRIGGERS, tbuf[MAXPATH];
	int	i, len, rc = 0;
	struct	dirent *e;
	DIR	*dh;

	t = av[0];

	if (strneq(t, "remote pull", 11) || strneq(t, "push", 4) ||
	    strneq(t, "remote clone", 12)) {
		what = "outgoing";
		var = "BK_OUTGOING";
	} else if (
	    strneq(t, "remote push", 11) || strneq(t, "clone", 5) ||
	    strneq(t, "pull", 4)) {
		what = "incoming";
		var = "BK_INCOMING";
	} else if (strneq(t, "commit", 6)) {
		what = "commit";
		var = "BK_COMMIT";
	} else if (strneq(t, "remote log push", 15)) {
		/*
		 * logs triggers are global over all logging trees
		 */
		unless (logRoot) return (0);
		triggerDir = tbuf;
		concat_path(tbuf, logRoot, "triggers");
		what = "incoming-log";
		var = "BK_INCOMING_LOG";
	} else {
		fprintf(stderr,
			"Warning: Unknown trigger event: %s, ignored\n", av[0]);
		return (0);
	}

	if ((bk_mode() == BK_BASIC) && !strneq("commit", t, 6)) return (0);
	if (status) {
		sprintf(buf, "%s=ERROR %d", var, status);
		putenv((strdup)(buf));
	} else unless (getenv(var)) {
		sprintf(buf, "%s=OK", var);
		putenv((strdup)(buf));
	}

	sprintf(buf, "%s:%s", sccs_gethost(), fullname(".", 0));
	for (len = 1, i = 0; av[i]; i++) {
		len += strlen(av[i]) + 1;
		if (len >= sizeof(buf)) continue;
		strcat(buf, " ");
		strcat(buf, av[i]);
	}
	unless (isdir(triggerDir)) return (0);
	sys("bk", "get", "-q", triggerDir, SYS);
	sprintf(file, "%s-%s", when, what);
	len = strlen(file);

	/*
	 * Find all the trigger scripts associated with this event.
	 * XXX TODO need to think about the spilt root case
	 */
	dh = opendir(triggerDir);
	assert(dh);
	while ((e = readdir(dh)) != NULL) {
		if ((strlen(e->d_name) >= len) &&
		    strneq(e->d_name, file, len)) {
			sprintf(file, "%s/%s",  triggerDir, e->d_name);
			triggers = addLine(triggers, strdup(file));
		}
	}
	closedir(dh);
	unless (triggers) return (0);

	/*
	 * Sort it and run the triggers
	 */
	sortLines(triggers);
	unless (strneq(t, "remote ", 7))  {
		rc = run_client_trigger(triggers, when, what, buf);
	} else if (streq(when, "pre")) {
		rc = run_bkd_pre_trigger(triggers, when, what, buf);
	} else 	{
		rc = run_bkd_post_trigger(triggers, when, what, buf);
	}
	freeLines(triggers);
	return (rc);
}

private int
runable(char *file)
{
#ifdef	WIN32
	return (1);
#else
	unless (access(file, X_OK) == 0) {
		fprintf(stderr, "Warning: %s is not executable, "
				"skipped\n", file);
		return (0);
	}
	return (1);
#endif
}

private int
runit(char *file, char *when, char *event, char *output)
{
	int	status, rc;
#ifdef	WIN32
		char *p;

		p = strrchr(file, '.');
		/*
		 * If no suffix, assumes it is a shell script
		 * so feed it to the bash shell
		 */
		unless (p) {
			status = sysio(0, output, 0,
					"bash", "-c", file, when, event, SYS);
		} else {
			/*
			 * XXX TODO: If suffix is .pl or .perl, run the perl
			 * interpretor. Win32 ".bat" file should just work
			 * with no special handling.
			 */
			status = sysio(0, output, 0, file, when, event, SYS);
		}
#else
		status = sysio(0, output, 0, file, when, event, SYS);
#endif
		rc = WEXITSTATUS(status);
		return (rc);
}

/*
 * Both pre and post client triggers are handled here
 */
private int
run_client_trigger(char **triggers, char *when, char *what, char *event)
{
	int	i, rc = 0;

	EACH(triggers) {
		unless (runable(triggers[i])) continue;
		rc = runit(triggers[i], when, event, 0);
		if (rc) break;
	}
	return (rc);
}

private int
run_bkd_pre_trigger(char **triggers, char *when, char *what, char *event)
{
	int	i, rc = 0, status, first = 1;
	char	output[MAXPATH], buf[MAXLINE];
	FILE	*f;

	/*
	 * WIN32 note: We must issue the waitpid() call before the
	 * process exit. Otherwise we loose the process exit status.
	 * This means using popen() does not work on NT, you almost always
	 * get back a undefined status when you a call pclose();
	 * Note: waitpid() is hiden inside sysio() which is called from runit();
	 * One way to fix the NT popen/pclose implementation is to
	 * create a thread at popen time to wait for child exit status.
	 * This is not done yet.
	 */
	gettemp(output, "trigger");
	EACH(triggers) {
		unless (runable(triggers[i])) continue;
		rc = runit(triggers[i], when, event, output);
		f = fopen(output, "rt");
		assert(f);
		while (fnext(buf, f)) {
			if (first) {
				first = 0;
				fputs("@TRIGGER INFO@\n", stdout);
			}
			printf("%c%s", BKD_DATA, buf);
		}
		fclose(f);
		if (rc) break;
	}
	unless (first) {
		printf("%c%d\n", BKD_RC, rc);
		fputs("@END@\n", stdout);
		fflush(stdout);
	}
	unlink(output);
	return (rc);
}

private int
run_bkd_post_trigger(char **triggers, char *when, char *what, char *event)
{
	int	fd1, i, rc = 0;

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
		rc = runit(triggers[i], when, event, 0);
		if (rc) break;
	}
	dup2(fd1, 1); close(fd1);
	return (rc);
}
