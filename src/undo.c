#include "system.h"
#include "sccs.h"

extern char *bin;
private char	*getrev(char *);
private void	clean_file(char *, char *);
extern	void	cat(char *);
private void	checkRev(char *rev);

int
undo_main(int ac,  char **av)
{
	int	c, rc;
	int	force = 0, save = 1;
	char	buf[MAXLINE], rmlist[MAXPATH];
	char	mvlist[MAXPATH], renamelist[MAXPATH], undolist[MAXPATH];
	char	*cmd;
	char	*qflag = "", *vflag = "-v";
	char	*p, *rev = 0;
	FILE	*f, *f1, *renum;
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

	sprintf(rmlist, "%s/bk_rmlist%d",  TMP_PATH, getpid());
	clean_file(rmlist, rev);

	sprintf(undolist, "%s/bk_undolist%d",  TMP_PATH, getpid());
	cmd = malloc(strlen(rev) + strlen(undolist) + 200);
	sprintf(cmd, "bk stripdel -Ccr%s ChangeSet 2> %s", rev, undolist);
	if (system(cmd) != 0) {
		gethelp("undo_error", bin, stdout);
		cat(undolist);
		unlink(undolist);
		exit(1);
	}

	unless (force) {
		printf(LINE);
		cat(rmlist);
		printf(LINE);
		printf("Remove these [y/n]? ");
		unless (fgets(buf, sizeof(buf), stdin)) buf[0] = 'n';
		if ((buf[0] != 'y') && (buf[0] != 'Y')) {
			unlink(rmlist);
			exit(0);
		}
	}

	if (save) {
		unless (isdir(BK_TMP)) {
			mkdirp(BK_TMP);
		}
		sprintf(cmd, "bk cset %s -ffm%s > %s", vflag, rev, BK_UNDO);
		system(cmd);
		if (size(BK_UNDO) <= 0) {
			printf("Failed to create undo backup %s\n", BK_UNDO);
			exit(1);
		}
	}

	sprintf(mvlist, "%s/bk_mvlist%d",  TMP_PATH, getpid());
	f1 = fopen(mvlist, "wb");
	f = fopen(rmlist, "rt");
	assert(f);
	assert(f1);
	while (fgets(buf, sizeof(buf), f)) {
		char	buf1[MAXLINE];

		chop(buf);
		p = strchr(buf, '@');
		assert(p);
		*p++ = 0;
		fprintf(f1, "%s\n", buf);
		sprintf(buf1, "bk stripdel %s -Cr%s %s", qflag, p, buf);
		if (system(buf1) != 0) {
			fprintf(stderr, "Undo of %s@%s failed\n", buf, p);
			fclose(f);
			fclose(f1);
			unlink(rmlist);
			unlink(mvlist);
			exit(1);
		}
	}
	fclose(f);
	fclose(f1);

	/*
	 * Handle any renames.  Done outside of stripdel because names only
	 * make sense at cset boundries.
	 * Also, run all files through renumber.
	 */
	sprintf(renamelist, "%s/bk_renaemlist%d",  TMP_PATH, getpid());
	sprintf(buf, "bk prs -hr+ -d':PN: :SPN:' - < %s > %s",
							mvlist, renamelist);
	system(buf);
	f = fopen(renamelist, "rt");
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
	unlink(mvlist); unlink(rmlist); unlink(undolist); unlink(renamelist);
	pclose(renum);
	if (streq(qflag, "") && save) {
		printf("Patch containing these undone deltas left in %s",
		    BK_UNDO);
	}
	if (streq(qflag, "")) printf("Running consistency check...\n");
	system("bk sfiles -r");
	sprintf(buf, "bk -r check -a");
	if ((rc = system(buf)) == 2) { /* 2 mean try again */
		if (streq(qflag, "")) {
			printf("Running consistency check again ...\n");
		}
		rc = system(buf);
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

private void
clean_file(char *rmlist, char *rev)
{
	FILE	*f, *f1;
	char	cleanlist[MAXPATH];
	char	buf[MAXLINE];
	char	*cmd;
	char	*p;

	assert(rev);
	sprintf(cleanlist, "%s/bk_cleanlist%d",  TMP_PATH, getpid());
	cmd = malloc(strlen(rev) + 100);
	sprintf(cmd, "bk cset -ffl%s > %s", rev, rmlist);
	system(cmd);
	if (size(rmlist) == 0) {
		printf("undo: nothing to undo in \"%s\"\n", rev);
		exit(0);
	}
	f = fopen(rmlist, "rt");
	f1 = fopen(cleanlist, "wb");
	assert(f);
	assert(f1);
	while (fgets(buf, sizeof(buf), f)) {
		p = strchr(buf, '@');
		assert(p);
		*p = 0; /* remove @rev part */
		fprintf(f1, "%s\n", buf);
	}
	fclose(f);
	fclose(f1);
	sprintf(buf, "bk clean - < %s", cleanlist);
	if (system(buf) != 0) {
		printf("Undo aborted.\n");
		unlink(rmlist);
		unlink(cleanlist);
		exit(1);
	}
	unlink(cleanlist);
}
