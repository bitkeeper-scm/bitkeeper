#include "bkd.h"

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
	int	i;
	int	keylen;
	char	*p;
	sccs	*s;

	unless (argc == 2) {
		fprintf(stderr, "usage: %s <file>\n", argv[0]);
		return(1);
	}

	/* build the SCCS file from the filename, then init it and get
	 * the rootkey and the bk root out of that
	 */
	sfile = name2sccs(argv[1]);
	unless (s = sccs_init(sfile, INIT_NOCKSUM|INIT_NOSTAT)) return(1);
	free(sfile);

	proj_cd2root();
	sccs_sdelta(s, sccs_ino(s), rootkey);
	sccs_free(s);

	i = sprintf(buf, "bk -R sccscat -hm " CHANGESET);

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
