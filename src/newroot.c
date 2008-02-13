/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
private int	newroot(char *ranbits, int quiet);

int
newroot_main(int ac, char **av)
{
	int	c, quiet = 0;
	char	*ranbits = 0;
	u8	*p;

	while ((c = getopt(ac, av, "k:q")) != -1) {
		switch (c) {
		    case 'k': ranbits = optarg; break;
		    case 'q': quiet = 1; break;
		    default:
usage:			sys("bk", "help", "-s", "newroot", SYS);
			return (1);
		}
	}
	if (ranbits && !strneq("B:", ranbits, 2)) {
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
	return (newroot(ranbits, quiet));
}

/*
 * Generate a new ROOTKEY
 * Update the csetfile pointer in all files.
 */
private int
newroot(char *ranbits, int quiet)
{
	sccs	*s;
	int	rc = 0;
	char	*p;
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
		if (strneq(ranbits, "B:", 2)) {
			if (strneq("B:", s->tree->random, 2)) return (0);
			sprintf(buf, "%s%s", ranbits, s->tree->random);
			assert(strlen(buf) < MAXPATH - 1);
		} else {
			p = buf;
			if (strneq("B:", s->tree->random, 2)) {
				p = strrchr(s->tree->random, ':');
				assert(p);
				strncpy(buf, s->tree->random,
				    p - s->tree->random + 1);
				p = buf + (p - s->tree->random + 1);
			}
			if ((p - buf + strlen(ranbits)) > (MAXPATH - 1)) {
				fprintf(stderr, "Rootkey too long\n");
				exit(1);
			}
			strcpy(p, ranbits);
		}
	} else {
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
	unless (quiet) {
		fprintf(stderr, "Pointing files at new changeset id...\n");
	}
	while (fnext(buf, f)) {
		chop(buf);
		s = sccs_init(buf, 0);
		unless (s && HASGRAPH(s)) {
			rc = 1;
			if (s) sccs_free(s);
			continue;
		}
		unless (quiet) fprintf(stderr, "%-78s\r", s->gfile);
		free(s->tree->csetFile);
		s->tree->csetFile = strdup(key);
		if (sccs_newchksum(s)) rc = 1;
		sccs_free(s);
	}
	pclose(f);
	unless (quiet) fprintf(stderr, "\n");
	return (rc);
}
