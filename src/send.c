#include "system.h"
#include "sccs.h"

#define	BK_LOG "BitKeeper/log"


/*
 * Compute the cset(s) we need to send.
 * Side effect: This function also update the sendlog
 */
private	char *
getNewRevs(char *to, char *rev)
{
	char	x_sendlog[MAXPATH], here[MAXPATH], has[MAXPATH];
	char	buf[MAXLINE];
	char	revbuf[MAXLINE] = "";
	static	char revsFile[MAXPATH];
	FILE	*f;
	int	first = 1;

	assert(!streq(to, "-"));

	unless (isdir(BK_LOG)) mkdirp(BK_LOG);
	sprintf(x_sendlog, "%s/send-%s", BK_LOG, to);
	sprintf(here, "%s/bk_here%d", TMP_PATH, getpid());
	sprintf(has, "%s/bk_has%d", TMP_PATH, getpid());
	sprintf(revsFile, "%s/bk_revs%d", TMP_PATH, getpid());
	close(open(x_sendlog, O_CREAT, 0660));

	if (rev == NULL) {
		sprintf(buf, "bk prs -hd':KEY:\n' ChangeSet | bk _sort > %s", here);
	} else {
		sprintf(buf,
		    "bk prs -hd':KEY:\n' -r%s ChangeSet | bk _sort > %s",
		    rev, here);
	}
	system(buf);
	sprintf(buf, "bk _sort -u < %s > %s", x_sendlog, has);
	system(buf);
	sprintf(buf, "comm -23 %s %s | bk key2rev ChangeSet > %s",
							here, has, revsFile);
	system(buf);
	f = fopen(revsFile, "rt");
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
	unlink(has);  unlink(here);
	if (revbuf[0] == '\0') {
		unlink(revsFile);
		return 0;
	}
	sprintf(buf, "cp %s %s", x_sendlog, here);
	system(buf);
	sprintf(buf, "bk prs -hd':KEY:\n' -r%s ChangeSet >> %s", revbuf, here);
	system(buf);
	sprintf(buf, "bk _sort -u < %s > %s", here, x_sendlog);
	system(buf);
	unlink(here);
	return (revsFile);
}

private void
listCsetRevs(FILE *f, char *revsFile, char *rev)
{
	FILE	*f1;
	char	buf[MAXLINE];

	fprintf(f, "This BitKeeper patch contains the following changesets:\n");

	if (revsFile) {
		f1 = fopen(revsFile, "rt");
		while (fnext(buf, f1)) {
			chop(buf);
			fputs(buf, f);
			fputs(" ", f);
		}
		fputs("\n", f);
		fclose(f1);
	} else {
		fprintf(f, "%s\n", rev);
	}
}

/*
 * Print patch header
 */
private void
printHdr(FILE *f, char *revsFile, char *rev, char *wrapper)
{
	listCsetRevs(f, revsFile, rev);

	if (wrapper) fprintf(f, "## Wrapped with %s ##\n\n", wrapper);
	fprintf(f, "\n");
	fflush(f);
}

int
send_main(int ac,  char **av)
{
	int	c, rc = 0, force = 0;
	char	*to, *p, *out, *cmd, *dflag = "", *qflag = "-vv";
	char	*wrapper = 0,*patch = 0, *revsFile = 0, *revArgs = 0;
	char	*wrapperArgs = "", *rev = "1.0..";
	FILE	*f;

	if (bk_mode() == BK_BASIC) {
		fprintf(stderr, upgrade_msg);
		exit(1);
	}
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help send");
		return (0);
	}
	while ((c = getopt(ac, av, "dfqr:w:")) != -1) {
		switch (c) {
		    case 'd':	dflag = "-d"; break;		/* doc 2.0 */
		    case 'f':	force++; break;			/* doc 2.0 */
		    case 'q':	qflag = ""; break;		/* doc 2.0 */
		    case 'r': 	rev = optarg; break;		/* doc 2.0 */
		    case 'w': 	wrapper = optarg; break;	/* doc 2.0 */
		    default :
			fprintf(stderr, "unknown option <%c>\n", c);
			system("bk help -s send");
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

	/*
	 * Set up rev list for makepatch
	 */
	if (!streq(to, "-") && !force) {
		/*
	 	 * We are sending a patch to some host,
		 * subtract the cset(s) we already sent eailer
		 */
		revsFile = getNewRevs(to, rev);
		if (revsFile == NULL) {
			printf("Nothing to send to %s, use -f to force.\n", to);
			exit(0);
		}
		revArgs = aprintf("-r - < %s", revsFile);
	} else {
		revArgs = aprintf("-r%s", rev);
	}

	/*
	 * Set up output mode
	 * The fake email address "hoser@nevdull.com" is
	 * used in regression test t.send
	 */
	if (streq(to, "-") || streq(to, "hoser@nevdull.com")) {
		f = stdout;
		out = "";
	} else {
		patch = aprintf(patch, "%s/bk_patch%d", TMP_PATH, getpid());
		f = fopen(patch, "wb");
		assert(f);
		out = aprintf(" >> %s", patch);
	}

	/*
	 * Set up wrapper
	 */
	if (wrapper) wrapperArgs = aprintf(" | bk %swrap", wrapper);

	/*
	 * Print patch header
	 */
	printHdr(f, revsFile, rev, wrapper);
	unless (f == stdout) fclose(f);

	/*
	 * Now make the patch
	 */
	cmd = aprintf("bk makepatch %s %s %s %s %s",
				    dflag, qflag, revArgs, wrapperArgs, out);
	if ((rc = system(cmd)) != 0)  goto out;


	/*
	 * Mail the patch if necessary
	 */
	if (patch) mail(to, "BitKeeper patch", patch);

out:	if (patch) {
		unlink(patch);
		free(patch);
	}
	if (revsFile) unlink(revsFile);
	if (revArgs) free(revArgs);
	if (cmd) free(cmd);
	if (*out) free(out);
	if (*wrapperArgs) free(wrapperArgs);
	return (rc);
}
