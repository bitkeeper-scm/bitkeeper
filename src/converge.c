#include "system.h"
#include "sccs.h"

#define	CTMP	"BitKeeper/tmp/CONTENTS"

private int	converge(char *file, int resync);
 
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

private int
sys(char *cmd)
{
	int	ret;

	ret = system(cmd);
	return (ret);
}

/*
 * Usage: converge [-R]
 */
int
converge_main(int ac, char **av)
{
	int	ret = 0, c, resync = 0;
	char	buf[MAXPATH];
	char	*files[] = {
			"BitKeeper/etc/gone",
			"BitKeeper/etc/ignore",
			"BitKeeper/etc/logging_ok",
			0
			};

	while ((c = getopt(ac, av, "R")) != -1) {
		switch (c) {
		    case 'R': resync = 1; break;
		    default:
			system("bk help -s converge");
			return (1);
                }
        }  

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "converge: cannot find root directory\n");
		return (1);
	}

	/*
	 * Do the per file work
	 */
	for (c = 0; files[c]; c++) {
		ret += converge(files[c], resync);
		sprintf(buf, "bk clean -q %s", files[c]);
		sys(buf);
	}
	return (ret);
}

private MDBM *
list(char *gfile)
{
	char	*sfile = name2sccs(gfile);
	char	*p, *t;
	char	key[MAXKEY];
	sccs	*s;
	FILE	*f;
	MDBM	*vals = mdbm_mem();
	char	buf[MAXPATH];

	/*
	 * Find the full list of files we need work through - which is the
	 * specified file plus any others which have the same root path.
	 */
	if ((s = sccs_init(sfile, 0, 0)) && s->tree) {
		sccs_sdelta(s, sccs_ino(s), key);
		mdbm_store_str(vals, key, sfile, 0);
		sccs_free(s);
	} else {
		if (s) sccs_free(s);
	}
	f = popen(
	    "bk sfiles BitKeeper/deleted | "
	    "bk prs -r+ -hd':ROOTKEY:\n:GFILE:\n' -",
	    "r");
	assert(f);
	while (fnext(key, f))  {
		p = strchr(key, '|') + 1;
		t = strchr(p, '|'); *t = 0;
		fnext(buf, f);
		unless (streq(p, gfile)) continue;
		*t = '|';
		chop(key);
		chop(buf);
		t = name2sccs(buf);
		mdbm_store_str(vals, key, t, 0);
		free(t);
	}
	pclose(f);
	free(sfile);
	return (vals);
}

/*
 * Generate both lists, if any are unique to the parent,
 * then find a spot for them locally and copy them down.
 * Delta them if we had to rename them.
 */
private MDBM *
resync_list(char *gfile)
{
	MDBM	*pvals, *vals;
	kvpair	kv;
	char	cmd[MAXPATH*2];
	char	*base, *r, *t;
	int	i;
	sccs	*s;
	delta	*d;

	chdir(RESYNC2ROOT);
	pvals = list(gfile);
	chdir(ROOT2RESYNC);
	vals = list(gfile);

	for (kv = mdbm_first(pvals); kv.key.dptr; kv = mdbm_next(pvals)) {
		if (mdbm_fetch_str(vals, kv.key.dptr)) continue;
		unless (exists(kv.val.dptr)) {
			mkdirf(kv.val.dptr);
			sprintf(cmd, "cp %s/%s %s",
			    RESYNC2ROOT, kv.val.dptr, kv.val.dptr);
			sys(cmd);
			mdbm_store_str(vals, kv.key.dptr, kv.val.dptr, 0);
			continue;
		}

		sprintf(cmd, "%s/%s", RESYNC2ROOT, kv.val.dptr);
		s = sccs_init(cmd, 0, 0); assert(s && s->tree);
		/* reset the proj->root to RESYNC root */
		free(s->proj->root);
		s->proj->root = strdup(".");
		t = sccs_rmName(s, 1);
		sccs_free(s);

		mkdirf(kv.val.dptr);
		sprintf(cmd, "cp %s/%s %s", RESYNC2ROOT, kv.val.dptr, t);
		sys(cmd);
		sprintf(cmd, "bk get -qe %s", t);
		sys(cmd);

		sprintf(cmd, "bk delta -dqy'Auto converge rename' %s", t);
		system(cmd);
		mdbm_store_str(vals, kv.key.dptr, t, 0);
		free(t);
	}
	mdbm_close(pvals);
	return (vals);
}
	
private int
converge(char *gfile, int resync)
{
	char	buf[MAXPATH], key[MAXKEY];
	char	*sfile;
	sccs	*s, *winner = 0;
	MDBM	*vals = resync ? resync_list(gfile) : list(gfile);
	kvpair	kv;
	int	i;

	/*
	 * If there was only one file, and it isn't a derived file, done
	 */
	for (i = 0, kv = mdbm_first(vals); kv.key.dptr; kv = mdbm_next(vals)) {
		i++;
	}
	if (i == 0) {
done:		mdbm_close(vals);
		return (0); /* nothing to do */ 
	}
	if (i == 1) {
		kv = mdbm_first(vals);
		if ((s = sccs_init(kv.val.dptr, 0, 0)) &&
		    s->tree && !hasCsetDerivedKey(s)) {
			sccs_free(s);
			goto done;
		}
		if (s) sccs_free(s);
	}
	
	/*
	 * Get the contents of all files, we'll sort -u them later.
	 */
	unlink(CTMP);
	for (kv = mdbm_first(vals); kv.key.dptr; kv = mdbm_next(vals)) {
		sprintf(buf, "bk get -kpq %s >> %s", kv.val.dptr, CTMP);
		sys(buf);
	}

	/*
	 * Figure out who is going to win, i.e.,
	 * the oldest file not changeset key based (that idea didn't work).
	 * It's OK if there is no winner, we'll create one.
	 */
	for (kv = mdbm_first(vals); kv.key.dptr; kv = mdbm_next(vals)) {
		s = sccs_init(kv.val.dptr, 0, 0);
		if (hasCsetDerivedKey(s)) {
			sccs_free(s);
			continue;	/* don't want that one */
		}
		unless (winner) {
			winner = s;
			continue;
		}
		/* if this is an older one, it  becomes the winner */
		if (sccs_ino(s)->date < sccs_ino(winner)->date) {
			sccs_free(winner);
			winner = s;
		} else if (sccs_ino(s)->date == sccs_ino(winner)->date) {
			char	key2[MAXKEY];

			sccs_sdelta(s, sccs_ino(s), key);
			sccs_sdelta(winner, sccs_ino(winner), key2);
			if (strcmp(key, key2) < 0) {
				/* we use lesser values to mean older,
				 * just like the time_t.
				 */
				sccs_free(winner);
				winner = s;
			}

		} else {
			sccs_free(s);
		}
	}
	sfile = name2sccs(gfile);

	/*
	 * If there is a winner and there is an existing sfile and
	 * that sfile is not the winner, then bk rm it so we can
	 * slide this one into place.
	 */
	if (winner && exists(sfile) && !streq(sfile, winner->sfile)) {
		sprintf(key, "bk rm %s", gfile);
		sys(key);
		sccs_close(winner);
		sprintf(key, "bk mv %s %s", winner->gfile, gfile);
		sys(key);
	}

	if (exists(CTMP)) {
		/*
		 * Update the winner with the saved content, or
		 * create a new file with the saved content.
		 */
		if (winner) {
			sprintf(key, "bk get -qeg %s", gfile);
			sys(key);
			sprintf(key, "sort -u < %s > %s", CTMP, gfile);
			sys(key);
			sprintf(key, "bk delta -qy'Auto converge' %s", gfile);
			sys(key);
		} else {
			/*
			 * The file may be there because it is cset derived
			 * and there was no winner.  So we remove it.
			 */
			if (exists(sfile)) {
				sprintf(key, "bk rm %s", gfile);
				sys(key);
			}
			sprintf(key, "sort -u < %s > %s", CTMP, gfile);
			sys(key);
			sprintf(key,
			    "bk delta -qiy'Auto converge/create' %s", gfile);
			sys(key);
		}
	}
	mdbm_close(vals);
	if (winner) sccs_free(winner);
	free(sfile);
	return (1);
}
