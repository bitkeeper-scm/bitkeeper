/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
private int	newroot(char *ranbits);

int
newroot_main(int ac, char **av)
{
	int	c;
	char	*ranbits = 0;

	while ((c = getopt(ac, av, "k:")) != -1) {
		switch (c) {
		    case 'k': ranbits = optarg; break;
		    default:
usage:			sys("bk", "help", "-s", "newroot", SYS);
			return (1);
		}
	}
	if (ranbits) {
		u8	*p;
		if (strlen(ranbits) > 16) {
k_err:			fprintf(stderr,
			    "ERROR: -k option can have at most 16 lower case "
			    "hex digits\n");
			goto usage;
		}
		for (p = ranbits; *p; p++) {
			unless (isxdigit(*p)) break;
			if (isupper(*p)) break;
		}
		if (*p) goto k_err;
	}
	return (newroot(ranbits));
}

/*
 * Generate a new ROOTKEY
 * Update the csetfile pointer in all files.
 */
private int
newroot(char *ranbits)
{
	sccs	*s;
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
	unlink("BitKeeper/log/ROOTKEY");
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
		if (sccs_newchksum(s)) rc = 1;
		sccs_free(s);
	}
	pclose(f);
	return (rc);
}
