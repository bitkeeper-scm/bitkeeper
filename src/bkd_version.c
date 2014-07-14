#include "sccs.h"
#include "bkd.h"
#include "logging.h"

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
	float	exp;
	time_t	now = time(0);
	int	seats;
	char	*key, *t;
	char	buf[MAXLINE];

	buf[0] = 0;
	fprintf(f, "BitKeeper version is ");
	if (*bk_tag) fprintf(f, "%s ", bk_tag);
	fprintf(f, "%s for %s\n", bk_utc, bk_platform);
	fflush(f);	/* put out ver info while waiting for lease */
	/* get a lease, but don't fail */
	if (key = lease_bkl(0, 0)) {
		license_info(key, buf, 0, &seats);
		fprintf(f, "Options: %s\n", buf);
		if (seats) fprintf(f, "MaxSeats: %d\n", seats);
		fprintf(f, "Customer ID: %.12s\n", key + 12);
		free(key);
	}
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

	if (test_release) {
		exp = ((time_t)bk_build_timet - now + 3*WEEK) / (float)DAY;
		if (exp > 0) {
			fprintf(f, "Expires in: %.1f days (test release).\n",
			    exp);
		} else {
			fprintf(f, "Expired (test release).\n");
		}
	}
}
