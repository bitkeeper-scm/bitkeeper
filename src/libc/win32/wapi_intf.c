/*
 * Copyright 1999-2009,2013-2016 BitMover, Inc
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

/* This file interface with the win32 API */
/*
 * In general, we view the application running in a layered environment
 * (see diagram below).
 * In the BitMover's Application Library Layer, we
 * use only "BitMover's path name format": i.e All path name have forward slash
 * BitMover's path name is like Unix path, but "drive-letter colon"
 * is also allowed. (e.g. C:/temp )
 * In the Win32 C runtime layer, All path name have backward slash
 * We sometime call the backward slash format the NT path format.
 *
 * For the code in User Interface and Application Library layers,
 * all calls to the C runtime is remapped (with #define) to
 * a glue function in the Win32 transition layers
 *
 * We use nt2bmname and bm2ntname to translate the path
 * when a function is called across layer boundary
 *
 * | User interface Layer
 * | (This layer allows both NT & Unix name)
 * | Things like admin.c ci.c co.c sits in this layers
 * --------------------------------------------------
 * | BitMover Appication library Layer
 * | (All Path name is in BitMover format)
 * | Things in slib sits in this layers
 * --------------------------------------------------
 * | Win32 transition layers
 * | (In this layer, we always return path name to upper layers
 * | in BitMover format. We always transltas pathname to NT
 * | format when we call down to the lower layer)
 * | Things in wcrt_intf.c & wapi_intf.c sits in this layers
 * ---------------------------------------------------
 * | Win32 C runtime layers
 * | (All path name has backward slash)
 * | Microsoft VC++ C run time stuff sits in this laters
 * | ------------------------------
 * | Win32 basic API layer
 * | Thing like "MapViewOfFile()" sits in this layer
 * ----------------------------------------------------
 *
 *
 * Note:  Two invariants are enforced for functions in the
 *	  Win32 transition layer:
 *	  a)	Path name is always translated to NT format before
 *		calling down to Win32 runtime layer,
 *	  b)	Path name is always translated to BitMover format
 *		before returning back to upper layers.
 *
 *				Andrew Chang awc@bitmover.com 1998
 */

#define	_RE_MAP_H_	/* Don't remap API */
#include "system.h"

static	int	win32_flags = WIN32_NOISY | WIN32_RETRY;

/* Win32 Retry Loops */
static	int	retries = 16;	/* how many times to retry */
#define	INC	100	/* milliseconds, we do 100, 200, 300, 400, etc */
#define	NOISY	10	/* start telling them after NOISY sleeps (~5 sec) */

private	BOOL	IsWow64(void);

/* get, set , and clear the flags for the operation of the win32
 * emulation layer look at the static int above and src/libc/win32.h
 * for the allowed values
 */
int
win32flags_get()
{
	return win32_flags;
}

/* see comment above */
void
win32flags_set(int flags)
{
	win32_flags |= flags;
}

/* see comment above */
void
win32flags_clear(int flags)
{
	win32_flags &= ~flags;
}

void
win32_retry(int times)
{
	unless (times) {
		win32flags_clear(WIN32_RETRY);
		retries = 1;
	} else {
		win32flags_set(WIN32_RETRY);
		retries = times;
	}
}

/*
 * This function returns the path name of the tmp directory.
 * It also sets the TMP environment variable to the tmp directory found.
 * The returned path name is in BitMover format.
 */
const char *
nt_tmpdir()
{
	static	char *tmpdir = NULL;
	char	*env[] = {"TMP", "TEMP", "USERPROFILE", 0};
	char	*paths[] = {"c:/tmp",
			    "c:/temp", "c:/windows", "c:/windows/temp", 0};
	char	*p;
	char	**pp;
	char	buf[MAX_PATH];

	if (tmpdir) return (tmpdir);

	/* try hard to find a suitable TMP directory */
	for (pp = env; *pp; pp++) {
		if ((p = getenv(*pp)) && (!nt_access(p, W_OK|R_OK)) ) {
			tmpdir = p;
			break;
		}
	}
	unless (tmpdir) {
		for (pp = paths; *pp; pp++) {
			if (nt_access(*pp, W_OK|R_OK) == 0) {
				tmpdir = *pp;
				break;
			}
		}
	}

	/* turn tmpdir into a short path because our guis barf 
	 * with paths that have spaces
	 */
	unless (tmpdir && GetShortPathName(tmpdir, buf, sizeof(buf))) {
		/* Hail Mary, full of grace... */
		tmpdir = 0;
		return (0);
	}

	nt2bmfname(buf, buf);
	p = malloc(strlen(buf) + 5);
	sprintf(p, "TMP=%s", buf);
	putenv(p);
	p = malloc(strlen(buf) + 6);
	sprintf(p, "TEMP=%s",buf);
	putenv(p);

	tmpdir = malloc(strlen(buf) + 1);
	strcpy(tmpdir, buf);
	return (tmpdir);
}

/*
 * Windows has a 100 ns resolution clocks.  Or so they say.
 */
void
gettimeofday(struct timeval *tv, struct timezone *tz)
{
	FILETIME ft;
	unsigned long long ticks;

	GetSystemTimeAsFileTime(&ft);
	ticks = (unsigned long long)ft.dwHighDateTime << 32;
	ticks += ft.dwLowDateTime;
	ticks /= 10;
	tv->tv_sec = ticks / 1000000;
	tv->tv_usec = ticks % 1000000;
}

char *
getlogin(void)
{
	static char name[NBUF_SIZE];
	long	len = sizeof(name);

	GetUserName(name, &len);
	if (len >= NBUF_SIZE) {
		name[NBUF_SIZE -1] = '\0';
		debug((stderr,
		    "getlogin: User name %s... is too long, %d\n", name, len));
	}

	/*
	 * Change all space in user name to dot
	 */
	_switch_char(name, name, ' ', '.');
	return (name);
}

void
nt_sync(void)
{
	HANDLE	vhandle;
	char	cwd[MAXPATH], volume[MAXPATH];

	if (GetCurrentDirectory(sizeof(cwd), cwd) == 0) return;
	if (GetVolumePathName(cwd, volume, sizeof(volume)) == 0) return;
	/*
	 * for non-Administrator users, the CreateFile() and FlushFileBuffers()
	 * calls should fail but we don't care.  Let it not be said we didn't
	 * do the least we could.
	 */
	vhandle = CreateFile(volume,
	    GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	if (vhandle == INVALID_HANDLE_VALUE) return;
	FlushFileBuffers(vhandle);
	CloseHandle(vhandle);
}

int
fsync (int fd)
{
	if (fd < 0) return (-1);
	if (FlushFileBuffers ((HANDLE) _get_osfhandle(fd)) == 0)
		return (-1);
	else	return (0);

}

static void
_getWinSockHostName(char buf[], int size)
{
	struct	hostent FAR * hostent;

	buf[0] = 0;
	nt_loadWinSock();

	gethostname(buf, size);
	if (!(hostent = gethostbyname(buf)) || !(hostent->h_name)) {
		return;
	} else {
		strncpy(buf, hostent->h_name, size -1);
		if (strlen(hostent->h_name) > (unsigned int) size) {
			debug((stderr,
			    "GetWinSockHostName: hostname too long: %s\n",
			    hostent->h_name));
			buf[size -1] = 0;
		}
	}
}

int
getdomainname(char *buf, size_t size)
{
	char	*p;

	_getWinSockHostName(buf, size);
	for (p = buf; *p && *p != '.'; p++); /* strip hostname */
	if (*p == '.') p++;
	strcpy(buf, p);
	return (0);
}

int
uname(struct utsname *u)
{
	long	len = sizeof(u->nodename);
	char	*name = u->nodename;

	/*
	 * First we try to get the internet hostname
	 */
	_getWinSockHostName(name, len);
	if (!name[0]) {
		/*
		 * No internet hostname, get the WIN32 hostname
		 */
		GetComputerName(name, &len);
		if (len >= sizeof(u->nodename)) {
			name[sizeof(u->nodename) -1] = '\0';
			debug((stderr,
			    "uname: System name %s... is too long, %d\n",
			    name, len));
			return (-1);
		}
	}
	strcpy(u->sysname, "Windows");
	return (0);
}

void
make_fd_inheritable(int fd)
{
	HANDLE	outhnd;

	outhnd = (HANDLE) _get_osfhandle(fd);
	SetHandleInformation(outhnd, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

}

void
make_fd_uninheritable(int fd)
{
	HANDLE	outhnd;

	outhnd = (HANDLE) _get_osfhandle(fd);
	SetHandleInformation(outhnd, HANDLE_FLAG_INHERIT, 0);
}

#define	MAX_NUM_DIR	96	
struct	dir_info dtbl[MAX_NUM_DIR] = {	{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L},
					{-1L}, {-1L}, {-1L}, {-1L}};


DIR *
opendir(const char *dirname)
{
	int	i;
	DIR 	*d;
	char	tmp_buf[NBUF_SIZE];
	struct	_finddata_t found_file;

	for (i = 0; dtbl[i].dh != -1L; i++);
	if (i >= MAX_NUM_DIR) {
		fprintf(stderr, "opendir: no space in dir info table\n");
		errno = ENOSPC; /* no more space in dir table */
		return (NULL);
	}

	d = &dtbl[i];
	strcpy(tmp_buf, dirname);
	strcat(tmp_buf, "/*.*");
	if ((d->dh = _findfirst(tmp_buf, &found_file)) == -1L) {
		errno = 0; /* clear the noise error */
		return (NULL);
	}
	nt2bmfname(found_file.name, d->first);
	return (d);
}

struct	dirent *
readdir(DIR *d)
{
	static	struct dirent ebuf;
	struct	_finddata_t found_file;

	if (d == NULL) return (NULL);

	if (d->first[0]) {
		strcpy(ebuf.d_name, d->first);
		d->first[0] = 0;
	} else {
		if (_findnext(d->dh, &found_file) != 0) {
			errno = 0; /* clear the noise error */
			return (NULL);
		}
		nt2bmfname(found_file.name, ebuf.d_name);
	}
	return (&ebuf);
}

void
closedir(DIR *d)
{
	_findclose(d->dh);
	d->dh = -1L;
}


/*
 * Maximum number of open mmap supported at the same time.
 */
#define	MAX_NUM_OF_MMAP 64

#define	BAD_ADDR ((caddr_t) -1)

private struct mmap_info {
	caddr_t	addr;
	int	fd;
	size_t	 size;
} mi[MAX_NUM_OF_MMAP];
private	int mi_init = 1;

caddr_t
mmap(caddr_t addr, size_t len, int prot, int flags, int fd, off_t off)
{
	HANDLE	fh, mh;
	LPVOID	p;
	DWORD	flProtect, dwDesiredAccess, err;
	int	i;
	char	mmap_name[100];

	debug((stderr, "mmap: fd = %d\n", fd));
	if (mi_init) {
		for (i = 0; i < MAX_NUM_OF_MMAP; i++) mi[i].fd = -1;
		mi_init = 0;
	}

	/*
	 * Bitkeeper sometime call mmap() with fd == -1
	 */
	if (fd < 0) return (BAD_ADDR);

	/*
	 * Check if we are re-mapping a existing mmap area
	 */
	for (i = 0; i < MAX_NUM_OF_MMAP; i++) {
		if (mi[i].fd == fd) {
			debug((stderr, "mmap: fd = %d, auto unmap\n", fd));
			munmap(mi[i].addr, mi[i].size); /* auto unmap */
			break;
		}
	}

	/*
	 * If this is a new mmap operation, look for a empty slot
	 */
	if (i >= MAX_NUM_OF_MMAP) {
		for (i = 0; i < MAX_NUM_OF_MMAP; i++) {
			if (mi[i].fd == -1) break;
		}
		if (i >= MAX_NUM_OF_MMAP) {
			fprintf(stderr,
			    "%s:%d: mmap table is full\n", __FILE__, __LINE__);
			exit(1);
		}
	}

	if (prot & PROT_WRITE) {
		flProtect = PAGE_READWRITE;
		dwDesiredAccess = FILE_MAP_WRITE;
	} else {
		flProtect = PAGE_READONLY;
		dwDesiredAccess = FILE_MAP_READ;
	}

	fh = (HANDLE) _get_osfhandle(fd);

	sprintf(mmap_name, "mmap%5d-%5d", getpid(), i); /* id must be unique */
	if ((mh = CreateFileMapping(
	    fh, NULL, flProtect, 0, len, mmap_name)) == NULL) {
		err = GetLastError(); /* set errno */
		fprintf(stderr, "mmap: cannot CreateFileMapping file, "
				"error code =%lu, writeMode = %x, fd =%d\n",
				err, prot & PROT_WRITE, fd);
		return (BAD_ADDR);
	}

	/*
	 * Mmap interface  support 32 bit offset
	 * MapViewOfFileEx support 64 bit offset
	 * We just set the high order 32 bit to zero for now
	 */
	p = MapViewOfFileEx(mh, dwDesiredAccess, 0,  off, len, addr);
	if (p == NULL) {
		err = GetLastError(); /* set errno */
		fprintf(stderr,
		    "mmap: can not MapViewOfFile , error code =%lu\n", err);
		return (BAD_ADDR);
	}
	safeCloseHandle(mh);
	mi[i].addr = p;
	mi[i].fd = fd;
	mi[i].size = len;

	debug((stderr,
		">mmap: fd = %d, addr = 0x%x, end = 0x%x\n",
		mi[i].fd, p, mi[i].addr + mi[i].size));
	return ((caddr_t) p);
}

int
munmap(caddr_t addr, size_t notused)
{
	DWORD	err;
	int i;

	/*
	 * Bitkeeper some time call munmap before a successfull mmap
	 */
	if (addr == BAD_ADDR) return (-1);

	for (i = 0; i < MAX_NUM_OF_MMAP; i++) {
		if (mi[i].addr == addr)
			break;
	}
	if (i >= MAX_NUM_OF_MMAP) {
		debug((stderr,
		    "munmap address does not equal previous mmap address\n"));
		return (-1);
	}

	if (UnmapViewOfFile((LPVOID) addr) == 0) {
		err = GetLastError(); /* set errno */
		debug((stderr,
		    "munmap: can not UnmapViewOfFile file, error code =%d\n",
		    err));
		return (-1);
	}

	debug((stderr,
	    "<mumap: fd = %d, addr = 0x%x, end = 0x%x\n",
	    mi[i].fd, mi[i].addr, mi[i].addr + mi[i].size));
	mi[i].fd = -1;
	mi[i].addr = BAD_ADDR;
	return (0);
}

void
msync(caddr_t addr, size_t size, int mode)
{
	if (FlushViewOfFile((LPVOID) addr, size) == 0)
		fprintf(stderr,
			"can not msync file, addr =0x%p, error code=%lu\n",
			addr, GetLastError());
}

#define	BAD_HANDLE	((HANDLE)-1)
int
ftruncate(int fd, size_t len)
{
	HANDLE h = (HANDLE) _get_osfhandle(fd);
	size_t old_loc;

	if (h == BAD_HANDLE) {
		errno = EBADF;
		return (-1);
	}

	/*
	 * Save current file location
	 */
	old_loc = _lseek(fd, 0, SEEK_CUR);

	_lseek(fd, len, SEEK_SET);
	if (!SetEndOfFile(h)) {
		(void)GetLastError();			/* set errno */
		fprintf(stderr, "error = %d\n", errno); /* awc debug */
		return (-1);
	}

	/*
	 * Restore original file location
	 */
	_lseek(fd, old_loc, 0);
	return (0);
}


void
nt_sleep(int i)
{
	Sleep(i*1000);
}


void
usleep(unsigned long i)
{
	Sleep(i/1000);
}


/*
 * Return ture if a console is detected
 */
int
hasConsole()
{
	HANDLE hConsole =
		CreateFile("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE,
			    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hConsole == INVALID_HANDLE_VALUE) return 0;
	safeCloseHandle(hConsole);
	return (1);

}

/*
 * Expand path for cmdname based on the path list in the PATH
 * environment variable
 * If the cmdname is a absolute path, use that.
 */
static char *
_expnPath(char *cmdname, char *ext, char *fullCmdPath)
{
	char	path[2048], *p, *t, cmdbuf[1024];
	char	*path_delim;

	strcpy(cmdbuf, cmdname);
	if (ext) strcat(cmdbuf, ext);

	/* Ignore PATH if cmdbuf contains full or partial path */
	if (strchr(cmdbuf, '/')) {
		strcpy(fullCmdPath, cmdbuf);
		if (nt_access(fullCmdPath, F_OK) != 0) fullCmdPath[0] = 0;
		return fullCmdPath;
	}

	p = getenv("PATH");
	if (!p) return NULL;
	path_delim = strchr(p, ';') ? ";" : ":";
	strncpy(path, p, sizeof (path));
	p = path;
	t = strtok(p, path_delim);
	while (t) {
		sprintf(fullCmdPath, "%s/%s", t, cmdbuf);
		if (nt_access(fullCmdPath, F_OK) == 0) return (fullCmdPath);
		t = strtok(NULL, path_delim);
	}

	/*
	 * Try the dot path, we need this for shell script
	 */
	strcpy(fullCmdPath, cmdbuf);
	if (nt_access(fullCmdPath, F_OK) == 0) return (fullCmdPath);

	fullCmdPath[0] = 0;
	return fullCmdPath;
}

/*
 * Expand path by tagging on ".exe" or ".com" extension
 */
static char *
expnPath(char *cmdname, char *fullCmdPath)
{
	/* first try it with a .exe extension */
	_expnPath(cmdname, ".exe", fullCmdPath);
	if (fullCmdPath[0] != 0) return fullCmdPath;
	/* now try it without tagging on any extension */
	_expnPath(cmdname, NULL, fullCmdPath);
	if (fullCmdPath[0] != 0) return fullCmdPath;
#if	0
	/* now try tagging on ".com" */
	_expnPath(cmdname, ".com", fullCmdPath);
#endif
	return fullCmdPath;
}

static	OSVERSIONINFOEX	osinfo;

static	void
get_osinfo(void)
{
	if (!osinfo.dwOSVersionInfoSize) {
		osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		if (GetVersionEx((OSVERSIONINFO *)&osinfo) == 0) {
			fprintf(stderr, "Warning: cannot get os version\n");
			osinfo.dwOSVersionInfoSize = 0;
		}
	}
}

/*
 * Return false if OS is too old.
 */
int
win_supported(void)
{
	get_osinfo();
	return (osinfo.dwMajorVersion > 5); /* older than Vista unsupported */
}

/*
 * Return true if OS is Vista, win7, win8...
 * XXX What is this for and is it or will it be invalid?
 */
int
has_UAC(void)
{
	get_osinfo();
	return (osinfo.dwMajorVersion >= 6);
}

/* from http://msdn.microsoft.com/en-us/library/ms684139(VS.85).aspx */

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

LPFN_ISWOW64PROCESS fnIsWow64Process;

private	BOOL
IsWow64(void)
{
    BOOL bIsWow64 = FALSE;

    fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(
        GetModuleHandle(TEXT("kernel32")),"IsWow64Process");
  
    if (NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
        {
            // handle error
        }
    }
    return bIsWow64;
}


#define	CUR_VER \
	"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"

/* return malloc'ed version string */
char *
win_verstr(void)
{
	int	major, minor;
	char	*p, *t;

	major = osinfo.dwMajorVersion;
	minor = osinfo.dwMinorVersion;
	p = reg_get(CUR_VER, "ProductName", 0);
	if (p) {
		for (t = p; *t; t++) {
			if (*t == ' ') *t = '_';
		}
	} else {
		p = aprintf("unknown-%d.%d", major, minor);
	}

	if (osinfo.wServicePackMajor) {
		t = p;
		p = aprintf("%s-sp%d", t, osinfo.wServicePackMajor);
		free(t);
	}
	if (IsWow64()) {
		t = p;
		p = aprintf("%s-64", t);
		free(t);
	}

	return (p);
}

#define	MAXPATH	1024

/*
 * Parse the Unix style magic "#! ...." header into a av[] vector
 * e.g. "perl -w -whatever" =>
 * 			av[0] = "perl"
 *			av[1] = "-w -whatever"
 *			av[2] = 0;
 * Also expand av[0] into full path format.
 */
static char *
getShell(char *p, char *buf)
{
	char	*q, *s;
	char	*out = buf;
	char	fullShellPath[MAXPATH];

	if (*p++ != '#') return (NULL);
	if (*p++ != '!') return (NULL);

	for (q = p; *q; q++);
	q--;
	if (*q == '\n') q--; /* strip LF */
	while ((q > p) && isspace(*q)) q--; /* strip trailing spaces */
	q[1] = 0;

	while (*p && isspace(*p)) p++; /* strip leading spaces */
	if (*p == 0) return (NULL);
	/* find end of program name */
	for (q = p; *q && !isspace(*q); q++);
	if (*q) *q++ = 0;
	/* ignore full path, we just want the base name */
	s = strrchr(p, '/');
	expnPath(s ? s + 1 : p, fullShellPath);
	if (!fullShellPath[0]) {
		fprintf(stderr, "Cannot expand shell path \"%s\"\n",
		    s ? s + 1 : p);
		return (NULL);
	}
	out += sprintf(out, "\"%s\" ", fullShellPath);
	while (*q && isspace(*q)) q++; /* skip space after interp */

	/* get option part, don't bother with quote junk */
	if (*q && !strchr(q, '\"') && !strchr(q, '\'')) {
		sprintf(out, "\"%s\" ", q);
	}
	return (buf);
}

typedef	struct {
	pid_t	pid;
	HANDLE	h;
} proc;

private	proc	*procs;
private	int	maxproc;

static	int
empty_proc(void)
{
	int	i;
	proc	*p;

	unless (procs) procs = calloc(maxproc = 10, sizeof(proc));
	for (i = 0; i < maxproc; ++i) unless (procs[i].pid) return (i);
	/* realloc */
	p = calloc(maxproc * 2, sizeof(proc));
	assert(p);
	memcpy(p, procs, maxproc * sizeof(proc));
	i = maxproc;
	maxproc *= 2;
	free(procs);
	procs = p;
	return (i);
}

/*
 *  This is a incomplete implementation of waitpid
 *  Missing is pid < -1 means reap group.
 */

private	pid_t
_waitpid(int i, int *status, int flags)
{
	DWORD	timeout = (flags & WNOHANG) ? 0 : INFINITE;
	DWORD	exitcode;
	int	ret;

	if (status) *status = 0;
	switch (WaitForSingleObject(procs[i].h, timeout)) {
	    case WAIT_OBJECT_0:
		unless (GetExitCodeProcess(procs[i].h, &exitcode)) {
			ret = -1;
			break;
		}
		if (exitcode == STILL_ACTIVE) {
			/* XXX: I dunno if a process can be in signaled
			 * state and not have ended. Better handle it.
			 */
			ret = 0;
			break;
		}
		if (status) *status = (int)(exitcode & 0xff);
	        ret = procs[i].pid;
		safeCloseHandle(procs[i].h);
		procs[i].h = 0;
		procs[i].pid = 0;
	        break;
	    case WAIT_TIMEOUT:
		ret = 0;
		break;
	    case WAIT_FAILED:
	    default:
	        ret = -1;
	        break;
	}
	if (ret == -1) GetLastError(); /* set errno */
	return (ret);
}

pid_t
waitpid(pid_t pid, int *status, int flags)
{
	int	i;

	if (pid > 0) {
		for (i = 0; i < maxproc; ++i) {
			if (procs[i].pid == pid) {
				return (_waitpid(i, status,flags));
			}
		}
		errno = ECHILD;
		return (-1);
	}
	assert (pid == -1);
	/* Look through them all, return when we find one */
	for (i = 0; i < maxproc; ++i) {
		unless (procs[i].pid) continue;
		/* ignore failures? */
		if ((pid = _waitpid(i, status, flags)) > 0) return (pid);
	}
	return (0);
}

/*
 * Our version of spawnvp;
 * we need this becuase we need to suppress the creation of
 * dos console if it is not already in existence.
 * fd 0, 1, 2 is also automaticaly inherited by the child process.
 *
 * Technical Notes on Windows process creation model:
 * The lowest process creation API in Windows win32 environment
 * is CreateProcess(). This API uses a flat line buffer to pass
 * command and argument into the new process.
 * Since C based process wants to use av[] vector to pass command and
 * arguments, the C library on Windows provides a wrapper function
 * named spawnvp_*(), which takes the av[] vector and converts it into
 * a flat command buffer. When the new child
 * process starts up, a hidden C-run-time-support code converts the
 * flat command line buffer in to ac and av[] that you normally
 * see in main(int ac, char **av) of a C program.
 * The conversion process from av[] -> flat command -> av[]
 * is a bit complicated due to the following reasons:
 * a) The quoting model is different between Windows and Unix.
 * b) There is a bug in the Windows C-runtime-support code when
 *    converting embeded double quote in the flat buffer back to the av[]
 *    vector. See the special code below that workaround this bug.
 */
pid_t
_spawnvp_ex(int flag, const char *cmdname, char *const av[], int fix_quote)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	HANDLE	hProc = GetCurrentProcess();
	char	*cmdLine, fullCmdPath[2048];
	char	*p, *q;
	DWORD	status = 0, err;
	int	cflags, i, j;
	int	len, cmd_len;
	int	do_quote = 1;

	/*
	 * Compute the size of the av[] vector
	 * and allocate a sufficiently big comdLine buffer
	 */
	for (i = 0, cmd_len = 0; av[i]; i++) cmd_len += strlen(av[i]) + 1;
	cmd_len += 1024; /* for quotes and/or shell path */
	cmdLine = (char *) malloc(cmd_len);
	assert(cmdLine);
	i = 0; /* important */


	expnPath((char *) cmdname, fullCmdPath);
	if (fullCmdPath[0] == 0) {
		fprintf(stderr, "%s: cannot expand path: %s\n",
						cmdname, getenv("PATH"));
		free(cmdLine);
		return (-1);
	}
	p = cmdLine;

	/*
	 * Support Windows batch file
	 */
	len = strlen(fullCmdPath);
	if ((len > 4) && (strcmp(&fullCmdPath[len - 4], ".bat") == 0)) {
		sprintf(cmdLine, "cmd.exe /c \"%s\"", fullCmdPath);
		i = 1;
		len = strlen(cmdLine);
		p = &cmdLine[len];
	} else if ((len <= 4) ||
	    (!strcmp(&fullCmdPath[len - 4], ".exe") == 0)) {
		FILE	*f;
		char	script[MAXPATH];
		char	header[MAXPATH + 80] = "";

		/*
		 * Not a real binary, may be a shell script
		 * so check the magic header in the file
	 	 * e.g "#! /usr/bin/perl -w" or "#! /bin/sh"
		 */
		if (!(f = fopen(fullCmdPath, "rt"))) {
			perror(fullCmdPath);
			return (-1);
		}
		if (!fgets(header, sizeof(header), f)) {
			perror(fullCmdPath);
			return (-1);
		}
		fclose(f);

		/*
		 * We want cmdLine to contain: 
		 *     av[0] ... av[n] script
		 *     e.g. /usr/bin/perl -w script
		 */
		if (getShell(header, cmdLine) == NULL) return (-1);
		nt2bmfname(fullCmdPath, script);
		if (do_quote = (int)strchr(script, ' ')) strcat(cmdLine, "\"");
		strcat(cmdLine, script);
		if (do_quote) strcat(cmdLine, "\"");
		len = strlen(cmdLine);
		p = &cmdLine[len];
		i = 1;
	}

	while (q = av[i]) {
		/*
		 * Quote each av[i] with ""
		 * otherwise CreateProcess() will break it up
		 * at the space.
		 * For NT shell, we only do quote if there is
		 * space in the av[i].
		 */
		do_quote = (int) strchr(q, ' ');
		if (i) *p++ = ' ';  /* add space   */
		if (do_quote) *p++ = '\"'; /* start quote " */
		while (*q) {
			if (*q == '\"') *p++ = '\"'; /* escape embedded quote */
			/*
			 * This strange code is a hack to get around 
			 * a bug in NT c-run time av[] conversion 
			 * and bug in cygwin av[] conversion.
			 */
			if (fix_quote && (*q == '\"')) {
				*p++ = '\"'; /* to force inquote" mode */
			}
			*p++ = *q++;
			if ((p - cmdLine) >= cmd_len) {
				fprintf(stderr,
				    "spawnvp: command line too long\n");
				free(cmdLine);
				return (-1);
			}
		}
		if (do_quote) *p++ = '\"'; /* end quote " */
		i++;
	}
	*p = 0;


	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);

	assert(_P_DETACH == P_DETACH);
	if (flag == P_DETACH) {
		/*
		 * What I want is to create a process that has no open fd's.
		 * That's perhaps a bit weird compared to unix, it's not the
		 * same as a setsid().  So anyplace that we call this interface
		 * with P_DETACH we had better be prepared to open up our own
		 * file descriptors.
		 *
		 * The reason for this is simple: if we don't completely
		 * detach from the terminal on windows the bkd -d process
		 * dies if you hit ^C in that shell.  Sucks but that's life.
		 *
		 * I did leave a note in the Unix spawn code, maybe you'll 
		 * read that and come here.  And I closed all the fd's to
		 * match this behaviour so you'll come here when things don't
		 * work.
		 *
		 * Caveat emptor.
		 */
		if (!CreateProcess(0, cmdLine, 0, 0, 0,
		    DETACHED_PROCESS|CREATE_NEW_PROCESS_GROUP, 0, 0, &si, &pi)){
			fprintf(stderr, "CreateProcess(%s) failed\n", cmdLine);
			free(cmdLine);
			return (-1);
		}
duph:		
		assert(pi.dwProcessId != 0);
		j = empty_proc();
		procs[j].h = pi.hProcess;
		procs[j].pid = pi.dwProcessId;
		safeCloseHandle(pi.hThread);
		free(cmdLine);
		return (pi.dwProcessId);
	}

	/*
	 * Pass fd 0,1,2 down to child via startInfo block.
	 * It is very important that this is done correctly,
	 * otherwise pipe() will fail to send EOF when it should.
	 */
#define	dupHandle(hProc, fd, hStd) \
	DuplicateHandle(hProc, (HANDLE) _get_osfhandle(fd),		\
			hProc, &(hStd), 0, TRUE, DUPLICATE_SAME_ACCESS)

	si.dwFlags = STARTF_USESTDHANDLES;
	dupHandle(hProc, 0, si.hStdInput);
	dupHandle(hProc, 1, si.hStdOutput);
	dupHandle(hProc, 2, si.hStdError);

	/*
	 * If no console, do not create one
	 * XXX - add CREATE_NEW_PROCESS_GROUP?
	 */
	cflags = hasConsole() ? 0 : DETACHED_PROCESS;
	if (!CreateProcess(NULL, cmdLine, 0, 0, 1, cflags, 0, 0, &si, &pi)) {
		err = GetLastError();
		fprintf(stderr,
		    "cannot create process: %s errno=%lu\n", cmdLine, err);
		free(cmdLine);
		return (-1);
	}

	safeCloseHandle(si.hStdInput);
	safeCloseHandle(si.hStdOutput);
	safeCloseHandle(si.hStdError);

	/* Note: _P_WAIT == 0, it's not a bitfield. */
	if (flag != _P_WAIT) goto duph;

	/*
	 * Wait for child to exit
	 */
	i = WaitForSingleObject(pi.hProcess, (DWORD) (-1L));
	if (i != WAIT_OBJECT_0 ) {
		fprintf(stderr, "WaitForSingleObject failed\n");
		status = 98;
		goto done;
	}
	if (GetExitCodeProcess(pi.hProcess, &status) == 0) {
		err = GetLastError();
		fprintf(stderr,
		    "GetExitCodeProcess failed: error = %lu\n", err);
		status = 99;
	}
	status &= 0x000000ff; /* paranoid */

done:
	safeCloseHandle(pi.hProcess);
	safeCloseHandle(pi.hThread);
	free(cmdLine);
	return (status);
}

int
nt_chmod(const char * file, int mode)
{
	int	attributes = FILE_ATTRIBUTE_NORMAL;

	if ((mode & _S_IWRITE) == 0) {
		attributes |= FILE_ATTRIBUTE_READONLY;
	}
	if (!SetFileAttributes(file, attributes)) {
		(void)GetLastError(); /* set errno */
		return (-1);
	}
	return (0);
}

char *
nt_getcwd(char *here, int len)
{
	if (GetCurrentDirectory(len, here) == 0)  return (NULL);
	nt2bmfname(here, here);

	/* Force drive letter to upper case */
	if (here[0] && (here[1] == ':')) here[0] = toupper(here[0]);
	return (here);
}

int
nt_chdir(char *dir)
{
	char	tmp[4];

	if ((dir[1] == ':') && (dir[2] == 0)) {
		tmp[0] = dir[0];
		tmp[1] = dir[1];
		tmp[2] = '\\';
		tmp[3] = 0;
		return (!SetCurrentDirectory(tmp));
	}
	return (!SetCurrentDirectory(dir));
}

private char *
error2msg(int error)
{
	LPTSTR  errormsg;
	char	*ret = 0;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	    NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	    (LPTSTR) &errormsg, 0, NULL);
	if (errormsg) {
		ret = strdup(errormsg);
		LocalFree(errormsg);
	}
	return (ret);
}

#define	stuck(e, times, format, args...)				\
	_stuck(e, times, 0, format, __FILE__, __LINE__, __FUNCTION__, ##args)

#define	bail(e, times, format, args...)					\
	_stuck(e, times, 1, format, __FILE__, __LINE__, __FUNCTION__, ##args)

private void
_stuck(DWORD e, int times, int bailed, char *format,
    char *file, int line, const char *func, ...)
{
	char	*m = 0, *fmt = 0;
	int	save;
	FILE	*f;
	va_list	ap;

	/* Avoid recursion if we can't log */
	save = win32_flags;
	win32_flags = 0;

	m = error2msg(e);
	fmt = aprintf("%s@%d(%2d): %s\n%4ld: %s\n",
	    func, line, times, format, e, m ? m : "Unknown");
	va_start(ap, func);
	if ((save & WIN32_NOISY) && (times > NOISY)) {
		vfprintf(stderr, fmt, ap);
	}
	if (bailed) {
		if (f = efopen("_BK_LOG_WINDOWS_ERRORS")) {
			vfprintf(f, fmt, ap);
			fclose(f);
		}
	}
	va_end(ap);
	if (fmt) free(fmt);
	if (m) free(m);
	win32_flags = save;
}

/*
 * Our version of open, does two additional things
 * a) Turn off inherite flag
 * b) Translate Bitmover filename to Win32 (i.e NT) path name
 */
int
nt_open(const char *filename, int flag, int pmode)
{
	int	fd, error = 0, i, acc, ret;
	DWORD	attrs;
	char	*p;

	flag |= _O_NOINHERIT;
	for (i = 1; i <= retries; i++) {
		fd = _open(filename, flag, pmode);
		if (fd >= 0) return (fd);
		error = GetLastError();
		unless (win32_flags & WIN32_RETRY) return (-1);
		if (streq(filename, DEV_TTY)) return (-1);
		if ((error == ERROR_ACCESS_DENIED) &&
		    ((attrs = GetFileAttributes(filename)) !=
			INVALID_FILE_ATTRIBUTES)) {
			/*
			 * Try to recognize some of the error cases
			 * without entering the wait loop below.
			 */
			if (((flag & (O_CREAT|O_EXCL)) == (O_CREAT|O_EXCL))) {
				/*
				 * special case where we don't retry, see
				 * utils.c:savefile()
				 */
				errno = EEXIST;
				return (-1);
			} else if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
				errno = EISDIR;
				return (-1);
			} else if ((attrs & FILE_ATTRIBUTE_READONLY) &&
			    (flag & (O_WRONLY|O_RDWR))) {
				/*
				 * We are trying to write a readonly
				 * file, the access denied error
				 * message is correct.
				 */
				errno = EACCES;
				return (-1);
			}
			
			/*
			 * OK, none of that worked, try the nt_access() code.
			 * This catches the case that the files are owner
			 * only perms.
			 */
			if (flag & O_CREAT) {
				acc = W_OK;
				p = dirname_alloc((char *)filename);
			} else {
				if (flag & (O_WRONLY|O_RDWR)) {
					acc = W_OK;
				} else {
					acc = R_OK;
				}
				p = strdup(filename);
			}
			ret = nt_access(p, acc);
			free(p);
			if (ret) return (-1);
		}
		if ((error == ERROR_ACCESS_DENIED) ||
		    (error == ERROR_SHARING_VIOLATION)) {
			Sleep(i * INC);
			stuck(error, i, "retrying on %s", filename);
		} else {
			/* don't retry for any other errors */
			return (-1);
		}
	}
	errno = EBUSY;
	bail(error, i, "bailing out on %s", filename);
	return (-1);
}

int
nt_rmdir(const char *dir)
{
	HANDLE	h;
	int	err, save, tries = 0, ret = -1;
	char	**files, *fmt;

again:
	/* Attempt to "lock" the dir by opening it */
	h = CreateFile(dir, GENERIC_READ, FILE_SHARE_DELETE,
	    0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
	/*
	 * If it doesn't exist, quit.
	 */
	if (h == INVALID_HANDLE_VALUE) {
		save = errno;
		err = GetLastError();	// set errno
		if (err == ERROR_FILE_NOT_FOUND ||
		    err == ERROR_PATH_NOT_FOUND) {
			return (-1);
		}
		errno = save; 
	}
	/*
	 * The lock attempt may have failed because Explorer
	 * has a handle.  Experiments show that RemoveDirectory
	 * will succeed in that case, so go ahead and try.
	 * If we still fail, then we need to pay attention and
	 * retry.
	 */
	unless (RemoveDirectory(dir)) {
		err = GetLastError();  // grab before calling safeCloseHandle()
		unless (h == INVALID_HANDLE_VALUE) {
			save = errno;
			safeCloseHandle(h);
			errno = save;
			h = INVALID_HANDLE_VALUE;
		}
		switch (err) {
		    case ERROR_SHARING_VIOLATION:
			errno = EBUSY;	// XXX: not EACCESS as err2errno sets?
			fmt = "retrying lock on %s";
			break;
		    case ERROR_DIR_NOT_EMPTY:
			/* See if it's really not empty */
			if (files = getdir((char *)dir)) {
				int	i;
				char	buf[MAXPATH];

				EACH(files) {
					concat_path(buf, (char *)dir, files[i]);
					if (GetFileAttributes(buf) !=
					    INVALID_FILE_ATTRIBUTES) {
						/* nope, not empty */
						freeLines(files, free);
						errno = ENOTEMPTY;
						goto err;
					}
				}
				freeLines(files, free);
			}
			/* We might be waiting for an antivirus. */
			fmt = "retrying on %s";
			break;
		    default:
			fprintf(stderr, "rmdir(%s): failed win32 err %d\n",
			    dir, err);
			// errno already set
			goto err;
		}
		unless (win32_flags & WIN32_RETRY) {
			goto err;
		}
		if (++tries <= retries) {
			Sleep(tries * INC);
			stuck(err, tries, fmt, dir);
			goto again;
		} else {
			bail(err, tries, "bailing out on %s", dir);
			goto err;
		}

	}
	ret = 0;
err:
	unless (h == INVALID_HANDLE_VALUE) {
		save = errno;
		safeCloseHandle(h);
		errno = save;
	}
	return (ret);
}

int
nt_unlink(const char *file)
{
	HANDLE	h;
	DWORD	attribs;
	int	i, error;
	char	*dir;

	if (!SetFileAttributes(file, FILE_ATTRIBUTE_NORMAL)) {
		errno = ENOENT;
		return (-1);
	}
	/*
	 * Wait until we can get exclusive access to the file to be
	 * deleted.  When we have this handle, we know no one else can
	 * read or write the file. Mark the file to be deleted on close.
	 */
	for (i = 1; i <= retries; i++) {
		h = CreateFile(file, GENERIC_READ, FILE_SHARE_DELETE, 0,
		    OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
		unless (h == INVALID_HANDLE_VALUE) {
			safeCloseHandle(h); /* real delete happens here */
			return (0);
		}
		error = GetLastError();

		attribs = GetFileAttributes(file);
		if (attribs == INVALID_FILE_ATTRIBUTES) {
			/*
			 * between CreateFile() and
			 * GetFileAttributes() it is possible that
			 * file has been removed by another process.
			 * Experimentally, we determined that
			 * situation ends up here, so grab the error
			 * (expecting ENOENT) and return
			 */
			GetLastError();
			return (-1);
		}
		if (attribs & FILE_ATTRIBUTE_DIRECTORY) {
			errno = EISDIR;
			return (-1);
		}

		unless (win32_flags & WIN32_RETRY) goto fail;


		dir = dirname_alloc((char*)file);
		if (nt_access(dir, W_OK) != 0) {
			free(dir);
			return (-1);
		}
		free(dir);

		/* On windows if you can't write it you can't delete */
		if (nt_access(file, W_OK) != 0) return (-1);

		Sleep(i * INC);
		stuck(error, i, "retrying lock on %s", file);
	}
	bail(GetLastError(), i, "bailing out on %s", file);
fail:	errno = EBUSY;
	return (-1);
}

private int
nt_mvdir(const char *oldf, const char *newf)
{
	HANDLE	h;
	int	i;
	char	*dir;

	for (i = 1; i <= retries; i++) {
		h = CreateFile(oldf, GENERIC_READ, 0, 0,
		    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
		unless (h == INVALID_HANDLE_VALUE) break;
		unless (win32_flags & WIN32_RETRY) {
fail:			errno = EBUSY;
			return (-1);
		}
		Sleep(i * INC);
		stuck(GetLastError(), i, "wait for source %s", oldf);
	}
	if (i > retries) {
		bail(GetLastError(), i, "bailing out on %s", oldf);
		goto fail;
	}

	safeCloseHandle(h);

	for (i = 1; i <= retries; i++) {
		if (MoveFileEx(oldf, newf, 0)) return (0);
		unless (win32_flags & WIN32_RETRY) goto fail;
		/*
		 * Make sure we can write the directory before looping,
		 * Oscar ported tcl's access so this should work.
		 */
		dir = dirname_alloc((char*)oldf);
		if (nt_access(dir, W_OK) != 0) {
			free(dir);
			return (-1);
		}
		free(dir);

		/*
		 * If the new file exists and is a dir, then test that,
		 * otherwise test the parent dir.
		 */
		if (isdir((char*)newf)) {
			dir = strdup(newf);
		} else {
			dir = dirname_alloc((char*)newf);
		}
		if (nt_access(dir, W_OK) != 0) {
			free(dir);
			return (-1);
		}
		free(dir);

		Sleep(i * INC);
		stuck(GetLastError(), i, "retrying %s -> %s", oldf, newf);
	}
	bail(GetLastError(), i, "bailing out on %s", oldf);
	goto fail;
	/* NOT REACHED */
}

int
nt_rename(const char *oldf, const char *newf)
{
	HANDLE	from, to;
	int	i, err = 0, rc = 0;
	DWORD	in, out, attribs;
	FILETIME ctime, atime, wtime;
	char	buf[BUFSIZ];

	if (streq(oldf, newf)) return (0);
	if (nt_unlink(newf) && (errno != ENOENT)) {
		fprintf(stderr, "rename: can't clear destination %s\n", newf);
		/* keep errno from unlink */
		return (-1);
	}
	if ((attribs = GetFileAttributes(oldf)) == INVALID_FILE_ATTRIBUTES) {
		errno = ENOENT;
		return (-1);
	}
	if (attribs & FILE_ATTRIBUTE_DIRECTORY) return (nt_mvdir(oldf, newf));

	for (i = 1; i <= retries; i++) {
		to = CreateFile(newf, GENERIC_WRITE,
		    0, 0, CREATE_ALWAYS, attribs, 0);
		unless (to == INVALID_HANDLE_VALUE) break;
		err = GetLastError();
		if (err == ERROR_PATH_NOT_FOUND) return (-1);
		// XXX permissions problems?
		unless (win32_flags & WIN32_RETRY) {
fail:			errno = EBUSY;
			return (-1);
		}
		Sleep(i * INC);
		stuck(err, i, "retry rename of %s", oldf);
	}
	if (i > retries) {
		bail(err, i, "bailing out on %s", newf);
		goto fail;
	}

	/* make sure we can delete the original file when we are done */
	SetFileAttributes(oldf, FILE_ATTRIBUTE_NORMAL);
	for (i = 1; i <= retries; i++) {
		from = CreateFile(oldf, GENERIC_READ, FILE_SHARE_DELETE,
		       0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
		unless (from == INVALID_HANDLE_VALUE) break;
		unless (win32_flags & WIN32_RETRY) goto fail;
		Sleep(i * INC);
		stuck(GetLastError(), i, "wait for source %s", oldf);
	}
	if (i > retries) {
		bail(GetLastError(), i, "bailing out on %s", oldf);
		goto fail;
	}

	while (ReadFile(from, buf, sizeof(buf), &in, 0) && (in > 0)) {
		WriteFile(to, buf, in, &out, 0);
		if (in != out) {
			fprintf(stderr, "rename: write error.\n");
			rc = -1;
			break;
		}
	}
	if (GetFileTime(from, &ctime, &atime, &wtime)) {
		SetFileTime(to, &ctime, &atime, &wtime);
	}
	safeCloseHandle(to);
	safeCloseHandle(from);	/* src deleted here */
	return (rc);
}

pid_t
nt_execvp(char *cmd, char **av)
{
	exit(_spawnvp_ex(_P_WAIT, cmd, av, 1));
}

int
kill(pid_t pid, int sig)
{
	HANDLE hProc;
	int	rc = 0;
	DWORD	bits = SYNCHRONIZE;

	if ((sig != 0) && (sig != SIGKILL)) {
		errno = EINVAL;
		return (-1);
	}
	if (sig == SIGKILL) bits |= PROCESS_TERMINATE;
	if ((hProc = OpenProcess(bits, 0, pid)) == (HANDLE) NULL) {
		errno = ESRCH;
		return (-1);
	}
	if (sig == SIGKILL) {
		unless (TerminateProcess(hProc, 255)) rc = -1;
	} else {
		if (WaitForSingleObject(hProc, 0) != WAIT_TIMEOUT) {
			errno = ESRCH;
			rc = -1;
		}
	}
	safeCloseHandle(hProc);
	return (rc);
}

int
wait(int *notused)
{
	debug((stderr, "wait() is a no-op on win32\n"));
	return (0);
}

private DWORD WINAPI
nt_timer(LPVOID seconds)
{
	Sleep((int) seconds * 1000);
	raise(SIGALRM); /* SIGALRM is really SIGABRT */
	return (0);
}

int
alarm(int seconds)
{
	HANDLE	timerThread;
	DWORD	threadId;

	assert(seconds != 0); /* we cannot support canceling a alarm yet */
	timerThread = CreateThread(NULL, 0, nt_timer, (LPVOID) seconds,
								0, &threadId);
	return (0);	
}
