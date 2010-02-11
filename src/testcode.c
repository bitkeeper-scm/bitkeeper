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

	while ((c = getopt(ac, av, "c:e:r:", 0)) != -1) {
		switch (c) {
		    case 'c': cdump = atoi(optarg); break;
		    case 'e': e = atoi(optarg); break;
		    case 'r': rc = atoi(optarg); break;
		    default: bk_badArg(c, av);
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

void
getMsg_tests(void)
{
	char	**args, *p;
	FILE	*f;

	f = fmem_open();
	assert(f);
	args = addLine(0, "-one+");
	args = addLine(args, "-two+");
	args = addLine(args, "-three+");
	args = addLine(args, "-four+");
	getMsgv("test-args", args, 0, 0, f);
	p = fmem_retbuf(f, 0);
	assert(streq(p, "lead:-one+-one+-two+ -three+BKARG#2-one+:trail\n"));
	free(p);
	fclose(f);
	freeLines(args, 0);
}

/* run specialized code tests */
int
unittests_main(int ac, char **av)
{
	if (av[1]) return (1);

	libc_tests();
	getMsg_tests();
	return (0);
}

int
getopt_test_main(int ac, char **av)
{
	char	*comment = 0;
	int	c;
	longopt	lopts[] = {
		{ "longf", 'f' },  /* alias for -f */
		{ "longx:", 'x' }, /* alias for -x */
		{ "longy;", 'y' }, /* alias for -y */
		{ "longz|", 'z' }, /* alias for -z */
		{ "unique", 400 },  /* unique option */
		{ "unhandled", 401 },
		{ 0, 0}
	};

	while ((c = getopt(ac, av, "fnpsx:y;z|Q", lopts)) != -1) {
		switch (c) {
		    case 'f':
		    case 'n':
		    case 'p':
		    case 's':
			printf("Got option %c\n", c);
			break;
		    case 'x':
		    case 'y':
		    case 'z':
			comment = optarg ? optarg : "(none)";
			printf("Got optarg %s with -%c\n", comment, c);
			break;
		    case 400:
			printf("Got option --unique\n");
			break;
		    default: bk_badArg(c, av);
		}
	}
	for (; av[optind]; optind++) {
		printf("av[%d] = %s\n", optind, av[optind]);
	}
	return (0);
}

int
recurse_main(int ac, char **av)
{
	if (av[1]) exit(1);

	if (system("bk -R _recurse")) return (1);
	return (0);
}

