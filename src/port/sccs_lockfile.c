#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang       All rights reserved.
 */
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
sccs_lockfile(const char *file, int waitsecs, int rm, int quiet)
{
	char	*p, *uniq;
	int	fd;
	int	uslp = 1000, waited = 0;
	struct	stat sb;

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
				    	}
					close(fd);
					return (0);
				}
			}
		}
		/* not true on windows file systems */
		if ((stat(uniq, &sb) == 0) && (sb.st_nlink == 2)) {
			if (rm) unlink(uniq);
			free(uniq);
			free(p);
			return (0);
		}
check:		if (sccs_stalelock(file, 1)) continue;
		unless (waitsecs) return (-1);
		if ((waitsecs > 0) && ((waited / 1000000) >= waitsecs)) {
			unless (quiet < 2) {
				fprintf(stderr,
				    "Timed out waiting for %s\n", file);
			}
			if (rm) unlink(uniq);
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

	if ((unlink(uniq) == 0) && (unlink((char*)file) == 0)) return (0);
	return (-1);
}

/*
 * WARNING: this may not break locks which have localhost or 127.0.0.1
 * etc, as the hostname.  That's what you get on poorly configured machines
 * and we'll have to respect those.
 */
int
sccs_stalelock(const char *file, int discard)
{
	int	fd;
	char	buf[1024];
	char	*host, *p;
	pid_t	pid;
	time_t	t = 0;
	const	int DAY = 24*60*60;

	unless ((fd = open(file, 0, 0)) >= 0) {
		if (exists((char*)file)) {
err:			perror(file);
			return (0);
		}
		return (0);	/* unknown, may have lost race */
	}
	bzero(buf, sizeof(buf));
	if (fsize(fd) == 0) {	/* old style, the file is pid@host.lock */
		struct	stat sb;

		fstat(fd, &sb);
		t = sb.st_mtime;
		close(fd);
		if (p = strrchr(file, '/')) {
			p++;
		} else {
			p = (char*)file;
		}
		pid = atoi(p);
		unless (p = strchr(p, '@')) return (0);	/* don't know */
		p++;
		host = sccs_realhost();
		if (strneq(p, host, strlen(host)) &&
		    (p[strlen(host)] == '.') && !isLocalHost(host)) {
			goto check;
		}
		goto err;
	} else {
		unless (read(fd, buf, sizeof(buf)) > 0) {
			close(fd);
			goto err;
		}
		chomp(buf);
		p = strchr(buf, ' ');
		assert(p);
		*p++ = 0;
		pid = atoi(buf);
		host = p;
		p = strchr(host, ' ');
		assert(p);
		*p++ = 0;
		t = atoi(p);
	}
	close(fd);

	if (streq(host, sccs_realhost()) && !isLocalHost(host)) {
check:		if (kill(pid, 0) == -1) {
stale:			unlink((char*)file);
			if (getenv("BK_DEBUG")) ttyprintf("STALE %s\n", file);
			return (1);
		}
		if (pid == getpid()) ttyprintf("LOCK LOOP on %s\n", file);
		return (0);
	}

	if (t && ((time(0) - t) > DAY)) {
		ttyprintf("STALE timestamp %s\n", file);
		goto stale;
	}
	return (0);
}
