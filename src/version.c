#include "system.h"
#include "sccs.h"
#include "logging.h"

extern	int	test_release;
extern	unsigned build_timet;

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
			return (0);
		}
	}
	bkversion(stdout);
	return (0);
}

void
bkversion(FILE *f)
{
	FILE	*f1;
	u32	bits;
	float	exp;
	char	*key, *t;
	char	buf[MAXLINE];

	lease_refresh();	/* get a lease, but don't fail */
	key = lease_latestbkl();
	if (key) {
		bits = license_bklbits(key);
		if (t = eula_type(bits)) sprintf(buf, "/%s", t);
	} else {
		buf[0] = 0;
		bits = 0;
	}

	// XXX - I really want this to have it's own line
	if (bits & LIC_WEB) strcat(buf, ",bkweb");
	if (bits & LIC_EVAL) strcat(buf, ",eval");
	if (bits & LIC_IMPORT) strcat(buf, ",import");
	if (bits & LIC_BUGDB) strcat(buf, ",bugdb");
	getMsg("version", buf, 0, f);

	if (f1 = popen("uname -s -r", "r")) {
		if (fnext(buf, f1)) {
			chomp(buf);
			fprintf(f, "Running on: %s\n", buf);
		}
		fclose(f1);
	}
	if (test_release) {
		exp = ((time_t)build_timet - time(0)) / (24*3600.0) + 14;
		if (exp > 0) {
			fprintf(f, "Expires in: %.1f days (test release).\n",
			    exp);
		} else {
			fprintf(f, "Expired (test release).\n");
		}
	}
}
