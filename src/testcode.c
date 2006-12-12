#include "sccs.h"

/*
 * Support for regression tests of internal interfaces.
 * Most of these are tested in t.internal
 */

int
filtertest1_main(int ac, char **av)
{
	int	rc = 0;
	int	c, i;
	int	e = -1;
	int	cdump = -1;
	int	in = 0, out = 0;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "c:e:r:")) != -1) {
		switch (c) {
		    case 'c': cdump = atoi(optarg); break;
		    case 'e': e = atoi(optarg); break;
		    case 'r': rc = atoi(optarg); break;
		    default:
			fprintf(stderr, "error\n");
			return (-1);
		}
	}
	unless (av[optind]) return (-1);
	fprintf(stderr, "start %s\n", av[optind]);
	i = 0;
	while (fnext(buf, stdin)) {
		in += strlen(buf);
		if (i == e) break;
		if (i == cdump) abort();
		out += printf("%s %s", av[optind], buf);
		++i;
	}
	fprintf(stderr, "end %s rc=%d (i%d o%d)\n", av[optind], rc, in, out);
	return (rc);
}

int
filtertest2_main(int ac, char **av)
{
	FILE	*f;
	char	**cmds = 0;
	int	rc = 0;
	char	buf[MAXLINE];

	unless (av[1] && av[2] && !av[3]) return (-1);
	f = fopen(av[2], "r");
	while (fnext(buf, f)) {
		chomp(buf);
		cmds = addLine(cmds, strdup(buf));
	}
	fclose(f);
	close(0);
	open(av[1], O_RDONLY, 0);

	rc = spawn_filterPipeline(cmds);
	fprintf(stderr, "spawn_filterPipeline returned %d\n", WEXITSTATUS(rc));
	return (0);
}
