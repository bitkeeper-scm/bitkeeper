#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

/*
 * r2c - convert rev to cset rev
 *
 * usage: file -r<rev> file
 *
 * XXX - expects to be standalone, one shot, does not free resources.
 */
int
main(int ac, char **av)
{
	char	*name;
	delta	*e;
	sccs	*s, *cset;
	char	buf[MAXPATH+200];
	char	tmpfile[MAXPATH];
	char	*t, *key, *shortkey;
	FILE	*f;
	int	len;
	RANGE_DECL;

	unless (av[1] && strneq(av[1], "-r", 2) && av[2] && !av[3]) {
		fprintf(stderr, "Usage: r2c -r<rev> file\n");
		exit(1);
	}
	name = name2sccs(av[2]);
	unless (s = sccs_init(name, INIT_NOCKSUM, 0)) {
		perror(name);
		exit(1);
	}
	if (sccs_cd2root(s, 0)) {
		fprintf(stderr, "r2c: can not find project root.\n");
		exit(1);
	}
	unless (e = sccs_getrev(s, &av[1][2], 0, 0)) {
		fprintf(stderr, "r2c: can't find rev like %s in %s\n",
		    &av[1][2], name);
	    	exit(1);
	}
	while (e && !(e->flags & D_CSET)) e = e->kid;
	unless (e) {
		fprintf(stderr,
		    "r2c: can not find cset marker at or below %s in %s\n",
		    &av[1][2], name);
	    	exit(1);
	}
	sccs_sdelta(s, e, buf);
	key = strdup(buf);
	if (t = sccs_iskeylong(buf)) {
		*t = 0;
		shortkey = strdup(buf);
	} else {
		shortkey = 0;
	}
	unless (cset = sccs_init(CHANGESET, INIT_NOCKSUM, 0)) {
		fprintf(stderr, "r2c: can not init ChangeSet\n");
		exit(1);
	}
	if (gettemp(tmpfile, "r2c")) {
		perror("gettemp");
		exit(1);
	}
	RANGE("r2c", cset, 2, 1);
	if (sccs_cat(cset, PRINT|GET_NOHASH|GET_REVNUMS, tmpfile)) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr, "r2c: sccscat of ChangeSet failed.\n");
		}
		exit(1);
	}
	unless (f = fopen(tmpfile, "r")) {
		perror(tmpfile);
		exit(1);
	}
	len = strlen(key);
	while (fnext(buf, f)) {
		/* 1.5\tkey key */
		t = strchr(buf, ' '); assert(t); t++;
		if (strneq(t, key, len) && t[len] == '\n') {
			t = strchr(buf, '\t'); assert(t); *t = 0;
			printf("%s\n", buf);
			goto out;
		}
	}
	unless (shortkey) {
notfound:	if (shortkey) {
			fprintf(stderr,
			    "r2c: can not find either of\n\t%s\n\t%s\n",
			    key, shortkey);
		} else {
			fprintf(stderr,
			    "r2c: can not find\n\t%s\n", key);
		}
		//unlink(tmpfile);
		exit(1);
	}
	rewind(f);
	len = strlen(shortkey);
	while (fnext(buf, f)) {
		/* 1.5 key key */
		t = strchr(buf, ' '); assert(t); t++;
		if (strneq(t, shortkey, len) && t[len] == '\n') {
			t = strchr(buf, '\t'); assert(t); *t = 0;
			printf("%s\n", buf);
			goto out;
		}
	}
	goto notfound;
next:	/* for range */
out:	unlink(tmpfile);
	exit(0);
}
