#include "bkd.h"

extern char *editor;
private int do_email(char *bug);
private int do_webmail(char *bug);

int
sendbug_main(int ac,  char **av)
{
	char	buf[MAXLINE], bug[MAXPATH];
	int	c, rc, webmail;
	FILE	*f;
	remote	*r;
	MMAP	*m;

	/*
	 * On windows webmail is the default
	 * On Unix  email is the default
	 */
#ifdef WIN32
	webmail = 1;
#else
	webmail = 0;
#endif

	if (ac > 1 && streq("--help", av[1]))  {
		system("bk help sendbug");
		return (0);
	}

	while ((c = getopt(ac, av, "we")) != -1) {
		switch (c) {
		    case 'w':   webmail = 1; break;
		    case 'e':   webmail = 0; break;
		    default:    fprintf(stderr, "usage: bk _logConfig [-d]\n");
				return (1);
		}
	}

	sprintf(bug, "%s/bk_bug%d", TMP_PATH, getpid());
	f = fopen(bug, "wt");
	if (webmail) {
		fprintf(f,
		    "To: bitkeeper-bugs@bitmover.com\n"
		    "From: %s@%s\n"
		    "Subject: BK Bug\n\n",
		    sccs_getuser(), sccs_gethost());
	}
	getMsg("bugtemplate", 0, 0, f);
	fclose(f);
	sprintf(buf, "%s %s", editor, bug);
	system(buf);
	while (1) {
		printf("(s)end, (e)dit, (q)uit? ");
		unless (fgets(buf, sizeof(buf), stdin)) buf[0] = 'q';
		switch (buf[0]) {
		    case 's':
			rc = webmail ? do_webmail(bug) : do_email(bug);
			if (rc == 0) {
				printf("Your bug has been sent, thank you.\n");
				unlink(bug);
			} else {
				printf(
				    "mailer failed: bug report saved in %s\n",
				    bug);
			}
			exit(0);
		    case 'e':
			sprintf(buf, "%s %s", editor, bug);
			system(buf);
			break;
		    case 'q':
			unlink(bug);
			printf("No bug sent.\n");
			exit(0);
		}
	}
}

private int
do_email(char *bug)
{
	pid_t	pid;
	int	status;

	pid = 
	    mail("bitkeeper-bugs@bitmover.com", "BK Bug", bug);
	if (pid == (pid_t) -1) {
		fprintf(stderr, "cannot start mailer\n");
		unlink(bug);
		exit(1);
	}
	waitpid(pid, &status, 0);
	return (status);
}

private int
do_webmail(char *bug)
{
	char	url[] = BK_WEBMAIL_URL;
	remote	*r;
	MMAP	*m;
	int	rc;

	r = remote_parse(url, 0);
	assert(r);
	loadNetLib();
	http_connect(r, WEB_MAIL_CGI);
	r->isSocket = 1;
	m = mopen(bug, "r");
	assert(m);
	rc = http_send(r, m->where, msize(m),
				0, "webmail", WEB_MAIL_CGI);
	mclose(m);
	r->trace = 1;
	skip_http_hdr(r);
	unless (rc) rc = get_ok(r, 0, 0);
	disconnect(r, 2);
	return (rc);
}
