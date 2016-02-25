/*
 * Copyright 2003-2010,2014-2016 BitMover, Inc
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

private	int	getType(char *file);
private	int	_walkdir(char *dir, walkfns fn, void *data);

/*
 * walkdir() will traverse the directory 'dir' and calls the fn()
 * function for every file and directory in that directory.  fn() is
 * passed the filename (dir + '/' + file), and a 'type' of that
 * object.  Type is one of 'f'(file), 'd'(dir), 'l'(symlink) or
 * 'o'(other).  Type is always set.  An additional 'void *' data item
 * is passed to every call of action for passing generic information.
 *
 * If fn() returns -1, then if that file is a directory it will be
 * pruned from the search path.  If fn() returns a values > 0, then
 * the search will be truncated and walkdir() will return that value.
 * If fn() returns -2, then the search will be truncated like above,
 * but 0 will be returned.  This is useful for pruning using .bk_skip .
 * The walk continues as normal after that.
 *
 * The files/directories are walked in an unspecified order
 *
 * The function fn() is called like the output of find(1) and so the
 * top level directory also generates a callback.
 *
 * walkdir() returns -1 if it encounters an error, 0 on success and >0 for
 * a user specified error code from fn().
 */
int
walkdir(char *dir, walkfns fn, void *data)
{
	int	ret;
	char	type;
	char	buf[MAXPATH];

	unless (type = getType(dir)) {
		perror(dir);
		return (-1);
	}
	strcpy(buf, dir);
	ret = fn.file ? fn.file(buf, type, data) : 0;
	if (ret > 0) return (ret);
	if ((type == 'd') && (ret != -1)) {
		strcpy(buf, dir); /* callback can trash buffer */
		return (_walkdir(buf, fn, data));
	}
	return (0);
}

/*
 * sort files in a directory so "SCCS" appears first and the rest are
 * alphabetical.
 */
private int
extsort(const void *a, const void *b)
{
	char	*l = *(char **)a;
	char	*r = *(char **)b;
	int	lcnt, rcnt;

	/* add up goodness */
	lcnt = streq(l, "SCCS");
	rcnt = streq(r, "SCCS");

	if (rcnt != lcnt) return (rcnt - lcnt);
	return (strcmp(l, r));
}


/*
 * This function includes an optimization to avoid calls to lstat().
 * If the underlying FS and OS support readdir returning d_type, then
 * the type will be stored after the name (f, d, s). If not, a '?' will
 * be there.  This can be used to skip an lstat.
 */
private int
_walkdir(char *dir, walkfns fn, void *data)
{
	char	**lines;
	int	i;
	int	len;
	int	type;
	int	ret = 0;
	char	**dirlist = 0;

	lines = getdir(dir);
	sortLines(lines, extsort);
	len = strlen(dir);
	dir[len] = '/';
	EACH (lines) {
		strcpy(&dir[len+1], lines[i]);
		if (ret == 0) {
			type = lines[i][strlen(lines[i]) + 1];
			if (type == '?') type = getType(dir);

			/* file disappeared, skip it */
			unless (type) continue;

			ret = fn.file ? fn.file(dir, type, data) : 0;
			if (type == 'd') {
				if (ret == 0) {
					dirlist = addLine(dirlist, lines[i]);
					lines[i] = 0;
				}
			}
			if (ret == -1) ret = 0;	/* prune is not an error */
		}
		if (lines[i]) free(lines[i]);
	}
	freeLines(lines, 0);
	if (!ret && fn.dir) {
		dir[len] = 0;
		ret = fn.dir(dir, data);
		dir[len] = '/';
	}
	EACH (dirlist) {
		unless (ret) {
			strcpy(&dir[len+1], dirlist[i]);
			ret = _walkdir(dir, fn, data);
		}
	}
	freeLines(dirlist, free);
	if (!ret && fn.tail) {
		dir[len] = 0;
		ret = fn.tail(dir, data);
	}
	if (ret == -2) ret = 0;	/* prunedir is not an error */
	return (ret);
}

private int
getType(char *file)
{
	struct stat sb;
	int	type;

	if (lstat(file, &sb)) return (0); /* file missing */

	if (S_ISDIR(sb.st_mode)) {
		type = 'd';	/* dir */
	} else if (S_ISLNK(sb.st_mode)) {
		type = 'l';	/* symlink */
	} else if (S_ISREG(sb.st_mode)) {
		type = 'f';	/* reg file */
	} else {
		type = 'o';	/* other, socket, device, fifo? */
	}
	return (type);
}

/* this is the non-remapped getdir */
#undef	getdir
#undef	lstat

#ifndef WIN32
/*
 * The semantics of this interface is that it must return a NON-NULL list
 * even if the list is empty.  The NULL return value is reserved for errors.
 * This removes all duplicates, ".", and "..".
 *
 * The type of each item is returned after the trailing null if it is
 * known:  file\0<t>
 *	f	file
 *	d	dir
 *	l	symlink
 *	?       unknown, information not provided
 */
char	**
getdir(char *dir)
{
	char	**lines = 0;
	DIR	*d;
	int	type;
	struct	dirent   *e;

	unless (d = opendir(dir)) {
		unless (errno == ENOENT) perror(dir);
		return (0);
	}
	lines = allocLines(16);
	while (e = readdir(d)) {
		if (streq(e->d_name, ".") || streq(e->d_name, "..")) {
			continue;
		}
		switch (e->d_type) {
		    case DT_DIR: type = 'd'; break;
		    case DT_REG: type = 'f'; break;
		    case DT_LNK: type = 'l'; break;
		    default: type = '?'; break;
		}
		lines = addLine(lines, aprintf("%s%c%c", e->d_name, 0, type));
	}
	closedir(d);
	uniqLines(lines, free);
	unless (lines) lines = allocLines(2);
	return (lines);
}
#else
char	**
getdir(char *dir)
{
	struct  _finddata_t found_file;
	char	*file = found_file.name;
	char	**lines = 0;
	char	buf[MAXPATH];
	long	dh;
	int	type;
	int	retry = 5;

	strcpy(buf, dir);
	strcat(buf, "/*.*");
again:
	if ((dh =  _findfirst(buf, &found_file)) == -1L) {
		if (errno == ENOENT) return (0);
		if ((errno == EINVAL) && (retry-- > 0)) {
			usleep(100000);
			goto again;
		}
		perror(dir);
		return (0);
	}
	do {
		if (streq(file, ".") || streq(file, "..")) {
			continue;
		}
		localName2bkName(file, file);
		if (found_file.attrib & _A_SUBDIR) {
			type = 'd';
		} else {
			type = 'f';
		}
		lines = addLine(lines, aprintf("%s%c%c", file, 0, type));
	} while (_findnext(dh, &found_file) == 0);
	_findclose(dh);
	sortLines(lines, 0);
	unless (lines) lines = allocLines(2);
	return (lines);
}
#endif
