#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */


#ifndef WIN32
private int
scanDir(char *dir, char *name, MDBM *db, char *realname)
{
	DIR *d;
	struct dirent *e;
	char path[MAXPATH];

	realname[0] = 0;
	d = opendir(dir);
	unless (d) goto done;

	while (e = readdir(d)) {
		if (streq(e->d_name, ".") || streq(e->d_name, "..")) continue;
		sprintf(path, "%s/%s", dir, e->d_name);
		if (db) mdbm_store_str(db, path, e->d_name, MDBM_INSERT);
		if (strcasecmp(e->d_name, name) == 0) {
			if (realname[0] == 0) {
				strcpy(realname, e->d_name);
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
	closedir(d);
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
 * File the official name for a file on the disk.  The name we pass in
 * might be a short name, or it might have the wrong case.  The quick way
 * to do this on Windows is to convert to the short name and then back
 * to the long name.
 */
int
getRealName(char *path, MDBM *db, char *realname)
{
	char	*p;
	char	buf[MAXPATH];

	unless (GetShortPathName(path, buf, MAXPATH)) strcpy(buf, path);
	unless (GetLongPathName(buf, realname, MAXPATH)) strcpy(realname, buf);

	/* The win98 emulation code returns backslashes */
	localName2bkName(realname, realname);

	/* GetLongPathName puts a / at the end of directories. */
	p = realname + strlen(realname);
	if ((p > realname) && (p[-1] == '/')) p[-1] = 0;

#if	0
	fprintf(stderr, "GetLong '%s' => '%s' => '%s'\n", path, buf, realname);
#endif

	return (0);
}
#endif

