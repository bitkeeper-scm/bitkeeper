/*
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>

extern unsigned int installer_size;
extern unsigned char installer_data[];
extern unsigned int data_size;
extern unsigned char data_data[];

main()
{
	char	installer_name[200];
	char	data_name[200];
	char	cmd[2048];
	int	fd;

	fprintf(stderr, "Please wait while we unpack the installer...");
	sprintf(installer_name, "/tmp/installer%d", getpid());
	fd = open(installer_name, O_WRONLY | O_TRUNC | O_CREAT | O_EXCL, 0755);
	if (fd == -1) {
		perror(installer_name);
		exit(1);
	}
	if (write(fd, installer_data, installer_size) != installer_size) {
		perror("write on installer");
		unlink(installer_name);
		exit(1);
	}
	close(fd);
	sprintf(data_name, "/tmp/data%d", getpid());
	fd = open(data_name, O_WRONLY | O_TRUNC | O_CREAT | O_EXCL, 0755);
	if (fd == -1) {
		perror(data_name);
		exit(1);
	}
	if (write(fd, data_data, data_size) != data_size) {
		perror("write on data");
		unlink(data_name);
		exit(1);
	}
	close(fd);
	fprintf(stderr, "done.\n");
	sprintf(cmd, "%s %s %s", installer_name, installer_name, data_name);
	system(cmd);
	exit(0);
}
