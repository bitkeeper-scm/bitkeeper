/*
 * resolve_create.c - resolver for created files
 */
#include "resolve.h"

int	do_diff(resolve *rs, char *left, char *right);
int	do_sdiff(resolve *rs, char *left, char *right);
int	do_difftool(resolve *rs, char *left, char *right);
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
	char	cmd[MAXPATH*2];
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
		sprintf(cmd, "cp %s/%s %s", RESYNC2ROOT, gfile, buf);
		system(cmd);
		unless (exists(buf)) return (0);
		return (strdup(buf));
	}
	free(sfile);

	sprintf(cmd, "bk get -ksp %s/%s > %s", RESYNC2ROOT, gfile, buf);
	system(cmd);
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
		differ(rs, rs->tnames->local, rs->tnames->remote);
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
		differ(rs->dname, right);
		chdir(ROOT2RESYNC);
		unlink(right);
		return (0);
	}
	if (rs->res_screate) {
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
		differ(rs, left, right);
		unlink(left);
		unlink(right);
		return (0);
	}
	fprintf(stderr, "Don't know how to diff the files.\n");
	return (0);
}

int
do_diff(resolve *rs, char *left, char *right)
{
	char	cmd[MAXPATH*3];

	sprintf(cmd, "bk diff %s %s | %s", left, right, rs->pager);
	sys(cmd, rs->opts);
	return (0);
}

int
do_sdiff(resolve *rs, char *left, char *right)
{
	char	cmd[MAXPATH*3];
	FILE	*p = popen("tput cols", "r");
	int	cols;

	unless (p && fnext(cmd, p) && (cols = atoi(cmd))) cols = 80;
	if (p) pclose(p);

	sprintf(cmd, "bk sdiff -w %d %s %s | %s", cols, left, right, rs->pager);
	sys(cmd, rs->opts);
	return (0);
}

int
do_difftool(resolve *rs, char *left, char *right)
{
	char	*av[10];

	av[0] = "bk";
	av[1] = "difftool";
	av[2] = left;
	av[3] = right;
	av[4] = 0;
	spawnvp_ex(_P_NOWAIT, "bk", av);
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
	char	cmd[MAXPATH];

	sprintf(cmd, "%s %s", rs->pager, file);
	sys(cmd, rs->opts);
	return (0);
}

int
res_vl(resolve *rs)
{
	if (rs->tnames) {
		more(rs, rs->tnames->local);
		return (0);
	}
	if (rs->res_gcreate) {
		chdir(RESYNC2ROOT);
		more(rs, rs->dname);
		chdir(ROOT2RESYNC);
		return (0);
	}
	if (rs->res_screate) {
		char	left[MAXPATH];
		sccs	*s = (sccs*)rs->opaque;

		gettemp(left, "left");
		if (sccs_get(s, 0, 0, 0, 0, SILENT|PRINT, left)) {
		    	fprintf(stderr, "get failed, can't view.\n");
			return (0);
		}
		more(rs, left);
		unlink(left);
		return (0);
	}
	fprintf(stderr, "Don't know how to view the file.\n");
	return (0);
}

int
res_vr(resolve *rs)
{
	char	cmd[MAXPATH];

	sprintf(cmd, "bk get -qp %s | %s", rs->s->gfile, rs->pager);
	sys(cmd, rs->opts);
	return (0);
}

int
sccstool(char *name)
{
	char	*av[10];

	av[0] = "bk";
	av[1] = "sccstool";
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
		sccstool(s->gfile);
		chdir(ROOT2RESYNC);
		return (0);
	}
	fprintf(stderr, "Don't know how to view the file.\n");
	return (0);
}

int
res_pr(resolve *rs)
{
	sccstool(rs->s->gfile);
	return (0);
}

private	void
setall(sccs *s)
{
	delta	*d;

	for (d = s->table; d; d = d->next) d->flags |= D_SET;
	if (streq(s->tree->rev, "1.0")) s->tree->flags &= ~D_SET;
}

int
res_hl(resolve *rs)
{
	if (rs->res_gcreate) {
		fprintf(stderr, "No history file available\n");
		return (0);
	}
	if (rs->res_screate) {
		sccs	*s = (sccs*)rs->opaque;

		setall(s);
		sccs_prs(s, 0, 0, 0, stdout);
		return (0);
	}
	unless (rs->revs) {
		setall(rs->s);
		sccs_prs(rs->s, 0, 0, 0, stdout);
		return (0);
	}
	prs_common(rs, rs->s, rs->revs->remote, rs->revs->local);
	return (0);
}

int
res_hr(resolve *rs)
{
	unless (rs->revs) {
		setall(rs->s);
		sccs_prs(rs->s, 0, 0, 0, stdout);
		return (0);
	}
	prs_common(rs, rs->s, rs->revs->local, rs->revs->remote);
	return (0);
}

/* XXX - this is so lame, it should mark the list and call sccs_prs */
int
prs_common(resolve *rs, sccs *s, char *a, char *b)
{
	char	cmd[2*MAXPATH];
	char	*list;

	list = sccs_impliedList(s, "resolve", a, b);
	if (rs->opts->debug) {
		fprintf(stderr, "prs(%s, %s, %s) = %s\n", s->gfile, a, b, list);
	}
	if (strlen(list) > MAXPATH) {	/* too big */
		free(list);
		setall(s);
		sccs_prs(s, 0, 0, 0, stdout);
		return (0);
	}
	sprintf(cmd, "bk prs -r%s %s | %s", list, rs->s->gfile, rs->pager);
	free(list);
	sys(cmd, rs->opts);
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
has a name conflict with a new file with the same name in the patch.\n\
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
common_ml(resolve *rs, char *buf)
{
	char	path[MAXPATH];
	char	*t;

	unless (prompt("Move local file to:", buf)) return (1);
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

	if (common_ml(rs, buf)) return (0);
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

	if (common_ml(rs, buf)) return (0);
	getFileConflict(rs->d->pathname, path);
	chdir(RESYNC2ROOT);
	if (rs->opts->debug) {
		fprintf(stderr, "rename(%s, %s)\n", path, buf);
	}
	if (rename(path, buf)) {
		perror("rename");
		exit(1);
	}
	chdir(ROOT2RESYNC);
	return (EAGAIN);
}

int
gc_remove(resolve *rs)
{
	char	buf[MAXPATH];
	opts	*opts = rs->opts;

	unless (rs->opts->force || confirm("Remove local file?")) return (0);
	sprintf(buf, "%s/%s", RESYNC2ROOT, rs->d->pathname);
	unlink(buf);
	if (opts->log) fprintf(stdlog, "unlink(%s)\n", buf);
	return (EAGAIN);
}

private void
getFileConflict(char *gfile, char *path)
{
	char	*t, *s;
	
	chdir(RESYNC2ROOT);
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

	unless (rs->opts->force || confirm("Remove local file?")) return (0);
	getFileConflict(rs->d->pathname, path);
	sprintf(buf, "%s/%s", RESYNC2ROOT, path);
	unlink(buf);
	if (opts->log) fprintf(stdlog, "unlink(%s)\n", buf);
	return (EAGAIN);
}

int
dc_explain(resolve *rs)
{
	char	path[MAXPATH];

	getFileConflict(rs->d->pathname, path);
	fprintf(stderr,
"The path of the remote file: ``%s''\n\
conflicts with a local file: ``%s''\n\
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
"wants to be in same place as local file\n\t``%s''\n\
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
		    "Can not [re]move modified local file %s\n", s->gfile);
		return (0);
	}
	unless (check_pending) return (1);
	d = sccs_getrev(s, "+", 0, 0);
	unless (d->flags & D_CSET) {
		fprintf(stderr,
		    "Can not [re]move uncommitted local file %s\n", s->gfile);
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
	if (resync->tree->date > here->tree->date) {	/* easy */
		if (rename(resync->sfile, LOGGING_OK)) {
			perror(LOGGING_OK);
			rs->opts->errors = 1;
			return (-1);
		}
		if (sccs_rm(LOGGING_OK, 1)) {
			rs->opts->errors = 1;
			return (-1);
		}
		sprintf(cmd,
		    "cp %s/%s %s", RESYNC2ROOT, LOGGING_OK, LOGGING_OK);
		if (sys(cmd, rs->opts)) {
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
		sprintf(cmd, "cp %s/%s BitKeeper/tmp/SCCS/%s",
		    RESYNC2ROOT, LOGGING_OK, basenm(LOGGING_OK));
		if (sys(cmd, rs->opts)) {
			perror(cmd);
			rs->opts->errors = 1;
			return (-1);
		}
		sprintf(cmd, "BitKeeper/tmp/%s", basenm(GLOGGING_OK));
		if (sccs_rm(cmd, 1)) {
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
	if (sys(cmd, rs->opts)) return (-1);
	sprintf(cmd, "cat %s %s | sort -u > %s", left, right, GLOGGING_OK);
	if (sys(cmd, rs->opts)) {
		perror(cmd);
		rs->opts->errors = 1;
		return (-1);
	}
	unlink(left);
	unlink(right);
	sprintf(cmd, "bk delta -y'Auto merged' %s %s",
	    rs->opts->quiet ? "-q" : "", GLOGGING_OK);
	if (sys(cmd, rs->opts)) {
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
	sprintf(cmd, "cp -p %s/%s BitKeeper/RENAMES/SCCS/s.%d",
	    RESYNC2ROOT, rs->dname, filenum);
	if (rs->opts->debug) fprintf(stderr, "%s\n", cmd);
	if (sys(cmd, rs->opts)) {
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
	char	*quiet = rs->opts->quiet ? "-q" : "";

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
	sprintf(repo,
	    "cp %s/%s %s", RESYNC2ROOT, ((sccs*)rs->opaque)->sfile, resync);
	if (sys(repo, rs->opts)) {
		perror(repo);
		exit(1);
	}

	/*
	 * Force a delta to lock it down to this name.
	 */
	sprintf(repo, "bk edit %s %s", quiet, resync);
	if (sys(repo, rs->opts)) {
		perror(repo);
		exit(1);
	}
	sprintf(repo, "bk delta %s -y'Delete: %s' %s",
	    quiet, ((sccs*)rs->opaque)->gfile, resync);
	if (sys(repo, rs->opts)) {
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
	char	cmd[MAXPATH*2];

	unless (rs->opts->force || confirm("Remove remote file?")) return (0);
	sccs_close(rs->s);
	sprintf(cmd, "bk rm %s", rs->s->sfile);
	if (sys(cmd, rs->opts)) {
		perror(cmd);
		exit(1);
	}
	return (1);
}

rfuncs	gc_funcs[] = {
    { "?", "help", "print this help", gc_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "d", "diff", "diff the local file against the remote file", res_diff },
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
    { "ml", "move local", "move the local file to someplace else", dc_ml },
    { "mr", "move remote", "move the remote file to someplace else", res_mr },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "rl", "remove local", "remove the local file", dc_remove },
    { "rr", "remove remote", "remove the remote file", sc_rmr },
    { "vr", "view remote", "view the remote file", res_vr },
    { "x", "explain", "explain the choices", dc_explain },
    { 0, 0, 0, 0 }
};

rfuncs	sc_funcs[] = {
    { "?", "help", "print this help", sc_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "d", "diff", "diff the local file against the remote file", res_diff },
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
 * Given an SCCS file, resolve the create.
 *	sfile conflicts can be handled by adding the sfile to the patch.
 *	gfile conflicts are not handled yet.
 */
int
resolve_create(resolve *rs, int type)
{
	int	ret;

	if (rs->opts->debug) fprintf(stderr, "TYPE=");
	switch (type) {
	    case GFILE_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "GFILE\n");
		rs->prompt = rs->d->pathname;
		rs->res_gcreate = 1;
		return (resolve_loop("resolve_create gc", rs, gc_funcs));
	    case DIR_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "DIR\n");
		rs->prompt = rs->d->pathname;
		rs->res_dirfile = 1;
		return (resolve_loop("resolve_create dc", rs, dc_funcs));
	    case SFILE_CONFLICT:
		if (rs->opts->debug) fprintf(stderr, "SFILE\n");
		rs->prompt = rs->d->pathname;
		rs->res_screate = 1;
		chdir(RESYNC2ROOT);
		rs->opaque = (void*)sccs_init(rs->dname, 0, 0);
		chdir(ROOT2RESYNC);
		ret = resolve_loop("resolve_create sc", rs, sc_funcs);
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
		rs->res_resync = 1;
	    	fprintf(stderr, "RESYNC sfile conflicts not done.\n");
		exit(1);
	}
	return (-1);
}
