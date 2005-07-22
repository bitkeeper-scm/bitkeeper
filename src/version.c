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
	lease_checking(0);
	bkversion(stdout);
	lease_checking(1);
	return (0);
}

void
bkversion(FILE *f)
{
	FILE	*f1;
	u32	o;
	float	exp;
	char	*p;
	char	buf[MAXLINE];

	buf[0] = 0;
	switch (bk_mode(0)) {
	    case BK_SINGLE: strcpy(buf, "/Single"); break;
	    case BK_FREE: strcpy(buf, "/Free"); break;
	    case BK_PRO: 
		p = eula_type();
		if (streq(p, "none")) goto look;
		sprintf(buf, "/%s", p);
		break;
	    default:
		/* Go see if we can find anything that looks commercial */
look:		if (p = lease_anycommercial()) {
			sprintf(buf, "/%s", p);
			free(p);
		}
	}
	if (buf[0]) {
		o = bk_options(0);
		if (o & BKOPT_WEB) strcat(buf, ",bkweb");
		if (o & BKOPT_EVAL) strcat(buf, ",eval");
	}

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
