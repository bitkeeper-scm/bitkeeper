#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

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
		*p = 0; 
		parent = path; base = &p[1];
	} else {
		parent = "."; base = path;
	}
	/*
	 * To increase the cache hit rate
	 * we use the realParentName if it is known
	 */
	dir = realParentName[0] ? realParentName: parent;
	if ((realParentName[0]) &&  !streq(parent, ".")) {
		if (strcasecmp(parent, realParentName)) {
			fprintf(stderr, "warning: name=%s, realname=%s\n",
							parent, realParentName);
		}
		assert(strcasecmp(parent, realParentName) == 0);
	}
	rc = scanDir(dir, base, db, realBaseName);
	if (p) *p = '/';
	return (rc);
}


int
getRealName(char *path, MDBM *db, char *realname)
{
	char	mypath[MAXPATH], name[MAXPATH], *p, *q, *r;
	int	first = 1;

	assert(path != realname); /* must be different buffer */
	cleanPath(path, mypath);

	realname[0] = 0;
	q = mypath;
	r = realname;
	
#ifdef WIN32 /* dos colon handling */
	if (q[1] == ':') {
		q[0] = toupper(q[0]); /* store drive letter in upper case */
		assert(!islower(q[0]));
		q = &q[3];
		if (*q == 0) {
			strcpy(realname, mypath);
			return (0);
		}
#else
	if (q[0] == '/') {
		q = &q[1];
#endif
	} else {
		q = mypath;
		while (strneq(q, "../", 3)) {
			q += 3;
		}
	}
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
	sprintf(r, "/%s", name);
	return (1);
err:	fprintf(stderr, "getRealName failed: mypath=%s\n", mypath);
	return (0);
}
