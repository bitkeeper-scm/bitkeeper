/*
 * Copyright 2014-2016 BitMover, Inc
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
#include "sccs.h"

/*
 * These routines provide an abstract interface for a database of
 * values associated with SCCS files.  The values are all C null-
 * terminated strings and may contains multiple lines of data.  The
 * 'keys' are single character letters for the different uses.
 *
 * The previous version of this code implimented this information as
 * lock files stored next to the s.file with the single letter key as
 * the first letter of the file. (so 's' is reserved) This version may
 * still use that approach underneath, but the user of this library
 * should not make any assumptions.
 *
 * Data stored in this DB is presistant in this repository and can be
 * used to pass information between BK programs, but the data is not
 * revision controlled and is not pushed to other repositories.  The
 * data is also not preserved by a 'clone'
 */

/*
 * current list of keys:
 *
 * 'p'
 * The LOCK key is used to indicate that a SCCS file is locked
 * for edit.  The contents are:
 *   <oldrev> <newrev> <user> <date> <includes> <excludes> <merges>
 *
 * 'c'
 * COMMENT stores the saved comment for the pending delta on
 * this file.
 *
 * 'd'
 * There is a delta pending. ???
 *
 * 'm'
 * resolve information to indicate a rename
 *
 * 'r'
 * resolve information about a contents merge
 *
 * 'a'
 * used by resolve to mark files that would be resolved 'again'
 */

/*
 * All access functions use a string to identify the sccs file that is
 * associated with this data. The string can be one of 3 possible forms:
 *     * the pathname to the s.file or x.file approved by sccs_filetype()
 *     * the pathname to the gfile
 *     * the ROOTKEY of the file
 */


private char *
getxfile(char *pathname, char key)
{
	char	*sfile;
	char	*p;

	if (strchr(pathname, BK_FS)) {
		/* pathname == rootkey */
		fprintf(stderr,
			"xfile functions by rootkey not yet supported.\n");
		abort();
	}
	sfile = name2sccs(pathname);
	p = strrchr(sfile, '/');
	p = p ? p + 1 : sfile;
	*p = key;

	return (sfile);
}

/*
 * give a file like: dir/SCCS/?.file
 * return ? if it is covered by the 'xfile' API
 */
int
is_xfile(char *file)
{
	int	type = sccs_filetype(file);

	if ((type == 's') || (type == 'x')) type = 0;
	return (type);
}

/*
 * exists returns a boolean to indicate if data has been stored for
 * this key for this file before.
 */
int
xfile_exists(char *pathname, char key)
{
	char	*xfile;
	int	ret;

	if (key == 'p') {
		struct	stat	sb;
		char	*p;
		char	buf[MAXPATH];

		/*
		 * A writable gfile implies we have a pfile so we don't
		 * even look.
		 */
		strcpy(buf, pathname);
		if ((p = strrchr(buf, '/')) &&
		    strneq(p-4, "SCCS", 4) &&
		    ((p-4 == buf) || (p[-5] == '/')) &&
		    (p[2] == '.')) {
			memmove(p-4, p+3, strlen(p+3)+1);
		}
		if (!lstat(buf, &sb) &&
		    S_ISREG(sb.st_mode) &&
		    (sb.st_mode & 0200)) {
			return (1);
		}
	}
	if (key == 'P') key = 'p';	/* P - physical pfile */
	xfile = getxfile(pathname, key);
	ret = exists(xfile);
	free(xfile);
	return (ret);
}

/*
 * delete will remove any data previously associated with this key
 */
int
xfile_delete(char *pathname, char key)
{
	char	*xfile;
	int	rc;

	xfile = getxfile(pathname, key);
	rc = unlink(xfile);
	free(xfile);
	return (rc);
}

/*
 * fetch will return the data previous accociated with this key
 * NULL indicates no data
 * The data returned is malloc'ed and the user must free()
 *
 * NOTE: Some callers of this function assume that if a file doesn't
 *       exist then zero will be returned and errno==ENOENT
 */
char *
xfile_fetch(char *pathname, char key)
{
	char	*data;
	char	*xfile, *sfile;
	sccs	*s;
	pfile	pf;

	xfile = getxfile(pathname, (key == 'P') ? 'p' : key);
	data = loadfile(xfile, 0);
	free(xfile);
	if (!data && (key == 'p') && xfile_exists(pathname, key)) {
		/* generate fake file */
		sfile = name2sccs(pathname);
		s = sccs_init(sfile, INIT_MUSTEXIST);
		if (s && HAS_PFILE(s) && !sccs_read_pfile(s, &pf)) {
			assert(pf.magic);
			data = aprintf("%s %s %s %s\n",
			    pf.oldrev, pf.newrev,
			    sccs_getuser(), time2date(time(0)));
			free_pfile(&pf);
		}
		sccs_free(s);
		free(sfile);
	}
	return (data);
}

/*
 * store is to associate data to a key.  Any previous value is
 * destroyed.
 *
 * The gfile-space directory for 'pathname' must already exist
 * or this command will fail.
 */
int
xfile_store(char *pathname, char key, char *value)
{
	char	*xfile;
	int	fd;

	xfile = getxfile(pathname, key);
	unless (value) {
		unlink(xfile);
		free(xfile);
		return 0;
	}
	fd = open(xfile, O_CREAT|O_WRONLY|O_TRUNC, 0664);

	/* try auto-creating SCCS dir */
	if ((fd < 0) && (errno == ENOENT)) {
		char	*d = dirname_alloc(xfile);

		if (!mkdir(d, 0777)) {
			fd = open(xfile, O_CREAT|O_WRONLY|O_TRUNC, 0664);
		}
		free(d);
	}
	if (fd == -1) return (-1);
	write(fd, value, strlen(value));
	close(fd);
	free(xfile);
	return (0);
}

/*
 * Given a gfile directory, return and possibly synthesize what would
 * have been in the original SCCS dir (dfiles, etc).
 */
char **
sdir_getdir(project *p, char *gdir)
{
	char	**list;
	char	buf[MAXPATH];

	concat_path(buf, gdir, "SCCS");
	list = getdir(buf);
	return (list);
}

/*
 * move a sfile to a new location in the same repository.
 * All metadata information (p.file, c.file) are also moved
 * at the same time
 */
int
sfile_move(project *p, char *from, char *to)
{
	char	*exts = "pcdmr";
	char	*sfile1 = name2sccs(from);
	char	*sfile2 = name2sccs(to);
	char	*s1, *s2, *t;
	int	rc;

	// XXX can replace an existing sfile2
	if (rc = fileMove(sfile1, sfile2)) goto out;

	s1 = strrchr(sfile1, '/') + 1;
	s2 = strrchr(sfile2, '/') + 1;
	for (t = exts; *t; t++) {
		*s1 = *s2 = *t;
		if ((rc = rename(sfile1, sfile2)) && (errno != ENOENT)) {
			goto out;
		}
	}
	rc = 0;
out:	free(sfile1);
	free(sfile2);
	return (rc);
}

/*
 * remove a sfile and all metadata information (p.file, c.file)
 */
int
sfile_delete(project *p, char *file)
{
	char	*sfile = name2sccs(file);
	char	*exts = "pcdmr";
	char	*s, *t;
	int	rc;

	if (rc = unlink(sfile)) goto out;

	s = strrchr(sfile, '/') + 1;
	for (t = exts; *t; t++) {
		*s = *t;
		if ((rc = unlink(sfile)) && (errno != ENOENT)) goto out;
	}
	rc = 0;
out:	free(sfile);
	return (rc);
}

int
sfile_exists(project *p, char *path)
{
	char	*sfile = name2sccs(path);
	int	rc;

	rc = exists(sfile);
	free(sfile);
	return (rc);
}
