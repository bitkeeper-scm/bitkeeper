/*
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
#include "../system.h"

#ifndef MAXPATH
#define	MAXPATH	1024
#endif

extern unsigned int sfio_size;
extern unsigned char sfio_data[];
extern unsigned int data_size;
extern unsigned char data_data[];

void	extract(char *, char *, unsigned int, char *);

main(int ac, char **av)
{
	char	*sfio;
	char	install_tmp[MAXPATH];
	char	cmd[4096];
	int	i, fd;
	pid_t	pid = getpid();
	char	*tmp = "C:/WINDOWS/Temp";

	fprintf(stderr, "Please wait while we unpack the installer...");

	sprintf(install_tmp, "%s/bk_install%-u", tmp, pid);
	if (mkdir(install_tmp, 0700)) {
		perror(install_tmp);
		exit(1);
	}
	if (chdir(install_tmp)) {
		perror(install_tmp);
		exit(1);
	}

	/* sfio.exe should work on all platforms */
	extract("sfio.exe", sfio_data, sfio_size, install_tmp);
	extract("data", data_data, data_size, install_tmp);

	/*
	 * Unpack the sfio file, this creates ./bitkeeper/
	 */
	if (system("./sfio.exe -im < data")) {
		perror(cmd);
		exit(1);
	}
	fprintf(stderr, "done.\n");
	
	/*
	 * Run the installer.
	 */
	sprintf(cmd, "./bitkeeper/bk installtool");
	for (i = 1; av[i]; i++) {
		strcat(cmd, " ");
		strcat(cmd, av[i]);
	}
	av[i] = 0;
	fprintf(stderr, "Running %s\n", cmd);
	system(cmd);

	/*
	 * Clean up.
	 */
	unless (getenv("BK_SAVE_INSTALL")) {
		sprintf(cmd, "/bin/rm -rf %s", install_tmp); 
		system(cmd);	/* careful */
	}

	/*
	 * Bitchin'
	 */
	exit(0);
}

void
extract(char *name, char *x_data, unsigned int x_size, char *install_tmp)
{
	int	fd;
	char	path[MAXPATH];
	extern	int	errno;
	static	pid_t pid = 0;

	sprintf(path, "%s/%s", install_tmp, name);
	fd = open(path, O_WRONLY | O_TRUNC | O_CREAT | O_EXCL, 0755);
	if (fd == -1) {
		perror(path);
		exit(1);
	}
	setmode(fd, _O_BINARY);
	if (write(fd, x_data, x_size) != x_size) {
		perror("write");
		unlink(path);
		exit(1);
	}
	close(fd);
	return(strdup(path));
}
