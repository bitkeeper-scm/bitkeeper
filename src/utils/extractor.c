/*
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
#include "../system.h"
#undef	mkdir
#undef	system
#undef	unlink
#undef	putenv
#undef	malloc

#ifndef MAXPATH
#define	MAXPATH		1024
#endif
#ifdef	WIN32
#define	mkdir(a, b)	_mkdir(a)
#define	RMDIR		"rmdir /s /q"
#else
#define	RMDIR		"/bin/rm -rf"
#endif

extern unsigned int sfio_size;
extern unsigned char sfio_data[];
extern unsigned int data_size;
extern unsigned char data_data[];

void	extract(char *, char *, unsigned int, char *);
char	*findtmp(void);

main(int ac, char **av)
{
	char	*sfio;
	char	instdir[MAXPATH];
	char	cmd[4096];
	int	i, fd;
	pid_t	pid = getpid();
	char	*tmp = findtmp();

	fprintf(stderr, "Please wait while we unpack to a temp dir...\n");

	sprintf(instdir, "%s/bksetup%u", tmp, pid);
	if (mkdir(instdir, 0700)) {
		perror(instdir);
		exit(1);
	}
	if (chdir(instdir)) {
		perror(instdir);
		exit(1);
	}

	/*
	 * Add this directory and BK directory to the path.
	 */
	tmp = malloc(strlen(getenv("PATH")) + 3 * strlen(instdir));
	sprintf(tmp, "PATH=%s%c%s/bitkeeper%c%s",
	    instdir, PATH_DELIM, instdir, PATH_DELIM, getenv("PATH"));
	putenv(tmp);
	
	/* The name "sfio.exe" should work on all platforms */
	extract("sfio.exe", sfio_data, sfio_size, instdir);
	extract("data", data_data, data_size, instdir);

	/* Unpack the sfio file, this creates ./bitkeeper/ */
	if (system("sfio.exe -im < data")) {
		perror(cmd);
		exit(1);
	}
	
	sprintf(cmd, "bk installtool");
	for (i = 1; av[i]; i++) {
		strcat(cmd, " ");
		strcat(cmd, av[i]);
	}
	av[i] = 0;
	system(cmd);

	/* Clean up your room, kids. */
	unless (getenv("BK_SAVE_INSTALL")) {
		chdir("..");
		sprintf(cmd, "%s bk%d", RMDIR, pid);
		system(cmd);	/* careful */
	}

	/*
	 * Bitchin'
	 */
	exit(0);
}

void
extract(char *name, char *x_data, unsigned int x_size, char *instdir)
{
	int	fd;
	char	path[MAXPATH];
	extern	int	errno;
	static	pid_t pid = 0;

	sprintf(path, "%s/%s", instdir, name);
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
}

int
istmp(char *path)
{
	char	*p = &path[strlen(path)];
	int	fd;

	sprintf(p, "/findtmp%d", getpid());
	fd = open(path, O_CREAT | O_RDWR | O_EXCL, 0666);
	if (fd == -1) {
err:		*p = 0;
		return (0);
	}
	if (write(fd, "Hi\n", 3) != 3) {
		close(fd);
		goto err;
	}
	close(fd);
	unlink(path);
	*p = 0;
	return (1);
}

/*
 * I'm not at all convinced we need this.
 */
char*
findtmp(void)
{
#ifdef	WIN32
	char	*places[] = {
			"Temp",
			"Tmp",
			"WINDOWS/Temp",
			"WINDOWS/Tmp",
			"WINNT/Temp",
			"WINNT/Tmp",
			"cygwin/tmp",
			0
		};
	int	i;
	char	drive;
	char	path[MAXPATH];

	sprintf(path, "%s", TMP_PATH);
	if (istmp(path)) return (strdup(path));
	for (drive = 'C'; drive <= 'Z'; drive++) {
		for (i = 0; places[i]; ++i) {
			sprintf(path, "%c:/%s", drive, places[i]);
			if (istmp(path)) return (strdup(path));
		}
	}
	fprintf(stderr, "Can't find a temp directory\n");
	exit(1);
#else
	return ("/tmp");
#endif
}
