/*
 * Copyright 2000-2013,2015-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bkd.h"
#include "cfg.h"

private int	runTriggers(int rem, char *ev, char *what, char *when,
		    char **trs);
private void	trigger_env(char *prefix, char *event, char *what);
private void	trigger_putenv(char *prefix, char *v, char *value);
private void	trigger_restoreEnv(void);
private char	**trigger_dirs(void);
private	char	**getTriggers(char **lines, char *dir, char *prefix);

private	int	dryrun;	/* see if there are triggers to run */

/*
 * trigger:  Fire triggers before and/or after repository level commands.
 *
 * Note: Caller must make sure we are at the package root before
 * calling this function.
 *
 * 3 similar variables and what they mean plus an example:
 * cmd - set by the external world: remote pull
 * what - the name of the trigger file to check out: {pre|post}-outgoing
 * event - paased to trigger as BK_EVENT: outgoing pull
 */
int
trigger(char *cmd, char *when)
{
	char	*what, *root, **triggers = 0, **dirs = 0;
	char	*event = 0;
	int	i;
	int	use_enclosing = 0, rc = 0;
	FILE	*f = 0;
	char	buf[MAXPATH], triggerDir[MAXPATH];

	if (getenv("BK_NO_TRIGGERS")) return (0);


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
		what = "incoming";
		event = "incoming clone";
	} else if (strneq(cmd, "_rclone", 6)) {
		what = "outgoing";
		event = "outgoing clone";
	} else if (strneq(cmd, "pull", 4)) {
		what = "incoming";
		event = "incoming pull";
	} else if (streq(cmd, "port")) {
		what = "incoming";
		event = "incoming port";
	} else if (strneq(cmd, "apply", 5) || strneq(cmd, "remote apply", 12)) {
		what = event = "apply";
		use_enclosing = 1;
	} else if (strneq(cmd, "commit", 6)) {
		what = event = "commit";
	} else if (strneq(cmd, "merge", 5)) {
		what = event = "commit";
		use_enclosing = 1;
	} else if (strneq(cmd, "delta", 5)) {
		what = event = "delta";
	} else if (strneq(cmd, "tag", 3)) {
		what = event = "tag";
	} else if (strneq(cmd, "fix", 3)) {
		what = event = "fix";
	} else if (streq(cmd, "collapse")) {
		what = event = "collapse";
	} else if (streq(cmd, "undo")) {
		what = event = cmd;
	} else if (streq(cmd, "remote nested")) {
		/* XXX: is this right?? */
		what = "incoming";
		event = "incoming push";
	} else {
		fprintf(stderr,
		    "Warning: Unknown trigger event: %s, ignored\n", cmd);
		return (0);
	}


	/* post-resolve == post-incoming */
	if (streq(when, "post") && streq(event, "resolve")) what = "incoming";
	sprintf(buf, "%s-%s", when, what);

	if (streq(what, "resolve")) {
		/*
		 * Run the resolve triggers in the RESYNC dir if there is one.
		 */
		assert(isdir(ROOT2RESYNC) && !use_enclosing);
		chdir(ROOT2RESYNC);
		use_enclosing = 1;
	}

	unless (root = proj_root(0)) {
		//ttyprintf("No root for triggers!\n");
		goto out;
	}

	unless (dirs = trigger_dirs()) goto out;

	/* run post-triggers with a read lock */
	if (streq(when, "post")) repository_downgrade(0);

	f = efopen("BK_SHOW_TRIGGERS");
	EACH (dirs) {
		if (streq(dirs[i], "|skip|")) continue;

		/* the use_enclosing ones must be called at project root */
		unless (streq(dirs[i], ".")) {
			sprintf(triggerDir, "%s/BitKeeper/triggers", dirs[i]);
		} else if (use_enclosing) {
			sprintf(triggerDir,
			    "%s/%s/BitKeeper/triggers", root, RESYNC2ROOT);
		} else {
			sprintf(triggerDir, "%s/BitKeeper/triggers", root);
		}
		unless (isdir(triggerDir)) {
			continue;
		} else if (f) {
			fprintf(f, "TRIGGER what(%s) when(%s) where(%s)", what, when, proj_cwd());
			unless (streq(dirs[i], ".")) {
				fprintf(f, " dir(%s)", dirs[i]);
			}
			fprintf(f, "\n");
		}

		/*
		 * XXX - we should see if we need to fork this process before
		 * doing so.  FIXME.
		 */
		sys("bk", "get", "-Sq", triggerDir, SYS);

		/*
		 * Find all the trigger scripts associated this dir/event.
		 */
		triggers = getTriggers(triggers, triggerDir, buf);
	}
	if (dryrun) {
		rc = (triggers != 0);
	} else if (triggers) {
		/*
		 * Run the triggers, they are already sorted by getdir().
		 * Run the resolve triggers in the RESYNC dir if there is one.
		 */
		unless (getenv("BK_STATUS")) putenv("BK_STATUS=UNKNOWN");
		rc = runTriggers(
		    strneq(cmd, "remote ",7), event, what, when, triggers);
		freeLines(triggers, free);
	} else {
		if (f) {
			fprintf(f, "No %s triggers found\n", buf);
		}
	}
out:	freeLines(dirs, free);
	if (streq(what, "resolve") && use_enclosing) chdir(RESYNC2ROOT);
	if (f) fclose(f);
	return (rc);
}

int
hasTriggers(char *cmd, char *when)
{
	int	rc;

	unless (proj_root(0)) return (0);
	dryrun = 1;
	rc = trigger(cmd, when);
	dryrun = 0;
	return (rc);
}

/*
 * returns lines array of all filenames in directory that start with
 * the given prefix.
 * The array is sorted.
 */
private	char	**
getTriggers(char **lines, char *dir, char *prefix)
{
	int	len = strlen(prefix);
	char	**files;
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
	char	*path = strdup(getenv("PATH"));

	safe_putenv("BK_TRIGGER=%s", basenm(file));
	safe_putenv("BK_TRIGGERPATH=%s", file);
	write_log("cmd_log", "Running trigger %s", file);
	safe_putenv("PATH=%s", getenv("BK_OLDPATH"));

	status = sysio(0, output, output, file, SYS);

	safe_putenv("PATH=%s", path);
	free(path);
	if (WIFEXITED(status)) {
		rc = WEXITSTATUS(status);
	} else {
		rc = 100;
	}
	write_log("cmd_log", "Trigger %s returns %d", file, rc);
	if (getenv("BK_SHOW_TRIGGERS")) {
		efprintf("BK_SHOW_TRIGGERS", "Trigger %s = %d\n", file, rc);
	} else if (getenv("BK_SHOWPROC")) {
		ttyprintf("TRIGGER %s => %d\n", file, rc);
	}
	return (rc);
}

/*
 * This function is called by client side to process the TRIGGER INFO block
 * sent by run_bkd_trigger() above.
 * If the trigger exited non-zero we always print if there is something to say.
 */
int
getTriggerInfoBlock(remote *r, int quiet)
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
	/* if 0 exit status and quiet goto out */
	if (!rc && quiet) goto out;
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
 * We send all trigger output to a file so we can funnel it where we want.
 */
private int
runTriggers(int remote, char *event, char *what, char *when, char **triggers)
{
	int	i, quiet, proto, len;
	int	rc = 0;
	char	*bkd_data, *trigger, *h, *p;
	FILE	*f, *out;
	FILE	*gui = 0, *logfile = 0;
	char	output[MAXPATH], buf[MAXLINE];

	/* pull|commit -q set BK_QUIET_TRIGGERS for us */
	quiet = (p = getenv("BK_QUIET_TRIGGERS")) && streq(p, "YES") && !gui;

	trigger_env(remote ? "BKD" : "BK", event, what);

	/*
	 * If we are a remote pre trigger then we wrap the protocol around
	 * the output.
	 */
	proto = remote && streq(when, "pre");

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

	if (proto) fputs("@TRIGGER INFO@\n", out);

	bktmp(output);
	EACH(triggers) {
		// XXX - warn them about permissions?
		unless (executable(triggers[i])) continue;

		trigger = basenm(triggers[i]);

		if (gui) fprintf(gui, "Running trigger \"%s\"\n", trigger);
		rc = runit(triggers[i], what, output);

		/* ignore exit status from 'post' triggers */
		if (rc && streq(when, "post")) rc = 0;

		unless (rc || size(output)) continue;

		/* allow people to surpress noise (like getTriggerInfobLock) */
		if (quiet && (rc == 0)) continue;

		if (out == stdout) progress_injectnl();
		fprintf(out, "%s>> Trigger \"%s\"", bkd_data, trigger);
		if (strneq("pre-delta", trigger, 9) && getenv("BK_FILE")) {
			fprintf(out, " on \"%s\"", getenv("BK_FILE"));
		}
		if (rc) {
			fprintf(out, " (exit status was %d)", rc);
			if (gui) {
				fprintf(gui,
				    "\t%s failed - exited %d\n", trigger, rc);
		    	}
		}
		fprintf(out, "\n");
		/*
		 * Caution: tricky stuff: if newline is output fputs style
		 * instead of in buf, regressions can fail because of
		 * bkd trigger stuff interleaved with takepatch output.
		 * See XXX in t.triggers / Pull w/trivial triggers
		 * More caution: receiving side has a 4096 line, and
		 * so we need a newline for each one of those.  So break
		 * up long lines with newlines if going over the wire.
		 */
		f = fopen(output, "rt");
		assert(f);
		while (fgets(buf, (sizeof(buf)-1), f)) {
			if (proto) {
				len = strlen(buf);
				if (buf[len-1] != '\n') {
					buf[len] = '\n';
					buf[len+1] = 0;
				}
			}
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
	trigger_restoreEnv();
	return (rc);
}

/*
 * set up the trigger environment
 */
private void
trigger_env(char *prefix, char *event, char *what)
{
	char	buf[100];
	char	*repoid;

	if (streq("BK", prefix)) {
		trigger_putenv("BK", "SIDE", "client");
	} else {
		trigger_putenv("BK", "SIDE", "server");
		trigger_putenv("BK", "HOST", getenv("_BK_HOST"));
		trigger_putenv("BK", "USER", getenv("BK_USER"));
	}
	trigger_putenv(prefix, "ROOT", proj_root(0));
	trigger_putenv(prefix, "HOST", sccs_gethost());
	trigger_putenv(prefix, "USER", sccs_getuser());
	trigger_putenv("BK", "EVENT", event);
	putroot(prefix);
	trigger_putenv(prefix, "TIME_T", bk_time);
	trigger_putenv(prefix, "UTC", bk_utc);
	trigger_putenv(prefix, "VERSION", bk_vers);
	sprintf(buf, "%d", getlevel());
	trigger_putenv(prefix, "LEVEL", buf);
	repoid = proj_repoID(0);
	if (repoid) trigger_putenv(prefix, "REPO_ID", repoid);
	if (proj_isProduct(0)) {
		trigger_putenv(prefix, "REPO_TYPE", "product");
	} else if (proj_isComponent(0)) {
		trigger_putenv(prefix, "REPO_TYPE", "component");
	} else {
		trigger_putenv(prefix, "REPO_TYPE", "standalone");
	}
	trigger_putenv(prefix, "REALUSER", sccs_realuser());
	trigger_putenv(prefix, "REALHOST", sccs_realhost());
	trigger_putenv(prefix, "PLATFORM", platform());
	if (streq(what, "resolve")) {
		char    pwd[MAXPATH];
		FILE    *f = fopen("BitKeeper/tmp/patch", "r");
		char    *p;

		buf[0] = 0;
		if (f) {
			fnext(buf, f);
			fclose(f);
		}
		if (buf[0]) {	/* if not null patch like poly_pull() */
			chomp(buf);
			chdir(RESYNC2ROOT);
			strcpy(pwd, proj_cwd());
			chdir(ROOT2RESYNC);
			p = aprintf("%s/%s", pwd, buf);
			trigger_putenv("BK", "PATCH", p);
			free(p);
		}
	}

	/*
	 * clear some values from the environment that we don't want
	 * sent to triggers.
	 */
	trigger_putenv("BK", "QUIET_TRIGGERS", 0);
}

static  char    **backup_env = 0;

/*
* Add a value to the environment for use by trigger scripts.  Also
* maintain a list of changes to the enviroment so that they can be
* undone when we are done running triggers.  We do this so that some
* variables from one trigger don't spill over to another when the
* more than one trigger is run by the same process.
* Also since we cannot portabilty delete variables from the enviroment
* we just NULL them on restore if they didn't exist previously.
*/
private void
trigger_putenv(char *prefix, char *v, char *value)
{
	char	*var = aprintf("%s_%s", prefix, v);
	char	*old = getenv(var);

	unless (value) value = "";
	if (old && streq(old, value)) {
		free(var);
		return;
	}

	if (old) {
		backup_env = addLine(backup_env, strdup(old - strlen(var) - 1));
	} else {
		backup_env = addLine(backup_env, aprintf("%s=", var));
	}

	safe_putenv("%s=%s", var, value);
}

private void
trigger_restoreEnv(void)
{
	int     i;

	EACH (backup_env) putenv(backup_env[i]);
	freeLines(backup_env, free);
	backup_env = 0;
}

private	char	**
trigger_dirs(void)
{
	char	*p, **dirs = 0;
	int	i;
	project	*proj;

	p = cfg_str(0, CFG_TRIGGERS);
	// old, remove in 6.0
	if (streq(p, "none")) return (0);

	// sanctioned way.
	if (streq(p, "$NONE")) return (0);

	dirs = splitLine(p, "|", 0);
	EACH(dirs) {
		unless (dirs[i][0] == '$') continue;
		if (streq(dirs[i], "$BK_DOTBK")) {
			free(dirs[i]);
			dirs[i] = strdup(getDotBk());
		} else if (streq(dirs[i], "$BK_BIN")) {
			free(dirs[i]);
			dirs[i] = strdup(bin);
		} else if (streq(dirs[i], "$PRODUCT")) {
			free(dirs[i]);
			if (proj_isComponent(0) && (proj = proj_product(0))) {
		    		dirs[i] = strdup(proj_root(proj));
			} else {
				dirs[i] = strdup("|skip|");
			}
		} else {
			/* XXX: no habla this $var ? */
			free(dirs[i]);
			dirs[i] = strdup("|skip|");
		}
	}
	return (dirs);
}

void
trigger_setQuiet(int yes)
{
	unless (getenv("BK_QUIET_TRIGGERS")) {
		safe_putenv("BK_QUIET_TRIGGERS=%s", yes ? "YES" : "NO");
	}
}
