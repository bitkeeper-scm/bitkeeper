/*
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifndef MAXPATH
#define	MAXPATH	1024
#endif

extern unsigned int sfio_size;
extern unsigned char sfio_data[];
extern unsigned int shell_size;
extern unsigned char shell_data[];
extern unsigned int installer_size;
extern unsigned char installer_data[];
extern unsigned int data_size;
extern unsigned char data_data[];

char * extract(char *, char *, unsigned int, char *);

main(int ac, char **av)
{
	char	*sfio_name;
	char	*shell_name;
	char	*installer_name;
	char	*data_name;
	char	install_tmp[MAXPATH];
	char	cmd[4096];
	int	fd;
	pid_t	pid = getpid();

	fprintf(stderr, "Please wait while we unpack the installer...");
	sprintf(install_tmp, "/tmp/bk_install%-u", pid);
	if (mkdir(install_tmp, 0700) && (errno != EEXIST)) {
		perror(install_tmp);
		exit(1);
	}
	chdir(install_tmp);
	sfio_name = extract("sfio", sfio_data, sfio_size, install_tmp);
	shell_name = extract("shell.sfio", shell_data, shell_size, install_tmp);
	installer_name = extract("installer",
				installer_data, installer_size, install_tmp);
	data_name = extract("data", data_data, data_size, install_tmp);


	/*
	 * Unpack the shell.sfio file
	 */
	sprintf(cmd, "%s -iqm < %s", sfio_name, shell_name);
	system(cmd);
	fprintf(stderr, "done.\n");

	sprintf(cmd, "TCL_LIBRARY=%s/lib/tcl8.4", install_tmp);
	putenv(strdup(cmd));
	sprintf(cmd, "%s/gui/bin/tclsh %s", install_tmp, installer_name);
	for (fd = 1; av[fd]; fd++) {
		strcat(cmd, " ");
		strcat(cmd, av[fd]);
	}
	strcat(cmd, " ");
	strcat(cmd, installer_name);	// XXX - this is not needed
	strcat(cmd, " ");
	strcat(cmd, data_name);
	system(cmd);
	chdir("..");
	// XXX - we can't depend on /bin/rm, that's going to have be a bk
	// command.
	sprintf(cmd, "/bin/rm -rf %s", install_tmp); 
	system(cmd);	/* careful */
	exit(0);
}

char *
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
	if (write(fd, x_data, x_size) != x_size) {
		perror("write on shell");
		unlink(path);
		exit(1);
	}
	close(fd);
	return(strdup(path));
}
