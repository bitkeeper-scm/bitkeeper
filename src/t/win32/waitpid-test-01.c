// Copyright 2005-2006,2008,2015 BitMover, Inc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * Test waitpid in the case when the child has exited before the
 * call to waitpid.
 */
#include "system.h"

char	*file = "CHILD_LOCK";
char	*helper = "prog01.exe";

int
main(int ac, char **av)
{
	int	ret, retry, rc;
	pid_t	pid;
	char	*argv[10];
	HANDLE	h;

	h = CreateFile(file, GENERIC_WRITE, FILE_SHARE_WRITE, 0, 
	    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
	if (h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "cannot create file %s. Error number %ld\n",
		    file, GetLastError());
		exit(1);
	}
	CloseHandle(h);

	argv[0] = helper;
	argv[1] = file;
	argv[2] = 0;
	
	if ((pid = _spawnvp_ex(_P_NOWAIT, argv[0], argv, 1)) == -1) {
		fprintf(stderr, "cannot spawn %s\n", argv[0]);
		exit(1);
	}

	/* Make sure the child is gone before we waitpid */
	retry = 10;
	while (retry && GetFileAttributes(file) != INVALID_FILE_ATTRIBUTES) {
		retry--;
		sleep(1);
	}
	if (retry == 0) {
		fprintf(stderr, "got tired of waiting for child\n");
		exit(1);
	}

	if (waitpid(pid, &ret, 0) < 0) {
		fprintf(stderr, "waitpid failed\n");
		exit(1);
	} else if (!WIFEXITED(ret)) {
		fprintf(stderr, "%s exited abnormally\n", argv[0]);
	}
	rc = WEXITSTATUS(ret);
	exit(rc);
}
