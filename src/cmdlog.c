#include "system.h"
#include "sccs.h"
#include "range.h"

int
cmdlog_main(int ac, char **av)
{
	FILE	*f;
	time_t	t;
	char	*p, *p1, *equals;
	char	*version;
	char	*user;
	char	buf[MAXPATH*3];
	int	yelled = 0, c, ind;
	struct	{
		u32	all:1;	   /* -a: show cmd_log instead of repo_log */
		u32	verbose:1; /* -v: print raw logfile */

		time_t	cutoff;    /* -c: cutoff date */
		char	*pattern;
		search	search;
	} opts;

	unless (proj_root(0)) return (1);
	memset(&opts, 0, sizeof(opts));
	while ((c = getopt(ac, av, "ac;v", 0)) != -1) {
		switch (c) {
		    case 'a': opts.all = 1; break;
		    case 'c': opts.cutoff = range_cutoff(optarg + 1); break;
		    case 'v': opts.verbose = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) {
		if (av[optind][0] != '^' && !opts.all) {
			opts.pattern = aprintf("^%s/", av[optind]);
		} else {
			opts.pattern = aprintf("%s/", av[optind]);
		}
		opts.search = search_parse(opts.pattern);
	}
	concat_path(buf, proj_root(0), "/BitKeeper/log/");
	concat_path(buf, buf, (opts.all ? "cmd_log" : "repo_log"));
	f = fopen(buf, "r");
	unless (f) return (1);
	while (fgets(buf, sizeof(buf), f)) {
		user = buf;
		for (p = log_versions; *p; ++p) {
			if (*p == buf[0]) {
				if ((p-log_versions) > LOGVER) {
					if (yelled) goto nextline;
					printf("cannot display this "
					       "log entry; please upgrade\n");
					yelled = 1;
					goto nextline;
				}
				user = buf+1;
				break;
			}
		}

		for (p = user; (*p != ' ') && (*p != '@'); p++);
		*p++ = 0;
		t = strtoul(p, &version, 0);
		while (isspace(*version)) ++version;
		if (*version == ':') {
			p = version;
			*p = 0;
			version = 0;
		} else {
			char *q;

			unless (p = strchr(p, ':')) continue;
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
		unless (t >= opts.cutoff) continue;
		p+=2; // skip colon and extra space inserted by log indenting
		chomp(p);
		/* find indent, if any */
		for (p1 = p; isspace(*p1); p1++);
		ind = p1 - p;

		/*
		 * if we got a pattern, do pattern matching
		 * skip this log entry on no match
		 */
		if (opts.pattern) {
			unless (search_either(p, opts.search)) continue;
			/*
			 * if somebody is looking for foo, then
			 * they'll also match the "bk cmdlog foo" so
			 * don't print ourselves, unless we are
			 * actually looking for cmdlog entries
			 */
			unless (strneq(opts.pattern, "cmdlog", 6) ||
			    strneq(opts.pattern, "^cmdlog", 7)) {
				if (strneq(p, "cmdlog ", 7)) continue;
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
		if (!opts.verbose && (!equals || ind)) continue;

		/* output requested annotations */
		if (opts.verbose) {
			printf("%s %.19s %14s ",
			    user, ctime(&t), version ? version : "");
		}
		if (equals) *equals = 0;

		/*
		 * Output command.  If opts.verbose is set, do
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
		if (!opts.verbose && !strneq(p1, "bk ", 3)) {
			fputs("bk ", stdout);
		}
		fputs(p1, stdout);
		if (equals && opts.verbose) {
			*equals = ' ';
			printf("%s", equals);
		}
		printf("\n");

nextline:	;
	}
	fclose(f);
	if (opts.pattern) free(opts.pattern);
	return (0);
}
