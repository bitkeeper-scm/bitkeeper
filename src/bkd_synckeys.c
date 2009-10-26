#include "bkd.h"

/*
 * Note: this function is also called from bkd_changes.c
 */
int
cmd_synckeys(int ac, char **av)
{
	char	*p, buf[MAXKEY], cmd[MAXPATH];
	int	c, n, status;
	MMAP    *m;
	FILE	*l;
	char	*lktmp;
	int	ret;
	int	syncRoot = 0;

	while ((c = getopt(ac, av, "S")) != -1) {
	    switch (c) {
		case 'S':	/* look through the root log for a match */
		    syncRoot = 1;
		    break;
		default: break;
	    }
	}

	setmode(0, _O_BINARY); /* needed for gzip mode */
	if (sendServerInfoBlock(0)) return (1);
	unless (isdir("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		out("@END@\n");
		return (1);
	}

	p = getenv("BK_REMOTE_PROTOCOL");
	unless (p && streq(p, BKD_VERSION)) {
		out("ERROR-protocol version mismatch, want: ");
		out(BKD_VERSION); 
		out(", got ");
		out(p ? p : "");
		out("\n");
		return (1);
	}

	if (emptyDir(".")) {
		out("@OK@\n");
		out("@EMPTY TREE@\n");
		return (0);
	}

	unless(isdir("BitKeeper")) { /* not a packageg root */
		out("ERROR-Not at package root\n");
		out("@END@\n");
		return (1);
	}

	lktmp = bktmp(0, "synckey");
	sprintf(cmd, "bk _listkey -r %s > '%s'", syncRoot?"-S":"", lktmp);
	l = popen(cmd, "w");
	while ((n = getline(0, buf, sizeof(buf))) > 0) {
		fprintf(l, "%s\n", buf);
		if (streq("@END PROBE@", buf)) break;
	}

	status = pclose(l);
	if (!WIFEXITED(status) || (WEXITSTATUS(status) > 1)) {
		perror(cmd);
		out("@END@\n"); /* just in case list key did not send one */
		sprintf(buf, "ERROR-listkey failed (status==%d)\n",
		    WEXITSTATUS(status));
		out(buf);
		return (1);
	}

	out("@OK@\n");
	m = mopen(lktmp, "r");
	ret = 0;
	unless (writen(1, m->where,  msize(m)) == msize(m)) {
		perror("write");
		ret = 1;
	}
	mclose(m);
	unlink(lktmp);
	free(lktmp);
	return (ret);
}
