#include "system.h"


/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifndef WIN32

private int	hasSymlink(char *path);


private inline int
samefile(char *a, struct stat *sa, char *b, struct stat *sb)
{
	assert(sa); assert(sb);
	if ((sa->st_dev == 0) && (sa->st_ino == 0)) {
		if (lstat(a, sa) == -1) return 0;
	}
	if ((sb->st_dev == 0) && (sb->st_ino == 0)) {
		if (lstat(b, sb) == -1) return 0;
	}
	return ((sa->st_dev == sb->st_dev) && (sa->st_ino == sb->st_ino));
}


char *
fast_getcwd(char *buf, int len)
{
	static	char pwd[MAXPATH] = "";
	static	struct	stat pwd_sb = {0, 0}; /* cached sb struct for pwd */
	struct 	stat dot_sb = {0, 0};
	char	*t;

	if  (pwd[0] && samefile(".", &dot_sb, pwd, &pwd_sb)) {
		strcpy(buf, pwd);
		assert(len > strlen(pwd));
		assert(IsFullPath(buf));
	} else {
		pwd_sb.st_ino = 0; /* clear the sb cache */
		pwd_sb.st_dev = 0;
		if  ((t = getenv("PWD")) &&
		    samefile(".", &dot_sb, t, &pwd_sb) &&
		    !hasSymlink(t)) {
			strcpy(pwd, t);
			strcpy(buf, t);
			assert(len > strlen(pwd));
			assert(IsFullPath(buf));
		} else {
			pwd[0] = 0;
			pwd_sb.st_ino = 0; /* clear the sb cache */
			pwd_sb.st_dev = 0;
			unless ((getcwd)(pwd, sizeof(pwd))) {
				perror("fast_getcwd:");
				fprintf(stderr,
				    "fast_getcwd: fatal error, unable to get "
				    "current directory. Did someone delete "
				    "your repository while your command is "
				    "running ?\n");
				exit(1);
			}
			strcpy(buf, pwd);
			assert(len > strlen(pwd));
			assert(IsFullPath(buf));
		}
	}
	return (buf);
}

private int
hasSymlink(char *path)
{
	char *p;
	char tmp[MAXPATH];

	strcpy(tmp, path);
	p =  (tmp[0] == '/')  ? &tmp[1] : &tmp[0];
	errno = 0;
	while (1) {
		while (*p && *p != '/') p++;
		if (*p == 0) break;
		*p = 0;
		if (isSymlnk(tmp)) return 1;
		if (errno == ENOENT) return 0;
		*p++ = '/';
	}
	if (isSymlnk(tmp)) return 1;
	return 0;
}

/*
 * Get full path name of a file
 * Path with symbolic link is converted to the "real" path.
 */
char	*
fullname(char *xfile)
{
	static	char tmp[MAXPATH];
	char	tailbuf[MAXPATH], cpath[MAXPATH];
	char	here[MAXPATH] = "";
	char	*t, *d, *tail;

	/*
	 * Clean the path, because symlink code can't handle trailing slash
	 */
	cleanPath(xfile, cpath);

	if (hasSymlink(cpath)) {
		here[0] = '\0';
		strcpy(tmp, cpath);
		d = dirname(tmp);

		if (!d || streq(".", d)) {
			strcpy(tailbuf, cpath);
		} else {
			/*
			 * Caller supplied path has directory component,
			 * and it have symlinked entry in it.  
			 * Since oome part of it may not yet exist,
			 * We chdir to the max possible path before we do
			 * getcwd().
			 */
			fast_getcwd(here, sizeof(here));
			t = &d[strlen(d) - 1];
			strcpy(tailbuf, basenm(cpath));
			while (chdir(d) != 0) {
				/* if cd failed, back up one level */
				while (--t) if ((t <= d) || (*t == '/')) break;
				if (t <= d) {
					*d = 0;
					here[0] = '\0';
					strcpy(tailbuf, cpath);
					break;
				} else {
					*t = '\0';
					sprintf(tailbuf, "%s/%s",
							&t[1], basenm(cpath));
					assert(!IsFullPath(tailbuf));
				}
			}
		}
		tail = tailbuf;
	} else {
		tail = cpath;
	}

	if (IsFullPath(tail)) {
		/*
		 * If they have a full path name, then just use that.
		 * It's quicker than calling getcwd.
		 */
		strcpy(tmp, tail);
	} else {
		fast_getcwd(tmp, sizeof(tmp));
		concat_path(tmp, tmp, tail);
	}

	cleanPath(tmp, tmp);
	if (here[0]) chdir(here);
	return (tmp);
}

#else /* WIN32 */
/*
 * Translate SCCS/s.foo.c to /u/lm/smt/sccs/SCCS/s.foo.c
 */
char	*
fullname(char *gfile)
{
	static char	new[MAXPATH];
	char	pwd[MAXPATH];

	if (IsFullPath(gfile)) {
		/*
		 * If they have a full path name, then just use that.
		 * It's quicker than calling getcwd.
		 */
		strcpy(new, gfile);
	} else {
		unless (nt_getcwd(pwd, sizeof(pwd))) {
			fprintf(stderr,
			    "nt_getcwd: fatal error %lu, unable to get "
			    "current directory. Did someone delete "
			    "your repository while your command is "
			    "running ?\n", GetLastError());
			exit(1);
		}
		/*
		 * TODO we should store the PWD info
		 * in the project stuct or some here
		 * so it will be faster on the next call
		 */
		concat_path(new, pwd, gfile);
	}

	cleanPath(new, new);
	return (new);
}
#endif /* WIN32 */
