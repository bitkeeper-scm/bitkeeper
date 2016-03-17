/*
 * Copyright 1999-2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"
#include "nested.h"
#include "range.h"
#include <time.h>

/*
 * commit options
 */
typedef struct {
	u32	force:1;
	u32	quiet:1;
	u32	resync:1;
	u32	standalone:1;
	u32	import:1;
	u32	clean_PENDING:1;	// if successful, clean PENDING marks
	u32	ci:1;			// do the ci/new as well if needed
} c_opts;


private int	do_commit(char **av, c_opts opts, char *sym,
			char *pendingFiles, char **modFiles, int dflags);
private int	do_ci(char ***modFiles, char *commentFile, char *pendingFiles);
private void	do_unmark(char **modFiles);
private	void	commitSnapshot(void);
private	void	commitRestore(int rc);

private	int	csetCreate(c_opts opts, sccs *cset, int flags,
    char *files, char **syms);

int
commit_main(int ac, char **av)
{
	FILE	*f, *fin;
	int	i, c, doit = 0;
	int	subCommit = 0;	/* commit called itself */
	char	**aliases = 0;
	char	*cmtsFile;
	char	*sym = 0;
	int	dflags = 0;
	char	**nav = 0;
	int	do_stdin;
	int	nested;		/* commiting all components? */
	c_opts	opts  = {0};
	char	pendingFiles[MAXPATH] = "";
	char	*cmd, *p, *bufp;
	char	**modFiles = 0;
	int	offset;
	longopt	lopts[] = {
		{ "standalone", 'S' },		/* new -S option */
		{ "subset", 's' },		/* aliases */

		{ "import", 290 },     /* part of an import */
		{ "tag:", 300 },		/* old -S option */
		{ "sub-commit", 301 },		/* calling myself */
		{ "ci", 302 },			/* ci/new as needed */
		{ 0, 0 }
	};
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "cdfFl:qRs|S|y:Y:", lopts)) != -1) {
		unless ((c == 's') || (c == 'S') || (c == 'Y')) {
			nav = bk_saveArg(nav, av, c);
		}
		switch (c) {
		    case 'c': dflags |= DELTA_CFILE; break;
		    case 'd': 
		    case 'f':
			doit = 1; break;			/* doc 2.0 */
		    case 'l':					/* doc */
			strcpy(pendingFiles, optarg); break;
		    case 'F':	opts.force = 1; break;		/* undoc */
		    case 'R':	opts.resync = 1;		/* doc 2.0 */
				break;
		    case 's':
			unless (optarg) {
				fprintf(stderr,
				    "bk %s -sALIAS: ALIAS "
				    "cannot be omitted\n", prog);
				return (1);
			}
			unless (aliases) {
				/* LM3DI: always commit product */
				aliases = addLine(aliases, strdup("PRODUCT"));
			}
			aliases = addLine(aliases, strdup(optarg));
			break;
		    case 'q':	opts.quiet = 1; break;		/* doc 2.0 */
		    case 'S':
			if (optarg) {
				fprintf(stderr, "%s: commit -S<tag> is now "
				    "commit --tag=<tag>\n", prog);
				return (1);
			}
			opts.standalone = 1;
			break;
		    case 'y':					/* doc 2.0 */
			dflags |= DELTA_DONTASK;
			if (comments_save(optarg)) return (1);
			break;
		    case 'Y':					/* doc 2.0 */
			/*
			 * Turn to a full path here and _then_ save
			 * it. This is so that the iterator below
			 * doesn't have relative path problems.
			 */
			unless (cmtsFile = fullname(optarg, 0)) {
				fprintf(stderr,
				    "%s: can't read comments from %s\n",
				    prog, optarg);
				exit(1);
			}
			optarg = cmtsFile;
			nav = bk_saveArg(nav, av, c);
			if (comments_savefile(optarg)) {
				fprintf(stderr,
				    "commit: can't read comments from %s\n",
				    optarg);
				exit(1);
			}
			dflags |= DELTA_DONTASK;
			free(cmtsFile);
			break;
		    case 290:	/* --import */
			opts.import = 1;
			break;
		    case 300:	/* --tag=TAG */
			sym = optarg;
			if (sccs_badTag(sym, ADMIN_NEWTAG)) exit (1);
			break;
		    case 301:	/* --sub-commit */
			opts.standalone = subCommit = 1;
			break;
		    case 302:	/* --ci */
			opts.ci = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}

	trigger_setQuiet(opts.quiet);

	nested = bk_nested2root(opts.standalone);
	if (opts.standalone && aliases) {
		fprintf(stderr, "%s: options -S and -sALIAS "
		    "cannot be combined.\n", prog);
		return (1);
	}
	if (aliases && !nested) {
		EACH(aliases) {
			unless (streq(aliases[i], ".") ||
			    strieq(aliases[i], "HERE") ||
			    strieq(aliases[i], "PRODUCT")) {
				fprintf(stderr,
				    "%s: -sALIAS only allowed in product\n",
				    prog);
				return (1);
			}
		}
		freeLines(aliases, free);
		aliases = 0;
	}
	if (opts.import && opts.resync) usage();
	if (sym && proj_isComponent(0)) {
		unless (subCommit) {
			fprintf(stderr,
			    "%s: component tags not yet supported.\n", prog);
			return (1);
		}
		sym = 0;
	}
	T_PERF("start");

	do_stdin = (av[optind] && streq("-", av[optind]));
	if (nested) {
		int	rc;

		if (pendingFiles[0] || do_stdin) {
			fprintf(stderr,
			    "%s: Must use -S with -l or \"-\"\n", prog);
			return (1);
		}
		cmdlog_lock(CMD_NESTED_WRLOCK);
		nav = unshiftLine(nav, strdup("--sub-commit"));
		nav = unshiftLine(nav, strdup("commit"));
		nav = unshiftLine(nav, strdup("bk"));

		unless (aliases) aliases = modified_pending(opts.ci ?
		    DS_PENDING|DS_EDITED : DS_PENDING);
		rc = nested_each(opts.quiet, nav, aliases);
		freeLines(aliases, free);
		freeLines(nav, free);
		if (rc) {
			fprintf(stderr,
			    "%s: failed to commit some components\n", prog);
		}
		return (rc);
	}

	unless (opts.resync) cmdlog_lock(CMD_WRLOCK|CMD_NESTED_WRLOCK);

	if (pendingFiles[0] && do_stdin) {
		fprintf(stderr, "commit: can't use -l when using \"-\"\n");
		return (1);
	}
	f = fmem();
	assert(f);
	if (pendingFiles[0] || do_stdin) {

		unless (dflags & (DELTA_DONTASK|DELTA_CFILE)) {
			fprintf(stderr,
			    "You must use one of the -c, -Y or -y "
			    "options when using \"-\"\n");
			return (1);
		}
		if (pendingFiles[0]) {
			unless (fin = fopen(pendingFiles, "r")) {
				perror(pendingFiles);
				return (1);
			}
		} else {
			fin = stdin;
		}
		setmode(0, _O_TEXT);
		while (bufp = fgetline(fin)) {
			if (strstr(bufp, "SCCS/s.") == 0) {
				bufp = name2sccs(bufp);
				fprintf(f, "%s\n", bufp);
				free(bufp);
			} else {
				fprintf(f, "%s\n", bufp);
			}
		}
		if (fin != stdin) fclose(fin);
		offset = 0;
	} else {
		cmd = aprintf("bk gfiles -v%s%spC",
		    opts.resync ? "r" : "",
		    opts.ci ? "c" : "");
		fin = popen(cmd, "r");
		assert(fin);
		while (bufp = fgetline(fin)) {
			fprintf(f, "%s\n", bufp);
		}
		if (pclose(fin)) {
			fclose(f);
			free(cmd);
			getMsg("duplicate_IDs", 0, 0, stdout);
			return (1);
		}
		free(cmd);
		opts.clean_PENDING = 1;
		offset = 8;
	}
	// parse fmem
	fin = f;
	rewind(fin);
	bktmp(pendingFiles);
	f = fopen(pendingFiles, "w");
	assert(f);
	while (bufp = fgetline(fin)) {
		p = strchr(bufp+offset, '|');
		if (((offset == 8) && bufp[2] == 'c' && opts.ci) || (p == 0)) {
			if (p) *p = 0;
			modFiles = addLine(modFiles,
			    strdup(bufp+offset));
		} else {
			fprintf(f, "%s\n", bufp+offset);
		}
	}
	fclose(fin); fclose(f);
	if (!opts.force && (size(pendingFiles) == 0 && modFiles == 0)) {
		unless (opts.quiet) fprintf(stderr, "Nothing to commit\n");
		unlink(pendingFiles);
		return (0);
	}
	/*
	 * Auto pickup a c.ChangeSet unless they already gave us comments.
	 * Prompt though, that's what we do in delta.
	 */
	unless (dflags & (DELTA_DONTASK|DELTA_CFILE)) {
		if (xfile_exists(CHANGESET, 'c')) {
			char	*t;

			bktmp_local(buf);
			t = xfile_fetch(CHANGESET, 'c');
			Fprintf(buf, "%s", t);
			free(t);
			if (!doit && comments_prompt(buf)) {
				fprintf(stderr, "Commit aborted.\n");
				return (1);
			}
			t = loadfile(buf, 0);
			xfile_store(CHANGESET, 'c', t);
			free(t);
			unlink(buf);
			dflags |= DELTA_CFILE;
		}
	}

	unless (dflags & (DELTA_DONTASK|DELTA_CFILE)) {
		char	*cmd, *p;
		FILE	*f, *f1;
		char	commentFile[MAXPATH];
		char	*line;

		bktmp(commentFile);
		f = popen("bk cat BitKeeper/templates/commit", "r");
		assert(f);
		if (fnext(buf, f)) {
			f1 = fopen(commentFile, "w");
			fputs(buf, f1);
			while (fnext(buf, f)) {
				fputs(buf, f1);
			}
			fclose(f1);
		}
		pclose(f);
		cmd = aprintf("bk sort -u | "
			"bk sccslog -DA - >> '%s'", commentFile);
		f = popen(cmd, "w");
		f1 = fopen(pendingFiles, "rt");
		assert(f); assert (f1);
		while (line = fgetline(f1)) {
			p = strrchr(line, BK_FS);
			assert(p);
			*p = 0;
			fputs(line, f);
			fputs("\n", f);
		}
		fclose(f1);
		pclose(f);
		free(cmd);

		if (!doit && comments_prompt(commentFile)) {
			fprintf(stderr, "Commit aborted.\n");
			unlink(pendingFiles);
			unlink(commentFile);
			return (1);
		}
		dflags |= DELTA_DONTASK;
		if (comments_savefile(commentFile)) return (1);
		unlink(commentFile);
	}
	unlink("ChangeSet");
	T_PERF("before do_commit");
	return (do_commit(av, opts, sym, pendingFiles, modFiles, dflags));
}

/*
 * Return the md5keys for all of the components that have 'flags'
 * set in the scanComps file.
 */
char **
modified_pending(u32 flags)
{
	int	i;
	int	product = 0;
	char	**dirs;
	project	*p;
	char	**aliases = 0;
	char	buf[MAXPATH];

	unless (dirs = proj_scanComps(0, flags)) return (0);
	EACH(dirs) {
		concat_path(buf, proj_root(proj_product(0)), dirs[i]);
		unless (p = proj_init(buf)) {
			unless (getenv("_BK_DEVELOPER")) continue;
			die("%s should be a repo", dirs[i]);
		}
		// Ah, I see.  If we have a standalone don't
		// add it.
		unless (proj_isEnsemble(p)) {
			if (getenv("_BK_DEVELOPER")) {
				die("standalone %s\n", dirs[i]);
			}
			proj_free(p);
			continue;
		}
		if (proj_isProduct(p)) product = 1;
		aliases = addLine(aliases, strdup(proj_md5rootkey(p)));
		T_NESTED("SCAN %s", dirs[i]);
		proj_free(p);
	}
	unless (product) {
		aliases = addLine(aliases, strdup("PRODUCT"));
		T_NESTED("SCAN PRODUCT");
	}
	freeLines(dirs, 0);
	return (aliases);
}

private int
do_commit(char **av,
	c_opts opts, char *sym, char *pendingFiles,
	char **modFiles, int dflags)
{
	int	rc, i;
	int	ntc = 0;	// nothing to commit
	sccs	*cset;
	char	**syms = 0;
	char	*p;
	FILE 	*f, *f2;
	char	*t;
	char	**list = 0;
	char	commentFile[MAXPATH] = "";
	char	pendingFiles2[MAXPATH];
	char	buf[MAXLINE];

	cset = sccs_csetInit(0);
	(void)sccs_defRootlog(cset);	/* if no rootlog, make one */
	commitSnapshot();
	if (!opts.resync && (rc = attr_update())) {
		if (rc < 0) {
			rc = 1;
			goto done;
		}
		bktmp(pendingFiles2);
		f = fopen(pendingFiles, "r");
		f2 = fopen(pendingFiles2, "w");
		i = strlen(ATTR);
		while (t = fgetline(f)) {
			if (begins_with(t, ATTR "|") ||
			    begins_with(t, SATTR "|")) {
				/* skip ATTR file */
				continue;
			}
			fprintf(f2, "%s\n", t);
		}
		fclose(f);
		fprintf(f2, "%s%c+\n", ATTR, BK_FS);
		fclose(f2);
		if (unlink(pendingFiles)) perror(pendingFiles);
		fileMove(pendingFiles2, pendingFiles);
	}
	/*
	 * XXX Do we want to fire the trigger when we are in RESYNC ?
	 */
	safe_putenv("BK_PENDING=%s", pendingFiles);

	/* XXX could avoid if we knew if a trigger would fire... */
	bktmp(commentFile);
	if (dflags & DELTA_CFILE) {
		unless (p = xfile_fetch(cset->sfile, 'c')) {
			fprintf(stderr, "commit: saved comments not found.\n");
			rc = 1;
			goto done;
		}
		cset->used_cfile = 1;
		f = fopen(commentFile, "w");
		fputs(p, f);
		free(p);
		fclose(f);
	} else {
		comments_writefile(commentFile);
	}
	safe_putenv("BK_COMMENTFILE=%s", commentFile);

	if (modFiles && (rc = do_ci(&modFiles, commentFile, pendingFiles))) {
		rc = 1;
		do_unmark(modFiles);
		goto done;
	}
	if (!opts.force && (size(pendingFiles) == 0 && modFiles == 0)) {
		unless (opts.quiet) fprintf(stderr, "Nothing to commit\n");
		ntc = 1;
		rc = 0;
		goto done;
	}

	if (rc = trigger(opts.resync ? "merge" : av[0], "pre")) goto done;
	comments_done();
	if (comments_savefile(commentFile)) {
		rc = 1;
		if (modFiles) do_unmark(modFiles);
		goto done;
	}
	if (opts.quiet) dflags |= SILENT;
	if (sym) syms = addLine(syms, strdup(sym));
	if (f = fopen("SCCS/t.ChangeSet", "r")) {
		while (fnext(buf, f)) {
			chop(buf);
			syms = addLine(syms, strdup(buf));
		}
		fclose(f);
		unlink("SCCS/t.ChangeSet");
	}

	/*
	 * I don't think it makes sense to prevent tags in the RESYNC tree.
	 * We need them to handle the tag merge.
	 * If we really want to prevent them then I think we need a way of
	 * listing them when we are at the pre-resolve stage so that a trigger
	 * could be written which detects that and fails the resolve.
	 */
	unless (opts.resync) {
		EACH (syms) {
			safe_putenv("BK_TAG=%s", syms[i]);
			rc = trigger("tag", "pre");
			switch (rc) {
			    case 0: break;
			    case 2:
				removeLineN(syms, i, free);
				/* we left shifted one, go around again */
				i--;
				break;
			    default: goto done;
			}
		}
	}
	rc = csetCreate(opts, cset, dflags, pendingFiles, syms);
	if (rc) goto fail;
	if (opts.import) {
		/*
		 * We are skipping the check, but still need to
		 * mark sfiles.
		 */
		sccs	*s;
		ser_t	d;
		char	**marklist = 0;
		hash	*h = hash_new(HASH_MEMHASH);

		EACH(modFiles) {
			/*
			 * If we delta'ed the file, then the files
			 * already have cset marks and can be skipped.
			 * Remember these files.
			 */
			p = modFiles[i];
			t = strchr(p, '|');
			assert(t);
			*t = 0;
			hash_insertStrSet(h, p);
			*t = '|';
		}
		marklist = file2Lines(0, pendingFiles);
		EACH(marklist) {
			p = marklist[i];
			t = strchr(p, '|');
			*t++ = 0;
			if (hash_fetchStr(h, p)) continue; /* already marked */
			s = sccs_init(p, INIT_MUSTEXIST);
			d = sccs_findrev(s, t);
			assert(!(FLAGS(s, d) & D_CSET));
			FLAGS(s, d) |= D_CSET;
			sccs_newchksum(s);
			sccs_free(s);
		}
		freeLines(marklist, free);
		hash_free(h);
		if (bin_needHeapRepack(cset)) {
			if (run_check(opts.quiet, 0, 0, 0, 0)) {
				rc = 1;
				goto fail;
			}
		}
	} else {
		// run check
		/*
		 * Note: by having no |+, the check.c:check() if (doMark...)
		 * code will skip trying to mark the ChangeSet file.
		 * Every other file will have a |<rev> and so will get marked.
		 */
		f = fopen(pendingFiles, "a");
		fprintf(f, CHANGESET "\n");
		fclose(f);
		if (sysio(pendingFiles, 0, 0,
		    "bk", "-?BK_NO_REPO_LOCK=YES", "check",
		    opts.resync ? "-cMR" : "-cM", "-", SYS)) {
			rc = 1;
			goto fail;
		}
	}
	T_PERF("after_check");
	if (opts.resync) {
		char	key[MAXPATH];
		FILE	*f;

		/*
		 * We created a commit in the RESYNC directory,
		 * probably a merge cset closing the open tips of the
		 * ChangeSet file. Log it in BitKeeper/etc/csets-in so
		 * that 'bk abort' can pick it up if we fail.
		 */
		sccs_sdelta(cset, sccs_top(cset), key);
		if (f = fopen(CSETS_IN, "a")) {
			fprintf(f, "%s\n", key);
			fclose(f);
		}
	}
	if (proj_isComponent(0)) {
		hash	*urllist;
		char	*file = proj_fullpath(proj_product(0), NESTED_URLLIST);

		/*
		 * Created a new cset for this component, the saved URLs
		 * for this component are now all invalid.
		 */
		if (urllist = hash_fromFile(0, file)) {
			hash_deleteStr(urllist, proj_rootkey(0));
			if (hash_toFile(urllist, file)) perror(file);
			hash_free(urllist);
		}
	}
	putenv("BK_STATUS=OK");
fail:	if (rc) {
		if (modFiles) do_unmark(modFiles);
		fprintf(stderr, "The commit is aborted.\n");
		putenv("BK_STATUS=FAILED");
	} else if (opts.clean_PENDING) {
		proj_dirstate(0, "*",
		    opts.ci ? DS_PENDING|DS_EDITED : DS_PENDING, 0);
		if (proj_isComponent(0)) {
			/* this component is still pending */
			proj_dirstate(0, ".", DS_PENDING, 1);
		}
	}
	trigger(opts.resync ? "merge" : av[0], "post");
done:	if (unlink(pendingFiles)) perror(pendingFiles);
	// Someone else created the c.file, unlink only on success
	if ((dflags & DELTA_CFILE) && !rc && !ntc) comments_cleancfile(cset);
	if (*commentFile) unlink(commentFile);
	sccs_free(cset);
	freeLines(list, free);
	commitRestore(rc);
	freeLines(modFiles, free);

	T_PERF("done");
	return (rc);
}

private int
do_ci(char ***modFiles, char *commentFile, char *pendingFiles)
{
	int	i, rc = 0;
	char	*cmd, **mlist = *modFiles;
	FILE	*f;
	char	didciFile[MAXPATH];

	bktmp(didciFile);
	cmd = aprintf("bk -?BK_NO_REPO_LOCK=YES ci -q -a "
	    "--prefer-cfile --csetmark --did-ci=\"%s\" -Y\"%s\" -",
	    didciFile, commentFile);

	f = popen(cmd, "w");
	assert(f);
	EACH(mlist) fprintf(f, "%s\n", mlist[i]);
	rc = (pclose(f) != 0);
	free(cmd);

	freeLines(mlist, free);
	mlist = file2Lines(0, didciFile);
	*modFiles = mlist;
	unlink(didciFile);

	f = fopen(pendingFiles, "a");
	assert(f);
	EACH(mlist) fprintf(f, "%s\n", mlist[i]);
	fclose(f);

	return (rc);
}


/*
 * Read in sfile and extend the cset weave
 */
private int
getfilekey(char *gfile, char *rev, sccs *cset, ser_t cset_d, char ***keys)
{
	sccs	*s;
	ser_t	d;
	u8	buf[MAXLINE];

	unless (s = sccs_init(gfile, 0)) {
		fprintf(stderr, "cset: can't init %s\n", gfile);
		return (1);
	}

	/*
	 * Only component cset files get added to the weave
	 */
	if (CSET(s) && !proj_isComponent(s->proj)) {
		sccs_free(s);
		return (0);
	}
	unless (d = sccs_findrev(s, rev)) {
		fprintf(stderr, "cset: can't find %s in %s\n", rev, gfile);
		return (1);
	}
	//assert(!(FLAGS(s, d) & D_CSET));

	sccs_sdelta(s, sccs_ino(s), buf);
	*keys = addLine(*keys, strdup(buf));
	sccs_sdelta(s, d, buf);
	*keys = addLine(*keys, strdup(buf));
	sccs_free(s);
	return (0);
}

/*
 * If lots of gone open tips, then we'll want to keep the gone DB open.
 * Similar logic to open tip error handling in check.c:buildKeys().
 */
private	int
missingMerge(sccs *cset, u32 rkoff)
{
	MDBM	*goneDB = loadDB(GONE, 0, DB_GONE);
	int	ret = 0;

	unless (mdbm_fetch_str(goneDB, HEAP(cset, rkoff))) {
		fprintf(stderr,
		    "commit: ChangeSet is a merge but is missing\n"
		    "a required merge delta for this rootkey\n");
		fprintf(stderr, "\t%s\n", HEAP(cset, rkoff));
		ret = 1;
	}
	mdbm_close(goneDB);
	return (ret);
}

/*
 * We are about to create a new cset 'd' with 'keys' in the weave.
 * This function computes the new SUM() for 'd'.  The changeset checksum
 * is the sum of the list of all rk/dk pairs for the current tip, so
 * we need to add the new files and then for updated files subtract the
 * old keys and add the new keys.
 *
 * SUM(cset, d) has been preset to the PARENTs checksum.  That's why
 * the MERGE case is asymmetrical - scanning RED for new and BLUE for
 * matches.
 *
 * Where possible we walk as little of the existing weave as possible to get
 * the data needed.
 *
 * hash: keys are rootkey offset in heap; values are what region
 * we were in when we first saw it, and an index to change the deltakey,
 * if it is the oldest.  If we first see in one side of the merge,
 * then again in the other side of the merge, without being in the merge,
 * then report error.
 */
private int
updateCsetChecksum(sccs *cset, ser_t d, char **keys)
{
	int	i, cnt = 0, todo = 0;
	u32	sum = 0;
	u32	seen;
	char	*rk, *dk;
	u32	rkoff, dkoff;
	u8	*p;
	ser_t	e, old;
	int	ret = 0;
	int	merge = 0;
	struct	{
		u32	n;
		u8	seen;
#define	S_REMOTE	0x01	/* first seen in remote */
#define	S_LOCAL		0x02	/* first seen in local */
#define	S_DONE		0x04	/* not looking for this key anymore */
#define	S_INREMOTE	0x08	/* seen in remote */
	} *rinfo;
	hash	*h = hash_new(HASH_U32HASH, sizeof(u32), sizeof(*rinfo));

	if (MERGE(cset, d)) {
		/*
		 * Add in some MERGE side keys (colored RED)
		 * to todo list.  Also color PARENT side BLUE and
		 * use the hash both for computing what csets in RED
		 * to add to todo, as well as look for non merged tips.
		 */
		cset->rstart = 0;
		range_walkrevs(cset,
		    L(PARENT(cset, d)), L(MERGE(cset, d)), WR_EITHER, 0, 0);
		if (cset->rstart) {
			merge = 1;
			todo++;
		}
	}
	EACH(keys) {
		++cnt;
		rk = keys[i++];
		dk = keys[i];
		for (p = dk; *p; p++) sum += *p; /* sum of new deltakey */
		if (rkoff = sccs_hasRootkey(cset, rk)) {
			++todo;
			rinfo = hash_insert(h, &rkoff, sizeof(rkoff),
			    0, sizeof(*rinfo));
			unless (rinfo) {
				fprintf(stderr,
				    "ERROR: same rootkey appears twice:\n%s\n",
				    rk);
				ret++;
				rinfo = h->vptr;
			}
			rinfo->n = i;
		} else {
			/* Should be truly new, so do now and call done */
			rkoff = sccs_addUniqRootkey(cset, rk);
			rinfo = hash_insert(h, &rkoff, sizeof(rkoff),
			    0, sizeof(*rinfo));
			assert(rkoff && rinfo);
			rinfo->seen = S_DONE;	/* don't checksum again */
			/* new file, just add rk now */
			for (p = rk; *p; p++) sum += *p;
			sum += ' ' + '\n';
			dk = aprintf("|%s", keys[i]);
			free(keys[i]);
			keys[i] = dk;
		}
	}
	sccs_rdweaveInit(cset);
	old = d;
	seen = 0;
	while (todo && (e = cset_rdweavePair(cset, 0, &rkoff, &dkoff))) {
		if (merge && (old != e)) {
			switch (FLAGS(cset, e) & (D_RED|D_BLUE)) {
			    case D_RED: seen = S_REMOTE; break;
			    case D_BLUE: seen = S_LOCAL; break;
			    default: seen = 0; break;
                        }
			/* clear colors for this cset and csets with no data */
			for (old--; old >= e; old--) {
				FLAGS(cset, old) &= ~(D_RED|D_BLUE);
			}
			old = e;
			if (e < cset->rstart) {
				/* all clean and done with merge */
				merge = 0;
				--todo;
			}
		}
		/* allocate rinfo */
		if (merge) {
			if (rinfo = hash_insert(h, &rkoff, sizeof(rkoff),
				0, sizeof(*rinfo))) {
				/* new rk first seen in merge (not in commit) */

				assert(dkoff);
				if (seen & S_REMOTE) {
					/* must be a delta on remote only */
					rinfo->seen = S_REMOTE;
					p = HEAP(cset, dkoff);
					while (*p) sum += *p++;
					++todo;
				} else {
					rinfo->seen = seen | S_DONE;
				}
			}
			rinfo = h->vptr;
			/* in one region and first seen in the other region */
			if (!ret &&
			    (((seen|rinfo->seen) & (S_LOCAL|S_REMOTE)) ==
				(S_LOCAL|S_REMOTE))) {
				/*
				 * Not in commit, but included in both
				 * local and remote side of merge
				 */
				ret = missingMerge(cset, rkoff);
			}
		} else {
			unless (rinfo = hash_fetch(h, &rkoff, sizeof(rkoff))) {
				// don't care about this key
				continue;
			}
		}
		if (rinfo->seen & S_DONE) continue; /* done with this rk */

		if (seen & S_REMOTE) {
			if (!dkoff) {
				/* new file */
				p = HEAP(cset, rkoff);
				while (*p) sum += *p++;
				sum += ' ' + '\n';

				rinfo->seen |= S_DONE;
				--todo;
			} else {
				// maybe a new file
				rinfo->seen |= S_INREMOTE;
				rinfo->n = e;
			}
		} else {
			assert(dkoff);
			/*
			 * found previous deltakey for one of my files,
			 * subtract off old key
			 */
			for (p = HEAP(cset, dkoff); *p; sum -= *p++);
			--todo;
			rinfo->seen |= S_DONE;
		}
	}
	sccs_rdweaveDone(cset);
	/*
	 * Any keys that remain in my hash must be new files so we
	 * need to add in the rootkey checksums.
	 */
	if (todo) EACH_HASH(h) {
		rkoff = *(u32 *)h->kptr;
		rinfo = h->vptr;
		if (rinfo->seen & S_DONE) continue;

		for (p = HEAP(cset, rkoff); *p; p++) sum += *p;
		sum += ' ' + '\n';
		assert(rinfo->n);
		if (rinfo->seen & S_INREMOTE) {
			/*
			 * file from weave was actually new
			 * Could update markers, but better to flush out
			 * when this happens and plug the holes.
			 */
			if (BWEAVE3(cset) && getenv("_BK_DEVELOPER")) {
				fprintf(stderr,
				    "serial =%d, oldest rk %s\n",
				    rinfo->n, HEAP(cset, rkoff));
				ret++;
			}
		} else {
			/* committed file was really new */
			i = rinfo->n;
			dk = aprintf("|%s", keys[i]);
			free(keys[i]);
			keys[i] = dk;
		}
		if (--todo == 0) break;
	}
	hash_free(h);

	/* update delta checksum */
	if (!cnt && !MERGE(cset, d) && !HAS_CLUDES(cset, d)) {
		sum = almostUnique();
	} else {
		sum = (sum_t)(SUM(cset, d) + sum);
	}
	SUM_SET(cset, d, sum);
	SORTSUM_SET(cset, d, sum);
	return (ret);
}

/*
 * Read file|rev from stdin and apply those to the changeset db.
 * Edit the ChangeSet file and add the new stuff to that file and
 * leave the file sorted.
 * Close the cset sccs* when done.
 */
private ser_t
mkChangeSet(c_opts opts, sccs *cset, char *files, char ***keys)
{
	ser_t	d, d2, p;
	char	*line, *rev, *t;
	FILE	*f;
	pfile	pf;
	char	buf[MAXLINE];

	/*
	 * Edit the ChangeSet file - we need it edited to modify it as well
	 */
	if (LOCKED(cset)) {
		if (sccs_read_pfile(cset, &pf)) return (0);
	} else {
		memset(&pf, 0, sizeof(pf));
		pf.oldrev = strdup("+");
	}
	d = sccs_dInit(0, 'D', cset, 0);
	if (d == TREE(cset)) {
		if (t = getenv("BK_RANDOM")) {
			strcpy(buf, t);
		} else {
			randomBits(buf);
		}
		RANDOM_SET(cset, d, buf);

		/* a rootkey can't have a realuser or realhost */
		sccs_parseArg(cset, d, 'U', sccs_getuser(), 0);
		sccs_parseArg(cset, d, 'H', sccs_gethost(), 0);

		/* nor a pathname for a component */
		sccs_parseArg(cset, d, 'P', "ChangeSet", 0);

		t = fullname(cset->gfile, 0);
		sprintf(buf, "BitKeeper file %s\n", t);
		free(t);
		COMMENTS_SET(cset, d, buf);

		cset->bitkeeper = 1;
		XFLAGS(cset, d) |= X_REQUIRED|X_LONGKEY;
		R0_SET(cset, d, 1);

		SUM_SET(cset, d, almostUnique());
	} else {
		p = sccs_findrev(cset, pf.oldrev);
		assert(p);
		PARENT_SET(cset, d, p);
		R0_SET(cset, d, R0(cset, p));	/* so renumber() is happy */
		XFLAGS(cset, d) = XFLAGS(cset, p);

		/*
		 * set initial sum to parent, in updateCsetChecksum we update
		 */
		if (d2 = sccs_getCksumDelta(cset, p)) {
			SUM_SET(cset, d, SUM(cset, d2));
		} else {
			SUM_SET(cset, d, 0);
		}

		/*
		 * bk normally doesn't set the MODE() for the
		 * ChangeSet file so the following line usually just
		 * sets MODE=0.  But on some repos like bk source we
		 * do have a mode and this propagates the existing
		 * value.
		 */
		MODE_SET(cset, d, MODE(cset, p));
	}
	SORTSUM_SET(cset, d, SUM(cset, d));

	if (sccs_setCludes(cset, d, pf.iLst, pf.xLst)) {
		fprintf(stderr, "%s: bad iLst in pfile\n", prog);
		free_pfile(&pf);
		return (0);
	}
	if (pf.mRev) {
		p = sccs_findrev(cset, pf.mRev);
		MERGE_SET(cset, d, p);
	}
	free_pfile(&pf);


	if (files) {
		/*
		 * Read each file|rev from files and add that to the cset.
		 * getfilekey() will ignore the ChangeSet entry itself.
		 */
		f = fopen(files, "rt");
		assert(f);
		while (line = fgetline(f)) {
			rev = strrchr(line, '|');
			*rev++ = 0;
			getfilekey(line, rev, cset, d, keys);
		}
		fclose(f);
		if (updateCsetChecksum(cset, d, *keys)) return (0);
	}

	if (d == TREE(cset)) {
		// set the CSETFILE backpointer for the 1.0 delta
		sccs_sdelta(cset, d, buf);
		sccs_parseArg(cset, d, 'B', buf, 0);
	} else {
		SAME_SET(cset, d, 1);
	}

	if (TABLE(cset) && (DATE(cset, d) <= DATE(cset, TABLE(cset)))) {
		time_t	tdiff;

		tdiff = DATE(cset, TABLE(cset)) - DATE(cset, d) + 1;
		DATE_SET(cset, d, DATE(cset, d) + tdiff);
		DATE_FUDGE_SET(cset, d, DATE_FUDGE(cset, d) + tdiff);
	}
#ifdef CRAZY_WOW
	Actually, this isn't so crazy wow.  I don't know what problem this
	caused but I believe the idea was that we wanted time increasing
	across all deltas in all files.  Sometimes the ChangeSet timestamp
	is behind the deltas in that changeset which is clearly wrong.

	Proposed fix is to record the highest fudged timestamp in global
	file in the repo and make sure the cset file is always >= that one.
	Should be done in the proj struct and written out when we free it
	if it changed.

	/*
	 * Adjust the date of the new rev, scripts can make this be in the
	 * same second.  It's OK that we adjust it here, we are going to use
	 * this delta * as part of the checkin on this changeset.
	 */
	if (DATE(cset, d) <= DATE(cset, table)) {
		DATE_FUDGE(cset, d) =
		    (DATE(cset, table) - DATE(csets, d)) + 1;
		DATE_SET(cset, d, (DATE(cset, d) + DATE_FUDGE(cset, d)));
	}
#endif
	if (uniq_adjust(cset, d)) return (0);
	return (d);
}

private int
csetCreate(c_opts opts, sccs *cset, int flags, char *files, char **syms)
{
	ser_t	d;
	int	i, error = 0;
	int	fd0;
	char	*line;
	char	**keys = 0;
	FILE	*out;

	T_PERF("csetCreate");

	if ((TABLE(cset) + 1 > 200) && getenv("BK_REGRESSION")) {
		fprintf(stderr, "Too many changesets for regressions.\n");
		exit(1);
	}

	/* write change set to diffs */
	unless (d = mkChangeSet(opts, cset, files, &keys)) {
		freeLines(keys, free);
		return (-1);
	}

	/* for compat with old versions of BK not using ensembles */
	if (proj_isComponent(cset->proj)) {
		updatePending(cset);
	} else {
		if (d > TREE(cset)) FLAGS(cset, d) |= D_CSET;
	}

	/*
	 * Make /dev/tty where we get input.
	 * XXX This really belongs in port/getinput.c
	 *     We shouldn't do this if we are not getting comments
	 *     interactively.
	 */
	fd0 = dup(0);
	close(0);
	if (open(DEV_TTY, 0, 0) < 0) {
		dup2(fd0, 0);
		close(fd0);
		fd0 = -1;
	}
	if ((flags & (DELTA_DONTASK|DELTA_CFILE)) &&
	    !(d = comments_get(0, 0, cset, d))) {
		error = -1;
		goto out;
	}
	if (fd0 >= 0) {
		dup2(fd0, 0);
		close(fd0);
		fd0 = -1;
	}
	sccs_insertdelta(cset, d, d);
	sccs_renumber(cset, 0);
	sccs_startWrite(cset);
	weave_set(cset, d, keys);
	EACH (syms) addsym(cset, d, 1, syms[i]);
	if (delta_table(cset, 0)) {
		perror("table");
		error = -1;
		goto out;
	}
	unless (BWEAVE_OUT(cset)) {
		out = sccs_wrweaveInit(cset);
		sccs_rdweaveInit(cset);
		while (line = sccs_nextdata(cset)) {
			fputs(line, out);
			fputc('\n', out);
		}
		sccs_rdweaveDone(cset);
		sccs_wrweaveDone(cset);
	}
	if (sccs_finishWrite(cset)) {
		error = -1;
		goto out;
	}
	T_PERF("wrote weave");
	unlink(cset->gfile);
	xfile_delete(cset->gfile, 'p');
	cset->state &= ~(S_GFILE|S_PFILE);

out:	unless (error || (flags & SILENT)) {
		fprintf(stderr, "ChangeSet revision %s: +%d\n",
		    REV(cset, cset->tip), nLines(keys)/2);
	}
	freeLines(keys, free);
	comments_done();
	return (error);
}

int
cset_setup(int flags)
{
	sccs	*cset;
	int	rc;
	c_opts	opts = {0};

	cset = sccs_csetInit(SILENT);
	assert(cset->state & S_CSET);
	cset->xflags |= X_LONGKEY;
	rc = csetCreate(opts, cset, flags, 0, 0);
	sccs_free(cset);
	return (rc);
}

#define	CSET_BACKUP	"BitKeeper/tmp/SCCS/commit.cset.backup"

static	char	*save_pfile;
static	char	*save_cfile;

/*
 * Save SCCS/?.ChangeSet files so we can restore them later if commit
 * fails.
 */
private void
commitSnapshot(void)
{
	char	*t;

	/*
	 * We don't save a backup of the ChangeSet heaps in
	 * [12].ChangeSet.  Since we will only append to these files
	 * if we rollback the s.ChangeSet file everything will still
	 * work.
	 *
	 * Saving a hardlink back wouldn't work because bk would copy
	 * the file when it need to append new data to the end.
	 */
	mkdir("BitKeeper/tmp/SCCS", 0777);
	fileLink("SCCS/s.ChangeSet", CSET_BACKUP ".s");
	if (t = xfile_fetch(CHANGESET, 'p')) save_pfile = t;
	if (t = xfile_fetch(CHANGESET, 'c')) save_cfile = t;
	if (exists(SATTR)) fileLink(SATTR, CSET_BACKUP "attr.s");
}

/*
 * If rc!=0, then commit failed and we need to restore the state saved by
 * commitSnapshot().  Otherwise delete that state.
 */
private void
commitRestore(int rc)
{
	char	save[MAXPATH];

	if (rc) {
		if (exists(CSET_BACKUP ".s")) {
			fileMove(CSET_BACKUP ".s", "SCCS/s.ChangeSet");
		}
		unlink("BitKeeper/log/TIP");
		if (save_pfile) xfile_store(CHANGESET, 'p', save_pfile);
		if (save_cfile) xfile_store(CHANGESET, 'c', save_cfile);
	} else {
		unlink(CSET_BACKUP ".s");
	}
	FREE(save_pfile);
	FREE(save_cfile);

	strcpy(save, CSET_BACKUP "attr.s");
	if (exists(save)) {
		if (rc) {
			fileMove(save, SATTR);
		} else {
			unlink(save);
		}
	}
}

private void
do_unmark(char **modFiles)
{
	int	i;
	sccs	*s;
	ser_t	d;
	char	*rev, *p;

	EACH(modFiles) {
		p = strchr(modFiles[i], '|');
		assert(p);
		*p = 0;
		rev = p+1;
		unless (s = sccs_init(modFiles[i], INIT_MUSTEXIST)) {
			perror(modFiles[i]);
			continue;
		}
		d = sccs_findrev(s, rev);
		if (FLAGS(s, d) & D_CSET) {
			FLAGS(s, d) &= ~D_CSET;
			if (sccs_newchksum(s)) {
				perror(modFiles[i]);
				sccs_free(s);
				continue;
			}
			updatePending(s);
		}
		sccs_free(s);
	}
}
