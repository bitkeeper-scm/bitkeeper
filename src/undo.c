#include "system.h"
#include "sccs.h"

#define	BACKUP_SFIO "BitKeeper/tmp/undo_backup_sfio"

extern char *bin;
private char	*getrev(char *);
private char	**mk_list(char *, char *);
private int	clean_file(char **);
private	int	moveAndSave(char **fileList);
private int	move_file();
private int	do_rename(char **, char *);
private void	checkRev(char *);
private int	check_patch();
private int	doit(char **fileList, char *rev_list, char *qflag);

int
undo_main(int ac,  char **av)
{
	int	c, rc, 	force = 0, save = 1, ckRev = 0;
	char	buf[MAXLINE];
	char	rev_list[MAXPATH], undo_list[MAXPATH] = { 0 };
	char	*qflag = "", *vflag = "-v";
	char	*cmd = 0, *rev = 0;
	char	**fileList;
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
		    case 'a':					/* doc 2.0 */
			rev = getrev(optarg);
			unless (rev) return (0); /* we are done */
			break;
		    case 'f': force  =  1; break;		/* doc 2.0 */
		    case 'q': qflag = "-q"; vflag = ""; break;	/* doc 2.0 */
		    case 'r': rev = optarg; ckRev++; break;	/* doc 2.0 */
		    case 's': save = 0; break;			/* doc 2.0 */
		    default :
			fprintf(stderr, "unknown option <%c>\n", c);
usage:			system("bk help -s undo");
			exit(1);
		}
	}
	unless (rev) goto usage;
	unlink(BACKUP_SFIO); /* remove old backup file */
	if (ckRev) checkRev(rev);
	sprintf(rev_list, "%s/bk_rev_list%d",  TMP_PATH, getpid());
	fileList = mk_list(rev_list, rev);
	if (!fileList || clean_file(fileList)) goto err;

	sprintf(undo_list, "%s/bk_undo_list%d",  TMP_PATH, getpid());
	cmd = malloc(strlen(rev) + strlen(undo_list) + 200);
	sprintf(cmd, "bk stripdel -Ccr%s ChangeSet 2> %s", rev, undo_list);
	if (system(cmd) != 0) {
		getMsg("undo_error", bin, 0, stdout);
		cat(undo_list);
err:		if (undo_list[0]) unlink(undo_list);
		unlink(rev_list);
		freeLines(fileList);
		if (cmd) free(cmd);
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
		sprintf(cmd, "bk cset %s -ffm%s > %s", vflag, rev, BK_UNDO);
		system(cmd);
		if (check_patch()) {
			printf("Failed to create undo backup %s\n", BK_UNDO);
			goto err;
		}
	}
	free(cmd); cmd = 0;

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
	sig_default();

	freeLines(fileList);
	unlink(rev_list); unlink(undo_list);
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
	 * Handle any renames.  Done outside of stripdel because names only
	 * make sense at cset boundries.
	 * Also, run all files through renumber.
	 */
	if (do_rename(fileList, qflag)) return (-1);
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

private char *
getrev(char *top_rev)
{
	static char *buf;
	char	tmpfile[MAXPATH];
	char	cmd[MAXKEY];
	int	fd, len, sz;
	char	*retptr = 0;

	checkRev(top_rev);
	sprintf(tmpfile, "%s/bk_tmp%d", TMP_PATH, getpid());
	/* use special prune code triggered by a revision starting with '*' */
	sprintf(cmd,
	    "bk -R prs -ohMa -r'*'%s -d':REV:,' ChangeSet > %s",
	    top_rev, tmpfile);
	system(cmd);
	fd = open(tmpfile, O_RDONLY, 0);
	if (buf) free(buf);
	sz = size(tmpfile);
	if (sz) {
		buf = (malloc)(sz + 1);
		if ((len = read(fd, buf, sz)) < 0) {
			perror(tmpfile);
			exit(1);
		}
		buf[len] = 0;
		retptr = buf;
	}
	close(fd);
	unlink(tmpfile);
	return (retptr);
}

char **
mk_list(char *rev_list, char *rev)
{
	char	*p, *cmd, buf[MAXLINE];
	char	**flist = 0;
	FILE	*f;
	MDBM	*db;
	kvpair	kv;

	assert(rev);
	cmd = malloc(strlen(rev) + 100);
	sprintf(cmd, "bk cset -ffr%s > %s", rev, rev_list);
	if (system(cmd) != 0) {
		printf("undo: %s\n", cmd);
		printf("undo: cannot extract revision list\n");
		return (NULL);
	}
	free(cmd);
	if (size(rev_list) == 0) {
		printf("undo: nothing to undo in \"%s\"\n", rev);
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
	sccs	*s;
	project *proj = 0;
	FILE	*f, *f1;
	char	renum_list[MAXPATH], rename_list[MAXPATH], buf[MAXLINE];
	int 	i, rc, status, warned = 0;

	bktemp(renum_list);
	bktemp(rename_list);
	f = fopen(renum_list, "wb"); assert(f);
	f1 = fopen(rename_list, "wb"); assert(f1);
	EACH (fileList) {
		char	*sfile, *old_path;
		delta	*d;

		sfile = fileList[i];
		unless (exists(sfile)) {
			continue;
		}
		s = sccs_init(sfile, INIT_NOCKSUM|INIT_SAVEPROJ, proj);
		assert(s);
		unless(proj) proj = s->proj;
		d = findrev(s, 0);
		assert(d);
		old_path = name2sccs(d->pathname);
		sccs_free(s);
		unless (streq(sfile, old_path)) fprintf(f1, "%s\n", sfile);
		fprintf(f, "%s\n", old_path);
		free(old_path);
	}
	if (proj) proj_free(proj);
	fclose(f);
	fclose(f1);

	sprintf(buf, "bk names %s -  < %s", qflag, rename_list);
	status = system(buf);
	rc = WEXITSTATUS(status);
	if (rc) goto done;

	sprintf(buf, "bk renumber %s -  < %s", qflag, renum_list);
	status = system(buf);
	rc = WEXITSTATUS(status);
done:	unlink(rename_list);
	unlink(renum_list);
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
	}
	pclose(f);
	return (rc);
}
