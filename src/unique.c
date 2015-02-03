#include "sccs.h"
#include "cfg.h"

private	int	uniq_open(int force);
private int	startUniqDaemon(void);

/*
 * Adjust the timestamp on a delta so that it is unique.
 */
int
uniq_adjust(sccs *s, ser_t d)
{
	int	fudge, retries = 3;
	int	force = 0, restart = 3;
	char	*p1, *p2;
	char	*t, *msg, *msg_md5;
	FILE	*fin, *fout;
	int	socket;
	int	len, ret;
	int	rc = 1;
	size_t	rlen;
	time_t	now = time(0);
	char	buf[1<<16];
	char	key[MAXKEY];

	if (getenv("_BK_NO_UNIQ")) return (0);
restart:if ((socket = uniq_open(force)) < 0) return (1);
	force = 0;

	fout = fmem();

	fprintf(fout, "insert-key\n@");
	sccs_shortKey(s, d, key);
	if (CSET(s)) {
		if (p1 = strstr(key, "/ChangeSet|")) {
			/* strip pathname */
			p2 = strchr(key, '|');
			assert(p2);
			++p1; ++p2;
			while ((*p2++ = *p1++));
		}
	}
	T_DEBUG("send key %s", key);
	hash_keyencode(fout, key);
	if (CSET(s)) {
		/* Add rand from syncRoot to cset delta keys */
		sccs_syncRoot(s, key);
		p2 = strrchr(key, '|');
		fputs(p2, fout);
	}
	if (DATE(s, d) > now) now = DATE(s, d);
	fprintf(fout, "\n%lx\n@\n", now);

	/* send the message */
	msg = fmem_close(fout, &rlen);
	msg_md5 = hashstr(msg, rlen);

again:	if (send(socket, msg, rlen, 0) < 0) {
		perror("send");
		goto out;
	}

	/* wait up to 10sec for response */
	if ((ret = readable(socket, 10)) < 0) {
		perror("select");
		goto out;
	}
	if (ret == 0) {
		/* timeout */
		if (retries--) goto again;
		if (restart--) {
			force = 1;
			retries = 3;
			if (restart == 1) {
				/* a hack to enable debugging before we fail */
				putenv("BK_TRACE_BITS=debug");
				trace_init(prog);
				putenv("BK_TRACE_BITS=");
			}
			goto restart;
		}
		goto out;
	}

	/* we know this won't block */
	if ((len = recv(socket, buf, sizeof(buf), 0)) < 0) {
		/* this shouldn't have failed, but if so, try again */
		T_DEBUG("recv failed (%s)", strerror(errno));
		closesocket(socket);
		goto restart;
	}
	fin = fmem_buf(buf, len);
	if ((t = fgetline(fin)) && !streq(t, msg_md5)) {
		/* answer to the wrong question */
		T_DEBUG("discarding mismatched response: %s %s\n",
		    msg_md5, t);
		fclose(fin);
		closesocket(socket);
		goto restart;
	}
	free(msg_md5);
	if ((t = fgetline(fin)) && strneq(t, "OK", 2)) {
		T_DEBUG("got %s", t);
		if (fudge = atoi(t+3)) {
			DATE_SET(s, d, DATE(s, d)+fudge);
			DATE_FUDGE_SET(s, d, DATE_FUDGE(s, d)+fudge);
		}
		rc = 0;
	} else {
		T_DEBUG("got bad response %s", t);
		fprintf(stderr, "uniqdb gave bad response: %s\n", buf);
	}
	fclose(fin);
out:	closesocket(socket);
	return (rc);
}

private int
startUniqDaemon(void)
{
	int	ret, startsock, nsock;
	char	**cmd, *s;
	pid_t	pid;
	FILE	*fin;

	if ((startsock = tcp_server("127.0.0.1", 0, 0)) < 0) {
		fprintf(stderr, "uniq: failed to start startsock.\n");
		exit(1);
	}
	cmd = 0;
	cmd = addLine(cmd, strdup("bk"));
	cmd = addLine(cmd, strdup("uniq_server"));
	cmd = addLine(cmd,
	    aprintf("--startsock=%d", sockport(startsock)));
	cmd = addLine(cmd, 0);
	pid = spawnvp(P_DETACH, "bk", cmd+1);
	freeLines(cmd, free);
	T_DEBUG("pid=%u", pid);
	if (pid < 0) return (-1);

	T_DEBUG("waiting for start sock on port %d....", sockport(startsock));
	if ((nsock = tcp_accept(startsock)) >= 0) {
		T_DEBUG("got start sock");
		fin = fdopen(nsock, "r");
		setlinebuf(fin);
		s = fgetline(fin);
		if (s && strneq(s, "OK", 2)) {
			T_DEBUG("daemon started OK");
			ret = 0;
		} else {
			T_DEBUG("daemon start error: %s", (s ? s : "EOF"));
			ret = -1;
		}
		fclose(fin);
		closesocket(nsock);
		closesocket(startsock);
	} else {
		T_DEBUG("accept error");
		ret = -1;
	}
	return (ret);
}

private int
uniq_open(int force)
{
	char	*t;
	int	fd, port, retries = 0;
	char	*portfile, *lockfile;
	char	**ports = 0;

	T_DEBUG("open");
again:	port = 0;
	t = uniq_dbdir(0);
	portfile = aprintf("%s/port", t);
	lockfile = aprintf("%s/lock", t);
	free(t);
	t = 0;
	T_DEBUG("looking for port file %s", portfile);
	if (!force && (t = loadfile(portfile, 0)) &&
	    (exists(lockfile) && !sccs_stalelock(lockfile, 1))) {
		ports = splitLine(t, "\n", 0);
		if (nLines(ports) < 2) {
			T_DEBUG("Too few lines!");
			fd = -1;
			goto out;
		}
		port = atoi(ports[2]);
		T_DEBUG("found port file, port %d", port);
		if ((fd = udp_connect("127.0.0.1", port)) < 0) {
			T_DEBUG("udp_connect: port %d errno %d", port, errno);
		}
	} else {
		/* failed to find existing uniq server */
		if (startUniqDaemon()) {
			fprintf(stderr, "error starting uniq_server\n");
			fd = -1;
			goto out;
		}
		if (++retries > 20) {
			fprintf(stderr, "could not start uniq_server "
				"after %d tries\n", retries);
			fd = -1;
			goto out;
		}
		free(t);
		free(portfile);
		free(lockfile);
		force = 0;
		goto again;
	}
	T_DEBUG("started");
out:	free(t);
	free(portfile);
	free(lockfile);
	return (fd);
}

/*
 * Determine the db directory, in the following order:
 *
 *   if env var _BK_UNIQ_DIR is defined, use that
 *   if "dir" is passed in, use that
 *   if "uniqdb" config option exists, use <config>/%HOST
 *   if /netbatch/.bk-%USER/bk-keys-db/%HOST is writable, use that
 *   use <dotbk>/bk-keys-db/%HOST
 */
char *
uniq_dbdir(char *dir)
{
	char	*ret, *s;

	if (s = getenv("_BK_UNIQ_DIR")) {
		ret = strdup(s);
	} else if (dir) {
		ret = strdup(dir);
	} else if (s = cfg_str(0, CFG_UNIQDB)) {
		ret = aprintf("%s/%s",
		    s, sccs_realhost());
	} else if (writable("/netbatch")) {
		ret = aprintf("/netbatch/.bk-%s/bk-keys-db/%s",
		    sccs_realuser(), sccs_realhost());
	} else {
		ret = aprintf("%s/bk-keys-db/%s",
		    getDotBk(), sccs_realhost());
	}
	return (ret);
}
