#include "sccs.h"
#include "resolve.h"
#include "regex.h"
#include "range.h"

#define	COLLAPSE_BACKUP_SFIO "BitKeeper/tmp/collpase_backup_sfio"
#define	COLLAPSE_BACKUP_PATCH "BitKeeper/tmp/collapse.patch"

/*
 * TODO:
 *   - test fixing an exclude cset
 *   - add collapse testcases
 *   - support -r
 *   - support no -e
 *   - add -m
 */

#define	COLLAPSE_FIX	0x10000000 /* 'bk fix' -> don't strip committed deltas */
#define	COLLAPSE_NOSAVE	0x20000000 /* don't save backup patch */
#define	COLLAPSE_NOLOG	0x40000000 /* Don't update BitKeeper/etc/collapsed */

private	int	do_cset(char *rev);
private	int	do_file(char *file, char *tiprev);
private	int	savedeltas(sccs *s, delta *d, void *data);
private	delta	*parent_of_tip(sccs *s);
private	char	**fix_genlist(char *rev);
private	int	fix_savesfio(char **flist, char *file);
private	int	fix_setupcomments(sccs *s, char **rmdeltas);
private	int	update_collapsed_file(char *newcsets);

private	int	flags;		/* global to pass to callbacks */
private	char	*me;		/* name of command */

int
collapse_main(int ac, char **av)
{
	int	c;
	char	*after = 0;
	char	*revlist = 0;
	int	edit = 0, merge = 0;

	me = "collapse";
	flags = 0;
	while ((c = getopt(ac, av, "a:eLmr:qs")) != -1) {
		switch (c) {
		    case 'a': after = optarg; break;
		    case 'e': edit = 1; break;
		    case 'L': flags |= COLLAPSE_NOLOG; break;
		    case 'm': merge = 1; break;
		    case 'r': revlist = optarg; break;
		    case 'q': flags |= SILENT; break;
		    case 's': flags |= COLLAPSE_NOSAVE; break;
		    default :
usage:			system("bk help -s collapse");
			return (1);
		}
	}
	if (av[optind]) goto usage;
	if (merge && (after || revlist)) {
		fprintf(stderr, "%s: cannot combine -m with -r or -a\n", me);
		return (1);
	}
	if (after && revlist) {
		fprintf(stderr, "%s: -r or -a, but not both.\n", me);
		return (1);
	}
	if (merge && edit) {	/* XXX this test should be more generic */
		fprintf(stderr, "%s: can't edit csets containing merges\n", me);
		return (1);
	}

	/* Modes we don't support yet */
	if (revlist) {
		fprintf(stderr, "%s: -r option not yet supported\n", me);
		return (1);
	}
	if (merge) {
		fprintf(stderr, "%s: -m option not yet supported\n", me);
		return (1);
	}
	if (!edit) {
		fprintf(stderr, "%s: -e option required\n", me);
		return (1);
	}
	return (do_cset(after));
}

int
fix_main(int ac,  char **av)
{
	int	c, i;
	int	cset = 0, rc = 0;

	me = "fix";
	flags = COLLAPSE_FIX;
	while ((c = getopt(ac, av, "cqs")) != -1) {
		switch (c) {
		    case 'c': cset = 1; flags &= ~COLLAPSE_FIX; break;
		    case 'q': flags |= SILENT; break;		/* undoc 2.0 */
		    case 's': flags |= COLLAPSE_NOSAVE; break;
		    default :
			system("bk help -s fix");
			return (1);
		}
	}
	if (cset) {
		rc = do_cset(0);
	} else {
		for (i = optind; av[i]; i++) {
			if (rc = do_file(av[i], 0)) break;
		}
	}
	return (rc);
}

/*
 * Fix all csets such that 'rev' is the new TOT.
 * Also delta's after that cset are collapsed.
 * If 'rev' is null it defaults to the parent of the current TOT.
 */
private int
do_cset(char *rev)
{
	sccs	*s = 0;
	delta	*d;
	int	i;
	char	*csetrev = 0;
	char	**flist = 0;
	int	rc = 1;
	char	*csetfile = "";
	FILE	*f;
	char	buf[64];

	if (proj_cd2root()) {
		fprintf(stderr, "%s: can't find repository root\n", me);
		return (1);
	}
	s = sccs_csetInit(0);
	unless (rev) {
		unless (d = parent_of_tip(s)) goto out;
		rev = d->rev;
	}
	unless (d = sccs_findrev(s, rev)) {
		fprintf(stderr, "%s: rev %s doesn't exist.\n", me, rev);
		goto out;
	}
	sccs_md5delta(s, d, buf);
	csetrev = aprintf("@%s", buf);

	/* BK_CSETLIST=/file/of/cset/keys */
	csetfile = bktmp(0, "collapse-csets");
	f = fopen(csetfile, "w");
	range_walkrevs(s, d, 0, walkrevs_printmd5key, f);
	fclose(f);
	safe_putenv("BK_CSETLIST=%s", csetfile);

	/* run 'pre-fix' trigger for 'bk fix' */
	if (streq(me, "fix") && trigger("fix", "pre")) goto out;

	/* always run 'pre-collapse' trigger */
	if (trigger("collapse", "pre")) goto out;

	save_log_markers();
	unlink(COLLAPSE_BACKUP_SFIO);	/* remote old backup file */
	unlink(COLLAPSE_BACKUP_PATCH);
	flist = fix_genlist(rev);
	unless (flags & COLLAPSE_NOSAVE) {
		if (sysio(csetfile, COLLAPSE_BACKUP_PATCH, 0,
			"bk", "makepatch", "-", SYS)) {
			fprintf(stderr, "%s: unable to save patch, abort.\n", me);
			goto out;
		}
	}
	sccs_free(s);
	s = 0;
	if (fix_savesfio(flist, COLLAPSE_BACKUP_SFIO)) {
		fprintf(stderr, "%s: unable to save sfio, abort.\n", me);
		goto out;
	}
	/* rollback all files */
	EACH (flist) {
		if (do_file(flist[i], csetrev)) {
			/* fix -c failed, restore backup if possible*/
			fprintf(stderr, "%s: failed, ", me);
			fprintf(stderr, "restoring backup\n");
			unless (restore_backup(COLLAPSE_BACKUP_SFIO, 1)) {
				unlink(COLLAPSE_BACKUP_SFIO);
			}
			goto out;
		}
	}
	unless (flags & COLLAPSE_NOLOG) {
		if (update_collapsed_file(csetfile)) goto out;
	}
	unlink(COLLAPSE_BACKUP_SFIO);
	update_log_markers(0);
	rc = 0;
out:
	if (csetfile[0]) {
		unlink(csetfile);
		free(csetfile);
	}
	if (s) sccs_free(s);
	if (csetrev) free(csetrev);
	freeLines(flist, free);
	return (rc);
}

/*
 * Rollback a sfile to an old tip without changing the gfile.  Any
 * non-contents changes will be remembered and recreated as new deltas
 * on the new tip key.
 */
private int
do_file(char *file, char *tiprev)
{
	sccs	*s;
	delta	*d, *e, *tipd;
	char	*pathname, *savefile = 0, *cmd;
	mode_t	mode;
	u32	xflags, flagdiffs;
	int	rc = 1;
	char	**rmdeltas = 0;
	int	i;
	struct	stat	sb;
	char	*sfile = 0, *gfile = 0, *pfile = 0;

	sfile = name2sccs(file);
	s = sccs_init(sfile, 0);
	unless (s && HASGRAPH(s)) {
		fprintf(stderr, "%s removed while fixing?\n", sfile);
		goto done;
	}
	gfile = strdup(s->gfile);
	pfile = strdup(s->pfile);
	d = sccs_findrev(s, "+");
	if (tiprev) {
		tipd = sccs_findrev(s, tiprev);
	} else {
		unless (tipd = parent_of_tip(s)) goto done;
		if (streq(tipd->rev, "1.0")) tipd = 0;
	}
	/* tipd is not the delta that will be the new tip */
	if (tipd == d) {
		rc = 0;
		goto done;
	}

	/* save deltas to remove in rmdeltas */
	if (range_walkrevs(s, tipd, d, savedeltas, &rmdeltas)) goto done;
	reverseLines(rmdeltas);	/* oldest first (for comments) */

	/*
	 * Edit the file if it is not edited already.  (This should
	 * handle not touching files that are not edited but don't
	 * need keyword expansion.)
	 */
	unless (IS_EDITED(s) || CSET(s)) {
		if (sccs_get(s, 0,0,0,0, SILENT|GET_EDIT|GET_NOREGET, "-")) {
			fprintf(stderr, "%s: unable to edit %s\n", me, gfile);
			rc = 1;
			goto done;
		}
	}

	if (fix_setupcomments(s, rmdeltas)) goto done;

	if (tipd) {
		/* remember mode, path, xflags */
		mode = d->mode;
		pathname = strdup(d->pathname);
		xflags = sccs_xflags(d);

		unless (CSET(s)) {
			savefile = aprintf("%s.fix.%u", gfile, getpid());
			rename(gfile, savefile);
		}
		/* remove p.file */
		s->state &= ~S_PFILE;
		unlink(pfile);

		/* mark deltas to remove. */
		EACH(rmdeltas) {
			e = (delta *)rmdeltas[i];
			e->flags |= D_SET;
		}
		range_markMeta(s);
		stripdel_fixTable(s, &i);

		if (sccs_stripdel(s, me)) {
			fprintf(stderr, "%s: stripdel of %s failed\n",
			    me, gfile);
			goto done;
		}
		/* branch might be tip */
		sys("bk", "renumber", file, SYS);

		/* restore mode, path, xflags */
		tipd = sccs_findrev((s = sccs_reopen(s)), "+");
		unless (mode == tipd->mode) {
			if (sccs_admin(s,
				0, 0, 0, 0, 0, 0, 0, 0, mode2a(mode), 0)) {
				sccs_whynot(me, s);
				goto done;
			}
			tipd = sccs_findrev((s = sccs_reopen(s)), "+");
		}
		unless (streq(pathname, tipd->pathname)) {
			if (sccs_admin(s, 0, ADMIN_NEWPATH,
				0, 0, 0, 0, 0, 0, 0, 0)) {
				sccs_whynot(me, s);
				goto done;
			}
			tipd = sccs_findrev((s = sccs_reopen(s)), "+");
		}
		free(pathname);

		/* make xflags match */
		while (flagdiffs = (xflags ^ sccs_xflags(tipd))) {
			/* pick right most bit */
			flagdiffs &= -flagdiffs;

			cmd = aprintf("bk admin -%c%s '%s'",
			    (flagdiffs & xflags) ? 'f' : 'F',
			    xflags2a(flagdiffs), gfile);
			sccs_close(s);	/* winblows */
			i = system(cmd);
			free(cmd);
			if (i) {
				fprintf(stderr,
				    "%s: failed to restore xflags in %s\n",
				    me, gfile);
				goto done;
			}
			tipd = sccs_findrev((s = sccs_reopen(s)), "+");
		}

		/* regenerate new p.file */
		unless (CSET(s)) {
			rename(savefile, gfile);
			free(savefile);
			savefile = 0;
			if (sccs_get(s, "+", 0, 0, 0,
				SILENT|GET_EDIT|GET_SKIPGET, "-")) {
				fprintf(stderr, "%s: get -g %s failed\n",
				    me, gfile);
			}
		}
		lstat(gfile, &sb);
		s->gtime = sb.st_mtime;
		sccs_setStime(s);
	} else {
		/* delete the entire sfile */
		unlink(pfile);
		sccs_free(s);
		s = 0;
		unlink(sfile);
	}
	rc = 0;
 done:
	if (s) sccs_free(s);
	if (rmdeltas) freeLines(rmdeltas, 0);
	if (savefile) {
		rename(savefile, gfile);
		free(savefile);
	}
	free(sfile);
	free(gfile);
	free(pfile);
	return (rc);
}

/*
 * a callback used in do_file()/range_walkrevs() to collect all the deltas
 * to be removed from the tree.
 * We also check for deltas with CSET marks for the single file case.
 */
private int
savedeltas(sccs *s, delta *d, void *data)
{
	char	***rmdeltas = data;

	*rmdeltas = addLine(*rmdeltas, d);
	if (d->flags & D_CSET && (flags & COLLAPSE_FIX)) {
		fprintf(stderr, "%s: can't fix committed delta %s@%s\n",
		    me, s->gfile, d->rev);
		return (1);
	}
	return (0);
}

/*
 * Write the SCCS/c.file will all the delta comments for deltas that
 * will be removed from this file.  Each delta will be seperated by
 * a line with '---' and any existing contents of that file will be
 * presevered at the end.
 */
private int
fix_setupcomments(sccs *s, char **rmdeltas)
{
	delta	*d;
	int	i;
	char	**comments = 0;
	char	*cfile = sccs_Xfile(s, 'c');
	char	*p;
	FILE	*f;
	char	skippat[] =
	    "^Rename: .* ->|"
	    "^Merge rename: .* ->|"
	    "^Delete:|"
	    "^Change mode to|"
	    "^Turn o[nf]+ [A-Z0-9_]+ flag$|"
	    "^Auto merged$|"
	    "^SCCS merged\\.*$|"
	    "^[mM]erged*\\.*$|"  /* merge merged merge. merged. */
	    "^auto-union$";

	/* generate the list of delta comments we skip */
	if (p = re_comp(skippat)) {
		fprintf(stderr, "%s: regex failed %s\npat = %s\n",
		    me, p, skippat);
		return (1);
	}
	EACH (rmdeltas) {
		d = (delta *)rmdeltas[i];

		if (streq(d->rev, "1.0")) continue;
		unless (d->comments && d->comments[1]) continue;

		/*
		 * If the comments are just one line and then match our
		 * pattern, then ignore these comments.
		 */
		if (!d->comments[2] && re_exec(d->comments[1])) continue;

		comments = addLine(comments, joinLines("\n", d->comments));
	}
	if (p = loadfile(cfile, 0)) {
		chomp(p);
		comments = addLine(comments, p);
	}

	if (comments) {
		f = fopen(cfile, "w");
		assert(f);
		EACH (comments) {
			if (i > 1) fputs("---\n", f);
			fputs(comments[i], f);
			fputc('\n', f);
			free(comments[i]);
		}
		freeLines(comments, 0);
		fclose(f);
	}
	return (0);

}

/* Return the parent of the tip delta if the tip is not a merge. */
private delta *
parent_of_tip(sccs *s)
{
	delta	*d;

	d = sccs_findrev(s, "+");
	if (d->merge) {
		fprintf(stderr,
		    "%s: Unable to fix just %s|%s, it is a merge.\n",
		    me, s->gfile, d->rev);
		return(0);
	}
	return (d->parent);
}

/*
 * Generate the list of files that will need to be edited as part
 * of the fix.
 */
private char **
fix_genlist(char *rev)
{
	char	**flist = 0;
	char	*cmd, *p;
	int	status;
	MDBM	*idDB;
	hash	*h;
	FILE	*f = 0;
	char	buf[2*MAXKEY];

	cmd = aprintf("bk annotate -R%s..+ ChangeSet", rev);
	f = popen(cmd, "r");
	free(cmd);
	unless (f) goto out;
	unless (idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
		perror("idcache");
		goto out;
	}
	flist = addLine(0, strdup(CHANGESET));
	h = hash_new(HASH_MEMHASH);
	while (fnext(buf, f)) {
		unless (p = separator(buf)) continue;
		unless (hash_insert(h, buf, p-buf, 0, 0)) continue;
		*p = 0;
		p = key2path(buf, idDB);
		flist = addLine(flist, name2sccs(p));
		free(p);
	}
	status = pclose(f);
	unless (WIFEXITED(status) && !WEXITSTATUS(status)) {
		freeLines(flist, free);
		flist = 0;
	}
	hash_free(h);
	mdbm_close(idDB);
out:
	return (flist);
}

/*
 * Save a SFIO of all the files that are going to be modified as part of
 * this operation, so that it can be restored later.
 */
private int
fix_savesfio(char **flist, char *file)
{
	char	*cmd;
	int	status, i;
	FILE	*sfio;

	cmd = aprintf("bk sfio -omq > %s", file);
	sfio = popen(cmd, "w");
	free(cmd);
	unless (sfio) return (-1);
	EACH (flist) fprintf(sfio, "%s\n", flist[i]);
	status = pclose(sfio);
	unless (WIFEXITED(status) && !WEXITSTATUS(status)) return (-1);
	return (0);
}

private int
update_collapsed_file(char *newcsets)
{
	FILE	*f;
	int	i;
	char	**csets = 0;
	char	buf[MAXLINE];

	get(COLLAPSED, SILENT|GET_EDIT, "-");
	if (f = fopen(COLLAPSED, "r")) {
		while (fnext(buf, f)) {
			chomp(buf);
			unless (*buf) continue;
			csets = addLine(csets, strdup(buf));
		}
		fclose(f);
	}
	if (f = fopen(newcsets, "r")) {
		while (fnext(buf, f)) {
			chomp(buf);
			csets = addLine(csets, strdup(buf));
		}
		fclose(f);
	}
	uniqLines(csets, free);
	f = fopen(COLLAPSED, "w");
	EACH(csets) fprintf(f, "%s\n", csets[i]);
	fclose(f);
	return (sys("bk", "delta", "-aqy", COLLAPSED, SYS));
}
