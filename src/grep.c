/* Copyright (c) 2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

int
grep_main(int ac, char **av)
{
	char	sccscat_opts[20], grep_opts[20];
	char	*s = sccscat_opts, *g = grep_opts;
	char	**sav = malloc(sizeof(char*) * (ac + 10));
	char	*gav[10];
	char	*grep = "grep";
	int	c, i, fd, none = 0, status;
	char	rev[200];
	char	range[200];
	pid_t	pid;

	if (ac == 2 && streq("--help", av[1])) {	
		system("bk help grep");
		return (1);
	}

	*s++ = '-';
	*g++ = '-';
	rev[0] = range[0] = 0;
	while ((c = getopt(ac, av, "ac|deimnNur|R|")) != -1) {
		switch (c) {
		    case 'a':
			none = 1;
			break;
		    case 'd':
		    case 'm':
		    case 'n':
		    case 'N':
		    case 'u':
			*s++ = c;
			break;
		    case 'e':
		    	grep = "egrep";
			break;
		    case 'r':
			sprintf(rev, "-r%s", optarg);
			break;
		    case 'c':
			sprintf(range, "-c%s", optarg);
			break;
		    case 'R':
			sprintf(range, "-r%s", optarg);
			break;
		    case 'i':
			*g++ = c;
			break;
		    default:
			system("bk help -s grep");
			exit(0);
		}
	}
	if (s[-1] == '-') s = 0; else *s = 0;
	if (g[-1] == '-') g = 0; else *g = 0;

	/*
	 * Set up the grep part of the command line and spawn it.
	 */
	gav[i=0] = grep;
	if (g) gav[++i] = grep_opts;
	gav[++i] = av[optind++];
	gav[++i] = 0;
	if ((pid = spawnvp_wPipe(gav, &fd, BIG_PIPE)) == -1) {
		fprintf(stderr, "cannot spawn grep\n");
		exit(1);
	}

	if (rev[0] && range[0]) {
		fprintf(stderr, "bk grep: can not mix -r with -R\n");
		exit(1);
	}

	if (rev[0]) {
		sav[i=0] = "get";
		sav[++i] = rev;
		sav[++i] = "-kqp";
	} else {
		sav[i=0] = "sccscat";
		sav[++i] = "-q";
		if (range[0]) sav[++i] = range;
	}
	if (s) {
		sav[++i] = sccscat_opts;
	} else {
		unless (none) sav[++i] = "-nm";
	}
	while (sav[++i] = av[optind++]);

	/*
	 * arrange to have our stdout go to the grep process' stdin.
	 */
	close(1); dup(fd); close(fd);
	getoptReset();
	if (rev[0]) {
		i = get_main(i, sav);
	} else {
		i = sccscat_main(i, sav);
	}
	if (i) {
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
