#include "../system.h"
#include "../sccs.h"

private	char	*uniqfile(const char *file);
private	int	linkcount(const char *file);

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
 */
int
sccs_lockfile(const char *file, int waitsecs, int quiet)
{
	char	*p, *uniq;
	int	fd;
	int	uslp = 1000, waited = 0;

	uniq = uniqfile(file);
	unlink(uniq);
	fd = creat(uniq, 0666);
	unless (fd >= 0) {
		fprintf(stderr, "Can't create lockfile %s\n", uniq);
		free(uniq);
		return (-1);
	}
	p = aprintf("%u %s %u\n", getpid(), sccs_realhost(), time(0));
	write(fd, p, strlen(p));
	unless (getenv("BK_REGRESSION")) fsync(fd);
	close(fd);
	for ( ;; ) {
		if (link(uniq, file) == -1) {
			if (errno == EPERM) {
				fd = open(file, O_EXCL|O_CREAT|O_WRONLY, 0666);
				if (fd != -1) {
					write(fd, p, strlen(p));
					unless (getenv("BK_REGRESSION")) {							fsync(fd);
						fsync(fd);
				    	}
					close(fd);
					free(uniq);
					free(p);
					return (0);
				}
			}
		}
		/* not true on windows file systems */
		if (linkcount(uniq) == 2) {
			free(uniq);
			free(p);
			return (0);
		}
		if (sccs_stalelock(file, 1)) continue;
		unless (waitsecs) {
			unlink(uniq);
			return (-1);
		}
		if ((waitsecs > 0) && ((waited / 1000000) >= waitsecs)) {
			unless (quiet < 2) {
				fprintf(stderr,
				    "Timed out waiting for %s\n", file);
			}
			unlink(uniq);
			free(uniq);
			free(p);
			return (-1);
		}
		waited += uslp;
		if (uslp < 1000000) uslp <<= 1;
		/* usleep() doesn't appear to work on NetBSD.  Sigh. */
		if (uslp < 1000000) {
			usleep(uslp);
		} else {
			unless (quiet) {
				fprintf(stderr, "Waiting for lock %s\n", file);
				sleep(1);
			}
		}
	}
	/* NOTREACHED */
}

int
sccs_unlockfile(const char *file)
{
	char	*uniq = uniqfile(file);
	int	error = 0;

	if (unlink(uniq)) error++;
	if (unlink((char*)file)) error++;
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
	char	*host;
	pid_t	pid;
	time_t	t;
	const	int DAY = 24*60*60;

	if (sccs_readlockf(file, &pid, &host, &t) == -1) return (0);

	if (streq(host, sccs_realhost()) && !isLocalHost(host)) {
		if (kill(pid, 0) == -1) {
stale:			if (discard) unlink((char*)file);
			if (getenv("BK_DBGLOCKS")) {
				ttyprintf("STALE %s\n", file);
			}
			free(host);
			return (1);
		}
		if (pid == getpid()) ttyprintf("LOCK LOOP on %s\n", file);
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

	unless ((fd = open(file, O_RDONLY, 0)) >= 0) {
		if (exists((char*)file)) {
			perror(file);
			return (-1);
		}
		return (-1);	/* unknown, may have lost race */
	}
	setmode(fd, _O_BINARY);
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
	assert(flen < sizeof (buf));
	assert(flen > 0);
	for (;;) {
		unless ((n = read(fd, buf, flen)) == flen) {
			close(fd);
			return (-1);
		}
		buf[n] = 0;
		for (p = buf, i= 0; *p; p++) if (*p == ' ') i++;
		if (i == 2) break;	/* should be pid host time_t */
		if (++try >= 100) {
			close(fd);
			fprintf(stderr, "sccs_readlockf: read failed\n");
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
	*pidp = atoi(buf);
	host = p;
	p = strchr(host, ' ');
	assert(p);
	*p++ = 0;
	*hostp = strdup(host);
	*tp = atoi(p);
	return (0);
}

private char *
uniqfile(const char *file)
{
	char	*p, *dir,  *uniq;

	if (strrchr(file, '/')) {
		dir = strdup(file);
		p = strrchr(dir, '/');
		*p++ = 0;
		uniq = aprintf("%s/%u@%s.%s",
		    dir, getpid(), sccs_realhost(), p);
		free(dir);
	} else {
		uniq = aprintf("%u@%s.%s", getpid(), sccs_realhost(), file);
	}
	return (uniq);
}

#ifdef WIN32
/*
 * TODO Move this inteface into the uwtlib
 * after we fixed all other code which uses
 * link() as a "fast copy" inteface.
 */
private int
link(const char *from, const char *to)
{
	errno = EPERM;
	return (-1);
}

private int
linkcount(const char *file)
{
	/*
	 * stat() is a _very_ expensive call on win32
	 * since no win32 FS supports hard links, just return 1
	 */
	return (1);
}
#else
private int
linkcount(const char *file)
{
	struct	stat sb;

	if (stat(file, &sb) != 0) return (-1);
	return (sb.st_nlink);

}
#endif

