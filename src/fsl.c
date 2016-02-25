/*
 * Copyright 2008,2010,2013-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
	int	rc;
	int	fd, xfile1, xfile2;
	char	*data;

	if (ac != 3) usage2(av[0]);

	xfile1 = is_xfile(av[1]);
	xfile2 = is_xfile(av[2]);
	if (xfile1 && xfile2) {
		data = xfile_fetch(av[1], xfile1);
		rc = xfile_store(av[2], xfile2, data);
		free(data);
	} else if (xfile1) {
		data = xfile_fetch(av[1], xfile1);
		fd = open(av[2], O_CREAT|O_WRONLY|O_TRUNC, 0664);
		if  (fd >= 0) {
			write(fd, data, strlen(data));
			close(fd);
			rc = 0;
		} else {
			rc = 1;
		}
		free(data);
	} else if (xfile2) {
		data = loadfile(av[1], 0);
		rc = xfile_store(av[2], xfile2, data);
		free(data);
	} else {
		rc = fileCopy(av[1], av[2]);
	}
	return (rc);
}

int
fslmv_main(int ac, char **av)
{
	int	rc;
	int	fd, xfile1, xfile2;
	char	*data;

	if (ac != 3) usage2(av[0]);
	xfile1 = is_xfile(av[1]);
	xfile2 = is_xfile(av[2]);
	unless (xfile2) unlink(av[2]);

	/*
	 * Handle the (uncommon) case in regressions where we are
	 * using rename to do an xfile <-> regular file conversion.
	 */
	if (xfile1 && xfile2) {
		data = xfile_fetch(av[1], xfile1);
		rc = xfile_store(av[2], xfile2, data);
		free(data);
		xfile_delete(av[1], xfile1);
	} else if (xfile1) {
		unless (data = xfile_fetch(av[1], xfile1)) {
			perror(av[1]);
			return (1);
		}
		fd = open(av[2], O_CREAT|O_WRONLY|O_TRUNC, 0664);
		if  (fd >= 0) {
			write(fd, data, strlen(data));
			close(fd);
			rc = 0;
		} else {
			rc = 1;
		}
		free(data);
		xfile_delete(av[1], xfile1);
	} else if (xfile2) {
		data = loadfile(av[1], 0);
		rc = xfile_store(av[2], xfile2, data);
		free(data);
		unlink(av[1]);
	} else if ((isSCCS(av[1]) == IS_FILE) || (isSCCS(av[2]) == IS_FILE)) {
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
	int	xfile;

	while ((c = getopt(ac, av, "fr", 0)) != -1) {
		switch (c) {
		    case 'f': force = 1; break;
		    case 'r': recurse = 1; break;
		    default: usage1(av[0]); break;
		}
	}

	unless (av[optind]) usage1(av[0]);
	while (--ac >= optind) {
		if (xfile = is_xfile(av[ac])) {
			rc = xfile_delete(av[ac], xfile);
			if (rc && !force) {
				perror(av[ac]);
				break;
			}
		} else if (isdir(av[ac])) {
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
