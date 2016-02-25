/*
 * Copyright 2002,2008,2011,2016 BitMover, Inc
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

#include "system.h"

#define	LOGFILE	"C:\\temp\\bkshellx.log"
char	*bkdir;
char	*bkexe;
hash	*icons;
int	shellx;

void
BkExec(char *cmd, char *dir, int flags)
{
	char	*command;

	if (bkdir) {
		command = aprintf("/D /S /%c \"%s\\%s\"",
		    (flags & SW_NORMAL) ? 'K' : 'C', bkdir, cmd);
	} else {
		command = aprintf("/D /S /%c \"%s\"",
		    (flags & SW_NORMAL) ? 'K' : 'C', cmd);
	}
	TRACE("(%s) cmd.exe %s", dir, command);
	_putenv("BK_GUI=YES");
	ShellExecute(NULL, NULL, "cmd.exe", command, dir, flags);
	_putenv("BK_GUI=");
	free(command);
}

HICON
GetIcon(char *name)
{
	HICON hIcon = NULL;
	HICON *phi;
	char buf[MAXPATH];

	unless (icons) icons = hash_new(HASH_MEMHASH);
	if (phi = hash_fetch(icons, name, (int)strlen(name)+1)) return (*phi);
	sprintf(buf, "%s\\icons\\%s.ico", bkdir, name);
	if (hIcon = (HICON)LoadImage(NULL, buf, IMAGE_ICON,
	    16, 16, LR_DEFAULTCOLOR | LR_LOADFROMFILE)) {
		hash_store(icons, name, (int)strlen(name)+1,
		    &hIcon, sizeof(HICON));
	}
	return (hIcon);
}

char *
dirname(char *path)
{
	char *p;
	char *d = strdup(path);

	p = strrchr(d, '/');
	unless (p) p = strrchr(d, '\\');
	if (p) *p = 0;
	return (d);
}

char *
basename(char *path)
{
	char	*p;
	char	*d = strdup(path);

	p = strrchr(d, '/');
	unless (p) p = strrchr(d, '\\');
	if (p) *p = 0;
	p++;
	p = strdup(p);
	free(d);
	return (p);
}

char *
getRepoParent(char *pathName)
{
	char *rootdir;
	FILE	*f;
	char tmp[MAXPATH];

	unless (rootdir = rootDirectory(pathName)) return (0);

	sprintf(tmp, "%s/%s", rootdir, PARENT);
	nt2bmfname(tmp, tmp);
	unless (f = fopen(tmp, "rb")) return (0);
	fgets(tmp, sizeof(tmp), f);
	fclose(f);
	chomp(tmp);
	return (strdup(tmp));
}

int
isBkRoot(const char *pathName)
{
	char tmp[MAXPATH];
	sprintf(tmp, "%s/%s",pathName, BKROOT);
	return (isdir(tmp));
}

/*
 * This function works like sprintf(), except it return a
 * malloc'ed buffer which caller should free when done
 */
char *
aprintf(char *fmt, ...)
{
	va_list ptr;
	int     rc, size = 512;
	char    *buf = (char *)malloc(size);

	va_start(ptr, fmt);
	rc = _vsnprintf_s(buf, size, size - 1, fmt, ptr);
	va_end(ptr);
	while ((rc == -1) || (rc >= size)) {
		if (rc == -1) size *= 2;
		if (rc >= size) size = rc + 1;
		free(buf);
		buf = (char *) calloc(size, 1);
		assert(buf);
		va_start(ptr, fmt);
		rc = _vsnprintf_s(buf, size, size - 1, fmt, ptr);
		va_end(ptr);
	}
	return (buf);
}

HANDLE
CreateChildProcess(char *cmd, const char *args, HANDLE hPipe)
{
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL    rc;
	char    *cmdline;

	cmdline = aprintf("\"%s\" %s", cmd, args);

	/*
	 *  Set up members of STARTUPINFO structure.
	 */
	bzero(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	siStartInfo.cb  = SW_HIDE;
	siStartInfo.hStdOutput  = hPipe;
	siStartInfo.hStdError    = hPipe;

	/*
	 * Create the child process.
	 */
	rc = CreateProcess(cmd,
	    cmdline,
	    0,	    // process security attributes
	    0,	    // primary thread security attributes
	    1,	    // handles are inherited
	    0,
	    0,	    // use parent's environment
	    0,	    // use parent's current directory
	    &siStartInfo,   // STARTUPINFO pointer
	    &piProcInfo);   // receives PROCESS_INFORMATION
	free(cmdline);
	if (rc) return (piProcInfo.hProcess);
	return ((void *) -1);
}

HANDLE
MakeReadPipe(char *cmd, const char *args,  HANDLE *hProc)
{
	SECURITY_ATTRIBUTES saAttr;
	HANDLE hPipeRd, hPipeWr;

	/*
	 *  Set the bInheritHandle flag so pipe handles are not inherited.
	 */
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = FALSE;
	saAttr.lpSecurityDescriptor = NULL;

	/*
	 *  Create a pipe for the child process's STDOUT.
	 */
	if (!CreatePipe(&hPipeRd, &hPipeWr, &saAttr, 0)) return ((void *) -1);

	SetHandleInformation(hPipeWr, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

	/*
	 * Now create the child process.
	 */
	*hProc = CreateChildProcess(cmd, args, hPipeWr);
	if (*hProc == ((void *) -1)) {
		CloseHandle(hPipeWr);
		CloseHandle(hPipeRd);
		return ((void *) -1);
	}

	if (!CloseHandle(hPipeWr)) {
		CloseHandle(*hProc);
		CloseHandle(hPipeRd);
		return ((void *) -1);
	}
	return (hPipeRd);
}

unsigned long
ClosePipe(HANDLE hPipe, HANDLE hProc)
{
	unsigned long status;

	WaitForSingleObject(hProc, (DWORD) (-1L));
	GetExitCodeProcess(hProc, &status);
	CloseHandle(hPipe);
	CloseHandle(hProc);
	return (status);
}

int
validDrive(char *path)
{
	char	drv[4];
	int	type;

	strncpy(drv, path, 3);
	drv[3] = 0;
	type = GetDriveType(drv);
	if (GetDriveType(drv) == DRIVE_REMOTE) {
		return (shellx & NETWORK_ENABLED);
	} else {
		return (shellx & LOCAL_ENABLED);
	}
}

/*
 * Scan ofn, replace all ochar to nchar, result is in nfn
 * caller is responsible to ensure nfn is at least as big as ofn.
 */
char *
switch_char(const char *ofn, char *nfn, char ochar, char nchar)
{
	const	char *p;
	char	*q = nfn;

	unless (ofn) return (NULL);
	p = &ofn[-1];

	/*
	 * Simply replace all ochar with nchar
	 */
	while (*(++p)) *q++ = (*p == ochar) ? nchar : *p;
	*q = '\0';
	return (nfn);
}

/*
 * Remove any trailing newline or CR from a string.
 */
void
chomp(char *s)
{
	while (*s) ++s;
	while ((s[-1] == '\n') || (s[-1] == '\r')) --s;
	*s = 0;
}

int
patheq(char *a, char *b)
{
	char buf1[MAXPATH], buf2[MAXPATH];

	nt2bmfname(a, buf1);
	nt2bmfname(b, buf2);
	return (strnieq(buf1, buf2, MAXPATH));
}

int
pathneq(char *a, char *b, size_t len)
{
	char buf1[MAXPATH], buf2[MAXPATH];

	nt2bmfname(a, buf1);
	nt2bmfname(b, buf2);
	return (strnieq(buf1, buf2, len));
}

int
exists(char *s)
{
	return (GetFileAttributes(s) != 0xffffffff);
}

int
isdir(char *s)
{
	DWORD   rc;

	rc = GetFileAttributes(s);
	if (rc == 0xffffffff) return (0);
	if (FILE_ATTRIBUTE_DIRECTORY & rc) return (1);
	return (0);
}

int
IsFullPath(char *path)
{
	return ((path[0] == '/') || (path[0] == '\\')
	    || isDriveColonPath(path));
}

/*
 * clean up "..", "." and "//" in a path name
 */
void
cleanPath(char *path, char cleanPath[])
{
	char    buf[MAXPATH], *p, *r, *top;
	int     dotCnt = 0;     /* number of "/.." */
#define isEmpty(buf, r)	(r ==  &buf[sizeof (buf) - 2])

	r = &buf[sizeof (buf) - 1]; *r-- = 0;
	p = &path[strlen(path) - 1];

	/* for win32 path */
	top = isDriveColonPath(path) ? &path[2] : path;

	/* trim trailing slash(s) */
	while ((p >= top) && (*p == '/')) p--;

	while (p >= top) {      /* scan backward */
		if ((p == top) && (p[0] == '.')) {
			p = &p[-1];		/* process "." in the front */
			break;
		} else if ((p == &top[1]) && (p[-1] == '.') && (p[0] == '.')) {
			dotCnt++; p = &p[-2];   /* process ".." in the front */
			break;
		} else if ((p >= &top[2]) && (p[-2] == '/') &&
		    (p[-1] == '.') && (p[0] == '.')) {
			dotCnt++; p = &p[-3];   /* process "/.." */
		} else if ((p >= &top[1]) && (p[-1] == '/') &&
		    (p[0] == '.')) {
			p = &p[-2];		/* process "/." */
		} else {
			if (dotCnt) {
				/* skip dir impacted by ".." */
				while ((p >= top) && (*p != '/')) p--;
				dotCnt--;
			} else {
				/* copy regular directory */
				unless (isEmpty(buf, r)) *r-- = '/';
				while ((p >= top) && (*p != '/')) *r-- = *p--;
			}
		}
		/* skip "/", "//" etc.. */
		while ((p >= top) && (*p == '/')) p--;
	}

	if (isEmpty(buf, r) || (top[0] != '/')) {
		/* put back any ".." with no known parent directory  */
		while (dotCnt--) {
			if (!isEmpty(buf, r) && (r[1] != '/')) *r-- = '/';
			*r-- = '.'; *r-- = '.';
		}
	}

	if (top[0] == '/') *r-- = '/';
	if (top != path) { *r-- = path[1]; *r-- = path[0]; }
	if (*++r) {
		strcpy(cleanPath, r);
	} else {
		strcpy(cleanPath, ".");
	}
	/* for win32 path */
	if ((r[1] == ':') && (r[2] == '\0')) strcat(cleanPath, "/");
#undef  isEmpty
}

void
concat_path(char *buf, char *first, char *second)
{
	size_t len;
	if (buf != first) strcpy(buf, first);
	len = strlen(buf);
	if ((len >= 2) &&
	    (buf[len -2] == '/') && (buf[len -1] == '.') && second[0]) {
		buf[len - 1] = 0; len--;
	}
	/*
	 * if "first" and "second" already have a seperator between them,
	 * don't add another one.
	 * Another special case is also checked here:
	 *      first or "second" is a null string.
	 */
	if ((buf[0] != '\0') && (second[0] != '\0') &&
	    (buf[len -1] != '/') && (second[0] != '/'))
		strcat(buf, "/");
	strcat(buf, second);
}

char *
bk_getcwd(char *here, int len)
{
	if (GetCurrentDirectory(len, here) == 0)  return (NULL);
	nt2bmfname(here, here);
	return (here);
}

/*
 * Find project root
 * Note: "path" most be a full path name, and must be "cleaned"
 * i.e No duplicate slash.
 */
char    *
rootDirectory(char *dir)
{
	char    *start, *end;
	char	path[MAXPATH];

	assert(IsFullPath(dir)); /* we only deal in full paths */
	strcpy(path, dir);
	start = (isDriveColonPath(path)) ? &path[2]: path; /* for win32 path */
	assert(*start == '\\');
	end = &path[strlen(path)];
	*end = '\\';
	/*
	 * Now work backwards up the tree until we find a root marker
	 */
	while (end >= start) {
		strcpy(++end, BKROOT);
		if (exists(path))  break;
		if (--end <= start) {
			/*
			 * if we get here, we hit the start
			 * and did not find the root marker
			 */
			return (0);
		}
		/* end -> \ in ...\foo\SCCS\s.foo.c */
		for (--end; (*end != '\\') && (end > start); end--);
	}
	assert(end >= start);
	end--;

	*end = 0; /* chop off the non-root part */
	return (strdup(path));
}

int
isDrive(char *path)
{
	size_t	len;

	unless (isDriveColonPath(path)) return (0);
	len = strlen(path);
	if (len == 2) return (1);
	if ((len == 3) && ((path[2] == '/') || (path[2] == '\\'))) return (1);
	return (0);
}


void
trace_msg(char *file, int line, const char *function, char *format, ...)
{
	char	*fmt;
	FILE	*f;
	va_list	ap;

	unless (exists(LOGFILE)) return;
	f = fopen(LOGFILE, "a");
	file = strrchr(file, '\\') + 1;
	if (format) {
		fmt = aprintf("shellx %u:%u [%s:%s:%d] '%s'\n",
		    GetCurrentProcessId(), GetCurrentThreadId(),
		    file, function, line, format);
		va_start(ap, format);
		vfprintf(f, fmt, ap);
		free(fmt);
		va_end(ap);
	} else {
		fprintf(f, "shellx %u:%u [%s:%s:%d]\n",
		    GetCurrentProcessId(), GetCurrentThreadId(),
		    file, function, line);
	}
	fclose(f);
}

char *
dosify(char *buf)
{
	size_t	len = strlen(buf);
	char	*inp, *output, *outp;
	int	linecount = 0;

	for (inp = buf; *inp; inp++) {
		if (*inp == '\n') linecount++;
	}
		
	outp = output = malloc(len + (2 * linecount));

	for (inp = buf; *inp; inp++, outp++) {
		if (*inp == '\n') *outp++ = '\r';
		*outp = *inp;
	}
	*outp = *inp;

	return (output);
}
