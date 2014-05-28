#include "system.h"
#include "sccs.h"

private void
usage1(char *cmd)
{
	fprintf(stderr, "bk %s file\n", cmd);
	exit(1);
}

private void
usage2(char *cmd)
{
	fprintf(stderr, "bk %s from to\n", cmd);
	exit(1);
}

private void
usage_chmod(void)
{
	fprintf(stderr, "bk _chmod <octal_mode> file\n");
	exit(1);
}

int
fslcp_main(int ac, char **av)
{
	if (ac != 3) usage2(av[0]);
	if (fileCopy(av[1], av[2])) return (1);
	return (0);
}

int
fslmv_main(int ac, char **av)
{
	int	rc;

	if (ac != 3) usage2(av[0]);
	unlink(av[2]);
	if ((isSCCS(av[1]) == IS_FILE) || (isSCCS(av[2]) == IS_FILE)) {
		/* can't rename from/to SCCS dirs */
		rc = fileCopy(av[1], av[2]);
		unlink(av[1]);
	} else {
		rc = rename(av[1], av[2]);
	}
	return (rc);
}

int
fslrm_main(int ac, char **av)
{
	int	c;
	int	rc = 0, force = 0;
	int	recurse = 0;

	while ((c = getopt(ac, av, "fr", 0)) != -1) {
		switch (c) {
		    case 'f': force = 1; break;
		    case 'r': recurse = 1; break;
		    default: usage1(av[0]); break;
		}
	}

	unless (av[optind]) usage1(av[0]);
	while (--ac >= optind) {
		if (isdir(av[ac])) {
			if (recurse) {
				if (rmtree(av[ac]) && !force) {
					rc = 1;
					perror(av[ac]);
					break;
				}
			} else {
				fprintf(stderr, "%s: is a directory\n",
				    av[ac]);
				rc = 1;
				unless (force) break;
			}
		} else if (unlink(av[ac]) && !force) {
			rc = 1;
			perror(av[ac]);
			break;
		}
	}
	return (rc);
}

int
fslchmod_main(int ac, char **av)
{
	int	i;
	int	rc = 0;
	mode_t	mode;

	if (ac < 3) usage_chmod();
	mode = strtol(av[1], 0, 8);
	if (mode == 0) {
		fprintf(stderr,
		    "bk _fslchmod: %s is not a valid non-zero octal mode\n",
		    av[1]);
		exit(1);
	}
	for (i = 2; i < ac; i++) {
		if (chmod(av[i], mode)) {
			perror(av[i]);
			rc = 1;
		}
	}
	return (rc);
}

int
fslmkdir_main(int ac, char **av)
{
	int	rc = 0;
	if (ac < 2) usage1(av[0]);
	while (--ac) {
		if (mkdir(av[ac], 0775)) {
			perror(av[ac]);
			rc = 1;
		}
	}
	return (rc);
}

int
fslrmdir_main(int ac, char **av)
{
	int	rc = 0;
	if (ac < 2) usage1(av[0]);
	while (--ac) {
		if (rmdir(av[ac])) {
			perror(av[ac]);
			rc = 1;
		}
	}
	return (rc);
}

/*
 * Write stdin to a file
 * Used for scripts to write something via bk's fslayer
 */
int
uncat_main(int ac, char **av)
{
	int	c;
	int	fd;
	char	buf[MAXLINE];

	unless (av[1] && !av[2]) usage();

	if ((fd = open(av[1], O_CREAT|O_WRONLY, 0666)) < 0) {
		perror(av[1]);
		return (-1);
	}
	while ((c = read(0, buf, sizeof(buf))) > 0) {
		writen(fd, buf, c);
	}
	close(fd);

	return (0);
}
