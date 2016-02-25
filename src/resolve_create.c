/*
 * Copyright 2000-2016 BitMover, Inc
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

/*
 * resolve_create.c - resolver for created files
 */
#include "resolve.h"

private	int	prs_common(resolve *rs, sccs *s, char *a, char *b);
private	void	getFileConflict(resolve *rs, char *gfile, char *path);

int
res_abort(resolve *rs)
{
	if (confirm("Abort patch?")) {
		if (rs->s) {
			sccs_free(rs->s);
			rs->s = 0;
		}
		resolve_cleanup(rs->opts, CLEAN_ABORT);
	}
	return (0);
}

/*
 * Diff the local against the remote;
 */
private int
res_diffCommon(resolve *rs, 
    int (*differ)(resolve *rs, char *left, char *right, int wait))
{
	char	*rev = 0;

	if (rs->tnames) {
		differ(rs, rs->tnames->local, rs->tnames->remote, 0);
		return (0);
	}
	if (rs->res_gcreate) {
		char	right[MAXPATH];

		bktmp(right);
		if (rs->revs) rev = rs->revs->remote;
		if (sccs_get(rs->s, rev, 0, 0, 0, SILENT, right, 0)) {
			fprintf(stderr, "get failed, can't diff.\n");
			return (0);
		}
		chdir(RESYNC2ROOT);
		differ(rs, PATHNAME(rs->s, rs->d), right, 1);
		chdir(ROOT2RESYNC);
		unlink(right);
		return (0);
	}
	if (rs->res_screate || rs->res_resync) {
		char	left[MAXPATH];
		char	right[MAXPATH];
		sccs	*s = (sccs*)rs->opaque;

		bktmp(left);
		bktmp(right);
		if (rs->revs) rev = rs->revs->remote;
		if (sccs_get(s, 0, 0, 0, 0, SILENT, left, 0) ||
		    sccs_get(rs->s, rev, 0, 0, 0, SILENT, right, 0)) {
			fprintf(stderr, "get failed, can't diff.\n");
			unlink(left);
			return (0);
		}
		differ(rs, left, right, 1);
		unlink(left);
		unlink(right);
		return (0);
	}
	fprintf(stderr, "Don't know how to diff the files.\n");
	return (0);
}

int
do_diff(resolve *rs, char *left, char *right, int wait)
{
	char	tmp[MAXPATH];

	bktmp(tmp);
	sysio(0, tmp, 0, "bk", "diff", "-a", left, right, SYS);
	more(rs, tmp);
	unlink(tmp);
	return (0);
}

private int
do_sdiff(resolve *rs, char *left, char *right, int wait)
{
	char	tmp[MAXPATH];
	int	cols = 0;

	if (tty_init()) {
		cols = tty_cols();
		tty_done();
	}
	if (cols <= 0) cols = 80;
	bktmp(tmp);
	systemf("bk ndiff --sdiff=%d '%s' '%s' > '%s'",
	    cols, left, right, tmp);
	more(rs, tmp);
	unlink(tmp);
	return (0);
}

private int
do_difftool(resolve *rs, char *left, char *right, int wait)
{
	char	*av[10];

	av[0] = "bk";
	av[1] = "difftool";
	av[2] = left;
	av[3] = right;
	av[4] = 0;
	if (wait) {
		return (spawnvp(_P_WAIT, "bk", av));
	} else {
		spawnvp(_P_NOWAIT, "bk", av);
	}
	return (0);
}

int
res_diff(resolve *rs)
{
	return (res_diffCommon(rs, do_diff));
}

int
res_sdiff(resolve *rs)
{
	return (res_diffCommon(rs, do_sdiff));
}

int
res_difftool(resolve *rs)
{
	return (res_diffCommon(rs, do_difftool));
}

int
res_mr(resolve *rs)
{
	char	buf[MAXPATH];
	char	*t, *why = 0;

	unless (prompt("Move file to:", buf)) return (0);
	if (bk_badFilename(0, buf)) {
		fprintf(stderr, "Illegal filename: %s\n", buf);
		return (0);
	}
	if ((buf[0] == '/') || strneq("../", buf, 3)) {
		fprintf(stderr, "Destination must be in repository.\n");
		return (0);
	}
	if (sccs_filetype(buf) != 's') {
		t = name2sccs(buf);
		strcpy(buf, t);
		free(t);
	}
	switch (slotTaken(rs, buf, &why)) {
	    case SFILE_CONFLICT:
		fprintf(stderr, "%s exists locally already\n", buf);
		return (0);
	    case GONE_SFILE_CONFLICT:
		fprintf(stderr,
		    "%s exists locally already but is marked gone\n", buf);
		return (0);
	    case GFILE_CONFLICT:
		t = sccs2name(buf);
		fprintf(stderr, "%s exists locally already\n", t);
		free(t);
		return (0);
	    case RESYNC_CONFLICT:
		fprintf(stderr, "%s exists in RESYNC already\n", buf);
		return (0);
	    case COMP_CONFLICT:
		fprintf(stderr,
		    "%s conflicts with another component (%s)\n", buf, why);
		return (0);
	}
	if (move_remote(rs, buf)) {
		perror("move_remote");
		exit(1);
	}
	return (1);
}

int
more(resolve *rs, char *file)
{
	char *cmd;

	/*
	 * must use system(), _not_ sysio()
	 * because on win32, pager may have arguments
	 */
	cmd = aprintf("%s < '%s'", rs->pager, file);
	system(cmd);
	return (0);
}

int
res_vl(resolve *rs)
{
	sccs	*s = 0;
	char	*rev = 0;
	char	left[MAXPATH];

	if (rs->tnames) {
		more(rs, rs->tnames->local);
		return (0);
	}
	if (rs->res_gcreate) {
		chdir(RESYNC2ROOT);
		more(rs, PATHNAME(rs->s, rs->d));
		chdir(ROOT2RESYNC);
		return (0);
	}
	unless (rs->res_screate || rs->res_resync) {
		fprintf(stderr, "Don't know how to view the file.\n");
		return (0);
	}
	if (rs->res_screate) {
		s = (sccs*)rs->opaque;
	} else {
		s = rs->s;
		if (rs->revs) rev = rs->revs->local;
	}
	bktmp(left);
	if (sccs_get(s, rev, 0, 0, 0, SILENT, left, 0)) {
		    fprintf(stderr, "get failed, can't view.\n");
		return (0);
	}
	more(rs, left);
	unlink(left);
	return (0);
}

int
res_vr(resolve *rs)
{
	char	*name = rs->s->gfile;
	char	tmp[MAXPATH];
	char	rev[MAXPATH];

	if (rs->tnames) {
		more(rs, rs->tnames->remote);
		return (0);
	}
	sprintf(rev, "-r%s", rs->revs ? rs->revs->remote : "+");
	if (rs->res_resync) name = rs->dname;
	bktmp(tmp);
	sysio(0, tmp, 0, "bk", "get", "-qkp", rev, name, SYS);
	more(rs, tmp);
	unlink(tmp);
	return (0);
}

private int
revtool(char *name)
{
	char	*av[10];

	av[0] = "bk";
	av[1] = "revtool";
	av[2] = name;
	av[3] = 0;
	spawnvp(_P_NOWAIT, "bk", av);
	return (0);
}

private int
res_pl(resolve *rs)
{
	if (rs->res_screate) {
		sccs	*s = (sccs*)rs->opaque;

		chdir(RESYNC2ROOT);
		revtool(s->gfile);
		chdir(ROOT2RESYNC);
		return (0);
	}
	fprintf(stderr, "Don't know how to view the file.\n");
	return (0);
}

private int
res_pr(resolve *rs)
{
	revtool(rs->s->gfile);
	return (0);
}

int
res_h(resolve *rs)
{
	char	tmp[MAXPATH];
	sccs	*s;

	if (rs->res_gcreate) {
		fprintf(stderr, "No history file available\n");
		return (0);
	}
	if (rs->res_screate) {
		s = (sccs*)rs->opaque;
	} else {
		s = rs->s;
	}
	bktmp(tmp);
	sysio(0, tmp, 0, "bk", "prs", s->gfile, SYS);
	more(rs, tmp);
	unlink(tmp);
	return (0);
}

int
res_hl(resolve *rs)
{
	if (rs->res_gcreate) {
		fprintf(stderr, "No history file available\n");
		return (0);
	}
	if (rs->res_screate) return (res_h(rs));
	unless (rs->revs) return (res_h(rs));
	prs_common(rs, rs->s, rs->revs->remote, rs->revs->local);
	return (0);
}

int
res_hr(resolve *rs)
{
	if (rs->res_resync) {
		char	tmp[MAXPATH];
		sccs	*s = (sccs*)rs->opaque;

		bktmp(tmp);
		sysio(0, tmp, 0, "bk", "prs", s->gfile, SYS);
		more(rs, tmp);
		unlink(tmp);
		return (0);
	}
	unless (rs->revs) return (res_h(rs));
	prs_common(rs, rs->s, rs->revs->local, rs->revs->remote);
	return (0);
}

private int
prs_common(resolve *rs, sccs *s, char *a, char *b)
{
	char	tmp[MAXPATH];
	char	*list, *l2;

	list = sccs_impliedList(s, "resolve", a, b);
	l2 = malloc(strlen(list) + 3);
	if (rs->opts->debug) {
		fprintf(stderr, "prs(%s, %s, %s) = %s\n", s->gfile, a, b, list);
	}
	sprintf(l2, "-r%s", list);
	bktmp(tmp);
	sysio(0, tmp, 0, "bk", "prs", l2, rs->s->gfile, SYS);
	more(rs, tmp);
	unlink(tmp);
	free(list);
	free(l2);
	return (0);
}

int
res_quit(resolve *rs)
{
	assert(exists(RESYNC2ROOT "/" ROOT2RESYNC));
	chdir(RESYNC2ROOT);
	proj_restoreAllCO(0, 0, 0, 0);
	sccs_unlockfile(RESOLVE_LOCK);
	exit(1);
	return (-1);
}

private int
gc_explain(resolve *rs)
{
	fprintf(stderr,
"There is a local working file:\n\
\t``%s''\n\
which has no associated revision history file.\n\
The patch you are importing has a file which wants to be in the same\n\
place as the local file.  Your choices are:\n\
a) do not move the local file, which means that the entire patch will\n\
   be aborted, discarding any other merges you may have done.  You can\n\
   then check in the local file and retry the patch.  This is the best\n\
   choice if you have done no merge work yet.\n\
b) remove the local file after making sure it is not something that you\n\
   need.  There are commands you can run now to show you both files.\n\
c) remove the remote file after making sure it is not something that you\n\
   need.  There are commands you can run now to show you both files.\n\
d) move the local file to some other pathname.\n\
e) move the remote file to some other pathname.\n\
\n\
The choices b, c, d, or e will allow the file in the patch to be created\n\
and you to continue with the rest of the patch.\n\
\n\
Warning: choices b and d are not recorded because there is no SCCS file\n\
associated with the local file.  So if the rest of the resolve does not\n\
complete for some reason, it is up to you to go find that file and move it\n\
back by hand, if that is what you want.\n\n",
	    PATHNAME(rs->s, rs->d));
	return (0);
}

private int
gc_help(resolve *rs)
{
	int	i;

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
Local file: ``%s''\n\
has a name conflict with a file with the same name in the patch.\n\
The local file is not under revision control.\n\
---------------------------------------------------------------------------\n",
	    PATHNAME(rs->s, rs->d));
	fprintf(stderr, "Commands are:\n\n");
	for (i = 0; rs->funcs[i].spec; i++) {
		fprintf(stderr, "  %-4s - %s\n", 
		    rs->funcs[i].spec, rs->funcs[i].help);
	}
	fprintf(stderr, "\n");
	return (0);
}

private int
common_ml(resolve *rs, char *p, char *buf)
{
	char	path[MAXPATH];
	char	*t;

	unless (prompt(p, buf)) return (1);
	if (bk_badFilename(0, buf)) {
		fprintf(stderr, "Illegal filename: %s\n", buf);
		return (0);
	}
	if ((buf[0] == '/') || strneq("../", buf, 3)) {
		fprintf(stderr, "Destination must be in repository.\n");
		return (1);
	}
	if (sccs_filetype(buf) != 's') {
		t = name2sccs(buf);
		strcpy(buf, t);
		free(t);
	}
	switch (slotTaken(rs, buf, 0)) {
	    case SFILE_CONFLICT:
		fprintf(stderr, "%s exists locally already\n", buf);
		return (1);
	    case GONE_SFILE_CONFLICT:
		fprintf(stderr,
		    "%s exists locally already but is marked gone\n", buf);
		return (1);
	    case DIR_CONFLICT:
		getFileConflict(rs, buf, path);
		fprintf(stderr, "file %s exists locally already\n", path);
		return (1);
	    case GFILE_CONFLICT:
		t = sccs2name(buf);
		fprintf(stderr, "%s exists locally already\n", t);
		free(t);
		return (1);
	    case RESYNC_CONFLICT:
		fprintf(stderr, "%s exists in RESYNC already\n", buf);
		return (1);
	}
	return (0);
}

private int
gc_ml(resolve *rs)
{
	char	buf[MAXPATH];
	char	*t;

	if (common_ml(rs, "Move local file to:", buf)) return (0);
	chdir(RESYNC2ROOT);
	t = sccs2name(buf);
	if (rs->opts->debug) {
		fprintf(stderr, "rename(%s, %s)\n", PATHNAME(rs->s, rs->d), t);
	}
	if (rename(PATHNAME(rs->s, rs->d), t)) {
		perror("rename");
		exit(1);
	}
	free(t);
	chdir(ROOT2RESYNC);
	return (EAGAIN);
}

private int
dc_ml(resolve *rs)
{
	int	count = 0;
	FILE	*list;
	char	*t;
	char	buf[MAXPATH];
	char	path[MAXPATH];

	/* Check to see that a simple rename is possible */
	getFileConflict(rs, PATHNAME(rs->s, rs->d), path);
	chdir(RESYNC2ROOT);
	t = aprintf("bk gfiles '%s'", path);
	list = popen(t, "r");
	free(t);
	while (fnext(buf, list)) count++;
	pclose(list);
	chdir(ROOT2RESYNC);
	if (count) {
		fprintf(stderr,
		    "Local directory has version controlled files.\n"
		    "Either do a 'mr', then fix up after the merge,\n"
		    "or 'bk abort' the merge, and 'bk mv' the directory.\n");
		return (0);
	}
	if (common_ml(rs, "Move local directory to:", buf)) return (0);
	t = sccs2name(buf);
	chdir(RESYNC2ROOT);
	if (rs->opts->debug) {
		fprintf(stderr, "rename(%s, %s)\n", path, t);
	}
	if (rename(path, t)) {
		perror("rename");
		exit(1);
	}
	chdir(ROOT2RESYNC);
	free(t);
	return (EAGAIN);
}

private int
gc_remove(resolve *rs)
{
	char	buf[MAXPATH];
	opts	*opts = rs->opts;

	sprintf(buf, "%s/%s", RESYNC2ROOT, PATHNAME(rs->s, rs->d));
	assert(!isdir(buf));
	unless (rs->opts->force || confirm("Remove local file?")) return (0);
	unlink(buf);
	if (opts->log) fprintf(stdlog, "unlink(%s)\n", buf);
	return (EAGAIN);
}

int
gc_sameFiles(resolve *rs)
{
	opts	*opts = rs->opts;
	char	buf[MAXPATH];
	int	same;

	bktmp(buf);
	if (sccs_get(rs->s, 0, 0, 0, 0, SILENT, buf, 0)) {
		fprintf(stderr, "get failed, can't diff.\n");
		return (0);
	}
	chdir(RESYNC2ROOT);
	same = sameFiles(PATHNAME(rs->s, rs->d), buf);
	chdir(ROOT2RESYNC);
	unlink(buf);
	if (same) {
		if (opts->log) {
			fprintf(stdlog, "same files %s\n",
			    PATHNAME(rs->s, rs->d));
		}
		sprintf(buf, "%s/%s", RESYNC2ROOT, PATHNAME(rs->s, rs->d));
		assert(!isdir(buf));
		if (unlink(buf)) {
			fprintf(stderr,
			    "Unable to remove local file %s\n",
			    PATHNAME(rs->s, rs->d));
			return (0);
		}
		if (opts->log) fprintf(stdlog, "unlink(%s)\n", buf);
		return (EAGAIN);
	}
	return (0);
}

private void
getFileConflict(resolve *rs, char *gfile, char *path)
{
	char	*t, *s;
	int	i;
	
	if (i = comp_overlap(rs->opts->complist, gfile)) {
		strcpy(path, rs->opts->complist[i]);
		return;
	}
	chdir(RESYNC2ROOT);
	if (exists(gfile)) {
		strcpy(path, gfile);
		chdir(ROOT2RESYNC);
		return;
	}
	for (t = strrchr(gfile, '/'); t; ) {
		*t = 0;
		if (exists(gfile) && !isdir(gfile)) {
			strcpy(path, gfile);
			*t = '/';
			chdir(ROOT2RESYNC);
			return;
		}
		s = t;
		t = strrchr(t, '/');
		*s = '/';
	}
	path[0] = 0;
	chdir(ROOT2RESYNC);
	return;
}

private int
walk_print(char *path, char type, void *data)
{
	char	***files = (char ***)data;

	if ((type == 'd') || isSCCS(path)) return (0);
	*files = addLine(*files, strdup(path));
	return (0);
}

private int
dc_remove(resolve *rs)
{
	char	buf[MAXPATH];
	char	path[MAXPATH];
	opts	*opts = rs->opts;
	int	ret;

	unless (rs->opts->force || confirm("Remove local directory?")) {
		return (0);
	}
	getFileConflict(rs, PATHNAME(rs->s, rs->d), path);
	sprintf(buf, "%s/%s", RESYNC2ROOT, path);
	if (ret = rmdir(buf)) {
		int	i, n, saved_errno;
		char	**files = 0;

		saved_errno = errno;

		if ((errno == ENOTEMPTY) || (errno == EACCES)) {
			walkdir(buf, (walkfns){ .file = walk_print}, &files);
			unless (n = nLines(files)) {
				errno = saved_errno;
				goto out;
			}
			fprintf(stderr, "Could not remove '%s' because "
			    "of the following file%s:\n", buf,
			    (n == 1) ? "" : "s");
			EACH(files) fprintf(stderr, "  %s\n", files[i]);
			freeLines(files, free);
		} else {
			perror(buf);
		}
	}
out:	if (opts->log) fprintf(stdlog, "rmdir(%s) = %d\n", buf, ret);
	return (EAGAIN);
}

private int
dc_explain(resolve *rs)
{
	char	path[MAXPATH];

	getFileConflict(rs, PATHNAME(rs->s, rs->d), path);
	fprintf(stderr,
"The path of the remote file: ``%s''\n\
conflicts with a local directory: ``%s''\n\
Your choices are:\n\
a) do not move the local file, which means that the entire patch will\n\
   be aborted, discarding any other merges you may have done.  You can\n\
   then check in the local file and retry the patch.  This is the best\n\
   choice if you have done no merge work yet.\n\
b) remove the local file after making sure it is not something that you\n\
   need.\n\
c) remove the remote file after making sure it is not something that you\n\
   need.\n\
d) move the local file to some other pathname.\n\
e) move the remote file to some other pathname.\n\
\n\
The choices b, c, d, or e will allow the file in the patch to be created\n\
and you to continue with the rest of the patch.\n\
\n\
Warning: choices b and d are not recorded because there is no SCCS file\n\
associated with the local file.  So if the rest of the resolve does not\n\
complete for some reason, it is up to you to go find that file and move it\n\
back by hand, if that is what you want.\n\n",
	    PATHNAME(rs->s, rs->d), path);
	return (0);
}

private int
dc_help(resolve *rs)
{
	int	i;
	sccs	*local;
	char	path[MAXPATH];
	char	buf[MAXKEY];

	getFileConflict(rs, PATHNAME(rs->s, rs->d), path);
	sccs_sdelta(rs->s, sccs_ino(rs->s), buf);
	chdir(RESYNC2ROOT);
	local = sccs_keyinit(0, buf, INIT_NOCKSUM, rs->opts->idDB);
	chdir(ROOT2RESYNC);
	fprintf(stderr,
"---------------------------------------------------------------------------\n\
Remote file:\n\t``%s''\n", PATHNAME(rs->s, rs->d));
	if (local) {
		fprintf(stderr, "which matches file\n\t``%s''\n", local->gfile);
		sccs_free(local);
	}
	fprintf(stderr,
"wants to be in same place as local directory\n\t``%s''\n\
---------------------------------------------------------------------------\n",
	    path);
	fprintf(stderr, "Commands are:\n\n");
	for (i = 0; rs->funcs[i].spec; i++) {
		fprintf(stderr, "  %-4s - %s\n", 
		    rs->funcs[i].spec, rs->funcs[i].help);
	}
	fprintf(stderr, "\n");
	return (0);
}

private int
sc_explain(resolve *rs)
{
	fprintf(stderr,
"There is a local working file:\n\
\t``%s''\n\
The patch you are importing has a file which wants to be in the same\n\
place as the local file.  Your choices are:\n\
a) do not move either file, which means that the entire patch will\n\
   be aborted, discarding any other merges you may have done.  You can\n\
   then move or delete the local file and retry the patch.\n\
   This is a good choice if you have done no merge work yet.\n\
b) remove the local file, which will leave you with the remote file.\n\
c) remove the remote file, which will leave you with the local file.\n\
d) move the local file to some other pathname.\n\
e) move the remote file to some other pathname.\n\
\n\
All choices other than (a) will allow the file in the patch to be created\n\
and you to continue with the rest of the patch.\n\n", PATHNAME(rs->s, rs->d));
	return (0);
}

private int
sc_help(resolve *rs)
{
	int	i;

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
Local file: ``%s''\n\
has a name conflict with a new file with the same name in the patch.\n\
---------------------------------------------------------------------------\n",
	    PATHNAME(rs->s, rs->d));
	fprintf(stderr, "Commands are:\n\n");
	for (i = 0; rs->funcs[i].spec; i++) {
		fprintf(stderr, "  %-4s - %s\n", 
		    rs->funcs[i].spec, rs->funcs[i].help);
	}
	fprintf(stderr, "\n");
	return (0);
}

/*
 * need to check that the local file isn't edited and doesn't
 * have any pending deltas.
 */
int
ok_local(sccs *s, int check_pending)
{
	ser_t	d;
	int	rc = 0;

	unless (s) {
		fprintf(stderr, "Can't init the conflicting local file\n");
		exit(1);
	}
	chdir(RESYNC2ROOT);
	if ((EDITED(s) || LOCKED(s)) && sccs_clean(s, SILENT)) {
		fprintf(stderr,
		    "Cannot [re]move modified local file %s\n", s->gfile);
		goto done;
	}
	unless (check_pending) goto good;
	d = sccs_top(s);
	unless (FLAGS(s, d) & D_CSET) {
		fprintf(stderr,
		    "Cannot [re]move uncommitted local file %s\n", s->gfile);
		goto done;
	}
good:	rc = 1;
done:
	chdir(ROOT2RESYNC);
	return (rc);
}

private int
sc_ml(resolve *rs)
{
	char	buf[MAXPATH];
	char	path[MAXPATH];
	char	cmd[MAXPATH*2];
	char	*to, *tmp;
	int	filenum = 0;
	resolve	*rs2;
	sccs	*s;

	unless (prompt("Move local file to:", buf)) return (0);
	if (bk_badFilename(0, buf)) {
		fprintf(stderr, "Illegal filename: %s\n", buf);
		return (0);
	}
	if ((buf[0] == '/') || strneq("../", buf, 3)) {
		fprintf(stderr, "Destination must be in repository.\n");
		return (0);
	}
	if (rs->opts->debug) fprintf(stderr, "%s\n", buf);
	if (sccs_filetype(buf) != 's') {
		to = name2sccs(buf);
	} else {
		to = strdup(buf);
	}
	switch (slotTaken(rs, to, 0)) {
	    case SFILE_CONFLICT:
		fprintf(stderr, "%s exists locally already\n", to);
		return (0);
	    case GONE_SFILE_CONFLICT:
		fprintf(stderr,
		    "%s exists locally already but is marked gone\n", buf);
		return (0);
	    case GFILE_CONFLICT:
		tmp = sccs2name(to);
		fprintf(stderr, "%s exists locally already\n", tmp);
		free(tmp);
		return (0);
	    case RESYNC_CONFLICT:
		fprintf(stderr, "%s exists in RESYNC already\n", to);
		return (0);
	}

	unless (ok_local((sccs*)rs->opaque, 1)) {
		free(to);
		return (0);
	}

	/*
	 * OK, there is no conflict so we can actually move this file to
	 * the resync directory.  What we do is copy it into the RENAMES
	 * dir and then call move_remote() to do the work.
	 * We call saveKey here because we are adding it to the tree.
	 */
	do {
		sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", ++filenum);
	} while (exists(path));
	sprintf(cmd, "%s/%s", RESYNC2ROOT, rs->dname);
	fileCopy(cmd, path);
	s = sccs_init(path, INIT_NOCKSUM);
	sccs_sdelta(s, sccs_ino(s), path);
	saveKey(rs->opts, path, to);
	rs2 = resolve_init(rs->opts, s);
	if (move_remote(rs2, to)) {
		perror(to);
		exit(1);
	}
	resolve_free(rs2);
	free(to);
	return (EAGAIN);
}

private int
sc_rml(resolve *rs)
{
	char	repo[MAXPATH];
	char	*resync;
	sccs	*s = (sccs *)rs->opaque;
	project	*saveproj;

	unless (rs->opts->force || confirm("Remove local file?")) return (0);
	unless (ok_local((sccs*)rs->opaque, 0)) {
		return (0);
	}
	/*
	 * OK, there is no conflict so we can actually move this file to
	 * the resync directory.
	 * We emulate much of the work of bk rm here because we can't call
	 * it directly in the local repository.
	 * Need to do the save and restore because of the oddness of having
	 * the sfile in the main repo, but the desired path in the RESYNC,
	 * and the path returned is absolute based on the proj struct.
	 */
	saveproj = s->proj;
	s->proj = rs->s->proj;
	resync = sccs_rmName(s);
	s->proj = saveproj;

	sprintf(repo, "%s/%s", RESYNC2ROOT, s->sfile);
	if (fileCopy(repo, resync)) {
		perror(repo);
		exit(1);
	}

	/*
	 * Force a delta to lock it down to this name.
	 */
	if (sys("bk", "edit", "-q", resync, SYS)) {
		perror(resync);
		exit(1);
	}
	sprintf(repo, "-PyDelete: %s", ((sccs*)rs->opaque)->gfile);
	if (sys("bk", "delta", "-f", repo, resync, SYS)) {
		perror(resync);
		exit(1);
	}
	sccs_sdelta((sccs*)rs->opaque, sccs_ino((sccs*)rs->opaque), repo);
	saveKey(rs->opts, repo, resync);
	free(resync);
	return (EAGAIN);
}

private int
sc_rmr(resolve *rs)
{
	char	*resync;

	unless (rs->opts->force || confirm("Remove remote file?")) return (0);
	resync = sccs_rmName(rs->s);
	if (move_remote(rs, resync)) {
		perror("move_remote");
		exit(1);
	}
	free(resync);
	return (1);	/* XXX - EAGAIN? */
}

private int
rc_explain(resolve *rs)
{
	fprintf(stderr,
"Two different files want to occupy the following location:\n\
\t``%s''\n\
For the purposes of this resolve, we call the files left and right,\n\
the left file is the one which wants to be in the specified location,\n\
and the right file is the one which is already there.\n\
\nYour choices are:\n\
a) remove the left file, which will leave you with the remote file.\n\
b) move the left file to some other pathname.\n\n\
Both of these choices leave the right file where it is.\n", rs->dname);
	return (0);
}

private int
rc_help(resolve *rs)
{
	int	i;

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
Two files want to be: ``%s''\n\
---------------------------------------------------------------------------\n",
	    PATHNAME(rs->s, rs->d));
	fprintf(stderr, "Commands are:\n\n");
	for (i = 0; rs->funcs[i].spec; i++) {
		fprintf(stderr, "  %-4s - %s\n", 
		    rs->funcs[i].spec, rs->funcs[i].help);
	}
	fprintf(stderr, "\n");
	return (0);
}

private int
rc_ml(resolve *rs)
{
	char	buf[MAXPATH];
	char	key[MAXKEY];
	char	*to, *tmp;

	unless (prompt("Move left file to:", buf)) return (0);
	if (bk_badFilename(0, buf)) {
		fprintf(stderr, "Illegal filename: %s\n", buf);
		return (0);
	}
	if ((buf[0] == '/') || strneq("../", buf, 3)) {
		fprintf(stderr, "Destination must be in repository.\n");
		return (0);
	}
	if (rs->opts->debug) fprintf(stderr, "%s\n", buf);
	if (sccs_filetype(buf) != 's') {
		to = name2sccs(buf);
	} else {
		to = strdup(buf);
	}
	switch (slotTaken(rs, to, 0)) {
	    case SFILE_CONFLICT:
		fprintf(stderr, "%s exists locally already\n", to);
		return (0);
	    case GONE_SFILE_CONFLICT:
		fprintf(stderr,
		    "%s exists locally already but is marked gone\n", buf);
		return (0);
	    case GFILE_CONFLICT:
		tmp = sccs2name(to);
		fprintf(stderr, "%s exists locally already\n", tmp);
		free(tmp);
		return (0);
	    case RESYNC_CONFLICT:
		fprintf(stderr, "%s exists in RESYNC already\n", to);
		return (0);
	}
	sccs_sdelta(rs->s, sccs_ino(rs->s), key);
	mdbm_store_str(rs->opts->rootDB, key, to, MDBM_REPLACE);
	if (move_remote(rs, to)) {
		perror("move_remote");
		exit(1);
	}
	if (rs->opts->resolveNames) rs->opts->renames2++;
	if (rs->opts->log) {
		fprintf(rs->opts->log, "rename(%s, %s)\n", rs->s->sfile, to);
	}
	free(to);
	return (1);
}

private int
rc_rml(resolve *rs)
{
	char	*resync;

	unless (rs->opts->force || confirm("Remove left file?")) return (0);
	resync = sccs_rmName(rs->s);
	if (move_remote(rs, resync)) {
		perror("move_remote");
		exit(1);
	}
	free(resync);
	return (1);
}

rfuncs	gc_funcs[] = {
    { "?", "help", "print this help", gc_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "d", "diff", "diff the local file against the remote file", res_diff },
    { "D", "difftool",
	"graphical diff of the local file against the remote file",
	res_difftool },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    { "ml", "move local", "move the local file to someplace else", gc_ml },
    { "mr", "move remote", "move the remote file to someplace else", res_mr },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "rl", "remove local", "remove the local file", gc_remove },
    { "rr", "remove remote", "remove the remote file", sc_rmr },
    { "sd", "sdiff",
      "side by side diff of the local file vs. the remote file", res_sdiff },
    { "vl", "view local", "view the local file", res_vl },
    { "vr", "view remote", "view the remote file", res_vr },
    { "x", "explain", "explain the choices", gc_explain },
    { 0, 0, 0, 0 }
};

rfuncs	dc_funcs[] = {
    { "?", "help", "print this help", dc_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "ml", "move local", "move the local directory to someplace else", dc_ml },
    { "mr", "move remote", "move the remote file to someplace else", res_mr },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "rl", "remove local", "remove the local directory", dc_remove },
    { "rr", "remove remote", "remove the remote file", sc_rmr },
    { "vr", "view remote", "view the remote file", res_vr },
    { "x", "explain", "explain the choices", dc_explain },
    { 0, 0, 0, 0 }
};

rfuncs	sc_funcs[] = {
    { "?", "help", "print this help", sc_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "d", "diff", "diff the local file against the remote file", res_diff },
    { "D", "difftool",
	"graphical diff of the local file against the remote file",
	res_difftool },
    { "hl", "hist local", "revision history of the local file", res_hl },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    /* XXX - should have a move local to RENAMES and come back to it */
    { "ml", "move local", "move the local file to someplace else", sc_ml },
    { "mr", "move remote", "move the remote file to someplace else", res_mr },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "pl", "graphically view local", "view the local file", res_pl },
    { "pr", "graphically view remote", "view the remote file", res_pr },
    { "rl", "remove local", "remove the local copy of the file", sc_rml },
    { "rr", "remove remote", "remove the remote copy of the file", sc_rmr },
    { "sd", "sdiff",
      "side by side diff of the local file vs. the remote file", res_sdiff },
    { "vl", "view local", "view the local file", res_vl },
    { "vr", "view remote", "view the remote file", res_vr },
    { "x", "explain", "explain the choices", sc_explain },
    { 0, 0, 0, 0 }
};

/*
 * rs->s is the file that wants to move,
 * rs->opaque is the file that is in the slot.
 */
rfuncs	rc_funcs[] = {
    { "?", "help", "print this help", rc_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "d", "diff", "diff the two files", res_diff },
    { "D", "difftool", "graphical diff the of two files", res_difftool },
    { "hl", "hist left", "revision history of the left file", res_hl },
    { "hr", "hist right", "revision history of the right file", res_hr },
    { "ml", "move left", "move the left file to someplace else", rc_ml },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "rl", "remove left", "remove the left copy of the file", rc_rml },
    { "x", "explain", "explain the choices", rc_explain },
    { 0, 0, 0, 0 }
};

/*
 * Given an SCCS file, resolve the create.
 */
int
resolve_create(resolve *rs, int type)
{
	int	ret;

        if (rs->opts->debug) {
		fprintf(stderr, "resolve_create: ");
		resolve_dump(rs);
	}
	if (rs->opts->debug) fprintf(stderr, "TYPE=");
	switch (type) {
	    case GFILE_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "GFILE\n");
		if (ret = gc_sameFiles(rs)) return (ret);
		rs->prompt = PATHNAME(rs->s, rs->d);
		rs->res_gcreate = 1;
		return (resolve_loop("create/gfile conflict", rs, gc_funcs));
	    case DIR_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "DIR\n");
		rs->prompt = PATHNAME(rs->s, rs->d);
		rs->res_dirfile = 1;
		return (resolve_loop("create/dir conflict", rs, dc_funcs));
	    case COMP_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "COMP\n");
		rs->prompt = PATHNAME(rs->s, rs->d);
		rs->res_dirfile = 1;
		return (resolve_loop("create/comp conflict", rs, dc_funcs));
	    case SFILE_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "SFILE\n");
		rs->prompt = PATHNAME(rs->s, rs->d);
		rs->res_screate = 1;
		chdir(RESYNC2ROOT);
		rs->opaque = (void*)sccs_init(rs->dname, 0);
		chdir(ROOT2RESYNC);
		ret = resolve_loop("create/sfile conflict", rs, sc_funcs);
		if (rs->opaque) sccs_free((sccs*)rs->opaque);
		return (ret);
	    case GONE_SFILE_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "GONE SFILE\n");
		rs->prompt = PATHNAME(rs->s, rs->d);
		rs->res_screate = 1;
		chdir(RESYNC2ROOT);
		rs->opaque = (void*)sccs_init(rs->dname, 0);
		chdir(ROOT2RESYNC);
		ret = resolve_loop("create/sfile marked gone conflict",
		    rs, sc_funcs);
		if (rs->opaque) sccs_free((sccs*)rs->opaque);
		return (ret);
	    case RESYNC_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "RESYNC\n");
		rs->prompt = PATHNAME(rs->s, rs->d);
		rs->res_resync = 1;
		rs->opaque = (void*)sccs_init(rs->dname, 0);
		ret = resolve_loop("create/resync conflict", rs, rc_funcs);
		if (rs->opaque) sccs_free((sccs*)rs->opaque);
		return (ret);
	}
	return (-1);
}
