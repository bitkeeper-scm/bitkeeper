/*
 * Copyright 2005,2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"
#include "bkd.h"

int
cmd_kill(int ac, char **av)
{
	int	sock;
	char	*p, *err;
	char	buf[4];

	unless (Opts.kill_ok) {
		out("ERROR-kill not enabled\n");
		return (0);
	}
	unless (p = getenv("_KILLSOCK")) {
		out("ERROR-killsock not set\n");
		return (0);
	}
	if ((sock = tcp_connect("127.0.0.1", atoi(p))) >= 0) {
		/* wait for it to really die */
		if ((read(sock, buf, 1) == 1) && (buf[0] == 'K')) {
			/*
			 * When a 'bk _kill' command returns sucessful
			 * it needs to be OK to delete the directory
			 * where the bkd was running. If this
			 * processes (spawned from the parent bkd)
			 * takes a log time to write logs and exit,
			 * then it be in a deleted directory.  We cd
			 * to / to fix this, but it does prevent the
			 * cmdlog from getting the end of the bkd_kill
			 * command.
			 */
			chdir("/");
			out("@END@\n");
		} else {
			out("ERROR-bkd not killed\n");
		}
		closesocket(sock);
	} else {
		err = aprintf("ERROR-failed to contact localhost:%s\n", p);
		out(err);
		free(err);
	}
	return (0);
}
