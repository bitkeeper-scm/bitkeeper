#include "bkd.h"

void	do_cmds(int in, int out);
int	findcmd(int ac, char **av);
int	getav(int in, int out, int *acp, char ***avp);
int	Interactive, ExitOnError;

int
main(int ac, char **av)
{
	int	c;

	while ((c = getopt(ac, av, "ei")) != -1) {
		switch (c) {
		    case 'e': ExitOnError++; break;
		    case 'i': Interactive++; break;
	    	}
	}
	putenv("PAGER=cat");
	do_cmds(0, 1);
	return (0);
}

void
drain(int fd)
{
	int	c;

	while (read(fd, &c, 1) == 1);
}

void
do_cmds(int in, int out)
{
	int	ac;
	char	**av;
	int	i;

	while (getav(in, out, &ac, &av)) {
		getoptReset();
		if ((i = findcmd(ac, av)) != -1) {
			if (cmds[i].cmd(ac, av, in, out) != 0) {
				if (Interactive) {
					writen(out, "ERROR-CMD FAILED\n");
				}
				if (ExitOnError) {
					writen(out, "ERROR-exiting\n");
					drain(in);
					exit(1);
				}
			}
		} else if (av[0]) {
			if (Interactive) writen(out, "ERROR-BAD CMD: ");
			if (Interactive) writen(out, av[0]);
			if (Interactive) writen(out, ", Try help\n");
		} else {
			if (Interactive) writen(out, "ERROR-Try help\n");
		}
	}
}

int
findcmd(int ac, char **av)
{
	int	i;

	if (ac == 0) return (-1);
	for (i = 0; cmds[i].name; ++i) {
		if (strcmp(av[0], cmds[i].name) == 0) return (i);
	}
	return (-1);
}

int
getav(int in, int out, int *acp, char ***avp)
{
	static	char buf[2500];		/* room for two keys */
	static	char *av[50];
	int	i, inspace = 1;
	int	ac;

	if (Interactive) write(out, "BK> ", 4);
	for (ac = i = 0; read(in, &buf[i], 1) == 1; i++) {
		if (buf[i] == '\n') {
			buf[i] = 0;
			av[ac] = 0;
			*acp = ac;
			*avp = av;
			return (1);
		}
		if (isspace(buf[i])) {
			buf[i] = 0;
			inspace = 1;
		} else if (inspace) {
			av[ac++] = &buf[i];
			inspace = 0;
		}
	}
	return (0);
}
