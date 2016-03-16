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

#include "sccs.h"
#include "resolve.h"
#include "range.h"
#include "nested.h"
#include "poly.h"

#define	COLLAPSE_BACKUP_SFIO "BitKeeper/tmp/collapse_backup_sfio"
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
#define	COLLAPSE_LOG	0x40000000 /* Update BitKeeper/etc/collapsed */
#define	COLLAPSE_DELTAS	0x80000000 /* -d save file deltas */
#define	COLLAPSE_PONLY	0x01000000 /* only collapse product */

private	int	do_cset(sccs *s, char *rev, char **nav);
private	int	do_file(char *file, char *tiprev);
private	int	savedeltas(sccs *s, ser_t d, void *data);
private	ser_t	parent_of_tip(sccs *s);
private	char	**fix_genlist(char *rev);
private	int	fix_savesfio(char **flist, char *file);
private	int	fix_setupcomments(sccs *s, ser_t *rmdeltas);
private	int	update_collapsed_file(char *newcsets);
private	int	gateCheck(void);

private	int	flags;		/* global to pass to callbacks */
private	char	*me;		/* name of command */

int
collapse_main(int ac, char **av)
{
	sccs	*s = 0;
	int	c, rc = 1;
	char	*after = 0;
	char	*revlist = 0;
	int	edit = 0, merge = 0, fromurl = 0;
	int	standalone = 0;
	char	**nav = 0;
	char	*url = 0;
	char	buf[MAXLINE];
	longopt	lopts[] = {
		{ "no-save", 310 },

		/* aliases */
		{ "standalone", 'S' },
		{ 0, 0 }
	};

	me = "collapse";
	flags = 0;
	while ((c = getopt(ac, av, "@|a:delmPqr:Ss|", lopts)) != -1) {
		/*
		 * Collect options for running collapse in components.
		 * lm sez: unless we are going to try and replay a
		 * partially completed collapse there is no reason to
		 * not respect -s (don't save a patch).
		 */
		unless ((c == 'a') || (c == 'r') || (c == '@')) {
			nav = bk_saveArg(nav, av, c);
		}
		switch (c) {
		    case '@':
			if (url) usage();
			if (optarg) url = parent_normalize(optarg);
			fromurl = 1;
			break;
		    case 'a':
			if (after) usage(); after = strdup(optarg); break;
		    case 'd': flags |= COLLAPSE_DELTAS; break;
		    case 'e': edit = 1; break;
		    case 'l': flags |= COLLAPSE_LOG; break;
		    case 'm': merge = 1; break;
		    case 'P': flags |= COLLAPSE_PONLY; break;
		    case 'r':
			if (revlist) usage(); revlist = optarg; break;
		    case 'q': flags |= SILENT; break;
		    case 'S': standalone = 1; break;
		    case 's':  /* reserved for --subset */
			fprintf(stderr,
			    "%s: -s was renamed to --no-save\n", prog);
			usage();
		    case 310: // --no-save
			 flags |= COLLAPSE_NOSAVE;
			 break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	bk_nested2root(standalone);
	if (gateCheck()) goto out;
	if (merge && (after || revlist || fromurl)) {
		fprintf(stderr, "%s: cannot combine -m with -r or -a\n", me);
		goto out;
	}
	if (!!after + !!revlist + !!fromurl > 1) {
		fprintf(stderr, "%s: only one of -r, -a or -@.\n", me);
		goto out;
	}
	if (merge && edit) {	/* XXX this test should be more generic */
		fprintf(stderr, "%s: can't edit csets containing merges\n", me);
		goto out;
	}

	/* Modes we don't support yet */
	if (revlist && streq(revlist, "+")) {
		/* roll back to my parent */
		assert(after == 0); /* we should be doing parent already */
	} else if (revlist) {
		fprintf(stderr, "%s: -r option not yet supported\n", me);
		goto out;
	}
	if (merge) {
		fprintf(stderr, "%s: -m option not yet supported\n", me);
		goto out;
	}
	if (!edit) {
		fprintf(stderr, "%s: -e option required\n", me);
		goto out;
	}
	s = sccs_csetInit(0);
	if (after && strchr(after, ',')) {
		fprintf(stderr,
		    "%s: rev passed to -a (%s) is not singular\n", prog, after);
		goto out;
	}
	if (fromurl) {
		FILE	*f;
		char	**parents;

		unless (url) {
			parents = parent_pullp();
			if (nLines(parents) != 1) {
				fprintf(stderr,
				    "%s: no single parent to use for -@\n",
				    me);
				freeLines(parents, free);
				goto out;
			}
			url = parent_normalize(parents[1]);
			freeLines(parents, free);
		}
		sprintf(buf,
		    "bk changes --same-component -r+ -Sqnd:MD5KEY: '%s'", url);
		f = popen(buf, "r");
		after = fgetline(f);
		if (after) after = strdup(after);
		if (pclose(f) || !after) {
			fprintf(stderr,
			    "%s: failed to contact %s for -@.\n", me, url);
			goto out;
		}
		unless (sccs_findKey(s, after)) {
			fprintf(stderr,
			    "%s: current repo is behind %s, must pull "
			    "before collapsing.\n", me, url);
			goto out;
		}
	}
	rc = do_cset(s, after, nav);
	s = 0;			/* do_cset frees s */
out:	freeLines(nav, free);
	if (s) sccs_free(s);
	if (after) free(after);
	if (url) free(url);
	return (rc);
}

int
fix_main(int ac,  char **av)
{
	sccs	*s;
	int	c, i;
	int	cset = 0, rc = 1;
	int	standalone = 0;
	char	*after = 0;
	char	**nav = 0;
	longopt	lopts[] = {
		{ "standalone", 'S' },
		{ 0, 0 }
	};

	me = "fix";
	flags = COLLAPSE_FIX;
	while ((c = getopt(ac, av, "a;cdPqSs", lopts)) != -1) {
		/*
		 * Collect options for running collapse in components.
		 */
		unless ((c == 'a') || (c == 'c')) {
			nav = bk_saveArg(nav, av, c);
		}
		switch (c) {
		    case 'a': after = optarg; break;
		    case 'c': cset = 1; flags &= ~COLLAPSE_FIX; break;
		    case 'd': flags |= COLLAPSE_DELTAS; break;
		    case 'P': flags |= COLLAPSE_PONLY; break;
		    case 'q': flags |= SILENT; break;		/* undoc 2.0 */
		    case 'S': standalone = 1; break;
		    case 's': flags |= COLLAPSE_NOSAVE; break;
		    default: bk_badArg(c, av);
		}
	}
	/* collapse needs -e */
	if (gateCheck()) goto out;
	nav = addLine(nav, strdup("-e"));
	if (cset) {
		if (after) usage(); /* use collapse instead */
		bk_nested2root(standalone);
		s = sccs_csetInit(0);
		rc = do_cset(s, 0, nav); /* this frees s */
	} else {
		for (i = optind; av[i]; i++) {
			if (rc = do_file(av[i], after)) break;
		}
	}
out:	freeLines(nav, free);
	return (rc);
}

private int
gateCheck(void)
{
	if (proj_isComponent(0)) {
		if (nested_isGate(0)) {
gaterr:			fprintf(stderr, "%s: not allowed in a gate\n", prog);
			return (1);
		}
	} else if (proj_isProduct(0)) {
		if (nested_isGate(0)) goto gaterr;
		if (nested_isPortal(0)) {
			fprintf(stderr, "%s: "
			    "not allowed for a product in a portal\n", prog);
			return (1);
		}
	}
	return (0);
}

/*
 * Fix all csets such that 'rev' is the new TOT.
 * Also delta's after that cset are collapsed.
 * If 'rev' is null it defaults to the parent of the current TOT.
 */
private int
do_cset(sccs *s, char *rev, char **nav)
{
	ser_t	d;
	int	i;
	char	*csetrev = 0;
	char	**flist = 0;
	int	rc = 1;
	char	*csetfile = "";
	wrdata	wr;
	FILE	*f;
	nested	*n = 0;
	comp	*c;
	char	buf[MD5LEN];

	if (proj_cd2root()) {
		fprintf(stderr, "%s: can't find repository root\n", me);
		goto out;
	}
	cmdlog_lock(CMD_NESTED_WRLOCK|CMD_WRLOCK);
	unless (rev) {
		unless (d = parent_of_tip(s)) goto out;
		rev = REV(s, d);
	}
	unless (d = sccs_findrev(s, rev)) {
		fprintf(stderr, "%s: rev %s doesn't exist.\n", me, rev);
		goto out;
	}
	sccs_md5delta(s, d, buf);
	csetrev = aprintf("@@%s", buf);

	/* BK_CSETLIST=/file/of/cset/keys */
	csetfile = bktmp(0);
	f = fopen(csetfile, "w");
	walkrevs_setup(&wr, s, L(d), 0, 0);
	while (d = walkrevs(&wr)) {
		char    buf[MAXKEY];

		sccs_md5delta(s, d, buf);
		fprintf(f, "%s\n", buf);
	}
	walkrevs_done(&wr);
	fclose(f);
	if (size(csetfile) == 0) {
		fprintf(stderr, "Nothing to collapse.\n");
		rc = 0;
		goto out;
	}
	safe_putenv("BK_CSETLIST=%s", csetfile);

	if (proj_isProduct(0) && !(flags & COLLAPSE_PONLY)) {
		char	**keys = file2Lines(0, csetfile);
		int	err = 0;

		unless (n = nested_init(0, 0, keys, 0)) {
			fprintf(stderr, "%s: ensemble failed.\n", me);
			exit (1);
		}
		if (n->cset) sccs_close(n->cset);
		freeLines(keys, free);
		EACH_STRUCT(n->comps, c, i) {
			if (c->included && !C_PRESENT(c)) {
				fprintf(stderr,
				    "%s: component %s needed, "
				    "but not populated.\n", me, c->path);
				err = 1;
			}
		}
		if (err) goto out;
	}


	/* run 'pre-fix' trigger for 'bk fix' */
	if (streq(me, "fix") && trigger("fix", "pre")) goto out;

	/* always run 'pre-collapse' trigger */
	if (trigger("collapse", "pre")) goto out;

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
	if (flags & COLLAPSE_LOG) {
		if (update_collapsed_file(csetfile)) goto out;
	}
	if (n) {
		char	**vp;
		int	i, j;

		/*
		 * The product has already been done, so just print
		 * out the message.
		 */
		unless (flags & SILENT) {
			printf("#### %c%s in Product ####\n",
			    toupper(me[0]), me+1);
			fflush(stdout);
		}
		START_TRANSACTION();
		EACH_STRUCT(n->comps, c, i) {
			if (c->product) continue;
			unless (c->included) continue;
			assert(C_PRESENT(c));
			if (c->new) {
				/*
				 * We are rolling back to before the
				 * component was created.  What to do?
				 */
				fprintf(stderr,
				    "%s: cannot collapse to before %s "
				    "was created.\n", me, c->path);
				goto out;
			}
			vp = addLine(0, strdup("bk"));
			vp = addLine(vp, strdup("-?BK_NO_REPO_LOCK=YES"));
			vp = addLine(vp, strdup("collapse"));
			EACH_INDEX(nav, j) vp = addLine(vp, strdup(nav[j]));
			vp = addLine(vp, aprintf("-Sa%s", c->lowerkey));
			vp = addLine(vp, 0);
			unless (flags & SILENT) {
				printf("#### %c%s in %s ####\n",
				    toupper(me[0]), me+1, c->path);
				fflush(stdout);
			}
			if (chdir(c->path)) {
				perror(c->path);
				freeLines(vp, free);
				goto out;
			}
			rc = spawnvp(_P_WAIT, "bk", &vp[1]);
			freeLines(vp, free);
			proj_cd2product();
			if (WIFEXITED(rc)) rc = WEXITSTATUS(rc);
			if (rc) {
				fprintf(stderr, "Could not %s to %s.\n",
				    me, c->lowerkey);
				goto out;
			}
			rc = 1;	/* restore default failure */
		}
		STOP_TRANSACTION();
	}
	unlink(COLLAPSE_BACKUP_SFIO);
	rc = 0;
out:
	if (n) nested_free(n);
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
	ser_t	d, e, m, tipd;
	char	*pathname, *savefile = 0, *cmd;
	mode_t	mode;
	u32	xflags, flagdiffs;
	int	rc = 1;
	ser_t	*rmdeltas = 0;
	int	i;
	pfile	pf = {0};
	char	*sfile = 0, *gfile = 0;
	u8	*premap;
	char	*inc, *exc = 0;
	time_t	gtime = 0;

	cmdlog_lock(CMD_WRLOCK);
	sfile = name2sccs(file);
	s = sccs_init(sfile, 0);
	unless (s && HASGRAPH(s)) {
		fprintf(stderr, "%s removed while fixing?\n", sfile);
		goto done;
	}
	gfile = strdup(s->gfile);
	d = sccs_findrev(s, "+");
	tipd = tiprev ? sccs_findrev(s, tiprev) : parent_of_tip(s);
	unless (tipd) goto done;
	if (tipd == TREE(s)) tipd = 0;
	/* tipd is not the delta that will be the new tip */
	if (tipd == d) {
		rc = 0;
		goto done;
	}

	/* save deltas to remove in rmdeltas */
	if (range_walkrevs(s, L(tipd), L(d), 0, savedeltas, &rmdeltas)) {
		goto done;
	}
	reverseArray(rmdeltas);	/* oldest first (for comments) */

	/*
	 * Edit the file if it is not edited already.  (This should
	 * handle not touching files that are not edited but don't
	 * need keyword expansion.)
	 */
	unless (EDITED(s) || CSET(s)) {
		if (sccs_get(s, 0,0,0,0,
		    SILENT|GET_EDIT|GET_NOREGET, s->gfile, 0)) {
			fprintf(stderr, "%s: unable to edit %s\n", me, gfile);
			rc = 1;
			goto done;
		}
	}
	/*
	 * Handle ChangeSet files in nested environment.
	 */
	if ((!CSET(s) && (flags & COLLAPSE_DELTAS)) ||
	    (CSET(s) && proj_isProduct(0) && proj_isComponent(s->proj))) {
		/*
		 * We need to preseve all the deltas and just strip
		 * the existing cset merge.  This is either -d on the
		 * command line or done always for a component cset
		 * file.
		 */
		EACH(rmdeltas) {
			e = rmdeltas[i];
			FLAGS(s, e) &= ~D_CSET;
		}
		updatePending(s);
		sccs_newchksum(s);
		rc = 0;
		goto done;
	} else if (CSET(s) && proj_isComponent(0)) {
		/*
		 * This is a component's cset file when running from
		 * the component.  Here we don't want to allow the
		 * collapse unless the csets to be removed were
		 * pending in the product.
		 */
		EACH(rmdeltas) {
			e = rmdeltas[i];
			if (FLAGS(s, e) & D_CSET) {
				fprintf(stderr,
"%s: cannot strip component cset that are still part of product\n", me);
				goto done;
			}
		}
	}
	if (fix_setupcomments(s, rmdeltas)) goto done;

	if (tipd) {
		/* remember mode, path, xflags */
		mode = MODE(s, d);
		pathname = strdup(PATHNAME(s, d));
		xflags = XFLAGS(s, d);

		if (CSET(s) || streq(s->gfile, ATTR) || IS_POLYPATH(s->gfile)) {
			unlink(gfile);
		} else {
			gtime = s->gtime;
			savefile = aprintf("%s.fix.%u", gfile, getpid());
			rename(gfile, savefile);

			/* calculate any excluded revs */
			if (HAS_PFILE(s)) sccs_read_pfile(s, &pf);
			m = 0;
			if (pf.mRev) {
				m = sccs_findrev(s, pf.mRev);
				unless (m) goto done;
			}
			premap = sccs_set(s, d, m, pf.iLst, pf.xLst);
			free_pfile(&pf);
			EACH(rmdeltas) {
				e = rmdeltas[i];
				premap[e] = 0;
			}
			sccs_graph(s, tipd, premap, &inc, &exc);
			free(premap);
		}
		/* remove p.file */
		xfile_delete(gfile, 'p');
		xfile_delete(gfile, 'd');
		s->state &= ~S_PFILE;

		/* mark deltas to remove. */
		EACH(rmdeltas) {
			e = rmdeltas[i];
			FLAGS(s, e) |= D_SET;
		}
		range_markMeta(s);
		stripdel_fixTable(s, &i);

		if (sccs_stripdel(s, me)) {
			fprintf(stderr, "%s: stripdel of %s failed\n",
			    me, gfile);
			goto done;
		}
		/* branch might be tip */
		sys("bk", "-?BK_NO_REPO_LOCK=YES", "renumber", "-q", file, SYS);

		/* restore mode, path, xflags */
		tipd = sccs_findrev((s = sccs_reopen(s)), "+");
		unless (S_ISLNK(mode) || (mode == MODE(s, tipd)) || CSET(s)) {
			if (sccs_admin(s,
			    0, 0, 0, 0, 0, 0, mode2a(mode), 0)) {
				sccs_whynot(me, s);
				goto done;
			}
			tipd = sccs_findrev((s = sccs_reopen(s)), "+");
		}
		unless (streq(pathname, PATHNAME(s, tipd))) {
			if (sccs_adminFlag(s, ADMIN_NEWPATH)) {
				sccs_whynot(me, s);
				goto done;
			}
			tipd = sccs_findrev((s = sccs_reopen(s)), "+");
		}
		free(pathname);

		/* make xflags match */
		while (flagdiffs = (xflags ^ XFLAGS(s, tipd))) {
			/* pick right most bit */
			flagdiffs &= -flagdiffs;

			cmd = aprintf("bk -?BK_NO_REPO_LOCK=YES admin -%c%s '%s'",
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
		unless (CSET(s) || streq(s->gfile, ATTR) ||
		    IS_POLYPATH(s->gfile)) {
			rename(savefile, gfile);
			free(savefile);
			savefile = 0;
			if (sccs_get(s, "+", 0, inc, exc,
			    SILENT|GET_EDIT|GET_SKIPGET, 0, 0)) {
				fprintf(stderr, "%s: get -g %s failed\n",
				    me, gfile);
			}
			FREE(inc);
			FREE(exc);
			s->gtime = gtime;
		}
		sccs_setStime(s, 0);
	} else {
		char	*t;

		sccs_free(s);
		s = 0;

		/* delete the entire sfile, but save c.file */
		t = xfile_fetch(gfile, 'c');
		sfile_delete(0, gfile);
		if (t) {
			xfile_store(gfile, 'c', t);
			free(t);
		}
	}
	rc = 0;
 done:
	if (s) sccs_free(s);
	if (rmdeltas) free(rmdeltas);
	if (savefile) {
		rename(savefile, gfile);
		free(savefile);
	}
	free(sfile);
	free(gfile);
	return (rc);
}

/*
 * a callback used in do_file()/range_walkrevs() to collect all the deltas
 * to be removed from the tree.
 * We also check for deltas with CSET marks for the single file case.
 */
private int
savedeltas(sccs *s, ser_t d, void *data)
{
	ser_t	**rmdeltas = data;

	addArray(rmdeltas, &d);
	if (FLAGS(s, d) & D_CSET && (flags & COLLAPSE_FIX)) {
		fprintf(stderr, "%s: can't fix committed delta %s@%s\n",
		    me, s->gfile, REV(s, d));
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
fix_setupcomments(sccs *s, ser_t *rmdeltas)
{
	ser_t	d;
	int	i;
	char	**comments = 0;
	const	char *perr;
	int	poff;
	char	*cmts;
	char	*p;
	FILE	*f;
	pcre	*re;
	char	skippat[] =
	    "^Rename: .* ->|"
	    "^Merge rename: .* ->|"
	    "^Delete:|"
	    "^Change mode to|"
	    "^Turn o[nf]+ [A-Z0-9_]+ flag\n$|"
	    "^Auto merged\n$|"
	    "^SCCS merged\\.*\n$|"
	    "^[mM]erged*\\.*\n$|"  /* merge merged merge. merged. */
	    "^auto-union\n$";

	/* generate the list of delta comments we skip */
	unless (re = pcre_compile(skippat, 0, &perr, &poff, 0)) {
		fprintf(stderr, "%s: regex failed %s\npat = %s\n",
		    me, perr, skippat);
		return (1);
	}
	EACH (rmdeltas) {
		d = rmdeltas[i];

		if (d == TREE(d)) continue;
		unless (HAS_COMMENTS(s, d)) continue;

		/*
		 * If the comments are just one line and then match our
		 * pattern, then ignore these comments.
		 */
		cmts = COMMENTS(s, d);
		if ((strcnt(cmts, '\n') == 1) &&
		    !pcre_exec(re, 0, cmts, strlen(cmts), 0, 0, 0, 0)) {
			continue;
		}

		p = strdup(cmts);
		chomp(p);
		comments = addLine(comments, p);
	}
	if (p = xfile_fetch(s->gfile, 'c')) {
		chomp(p);
		comments = addLine(comments, strdup(p));
		free(p);
	}
	free(re);

	if (comments) {
		f = fmem();
		assert(f);
		EACH (comments) {
			if (i > 1) fputs("---\n", f);
			fputs(comments[i], f);
			fputc('\n', f);
			free(comments[i]);
		}
		freeLines(comments, 0);
		if (p = fmem_peek(f, 0)) {
			xfile_store(s->gfile, 'c', p);
		}
		fclose(f);
	}
	return (0);

}

/* Return the parent of the tip delta if the tip is not a merge. */
private ser_t
parent_of_tip(sccs *s)
{
	ser_t	d;

	d = sccs_findrev(s, "+");
	if (MERGE(s, d)) {
		fprintf(stderr,
		    "%s: Unable to fix just %s|%s, it is a merge.\n",
		    me, s->gfile, REV(s, d));
		return(0);
	}
	return (PARENT(s, d));
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
	MDBM	*idDB, *goneDB;
	hash	*h;
	FILE	*f = 0;
	char	buf[2*MAXKEY];

	cmd = aprintf("bk annotate -R'%s'..+ -h ChangeSet", rev);
	f = popen(cmd, "r");
	free(cmd);
	unless (f) goto out;
	unless (idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
		perror("idcache");
		goto out;
	}
	goneDB = loadDB(GONE, 0, DB_GONE);
	flist = addLine(0, strdup(CHANGESET));
	h = hash_new(HASH_MEMHASH);
	while (fnext(buf, f)) {
		unless (p = separator(buf)) continue;
		unless (hash_insert(h, buf, p-buf, 0, 0)) continue;
		*p = 0;
		if (p = key2path(buf, idDB, goneDB, 0)) {
			flist = addLine(flist, name2sccs(p));
			free(p);
		}
	}
	status = pclose(f);
	unless (WIFEXITED(status) && !WEXITSTATUS(status)) {
		freeLines(flist, free);
		flist = 0;
	}
	hash_free(h);
	mdbm_close(idDB);
	mdbm_close(goneDB);
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

	cmd = aprintf("bk sfio -omq > '%s'", file);
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

	get(COLLAPSED, SILENT|GET_EDIT);
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
	return (sys("bk", "-?BK_NO_REPO_LOCK=YES", "delta", "-aqy", COLLAPSED, SYS));
}
