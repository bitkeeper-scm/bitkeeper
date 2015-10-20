#include "bkd.h"

int
sendbug_main(int ac,  char **av)
{
	char	buf[MAXLINE], bug[MAXPATH];
	int	c, rc, textmode = 0;
	char	*name;
	FILE	*f;
	char	*url = "http://bitmover.com/cgi-bin/bkdmail";
	char	*key, **email;

	/* we want to default to GUI tools, and let -t or no DISPLAY override that */
	putenv("BK_GUI=YES");
	
	if (name = strrchr(av[0], '/')) {
		name++;
	} else {
		name = av[0];
	}
	while ((c = getopt(ac, av, "etTw", 0)) != -1) {
		switch (c) {
		    case 'w':
			url = "http://bitmover.com/cgi-bin/bkdmail";
			break;
		    case 'e':
			url = "SMTP";
			break;
		    case 'T': /* -T is preferred, remove -t in 5.0 */
		    case 't':   textmode = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (streq(name, "support")) {
		key = "support";
		email = addLine(0, "bitkeeper-support@bitkeeper.com");
	} else {
		key = "bug";
		email = addLine(0, "bitkeeper-bugs@bitkeeper.com");
	}

	if (!textmode && gui_useDisplay()) {
		sprintf(buf, "%sform", key);
		return (launch_wish(buf, &av[optind]));
	}

	bktmp(bug);
	f = fopen(bug, "wt");
	sprintf(buf, "%stemplate", key);
	getMsg(buf, 0, 0, f);
	fclose(f);
	sprintf(buf, "%s '%s'", editor, bug);
	system(buf);
	while (1) {
		printf("(s)end, (e)dit, (q)uit? ");
		unless (fgets(buf, sizeof(buf), stdin)) buf[0] = 'q';
		switch (buf[0]) {
		    case 's':
			sprintf(buf, "BK %s Request", key);
			rc = bkmail(url, email, buf, bug);
			freeLines(email, 0);
			if (rc == 0) {
				printf(
 "Your message has been sent, thank you.\n");
				unlink(bug);
			} else {
				printf(
				    "mailer failed: %s request saved in %s\n",
				    key, bug);
			}
			exit(0);
		    case 'e':
			sprintf(buf, "%s %s", editor, bug);
			system(buf);
			break;
		    case 'q':
			unlink(bug);
			printf("No message sent.\n");
			exit(0);
		}
	}
}
