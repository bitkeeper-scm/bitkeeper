/*
 * trigger:  fire triggers before and/or after repository level commands.
 *
 * Copyright (c) 2000, David Parsons & Larry McVoy.
 */

#include "system.h"
#include "sccs.h"

int
trigger(char *action, char *when, int status)
{
	int	ret = 0;
	char	*what;
	char	*var;
	char	*t;
	char	file[MAXPATH];

	unless (bk_proj && bk_proj->root) return (1);
	unless (t = strchr(action, ' ')) return (1);
	t++;

	if (strneq(t, "remote pull", 11) || strneq(t, "push", 4) ||
	    strneq(t, "clone", 5) || strneq(t, "remote clone", 12)) {
		what = "outgoing";
		var = "BK_OUTGOING";
    	} else if (
	    strneq(t, "remote push", 11) || strneq(t, "pull", 4)) {
		what = "incoming";
		var = "BK_INCOMING";
	} else if (strneq(t, "commit", 6)) {
		what = "commit";
		var = "BK_COMMIT";
	} else {
		return (1);
	}

	sprintf(file, "%s/%s/%s-%s", bk_proj->root, TRIGGERS, when, what);
	get(file, SILENT, "-");
	if (access(file, X_OK) == 0) {
		char	env[200];
		char	cmd[MAXPATH*4];

		if (status) {
			sprintf(env, "%s='ERROR %d'", var, status);
		} else if (t = getenv(var)) {
			sprintf(env, "%s=%s", var, t);
		} else {
			sprintf(env, "%s=OK", var);
		}
		if (streq(when, "post")) {
			sprintf(cmd, "cd %s; env %s %s %s %s",
			    bk_proj->root, env, file, when, action, status);
		} else {
			sprintf(cmd, "cd %s; env %s %s %s %s",
			    bk_proj->root, env, file, when, action);
		}
		if (system(cmd)) return (1);
	}
	return (0);
}
