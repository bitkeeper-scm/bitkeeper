#include "system.h"
#include "sccs.h" 

extern char *bin;

main(int ac,  char **av)
{
	int c;
	int force = 0;
	char buf[MAXLINE], rmlist[MAXPATH], cleanlist[MAXPATH];
	char mvlist[MAXPATH], renamelist[MAXPATH], undolist[MAXPATH];
	char *qflag = "", *vflag = "-v";
	char *p, *csets;
	FILE *f, *f1;
#define LINE "---------------------------------------------------------\n"
#define BK_TMP  "BitKeeper/tmp"
#define BK_UNDO "BitKeeper/tmp/undo"

	platformInit();  

	while ((c = getopt(ac, av, "fq")) != -1) {
		switch (c) { 
		    case 'f': force  =  1; break;
		    case 'q': qflag = "-q"; vflag = ""; break;
		    default :
			fprintf(stderr, "unknow option <%c>\n", c);
			exit(1);
		}
	}
	cd2root();
	csets = av[optind];
	unless (csets) {
		fprintf(stderr, "usage bk undo cset-revision\n");
		exit(1);
	}
	if (av[optind + 1] != NULL) {
		fprintf(stderr,
		"undo: specify multiple changesets as a set, like 1.2,1.3\n"); 
		exit(1);
	}

	sprintf(rmlist, "%s/bk_rmlist%d",  TMP_PATH, getpid());
	clean_file(rmlist, csets);

	sprintf(undolist, "%s/bk_undolist%d",  TMP_PATH, getpid());
	sprintf(buf, "%sstripdel -Ccr%s ChangeSet 2> %s", bin, csets, undolist);
	if (system(buf) != 0) {
		gethelp("undo_error", bin, stdout);
		sprintf(buf, "cat %s", undolist);
		system(buf);
		unlink(undolist);
		exit(1);
	}

	unless (force) {
		printf(LINE);
		sprintf(buf, "cat %s", rmlist);
		system(buf);
		printf(LINE);
		printf("Remove these [y/n]? ");
		fgets(buf, sizeof(buf), stdin);
		if ((buf[0] != 'y') && (buf[0] != 'Y')) {
			unlink(rmlist);
			exit(0);
		}
	}

	unless (isdir(BK_TMP)) {
		mkdirp(BK_TMP);
		//chmod(BK_TMP, 0777);
	}
	//if (exists(BK_UNDO)) unlink(BK_UNDO):
	sprintf(buf, "%scset %s -ffm%s > %s", bin, vflag, csets, BK_UNDO);
	system(buf);

	
	sprintf(mvlist, "%s/bk_mvlist%d",  TMP_PATH, getpid());
	f1 = fopen(mvlist, "wb");
	f = fopen(rmlist, "rt");
	assert(f);
	assert(f1);
	while (fgets(buf, sizeof(buf), f)) {
		char buf1[MAXLINE];

		chop(buf);
		p = strchr(buf, '@');
		assert(p);
		*p++ = 0;
		fprintf(f1, "%s\n", buf);
		sprintf(buf1, "%sstripdel %s -Cr%s %s", bin, qflag, p, buf);
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
	 */
	sprintf(renamelist, "%s/bk_renaemlist%d",  TMP_PATH, getpid());
	sprintf(buf, "%sprs -hr+ -d':PN: :SPN:' - < %s > %s",
						bin, mvlist, renamelist);
	system(buf);
	f = fopen(renamelist, "rt");
	while (fgets(buf, sizeof(buf), f)) {
		char buf1[MAXLINE];

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
				rename(buf, p);
			}
		}
		sprintf(buf1, "%srenumber %s", bin, p);
		system(buf1);
	}
	unlink(mvlist); unlink(rmlist); unlink(undolist); unlink(renamelist);
	if (streq(qflag, "")) {
		printf("Patch containing these undone deltas left in %s",
								       BK_UNDO);
		printf("Running consistency check...\n");
	}
	sprintf(buf, "%ssfiles -r", bin);
	system(buf);
	sprintf(buf, "bk -r check -a");
	system(buf);
	return (0);
}

clean_file(char *rmlist, char *csets)
{
	FILE *f, *f1;
	char cleanlist[MAXPATH];
	char buf[MAXLINE];
	char *p;

	sprintf(cleanlist, "%s/bk_cleanlist%d",  TMP_PATH, getpid());
	sprintf(buf, "%scset -ffl%s > %s", bin, csets, rmlist);
	system(buf);
	if (size(rmlist) == 0) {
		printf("undo: nothing to undo in \"%s\"\n", csets);
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
		fputs(buf, f1);
	}
	fclose(f);
	fclose(f1);
	sprintf(buf, "%sclean - < %s", bin, cleanlist);
	if (system(buf) != 0) {
		printf("Undo aborted.\n");
		unlink(rmlist);
		unlink(cleanlist);
		exit(1);
	}
	unlink(cleanlist);
}
