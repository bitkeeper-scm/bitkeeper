#include "system.h"
#include "sccs.h"
#include "bkd.h"

int
cmd_kill(int ac, char **av)
{
	int	good = 1;
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
		unless ((read(sock, buf, 1) == 1) && (buf[0] == 'K')) {
			out("ERROR-bkd not killed\n");
			good = 0;
		}
		closesocket(sock);
	} else {
		err = aprintf("ERROR-failed to contact localhost:%s\n", p);
		out(err);
		free(err);
		good = 0;
	}
	if (good) out("@END@\n");
	return (0);
}
