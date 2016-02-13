#include "sccs.h"
#include "bkd.h"

/*
 * Show bkd version
 */
int
cmd_version(int ac, char **av)
{
	out("OK-");
	out(BKD_VERSION1_2);
	out("\n");
	return (0);
}

/*
 * remote enabled commands need to be in a bkd_*.c file
 */

int
version_main(int ac, char **av)
{
	int	c;
	char	*p;
	char	**remcmd, *freeme;
	int	i;
	int	justver = 0;
	int	rc;

	while ((c = getopt(ac, av, "s", 0)) != -1) {
		switch (c) {
		    case 's':
			justver = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}

	if (av[optind] && av[optind+1]) {
		fprintf(stderr,
		    "%s: only one URL on the command line\n", prog);
		return (1);
	}
	if (av[optind]) {
		freeme = aprintf("-q@%s", av[optind]);
		remcmd = addLine(0, "bk");
		remcmd = addLine(remcmd, freeme);
		for (i = 0; i < optind; i++) remcmd = addLine(remcmd, av[i]);
		remcmd = addLine(remcmd, 0);
		rc = spawnvp(_P_WAIT, "bk", &remcmd[1]);
		rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 99;
		freeLines(remcmd, 0);
		free(freeme);
		return (rc);
	}

	if (justver) {
		p = bk_vers;
		if (strneq(p, "bk-", 3)) p += 3;
		puts(p);
		return (0);
	}

	bkversion(stdout);
	return (0);
}

void
bkversion(FILE *f)
{
	FILE	*f1;
	time_t	now = time(0);
	char	*t;
	char	buf[MAXLINE];

	buf[0] = 0;
	fprintf(f, "BitKeeper version is ");
	if (*bk_tag) fprintf(f, "%s ", bk_tag);
	fprintf(f, "%s for %s\n", bk_utc, bk_platform);
	fflush(f);	/* put out ver info while waiting for lease */
	fprintf(f, "Built by: %s in %s\n", bk_build_user, bk_build_dir);

	strftime(buf, sizeof(buf), "%a %b %d %Y %H:%M:%S %Z",
	    localtimez(&bk_build_timet, 0));
	fprintf(f, "Built on: %s (%s ago)\n", buf,
	    age(now - bk_build_timet, " "));

	fprintf(f, "Running on: %s\n", platform());

	/* latest version information */
	concat_path(buf, getDotBk(), "latest-bkver");
	if (f1 = fopen(buf, "r")) {
		fnext(buf, f1);
		chomp(buf);
		fclose(f1);
		if (t = strchr(buf, ',')) {
			*t++ = 0;
		}
		unless (streq(buf, bk_vers)) {
			fprintf(f, "Latest version: %s", buf);
			if (t) {
				fprintf(f, " (released %s ago)",
				    age(now - sccs_date2time(t, 0), " "));
			}
			fprintf(f, "\n");
		}
	}
}
