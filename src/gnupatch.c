/*
 * Copyright 2000-2006,2008,2010-2011,2015-2016 BitMover, Inc
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

private	void	print_title(char *, char *);

private	int	expandkeywords;

private void
mkgfile(sccs *s, char *rev, char *path, char *tmpdir, char *tag,
					int fix_mod_time, MDBM *db)
{
	char	tmp_path[MAXPATH];
	ser_t	d;
	int	flags = SILENT;

	sprintf(tmp_path, "%s/%s/%s", tmpdir, tag, path);
	if (isNullFile(rev, path))  return;
	d = sccs_findrev(s, rev);
	assert(d);
	unless ((MODE(s, d) == 0) || S_ISREG(MODE(s, d))) {
		fprintf(stderr,
		    "%s is not regular file, converted to empty file\n",
		    PATHNAME(s, d));
		return;
	}
	mkdirf(tmp_path);
	if (fix_mod_time) flags |= GET_DTIME;
	if (expandkeywords) flags |= GET_EXPAND;
	if (sccs_get(s, rev, 0, 0, 0, flags, tmp_path, 0)) {
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
	s = sccs_init(t, SILENT);
	free(t);
	assert(s);
	unless (ASCII(s)) {
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
		/* /dev/null must be printed with EPOCH time */
		fputs("/dev/null\tThu Jan  1 00:00:00 1970 -00:00\n", stdout);
	} else {
		*p = '\t';
		fputs(line, stdout);
	}
}

private void
print_title(char *r1, char *r2)
{
	FILE	*f;
	char	*p;
	char	*d = ":DPN:\\n  :D: :T::TZ: :P:@:HT: "
		     "$unless(:DPN:=ChangeSet){+:LI: -:LD:}\\n"
		     "$each(:C:){  (:C:)\\n}"
		     "$each(:SYMBOL:){  TAG: (:SYMBOL:)\\n}\\n";
	char	buf[BUFSIZ];

	printf("# This is a BitKeeper generated diff -Nru style patch.\n#\n");
	p = aprintf("bk changes -r'%s' -r'%s' -Pvd'%s' -",
	    r1, r2, d);
	f = popen(p, "r");
	free(p);
	while (fnext(buf, f)) printf("# %s", buf);
	pclose(f);
}

int
gnupatch_main(int ac, char **av)
{
	char here[MAXPATH];
	char buf[MAXPATH * 3];
	char tmpdir[MAXPATH];
	char *path0, *path1, *rev1, *path2, *rev2;
	char *cset1 = 0,  *cset2 = 0;
	char *diff_style = "u";
	char diff_opts[50] ;
	char *diff_av[] = { "diff", diff_opts, "a", "b", 0 };
	int  c, header = 1, fix_mod_time = 0, got_start_header = 0;
	int  n = 0;
	FILE *pipe;
	MDBM *db;

	while ((c = getopt(ac, av, "d|ehpT", 0)) != -1) {
		switch (c) {
		    case 'd':
			diff_style = notnull(optarg); break;
		    case 'e':
			expandkeywords = 1; break;		/* doc 2.1 */
		    case 'h':					/* doc 2.0 */
			header = 0; break; /* disable header */
		    case 'p':
			diff_style = "up"; break;
		    case 'T':	fix_mod_time = 1; break;	/* doc 2.0 */
		    default: bk_badArg(c, av);
		}
	}

        if (proj_cd2root()) {
                fprintf(stderr, "gnupatch: cannot find package root.\n");
                exit(1);
        }
	strcpy(here, proj_cwd());

	bktmp_dir(tmpdir);
	sprintf(buf, "%s/a", tmpdir);
	if (mkdirp(buf)) {
                fprintf(stderr, "gnupatch: cannot mkdir %s.\n", buf);
		exit(1);
	}
	sprintf(buf, "%s/b", tmpdir);
	if (mkdirp(buf)) {
                fprintf(stderr, "gnupatch: cannot mkdir %s.\n", buf);
		exit(1);
	}
	db = mdbm_open(NULL, 0, 0, GOOD_PSIZE); /* db for null file */

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
			assert(isdir(tmpdir));
			rmtree(tmpdir);
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
		if (streq(path0, "ChangeSet")) {
			cset1 = strdup(rev1);
			cset2 = strdup(rev2);
			if (header) print_title(rev1, rev2);
			continue;
		}
		/*
		 * populate the left & right tree
		 */
		process(path0, path1, rev1, path2, rev2, tmpdir,
							fix_mod_time, db);
		n++;
	}

	unless (n) {
		fprintf(stderr, "%s: no revs on stdin\n", prog);
		rmtree(tmpdir);
		exit(1);
	}

	chdir(tmpdir);
	/*
	 * now "diff -Nr" the left & right tree
	 * and fix up the diff header
	 */
	sprintf(diff_opts, "-Nr%s", diff_style);
	pipe = popenvp(diff_av, "r");
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
	pclose(pipe);

	/*
	 * all done, clean up
	 */
	mdbm_close(db);
	if (cset1) free(cset1);
	if (cset2) free(cset2);
	/*
	 * proj_cd2root() will get confused by the tmp tree so
	 * use the saved location or we'll litter this tmpdir.
	 */
	chdir(here);
	assert(isdir(tmpdir));
	rmtree(tmpdir);
	return (0);
}
