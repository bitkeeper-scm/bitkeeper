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
#else
private int
scanDir(char *dir, char *name, MDBM *db, char *realname)
{
        struct  _finddata_t found_file;
        char    *file = found_file.name;
        char    buf[MAXPATH];
	char	path[MAXPATH];
        long    dh;

	realname[0] = 0;
        bm2ntfname(dir, buf);
        strcat(buf, "\\*.*");
        if ((dh =  _findfirst(buf, &found_file)) == -1L) goto done;

        do {
		if (streq(file, ".") || streq(file, "..")) continue;
		sprintf(path, "%s/%s", dir, file);
		if (db) mdbm_store_str(db, path, file, MDBM_INSERT);
		if (strcasecmp(file, name) == 0) {
			if (realname[0] == 0) {
				strcpy(realname, file);
			} else {
				strcpy(realname, name);
				break;
			}
		}
        } while (_findnext(dh, &found_file) == 0);
        _findclose(dh);

	/*
	 * If the entry does not exist (directory/file not created yet)
	 * then the given name is the real name.
	 */
done:	if (realname[0] == 0) {
		strcpy(realname, name);
		sprintf(path, "%s/%s", dir, name);
		if (db) mdbm_store_str(db, path, name, MDBM_INSERT);
	}
	return (0); /* ok */
	
}
#endif

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
	if (!path[0]) {
		realname[0] = 0;
		return (0);
	}
	cleanPath(path, mypath);

	realname[0] = 0;
	q = mypath;
	r = realname;
	
#ifdef WIN32 /* dos colon handling */
	if (q[1] == ':') {
		/*
		 * XXX This will break if we are on a different drive
		 */
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
