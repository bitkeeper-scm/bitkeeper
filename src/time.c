#include "sccs.h"

/*
 * This is a mimimal subset of the linux time(1) command.
 */
int
time_main(int ac, char **av)
{
	int	c;
	char	*freeme = 0, *format = "%e secs\n";
	char	*p, *cmd = 0;
	int	rc;
	struct	timeval tv1, tv2;

	while ((c = getopt(ac, av, "c:f;")) != -1) {
		switch (c) {
		    case 'c': cmd = strdup(optarg); break;
		    case 'f': freeme = format = strdup(optarg); break;
		    default:
usage:			sys("bk", "help", "-s", av[0], SYS);
			rc = 1;
			goto out;
		}
	}
	unless ((cmd && cmd[0] && !av[optind]) ||
	    (!cmd && av[optind] && av[optind][0])) {
		goto usage;
	}
	unless (cmd) {
		char	**list = 0;

		for (c = optind; av[c]; c++) {
			list = addLine(list, shellquote(av[c]));
		}
		cmd = joinLines(" ", list);
		freeLines(list, free);
	}
	gettimeofday(&tv1, 0);
	rc = system(cmd);
	gettimeofday(&tv2, 0);
	rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 255;
	free(cmd);
	cmd = 0;

	for (p = format; *p; p++) {
		switch (*p) {
		    case '%':
			++p;
			switch (*p) {
			    case '%':
				fputc(*p, stderr);
				break;
			    case 'e':
				fprintf(stderr, "%.1f",
				    (float)(tv2.tv_sec - tv1.tv_sec) +
				    (tv2.tv_usec - tv1.tv_usec) / 1e6 + 0.05);
				break;
			    default:
				fputc('%', stderr);
				unless (*p) goto out; /* string ends in % */
				fputc(*p, stderr);
				break;
			}
			break;
		    case '\\':
			++p;
			switch (*p) {
			    case '\\': fputc(*p, stderr); break;
			    case 'n': fputc('\n', stderr); break;
			    default:
				fputc('\\', stderr);
				unless (*p) goto out; /* string ends in \ */
				fputc(*p, stderr);
				break;
			}
			break;
		    default:
			fputc(*p, stderr);
		}
	}
out:
	if (cmd) free(cmd);
	if (freeme) free(freeme);
	return (rc);
}
