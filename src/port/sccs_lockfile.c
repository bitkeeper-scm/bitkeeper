#include "../system.h"
#include "../sccs.h"
#include "../lib_tcp.h"

#ifdef	WIN32
#define		ino(file)	0
#define		linkcount(file)	0
#else
private	int	linkcount(char *file);
private	u32	ino(char *file);
#endif
private int	readlockf(char *file, pid_t *, char **hostp, time_t *tp);
private	char	*uniqfile(char *file, pid_t p, char *host);
private	void	setx(char *lock);
private	void	rmx(char *lock);

private	int	incr = 25000;
private	int	failed = 0;

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

retry:	unlink(uniq);
	unless ((fd = open(uniq, O_CREAT|O_RDWR|O_EXCL, 0644)) >= 0) {
		fprintf(stderr, "Can't create lockfile %s\n", uniq);
		free(uniq);
		return (-1);
	}
	p = aprintf("%u %s %u\n", getpid(), sccs_realhost(), time(0));
	if (write(fd, p, strlen(p)) != strlen(p)) {
		perror(file);
		close(fd);
		return (-1);
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
				free(p);
				free(uniq);
				return (0);
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
		    (linkcount(uniq) == 1) && (linkcount(file) == 1)) {
			if (getenv("BK_DBGLOCKS")) {
				ttyprintf(
				    "%s: waiting for attribute cache\n", file);
			}
		    	sleep(1);
		}

		if ((linkcount(uniq) == 2) && (ino(uniq) == ino(file))) {
			free(uniq);
			free(p);
			return (0);
		}

		/* Certain NFS deletions are renames to .nfs... and sometimes
		 * the link count ends up being 3.  Retry, recreating the
		 * unique file to break the link.
		 */
		if (linkcount(uniq) > 2) {
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
			return (-1);
		}
		if ((waitsecs != -1) && ((waited / 1000000) >= waitsecs)) {
			/* One last try to see if it is stale */
			if (sccs_stalelock(file, 1)) continue;
			unless (quiet < 2) {
				fprintf(stderr,
				    "Timed out waiting for %s\n", file);
			}
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
sccs_unlockfile(char *file)
{
	char	*uniq = uniqfile(file, getpid(), sccs_realhost());
	int	error = 0;

	if (unlink(uniq)) error++;
	if (unlink(file)) error++;
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
	const	int DAY = 24*60*60;

	if (readlockf(file, &pid, &host, &t) == -1) return (0);

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
				    linkcount(uniq), linkcount(file));
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

	if (readlockf(file, &pid, &host, &t) == -1) return (0);
	if ((getpid() == pid) &&
	    streq(host, sccs_realhost()) && !isLocalHost(host)) {
	    	free(host);
		return (1);
	}
	free(host);
	return (0);
}

private int
readlockf(char *file, pid_t *pidp, char **hostp, time_t *tp)
{
	int	fd, flen;
	int	try = 0;
	char	buf[1024];
	char	*host, *p;
	int	i, n;

	unless ((fd = open(file, O_RDONLY, 0)) >= 0) return (-1);

	bzero(buf, sizeof(buf));
	if ((flen = fsize(fd)) < 0) {
		perror("readlockf: read");
		close(fd);
		return (-1);
	}
	if (flen == 0) {
		close(fd);
		if ((p = getenv("BK_DBGLOCKS")) && (atoi(p) > 1)) {
			fprintf(stderr,
			     "readlockf: empty file: %s\n", file);
		}
		return (-1);
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

#ifndef WIN32
private int
linkcount(char *file)
{
	struct	stat sb;

	if (stat(file, &sb) != 0) return (-1);
	return (sb.st_nlink);

}

u32
ino(char *s)
{
	struct	stat sbuf;

	if (lstat(s, &sbuf)) return (0);
	return ((u32)sbuf.st_ino);
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
	}
	unless (lock = av[1]) exit(1);
	if (av[2]) n = atoi(av[2]);
	sprintf(tmp, "/tmp/lock-%s", sccs_getuser());
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
			usleep(20000);
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
		usleep(1);
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
		exit(1);
	}
	close(sock);
}
