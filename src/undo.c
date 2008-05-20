#include "system.h"
#include "sccs.h"
#include "logging.h"

#define	BACKUP_SFIO "BitKeeper/tmp/undo_backup_sfio"
#define	UNDO_CSETS  "BitKeeper/tmp/undo_csets"

#define	UNDO_ERR	1	/* exitcode for errors */
#define	UNDO_SKIP	2	/* exitcode for early exit with no work */

private char	**getrev(char *rev, int aflg);
private char	**mk_list(char *, char **);
private int	clean_file(char **);
private	int	moveAndSave(char **fileList);
private int	move_file(char *checkfiles);
private int	do_rename(char **, char *);
private int	check_patch(char *patch);
private int	doit(char **fileList, char *rev_list, char *qflag, char *);

private	int	fromclone;

int
undo_main(int ac,  char **av)
{
	int	c, rc, 	force = 0, save = 1;
	char	buf[MAXLINE];
	char	rev_list[MAXPATH], undo_list[MAXPATH] = { 0 };
	FILE	*f;
	int	i;
	int	status;
	int	rmresync = 1;
	char	**csetrev_list = 0;
	char	*qflag = "";
	char	*cmd = 0, *rev = 0;
	int	aflg = 0, quiet = 0, verbose = 0;
	char	**fileList = 0;
	char	*checkfiles;	/* filename of list of files to check */
	char	*patch = "BitKeeper/tmp/undo.patch";
	char	*p;

	if (proj_cd2root()) {
		fprintf(stderr, "undo: cannot find package root.\n");
		return (UNDO_ERR);
	}

	fromclone = 0;
	while ((c = getopt(ac, av, "a:Cfqp;r:sv")) != -1) {
		switch (c) {
		    case 'a': aflg = 1;				/* doc 2.0 */
			/* fall though */
		    case 'r': rev = optarg; break;		/* doc 2.0 */
		    case 'C': fromclone = 1; break;
		    case 'f': force  =  1; break;		/* doc 2.0 */
		    case 'q':					/* doc 2.0 */
		    	quiet = 1; qflag = "-q"; break;
		    case 'p': save = 1; patch = optarg; break;
		    case 's': save = 0; break;			/* doc 2.0 */
		    case 'v': verbose = 1; break;
		    default :
			fprintf(stderr, "unknown option <%c>\n", c);
usage:			system("bk help -s undo");
			return (UNDO_ERR);
		}
	}
	unless (rev) goto usage;

	save_log_markers();
	unlink(BACKUP_SFIO); /* remove old backup file */
	unless (csetrev_list = getrev(rev, aflg)) {
		/* No revs we are done. */
		return (fromclone ? UNDO_SKIP : 0);
	}
	rev = 0;  /* don't use wrong value */
	bktmp(rev_list, "rev_list");
	fileList = mk_list(rev_list, csetrev_list);
	unless (fileList) goto err;

	bktmp(undo_list, "undo_list");
	cmd = aprintf("bk stripdel -Cc - 2> '%s'", undo_list);
	f = popen(cmd, "w");
	free(cmd);
	unless (f) {
err:		if (undo_list[0]) unlink(undo_list);
		unlink(rev_list);
		unlink(UNDO_CSETS);
		freeLines(fileList, free);
		if ((size(BACKUP_SFIO) > 0) && restore_backup(BACKUP_SFIO,0)) {
			return (UNDO_ERR);
		}
		unlink(BACKUP_SFIO);
		if (rmresync && exists("RESYNC")) rmtree("RESYNC");
		return (UNDO_ERR);
	}
	EACH (csetrev_list) {
		fprintf(f, "ChangeSet%c%s\n", BK_FS, csetrev_list[i]);
	}
	status = pclose(f);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		getMsg("undo_error", bin, 0, stdout);
		cat(undo_list);
		goto err;
	}
	unless (force) {
		for (i = 0; i<79; ++i) putchar('-'); putchar('\n');
		fflush(stdout);
		f = popen(verbose? "bk changes -av -" : "bk changes -a -", "w");
		EACH (csetrev_list) fprintf(f, "%s\n", csetrev_list[i]);
		pclose(f);
		printf("Remove these [y/n]? ");
		unless (fgets(buf, sizeof(buf), stdin)) buf[0] = 'n';
		if ((buf[0] != 'y') && (buf[0] != 'Y')) {
			unlink(rev_list);
			unlink(undo_list);
			freeLines(fileList, free);
			return (UNDO_ERR);
		}
	}

	unless (fromclone) {
		f = fopen(UNDO_CSETS, "w");
		EACH (csetrev_list) fprintf(f, "%s\n", csetrev_list[i]);
		fclose(f);
		putenv("BK_CSETLIST=" UNDO_CSETS);
		if (trigger("undo", "pre")) goto err;
	}

	if (save) {
		unless (isdir(BKTMP)) mkdirp(BKTMP);
		/* like bk makepatch but skips over missing files/keys */
		cmd = aprintf("bk cset -Bffm - > '%s'", patch);
		f = popen(cmd, "w");
		free(cmd);
		if (f) {
			EACH(csetrev_list) fprintf(f, "%s\n", csetrev_list[i]);
			pclose(f);
		}
		if (check_patch(patch)) {
			printf("Failed to create undo backup %s\n", patch);
			goto err;
		}
	}
	freeLines(csetrev_list, free);
	if (clean_file(fileList)) goto err;
	sig_ignore();

	/*
	 * Move file to RESYNC and save a copy in a sfio backup file
	 */
	switch (moveAndSave(fileList)) {
	    case -2: rmresync = 0; goto err;
	    case -1: goto err;
	}

	checkfiles = bktmp(0, "undo_ck");
	chdir(ROOT2RESYNC);
	if (doit(fileList, rev_list, qflag, checkfiles)) {
		chdir(RESYNC2ROOT);
		unlink(checkfiles);
		free(checkfiles);
		goto err;
	}
	chdir(RESYNC2ROOT);

	rmEmptyDirs(quiet);
	if (!quiet && save) printf("Backup patch left in \"%s\".\n", patch);

	idcache_update(checkfiles);
	proj_restoreAllCO(0, 0);

	unless (quiet) printf("Running consistency check...\n");
	fflush(stdout);
	if (fromclone) {
		p = quiet ? "-fT" : "-fvT";
	} else {
		p = quiet ? "-f" : "-fv";
	}
	if (proj_configbool(0, "partial_check")) {
		rc = run_check(checkfiles, p, 0);
	} else {
		rc = run_check(0, p, 0);
	}
	unlink(checkfiles);
	free(checkfiles);
	sig_default();

	freeLines(fileList, free);
	unlink(rev_list);
	unlink(undo_list);
	unless (fromclone) unlink(UNDO_CSETS);
	update_log_markers(!quiet);
	if (rc) return (rc); /* do not remove backup if check failed */
	unlink(BACKUP_SFIO);
	rmtree("RESYNC");
	unlink(CSETS_IN);	/* no longer valid */
	unless (fromclone) {
		putenv("BK_CSETLIST=");
		putenv("BK_STATUS=OK");
		trigger("undo", "post");
	}
	return (rc);
}

private int
doit(char **fileList, char *rev_list, char *qflag, char *checkfiles)
{
	char	buf[MAXLINE];

	sprintf(buf, "bk stripdel %s -C - < '%s'", qflag, rev_list);
	if (system(buf) != 0) {
		fprintf(stderr, "Undo failed\n");
		return (-1);
	}

	/*
	 * Make sure stripdel
	 * did not delete BitKeeper/etc when removing empty dir
	 */
	assert(exists(BKROOT));

	/*
	 * Handle any renames.  Done outside of stripdel because names only
	 * make sense at cset boundries.
	 * Also, run all files through renumber.
	 */
	putenv("BK_IGNORE_WRLOCK=YES");
	if (do_rename(fileList, qflag)) {
		putenv("BK_IGNORE_WRLOCK=NO");
		return (-1);
	}
	putenv("BK_IGNORE_WRLOCK=NO");
	/* mv from RESYNC to user tree */
	if (move_file(checkfiles)) return (-1);
	return (0);
}

private int
check_patch(char *patch)
{
	MMAP	*m = mopen(patch, "");

	unless (m) return (1);
	unless (strstr(m->mmap, "\n\n# Patch checksum=")) {
		mclose(m);
		return (1);
	}
	mclose(m);
	return (0);
}

private char **
getrev(char *top_rev, int aflg)
{
	char	*cmd, *rev;
	int	status;
	char	**list = 0;
	FILE	*f;
	char	revline[MAXKEY];

	if (aflg) {
		rev = aprintf("-r'%s..'", top_rev);
	} else if (IsFullPath(top_rev) && isreg(top_rev)) {
		rev = aprintf("- < '%s'", top_rev);
	} else {
		rev = aprintf("-r'%s'", top_rev);
	}
	cmd = aprintf("bk changes -and:KEY: %s 2>" DEVNULL_WR, rev);
	free(rev);
	f = popen(cmd, "r");
	free(cmd);
	while (fnext(revline, f)) {
		chomp(revline);
		list = addLine(list, strdup(revline));
	}
	status = pclose(f);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		fprintf(stderr, "No such rev '%s' in ChangeSet\n", top_rev);
		exit(UNDO_ERR);
	}
	return (list);
}

private char **
mk_list(char *rev_list, char **csetrev_list)
{
	char	*p, *cmd, buf[MAXLINE];
	char	**flist = 0;
	FILE	*f;
	int	i;
	int	status;
	MDBM	*db;
	kvpair	kv;

	assert(csetrev_list);
	cmd = aprintf("bk cset -ffl5 - > '%s'", rev_list);
	f = popen(cmd, "w");
	if (f) {
		EACH(csetrev_list) fprintf(f, "%s\n", csetrev_list[i]);
	}
	status = pclose(f);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		printf("undo: %s\n", cmd);
		printf("undo: cannot extract revision list\n");
		return (NULL);
	}
	free(cmd);
	if (size(rev_list) == 0) {
		printf("undo: nothing to undo in \"");
		EACH(csetrev_list) printf("%s,", csetrev_list[i]);
		printf("\"\n");
		exit(fromclone ? UNDO_SKIP : 0);
	}
	f = fopen(rev_list, "rt");
	db = mdbm_open(NULL, 0, 0, MAXPATH);
	assert(db);
	while (fgets(buf, sizeof(buf), f)) {
		p = strchr(buf, BK_FS);
		assert(p);
		*p = 0; /* remove rev part */

		p = name2sccs(buf);
		mdbm_store_str(db, p, "", MDBM_REPLACE); /* remove dup */
		free(p);
	}
	fclose(f);

	for (kv = mdbm_first(db); kv.key.dsize; kv = mdbm_next(db)) {
		flist = addLine(flist, strdup(kv.key.dptr));
	}
	mdbm_close(db);
	return (flist);
}

private int
do_rename(char **fileList, char *qflag)
{
	FILE	*f;
	int 	i, rc, status;
	char	*flist;
	char	*quiet;

	flist = bktmp(0, "undorename");
	assert(flist);
	f = fopen(flist, "w");
	assert(f);
	EACH (fileList) {
		char	*sfile = fileList[i];
		unless (exists(sfile)) continue;
		fprintf(f, "%s\n", sfile);
	}
	fclose(f);

	quiet = streq(qflag, "") ? "--" : qflag;
	status = sysio(flist, 0, 0, "bk", "renumber", quiet, "-", SYS);
	rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	unless (rc) {
		status = sysio(flist, 0, 0, "bk", "names", quiet, "-", SYS);
		rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	}
	unlink(flist);
	free(flist);
	return (rc);
}

private int
clean_file(char **fileList)
{
	sccs	*s;
	char	*name;
	int	i;

	/*
	 * First loop checks for diffs on any writable file,
	 * second loop actually cleans.  Done this way so that we don't
	 * clean partway and then exit, that will leave files not checked out.
	 */
	EACH(fileList) {
		if (streq(fileList[i], CHANGESET)) continue;
		s = sccs_init(fileList[i], INIT_NOCKSUM);
		assert(s && HASGRAPH(s));
		if (sccs_hasDiffs(s, SILENT, 1)) {
			fprintf(stderr,
			    "Cannot clean %s, undo aborted\n", s->gfile);
			sccs_free(s);
			return (-1);
		}
		proj_saveCO(s);
		sccs_free(s);
	}
	EACH(fileList) {
		name = sccs2name(fileList[i]);
		if (!writable(name) && (unlink(name) == 0)) {
			free(name);
			continue;
		} /* else let clean try and sort it out */
		free(name);
		s = sccs_init(fileList[i], INIT_NOCKSUM);
		assert(s && HASGRAPH(s));
		if (sccs_clean(s, SILENT)) {
			fprintf(stderr,
			    "Cannot clean %s, undo aborted\n", s->gfile);
			sccs_free(s);
			proj_restoreAllCO(0, 0);
			return (-1);
		}
		sccs_free(s);
	}
	return (0);
}

/*
 * Move file to RESYNC and save a backup copy in sfio file
 * Return: 0 on success; -2 if RESYNC directory already exists; -1 on err
 */
private int
moveAndSave(char **fileList)
{
	FILE	*f;
	char	tmp[MAXPATH];
	int	i, rc = 0;

	if (isdir("RESYNC")) {
		fprintf(stderr, "Repository locked by RESYNC directory\n");
		return (-2);
	}

	strcpy(tmp, "RESYNC/BitKeeper/etc");
	if (mkdirp(tmp)) {
		perror(tmp);
		return (-1);
	}

	strcpy(tmp, "RESYNC/BitKeeper/tmp");
	if (mkdirp(tmp)) {
		perror(tmp);
		return (-1);
	}

	/*
	 * Run sfio under RESYNC, because we pick up the files _after_
	 * it is moved to RESYNC. This works better for win32; We usually
	 * detect access conflict when we moves file.
	 */
	chdir(ROOT2RESYNC);
	f = popen("bk sfio -omq > " "../" BACKUP_SFIO, "w");
	chdir(RESYNC2ROOT);
	unless (f) {
		perror("sfio");
		return (-1);
	}
	EACH (fileList) {
		sprintf(tmp, "RESYNC/%s", fileList[i]);
		if (mv(fileList[i], tmp)) {
			fprintf(stderr,
			    "Cannot mv %s to %s\n", fileList[i], tmp);
			rc = -1;
			break;
		} else {
			/* Important: save a copy only if mv is successful */
			fprintf(f, "%s\n", fileList[i]);
		}
	}
	pclose(f);
	return (rc);
}


/*
 * Move file from RESYNC tree to the user tree
 */
private int
move_file(char *checkfiles)
{
	char	from[MAXPATH], to[MAXPATH];
	FILE	*f;
	FILE	*chk;
	int	rc = 0;
	int	sync = proj_sync(0);
	int	fd;

	/*
	 * Cannot trust fileList, because file may be renamed
	 *
	 * Paranoid, check for name conflict before we throw it over the wall
	 */
	f = popen("bk sfiles", "r");
	while (fnext(from, f)) {
		chop(from);
		sprintf(to, "../%s", from);
		/*
		 * This should never happen if the repo is in a sane state
		 */
		if (exists(to)) {
			fprintf(stderr, "%s: name conflict\n", to);
			rc = -1;
			break;
		}
	}
	pclose(f);
	if (rc) return (rc); /* if error, abort */

	/*
	 * Throw the file "over the wall"
	 */
	chk = fopen(checkfiles, "w");
	f = popen("bk sfiles", "r");
	while (fnext(from, f)) {
		fputs(from, chk);
		chop(from);
		sprintf(to, "%s/%s", RESYNC2ROOT, from);
		if (mv(from, to)) {
			fprintf(stderr,
			    "Cannot move %s to %s\n", from, to);
			rc = -1;
			break;
		}
		if (sync) {
		    	fd = open(to, O_RDONLY, 0);
			fsync(fd);
			close(fd);
		}
	}
	pclose(f);
	fclose(chk);
	return (rc);
}

private	int	valid_marker;

void
save_log_markers(void)
{
	char	*mark;
	sccs	*s = sccs_csetInit(0);
	unless (s) return;

	valid_marker = 0;
	if (mark = signed_loadFile(CMARK)) {
		if (sccs_findKey(s, mark)) valid_marker = 1;
		free(mark);
	}
	sccs_free(s);
}

void
update_log_markers(int verbose)
{
	sccs	*s = sccs_csetInit(0);
	char	*mark;

	unless (s) return;

	if (valid_marker) {
		if (mark = signed_loadFile(CMARK)) {
			if (sccs_findKey(s, mark)) valid_marker = 0;
			free(mark);
		}
	}
	sccs_free(s);
	if (valid_marker) updLogMarker(verbose, stderr);
}
