/*
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
#include "../system.h"
#include "../zlib/zlib.h"
#ifdef WIN32
#include "../win32/uwtlib/misc.h"
#endif
#undef	malloc
#undef	mkdir
#undef	putenv
#undef	strdup
#undef	unlink
#undef	fclose

#ifndef MAXPATH
#define	MAXPATH		1024
#endif
#ifdef	WIN32
#define	mkdir(a, b)	_mkdir(a)
#define	RMDIR		"rmdir /s /q"
#define	PFKEY		"\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion"
#else
#define	RMDIR		"/bin/rm -rf"
#endif
#define	TMP		"bksetup"

extern unsigned int sfio_size;
extern unsigned char sfio_data[];
extern unsigned int data_size;
extern unsigned char data_data[];

void	extract(char *, char *, u32, char *);
char	*findtmp(void);
int	isdir(char*);
void	rmTree(char *dir);
char	*getdest(void);

int
main(int ac, char **av)
{
	char	*p, *dest = 0, *tmp = findtmp();
	int	i;
	int	rc = 0;
	pid_t	pid = getpid();
	FILE	*f;
	int	dolinks = 0;
	int	upgrade = 0;
	char	tmpdir[MAXPATH];
	char	buf[MAXPATH];
#ifndef	WIN32
	char	*bindir = "/usr/libexec/bitkeeper";
#else
	char	*bindir = 0;
	char	regbuf[1024];
	int	len = sizeof(regbuf);
	HCURSOR h;

	_fmode = _O_BINARY;
	if (getReg(HKEY_LOCAL_MACHINE,
	    PFKEY, "ProgramFilesDir", regbuf, &len)) {
		sprintf(buf, "%s/BitKeeper", regbuf);
		bindir = strdup(buf);
	} else {
		bindir = "C:/Program Files/BitKeeper";
	}
#endif

	/* rxvt bugs */
	setbuf(stderr, 0);
	setbuf(stdout, 0);

	/*
	 * If they want to upgrade, go find that dir before we fix the path.
	 */
	if (av[1] && (streq(av[1], "-u") || streq(av[1], "--upgrade"))) {
		upgrade = 1;
		unless (dest = getdest()) dest = bindir;
		if (chdir(tmp)) {
			perror(tmp);
			exit(1);
		}
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
#ifndef	WIN32
		unless (getenv("BK_NOLINKS")) dolinks = 1;
#endif
	} else if (av[1]
#ifndef WIN32
		   || !getenv("DISPLAY")
#endif
		   ) {
		fprintf(stderr, "usage: %s [-u || <directory>]\n", av[0]);
		fprintf(stderr,
"Installs BitKeeper on the system.\n"
"\n"
"With no arguments this installer will unpack itself in a temp\n"
"directory and then start a graphical installer to walk through the\n"
"installation.\n"
"\n"
"If a directory is provided on the command line then a default\n"
"installation is written to that directory.\n"
"\n"
"The -u option is for batch upgrades.  The existing BitKeeper is\n"
"found on your PATH and then this version is installed over the top\n"
"of it.  If no existing version of BitKeeper can be found, then a\n"
"new installation is written to %s\n"
"\n"
#ifdef WIN32
"Administrator privileges are required for a full installation.  If\n"
"installing from a non-privileged account, then the installer will only\n"
"be able to do a partial install.\n"
#else
"Normally symlinks are created in /usr/bin for 'bk' and common SCCS\n"
"tools.  If the user doesn't have permissions to write in /usr/bin\n"
"or BK_NOLINKS is set then this step will be skipped.\n"
"\n"
"If DISPLAY is not set in the environment, then the destination must\n"
"be set on the command line.\n"
#endif
			, bindir);
		exit(1);
	}
	/* dest =~ s,\,/,g */
	if (dest) for (p = dest; *p; p++) if (*p == '\\') *p = '/';

	sprintf(tmpdir, "%s/%s%u", tmp, TMP, pid);
#ifdef	WIN32
	h = SetCursor(LoadCursor(0, IDC_WAIT));
#endif
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
		sprintf(buf, "bk install %s %s \"%s\"",
			dolinks ? "-S" : "",
			upgrade ? "-f" : "",
			dest);
		unless (system(buf)) {
			/* Why not just run <dest>/bk version  ?? -Wayne */
			p = getenv("BK_OLDPATH");
			tmp = malloc(strlen(p) + MAXPATH);
			sprintf(tmp, "PATH=%s%c%s", dest, PATH_DELIM, p);
			putenv(tmp);
			fprintf(stderr, "\nInstalled version information:\n\n");
			sprintf(buf, "bk version");
			system(buf);
			fprintf(stderr, "\nInstallation directory: ");
			sprintf(buf, "bk bin");
			system(buf);
			fprintf(stderr, "\n");
		}
	} else {
		sprintf(buf, "bk installtool");
		for (i = 1; av[i]; i++) {
			strcat(buf, " ");
			strcat(buf, av[i]);
		}
		av[i] = 0;
#ifdef	WIN32
		fprintf(stderr, "Running installer...\n");
		SetCursor(h);
#endif
		/*
		 * Use our own version of system()
		 * because the native one on win98 does not return the
		 * correct exit code
		 */
		rc = system(buf);
	}

	/* Clean up your room, kids. */
	unless (getenv("BK_SAVE_INSTALL")) {
		chdir("..");
		fprintf(stderr,
		    "Cleaning up temp files in %s%u ...\n", TMP, pid);
		sprintf(buf, "%s%u", TMP, pid);
		rmTree(buf); /* careful */
	}

	/*
	 * Bitchin'
	 */
#ifdef WIN32
	if (rc == 2) {
		do_reboot(
		    "Some BitKeeper files from the previous install\n"
		    "are in active use and cannot be deleted immediately.\n"
	       	    "They will be deleted after the next reboot.\n"
		    "Do you want to reboot the system now?\n");
	}
#endif
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

char *
getdest(void)
{
	FILE	*f = popen("bk bin", "r");
	char	*p;
	char	buf[MAXPATH], buf2[MAXPATH];

	unless (f) return (0);

	buf[0] = 0;
	fgets(buf, sizeof(buf), f);
	unless (buf[0]) {
		pclose(f);
		return (0);
	}
	pclose(f);
	for (p = buf; *p; p++);
	*--p = 0;
	if (p[-1] == '\r') p[-1] = 0;
	sprintf(buf2, "bk pwd '%s'", buf);
	f = popen(buf2, "r");
	buf[0] = 0;
	fgets(buf, sizeof(buf), f);
	unless (buf[0]) {
		pclose(f);
		return (0);
	}
	pclose(f);
	for (p = buf; *p; p++);
	*--p = 0;
	if (p[-1] == '\r') p[-1] = 0;
	return (strdup(buf));
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

#ifdef	WIN32
void
rmTree(char *dir)
{
	char buf[MAXPATH], cmd[MAXPATH + 12];

	if (isWin98()) {
		sprintf(cmd, "deltree /Y %s", dir);
	} else {
		sprintf(cmd, "rmdir /s /q %s", dir);
	}
	/* use native system() funtion  so we get cmd.exe/command.com */
	(system)(cmd);
}

#else
void
rmTree(char *dir)
{
	char cmd[MAXPATH + 11];

	sprintf(cmd, "/bin/rm -rf %s", dir);
	system(cmd);
}
#endif

#ifdef	WIN32

/* stolen from BK source */
int
getReg(HKEY hive, char *key, char *valname, char *valbuf, int *lenp)
{
        int	rc;
        HKEY    hKey;
        DWORD   valType = REG_SZ;
	DWORD	len = *lenp;

	valbuf[0] = 0;
        rc = RegOpenKeyEx(hive, key, 0, KEY_QUERY_VALUE, &hKey);
        if (rc != ERROR_SUCCESS) return (0);

        rc = RegQueryValueEx(hKey,valname, NULL, &valType, valbuf, &len);
	*lenp = len;
        if (rc != ERROR_SUCCESS) return (0);
        RegCloseKey(hKey);
        return (1);
}
#endif
