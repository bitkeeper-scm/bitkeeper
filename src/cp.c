/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
private int	cp(char *from, char *to, int force);

/*
 * cp - copy a file to another file,
 * removing the changeset marks,
 * generating new random bits,
 * and storing a new pathname.
 */
int
cp_main(int ac, char **av)
{
	int	force = 0;
	int	c;

	while ((c = getopt(ac, av, "f", 0)) != -1) {
		switch (c) {
		    case 'f': force = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	unless ((ac - optind) == 2) {
		close(0);	/* XXX - what is this for? */
		usage();
	}
	return (cp(av[optind], av[optind+1], force));
}

private int
cp(char *from, char *to, int force)
{
	sccs	*s;
	delta	*d;
	char	buf[100];
	char	*sfile, *gfile, *tmp;
	int	i, err;

	assert(from && to);
	unless (proj_samerepo(from, to) || force) {
		fprintf(stderr, "bk cp: must be in same repo or use cp -f.\n");
		return (1);
	}
	sfile = name2sccs(from);
	unless (s = sccs_init(sfile, 0)) return (1);
	unless (HASGRAPH(s) && s->cksumok) return (1);
	free(sfile);
	sfile = name2sccs(to);
	mkdirf(sfile);
	gfile = sccs2name(sfile);
	if (exists(sfile)) {
		fprintf(stderr, "%s: file exists\n", sfile);
		sccs_free(s);
		return (1);
	}
	if (exists(gfile)) {
		fprintf(stderr, "%s: file exists\n", gfile);
		sccs_free(s);
		return (1);
	}

	/*
	 * This code assumes we have only one set of random bits on the 1.0
	 * delta.  XXX - need to fix this if/when we support grafting.
	 */
	randomBits(buf);
	if (s->tree->random) {
		assert(!streq(buf, s->tree->random));
		free(s->tree->random);
	}
	s->tree->random = strdup(buf);

	/*
	 * Try using the new filename as the original filename.
	 * Only necessary in long/short key trees like BitKeeper.
	 */
	tmp = _relativeName(gfile, 0, 0, 0, 0);
	for (i = 1; i < s->nextserial; i++) {
		unless (d = SFIND(s, i)) continue;
		sccs_setPath(s, d, tmp);
	}
	sccs_clearbits(s, D_CSET);
	free(s->sfile);
	s->sfile = sfile;
	free(s->gfile);
	s->gfile = gfile;
	sccs_newchksum(s);
	sccs_free(s);
	err = sys("bk", "edit", "-q", to, SYS);
	unless (WIFEXITED(err) && WEXITSTATUS(err) == 0) return (1);
	putenv("_BK_MV_OK=1");
	tmp = aprintf("-ybk cp %s %s", from, to);
	err = sys("bk", "delta", "-f", tmp, to, SYS);
	free(tmp);
	unless (WIFEXITED(err) && WEXITSTATUS(err) == 0) return (1);
	return (0);
}
