#include "system.h"
#include "sccs.h"
#include <time.h>

/*
 * commit options
 */
typedef struct {
	u32	checklog:1;
	u32	quiet:1;
	u32	lod:1;
	u32	resync:1;
} c_opts;

extern	char	*editor, *bin, *BitKeeper;
extern	int	do_clean(char *, int);

private	void	make_comment(char *cmt, char *commentFile);
private int	do_commit(c_opts opts, char *sym, char *commentFile);
private	int	checkConfig();
void	cat(char *file);

/*
 *  Note: -f -s are internal options, do not document.
 *  XXX	-L is part of the lod work, not done yet.
 */
private char    *commit_help = "\n\
usage: commit  [-dFRqS:y:Y:]\n\n\
    -d		don't run interactively, just do the commit with the \n\
		default comment\n\
    -F		force a commit even if no pending deltas\n\
    -R		this tells commit that it is processing the resync directory\n\
    -q		run quietly\n\
    -S<sym>	set the symbol to <sym> for the new changeset\n\
    -y<comment>	set the changeset comment to <comment>\n\
    -Y<file>	set the changeset comment to the content of <file>\n";

int
commit_main(int ac, char **av)
{
	int	c, doit = 0, force = 0, resync = 0, getcomment = 1;
	char	buf[MAXLINE], s_cset[MAXPATH] = CHANGESET;
	char	commentFile[MAXPATH], pendingDeltas[MAXPATH];
	char	*sym = 0;
	c_opts	opts  = {1, 0 , 0, 0};

	if (ac > 1 && streq("--help", av[1])) {
		fputs(commit_help, stderr);
		return (1);
	}

	sprintf(commentFile, "%s/bk_commit%d", TMP_PATH, getpid());
	while ((c = getopt(ac, av, "dfFLRqsS:y:Y:")) != -1) {
		switch (c) {
		    case 'd': 	doit = 1; break;
		    case 'f':	opts.checklog = 0; break;
		    case 'F':	force = 1; break;
		    case 'L':	opts.lod = 1; break;
		    case 'R':	BitKeeper = "../BitKeeper/";
				opts.resync = 1;
				break;
		    case 's':	/* fall thru  */ 	/* internal option */
		    case 'q':	opts.quiet = 1; break;
		    case 'S':	sym = optarg; break;
		    case 'y':	doit = 1; getcomment = 0;
				make_comment(optarg, commentFile);
				break;
		    case 'Y':	doit = 1; getcomment = 0;
				strcpy(commentFile, optarg);
				break;
		}
	}
	if (sccs_cd2root(0, 0) == -1) {
		printf("Can not find root directory\n");
		exit(1);
	}
	unless(opts.resync) remark(opts.quiet);
	sprintf(pendingDeltas, "%s/bk_list%d", TMP_PATH, getpid());
	sprintf(buf, "bk sfiles -CA > %s", pendingDeltas);
	if (system(buf) != 0) {
		unlink(pendingDeltas);
		unlink(commentFile);
		gethelp("duplicate_IDs", "", stdout);
		exit(1);
	}
	if ((force == 0) && (size(pendingDeltas) == 0)) {
		unless (opts.quiet) fprintf(stderr, "Nothing to commit\n");
		unlink(pendingDeltas);
		unlink(commentFile);
		exit(0);
	}
	if (getcomment) {
		sprintf(buf,
		    "bk sccslog -C - < %s > %s", pendingDeltas, commentFile);
		system(buf);
	}
	unlink(pendingDeltas);
	do_clean(s_cset, SILENT);
	if (doit) exit(do_commit(opts, sym, commentFile));

	while (1) {
		printf("\n-------------------------------------------------\n");
		cat(commentFile);
		printf("-------------------------------------------------\n");
		printf("Use these comments (e)dit, (a)bort, (u)se? ");
		fflush(stdout);
		unless (getline(0, buf, sizeof(buf)) > 0) goto Abort;
		switch (buf[0]) {
		    case 'y':  /* fall thru */
		    case 'u':
			//exit(do_commit(quiet, checklog, lod, sym, commentFile));
			exit(do_commit(opts, sym, commentFile));
			break;
		    case 'e':
			sprintf(buf, "%s %s", editor, commentFile);
			system(buf);
			break;
		    case 'a':
Abort:			printf("Commit aborted.\n");
			unlink(pendingDeltas);
			unlink(commentFile);
			exit(1);
		}
	}
}

void
cat(char *file)
{
	MMAP	*m = mopen(file, "r");

	write(1, m->mmap, m->size);
	mclose(m);
}

private int
do_commit(c_opts opts, char *sym, char *commentFile)
{
	int	hasComment =  (exists(commentFile) && (size(commentFile) > 0));
	int	rc;
	char	buf[MAXLINE], sym_opt[MAXLINE] = "";
	char	s_cset[MAXPATH] = CHANGESET;
	char	commit_list[MAXPATH];
	sccs	*s;
	delta	*d;

	if (checkConfig() != 0) {
		unlink(commentFile);
		exit(1);
	}
	if (opts.checklog) {
		if (checkLog(opts.quiet, opts.resync) != 0) {
			unlink(commentFile);
			exit(1);
		}
	}
	sprintf(commit_list, "%s/commit_list%d", TMP_PATH, getpid());
	if (sym) sprintf(sym_opt, "-S\"%s\"", sym);
	sprintf(buf, "bk sfiles -C > %s", commit_list);
	system(buf);
	sprintf(buf, "bk cset %s %s %s %s%s < %s",
		opts.lod ? "-L": "", opts.quiet ? "-q" : "", sym_opt,
		hasComment? "-Y" : "", hasComment ? commentFile : "",
		commit_list);
	rc = system(buf);
	unlink(commentFile);
	unlink(commit_list);
	notify();
	s = sccs_init(s_cset, 0, 0);
	d = findrev(s, 0);
	logChangeSet(d->rev, opts.quiet);
	sccs_free(s);
	return (rc);
}

private	int
checkConfig()
{
	char	buf[MAXLINE], s_config[MAXPATH], g_config[MAXPATH];

	sprintf(s_config, "%setc/SCCS/s.config", BitKeeper);
	sprintf(g_config, "%setc/config", BitKeeper);
	unless (exists(s_config)) {
		gethelp("chkconfig_missing", bin, stdout);
		return (1);
	}
	if (exists(g_config)) do_clean(s_config, SILENT);
	get(s_config, SILENT, 0);
	sprintf(buf, "cmp -s %setc/config %s/bitkeeper.config", BitKeeper, bin);
	if (system(buf) == 0) {
		gethelp("chkconfig_inaccurate", bin, stdout);
		return (1);
	}
	return (0);
}

private	void
make_comment(char *cmt, char *commentFile)
{
	int fd;

	if ((fd = open(commentFile, O_CREAT|O_TRUNC|O_WRONLY, 0664)) == -1)  {
		perror("commit");
		exit(1);
	}
	write(fd, cmt, strlen(cmt));
	close(fd);
}
