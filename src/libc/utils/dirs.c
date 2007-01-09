#include "system.h"

private	int	_walkdir(char *dir, struct stat *sbufp, walkfn fnc, void *data);
private	char	**_getdir(char *dir, struct stat *sb1);

/*
 * walkdir() will traverse the directory 'dir' and calls the func()
 * function for every file and directory in that directory.  func()
 * is passed the filename (dir + '/' + file), and a pointer to the
 * stat struct for that file.  An additional 'void *' data item
 * is passed to every call of action for passing generic information.
 *
 * The statbuf that func() receives may not be valid, walkdir() does stat
 * optimizations and may not need to call stat for every file.  In
 * that case st_mode==0 and the user will needs to call stat if that
 * information is desired.  (NOTE: S_ISDIR() will always return the
 * correct answer because directories will always have valid statbufs)
 * If func() returns -1, then if that file is a directory it will be
 * pruned from the search path.  If func() returns a values > 0, then
 * the search will be truncated and walkdir() will return that value.
 * If func() returns -2, then the current directory and subdirectories
 * will be removed for the list to process.  All remaining files in
 * the current directory will be discarded and no subdirectories will
 * be entered.  The walk continues as normal after that.
 *
 * The files/directories are walked in an unspecified order
 *
 * The function func() is called like the output of find(1) and so the
 * top level directory also generates a callback.
 *
 * walkdir() returns -1 if it encounters an error, 0 on success and >0 for
 * a user specified error code from func().
 */
int
walkdir(char *dir, walkfn fn, void *data)
{
	struct	stat	statbuf;
	int	ret;
	char	buf[MAXPATH];

	if (lstat(dir, &statbuf)) {
		perror(dir);
		return (-1);
	}
	strcpy(buf, dir);
	ret = fn(buf, &statbuf, data);
	if (ret > 0) return (ret);
	if (S_ISDIR(statbuf.st_mode) && ret != -1) {
		strcpy(buf, dir); /* fn() can trash buffer */
		return (_walkdir(buf, &statbuf, fn, data));
	}
	return (0);
}

/*
 * sort so that files without extensions appear first,
 * these files are more likely to be directories.
 * Also we know "SCCS" is a directory.
 */
private int
extsort(const void *a, const void *b)
{
	char	*l = *(char **)a;
	char	*r = *(char **)b;
	int	lcnt, rcnt;

	/* add up goodness */
	lcnt = !strchr(l, '.') + streq(l, "SCCS");
	rcnt = !strchr(r, '.') + streq(r, "SCCS");

	if (rcnt != lcnt) return (rcnt - lcnt);
	return (strcmp(l, r));
}


/*
 * This function includes an optimization to avoid calls to lstat().
 * If a file is a directory on a UNIX filesystem, then st_nlink equals
 * the number of subdirectories including '.' and '..'.  So after we have
 * found all the subdirectories, then stats are no longer needed.
 *
 * For Windows or windows filesystems, we count on lstat() not
 * returning st_nlink > 2.  That seems it hold is the cases tried.
 */
private int
_walkdir(char *dir, struct stat *sbufp, walkfn fn, void *data)
{
	char	**lines;
	int	i;
	int	links;
	int	files, dirs = 0;
	int	len;
	int	ret = 0;
	struct	dirlist {
		char	*dir;
		struct	stat	sb;
	} *dl;
	char	**dirlist = 0;

	lines = _getdir(dir, sbufp);
	sortLines(lines, extsort);
	files = nLines(lines);
	/* if >= 2, then == numdirs + 2 */
	links = sbufp->st_nlink;
	len = strlen(dir);
	dir[len] = '/';
	EACH (lines) {
		strcpy(&dir[len+1], lines[i]);
		if (ret == 0) {
			if (links == 2 || lstat(dir, sbufp)) {
				sbufp->st_mode = 0;
			}
			ret = fn(dir, sbufp, data);
			if (S_ISDIR(sbufp->st_mode)) {
				++dirs;
				if (links > 2) --links;
				if (ret == 0) {
					dl = malloc(sizeof(*dl));
					dl->dir = lines[i];
					lines[i] = 0;
					memcpy(&dl->sb, sbufp, sizeof(*sbufp));
					dirlist = addLine(dirlist, (char *)dl);
				}
			}
			if (ret == -1) ret = 0;	/* prune is not an error */
		}
		if (lines[i]) free(lines[i]);
	}
	freeLines(lines, 0);
	/*
	 * If the loop above didn't exit early one of the follow
	 * should be true:
	 *    - links == 0 or 1  because the filesystem didn't use them.
	 *      (windows or samba)
	 *    - links == 2 because we found all the subdirectories
	 *      (unix)
	 *    - the number of links originally matched the number of files
	 *      (macos)
	 */
        unless (ret != 0 || links <= 2 || links >= files - dirs + 2) {
		dir[len] = 0;
		fprintf(stderr, "walkdir() hit unexpected condition:\n\
\tdir=%s (links=%d files=%d dirs=%d)\n",
		    dir, links, files, dirs);
		exit(1);
	}
	EACH (dirlist) {
		dl = (struct dirlist *)dirlist[i];
		unless (ret) {
			strcpy(&dir[len+1], dl->dir);
			ret = _walkdir(dir, &dl->sb, fn, data);
		}
		free(dl->dir);
	}
	freeLines(dirlist, free);
	if (ret == -2) ret = 0;	/* prunedir is not an error */
	return (ret);
}


#ifndef WIN32
/*
 * The semantics of this interface is that it must return a NON-NULL list
 * even if the list is empty.  The NULL return value is reserved for errors.
 * This removes all duplicates, ".", and "..".
 * It also checks for updates to the dir and retries if it sees one.
 */
char	**
getdir(char *dir)
{
	struct	stat	statbuf;

	if (lstat(dir, &statbuf)) {
		unless (errno == ENOENT) perror(dir);
		return (0);
	}
	return (_getdir(dir, &statbuf));
}

private char	**
_getdir(char *dir, struct stat *sb1)
{
	char	**lines = 0;
	DIR	*d;
	struct	dirent   *e;
	struct  stat	sb2;
	int	i;

again:
	unless (d = opendir(dir)) {
		unless (errno == ENOENT) perror(dir);
		return (0);
	}
	lines = allocLines(16);
	while (e = readdir(d)) {
		unless (streq(e->d_name, ".") || streq(e->d_name, "..")) {
			lines = addLine(lines, strdup(e->d_name));
		}
	}
	closedir(d);
	if (lstat(dir, &sb2)) {
		unless (errno == ENOENT) perror(dir);
		freeLines(lines, free);
		return (0);
	}
	if ((sb1->st_mtime != sb2.st_mtime) ||
	    (sb1->st_size != sb2.st_size)) {
		freeLines(lines, free);
		memcpy(sb1, &sb2, sizeof(sb2));
		goto again;
	}
	uniqLines(lines, free);
	unless (lines) lines = allocLines(2);
	return (lines);
}
#else
char	**
getdir(char *dir)
{
	return (_getdir(dir, 0));
}

private char	**
_getdir(char *dir, struct stat *sb1)
{
	struct  _finddata_t found_file;
	char	*file = found_file.name;
	char	**lines = 0;
	char	buf[MAXPATH];
	long	dh;

	bm2ntfname(dir, buf);
	strcat(buf, "\\*.*");
	if ((dh =  _findfirst(buf, &found_file)) == -1L) {
		if (errno == ENOENT) return (0);
		perror(dir);
		return (0);
	}
	do {
		unless (streq(file, ".") || streq(file, "..")) {
			localName2bkName(file, file);
			lines = addLine(lines, strdup(file));
		}
	} while (_findnext(dh, &found_file) == 0);
	_findclose(dh);
	sortLines(lines, 0);
	unless (lines) lines = allocLines(2);
	return (lines);
}
#endif
