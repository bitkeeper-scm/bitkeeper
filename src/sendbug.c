#include "bkd.h"

extern char *editor;
extern	int launch_wish(char *script, char **av);
extern char *bin;

int
sendbug_main(int ac,  char **av)
{
	char	buf[MAXLINE], bug[MAXPATH];
	int	c, rc, textmode = 0;
	char	*name;
	FILE	*f;
	char	*url = "http://bitmover.com/cgi-bin/bkdmail";
	char	*key, **email;

	if (name = strrchr(av[0], '/')) {
		name++;
	} else {
		name = av[0];
	}
	if (ac > 1 && streq("--help", av[1]))  {
		sys("bk", "help", name, SYS);
		return (0);
	}

	while ((c = getopt(ac, av, "twe")) != -1) {
		switch (c) {
		    case 'w':
			url = "http://bitmover.com/cgi-bin/bkdmail";
			break;
		    case 'e':
			url = "SMTP";
			break;
		    case 't':   textmode = 1; break;
		    default:
			sys("bk", "help", "-s", name, SYS);
			return (1);
		}
	}
	if (streq(name, "support")) {
		key = "support";
		email = addLine(0, "bitkeeper-support@bitmover.com");
	} else {
		key = "bug";
		email = addLine(0, "bitkeeper-bugs@bitmover.com");
	}

	if (!textmode && gui_useDisplay()) {
		sprintf(buf, "%sform", key);
		return (launch_wish(buf, &av[optind]));
	}

	bktmp(bug, "bug");
	f = fopen(bug, "wt");
	sprintf(buf, "%stemplate", key);
	getMsg(buf, 0, 0, 0, f);
	fclose(f);
	sprintf(buf, "%s %s", editor, bug);
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
