/*
 * Copyright (c) 2000, BitMover, Inc.  All rights reserved.
 */

/*
 * trigger:  fire triggers
 */

#include <stdio.h>
#include <unistd.h>
#include "system.h"
#include "sccs.h"

int
trigger(char *action, char *when, int status)
{
	int ret;
	char *what;
	char cmd[MAXPATH];

	unless (bk_proj && bk_proj->root) return 1;

	if (strneq(action, "remote pull", 11)
	                    || strneq(action, "push", 4)
			    || strneq(action, "clone", 5)
			    || strneq(action, "remote clone", 12))
		what = "outgoing";
	else if (strneq(action, "remote push", 11)
			    || strneq(action, "pull", 4))
		what = "incoming";
	else if (strneq(action, "commit", 6))
		what = "commit";
	else
		return 1;

	sprintf(cmd, "%s/" TRIGGERS "/%s%s/%s-%s",
			    bk_proj->root,
			    sccs_gethost(),
			    fullname(bk_proj->root, 0),
			    when, what);

	unless (access(cmd, X_OK) == 0)
		sprintf(cmd, "%s/" TRIGGERS "/%s-%s",
			    bk_proj->root, when, what);

	if (access(cmd, X_OK) == 0) {
		char statusenv[40];

		sprintf(statusenv, "BK_STATUS=%d", status);
		putenv(statusenv);

		ret = system(cmd);
		status = ret ? ret : status;
	}

	return status;
}
