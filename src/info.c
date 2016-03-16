/*
 * Copyright 2012,2015-2016 BitMover, Inc
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
#include "tomcrypt.h"
#include "tomcrypt/randseed.h"

#define	INFO_DELETE	2	/* delete items */
#define	INFO_GET	3	/* print items */
#define	INFO_INSERT	4	/* insert if not already there */
#define	INFO_SET	5	/* set, replacing any earlier value */
#define	INFO_UNIQUE	6	/* return a unique integer */
#define	INFO_VERSION	7	/* print info version */
#define	INFO_COUNT	8	/* count up items matching a regexp */
#define	INFO_INSERT_KEY	9	/* uniq db delta-key insert */

/* Keys that start with a space are hidden metadata. */
#define	DB_VERSION	" version"
#define	DB_UNIQUE	" unique"

/*
 * This hidden metadata key is written to the uniqdb and must have
 * valid bk6 uniqdb key syntax. Its value is the time_t - 1year of the
 * last time bk6 touched its uniqdb. This always gets pruned from the
 * uniqdb by bk6 but bk7 leaves it.
 */
#define	DB_MODTIME	"uniqdb@bitkeeper.com|METADATA|00000000000001"

/* if this doesn't match we need to do a conversion */
#define	VERSION		"1.0"

#define	PORT		0x6569	/* Wayne's birthday 6/5/69 */

/* options and global state */
typedef struct {
	char	*dir;		/* dir containing db, port, and lock files */
	FILE	*db_primf;	/* the open uniqdb, primary */
	FILE	*db_backf;	/* the open uniqdb, backup */
	FILE	*fin;		/* from client stdout */
	FILE	*fout;		/* to client stdin */
	FILE	*logf;
	int	logfd;
	int	startsock_port;	/* --startsock=port */
	int	startsock_sock;	/* startsock socket fd, if open */
	hash	*db;
	u32	uniq_daemon:1;	/* =1 if run as bk uniq_daemon */
	u32	do_sync:1;	/* open logfile with O_SYNC */
	u32	log:1;		/* maintain backup logfile */
	u32	dashx:1;	/* -x echo input read */
	u32	nolock:1;	/* don't use a lock file */
	u32	quiet:1;
	u32	quit:1;		/* tell the running server to quit */
	u32	exit:1;		/* exit after processing current request */
} Opts;

private void	db_close(Opts *opts);
private int	db_open(Opts *opts);
private void	debug_sleep(char *env);
private void	inc_date(char *p);
private	int	info_cmds(Opts *opts);
private	void	op(Opts *opts, int cmd, hash *h, char *regexp);
private int	quit(int quiet);
private void	startsock_close(Opts *opts);
private void	startsock_printf(Opts *opts, const char *fmt, ...);
private void	uniqdb_append(Opts *opts, char *kptr, char *vptr);
private void	uniqdb_close(Opts *opts);
private int	uniqdb_lock(Opts *opts);
private char	*uniqdb_lock_path(void);
private int	uniqdb_unlock(void);
private int	uniqdb_open(Opts *opts);
private void	uniqdb_prune(hash *db);
private time_t	uniqdb_read(Opts *opts);
private void	uniqdb_sync(Opts *opts);
private void	uniqdb_write(Opts *opts);
private int	uniqdb_write_file(Opts *opts, char *path);
private void	uniqdb_write_rec(FILE *f, char *k, char *v);
private	void	unique(Opts *opts, char *arg);
private time_t	uniq_drift(void);
private	void	version(hash *db, FILE *fout);

int
info_server_main(int ac, char **av)
{
	int	c, len;
	int	nreqs = 0, rc = 1, sock = -1, tcpsock = -1, udpsock = -1;
	int	keys_since_sync = 0;
	int	port = 0;
	char	*peer = 0, *s, *lockfile, *portfile;
	int	idle = 0;
	fd_set	fds;
	struct timeval delay;
	time_t	t;
	Opts	opts = {0};
	char	buf[1<<16];
	struct	sockaddr_in cliaddr;
	longopt	lopts[] = {
		{ "idle;", 300 },
		{ "no-log", 310 },
		{ "startsock;", 320 },
		{ "dir;", 330 },
		{ "no-lock", 340 },
		{ "quit", 350 },
		{ 0, 0 }
	};

	opts.do_sync = 0;
	opts.log = 1;
	opts.startsock_sock = -1;
	if (streq(prog, "uniq_server")) {
		opts.uniq_daemon = 1;
		opts.log = 0;
		idle = getenv("BK_REGRESSION") ? 10 : 10*MINUTE;
	}
	while ((c = getopt(ac, av, "fp:qx", lopts)) != EOF) {
		switch (c) {
		    case 'f': opts.do_sync = 1; break;
		    case 'p': port = atoi(optarg); break;
		    case 'q': opts.quiet = 1; break;
		    case 'x': opts.dashx = 1; break;
		    case 300:	// --idle=<secs>
			idle = atoi(optarg);
			if (idle <= 0) usage();
			break;
		    case 310:	// --no-log
			opts.log = 0;
			break;
		    case 320:	// --startsock=<port>
			opts.startsock_port = atoi(optarg);
			break;
		    case 330:	// --dir=<db-directory>
			safe_putenv("_BK_UNIQ_DIR=%s", optarg);
			break;
		    case 340:	// --no-lock
			opts.nolock = 1;
			break;
		    case 350:	// --quit
			opts.quit = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}

	/*
	 * Verify the db directory is writable. It's fatal if it
	 * isn't.  Then change to /tmp and stay there so we have no
	 * chance of holding the db dir open during our exit path.
	 * Use the following (see uniq_dbdir()):
	 *
	 *   if env var _BK_UNIQ_DIR is defined, use that (--dir sets it)
	 *   if "uniqdb" config option exists, use <config>/%HOST
	 *   if /netbatch/.bk-%USER/bk-keys-db/%HOST is writable, use that
	 *   use <dotbk>/bk-keys-db/%HOST
	 */
	opts.dir = uniq_dbdir();
	mkdirp(opts.dir);
	opts.dir = fullLink(opts.dir, 0, 1);
	unless (!chdir(opts.dir) && writable(opts.dir)) {
		fprintf(stderr, "%s not writable for uniqdb\n", opts.dir);
		T_DEBUG("dir %s not writable", opts.dir);
		startsock_printf(&opts, "ERROR-no writable directory\n");
		startsock_close(&opts);
		return (1);
	}
	lockfile = aprintf("%s/lock", opts.dir);
	portfile = aprintf("%s/port", opts.dir);
	chdir("/tmp");
	T_DEBUG("using db dir %s but running in /tmp", opts.dir);

	if (opts.quit) return (quit(opts.quiet));

	/*
	 * Use a lock file to ensure at most one instance of this
	 * daemon is ever running and servicing requests. One might
	 * get started and then exit quickly if it's unable to acquire
	 * the lock, but that's OK since it will have no side effects
	 * (beyond the startsock handshake if the client requests it).
	 */
	if (opts.nolock || !sccs_lockfile(lockfile, 0, 1)) {
		if ((tcpsock = tcp_server(0, port, 0)) < 0) goto out;
		if ((udpsock = udp_server(0, port, 0)) < 0) goto out;
		T_DEBUG("started server on port (%d tcp) (%d udp)",
		    sockport(tcpsock), sockport(udpsock));
		Fprintf(portfile, "%d\n%d\n",
		    sockport(tcpsock), sockport(udpsock));
	} else {
		T_DEBUG("another server instance already running");
	}

	/* Bail if another instance beat us to the lock. */
	if (tcpsock < 0) {
		startsock_printf(&opts, "OK-daemon already running\n");
		startsock_close(&opts);
		return (0);
	}

	/*
	 * For uniqdb interop with older bk, acquire and hold the
	 * oldbk uniqdb lock file the entire time we're running. This
	 * locks out an older bk from accessing the db so we can
	 * safely read it here.
	 */
	if (opts.uniq_daemon) {
		if (uniqdb_lock(&opts)) {
			T_DEBUG("uniqdb lock timeout -- bailing");
			fprintf(stderr, "error: uniqdb lock timeout\n");
			startsock_printf(&opts, "ERROR-uniqdb lock timeout\n");
			startsock_close(&opts);
			goto err;
		}
		t = uniqdb_read(&opts);
		uniqdb_open(&opts);
		if ((time(0) - t) < 1*DAY) {
			T_DEBUG("bk6 touched uniqdb (%ld), using 10s timeout",
				time(0)-t);
			idle = 10;
		}
	}

	/* Handshake to know portfile has been written and locks acquired. */
	startsock_printf(&opts, "OK-daemon started in %s\n", opts.dir);
	startsock_close(&opts);

	T_DEBUG("starting loop");
	while (1) {
		FD_ZERO(&fds);
		FD_SET(tcpsock, &fds);
		FD_SET(udpsock, &fds);
		delay.tv_sec = idle ? idle : 60;
		delay.tv_usec = 0;
		rc = select(max(tcpsock, udpsock)+1, &fds, 0, 0, &delay);
		if (rc < 0) {
			perror("sock");
			rc = 1;
			break;
		} else if ((rc == 0) && idle) {
			/* timeout occurred */
			rc = 0;
			break;
		}
		if (FD_ISSET(tcpsock, &fds)) {
			if ((sock = tcp_accept(tcpsock)) < 0) continue;
			peer = peeraddr(sock);
			T_DEBUG("tcp connection from %s", peer);
			opts.fin = fdopen(sock, "r");
			opts.fout = fdopen(sock, "w");
			setlinebuf(opts.fin);
			unless (db_open(&opts)) {
				info_cmds(&opts);
			}
			db_close(&opts);
			if (opts.exit) break;
			fclose(opts.fin);
			fclose(opts.fout);
			close(sock);
			T_DEBUG("%s tcp is done", peer);
		}
		if (FD_ISSET(udpsock, &fds)) {
			int	n;
			size_t	rlen;
			char	*resp, *md5sum;

			len = sizeof(cliaddr);
			n = recvfrom(udpsock, buf, sizeof(buf), 0,
			    (struct sockaddr*)&cliaddr, &len);
			if (n < 0) {
				perror("recvfrom");
				T_DEBUG("n=%d err=%s", n, strerror(errno));
				continue;
			}
			peer = inet_ntoa(cliaddr.sin_addr);
			T_DEBUG("udp request from %s", peer);
			opts.fin = fmem_buf(buf, n);
			opts.fout = fmem();
			md5sum = hashstr(buf, n);
			fprintf(opts.fout, "%s\n", md5sum);
			free(md5sum);

			unless (db_open(&opts)) {
				info_cmds(&opts);
			}
			db_close(&opts);
			if (opts.exit) break;
			fclose(opts.fin);
			resp = fmem_close(opts.fout, &rlen);
			debug_sleep("_BK_UNIQ_REQ_SLEEP");
			n = sendto(udpsock, resp, rlen, 0,
				(struct sockaddr*)&cliaddr, len);
			T_DEBUG("n=%d %s udp is done", n, peer);
		}
		/* periodically sync or close/open the uniqdb */
		if (opts.uniq_daemon && (++keys_since_sync > 20)) {
			if (proj_sync(0)) {
				uniqdb_sync(&opts);
			} else {
				uniqdb_close(&opts);
				uniqdb_open(&opts);
			}
			keys_since_sync = 0;
		}
		/* possibly bail early for test & debug */
		if ((s = getenv("_BK_UNIQ_REQS")) && (++nreqs >= atoi(s))) {
			if (s[strlen(s)-1] == '!') {
				T_DEBUG("simulate crash after %d reqs", nreqs);
				exit(1);
			} else {
				T_DEBUG("debug exit after %d reqs", nreqs);
				break;
			}
		}
	}
	T_DEBUG("cleaning up (port %d, dir %s)...", sockport(sock), opts.dir);
	debug_sleep("_BK_UNIQ_EXIT_SLEEP");
	if (opts.uniq_daemon) {
		uniqdb_prune(opts.db);
		uniqdb_close(&opts);
		uniqdb_write(&opts);
		uniqdb_unlock();
	}
err:	unlink(portfile);
	unless (opts.nolock) sccs_unlockfile(lockfile);
	if (opts.exit) {
		char	*msg = "OK-exiting\n";

		T_DEBUG("%s requested exit", peer);
		sendto(udpsock, msg, sizeof(msg), 0,
		       (struct sockaddr*)&cliaddr, len);
		fprintf(opts.fout, "OK-exiting\n");
		fflush(opts.fout);
		fclose(opts.fin);
		fclose(opts.fout);
		close(sock);
	}
	T_DEBUG("exiting...");
out:	unless (tcpsock < 0) closesocket(tcpsock);
	unless (udpsock < 0) closesocket(udpsock);
	return (rc);
}

int
info_shell_main(int ac, char **av)
{
	int	c;
	Opts	opts = {0};
	longopt	lopts[] = {
		{ "dir;", 330 },
		{ "uniqdb", 340 },
		{ 0, 0 }
	};

	opts.do_sync = 1;
	opts.log = 1;
	while ((c = getopt(ac, av, "fx", lopts)) != EOF) {
		switch (c) {
		    case 'f': opts.do_sync = 0; break;
		    case 'x': opts.dashx = 1; break;
		    case 330:	// --dir=<db-directory>
			opts.dir = strdup(optarg);
			break;
		    case 340:	// --uniqdb
			opts.uniq_daemon = 1;
			break;
		}
	}
	unless (opts.dir || opts.uniq_daemon) {
		fprintf(stderr, "--dir or --uniqdb option required\n");
		return (1);
	}
	T_DEBUG("info_shell starting");
	if (opts.uniq_daemon) {
		uniqdb_read(&opts);
	} else {
		mkdirp(opts.dir);
		opts.dir = fullLink(opts.dir, 0, 1);
		chdir(opts.dir);
	}
	opts.fin = stdin;
	opts.fout = stdout;
	unless (db_open(&opts)) {
		info_cmds(&opts);
	}
	db_close(&opts);
	return (0);
}

private int
info_cmds(Opts *opts)
{
	int	cmd, space;
	char	*p, *t, *arg;
	hash	*h = 0;

	/*
	 * Parse the command and put the trailing, if any, in arg.
	 * Load the hash, if any.
	 * Then do the command.
	 */
	while (1) {
		fflush(opts->fout);
		if (opts->log) fflush(opts->logf);
#ifndef	O_SYNC
		if (opts->log && opts->do_sync) fsync(opts->logfd);
#endif
		unless (p = fgetline(opts->fin)) break;
		T_DEBUG("got '%s'", p);
		if (opts->dashx) fprintf(opts->fout, "IN-%s\n", p);
		arg = 0;
		space = 0;
		for (t = p; *t && !isspace(*t); t++);
		if (isspace(*t)) {
			space = *t;
			*t = 0;
		}
		if (streq(p, "delete")) {
			cmd = INFO_DELETE;
		} else if (streq(p, "get")) {
			cmd = INFO_GET;
		} else if (streq(p, "insert")) {
			cmd = INFO_INSERT;
		} else if (streq(p, "insert-key")) {
			cmd = INFO_INSERT_KEY;
		} else if (streq(p, "set")) {
			cmd = INFO_SET;
		} else if (streq(p, "unique")) {
			cmd = INFO_UNIQUE;
		} else if (streq(p, "version")) {
			cmd = INFO_VERSION;
		} else if (streq(p, "count")) {
			cmd = INFO_COUNT;
		} else if (streq(p, "quit")) {
			fprintf(opts->fout, "OK-Good Bye\n");
			break;
		} else if (streq(p, "exit")) {
			opts->exit = 1;
			break;
		} else {
			if (space) *t = space;
bad:			fprintf(opts->fout,
			    "ERROR-bad cmd '%s' (%u bytes)\n",
			    p, (u32)strlen(p));
			continue;
		}
		if (space) {
			*t = space;
			for (t++; *t && isspace(*t); t++);
			if (*t) arg = t;
		}

		switch (cmd) {
		    case INFO_INSERT:
		    case INFO_INSERT_KEY:
		    case INFO_SET:
			if (arg) goto bad;
		    case INFO_DELETE:
		    case INFO_GET:
			if (h) {
				hash_free(h);
				h = 0;
			}
			unless (arg) h = hash_fromStream(0, opts->fin);
			op(opts, cmd, h, arg);
			break;

		    case INFO_COUNT:
			op(opts, cmd, h, arg);
			break;

		    case INFO_UNIQUE:
			unique(opts, arg);
			break;

		    case INFO_VERSION:
			version(opts->db, opts->fout);
			break;
		}
	}
	fflush(opts->fout);
	return (0);
}

private void
op(Opts *opts, int cmd, hash *h, char *regexp)
{
	int	n = 0;
	hash	*h2 = hash_new(HASH_MEMHASH);
	pcre	*re = 0;
	char	*date, *p;
	const	char	*perr;
	int	fudge, poff;
	char	buf[32];
	time_t	t;

	if ((cmd == INFO_COUNT) && !regexp) {
		EACH_HASH(opts->db) {
			if (*(char *)opts->db->kptr == ' ') continue;
			n++;
		}
	}

	/*
	 * delete | get | count
	 */
	if (regexp) {
		assert(!h);
		unless (re = pcre_compile(regexp, 0, &perr, &poff, 0)) {
			fprintf(opts->fout, "ERROR-bad regexp %s\n", regexp);
			goto err;
		}
		switch (cmd) {
		    case INFO_DELETE:
		    case INFO_GET:
		    case INFO_COUNT:
			break;
		    default:
			fprintf(opts->fout,
			    "ERROR-bad command %d with regexp %s\n",
			    cmd, regexp);
			hash_free(h2);
			free(re);
			return;
		}
		unless (cmd == INFO_COUNT) h = hash_new(HASH_MEMHASH);
		EACH_HASH(opts->db) {
			if (*(char *)opts->db->kptr == ' ') continue;
			if (pcre_exec(re, 0,
				opts->db->kptr, strlen(opts->db->kptr),
				    0, 0, 0, 0)) continue;
			if (cmd == INFO_COUNT) {
				n++;
			} else {
				hash_store(h, opts->db->kptr, opts->db->klen,
					   "", 1);
			}
		}
		free(re);
	}

	if (cmd == INFO_COUNT) {
		fprintf(opts->fout, "OK-%d keys\n", n);
		hash_free(h2);
		return;
	}

	/*
	 * delete | get | insert | set
	 */
	EACH_HASH(h) {
		if (*(char *)h->kptr == ' ') continue;
		/* save original value from db */
		if (hash_fetch(opts->db, h->kptr, h->klen)) {
			hash_store(h2,
			    h->kptr, h->klen,
			    opts->db->vptr, opts->db->vlen);
		}
		switch (cmd) {
		    case INFO_DELETE:
			unless (hash_delete(opts->db, h->kptr, h->klen) == 0) {
				fprintf(opts->fout, "ERROR-delete %s failed\n",
				    (char *)h->kptr);
				goto err;
			}
			n++;
			break;

		    case INFO_GET:
			if (opts->db->vlen) n++;  /* XXX: when is vlen 0 ? */
			break;

		    case INFO_INSERT:
			unless (hash_insert(opts->db,
				h->kptr, h->klen, h->vptr, h->vlen)) {
				fprintf(opts->fout,
				    "ERROR-insert of %s failed\n",
				    (char *)h->kptr);
				goto err;
			}
			n++;
			break;

		    case INFO_INSERT_KEY:
			/*
			 * The key is like either of the following:
			 *   <user@host>|<path>|20141030184638
			 *   <user@host>|<path>|20141030184638|dd65b79936ebdde5
			 * where the latter is for a cset. Get a pointer to
			 * the date part.
			 */
			unless ((p = strchr(h->kptr, '|')) &&
				(date = strchr(p+1, '|')) &&
				strlen(date) >= 14) {
				fprintf(opts->fout,
					"ERROR-insert-key of %s (bad key)",
					(char *)h->kptr);
				goto err;
			}
			date++;
			/*
			 * Insert the key and if it's a dup then increment the
			 * date by one second and try again. Return the date
			 * fudge to the client.
			 */
			t = strtoul(h->vptr, 0, 16);
			fudge = 0;
			while (1) {
				sprintf(buf, "%lx", t);
				if (hash_insert(opts->db, h->kptr, h->klen,
						buf, strlen(buf)+1)) {
					fprintf(opts->fout, "OK-%d fudge\n",
						fudge);
					break;
				}
				inc_date(date);
				fudge++;
				t++;
			}
			/* Append to the on-disk version of the table. */
			if (opts->uniq_daemon) {
				uniqdb_append(opts, h->kptr, buf);
			}
			n++;
			break;

		    case INFO_SET:
			unless (hash_store(opts->db,
				h->kptr, h->klen, h->vptr, h->vlen)) {
				fprintf(opts->fout, "ERROR-set of %s failed\n",
				    (char *)h->kptr);
				goto err;
			}
			n++;
			break;
		}
	}
	switch (cmd) {
	    case INFO_INSERT:
		if (opts->log) fprintf(opts->logf, "insert\n");
	    case INFO_SET:
		if (opts->log) {
			unless (cmd == INFO_INSERT) fprintf(opts->logf,"set\n");
			hash_toStream(h, opts->logf);
			fprintf(opts->logf, "@\n");
		}
		fprintf(opts->fout, "OK-%d %s\n",
		    n, cmd == INFO_SET ? "set" : "inserted");
		break;
	    case INFO_DELETE:
		if (opts->log) {
			fprintf(opts->logf, "delete\n");
			hash_toStream(h, opts->logf);
			fprintf(opts->logf, "@\n");
		}
		fprintf(opts->fout, "OK-%d deleted\n", n);
		break;
	    case INFO_GET:
		fprintf(opts->fout, "OK-%d found\n", n);
		hash_toStream(h2, opts->fout);
		fprintf(opts->fout, "@\n");
		break;
	}
	hash_free(h2);
	return;

err:	/* restore any entry changed before error */
	EACH_HASH(h2) {
		hash_store(opts->db, h2->kptr, h2->klen, h2->vptr, h2->vlen);
	}
	hash_free(h2);
}

private int
db_open(Opts *opts)
{
	int	oflags;
	char	*path;

	if (opts->uniq_daemon) return (0);

	if (opts->db) hash_close(opts->db);
	path = aprintf("%s/db", opts->dir);
	opts->db = hash_open(HASH_MDBM, path, O_RDWR|O_CREAT, 0664);
	free(path);
	unless (opts->db) {
		fprintf(opts->fout, "ERROR-hash open of %s/db\n", opts->dir);
		return (1);
	}
	if (!hash_storeStr(opts->db, DB_VERSION, VERSION)) {
		fprintf(opts->fout, "ERROR-hash store version in %s/db\n",
			opts->dir);
err:		hash_close(opts->db);
		opts->db = 0;
		return (1);
	}
	if (opts->log) {
		oflags = O_WRONLY | O_APPEND | O_CREAT;
#ifdef	O_SYNC
		if (opts->do_sync) oflags |= O_SYNC;
#endif
		path = aprintf("%s/log", opts->dir);
		opts->logfd = open(path, oflags, 0664);
		free(path);
		unless (opts->logf = fdopen(opts->logfd, "a")) {
			fprintf(opts->fout, "ERROR-error opening %s/log\n",
				opts->dir);
			goto err;
		}
	}
	return (0);
}

private void
db_close(Opts *opts)
{
	if (opts->uniq_daemon) return;
	if (opts->log) {
		fclose(opts->logf);
		opts->logf = 0;
		close(opts->logfd);
		opts->logfd = -1;
	}
	hash_close(opts->db);
	opts->db = 0;
}

private char *
uniqdb_lock_path(void)
{
	char	*t;

	/* Check BK_DOTBK to support parallel regressions. */
	if (t = getenv("BK_DOTBK")) {
		return (aprintf("%s/bk-keys/%s.lock", t, sccs_realhost()));
	} else {
		return (aprintf("%s/.uniq_keys_%s", TMP_PATH,
				sccs_realuser()));
	}
}

private char *
uniqdb_backup_path(void)
{
	/* Check BK_DOTBK to support parallel regressions. */
	if (getenv("BK_DOTBK")) {
		return (0);
	} else {
		return (aprintf("%s/.bk-keys-%s", TMP_PATH,
				sccs_realuser()));
	}
}

/*
 * Acquire the pre-bk7 uniqdb lock. Timeout the lock a few times while
 * surfacing that info to the startsock (so the client can tell the
 * user), then bail after enough retries.
 */
private int
uniqdb_lock(Opts *opts)
{
	int	timeout = 5, tries_left = 3;
	char    *host, *lock;
	pid_t	pid;
	time_t	t;

	lock = uniqdb_lock_path();
	mkdirf(lock);
	while (tries_left--) {
		if (sccs_lockfile(lock, timeout, 1) == 0) {
			free(lock);
			return (0);
		}
		host = 0; pid = 0;
		sccs_readlockf(lock, &pid, &host, &t);
		startsock_printf(opts,
		    "WARNING-waiting for lock %s "
		    "(pid %d getpid %d findpid %d real %s host %s local %d)\n",
		    lock, pid, getpid(), findpid(pid), sccs_realhost(),
		    host ? host : "(unk)", isLocalHost(host));
		if (host) free(host);
		timeout *= 2;
	}
	free(lock);
	return (-1);
}

/* Release the pre-bk7 uniqdb lock. */
private int
uniqdb_unlock(void)
{
	char	*lock;
	int	rc;

	lock = uniqdb_lock_path();
	rc = sccs_unlockfile(lock);
	free(lock);
	return (rc);
}

/*
 * Open the uniqdb that we'll be writing to. There can be a primary
 * (in .bk) and a backup (in /tmp). When reading, both might be read,
 * but when writing, only one is used. Open that one and keep it open.
 */
private int
uniqdb_open(Opts *opts)
{
	int	ret = 0;
	char	*prim = aprintf("%s/bk-keys/%s", getDotBk(), sccs_realhost());
	char	*back = uniqdb_backup_path();

	if (opts->db_primf) fclose(opts->db_primf);
	if (opts->db_backf) fclose(opts->db_backf);
	opts->db_backf = 0;

	mkdirf(prim);
	if (opts->db_primf = fopen(prim, "a")) {
		T_DEBUG("opened primary %s", prim);
	} else if (back && (opts->db_backf = fopen(back, "a"))) {
		T_DEBUG("opened backup %s", back);
	} else {
		T_DEBUG("error %d opening uniqdb for writing", errno);
		ret = 1;
	}

	free(prim);
	free(back);
	return (ret);
}

private void
uniqdb_close(Opts *opts)
{
	if (opts->db_primf) {
		fclose(opts->db_primf);
		opts->db_primf = 0;
	}
	if (opts->db_backf) {
		fclose(opts->db_backf);
		opts->db_backf = 0;
	}
}

private void
uniqdb_sync(Opts *opts)
{
	if (opts->db_primf) fsync(fileno(opts->db_primf));
	if (opts->db_backf) fsync(fileno(opts->db_backf));
}

/*
 * Read the ascii uniqdb file into an in-memory hash. Most of this is
 * from the old unique.c.
 */
private void
uniqdb_load_file(hash *h, char *path)
{
	int	klen, pipes, vlen;
	time_t	t, t2;
	char	*kptr, *q, *s, *v2;
	char	buf[MAXPATH*2], vptr[32];
	FILE	*f;

	T_DEBUG("looking for uniqdb %s", path);
	unless (f = fopen(path, "r")) return;
	while (fnext(buf, f)) {
		/*
		 * expected format:
		 * user@host|path|date timet [syncRoot]
		 *
		 * Notes:
		 *   - bk-4.x only parses upto the timet
		 *   - ChangeSet files will always have path==ChangeSet
		 *     (no component pathnames)
		 *   - syncRoot only occurs on ChangeSet files and is
		 *     the random bits of the syncRoot key
		 *   - bk-4.x might warn and drop lines that differ by
		 *     only the syncRoot, but that still gives correct
		 *     behavior.
		 */
		for (pipes = 0, s = buf; *s; s++) {
			if (*s == '|') pipes++;
			if ((*s == ' ') && (pipes == 2)) break;
		}
		unless ((pipes == 2) &&
			s && isdigit(s[1]) && (chop(buf) == '\n')) {
			continue;
		}
		*s++ = 0;
		t = (time_t)strtoul(s, &s, 0);
		if (*s == ' ') {  /* more data after timestamp */
			++s;
			q = buf + strlen(buf);
			*q++ = '|';
			while (*s && (*s != '|')) *q++ = *s++;
			*q = 0;
		}
		kptr = buf;
		klen = strlen(buf) + 1;
		sprintf(vptr, "%lx", t);
		vlen = strlen(vptr) + 1;
		T_DEBUG("read %s %s", kptr, vptr);
		if (hash_insert(h, kptr, klen, vptr, vlen)) {
			v2 = hash_fetch(h, kptr, klen);
			t2 = (time_t)strtoul(v2, 0, 16);
			if (t > t2) hash_store(h, kptr, klen, vptr, vlen);
		}
	}
	fclose(f);
}

/*
 * Read the uniqdb into a hash. Add the DB_MODTIME key if not already
 * there with a time_t value of t - 1year where t is the last time
 * that bk6 touched its uniqdb.  Bk7 never purges this key but bk6
 * always will, so if it's not in the bk6 uniqdb, either bk7 has yet
 * to run or bk6 was the last to touch its db.
 */
private time_t
uniqdb_read(Opts *opts)
{
	char	buf[32], *s;
	time_t	t1, t2 = 0;

	opts->db = hash_new(HASH_MEMHASH);

	/* The main uniqdb. */
	s = aprintf("%s/bk-keys/%s", getDotBk(), sccs_realhost());
	uniqdb_load_file(opts->db, s);
	t1 = mtime(s);
	free(s);

	/* Possible backup uniqdb. */
	if (s = uniqdb_backup_path()) {
		uniqdb_load_file(opts->db, s);
		t2 = mtime(s);
		free(s);
	}

	if (s = hash_fetchStr(opts->db, DB_MODTIME)) {
		t1 = (time_t)strtoul(s, 0, 16);
		T_DEBUG("using DB_MODTIME %lx", t1);
	} else {
		if (t2 > t1) t1 = t2;
		if (t1) t1 -= 1*YEAR;
		sprintf(buf, "%lx", t1);
		hash_storeStr(opts->db, DB_MODTIME, buf);
		T_DEBUG("storing DB_MODTIME %s", buf);
	}
	return (t1 + 1*YEAR);
}

private void
uniqdb_write_rec(FILE *f, char *k, char *v)
{
	char	*date, *p, *syncroot;
	time_t	t;

	if (*k == ' ') return;  // hidden key
	unless ((p = strchr(k, '|')) && (date = strchr(p+1, '|'))) {
		return;  // can't happen? bad key
	}
	t = (time_t)strtoul(v, 0, 16);
	/*
	 * If present, extract the syncRoot part from the delta key
	 * since older bks expect lines like
	 *     user@host|path|date time_t [syncRoot]
	 */
	if (syncroot = strchr(date+1, '|')) {
		*syncroot = 0;
		T_DEBUG("writing %s %ld %s", k, t, syncroot+1);
		fprintf(f, "%s %ld %s\n", k, t, syncroot+1);
		*syncroot = '|';
	} else {
		T_DEBUG("writing %s %ld", k, t);
		fprintf(f, "%s %ld\n", k, t);
	}
}

/*
 * Write the uniqdb out to an ascii file.
 */
private int
uniqdb_write_file(Opts *opts, char *path)
{
	FILE	*f;

	mkdirf(path);
	unless (f = fopen(path, "w")) return (1);
	T_DEBUG("writing uniqdb to %s", path);
	EACH_HASH(opts->db) {
		uniqdb_write_rec(f, opts->db->kptr, opts->db->vptr);
	}
	return (fclose(f));
}

/*
 * Write out the in-memory uniqdb hash to an ascii file. If the
 * primary file location (.bk/bk-keys/HOST) is not writable,
 * write to the backup location (/tmp/.bk-keys-USER).
 */
private void
uniqdb_write(Opts *opts)
{
	char	*prim = aprintf("%s/bk-keys/%s", getDotBk(), sccs_realhost());
	char	*back = uniqdb_backup_path();

	unless (uniqdb_write_file(opts, prim)) {
		if (back) unlink(back);
	} else if (back) {
		uniqdb_write_file(opts, back);
	}
	free(prim);
	free(back);
}

private void
uniqdb_append(Opts *opts, char *kptr, char *vptr)
{
	FILE	*f;

	unless ((f = opts->db_primf) || (f = opts->db_primf)) return;
	T_DEBUG("appending %s %s", kptr, vptr);
	uniqdb_write_rec(f, kptr, vptr);
}

/*
 * Contact the running info server and ask it to exit and wait for its
 * acknowledgement. Return 0 on success or if the server wasn't
 * running to begin with, and 1 if we don't get the ack handshake.
 */
private int
quit(int quiet)
{
	int	ret;
	size_t	resplen;
	char	msg[] = "exit\n";
	char	resp[64], *out, *s;
	FILE	*fin;

	T_DEBUG("sending exit request");
	resplen = sizeof(resp);
	if (uniqdb_req(msg, sizeof(msg), resp, &resplen)) return (0);
	fin = fmem_buf(resp, resplen);
	if ((s = fgetline(fin)) && strneq(s, "OK", 2)) {
		out = aprintf("uniq_server stopped");
		ret = 0;
	} else {
		out = aprintf("error stopping uniq_server: %s", s);
		ret = 1;
	}
	fclose(fin);
	unless (quiet) printf("%s\n", out);
	T_DEBUG("%s", out);
	free(out);
	return (ret);
}

/*
 * Return a monotonically increasing integer.
 */
private	void
unique(Opts *opts, char *arg)
{
	u32	u = 0;

	u = hash_fetchStrNum(opts->db, DB_UNIQUE);
	u++;
	if (arg) u = atoi(arg);
	fprintf(opts->fout, "OK-%u\n", u);
	if (opts->log) fprintf(opts->logf, "unique %u\n", u);
	hash_storeStrNum(opts->db, DB_UNIQUE, u);
}

private	void
version(hash *db, FILE *fout)
{
	char	*v = hash_fetchStr(db, DB_VERSION);

	if (v) {
		fprintf(fout, "OK-stored version=%s\n", v);
	} else {
		fprintf(fout, "ERROR-no stored version\n");
	}
}

/*
 * Prune old keys from the uniq db. Done only when invoked as uniq_server.
 */
private void
uniqdb_prune(hash *db)
{
	int	i;
	char	**todelete = 0;
	time_t	cutoff, val;

	/* delete keys w/values < cutoff */
	cutoff = time(0) - uniq_drift();
	T_DEBUG("pruning keys, now=%ld cutoff=%ld (%ld drift)",
		cutoff+uniq_drift(), cutoff, uniq_drift());
	EACH_HASH(db) {
		if (*(char *)db->kptr == ' ') continue;  // hidden key
		if (streq(db->kptr, DB_MODTIME)) continue; // never purge this
		val = (time_t)strtoul(db->vptr, 0, 16);
		if (val < cutoff) {
			T_DEBUG("pruning %s (%ld)",
			    (char *)db->kptr, cutoff-val);
			todelete = addLine(todelete, strdup(db->kptr));
		} else {
			T_DEBUG("keeping %s (%ld)",
			    (char *)db->kptr, cutoff-val);
		}
	}
	EACH(todelete) hash_deleteStr(db, todelete[i]);
	freeLines(todelete, free);
}

private time_t
uniq_drift(void)
{
	static	time_t t = (time_t)-1;
	char	*drift;

	if (t != (time_t)-1) return (t);
	if (drift = getenv("CLOCK_DRIFT")) {
		t = atoi(drift);
	} else {
		t = CLOCK_DRIFT;
	}
	return (max(t, 120));
}

private void
startsock_printf(Opts *opts, const char *fmt, ...)
{
	char	*buf;
	va_list	ap;

	unless (opts->startsock_port) return;
	unless (opts->startsock_sock > 0) {
		T_DEBUG("startsock connecting port %d", opts->startsock_port);
		opts->startsock_sock = tcp_connect("127.0.0.1",
						   opts->startsock_port);
		if (opts->startsock_sock < 0) {
			T_DEBUG("startsock connect error %d: %s\n", errno,
				strerror(errno));
			return;
		}
	}

	va_start(ap, fmt);
	if (vasprintf(&buf, fmt, ap) < 0) buf = 0;
	va_end(ap);
	unless (buf) return;
	write(opts->startsock_sock, buf, strlen(buf));
	free(buf);
}

private void
startsock_close(Opts *opts)
{
	if (opts->startsock_sock > 0) {
		closesocket(opts->startsock_sock);
		opts->startsock_sock = -1;
	}
}

/* Increment in-place by one second a string holding a date YYYYMMDDHHMMSS. */
private void
inc_date(char *p)
{
	char	save;
	struct	tm *tp;

	tp = utc2tm(sccs_date2time(p, 0) + 1);
	/* sprintf adds a terminating nul, but we don't want it */
	save = p[14];
	sprintf(p, "%4d%02d%02d%02d%02d%02d",
		tp->tm_year + 1900,
		tp->tm_mon + 1,
		tp->tm_mday,
		tp->tm_hour,
		tp->tm_min,
		tp->tm_sec);
	p[14] = save;
}

/* Helper for test & debug of race conditions: do a sleep. */
private void
debug_sleep(char *env)
{
	char		*s;
	unsigned short	r = 0;
	time_t		t;

	if (s = getenv(env)) {
		if (*s == 'r') {
			rand_getBytes((void *)&r, 2);
			t = (float)r/65536.0 * atoi(s+1);
		} else {
			t = atoi(s);
		}
		T_DEBUG("debug usleep %ld", t);
		usleep(t);
	}
}
