/*
 * Copyright 2001-2003,2005,2009-2011,2015-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"
#include "sccs.h"
#define	NTAGS	50

private void	m(sccs *s, ser_t l, ser_t r);
private	int	tagmerge(void);

/*
 * XXX - tagmerge should go away once we know we don't have tag merge problems
 * any more.
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
	ser_t	d, e, a = 0, b = 0;
	int	j, i = 0;
	char	cset[] = CHANGESET;

	unless (s = sccs_init(cset, 0)) {
		fprintf(stderr, "Cannot init ChangeSet\n");
		return (1);
	}
	/*
	 * Find the two oldest tag tips, and count up all tips.
	 */
	for (d = TABLE(s), i = 0; d >= TREE(s); d--) {
		unless (SYMGRAPH(s, d)) continue;
		EACH_PTAG(s, d, e, j) FLAGS(s, e) |= D_RED;
		if (FLAGS(s, d) & D_RED) {
			FLAGS(s, d) &= ~D_RED;
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
	    REV(s, a), a, REV(s, b), b, i);
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
m(sccs *s, ser_t l, ser_t r)
{
	ser_t	d, p;
	char	key[MAXKEY];
	char	zone[20];
	int	i, sign, hwest, mwest;
	struct	tm *tm;
	time_t	tt;
	long	seast;
	u32	sum = 0;
	char	tmp[20], buf[MAXKEY];

	p = (DATE(s, l) < DATE(s, r)) ? r : l;
	i = 1;
	do {
		tt = DATE(s, p) + i++;
		for (d = TABLE(s); d >= TREE(s); d--) {
			if (DATE(s, d) < tt) {
				d = 0;
				break;
			}
			if ((DATE(s, d) == tt) &&
			    streq(USERHOST(s, d), USERHOST(s, p)) &&
			    streq(PATHNAME(s, d), PATHNAME(s, p))) {
			    	break;
			}
		}
	} while (d);
	while (PARENT(s, p) && TAG(s, p)) p = PARENT(s, p);
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

	sprintf(buf, "M %s %s%s %s +0 -0\n",
	    "0.0", tmp, zone, USERHOST(s, p));
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
		fprintf(stderr, "takepatch failed, "
		    "contact support@bitkeeper.com please.\n");
		exit(1);
	}
	unlink("SCCS/s.ChangeSet");
	rename("RESYNC/SCCS/s.ChangeSet", "SCCS/s.ChangeSet");
	system("bk -?BK_NO_REPO_LOCK=YES abort -f");
}
