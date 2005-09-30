#include "../system.h"
#include "../sccs.h"

#ifndef	WIN32
private	int	linkcount(const char *file);
#endif
private	char	*uniqfile(const char *file, pid_t p, char *host);

private	int	incr = 25000;
private	int	failed = 0;
private	void	addLock(const char *, const char *);

private	char	**lockfiles;
private	int	lockfiles_pid;

/*
 * Create a file with a unique name,
 * try and link it to the lock file,
 * if our link count == 2, we won.
 * If we get EPERM, we know we are on a Unix client against a SMB fs,
 * so switch to O_EXCL mode.
 * Write "pid host time_t\n" into the file so we can expire stale locks.
 * If waitsecs is set, wait for the lock that many seconds before giving up.
 * Note: certain client side NFS implementations take a long time to time
 * out the attributes so calling this with a low value (under 120 or so)
 * for the seconds arg is not suggested.
 *
 * sccs_unlock() and others want the uniq file even if we are windows.
 */
int
sccs_lockfile(const char *file, int waitsecs, int quiet)
{
	char	*p, *uniq;
	int	fd;
	int	uslp = incr;
	u64	waited = 0;

	p = dirname_alloc((char*)file);
	unless (access(p, W_OK) == 0) {
		if (chmod(p, 0775)) {
			fprintf(stderr, "lockfile: %s is not writable.\n", p);
			free(p);
			return (-1);
		}
	}
	free(p);
	uniq = uniqfile(file, getpid(), sccs_realhost());
	unlink(uniq);
	unless ((fd = open(uniq, O_CREAT|O_RDWR|O_EXCL, 0644)) >= 0) {
		fprintf(stderr, "Can't create lockfile %s\n", uniq);
		free(uniq);
		return (-1);
	}
	p = aprintf("%u %s %u\n", getpid(), sccs_realhost(), time(0));
	write(fd, p, strlen(p));
	for ( ;; ) {
#ifdef	WIN32
		HANDLE	h;
		DWORD	out;

		h = CreateFile(file, GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
		unless (h == INVALID_HANDLE_VALUE) {
			WriteFile(h, p, strlen(p), &out, 0);
			CloseHandle(h);
			if (out == strlen(p)) {
				close(fd);
				addLock(uniq, file);
				free(p);
				free(uniq);
				return (0);
			}
			unlink(file);
		}
#else
		if ((link(uniq, file) == 0) && (linkcount(uniq) == 2)) {
			close(fd);
			addLock(uniq, file);
			free(uniq);
			free(p);
			return (0);
		}
#endif
		failed++;
		if (sccs_stalelock(file, 1)) continue;
		unless (waitsecs) {
			close(fd);
			unlink(uniq);
			free(uniq);
			free(p);
			return (-1);
		}
		if ((waitsecs != -1) && ((waited / 1000000) >= waitsecs)) {
			unless (quiet < 2) {
				fprintf(stderr,
				    "Timed out waiting for %s\n", file);
			}
			close(fd);
			unlink(uniq);
			free(uniq);
			free(p);
			return (-1);
		}
		if (!quiet && ((quiet*1000000) > waited)) {
			fprintf(stderr,
			    "%d: waiting for lock %s\n", getpid(), file);
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
	/* NOTREACHED */
}

int
sccs_unlockfile(const char *file)
{
	char	*uniq = uniqfile(file, getpid(), sccs_realhost());
	int	error = 0;

	if (unlink(uniq)) error++;
	if (unlink((char*)file)) error++;
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
sccs_stalelock(const char *file, int discard)
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
				unlink((char*)file);
			}
			if (getenv("BK_DBGLOCKS")) {
				ttyprintf("STALE %s\n", file);
			}
			free(host);
			return (1);
		}
		if (pid == getpid()) {
			ttyprintf("%u@%s: LOCK LOOP on %s\n",
					getpid(), sccs_realhost(), file);
		}
		free(host);
		return (0);
	}

	if (t && ((time(0) - t) > DAY)) {
		ttyprintf("STALE timestamp %s\n", file);
		goto stale;
	}
	free(host);
	return (0);
}

int
sccs_mylock(const char *file)
{
	char	*host;
	pid_t	pid;
	time_t	t;

	if (sccs_readlockf(file, &pid, &host, &t) == -1) return (0);
	if ((getpid() == pid) &&
	    streq(host, sccs_realhost()) && !isLocalHost(host)) {
	    	free(host);
		return (1);
	}
	free(host);
	return (0);
}

int
sccs_readlockf(const char *file, pid_t *pidp, char **hostp, time_t *tp)
{
	int	fd, flen;
	int	try = 0;
	char	buf[1024];
	char	*host, *p;
	int	i, n;

	unless ((fd = open(file, O_RDONLY, 0)) >= 0) return (-1);

	bzero(buf, sizeof(buf));
	if ((flen = fsize(fd)) < 0) {
		perror("fsize");
		return (-1);
	}
	
	if (flen == 0) {	/* old style, the file is pid@host.lock */
		struct	stat sb;

		fstat(fd, &sb);
		*tp = sb.st_mtime;
		close(fd);
		p = basenm((char*)file);
		*pidp = atoi(p);
		unless (p = strchr(p, '@')) return (-1);	/* don't know */
		host = ++p;
		unless ((p = strrchr(p, '.')) && streq(p, ".lock")) return (-1);
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
		for (p = buf, i= 0; *p; p++) if (*p == ' ') i++;
		if (i == 2) break;	/* should be pid host time_t */
		if (++try >= 100) {
			close(fd);
			fprintf(stderr, "readlockf: read failed\n");
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
uniqfile(const char *file, pid_t pid, char *host)
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

#ifndef WIN32
private int
linkcount(const char *file)
{
	struct	stat sb;

	if (stat(file, &sb) != 0) return (-1);
	return (sb.st_nlink);

}
#endif

/*
 * usage: bk locktest file [n]
 *
 * This will loop calling sccs_lockfile(file) and while it is locked go
 * make sure that we can do an exclusive open of /tmp/lock
 */
int
locktest_main(int ac, char **av)
{
	char	*lock = av[1];
	int	i, fd, fd2;
	int	n = 1000, lie = getenv("BK_FAKE_LOCK") != 0;
	char	tmp[200];

	unless (av[1]) exit(1);
	if (av[2]) n = atoi(av[2]);
	sprintf(tmp, "/tmp/lock-%s", sccs_getuser());
	if (streq(av[1], "O_EXCL")) {
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
		fd = open(tmp, O_CREAT|O_RDWR|O_EXCL, 0600);
		if (fd == -1) {
			perror(tmp);
			fprintf(stderr, "Got lock but not O_EXCL lock\n");
			return (1);
		}

		usleep(1);
		close(fd);
		unlink(tmp);
		sccs_unlockfile(lock);
		usleep(1);
	}
	printf("%d failed attempts, %d successes\n", failed, n);
	return (0);
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
			unlink(lockfiles[i]);
		}
	}
	freeLines(lockfiles, free);
	lockfiles = 0;
}

private void
addLock(const char *uniq, const char *file)
{
	unless (lockfiles_pid == getpid()) {
		freeLines(lockfiles, free);
		lockfiles = 0;
		lockfiles_pid = getpid();
	}
	lockfiles = addLine(lockfiles, strdup(uniq));
	lockfiles = addLine(lockfiles, strdup(file));
}
