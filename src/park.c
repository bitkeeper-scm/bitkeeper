/*
 * Copyright (c) 2002, Andrew Chang
 */
#include "system.h"
#include "sccs.h"

#define PARKFILE_VERSION "# BITKEEPER PARKFILE VERSION: 2.1"
#define PARKDIR "BitKeeper/tmp/park_dir"
#define PARK2ROOT "../../.."
#define ROOT2PARK PARKDIR

private int listParkFile(void);
private int purgeParkFile(int);
private int diffable_text(sccs *, delta *);
private int unsupported_file_type(sccs *);
private int append(char *, char *);
private int parkfile_header(sccs *, delta *, char *, FILE *);
private	char *name2tname(char *);
private char *tname2sname(char *);
char **getParkComment(int *err);

private project *parkdir_proj = 0;

private void
usage1(void)
{
	fprintf(stderr, "Usage: bk park [-a] [-f] [-l] [-q] [-u]"
						    "[-p <num>] [-y<comment>]\n");
}

private void
usage2(void)
{
	fprintf(stderr, "usage: bk unpark [-c] [-f] [-]\n");
}

/*
 * Park the un-delta'ed change in a parkfile, the basic steps are simple:
 * a) find all the file that needs to be park, store them in SCCS/t.file
 *    in PARKDIR.
 * b) make a sfio file from all the SCCS/t.file(s) in PARKDIR
 * c) clean-up (unedit) the file(s) that are parked.
 */
int
park_main(int ac, char **av)
{
	char	sfio_list[MAXPATH], parkfile[MAXPATH], buf[MAXPATH];
	char	changedfile[MAXPATH], parkedfile[MAXPATH];
	char	*tname = 0, *sname = 0;
	char	*p;
	char 	**comments = NULL, **ccomments = NULL;
	int 	lflag = 0, qflag = 0, purge = 0, try = 0, force = 0;
	int	rc = 0, clean = 0, aflag = 0, unedit = 1, ask = 1;
	int	err = 0;
	int	i, c;
	sccs	*s = 0;
	time_t	tt;
	FILE	*f = 0, *f2 = 0;

	if (ac == 2 && streq("--help", av[1])) {
		usage1();
		return (rc);
	}

	while ((c = getopt(ac, av, "acflp:quy|")) != -1) {
		switch (c) {
		    case 'a':	aflag = 1; break; /* all */
		    case 'f':	force = 1; break;
		    case 'c':	clean = 1; break;
		    case 'l':	lflag = 1; break;
		    case 'p':	purge = atoi(optarg); break;
		    case 'q':	qflag = 1; break;
		    case 'u':	unedit = 0; break;
		    case 'y':	if (optarg) {			
					comments = addLine(0, strdup(optarg));
				}
				ask = 0;
				break;
		    default: 	usage1();
				return (1);
		}
	}

	sfio_list[0] = parkfile[0] = changedfile[0] = parkedfile[0] = 0;

	p = _relativeName(".", 1, 0, 1, 0, bk_proj, buf);
	unless (p) {
		fprintf(stderr, "Can't find package root\n");
err:		if (s) sccs_free(s);
		if (tname) free(tname);
		if (sname) free(sname);
		if (f) fclose(f);
		if (f2) fclose(f2);
		if (sfio_list[0]) unlink(sfio_list);
		if (changedfile[0]) unlink(changedfile);
		if (parkedfile[0]) unlink(parkedfile);
		freeLines(comments, free);
		freeLines(ccomments, free);
		return (1);
	}
	if (chdir(buf)) {
		perror(buf);
		fprintf(stderr, "Can't chdir to package root\n");
		goto err;
	}
	strcpy(buf, p);

	if (lflag) return (listParkFile());
	if (purge) return (purgeParkFile(purge));
	if (clean) return (sys(RM, "-rf", PARKDIR, SYS));
	if (force && exists(PARKDIR)) sys(RM, "-rf", PARKDIR, SYS);
	if (exists(PARKDIR)) {
		fprintf(stderr, "%s exists, park aborted\n", PARKDIR);
		goto err;
	}

	bktmp(changedfile, "cfile");
	if (av[optind] && streq(av[optind], "-")) {
		f = fopen(changedfile, "wb");
		while (fnext(buf, stdin)) {
			char *gname, *sname;
			char buf2[MAXPATH];

			chomp(buf);
			sname = name2sccs(buf);
			gname = sccs2name(sname);
			concat_path(buf2, buf, gname); /* buf[] == pwd */
			fprintf(f, "%s\n", buf2);
			free(gname);
			free(sname);
		}
		fclose(f);
	} else {
		sysio(0, changedfile, 0,
			"bk", "sfiles", aflag ? "-Ugcx" : "-Ugc", SYS);
	}
	if (size(changedfile) == 0) {
		unless (qflag) printf("Nothing to park\n");
		unlink(changedfile);
		return (0);
	}

	do {
		sprintf(parkfile, "%s/parkfile_%d.sfio", BKTMP, ++try);
	} while (exists(parkfile));

	if (force && exists(PARKDIR)) sys(RM, "-rf", PARKDIR, SYS);
	if (exists(PARKDIR)) {
		fprintf(stderr, "%s exists, park aborted\n", PARKDIR);
		goto err;
	}

	/*
	 * Process files and symlinks under BitKeeper control
	 * Here is where we make the t.file, we will make a sfio file
	 * from these t.files later.
	 */
	bktmp(parkedfile, "parkf");
	f = fopen(changedfile, "rt");
	f2 = fopen(parkedfile, "wb");
	assert(f); assert(f2);
	while (fnext(buf, f)) {
		char	tmp[MAXPATH];
		delta	*top;
		FILE	*out;

		chomp(buf);
		sname = name2sccs(buf);
		sprintf(tmp, "%s/%s", PARKDIR, buf);
		tname = name2tname(tmp);
		mkdirf(tname);

		s = sccs_init(sname, INIT_SAVEPROJ, bk_proj);
		assert(s);
		if (!exists(s->gfile)) {
			fprintf(stderr,
				"%s does not exists, park aborted\n",
				s->gfile);
			goto err;
		}
		top = sccs_top(s);
		if (!diffable_text(s, top) && !aflag)  {
			fprintf(stderr,
			    "Warning: %s: non-diff-able file, "
			    "not parked.\n",
			     s->gfile);
			sccs_free(s);
			s = NULL;
			continue;
		}

		if (!HAS_SFILE(s) && unsupported_file_type(s)) {
			/* skip unsupported xtra file type, just in case */
			fprintf(stderr,
				"%s: skipping unsupported file type\n",
				s->gfile);
			sccs_free(s);
			s = NULL;
			continue;
		}

		/*
		 * Break out to different file type
		 * and store parked data in the t.file
		 */
		out = fopen(tname, "wb");
		if (!HAS_SFILE(s) && S_ISREG(s->mode)) {
			rc |= parkfile_header(s, top, "EXTRA_REG", out);
			fclose(out); out = NULL;
			rc |= append(s->gfile, tname);
		} else if (!HAS_SFILE(s) && S_ISLNK(s->mode)) {
			rc |= parkfile_header(s, top, "EXTRA_SYMLINK", out);
			fprintf(out, "%s\n", s->symlink);
		} else if (diffable_text(s, top)) {
			rc |= parkfile_header(s, top, "TEXT_DIFFS", out);
			rc |= sccs_diffs(s, NULL, NULL,
					DIFF_HEADER, DF_UNIFIED, NULL, out);
		} else if (S_ISLNK(s->mode)) {
			rc |= parkfile_header(s, top, "SYMLINK", out);
			fprintf(out, "%s\n", s->symlink);
		} else if (ascii(s->gfile)) {
			assert(S_ISREG(s->mode));
			rc |= parkfile_header(s, top, "TEXT", out);
			fclose(out); out = NULL;
			rc |= append(s->gfile, tname);
		} else {
			assert(S_ISREG(s->mode));
			rc |= parkfile_header(s, top, "BINARY", out);
			fclose(out); out = NULL;
			rc |= append(s->gfile, tname);
		} 
		if (out) fclose(out);
		unless (qflag) fprintf(stderr, "%s\n", s->gfile);
		sccs_free(s);
		s = NULL;
		fprintf(f2, "%s\n", buf);
		free(tname);
		free(sname);
		tname = sname = NULL;
	}
	fclose(f);
	fclose(f2);
	if (rc != 0) {
		fprintf(stderr, "bk diffs failed, park aborted\n");
		goto err;
	}

	/*
	 * Get cset comments
	 */
	f = fopen(CCHANGESET, "rt");
	if (f) {
		while (fnext(buf, f)) {
			chomp(buf);
			ccomments = addLine(ccomments, strdup(buf));
		}
		fclose(f);
	}

	/*
	 * Get park comments
	 */
	if (ask && !comments)  comments = getParkComment(&err);
	if (err) {
		sys(RM, "-rf", PARKDIR, SYS);
		return (1);
	}

	/*
	 * Make a list of file we want to feed to sfio
	 * Note: the (fake) Changeset file must be first in the list
	 */
	bktmp_local(sfio_list, "sfio_list");
	f = fopen(sfio_list, "w");
	assert(f);
	fprintf(f, "%s\n", "ChangeSet");
	fclose(f); 
	if (chdir(ROOT2PARK)) {
		perror(ROOT2PARK);
		goto err;
	}
	sprintf(buf, "bk _find >> %s/%s", PARK2ROOT, sfio_list);
	system(buf);

	/*
	 * Note: pwd is PARKDIR for all code below this line
	 *
	 * Populate the "fake" ChangeSet with parkfile headers
	 * It looks like this
	 * # BITKEEPER PARKFILE VERSION: 2
	 * # ROOTKEY: awc@bitmover.com|ChangeSet|20020421190738|44310|5b7a1c947f
	 * # DELTKEY: awc@bitmover.com|ChangeSet|20020421191047|19451
	 * # PARK_ID: 5b7a1c91f5a
	 * # PARK_BY: awc@etp1.bitmover.com
	 * # PRKDATE: 2002/04/21 13:06:11
	 * # COMMENT: This is a cset comment, line 1
	 * # COMMENT: This is a cset comment, line 2
	 * # PARKCMT: This is a optional parkfile comment, line 1
	 * #
	 */
	f = fopen("ChangeSet", "wb");
	fprintf(f, "%s\n", PARKFILE_VERSION);
	/* We are in PARKDIR, so bk_proj is wrong, but we don't use it here */
	s = sccs_init(PARK2ROOT "/" CHANGESET, INIT_SAVEPROJ, bk_proj);
	fputs("# ROOTKEY: ", f);
	sccs_pdelta(s, sccs_ino(s), f);
	fputs("\n# DELTKEY: ", f);
	sccs_pdelta(s, sccs_top(s), f);
	fputs("\n", f);
	sccs_free(s);
	s = 0;
	EACH (ccomments) fprintf(f, "# COMMENT: %s\n", ccomments[i]);
	randomBits(buf);
	fprintf(f, "# PARK_ID: %s\n", buf);
	fprintf(f, "# PARK_BY: %s@%s\n", sccs_getuser(), sccs_gethost());
	time(&tt);
	strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S", localtimez(&tt, 0));
	fprintf(f, "# PRKDATE: %s\n", buf);
	EACH (comments) fprintf(f, "# PARKCMT: %s\n", comments[i]);
	fputs("#\n", f);
	if (fclose(f)) {
		perror("ChangeSet");
		goto err;
	}

	/*
	 * OK, now we make the final parkfile.sfio from the sfio list.
	 */
	sprintf(buf, "bk  sfio -qo < %s/%s > %s/%s",
		PARK2ROOT, sfio_list, PARK2ROOT, parkfile);
	if (system(buf)) {
		fprintf(stderr, "failed to create %s, i/o error ?\n", parkfile);
		goto err;
	}

	if (chdir(PARK2ROOT)) {
		fprintf(stderr, "park: cannot chdir to project root.\n");
		goto err;
	}

	/*
	 * clean up
	 * pwd is project root
	 */
	sys(RM, "-rf", PARKDIR, SYS);

	unless (unedit) goto done; /* skip unedit */

	/*
	 * bk unedit the file that are parked
	 * also clean up the c.file
	 */
	f = fopen(parkedfile, "rt");
	assert(f);
	while (fnext(buf, f)) {
		char *cname;

		chomp(buf);
		sname = name2sccs(buf);
		s = sccs_init(sname, INIT_SAVEPROJ, bk_proj);
		if (HAS_SFILE(s)) {
			sccs_unedit(s, SILENT);
			cname = sccs_Xfile(s, 'c');
			unlink(cname); /* clean c.file if exists */
		} else {
			unlink(s->gfile);
		}
		sccs_free(s);
		free(sname);
	}
	fclose(f);
	unlink(CCHANGESET);

	unless (qflag) {
		fprintf(stderr, "Parked changes in %s.\n", parkfile);
	}

done:	unlink(sfio_list);
	unlink(changedfile);
	unlink(parkedfile);
	freeLines(comments, free);
	freeLines(ccomments, free);
	return (0);
}


private int
check_compat(char *header)
{
	if (strlen(header) < 30) return (-1);
	if (!strneq(header,"# BITKEEPER PARKFILE VERSION: ", 30)) return (-1);
	if (!streq(&header[30], "2") && !streq(&header[30], "2.1")) return (-1);

	if (streq(&header[30], "2")) return (1);
	return (0); /* version 2.1 */
}

private void
printComments(char *parkfile)
{
	FILE	*f;
	char	buf[MAXLINE];
	char	*commentHdr;
	int	compat_mode = 0;

	f = fopen(parkfile, "rb");
	assert(f);
	fnext(buf, f);

	switch (check_compat(buf)) {
	    case -1:
		fprintf(stderr, "Bad park file , version mismatch ?\n");
		return;
	    case 1: compat_mode = 1;
		break;
	    case 0: /* version 2.1 */
		break;
	    default:
		assert("check_compat() failed" == 0);
	}

	commentHdr = compat_mode ? "# COMMENT: " : "# PARKCMT: ";
	
	while (fnext(buf, f)) {
		chomp(buf);
		if (streq("#", buf)) break; /* end of header */
		if (strneq(commentHdr, buf, 11)) printf("  %s\n", &buf[11]);
	}
	fclose(f);
}

private int
listParkFile()
{
	struct	dirent *e;
	DIR	*dh;
	char	parkfile[MAXPATH];

	dh = opendir(BKTMP);
	unless (dh) return (0);
	while ((e = readdir(dh)) != NULL) {
		if ((strlen(e->d_name) > 9) &&
		    strneq(e->d_name, "parkfile_", 9)) {
			printf("%s\n", e->d_name);
			sprintf(parkfile, "%s/%s", BKTMP, e->d_name);
			printComments(parkfile);
		}
	}
	closedir(dh);
	return (0);
}

private int
purgeParkFile(int id)
{
	char	parkfile[MAXPATH];

	sprintf(parkfile, "%s/parkfile_%d.sfio", BKTMP, id);
	if (unlink(parkfile)) {
		perror(parkfile);
		return (1);
	}
	printf("%s purged\n", parkfile);
	return (0);
}

private int
symlnkCopy(char *from, char *to)
{
	char	symTarget[MAXPATH];
	int	len;

	len = readlink(from, symTarget, MAXPATH);
	if (len <= 0) {
		fprintf(stderr, "Cannot read symlink %s\n", from);
		return (-1);
	}
	assert(len < MAXPATH);
	if (symlink(symTarget, to)) {
		perror(to);
		return (-1);
	}
	return (0);
}

private int
copyFileOrLink(char *from, char *to)
{
	int	rc = 0;

	if (exists(from)) {
		if (isreg(from)) {
			fileCopy(from, to);
		} else if (isSymlnk(from)) {
			symlnkCopy(from, to);
		} else {
			fprintf(stderr, "%s: unsopported from type\n", from);
			rc = -1;
		}
	}
	return (rc);
}

private int
badSpath(char *root, char *Gpath)
{
	char	tmp[MAXPATH];
	char	*Spath;

	if (!Gpath) return (1);
	Spath = name2sccs(Gpath);
	sprintf(tmp, "%s/%s", root, Spath); 
	free(Spath);
	/*
	 * XXX TODO, To be sure we need to extract the root key and compare them
	 * For now we trust the idcache, which is probaly good enough
	 */
	if (exists(tmp)) return (0);
	return (1);
}

private char *
name2cname(char *name)
{
	char *cname, *p;

	cname = name2sccs(name);
	p = strrchr(cname, '/');
	assert(p);
	assert(p[1] == 's');
	p[1] = 'c'; /* change s.file to c.file */
	return (cname);
}


private char *
name2tname(char *name)
{
	char *tname, *p;

	tname = name2sccs(name);
	assert(tname);
	p = strrchr(tname, '/');
	assert(p);
	assert(p[1] == 's');
	p[1] = 't'; /* change s.file to t.file */
	return (tname);
}


private char *
sname2pname(char *tname)
{
	char *p, *sname;

	p = strrchr(tname, '/');
	assert(p);
	assert(p[1] == 's');
	p[1] = 'p';
	sname = strdup(tname);
	p[1] = 's'; /* restore */
	return (sname);
}

private char *
tname2gname(char *tname)
{
	char *p, *gname;

	p = strrchr(tname, '/');
	assert(p);
	assert(p[1] == 't');
	p[1] = 's'; /* sccs2name() wants this */
	gname = sccs2name(tname);
	p[1] = 't'; /* restore */
	return (gname);
}

private char *
tname2sname(char *tname)
{
	char *p, *sname;

	p = strrchr(tname, '/');
	assert(p);
	assert(p[1] == 't');
	p[1] = 's';
	sname = strdup(tname);
	p[1] = 't'; /* restore */
	return (sname);
}

private char *
tname2cname(char *tname)
{
	char *p, *sname;

	p = strrchr(tname, '/');
	assert(p);
	assert(p[1] == 't');
	p[1] = 'c';
	sname = strdup(tname);
	p[1] = 't'; /* restore */
	return (sname);
}

private char *
key2Gpath(char *key, MDBM **idDB)
{
	char	*gpath;
	int	try = 0;

retry:	gpath = key2path(key, *idDB);
	if (badSpath(PARK2ROOT, gpath)) {
		if (try == 0) {
			chdir(PARK2ROOT);
			sys("bk", "idcache", SYS);
			mdbm_close(*idDB);
			unless (*idDB = loadDB(IDCACHE, 0,
						DB_KEYFORMAT|DB_NODUPS)) {
				perror("idcache");
				exit(1);
			}
			chdir(ROOT2PARK);
			try++;
			goto retry;
		} else {
			return(NULL);
		}
	}
	return (gpath);
}



/*
 * Copy gfile, sfile and pfile from user tree to PARKDIR, "bk edit" if necessary
 * Note 1: If the gfile is already in edit state and has additinal modification
 * that's ok, we want to apply the diffs on top of that.
 * Note 2: This function assume we are in the PARKDIR
 */
private int
copyGSPfile(char *oldpath, char *key,
				MDBM **idDB, FILE *unpark_list)
{
	char	*newGpath;
	char	*p, *q, *r, *oldGpath, *oldSpath, *newSpath;
	int	rc = 0;
	int	isExtraFile;

	isExtraFile = (key == NULL);
	if (isExtraFile) {
		newGpath = tname2gname(oldpath);
	} else {
		newGpath = key2Gpath(key, idDB);
		if (!newGpath) {
			fprintf(stderr, "can not find path for key %s\n", key);
			return (-1);
		}
	}
	newSpath = name2sccs(newGpath);
	oldGpath = tname2gname(oldpath);
	oldSpath = name2sccs(oldGpath);

	p = aprintf("%s/%s", PARK2ROOT, newGpath);
	rc |= copyFileOrLink(p, oldGpath);
	free(p);

	p = aprintf("%s/%s", PARK2ROOT, newSpath);
	q = sname2pname(p);
	r = sname2pname(oldSpath);

	if (!isExtraFile) {
		rc |= fileCopy(p, oldSpath);		/* copy s.file */
	} else if (exists(p)) {
		rc |= fileCopy(p, oldSpath);		/* copy s.file */
	}
	if (exists(q)) rc |= fileCopy(q, r);	/* copy p.file */
	free(p); free(q); free(r);

	assert(!strchr(oldGpath, '|'));
	assert(!strchr(newGpath, '|'));
	fprintf(unpark_list, "%s|%s\n", oldGpath, newGpath);

	if (!streq(oldGpath, newGpath)) {
		fprintf(stderr,
			"Warning: file is moved after parked: "
			"path: %s -> %s\n", oldGpath, newGpath);
	} 
	free(newGpath);
	free(oldSpath);
	free(newSpath);
	return (0);
}

/*
 * We need this to prevent "bk idcache" from decending into PARKDIR
 */
private void
mkFakeRoot(void)
{
	char p[] = BKROOT;
	char q[] = CHANGESET;

	mkdirp(p);
	mkdirf(q);
	close(open(CHANGESET, O_CREAT|O_WRONLY, 0666));
}

/*
 * Return true if two file are the same
 */
private int
same(MMAP *m1, MMAP *m2)
{
	char *p, *q, *r;

	if ((m1->end - m1->where) != (m2->end - m2->where)) return (0);
	p = m1->where; q = m2->where;
	r = m1->end;

	while (p < r) {
		if (*p++ != *q++) return (0);
	}
	return (1);
}

/*
 * Return true if two file are the same
 */
private int
same2(char *file, MMAP *m)
{
	MMAP	*m2;
	int	rc;

	m2 = mopen(file, "b");
	rc = same(m, m2);
	mclose(m2);
	return (rc);
}

/*
 * Return true if two file are the same
 */
private int
same3(sccs *s, MMAP *m)
{
	char	unpark_tmp[MAXPATH];
	int	rc;

	bktmp(unpark_tmp, "unpark");
	sccs_get(s, 0, 0, 0, 0, SILENT, unpark_tmp); 
	rc = same2(unpark_tmp, m);
	unlink(unpark_tmp);
	return (rc);
}

/*
 * Return true if two symlink are the same
 */
private int
sameLink(char *oldTarget, char *newTarget)
{
	assert(newTarget);
	if (!oldTarget) return (0);
	return (streq(oldTarget, newTarget));
}


private int
do_text_diffs_unpark(MMAP *m, char *path, MDBM **idDB, FILE *unpark_list)
{
	FILE 	*cf = NULL;
	char	*cname = NULL;
	char	*buf, *key;
	int	rc = 0;

	buf = mkline(mnext(m)); /* get root key */
	assert(strneq("# ROOTKEY: ", buf, 11));
	key = &buf[11];
	rc |= copyGSPfile(path, key, idDB, unpark_list);
	if (rc) return (rc);

	/*
	 * skip header
	 */
	while (buf = mkline(mnext(m))) {
		if (strneq(buf, "# PARKCMT: ", 11)) {
			if (!cname) cname = tname2cname(path);
			if (!cf) cf = fopen(cname, "wb");
			fprintf(cf, "%s\n", &buf[11]);
		} else if (streq(buf, "#")) break;
	}
	if (cf) fclose(cf);
	if (cname) free(cname);

	/*
	 * TODO should make sure the file did not changed type or encoding
	 */
	rc = sysio(path, NULL, NULL, "bk", "patch", "-p1", "-g1", "-f",  SYS);
	return (rc);
}

/*
 * A baseline key is the list of deltas collected when you walk backword
 * from the top of trunk and stop at the first non-null delta. (inclusive 
 * of the terminating non-null delta)
 */
private int
isBaselineKey(sccs *s, char *key)
{
	delta	*d;
	char 	baselineKey[MAXKEY];

	d = sccs_top(s);

	while ((d->added == 0) && (d->deleted == 0)) {
		sccs_sdelta(s, d, baselineKey);
		if (streq(key, baselineKey)) return (1);
		if (d->parent == NULL) break;
		d = d->parent;
	}
	sccs_sdelta(s, d, baselineKey);
	if (streq(key, baselineKey)) return (1);
	return (0);

}

private int
do_file_unpark(MMAP *m, char *path, int force, char *type,
					MDBM **idDB, FILE *unpark_list)
{
	char	*buf, *key, *basekey;
	char	*gname = 0, *sname = 0;
	int	fd, flags;
	int	error = 0;
	sccs	*s;

	buf = mkline(mnext(m)); /* get root key */
	assert(strneq("# ROOTKEY: ", buf, 11));
	key = &buf[11];
	error |= copyGSPfile(path, key, idDB, unpark_list);
	if (error) return (error);

	buf = mkline(mnext(m)); /* get delta key */
	basekey = strdup(&buf[11]);

	/*
	 * skip header
	 */
	while (buf = mkline(mnext(m))) {
		if (streq(buf, "#")) break;
	}

	sname = tname2sname(path);
	gname = tname2gname(path);

	s = sccs_init(sname, INIT_SAVEPROJ, parkdir_proj);
	assert(s && HASGRAPH(s));
	if (!HAS_GFILE(s) && isBaselineKey(s, basekey)) {
		goto doit;
	} else if (HAS_GFILE(s) && same2(s->gfile, m)) {
		goto out; /* done */
	} else if (!HAS_GFILE(s) && same3(s, m)) {
		/* does not have a gfile, but top delta same as parked binary */
		goto doit;
	} else {
		if (force) {
			unlink(gname);
			goto doit;
		}
		fprintf(stderr,
		    "unsafe to unpark over changed %s file: %s\n",
		    type, gname);
		error |= -1;
		goto out;
	}

	/*
	 * extract the binary data
	 * XXX TODO win32 may not like big buffer
	 */
doit:	printf("restoring %s file: %s\n", type, gname);
	flags = O_CREAT|O_TRUNC|O_WRONLY;
	fd = open(gname, flags, 0666);
	write(fd, m->where, m->end - m->where);
	close(fd);

out:	sccs_free(s);
	free(gname);
	free(sname);
	free(basekey);
	return (error);
}

private int
do_extra_reg_unpark(MMAP *m, char *path, int force,
					MDBM **idDB, FILE *unpark_list)
{
	char	*buf;
	char	*gname = 0, *sname = 0;
	int	rc = 0;
	int	fd, flags;
	sccs	*s = 0;

	copyGSPfile(path, NULL, idDB, unpark_list);

	/*
	 * skip header
	 */
	while (buf = mkline(mnext(m))) {
		if (streq(buf, "#")) break;
	}

	sname = tname2sname(path);
	gname = tname2gname(path);

	if (force) goto doit;
	s = sccs_init(sname, INIT_SAVEPROJ, parkdir_proj);
	assert(s);
	if (!HAS_GFILE(s) && !HAS_GFILE(s)) {
		goto doit; 
	} 
	if (HAS_GFILE(s)) {
		if (same2(s->gfile, m)) {
			goto doit;
		} else {
err:			fprintf(stderr,
			    "%s: conflict with existing file\n", gname);
			free(gname);
			free(sname);
			sccs_free(s);
			return (-1);
		}
	}
	if (HAS_SFILE(s)) {
		assert(!HAS_GFILE(s));
		if (same3(s, m)) {
			goto doit;
		} else {
			goto err;
		}
	}

doit:	printf("restoring extra file: %s\n", gname);
	flags = O_CREAT|O_TRUNC|O_WRONLY;
	fd = open(gname, flags, 0666);
	write(fd, m->where, m->end - m->where);
	close(fd);
	if (s) sccs_free(s);
	return (rc);
}

private int
do_symlink_unpark(MMAP *m, char *path, int force,
					MDBM **idDB, FILE *unpark_list)
{
	char	*buf, *key, *p, *sname, *gname;
	char 	*newTarget;
	char	*dkey = 0;
	char	key2[MAXKEY];
	int	rc = 0, restore_msg = 0;
	sccs 	*s;
	delta	*top;

	buf = mkline(mnext(m)); /* get root key */
	assert(strneq("# ROOTKEY: ", buf, 11));
	key = &buf[11];
	rc |= copyGSPfile(path, key, idDB, unpark_list);
	if (rc) return (rc);

	/*
	 * skip header
	 */
	buf = mkline(mnext(m)); /* get delta key */
	dkey = strdup(&buf[11]);
	while (buf = mkline(mnext(m))) {
		if (streq(buf, "#")) break;
	}
	p = mkline(mnext(m));
	assert(p);
	newTarget = strdup(p);

	sname = tname2sname(path);
	gname = tname2gname(path);

	s = sccs_init(sname, INIT_SAVEPROJ, parkdir_proj);
	assert(s && HASGRAPH(s));
	top = sccs_top(s); /* Do we want to follow a rename s.file ? */
			   /* if so we need to allow a rename delta  */
			   /* to be part of the baseline key list    */
	sccs_sdelta(s, top, key2);
	if (!HAS_GFILE(s) && streq(dkey, key2)) {
		/* no gfile, but have same baseline delta */
			rc |= symlink(newTarget, gname);
			if (rc == 0) restore_msg = 1;
	} else if (top->symlink && streq(top->symlink, newTarget)) {
		/*
		 * If we get here, the top delta have the same symlink as
		 * the parked sym link, i.e no conflict
		 * just need to make sure we have a gfile
		 */
		if (!HAS_GFILE(s)) {
			rc |= symlink(newTarget, gname);
			if (rc == 0) restore_msg = 1;
		}
	} else {
		/* conflict */
		if (force) {
			unlink(gname);
			rc |= symlink(newTarget, gname);
			if (rc == 0) restore_msg = 1;
		} else {
			rc |= -1;
		}
	};

	if (restore_msg) {
		fprintf(stderr,
			"restoring symlink %s -> %s\n", gname, newTarget);
	}

	sccs_free(s);
	free(sname);
	free(newTarget);
	if (dkey) free(dkey);
	return (rc);
}

private int
do_extra_symlink_unpark(MMAP *m, char *path, int force,
					MDBM **idDB, FILE *unpark_list)
{
	char	*buf, *p;
	char	*gname = 0, *sname = 0;
	char 	*newTarget;
	int	rc = 0;
	sccs	*s = 0;
	delta	*top;

	copyGSPfile(path, NULL, idDB, unpark_list);

	/*
	 * skip header
	 */
	while (buf = mkline(mnext(m))) {
		if (streq(buf, "#")) break;
	}

	p = mkline(mnext(m));
	assert(p);
	newTarget = strdup(p);

	sname = tname2sname(path);
	gname = tname2gname(path);

	if (force) goto doit;
	s = sccs_init(sname, INIT_SAVEPROJ, parkdir_proj);
	top = sccs_top(s);
	assert(s);
	if (!HAS_GFILE(s) && !HAS_GFILE(s)) {
		goto doit; 
	} 
	if (HAS_GFILE(s)) {
		if (sameLink(s->symlink, newTarget)) {
			goto done;
		} else {
err:			fprintf(stderr,
			    "%s: conflict with existing file\n", gname);
			free(gname);
			free(sname);
			sccs_free(s);
			return (-1);
		}
	}
	if (HAS_SFILE(s)) {
		assert(!HAS_GFILE(s));
		if (sameLink(top->symlink, newTarget)) {
			goto doit;
		} else {
			goto err;
		}
	}

doit:	printf("restoring extra symlink: %s -> %s\n", gname, newTarget);
	rc |= symlink(newTarget, gname);
done:	free(gname);
	free(sname);
	if (s) sccs_free(s);
	return (rc);
}


private int
do_unpark(int id, int clean, int force)
{
	int	rc = 0, error = 0, compat_mode = 0;
	char	parkfile[MAXPATH], sfio_list[MAXPATH], unpark_list[MAXPATH];
	char	path[MAXPATH];
	char	*cset_key = "";
	char	*to;
	MDBM    *idDB;
	FILE	*f, *f3;
	FILE	*f2 = NULL;

	sfio_list[0] = unpark_list[0] = 0;
	if (clean && exists(PARKDIR)) sys(RM, "-rf", PARKDIR, SYS);

	if (exists(PARKDIR)) {
		fprintf(stderr, "%s exists, unpark aborted\n", PARKDIR);
err:		if (sfio_list[0]) unlink(sfio_list);
		if (unpark_list[0]) unlink(unpark_list);
		if (parkdir_proj) proj_free(parkdir_proj);
		return (-1);
	}

	unless (idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
		perror("idcache");
		goto err;
	}

	if (id == -1) {
		strcpy(parkfile, "-");
	} else {
		sprintf(parkfile, "%s/parkfile_%d.sfio", BKTMP, id);
		unless (exists(parkfile)) {
			fprintf(stderr, "%s does not exist\n", parkfile);
			goto err;
		}
	}
	fprintf(stderr, "Unparking %s\n", parkfile);
	bktmp(sfio_list, "sfio_list");
	bktmp(unpark_list, "unpark_list");

	/*
	 * Get full path, because we chdir() below
	 */
	unless (streq(parkfile, "-")) strcpy(parkfile, fullname(parkfile, 0));
	strcpy(sfio_list, fullname(sfio_list, 0));
	strcpy(unpark_list, fullname(unpark_list, 0));

	mkdirp(PARKDIR);
	if (chdir(PARKDIR)) {
		perror(PARKDIR);
		goto err;
	}
	mkFakeRoot(); /* for bk idcache */
	parkdir_proj = proj_init(0);

	rc = sysio((id == -1) ? NULL : parkfile,
				NULL, sfio_list, "bk", "sfio", "-i", SYS);
	if (rc) {
		fprintf(stderr, "sfio errored, unpark failed\n");
		goto err;
	}

	f = fopen ("ChangeSet", "rt");
	if (!f) goto err;
	fnext(path, f); /* get parkfile header */
	chomp(path);

	switch (check_compat(path)) {
	    case -1:
		fprintf(stderr, "Bad park file , version mismatch ?\n");
		fclose(f);
		goto err;
	    case 1: compat_mode = 1;
		break;
	    case 0: /* version 2.1 */
		break;
	    default:
		assert("check_compat() failed" == 0);
	}

	if (!compat_mode) {
		while (fnext(path, f)) {
			if (strneq("# COMMENT: ", path, 11)) {
				unless (f2) f2 = fopen(CCHANGESET, "wb");
				fprintf(f2, "%s", &path[11]);
			}
		}
		if (f2) fclose(f2);
	}
	fclose(f);

	/*
	 * TODO check the repo root id and make sure they match
	 * if not, warn, but allow user to orveride
	 */
	f = fopen(sfio_list, "rt");
	f3 = fopen(unpark_list, "wb");
	fnext(path, f); /* skip the "Fake" ChangeSet file for Now */
	chomp(path);
	unless (streq("ChangeSet", path)) {
		fprintf(stderr, "Expect \"ChangeSet\", got \"%s\"\n", path);
		goto err;
	}

	/*
	 * main loop 
	 * pwd is PARKDIR
	 */
	while (fnext(path, f)) {
		char 	*buf2;
		MMAP	*m;

		chomp(path);
		unless (m = mopen(path, "b")) {
			fprintf(stderr, "%s: bad file\n", path);
			goto err;
		}
		buf2 = mkline(mnext(m));
		if (streq("# TYPE: TEXT_DIFFS", buf2)) {
			error |= do_text_diffs_unpark(m, path, &idDB, f3);
		} else if (streq("# TYPE: BINARY", buf2)) {
			error |= do_file_unpark(m, path,
						force, "binary", &idDB, f3);
		} else if (streq("# TYPE: TEXT", buf2)) {
			error |= do_file_unpark(m, path, 
						force, "text", &idDB, f3);
		} else if (streq("# TYPE: SYMLINK", buf2)) {
			error |= do_symlink_unpark(m, path, 
							force, &idDB, f3);
		} else if (streq("# TYPE: EXTRA_REG", buf2)) {
			error |= do_extra_reg_unpark(m, path, 
							force, &idDB, f3);
		} else if (streq("# TYPE: EXTRA_SYMLINK", buf2)) {
			error |= do_extra_symlink_unpark(m, path, 
							force, &idDB, f3);
		} else {
			error |= 1;
			fprintf(stderr, "Unknow file type: %s\n", buf2);
		}
		mclose(m);
	}
	fclose(f);
	fclose(f3);
	if (error && !force) {
		f = fopen (GCHANGESET, "rt");
		assert(f);
		fnext(path, f); /* version */
		fnext(path, f); /* rootkey */
		fnext(path, f); /* deltkey */
		fclose(f);
		chomp(path);
		assert(strneq("# DELTKEY: ", path, 11));
		cset_key = strdup(&path[11]);
		goto skip_apply;
	}

	/*
	 * Now we throw the unparked file over the wall
	 * pwd is PARKDIR
	 * TODO: we may want to save the displaced file in a sfio
	 * just in case the unpark is aborted or interrupted.
	 */
	f = fopen(unpark_list, "rt");
	while (fnext(path, f)) {
		char *p, *from, *to;

		chomp(path);
		p = strchr(path, '|');
		assert(p);
		*p++ = '\0';
		from = path;
		to = aprintf("%s/%s", PARK2ROOT, p);
		/*
		 * Send output to /dev/null because we do not want to see the 
		 * warning message when we try to " bk edit" a extra file
		 */
		sysio(NULL, DEV_NULL, DEV_NULL, "bk", "edit", "-q", to, SYS);
		unlink(to); /* careful */
		copyFileOrLink(from, to);
		from = name2cname(from);
		if (exists(from)) {
			p = name2cname(to);
			/* XXX TODO we might want to append, not overwite */
			/* if there is a existing c.file		  */
			fileCopy(from, p); 	/* copy c.file */
			free(p);
		}
		free(from);
		free(to);
	}
	fclose(f);
	
	/*
	 * copy c.Changeset
	 */
	if (exists(CCHANGESET)) {
		to = aprintf("%s/%s", PARK2ROOT, CCHANGESET);
		fileCopy(CCHANGESET, to);
		free(to);
	}

	if (force) {
		f = popen("bk _find . -name '*.rej'", "r");
		while (fnext(path, f)) {
			chomp(path);
			to = aprintf("%s/%s", PARK2ROOT, path);
			/*
			 * We do not want the *.rej file under the SCCS 
			 * directory, those are handled above already.
			 * This only happen when we have a extra or a checked-in
			 * file with * "rej" suffix.
			 */
			if (strstr(to, "/SCCS/")) {
				free(to);
				continue;
			}
			fileCopy(path, to); 	/* copy *.rej file */
			free(to);
		}
		pclose(f);
	}

	if  (streq(parkfile, "-")) {
		if (!error) fprintf(stderr, "Unpark is successful\n");
	} else {
		unlink(parkfile); /* careful */
		if (!error) {
			fprintf(stderr,
			    "Unpark of parkfile_%d is successful\n", id);
		}
	}

skip_apply:
	if (chdir(PARK2ROOT)) {
		perror(PARK2ROOT);
		goto err;
	}
	mdbm_close(idDB);
	if (error && !force) {
		char	s_cset[] = CHANGESET;
		sccs	*s;
		delta 	*d;

		s = sccs_init(s_cset, INIT_SAVEPROJ, bk_proj);
		d = sccs_findKey(s, cset_key);
		fprintf(stderr,
		    "Unpark of parkfile_%d failed. %s not removed.\n"
		    "You can examine the conflict in %s.\n"
		    "You can also get a clean upark by clone -r back to \n"
		    "baseline version: %s\nrev : %s\n",
		    id, PARKDIR, PARKDIR, cset_key, d ? d->rev : "");
		sccs_free(s);
	} else {
		sys(RM, "-rf", PARKDIR, SYS);
	}
	unlink(sfio_list);
	unlink(unpark_list);
	if (parkdir_proj) proj_free(parkdir_proj);
	return (error);
}

int
unpark_main(int ac, char **av)
{
	struct	dirent *e;
	DIR	*dh;
	int	i, c;
	int	top = 0, force = 0, clean = 0;

	if (ac == 2 && streq("--help", av[1])) {
		usage2();
		return (0);
	}

	while ((c = getopt(ac, av, "cf")) != -1) {
		switch (c) {
		    case 'c':	clean = 1; break;
		    case 'f':	force = 1; break;
		    default: 	usage2();
				return (1);
		}
	}


	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "Can't find package root\n");
		return (0);
	}

	if (av[optind]) {
		int id;
		if (streq(av[optind], "-")) {
			id = -1;
		} else {
			id = atoi(av[optind]);
		}
		return (do_unpark(id, clean, force)); 
	}

	dh = opendir(BKTMP);
	unless (dh) {
empty:		fprintf(stderr, "No parkfile found\n");
		return (0);
	}

	/*
	 * The parkfile list is a LIFO, last one parked got unprak first
	 */
	while ((e = readdir(dh)) != NULL) {
		if ((strlen(e->d_name) > 9) &&
		    strneq(e->d_name, "parkfile_", 9)) {
			i = atoi(&(e->d_name[9]));
			if (i > top) top = i; /* get the highest number */
		}
	}
	closedir(dh);
	if (top == 0) goto empty;

	return (do_unpark(top, clean, force));
}





/*
 * Return ture if we can run diff over the baseline delta and the gfile
 */
private int
diffable_text(sccs *s, delta *top)
{
	return (HAS_SFILE(s) && HAS_GFILE(s) &&
		S_ISREG(s->mode) && ((top->mode == 0) || S_ISREG(top->mode)) &&
	    	IS_TEXT(s) && ascii(s->gfile));
}

private int
unsupported_file_type(sccs *s)
{
	return (!S_ISREG(s->mode) && !S_ISLNK(s->mode));
}

private int
parkfile_header(sccs *s, delta *top, char *type, FILE *out)
{
	FILE	*f;
	char	buf[MAXLINE];

	fprintf(out, "# TYPE: %s\n", type);
	if (HAS_SFILE(s)) {
		fputs("# ROOTKEY: ", out);
		sccs_pdelta(s, sccs_ino(s), out);
		fputs("\n", out);
		fputs("# DELTKEY: ", out);
		sccs_pdelta(s, top, out);
		fputs("\n", out);
	}
	f = fopen(sccs_Xfile(s, 'c'), "rt");
	if (f) {
		while (fnext(buf, f)) {
			chomp(buf);
			fprintf(out, "# PARKCMT: %s\n", buf);
		}
		fclose(f);
	}
	fprintf(out, "%c\n", '#'); /* end of header */
	if (fflush(out)) {
		perror("parkfile header");
		return (1); /* failed */
	}
	return (0); /* ok */
}

/*
 * Append "from" file to "to" file
 */
private int
append(char *from, char *to)
{
	MMAP	*m;
	int	fd, n, rc;

	m = mopen(from, "b");
	assert(m);
	fd = open(to, O_APPEND|O_WRONLY, 0600);
	assert(fd >= 0);
	n = write(fd, m->where, msize(m));
	rc = (n != msize(m));
	if (close(fd)) perror(to);
	mclose(m);
	return (rc);
}
