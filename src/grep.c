/* Copyright 2003 BitMover, Inc. */
#include "regex/regex.h"
#include "system.h"
#include "sccs.h"

private	void	doit(FILE *f);
private void	context(char *arg);
private char	*getfile(char *buf);
private void	done(char *file, int count);

struct	grep {
	u32	count:1;	/* count up the matches */
	u32	invert:1;	/* show non matching lines */
	u32	lineno:1;	/* also list line number */
	u32	list:1;		/* just list matching file names */
	u32	List:1;		/* just list non-matching file names */
	u32	nocase:1;	/* ignore case */
	u32	quiet:1;	/* exit status only */
	u32	wholeline:1;	/* add ^ $ to the pat */
	u32	name:1;		/* print file name */

	u32	before;		/* lines of context before the match */
	u32	after;		/* lines of context after the match */
	u32	found;		/* set if any matches were found in any file */
	u32	line;		/* line number in current file */
} opts;

int
grep_main(int ac, char **av)
{
	int	c;
	char	*pat;
	FILE	*f;
	char	*rev = 0, *range = 0;
	char	**cmd;
	pid_t	pid;
	int	pfd;

	opts.name = 1;
	while ((c = getopt(ac, av, "a;A;cC|d;hilLnqr;R|vx")) != EOF) {
		switch (c) {
		    case 'a':				/* BK anno opt */
		    case 'A':				/* BK anno opt */
			fprintf(stderr, "Annotations need merge w/ Wayne\n");
		    	break;
		    case 'r':				/* BK rev opt */
			rev = aprintf("-r%s", optarg);
			break;
		    case 'd':				/* BK date opt */
			range = aprintf("-c%s", optarg);
			break;
		    case 'R':				/* BK range opt */
			if (optarg && optarg[0]) {
				range = aprintf("-r%s", optarg);
			} else {
				range = aprintf("-r1.0..");
			}
			break;

		    case 'c': opts.count = 1; break;	/* GNU compat */
		    case 'C': context(optarg); break;	/* semi GNU compat */
		    case 'h': opts.name = 0; break;	/* GNU compat */
		    case 'i': opts.nocase = 1; break;	/* GNU compat */
		    case 'l': opts.list = 1; break;	/* GNU compat */
		    case 'L': opts.List = 1; break;	/* GNU compat */
		    case 'n': opts.lineno = 1; break;	/* GNU compat */
		    case 'q': opts.quiet = 1; break;	/* GNU compat */
		    case 'v': opts.invert = 1; break;	/* GNU compat */
		    case 'x': opts.wholeline = 1; break;/* GNU compat */
		}
	}
	unless (av[optind]) exit(2);
	if (opts.nocase) {
		for (pat = av[optind]; *pat; pat++) *pat = tolower(*pat);
	}
	pat = av[optind];
	optind++;
	if (opts.wholeline) {
		char	*s = malloc(strlen(pat) + 3);

		if (*pat != '^') {
			sprintf(s, "^%s", pat);
		} else {
			strcpy(s, pat);
		}
		while (*pat++);
		unless ((pat[-1] == '$') && (pat[-2] != '\\')) {
			strcat(s, "$");
		}
		pat = s;
	}
	if (re_comp(pat)) exit(2);
	if (rev && range) {
		fprintf(stderr, "bk grep: can not mix -r with -R\n");
		exit(2);
	}
	cmd = addLine(0, "bk");
	unless (range || rev) rev = "-r+";
	if (rev) {
		cmd = addLine(cmd, "get");
		cmd = addLine(cmd, "-kpq");
		cmd = addLine(cmd, rev);
	} else {
		cmd = addLine(cmd, "sccscat");
		cmd = addLine(cmd, "-q");
		if (range) cmd = addLine(cmd, range);
	}
	while (av[optind]) cmd = addLine(cmd, av[optind++]);
	/* force a null entry at the end for spawn */
	cmd = addLine(cmd, "");
	cmd[nLines(cmd)] = 0;
	putenv("BK_PRINT_EACH_NAME=YES");
	pid = spawnvp_rPipe(&cmd[1], &pfd, 0);
	f = fdopen(pfd, "r");
	doit(f);
	exit(opts.found ? 0 : 1);

	// TODO - figure out a way to make this work with non-bk files so we
	// we can use it in the regressions.
}

void
doit(FILE *f)
{
	char	*p, *file = strdup("?");
	int	match;
	int	first = 1, skip = 0;
	u32	count = 0;
	char	buf[BUFSIZ];
	char	lower[BUFSIZ];

	opts.line = 0;
	while (fgets(buf, sizeof(buf), f)) {
		if (p = getfile(buf)) {
			unless (first) done(file, count);
			count = opts.line = first = skip = 0;
			free(file);
			file = strdup(p);
			continue;
		}
		if (skip) continue;
		opts.line++;
		chomp(buf);
		unless (buf[0]) continue;
		if (opts.nocase) {
			int	i;

			for (i = 0; buf[i]; i++) lower[i] = tolower(buf[i]);
			lower[i] = 0;
			match = re_exec(lower);
		} else {
			match = re_exec(buf);
		}
		if (opts.invert) match = !match;
		if (match) {
			count++;
			if (opts.quiet) exit(0);
			opts.found = 1;
			if (opts.list) {
				unless (file) file = "(standard input)";
				printf("%s\n", file);
				skip = 1;
				continue;
			}
			unless (opts.List || opts.count) {
				if (file && opts.name) printf("%s:", file);
				if (opts.lineno) printf("%d:", opts.line);
				puts(buf);
			}
		}
	}
	done(file, count);
}

private void
done(char *file, int count)
{
	if (!opts.found && opts.List) {
		unless (file) file = "(standard input)";
		printf("%s\n", file);
		return;
	}
	if (opts.count && count) {
		if (file) printf("%s:", file);
		printf("%u\n", count);
	}
}

private char *
getfile(char *buf)
{
	char	*p, *file;

	/* |FILE|src/slib.c|CRC|12345 */
	unless (strneq(buf, "|FILE|", 6)) return (0);
	file = &buf[6];
	p = strchr(file, '|');
	unless (p && strneq(p, "|CRC|", 5)) return (0);
	*p = 0;
	unless (crc(file) == atoi(p + 5)) {
		*p = '|';
		return (0);
	}
	return (file);
}

private void
context(char *arg)
{
	/* -C sets before/after to 3 */
	unless (arg && *arg) {
		opts.before = opts.after = 3;
		return;
	}
	/* -C%d sets before/after to %d */
	if (isdigit(optarg[0])) {
		opts.before = opts.after = atoi(arg);
		return;
	}
	/* -C+5 -C-3 -C+5-3 -C-3+5 etc */
	while (*arg) {
		if (*arg == '+') {
			arg++;
			opts.after = atoi(arg);
		} else if (*arg == '-') {
			arg++;
			opts.before = atoi(arg);
		} else {
			system("bk help -s grep");
			exit(2);
		}
		while (*arg && isdigit(*arg)) arg++;
	}
}
