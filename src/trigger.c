/*
 * Copyright (c) 2001-2006 BitMover, Inc.
 */
#include "bkd.h"
private int runTriggers(int rem, char *ev, char *what, char *when, char **trs);

private void
put_trigger_env(char *prefix, char *v, char *value)
{
	unless (value) value = "";
	safe_putenv("%s_%s=%s", prefix, v, value);
}

/*
 * set up the trigger environment
 */
private void
trigger_env(char *prefix, char *event, char *what)
{
	char	buf[100];
	char	*repoid, *lic;

	if (streq("BK", prefix)) {
		put_trigger_env("BK", "SIDE", "client");
		if (lic = licenses_accepted()) {
			safe_putenv("BK_ACCEPTED=%s", lic);
			free(lic);
		}
	} else {
		put_trigger_env("BK", "SIDE", "server");
		put_trigger_env("BK", "HOST", getenv("_BK_HOST"));
		put_trigger_env("BK", "USER", getenv("BK_USER"));
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
	repoid = proj_repoID(0);
	if (repoid) put_trigger_env(prefix, "REPO_ID", repoid);
	put_trigger_env(prefix, "REALUSER", sccs_realuser());
	put_trigger_env(prefix, "REALHOST", sccs_realhost());
	put_trigger_env(prefix, "PLATFORM", platform());
	if (streq(event, "resolve")) {
		char    pwd[MAXPATH];
		FILE    *f = fopen("BitKeeper/tmp/patch", "r");
		char    *p;

		if (f) {
			buf[0] = 0;
			fnext(buf, f);
			fclose(f);
			assert(buf[0]);
			chomp(buf);
			chdir(RESYNC2ROOT);
			getcwd(pwd, sizeof(pwd));
			chdir(ROOT2RESYNC);
			p = aprintf("%s/%s", pwd, buf);
			put_trigger_env("BK", "PATCH", p);
			free(p);
		}
	}
}

/*
 * trigger:  Fire triggers before and/or after repository level commands.
 *
 * Note: Caller must make sure we are at the package root before
 * calling this function.
 */
int
trigger(char *cmd, char *when)
{
	char	buf[MAXPATH], triggerDir[MAXPATH];
	char	*what, *t, **triggers = 0;
	char	*event = 0;
	int	rc = 0;

	if (getenv("BK_SHOW_TRIGGERS")) {
		ttyprintf("TRIGGER cmd(%s) when(%s)\n", cmd, when);
	}

	if (getenv("BK_NO_TRIGGERS")) return (0);
	/*
	 * For right now, if you set this at all, it means /etc.
	 * In the 3.0 tree, we'll actually respect it.
	 */
	if (getenv("BK_TRIGGER_PATH")) {
		t = strdup("/etc");
	} else unless (t = proj_root(0)) {
		ttyprintf("No root for triggers!\n");
		return (0);
	}
	unless (streq(t, ".")) {
		sprintf(triggerDir, "%s/BitKeeper/triggers", t);
	} else {
		strcpy(triggerDir, "BitKeeper/triggers");
	}

	if (strneq(cmd, "remote pull", 11)) {
		what = "outgoing";
		event = "outgoing pull";
	} else if (strneq(cmd, "push", 4)) {
		what = "outgoing";
		event = "outgoing push";
	} else if (strneq(cmd, "remote clone", 12)) {
		what = "outgoing";
		event = "outgoing clone";
	} else if (strneq(cmd, "remote push", 11)) {
		what = "incoming";
		event = "incoming push";
	} else if (strneq(cmd, "remote rclone", 12)) {
		what = "incoming";
		event = "incoming clone";
	} else if (streq(cmd, "resolve") || streq(cmd, "remote resolve")) {
		what = event = "resolve";
	} else if (strneq(cmd, "clone", 5)) {
		/* XXX - can this happen?  Where's the trigger? */
		what = "incoming";
		event = "incoming clone";
	} else if (strneq(cmd, "_rclone", 6)) {
		what = "outgoing";
		event = "outgoing clone";
	} else if (strneq(cmd, "pull", 4)) {
		what = "incoming";
		event = "incoming pull";
	} else if (strneq(cmd, "apply", 5) || strneq(cmd, "remote apply", 12)) {
		what = event = "apply";
		strcpy(triggerDir, RESYNC2ROOT "/BitKeeper/triggers");
	} else if (strneq(cmd, "commit", 6)) {
		what = event = "commit";
	} else if (strneq(cmd, "merge", 5)) {
		what = event = "commit";
		strcpy(triggerDir, RESYNC2ROOT "/BitKeeper/triggers");
	} else if (strneq(cmd, "delta", 5)) {
		what = event = "delta";
	} else if (strneq(cmd, "tag", 3)) {
		what = event = "tag";
	} else if (strneq(cmd, "fix", 3)) {
		what = event = "fix";
	} else if (streq(cmd, "collapse")) {
		what = event = "collapse ";
	} else if (streq(cmd, "lease-proxy")) {
		what = event = cmd;
	} else if (streq(cmd, "undo")) {
		what = event = cmd;
	} else {
		fprintf(stderr,
		    "Warning: Unknown trigger event: %s, ignored\n", cmd);
		return (0);
	}

	unless (isdir(triggerDir)) {
		if (getenv("BK_SHOW_TRIGGERS")) {
			ttyprintf("No trigger dir %s\n", triggerDir);
		}
		return (0);
	}

	/*
	 * XXX - we should see if we need to fork this process before
	 * doing so.  FIXME.
	 */
	sys("bk", "get", "-Sq", triggerDir, SYS);

	/* run post-triggers with a read lock */
	if (streq(when, "post")) repository_downgrade();

	/* post-resolve == post-incoming */
	if (streq(when, "post") && streq(event, "resolve")) what = "incoming";

	/* Run the incoming trigger in the RESYNC dir if there is one.  */
	if (streq(what, "resolve")) {
		strcpy(triggerDir, RESYNC2ROOT "/BitKeeper/triggers");
		assert(isdir(ROOT2RESYNC));
	    	chdir(ROOT2RESYNC);
	}

	/*
	 * Find all the trigger scripts associated with this event.
	 */
	sprintf(buf, "%s-%s", when, what);
	unless (triggers = getTriggers(triggerDir, buf)) {
		if (streq(what, "resolve")) chdir(RESYNC2ROOT);
		if (getenv("BK_SHOW_TRIGGERS")) {
			ttyprintf("No %s triggers in %s \n", buf, triggerDir);
		}
		return (0);
	}

	/*
	 * Run the triggers, they are already sorted by getdir().
	 */
	unless (getenv("BK_STATUS")) putenv("BK_STATUS=UNKNOWN");
	rc = runTriggers(strneq(cmd, "remote ",7), event, what, when, triggers);
	freeLines(triggers, free);
	if (streq(what, "resolve")) chdir(RESYNC2ROOT);
	return (rc);
}

/*
 * returns lines array of all filenames in directory that start with
 * the given prefix.
 * The array is sorted.
 */
char	**
getTriggers(char *dir, char *prefix)
{
	int	len = strlen(prefix);
	char	**files;
	char	**lines = 0;
	int	i;

	files = getdir(dir);
	unless (files) return (0);
	EACH (files) {
		int	flen = strlen(files[i]);
		/* skip files that don't match prefix */
		unless (flen >= len && strneq(files[i], prefix, len)) continue;
		/* skip emacs backup files */
		if (files[i][flen - 1] == '~') continue;
		lines = addLine(lines, aprintf("%s/%s", dir, files[i]));
	}
	freeLines(files, free);
	return (lines);
}

private int
runit(char *file, char *what, char *output)
{
	int	status, rc;
	char	*root = streq(what, "resolve") ? RESYNC2ROOT : ".";
	char	*path = strdup(getenv("PATH"));

	safe_putenv("BK_TRIGGER=%s", basenm(file));
	write_log(root, "cmd_log", 0, "Running trigger %s", file);
	if (getenv("BK_SHOW_TRIGGERS")) ttyprintf("Running trigger %s\n", file);
	safe_putenv("PATH=%s", getenv("BK_OLDPATH"));

	status = sysio(0, output, output, file, SYS);

	safe_putenv("PATH=%s", path);
	free(path);
	if (WIFEXITED(status)) {
		rc = WEXITSTATUS(status);
	} else {
		rc = 100;
	}
	write_log(root, "cmd_log", 0, "Trigger %s returns %d", file, rc);
	if (getenv("BK_SHOWPROC") || getenv("BK_SHOW_TRIGGERS")) {
		ttyprintf("TRIGGER %s => %d\n", basenm(file), rc);
	}
	return (rc);
}

/*
 * This function is called by client side to process the TRIGGER INFO block
 * sent by run_bkd_trigger() above.
 * If the trigger exited non-zero we always print if there is something to say.
 */
int
getTriggerInfoBlock(remote *r, int verbose)
{
	int	i = 0, rc = 0;
	char	**lines = 0;
	char	buf[4096];

	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (buf[0] == BKD_DATA) {
			lines = addLine(lines, strdup(&buf[1]));
		} else if (buf[0] == BKD_RC) {
			rc = atoi(&buf[1]);
			if (rc && r->trace) {
				fprintf(stderr, "trigger failed rc=%d\n", rc);
			}
		}
		if (streq(buf, "@END@")) break;
	}
	/* Nothing to say */
	unless (lines) goto out;
	/* if 0 exit status and not verbose goto out */
	if (!rc && !verbose) goto out;
	fprintf(stderr, "------------------------- "
	    "Remote trigger message --------------------------\n");
	EACH(lines) fprintf(stderr, "%s\n", lines[i]);
	fprintf(stderr, "--------------------------"
	    "-------------------------------------------------\n");
out:	freeLines(lines, free);
	return (rc);
}

/*
 * This is called for both local triggers and remote triggers.
 * The remote stuff has to be wrapped so that gets a little complicated.
 * lclone looks like it is remote but doesn't go through a bkd so it is
 * actually handled like it is local.
 * We send all trigger output to a file so we can funnel it where we want.
 */
private int
runTriggers(int remote, char *event, char *what, char *when, char **triggers)
{
	int	i, quiet, proto;
	int	rc = 0;
	char	*bkd_data, *trigger, *h, *p;
	FILE	*f, *out;
	FILE	*gui = 0, *logfile = 0;
	char	output[MAXPATH], buf[MAXLINE];

	trigger_env(remote ? "BKD" : "BK", event, what);

	/*
	 * If we are a remote pre trigger then we wrap the protocol around
	 * the output unless it is lclone.
	 */
	proto = remote && streq(when, "pre") && !getenv("_BK_LCLONE");
	if (proto) {
	    	bkd_data = "D";
		out = stdout;
	} else {
		bkd_data = "";
		if (p = getenv("_BKD_LOGFILE")) {
			logfile = out = fopen(p, "a");
		} else {
			out = stderr;
		}
	}

	/* Send status for citool */
	if ((h = getenv("_BK_TRIGGER_SOCK")) && (p = strchr(h, ':'))) {
		*p++ = 0;
		if ((i = tcp_connect(h, atoi(p))) != -1) {
			gui = fdopen(i, "w");
			setlinebuf(gui);
			out = stdout;
			setlinebuf(out);
		}
	}

	/* pull|commit -q set BK_QUIET_TRIGGERS for us */
	quiet = getenv("BK_QUIET_TRIGGERS") && !gui;

	if (proto) fputs("@TRIGGER INFO@\n", out);

	bktmp(output, "trigger");
	EACH(triggers) {
		// XXX - warn them about permissions?
		unless (executable(triggers[i])) continue;

		trigger = basenm(triggers[i]);

		if (gui) fprintf(gui, "Running trigger \"%s\"\n", trigger);
		rc = runit(triggers[i], what, output);

		unless (rc || size(output)) continue;

		/* allow people to surpress noise (like getTriggerInfobLock) */
		if (quiet && (rc == 0)) continue;

		fprintf(out, "%s>> Trigger \"%s\"", bkd_data, trigger);
		if (rc) {
			fprintf(out, " (exit status was %d)", rc);
			if (gui) {
				fprintf(gui,
				    "\t%s failed - exited %d\n", trigger, rc);
		    	}
		}
		fprintf(out, "\n");
		f = fopen(output, "rt");
		assert(f);
		while (fnext(buf, f)) {
			fprintf(out, "%s%s", bkd_data, buf);
		}
		fclose(f);

		/* Stop on first failing pre- trigger */
		if (rc && streq(when, "pre")) {
			if (proto) fprintf(out, "%c%d\n", BKD_RC, rc);
			break;
		}
	}
	if (proto) fputs("@END@\n", out);
	fflush(out);
	if (gui) fclose(gui);	/* citool socket */
	unlink(output);
	if (logfile) fclose(logfile);
	return (rc);
}
