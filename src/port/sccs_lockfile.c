/*
 * Copyright 2000-2009,2011-2013,2015-2016 BitMover, Inc
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

#ifdef	WIN32
#define		ino(file)	0
#define		links(file)	0
#else
private	int	links(char *file);
private	u32	ino(char *file);
#endif
private	char	*uniqfile(char *file, pid_t p, char *host);
private	void	setx(char *lock);
private	void	rmx(char *lock);

private	int	incr = 25000;
private	int	failed = 0;
private	void	addLock(char *, char *);

private	char	**lockfiles;
private	int	lockfiles_pid;

/*
 * This is difficult because we are dealing with all sorts of NFS
 * implementations.
 * I know for a fact that it is a waste of time to test on HPUX 10.20,
 * Linux 2.0.36 and IRIX seems a little better but it is also broken.
 * All the other onses seem to work with the following code.
 *
 * Create a file with a unique name, try and link it to the lock file,
 * if our link count == 2, we won.
 * Write "pid host time_t\n" into the file so we can expire stale locks.
 * If waitsecs is set, wait for the lock that many seconds before giving up.
 *
 * Note: certain client side NFS implementations take a long time to time
 * out the attributes so calling this with a low value (under 120 or so)
 * for the seconds arg is not suggested.
 *
 * sccs_unlock() and others want the uniq file even if we are windows.
 */
int
sccs_lockfile(char *file, int waitsecs, int quiet)
{
	char	*p, *uniq;
	int	fd;
	int	uslp = 500;
	u64	waited = 0;
	int	rc = -1;
	int	bark = 10;
	int	fs_en = fslayer_enable(0); /* turn off fslayer */

	p = dirname_alloc((char*)file);
	unless (writable(p)) chmod(p, 0775);

	/*
	 * Could be that the chmod doesn't allow access if we're
	 * using ACLs (e.g. Windows)
	 */
	unless (access(p, W_OK) == 0) {
		fprintf(stderr, "lockfile: %s is not writable.\n", p);
		free(p);
		goto out;
	}
	free(p);

	uniq = uniqfile(file, getpid(), sccs_realhost());

retry:	unlink(uniq);
	unless ((fd = open(uniq, O_CREAT|O_RDWR|O_EXCL, 0644)) >= 0) {
		fprintf(stderr, "Can't create lockfile %s\n", uniq);
		perror(uniq);
		free(uniq);
		goto out;
	}
	p = aprintf("%u %s %u\n", getpid(), sccs_realhost(), (int)time(0));
	if (write(fd, p, strlen(p)) != strlen(p)) {
		perror(file);
		close(fd);
		goto out;
	}
	close(fd);

	for ( ;; ) {
#ifdef	WIN32
		HANDLE	h;
		DWORD	out;

		/* XXX - unlikely to work on SFU NFS */
		h = CreateFile(file, GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
		unless (h == INVALID_HANDLE_VALUE) {
			WriteFile(h, p, strlen(p), &out, 0);
			CloseHandle(h);
			if (out == strlen(p)) {
				addLock(uniq, file);
				free(p);
				free(uniq);
				rc = 0;
				goto out;
			}
			unlink(file);
		}
#else
		/*
		 * OpenBSD 3.6 will tell you it fails when it worked.
		 */
		(void)link(uniq, file);

		/* Wait for the attribute cache to time out */
		while ((ino(uniq) == ino(file)) &&
		    (links(uniq) == 1) && (links(file) == 1)) {
			if (getenv("BK_DBGLOCKS")) {
				ttyprintf(
				    "%s: waiting for attribute cache\n", file);
			}
			sleep(1);
		}

		if ((links(uniq) == 2) && (ino(uniq) == ino(file))) {
			addLock(uniq, file);
			free(uniq);
			free(p);
			rc = 0;
			goto out;
		}

		/* Certain NFS deletions are renames to .nfs... and sometimes
		 * the link count ends up being 3.  Retry, recreating the
		 * unique file to break the link.
		 */
		if (links(uniq) > 2) {
			if (getenv("BK_DBGLOCKS")) {
				ttyprintf(
				    "%s: NFS is confused, nlink > 2\n", file);
			}
			sleep(1);
			if (ino(uniq) == ino(file)) unlink(file);
			goto retry;
		}
#endif
		failed++;
		unless (exists(uniq)) {
			fprintf(stderr, "lockfile: uniq file removed?!?\n");
			goto err;
		}

		/*
		 * Check for stale locks the first time through and after
		 * waiting a while, there is no point in doing it on each
		 * interation.
		 */
		if ((waited == 0) && sccs_stalelock(file, 1)) continue;

		unless (waitsecs) {
err:			unlink(uniq);
			free(uniq);
			free(p);
			goto out;
		}
		if ((waitsecs != -1) && ((waited / 1000000) >= waitsecs)) {
			/* One last try to see if it is stale */
			if (sccs_stalelock(file, 1)) continue;
			unless (quiet) {
				fprintf(stderr,
				    "%s: failed to get lock %s\n", prog, file);
			}
			goto err;
		}
		unless (quiet) {
			if ((waited / 1000000) == bark) {
				fprintf(stderr,
				    "%s %d: waiting for lock %s\n",
				    prog, getpid(), file);
				bark += 10;
			}
		}
		waited += uslp;
		/*
		 * If there is a lot of contention this means that some
		 * processes may starve if we go too high.  So cap it
		 * at a level that isn't going to hurt system performance
		 * with everyone after the lock but isn't too long either.
		 */
		if ((uslp < 100000) && (incr > 1)) uslp += incr;
#ifdef	__NetBSD__
		/* usleep() doesn't appear to work on NetBSD.  Sigh. */
		sleep(1);
#else
		usleep(uslp);
#endif
	}
out:	fslayer_enable(fs_en);
	return (rc);
}

int
sccs_unlockfile(char *file)
{
	char	*uniq = uniqfile(file, getpid(), sccs_realhost());
	int	error = 0;

	if (unlink(uniq)) error++;
	if (unlink(file)) error++;
	if (lockfiles_pid == getpid()) {
		removeLine(lockfiles, uniq, free);
		removeLine(lockfiles, (char *)file, free);
	}
	free(uniq);
	return (error ? -1 : 0);
}

/*
 * WARNING: this may not break locks which have localhost or 127.0.0.1
 * etc, as the hostname.  That's what you get on poorly configured machines
 * and we'll have to respect those.
 */
int
sccs_stalelock(char *file, int discard)
{
	char	*host, *uniq;
	pid_t	pid;
	time_t	t;

	if (sccs_readlockf(file, &pid, &host, &t) == -1) return (0);

	if (streq(host, sccs_realhost()) && !isLocalHost(host)) {
		if (findpid(pid) == 0) {
stale:			if (discard) {
				uniq = uniqfile(file, pid, host);
				unlink(uniq);
				free(uniq);
				unlink(file);
			}
			if (getenv("BK_DBGLOCKS")) {
				ttyprintf("STALE %s\n", file);
			}
			free(host);
			return (1);
		}

		/*
		 * This is almost certainly an NFS issue rather than a true
		 * lock loop.
		 */
		if (pid == getpid()) {
			if (getenv("BK_DBGLOCKS")) {
				uniq = uniqfile(file, pid, sccs_realhost());
				ttyprintf("%u@%s: LOCK LOOP on %s\n",
				    getpid(), sccs_realhost(), file);
				ttyprintf("%u:%u %d:%d\n",
				    ino(uniq), ino(file),
				    links(uniq), links(file));
				free(uniq);
			}
			usleep(100000);
		}
		free(host);
		return (0);
	}

	/*
	 * This can also happen with NFS but it's possible in real life
	 * so we bitch.
	 */
	if (t && ((time(0) - t) > DAY)) {
		ttyprintf("STALE timestamp %s\n", file);
		goto stale;
	}
	free(host);
	return (0);
}

int
sccs_mylock(char *file)
{
	char	*host;
	pid_t	pid;
	time_t	t;
	int	ret = 0;

	if (sccs_readlockf(file, &pid, &host, &t) == -1) return (0);
	if ((getpid() == pid) &&
	    streq(host, sccs_realhost()) && !isLocalHost(host)) {
		ret = 1;
	}
	T_LOCK("ret: %d, pid: %d, getpid: %d, host: %s, realhost: %s, !isLocalhost(host): %d",
	    ret, pid, getpid(), host, sccs_realhost(), !isLocalHost(host));
	free(host);
	return (ret);
}

int
sccs_readlockf(char *file, pid_t *pidp, char **hostp, time_t *tp)
{
	int	i, n, fd, flen;
	int	try = 0;
	char	*host, *p;
	struct	stat sb;
	char	buf[1024];

	unless ((fd = open(file, O_RDONLY, 0)) >= 0) return (-1);

	/*
	 * Once again, OpenBSD is weird.  On a local filesystem I've seen it
	 * get a zero length in the write lock.  It's very weird since it
	 * happens on the 3rd test in t.import, consistently, which is single
	 * threaded unless I'm mistaken.
	 * So we wait for up to a half second for it to sort itself out but
	 * only if it is a lock that we don't expect to be zero lengthed.
	 */
	for (i = 0; i < 10; i++) {
		if ((flen = fsize(fd)) < 0) {
			perror("readlockf: read");
			close(fd);
			return (-1);
		}
		if (flen) break;
		if (strstr(file, READER_LOCK_DIR)) break;
		usleep(50000);
	}
	if (flen == 0) {	/* expect BitKeeper/readers/pid@host.lock */
		unless (strstr(file, READER_LOCK_DIR)) {
			/*
			 * We get lots of short file reads under stress tests.
			 * In all other cases the user needs to know.
			 */
			unless (getenv("_BK_NFS_TEST")) {
				fprintf(stderr,
				    "readlockf: empty lock: %s\n", file);
			}
			close(fd);
			return (-1);
		}

		i = fstat(fd, &sb);
		assert(i != -1);
		*tp = sb.st_mtime;
		close(fd);
		p = basenm(file);
		*pidp = atoi(p);
		unless (p = strchr(p, '@')) {
err:			fprintf(stderr,
			     "readlockf: malformed empty lock: %s\n", file);
			return (-1);
		}
		host = ++p;
		unless ((p = strrchr(p, '.')) && streq(p, ".lock")) goto err;
		*p = 0;
		*hostp = strdup(host);
		*p = '.';
		return (0);
	}

	/*
	 * Win33 samba clients sometimes read garbage.
	 * If we detect this case, we wait a little and
	 * re-do the read().
	 */
	if (flen >= sizeof(buf)) {
		fprintf(stderr, "Corrupt lock file: %s\n", file);
		close(fd);
		return(-1);
	}
	assert(flen > 0);
	for (try = 0; ; ) {
		unless ((n = read(fd, buf, flen)) == flen) {
			close(fd);
			return (-1);
		}
		buf[n] = 0;
		for (p = buf, i = 0; *p; p++) if (*p == ' ') i++;
		if (i == 2) break;	/* should be pid host time_t */
		if (++try >= 100) {
			close(fd);
			fprintf(stderr,
			     "readlockf: bad format in %s\n", file);
			return (-1);
		}
		usleep(5000);
		if (lseek(fd, 0L, SEEK_SET) != 0) perror("lseek");
	}
	close(fd);
	chomp(buf);
	p = strchr(buf, ' ');
	assert(p);
	*p++ = 0;
	*pidp = strtoul(buf, 0, 0);
	host = p;
	p = strchr(host, ' ');
	assert(p);
	*p++ = 0;
	*hostp = strdup(host);
	*tp = strtoul(p, 0, 0);
	return (0);
}

private char *
uniqfile(char *file, pid_t pid, char *host)
{
	char	*p, *dir,  *uniq;

	if (strrchr(file, '/')) {
		dir = strdup(file);
		p = strrchr(dir, '/');
		*p++ = 0;
		uniq = aprintf("%s/%u@%s.%s",
		    dir, pid, host, p);
		free(dir);
	} else {
		uniq = aprintf("%u@%s.%s", pid, host, file);
	}
	return (uniq);
}

private void
addLock(char *uniq, char *file)
{
	unless (lockfiles_pid == getpid()) {
		freeLines(lockfiles, free);
		lockfiles = 0;
		lockfiles_pid = getpid();
	}
	lockfiles = addLine(lockfiles, strdup(uniq));
	lockfiles = addLine(lockfiles, strdup(file));
}

void
lockfile_cleanup(void)
{
	int	i;

	unless (lockfiles_pid == getpid()) return;
	EACH(lockfiles) {
		if (exists(lockfiles[i])) {
			fprintf(stderr, "WARNING: "
			    "deleting orphan lock file %s\n", lockfiles[i]);
			if (getenv("_BK_DEVELOPER")) {
				fprintf(stderr, "_BK_CALLSTACK=%s\n"
				    "pid: %d\n",
				    getenv("_BK_CALLSTACK"), getpid());
				systemf("cat \"%s\"", lockfiles[i]);
			}
			unlink(lockfiles[i]);
		}
	}
	freeLines(lockfiles, free);
	lockfiles = 0;
}

#ifndef WIN32
/*
 * MacOS can return bad data for heavily used files like lock files.
 * So give it some time to get it right.
 */
private int
getstat(char *file, struct stat *sb)
{
	int	ret;
	int	retry = 0;

#if	defined(__APPLE__)
	retry = 100;
#endif
	while (((ret = lstat(file, sb)) || (sb->st_nlink == 0)) && retry--) {
		usleep(1000);
	}
	return (ret);
}

private int
links(char *file)
{
	struct	stat sb;

	if (getstat(file, &sb)) return (0);
	return (sb.st_nlink);
}

u32
ino(char *file)
{
	struct	stat sb;

	if (getstat(file, &sb)) return (0);
	return ((u32)sb.st_ino);
}
#endif

/*
 * usage: bk locktest file [n]
 *
 * This will loop calling sccs_lockfile(file) and while it is locked go
 * make sure that we really own the lock by talking to a daemon that
 * currently lives in work:/home/bk/lm/bitcluster/cmd/smapper
 */
int
locktest_main(int ac, char **av)
{
	char	*lock;
	int	i, fd, fd2;
	int	net = 0, n = 1000, lie = getenv("BK_FAKE_LOCK") != 0;
	char	tmp[200];

	unless (av[1]) exit(1);
	if (streq(av[1], "-n")) {
		net++;
		av++;
		ac--;
		putenv("_BK_NFS_TEST=1");
	}
	unless (lock = av[1]) exit(1);
	if (av[2]) n = atoi(av[2]);
	sprintf(tmp, "%s/lock-%s",
	    getenv("TMP") ? getenv("TMP") : "/tmp", sccs_getuser());
	if (streq(lock, "O_EXCL")) {
		unlink(tmp);
		fd = open(tmp, O_CREAT|O_RDWR|O_EXCL, 0600);
		fd2 = open(tmp, O_CREAT|O_RDWR|O_EXCL, 0600);
		if (fd == -1) {
			perror("first open");
			return (1);
		}
		if (fd2 != -1) {
			perror("second open");
			close(fd);
			unlink(tmp);
			return (1);
		}
		close(fd);
		return (0);
	}
	incr = 1;	/* so we hammer on the lock */
	for (i = 0; i < n; ++i) {
		if (!lie && sccs_lockfile(lock, -1, 1)) {
			perror("lockfile");
			return (1);
		}
		if (net) {
			setx(lock);
			usleep(500);
			rmx(lock);
		} else {
			fd = open(tmp, O_CREAT|O_RDWR|O_EXCL, 0600);
			if (fd == -1) {
				perror(tmp);
				fprintf(stderr,
				    "Got lock but not O_EXCL lock\n");
				return (1);
			}
			usleep(1);
			close(fd);
			unlink(tmp);
		}
		sccs_unlockfile(lock);
		usleep(500);
	}
	printf("%d failed attempts, %d successes\n", failed, n);
	return (0);
}

private void
setx(char *lock)
{
	int	n;
	int	sock = tcp_connect("work", 3962);
	char	buf[1024];

	sprintf(buf, "setx\r\n%s\r\n%s@%s:%d(%u)\r\n",
	    lock, sccs_getuser(), sccs_gethost(), (int)getpid(), ino(lock));
	unless (writen(sock, buf, strlen(buf)) == strlen(buf)) {
		perror("write");
		exit(1);
	}
	n = read(sock, buf, sizeof(buf));
	unless (n > 2) {
		perror("read");
		exit(1);
	}
	unless (strneq(buf, "OK", 2)) {
		fprintf(stderr, "%d: Failed to set lock\n", getpid());
		fprintf(stderr, "smap: %s\n", buf);
		exit(1);
	}
	close(sock);
}

private void
rmx(char *lock)
{
	int	n;
	int	sock = tcp_connect("work", 3962);
	char	buf[1024];

	sprintf(buf, "rmx\r\n%s\r\n%s@%s:%d(%u)\r\n",
	    lock, sccs_getuser(), sccs_gethost(), (int)getpid(), ino(lock));
	unless (writen(sock, buf, strlen(buf)) == strlen(buf)) {
		perror("write");
		exit(1);
	}
	n = read(sock, buf, sizeof(buf));
	unless (n > 2) {
		perror("read");
		exit(1);
	}
	unless (strneq(buf, "OK", 2)) {
		fprintf(stderr, "%d: Failed to remove lock\n", getpid());
		fprintf(stderr, "smap: %s\n", buf);
		exit(1);
	}
	close(sock);
}
