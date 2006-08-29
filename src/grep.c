/*
 * TODO
 *	- t.grep
 *	  Test all the GNU options.
 *	  Make sure that every use of grep in our t.* files could be replaced
 *	  with "bk grep ..."
 *	- try and figure out what to do when someone hits ^C.
 *	- fix the context stuff to be identical to gnu grep
 *	- maybe look at a better way of grep -i in regex.c
 *	- maybe look at a pseudo DFA in regex.c where you grap the first
 *	  match char from each and walk the string for all in parallel.
 *	  Or something.
 *	- \<something\> is substantially slower than just "something"
 */
/* Copyright 2003 BitMover, Inc. */
#include "sccs.h"
#include "regex.h"	/* has to be second, conflicts w/ system .h's */

private	void	doit(FILE *f);
private char	*getfile(char *buf);
private void	done(char *file, int count);

private	struct	{
	u32	align:1;	/* try and align the output */
	u32	count:1;	/* count up the matches */
	u32	invert:1;	/* show non matching lines */
	u32	lineno:1;	/* also list line number */
	u32	list:1;		/* just list matching file names */
	u32	List:1;		/* just list non-matching file names */
	u32	nocase:1;	/* ignore case */
	u32	quiet:1;	/* exit status only */
	u32	wholeline:1;	/* add ^ $ to the pat */
	u32	name:1;		/* print file name */
	u32	Name:1;		/* they said they really wanted it */
	u32	anno:1;		/* skip over the annotations when searching */
	u32	firstmatch:1;	/* if set, we are the first match */

	int	before;		/* lines of context before the match */
	int	after;		/* lines of context after the match */
	u32	found;		/* set if any matches were found in any file */
	int	line;		/* line number in current file */
	short	fname;		/* if set, position in annotations of file */
	short	user;		/* if set, position in annotations of user */
	short	rev;		/* if set, position in annotations of revnums */
	short	date;		/* if set, position in annotations of date */
} opts;

int
grep_main(int ac, char **av)
{
	int	c;
	char	*pat;
	FILE	*f;
	char	*rev = 0, *range = 0, **cmd;
	int	aflags = 0, args = 0;
	char	aopts[20];

	opts.firstmatch = opts.name = 1;
	while ((c = getopt(ac, av, "a;A:B:cC|d;hHilLnqr;R|vx")) != EOF) {
		switch (c) {
		    case 'A':				/* GNU -A %d */
			if (optarg && isdigit(*optarg)) {
				opts.after = atoi(optarg);
				break;
			}
			opts.align = 1;
			/* fall through */
		    case 'a':				/* BK anno opt */
			aflags = annotate_args(aflags, optarg);
			if (aflags == -1) exit(2);
			opts.anno = 1;
		    	break;
		    case 'r':				/* BK rev opt */
			rev = aprintf("-r%s", optarg);
			break;
		    case 'd':				/* BK date opt */
			range = aprintf("-c%s", optarg);
			break;
		    case 'R':				/* BK range opt */
			if (optarg && optarg[0]) {
				range = aprintf("-R%s", optarg);
			} else {
				range = aprintf("-R..");
			}
			break;

		    case 'B': opts.before = atoi(optarg); break;
		    case 'c': opts.count = 1; break;	/* GNU compat */
		    case 'C':				/* GNU compat */
			if (optarg && (opts.before = atoi(optarg))) {
				opts.after = opts.before;
			} else {
				opts.before = opts.after = 2;
			}
			break;
		    case 'h': opts.name = 0; break;	/* GNU compat */
		    case 'H': opts.Name = 1; break;	/* GNU compat */
		    case 'i': opts.nocase = 1; break;	/* GNU compat */
		    case 'l': opts.list = 1; break;	/* GNU compat */
		    case 'L': opts.List = 1; break;	/* GNU compat */
		    case 'n': opts.lineno = 1; break;	/* GNU compat */
		    case 'q': opts.quiet = 1; break;	/* GNU compat */
		    case 'v': opts.invert = 1; break;	/* GNU compat */
		    case 'x': opts.wholeline = 1; break;/* GNU compat */
		    default: exit(2);
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
		fprintf(stderr, "grep: can't mix -r with -R\n");
		exit(2);
	}
	cmd = addLine(0, "bk");
	if (aflags && !rev && !range) rev = "-r+";
	if (rev) {
		cmd = addLine(cmd, "get");
		cmd = addLine(cmd, "-kpq");
		cmd = addLine(cmd, rev);
	} else if (range) {
		cmd = addLine(cmd, "annotate");
		cmd = addLine(cmd, range);
	} else {
		cmd = addLine(cmd, "cat");
	}
	if (aflags) {
		aopts[0] = '-';
		aopts[1] = 'A';
		aopts[2] = 0;
		if (aflags & GET_PREFIXDATE)	strcat(aopts, "d");
		if (aflags & GET_RELPATH)	strcat(aopts, "p");
		if (aflags & GET_MODNAME)	strcat(aopts, "b");
		if (aflags & GET_REVNUMS)	strcat(aopts, "r");
		if (aflags & GET_USER)		strcat(aopts, "u");
		cmd = addLine(cmd, aopts);
		if (aflags & (GET_RELPATH|GET_MODNAME)) {
			opts.Name = opts.name = 0;
		}

		opts.date = opts.fname = opts.user = opts.rev = 1;
		if (opts.name) {
			opts.fname++, opts.user++, opts.rev++, opts.date++;
		}
		if (opts.lineno) {
			opts.fname++, opts.user++, opts.rev++, opts.date++;
		}
		if (aflags & GET_RELPATH) opts.user++, opts.rev++, opts.date++;
		if (aflags & GET_MODNAME) opts.user++, opts.rev++, opts.date++;
		if (aflags & GET_PREFIXDATE) opts.user++, opts.rev++;
		if (aflags & GET_USER) opts.rev++;

		unless (aflags & GET_USER) opts.user = 0;
		unless (aflags & GET_REVNUMS) opts.rev = 0;
		unless (aflags & (GET_RELPATH|GET_MODNAME)) opts.fname = 0;
		unless (aflags & GET_PREFIXDATE) opts.date = 0;
	}
	cmd = addLine(cmd, "-B");
	while (av[optind]) {
		unless (streq("-", av[optind])) args++;
		cmd = addLine(cmd, av[optind++]);
	}
	if (args == 1) opts.name = opts.Name;

	/* force a null entry at the end for spawn */
	cmd = addLine(cmd, "");
	cmd[nLines(cmd)] = 0;
	putenv("BK_PRINT_EACH_NAME=YES");
	f = popenvp(&cmd[1], "r");
	doit(f);
	pclose(f);
	exit(opts.found ? 0 : 1);
}

typedef	struct {
	char	*buf;
	int	bufsiz;
} line;

private	line	*lines;	/* array of opts.before+1 lines */
private	char	*lower;
private	int	cur, lsiz, nlines;

char	*
fgetline(FILE *f)
{
	char	*p;
	int	nchar;
	line	*l;

	unless (lines) {
		nlines = opts.before + 1;
		lines = calloc(nlines, sizeof(line));
	}
	if (opts.before) cur = (cur + 1) % nlines;
	l = &lines[cur];
	unless (l->bufsiz) {
		l->bufsiz = 512;
realloc:
		if (l->buf) {
			char	*tmp = malloc(l->bufsiz << 1);

			bcopy(l->buf, tmp, l->bufsiz);
			free(l->buf);
			l->buf = tmp;

			p = &(l->buf[l->bufsiz - 1]);
			assert(p[-1] && !p[0]);
			nchar = l->bufsiz + 1;
			l->bufsiz <<= 1;
		} else {
			p = l->buf = malloc(l->bufsiz);
			nchar = l->bufsiz;
		}
		if (opts.nocase && (l->bufsiz > lsiz)) {
			if (lower) free(lower);
			lower = malloc(lsiz = l->bufsiz);
		}
	} else {
		p = l->buf;
		nchar = l->bufsiz;
	}
	unless (fgets(p, nchar, f)) return (0);
	unless (chomp(p)) goto realloc;
	return (l->buf);
}

private void
pline(char *file, char *buf, char c, int lineno)
{
	int	len = 0;
	int	n = 0;

	if (file && opts.name) {
		printf("%s%c", file, c);
		n++;
	}
	if (opts.lineno) {
		printf("%d%c", lineno, c);
		n++;
	}
	if (opts.anno) {
		char	*p;

		for (p = buf; *p != '|'; p++) {
			unless (isspace(*p)) {
				putchar(*p);
				len++;
				continue;
			}
			n++;
			if (opts.align) {
				if ((n == opts.fname) || (n == opts.user) ||
				    (n == opts.rev) || (n == opts.date)) {
					len = 16 - len;
					if (len > 0) {
						printf("%*s", len, "");
					} else {
						putchar('\t');
					}
				} else {
					putchar(c);
				}
			} else {
				putchar(c);
			}
			while (isspace(p[1])) p++;
			len = 0;
		}
		assert(*p == '|');
		unless (opts.align) p += 2;
		puts(p);
	} else {
		puts(buf);
	}
}

private void
doit(FILE *f)
{
	char	*p, *file = strdup("?");
	int	match;
	int	first = 1, skip = 0, print = 0;
	int	lastprinted = 0;
	u32	count = 0;
	char	*buf = 0;

	opts.line = 0;
	while (buf = fgetline(f)) {
		if ((buf[0] == '|') && (p = getfile(buf))) {
			unless (first) done(file, count);
			count = opts.line = first = skip = 0;
			print = lastprinted = 0;
			free(file);
			file = strdup(p);
			continue;
		}
		if (skip) continue;
		opts.line++;
		p = opts.anno ? strchr(buf, '|') + 2 : buf;
		if (opts.nocase) {
			int	i;

			for (i = 0; p[i]; i++) lower[i] = tolower(p[i]);
			lower[i] = 0;
			match = re_exec(lower);
		} else {
			match = re_exec(p);
		}
		if (opts.invert) match = !match;
		unless (match || print) continue;
		if (match) {
			count++;
			print = opts.after;
		} else {
			print--;
		}
		if (opts.quiet) exit(0);
		opts.found = 1;
		if (opts.list) {
			unless (file) file = "(standard input)";
			printf("%s\n", file);
			skip = 1;
			continue;
		}
		if (opts.List || opts.count) continue;
		if (match && (opts.before || opts.after) && lastprinted &&
				((lastprinted + 1 + opts.before) < opts.line)) {
			puts("--");
		}
		opts.firstmatch = 0;
		if (match && opts.before) {
			int	i, j, n = opts.line;

			for (i=max(n-opts.before, lastprinted+1); i < n; ++i) {
				j = (cur + (i-n) + nlines) % nlines;
				pline(file, lines[j].buf, '-', i);
			}
		}
		pline(file, buf, match ? ':' : '-', opts.line);
		lastprinted = opts.line;
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
	unless (crc(file) == strtoul(p + 5, 0, 10)) {
		*p = '|';
		return (0);
	}
	return (file);
}
