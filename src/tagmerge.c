#include "system.h"
#include "sccs.h"
#define	NTAGS	50
WHATSTR("@(#)%K%");

private void	m(sccs *s, delta *l, delta *r);
private	int	tagmerge(void);

/*
 * XXX - tagmerge should go away once we know we don't have tag merge problems
 * any more.
 * Copyright (c) 2001 L.W.McVoy
 */


int
dsort(const void *a, const void *b)
{
	return ((*(delta**)a)->date - (*(delta**)b)->date);
}

int
tagmerge_main(int ac, char **av)
{
	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		//system("bk help tagmerge");
		return (1);
	}
	if (!exists("SCCS/s.ChangeSet") && sccs_cd2root(0, 0)) {
		fprintf(stderr, "Cannot find package root\n");
		return (1);
	}
	while (tagmerge());
	system("bk admin -hhh ChangeSet");
	return (0);
}

private int
tagmerge()
{
	sccs	*s;
	delta	*d, *a = 0, *b = 0;
	int	i = 0;
	char	cset[] = CHANGESET;

	unless (s = sccs_init(cset, 0, 0)) {
		fprintf(stderr, "Cannot init ChangeSet\n");
		return (1);
	}
	/*
	 * Find the two oldest tag tips, and count up all tips.
	 */
	for (d = s->table, i = 0; d; d = d->next) {
		unless (d->symLeaf) continue;
		i++;
		unless (a) {
			a = d;
			continue;
		}
		unless (b) {
			b = d;
			continue;
		}
		if (d->date < a->date) {
			a = d;
			continue;
		}
		if (d->date < b->date) {
			b = d;
			continue;
		}
	}
	if (i <= 1) {
		sccs_free(s);
		return (0);
	}
	fprintf(stderr, "Merge tips %s/%d %s/%d (%d tips total)\n",
	    a->rev, a->serial, b->rev, b->serial, i);
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
	char	*zone = sccs_zone();
	int	i;
	struct	tm *tm;
	time_t	tt;
	u32	sum = 0;
	char	tmp[20], buf[MAXKEY];

	p = (l->date < r->date) ? r : l;
	i = 1;
	do {
		tt = p->date + i++;
		for (d = s->table; d; d = d->next) {
			if (d->date < tt) {
				d = 0;
				break;
			}
			if ((d->date == tt) &&
			    streq(d->user, p->user) &&
			    streq(d->hostname, p->hostname) &&
			    streq(d->pathname, p->pathname)) {
			    	break;
			}
		}
	} while (d);
	while (p->parent && (p->type != 'D')) p = p->parent;
	tm = localtimez(&tt, 0);
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
	strftime(tmp, sizeof(tmp), "%y/%m/%d %H:%M:%S", tm);
	sprintf(buf, "M %s %s%s %s@%s +0 -0\n",
	    "0.0", tmp, zone, p->user, p->hostname);
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
	system("mv -f RESYNC/SCCS/s.ChangeSet SCCS/s.ChangeSet");
	system("bk abort -f");
	free(zone);
}
