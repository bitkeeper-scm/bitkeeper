#include "system.h"
#include "sccs.h"

/* usage bk merge L G R M */
int
main(int ac, char **av)
{
	char	*new_av[8];
	int	rc, fd, fd1;

	new_av[0] = "bk";
	new_av[1] = "diff3";
	new_av[2] = "-E";
	new_av[3] = "-am";
	new_av[4] = av[1];
	new_av[5] = av[2];
	new_av[6] = av[3];
	new_av[7] = 0;

	if (ac != 5) {
		fprintf(stderr, "Usage: bk merge L G R M\n");
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
	make_fd_uninheritable(fd1);
	rc = spawnvp_ex(_P_WAIT, new_av[0], new_av);
	/* restore parent's stdout */
	close(1); dup2(fd1, 1);
	exit (rc);
}

