/*
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
#include "system.h"
#include "../zlib/zlib.h"
#undef	malloc
#undef	strdup

#ifndef MAXPATH
#define	MAXPATH		1024
#endif
#ifdef	WIN32
#define	PFKEY		"\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion"
#define	WIN_UNSUPPORTED	"Windows 2000 or later required to install BitKeeper"
#else
#endif
#define	TMP		"bksetup"

extern unsigned int sfio_size;
extern unsigned char sfio_data[];
extern unsigned int data_size;
extern unsigned char data_data[];
extern unsigned int keys_size;
extern unsigned char keys_data[];

void	cd(char *dir);
void	chomp(char *buf);
void	extract(char *, char *, u32, char *);
char*	findtmp(void);
char*	getdest(void);
void	symlinks(void);

int
main(int ac, char **av)
{
	int	i;
	int	rc = 0, dolinks = 0, upgrade = 0;
	pid_t	pid = getpid();
	FILE	*f;
	char	*p;
	char	*dest = 0, *tmp = findtmp();
	char	tmpdir[MAXPATH], buf[MAXPATH], pwd[MAXPATH];
#ifndef	WIN32
	char	*bindir = "/usr/libexec/bitkeeper";
#else
	char	*bindir = 0;
	char	regbuf[1024];
	int	len = sizeof(regbuf);
	HCURSOR h;

	/* Refuse to install on unsupported versions of Windows */

	/* The following code has been commented out because right now the
	 * bk installer is a Console application (i.e. if it doesn't have
	 * a console because the user double-clicked on the icon, a console
	 * is created. There's a cset in the RTI queue (2005-08-18-001) that
	 * GUIfies the installer. When that cset gets pulled, this code should
	 * be used instead of the next block.
	 *
	 *unless (win_supported()) {
	 *	if (hasConsole()) {
	 *		fprintf(stderr, "%s\n", WIN_UNSUPPORTED);
	 *		exit(1);
	 *	} else {
	 *		MessageBox(0, WIN_UNSUPPORTED, 0, 
	 *		    MB_OK | MB_ICONERROR);
	 *		exit(1);
	 *	}
	 *}
	 */
	 unless (win_supported()) {
	 	MessageBox(0, WIN_UNSUPPORTED, 0, MB_OK | MB_ICONERROR);
	 	exit(1);
	 }

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

	getcwd(pwd, sizeof(pwd));

	/*
	 * If they want to upgrade, go find that dir before we fix the path.
	 */
	if (av[1] && (streq(av[1], "-u") || streq(av[1], "--upgrade"))) {
		upgrade = 1;
		unless (dest = getdest()) dest = bindir;
		cd(tmp);
	} else if (av[1] && (av[1][0] != '-')) {
#ifdef	WIN32
		unless ((av[1][1] ==':') || (av[1][0] == '/')) {
#else
		unless (av[1][0] == '/') {
#endif
			sprintf(buf, "%s/%s", pwd, av[1]);
			dest = strdup(buf);
		} else {
			dest = av[1];
		}
#ifndef	WIN32
		unless (getenv("BK_NOLINKS")) dolinks = 1;
#endif
	} else if (av[1]
#if !defined(WIN32) && !defined(__APPLE__)
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
	cd(tmpdir);

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
	extract("config", keys_data, keys_size, tmpdir);

	/* Unpack the sfio file, this creates ./bitkeeper/ */
#ifdef	WIN32
	/* Winblows is so slow they need status */
	if (system("sfio.exe -im < sfioball")) {
#else
	if (system("sfio.exe -iqm < sfioball")) {
#endif
		if (errno == EPERM) {
			fprintf(stderr,
"bk install failed because it was unabled to execute sfio in %s.\n"
"On some systems this might be /tmp does not have execute permissions.\n"
"In that case try rerunning with TMPDIR set to a new directory.\n",
			    tmpdir);
			exit(1);
		}
		perror("sfio");
		exit(1);
	}
	symlinks();
	
	/*
	 * See if we have an embedded license and move it in place
	 * but don't overwrite an existing config file.
	 * If we find a license in the old dir, set BK_CONFIG with it.
	 */
	sprintf(buf, "%s/config", dest);
	unless (upgrade && exists(buf)) {
		system("bk _eula -v < config > bitkeeper/config 2>"
		    DEVNULL_WR);
		unlink("config");
		/*
		 * If that didn't work, try looking in the original directory.
		 */
		unless (size("bitkeeper/config") > 0) {
			char	buf[MAXPATH*2];
			char	*config = 0;

			unlink("bitkeeper/config");
			cd(pwd);
			sprintf(buf, "bk _preference|bk _eula -v 2>"DEVNULL_WR);
			f = popen(buf, "r");
			while (fgets(buf, sizeof(buf), f)) {
				chomp(buf);
				if (strneq("license:", buf, 8)) {
					config = malloc(2000);
					sprintf(config, "BK_CONFIG=%s;", buf);
					continue;
				}
				unless (strneq("licsign", buf, 7)) continue;
				strcat(config, buf);
				strcat(config, ";");
			}
			pclose(f);
			if (config) putenv(config);
			cd(tmpdir);
		}
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
		cd(pwd);		/* so relative paths work */
		rc = system(buf);
		cd(tmpdir);
			
	}

	/* Clean up your room, kids. */
	unless (getenv("BK_SAVE_INSTALL")) {
		cd("..");
		fprintf(stderr,
		    "Cleaning up temp files in %s%u ...\n", TMP, pid);
		sprintf(buf, "%s%u", TMP, pid);
		/*
		 * Sometimes windows has processes sitting in here and we
		 * have to wait for them to go away.
		 * XXX - what processes?  Why are they here?
		 */
		for (i = 0; i < 10; ) {
			/* careful */
			rmtree(buf);
			unless (isdir(buf)) break;
			sleep(++i);
		}
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
chomp(char *buf)
{
	char	*p;

	if ((p = strchr(buf, '\r')) || (p = strchr(buf, '\n'))) *p = 0;
}

/*
 * If we have symlinks file then emulate:
 * while read a b; do ln -s $a $b; done < symlinks
 */
void
symlinks(void)
{
	FILE	*f;
	char	*p;
	char	buf[MAXPATH*2];

	unless (size("bitkeeper/symlinks") > 0) return;
	cd("bitkeeper");
	unless (f = fopen("symlinks", "r")) goto out;
	while (fgets(buf, sizeof(buf), f)) {
		chomp(buf);
		unless (p = strchr(buf, '|')) goto out;
		*p++ = 0;
		symlink(buf, p);
	}
out:	fclose(f);
	cd("..");
}

void
cd(char *dir)
{
	if (chdir(dir)) {
		perror(dir);
		exit(1);
	}
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
	sprintf(buf, "%s/%s.zz", dir, name);
	unlink(buf);
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

#ifdef WIN32
private int
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
#endif

/*
 * I'm not at all convinced we need this.
 */
char*
findtmp(void)
{
	char	*p;

	if (p = getenv("TMPDIR")) return (p);
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

/* ditto */
#if	defined(linux) && defined(sparc)
#undef  fclose
sparc_fclose(FILE *f)
{
	return (fclose(f));
}
#endif

