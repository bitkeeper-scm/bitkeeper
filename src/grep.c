/* Copyright (c) 2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");
private	const char grep_help[] = "\
usage: grep [-dimnu]  [files... | -]\n\
   -d	prefix each line with the date it was last modified\n\
   -i	ignore the case of letters in making comparisons\n\
   -m	prefix each line with the rev it was last modified in\n\
   -n	prefix each line with the filename\n\
   -u	prefix each line with the user who last modified it\n";

int
grep_main(int ac, char **av)
{
	char	sccscat_opts[20], grep_opts[20];
	char	*s = sccscat_opts, *g = grep_opts;
	char	**sav = malloc(sizeof(char*) * (ac + 10));
	char	*gav[10];
	int	c, i, fd, status;
	pid_t	pid;

	*s++ = '-';
	*g++ = '-';
	while ((c = getopt(ac, av, "dimnu")) != -1) {
		switch (c) {
		    case 'd':
		    case 'm':
		    case 'n':
		    case 'u':
			*s++ = c;
			break;
		    case 'i':
			*g++ = c;
			break;
		    default:
			fprintf(stderr, "%s", grep_help);
			exit(0);
		}
	}
	if (s[-1] == '-') s = 0; else *s = 0;
	if (g[-1] == '-') g = 0; else *g = 0;

	/*
	 * Set up the grep part of the command line and spawn it.
	 */
	gav[i=0] = "grep";
	if (g) gav[++i] = grep_opts;
	gav[++i] = av[optind++];
	gav[++i] = 0;
	if ((pid = spawnvp_wPipe(gav, &fd)) == -1) {
		fprintf(stderr, "cannot spawn grep\n");
		exit(1);
	}

	/*
	 * Set up the sccscat part of the command line.
	 */
	sav[i=0] = "sccscat";
	if (s) sav[++i] = sccscat_opts;
	while (sav[++i] = av[optind++]);

	/*
	 * arrange to have our stdout go to the grep process' stdin.
	 */
	close(1); dup(fd); close(fd);
	getoptReset();
	if (sccscat_main(i, sav)) {
		kill(pid, SIGTERM);
		waitpid(pid, 0, 0);
		exit(100);
	}
	fflush(stdout);
	close(1);
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) exit(WEXITSTATUS(status));
	exit(101);
}
