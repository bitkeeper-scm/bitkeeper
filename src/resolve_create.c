/*
 * resolve_create.c - resolver for created files
 */
#include "resolve.h"

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
		if (sccs_get(rs->s, 0, 0, 0, 0, SILENT, right)) {
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
		if (sccs_get(s, 0, 0, 0, 0, SILENT, left) ||
		    sccs_get(rs->s, 0, 0, 0, 0, SILENT, right)) {
		    	fprintf(stderr, "get failed, can't diff.\n");
			unlink(left);
			return (0);
		}
		differ(rs, left, right);
		unlink(left);
		unlink(right);
	}
	fprintf(stderr, "Don't know how to diff the files.\n");
	return (0);
}

do_diff(resolve *rs, char *left, char *right)
{
	char	cmd[MAXPATH*3];

	sprintf(cmd, "bk diff %s %s | %s", left, right, rs->pager);
	system(cmd);
}

do_sdiff(resolve *rs, char *left, char *right)
{
	char	cmd[MAXPATH*3];
	FILE	*p = popen("tput cols", "r");
	int	cols;

	unless (p && fnext(cmd, p) && (cols = atoi(cmd))) cols = 80;
	if (p) pclose(p);

	sprintf(cmd, "bk sdiff -w %d %s %s | %s", cols, left, right, rs->pager);
	system(cmd);
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

more(resolve *rs, char *file)
{
	char	cmd[MAXPATH];

	sprintf(cmd, "%s %s", rs->pager, file);
	system(cmd);
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
		if (sccs_get(s, 0, 0, 0, 0, SILENT, left)) {
		    	fprintf(stderr, "get failed, can't view.\n");
			return (0);
		}
		more(rs, left);
		unlink(left);
	}
	fprintf(stderr, "Don't know how to view the file.\n");
	return (0);
}

int
res_vr(resolve *rs)
{
	char	cmd[MAXPATH];

	fprintf(stderr, "--- Viewing %s ---\n", rs->s->gfile);
	sprintf(cmd, "bk get -qp %s | %s", rs->s->gfile, rs->pager);
	system(cmd);
	return (0);
}

int
res_hl(resolve *rs)
{
	unless (rs->revs) {
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
		sccs_prs(rs->s, 0, 0, 0, stdout);
		return (0);
	}
	prs_common(rs, rs->s, rs->revs->local, rs->revs->remote);
	return (0);
}

/* XXX - this is so lame, it should mark the list and call sccs_prs */
prs_common(resolve *rs, sccs *s, char *a, char *b)
{
	char	cmd[2*MAXPATH];
	char	*list;

	list = sccs_impliedList(s, "resolve", a, b);
	if (strlen(list) > MAXPATH) {	/* too big */
		free(list);
		sccs_prs(s, 0, 0, 0, stdout);
		return;
	}
	sprintf(cmd, "bk prs -r%s %s | %s", list, rs->s->gfile, rs->pager);
	system(cmd);
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
c) move the local file to some other pathname.\n\
d) move the remote file to some other pathname.\n\
\n\
The choices b, c, or d will allow the file in the patch to be created\n\
and you to continue with the rest of the patch.\n\
\n\
Warning: choices b and c are not recorded because there is no SCCS file\n\
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

int
gc_ml(resolve *rs)
{
	char	buf[MAXPATH];
	char	*t;

	unless (prompt("Move local file to:", buf)) return (0);
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
gc_remove(resolve *rs)
{
	char	buf[MAXPATH];
	opts	*opts = rs->opts;

	unless (confirm("Remove local file?")) return (0);
	sprintf(buf, "%s/%s", RESYNC2ROOT, rs->d->pathname);
	unlink(buf);
	if (opts->log) fprintf(stdlog, "unlink(%s)\n", buf);
	return (EAGAIN);
}

int
sc_explain(resolve *rs)
{
	fprintf(stderr,
"There is a local working file:\n\
\t``%s''\n\
The patch you are importing has a file which wants to be in the same\n\
place as the local file.  Your choices are:\n\
a) do not move the local file, which means that the entire patch will\n\
   be aborted, discarding any other merges you may have done.  You can\n\
   then move or delete the local file and retry the patch.\n\
   This is a good choice if you have done no merge work yet.\n\
b) remove the local file after making sure it is not something that you\n\
   need.  There are commands you can run now to show you both files.\n\
c) move the local file to some other pathname.\n\
d) move the remote file to some other pathname.\n\
\n\
The choices b, c, or d will allow the file in the patch to be created\n\
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
	if (system(cmd)) {
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
sc_remove(resolve *rs)
{
	char	cmd[MAXPATH*2];
	char	path[MAXPATH];
	int	filenum = 0;

	unless (confirm("Remove local file?")) return (0);
	unless (ok_local((sccs*)rs->opaque, 0)) {
		return (0);
	}
	/*
	 * OK, there is no conflict so we can actually move this file to
	 * the resync directory.  What we do is copy it into the RENAMES
	 * dir and then call bk rm to do the work.
	 */
	do {
		sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", ++filenum);
	} while (exists(path));
	sprintf(cmd, "cp -p %s/%s BitKeeper/RENAMES/SCCS/s.%d",
	    RESYNC2ROOT, ((sccs*)rs->opaque)->sfile, filenum);
	if (rs->opts->debug) fprintf(stderr, "%s\n", cmd);
	if (system(cmd)) {
		perror(cmd);
		exit(1);
	}
	sprintf(cmd, "bk rm BitKeeper/RENAMES/SCCS/s.%d", filenum);
	if (rs->opts->debug) fprintf(stderr, "%s\n", cmd);
	if (system(cmd)) {
		perror(cmd);
		exit(1);
	}
	sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", filenum);
	sccs_sdelta((sccs*)rs->opaque, sccs_ino((sccs*)rs->opaque), cmd);
	saveKey(rs->opts, cmd, path);
	return (EAGAIN);
}

rfuncs	gc_funcs[] = {
    { "?", "help", "print this help", gc_help },
    { "a", "abort", "abort the patch, DISCARDING all merges", res_abort },
    { "d", "diff", "diff the local file against the remote file", res_diff },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    { "ml", "move local", "move the local file to someplace else", gc_ml },
    { "mr", "move remote", "move the remote file to someplace else", res_mr },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "r", "remove local", "remove the local file", gc_remove },
    { "sd", "sdiff",
      "side by side diff of the local file vs. the remote file", res_sdiff },
    { "vl", "view local", "view the local file", res_vl },
    { "vr", "view remote", "view the remote file", res_vr },
    { "x", "explain", "explain the choices", gc_explain },
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
    { "r", "remove local", "remove the local file", sc_remove },
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

	switch (type) {
	    case GFILE_CONFLICT:
		rs->prompt = rs->dname;
		rs->res_gcreate = 1;
		return (resolve_loop("resolve_create gc", rs, gc_funcs));
	    case SFILE_CONFLICT:
		rs->prompt = rs->dname;
		rs->res_screate = 1;
		chdir(RESYNC2ROOT);
		rs->opaque = (void*)sccs_init(rs->dname, 0, 0);
		chdir(ROOT2RESYNC);
		ret = resolve_loop("resolve_create sc", rs, sc_funcs);
		if (rs->opaque) sccs_free((sccs*)rs->opaque);
		return (ret);
	    case RESYNC_CONFLICT:
		rs->res_resync = 1;
	    	fprintf(stderr, "RESYNC sfile conflicts not done.\n");
		exit(1);
	}
	return (-1);
}
