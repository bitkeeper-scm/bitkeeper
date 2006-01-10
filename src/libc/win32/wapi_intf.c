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

#include "win32.h"

int
link(char *from, char *to)
{
	char	nt_from[NBUF_SIZE], nt_to[NBUF_SIZE];

	bm2ntfname(from, nt_from);
	bm2ntfname(to, nt_to);
	return (!CopyFile(from, to, 1));
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
		if ((p = getenv(*pp)) && (!access(p, 6)) ) {
			tmpdir = p;
			break;
		}
	}
	unless (tmpdir) {
		for (pp = paths; *pp; pp++) {
			if (access(*pp, 6) == 0) {
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

int
fsync (int fd)
{
	if (fd < 0) return (-1);
	if (FlushFileBuffers ((HANDLE) _get_osfhandle(fd)) == 0)
		return (-1);
	else	return (0);

}

/*
 * Load win sock library
 */
void
nt_loadWinSock(void)
{
	WORD	version;
	WSADATA	wsaData;
	int	iSockOpt = SO_SYNCHRONOUS_NONALERT;
	static	int	winsock_loaded = 0;

	if (winsock_loaded) return;

	version = MAKEWORD(2, 2);
	/* make sure we get version 2.2 or higher */
	if (WSAStartup(version, &wsaData)) {
		fprintf(stderr, "Failed to load WinSock\n");
		exit(1);
	}
	/* Enable the use of sockets as filehandles */
	setsockopt(INVALID_SOCKET, SOL_SOCKET, SO_OPENTYPE,
	    (char *)&iSockOpt, sizeof(iSockOpt));

	winsock_loaded = 1;
}

#define	OPEN_SOCK(x)	_open_osfhandle((x),_O_RDWR|_O_BINARY)
#define TO_SOCK(x)	_get_osfhandle(x)
#define SOCK_TEST(x, y)	if((x) == (y)) errno = WSAGetLastError()
#define SOCK_TEST_ERROR(x) SOCK_TEST(x, SOCKET_ERROR)

int
nt_socket(int af, int type, int protocol)
{
	SOCKET	s;

	nt_loadWinSock();
	if ((s = socket(af, type, protocol)) == INVALID_SOCKET) {
		errno = WSAGetLastError();
	} else {
		s = OPEN_SOCK(s);
	}
	return (s);
}

int
nt_accept(int s, struct sockaddr *addr, int *addrlen)
{
    SOCKET	r;

    SOCK_TEST((r = accept(TO_SOCK(s), addr, addrlen)), INVALID_SOCKET);
    return (OPEN_SOCK(r));
}

int
nt_bind(int s, const struct sockaddr *addr, int addrlen)
{
	int	r;

	SOCK_TEST_ERROR(r = bind(TO_SOCK(s), addr, addrlen));
	return (r);
}

int
nt_connect(int s, const struct sockaddr *addr, int addrlen)
{
	int	r;

	SOCK_TEST_ERROR(r = connect(TO_SOCK(s), addr, addrlen));
	return (r);
}

int
nt_getpeername(int s, struct sockaddr *addr, int *addrlen)
{
	int	r;

	SOCK_TEST_ERROR(r = getpeername(TO_SOCK(s), addr, addrlen));
	return (r);
}

int
nt_getsockname(int s, struct sockaddr *addr, int *addrlen)
{
	int	r;

	SOCK_TEST_ERROR(r = getsockname(TO_SOCK(s), addr, addrlen));
	return (r);
}

int
nt_setsockopt(int s, int level, int optname, const char *optval, int optlen)
{
	int	r;

	SOCK_TEST_ERROR(r = setsockopt(TO_SOCK(s), level, optname,
	    optval, optlen));
	return (r);
}

int
nt_send(int s, const char *buf, int len, int flags)
{
	int	r;

	SOCK_TEST_ERROR(r = send(TO_SOCK(s), buf, len, flags));
	return (r);
}

int
nt_recv(int s, char *buf, int len, int flags)
{
	int	r;

	SOCK_TEST_ERROR(r = recv(TO_SOCK(s), buf, len, flags));
	return (r);
}

int
nt_listen(int s, int backlog)
{
	int	r;

	SOCK_TEST_ERROR(r = listen(TO_SOCK(s), backlog));
	return (r);
}

int
nt_closesocket(int s)
{
	int	sock = TO_SOCK(s);

	close(s);
	return (closesocket(sock));
}

int
nt_shutdown(int s, int how)
{
	int	r;

	SOCK_TEST_ERROR(r = shutdown(TO_SOCK(s), how));
	return (r);
}

/*
 * Our own wrapper for winsock's select() function.  On Windows, select can
 * _only_ be used for sockets.  So we translate the filenames to socket
 * handles and then call the real select.
 *
 * XXX We don't do the reverse mapping so the sets are not modified when
 * select returns.  This can be added if it is needed.
 */

static void
fdset2sock(fd_set *fds, int **map)
{
	int	i;
	int	n, o;
	int	*p = *map;

	for (i = 0; i < fds->fd_count; i++) {
		o = fds->fd_array[i];
		n = TO_SOCK(o);
		*p++ = n;
		*p++ = o;
		fds->fd_array[i] = n;
	}
	*map = p;
}

static void
sockset2fd(fd_set *fds, int *map)
{
	int	i, j;

	for (i = 0; i < fds->fd_count; i++) {
		for (j = 0; map[j]; j += 2) {
			if (map[j] == fds->fd_array[i]) {
				fds->fd_array[i] = map[j+1];
				break;
			}
		}
		assert(map[j]);
	}
}

int
nt_select(int n, fd_set *rfds, fd_set *wfds, fd_set *efds, struct timeval *t)
{
	int	map[2*64];
	int	*mapp = map;
	int	rc;

	if (rfds) fdset2sock(rfds, &mapp);
	if (wfds) fdset2sock(wfds, &mapp);
	if (efds) fdset2sock(efds, &mapp);

	assert((mapp - map) < sizeof(map)/sizeof(int));
	*mapp = 0;

	rc = select(n, rfds, wfds, efds, t);
	
	if (rfds) sockset2fd(rfds, map);
	if (wfds) sockset2fd(wfds, map);
	if (efds) sockset2fd(efds, map);
	
	return (rc);
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
	bm2ntfname(dirname, tmp_buf);
	strcat(tmp_buf, "\\*.*");
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
 * Maximum number of open mmap supported
 * If you change this, you must also ajust
 * the mi table below.(add initializer)
 */
#define	MAX_NUM_OF_MMAP 12

#define	BAD_ADDR ((caddr_t) -1)
static struct mmap_info 	/* mmap information table */
	{
		caddr_t addr;
		int fd;
		unsigned long size;
	} mi[MAX_NUM_OF_MMAP] = {
		{BAD_ADDR, -1}, {BAD_ADDR, -1}, {BAD_ADDR, -1}, {BAD_ADDR, -1},
		{BAD_ADDR, -1}, {BAD_ADDR, -1}, {BAD_ADDR, -1}, {BAD_ADDR, -1},
		{BAD_ADDR, -1}, {BAD_ADDR, -1}, {BAD_ADDR, -1}, {BAD_ADDR, -1}
	};
caddr_t
mmap(caddr_t addr, size_t len, int prot, int flags, int fd, off_t off)
{
	HANDLE	fh, mh;
	LPVOID	p;
	DWORD	flProtect, dwDesiredAccess;
	int	i;
	char	mmap_name[100];

	debug((stderr, "mmap: fd = %d\n", fd));

	/*
	 * Bitkeeper sometime call mmap() with fd == -1
	 */
	if (fd < 0) return BAD_ADDR;

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
		for (i = 0; i < MAX_NUM_OF_MMAP; i++)
			if (mi[i].addr == BAD_ADDR) break;
		if (i >= MAX_NUM_OF_MMAP) {
			debug((stderr, "mmap table is full !!\n"));
			exit(1);
		}
	}

	if (fd < 0) return BAD_ADDR;

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
		fprintf(stderr, "mmap: can not CreateFileMapping file, "
				"error code =%lu, writeMode = %x, fd =%d\n",
				GetLastError(), prot & PROT_WRITE, fd);
		return (BAD_ADDR);
	}

	/*
	 * Mmap interface  support 32 bit offset
	 * MapViewOfFileEx support 64 bit offset
	 * We just set the high order 32 bit to zero for now
	 */
	p = MapViewOfFileEx(mh, dwDesiredAccess, 0,  off, len, addr);
	if (p == NULL) {
		fprintf(stderr,
		    "mmap: can not MapViewOfFile , error code =%lu\n",
		    GetLastError());
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
		debug((stderr,
		    "munmap: can not UnmapViewOfFile file, error code =%d\n",
		    GetLastError()));
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
		errno = GetLastError();
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
		if (_access(fullCmdPath, 0) != 0) fullCmdPath[0] = 0;
		bm2ntfname(fullCmdPath, fullCmdPath);
		return fullCmdPath;
	}

	p = getenv("PATH");
	if (!p) return NULL;
	path_delim = strchr(p, ';') ? ";" : ":";
	strncpy(path, p, sizeof (path));
	p = path;
	t = strtok(p, path_delim);
	while (t) {
		sprintf(fullCmdPath, "%s\\%s", t, cmdbuf);
		bm2ntfname(fullCmdPath, fullCmdPath);
		if (_access(fullCmdPath, 0) == 0) return (fullCmdPath);
		t = strtok(NULL, path_delim);
	}

	/*
	 * Try the dot path, we need this for shell script
	 */
	strcpy(fullCmdPath, cmdbuf);
	bm2ntfname(fullCmdPath, fullCmdPath);
	if (_access(fullCmdPath, 0) == 0) return (fullCmdPath);

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

static	OSVERSIONINFO	osinfo;

static	void
get_osinfo(void)
{
	if (!osinfo.dwOSVersionInfoSize) {
		osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		if (GetVersionEx(&osinfo) == 0) {
			fprintf(stderr, "Warning: cannot get os version\n");
			osinfo.dwOSVersionInfoSize = 0;
		}
	}
}

/*
 * Return false if OS is Win98 (or older) (Including WinNT)
 */
int
win_supported(void)
{
	get_osinfo();
	return (osinfo.dwMajorVersion > 4);
}

/*
 * Return true if OS is Windows 2000 (only)
 */
int
isWin2000(void)
{
	get_osinfo();
	return ((osinfo.dwMajorVersion == 5) && (osinfo.dwMinorVersion == 0));
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
	if (ret == -1) errno = EINVAL;
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
	DWORD	status = 0;
	int	cflags, i, j;
	int 	len, cmd_len;
	int	do_quote = 1, needEndQuote = 0;

	/*
	 * Compute the size of the av[] vector
	 * and allocate a sufficiently big comdLine buffer
	 */
	for (i = 0, cmd_len = 0; av[i]; i++) cmd_len += strlen(av[i]) + 1;
	cmd_len += 1024; /* for quotes and/or shell path */
	cmdLine = (char *) malloc(cmd_len);
	assert(cmdLine);
	i = 0; /* important */

#define	dupHandle(hProc, fd, hStd) \
	DuplicateHandle(hProc, (HANDLE) _get_osfhandle(fd), \
			hProc, &(hStd), 0, TRUE, DUPLICATE_SAME_ACCESS)

	expnPath((char *) cmdname, fullCmdPath);
	if (fullCmdPath[0] == 0) {
		fprintf(stderr, "%s: can not expand path: %s\n",
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
		sprintf(cmdLine, "%s /c %s",
		    (_osver & 0x8000) ? "command.com" : "cmd.exe", fullCmdPath);
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
		strcat(cmdLine, script);
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
					"spawnvp_ex: command line too long\n");
				free(cmdLine);
				return (-1);
			}
		}
		if (do_quote) *p++ = '\"'; /* end quote " */
		i++;
	}
	if (needEndQuote) *p++ = '\''; /* for unix shell script */
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
	 *
	 * XXX We have to transfer the handles with different method because
	 * Win98 does not seems to hornor the "no inherit" flag if it is set
	 * after the handle is created. It works if you create the handle with
	 * with the "no inherit" flag set, however.
	 */
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
		fprintf(stderr,
		    "can not create process: %s errno=%lu\n",
		    cmdLine, GetLastError());
		free(cmdLine);
		return (-1);
	}

	safeCloseHandle(si.hStdInput);
	safeCloseHandle(si.hStdOutput);
	safeCloseHandle(si.hStdError);

	if (flag != P_WAIT) goto duph;

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
		fprintf(stderr,
		    "GetExitCodeProcess failed: error = %lu\n", GetLastError());
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
	char	ntfname[1024];
	int	attributes = FILE_ATTRIBUTE_NORMAL;

	bm2ntfname(file, ntfname);
	if ((mode & _S_IWRITE) == 0) {
		attributes |= FILE_ATTRIBUTE_READONLY;
	}
	if (!SetFileAttributes(ntfname, attributes)) {
		errno = ENOENT;
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

#define	INC	100
#define	NOISY	5	/* start telling them after INC * NOISY ms */
#define	WAITMAX	600	/* /10 to get seconds */

private int
waited(int i)
{
	int	total = 0;

	while (i) total += i--;
	return (total);
}

private void
stuck(char *fmt, const char *arg)
{
	fprintf(stderr, fmt, arg);
}

int
nt_rmdir(char *dir)
{
	HANDLE	h;
	int	err, i = 0;

again:
	h = CreateFile(dir, GENERIC_READ, FILE_SHARE_DELETE,
		       0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
	if (h == INVALID_HANDLE_VALUE) {
		switch (err = GetLastError()) {
		    case ERROR_FILE_NOT_FOUND:
		    case ERROR_PATH_NOT_FOUND:
			errno = ENOTDIR;
			return (-1);
		    default:
			fprintf(stderr,
				"rmdir(%s): unexpected win32 error %d\n",
				dir, err);
			/* FALLTHROUGH */
		    case ERROR_SHARING_VIOLATION:
			Sleep(++i * INC);
			if (i > NOISY) {
				stuck("rmdir: retrying lock on %s\n", dir);
			}
			if (waited(i) > WAITMAX) {
				stuck("bailing out on %s\n\n", dir);
				errno = EBUSY;
				return (-1);
			}
			goto again;
		}
	}
	unless (RemoveDirectory(dir)) {
		safeCloseHandle(h);
		switch (err = GetLastError()) {
		    case ERROR_DIR_NOT_EMPTY:
			errno = ENOTEMPTY;
			break;
		    default:
			fprintf(stderr, "rmdir(%s): failed win32 err %ld\n",
				dir, GetLastError());
			errno = EINVAL;
			break;
		}
		return (-1);
	}
	safeCloseHandle(h);
	return (0);
}

int
nt_unlink(const char *file)
{
	HANDLE	h;
	int	i = 0, rc = 0;

	if (!SetFileAttributes(file, FILE_ATTRIBUTE_NORMAL)) {
		errno = ENOENT;
		return (-1);
	}
	/*
	 * Wait until we can get exclusive access to the file to be
	 * deleted.  When we have this handle, we know no one else can
	 * read or write the file. Mark the file to be deleted on close.
	 */
	for ( ;; ) {
		h = CreateFile(file, GENERIC_READ, FILE_SHARE_DELETE, 0,
		    OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
		unless (h == INVALID_HANDLE_VALUE) break;
		Sleep(++i * INC);
		if (i > NOISY) {
			stuck("unlink: retrying lock on %s\n", file);
		}
		if (waited(i) > WAITMAX) {
			stuck("bailing out on %s\n", file);
			errno = EBUSY;
			return (-1);
		}
	}
	safeCloseHandle(h); /* real delete happens here */
	return (rc);
}

private int
nt_mvdir(const char *oldf, const char *newf)
{
	HANDLE	h;
	int	i;

	for (i = 0;; ) {
		h = CreateFile(oldf, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
		unless (h == INVALID_HANDLE_VALUE) break;
		Sleep(++i * INC);
		if (i > NOISY) {
			stuck("rename: wait for source %s\n", oldf);
		}
		if (waited(i) > WAITMAX) {
			stuck("bailing out on %s\n", oldf);
			errno = EBUSY;
			return (-1);
		}
	}
	safeCloseHandle(h);
	for (i = 0; ; ) {
		if (MoveFileEx(oldf, newf, 0)) return (0);
		Sleep(++i * INC);
		if (i > NOISY) {
			char	buf[2000];

			sprintf(buf, "%s -> %s", oldf, newf);
			stuck("mvdir: retrying %s\n", (const char*)buf);
		}
		if (waited(i) > WAITMAX) {
			stuck("bailing out on %s\n", oldf);
			errno = EBUSY;
			return (-1);
		}
	}
	return (-1);
}

int
nt_rename(const char *oldf, const char *newf)
{
	HANDLE	from, to;
	int	i, err, rc = 0;
	DWORD	in, out, attribs;
	char	buf[BUFSIZ];

	if (nt_unlink(newf) && (errno != ENOENT)) {
		fprintf(stderr, "rename: can't clear destination %s\n", newf);
		/* keep errno from unlink */
		return (-1);
	}
	if ((attribs = GetFileAttributes(oldf)) == INVALID_FILE_ATTRIBUTES) {
		fprintf(stderr, "rename: no source %s\n", oldf);
		errno = ENOENT;
		return (-1);
	}
	if (attribs & FILE_ATTRIBUTE_DIRECTORY) return (nt_mvdir(oldf, newf));

	for (i = 0;; ) {
		to = CreateFile(newf, GENERIC_WRITE,
		    0, 0, CREATE_ALWAYS, attribs, 0);
		unless (to == INVALID_HANDLE_VALUE) break;
		switch (err = GetLastError()) {
		    case ERROR_PATH_NOT_FOUND:
			errno = ENOENT;
			return (-1);
		   default:
			break;
		}
		// XXX permissions problems?
		Sleep(++i * INC);
		if (i > NOISY) {
			stuck("rename: wait for dest %s\n", newf);
		}
		if (waited(i) > WAITMAX) {
			stuck("bailing out on %s\n", newf);
			errno = EBUSY;
			return (-1);
		}
	}
	/* make sure we can delete the original file when we are done */
	SetFileAttributes(oldf, FILE_ATTRIBUTE_NORMAL);
	for (i = 0;; ) {
		from = CreateFile(oldf, GENERIC_READ, FILE_SHARE_DELETE,
		       0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
		unless (from == INVALID_HANDLE_VALUE) break;
		Sleep(++i * INC);
		if (i > NOISY) {
			stuck("rename: wait for source %s\n", oldf);
		}
		if (waited(i) > WAITMAX) {
			stuck("bailing out on %s\n", oldf);
			errno = EBUSY;
			return (-1);
		}
	}
	while (ReadFile(from, buf, sizeof(buf), &in, 0) && (in > 0)) {
		WriteFile(to, buf, in, &out, 0);
		if (in != out) {
			fprintf(stderr, "rename: write error.\n");
			rc = -1;
			break;
		}
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

	if (sig != 0) {
		debug((stderr, "only signal 0 is supported on NT\n"));
		errno = EINVAL;
		return (-1);
	}
	if ((hProc = OpenProcess(SYNCHRONIZE, 0, pid)) == (HANDLE) NULL) {
		errno = ESRCH;
		return (-1);
	}
	safeCloseHandle(hProc);
	return (0);
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

int
setShutDownPrivilege(void) 
{
	TOKEN_PRIVILEGES tp;
	LUID luid;
	HANDLE hProc = NULL, hToken = NULL; 

	hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, getpid());
	OpenProcessToken(hProc, TOKEN_ADJUST_PRIVILEGES, &hToken);
	if (hProc) safeCloseHandle(hProc);

	if ( !LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &luid ) ) {
    		fprintf(stderr,
		    "LookupPrivilegeValue error: %lu\n", GetLastError() ); 
err:		if (hToken) safeCloseHandle(hToken);
    		return (FALSE); 
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	AdjustTokenPrivileges(
		hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), 
       		(PTOKEN_PRIVILEGES) NULL, (PDWORD) NULL); 
	if (GetLastError() != ERROR_SUCCESS) { 
      		fprintf(stderr,
		    "AdjustTokenPrivileges failed: %lu\n", GetLastError() ); 
		goto err;
	} 

	if (hToken) safeCloseHandle(hToken);
	return (TRUE);
}


int
do_reboot(char *msg)
{
	UINT	flags;

	flags = MB_YESNO|MB_ICONQUESTION|MB_TOPMOST;
	if (MessageBox(NULL, msg, "BitKeeper Extractor", flags) == IDYES) {
		setShutDownPrivilege();
		FreeConsole();
		if (ExitWindowsEx(EWX_REBOOT, 0) == 0) {
			fprintf(stderr,
			    "ExitWindowsEx return error %lu\n",
			    GetLastError());
		}
	}
	return (1);
}

/*
 * Compile the subs used by NewAPIs.h header file.
 */

#define	WANT_GETLONGPATHNAME_WRAPPER
#define	WANT_GETFILEATTRIBUTESEX_WRAPPER
#define	COMPILE_NEWAPIS_STUBS

#include "NewAPIs.h"
