#include "system.h"
#include "sccs.h"

extern char *bin;
private char	*getrev(char *);
private MDBM	*mk_list(char *, char *);
private int	clean_file(MDBM *);
private int	do_rename(MDBM *, char *);
extern	void	cat(char *);
private void	checkRev(char *);

int
undo_main(int ac,  char **av)
{
	int	c, rc, 	force = 0, save = 1, ckRev = 0;
	char	buf[MAXLINE];
	char	rev_list[MAXPATH], undo_list[MAXPATH] = { 0 };
	char	*qflag = "", *vflag = "-v";
	char	*cmd = 0, *rev = 0;
	MDBM	*fileList;
#define	LINE "---------------------------------------------------------\n"
#define	BK_TMP  "BitKeeper/tmp"
#define	BK_UNDO "BitKeeper/tmp/undo"

	while ((c = getopt(ac, av, "a:fqsr:")) != -1) {
		switch (c) {
		    case 'a':
		    	rev = getrev(optarg);
			break;
		    case 'f': force  =  1; break;
		    case 'q': qflag = "-q"; vflag = ""; break;
		    case 'r': rev = optarg; ckRev++; break;
		    case 's': save = 0; break;
		    default :
			fprintf(stderr, "unknow option <%c>\n", c);
			exit(1);
		}
	}
	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "undo: can not find package root.\n");
		exit(1);
	}
	unless (rev) {
		fprintf(stderr, "usage bk undo [-afqs] -rcset-revision\n");
		exit(1);
	}
	if (ckRev) checkRev(rev);
	sprintf(rev_list, "%s/bk_rev_list%d",  TMP_PATH, getpid());
	fileList = mk_list(rev_list, rev);
	if (!fileList || clean_file(fileList)) goto err;

	sprintf(undo_list, "%s/bk_undo_list%d",  TMP_PATH, getpid());
	cmd = malloc(strlen(rev) + strlen(undo_list) + 200);
	sprintf(cmd, "bk stripdel -Ccr%s ChangeSet 2> %s", rev, undo_list);
	if (system(cmd) != 0) {
		gethelp("undo_error", bin, 0, stdout);
		cat(undo_list);
err:		if (undo_list[0]) unlink(undo_list);
		unlink(rev_list);
		if (fileList) mdbm_close(fileList);
		if (cmd) free(cmd);
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
			mdbm_close(fileList);
			exit(0);
		}
	}

	if (save) {
		unless (isdir(BK_TMP)) mkdirp(BK_TMP);
		sprintf(cmd, "bk cset %s -ffm%s > %s", vflag, rev, BK_UNDO);
		system(cmd);
		if (size(BK_UNDO) <= 0) {
			printf("Failed to create undo backup %s\n", BK_UNDO);
			goto err;
		}
	}
	free(cmd); cmd = 0;

	sprintf(buf, "bk stripdel %s -C - < %s", qflag, rev_list);
	if (system(buf) != 0) {
		fprintf(stderr, "Undo failed\n");
		goto err;
	}

	/*
	 * Handle any renames.  Done outside of stripdel because names only
	 * make sense at cset boundries.
	 * Also, run all files through renumber.
	 */
	if (do_rename(fileList, qflag)) goto err;
	mdbm_close(fileList);
	unlink(rev_list); unlink(undo_list);

	if (streq(qflag, "") && save) {
		printf("Patch containing these undone deltas left in %s\n",
		    BK_UNDO);
	}
	if (streq(qflag, "")) printf("Running consistency check...\n");
	if ((rc = system("bk -r check -a")) == 2) { /* 2 mean try again */
		if (streq(qflag, "")) {
			printf("Running consistency check again ...\n");
		}
		rc = system("bk -r check -a ");
	}
	return (rc);
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

	checkRev(top_rev);
	sprintf(tmpfile, "%s/bk_tmp%d", TMP_PATH, getpid());
	/* use special prune code triggered by a revision starting with '*' */
	sprintf(cmd,					/* CSTYLED */
		"bk -R prs -ohMa -r'*'%s -d\":REV:,\\c\" ChangeSet > %s",
		top_rev, tmpfile);
	system(cmd);
	fd = open(tmpfile, O_RDONLY, 0);
	if (buf) free(buf);
	sz = size(tmpfile);
	buf = malloc(sz + 1);
	if ((len = read(fd, buf, sz)) < 0) {
		perror(tmpfile);
		exit(1);
	}
	close(fd);
	unlink(tmpfile);
	buf[len] = 0;
	return (buf);
}

MDBM *
mk_list(char *rev_list, char *rev)
{
	MDBM 	*DB;
	char	*p, *cmd, buf[MAXLINE];
	FILE	 *f;

	assert(rev);
	cmd = malloc(strlen(rev) + 100);
	sprintf(cmd, "bk cset -ffl%s > %s", rev, rev_list);
	if (system(cmd) != 0) {
		printf("undo: can not extact revision list\n");
		return (NULL);
	}
	free(cmd);
	if (size(rev_list) == 0) {
		printf("undo: nothing to undo in \"%s\"\n", rev);
		exit(0);
	}
	f = fopen(rev_list, "rt");
	DB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	assert(f); assert(DB);
	while (fgets(buf, sizeof(buf), f)) {
		p = strchr(buf, '@');
		assert(p);
		*p = 0; /* remove @rev part */
		mdbm_store_str(DB, buf, "", MDBM_INSERT);
	}
	fclose(f);
	return DB;
}

private int
do_rename(MDBM *fileList, char *qflag)
{
	sccs	*s;
	project *proj = 0;
	kvpair  kv;
	FILE	*f;
	char	rc, renum_list[MAXPATH], buf[MAXLINE];

	sprintf(renum_list, "%s/bk_renum_list%d",  TMP_PATH, getpid());
	f = fopen(renum_list, "wb"); assert(f);
	for (kv = mdbm_first(fileList); kv.key.dsize;
						kv = mdbm_next(fileList)) {
		char	*sfile, *old_path;
		delta	*d;

		sfile = name2sccs(kv.key.dptr);
		unless (exists(sfile)) {
			free(sfile);
			continue;
		}
		s = sccs_init(sfile, INIT_NOCKSUM|INIT_SAVEPROJ, proj);
		assert(s);
		unless(proj) proj = s->proj;
		d = findrev(s, 0);
		assert(d);
		old_path = name2sccs(d->pathname);
		sccs_free(s);
		unless (streq(sfile, old_path)) {
			if (exists(old_path)) {
				printf("Unable to mv %s %s, %s exists\n",
						    sfile, old_path, old_path);
			} else {
				mkdirf(old_path);
				if (streq(qflag, "")) {
					printf("mv %s %s\n", sfile, old_path);
				}
				if (rename(sfile, old_path) != 0) {
					sprintf(buf, "mv %s %s",
							    sfile, old_path);
					assert(strlen(buf) < sizeof(buf));
					if (system(buf) != 0) {
						perror("rename failed");
						exit(1);
					}
				}
			}
		}
		/* must be AFTER the move */
		fprintf(f, "%s\n", old_path);
		free(sfile);
		free(old_path);
	}
	if (proj) proj_free(proj);
	fclose(f);
	sprintf(buf, "bk renumber %s -  < %s ", qflag, renum_list);
	rc = system(buf);
	unlink(renum_list);
	return rc;
}

private int
clean_file(MDBM *fileList)
{
	sccs	*s;
	project *proj = 0;
	kvpair  kv;

	for (kv = mdbm_first(fileList); kv.key.dsize;
						kv = mdbm_next(fileList)) {
		char *sfile;

		sfile = name2sccs(kv.key.dptr);
		s = sccs_init(sfile, INIT_NOCKSUM|INIT_SAVEPROJ, proj);
		assert(s);
		unless(proj) proj = s->proj;
		if (sccs_clean(s, SILENT)) {
			printf("Can not clean %s, Undo aborted\n", sfile);
			sccs_free(s);
			free(sfile);
			if (proj) proj_free(proj);
			return (-1);
		}
		sccs_free(s);
		free(sfile);
	}
	if (proj) proj_free(proj);
	return (0);
}
