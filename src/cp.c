/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
private int	cp(char *from, char *to);
WHATSTR("@(#)%K%");

/*
 * cp - copy a file to another file,
 * removing the changeset marks,
 * generating new random bits,
 * and storing a new pathname.
 */
int
cp_main(int ac, char **av)
{
	if ((ac == 2) && streq("--help", av[1])) {
usage:		close(0);
		//system("bk help -s cp");
		fprintf(stderr, "No help for cp yet\n");
		return (1);
	}
	unless (ac == 3) goto usage;
	exit(cp(av[1], av[2]));
}

/*
 */
private int
cp(char *from, char *to)
{
	sccs	*s;
	delta	*d;
	char	buf[100];
	char	*sfile, *gfile, *tmp;

	assert(from && to);
	sfile = name2sccs(from);
	unless (s = sccs_init(sfile, 0, 0)) return (1);
	unless (s->tree && s->cksumok) return (1);
	free(sfile);
	sfile = name2sccs(to);
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
	if (s->tree->pathname) free(s->tree->pathname);
	s->tree->pathname = strdup(_relativeName(gfile, 0, 0, 0, 0, 0, 0));
	for (d = s->table; d; d = d->next) d->flags &= ~D_CSET;
	free(s->sfile);
	s->sfile = sfile;
	free(s->gfile);
	s->gfile = gfile;
	sccs_newchksum(s);
	sccs_free(s);
	sys("bk", "edit", "-q", to, SYS);
	tmp = aprintf("-ybk cp %s %s", from, to);
	sys("bk", "delta", tmp, to, SYS);
	free(tmp);
	return (0);
}
