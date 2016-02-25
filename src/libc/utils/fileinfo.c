/*
 * Copyright 2001-2010,2013,2015-2016 BitMover, Inc
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

	/*
	 * if our stat is lying about st_ino then this is busted... which
	 * is the case if the indexsvr code is enabled
	 */
	//assert(0);
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
 * Get the permissions of a file.
 */
int
perms(char *file)
{
	struct	stat sbuf;

	if (lstat(file, &sbuf)) return (0);
	return (sbuf.st_mode);
}

/*
 * Determine if a file is writable by someone.  This does NOT
 * determine if the file is writable by this process.  Use access(2)
 * for that.  This is mainly used for testing if a gfile has been
 * edited.
 */
int
writable(char *s)
{
	struct  stat sbuf;

	if (lstat(s, &sbuf)) return (0);
	if (S_ISLNK(sbuf.st_mode)) return (1);
	if (sbuf.st_mode & 0222) return (1);

	/* Set errno like access(2) because we use this as an error check */
	errno = EACCES;
	return (0);
}

/*
 * Similar to writable() above, but also must be a 'regular' file.
 * (not symlink and not directory)
 */
int
writableReg(char *s)
{
	struct  stat sbuf;

	if (lstat(s, &sbuf)) return (0);
	if (S_ISREG(sbuf.st_mode) && (sbuf.st_mode & 0222)) return (1);

	/* Set errno like access(2) because we use this as an error check */
	errno = EACCES;
	return (0);
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
 * has been observed to return -2.  Grr.  So just close if < 0.
 */
void
closeBadFds(void)
{
	int	i;

	for (i = 0; i < 3; i++) {
		if (_get_osfhandle(i) < 0) close(i);
	}
}
#endif
