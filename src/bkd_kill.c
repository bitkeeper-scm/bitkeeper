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
