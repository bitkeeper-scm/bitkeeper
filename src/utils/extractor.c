/*
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
#include "../system.h"
#include "../zlib/zlib.h"
#undef	malloc
#undef	mkdir
#undef	putenv
#undef	strdup
#undef	system
#undef	unlink
#undef	fclose

#ifndef MAXPATH
#define	MAXPATH		1024
#endif
#ifdef	WIN32
#define	mkdir(a, b)	_mkdir(a)
#define	RMDIR		"rmdir /s /q"
#define	BINDIR		"C:/PROGRA~1/BitKeeper"
#else
#define	RMDIR		"/bin/rm -rf"
#define	BINDIR		"/usr/libexec/bitkeeper"
#endif
#define	TMP		"bksetup"

extern unsigned int sfio_size;
extern unsigned char sfio_data[];
extern unsigned int data_size;
extern unsigned char data_data[];

void	extract(char *, char *, u32, char *);
char	*findtmp(void);
int	isdir(char*);

main(int ac, char **av)
{
	char	*p, *sfio, *dest = 0, *tmp = findtmp();
	int	i, fd;
	pid_t	pid = getpid();
	FILE	*f;
	char	tmpdir[MAXPATH];
	char	buf[MAXPATH];

#ifdef	WIN32
	_fmode = _O_BINARY;
#endif

	/* rxvt bugs */
	setbuf(stderr, 0);
	setbuf(stdout, 0);

	/*
	 * If they want to upgrade, go find that dir before we fix the path.
	 */
	if (av[1] && (streq(av[1], "-u") || streq(av[1], "--upgrade"))) {
		if (chdir(tmp)) {
			perror(tmp);
			exit(1);
		}
		sprintf(buf, "bk bin > bindir%u", pid);
		system(buf);
		sprintf(buf, "bindir%u", pid);
		f = fopen(buf, "r");
		if (f && fgets(buf, sizeof(buf), f)) {
			for (dest = buf; *dest; dest++);
			*--dest = 0;
			if (dest[-1] == '\r') dest[-1] = 0;
			dest = strdup(buf);
		} else {
			dest = BINDIR;
		}
		if (f) fclose(f);
		sprintf(buf, "bindir%u", pid);
		unlink(buf);
	} else if (av[1] && (av[1][0] != '-')) {
#ifdef	WIN32
		unless ((av[1][1] ==':') || (av[1][0] == '/')) {
#else
		unless (av[1][0] == '/') {
#endif
			getcwd(buf, sizeof(buf));
			strcat(buf, "/");
			strcat(buf, av[1]);
			dest = strdup(buf);
		} else {
			dest = av[1];
		}
	} else if (av[1]) {
		fprintf(stderr, "usage: %s [-u || <directory>]\n", av[0]);
		exit(1);
	}
	if (dest) for (p = dest; *p; p++) if (*p == '\\') *p = '/';

	sprintf(tmpdir, "%s/%s%u", tmp, TMP, pid);
	fprintf(stderr, "Please wait while we unpack in %s ...\n", tmpdir);
	if (mkdir(tmpdir, 0700)) {
		perror(tmpdir);
		exit(1);
	}
	if (chdir(tmpdir)) {
		perror(tmpdir);
		exit(1);
	}

	/*
	 * Add this directory and BK directory to the path.
	 * Save the old path first, subprocesses need it.
	 */
	tmp = malloc(strlen(getenv("PATH")) + 20);
	sprintf(tmp, "BK_OLDPATH=%s", getenv("PATH"));
	putenv(tmp);

	tmp = malloc(strlen(getenv("PATH")) + 3*MAXPATH);
	sprintf(tmp, "PATH=%s%c%s/bitkeeper%c%s",
	    tmpdir, PATH_DELIM, tmpdir, PATH_DELIM, getenv("PATH"));
	putenv(tmp);
	
	/* The name "sfio.exe" should work on all platforms */
	extract("sfio.exe", sfio_data, sfio_size, tmpdir);
	extract("sfioball", data_data, data_size, tmpdir);

	/* Unpack the sfio file, this creates ./bitkeeper/ */
#ifdef	WIN32
	/* Winblows is so slow they need status */
	if (system("sfio.exe -im < sfioball")) {
#else
	if (system("sfio.exe -iqm < sfioball")) {
#endif
		perror("sfio");
		exit(1);
	}
	
#ifdef	WIN32
	mkdir("bitkeeper/gnu/tmp", 0777);
#endif
	if (dest) {
		fprintf(stderr, "Installing BitKeeper in %s\n", dest);
		sprintf(buf, "bk install -f \"%s\"", dest);
		system(buf);
		p = getenv("BK_OLDPATH");
		tmp = malloc(strlen(p) + MAXPATH);
		sprintf(tmp, "PATH=%s%c%s", dest, PATH_DELIM, p);
		putenv(tmp);
		fprintf(stderr, "\nInstalled version information:\n\n");
		sprintf(buf, "bk version", dest);
		system(buf);
		fprintf(stderr, "\nInstallation directory: ");
		sprintf(buf, "bk bin", dest);
		system(buf);
		fprintf(stderr, "\n");
	} else {
		sprintf(buf, "bk installtool");
		for (i = 1; av[i]; i++) {
			strcat(buf, " ");
			strcat(buf, av[i]);
		}
		av[i] = 0;
#ifdef	WIN32
		fprintf(stderr, "Running installer...\n");
#endif
		system(buf);
	}

	/* Clean up your room, kids. */
	unless (getenv("BK_SAVE_INSTALL")) {
		chdir("..");
		fprintf(stderr,
		    "Cleaning up temp files in %s%u ...\n", TMP, pid);
		sprintf(buf, "%s %s%u", RMDIR, TMP, pid);
		system(buf);	/* careful */
	}

	/*
	 * Bitchin'
	 */
	exit(0);
}

void
extract(char *name, char *data, u32 size, char *dir)
{
	int	fd, n;
	GZIP	*gz;
	char	buf[BUFSIZ];

	sprintf(buf, "%s/%s.zz", dir, name);
	fd = open(buf, O_WRONLY | O_TRUNC | O_CREAT | O_EXCL, 0755);
	if (fd == -1) {
		perror(buf);
		exit(1);
	}
	setmode(fd, _O_BINARY);
	if (write(fd, data, size) != size) {
		perror(buf);
		exit(1);
	}
	close(fd);
	unless (gz = gzopen(buf, "rb")) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/%s", dir, name);
	fd = open(buf, O_WRONLY | O_TRUNC | O_CREAT | O_EXCL, 0755);
	setmode(fd, _O_BINARY);
	while ((n = gzread(gz, buf, sizeof(buf))) > 0) {
		write(fd, buf, n);
	}
	close(fd);
	gzclose(gz);
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

int
isdir(char *path)
{
	struct	stat sbuf;

	if (stat(path, &sbuf)) return (0);
	return (S_ISDIR(sbuf.st_mode));
}
