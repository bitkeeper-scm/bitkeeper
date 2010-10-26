/*
 * Copyright (c) 1997 L.W.McVoy
 *
 * This version handles
 *
 *	-	(leaves it and returns)
 *	--	end of options
 *	-a
 *	-abcd
 *	-r <arg>
 *	-r<arg>
 *	-abcr <arg>
 *	-abcr<arg>
 *	-r<arg> -R<arg>, etc.
 *	--long
 *	--long=<arg>
 *	--long <arg>
 *
 * Patterns in getopt string:
 *	d	boolean option		-d
 *	d:	required arg		-dARG or -d ARG
 *	d;	required arg no space   -dARG
 *	d|	optionial arg no space	-dARG or -d
 *
 * With long options:
 *	long	boolean option		--long
 *	long:	required arg		--long=ARG or --long ARG
 *	long;	required arg no space   --long=ARG
 *	long|	optionial arg no space	--long=ARG or --long
 *
 * Can be tested with 'bk _getopttest'
 */
#include "system.h"

int	optopt;		/* option that is in error, if we return an error */
int	optind;		/* next arg in argv we process */
char	*optarg;	/* argument to an option */
static int n;		/* current position == av[optind][n] */

private	int	doLong(int ac, char **av, longopt *lopts);
private	void	printUsage(char *prog, char *opts, longopt *lopts);

void
getoptReset(void)
{
	optopt = optind = 0;
	optarg = 0;
}

/*
 * Returns:
 *    - char if option found
 *    - EOF(-1) if end of arguments reached
 *    - GETOPT_ERR(256) if unknown option found.
 */
int
getopt(int ac, char **av, char *opts, longopt *lopts)
{
	char	*t;

	optarg = 0;	/* clear out arg from last round */
	optopt = 0;	/* clear error return */
	if (!optind) {
		optind = 1;
		n = 1;
	}
	debug((stderr, "GETOPT ind=%d n=%d av[%d]='%s'\n",
	    optind, n, optind, av[optind]));

	if ((optind >= ac) || (av[optind][0] != '-') || !av[optind][1]) {
		return (EOF);
	}
	/* Stop processing options at a -- and return arguments after */
	if (streq(av[optind], "--")) {
		optind++;
		n = 1;
		return (EOF);
	}
	if (streq(av[optind], "--usage")) printUsage(av[0], opts, lopts);
	if (strneq(av[optind], "--", 2)) return (doLong(ac, av, lopts));

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
		return (GETOPT_ERR);
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
		optarg = &av[optind][n+1];
		optind++;
		n = 1;
		debug((stderr, "\t%c with %s\n", *t, optarg));
		return (*t);
	}

	/* If it was not there, and it is optional, OK */
	if (t[1] == '|') {
		optind++;
		n = 1;
		debug((stderr, "\t%c without arg\n", *t));
		return (*t);
	}

	/* was it supposed to be there? */
	if (t[1] == ';') {
		optind++;
		optopt = *t;
		debug((stderr, "\twanted another word\n"));
		return (GETOPT_ERR);
	}

	/* Nope, there had better be another word. */
	if ((optind + 1 == ac) || (av[optind+1][0] == '-')) {
		optopt = av[optind][n];
		debug((stderr, "\twanted another word\n"));
		return (GETOPT_ERR);
	}
	optarg = av[optind+1];
	optind += 2;
	n = 1;
	debug((stderr, "\t%c with arg %s\n", *t, optarg));
	return (*t);
}

private int
doLong(int ac, char **av, longopt *lopts)
{
	char	*s, *t;
	int	len1, len2;

	unless (lopts) {
err:		n = 1;
		optind++;
		optopt = 0;
		return (GETOPT_ERR);
	}
	/* len of option without =value part */
	s = av[optind] + 2;
	len1 = (t = strchr(s, '=')) ? (t - s) : strlen(s);
	for (; t = lopts->name; lopts++) {
		s = av[optind] + 2;
		/* len of lopts array without suffix */
		len2 = strlen(t);
		if (strspn(t+len2-1, ":;|") == 1) --len2;

		if ((len1 == len2) && strneq(s, t, len1)) {
			s += len1;
			t += len2;
			break;	/* found a match */
		}
	}
	unless (t) goto err;

	/* OK, we found a legit option, let's see what to do with it.
	 * If it isn't one that takes an option, just advance and return.
	 */
	unless (*t) {
		if (*s == '=') goto err; /* got option anyway */
		optind++;
		n = 1;
		return (lopts->ret);
	}

	/* got one with an option, see if it is cozied up to the flag */
	if (*s == '=') {
		optarg = s + 1;
		optind++;
		n = 1;
		return (lopts->ret);
	}

	/* If it was not there, and it is optional, OK */
	if (*t == '|') {
		optind++;
		n = 1;
		return (lopts->ret);
	}

	/* was it supposed to be there? */
	if (*t == ';') goto err;

	/* Nope, there had better be another word. */
	if ((optind + 1 == ac) || (av[optind+1][0] == '-')) {
		goto err;
	}
	optarg = av[optind+1];
	optind += 2;
	n = 1;
	return (lopts->ret);
}

/*
 * look at the getopt() arguments and create a fake usage line.
 * useful as a backup in case no other help is available
 */
private	void
printUsage(char *prog, char *opts, longopt *lopts)
{
	int	len, first, i;
	char	*p;
	char	**extra = 0;

	len = fprintf(stderr, "usage: %s", prog);

	first = 0;
	for (p = opts; *p; p++) {
		if ((p[1] == ':') || (p[1] == ';') || (p[1] == '|')) {
			extra = addLine(extra, aprintf(" [-%c<arg>]", *p));
			++p;
		} else {
			unless (first) {
				len += fprintf(stderr, " [-");
				first = 1;
			}
			fputc(*p, stderr);
			++len;
		}
	}
	if (first) {
		fputc(']', stderr);
		++len;
	}
	while (lopts && (p = lopts->name)) {
		i = strlen(p);
		if (strspn(p+i-1, ":;|") == 1) --i;
		extra = addLine(extra,
		    aprintf(" [--%.*s%s]", i, p, p[i] ? "=<arg>" : ""));
		++lopts;
	}
	extra = addLine(extra, strdup(" args..."));
	EACH(extra) {
		if (len + strlen(extra[i]) > 72) {
			fputs("\n\t", stderr);
			len = 8;
		}
		len += fprintf(stderr, "%s", extra[i]);
	}
	freeLines(extra, free);
	fputc('\n', stderr);
	exit(1);
}
