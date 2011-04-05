#include "system.h"
#include "sccs.h"
#define	NTAGS	50

private void	m(sccs *s, delta *l, delta *r);
private	int	tagmerge(void);

/*
 * XXX - tagmerge should go away once we know we don't have tag merge problems
 * any more.
 * Copyright (c) 2001 L.W.McVoy
 */

int
tagmerge_main(int ac, char **av)
{
	if (!exists("SCCS/s.ChangeSet") && proj_cd2root()) {
		fprintf(stderr, "Cannot find package root\n");
		return (1);
	}
	while (tagmerge());
	system("bk admin -hhh ChangeSet");
	return (0);
}

private int
tagmerge(void)
{
	sccs	*s;
	delta	*d, *a = 0, *b = 0;
	int	i = 0;
	char	cset[] = CHANGESET;

	unless (s = sccs_init(cset, 0)) {
		fprintf(stderr, "Cannot init ChangeSet\n");
		return (1);
	}
	/*
	 * Find the two oldest tag tips, and count up all tips.
	 */
	for (d = s->table, i = 0; d; d = NEXT(d)) {
		unless (d->symGraph) continue;
		if (d->ptag) sfind(s, d->ptag)->flags |= D_RED;
		if (d->mtag) sfind(s, d->mtag)->flags |= D_RED;
		if (d->flags & D_RED) {
			d->flags &= ~D_RED;
			continue;
		}
		i++;
		b = a;	/* b will be next oldest */
		a = d; 	/* a will be oldest */
	}
	if (i <= 1) {
		sccs_free(s);
		return (0);
	}
	fprintf(stderr, "Merge tips %s/%d %s/%d (%d tips total)\n",
	    REV(s, a), a->serial, REV(s, b), b->serial, i);
	m(s, a, b);
	return (1);
}

private u32
doit(u32 sum, char *buf)
{
	static	FILE	*f;
	int	len;

	unless (buf) {
		assert(f);
		fprintf(f, "# Patch checksum=%x\n", sum);
		fclose(f);
		f = 0;
		return (0);
	}
	unless (f) {
		mkdir("PENDING", 0775);
		f = fopen("PENDING/tagmerge", "w");
		assert(f);
	}
	len = strlen(buf);
	sum = adler32(sum, buf, len);
	fwrite(buf, len, 1, f);
	return (sum);
}

private void
m(sccs *s, delta *l, delta *r)
{
	delta	*d, *p;
	char	key[MAXKEY];
	char	zone[20];
	int	i, sign, hwest, mwest;
	struct	tm *tm;
	time_t	tt;
	long	seast;
	u32	sum = 0;
	char	tmp[20], buf[MAXKEY];

	p = (l->date < r->date) ? r : l;
	i = 1;
	do {
		tt = p->date + i++;
		for (d = s->table; d; d = NEXT(d)) {
			if (d->date < tt) {
				d = 0;
				break;
			}
			if ((d->date == tt) &&
			    streq(USER(s, d), USER(s, p)) &&
			    (d->hostname == p->hostname) &&
			    streq(PATHNAME(s, d), PATHNAME(s, p))) {
			    	break;
			}
		}
	} while (d);
	while (p->pserial && TAG(p)) p = PARENT(s, p);
	sprintf(buf, "# Patch vers:\t1.3\n# Patch type:\tREGULAR\n\n");
	sum = doit(sum, buf);
	sprintf(buf, "== %s ==\n", s->gfile);
	sum = doit(sum, buf);
	sccs_sdelta(s, sccs_ino(s), key);
	sprintf(buf, "%s\n", key);
	sum = doit(sum, buf);
	sccs_sdelta(s, p, key);
	sprintf(buf, "%s\n", key);
	sum = doit(sum, buf);
	tm = localtimez(&tt, &seast);
	strftime(tmp, sizeof(tmp), "%y/%m/%d %H:%M:%S", tm);
	if (seast < 0) {
		sign = '-';
		seast = -seast;  /* now swest */
	} else {
		sign = '+';
	}
	hwest = seast / 3600;
	mwest = (seast % 3600) / 60;
	sprintf(zone, "%c%02d:%02d", sign, hwest, mwest);

	sprintf(buf, "M %s %s%s %s@%s +0 -0\n",
	    "0.0", tmp, zone, USER(s, p), HOSTNAME(s, p));
	sum = doit(sum, buf);
	sccs_sdelta(s, sccs_ino(s), key);
	sprintf(buf, "B %s\n", key);
	sum = doit(sum, buf);
	sprintf(buf, "P %s\n", s->gfile);
	sum = doit(sum, buf);
	sprintf(buf, "s g\n");
	sum = doit(sum, buf);
	sprintf(buf, "s l\n");
	sum = doit(sum, buf);
	sccs_sdelta(s, l, key);
	sprintf(buf, "s %s\n", key);
	sum = doit(sum, buf);
	sccs_sdelta(s, r, key);
	sprintf(buf, "s %s\n", key);
	sum = doit(sum, buf);
	sprintf(buf, "------------------------------------------------\n\n\n");
	sum = doit(sum, buf);
	doit(sum, 0);
	sccs_free(s);
	system("bk takepatch -vvvf PENDING/tagmerge");
	if (exists("core") || exists("RESYNC/core")) {
		fprintf(stderr, "takepatch failed, contact BitMover please.\n");
		exit(1);
	}
	unlink("SCCS/s.ChangeSet");
	rename("RESYNC/SCCS/s.ChangeSet", "SCCS/s.ChangeSet");
	system("bk -?BK_NO_REPO_LOCK=YES abort -f");
}
