/* Copyright (c) 2000 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "zlib/zlib.h"

WHATSTR("@(#)%K%");
private int	undoit(MDBM *m);
private int	doit(int flags, char *file, char *op, char *revs, delta *d);
private int	commit(int quiet, delta *d);
private	delta	*getComments(char *op, char *revs);
private void	clean(char *file);
private void	unedit();

/*
 * cset_inex.c - changeset include/exclude processing.
 *
 * Something like this:
 *	bk cset -r<revs> | while read file rev
 *	do	bk get -e -i<rev> <file>
 *		bk delta <file>
 *	done
 *	bk commit
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
	char	buf[MAXPATH+200];
	char	file[MAXPATH];
	char	revbuf[MAXPATH];

	av[i = 0] = "bk";
	av[++i] = "cset";
	sprintf(buf, "-r%s", revs);
	av[++i] = buf;
	av[++i] = 0;
	pid = spawnvp_rPipe(av, &fd);
	if (pid == -1) {
		perror("spawnvp_rPipe");
		return (1);
	}
	unless (f = fdopen(fd, "r")) {
		perror("fdopen");
#ifndef WIN32
		kill(pid, SIGKILL);
#endif
		return (1);
	}

	d = getComments(op, revs);

	file[0] = 0;
	revbuf[0] = 0;
	while (fgets(buf, sizeof(buf), f)) {
#ifdef OLD_LICENSE
		if (checkLog(0, 1)) {
			fprintf(stderr, "Cset aborted, no changes applied\n");
#ifndef WIN32
			kill(pid, SIGKILL);
			wait(0);
#endif
			mdbm_close(m);
			return (1);
		}
#endif
		chop(buf);
		t = strchr(buf, '@');
		assert(t);
		*t = 0;
		if (file[0]) {
			if (streq(file, buf)) {	/* add this rev */
				if (revbuf[0]) {
					strcat(revbuf, ",");
					strcat(revbuf, &t[1]);
				} else {
					strcpy(revbuf, &t[1]);
				}
			} else {		/* flush file, start again */
				if (doit(flags, file, op, revbuf, d)) {
#ifndef WIN32
					kill(pid, SIGKILL);
					wait(0);
#endif
					clean(file);
					/*
					 * Nota bene: this "knows" that the
					 * ChangeSet file is never edited
					 * in a normal tree.
					 */
					unedit();
					return (undoit(m));
				}
				mdbm_store_str(m, file, "", 0);
				strcpy(file, buf);
				strcpy(revbuf, &t[1]);
			}
		} else {
			strcpy(file, buf);
			strcpy(revbuf, &t[1]);
		}
	}
	if (file[0]) {
		if (doit(flags, file, op, revbuf, d)) {
#ifndef WIN32
			kill(pid, SIGKILL);
			wait(0);
#endif
			clean(file);
			unedit();
			return (undoit(m));
		}
		mdbm_store_str(m, file, "", 0);
	}
#ifndef WIN32
	wait(0);
#endif
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
	char	buf[MAXLINE];

	/*
	 * First get the Cset key.
	 */
	av[i = 0] = "bk";
	av[++i] = "prs";
	av[++i] = "-h";
	av[++i] = "-d:SHORTKEY:\n";
	sprintf(buf, "-r%s", revs);
	av[++i] = buf;
	av[++i] = CHANGESET;
	av[++i] = 0;
	pid = spawnvp_rPipe(av, &i);
	if (pid == -1) {
		perror("spawnvp_rPipe");
		return (0);
	}
	unless (f = fdopen(i, "r")) {
		perror("fdopen");
#ifndef WIN32
		kill(pid, SIGKILL);
#endif
		return (0);
	}
	d = calloc(1, sizeof(delta));
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
#ifndef WIN32
	wait(0);
#endif

#if 0
	/*
	 * Now get the cset comments
	 *
	 * Currently commented out because the right thing to do is to have
	 * sccstool look this stuff up.
	 */
	av[i = 0] = "bk";
	av[++i] = "prs";
	av[++i] = "-h";
	av[++i] = "-d$each(:C:){:C:\n}";
	sprintf(buf, "-r%s", revs);
	av[++i] = buf;
	av[++i] = CHANGESET;
	av[++i] = 0;
	pid = spawnvp_rPipe(av, &i);
	if (pid == -1) {
		perror("spawnvp_rPipe");
		return (0);
	}
	unless (f = fdopen(i, "r")) {
		perror("fdopen");
		kill(pid, SIGKILL);
		return (0);
	}
	while (fgets(buf, sizeof(buf), f)) {
		chop(buf);
		d->comments = addLine(d->comments, strdup(buf));
	}
	fclose(f);
	wait(0);
#endif

	return (d);
}

private int
commit(int quiet, delta *d)
{
	int	i, j;
	char	*comment = 0;
	char	*cmds[10];

	j = 0;
	EACH(d->comments) {
		j += strlen(d->comments[i]) + 1;
	}
	assert(j);
	j += 5;
	comment = malloc(j);
	strcpy(comment, "-y");
	EACH(d->comments) {
		strcat(comment, d->comments[i]);
		strcat(comment, "\n");
	}
	sccs_freetree(d);
	cmds[i=0] = "bk";
	cmds[++i] = "commit";
	cmds[++i] = "-dFa";
	if (quiet) cmds[++i] = "-s";
	cmds[++i] = comment;
	cmds[++i] = 0;
	i = spawnvp_ex(_P_WAIT, "bk", cmds);
	if (!WIFEXITED(i) || WEXITSTATUS(i)) {
		free(comment);
		fprintf(stderr, "Commit says %d\n", i);
		return (1);
	}
	free(comment);
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
	pid = spawnvp_rPipe(av, &i);
	if (pid == -1) {
		perror("spawnvp_rPipe");
		exit(1);
	}
	unless (f = fdopen(i, "r")) {
		perror("fdopen");
#ifndef WIN32
		kill(pid, SIGKILL);
#endif
		exit(1);
	}
	while (fgets(buf, sizeof(buf), f)) {
		chop(buf);
		t = strchr(buf, '@');
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
#ifndef WIN32
	wait(0);
#endif
	mdbm_close(m);
	if (worked) fprintf(stderr, "Successfully cleaned up all files.\n");
	return (1);
}

private int
doit(int flags, char *file, char *op, char *revs, delta *d)
{
	sccs	*s;
	delta	*copy = 0;
	char	*sfile;
	int	i, ret;

	flags |= GET_EDIT;
	sfile = name2sccs(file);
	assert(sfile);
	unless (s = sccs_init(sfile, 0, 0)) {
		fprintf(stderr, "Could not init %s\n", sfile);
		free(sfile);
		return (1);
	}
	free(sfile);
	unless (s->tree) {
		fprintf(stderr, "No graph in %s?\n", s->gfile);
		sccs_free(s);
		return (1);
	}
	unless (sccs_top(s)->flags & D_CSET) {
		fprintf(stderr,
		    "%s has uncommitted deltas, aborting.\n", s->gfile);
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
		fprintf(stderr, "Get -e of %s failed\n", s->gfile);
		sccs_free(s);
		return (1);
	}
	if (flags & GET_SKIPGET) goto ok;
	if (d) {
		copy = calloc(1, sizeof(delta));
		EACH(d->comments) {
			copy->comments =
			    addLine(copy->comments, strdup(d->comments[i]));
		}
	}
	sccs_restart(s);
	if (sccs_delta(s, SILENT|DELTA_FORCE, copy, 0, 0, 0)) {
		fprintf(stderr, "Could not delta %s\n", s->gfile);
		sccs_free(s);
		return (1);
	}
ok:	sccs_free(s);
	return (0);
}
