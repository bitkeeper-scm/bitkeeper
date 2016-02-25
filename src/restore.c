/*
 * Copyright 2003,2010-2013,2016 BitMover, Inc
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

#include "sccs.h"

int
restore_main(int ac,  char **av)
{
	return (restore_backup(av[1], 0));
}

int
restore_backup(char *backup, int overwrite)
{
	char	*tmpfile;
	int	rc = 0;

	unless (backup) usage();
	unless (access(backup, R_OK) == 0) {
		fprintf(stderr, "restore: unable read backup %s\n", backup);
		return (1);
	}
	if (sysio(backup, 0, 0,
		"bk", "sfio", "-q", "-im", (overwrite ? "-f" : "--"), SYS)){
		getMsg("restore_failed", backup, '!', stderr);
		return (1);
	}
	unlink("BitKeeper/log/TIP");
	getMsg("restore_checking", 0, 0, stderr);
	tmpfile = bktmp(0);
	if (rc = systemf("bk -?BK_NO_REPO_LOCK=YES -r check -ac >'%s' 2>&1",
	    tmpfile)) {
		if (proj_isProduct(0) && isdir("RESYNC")) {
			fprintf(stderr, "\nThere were errors during check.\n");
			getMsg("pull_in_progress", 0, 0, stderr);
		} else {
			char	*msg = loadfile(tmpfile, 0);

			fputs(msg, stderr);
			FREE(msg);
			getMsg("restore_check_failed", backup, '=', stderr);
		}
	} else {
		fprintf(stderr, "check passed.\n");
	}
	unlink(tmpfile);
	FREE(tmpfile);
	return (rc);
}
