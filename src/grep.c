/*
 * Copyright 2000-2012,2014-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * TODO
 *	- t.grep
 *	  Test all the GNU options.
 *	  Make sure that every use of grep in our t.* files could be replaced
 *	  with "bk grep ..."
 *	- maybe look at a better way of grep -i in regex.c
 *	- maybe look at a pseudo DFA in regex.c where you grap the first
 *	  match char from each and walk the string for all in parallel.
 *	  Or something.
 *	- \<something\> is substantially slower than just "something"
 */

#include "sccs.h"

private	void	doit(FILE *f, pcre *re);
private char	*getfile(char *buf);
private void	done(char *file);

private	struct	{
	u32	invert:1;	/* show non matching lines */
	u32	lineno:1;	/* also list line number */
	u32	list:1;		/* just list matching file names */
	u32	List:1;		/* just list non-matching file names */
	u32	nocase:1;	/* ignore case */
	u32	quiet:1;	/* exit status only */
	u32	wholeline:1;	/* add ^ $ to the pat */
	u32	anno:1;		/* skip over the annotations when searching */
	u32	nonames:1;	/* never print names */
	u32	firstmatch:1;	/* if set, we are the first match */

	int	before;		/* lines of context before the match */
	int	after;		/* lines of context after the match */
	u32	found;		/* set if any matches were found in any file */
	int	line;		/* line number in current file */
	char	style;		/* A or A: anno style */
	int	seps;		/* | is 1, : is 1 or more */
} opts;

int
grep_main(int ac, char **av)
{
	int	c;
	char	*pat;
	FILE	*f;
	const	char *perr;
	int	poff;
	char	*rev = 0, *range = 0, **cmd;
	int	aflags = 0, args = 0;
	pcre	*re = 0;
	char	aopts[20];

	opts.firstmatch = 1;
	while ((c = getopt(ac, av, "a;A:B:c;d;C|hHilLnqr;R|vx", 0)) != EOF) {
		switch (c) {
		    case 'A':				/* GNU -A %d */
			if (optarg && isdigit(*optarg)) {
				opts.after = atoi(optarg);
				break;
			}
			opts.style = 'A';
			/* fall through */
		    case 'a':				/* BK anno opt */
			aflags = annotate_args(aflags, optarg);
			if (aflags == -1) exit(2);
			unless (opts.style) opts.style = 'a';
		    	break;
		    case 'r':				/* BK rev opt */
			if (strstr(optarg, "..")) {
				fprintf(stderr, "grep: use -R for ranges.\n");
				exit(1);
			}
			rev = aprintf("-r%s", optarg);
			break;
		    case 'd':				// old, compat
		    case 'c':				/* BK date opt */
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
		    case 'C':				/* GNU compat */
			if (optarg && (opts.before = atoi(optarg))) {
				opts.after = opts.before;
			} else {
				opts.before = opts.after = 2;
			}
			break;
		    case 'h': opts.nonames = 1; break;	/* GNU compat */
		    case 'H': 				/* GNU compat -Ap */
			aflags |= GET_RELPATH; break;
		    case 'i': opts.nocase = 1; break;	/* GNU compat */
		    case 'l': opts.list = 1; break;	/* GNU compat */
		    case 'L': opts.List = 1; break;	/* GNU compat */
		    case 'n':				/* GNU compat for -An */
			aflags |= GET_LINENUM; break;
		    case 'q': opts.quiet = 1; break;	/* GNU compat */
		    case 'v': opts.invert = 1; break;	/* GNU compat */
		    case 'x': opts.wholeline = 1; break;/* GNU compat */
		    default: bk_badArg(c, av);
		}
	}
	if (aflags && !opts.style) opts.style = 'a';	/* GNU compat */
	if ((opts.before || opts.after) && (opts.style == 'A')) usage();
	if (opts.list || opts.List) {
		aflags = opts.style = opts.before = opts.after = 0;
		opts.nonames = 1;
	}
	if (!av[optind] || (!av[optind+1] && streq(av[optind], "-"))) usage();
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
	unless (re = pcre_compile(pat, 0, &perr, &poff, 0)) exit(2);
	if (rev && range) {
		fprintf(stderr, "grep: can't mix -r with -R\n");
		exit(2);
	}
	cmd = addLine(0, "bk");
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
	cmd = addLine(cmd, "-B");
	if (aflags) {
		aopts[0] = '-';
		aopts[1] = opts.style;
		aopts[2] = 0;
		if (aflags & GET_MODNAME)	strcat(aopts, "b");
		if (aflags & GET_PREFIXDATE)	strcat(aopts, "d");
		if (aflags & GET_RELPATH)	strcat(aopts, "p");
		if (aflags & GET_REVNUMS)	strcat(aopts, "r");
		if (aflags & GET_LINENUM)	strcat(aopts, "n");
		if (aflags & GET_SERIAL)	strcat(aopts, "S");
		if (aflags & GET_USER)		strcat(aopts, "u");
	} else {
		strcpy(aopts, "--");
	}
	cmd = addLine(cmd, aopts);
	/*
	 * The rest of the args are files.  We want to know if we are
	 * processing multiple files.
	 */
	while (av[optind]) {
		cmd = addLine(cmd, av[optind++]);
		args++;
	}
	if (!opts.nonames &&
	    (streq("-", av[optind-1]) || (args > 1) || (args == 0))) {
		if (streq(aopts, "--")) {
			strcpy(aopts, "-ap");
			opts.style = 'a';
		} else {
	    		unless (strchr(aopts, 'p') || strchr(aopts, 'b')) {
				strcat(aopts, "p");
			}
		}
	}
	if (streq(aopts, "--")) {
		strcpy(aopts, "-Anone");
	} else {
		opts.seps = strlen(aopts) - strlen("-A");
	}

	/* force a null entry at the end for spawn */
	cmd = addLine(cmd, "");
	cmd[nLines(cmd)] = 0;
	putenv("BK_PRINT_EACH_NAME=YES");
	f = popenvp(&cmd[1], "r");
	doit(f, re);
	free(re);
	if (pclose(f)) exit(2);
	exit(opts.found ? 0 : 1);
}

typedef	struct {
	char	*buf;
	int	bufsiz;
} line;

private	line	*lines;	/* array of opts.before+1 lines */
private	char	*lower;
private	int	cur, lsiz, nlines;

private char	*
grep_getline(FILE *f)
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
	unless (strlen(p) < (nchar-1)) goto realloc;
	chomp(p);
	return (l->buf);
}

private void
doit(FILE *f, pcre *re)
{
	char	*p, *file = strdup("?");
	int	match, i, j, k, n;
	int	first = 1, skip = 0, print = 0;
	int	lastprinted = 0;
	char	*buf = 0;

	opts.line = 0;
	while (buf = grep_getline(f)) {
		if ((buf[0] == '|') && (p = getfile(buf))) {
			unless (first) done(file);
			opts.line = first = skip = 0;
			print = lastprinted = 0;
			free(file);
			file = strdup(p);
			continue;
		}
		if (skip) continue;
		opts.line++;
		switch (opts.style) {
		    case 'A':
			p = strchr(buf, '|');
			assert(p);
			p += 2;
			break;
		    case 'a':
			for (p = buf, k = opts.seps; k--; ) {
				p = strchr(p+1, '\t');
				assert(p);
				*p =':';
			}
			p++;
			break;
		    default:
			p = buf;
			break;
		}
		if (opts.nocase) {
			for (i = 0; p[i]; i++) lower[i] = tolower(p[i]);
			lower[i] = 0;
			match = !pcre_exec(re, 0,
			    lower, strlen(lower), 0, 0, 0, 0);
		} else {
			match = !pcre_exec(re, 0, p, strlen(p), 0, 0, 0, 0);
		}
		if (opts.invert) match = !match;
		unless (match || print) continue;
		if (match) {
			print = opts.after;
		} else {
			print--;
		}
		opts.found = 1;
		if (opts.quiet) return;
		if (opts.list) {
			unless (file) file = "(standard input)";
			printf("%s\n", file);
			skip = 1;
			continue;
		}
		if (opts.List) continue;
		if (match && (opts.before || opts.after) && lastprinted &&
				((lastprinted + 1 + opts.before) < opts.line)) {
			puts("--");
		}
		opts.firstmatch = 0;
		if (match && opts.before) {
			n = opts.line;
			for (i=max(n-opts.before, lastprinted+1); i < n; ++i) {
				j = (cur + (i-n) + nlines) % nlines;
				if (opts.seps) {
					int	k;

					p = lines[j].buf;
					for (k = opts.seps; k--; ) {
						p = strchr(p+1, ':');
						*p = '-';
					}
				}
				puts(lines[j].buf);
			}
		}
		if (!match && opts.after) {
			for (p = buf, k = opts.seps; p && k--; ) {
				p = strchr(p+1, ':');
				*p = '-';
			}

		}
		puts(buf);
		lastprinted = opts.line;
	}
	done(file);
}

private void
done(char *file)
{
	if (!opts.found && opts.List) {
		unless (file) file = "(standard input)";
		printf("%s\n", file);
		return;
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
	unless (adler32(0, file, strlen(file)) == strtoul(p + 5, 0, 10)) {
		*p = '|';
		return (0);
	}
	return (file);
}
