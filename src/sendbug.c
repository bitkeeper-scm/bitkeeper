#include "system.h"
#include "sccs.h"

extern char *editor;

int
sendbug_main(int ac,  char **av)
{
	char	buf[MAXLINE];
	char	bug[MAXPATH];
	int	status;
	pid_t	pid;
	FILE	*f;

	if (ac > 1 && streq("--help", av[1]))  {
		system("bk help sendbug");
		return (0);
	}

	sprintf(bug, "%s/bk_bug%d", TMP_PATH, getpid());
	f = fopen(bug, "wt");
	getMsg("bugtemplate", 0, 0, f);
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
			if (status == 0) {
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
