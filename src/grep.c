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
	int	c, i, fd, status;
	char	buf[200];
	pid_t	pid;

	if (ac == 2 && streq("--help", av[1])) {	
		system("bk help grep");
		return (1);
	}

	*s++ = '-';
	*g++ = '-';
	buf[0] = 0;
	while ((c = getopt(ac, av, "dimnur|")) != -1) {
		switch (c) {
		    case 'd':
		    case 'm':
		    case 'n':
		    case 'u':
			*s++ = c;
			break;
		    case 'r':
			sprintf(buf, "-r%s", optarg);
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
	if (buf[0]) {
		sav[i=0] = "get";
		sav[++i] = buf;
		sav[++i] = "-kqp";
	} else {
		sav[i=0] = "sccscat";
	}
	if (s) {
		sav[++i] = sccscat_opts;
	} else {
		sav[++i] = buf[0] ? "-n" : "-nm";
	}
	while (sav[++i] = av[optind++]);

	/*
	 * arrange to have our stdout go to the grep process' stdin.
	 */
	close(1); dup(fd); close(fd);
	getoptReset();
	if (buf[0]) {
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
