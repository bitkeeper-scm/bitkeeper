#include "system.h"
#include "sccs.h"

#define	BK_LOG "BitKeeper/log"


/*
 * Compute the cset(s) we need to send.
 * Side effect: This function also update the sendlog
 */
private	char *
getNewRevs(char *to, char *rev, char *url)
{
	char	x_sendlog[MAXPATH], here[MAXPATH];
	char	buf[MAXLINE];
	static	char keysFile[MAXPATH];
	FILE	*f;
	FILE	*fprs;
	int	empty = 1;

	assert(url || !streq(to, "-"));

	unless (isdir(BK_LOG)) mkdirp(BK_LOG);
	sprintf(x_sendlog, "%s/send-%s", BK_LOG, to);
	unless (url) bktmp(here, "bk_here");
	sprintf(keysFile, "%s/bk_keys%u", TMP_PATH, getpid());
	close(open(x_sendlog, O_CREAT, 0660));

	if (url) {
		sprintf(buf, "bk synckeys -lk %s > %s", url, keysFile);
		system(buf);
		if (size(keysFile) == 0) {
			unlink(keysFile);
			return (0);
		}
		return (keysFile);
	}

	if (rev == NULL) {
		sprintf(buf, "bk prs -hd':KEY:\n' ChangeSet | bk _sort > %s", here);
	} else {
		sprintf(buf,
		    "bk prs -hd':KEY:\n' -r%s ChangeSet | bk _sort > %s",
		    rev, here);
	}
	system(buf);

	/*
	 * XXX TODO:
	 * For historical reason, we store the revs in x_sendlog
	 * We should reallly store the keys, because the rev notation
	 * does not record if tags are transfered
	 */
	sprintf(buf, "bk _sort -u < %s | comm -23 %s - > %s",
		x_sendlog, here, keysFile);
	system(buf);
	sprintf(buf, "cp %s %s", x_sendlog, here);
	system(buf);
	sprintf(buf, "bk key2rev ChangeSet < %s", keysFile);
	f = popen(buf, "r");
	sprintf(buf, "bk prs -hd':KEY:\n' - >> %s", here);
	fprs = popen(buf, "w");
	while (fgets(buf, sizeof(buf), f)) {
		chop(buf);
		fprintf(fprs, "ChangeSet|%s\n", buf);
		empty = 0;
	}
	pclose(f);
	pclose(fprs);
	sprintf(buf, "bk _sort -u < %s > %s", here, x_sendlog);
	system(buf);
	unlink(here);
	if (empty) {
		unlink(keysFile);
		return (0);
	}
	return (keysFile);
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
			fputs(buf, f);
		}
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
	char	*to, *out, *cmd, *dflag = "", *qflag = "-vv";
	char	*wrapper = 0,*patch = 0, *keysFile = 0, *revArgs = 0;
	char	*wrapperArgs = "", *rev = "1.0..";
	char	*url = NULL;
	FILE	*f;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help send");
		return (0);
	}
	while ((c = getopt(ac, av, "dfqr:u:w:")) != -1) {
		switch (c) {
		    case 'd':	dflag = "-d"; break;		/* doc 2.0 */
		    case 'f':	force++; break;			/* doc 2.0 */
		    case 'q':	qflag = ""; break;		/* doc 2.0 */
		    case 'r': 	rev = optarg; break;		/* doc 2.0 */
		    case 'w': 	wrapper = optarg; break;	/* doc 2.0 */
		    case 'u': 	url = optarg; break;
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
	if ((url || !streq(to, "-")) && !force) {
		/*
	 	 * We are sending a patch to some host,
		 * subtract the cset(s) we already sent eailer
		 */
		keysFile = getNewRevs(to, rev, url);
		if (keysFile == NULL) {
			printf("Nothing to send to %s, use -f to force.\n", to);
			exit(0);
		}
		revArgs = aprintf("-r - < %s", keysFile);
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
		patch = aprintf("%s/bk_patch%u", TMP_PATH, getpid());
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
	printHdr(f, keysFile, rev, wrapper);
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
	if (keysFile) unlink(keysFile);
	if (revArgs) free(revArgs);
	if (cmd) free(cmd);
	if (*out) free(out);
	if (*wrapperArgs) free(wrapperArgs);
	return (rc);
}
