/*
 * Copyright 2010,2015-2016 BitMover, Inc
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

typedef	struct {
	char	*dev;		// or server
	char	*mountpoint;	// where mounted
	char	*fs;		// nfs, disk, ssd, ?
} MP;
private	MP	*mountpoint(char *path);
private	int	disktype(char *dev);

/* reverse string sort by pathname, we want /home/bk/test_data before /home */
private int
MP_sort(const void *a, const void *b)
{
	MP	*l, *r;

	l = *(MP**)a;
	r = *(MP**)b;
	return (strcmp(r->mountpoint, l->mountpoint));
}

private void
MP_free(void *v)
{
	MP	*m = (MP*)v;

	free(m->dev);
	free(m->mountpoint);
	free(m->fs);
	free(m);
}

/*
 * Given a path, try and find the mount point and fs type.
 * Read the mtab, reverse sort pathnames, return the first match
 * This avoids problems when you have nested mounts.
 */
private MP *
mountpoint(char *path)
{
	FILE	*f;
	char	**v = 0;
	MP	**mp = 0;
	MP	*m;
	int	i;
	char	*t;
	char	fullpath[MAXPATH];

#ifdef	WIN32
	return (0);
#endif
	assert(path && *path);
	fullname(path, fullpath);
	t = fullpath + strlen(fullpath);
	if (t[-1] != '/') strcpy(t, "/");

	f = fopen("/etc/mtab", "r");
	unless (f) f = fopen("/etc/mnttab", "r");
	unless (f) f = fopen("/var/log/mount.today", "r");
	unless (f) return (0);

	while (t = fgetline(f)) {
		/*
		 * Skip all the none stuff
		 */
		unless (strchr(t, ':') || (t[0] == '/')) continue;

		v = splitLine(t, " \t", 0);
		unless (nLines(v) >= 3) {
			freeLines(v, free);
			continue;
		}
		m = new(MP);
		m->dev = strdup(v[1]);
		if (streq(v[2], "/")) {
			m->mountpoint = strdup(v[2]);
		} else {
			m->mountpoint = aprintf("%s/", v[2]);
		}
		m->fs = strdup(v[3]);
		mp = (MP**)addLine((char**)mp, (char*)m);
		freeLines(v, free);
	}
	fclose(f);
	sortLines((char**)mp, MP_sort);
	if (bk_trace) {
		EACH(mp) {
			T_DEBUG("%s %s %s",
			    mp[i]->dev,
			    mp[i]->mountpoint,
			    mp[i]->fs);
		}
	}
	m = 0;
	EACH(mp) {
		if (begins_with(fullpath, mp[i]->mountpoint)) {
			T_DEBUG("%s matches %s", fullpath, mp[i]->mountpoint);
			m = new(MP);
			m->dev = mp[i]->dev;
			mp[i]->dev = 0;
			m->mountpoint = mp[i]->mountpoint;
			mp[i]->mountpoint = 0;
			m->fs = mp[i]->fs;
			mp[i]->fs = 0;
			break;
		}
	}
	freeLines((char**)mp, MP_free);
	return (m);
}

/*
 * This is linux specific though maybe the info is available elsewhere.
 *
 * Linux passes up whether the device is rotating or not in
 * /sys/block/sda/queue/rotational
 * or
 * /sys/devices/virtual/block/md0/queue/rotational
 *
 * The second one is for newer kernals and works for MD raid devices.
 *
 * Sometimes the devices in the mount table are a chain of funky
 * symlinks:
 *    /dev/mapper/vg--nvme-wscott -> ../dm-3 -> /dev/dm-3
 *    /dev/disk/by-uid/XXX -> ../../sda
 * (I think it is possible to get a chain of symlinks as well)
 *
 * NOTE: the main block device needs to be looked up so /dev/sda1
         should look at /sys/block/sda/queue/rotational
 *
 * Older LVM disk do not follow this pattern, so glibc26-vm has
 * /dev/mapper/VolGroup00-LogVol00 which is a block device directly
 * but that kernel doesn't have the 'rotational' flag in /sys anyway.
 * So this code needs to bail silently when something isn't right.
 */
private int
disktype(char *path)
{
#ifndef	linux
	return (FS_DISK);
#else
	int	rc, i;
	FILE	*f;
	char	*t;
	struct	stat sb;
	char	buf[MAXPATH], sym[MAXPATH];

	/* full pathname expanding all symlinks
	 * This is based on fullname_expand() in bkd_cd.c, the same function
	 * in proj.c is subtly different. ;-)
	 * proj.c: only follows symlinks if they resolve to a dir.
	 * bkd_cd.c: follow symlinks anyway.
	 * XXX: why not parameterize to one routine?
	 * Or make the bkd_cd.c one global?
	 */
	while (1) {
		/* convert dir to a full pathname and expand symlinks */
		path = fullname(path, buf);
		unless (isSymlnk(path)) break;

		/*
		 * fullname() doesn't expand symlinks in the last
		 * component so fix that.
		 */
		i = readlink(path, sym, sizeof(sym));
		assert(i < sizeof(sym));
		sym[i] = 0;
		if (IsFullPath(sym)) {
			strcpy(buf, sym);
		} else {
			concat_path(buf, dirname(path), sym);
		}
	}
	unless (strneq(buf, "/dev/", 5) &&
	    !stat(buf, &sb) &&
	    S_ISBLK(sb.st_mode)) {
		/* we are expecting a block device in /dev */
		return (FS_UNKNOWN);
	}
	rc = -1;
	sprintf(sym, "/sys/block/%s/queue/rotational", buf+5);
	if (f = fopen(sym, "r")) {
		rc = fgetc(f);
		fclose(f);
		if (rc != -1) goto out;
	}

	/*
	 * For stuff like /dev/sda1 see if 'sda' works
	 */
	for (t = buf+strlen(buf)-1; isdigit(*t); --t);
	if (isdigit(t[1]) && isalpha(t[0])) {
		sprintf(sym, "/sys/block/%.*s/queue/rotational",
		    (int)(t-buf-5+1), buf+5);
		if (f = fopen(sym, "r")) {
			rc = fgetc(f);
			fclose(f);
			if (rc != -1) goto out;
		}
	}

	sprintf(sym, "/sys/devices/virtual/block/%s/queue/rotational",
	    buf+5);
	if (f = fopen(sym, "r")) {
		rc = fgetc(f);
		fclose(f);
	}
out:	if (rc != -1) T_DEBUG("found %s", sym);
	switch(rc) {
	    case '0':
		return (FS_SSD);
	    case '1':
		return (FS_DISK);
	    default:
		return (FS_UNKNOWN);
	}
#endif
}

struct fsmap {
	char	*name;
	int	type;
} fsmap[] = {
	{"ssd", FS_SSD},
	{"disk", FS_DISK},
	{"nfs", FS_NFS},
	{"?", FS_UNKNOWN},
	{0, 0}
};

int
fstype(char *dir)
{
	char	*name;
	int	type;
	MP	*m;

	/* for stupid md that doesn't send up the fstype */
	if (name = getenv("_BK_FSTYPE")) {
		struct	fsmap	*mp;

		for (mp = fsmap; mp->name; mp++) {
			if (streq(mp->name, name)) return (mp->type);
		}
		fprintf(stderr, "_BK_FSTYPE '%s' unknown\n", name);
		return (FS_UNKNOWN);
	}

	unless (isdir(dir)) return (FS_UNKNOWN);
	if (m = mountpoint(dir)) {
/*
		printf("%s %s %s\n",
		    m->dev,
		    m->mountpoint,
		    m->fs);
*/
		if (strneq("nfs", m->fs, 3)) {
			type = FS_NFS;
		} else {
			type = disktype(m->dev);
		}
		MP_free(m);
	} else {
		type = FS_UNKNOWN;
	}
	return (type);
}

/* Interface patterned after bk repotype */
int
fstype_main(int ac, char **av)
{
	int	c;
	int	quiet = 0;
	int	verbose = 0;
	char	*dir;
	int	type;
	struct	fsmap	*mp;

	while ((c = getopt(ac, av, "qv", 0)) != -1) {
		switch (c) {
		    case 'q': quiet = 1; break;
		    case 'v': verbose = 1; break;
		}
	}
	unless (dir = av[optind]) dir = ".";
	unless (isdir(dir)) {
		fprintf(stderr, "fstype: %s is not a directory\n", dir);
		return (FS_ERROR);
	}
	type = fstype(dir);
	for (mp = fsmap; mp->name; mp++) {
		if (mp->type == type) break;
	}
	assert(mp->name);
	unless (quiet) {
		if (verbose) printf("%s: ", dir);
		printf("%s\n", mp->name);
	}
	return (quiet ? type : 0);
}
