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
		    case 'i': indent = atoi(optarg); break;  	/* undoc? 2.0 */
		    case 't': tagOnly = 1; break;		/* doc 2.0 */
		    case 'v':					/* doc 2.0 */
			verbose = 1;
			indent = optarg ? atoi(optarg) : 2;
			break;
		    case 'r': rev = optarg; break;		/* doc 2.0 */
		    default:
			usage();
	    	}
	}
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "Can't find package root\n");
		exit(1);
	}
	return(doit(verbose,
		    rev, indent, tagOnly, av[optind] && streq("-", av[optind])));
}


/*
 * Convert a command line to a av[] vector 
 *
 * This function is copied from win32/uwtlib/wapi_intf.c
 * XXX TODO we should propably move this to util.c if used by
 * other code.
 */
private void
line2av(char *cmd, char **av)
{
	char	*p, *q, *s;
	int	i = 0;
#define isQuote(q) (strchr("\"\'", *q) && q[-1] != '\\')
#define isDelim(c) isspace(c)
#define	MAXARGS 100

	p = cmd;
	while (isspace(*p)) p++; 
	while (*p) {
		av[i++] = p;
		s = q = p;
		while (*q && !isDelim(*q)) {
			if (*q == '\\') {
				q++;
				*s++ = *q++;
			} else if (isQuote(q)) {
				q++; /* strip begin quote */
				while (!isQuote(q)) {
					*s++ = *q++;
				}
				q++; /* strip end quote */
			} else {
				*s++ = *q++;
			}
		}
		if (*q == 0) {
			*s = 0;
			break;
		}
		*s = 0;
		p = ++q;
		while (isspace(*p)) p++; 
	}
	av[i] = 0;
	assert(i < MAXARGS);
	return;
}

/*
 * XXX May need to change the @ to BK_FS in the following dspec
 */
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
	char	*pager_av[MAXARGS];
	int	pfd;

	dashfile[0] = 0;
	if (rev) {
		sprintf(cmd, "bk prs -Yhd'%s' -r%s ChangeSet", spec, rev);
	} else if (dash) {
		gettemp(dashfile, "dash");
		f = fopen(dashfile, "w");
		while (fgets(cmd, sizeof(cmd), stdin)) {
			switch (*cmd) {
			    case '#': case '\n': 
				/* ignore blank lines and comments */
				break;
			    case '0': case '1': case '2': case '3': case '4':
			    case '5': case '6': case '7': case '8': case '9':
				fprintf(f, "ChangeSet%c%s", BK_FS, cmd);
				break;
			    default:
				fprintf(stderr, "Illegal line: %s", cmd);
				fclose(f);
				if (dashfile[0]) unlink(dashfile);
				return (1);
			}
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
	signal(SIGPIPE, SIG_IGN);
	strcpy(tmpfile, pager); /* line2av stomp */
	line2av(tmpfile, pager_av); /* because pager is "less -E" on win32 */
	pid = spawnvp_wPipe(pager_av, &pfd, 0);
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
