#include "system.h"
#include "sccs.h"

private int	doit(int verbose, char *rev, int indent, int tagOnly, int dash);

private void
usage()
{
	system("bk help -s changes");
    	exit(1);
}

int
changes_main(int ac, char **av)
{
	int	c, indent = 0, verbose = 0, tagOnly = 0;
	char	*rev = 0;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help changes");
		return (1);
	}   

	while ((c = getopt(ac, av, "tr|v|")) != -1) {
		switch (c) {
		    case 'i': indent = atoi(optarg); break;
		    case 't': tagOnly = 1; break;
		    case 'v':
			verbose = 1;
			indent = optarg ? atoi(optarg) : 2;
			break;
		    case 'r': rev = optarg; break;
		    default:
			usage();
	    	}
	}
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "Can't find package root\n");
		exit(1);
	}
	exit(doit(verbose,
	    rev, indent, tagOnly, av[optind] && streq("-", av[optind])));
}

#define	DSPEC	":DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\n$each(:C:){  (:C:)\n}$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}\n"
#define	TSPEC	"$if(:TAG:){:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\n$each(:C:){  (:C:)\n}$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}\n}"

private int
doit(int verbose, char *rev, int indent, int tagOnly, int dash)
{
	FILE	*f;
	char	cmd[MAXKEY];
	char	tmpfile[MAXPATH];
	char	dashfile[MAXPATH];
	char	buf[100];
	char	*spec = tagOnly ? TSPEC : DSPEC;
	pid_t	pid;
	extern	char *pager;
	char	*av[2] = { pager, 0 };
	int	pfd;

	dashfile[0] = 0;
	if (rev) {
		sprintf(cmd, "bk prs -Yhd'%s' -r%s ChangeSet", spec, rev);
	} else if (dash) {
		gettemp(dashfile, "dash");
		f = fopen(dashfile, "w");
		while (fgets(cmd, sizeof(cmd), stdin)) {
			fprintf(f, "ChangeSet@%s", cmd);
		}
		fclose(f);
		sprintf(cmd, "bk prs -Yhd'%s' - < %s", spec, dashfile);
	} else {
		sprintf(cmd, "bk prs -Yhd'%s' ChangeSet", spec);
	}
	unless (verbose) {
		strcat(cmd, " | ");
		strcat(cmd, pager);
		system(cmd);
		if (dashfile[0]) unlink(dashfile);
		return (0);
	}
#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	pid = spawnvp_wPipe(av, &pfd, 0);
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
			sprintf(cmd,
			    "bk cset -Hr%s | bk _sort | bk sccslog -i%d - > %s",
			    buf, indent, tmpfile);
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
