/*
 * Copyright 2000-2003,2005-2008,2011,2013-2016 BitMover, Inc
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

#include "sccs.h"
#include "nested.h"

private int	undoit(MDBM *m);
private int	doit(int flags, char *file, char *op, char *revs);
private	int	commit(int quiet, char **cmts);
private	char	**getComments(char *op, char *revs);
private void	clean(char *file);
private int	mergeInList(sccs *s, char *revs);

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
	int	i;
	FILE	*f;
	char	*av[20];
	MDBM	*m = mdbm_mem();
	char	**cmts;
	char	*t;
	char	*revarg;
	char	buf[MAXKEY];
	char	file[MAXPATH];
	char	*revbuf, **rlist = 0;

	av[i = 0] = "bk";
	av[++i] = "-?BK_NO_REPO_LOCK=YES";
	av[++i] = "cset";
	av[++i] = "--show-comp";
	revarg = aprintf("-r%s", revs);
	av[++i] = revarg;
	av[++i] = 0;

	flags &= ~PRINT;	/* can't edit and print */
	unless (f = popenvp(av, "r")) {
		perror("popenvp");
		free(revarg);
		return (1);
	}

	cmts = getComments(op, revarg);
	free(revarg);

	START_TRANSACTION();
	file[0] = 0;
	while (fgets(buf, sizeof(buf), f)) {
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
					pclose(f);
					clean(file);
					/*
					 * Nota bene: this "knows" that the
					 * ChangeSet file is never edited
					 * in a normal tree.
					 */
					xfile_delete(CHANGESET, 'p');
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
	if (pclose(f)) {
		return (undoit(m));
	}
	if (file[0]) {
		revbuf = joinLines(",", rlist);
		freeLines(rlist, free);
		rlist = 0;
		if (doit(flags, file, op, revbuf)) {
			clean(file);
			xfile_delete(CHANGESET, 'p');
			free(revbuf);
			return (undoit(m));
		}
		mdbm_store_str(m, file, "", 0);
		free(revbuf);
	}
	STOP_TRANSACTION();
	mdbm_close(m);
	return (commit(flags & SILENT, cmts));
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
	spawnvp(_P_WAIT, "bk", av);
}

private	char **
getComments(char *op, char *revs)
{
	int	i;
	FILE	*f;
	char	*av[20];
	char	**cmts = 0;
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
	unless (f = popenvp(av, "r")) {
		perror("popenvp");
		return (0);
	}
	if (comments_got()) cmts = comments_return(0);
	if (streq(op, "-i")) {
		strcpy(buf, "Cset include: ");
	} else {
		strcpy(buf, "Cset exclude: ");
	}
	while (fgets(&buf[14], sizeof(buf) - 14, f)) {
		chop(buf);
		cmts = addLine(cmts, strdup(buf));
	}
	pclose(f);
	return (cmts);
}

private int
commit(int quiet, char **cmts)
{
	int	i;
	char	*comment = 0;
	char	*cmds[20];
	char	*tmp = bktmp(0);
	FILE	*f = fopen(tmp, "w");

	EACH(cmts) {
		fputs(cmts[i], f);
		fputc('\n', f);
	}
	freeLines(cmts, free);
	fclose(f);
	comment = aprintf("-Y%s", tmp);
	cmds[i=0] = "bk";
	cmds[++i] = "-?BK_NO_REPO_LOCK=YES";
	cmds[++i] = "commit";
	cmds[++i] = "-dF";
	cmds[++i] = "-S";
	if (quiet) cmds[++i] = "-q";
	cmds[++i] = comment;
	cmds[++i] = 0;
	i = spawnvp(_P_WAIT, "bk", cmds);
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
	int	i, rc, worked = 1;
	char	*t, *p;
	FILE	*f;
	char	*av[10];
	char	rev[MAXREV+10];
	char	buf[MAXPATH];

	fprintf(stderr,
	    "\n!!! Cset operation failed.  Undoing changes... !!!\n\n");
	av[i=0] = "bk";
	av[++i] = "gfiles";
	av[++i] = "-pA";
	av[++i] = 0;
	unless (f = popenvp(av, "r")) {
		perror("popenvp");
		exit(1);
	}
	while (fgets(buf, sizeof(buf), f)) {
		chop(buf);
		t = strchr(buf, BK_FS);
		assert(t);
		*t = 0;
		unless (mdbm_fetch_str(m, buf)) continue;
		if ((p = strstr(buf, "/" GCHANGESET)) &&
		    streq(p, "/" GCHANGESET)) {
			*p = 0;
			i = systemf(
			    "bk --cd='%s' bk undo -fSr%s", buf, &t[1]);
			*p = '/';
			if (i) worked = 0;
			continue;
		}
		av[i=0] = "bk";
		av[++i] = "-?BK_NO_REPO_LOCK=YES";
		av[++i] = "stripdel";
		sprintf(rev, "-r%s", &t[1]);
		av[++i] = rev;
		av[++i] = buf;
		av[++i] = 0;
		rc = spawnvp(_P_WAIT, av[0], av);
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
	pclose(f);
	mdbm_close(m);
	if (worked) fprintf(stderr, "Successfully cleaned up all files.\n");
	return (1);
}

/* no merge allowed in cset list */
private int
mergeInList(sccs *s, char *revs)
{
	char	*p, *t;
	ser_t	d;

	assert(CSET(s));
	unless (revs) return (0);
	assert(!strchr(revs, '-'));	/* we know list is expanded already */
	t = revs;
	while (t && *t) {
		if (p = strchr(t, ',')) *p = 0;
		d = sccs_findrev(s, t);
		if (d && MERGE(s, d)) {
			fprintf(stderr,
			    "cset: Merge cset found in revision list: (%s).  "
			    "Aborting. (cset1)\n", t);
			if (p) *p = ',';
			return (1);
		}
		if (p) *p++ = ',';
		t = p;
	}
	return (0);
}

private int
doit(int flags, char *file, char *op, char *revs)
{
	sccs	*s;
	ser_t	d = 0;
	char	*sfile, *p;
	int	ret;

	sfile = name2sccs(file);
	assert(sfile);
	if ((p = strstr(file, "/" GCHANGESET)) && streq(p, "/" GCHANGESET)) {
		/* Got a component ChangeSet file */
		*p = 0;
		unless (exists(sfile)) {
			/* Belts and Suspenders: failure caught in cset.c */
			fprintf(stderr,
			    "Component '%s' not populated.  "
			    "Populate and try again.\n", file);
			ret = 1;
		} else {
			ret = systemf(
			    "bk --cd='%s' bk cset -S%s %s%s",
			    file, (flags & SILENT) ? "q" : "", op, revs);
		 }
		*p = '/';
		free(sfile);
		return (ret);
	}
	flags |= GET_EDIT;
	unless (s = sccs_init(sfile, 0)) {
		fprintf(stderr, "cset: could not init %s\n", sfile);
		free(sfile);
		return (1);
	}
	free(sfile);
	unless (HASGRAPH(s)) {
		fprintf(stderr, "cset: no graph in %s?\n", s->gfile);
err:		sccs_free(s);
		return (1);
	}
	/*
	 * If we are a product cset, don't do this test, we have no marks.
	 * XXX - if we ever make marks in normal repos optional this breaks.
	 */
	unless ((CSET(s) && proj_isProduct(s->proj)) ||
	    (FLAGS(s, sccs_top(s)) & D_CSET)) {
		fprintf(stderr,
		    "cset: %s has uncommitted deltas, aborting.\n", s->gfile);
		goto err;
	}
	if (sccs_clean(s, SILENT)) goto err;
	if (CSET(s)) {
		if (mergeInList(s, revs)) goto err;
		flags |= GET_SKIPGET;
	}
	if (streq(op, "-i")) {
		ret = sccs_get(s, 0, 0, revs, 0, flags, s->gfile, 0);
	} else {
		ret = sccs_get(s, 0, 0, 0, revs, flags, s->gfile, 0);
	}
	if (ret) {
		fprintf(stderr, "cset: get -e of %s failed\n", s->gfile);
		goto err;
	}
	if (flags & GET_SKIPGET) goto ok;	/* we are the cset file */
	unless (comments_got()) {
		char	*what = streq(op, "-x") ? "Exclude" : "Include";

		d = sccs_parseArg(s, 0, 'C', what, 0);
	}
	sccs_restart(s);
	if (sccs_delta(s, SILENT|DELTA_FORCE, d, 0, 0, 0)) {
		fprintf(stderr, "cset: could not delta %s\n", s->gfile);
		goto err;
	}
	sccs_restart(s);
	do_checkout(s, 0, 0);
ok:	sccs_free(s);
	return (0);
}
