#include "system.h"
#include "sccs.h"

 
/*
 * Return TRUE if s has cset derived root key
 */
private int
hasCsetDerivedKey(sccs *s)
{
        sccs    *sc;
        char    buf1[MAXKEY], buf2[MAXKEY], *p;
        delta   *d1, *d2;
 
	d1 = sccs_ino(s);
        sccs_sdelta(s, d1, buf1);
 
        sprintf(buf2, "%s/%s", s->proj->root, CHANGESET);
        sc = sccs_init(buf2, INIT_SAVEPROJ, s->proj);
        assert(sc);
	d2 = sccs_ino(sc);
        assert(d2);
        p = d2->pathname;
        d2->pathname = d1->pathname;
        sccs_sdelta(sc, d2, buf2);
        d2->pathname = p;
        sccs_free(sc);
 
        return (streq(buf1, buf2));
}  

/*
 * Return ture if s1 is older than s2
 */
private int
older(sccs *s1, sccs *s2)
{
	delta	*d1, *d2;
	int	rc = 0;

	d1 = sccs_ino(s1);
	d2 = sccs_ino(s2);
	if (d1->date < d2->date) rc = 1;
	return (rc);
}


/*
 * file must a name relative to root
 */
int
migrate_main(int ac, char **av)
{
	FILE 	*f, *f1;
	char	buf[MAXPATH], cmd[100], key[MAXKEY];
	char	*p, *file;
	int	i, c, resync = 0, update_gone_file = 0;
	sccs	*s_winner = 0, *s;

	while ((c = getopt(ac, av, "R")) != -1) {
		switch (c) {
		    case 'R': resync = 1; break;
		    default:
usage:			system("bk help -s migrate");
			return (1);
                }
        }  

	file = av[optind];
	unless (file) goto usage;

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "migrate: cannot find root directory\n");
		return (1);
	}

	unlink("BitKeeper/tmp/GONE");
	unlink("BitKeeper/tmp/RM");

	/*
	 * find all the files related to "file"
	 * XXX TODO if we are in RESYC need to also scan the local tree
	 */
	sprintf(cmd,
		"bk -r prs -hr1.0 -d'$if(:DPN:=%s){:SFILE:\\n'} > "
						"BitKeeper/tmp/LIST", file);
	system(cmd);


	f = fopen("BitKeeper/tmp/LIST", "rt");
	assert(f);
	i = 0;
	while (fgets(buf, sizeof(buf), f))  {
		i++;
		if (i > 1) break;
	}
	fclose(f);
	assert(i > 0);
	if (i == 1) {
		s = sccs_init(buf, 0, 0);
		if (!hasCsetDerivedKey(s)) {
			sccs_free(s);
			return 0; /* done */
		}
		sccs_free(s);
	}
	
	/*
	 * Sort merge all content
	 */
	system("cat BitKeeper/tmp/LIST | "
			"bk get -pkq - | sort -u > BitKeeper/tmp/allversions");
	
	
	f = fopen("BitKeeper/tmp/LIST", "rt");
	f1 = fopen("BitKeeper/tmp/GONE", "wb");
	while (fgets(buf, sizeof(buf), f))  {
		chop(buf);
		s = sccs_init(buf, 0, 0);
		if (!hasCsetDerivedKey(s) && 
		    (!s_winner || older(s, s_winner))) {
			s_winner = s;
		} else {
			/* s is a loser */
			update_gone_file = 1;
			sccs_sdelta(s, sccs_ino(s), key);
			fprintf(f1, "%s\n", key);
			sccs_free(s); 
			unlink(buf);
		}
	}
	fclose(f); fclose(f1);


	/*
	 * If winner is not in the right place, move it
	 */
	p = sccs2name(s_winner->sfile);
	sccs_free(s_winner);
	if (!streq(p, file)) {
		assert(!exists(file));
		sprintf(cmd, "bk mv %s %s", p, file);
		system(cmd);
	}
	free(p);

	/*
	 * Update the winner with the sort merges of all versions
	 */
	sprintf(cmd, "bk get -geq %s", file);
	system(cmd);
	rename("BitKeeper/tmp/allversions", file);
	sprintf(cmd, "bk delta -q -y'Auto converted' %s", file);
	system(cmd);

	/*
	 * update gone file
	 */
	if (update_gone_file) {
		system("bk get -kpq BitKeeper/etc/gone >> BitKeeper/tmp/GONE");
		system("bk get -egq  BitKeeper/etc/gone");
		system("sort -u BitKeeper/tmp/GONE > BitKeeper/etc/gone");
		/*
		 * We use "bk ci" here, becuase we do not want to create
		 * a new delat if no change
		 */
		system("bk ci -q -y'Auto updated' BitKeeper/etc/gone");
	}
	system("bk -r check -a");
}
