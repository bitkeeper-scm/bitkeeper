/* Copyright (c) 2000 Andrew Chang */ 
#include "system.h"
#include "sccs.h"

private	void	print_title(void);

private project *proj;
private	int	expandkeywords;

private void
mkgfile(sccs *s, char *rev, char *path, char *tmpdir, char *tag,
					int fix_mod_time, MDBM *db)
{
	char	tmp_path[MAXPATH];
	delta *d;
	int flags = SILENT;

	sprintf(tmp_path, "%s/%s/%s", tmpdir, tag, path);
	if (isNullFile(rev, path))  return;
	d = sccs_findrev(s, rev);
	assert(d);
	unless ((d->mode == 0) || S_ISREG(d->mode)) {
		fprintf(stderr,
    "%s is not regular file, converted to empty file\n", d->pathname);
		return;
	}
	free(s->gfile);
	s->gfile = strdup(tmp_path);
	check_gfile(s, 0);
	mkdirf(tmp_path);
	if (fix_mod_time) flags |= GET_DTIME;
	if (expandkeywords) flags |= GET_EXPAND;
	if (sccs_get(s, rev, 0, 0, 0, flags, "-")) {
		fprintf(stderr, "Cannot get %s, rev %s\n",
							s->sfile, rev);
		exit(1);
	}
	sprintf(tmp_path, "%s/%s", tag, path);
	mdbm_store_str(db, tmp_path, "", MDBM_INSERT);
}

private void
process(char *path0, char *path1, char *rev1,
	char *path2, char *rev2, char *tmpdir, int fix_mod_time, MDBM *db)
{
	sccs *s;
	char *t;

	t = name2sccs(path0);
	s = sccs_init(t, SILENT|INIT_SAVEPROJ, proj);
	free(t);
	assert(s);
	unless (proj) proj = s->proj;
	unless ((s->encoding == E_ASCII) || (s->encoding == E_GZIP)) {
		fprintf(stderr, "Warning: %s is not a text file\n", s->sfile);
	}
	mkgfile(s, rev1, path1, tmpdir, "a", fix_mod_time, db);
	mkgfile(s, rev2, path2, tmpdir, "b", fix_mod_time, db);
	sccs_free(s);
}

/*
 * If the file is listed in the "null file db"
 * fix the "diff -Nur" header line to show "/dev/null ..."
 * Note: This really only needed for SGI IRIX, other system seems to
 * handle the original diff header just fine.
 */
private void
fix_header(char *buf, MDBM *db)
{
	char *p, *line;

	if (!strneq(buf, "+++ ", 4) &&
	    !strneq(buf, "--- ", 4) &&
	    !strneq(buf, "*** ", 4)) {
		fprintf(stderr, "gnupatch: bad header line:<%s>\n", buf);
		return;
	}
	line = &buf[4];
	p = strchr(line, '\t');
	unless (p) {
		fprintf(stderr, "gnupatch: bad header line:<%s>\n", buf);
		fputs(line, stdout);
		return;
		
	}

	buf[3] = 0;
	fputs(buf, stdout); fputs(" ", stdout);
	*p = 0;
	unless (mdbm_fetch_str(db, line)) {
		/* /dev/null must be printed with EPOC time */
		fputs("/dev/null\t", stdout);
		memmove(++p,"Wed Dec 31 16:00:00 1969", 24);
		fputs(p, stdout);
	} else {
		*p = '\t';
		fputs(line, stdout);
	}
}

private void
print_title()
{
	printf(
"# This is a BitKeeper generated patch for the following project:\n\
# Project Name: %s\n\
# This patch format is intended for GNU patch command version 2.5 or higher.\n\
# This patch includes the following deltas:\n", package_name());               
}

/*
 * print info for new/moved/deleted files
 */
private void
print_moved(char *path1, char *rev1, char *path2, char *rev2)
{
	printf("#\t"); 
	if (isNullFile(rev1, path1)) {
		printf("%20s\t%-7s", "(new)", "");
	} else {
		printf("%20s\t%-7s", path1, rev1);
	}
	printf(" -> ");
	if (isNullFile(rev2, path2)) {
		printf("%-7s %-15s\n", "", "(deleted)");
	} else {
		if (isNullFile(rev1, path1)) {
			printf("%-7s %-15s\n", rev2,  path2);
		} else {
			printf("%-7s %-15s (moved)\n", rev2,  path2);
		}
	}
}

private void
print_entry(char *path1, char *rev1, char *path2, char *rev2)
{
	if (!streq(path1, path2) ||
	    streq(rev1, "1.0") ||
	    streq(rev2, "1.0")) {
		print_moved(path1, rev1, path2, rev2);
	} else {
		printf("#\t%20s\t%-7s -> %-7s\n", path1, rev1, rev2);
	}
}

private void
print_cset_log(char *cset1, char *cset2)
{
	char revs[50], xrev[50];
	char *dspec =
"-d# :D:\t:P:@:HT:\t:I:\n$each(:C:){# (:C:)\n}# --------------------------------------------\n";
	char *prs_av[] = {"bk", "prs", "-hb", revs, xrev, dspec, "ChangeSet", 0};

	/*
	 * XXX FIXME need to handle the case where
	 * cset1 is parent+merge_parent
	 */
	if (strchr(cset1, '+')) goto done;

	printf("#\n# The following is the BitKeeper ChangeSet Log\n");
	printf("# --------------------------------------------\n");
	fflush(stdout);
	sprintf(revs, "-r%s..%s", cset1, cset2);
	sprintf(xrev, "-x%s", cset1);
	spawnvp_ex(_P_WAIT, "bk", prs_av);
done:	printf("#\n");
	fflush(stdout);
}

int
gnupatch_main(int ac, char **av)
{
	char buf[MAXPATH * 3];
	char tmpdir[MAXPATH];
	char *path0, *path1, *rev1, *path2, *rev2;
	char *cset1 = 0,  *cset2 = 0;
	char *diff_style = 0;
	char diff_opts[50] ;
	char *diff_av[] = { "diff", diff_opts, "a", "b", 0 };
	char *clean_av[] = { "rm", "-rf", tmpdir, 0 };
	int  c, rfd, header = 1, fix_mod_time = 0, got_start_header = 0;
	FILE *pipe;
	MDBM *db;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help gnupatch");
		return (1);
	}

	while ((c = getopt(ac, av, "ehTd|")) != -1) { 
		switch (c) {
		    case 'e':
			expandkeywords = 1; break;		/* doc 2.1 */
		    case 'h':					/* doc 2.0 */
			header = 0; break; /* disable header */
		    case 'T':	fix_mod_time = 1; break;	/* doc 2.0 */
		    case 'd':					/* doc 2.0 */
			diff_style = optarg ? optarg : ""; break;
		    default:	system("bk help -s gnupatch");
				return (1);
		}
	}

        if (sccs_cd2root(0, 0)) {
                fprintf(stderr, "gnupatch: cannot find package root.\n");
                exit(1);
        }      

	sprintf(tmpdir, "%s/bk%u", TMP_PATH, getpid());
	sprintf(buf, "%s/a", tmpdir);
	if (mkdirp(buf)) {
                fprintf(stderr, "gnupatch: cannot mkdir%s.\n", buf);
		exit(1);
	}
	sprintf(buf, "%s/b", tmpdir);
	if (mkdirp(buf)) {
                fprintf(stderr, "gnupatch: cannot mkdir%s.\n", buf);
		exit(1);
	}
	db = mdbm_open(NULL, 0, 0, GOOD_PSIZE); /* db for null file */
	if (header) print_title();

	/*
	 * parse the rev list from stdin
	 */
	while (fgets(buf, sizeof(buf), stdin)) {
		chop(buf);
		path0 = buf;
		path1 = strchr(buf, BK_FS);
		unless (path1) {
		err:
			fprintf(stderr, 
"gnupatch ERROR: bad input format expected: <cur>%c<start>%c<rev>%c<end>%c<rev>\n",
			    BK_FS, BK_FS, BK_FS, BK_FS);
			exit(1);
		}
		assert(path1);
		*path1++ = 0;
		rev1 = strchr(path1, BK_FS);
		unless (rev1) goto err;
		*rev1++ = 0;
		path2 = strchr(rev1, BK_FS);
		unless (path2) goto err;
		*path2++ = 0;
		rev2 = strchr(path2, BK_FS);
		unless (rev2) goto err;
		*rev2++ = 0;
		if (header) print_entry(path1, rev1, path2, rev2);
		if (streq(path0, "ChangeSet")) {
			cset1 = strdup(rev1);
			cset2 = strdup(rev2);
			continue;
		}
		/*
		 * populate the left & right tree
		 */
		process(path0, path1, rev1, path2, rev2, tmpdir,
							fix_mod_time, db);
	}

	if (header && cset1) print_cset_log(cset1, cset2);
	chdir(tmpdir);
	/*
	 * now "diff -Nr" the left & right tree
	 * and fix up the diff header
	 */
	unless (diff_style) diff_style = "u";
	sprintf(diff_opts, "-Nr%c", diff_style[0]);
	spawnvp_rPipe(diff_av, &rfd, BIG_PIPE);
	pipe = fdopen(rfd, "r");
	while (fgets(buf, sizeof(buf), pipe)) {
		if (got_start_header) {
			got_start_header--;
			fix_header(buf, db);
			continue;
		}
	
		if (strneq("diff -Nru", buf, 9) ||
		    strneq("diff -Nrc", buf, 9)) { 
			got_start_header = 2;
		} else {
			got_start_header = 0;
		}
		fputs(buf, stdout);
	}
	fclose(pipe);

	/*
	 * all done, clean up
	 */
	mdbm_close(db);
	if (proj) proj_free(proj);
	if (cset1) free(cset1);
	if (cset2) free(cset2);
	chdir((char *) TMP_PATH);
	spawnvp_ex(_P_NOWAIT, "rm", clean_av);
	return(0);
}
