#include "system.h"
#include "sccs.h"

private int	doit(int verbose, char *rev, int dash);

private void
usage()
{
    	exit(1);
}

int
changes_main(int ac, char **av)
{
	int	c;
	int	verbose = 0;
	char	*rev = 0;

	while ((c = getopt(ac, av, "r|v")) != -1) {
		switch (c) {
		    case 'v': verbose = 1; break;
		    case 'r': rev = optarg; break;
		    default:
			usage();
	    	}
	}
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "Can't find package root\n");
		exit(1);
	}
	exit(doit(verbose, rev, av[optind] && streq("-", av[optind])));
}

#define	DSPEC	":DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\n$each(:C:){  (:C:)}\n$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}"

private int
doit(int verbose, char *rev, int dash)
{
	FILE	*f;
	char	cmd[MAXKEY];
	char	tmpfile[MAXPATH];
	char	dashfile[MAXPATH];
	char	buf[100];
	pid_t	pid;
	extern	char *pager;
	char	*av[2] = { pager, 0 };
	int	pfd;

	dashfile[0] = 0;
	if (rev) {
		sprintf(cmd,
		    "BK_YEAR4=1 bk prs -hd'%s' -r%s ChangeSet", DSPEC, rev);
	} else if (dash) {
		gettemp(dashfile, "dash");
		f = fopen(dashfile, "w");
		while (fgets(cmd, sizeof(cmd), stdin)) {
			fprintf(f, "ChangeSet@%s", cmd);
		}
		fclose(f);
		sprintf(cmd,
		    "BK_YEAR4=1 bk prs -hd'%s' - < %s", DSPEC, dashfile);
	} else {
		sprintf(cmd, "BK_YEAR4=1 bk prs -hd'%s' ChangeSet", DSPEC);
	}
	unless (verbose) {
		strcat(cmd, " | ");
		strcat(cmd, pager);
		system(cmd);
		if (dashfile[0]) unlink(dashfile);
		return (0);
	}
	signal(SIGPIPE, SIG_IGN);
	pid = spawnvp_wPipe(av, &pfd);
	close(1);
	dup2(pfd, 1);
	close(pfd);

	gettemp(tmpfile, "changes");
	f = popen(cmd, "r");
	while (fgets(cmd, sizeof(cmd), f)) {
		if (strneq(cmd, "ChangeSet@", 10)) {
			char	*p = strchr(cmd, ',');
			*p = 0;
			strcpy(buf, &cmd[10]);
			*p = ',';
		}
		fputs(cmd, stdout);
		if (streq(cmd, "\n")) {
			if (fflush(stdout)) break;
			/*
			 * XXX - this part gets mucho faster when we have
			 * the logging cache.
			 */
			sprintf(cmd, "bk cset -r%s | sort | bk sccslog - > %s",
			    buf, tmpfile);
			system(cmd);
			if (cat(tmpfile)) break;
		}
	}
	close(1);
	pclose(f);
	waitpid(pid, 0, 0);
	unlink(tmpfile);
	if (dashfile[0]) unlink(dashfile);
	return (0);
}
