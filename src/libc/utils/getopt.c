/*
 * Copyright (c) 1997 L.W.McVoy
 *
 * This version handles
 *
 *	-	(leaves it and returns)
 *	-a
 *	-abcd
 *	-r <arg>
 *	-r<arg>
 *	-abcr <arg>
 *	-abcr<arg>
 *	-r<arg> -R<arg>, etc.
 *
 * Patterns in getopt string:
 *	d	boolean option		-d
 *	d:	required arg		-dARG or -d ARG
 *	d;	required arg no space   -dARG
 *	d|	optionial arg no space	-dARG or -d
 */
#include "system.h"

int	optopt;		/* option that is in error, if we return an error */
int	optind;		/* next arg in argv we process */
char	*optarg;	/* argument to an option */
static int n;

void
getoptReset(void)
{
	optopt = optind = 0;
	optarg = 0;
}

int
getopt(int ac, char **av, char *opts)
{
	char	*t;

	if (!optind) {
		optind = 1;
		n = 1;
	}
	debug((stderr, "GETOPT ind=%d n=%d arg=%s av[%d]='%s'\n",
	    optind, n, optarg ? optarg : "", optind, av[optind]));

	if ((optind >= ac) || (av[optind][0] != '-') || !av[optind][1]) {
		return (EOF);
	}
	/* Stop processing options at a -- and return arguments after */
	if (streq(av[optind], "--")) {
		optind++;
		n = 1;
		return (EOF);
	}

	assert(av[optind][n]);
	for (t = (char *)opts; *t; t++) {
		if (*t == av[optind][n]) {
			break;
		}
	}
	if (!*t) {
		optopt = av[optind][n];
		debug((stderr, "\tran out of option letters\n"));
		if (av[optind][n+1]) {
			n++;
		} else {
			n = 1;
			optind++;
		}
		return ('?');
	}

	/* OK, we found a legit option, let's see what to do with it.
	 * If it isn't one that takes an option, just advance and return.
	 */
	if (t[1] != ':' && t[1] != '|' && t[1] != ';') {
		if (!av[optind][n+1]) {
			optind++;
			n = 1;
		} else {
			n++;
		}
		debug((stderr, "\tLegit singleton %c\n", *t));
		return (*t);
	}

	/* got one with an option, see if it is cozied up to the flag */
	if (av[optind][n+1]) {
		if (av[optind][n+1]) {
			optarg = &av[optind][n+1];
		} else {
			optarg = 0;
		}
		optind++;
		n = 1;
		debug((stderr, "\t%c with %s\n", *t, optarg));
		return (*t);
	}

	/* If it was not there, and it is optional, OK */
	if (t[1] == '|') {
		optarg = 0;
		optind++;
		n = 1;
		debug((stderr, "\t%c without arg\n", *t));
		return (*t);
	}

	/* was it supposed to be there? */
	if (t[1] == ';') {
		optarg = 0;
		optind++;
		optopt = *t;
		debug((stderr, "\twanted another word\n"));
		return ('?');
	}

	/* Nope, there had better be another word. */
	if ((optind + 1 == ac) || (av[optind+1][0] == '-')) {
		optopt = av[optind][n];
		debug((stderr, "\twanted another word\n"));
		return ('?');
	}
	optarg = av[optind+1];
	optind += 2;
	n = 1;
	debug((stderr, "\t%c with arg %s\n", *t, optarg));
	return (*t);
}

#ifdef	TEST

/* XXX a.out -y file */
main(int ac, char **av)
{
	extern	char *optarg;
	extern	int optind;
	char	*comment = 0;
	int	c;

	while ((c = getopt(ac, av, "fnpsx:y|")) != -1) {
		switch (c) {
		    case 'f':
		    case 'n':
		    case 'p':
		    case 's':
			printf("Got option %c\n", c);
			break;
		    case 'x':
		    case 'y':
			    comment = optarg;
			    printf("Got optarg %s with -%c\n", comment, c);
			    break;
		    case '?':
			fprintf(stderr, "bad option %c\n", optopt);
			break;
		    default:
			fprintf(stderr, "unknown ret %c\n", c);
			break;
		}
	}
	while (av[optind]) {
		printf("av[%d] = %s\n", optind, av[optind++]);
	}
	exit(0);
}
#endif
