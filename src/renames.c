#if 1
main() { exit(0); }
#else
/*
 * renames.c - make sure all files are where they are supposed to be.
 *
 * a) pull everything out of RESYNC into RENAMES which has pending name changes
 * b) build a DBM of rootkeys true/false of all the files in the RESYNC dir
 *    (takepatch could do this and leave it in BitKeeper/tmp).
 * c) process the files in RENAMES, resolving any conflicts, and put the file
 *    in RESYNC - if there isn't a file there already.  If there is, ask if 
 *    they want to move it and if so, move it to RENAMES.
 * d) before putting the file in RESYNC, if the same pathname is taken in the
 *    repository and the rootkey of that file is not in our list, then ask
 *    the user if they want to move the other file out of the way or move
 *    this file somewhere else.  If somewhere else is chosen, repeat process
 *    using that name.  If move is chosen, then copy the file to RENAMES and
 *    put this file in RESYNC.
 * e) repeat RENAMES processing until RENAMES dir is empty.  Potentially all
 *    files could pass through RENAMES.
 * f) move each file from RESYNC to repository, making sure that if the file
 *    exists in the repository, we have either overwritten it or deleted it.
 */
#include "system.h"
#include "sccs.h"

int
main(int ac, char **av)
{
	renames();
}

renames()
{
	sccs	*s;
	char	buf[MAXPATH];
	FILE	*p;
	MDBM	*rootDB;

	/*
	 * Make sure we are where we think we are.  We think we are in the
	 * RESYNC dir inside a populated repository.
	 */
	unless (exists("BitKeeper/etc") &&
	    exists("../BitKeeper/etc") && exists("../RESYNC")) {
	    	fprintf(stderr, "renames: must be called inside RESYNC dir\n");
		return (1);
	}

	unless (p = popen("bk sfiles .", "r")) {
		perror("popen of bk sfiles");
		return (1);
	}

	unless (rootDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) {
		perror("mdbm_open");
		pclose(p);
		return (1);
	}

	while (fnext(buf, p)) {
		chop(buf);
		unless (s = sccs_init(buf, 0, 0)) continue;
		renames_pass1(s, rootDB);
	}
	renames_pass2(rootDB);
}

/*
 * Pass 1 - move to RENAMES and/or build up the DBM of root keys
 */
pass1(sccs *s, MDBM *db)
{
	char	path[MAXPATH];
	static	int filenum;
	char	*sfile;
	char	*t;

	if (nameOK(s)) {
		sccs_sdelta(s, sccs_ino(s), path);
		if (mdbm_store_str(db, path, "", MDBM_INSERT)) {
			/* paranoia - duplicate keys */
			perror("mdbm_store - duplicate key");
			exit(1);
		}
		sccs_free(s);
		return;
	}

	unless (filenum) {
		mkdir("BitKeeper/RENAMES", 0775);
		mkdir("BitKeeper/RENAMES/SCCS", 0775);
	}
	sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", ++filenum);
	sfile = strdup(s->sfile);
	sccs_free(s);
	if (rename(sfile, path)) {
		fprintf(stderr, "Unable to rename(%s, %s)\n", sfile, path);
		exit(1);
	}

	free(sfile);
}

/*
 * Return true if the name of TOT is the same as the sfile pathname.
 */
int
nameOK(sccs *s)
{
	delta	*d = sccs_getrev(s, "+", 0, 0);

	return (streq(d->pathname, s->gfile));
}

pass2()
{
	char	path[MAXPATH];
	sccs	*s;
	delta	*d;
	int	worked = 0, failed = 0;
	int	i;
	
	unless (filenum) return;
	for (i = 1; i <= filenum; ++i) {
		sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", i);
		unless (s = sccs_init(path, 0, 0)) {
			fprintf(stderr, "Unable to init %s\n", path);
			continue;
		}
		unless (d = sccs_getrev(s, "+", 0, 0)) {
			/* should NEVER happen */
			fprintf(stderr, "Can't find TOT in %s\n", path);
			fprintf(stderr, "ERROR: File left in %s\n", path);
			sccs_free(s);
			failed++;
			continue;
		}
		if (try_rename(s, d, 0)) {
			fprintf(stderr, "Can't rename %s -> %s\n",
			    s->gfile, d->pathname);
			fprintf(stderr, "ERROR: File left in %s\n", path);
			sccs_free(s);
			failed++;
			continue;
		}
		sccs_free(s);
		worked++;
	}
	fprintf(stderr,
	    "renames: %d/%d worked, %d/%d failed\n",
	    worked, filenum, failed, filenum);
}

/*
 * Just for fun, see if the place where this wants to go is taken.
 * If not, just move it there.  We should be clean so just do the s.file.
 */
int
try_rename(sccs *s, delta *d, int dopass1)
{
	char	*sfile = name2sccs(d->pathname);

	assert(sfile);
	if (exists(sfile)) {
		/* circular or deadlock */
		free(sfile);
		if (dopass1) pass1(s);
		return (1);
	}
	if (rename(s->sfile, sfile)) {
		free(sfile);
		if (dopass1) pass1(s);
		return (1);
	}
	fprintf(stderr, "rename: %s -> %s\n", s->sfile, sfile);
	free(sfile);
	return (0);
}
#endif
