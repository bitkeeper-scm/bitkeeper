/*
 * resolve_create.c - resolver for created files
 */
#include "resolve.h"

int	do_diff(resolve *rs, char *left, char *right, int w);
int	do_sdiff(resolve *rs, char *left, char *right, int w);
int	do_difftool(resolve *rs, char *left, char *right, int w);
int	prs_common(resolve *rs, sccs *s, char *a, char *b);
private void getFileConflict(char *gfile, char *path);

int
res_abort(resolve *rs)
{
	if (confirm("Abort patch?")) resolve_cleanup(rs->opts, CLEAN_RESYNC);
	return (0);
}

/*
 * Return the pathname of a _COPY_ of the gfile or checked out gfile.
 * The caller frees the path and unlinks the temp file.
 */
char	*
res_getlocal(char *gfile)
{
	char	buf[MAXPATH];
	char	tmp[MAXPATH*2];
	char	*sfile;

	sprintf(buf, "%s/%s", RESYNC2ROOT, gfile);
	sfile = name2sccs(buf);
	unless (exists(sfile) || exists(buf)) {
		free(sfile);
		return (0);
	}

	unless (exists(sfile)) {
		free(sfile);
		gettemp(buf, "local");
		sprintf(tmp, "%s/%s", RESYNC2ROOT, gfile);
		sys("cp", tmp, buf, SYS);
		unless (exists(buf)) return (0);
		return (strdup(buf));
	}
	free(sfile);

	sprintf(tmp, "%s/%s", RESYNC2ROOT, gfile);
	sysio(NULL, buf, NULL, "bk", "get", "-ksp", tmp, SYS);
	unless (exists(buf)) return (0);
	return (strdup(buf));
}

/*
 * Diff the local against the remote;
 */
int
res_diffCommon(resolve *rs, rfunc differ)
{
	if (rs->tnames) {
		differ(rs, rs->tnames->local, rs->tnames->remote, 0);
		return (0);
	}
	if (rs->res_gcreate) {
		char	right[MAXPATH];

		gettemp(right, "right");
		if (sccs_get(rs->s, 0, 0, 0, 0, SILENT|PRINT, right)) {
		    	fprintf(stderr, "get failed, can't diff.\n");
			return (0);
		}
		chdir(RESYNC2ROOT);
		differ(rs, rs->d->pathname, right, 1);
		chdir(ROOT2RESYNC);
		unlink(right);
		return (0);
	}
	if (rs->res_screate || rs->res_resync) {
		char	left[MAXPATH];
		char	right[MAXPATH];
		sccs	*s = (sccs*)rs->opaque;

		gettemp(left, "left");
		gettemp(right, "right");
		if (sccs_get(s, 0, 0, 0, 0, SILENT|PRINT, left) ||
		    sccs_get(rs->s, 0, 0, 0, 0, SILENT|PRINT, right)) {
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

	bktemp(tmp);
	sysio(0, tmp, 0, "bk", "diff", left, right, SYS);
	more(rs, tmp);
	unlink(tmp);
	return (0);
}

int
do_sdiff(resolve *rs, char *left, char *right, int wait)
{
	char	tmp[MAXPATH];
	char	cols[10];
	FILE	*p = popen("tput cols", "r");

	unless (p && fnext(cols, p) && (atoi(cols) > 0)) {
		strcpy(cols, "80");
	} else {
		chop(cols);
	}
	if (p) pclose(p);
	bktemp(tmp);
	sysio(0, tmp, 0, "bk", "sdiff", "-w", cols, left, right, SYS);
	more(rs, tmp);
	unlink(tmp);
	return (0);
}

int
do_difftool(resolve *rs, char *left, char *right, int wait)
{
	char	*av[10];

	av[0] = "bk";
	av[1] = "difftool";
	av[2] = left;
	av[3] = right;
	av[4] = 0;
	spawnvp_ex(wait ? _P_WAIT : _P_NOWAIT, "bk", av);
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
	char	*t;

	unless (prompt("Move file to:", buf)) return (0);
	if ((buf[0] == '/') || strneq("../", buf, 3)) {
		fprintf(stderr, "Destination must be in repository.\n");
		return (0);
	}
	if (sccs_filetype(buf) != 's') {
		t = name2sccs(buf);
		strcpy(buf, t);
		free(t);
	}
	switch (slotTaken(rs->opts, buf)) {
	    case SFILE_CONFLICT:
		fprintf(stderr, "%s exists locally already\n", buf);
		return (0);
	    case GFILE_CONFLICT:
		t = sccs2name(buf);
		fprintf(stderr, "%s exists locally already\n", t);
		free(t);
		return (0);
	    case RESYNC_CONFLICT:
		fprintf(stderr, "%s exists in RESYNC already\n", buf);
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
	cmd = aprintf("%s < %s", rs->pager, file);
	system(cmd);
	return (0);
}

int
res_vl(resolve *rs)
{
	char	left[MAXPATH];
	sccs	*s = (sccs*)rs->opaque;

	if (rs->tnames) {
		more(rs, rs->tnames->local);
		return (0);
	}
	if (rs->res_gcreate) {
		chdir(RESYNC2ROOT);
		more(rs, rs->d->pathname);
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
	}
	gettemp(left, "left");
	if (sccs_get(s, 0, 0, 0, 0, SILENT|PRINT, left)) {
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
	char	tmp[MAXPATH];
	char	*name = rs->s->gfile;

	if (rs->res_resync) name = rs->dname;
	bktemp(tmp);
	sysio(0, tmp, 0, "bk", "get", "-qkp", name, SYS);
	more(rs, tmp);
	unlink(tmp);
	return (0);
}

int
revtool(char *name)
{
	char	*av[10];

	av[0] = "bk";
	av[1] = "revtool";
	av[2] = name;
	av[3] = 0;
	spawnvp_ex(_P_NOWAIT, "bk", av);
	return (0);
}

int
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

int
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
	bktemp(tmp);
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

		bktemp(tmp);
		sysio(0, tmp, 0, "bk", "prs", s->gfile, SYS);
		more(rs, tmp);
		unlink(tmp);
		return (0);
	}
	unless (rs->revs) return (res_h(rs));
	prs_common(rs, rs->s, rs->revs->local, rs->revs->remote);
	return (0);
}

int
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
	bktemp(tmp);
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
	exit(1);
	return (-1);
}

int
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
back by hand, if that is what you want.\n\n", rs->d->pathname);
	return (0);
}

int
gc_help(resolve *rs)
{
	int	i;

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
Local file: ``%s''\n\
has a name conflict with a file with the same name in the patch.\n\
The local file is not under revision control.\n\
---------------------------------------------------------------------------\n",
	    rs->d->pathname);
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
	if ((buf[0] == '/') || strneq("../", buf, 3)) {
		fprintf(stderr, "Destination must be in repository.\n");
		return (1);
	}
	if (sccs_filetype(buf) != 's') {
		t = name2sccs(buf);
		strcpy(buf, t);
		free(t);
	}
	switch (slotTaken(rs->opts, buf)) {
	    case SFILE_CONFLICT:
		fprintf(stderr, "%s exists locally already\n", buf);
		return (1);
	    case DIR_CONFLICT:
		getFileConflict(buf, path);
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

int
gc_ml(resolve *rs)
{
	char	buf[MAXPATH];
	char	*t;

	if (common_ml(rs, "Move local file to:", buf)) return (0);
	chdir(RESYNC2ROOT);
	t = sccs2name(buf);
	if (rs->opts->debug) {
		fprintf(stderr, "rename(%s, %s)\n", rs->d->pathname, t);
	}
	if (rename(rs->d->pathname, t)) {
		perror("rename");
		exit(1);
	}
	free(t);
	chdir(ROOT2RESYNC);
	return (EAGAIN);
}

int
dc_ml(resolve *rs)
{
	char	buf[MAXPATH];
	char	path[MAXPATH];
	char	*t;

	if (common_ml(rs, "Move local directory to:", buf)) return (0);
	t = sccs2name(buf);
	getFileConflict(rs->d->pathname, path);
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

int
gc_remove(resolve *rs)
{
	char	buf[MAXPATH];
	opts	*opts = rs->opts;

	sprintf(buf, "%s/%s", RESYNC2ROOT, rs->d->pathname);
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

	gettemp(buf, "right");
	if (sccs_get(rs->s, 0, 0, 0, 0, SILENT|PRINT, buf)) {
		fprintf(stderr, "get failed, can't diff.\n");
		return (0);
	}
	chdir(RESYNC2ROOT);
	same = sameFiles(rs->d->pathname, buf);
	chdir(ROOT2RESYNC);
	unlink(buf);
	if (same) {
		if (opts->log) {
			fprintf(stdlog, "same files %s\n", rs->d->pathname);
		}
		sprintf(buf, "%s/%s", RESYNC2ROOT, rs->d->pathname);
		assert(!isdir(buf));
		if (unlink(buf)) {
			fprintf(stderr,
			    "Unable to remove local file %s\n",
			    rs->d->pathname);
			return (0);
		}
		if (opts->log) fprintf(stdlog, "unlink(%s)\n", buf);
		return (EAGAIN);
	}
	return (0);
}

private void
getFileConflict(char *gfile, char *path)
{
	char	*t, *s;
	
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

int
dc_remove(resolve *rs)
{
	char	buf[MAXPATH];
	char	path[MAXPATH];
	opts	*opts = rs->opts;
	int	ret;

	unless (rs->opts->force || confirm("Remove local directory?")) {
		return (0);
	}
	getFileConflict(rs->d->pathname, path);
	sprintf(buf, "%s/%s", RESYNC2ROOT, path);
	ret = rmdir(buf);
	if (opts->log) fprintf(stdlog, "rmdir(%s) = %d\n", buf, ret);
	return (EAGAIN);
}

int
dc_explain(resolve *rs)
{
	char	path[MAXPATH];

	getFileConflict(rs->d->pathname, path);
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
back by hand, if that is what you want.\n\n", rs->d->pathname, path);
	return (0);
}

int
dc_help(resolve *rs)
{
	int	i;
	sccs	*local;
	char	path[MAXPATH];
	char	buf[MAXKEY];

	getFileConflict(rs->d->pathname, path);
	sccs_sdelta(rs->s, sccs_ino(rs->s), buf);
	chdir(RESYNC2ROOT);
	local = sccs_keyinit(buf, INIT, rs->opts->local_proj, rs->opts->idDB);
	chdir(ROOT2RESYNC);
	fprintf(stderr,
"---------------------------------------------------------------------------\n\
Remote file:\n\t``%s''\n", rs->d->pathname);
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

int
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
and you to continue with the rest of the patch.\n\n", rs->d->pathname);
	return (0);
}

int
sc_help(resolve *rs)
{
	int	i;

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
Local file: ``%s''\n\
has a name conflict with a new file with the same name in the patch.\n\
---------------------------------------------------------------------------\n",
	    rs->d->pathname);
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
	delta	*d;

	unless (s) {
		fprintf(stderr, "Can't init the conflicting local file\n");
		exit(1);
	}
	if ((IS_EDITED(s) || IS_LOCKED(s)) && sccs_clean(s, SILENT)) {
		fprintf(stderr,
		    "Cannot [re]move modified local file %s\n", s->gfile);
		return (0);
	}
	unless (check_pending) return (1);
	d = sccs_getrev(s, "+", 0, 0);
	unless (d->flags & D_CSET) {
		fprintf(stderr,
		    "Cannot [re]move uncommitted local file %s\n", s->gfile);
		return (0);
	}
	return (1);
}

/*
 * Create the union of the two files in the older file and delete the
 * younger file.
 * If you are really thinking, you'll realize that remote updates to
 * the deleted file will never be put in the non-deleted file.  That's
 * true but not fatal, all it means is that some poor schmuck will be
 * asked multiple times about the logging.
 */
int
res_loggingok(resolve *rs)
{
	sccs	*here = (sccs*)rs->opaque;
	sccs	*resync = (sccs*)rs->s;
	char	left[MAXPATH];
	char	right[MAXPATH];
	char	cmd[MAXPATH*3];

	/*
	 * save the contents before we start moving stuff around.
	 */
	gettemp(left, "left");
	gettemp(right, "right");
	if (sccs_get(here, 0, 0, 0, 0, SILENT|PRINT, left) ||
	    sccs_get(resync, 0, 0, 0, 0, SILENT|PRINT, right)) {
		fprintf(stderr, "get failed, can't merge.\n");
		unlink(left);
		rs->opts->errors = 1;
		return (0);
	}

	/*
	 * Figure out which one we are going to move.
	 * If the RESYNC one is the one to move, that's easy.
	 * If the local one is the one to move, we need to remove
	 * it but it doesn't exist in the RESYNC tree so we dance a little.
	 */
	sccs_close(resync);
	sccs_close(here);
	if (sccs_ino(resync)->date > sccs_ino(here)->date) {	/* easy */
		if (rename(resync->sfile, LOGGING_OK)) {
			perror(LOGGING_OK);
			rs->opts->errors = 1;
			return (-1);
		}
		if (sccs_rm(LOGGING_OK, NULL, 1)) {
			rs->opts->errors = 1;
			return (-1);
		}
		if (sys("cp", RESYNC2ROOT "/" LOGGING_OK, LOGGING_OK, SYS)) {
			perror(cmd);
			rs->opts->errors = 1;
			return (-1);
		}
	} else {
		/*
		 * Copy the sfile to a temp dir and then remove it.
		 * Then move the file out of RENAMES into the right place.
		 */
		mkdirp("BitKeeper/tmp/SCCS");
		sprintf(cmd, "BitKeeper/tmp/SCCS/%s", basenm(LOGGING_OK));
		if (sys("cp", RESYNC2ROOT "/" LOGGING_OK, cmd, SYS)) {
			perror(cmd);
			rs->opts->errors = 1;
			return (-1);
		}
		sprintf(cmd, "BitKeeper/tmp/%s", basenm(GLOGGING_OK));
		if (sccs_rm(cmd, NULL, 1)) {
			rs->opts->errors = 1;
			return (-1);
		}
		mkdirp("BitKeeper/etc/SCCS");
		if (rename(resync->sfile, LOGGING_OK)) {
			perror(LOGGING_OK);
			rs->opts->errors = 1;
			return (-1);
		}
	}

	/*
	 * OK, cool, we have the right file in LOGGING_OK so edit it and
	 * sort -u the data files into it, and delta it.
	 * XXX - we're going to want this code for the conflict case too.
	 */
	sprintf(cmd,
	    "bk get -eg %s %s", rs->opts->quiet ? "-q" : "", GLOGGING_OK);
	if (oldsys(cmd, rs->opts)) return (-1);
	sprintf(cmd, "cat %s %s | sort -u > %s", left, right, GLOGGING_OK);
	if (oldsys(cmd, rs->opts)) {
		perror(cmd);
		rs->opts->errors = 1;
		return (-1);
	}
	unlink(left);
	unlink(right);
	sprintf(cmd, "bk delta -Py'Auto merged' %s %s",
	    rs->opts->quiet ? "-q" : "", GLOGGING_OK);
	if (oldsys(cmd, rs->opts)) {
		perror(cmd);
		rs->opts->errors = 1;
		return (-1);
	}
	return (0);
}

int
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
	switch (slotTaken(rs->opts, to)) {
	    case SFILE_CONFLICT:
		fprintf(stderr, "%s exists locally already\n", to);
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
	if (sys("cp", "-p", cmd, path, SYS)) {
		perror(cmd);
		exit(1);
	}
	s = sccs_init(path, INIT, rs->opts->resync_proj);
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

int
sc_rml(resolve *rs)
{
	char	repo[MAXPATH*2];
	char	resync[MAXPATH];
	int	filenum = 0;
	char	*nm = basenm(((sccs*)rs->opaque)->gfile);

	unless (rs->opts->force || confirm("Remove local file?")) return (0);
	unless (ok_local((sccs*)rs->opaque, 0)) {
		return (0);
	}
	/*
	 * OK, there is no conflict so we can actually move this file to
	 * the resync directory.
	 * We emulate much of the work of bk rm here because we can't call
	 * it directly in the local repository.
	 */
	sprintf(resync, "BitKeeper/deleted/SCCS/s..del-%s", nm);
	sprintf(repo, "%s/%s", RESYNC2ROOT, resync);
	while (exists(resync) || exists(repo)) {
		sprintf(resync,
		    "BitKeeper/deleted/SCCS/s..del-%s~%d", nm, ++filenum);
		sprintf(repo, "%s/%s", RESYNC2ROOT, resync);
	}
	sprintf(repo, "%s/%s", RESYNC2ROOT, ((sccs*)rs->opaque)->sfile);
	if (sys("cp", repo, resync, SYS)) {
		perror(repo);
		exit(1);
	}

	/*
	 * Force a delta to lock it down to this name.
	 */
	if (sys("bk", "edit", "-q", resync, SYS)) {
		perror(repo);
		exit(1);
	}
	sprintf(repo, "-PyDelete: %s", ((sccs*)rs->opaque)->gfile);
	if (sys("bk", "delta", repo, resync, SYS)) {
		perror(repo);
		exit(1);
	}
	sccs_sdelta((sccs*)rs->opaque, sccs_ino((sccs*)rs->opaque), repo);
	saveKey(rs->opts, repo, resync);
	return (EAGAIN);
}

int
sc_rmr(resolve *rs)
{
	char	repo[MAXPATH*2];
	char	resync[MAXPATH];
	int	filenum = 0;
	char	*nm = basenm(rs->d->pathname);

	unless (rs->opts->force || confirm("Remove remote file?")) return (0);
	sprintf(resync, "BitKeeper/deleted/SCCS/s..del-%s", nm);
	sprintf(repo, "%s/%s", RESYNC2ROOT, resync);
	while (exists(resync) || exists(repo)) {
		sprintf(resync,
		    "BitKeeper/deleted/SCCS/s..del-%s~%d", nm, ++filenum);
		sprintf(repo, "%s/%s", RESYNC2ROOT, resync);
	}
	sccs_close(rs->s);
	if (rename(rs->s->sfile, resync)) {
		perror("rename");
		fprintf(stderr, "rename(%s, %s)\n", rs->s->sfile, resync);
		exit(1);
	}

	/*
	 * Force a delta to lock it down to this name.
	 */
	if (sys("bk", "edit", "-q", resync, SYS)) {
		perror(repo);
		exit(1);
	}
	sprintf(repo, "-PyDelete: %s", ((sccs*)rs->opaque)->gfile);
	if (sys("bk", "delta", repo, resync, SYS)) {
		perror(repo);
		exit(1);
	}
	return (1);	/* XXX - EAGAIN? */
}

int
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

int
rc_help(resolve *rs)
{
	int	i;

	fprintf(stderr,
"---------------------------------------------------------------------------\n\
Two files want to be: ``%s''\n\
---------------------------------------------------------------------------\n",
	    rs->d->pathname);
	fprintf(stderr, "Commands are:\n\n");
	for (i = 0; rs->funcs[i].spec; i++) {
		fprintf(stderr, "  %-4s - %s\n", 
		    rs->funcs[i].spec, rs->funcs[i].help);
	}
	fprintf(stderr, "\n");
	return (0);
}

int
rc_ml(resolve *rs)
{
	char	buf[MAXPATH];
	char	key[MAXKEY];
	char	*to, *tmp;

	unless (prompt("Move left file to:", buf)) return (0);
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
	switch (slotTaken(rs->opts, to)) {
	    case SFILE_CONFLICT:
		fprintf(stderr, "%s exists locally already\n", to);
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
	sccs_close(rs->s);
	sys("bk", "mv", rs->s->sfile, to, SYS);
	unlink(sccs_Xfile(rs->s, 'm'));
	if (rs->opts->resolveNames) rs->opts->renames2++;
	if (rs->opts->log) {
		fprintf(rs->opts->log, "rename(%s, %s)\n", rs->s->sfile, to);
	}
	free(to);
	return (1);
}

int
rc_rml(resolve *rs)
{
	unless (rs->opts->force || confirm("Remove local file?")) return (0);
	sys("bk", "rm", rs->s->sfile, SYS);
	unlink(sccs_Xfile(rs->s, 'm'));
	if (rs->opts->resolveNames) rs->opts->renames2++;
	if (rs->opts->log) {
		fprintf(rs->opts->log, "remove(%s)\n", rs->s->sfile);
	}
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
		rs->prompt = rs->d->pathname;
		rs->res_gcreate = 1;
		return (resolve_loop("create/gfile conflict", rs, gc_funcs));
	    case DIR_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "DIR\n");
		rs->prompt = rs->d->pathname;
		rs->res_dirfile = 1;
		return (resolve_loop("create/dir conflict", rs, dc_funcs));
	    case SFILE_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "SFILE\n");
		rs->prompt = rs->d->pathname;
		rs->res_screate = 1;
		chdir(RESYNC2ROOT);
		rs->opaque = (void*)sccs_init(rs->dname, 0, 0);
		chdir(ROOT2RESYNC);
		ret = resolve_loop("create/sfile conflict", rs, sc_funcs);
		if (rs->opaque) sccs_free((sccs*)rs->opaque);
		return (ret);
	    case LOGGING_OK_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "LOGGING\n");
		rs->prompt = rs->d->pathname;
		rs->res_screate = 1;
		chdir(RESYNC2ROOT);
		rs->opaque = (void*)sccs_init(rs->dname, 0, 0);
		chdir(ROOT2RESYNC);
		ret = res_loggingok(rs);
		if (rs->opaque) sccs_free((sccs*)rs->opaque);
		return (ret);
	    case RESYNC_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "RESYNC\n");
		rs->prompt = rs->d->pathname;
		rs->res_resync = 1;
		rs->opaque = (void*)sccs_init(rs->dname, 0, 0);
		ret = resolve_loop("create/resync conflict", rs, rc_funcs);
		if (rs->opaque) sccs_free((sccs*)rs->opaque);
		return (ret);
	}
	return (-1);
}
