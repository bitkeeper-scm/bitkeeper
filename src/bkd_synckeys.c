#include "bkd.h"

/*
 * Note: this finction is also called from bkd_changes.c
 */
int
cmd_synckeys(int ac, char **av)
{
	char	*p, buf[MAXKEY], cmd[MAXPATH];
	int	n, status;
	MMAP    *m;
	FILE	*l;

	setmode(0, _O_BINARY); /* needed for gzip mode */
	sendServerInfoBlock(0);

	unless (isdir("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		out("@END@\n");
		drain();
		return (1);
	}

	p = getenv("BK_REMOTE_PROTOCOL");
	unless (p && streq(p, BKD_VERSION)) {
		out("ERROR-protocol version mismatch, want: ");
		out(BKD_VERSION); 
		out(", got ");
		out(p ? p : "");
		out("\n");
		drain();
		return (1);
	}

	if (emptyDir(".")) {
		out("@OK@\n");
		out("@EMPTY TREE@\n");
		drain();
		return (0);
	}

	unless(isdir("BitKeeper")) { /* not a packageg root */
		out("ERROR-Not at package root\n");
		out("@END@\n");
		drain();
		return (1);
	}

	if ((bk_mode() == BK_BASIC) && !exists(BKMASTER)) {
		out("ERROR-bkd std cannot access non-master repository\n");
		out("@END@\n");
		drain();
		return (1);
	}
		
	signal(SIGCHLD, SIG_DFL); /* for free bsd */
	sprintf(cmd, "bk _listkey -r > BitKeeper/tmp/lk%u", getpid());
	l = popen(cmd, "w");
	while ((n = getline(0, buf, sizeof(buf))) > 0) {
		fprintf(l, "%s\n", buf);
		if (streq("@END PROBE@", buf)) break;
	}

	status = pclose(l);
	if (!WIFEXITED(status) || (WEXITSTATUS(status) > 1)) {
		perror(cmd);
		out("@END@\n"); /* just in case list key did not send one */
		out("ERROR-listkey failed\n");
		return (1);
	}

	out("@OK@\n");
	sprintf(cmd, "BitKeeper/tmp/lk%u", getpid());
	m = mopen(cmd, "r");
	unless (writen(1, m->where,  msize(m)) == msize(m)) {
		perror("write");
		mclose(m);
		unlink(cmd);
		return (1);
	}
	mclose(m);
	unlink(cmd);
	return (0);
}
