#include "system.h"
#include "sccs.h"

extern char *bin;
private char	*getrev(char *);
private MDBM	*mk_list(char *, char *, char *);
private void	clean_file(MDBM *);
extern	void	cat(char *);
private void	checkRev(char *rev);

int
undo_main(int ac,  char **av)
{
	int	c, rc, 	force = 0, save = 1;
	char	buf[MAXLINE];
	char	rev_list[MAXPATH], file_list[MAXPATH];
	char	rename_list[MAXPATH], undo_list[MAXPATH];
	char	*qflag = "", *vflag = "-v";
	char	*cmd, *p, *rev = 0;
	FILE	*f, *renum;
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
		    case 'r': checkRev(rev = optarg); break;
		    case 's': save = 0; break;
		    default :
			fprintf(stderr, "unknow option <%c>\n", c);
			exit(1);
		}
	}
	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "undo: can not find project root.\n");
		exit(1);
	}
	unless (rev) {
		fprintf(stderr, "usage bk undo [-afqs] -rcset-revision\n");
		exit(1);
	}

	sprintf(rev_list, "%s/bk_rev_list%d",  TMP_PATH, getpid());
	sprintf(file_list, "%s/bk_file_list%d",  TMP_PATH, getpid());
	fileList = mk_list(rev_list, file_list, rev);
	clean_file(fileList);
	mdbm_close(fileList);

	sprintf(undo_list, "%s/bk_undo_list%d",  TMP_PATH, getpid());
	cmd = malloc(strlen(rev) + strlen(undo_list) + 200);
	sprintf(cmd, "bk stripdel -Ccr%s ChangeSet 2> %s", rev, undo_list);
	if (system(cmd) != 0) {
		gethelp("undo_error", bin, stdout);
		cat(undo_list);
		unlink(undo_list);
		free(cmd);
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
			exit(0);
		}
	}

	if (save) {
		unless (isdir(BK_TMP)) mkdirp(BK_TMP);
		sprintf(cmd, "bk cset %s -ffm%s > %s", vflag, rev, BK_UNDO);
		system(cmd);
		if (size(BK_UNDO) <= 0) {
			printf("Failed to create undo backup %s\n", BK_UNDO);
			exit(1);
		}
	}
	free(cmd); cmd = 0;

	sprintf(buf, "bk stripdel %s -C - < %s", qflag, rev_list);
	if (system(buf) != 0) {
		fprintf(stderr, "Undo failed\n");
		unlink(rev_list);
		exit(1);
	}

	/*
	 * Handle any renames.  Done outside of stripdel because names only
	 * make sense at cset boundries.
	 * Also, run all files through renumber.
	 */
	sprintf(rename_list, "%s/bk_rename_list%d",  TMP_PATH, getpid());
	sprintf(buf, "bk prs -hr+ -d':PN: :SPN:' - < %s > %s",
							file_list, rename_list);
	system(buf);
	f = fopen(rename_list, "rt");
	sprintf(buf, "bk renumber %s -", qflag);
	renum = popen(buf, "w");
	while (fgets(buf, sizeof(buf), f)) {
		chop(buf);
		p = strchr(buf, ' ');
		assert(p);
		*p++ = 0;
		unless (streq(buf, p)) {
			if (exists(p)) {
				printf("Unable to mv %s %s, %s exists\n",
								buf, p, p);
			} else {
				mkdirf(p);
				if (streq(qflag, "")) {
					printf("mv %s %s\n", buf, p);
				}
				if (rename(buf, p) != 0) {
					perror("rename failed");
					exit(1);
				}
			}
		}
		/* must be AFTER the move */
		fprintf(renum, "%s\n", p);
	}
	unlink(file_list); unlink(rev_list);
	unlink(undo_list); unlink(rename_list);

	pclose(renum);
	if (streq(qflag, "") && save) {
		printf("Patch containing these undone deltas left in %s",
		    BK_UNDO);
	}
	if (streq(qflag, "")) printf("Running consistency check...\n");
	system("bk sfiles -r");
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
	char	*file = CHANGESET;
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
	sprintf(cmd,					/* CSTYLED */
		"bk -R prs -ohMa -r1.0..%s -d\":REV:,\\c\" ChangeSet > %s",
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
mk_list(char *rev_list, char *file_list, char *rev)
{
	kvpair  kv;
	MDBM 	*DB;
	char	*p, *cmd, buf[MAXLINE];
	FILE	 *f;

	assert(rev);
	cmd = malloc(strlen(rev) + 100);
	sprintf(cmd, "bk cset -ffl%s > %s", rev, rev_list);
	system(cmd);
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
	f = fopen(file_list, "wb");
	for (kv = mdbm_first(DB); kv.key.dsize; kv = mdbm_next(DB)) {
		fprintf(f, "%s\n", kv.key.dptr);
	}
	fclose(f);
	return DB;
}

private void
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
			mdbm_close(fileList);
			if (proj) proj_free(proj);
			exit(1);
		}
		sccs_free(s);
		free(sfile);
	}
	if (proj) proj_free(proj);
}
