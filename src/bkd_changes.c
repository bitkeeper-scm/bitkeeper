#include "bkd.h"

int
cmd_chg_part1(int ac, char **av)
{
	int	c, keysync = 0;
	int 	rfd, i, j;
	char 	buf[MAXLINE], buf2[MAXLINE + 1];
	char	*new_av[100];
	FILE 	*f;
	pid_t	pid;

	while ((c = getopt(ac, av, "k")) != -1) {
		switch (c) {
		    case 'k': keysync= 1; break;
		}
	}

	if (keysync) return (cmd_synckeys(ac, av));


	setmode(0, _O_BINARY);
	sendServerInfoBlock(0);

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

	out("@OK@\n");
	new_av[0] =  "bk";
	new_av[1] =  "changes";
	for (j = 2, i = 1; av[i]; j++, i++) new_av[j] = av[i];

	pid = spawnvp_rPipe(new_av, &rfd, 0);
	f = fdopen(rfd, "rt");
	out("@CHANGES INFO@\n");
	while (fnext(buf, f)) {
		sprintf(buf2, "%c%s", BKD_DATA, buf);
		out(buf2);
	}
	out("@END@\n");
	return (0);
}

int
cmd_chg_part2(int ac, char **av)
{
	char	*p, buf[MAXKEY], cmd[MAXPATH];
	char	vopt[50] = "", *topt = "";
	int	c, rc, status;
	FILE	*f;

	while ((c = getopt(ac, av, "v:t")) != -1) {
		switch (c) {
		    case 'v':	sprintf(vopt, "-v%s", optarg);  break;
		    case 't':	topt = "-t"; break;
		    deafule:	out("ERROR-bad option\n");
				drain();
				return (1);
		}
	}

	setmode(0, _O_BINARY);
	sendServerInfoBlock(0);

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
		
	/*
	 * XXX TODO look for @KEY@
	 */
	getline(0, buf, sizeof(buf));
	if (streq("@NOTHING TO SEND@", buf)) {
		rc = 0;
		goto skip;
	} else if (!streq("@KEY LIST@", buf)) {
		rc = 1;
		goto skip;
	}

	signal(SIGCHLD, SIG_DFL); /* for free bsd */
	sprintf(cmd, "bk changes %s %s - > BitKeeper/tmp/chg%d",
	    vopt, topt, getpid());
	f = popen(cmd, "w");
	assert(f);
	while (getline(0, buf, sizeof(buf)) > 0) {
		if (streq("@END@", buf)) break;
		fprintf(f, "%s\n",  buf);
		fflush(f);
	}
	status = pclose(f);

	if (!WIFEXITED(status) || (WEXITSTATUS(status) > 1)) {
		perror(cmd);
		out("ERROR-bk changes failed\n");
		return (1);
	}

	sprintf(cmd, "BitKeeper/tmp/chg%d", getpid());
	out("@CHANGES INFO@\n");
	f = fopen(cmd, "rt");
	assert(f);
	while (fnext(buf, f)) {
		outc(BKD_DATA);
		out(buf);
	}
	fclose(f);
	out("@END@\n");
	unlink(cmd);
skip:	return (0);
}
