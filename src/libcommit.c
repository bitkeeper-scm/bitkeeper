#include "system.h"
#include "sccs.h"
#include <time.h>


extern char	*editor, *pager, *bin;
extern char	*BitKeeper;

void
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
	unless (s && s->tree) {
		if (s) sccs_free(s);
		return (-1);
	}
	ret = sccs_get(s, 0, 0, 0, 0, flags, output);
	sccs_free(s);
	return (ret ? -1 : 0);
}

char *
package_name()
{
	MDBM	*m;
	char	*n;
	static	char *name = 0;

	if (name) return (name);
	unless (m = loadConfig(".", 0)) return ("");
	unless (n = mdbm_fetch_str(m, "description")) {
		mdbm_close(m);
		return ("");
	}
	mdbm_close(m);
	name = (strdup)(n);	/* hide it from purify */
	return (name);
}

void
notify()
{
	char	buf[MAXPATH], notify_file[MAXPATH], notify_log[MAXPATH];
	char	subject[MAXLINE], *packagename;
	FILE	*f;
	int	ret;
	pid_t 	pid;

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
	sprintf(buf, "bk sccslog -r+ ChangeSet > %s", notify_log);
	system(buf);
	sprintf(buf, "bk cset -r+ | bk sccslog - >> %s", notify_log);
	system(buf);
	f = fopen(notify_log, "ab");
	status(0, f);
	fclose(f);
	packagename = package_name();
	if (packagename[0]) {
		sprintf(subject, "BitKeeper changeset in %s by %s",
		    packagename, sccs_getuser());
	} else {
		sprintf(subject, "BitKeeper changeset by %s", sccs_getuser());
	}
	f = fopen(notify_file, "rt");
	while (fgets(buf, sizeof(buf), f)) {
		chop(buf);
		pid = mail(buf, subject, notify_log);
		fprintf(stdout, "Waiting for mailer...\n");
		fflush(stdout); /* needed for citool */
		waitpid(pid, &ret, 0);
		if (WEXITSTATUS(ret) != 0) {
			fprintf(stdout, "cannot notify %s\n", buf);
			fflush(stdout); /* needed for citool */
		}
	}
	fclose(f);
	unlink(notify_log);
}

void
remark(int quiet)
{
	if (exists("BitKeeper/etc/SCCS/x.marked")) return;
	unless (quiet) gethelp("consistency_check", 0, 0, stdout);
	system("bk cset -M1.0..");
	close(open("BitKeeper/etc/SCCS/x.marked", O_CREAT|O_TRUNC, 0664));
	unless(quiet) {
		printf("Consistency check completed, thanks for waiting.\n\n");
	}
}

void
status(int verbose, FILE *f)
{
	char	buf[MAXLINE], parent_file[MAXPATH];
	char	tmp_file[MAXPATH];
	FILE	*f1;

	fprintf(f, "Status for BitKeeper repository %s:%s\n",
	    sccs_gethost(), fullname(".", 0));
	gethelp("version", bk_model(), 0, f);
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
		bkusers(0, 0, 0, f1);
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

		fprintf(f,
		    "%6d people have made deltas.\n", bkusers(1, 0, 0, 0));
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
		system(buf);
		f1 = fopen(tmp_file, "rt");
		for (i = 0;  fgets(buf, sizeof (buf), f1); i++);
		fclose(f1);
		fprintf(f,
		    "%6d files with checked in, but not committed, deltas.\n",
		    i);
	}
	unlink(tmp_file);
}

int
gethelp(char *help_name, char *bkarg, char *prefix, FILE *outf)
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
		if (prefix) fputs(prefix, outf);
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
mkconfig(FILE *out)
{
	FILE	*in;
	int	found = 0;
	int	first = 1;
	char	buf[200], pattern[200];

	sprintf(buf, "%s/bkhelp.txt", bin);
	unless (in = fopen(buf, "rt")) {
		fprintf(stderr, "Unable to locate help file %s\n", buf);
		return (-1);
	}
	gethelp("config_preamble", 0, "# ", out);
	fputs("\n", out);
	while (fgets(buf, sizeof(buf), in)) {
		if (streq("#config_template\n", buf)) {
			found = 1;
			break;
		}
	}
	unless (found) {
		fclose(in);
		return (-1);
	}
	while (fgets(buf, sizeof(buf), in)) {
		if (first && (buf[0] == '#')) continue;
		first = 0;
		if (streq("$\n", buf)) break;
		chop(buf);
		sprintf(pattern, "config_%s", buf);
		gethelp(pattern, 0, "# ", out);
		fprintf(out, "%s: \n", buf);
	}
	fclose(in);
	return (0);
}
