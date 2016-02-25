/*
 * Copyright 2011-2013,2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"
#include "range.h"
#include "nested.h"

typedef struct {
	u32	all:1;	      /* -a: show cmd_log instead of repo_log */
	u32	verbose:1;    /* -v: print raw logfile */
	u32	standalone:1; /* -S: standalone */
	u32	relative:1;   /* -r: relative */
	time_t	cutoff;       /* -c: cutoff date */
	char	*pattern;
	search	search;
	int	complen;      /* max component pathname (for -N) */
} Opts;

typedef struct {
	char	*prefix;	/* component owning log */
	FILE	*fp;		/* open to comp's cmd_log file */
	char	*buf;		/* next line from logfile */
	struct timeval tv;	/* timestamp on this line */
} logmux;

private void	parsePrint(Opts *opts, char *prefix, char *buf);
private void	parseTimestamp(logmux *lmp);
private int	logmux_oldest(char **lmlist);
private void	logmux_free(void *lmp);

int
cmdlog_main(int ac, char **av)
{
	Opts	*opts;
	FILE	*f;
	nested	*n;
	comp	*cp;
	int	c, i, nested;
	char	*t, *log;
	logmux	*lmp;
	char	**lmlist = 0;
	char	buf[MAXPATH];
	longopt	lopts[] = {
		{ "standalone", 'S' },
		{ 0, 0 }
	};

	opts = new(Opts);
	while ((c = getopt(ac, av, "Sac;rv", lopts)) != -1) {
		switch (c) {
		    case 'S': opts->standalone = 1; break;
		    case 'a': opts->all = 1; break;
		    case 'c': opts->cutoff = range_cutoff(optarg + 1); break;
		    case 'r':
			opts->relative = opts->verbose = opts->all = 1;
			break;
		    case 'v': opts->verbose = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	nested = bk_nested2root(opts->standalone);
	if (av[optind]) {
		if (av[optind+1]) usage();
		if (av[optind][0] != '^' && !opts->all) {
			opts->pattern = aprintf("^%s/", av[optind]);
		} else {
			opts->pattern = aprintf("%s/", av[optind]);
		}
		opts->search = search_parse(opts->pattern);
	}

	log = opts->all ? "cmd_log" : "repo_log";

	/* standalone case */
	unless (nested) {
		concat_path(buf, proj_root(0), "/BitKeeper/log/");
		concat_path(buf, buf, log);
		unless (f = fopen(buf, "r")) return (1);
		while (t = fgetline(f)) parsePrint(opts, 0, t);
		fclose(f);
		if (opts->pattern) free(opts->pattern);
		return (0);
	}

	n = nested_init(0, 0, 0, NESTED_PENDING);
	assert(n);
	EACH_STRUCT(n->comps, cp, i) {
		unless (C_PRESENT(cp)) continue;
		lmp = new(logmux);
		lmp->prefix = strdup(cp->path);
		c = strlen(cp->path);
		if (c > opts->complen) opts->complen = c;
		sprintf(buf, "%s/BitKeeper/log/%s", lmp->prefix, log);
		unless (exists(buf)) {
			lmp->fp = 0;
			logmux_free(lmp);
			continue;
		}
		unless (lmp->fp = fopen(buf, "r")) {
			perror("fopen");
			return (1);
		}
		/* grab the first line, if any */
		unless (lmp->buf = fgetline(lmp->fp)) {
			logmux_free(lmp);
			continue;
		}
		parseTimestamp(lmp);
		lmlist = addLine(lmlist, lmp);
	}
	nested_free(n);

	while (i = logmux_oldest(lmlist)) {
		lmp = (logmux *)lmlist[i];
		parsePrint(opts, lmp->prefix, lmp->buf);
		unless (lmp->buf = fgetline(lmp->fp)) {
			removeLineN(lmlist, i, logmux_free);
			continue;
		}
		parseTimestamp(lmp);
	}
	free(opts);
	return (0);
}

private void
parsePrint(Opts *opts, char *prefix, char *buf)
{
	time_t	t;
	u32	usec = 0;
	char	*p, *p1, *equals;
	char	*version;
	char	*user;
	int	yelled = 0, ind, logver = 0;
	double	rmsec = 0.0;

	user = buf;
	for (p = log_versions; *p; ++p) {
		if (*p == buf[0]) {
			logver = p - log_versions;
			if (logver > LOGVER) {
				if (yelled) return;
				printf("cannot display this "
				    "log entry; please upgrade\n");
				yelled = 1;
				return;
			}
			user++;
			break;
		}
	}

	for (p = user; (*p != ' ') && (*p != '@'); p++);
	*p++ = 0;
	t = strtoul(p, &version, 10);
	while (isspace(*version)) ++version;
	if (logver > 0) {
		p = version + 1; // skip decimal point
		usec = strtoul(p, &version, 10);
		p = version;
		rmsec = strtod(p, &version);
		while (isspace(*version)) ++version;
	}
	if (*version == ':') {
		p = version;
		*p = 0;
		version = 0;
	} else {
		char *q;

		unless (p = strchr(p, ':')) return;
		*p = 0;

		unless (isalnum(*version)) {
			version = 0;
		} else  {
			for (q = 1+version; *q; ++q) {
				if ( (*q & 0x80) || (*q < ' ') ) {
					version = 0;
					break;
				}
			}
		}
	}
	unless (t >= opts->cutoff) return;
	p+=2; // skip colon and extra space inserted by log indenting
	chomp(p);
	/* find indent, if any */
	for (p1 = p; isspace(*p1); p1++);
	ind = p1 - p;

	/*
	 * if we got a pattern, do pattern matching
	 * skip this log entry on no match
	 */
	if (opts->pattern) {
		unless (search_either(p, opts->search)) return;
		/*
		 * if somebody is looking for foo, then
		 * they'll also match the "bk cmdlog foo" so
		 * don't print ourselves, unless we are
		 * actually looking for cmdlog entries
		 */
		unless (strneq(opts->pattern, "cmdlog", 6) ||
		    strneq(opts->pattern, "^cmdlog", 7)) {
			if (strneq(p, "cmdlog ", 7)) return;
		}
	}

	/*
	 * - the presence of a indent means this was not a user
	 *   run command.
	 * - if there is no equal sign it's not a command completion
	 * - if we have no verbose flag, we skip all but entries
	 *   for completed user commands
	 */
	equals = strstr(p, " = ");
	if (!opts->verbose && (!equals || ind)) return;

	/* output requested annotations */
	if (opts->verbose) {
		switch (logver) {
		    case 0:
			printf("%-8s %.19s %14s ",
			    user, ctime(&t), version ? version : "");
			break;
		    case 1:
			if (opts->relative) {
				printf("%.3fs ", rmsec);
			} else {
				printf("%-8s %.19s.%03u %14s ",
				    user, ctime(&t), usec / 1000,
				    version ? version : "");
			}
			break;
		    default:
			// Should never happen
			printf("Unknown log version ");
			break;
		}
		if (prefix) printf("[%-*s] ", opts->complen, prefix);
	}
	if (equals) *equals = 0;

	/*
	 * Output command.  If opts->verbose is set, do
	 * not prefix with "bk ".
	 * Note: the ind+1 is to balance the cmd_log being
	 * written with printf("%*s", x, " "); which
	 * prints out 1 space for x = 0, otherwise it prints x
	 * spaces.  Above (the +=2) we eat one space
	 * corresponding to x = 0, so x = 4 means 3 spaces,
	 * and x = 8 means 7 spaces.  So we're unmathing it and
	 * getting it to look regular.
	 */
	if (ind) printf("%*s", ind+1, " ");
	if (!opts->verbose && !strneq(p1, "bk ", 3)) {
		fputs("bk ", stdout);
	}
	fputs(p1, stdout);
	if (equals && opts->verbose) {
		*equals = ' ';
		printf("%s", equals);
	}
	printf("\n");
}

private void
parseTimestamp(logmux *lmp)
{
	char	*p, *p1;
	int	logver = 0;

	for (p = log_versions; *p; ++p) {
		if (*p == lmp->buf[0]) {
			logver = p - log_versions;
			break;
		}
	}
	for (p = lmp->buf; *p != ' '; p++);
	p++;
	lmp->tv.tv_sec = strtoul(p, &p1, 10);
	if (logver > 0) {
		lmp->tv.tv_usec = strtoul(++p1, 0, 10);
	} else {
		lmp->tv.tv_usec = 0;
	}
}

/*
 * Pick the nested cmd_log line to print from an array of the next line
 * from each component.
 * Lines are sorted by time.
 */
private int
logmux_oldest(char **lmlist)
{
	int	i, ret = 0;
	logmux	*lmp, *oldest = 0;

	EACH_STRUCT(lmlist, lmp, i) {
		if (!oldest ||
		    ((timercmp(&lmp->tv, &oldest->tv, ==)
			? (strcmp(lmp->prefix, oldest->prefix) < 0)
			: timercmp(&lmp->tv, &oldest->tv, <)))) {
			oldest = lmp;
			ret = i;
		}
	}

	return (ret);
}

private void
logmux_free(void *lmp)
{
	logmux	*p = lmp;

	free(p->prefix);
	if (p->fp) fclose(p->fp);
	free(p);
}
