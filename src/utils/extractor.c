/*
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
#include "system.h"
#include "../zlib/zlib.h"

#ifndef MAXPATH
#define	MAXPATH		1024
#endif
#ifdef	WIN32
#define	PFKEY		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion"
#define	SFIOCMD		"sfio.exe -im < sfioball"
#define	WIN_UNSUPPORTED	"Windows 2000 or later required to install BitKeeper"
#else
#define	SFIOCMD		"sfio.exe -iqm < sfioball"
#endif
#define	TMP		"bksetup"

static char LICENSE_ERROR[] =
"You do not have a license for BitKeeper and you need one\n"
"to install the software\n"
"You can get a temporary license by sending a mail request\n"
"to sales@bitmover.com.\n"
"\n"
"BitMover can be reached at:\n"
"   +1-408-370-9911 (international and California)\n"
"   888-401-8808 (toll free in the US & Canada)\n"
"during business hours (PST) or via email at sales@bitmover.com.\n"
"Thanks!\n";

static char UPGRADE_ERROR[] =
"You requested an automatic upgrade, but you don't seem to have\n"
"BitKeeper installed on your machine. Make sure that the directory\n"
"where you installed BitKeeper is in your PATH environment variable\n"
"and that you can run 'bk version'\n"
"\n"
"If you still have trouble, BitMover can be reached at:\n"
"   +1-408-370-9911 (international and California)\n"
"   888-401-8808 (toll free in the US & Canada)\n"
"during business hours (PST) or via email at sales@bitmover.com.\n"
"Thanks!\n";

#ifdef	WIN32
static char MSYS_ERROR[] =
"You seem to be running the installer under MINGW-MSYS. The installer\n"
"is not supported under this configuration. Please start a cmd.exe console\n"
"and run the installer from there.\n"
"Thanks!\n";
#endif

extern unsigned int sfio_size;
extern unsigned char sfio_data[];
extern unsigned int data_size;
extern unsigned char data_data[];
extern unsigned int keys_size;
extern unsigned char keys_data[];

void	cd(char *dir);
void	extract(char *, char *, u32, char *);
char*	findtmp(void);
char*	getbkpath(void);
void	symlinks(void);
int	hasDisplay(void);
char	*getBinDir(void);

int
main(int ac, char **av)
{
	int	i;
	int	rc = 0, dolinks = 0, upgrade = 0;
	pid_t	pid = getpid();
	FILE	*f;
	char	*dest = 0, *bkpath = 0, *tmp = findtmp();
	char	*bindir = getBinDir();
	char	tmpdir[MAXPATH], buf[MAXPATH], pwd[MAXPATH];

#ifdef	WIN32
	HCURSOR h;
	char	*p;

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
	 if ((p = getenv("OSTYPE")) && streq(p, "msys")
	      && !getenv("BK_REGRESSION")) {
		 fprintf(stderr, MSYS_ERROR);
		 exit(1);
	 }
	_fmode = _O_BINARY;
#endif

	/* rxvt bugs */
	setbuf(stderr, 0);
	setbuf(stdout, 0);

	getcwd(pwd, sizeof(pwd));

	/*
	 * If they want to upgrade, go find that dir before we fix the path.
	 */
	bkpath = getbkpath();
	if (av[1] && (streq(av[1], "-u") || streq(av[1], "--upgrade"))) {
		upgrade = 1;
		unless (dest = bkpath) {
			fprintf(stderr, UPGRADE_ERROR);
			exit(1);
		}
	} else if (av[1] && (av[1][0] != '-')) {
		dest = strdup(fullname(av[1]));
#ifndef	WIN32
		unless (getenv("BK_NOLINKS")) dolinks = 1;
#endif
	} else if (av[1] && !hasDisplay()) {
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
	safe_putenv("BK_OLDPATH=%s", getenv("PATH"));
	safe_putenv("PATH=%s%c%s/bitkeeper%c%s",
	    tmpdir, PATH_DELIM, tmpdir, PATH_DELIM, getenv("PATH"));

	/* The name "sfio.exe" should work on all platforms */
	extract("sfio.exe", sfio_data, sfio_size, tmpdir);
	extract("sfioball", data_data, data_size, tmpdir);

	/* Unpack the sfio file, this creates ./bitkeeper/ */
	if (system(SFIOCMD)) {
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
	 * extract the embedded config file
	 */
	if (bkpath) concat_path(buf, bkpath, "config");
	if (keys_data[0]) {
		if (bkpath && exists(buf)) {
			/* merge embedded file into existing config */
			sprintf(buf,
			    "bk config -m '%s'/config - > bitkeeper/config",
			    bkpath);
			f = popen(buf, "w");
			fputs(keys_data, f);
			pclose(f);
		} else {
			/* Just write embedded config */
			f = fopen("bitkeeper/config", "w");
			fputs(keys_data, f);
			fclose(f);
		}
	} else if (bkpath && exists(buf)) {
		/* just copy existing config */
		fileCopy(buf, "bitkeeper/config");
		chmod("bitkeeper/config", 0666);
	}

	mkdir("bitkeeper/gnu/tmp", 0777);

	/* cd back to the original pwd so we get any config's */
	cd(pwd);

	if (dest) {
		putenv("BK_NO_GUI_PROMPT=1");
		buf[0] = 0;
		if (f = popen("bk _logging 2>"DEVNULL_WR, "r")) {
			fnext(buf, f);
			pclose(f);
		}
		unless (strstr(buf, "license is current")) {
			    fprintf(stderr, LICENSE_ERROR);
			    rc = 1;
			    goto out;
		    }
		fprintf(stderr, "Installing BitKeeper in %s\n", dest);
		sprintf(buf, "bk install %s %s \"%s\"",
			dolinks ? "-S" : "",
			upgrade ? "-f" : "",
			dest);
		unless (rc = system(buf)) {
			fprintf(stderr, "\nInstalled version information:\n\n");
			sprintf(buf, "'%s/bk' version", dest);
			system(buf);
			fprintf(stderr, "\nInstallation directory: ");
			sprintf(buf, "'%s/bk' bin", dest);
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
		rc = system(buf);
	}

	/* Clean up your room, kids. */
out:	cd(tmpdir);
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
	exit(rc);
}

char *
getBinDir(void)
{
#ifdef WIN32
	char	*bindir;
	char	*buf;

	if (buf = reg_get(PFKEY, "ProgramFilesDir", 0)) {
		bindir = aprintf("%s/BitKeeper", buf);
		free(buf);
	} else {
		bindir = "C:/Program Files/BitKeeper";
	}
	return (bindir);
#else
	return ("/usr/libexec/bitkeeper");
#endif
}

int
hasDisplay(void)
{
#if defined(WIN32) || defined (__APPLE__)
	return (1);
#else
	return (getenv("DISPLAY") != 0);
#endif
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
getbkpath(void)
{
	FILE	*f;
	char	*p;
	char	buf[MAXPATH], buf2[MAXPATH];

	unless (p = which("bk")) return (0);
	free(p);
	unless (f = popen("bk bin", "r")) return (0);

	buf[0] = 0;
	fnext(buf, f);
	pclose(f);
	unless (buf[0])	return (0);
	chomp(buf);
	sprintf(buf2, "bk pwd '%s'", buf);
	f = popen(buf2, "r");
	buf[0] = 0;
	fnext(buf, f);
	pclose(f);
	unless (buf[0]) return (0);
	chomp(buf);
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
