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
 * The local may be just a gfile, if it is, use it,
 * otherwise get the local file into a temp file without keywords.
 */
int
res_diff(resolve *rs)
{
	char	cmd[MAXPATH*3];
	char	*left;

	unless (left = res_getlocal(rs->d->pathname)) {
		fprintf(stderr, "diff: can't find %s\n", rs->d->pathname);
		return (0);
	}
	sprintf(cmd, "bk get -ksp %s | bk diff %s - | %s",
	    left, RESYNC2ROOT, rs->d->pathname, rs->pager);
	system(cmd);
	unlink(left);
	free(left);
	return (0);
}

int	
res_difftool(resolve *rs)
{
	char	*av[10];

	unless (rs->tnames) {
		fprintf(stderr, "No temp names setup, can not run difftool\n");
		return (0);
	}
	av[0] = "bk";
	av[1] = "difftool";
	av[2] = rs->tnames->local;
	av[3] = rs->tnames->remote;
	av[4] = 0;
	spawnvp_ex(_P_NOWAIT, "bk", av);
	return (0);
}

int
res_mr(resolve *rs)
{
	char	buf[MAXPATH];
	char	*t;

	unless (prompt("Move remote file to:", buf)) return (0);
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
res_vl(resolve *rs)
{
	char	cmd[MAXPATH];
	char	*left;

	unless (left = res_getlocal(rs->d->pathname)) {
		fprintf(stderr, "diff: can't find %s\n", rs->d->pathname);
		return (0);
	}
	fprintf(stderr, "--- Viewing %s ---\n", rs->d->pathname);
	sprintf(cmd, "%s %s", rs->pager, left);
	system(cmd);
	unlink(left);
	free(left);
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
	if (IS_EDITED(s) && sccs_clean(s, SILENT)) {
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
    { "a", "abort", "abort the patch", res_abort },
    { "d", "diff", "diff the local file against the remote file", res_diff },
    { "e", "explain", "explain the choices", gc_explain },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    { "ml", "move local", "move the local file to someplace else", gc_ml },
    { "mr", "move remote", "move the remote file to someplace else", res_mr },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "r", "remove local", "remove the local file", gc_remove },
    { "vl", "view local", "view the local file", res_vl },
    { "vr", "view remote", "view the remote file", res_vr },
    { 0, 0, 0, 0 }
};

rfuncs	sc_funcs[] = {
    { "?", "help", "print this help", sc_help },
    { "a", "abort", "abort the patch", res_abort },
    { "d", "diff", "diff the local file against the remote file", res_diff },
    { "e", "explain", "explain the choices", sc_explain },
    { "hl", "hist local", "revision history of the local file", res_hl },
    { "hr", "hist remote", "revision history of the remote file", res_hr },
    /* XXX - should have a move local to RENAMES and come back to it */
    { "ml", "move local", "move the local file to someplace else", sc_ml },
    { "mr", "move remote", "move the remote file to someplace else", res_mr },
    { "q", "quit", "immediately exit resolve", res_quit },
    { "r", "remove local", "remove the local file", sc_remove },
    { "vl", "view local", "view the local file", res_vl },
    { "vr", "view remote", "view the remote file", res_vr },
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
		return (resolve_loop("resolve_create gc", rs, gc_funcs));
	    case SFILE_CONFLICT:
		rs->prompt = rs->dname;
		chdir(RESYNC2ROOT);
		rs->opaque = (void*)sccs_init(rs->dname, 0, 0);
		chdir(ROOT2RESYNC);
		ret = resolve_loop("resolve_create sc", rs, sc_funcs);
		if (rs->opaque) sccs_free((sccs*)rs->opaque);
		return (ret);
	    case RESYNC_CONFLICT:
	    	fprintf(stderr, "RESYNC sfile conflicts not done.\n");
		exit(1);
	}
	return (-1);
}
