#include "system.h"
#include "sccs.h"

extern char *editor;

private char *sendbug_help = "\n\
usage: sendbug [ --help]\n\
	This command is used to file a bug report.\n\
	This is a interactive command, it will display a\n\
	bug report template, bring up a editor so the user\n\
	can fill in the bug detail. When the user exit the editor,\n\
	it will ask for confimation to email the bug report\n\
	to the BitKeeper database.\n";

int
sendbug_main(int ac,  char **av)
{
	char	buf[MAXLINE];
	char	bug[MAXPATH];
	int	status;
	pid_t	pid;
	FILE	*f;

	if (ac > 1 && streq("--help", av[1]))  {
		fputs(sendbug_help, stderr);
		exit(1);
	}
	sprintf(bug, "%s/bk_bug%d", TMP_PATH, getpid());
	f = fopen(bug, "wt");
	getmsg("bugtemplate", 0, 0, f);
	fclose(f);
	sprintf(buf, "%s %s", editor, bug);
	system(buf);
	while (1) {
		printf("(s)end, (e)dit, (q)uit? ");
		unless (fgets(buf, sizeof(buf), stdin)) buf[0] = 'q';
		switch (buf[0]) {
		    case 's':
			pid = 
			    mail("bitkeeper-bugs@bitmover.com", "BK Bug", bug);
			if (pid == (pid_t) -1) {
				fprintf(stderr, "cannot start mailer\n");
				unlink(bug);
				exit(1);
			}
			waitpid(pid, &status, 0);
			unlink(bug);
			printf("Your bug has been sent, thank you.\n");
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
