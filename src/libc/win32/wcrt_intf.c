/* %K% Copyright (c) 1999 Andrew Chang */
/* This file interface with the win32 C run time library */
/*
 * In general, we view the application running in a layered environment
 * (see diagram below).
 * In the BitMover's Application Library Layer, we
 * use only "BitMover's path name format": i.e All path name have forward slash
 * BitMover's path name is like Unix path, but "drive-letter colon"
 * is also allowed. (e.g. C:/temp)
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
 * | In this layer, we always return path name to upper layers
 * | in BitMover format. We always transltas pathname to NT format
 * | when we call down to the lower layer)
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

/*
 * Scan ofn, replace all ochar to nchar, result is in nfn
 * caller is responsible to ensure nfn is at least as big as ofn.
 */
char *
_switch_char(const char *ofn, char *nfn, char ochar, char nchar)
{
	const	char *p;
	char	*q;

	if (!ofn) return (0);
	
	/*
	 * Simply replace all ochar with nchar
	 */
	if (ofn == nfn) {
		/* inplace edit, don't write if we don't have to */
		for (q = nfn; *q; q++) if (*q == ochar) *q = nchar;
	} else {
		p = ofn;
		q = nfn;
		while (1) {
			*q++ = (*p == ochar) ? nchar : *p;
			if (!*p) break;
			++p;
		}
	}
	return (nfn);
}

/*
 * Our version of fopen, does two additional things
 * a) Translate Bitmover filename to Win32 (i.e NT) path name
 * b) Make fd uninheritable
 */
FILE *
nt_fopen(const char *filename, const char *mode)
{
	FILE	*f;
	char	buf[1024];
	int	fd;

	f = fopen(bm2ntfname(filename, buf), mode);
	if (f == NULL) {
		debug((stderr,
		    "nt_fopen: fail to open file %s, mode %s\n",
		    filename, mode));
		return (NULL);
	}

	fd = _fileno(f);
	if (fd >= 0) make_fd_uninheritable(fd);
	return (f);
}


private int
_exists(char *file)
{
	return (GetFileAttributes(file) != INVALID_FILE_ATTRIBUTES);
}


/*
 * Our version of open, does two additional things
 * a) Turn off inherite flag
 * b) Translate Bitmover filename to Win32 (i.e NT) path name
 */
int
nt_open(const char *filename, int flag, int pmode)
{
	char	buf[1024];
	int	fd;

	flag |= _O_NOINHERIT;
	fd = _open(bm2ntfname(filename, buf), flag, pmode);
	if (fd < 0) {
		switch (GetLastError()) {
		  case ERROR_SHARING_VIOLATION: errno = EAGAIN; break;
		  case ERROR_ACCESS_DENIED:
			if ((flag & O_CREAT) &&
			    		(flag & O_EXCL) && _exists(buf)) {
				errno = EEXIST;
			} else {
				errno = EACCES;
			};
			break;
		  default: break; /* use errno set by _open() */
		}
	}
	return (fd);
}


int
nt_dup(int fd)
{
	int new_fd;

	new_fd = (dup)(fd);
	if (new_fd >= 0) make_fd_uninheritable(new_fd);
	return (new_fd);
}

int
nt_dup2(int fd1, int fd2)
{
	int rc;

	rc = (dup2)(fd1, fd2);
	if (rc == -1) return (-1);
	make_fd_uninheritable(fd2);
	return (fd2);
}


int
nt_access(const char *file, int mode)
{
	char	ntfname[1024];
	int	rc;

	/*
	 * Emulate the posix semantics for access(2).
	 *
	 * XXX This doesn't really handle X_OK correctly.  As far as I
	 * can tell, X_OK is the same as F_OK, so it just checks if
	 * the file is there.  We really need to know if our spawnvp()
	 * function would work.  To do that the file would either
	 * need to end in .exe or contain a '#!' at the start and the
	 * path to an executable file.
	 *
	 * A previous hint said to look at GetBinaryType(), but that
	 * is really not enough.
	 */
	bm2ntfname(file, ntfname);
	rc = _access(ntfname, mode);
	if (rc && ((mode & X_OK) == X_OK)) {
		strcat(ntfname, ".exe");
		rc = _access(ntfname, mode);
	}
	return (rc);
}

int
nt_mkdir (char *dirname)
{
	char	ntfname[1024];
	char	*p;
	int	rc;

	bm2ntfname(dirname, ntfname);
	if (rc = _mkdir(ntfname)) return (rc);

	/* make SCCS dirs hidden */
	if (p = strrchr(ntfname, '\\')) {
		++p;
	} else {
		p = ntfname;
	}
	if (strcasecmp(p, "SCCS") == 0) {
		SetFileAttributes(ntfname, FILE_ATTRIBUTE_HIDDEN);
	}
	return (0);
}

/*
 * Create a name for a tmp file
 */
char *
nt_tmpnam(void)
{
	static 	char tmpfbuf[1024];

	sprintf(tmpfbuf, "%s%s", nt_tmpdir(), tmpnam(0));

	/*
	 * Because tmpnam(0) return path in NT format
	 */
	nt2bmfname(tmpfbuf, tmpfbuf);
	return (tmpfbuf);
}

int
mkstemp(char *template)
{
	int	fd;
	char	*result;

	bm2ntfname(template, template);
	result = _mktemp(template);
	if (!result) return (-1);
	fd = _open(result, _O_CREAT|_O_EXCL|_O_BINARY|_O_SHORT_LIVED, 0600);
	if (fd >= 0) {
		nt2bmfname(result, result);
	} else {
		perror(result);
	}
	return (fd);
}

/*
 * Return ture if the path name is a full path name
 */
int
nt_is_full_path_name(char *path)
{
	/*
	 * Account for NT's
	 * a) \\remote_host\directory\filename format
	 * b) drive:\filename format
	 */
	if ((path[0] == '/') || (path[0] == '\\') || (path[1] == ':')) {
		return (1);	/* Yup. it's a full path name */
	}
	return (0);		/* Nope, not a full path name  */
}


int
pipe(int fd[2], int pipe_size)
{
	if (pipe_size == 0) pipe_size = 512;
	return (_pipe(fd, pipe_size, _O_BINARY|_O_NOINHERIT));
}

int
snprintf(char *buf, size_t count, const char *format, ...)
{
	va_list	ap;
	int	rc;

	va_start(ap, format);
	rc = _vsnprintf(buf, count, format, ap);
	va_end(ap);
	return (rc);
}
