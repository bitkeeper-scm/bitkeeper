/*
 * Copyright 2000-2002,2004-2006,2015-2016 BitMover, Inc
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

#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */


#ifndef WIN32
private int
scanDir(char *dir, char *name, MDBM *db, char *realname)
{
	int	i;
	char	**d;
	char	path[MAXPATH];

	realname[0] = 0;
	unless (isdir(dir) && (d = getdir(dir))) goto done;
	EACH (d) {
		sprintf(path, "%s/%s", dir, d[i]);
		if (db) mdbm_store_str(db, path, d[i], MDBM_INSERT);
		if (strcasecmp(d[i], name) == 0) {
			if (realname[0] == 0) {
				strcpy(realname, d[i]);
			} else {
				/*
				 * if this dir have 2 entries differ
				 * only by case, it cannot be a case folding
				 * FS; so "name" must be the realname
				 */
				strcpy(realname, name);
				break;
			}
		}
	}
	freeLines(d, free);
	/*
	 * If the entry does not exist (directory/file not created yet)
	 * then the given name is the real name.
	 */
done:	if (realname[0] == 0) strcpy(realname, name);
	sprintf(path, "%s/%s", dir, name);
	if (db) mdbm_store_str(db, path, name, MDBM_INSERT);
	return (0); /* ok */

}

/*
 * Given a path, find the real name of the base part
 */
private int
getRealBaseName(char *path, char *realParentName, MDBM *db, char *realBaseName)
{
	char *p, *parent, *base, *dir;
	int rc;

	if (db) {
		p = mdbm_fetch_str(db, path);
		if (p) { /* cache hit */
			//fprintf(stderr, "@@@ cache hit: path=%s\n", path);
			strcpy(realBaseName, p);
			return (0); /* ok */
		}
	}
	p = strrchr(path, '/');
	if (p) {
		if (p == path) { 
			parent = "/"; base = &p[1];
			p = NULL;
		} else {
			*p = 0; 
			parent = path; base = &p[1];
		}
	} else {
		parent = "."; base = path;
	}
	/*
	 * Increase the cache hit, use the realParentName if known
	 */
	dir = realParentName[0] ? realParentName: parent;
	rc = scanDir(dir, base, db, realBaseName);
	if (p) *p = '/';
	return (rc);
}


int
getRealName(char *path, MDBM *db, char *realname)
{
	int	first = 1;
	char	*p, *q, *r;
	char	mypath[MAXPATH], name[MAXPATH];

	assert(path != realname); /* must be different buffer */
	if (!path[0]) {
		realname[0] = 0;
		return (0);
	}
	cleanPath(path, mypath);

	realname[0] = 0;
	q = mypath;
	r = realname;
	
	if (q[0] == '/') {
		q = &q[1];
	} else {
		q = mypath;
		while (strneq(q, "../", 3)) {
			q += 3;
		}
	}

	/*
	 * Scan each component in the path from top to bottom
	 */
	while (p  = strchr(q, '/')) {
		*p = 0;
		if (getRealBaseName(mypath, realname, db, name))  goto err;
		if (first) {
			char *t;
			t = strrchr(mypath, '/');
			if (t) {
				*t = 0;
				sprintf(r, "%s/%s", mypath, name);
				*t = '/';
			} else {
				sprintf(r, "%s", name);
			}
			r += strlen(r);
			first = 0;
		} else {
			sprintf(r, "/%s", name);
			r += strlen(name) + 1;
		}
		*p = '/';
		q = ++p;
	}
	if (getRealBaseName(mypath, realname, db, name))  goto err;
	*r = 0;
	
	/*
	 * Compute parent
	 * We need this because if parent is "/", realanme[] is
	 * never populated. 
	 * We don't use dirname() here, because we don't want "."
	 * when parent is dot.
	 */
	if (realname[0]) {
		p = realname;
	} else {
		q = strrchr(mypath, '/');
		if (q == NULL) { /* simple relative path: e.g. ChangeSet */
			p = "";
		} else if (q == mypath) {	/* pareht is "/" */
			p = "/";
		} else  {
			p = mypath;
			*q = 0;			/* chop off the base part */
		}
	}
	/* when we get here, p point to the parent directory */

	concat_path(realname, p, name);
	return (1);
err:	fprintf(stderr, "getRealName failed: mypath=%s\n", mypath);
	return (0);
}
#else

/*
 * Find the official name for a file on the disk.  The name we pass in
 * might be a short name, or it might have the wrong case.  This used to
 * try to work by doing a conversion to short and then conversion back
 * to long but some instances of GetLongPathName() will convert a partial
 * path into a full path.  So we tried this approach which avoids that
 * interface and seems to be the workaround others use.
 */
int
getRealName(char *path, MDBM *db, char *realname)
{
	char	**parts;
	int	i;
	char	buf[MAXPATH];
	char	buf2[MAXPATH];
	char	buf3[MAXPATH];

	parts = splitLine(path, "/\\", 0);
	/*
	 * Make sure that \path\to\where and \\machine are preserved and
	 * forward slashes.
	 * Also preserve / and //
	 */
	buf[0] = buf[1] = buf[2] = 0;
	if ((path[0] == '/') || (path[0] == '\\')) {
		buf[0] = '/';
		if ((path[1] == '/') || (path[1] == '\\')) buf[1] = '/';
	}
	EACH(parts) {
		if ((parts[i][1] == ':') ||
		    streq(parts[i], "..") || streq(parts[i], ".")) {
			concat_path(buf, buf, parts[i]);
			continue;
		}
		concat_path(buf2, buf, parts[i]);
		realBasename(buf2, buf3);
		concat_path(buf, buf, buf3);
	}
	strcpy(realname, buf);
	freeLines(parts, free);
	return (0);
}
#endif

