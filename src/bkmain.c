#include "system.h"
#include "sccs.h" 

#define BK "bk"

extern char *bin;
char *find_wish();
char *find_perl5();

int setup_main(int, char **);
int commit_main(int, char **);
int status_main(int, char **);
int version_main(int, char **);
int help_main(int, char **);
int users_main(int, char **);
int parent_main(int, char **);
int clone_main(int, char **);
int send_main(int, char **);
int unwrap_main(int, char **);
int receive_main(int, char **);
int fix_main(int, char **);
int undo_main(int, char **);
int sendbug_main(int, char **);
int export_main(int, char **);
int setlog_main(int, char **);
int getlog_main(int, char **);
int gethelp_main(int, char **);
int logaddr_main(int, char **);
int sendconfig_main(int, char **);
int takepatch_main(int, char **);
int clean_main(int, char **);
int prs_main(int, char **);
int mv_main(int, char **);
int rm_main(int, char **);
int get_main(int, char **);
int delta_main(int, char **);
int sinfo_main(int, char **);
int sccscat_main(int, char **);
int cset_main(int, char **);
int diffs_main(int, char **);
int sccslog_main(int, char **);
int rmdel_main(int, char **);
int getuser_main(int, char **);
int gethost_main(int, char **);
int admin_main(int, char **);
int g2sccs_main(int, char **);
int key2rev_main(int, char **);
int lines_main(int, char **);
int sfiles_main(int, char **);
int sfio_main(int, char **);
int sids_main(int, char **);
int check_main(int, char **);
int chksum_main(int, char **);
int fdiff_main(int, char **);
int adler32_main(int, char **);
int lod_main(int, char **);
int range_main(int, char **);
int rechksum_main(int, char **);
int renumber_main(int, char **);
int stripdel_main(int, char **);
int smoosh_main(int, char **);
int undos_main(int, char **);
int what_main(int, char **);
int gca_main(int, char **);
int mtime_main(int, char **);
int zone_main(int, char **);
int isascii_main(int, char **);
int resyncwrap_main(int, char **);	/* XXX do we need this ? */
int r2c_main(int, char **);
int rename_main(int, char **);		/* XXX do we need this ? */
int pending_main(int, char **);

struct command cmdtbl[100] = {
	{"setup", setup_main },
	{"commit", commit_main},
	{"status", status_main},
	{"version", version_main},
	{"help", help_main},
	{"users", users_main},
	{"parent", parent_main},
	{"clone", clone_main},
	{"send", send_main},
	{"unwrap", unwrap_main},
	{"receive", receive_main},
	{"fix", fix_main},
	{"undo", undo_main},
	{"sendbug", sendbug_main},
	{"export", export_main},
	{"setlog", setlog_main},
	{"getlog", getlog_main},
	{"gethelp", gethelp_main},
	{"logaddr", logaddr_main},
	{"sendconfig", sendconfig_main},
	{"takepatch", takepatch_main},
	{"clean", clean_main},
	{"unedit", clean_main},	/* aliases */
	{"unget", clean_main},	/* aliases */
	{"unlock", clean_main},	/* aliases */
	{"prs", prs_main},
	{"mv", mv_main},
	{"sccsmv", mv_main},
	{"rm", rm_main},
	{"sccsrm", rm_main},
	{"get", get_main},
	{"edit", get_main},	/* aliases */
	{"co", get_main},
	{"delta", delta_main},
	{"ci", delta_main},
	{"new", delta_main},	/* aliases */
	{"sinfo", sinfo_main},
	{"info", sinfo_main},	/* aliases */
	{"sccscat", sccscat_main},
	{"cset", cset_main},
	{"diffs", diffs_main},
	{"sccslog", sccslog_main},
	{"rmdel", rmdel_main},
	{"getuser", getuser_main},
	{"gethost", gethost_main},
	{"admin", admin_main},
	{"g2sccs", g2sccs_main},
	{"key2rev", key2rev_main},
	{"lines", lines_main},
	{"sfiles", sfiles_main},
	{"sfio", sfio_main},
	{"sids", sids_main},
	{"check", check_main},
	{"chksum", chksum_main},
	{"fdiff", fdiff_main},
	{"adler32", adler32_main},
	{"lod", lod_main},
	{"range", range_main},
	{"rechksum", rechksum_main},
	{"renumber", renumber_main},
	{"stripdel", stripdel_main},
	{"smoosh", smoosh_main},
	{"undos", undos_main},
	{"what", what_main},
	{"gca", gca_main},
	{"mtime", mtime_main},
	{"zone", zone_main},
	{"isascii", isascii_main},
	{"resyncwrap", resyncwrap_main}, 
	{"r2c", r2c_main},
	{"rev2cset", r2c_main},
	{"rename", rename_main},
	{"pending", pending_main},
	0
};

main(int ac, char **av)
{
	int i, j, rc;

	platformInit(); 
	av[0] = basenm(av[0]);
	if (streq(av[0], BK)) {
		if (av[1] == NULL) {
			printf("usage $0 command '[options]' '[args]'\n");
			printf("Try bk help for help.\n");
			exit(0);
		} else if (streq(av[1], "-h")) {
			av++; ac--;
			return (help_main(ac, av));
		} else if (streq(av[1], "-r")) {
			if (isdir(av[2])) {
				chdir(av[2]);
				av++; ac--;
			} else {
				cd2root();
			}
			av++; ac--;
			if (streq(av[1], "-R")) {
				cd2root();
				av++; ac--;
			}
			unless (streq(av[1], "sfiles")) {
				char buf[MAXLINE];
				char *p, *q;

				sprintf(buf, "%sbk sfiles | %sbk", bin, bin);
				p = &buf[strlen(buf)];
				for (i = 1; q = av[i]; i++) {
					*p++ = ' ';
					*p++ = '\"';
					while (*q) {
						if (*q == '\"') *p = '\\';
						*p++ = *q++;
					}
					*p++ = '\"';
				}
				*p++ = ' ';
				*p++ = '-';
				*p = '\0';
				return(system(buf));
			}
		}
		if (streq(av[1], "-R")) {
			cd2root();
			av++; ac--;
		}
		av++; ac--;
	}

	/*
	 * Set up PATH variable
	 */
	unless (strneq(bin, getenv("PATH"), strlen(bin) - 1)) {
		char path[MAXLINE];
		int last = strlen(bin) -1;

		bin[last] = 0; /* trim tailing slash */
		sprintf(path, "PATH=%s:%s", bin, getenv("PATH"));
		bin[last] = '/'; /* restore tailing slash */
		putenv(path);

	}

	/*
	 * look up the internal command 
	 */
	for (i = 0; cmdtbl[i].name; i++) {
		if (streq(cmdtbl[i].name, av[0])){
			return((*(cmdtbl[i].func))(ac, av));
		}
	}

	/*
	 * Is it is a perl 4 script ?
	 */
	if (streq(av[0], "resolve") || streq(av[0], "pmerge")) {
		char *argv[100] ={ "perl", 0};

		for (i = 1, j = 0; av[j]; i++, j++) argv[i] = av[j];
		return (spawnvp_ex(_P_WAIT, av[0], av));;
	}

	/*
	 * Is it is a perl 5 script ?
	 */
	if (streq(av[0], "resync") ||
	    streq(av[0], "mkdiffs") ||
	    streq(av[0], "rcs2sccs")) {
		char cmd_path[MAXPATH];
		char *argv[100];

		argv[0] = find_perl5();
		sprintf(cmd_path, "%s%s", bin, av[0]);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) argv[i] = av[j];
		return (spawnvp_ex(_P_WAIT, argv[0], argv));;
	}

	/*
	 * Perl scrilpt aliases 
	 * This should be in pull_main()  & push_main()
	 */
	if (streq(av[0], "pull") ||
	    streq(av[0], "push")) {
		char *argv[100];
		char resync_path[MAXPATH];
		extern char *find_perl5();
		argv[0] = find_perl5();
		sprintf(resync_path, "%sresync", bin);
		argv[1] = resync_path;
		argv[2] = streq(av[0], "pull") ? "-A" : "-Ab";
		
		for (i = 3, j = 1; av[j]; i++, j++) {
			argv[i] = av[j];
		}
		cd2root();
		return (spawnvp_ex(_P_WAIT, argv[0], argv));;
	}

	/*
	 * Handle Gui script
	 */
	if (streq(av[0], "fm") ||
	    streq(av[0], "fm3") ||
	    streq(av[0], "citool") ||
	    streq(av[0], "sccstool") ||
	    streq(av[0], "fmtool") ||
	    streq(av[0], "fm3tool") ||
	    streq(av[0], "difftool") ||
	    streq(av[0], "helptool") ||
	    streq(av[0], "csettool") ||
	    streq(av[0], "renametool")) {
		char cmd_path[MAXPATH];
		char *argv[100];

		argv[0] = find_wish();
		sprintf(cmd_path, "%s%s", bin, av[0]);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) argv[i] = av[j];
		return (spawnvp_ex(_P_WAIT, argv[0], argv));;
	}
	
	/*
	 * Try external command
	 */
	rc = spawnvp_ex(_P_WAIT, av[0], av);
	if (rc != 0) {
		fprintf(stderr, "Cammnad %s failed; rc=%d\n", av[0], rc);
	}
	return (rc);
}

char *
find_wish()
{
	char buf[MAXLINE];
	char *p, *s;
	char path[MAXLINE];
	static char wish_path[MAXPATH];
	int more = 1;

	p  = getenv("PATH");
	if (p) {;
		sprintf(path, "%s:/usr/local/bin", p);
		localName2bkName(path, path);
	} else {
		strcpy(path, "/usr/local/bin");
	}
	p = path;
	while (more) {
		for (s = p; (*s != PATH_DELIM) && (*s != '\0');  s++);
		if (*s == '\0') more = 0;
		*s = '\0';
#ifdef WIN32
		sprintf(wish_path, "%s/wish81.exe", p);
		if (exists(wish_path)) return (wish_path);
#else
		sprintf(wish_path, "%s/wish8.2", p);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish8.0", p);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish", p);
		if (exists(wish_path)) return (wish_path);
#endif
		p = ++s;
	}
	fprintf(stderr, "Can not find wish to run\n");
	exit(1);
}

char *
find_perl5()
{
	char buf[MAXLINE];
	char *p, *s;
	char path[MAXLINE];
	static char perl_path[MAXPATH];
	int more = 1;

	p  = getenv("PATH");
	if (p) {;
		sprintf(path, "%s:/usr/local/bin", p);
		localName2bkName(path, path);
	} else {
		strcpy(path, "/usr/local/bin");
	}
	p = path;
	while (more) {
		for (s = p; (*s != PATH_DELIM) && (*s != '\0');  s++);
		if (*s == '\0') more = 0;
		*s = '\0';
#ifdef WIN32
		sprintf(perl_path, "%s/perl.exe", p);
		unless (exists(perl_path)) goto next;
		return(perl_path); /* win32 perl is version 5 */
#else
		sprintf(perl_path, "%s/perl", p);
		unless (executable(perl_path)) goto next;
#endif
		sprintf(buf, "%s -v | grep 'version 5.0' > %s",
			perl_path, DEV_NULL);
		if (system(buf) == 0)	return(perl_path);
next:		p = ++s;
	}
	fprintf(stderr, "Can not find perl5 to run\n");
	exit(1);
}

