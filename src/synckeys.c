/*
 * Copyright (c) 2000, Larry McVoy & Andrew Chang
 */
#include "bkd.h"

sccs *
mk_probekey(FILE *f)
{
	sccs	*s;
	delta	*d;
	int	i, j;
	char	key[MAXKEY];
	char	s_cset[] = CHANGESET;

	unless ((s = sccs_init(s_cset, 0, 0)) && s->tree) {
		fprintf(stderr, "Can't init changeset\n");
		exit(1);
	}

	fputs("@PROBE KEY@\n", f);
	/*
	 * Phase 1, send the probe keys.
	 * NB: must be in most recent to least recent order.
	 */
	d = sccs_top(s);
	for (i = 1; d && (d != s->tree); i *= 2) {
		for (j = i; d && --j; d = d->parent);
		if (d) {
			assert(d->type == 'D');
			sccs_sdelta(s, d, key);
			fprintf(f, "%s\n", key);
		}
	}

	/*
	 * Always send the root key
	 * We wnat to froce a match
	 * No match is a error condition
	 */
	sccs_sdelta(s, sccs_ino(s), key);
	fprintf(f, "%s\n", key);
	fputs("@END@\n", f);
	return (s);
}

int
probekey_main(int ac, char **av)
{
	sccs *s;

	s = mk_probekey(stdout);
	sccs_free(s);
	return (0);
}

int
listkey_main(int ac, char **av)
{
	sccs	*s;
	delta	*d = 0;
	int	quiet = 0;
	char	key[MAXKEY] = "", s_cset[] = CHANGESET;

	if (ac == 2 && streq(av[1], "-q")) quiet = 1;
	unless ((s = sccs_init(s_cset, 0, 0)) && s->tree) {
		fprintf(stderr, "Can't init changeset\n");
		return(1); /* cset error */
	}

	/*
	 * Phase 1, get the probe keys.
	 */
	if (getline(0, key, sizeof(key)) <= 0) {
		unless (quiet) {
			fprintf(stderr, "Expect \"@PROBE KEY\"@, Got EOF\n");
			return (2);
		}
	}
	unless (streq("@PROBE KEY@", key)) {
		unless (quiet) {
			fprintf(stderr,
				"Expect \"@PROBE KEY\"@, Got \"%s\"\n", key);
		}
		return(3); /* protocol error or repo locked */
	}

	while (getline(0, key, sizeof(key)) > 0) {
	if (streq("@END@", key)) break;
		if (d = sccs_findKey(s, key)) {
			break;
		}
	}

	/*
	 * Phase 2, send the non marked keys.
	 */
	sccs_color(s, d);
	unless (d) {
		out("@NO MATCH@\n");
		out("@END@\n");
		return (2); /* package key mismatch */
	}

	out("@MATCH@\n");
	sccs_sdelta(s, d, key);
	out(key);
	out("\n");
	for (d = s->table; d; d = d->next) {
		if (d->flags & D_VISITED) continue;
		unless (d->type == 'D') continue;
		sccs_sdelta(s, d, key);
		out(key);
		out("\n");
	}
	out("@END@\n");
	return (0);
}



int
prunekey(sccs *s, remote *r, int outfd, int quiet,
					int *local_only, int *remote_only)
{
	char	key[MAXKEY] = "";
	delta	*d;
	int	rc = 0, i = 0, j = 0, got_match = 0;	

	/*
	 * Receive the keys and eliminate those from the set
	 * that we can.
	 */
	if (getline2(r, key, sizeof(key)) > 0) {
		if (streq(key, "@MATCH@")) goto next;
		if (streq(key, "@NO MATCH@")) goto next;
		if (streq(key, "@EMPTY TREE@")) goto next;
		if (strneq(key, "@FAIL@-", 7)) goto next;
	}
	unless (quiet) {
		fprintf(stderr,
			"prunekey: expect @MATCH@|@NO MATCH@|@EMPTY TREE@, "
			"got \"%s\"\n", key);
	}
	return (-1);
	
next:	if (streq(key, "@MATCH@")) {
		unless (getline2(r, key, sizeof(key)) > 0) {
			perror("getline, expected key");
			exit(2);
		}
		d = sccs_findKey(s, key);
		assert(d);
		sccs_color(s, d);
		got_match = 1;
	} else if (streq(key, "@NO MATCH@")) {
		rc = -2;
		goto done;
	} else if (streq(key, "@EMPTY TREE@")) {
		rc = -3;
	} else 	if (strneq(key, "@FAIL@-", 7)) {
		unless (quiet) fprintf(stderr, "%s\n", &key[7]);
		rc = -1;
		goto done;
	} else {
		unless (quiet) fprintf(stderr, "unknow response: %s\n", key);
		return (-1);
	}

	/*
	 * Eliminate all keys that we might send but they have.
	 */
	if (got_match) {
		while (getline2(r, key, sizeof(key)) > 0) {
			if (streq("@END@", key)) break;
			if (d = sccs_findKey(s, key)) {
				d->flags |= D_VISITED;
			} else {
				i++;
			}
		}
		if (!streq("@END@", key))  {
			rc = -4; /* protocol error */
			goto done;
		}
	}

	for (d = s->table; d; d = d->next) {
		unless (d->type == 'D') continue;
		if (d->flags & D_VISITED) continue;
		sprintf(key, "%s\n", d->rev);
		write(outfd, key, strlen(key));
		j++;
	}
	*remote_only = i;	/* number of cset in remote only */
	*local_only = j;	/* number of cset in local only  */
	rc = j; 

done:	return (rc);
}
