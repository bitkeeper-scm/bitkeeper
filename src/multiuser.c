/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
private int	newroot(int single);
WHATSTR("@(#)%K%");

/*
 * multiuser - convert a repository to multi-user from single user.
 */
int
multiuser_main(int ac, char **av)
{
	if (((ac == 2) && streq("--help", av[1])) || (ac != 1)) {
		close(0);
		system("bk help -s multiuser");
		return (1);
	}
	exit(newroot(1));
}

int
newroot_main(int ac, char **av)
{
	if (((ac == 2) && streq("--help", av[1])) || (ac != 1)) {
		close(0);
		system("bk help -s newroot");
		return (1);
	}
	exit(newroot(0));
}

/*
 * Start with the ChangeSet file, bail if it is not single && want single.
 * Generate a new ROOTKEY by adding "+" to the end of random bits.
 * Update the csetfile pointer in all files.
 * Update all xflags (single case).
 * Update the config file (single case).
 * Create a changeset (single case).
 */
private int
newroot(int single)
{
	sccs	*s;
	delta	*d;
	project	*proj = 0;
	int	rc = 0;
	char	cset[] = CHANGESET;
	char	buf[MAXPATH];
	char	key[MAXKEY];
	FILE	*f;

	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "Cannot find package root.\n");
		exit(1);
	}
	unless ((s = sccs_init(cset, 0, 0)) && s->tree) {
		fprintf(stderr, "Cannot init ChangeSet.\n");
		exit(1);
	}
	if (single && !(s->tree->xflags & X_SINGLE)) {
		fprintf(stderr, "Already converted.\n");
		exit(0);
	}
	randomBits(buf);
	assert(!streq(buf, s->tree->random));
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
		for (d = s->table; d && single; d = d->next) {
			if (d->xflags & X_SINGLE) d->xflags &= ~X_SINGLE;
		}
		if (sccs_newchksum(s)) rc = 1;
		sccs_free(s);
	}
	pclose(f);
	unless (single) return (rc);
	system("bk get -egq BitKeeper/etc/config; "
	    "bk get -qkp BitKeeper/etc/config | "
	    "grep -v '^single_' > BitKeeper/etc/config;"
	    "bk delta -qy'Convert to multi user' BitKeeper/etc/config");
	system("bk commit -y'Convert to multi user'");
	return (rc);
}
