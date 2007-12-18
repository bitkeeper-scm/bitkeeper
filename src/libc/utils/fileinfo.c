#include "system.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

time_t
mtime(char *s)
{
	struct	stat sbuf;
	
	if (lstat(s, &sbuf) != 0) return (0);
	return (sbuf.st_mtime);
}

int
exists(char *s)
{
	struct	stat sbuf;

	return (lstat(s, &sbuf) == 0);
}

int
isdir(char *s)
{
	struct	stat sbuf;

	if (lstat(s, &sbuf)) return (0);
	return (S_ISDIR(sbuf.st_mode));
}

/*
 * True if it is a dir or a symlink to a dir
 */
#if WIN32
int
isdir_follow(char *s)
{
	return (isdir(s));
}
#else
int
isdir_follow(char *s)
{
	struct	stat sbuf;

	if (stat(s, &sbuf)) return (0); /* follow symlinks to dirs */
	return (S_ISDIR(sbuf.st_mode));
}
#endif

int
isreg(char *s)
{
	struct	stat sbuf;

	if (lstat(s, &sbuf)) return (0);
	return (S_ISREG(sbuf.st_mode));
}


#ifdef WIN32
int isSymlnk(char *s)
{
	return (0);
}
#else
int isSymlnk(char *s)
{
	struct	stat sbuf;

	if (lstat(s, &sbuf)) return (0);
	return (S_ISLNK(sbuf.st_mode));
}
#endif

#ifdef	WIN32
int
hardlinked(char *a, char *b)
{
	HANDLE	fa = 0, fb = 0;
	BY_HANDLE_FILE_INFORMATION	infa, infb;
	int	rc = 0;

	fa = CreateFile(a, 0, FILE_SHARE_READ, 0,
	    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
	if (fa == INVALID_HANDLE_VALUE) goto END;
	fb = CreateFile(b, 0, FILE_SHARE_READ, 0,
	    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
	if (fb == INVALID_HANDLE_VALUE) goto END;
	unless (GetFileInformationByHandle(fa, &infa)) goto END;
	unless (GetFileInformationByHandle(fb, &infb)) goto END;

	if ((infa.nFileIndexHigh == infb.nFileIndexHigh) &&
	    (infa.nFileIndexLow == infb.nFileIndexLow) &&
	    (infa.dwVolumeSerialNumber == infb.dwVolumeSerialNumber) &&
	    (infa.nFileSizeHigh == infb.nFileSizeHigh) &&
	    (infa.nFileSizeLow == infb.nFileSizeLow) &&
	    (infa.nNumberOfLinks == infb.nNumberOfLinks) &&
	    (infa.nNumberOfLinks >= 2)) {
		rc = 1;
	}
END:	if (fa) CloseHandle(fa);
	if (fb) CloseHandle(fb);
	return (rc);
}
#else
int
hardlinked(char *a, char *b)
{
	struct	stat	sa, sb;
	if (stat(a, &sa) || stat(b, &sb)) return (0);
	if ((sa.st_size == sb.st_size) &&
	    (sa.st_dev == sa.st_dev) &&
	    (sa.st_ino == sa.st_ino) &&
	    (sa.st_nlink == sb.st_nlink) &&
	    (sa.st_nlink >= 2)) {
		return (1);
	}
	return (0);
}
#endif

/*
 * Determine if a file is writable by someone.  This does NOT
 * determine if the file is writable by this process.  Use access(2)
 * for that.  This is mainly used for testing if a gfile has been
 * edited.
 */
int
writable(char *s)
{
	struct	stat sbuf;

	if (lstat(s, &sbuf)) return (0);
	if (S_ISLNK(sbuf.st_mode)) return (1);
	return ((sbuf.st_mode & 0222) != 0);
}


#ifdef WIN32
off_t
fsize(int fd)
{
	DWORD	l;
	HANDLE	h;
	
	h = (HANDLE) _get_osfhandle(fd);
	if (h == INVALID_HANDLE_VALUE) return (0);
	l = GetFileSize(h, 0);
	if (l == 0xffffffff) return (0);
	return (l);
}
#else
off_t
fsize(int fd)
{
	struct	stat sbuf;

	if (fstat(fd, &sbuf)) return (0);
	unless (S_ISREG(sbuf.st_mode)) return (0);
	return (sbuf.st_size);
}
#endif

off_t
size(char *s)
{
	struct	stat sbuf;

	if (lstat(s, &sbuf)) return (0);
	unless (S_ISREG(sbuf.st_mode)) return (0);
	return (sbuf.st_size);
}

/*
 * This function returns true if the file has exactly one link
 * Used by the locking code
 */
#ifdef WIN32
int
onelink(char *s)
{
	return (1); /* win32 has no hard links */
}
#else
int
onelink(char *s)
{
	struct stat sbuf;

	if ((stat(s, &sbuf) == 0) && (sbuf.st_nlink == 1)) return (1);
	return (0);
}
#endif


#ifdef WIN32
/*
 * Return true if basename is a reserve name
 */
int
Reserved(char *baseName)
{
	char *p;

	if (patheq("con", baseName)) return (1);
	if (patheq("prn", baseName)) return (1);
	if (patheq("nul", baseName)) return (1);
	if (pathneq("lpt", baseName, 3)) {
		p = &baseName[3];
		
		while (*p) {
			if (!isdigit(*p)) return (0);
		}
		return (1);
	}
	
	/* check for drive colon path */
	if ((baseName[0] != '\0') && 
	    (baseName[1] == ':') &&
	    (baseName[2] == '\0')) {
		return (1);
	}
	return (0);
}
#endif


#ifdef WIN32
/*
 * On Windows, if a parent process spawns a child with stdin and stdout closed,
 * the child is started with the low level handle closed, _but_ the C runtime
 * fd0 and fd1 are still marked as taken. This function is used to probe
 * the low level os handles and close fd0 and fd1 if they point to invalid
 * handles.
 *
 * On Vista, _get_osfhandle() which is documented to return INVALID_HANDLE_VALUE
 * has been observed to return -2.  Grr.
 */
#define INVALID_HANDLE_VALUE_VISTA -2
void
closeBadFds(void)
{
	int	i;
	HANDLE	fh;

	for (i = 0; i < 3; i++) {
		fh = (HANDLE)_get_osfhandle(i);
		if ((fh == INVALID_HANDLE_VALUE) ||
		    (is_vista() && (fh == INVALID_HANDLE_VALUE_VISTA))) {
			close(i);
		}
	}
}
#endif
