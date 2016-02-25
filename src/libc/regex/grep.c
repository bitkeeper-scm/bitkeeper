/*
 * Copyright 2003,2006,2016 BitMover, Inc
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

#include "regex.h"
#include "system.h"
#undef	malloc		/* XXX - remove when we integrate with bk */
#undef	system		/* XXX - remove when we integrate with bk */
#undef	exit		/* XXX - remove when we integrate with bk */

private	void	doit(char *file, FILE *f);
private	char	*ignore(char *s);
private void	context(char *arg);

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
main(int ac, char **av)
{
	int	c;
	char	*pat;
	FILE	*f;

	opts.name = 1;
	while ((c = getopt(ac, av, "a;A;cC|d;hilLnqr;R|vx")) != EOF) {
		switch (c) {
		    case 'a':				/* BK anno opt */
		    case 'A':				/* BK anno opt */
		    case 'd':				/* BK date opt */
		    case 'r':				/* BK rev opt */
		    case 'R':				/* BK range opt */
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
	pat = opts.nocase ? ignore(av[optind]) : av[optind];
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
	if (re_comp(pat)) exit(3);
	if (av[++optind]) {
		while (av[optind]) {
			if ((f = fopen(av[optind], "r"))) {
				doit(av[optind], f);
				fclose(f);
			} else {
				exit(2);
			}
			optind++;
		}
	} else {
		doit(0, stdin);
	}
	exit(opts.found ? 0 : 1);
}

void
doit(char *file, FILE *f)
{
	int	match;
	u32	count = 0;
	char	buf[BUFSIZ];

	opts.line = 0;
	while (fgets(buf, sizeof(buf), f)) {
		chomp(buf);
		opts.line++;
		match = re_exec(buf);
		if (opts.invert) match = !match;
		if (match) {
			count++;
			if (opts.quiet) exit(0);
			opts.found = 1;
			if (opts.list) {
				unless (file) file = "(standard input)";
				printf("%s\n", file);
				return;
			}
			unless (opts.List || opts.count) {
				if (file && opts.name) printf("%s:", file);
				if (opts.lineno) printf("%d:", opts.line);
				puts(buf);
			}
		}
	}
	if (!opts.found && opts.List) {
		unless (file) file = "(standard input)";
		printf("%s\n", file);
		return;
	}
	if (opts.count) {
		if (file) printf("%s:", file);
		printf("%u\n", count);
	}
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

/* sleazy beyond belief.  I suspect there is some way to wack the tables
 * above for case ignoring.
 */
private	char *
ignore(char *s)
{
	char	*str = malloc(strlen(s) * 4);
	char	*p = str;

	assert(s && *s);
	while (*s) {
		if (isalpha(*s)) {
			*p++ = '[';
			*p++ = tolower(*s);
			*p++ = toupper(*s++);
			*p++ = ']';
		} else {
			*p++ = *s++;
		}
	}
	*p = 0;
	return (str);
}
