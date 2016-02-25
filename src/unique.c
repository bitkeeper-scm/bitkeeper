/*
 * Copyright 1999-2013,2015-2016 BitMover, Inc
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

#include "sccs.h"
#include "cfg.h"

private int debug_trace = 0;  // for debug
private	int reopens = 0;

private int	uniqdb_socket(int force);
private int	startUniqDaemon(void);

/*
 * Adjust the timestamp on a delta so that it is unique.
 */
int
uniq_adjust(sccs *s, ser_t d)
{
	int	fudge, rc;
	char	*msg, *msg_md5, *p1, *p2, *t;
	FILE	*fin, *fout;
	size_t	msglen, resplen;
	time_t	now = time(0);
	char	key[MAXKEY], resp[64];

	if (getenv("_BK_NO_UNIQ")) return (0);

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
	if (t = getenv("_BK_UNIQ_TIMET")) now = atoi(t);  // for test & debug
	fprintf(fout, "\n%lx\n@\n", now);
	msg = fmem_close(fout, &msglen);

	/* send the request to the uniq daemon and process the response */
again:	resplen = sizeof(resp);
	if (uniqdb_req(msg, msglen, resp, &resplen)) {
		T_DEBUG("uniqdb send error");
		free(msg);
		return (1);
	}
	fin = fmem_buf(resp, resplen);

	msg_md5 = hashstr(msg, msglen);
	if ((t = fgetline(fin)) && !streq(t, msg_md5)) {
		/* answer to the wrong question */
		T_DEBUG("discarding mismatched response: %s %s\n",
		    msg_md5, t);
		fclose(fin);
		free(msg_md5);
		goto again;
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
		fprintf(stderr, "uniqdb gave bad response: %s\n", resp);
		rc = 1;
	}

	fclose(fin);
	free(msg);
	return (rc);
}
/*
 * Send a request to the uniq_daemon and return its response, starting
 * the daemon if it's not already running.
 */
int
uniqdb_req(char *msg, int msglen, char *resp, size_t *resplen)
{
	int	force = 0, rc = 0, resends, ret, sock = -1, timeout;
	ssize_t	len;

	/*
	 * Comm with the daemon is via a UDP socket. Resend the
	 * request if no response is seen within a timeout period,
	 * starting with a 1-second timeout and backing off
	 * additively. The send or select (readable() call) below has
	 * been observed to not fail if the daemon isn't listening any
	 * more, but the recv will error after the timeout period, so
	 * go back and look for a port file etc on any error. The
	 * global variable "reopens" counts the number of such retries
	 * so we can avoid retrying forever.
	 */

reopen:	unless (sock < 0) closesocket(sock);
	if ((sock = uniqdb_socket(force)) < 0) return (1);
	force = 1;		/* restart server if we come around again */
	resends = 3;		/* try 3 packets before we restart */
	timeout = -2;
resend: timeout += 3;		/* timeout 1, 4, 7 secs */
	if (send(sock, msg, msglen, 0) < 0) {
		T_DEBUG("send err %d (%s), resends %d reopens %d",
			errno, strerror(errno), resends, reopens);
		goto reopen;
	}

	/* Wait for a response (timeout increases each round) */
	ret = readable(sock, timeout);
	if (ret < 0) {  /* error */
		T_DEBUG("select error %d (%s)", errno, strerror(errno));
		perror("select");
		rc = 1;
		goto out;
	} else if (ret == 0) {  /* timeout */
		T_DEBUG("readable timeout after %d secs, resends %d reopens %d",
			timeout, resends, reopens);
		if (resends-- > 0) goto resend;
		goto reopen;
	}

	/* this shouldn't normally block */
	if ((len = recv(sock, resp, *resplen, 0)) < 0) {
		T_DEBUG("recv error %d (%s) after %d secs, "
			"resends %d reopens %d",
			errno, strerror(errno), timeout, resends, reopens);
		goto reopen;
	}

	/* success */
	reopens = 0;
	*resplen = len;

	if (debug_trace) {
		T_DEBUG("successful request -- disabling tracing");
		safe_putenv("BK_TRACE=");
		safe_putenv("BK_TRACE_BITS=");
		safe_putenv("BK_TRACE_PIDS=");
		safe_putenv("BK_TRACE_FILES=");
		trace_init(prog);
		debug_trace = 0;
	}

out:	closesocket(sock);
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
		T_DEBUG("error %d creating startsock", errno);
		fprintf(stderr, "uniq: failed to create startsock.\n");
		return (-1);
	}
	cmd = 0;
	cmd = addLine(cmd, strdup("bk"));
	cmd = addLine(cmd, strdup("uniq_server"));
	cmd = addLine(cmd, aprintf("--startsock=%d", sockport(startsock)));
	cmd = addLine(cmd, 0);
	pid = spawnvp(P_DETACH, "bk", cmd+1);
	freeLines(cmd, free);
	T_DEBUG("new uniq_server spawned, pid=%u", pid);
	if (pid < 0) return (-1);

	T_DEBUG("waiting for startsock on port %d", sockport(startsock));
	ret = -1;
	if ((nsock = tcp_accept(startsock)) >= 0) {
		fin = fdopen(nsock, "r");
		setlinebuf(fin);
		while (s = fgetline(fin)) {
			T_DEBUG("startsock: %s", s);
			if (strneq(s, "OK", 2)) {
				ret = 0;
				break;
			} else if (strneq(s, "WARNING-waiting", 15)) {
				fprintf(stderr,"waiting for uniqdb lock...%s\n",s);
			} else {
				break;
			}
		}
		unless (ret == 0) {
			fprintf(stderr, "error starting uniq_server: ");
			if (s) {
				fprintf(stderr, "%s\n", s);
			} else {
				fprintf(stderr, "unknown error\n");
			}
		}
		fclose(fin);
		closesocket(nsock);
	} else {
		T_DEBUG("startsock accept error");
	}
	closesocket(startsock);
	return (ret);
}

private int
uniqdb_socket(int force)
{
	char	*t;
	int	fd, port, thresh;
	char	*portfile;
	char	**ports = 0;

again:	T_DEBUG("retry %d", reopens);

	/*
	 * Last-ditch effort at getting debug info: if we're starting to
	 * retry a lot, turn on tracing.
	 */
	if (t = getenv("_BK_UNIQ_TRACE_THRESH")) {
		thresh = atoi(t);
		if ((reopens >= thresh) && !getenv("BK_TRACE") && !debug_trace){
			safe_putenv("BK_TRACE=/tmp/uniqdb.log");
			safe_putenv("BK_TRACE_BITS=debug");
			safe_putenv("BK_TRACE_PIDS=1");
			safe_putenv("BK_TRACE_FILES=info.c:unique.c");
			trace_init(prog);
			T_DEBUG("retry threshold exceeded -- starting trace");
			debug_trace = 1;
		}
	}

	if (++reopens > 20) {
		fprintf(stderr, "could not start uniq_server after %d tries\n",
			reopens);
		T_DEBUG("failing -- too many retries");
		return (-1);
	}
	port = 0;
	t = uniq_dbdir();
	portfile = aprintf("%s/port", t);
	free(t);
	t = 0;
	T_DEBUG("looking for port file %s", portfile);
	if (!force && (t = loadfile(portfile, 0))) {
		ports = splitLine(t, "\n", 0);
		if (nLines(ports) < 2) {
			T_DEBUG("Too few lines!");
			fd = -1;
			goto out;
		}
		port = atoi(ports[2]);
		/*
		 * This doesn't really connect. It just binds the
		 * destination addr/port to the socket. If no
		 * uniq_daemon is listening on this port any more,
		 * we'll end up back here to retry.
		 */
		if ((fd = udp_connect("127.0.0.1", port)) < 0) {
			T_DEBUG("udp_connect: port %d errno %d", port, errno);
		}
		T_DEBUG("udp socket created, port %d", port);
	} else {
		/* failed to find existing uniq server */
		if (startUniqDaemon()) {
			fd = -1;
			goto out;
		}
		free(t);
		free(portfile);
		force = 0;
		goto again;
	}
out:	free(t);
	free(portfile);
	return (fd);
}

/*
 * Determine the db directory, in the following order:
 *
 *   if env var _BK_UNIQ_DIR is defined, use that
 *   if "uniqdb" config option exists, use <config>/%HOST
 *   if /netbatch/.bk-%USER/bk-keys-db/%HOST is writable, use that
 *   use <dotbk>/bk-keys-db/%HOST
 */
char *
uniq_dbdir(void)
{
	char	*ret, *s;

	if (s = getenv("_BK_UNIQ_DIR")) {
		ret = strdup(s);
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
