#include "bkd.h"

void	do_cmds(int in, int out, int err);
int	findcmd(int ac, char **av);
int	getav(int in, int out, int err, int *acp, char ***avp);
int	Debug, Interactive, ExitOnError;

int
main(int ac, char **av)
{
	int	c;

	while ((c = getopt(ac, av, "dei")) != -1) {
		switch (c) {
		    case 'd': Debug++; break;
		    case 'e': ExitOnError++; break;
		    case 'i': Interactive++; break;
	    	}
	}
	putenv("PAGER=cat");
	do_cmds(0, 1, 2);
	return (0);
}

void
drain(int fd)
{
	int	c;

	while (read(fd, &c, 1) == 1);
}

void
do_cmds(int in, int out, int err)
{
	int	ac;
	char	**av;
	int	i;

	while (getav(in, out, err, &ac, &av)) {
		getoptReset();
		if ((i = findcmd(ac, av)) != -1) {
			if (cmds[i].cmd(ac, av, in, out, err) == 0) {
				if (Interactive) writen(err, "CMD OK\n");
			} else {
				if (Interactive) writen(err, "CMD FAILED\n");
				if (ExitOnError) {
					/*
					 * I suspect there is some good reason
					 * for this, probably for a push, but it
					 * screws up pulls if it is before the
					 * ERROR.
					 */
					//drain(in);
					writen(out, "ERROR\n");
					drain(in);
					exit(1);
				}
			}
		} else if (av[0]) {
			if (Interactive) write(err, "BAD CMD: ", 9);
			if (Interactive) write(err, av[0], strlen(av[0]));
			if (Interactive) write(err, ", Try help\n", 11);
		} else {
			if (Interactive) write(err, "Try help\n", 9);
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
getav(int in, int out, int err, int *acp, char ***avp)
{
	static	char buf[2500];		/* room for two keys */
	static	char *av[50];
	int	i, inspace = 1;
	int	ac;

	if (Interactive) write(err, "BK> ", 4);
	for (ac = i = 0; read(in, &buf[i], 1) == 1; i++) {
		if (Debug) { write(err, &buf[i], 1); write(err, "\n", 1); }
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
