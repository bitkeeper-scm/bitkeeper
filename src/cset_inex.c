/* Copyright (c) 2000 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "zlib/zlib.h"

WHATSTR("@(#)%K%");
private int	undoit(MDBM *m);
private int	doit(int flags, char *file, char *op, char *revs);
private int	commit(int quiet, delta *d);
private	delta	*getComments(char *op, char *revs);
private void	clean(char *file);
private void	unedit(void);

/*
 * cset_inex.c - changeset include/exclude processing.
 *
 * Something like this:
 *	bk cset -r<revs> | while read file rev
 *	do	bk get -e -i<rev> <file>
 *		bk delta <file>
 *	done
 *	bk commit
 *
 * Win32 note: kill(pid, SIGKILL) & wait(0) are no-op on win32.
 */
int
cset_inex(int flags, char *op, char *revs)
{
	int	i, fd, pid;
	FILE	*f;
	char	*av[20];
	MDBM	*m = mdbm_mem();
	delta	*d;
	char	*t;
	char	*revarg;
	char	buf[MAXKEY];
	char	file[MAXPATH];
	char	*revbuf, **rlist = 0;

	av[i = 0] = "bk";
	av[++i] = "cset";
	revarg = aprintf("-r%s", revs);
	av[++i] = revarg;
	av[++i] = 0;
	pid = spawnvp_rPipe(av, &fd, 0);
	if (pid == -1) {
		perror("spawnvp_rPipe");
		free(revarg);
		return (1);
	}
	unless (f = fdopen(fd, "r")) {
		perror("fdopen");
		kill(pid, SIGKILL);
		free(revarg);
		return (1);
	}

	d = getComments(op, revarg);
	free(revarg);

	file[0] = 0;
	while (fgets(buf, sizeof(buf), f)) {
#ifdef OLD_LICENSE
		if (checkLog(0, 1)) {
			fprintf(stderr, "Cset aborted, no changes applied\n");
			kill(pid, SIGKILL);
			wait(0);
			mdbm_close(m);
			return (1);
		}
#endif
		chop(buf);
		t = strchr(buf, BK_FS);
		assert(t);
		*t = 0;
		if (file[0]) {
			if (streq(file, buf)) {	/* add this rev */
				rlist = addLine(rlist, strdup(&t[1]));
			} else {		/* flush file, start again */
				revbuf = joinLines(",", rlist);
				freeLines(rlist, free);
				rlist = 0;
				if (doit(flags, file, op, revbuf)) {
					kill(pid, SIGKILL);
					wait(0);
					clean(file);
					/*
					 * Nota bene: this "knows" that the
					 * ChangeSet file is never edited
					 * in a normal tree.
					 */
					unedit();
					free(revbuf);
					return (undoit(m));
				}
				free(revbuf);
				mdbm_store_str(m, file, "", 0);
				strcpy(file, buf);
				rlist = addLine(rlist, strdup(&t[1]));
			}
		} else {
			strcpy(file, buf);
			rlist = addLine(rlist, strdup(&t[1]));
		}
	}
	if (file[0]) {
		revbuf = joinLines(",", rlist);
		freeLines(rlist, free);
		rlist = 0;
		if (doit(flags, file, op, revbuf)) {
			kill(pid, SIGKILL);
			wait(0);
			clean(file);
			unedit();
			free(revbuf);
			return (undoit(m));
		}
		mdbm_store_str(m, file, "", 0);
		free(revbuf);
	}
	wait(0);
	mdbm_close(m);
	return (commit(flags & SILENT, d));
}

private void
clean(char *file)
{
	int	i;
	char	*av[20];

	av[i = 0] = "bk";
	av[++i] = "clean";
	av[++i] = "-q";
	av[++i] = file;
	av[++i] = 0;
	spawnvp_ex(_P_WAIT, "bk", av);
}

private void
unedit()
{
	int	i;
	char	*av[20];

	av[i = 0] = "bk";
	av[++i] = "unlock";
	av[++i] = CHANGESET;
	av[++i] = 0;
	spawnvp_ex(_P_WAIT, "bk", av);
}

private	delta *
getComments(char *op, char *revs)
{
	int	i, pid;
	FILE	*f;
	char	*av[20];
	delta	*d;
	char	buf[MAXKEY];

	assert(strneq("-r", revs, 2));

	/*
	 * First get the Cset key.
	 */
	av[i = 0] = "bk";
	av[++i] = "prs";
	av[++i] = "-h";
	av[++i] = "-d:KEY:\n";
	av[++i] = revs;
	av[++i] = CHANGESET;
	av[++i] = 0;
	pid = spawnvp_rPipe(av, &i, 0);
	if (pid == -1) {
		perror("spawnvp_rPipe");
		return (0);
	}
	unless (f = fdopen(i, "r")) {
		perror("fdopen");
		kill(pid, SIGKILL);
		return (0);
	}
	d = calloc(1, sizeof(delta));
	if (comments_got()) d = comments_get(d);
	if (streq(op, "-i")) {
		strcpy(buf, "Cset include: ");
	} else {
		strcpy(buf, "Cset exclude: ");
	}
	while (fgets(&buf[14], sizeof(buf) - 14, f)) {
		chop(buf);
		d->comments = addLine(d->comments, strdup(buf));
	}
	fclose(f);
	wait(0);
	return (d);
}

private int
commit(int quiet, delta *d)
{
	int	i;
	char	*comment = 0;
	char	*cmds[20];
	char	*tmp = bktmp(0, "commit");
	FILE	*f = fopen(tmp, "w");

	EACH(d->comments) {
		fputs(d->comments[i], f);
		fputc('\n', f);
	}
	fclose(f);
	comment = aprintf("-Y%s", tmp);
	sccs_freetree(d);
	cmds[i=0] = "bk";
	cmds[++i] = "commit";
	cmds[++i] = "-dF";
	if (quiet) cmds[++i] = "-s";
	cmds[++i] = comment;
	cmds[++i] = 0;
	i = spawnvp_ex(_P_WAIT, "bk", cmds);
	if (!WIFEXITED(i) || WEXITSTATUS(i)) {
		fprintf(stderr, "cset: commit says 0x%x\n", (u32)i);
		free(comment);
		unlink(tmp);
		return (1);
	}
	free(comment);
	unlink(tmp);
	return (0);
}

private int
undoit(MDBM *m)
{
	int	i, rc, pid, worked = 1;
	char	*t;
	FILE	*f;
	char	*av[10];
	char	rev[MAXREV+10];
	char	buf[MAXPATH];

	fprintf(stderr,
	    "\n!!! Cset operation failed.  Undoing changes... !!!\n\n");
	av[i=0] = "bk";
	av[++i] = "sfiles";
	av[++i] = "-gpAC";
	av[++i] = 0;
	pid = spawnvp_rPipe(av, &i, 0);
	if (pid == -1) {
		perror("spawnvp_rPipe");
		exit(1);
	}
	unless (f = fdopen(i, "r")) {
		perror("fdopen");
		kill(pid, SIGKILL);
		exit(1);
	}
	while (fgets(buf, sizeof(buf), f)) {
		chop(buf);
		t = strchr(buf, BK_FS);
		assert(t);
		*t = 0;
		unless (mdbm_fetch_str(m, buf)) continue;
		av[i=0] = "bk";
		av[++i] = "stripdel";
		sprintf(rev, "-r%s", &t[1]);
		av[++i] = rev;
		av[++i] = buf;
		av[++i] = 0;
		rc = spawnvp_ex(_P_WAIT, av[0], av);
		if (!WIFEXITED(rc) || WEXITSTATUS(rc)) {
			for (i = 0; av[i]; ++i) {
				if (i) fprintf(stderr, " ");
				fprintf(stderr, "%s", av[i]);
			}
			fprintf(stderr, ": failed.\n");
			worked = 0;
			/* Keep going */
		}
	}
	fclose(f);
	wait(0);
	mdbm_close(m);
	if (worked) fprintf(stderr, "Successfully cleaned up all files.\n");
	return (1);
}

private int
doit(int flags, char *file, char *op, char *revs)
{
	sccs	*s;
	delta	*d = 0;
	char	*sfile;
	int	ret;

	flags |= GET_EDIT;
	sfile = name2sccs(file);
	assert(sfile);
	unless (s = sccs_init(sfile, 0, 0)) {
		fprintf(stderr, "cset: could not init %s\n", sfile);
		free(sfile);
		return (1);
	}
	free(sfile);
	unless (HASGRAPH(s)) {
		fprintf(stderr, "cset: no graph in %s?\n", s->gfile);
		sccs_free(s);
		return (1);
	}
	unless (sccs_top(s)->flags & D_CSET) {
		fprintf(stderr,
		    "cset: %s has uncommitted deltas, aborting.\n", s->gfile);
		sccs_free(s);
		return (1);
	}
	if (sccs_clean(s, SILENT)) {
		sccs_free(s);
		return (1);
	}
	if (streq(file, CHANGESET) || streq(file, GCHANGESET)) {
		flags |= GET_SKIPGET;
	}
	if (streq(op, "-i")) {
		ret = sccs_get(s, 0, 0, revs, 0, flags, "-");
	} else {
		ret = sccs_get(s, 0, 0, 0, revs, flags, "-");
	}
	if (ret) {
		fprintf(stderr, "cset: get -e of %s failed\n", s->gfile);
		sccs_free(s);
		return (1);
	}
	if (flags & GET_SKIPGET) goto ok;	/* we are the cset file */
	if (comments_got()) {
		d = comments_get(0);
	} else {
		char	*what = streq(op, "-x") ? "Exclude" : "Include";

		d = sccs_parseArg(0, 'C', what, 0);
	}
	sccs_restart(s);
	if (sccs_delta(s, SILENT|DELTA_FORCE, d, 0, 0, 0)) {
		fprintf(stderr, "cset: could not delta %s\n", s->gfile);
		sccs_free(s);
		return (1);
	}
	do_checkout(s);
ok:	sccs_free(s);
	return (0);
}
