#include "system.h"
#include "sccs.h"
#include "logging.h"

#define	BACKUP_SFIO "BitKeeper/tmp/undo_backup_sfio"

extern char *bin;

private char	**getrev(char *rev, int aflg);
private char	**mk_list(char *, char **);
private int	clean_file(char **);
private	int	moveAndSave(char **fileList);
private int	move_file(void);
private int	do_rename(char **, char *);
private int	check_patch(void);
private int	doit(char **fileList, char *rev_list, char *qflag);
private	void	save_log_markers(void);
private	void	update_log_markers(int verbose);

private	int	checkout;

int
undo_main(int ac,  char **av)
{
	int	c, rc, 	force = 0, save = 1;
	char	buf[MAXLINE];
	char	rev_list[MAXPATH], undo_list[MAXPATH] = { 0 };
	FILE	*f;
	int	i;
	int	status;
	char	**csetrev_list = 0;
	char	*qflag = "", *vflag = "-v";
	char	*cmd = 0, *rev = 0;
	int	aflg = 0;
	char	**fileList = 0;
#define	LINE "---------------------------------------------------------\n"
#define	BK_TMP  "BitKeeper/tmp"
#define	BK_UNDO "BitKeeper/tmp/undo_backup"

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help undo");
		return (0);
	}
	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "undo: cannot find package root.\n");
		exit(1);
	}
	
	while ((c = getopt(ac, av, "a:fqsr:")) != -1) {
		switch (c) {
		    case 'a': aflg = 1;				/* doc 2.0 */
			/* fall though */
		    case 'r': rev = optarg; break;		/* doc 2.0 */
		    case 'f': force  =  1; break;		/* doc 2.0 */
		    case 'q': qflag = "-q"; vflag = ""; break;	/* doc 2.0 */
		    case 's': save = 0; break;			/* doc 2.0 */
		    default :
			fprintf(stderr, "unknown option <%c>\n", c);
usage:			system("bk help -s undo");
			exit(1);
		}
	}
	unless (rev) goto usage;

	/* do checkouts? */
	if (strieq(user_preference("checkout"), "get")) checkout = 1;
	if (strieq(user_preference("checkout"), "edit")) checkout = 2;

	save_log_markers();
	unlink(BACKUP_SFIO); /* remove old backup file */
	unless (csetrev_list = getrev(rev, aflg)) {
		/* No revs we are done. */
		return (0);
	}
	rev = 0;  /* don't use wrong value */
	gettemp(rev_list, "bk_rev_list");
	fileList = mk_list(rev_list, csetrev_list);
	if (!fileList || clean_file(fileList)) goto err;

	gettemp(undo_list, "bk_undo_list");
	cmd = aprintf("bk stripdel -Cc - 2> %s", undo_list);
	f = popen(cmd, "w");
	free(cmd);
	unless (f) {
err:		if (undo_list[0]) unlink(undo_list);
		unlink(rev_list);
		freeLines(fileList);
		if (size(BACKUP_SFIO) > 0) {
			if (sysio(BACKUP_SFIO, 0, 0,
						"bk", "sfio", "-im", SYS)) {
				fprintf(stderr,
"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
"Your repository is only partially restored.   This is an error.  Please\n"
"examine the list of failures above and find out why they were not restored.\n"
"You must restore them place by hand before the repository is usable.\n"
"\n"
"A backup sfio is in %s\n"
"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",
				BACKUP_SFIO);
				exit(1);
			}
			fprintf(stderr,
"Your repository should be back to where it was before undo started\n"
"We are running a consistency check to verify this.\n");
			if (sys("bk", "-r", "check", "-a", SYS)) {
				fprintf(stderr, "check FAILED\n");
				exit(1);
			}
			fprintf(stderr, "check passed\n");
		}
		unlink(BACKUP_SFIO);
		sys(RM, "-rf", "RESYNC", SYS);
		exit(1);
	}
	EACH (csetrev_list) {
		fprintf(f, "ChangeSet%c%s\n", BK_FS, csetrev_list[i]);
	}
	status = pclose(f);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		getMsg("undo_error", bin, 0, 0, stdout);
		cat(undo_list);
		goto err;
	}
	unless (force) {
		printf(LINE);
		cat(rev_list);
		printf(LINE);
		printf("Remove these [y/n]? ");
		unless (fgets(buf, sizeof(buf), stdin)) buf[0] = 'n';
		if ((buf[0] != 'y') && (buf[0] != 'Y')) {
			unlink(rev_list);
			freeLines(fileList);
			exit(0);
		}
	}

	if (save) {
		if (streq(qflag, "")) {
			fprintf(stderr, "Saving a backup patch...\n");
		}
		unless (isdir(BK_TMP)) mkdirp(BK_TMP);
		cmd = aprintf("bk cset %s -ffm - > %s", vflag, BK_UNDO);
		f = popen(cmd, "w");
		free(cmd);
		if (f) {
			EACH(csetrev_list) fprintf(f, "%s\n", csetrev_list[i]);
			pclose(f);
		}
		if (check_patch()) {
			printf("Failed to create undo backup %s\n", BK_UNDO);
			goto err;
		}
	}
	freeLines(csetrev_list);
	sig_ignore();

	/*
	 * Move file to RESYNC and save a copy in a sfio backup file
	 */
	if (moveAndSave(fileList)) goto err;

	chdir(ROOT2RESYNC);
	if (doit(fileList, rev_list, qflag)) {
		chdir(RESYNC2ROOT);
		goto err;
	}
	chdir(RESYNC2ROOT);

	rmEmptyDirs(!streq(qflag, ""));
	if (streq(qflag, "") && save) {
		printf("Patch containing these undone deltas left in %s\n",
		    BK_UNDO);
	}
	if (streq(qflag, "")) printf("Running consistency check...\n");
	if ((rc = system("bk -r check -af")) == 2) { /* 2 means try again */
		if (streq(qflag, "")) {
			printf("Running consistency check again ...\n");
		}
		rc = system("bk -r check -a ");
	}
	rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 0;
	sig_default();

	freeLines(fileList);
	unlink(rev_list); unlink(undo_list);
	update_log_markers(streq(qflag, ""));
	if (rc) return (rc); /* do not remove backup if check failed */
	unlink(BACKUP_SFIO);
	sys(RM, "-rf", "RESYNC", SYS);
	return (rc);
}

private int
doit(char **fileList, char *rev_list, char *qflag)
{
	char	buf[MAXLINE];

	sprintf(buf, "bk stripdel %s -C - < %s", qflag, rev_list);
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
	putenv("BK_IGNORELOCK=YES");
	if (do_rename(fileList, qflag)) {
		putenv("BK_IGNORELOCK=NO");
		return (-1);
	}
	putenv("BK_IGNORELOCK=NO");
	if (move_file()) return (-1); /* mv from RESYNC to user tree */
	return (0);
}

private int
check_patch()
{
	MMAP	*m = mopen(BK_UNDO, "");
	char	*p;

	if (!m) return (1);
	for (p = m->mmap + msize(m) - 2; (p > m->mmap) && (*p != '\n'); p--);
	if (p <= m->mmap) {
bad:		mclose(m);
		return (1);
	}
	unless (strneq(p, "\n# Patch checksum=", 18)) goto bad;
	mclose(m);
	return (0);
}

private void
checkRev(char *rev)
{
	char	file[MAXPATH] = CHANGESET;
	sccs	*s = sccs_init(file, 0, 0);
	delta	*d;

	unless (s) {
		fprintf(stderr, "Can't init %s\n", file);
		exit(1);
	}
	d = sccs_getrev(s, rev, 0, 0);
	sccs_free(s);
	unless (d) {
		fprintf(stderr, "No such rev '%s' in ChangeSet\n", rev);
		exit(1);
	}
}

private char **
getrev(char *top_rev, int aflg)
{
	char	*cmd;
	int	status;
	char	**list = 0;
	FILE	*f;
	char	revline[MAXREV+1];

	checkRev(top_rev);
	if (aflg) {
		cmd = aprintf("bk -R prs -ohnMa -r'1.0..%s' -d:REV: ChangeSet",
		    top_rev);
	} else{
		cmd = aprintf("bk -R prs -hnr'%s' -d:REV: ChangeSet", 
		    top_rev);
	}
	f = popen(cmd, "r");
	free(cmd);
	while (fnext(revline, f)) {
		chomp(revline);
		list = addLine(list, strdup(revline));
	}
	status = pclose(f);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		fprintf(stderr, "undo: prs failed\n");
		exit(1);
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
	cmd = aprintf("bk cset -ffl - > %s", rev_list);
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
		exit(0);
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
	project *proj = 0;
	FILE	*f;
	int 	i, rc, status;
	char	*cmd;
	
	cmd = aprintf("bk renumber %s -", qflag);
	f = popen(cmd, "w"); 
	assert(f);
	free(cmd);
	EACH (fileList) {
		char	*sfile = fileList[i];
		unless (exists(sfile)) continue;
		fprintf(f, "%s\n", sfile);
	}
	status = pclose(f);
	rc = WEXITSTATUS(status);
	if (rc) return(rc);

	cmd = aprintf("bk names %s -", qflag);
	f = popen(cmd, "w"); 
	assert(f);
	free(cmd);
	EACH (fileList) {
		char	*sfile = fileList[i];
		char	*old_path;
		delta	*d;
		sccs	*s;

		unless (exists(sfile)) continue;
		s = sccs_init(sfile, INIT_NOCKSUM|INIT_SAVEPROJ, proj);
		assert(s);
		unless(proj) proj = s->proj;
		d = findrev(s, 0);
		assert(d);
		old_path = name2sccs(d->pathname);
		sccs_free(s);
		unless (streq(sfile, old_path)) fprintf(f, "%s\n", sfile);
		free(old_path);
	}
	if (proj) proj_free(proj);
	status = pclose(f);
	rc = WEXITSTATUS(status);

	return rc;
}

private int
clean_file(char **fileList)
{
	sccs	*s;
	project *proj = 0;
	int	i;

	EACH(fileList) {
		s = sccs_init(fileList[i], INIT_NOCKSUM|INIT_SAVEPROJ, proj);
		assert(s && HASGRAPH(s));
		unless(proj) proj = s->proj;
		if (sccs_clean(s, SILENT)) {
			printf("Cannot clean %s, Undo aborted\n", fileList[i]);
			sccs_free(s);
			if (proj) proj_free(proj);
			return (-1);
		}
		sccs_free(s);
	}
	if (proj) proj_free(proj);
	return (0);
}

/*
 * Move file to RESYNC and save a backup copy in sfio file
 */
private int
moveAndSave(char **fileList)
{
	FILE	*f;
	char	tmp[MAXPATH];
	int	i, rc = 0;

	if (isdir("RESYNC")) {
		fprintf(stderr, "Repository locked by RESYNC directory\n");
		return (-1);
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
move_file()
{
	char	from[MAXPATH], to[MAXPATH];
	FILE	*f;
	int	rc = 0;

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
		};
	}
	pclose(f);
	if (rc) return (rc); /* if error, abort */

	/*
	 * Throw the file "over the wall"
	 */
	f = popen("bk sfiles", "r");
	while (fnext(from, f)) {
		chop(from);
		sprintf(to, "../%s", from);
		if (mv(from, to)) {
			fprintf(stderr,
			    "Cannot move %s to %s\n", from, to);
			rc = -1;
			break;
		};
		if (checkout) {
			/* 
			 * We don't use do_checkout() because the proj
			 * struct is not realiable.
			 */
			if (checkout == 1) {
				sys("bk", "get", "-q", to, SYS);
			} else {
				sys("bk", "edit", "-q", to, SYS);
			}
		};
	}
	pclose(f);
	return (rc);
}

private	char	*markfile[] = { LMARK, CMARK };
private	int	valid_marker[2];

private void
save_log_markers(void)
{
	int	i;
	sccs	*s = sccs_csetInit(0, 0);
	unless (s) return;
	
	for (i = 0; i < 2; i++) {
		FILE	*f;
		char	key[MAXKEY];
		valid_marker[i] = 0;
		f = fopen(markfile[i], "rb");
		if (f) {
			if (fnext(key, f)) {
				chomp(key);
				if (sccs_findKey(s, key)) valid_marker[i] = 1;
			}
			fclose(f);
		}
	}
	sccs_free(s);
}

private void
update_log_markers(int verbose)
{
	int	i;
	sccs	*s = sccs_csetInit(0, 0);
	unless (s) return;
	
	/* Clear marks that are still valid */
	for (i = 0; i < 2; i++) {
		FILE	*f;
		char	key[MAXKEY];
		
		unless (valid_marker[i]) continue;
		f = fopen(markfile[i], "rb");
		if (f) {
			if (fnext(key, f)) {
				chomp(key);
				if (sccs_findKey(s, key)) valid_marker[i] = 0;
			}
			fclose(f);
		}
	}
	sccs_free(s);

	/* Any remaing mark must have been deleted by the undo */
	for (i = 0; i < 2; i++) {
		if (valid_marker[i]) updLogMarker(i, verbose, stderr);
	}
}
