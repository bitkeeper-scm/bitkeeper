#include "system.h"
#include "sccs.h"

/* usage bk merge L G R M */
int
merge_main(int ac, char **av)
{
	char	*new_av[8];
	int	rc, fd, fd1;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help merge");
		return (0);
	}

	new_av[0] = "bk";
	new_av[1] = "diff3";
	new_av[2] = "-E";
	new_av[3] = "-am";
	new_av[4] = av[1];
	new_av[5] = av[2];
	new_av[6] = av[3];
	new_av[7] = 0;

	if (ac != 5) {
		system("bk help -s merge");
		return (1);
	}
	/* redirect stdout to av[4] for the child to inherit it*/
	fd1  = dup(1); close(1);
	fd = open(av[4], O_CREAT|O_WRONLY|O_TRUNC, 0664);
	if (fd < 0 )  {
		perror(av[4]);
		return(1);
	}
	assert(fd == 1);
	rc = spawnvp_ex(_P_WAIT, new_av[0], new_av);
	/* restore parent's stdout */
	close(1); dup2(fd1, 1);
	unless (WIFEXITED(rc)) return (-1);
	return (WEXITSTATUS(rc));
}


/* usage bk sortmerge L G R M */
int
sortmerge_main(int ac, char **av)
{
	char	cmd[(MAXPATH * 3) + 20];

	//XXX FIXME update man page
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help sortmerge");
		return (0);
	}

	if (ac != 5) {
		system("bk help -s sortmerge");
		return (1);
	}

	/* GCA is ignored */
	sprintf(cmd, "cat %s %s | sort -u > %s", av[1], av[3], av[4]);
	unless (system(cmd)) return (0); /* ok */
	return (2); /* error */
}

