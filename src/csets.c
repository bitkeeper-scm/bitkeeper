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
	if (p = strrchr(argv[1], '/')) {
		++p;
	} else {
		p = argv[1];
	}

	sprintf(buf, "%.*sSCCS/s.%s", (p-argv[1]), argv[1], p);

	unless (s = sccs_init(buf, INIT_NOCKSUM|INIT_NOSTAT, 0)) return(1);

	sccs_cd2root(s, 0);
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
