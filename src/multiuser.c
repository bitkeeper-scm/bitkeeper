/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
private int	newroot(int single, char *ranbits);
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
	exit(newroot(1, 0));
}

int
newroot_main(int ac, char **av)
{
	int	c;
	char	*ranbits = 0;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		system("bk help -s newroot");
		return (1);
	}
	while ((c = getopt(ac, av, "k:")) != -1) {
		switch (c) {
		    case 'k': ranbits = optarg; break;
		    default: goto usage;
		}
	}
	if (ranbits) {
		u8	*p;
		if (strlen(ranbits) != 16) {
k_err:			fprintf(stderr,
			    "ERROR: -k option must have 16 lower case "
			    "hex digits\n");
			goto usage;
		}
		for (p = ranbits; *p; p++) {
			unless (isxdigit(*p)) break;
			if (isupper(*p)) break;
		}
		if (*p) goto k_err;
	}
	exit(newroot(0, ranbits));
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
newroot(int single, char *ranbits)
{
	sccs	*s;
	delta	*d;
	int	rc = 0;
	char	cset[] = CHANGESET;
	char	buf[MAXPATH];
	char	key[MAXKEY];
	FILE	*f;

	if (proj_cd2root()) {
		fprintf(stderr, "Cannot find package root.\n");
		exit(1);
	}
	unless ((s = sccs_init(cset, 0)) && HASGRAPH(s)) {
		fprintf(stderr, "Cannot init ChangeSet.\n");
		exit(1);
	}
	if (single && !(s->tree->xflags & X_SINGLE)) {
		fprintf(stderr, "Already converted.\n");
		exit(0);
	}
	if (ranbits) {
		if (strlen(ranbits) > MAXPATH - 1) {
			fprintf(stderr, "Rootkey too long\n");
			exit(1);
		}
		strcpy(buf, ranbits);
	}
	else {
		randomBits(buf);
	}
	if (s->tree->random) {
		if (streq(buf, s->tree->random)) {
			fprintf(stderr,
			    "newroot: error: new key matches old\n");
			exit (1);
		}
		free(s->tree->random);
	}
	s->tree->random = strdup(buf);
	sccs_sdelta(s, sccs_ino(s), key);
	sccs_newchksum(s);
	sccs_free(s);
	f = popen("bk sfiles", "r");
	while (fnext(buf, f)) {
		chop(buf);
		s = sccs_init(buf, 0);
		unless (s && HASGRAPH(s)) {
			rc = 1;
			if (s) sccs_free(s);
			continue;
		}
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
