#include "system.h"
#include "sccs.h" 
#include <time.h>

extern char *editor, *pager, *bin;
extern char *bk_dir;
extern int resync, quiet;

char commit_file[MAXPATH], list[MAXPATH];
int force = 0, lod = 0;
int checklog = 1, getcomment = 1;
char *sym = 0;

void make_comment(char *cmt);

commit_main(int ac, char **av)
{
	int c, doit = 0;
	int rc;
	char buf[MAXLINE];
	char s_cset[MAXPATH] = CHANGESET;

	platformInit();

	sprintf(commit_file, "%s/bk_commit%d", TMP_PATH, getpid());
	while ((c = getopt(ac, av, "dfFLRqsS:y:Y:")) != -1) {
		switch (c) {
		    case 'd': 	doit = 1; break;
		    case 'f':	checklog = 0; break;
		    case 'F':	force = 1; break;
		    case 'L':	lod = 1; break;
		    case 'R':	resync = 1; 
				bk_dir = "../BitKeeper/";
				break;
		    case 's':	/* fall thru  */
		    case 'q':	quiet = 1; break;
		    case 'S':	sym = optarg; break;
		    case 'y':	doit = 1; getcomment = 0;
				make_comment(optarg);
				break;
		    case 'Y':	doit = 1; getcomment = 0;
				strcpy(commit_file, optarg);
				break;
		}
	}
	cd2root();
	unless(resync) remark(quiet);
	sprintf(list, "%s/bk_list%d", TMP_PATH, getpid());
	sprintf(buf, "%sbk sfiles -CA > %s", bin, list);
	if (system(buf) != 0) {
		unlink(list);
		unlink(commit_file);
		gethelp("duplicate_IDs", "", stdout);
		exit (1);
	}
	if ((force == 0) && (size(list) == 0)) {
		unless (quiet) fprintf(stderr, "Nothing to commit\n");
		unlink(list);
		unlink(commit_file);
		exit (0);
	}
	if (getcomment) {
		sprintf(buf,
			"%sbk sccslog -C - < %s > %s", bin, list, commit_file);
		system(buf);
	}
	unlink(list);
	do_clean(s_cset, SILENT);
	if (doit) exit (do_commit());

	while (1) {
		printf("\n-------------------------------------------------\n");
		cat(commit_file);
		printf("-------------------------------------------------\n");
		printf("Use these comments (e)dit, (a)bort, (u)se? ");
		fflush(stdout);
		unless (getline(0, buf, sizeof(buf)) > 0) goto Abort;
		switch (buf[0]) {
		    case 'y':  /* fall thru */
		    case 'u':
			exit(do_commit()); break;
		    case 'e':
			sprintf(buf, "%s %s", editor, commit_file);
			system(buf);
			break;
		    case 'a':	
Abort:			printf("Commit aborted.\n");
			unlink(list);
			unlink(commit_file);
			exit (1);
		}
		
	}
}

cat(char *file)
{
	MMAP	*m = mopen(file, "r");

	write(1, m->mmap, m->size);
	mclose(m);
}

do_commit()
{
	int hasComment =  (exists(commit_file) && (size(commit_file) > 0));
	char buf[MAXLINE], sym_opt[MAXLINE] = "";
	char s_cset[MAXPATH] = CHANGESET;
	char commit_list[MAXPATH];
	int rc;
	sccs *s;
	delta *d;

	if (checkConfig() != 0) {
		unlink(list);
		unlink(commit_file);
		exit(1);
	}
	if (checklog) {
		if (checkLog() != 0) {
			unlink(commit_file);
			exit (1);
		}
	}
	sprintf(commit_list, "%s/commit_list%d", TMP_PATH, getpid());
	if (sym) sprintf(sym_opt, "-S\"%s\"", sym);
	sprintf(buf, "%sbk sfiles -C > %s", bin, commit_list);
	system(buf);
	sprintf(buf, "%sbk cset %s %s %s %s%s < %s",
		bin, lod ? "-L": "", quiet ? "-q" : "", sym_opt,
		hasComment? "-Y" : "", hasComment ? commit_file : "", commit_list);
	rc = system(buf);
	unlink(list);
	unlink(commit_file);
	unlink(commit_list);
	notify();
	s = sccs_init(s_cset, 0, 0);
	d = findrev(s, 0);
	logChangeSet(d->rev);
	sccs_free(s);
	return(rc);
}

checkConfig()
{
	char buf[MAXLINE];
	char s_config[MAXPATH];
	char g_config[MAXPATH];
	
	sprintf(s_config, "%setc/SCCS/s.config", bk_dir);
	sprintf(g_config, "%setc/config", bk_dir);
	unless (exists(s_config)) {
		gethelp("chkconfig_missing", bin, stdout);
		return 1;
	}
	if (exists(g_config)) do_clean(s_config, SILENT);
	get(s_config, SILENT, 0);
	sprintf(buf, "cmp -s %setc/config %sbitkeeper.config", bk_dir, bin);
	if (system(buf) == 0) {
		gethelp("chkconfig_inaccurate", bin, stdout);
		return 1;
	}
	return 0;
}

void
make_comment(char *cmt)
{
	int fd;

	if ((fd = open(commit_file, O_CREAT|O_TRUNC|O_WRONLY, 0664)) == -1)  {
		perror("commit");
		exit (1);
	}
	write(fd, cmt, strlen(cmt));
	close(fd);
}

