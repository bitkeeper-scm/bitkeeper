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
	name = (strdup)(n);	/* hide it from purify */
	mdbm_close(m);
	return (name);
}

void
notify()
{
}

void
remark(int quiet)
{
	int i;

	if (exists("BitKeeper/etc/SCCS/x.marked")) return;
	unless (quiet) getmsg("consistency_check", 0, 0, stdout);
	system("bk cset -M1.0..");
	i = open("BitKeeper/etc/SCCS/x.marked", O_CREAT|O_TRUNC|O_WRONLY, 0664);
	if (i < 0) {
		perror("BitKeeper/etc/SCCS/x.marked");
		return;
	}
	close(i);
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
	getmsg("version", bk_model(buf, sizeof(buf)), 0, f);
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

	if (verbose) {
		gettemp(tmp_file, "status");
		f1 = fopen(tmp_file, "wb");
		assert(f1);
		bkusers(0, 0, 0, f1);
		fclose(f1);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "User:\t%s", buf);
		}
		fclose(f1);
		sprintf(buf, "bk sfind -sx > %s", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Extra:\t%s", buf);
		}
		fclose(f1);
		sprintf(buf, "bk sfind -g -s,c > %s", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Modified:\t%s", buf);
		}
		fclose(f1);
		sprintf(buf, "bk sfind -g -s,,p -C > %s", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Uncommitted:\t%s", buf);
		}
		fclose(f1);
		unlink(tmp_file);
	} else {
		int i;

		fprintf(f,
		    "%6d people have made deltas.\n", bkusers(1, 0, 0, 0));
		f1 = popen("bk sfind -S -sx,c,p", "r");
		while (fgets(buf, sizeof (buf), f1)) fputs(buf, f);
		pclose(f1);
	}
}


int
getmsg(char *msg_name, char *bkarg, char *prefix, FILE *outf)
{
	char	buf[MAXLINE], pattern[MAXLINE];
	FILE	*f, *f1;
	int	found = 0;
	int	first = 1;

	if (bkarg == NULL) bkarg = "";
	sprintf(buf, "%s/bkmsg.txt", bin);
	f = fopen(buf, "rt");
	unless (f) {
		fprintf(stderr, "Unable to open %s\n", buf);
		exit(1);
	}
	sprintf(pattern, "#%s\n", msg_name);
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
		if (p = strstr(buf, "#BKARG#")) {
			*p = 0;
			fputs(buf, outf);
			fputs(bkarg, outf);
			fputs(&p[7], outf);
		} else if (p = strstr(buf, "#BKEXEC#")) {
			f1 = popen(&p[8], "r");
			while (fgets(buf, sizeof (buf), f1)) {
				fputs("\t", outf);
				fputs(buf, outf);
			}
			*p = 0;
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

	sprintf(buf, "%s/bkmsg.txt", bin);
	unless (in = fopen(buf, "rt")) {
		fprintf(stderr, "Unable to open %s\n", buf);
		return (-1);
	}
	getmsg("config_preamble", 0, "# ", out);
	fputs("\n", out);

	/*
	 * look for config template
	 */
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

	/*
	 * Now print the help message for each config entry
	 */
	while (fgets(buf, sizeof(buf), in)) {
		if (first && (buf[0] == '#')) continue;
		first = 0;
		if (streq("$\n", buf)) break;
		chop(buf);
		sprintf(pattern, "config_%s", buf);
		getmsg(pattern, 0, "# ", out);
		fprintf(out, "%s: \n", buf);
	}
	fclose(in);
	return (0);
}
