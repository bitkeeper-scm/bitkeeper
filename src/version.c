#include "system.h"
#include "sccs.h"
#include "logging.h"

int
version_main(int ac, char **av)
{
	int	c;
	char	*p;

	while ((c = getopt(ac, av, "s")) != -1) {
		switch (c) {
		    case 's':
			p = bk_vers;
			if (strneq(p, "bk-", 3)) p += 3;
			puts(p);
			return (0);
		    default:
			system("bk help -s version");
			return (1);
		}
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
	char	*key, *t;
	char	buf[MAXLINE];

	buf[0] = 0;
	/* get a lease, but don't fail */
	if (key = lease_bkl(0, 0)) {
		license_info(key, buf, 0);
		free(key);
	}
	fprintf(f, "BitKeeper version is ");
	if (*bk_tag) fprintf(f, "%s ", bk_tag);
	fprintf(f, "%s for %s\n", bk_utc, bk_platform);
	fprintf(f, "Options: %s\n", buf);
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
		exp = ((time_t)bk_build_timet - time(0)) / (24*3600.0) + 14;
		if (exp > 0) {
			fprintf(f, "Expires in: %.1f days (test release).\n",
			    exp);
		} else {
			fprintf(f, "Expired (test release).\n");
		}
	}
}
