/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * multiuser - convert a repository to multi-user from single user.
 *
 * Start with the ChangeSet file, bail if it is not single.
 * Generate a new ROOTKEY by changing the first char of the random bits to "+".
 * Update the csetfile pointer in all files.
 * Update all xflags.
 * Update the config file.
 * Create a changeset.
 */
int
multiuser_main(int ac, char **av)
{
	sccs	*s;
	delta	*d;
	char	cset[] = CHANGESET;
	project	*proj = 0;
	int	rc = 0;
	char	buf[MAXPATH];
	char	key[MAXKEY];
	FILE	*f;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		system("bk help multiuser");
		return (1);
	}
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "Cannot find package root.\n");
		exit(1);
	}
	unless ((s = sccs_init(cset, 0, 0)) && s->tree) {
		fprintf(stderr, "Cannot init ChangeSet.\n");
		exit(1);
	}
	unless (s->tree->xflags & X_SINGLE) {
		fprintf(stderr, "Already converted.\n");
		exit(0);
	}
	sprintf(buf, "+%s", s->tree->random);
	free(s->tree->random);
	s->tree->random = strdup(buf);
	sccs_sdelta(s, s->tree, key);
	sccs_newchksum(s);
	sccs_free(s);
	f = popen("bk sfiles", "r");
	while (fnext(buf, f)) {
		chop(buf);
		unless ((s = sccs_init(buf, INIT_SAVEPROJ, proj)) && s->tree) {
			rc = 1;
			if (s) sccs_free(s);
			continue;
		}
		unless (proj) proj = s->proj;
		free(s->tree->csetFile);
		s->tree->csetFile = strdup(key);
		for (d = s->table; d; d = d->next) {
			if (d->xflags & X_SINGLE) d->xflags &= ~X_SINGLE;
		}
		if (sccs_newchksum(s)) rc = 1;
		sccs_free(s);
	}
	pclose(f);
	system("bk get -egq BitKeeper/etc/config; "
	    "bk get -qkp BitKeeper/etc/config | "
	    "grep -v '^single_' > BitKeeper/etc/config;"
	    "bk delta -qy'Convert to multi user' BitKeeper/etc/config");
	system("bk commit -y'Convert to multi user'");
	return (rc);
}
