#include "bkd.h"



int
csets_main(int ac, char **av)
{
	int	diffs = 0, gui = 1, verbose = 0, standalone = 0, resync = 0;
	int	stats = 0;
	int	c;
	int	rc = 0;
	longopt	lopts[] = {
		{ "stats", 320 }, /* --stats */
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "DTv", lopts)) != -1) {
		switch (c) {
		    case 'D': diffs = 1; break;
		    case 'T': gui = 0; break;
		    case 'v': gui = 0; verbose = 1; break;
		    case 320:	/* --stats */
			gui = 0; diffs = 1;
			stats = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	bk_nested2root(standalone);
	if (diffs) {
		char	*range;

		range = backtick("bk range -u", &rc);
		if (rc) goto out;
		if (gui) {
			rc = systemf("bk difftool -r%s", range);
		} else {
			rc = systemf("bk rset --elide -Hr%s | bk diffs %s -",
			    range, stats ? "--stats-only" : "-Hpu");
		}
		goto out;
	}
	if (exists("RESYNC/BitKeeper/etc/csets-in")) {
		chdir("RESYNC");
		resync = 1;
	}
	if (exists("BitKeeper/etc/csets-in")) {
		if (gui) {
			printf("Viewing %sBitKeeper/etc/csets-in\n",
			    resync ? "RESYNC/" : "");
			rc = system("bk changes -nd:I: - "
			    "< BitKeeper/etc/csets-in "
			    "| bk csettool -");
		} else {
			rc = systemf("bk changes %s - "
			    "< BitKeeper/etc/csets-in", verbose ? "-v" : "");
		}
	} else {
		fprintf(stderr, "Cannot find csets to view.\n");
		rc = 1;
	}
out:	return (rc);
}

/*
 * XXX - this really needs to be rewritten to be general like the rest of
 * the commands.
 * XXX - really needs to be part of r2c.
 */
int
f2csets_main(int argc, char **argv)
{
	char	rootkey[MAXKEY];
	FILE	*f;
	char	*sfile;
	char	buf[MAXPATH+100];
	char	key[MAXKEY*2];
	int	keylen;
	char	*p;
	sccs	*s;

	unless (argc == 2) usage();

	/* build the SCCS file from the filename, then init it and get
	 * the rootkey and the bk root out of that
	 */
	sfile = name2sccs(argv[1]);
	unless (s = sccs_init(sfile, INIT_MUSTEXIST|INIT_NOCKSUM|INIT_NOSTAT)){
		return(1);
	}
	free(sfile);

	has_proj("f2csets");
	sccs_sdelta(s, sccs_ino(s), rootkey);
	sccs_free(s);

	sprintf(buf, "bk -R annotate -R -ar -h " CHANGESET);

	unless (f = popen(buf, "r")) {
		perror(buf);
		return(1);
	}

	keylen = strlen(rootkey);
	while (fnext(key,f)) {
		chop(key);
		for (p = key; *p && *p != '\t'; ++p) ;

		if (*p == '\t' && strneq(1+p, rootkey, keylen) &&
		    separator(1+p) == (1+p+keylen)) {
			*p = 0;
			printf("%s\n", key);
		}
	}
	pclose(f);
	return(0);
}
