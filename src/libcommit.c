#include "system.h"
#include "sccs.h"
#include "logging.h"
#include <time.h>

void
do_prsdelta(char *file, char *rev, int flags, char *dspec, FILE *out)
{
	sccs *s;
	ser_t d;

	s = sccs_init(file, INIT_NOCKSUM);
	assert(s);
	s->state &= ~S_SET;
	d = sccs_findrev(s, rev);
	sccs_prsdelta(s, d, flags, dspec, out);
	sccs_free(s);
}

int
get(char *path, int flags)
{
	sccs *s;
	int ret;

	if (sccs_filetype(path) == 's') {
		s = sccs_init(path, SILENT|INIT_MUSTEXIST);
	} else {
		char	*p = name2sccs(path);

		s = sccs_init(p, SILENT|INIT_MUSTEXIST);
		free(p);
	}
	unless (s && HASGRAPH(s)) {
		if (s) sccs_free(s);
		return (-1);
	}
	ret = sccs_get(s, 0, 0, 0, 0, flags, s->gfile, 0);
	sccs_free(s);
	return (ret ? -1 : 0);
}

void
status(int verbose, FILE *f)
{
	char	buf[MAXLINE], parent_file[MAXPATH];
	char	tmp_file[MAXPATH];
	FILE	*f1;

	fprintf(f, "Status for BitKeeper repository %s:%s\n",
	    sccs_gethost(), proj_cwd());
	bkversion(f);
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
		bktmp(tmp_file);
		f1 = fopen(tmp_file, "w");
		assert(f1);
		bkusers(0, 0, f1);
		fclose(f1);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "User:\t%s", buf);
		}
		fclose(f1);
		sprintf(buf, "bk sfiles -x > '%s'", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Extra:\t%s", buf);
		}
		fclose(f1);
		sprintf(buf, "bk sfiles -gc > '%s'", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Modified:\t%s", buf);
		}
		fclose(f1);
		sprintf(buf, "bk sfiles -gpC > '%s'", tmp_file);
		system(buf);
		f1 = fopen(tmp_file, "rt");
		while (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "Uncommitted:\t%s", buf);
		}
		fclose(f1);
		unlink(tmp_file);
	} else {
		fprintf(f,
		    "%6d people have made deltas.\n", bkusers(0, 0, 0));
		f1 = popen("bk sfiles -ES", "r");
		while (fgets(buf, sizeof (buf), f1)) fputs(buf, f);
		pclose(f1);
	}
}

private void
line(char b, FILE *f)
{
	int	i;

	for (i = 0; i < 79; ++i) fputc(b, f);
	fputc('\n', f);
}

int
getMsg(char *msg_name, char *bkarg, char b, FILE *outf)
{
	char	**args;
	int	rc;

	unless (bkarg) bkarg = "";
	args = addLine(0, bkarg);
	rc = getMsgv(msg_name, args, 0, b, outf);
	freeLines(args, 0);
	return (rc);
}

int
getMsgP(char *msg_name, char *bkarg, char *prefix, char b, FILE *outf)
{
	char	**args;
	int	rc;

	unless (bkarg) bkarg = "";
	args = addLine(0, bkarg);
	rc = getMsgv(msg_name, args, prefix, b, outf);
	freeLines(args, 0);
	return (rc);
}

int
getMsg2(char *msg_name, char *arg1, char *arg2, char b, FILE *outf)
{
	char	**args;
	int	rc;

	args = addLine(0, arg1);
	args = addLine(args, arg2);
	rc = getMsgv(msg_name, args, 0, b, outf);
	freeLines(args, 0);
	return (rc);
}

int
getMsgv(char *msg_name, char **bkargs, char *prefix, char b, FILE *outf)
{
	FILE	*f, *f1;
	int	found = 0;
	int	first = 1;
	int	n;
	char	buf[MAXLINE], pattern[MAXLINE];

	unless (msg_name) return (0);
	sprintf(buf, "%s/bkmsg.txt", bin);
	unless (f = fopen(buf, "rt")) {
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
	if (found && b) line(b, outf);
	while (fgets(buf, sizeof(buf), f)) {
		char	*p, *b;

		if (first && (buf[0] == '#')) continue;
		first = 0;
		if (streq("$\n", buf)) break;
		if (prefix) fputs(prefix, outf);

		/*
		 * #BKARG# or #BKARG#1# is the first entry.
		 * #BKARG#%d# is the Nth arg.
		 */
		if (p = strstr(buf, "#BKARG#")) {
			b = buf;
			do {
				*p = 0;
				p += 7;
				/* #BKARG#%d# */
				if (isdigit(*p)) {
					n = atoi(p);
					p = strchr(p, '#');
					assert(p);
					p++;
				} else {
					n = 1;
				}
				fputs(b, outf);
				if (n <= nLines(bkargs)) {
					fputs(bkargs[n], outf);
				} else {
					// not enough arguments
					fprintf(outf, "#BKARG#%d#", n);
				}
				b = p;
			} while (p = strstr(b, "#BKARG#"));
			fputs(b, outf);
		} else if (p = strstr(buf, "#BKEXEC#")) {
			if (f1 = popen(&p[8], "r")) {
				while (fgets(buf, sizeof (buf), f1)) {
					fputs("\t", outf);
					fputs(buf, outf);
				}
				pclose(f1);
			}
			*p = 0;
		} else {
			fputs(buf, outf);
		}
	}
	fclose(f);
	if (found && b) line(b, outf);
	return (found);
}
