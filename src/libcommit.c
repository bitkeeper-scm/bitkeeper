#include "system.h"
#include "sccs.h"
#include <sys/utsname.h>
#include <time.h>

char *editor = 0, *pager = 0, *bin = 0; 
char *bk_dir = "BitKeeper/";
int resync = 0, quiet = 0;

char *
logAddr()
{
	static char buf[MAXLINE];
	static char *logaddr = NULL;
	FILE *pipe;

	if (logaddr) return logaddr;
	sprintf(buf, "%sget -qp %setc/config", bin, bk_dir);
	pipe = popen(buf, "r");
	assert(pipe);
	while (fgets(buf, sizeof(buf), pipe)) {
		if (strneq("logging:", buf, 8)) {
			char *p, *q;
			for (p = &buf[8]; (*p == ' ' || *p == '\t'); p++);
			q = &p[1];
			while (strchr(" \t\n", *q) == 0) q++;
			*q = 0;
			pclose(pipe);
			logaddr = p;
			return logaddr;
		}
	}
	pclose(pipe);
	logaddr = "";
	return logaddr;
}

void
sendConfig(char *to)
{
	char *dspec;
	char config_log[MAXPATH], buf[MAXLINE];
	FILE *f, *pipef;
	time_t tm;
	extern int bkusers();

	if (bkusers(1, 1) <= 1) return;
	sprintf(config_log, "%s/bk_config%d", TMP_PATH, getpid());
	if (exists(config_log)) {
		fprintf(stderr, "Error %s already exist", config_log);
		exit(1);
	}
	status(0, config_log);
	dspec = "$each(:FD:){Project:\\t(:FD:)}\\nChangeSet ID:\\t:LONGKEY:";
	sprintf(buf, "%sprs -hr1.0 -d'%s' ChangeSet >> %s",
							bin, dspec, config_log);
	system(buf);
	f = fopen(config_log, "a");
	fprintf(f, "User:\t%s\n", sccs_getuser());
	fprintf(f, "Host:\t%s\n", sccs_gethost());
	fprintf(f, "Root:\t%s\n", fullname(".", 0));
	tm = time(0);
	fprintf(f, "Date:\t%s", ctime(&tm)); 
	sprintf(buf, "%sget -pq %setc/config", bin, bk_dir);
	pipef = popen(buf, "r");
	while (fgets(buf, sizeof(buf), pipef)) {
		if ((buf[0] == '#') || (buf[0] == '\n')) continue;
		fputs(buf, f);
	}
	pclose(pipef);
	fprintf(f, "User List:\n");
	fclose(f);
	sprintf(buf, "%sbkusers >> %s", bin, config_log);
	system(buf);
	f = fopen(config_log, "a");
	fprintf(f, "=====\n");
	fclose(f);
	sprintf(buf, "%setc/SCCS/s.aliases", bk_dir);
	if (exists(buf)) {
		f = fopen(config_log, "a");
		fprintf(f, "Alias  List:\n");
		sprintf(buf, "%sget -pq %setc/aliases", bin, bk_dir);
		pipef = popen(buf, "r");
		while (fgets(buf, sizeof(buf), pipef)) {
			if ((buf[0] == '#') || (buf[0] == '\n')) continue;
			fputs(buf, f);
		}
		pclose(pipef);
		fprintf(f, "=====\n");
		fclose(f);
	}

	if (getenv("BK_TRACE_LOG") && streq(getenv("BK_TRACE_LOG"), "YES")) {
		printf("sending config file...\n");
	}
	sprintf(buf, "BitKeeper config: %s", project_name());
	mail(to, buf, config_log);
	unlink(config_log);
}


void
logChangeSet(char *rev)
{
	char commit_log[MAXPATH], buf[MAXLINE], *p;
	char start_rev[1024];
	FILE *pipe, *f;
	int dotCount = 0, n;

	sprintf(buf, "%sgetlog %s", bin, resync ? "-R" : "");
	pipe = popen(buf, "r");
	fgets(buf, sizeof(buf), pipe);
	chop(buf);
	pclose(pipe);
	unless (streq("commit_and_maillog", buf))  return;

	// XXX TODO  Determine if this is the first rev where logging is active.
	// if so, send all chnage log from 1.0

	strcpy(start_rev, rev);
	p = start_rev;
	while (*p) { if (*p++ == '.') dotCount++; }
	p--;
	while (*p != '.') p--;
	p++;
	if (dotCount == 4) {
		n = atoi(p) - 5;
	} else {
		n = atoi(p) - 10;
	}
	if (n < 0) n = 1;
	sprintf(p, "%d", n);
	sprintf(commit_log, "%s/commit_log%d", TMP_PATH, getpid());
	f = fopen(commit_log, "wb");
	fprintf(f, "---------------------------------\n");
	sprintf(buf, "%ssccslog -r%s ChangeSet >> %s", bin, rev, commit_log);
	system(buf);
	sprintf(buf, "%scset -r+ | %ssccslog - >> %s", bin, bin, commit_log);
	system(buf);
	fprintf(f, "---------------------------------\n");
	sprintf(buf, "%scset -c -r%s..%s >> %s",
					bin, start_rev, rev, commit_log);
	system(buf);
	if (getenv("BK_TRACE_LOG") && streq(getenv("BK_TRACE_LOG"), "YES")) {
		printf("sending ChangeSet to %s...\n", logAddr());
	}
	mail(logAddr(), project_name(), commit_log);
	unlink(commit_log);
}

get(char *path, int flags, char *output)
{
	sccs *s = sccs_init(path, SILENT, 0);
	int ret;

	unless (s) return (-1);
	ret = sccs_get(s, 0, 0, 0, 0, flags, output);
	sccs_free(s);
	return (ret ? -1 : -0);
}

char *
project_name()
{
	sccs *s;
	static char pname[MAXLINE] = "";
	cd2root();
	s = sccs_init(CHANGESET, 0, 0);
	if (s && s->text && (int)(s->text[0])  >= 1) strcpy(pname, s->text[1]);
	sccs_free(s);
	return (pname);
}

void
notify()
{
	char buf[MAXPATH], notify_file[MAXPATH], notify_log[MAXPATH];
	char parent_file[MAXPATH], subject[MAXLINE], *projectname;
	FILE *f;

	sprintf(notify_file, "%setc/notify", bk_dir);
	unless (exists(notify_file)) {
		char notify_sfile[MAXPATH];	
		sprintf(notify_sfile, "%setc/SCCS/s.notify", bk_dir);
		if (exists(notify_sfile)) {
			get(notify_sfile, SILENT, "-");
			assert(exists(notify_file));
		}
	}
	if (size(notify_file) <= 0) return;
	sprintf(notify_log, "%s/bk_notify%d", TMP_PATH, getpid());
	f = fopen(notify_log, "w");
	gethelp("version", 0, f);
	fprintf(f, "BitKeeper repository %s : %s\n",
					sccs_gethost(), fullname(".", 0));
	sprintf(parent_file, "%slog/parent", bk_dir);
	if (exists(buf)) {
		FILE *f1;
		f1 = fopen(parent_file, "r");
		while (fgets(buf, sizeof(buf), f1)) fputs(buf, f);
		fclose(f1);
	}
	fprintf(f, "\n");
	fclose(f);
	sprintf(buf, "%ssccslog -r+ ChangeSet >> %s", bin, notify_log);
	system(buf);
	sprintf(buf, "%scset -r+ | %ssccslog - >> %s", bin, bin, notify_log);
	system(buf);
	projectname = project_name();
	if (projectname[0]) {
		sprintf(subject, "BitKeeper changeset in %s by %s",
						projectname, sccs_getuser());
	} else {
		sprintf(subject, "BitKeeper changeset by %s", sccs_getuser());
	}
	f = fopen(notify_file, "r");
	while (fgets(buf, sizeof(buf), f)) {
		chop(buf);
		mail(buf, subject, notify_log);
	}
	fclose(f);
	unlink(notify_log);
}

void
mail(char *to, char *subject, char *file)
{
	int i = -1;
	char sendmail[MAXPATH], *mail;
	char buf[MAXLINE];
	struct utsname ubuf;
	char *paths[] = {
		"/usr/bin",
		"/usr/sbin",
		"/usr/lib",
		"/usr/etc",
		"/etc",
		"/bin",
		0
	};

	if (streq("BitKeeper Test repository", project_name()) &&
	    (bkusers(1,1) <= 5)) {
		return;
	}
	while (paths[++i]) {
		sprintf(sendmail, "%s/sendmail", paths[i]);
		if (exists(sendmail)) {
			FILE *pipe, *f;

			sprintf(buf, "%s -i %s", sendmail , to);
			pipe = popen(buf, "w"); 
			fprintf(pipe, "To: %s\n", to);
			if (subject && *subject) {
				fprintf(pipe, "Subject: %s\n", subject);
			}
			f = fopen(file, "r");
			while (fgets(buf, sizeof( buf), f)) fputs(buf, pipe);
			fclose(f);
			pclose(pipe);
			return;
		}
	}

	mail = exists("/usr/bin/mailx") ? "/usr/bin/mailx" : "mail";

	/* We know that the ``mail -s "$SUBJ"'' form doesn't work on IRIX */
	assert(uname(&ubuf) == 0);
	if (strstr(ubuf.sysname, "IRIX")) {
		sprintf(buf, "%s %s < %s", mail, to, file);
	}  else {
		sprintf(buf, "%s -s %s %s < %s", mail, subject, to, file);
	}
	system(buf);

}

void
remark(int quiet)
{
	char buf[MAXLINE];

	if (exists("BitKeeper/etc/SCCS/x.marked")) return;
	unless (quiet) gethelp("consistency_check", "", stdout);
	sprintf(buf, "%scset -M1.0..", bin);
	system(buf);
	close(open("BitKeeper/etc/SCCS/x.marked", O_CREAT|O_TRUNC, 0664));
	unless(quiet) {
		printf("Consistency check completed, thanks for waiting.\n\n");
	}
}

void
status(int verbose, char *status_log)
{
	char buf[MAXLINE], parent_file[MAXPATH];
	FILE *f, *f1;
	extern int bkusers();

	f = fopen(status_log, "a");
	fprintf(f,"Status for BitKeeper repository %s\n",  fullname(".", 0));
	gethelp("version", 0, f);
	sprintf(parent_file, "%slog/parent", bk_dir);
	if (exists(parent_file)) {
		fprintf(f, "Parent repository is ");
		f1 = fopen(parent_file, "r");
		while (fgets(buf, sizeof(buf), f1)) fputs(buf, f);	
		fclose(f1);
	}
	if (isdir( "RESYNC")) {
		fprintf(f, "Resync in progress\n");
	} else if (isdir("PENDING")) {
		fprintf(f, "Pending patches\n");
	}

	if (verbose) {
		sprintf(buf, "%sbkusers", bin);
		f1 = popen(buf, "r");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "User:\t%s", buf);
		}
		pclose(f1);
		sprintf(buf, "%ssfiles -x", bin);
		f1 = popen(buf, "r");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Extra:\t%s", buf);
		}
		pclose(f1);
		sprintf(buf, "%ssfiles -cg", bin);
		f1 = popen(buf, "r");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Modified:\t%s", buf);
		}
		pclose(f1);
		sprintf(buf, "%ssfiles -Cg", bin);
		f1 = popen(buf, "r");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Uncommitted:\t%s", buf);
		}
		pclose(f1);
	} else {
		int i;

		fprintf(f, "%d people have made deltas.\n", bkusers(1, 0));
		sprintf(buf, "%ssfiles", bin);
		f1 = popen(buf, "r");
		for(i = 0;  fgets(buf, sizeof(buf), f1); i++);
		pclose(f1);
		fprintf(f, "%d files under revision control.\n", i);
		sprintf(buf, "%ssfiles -x", bin);
		f1 = popen(buf, "r");
		for(i = 0;  fgets(buf, sizeof(buf), f1); i++);
		pclose(f1);
		fprintf(f, "%d files not under revision control.\n", i);
		sprintf(buf, "%ssfiles -c", bin);
		f1 = popen(buf, "r");
		for(i = 0;  fgets(buf, sizeof(buf), f1); i++);
		pclose(f1);
		fprintf(f, "%d files modified and not checked in.\n", i);
		sprintf(buf, "%ssfiles -C", bin);
		f1 = popen(buf, "r");
		for(i = 0;  fgets(buf, sizeof(buf), f); i++);
		pclose(f1);
		fprintf( f,
		   "%d files with checked in, but not committed, deltas.\n", i);
	}
	fclose(f);
}

void
gethelp(char *help_name, char *bkarg, FILE *outf)
{
	char buf[MAXLINE], pattern[MAXLINE];
	FILE *f;

	if (bkarg == NULL) bkarg = "";
	sprintf(buf, "%sbkhelp.txt", bin);
	f = fopen(buf, "r");
	assert(f);
	sprintf(pattern, "#%s\n", help_name);
	while (fgets(buf, sizeof(buf), f)) {
		if (streq(pattern, buf)) break;
	}
	while (fgets(buf, sizeof(buf), f)) {
		char *p;

		if (streq("$\n", buf)) break;
		p = strstr(buf, "#BKARG#");
		if (p) {
			*p = 0;
			fputs(buf, outf);
			fputs(bkarg, outf);
			fputs(&p[7], outf);
		} else {
			fputs(buf, outf);
		}
	}
	fclose(f);
}


void 
cd2root()
{
	char *root = sccs_root(0);
	
	if (chdir(root) != 0) {
		perror("chdir");
		exit(1);
	}
	free(root);
}

void
platformInit()
{
	char *paths[] = {
		"/usr/libexec/bitkeeper/",
		"/usr/lib/bitkeeper/",
		"/usr/bitkeeper/",
		"/opt/bitkeeper/",
		"/usr/local/bitkeepe/",
		"/usr/local/bin/bitkeeper/",
		"/usr/bin/bitkeeper/",
		0
	};

	char buf[MAXPATH];
	int i = -1;

	if ((editor = getenv("EDITOR")) == NULL) editor="vi";
	if ((pager = getenv("PAGER")) == NULL) pager="more";

#define TAG_FILE "sccslog"
	if ((bin = getenv("BK_BIN")) != NULL) {
		char buf[MAXPATH];
		sprintf(buf, "%s%s", bin, TAG_FILE);
		if (exists(buf)) return;
	}

	while (paths[++i]) {
		sprintf(buf, "%s%s", paths[i], TAG_FILE);
		if (exists(buf)){
			bin =  strdup(paths[i]);
			sprintf(buf, "BK_BIN=%s", paths[i]);
			putenv(buf);
			sprintf(buf, "PATH=%s:/usr/xpg4/bin:%s",
					    paths[i], getenv("PATH"));
			putenv(buf);
		}
	}
}

