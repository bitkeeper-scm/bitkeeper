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
	char	*what, *var, *t, **lines = 0, buf[MAXPATH*4], file[MAXPATH];
	int	i, len, rc = 0;
	struct	dirent *e;
	DIR	*dh;

	t = av[0];

	if (strneq(t, "remote pull", 11) || strneq(t, "push", 4) ||
	    strneq(t, "clone", 5) || strneq(t, "remote clone", 12)) {
		what = "outgoing";
		var = "BK_OUTGOING";
	} else if (
	    strneq(t, "remote push", 11) ||
	    strneq(t, "pull", 4)) {
		what = "incoming";
		var = "BK_INCOMING";
	} else if (strneq(t, "commit", 6)) {
		what = "commit";
		var = "BK_COMMIT";
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
	unless (isdir(TRIGGERS)) return (0);
	sys("bk", "get", "-q", TRIGGERS, SYS);
	sprintf(file, "%s-%s", when, what);
	len = strlen(file);

	/*
	 * Find all the trigger scripts associated with this event.
	 * XXX TODO need to think about the spilt root case
	 */
	dh = opendir(TRIGGERS);
	assert(dh);
	while ((e = readdir(dh)) != NULL) {
		if ((strlen(e->d_name) >= len) &&
		    strneq(e->d_name, file, len)) {
			lines = addLine(lines, strdup(e->d_name));
		}
	}
	closedir(dh);
	unless (lines) return (0);

	/*
	 * Sort it and run the triggers
	 */
	sortLines(lines);
	unless (strneq(t, "remote ", 7))  {
		rc = run_client_trigger(lines, when, what, buf);
	} else if (streq(when, "pre")) {
		rc = run_bkd_pre_trigger(lines, when, what, buf);
	} else 	{
		rc = run_bkd_post_trigger(lines, when, what, buf);
	}
	freeLines(lines);
	return (rc);
}

private int
makeTriggerCmd(char *tscript, char *when, char *what, char *event, char *tcmd)
{
	char	file[MAXPATH];

	sprintf(file, "%s/%s", TRIGGERS, tscript);
#ifdef	WIN32
	sprintf(tcmd, "bash -c \"%s %s %s\"", file, when, event);
#else
	unless (access(file, X_OK) == 0) {
		fprintf(stderr,
			"Warning: %s is not executable, skipped\n", file);
		return (1);
	}
	sprintf(tcmd, "%s %s %s", file, when, event);
#endif
	return (0);
}

private int
run_client_trigger(char **lines, char *when, char *what, char *event)
{
	int	i, rc = 0;
	char	cmd[MAXPATH*4];

	EACH(lines) {
		if (makeTriggerCmd(lines[i], when, what, event, cmd)) continue;
		rc = system(cmd);
		if (rc) break;
	}
	return (rc);
}

private int
run_bkd_pre_trigger(char **lines, char *when, char *what, char *event)
{
	int	i, rc = 0, status, first = 1;
	char	cmd[MAXPATH*4];
	FILE	*f;

	EACH(lines) {
		if (makeTriggerCmd(lines[i], when, what, event, cmd)) continue;
		f = popen(cmd, "r");
		assert(f);
		while (fnext(cmd, f)) {
			if (first) {
				first = 0;
				fputs("@TRIGGER INFO@\n", stdout);
			}
			printf("%c%s", BKD_DATA, cmd);
		}
		status = pclose(f);
		rc = WEXITSTATUS(status);
		if (rc) break;
	}
	unless (first) {
		printf("%c%d\n", BKD_RC, rc);
		fputs("@END@\n", stdout);
		fflush(stdout);
	}
	return (rc);
}

private int
run_bkd_post_trigger(char **lines, char *when, char *what, char *event)
{
	int	fd1, i, rc = 0;
	char	cmd[MAXPATH*4];

	/*
	 * Process post trigger for remote client
	 *
	 * Post trigger output is not sent to remote client
	 * So redirect output to stderr
	 */
	fflush(stdout);
	fd1 = dup(1); assert(fd1 > 0);
	if (dup2(2, 1) < 0) perror("trigger: dup2");
	EACH(lines) {
		if (makeTriggerCmd(lines[i], when, what, event, cmd)) continue;
		rc = system(cmd);
		if (rc) break;
	}
	dup2(fd1, 1); close(fd1);
	return (rc);
}
