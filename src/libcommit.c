#include "system.h"
#include "sccs.h"
#include <time.h>


extern char	*editor, *pager, *bin;
extern char	*BitKeeper;

private	void
do_prsdelta(char *file, char *rev, int flags, char *dspec, FILE *out)
{
	sccs *s;
	delta *d;

	s = sccs_init(file, INIT_NOCKSUM, NULL);
	assert(s);
	s->state &= ~S_SET;
	d = findrev(s, rev);
	sccs_prsdelta(s, d, flags, dspec, out);
	sccs_free(s);
}

void
do_clean(char *file, int flags)
{
	sccs *s;
	s = sccs_init(file, INIT_NOCKSUM, NULL);
	assert(s);
	sccs_clean(s, flags);
	sccs_free(s);
}

char *
logAddr()
{
	static char buf[MAXLINE];
	static char *logaddr = NULL;
	char	config[MAXPATH];
	FILE	*f1;

	if (logaddr) return logaddr;
	sprintf(config, "%s/bk_configY%d", TMP_PATH, getpid());
	sprintf(buf, "%setc/SCCS/s.config", BitKeeper);
	get(buf, SILENT|PRINT, config);
	assert(exists(config));

	f1 = fopen(config, "rt");
	assert(f1);
	while (fgets(buf, sizeof(buf), f1)) {
		if (strneq("logging:", buf, 8)) {
			char	*p, *q;
			for (p = &buf[8]; (*p == ' ' || *p == '\t'); p++);
			q = &p[1];
			while (strchr(" \t\n", *q) == 0) q++;
			*q = 0;
			logaddr = p;
			goto done;
		}
	}
	logaddr = "";
done:	fclose(f1);
	unlink(config);
	return logaddr;
}

void
sendConfig(char *to, int quiet, int quota)
{
	char	*dspec, subject[MAXLINE];
	char	config_log[MAXPATH], buf[MAXLINE];
	char	config[MAXPATH], aliases[MAXPATH];
	char	s_cset[MAXPATH] = CHANGESET;
	FILE *f, *f1;
	time_t tm;
	char 	*av[] = {
		"bk",
		"log",
		"http://www.bitkeeper.com/cgi-bin/logit",
		to,
		subject, 
		config_log,
		0
	};

	if (bkusers(1, 1, 0) <= quota) return;
	sprintf(config_log, "%s/bk_config_log%d", TMP_PATH, getpid());
	if (exists(config_log)) {
		fprintf(stderr, "Error %s already exist", config_log);
		exit(1);
	}
	status(0, config_log);

	dspec = "$each(:FD:){Proj:\\t(:FD:)}\\nID:\\t:KEY:";
	f = fopen(config_log, "ab");
	do_prsdelta(s_cset, "1.0", 0, dspec, f);
	fclose(f);

	f = fopen(config_log, "ab");
	fprintf(f, "User:\t%s\n", sccs_getuser());
	fprintf(f, "Host:\t%s\n", sccs_gethost());
	fprintf(f, "Root:\t%s\n", fullname(".", 0));
	tm = time(0);
	fprintf(f, "Date:\t%s", ctime(&tm));
	sprintf(config, "%s/bk_configX%d", TMP_PATH, getpid());
	sprintf(buf, "%setc/SCCS/s.config", BitKeeper);
	get(buf, SILENT|PRINT, config);
	f1 = fopen(config, "rt");
	while (fgets(buf, sizeof(buf), f1)) {
		if ((buf[0] == '#') || (buf[0] == '\n')) continue;
		fputs(buf, f);
	}
	fclose(f1);
	unlink(config);
	fprintf(f, "User List:\n");
	bkusers(0, 0, f);
	fprintf(f, "=====\n");
	fclose(f);
	sprintf(buf, "%setc/SCCS/s.aliases", BitKeeper);
	if (exists(buf)) {
		f = fopen(config_log, "ab");
		fprintf(f, "Alias  List:\n");
		sprintf(aliases, "%s/bk_aliasesX%d", TMP_PATH, getpid());
		sprintf(buf, "%setc/SCCS/s.aliases", BitKeeper);
		get(buf, SILENT|PRINT, aliases);
		f1 = fopen(aliases, "r");
		while (fgets(buf, sizeof(buf), f1)) {
			if ((buf[0] == '#') || (buf[0] == '\n')) continue;
			fputs(buf, f);
		}
		fclose(f1);
		unlink(aliases);
		fprintf(f, "=====\n");
		fclose(f);
	}

	if (getenv("BK_TRACE_LOG") && streq(getenv("BK_TRACE_LOG"), "YES")) {
		printf("sending config file...\n");
	}
	sprintf(subject, "BitKeeper config: %s", project_name());
	spawnvp_ex(_P_NOWAIT, av[0], av);
}

private void
header(FILE *f)
{
	char	parent_file[MAXPATH];
	char	buf[MAXPATH];
	char	*p;

	gethelp("version", 0, f);
	fprintf(f, "Repository %s:%s\n",
	    sccs_gethost(), fullname(".", 0));
	sprintf(parent_file, "%slog/parent", BitKeeper);
	if (exists(parent_file)) {
		FILE	*f1;

		f1 = fopen(parent_file, "rt");
		if (fgets(buf, sizeof(buf), f1)) {
			fputs("Parent repository ", f);
			fputs(buf, f);
		}
		fclose(f1);
	}
	p = project_name();
	if (p[0]) {
		fprintf(f,
		    "Changeset in %s by %s\n", p, sccs_getuser());
	} else {
		fprintf(f, "Changeset by %s\n", sccs_getuser());
	}
	fprintf(f, "\n");
}

void
logChangeSet(char *rev, int quiet)
{
	char	commit_log[MAXPATH], buf[MAXLINE], *p;
	char	subject[MAXLINE];
	char	start_rev[1024];
	FILE	*f;
	int	dotCount = 0, n;

	unless (streq("commit_and_maillog", getlog(NULL, quiet)))  return;

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
	header(f);
	fprintf(f, "---------------------------------\n");
	fclose(f);
	sprintf(buf, "bk sccslog -r%s ChangeSet >> %s", rev, commit_log);
	system(buf);
	sprintf(buf, "bk cset -r+ | bk sccslog - >> %s", commit_log);
	system(buf);
	f = fopen(commit_log, "ab");
	fprintf(f, "---------------------------------\n");
	fclose(f);
	sprintf(buf, "bk cset -c -r%s..%s >> %s", start_rev, rev, commit_log);
	system(buf);
	if (getenv("BK_TRACE_LOG") && streq(getenv("BK_TRACE_LOG"), "YES")) {
		printf("sending ChangeSet to %s...\n", logAddr());
	}
	sprintf(subject, "BitKeeper ChangeSet log: %s", project_name());
	mail(logAddr(), subject, commit_log);
	unlink(commit_log);
}

int
get(char *path, int flags, char *output)
{
	sccs *s;
	int ret;

	if (sccs_filetype(path) == 's') {
		s = sccs_init(path, SILENT, 0);
	} else {
		char	*p = name2sccs(path);

		s = sccs_init(p, SILENT, 0);
		free(p);
	}
	unless (s) return (-1);
	ret = sccs_get(s, 0, 0, 0, 0, flags, output);
	sccs_free(s);
	return (ret ? -1 : 0);
}

char *
project_name()
{
	sccs	*s;
	static	char pname[MAXLINE] = "";
	char	changeset[MAXPATH] = CHANGESET;

	if (pname[0]) return(pname); /* cached */
	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "project name: Can not find project root\n");
		return (pname);
	}
	s = sccs_init(changeset, 0, 0);
	if (s && s->text && (int)(s->text[0])  >= 1) strcpy(pname, s->text[1]);
	sccs_free(s);
	return (pname);
}

void
notify()
{
	char	buf[MAXPATH], notify_file[MAXPATH], notify_log[MAXPATH];
	char	subject[MAXLINE], *projectname;
	FILE	*f;

	sprintf(notify_file, "%setc/notify", BitKeeper);
	unless (exists(notify_file)) {
		char	notify_sfile[MAXPATH];

		sprintf(notify_sfile, "%setc/SCCS/s.notify", BitKeeper);
		if (exists(notify_sfile)) {
			get(notify_sfile, SILENT, "-");
			assert(exists(notify_file));
		}
	}
	if (size(notify_file) <= 0) return;
	sprintf(notify_log, "%s/bk_notify%d", TMP_PATH, getpid());
	f = fopen(notify_log, "wb");
	header(f);
	fclose(f);
	sprintf(buf, "bk sccslog -r+ ChangeSet >> %s", notify_log);
	system(buf);
	sprintf(buf, "bk cset -r+ | bk sccslog - >> %s", notify_log);
	system(buf);
	projectname = project_name();
	if (projectname[0]) {
		sprintf(subject, "BitKeeper changeset in %s by %s",
		    projectname, sccs_getuser());
	} else {
		sprintf(subject, "BitKeeper changeset by %s", sccs_getuser());
	}
	f = fopen(notify_file, "rt");
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
	int	i = -1;
	char	sendmail[MAXPATH], *mail;
	char	buf[MAXLINE];
	struct	utsname ubuf;
	char	*paths[] = {
		"/usr/bin",
		"/usr/sbin",
		"/usr/lib",
		"/usr/etc",
		"/etc",
		"/bin",
		0
	};

	if (strstr(project_name(), "BitKeeper Test repo") &&
	    (bkusers(1, 1, 0) <= 5)) {
		/* TODO : make sure our root dir is /tmp/.regression... */
		return;
	}

	while (paths[++i]) {
		sprintf(sendmail, "%s/sendmail", paths[i]);
		if (exists(sendmail)) {
			FILE *pipe, *f;

			sprintf(buf, "%s -i %s", sendmail, to);
			pipe = popen(buf, "w");
			fprintf(pipe, "To: %s\n", to);
			if (subject && *subject) {
				fprintf(pipe, "Subject: %s\n", subject);
			}
			f = fopen(file, "r");
			while (fgets(buf, sizeof(buf), f)) fputs(buf, pipe);
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
	} else if (strstr(ubuf.sysname, "Windows")) {
		sprintf(buf,
		    "%s/%s -s \"%s\" %s < %s", bin, mail, subject, to, file);
	}  else {
		sprintf(buf, "%s -s \"%s\" %s < %s", mail, subject, to, file);
	}
	system(buf);

}

void
remark(int quiet)
{
	if (exists("BitKeeper/etc/SCCS/x.marked")) return;
	unless (quiet) gethelp("consistency_check", "", stdout);
	system("bk cset -M1.0..");
	close(open("BitKeeper/etc/SCCS/x.marked", O_CREAT|O_TRUNC, 0664));
	unless(quiet) {
		printf("Consistency check completed, thanks for waiting.\n\n");
	}
}

void
status(int verbose, char *status_log)
{
	char	buf[MAXLINE], parent_file[MAXPATH];
	char	tmp_file[MAXPATH];
	FILE	*f, *f1;

	f = fopen(status_log, "a");
	fprintf(f, "Status for BitKeeper repository %s\n", fullname(".", 0));
	gethelp("version", 0, f);
	sprintf(parent_file, "%slog/parent", BitKeeper);
	if (exists(parent_file)) {
		fprintf(f, "Parent repository is ");
		f1 = fopen(parent_file, "r");
		while (fgets(buf, sizeof(buf), f1)) fputs(buf, f);
		fclose(f1);
	}
	unless (repository_lockers(0)) {
		if (isdir("PENDING")) {
			fprintf(f, "Pending patches\n");
		}
	}

	sprintf(tmp_file, "%s/bk_tmp%d", TMP_PATH, getpid());
	if (verbose) {
		f1 = fopen(tmp_file, "wb");
		assert(f1);
		bkusers(0, 0, f1);
		fclose(f1);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "User:\t%s", buf);
		}
		fclose(f1);
		sprintf(buf, "bk sfiles -x > %s", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Extra:\t%s", buf);
		}
		fclose(f1);
		sprintf(buf, "bk sfiles -cg > %s", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Modified:\t%s", buf);
		}
		fclose(f1);
		sprintf(buf, "bk sfiles -Cg > %s", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Uncommitted:\t%s", buf);
		}
		fclose(f1);
	} else {
		int i;

		fprintf(f, "%6d people have made deltas.\n", bkusers(1, 0, 0));
		sprintf(buf, "bk sfiles > %s", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		for (i = 0; fgets(buf, sizeof (buf), f1); i++);
		fclose(f1);
		fprintf(f, "%6d files under revision control.\n", i);
		sprintf(buf, "bk sfiles -x > %s", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		for (i = 0;  fgets(buf, sizeof (buf), f1); i++);
		fclose(f1);
		fprintf(f, "%6d files not under revision control.\n", i);
		sprintf(buf, "bk sfiles -c > %s", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		for (i = 0;  fgets(buf, sizeof (buf), f1); i++);
		fclose(f1);
		fprintf(f, "%6d files modified and not checked in.\n", i);
		sprintf(buf, "bk sfiles -C > %s", tmp_file);
		f1 = fopen(tmp_file, "rt");
		for (i = 0;  fgets(buf, sizeof (buf), f); i++);
		fclose(f1);
		fprintf(f,
		    "%6d files with checked in, but not committed, deltas.\n",
		    i);
	}
	unlink(tmp_file);
	fclose(f);
}

int
gethelp(char *help_name, char *bkarg, FILE *outf)
{
	char	buf[MAXLINE], pattern[MAXLINE];
	FILE	*f;
	int	found = 0;
	int	first = 1;

	if (bkarg == NULL) bkarg = "";
	sprintf(buf, "%s/bkhelp.txt", bin);
	f = fopen(buf, "rt");
	unless (f) {
		fprintf(stderr, "Unable to locate help file %s\n", buf);
		exit(1);
	}
	sprintf(pattern, "#%s\n", help_name);
	while (fgets(buf, sizeof(buf), f)) {
		if (streq(pattern, buf)) {
			found = 1;
			break;
		}
	}
	while (fgets(buf, sizeof(buf), f)) {
		char	*p;

		if (first && (buf[0] == '#')) continue;
		first = 0;
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
	return (found);
}

int
checkLog(int quiet, int resync)
{
	char	ans[MAXLINE], buf[MAXLINE], *r_opt;

	strcpy(buf, getlog(NULL, quiet));
	if (strneq("ask_open_logging:", buf, 17)) {
		gethelp("open_log_query", logAddr(), stdout);
		printf("OK [y/n]? ");
		fgets(ans, sizeof(ans), stdin);
		if ((ans[0] == 'Y') || (ans[0] == 'y')) {
			setlog(&buf[17]);
			return (0);
		} else {
			gethelp("log_abort", logAddr(), stdout);
			return (1);
		}
	} else if (strneq("ask_close_logging:", buf, 18)) {
		gethelp("close_log_query", logAddr(), stdout);
		printf("OK [y/n]? ");
		fgets(ans, sizeof(ans), stdin);
		if ((ans[0] == 'Y') || (ans[0] == 'y')) setlog(&buf[18]);
		sendConfig("config@openlogging.org", 1, 1);
		return (0);
	} else if (streq("need_seats", buf)) {
		gethelp("seat_info", "", stdout);
		return (1);
	} else if (streq("commit_and_mailcfg", buf)) {
		sendConfig("config@openlogging.org", 1, 1);
		return (0);
	} else if (streq("commit_and_maillog", buf)) {
		return (0);
	} else {
		fprintf(stderr, "unknown return code <%s>\n", buf);
		return (1);
	}
}
