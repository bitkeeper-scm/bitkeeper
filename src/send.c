#include "system.h"
#include "sccs.h"

#define	BK_LOG "BitKeeper/log"

private	char *
sendlog(char *to, char *rev)
{
	char	x_sendlog[MAXPATH], here[MAXPATH], has[MAXPATH];
	char	revs[MAXPATH];
	char	buf[MAXLINE];
	static	char revbuf[MAXLINE] = "";
	FILE	*f;
	int	first = 1;

	if (streq(to, "-")) return (rev);

	unless (isdir(BK_LOG)) mkdirp(BK_LOG);
	sprintf(x_sendlog, "%s/send-%s", BK_LOG, to);
	sprintf(here, "%s/bk_here%d", TMP_PATH, getpid());
	sprintf(has, "%s/bk_has%d", TMP_PATH, getpid());
	sprintf(revs, "%s/bk_revs%d", TMP_PATH, getpid());
	close(open(x_sendlog, O_CREAT, 0660));

	if (rev == NULL) {
		sprintf(buf, "bk prs -hd':KEY:\n' ChangeSet | sort > %s", here);
	} else {
		sprintf(buf,
		    "bk prs -hd':KEY:\n' -r%s ChangeSet | sort > %s",
		    rev, here);
	}
	system(buf);
	sprintf(buf, "sort -u < %s > %s", x_sendlog, has);
	system(buf);
	sprintf(buf, "sort -u < %s > %s", x_sendlog, has);
	sprintf(buf, "comm -23 %s %s | bk key2rev ChangeSet > %s",
							here, has, revs);
	system(buf);
	f = fopen(revs, "rt");
	while (fgets(buf, sizeof(buf), f)) {
		chop(buf);
		if (first) {
			first  = 0;
		} else {
			strcat(revbuf, ",");
		}
		strcat(revbuf, buf);
	}
	fclose(f);
	unlink(has); unlink(revs); unlink(here);
	if (revbuf[0] == '\0') return 0;
	sprintf(buf, "cp %s %s", x_sendlog, here);
	system(buf);
	sprintf(buf, "bk prs -hd':KEY:\n' -r%s ChangeSet >> %s", revbuf, here);
	system(buf);
	sprintf(buf, "sort -u < %s > %s", here, x_sendlog);
	system(buf);
	unlink(here);
	return (revbuf);
}

int
send_main(int ac,  char **av)
{
	int	c, use_stdout = 0;
	int	force = 0;
	char	*dflag = "", *qflag = "-vv";
	char	*wrapper = NULL;
	char	*rev = "1.0..";
	char	*to, *p;
	char	buf[MAXLINE];
	char	patch[MAXPATH], out[MAXPATH];
	FILE	*f;

	if (bk_mode() == BK_BASIC) {
		fprintf(stderr, upgrade_msg);
		exit(1);
	}
	while ((c = getopt(ac, av, "dfqr:w:")) != -1) {
		switch (c) {
		    case 'd':	dflag = "-d"; break;
		    case 'f':	force++; break;
		    case 'q':	qflag = ""; break;
		    case 'r': 	rev = optarg; break;
		    case 'w': 	wrapper = optarg; break;
		    default :
			fprintf(stderr, "unknown option <%c>\n", c);
			exit(1);
		}
	}
	to = av[optind];

	if ((to == NULL) || av[optind + 1]) {
		fprintf(stderr,
		"usage: bk send [-dq] [-wWrapper] [-rCsetRevs] user@host|-\n");
		exit(1);
	}

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "send: cannot find package root.\n");
		exit(1);
	}
	if (!streq(to, "-") && !force) {
		rev = sendlog(to, rev);
		if (rev == NULL) {
			printf("Nothing to send to %s, use -f to force.\n", to);
			exit(0);
		}
	}
	if (streq(to, "-") || streq(to, "hoser@nevdull.com")) use_stdout = 1;
	if (use_stdout) {
		f = stdout;
		out[0] = '\0';
	} else {
		sprintf(patch, "%s/bk_patch%d", TMP_PATH, getpid());
		f = fopen(patch, "wb");
		assert(f);
		sprintf(out, " >> %s", patch);
	}
	fprintf(f, "This BitKeeper patch contains the following changesets:\n");
	for (p = rev; *p; p++) if (*p == ',') *p = ' ';
	fputs(rev, f); fputs("\n", f);
	for (p = rev; *p; p++) if (*p == ' ') *p = ',';
	if (wrapper) fprintf(f, "## Wrapped with %s ##\n\n", wrapper);
	fprintf(f, "\n");
	fflush(f);
	unless (use_stdout) fclose(f);
	unless (wrapper) {
		sprintf(buf, "bk cset %s -m%s %s %s",
						dflag, rev, qflag, out);
	} else {
		sprintf(buf, "bk cset %s -m%s %s | bk %swrap%s",
					    dflag, rev, qflag, wrapper, out);
	}
	if (system(buf) != 0)  {
		unless (use_stdout) unlink(patch);
		exit(1);
	}

	unless (use_stdout) {
		mail(to, "BitKeeper patch", patch);
		unlink(patch);
	}
	return (0);
}
