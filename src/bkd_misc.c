/*
 * Copyright 1999-2013,2016 BitMover, Inc
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

#include "bkd.h"
#include "cmd.h"

int
cmd_quit(int ac, char **av)
{
	out("OK-Goodbye\n");
	return (0);
}

int
cmd_help(int ac, char **av)
{
	int	i;

	for (i = 0; cmds[i].name; i++) {
		out(cmds[i].name);
		out(" - ");
		out(cmds[i].description);
		out("\n");
	}
	return (0);
}

char *
vpath_translate(char *path)
{
	char	*vhost;
	char	buf[MAXPATH];
	char	*s;
	char	*t;

	unless (path && (vhost = getenv("BK_VHOST"))) return (strdup("."));
	s = path;
	t = buf;
	while (*s) {
		if (*s != '%') {
			*t++ = *s++;
		} else {
			char	*p;
			s++;
			switch (*s) {
			    case '%': *t++ = '%'; break;
			    case 'c': *t++ = vhost[0]; break;
			    case 'h':
				p = vhost;
				while (*p && *p != '.') *t++ = *p++;
				break;
			    case 'H':
				p = vhost;
				while (*p) *t++ = *p++;
				break;
			    default:
				fprintf(stderr,
				    "Unknown escape %%%c in -V path\n", *s);
				exit(1);
			}
			s++;
		}
	}
	*t = 0;

	return (strdup(buf));
}

int
cmd_putenv(int ac, char **av)
{
	char	*p;
	char	*oldenv;
	char	*var;
	int	ret = 1;

	unless (av[1]) return (1);
	unless (p = strchr(av[1], '=')) return (1);

	var = strdup_tochar(av[1], '=');
	/*
	 * We also disallow anything not starting with one of
	 * _BK_, BK_, or BKD_.  Not sure we need the BKD_, but hey.
	 * Also disable BK_HOST, this screws up the locks.
	 */
	if (streq("BK_NO_TRIGGERS", var)) goto out;
	if (streq("BK_HOST", var)) goto out;
	unless (strneq("BK_", var, 3) || strneq("BKU_", var, 4) ||
	    strneq("BKD_", var, 4) || strneq("_BK_", var, 4)) {
	    	goto out;
	}
	oldenv = getenv(var);
	unless (oldenv && streq(oldenv, p+1)) {
		if (streq(var, "_BK_USER")) {
			sccs_resetuser();
			putenv(&av[1][1]);	/* convert to BK_USER */
		} else if (streq(var, "BK_NESTED_LOCK")){
			/* convert to _BK_NESTED_LOCK */
			safe_putenv("_%s", av[1]);
		} else {
			putenv(av[1]);
		}

		if (streq(var, "BK_VHOST")) {
			/*
			 * Lookup new vhost, do path translation and cd to new
			 * bkd root.  only do this once!
			 */
			char	*newpath = vpath_translate(Opts.vhost_dirpath);
			chdir(newpath);
			free(newpath);
		}
	}

	ret = 0;
out:	free(var);
	return (ret);
}

int
cmd_abort(int ac, char **av)
{
	int	status, rc;

	out("@ABORT INFO@\n");
	status = system("bk abort -f 2>&1");
	rc = WEXITSTATUS(status);
	fputc(BKD_NUL, stdout);
	fputc('\n', stdout);
	if (rc) printf("%c%d\n", BKD_RC, rc);
	fflush(stdout);
	out("@END@\n");
	return (rc);
}

int
cmd_check(int ac, char **av)
{
	int	status, rc;
	
	out("@CHECK INFO@\n");
	status = system("bk -r check -ac 2>&1");
	rc = WEXITSTATUS(status); 
	fputc(BKD_NUL, stdout);
	fputc('\n', stdout);
	if (rc) printf("%c%d\n", BKD_RC, rc);
	fflush(stdout);
	out("@END@\n");
	return (rc);
}

int
checked_main(int ac, char **av)
{
	unless (av[1] && !av[2]) {
		fprintf(stderr, "usage: checked <tiprev>\n");
		return (1);
	}
	if (proj_cd2root()) {
		fprintf(stderr, "Not in a repository\n");
		return (1);
	}

	if (streq(av[1], proj_tipkey(0)) ||
	    streq(av[1], proj_tipmd5key(0)) ||
	    streq(av[1], proj_tiprev(0))) {
		touch_checked();
	}
	return (0);
}

/*
 * Client side of cmd_checked()
 * XXX - this is bogus if we haven't updated the tip but somewhat harmless,
 * the other side won't match.  But it makes this useless if that is the case.
 * Since bk check updates it we should be fine.
 */
int
remote_checked(char *url)
{
	char	*at;
	char	**tips;
	int	rc;

	// I don't care about errors and
	// this needed to be sync or the repo disappears in regressions.
	unless (tips = file2Lines(0, "BitKeeper/log/TIP")) return (1);
	at = aprintf("-@%s", url);
	rc = sysio(0, DEVNULL_WR, DEVNULL_WR, "bk", at, "checked", tips[1],SYS);
	free(at);
	freeLines(tips, free);
	return (rc);
}

#define GZ_FROMREMOTE	1	/* ungzip the stdin we get */
#define GZ_TOREMOTE	2	/* gzip the stdout we send back */

int
cmd_bk(int ac, char **av)
{
	int	zero, bits, status, i, bytes = 0, rc = 1;
	int	gzip = GZ_FROMREMOTE|GZ_TOREMOTE;
	int	fd0, fd1, fd2, oldfd0, oldfd1, oldfd2;
	int	p[2];
	pid_t	pid;
	fd_set	rfds, wfds;
	CMD     *cmd = 0;
	FILE	*zin = 0, *fsave = 0;
	FILE	*zout = stdout;
	char    *line, *wnext;
	int     wtodo = 0;
	char	buf[BSIZE];	/* must match remote.c:doit()/buf */
	char	wbuf[BSIZE];

	for (i = 1; av[i]; i++) {
		if (av[i][0] != '-') break;
		if (streq(av[i], "-z0")) gzip = 0;
		if (streq(av[i], "-zi0")) gzip &= ~GZ_FROMREMOTE;
		if (streq(av[i], "-zo0")) gzip &= ~GZ_TOREMOTE;
	}
	if (line = getenv("_BK_REMOTEGZIP")) gzip = atoi(line);
	if (gzip & GZ_FROMREMOTE) {
		/* this read needs to be unbuffered below... */
		assert(!Opts.use_stdio);
		fsave = fdopen(dup(0), "rb");
		zin = fopen_zip(fsave, "rhu");
	}
	if (gzip & GZ_TOREMOTE) {
		fputs("@GZIP@\n", stdout);
		zout = fopen_zip(stdout, "wh", -1);
	}

	/*
	 * We only allow remote commands if the bkd told us that was OK.
	 */
	for (i = 1; av[i] && (av[i][0] == '-'); i++);
	if (av[i]) cmd = cmd_lookup(av[i], strlen(av[i]));
	unless (Opts.unsafe) {
		unless (cmd && cmd->remote) {
			fprintf(zout, "ERROR-remote commands are "
			    "not enabled (cmd[%d] = %s; cmd =", i, av[i]);
			for (i = 0; av[i]; i++) {
				fprintf(zout, " %s", av[i]);
			}
			fprintf(zout, ").\n");
			goto out;
		}
	}

	/*
	 * Don't give them a huge hole through which to walk
	 * By the time we get here we need to be at a repo root and we
	 * need to insure that we don't open files not in this repo.
	 * XXX - probably don't need this and probably should remove
	 * after review.
	 */
	unless (isdir(BKROOT) || (cmd && streq(cmd->name, "version"))) {
		fprintf(zout, "ERROR-not at repository root\n");
		goto out;
	}
	putenv("_BK_VIA_REMOTE=1");
	putenv("BKD_DAEMON=");	/* allow new bkd connections */

	setmode(0, _O_BINARY);

	/* set up a pipe to their stdin */
	tcp_pair(p);
	oldfd0 = dup(0);
	dup2(p[0], 0);
	close(p[0]);
	make_fd_uninheritable(p[1]);
	fd0 = p[1];

	/* read stdout from a pipe */
	tcp_pair(p);
	oldfd1 = dup(1);
	dup2(p[1], 1);
	close(p[1]);
	make_fd_uninheritable(p[0]);
	fd1 = p[0];

	/* read stderr from a pipe */
	tcp_pair(p);
	oldfd2 = dup(2);
	dup2(p[1], 2);
	close(p[1]);
	make_fd_uninheritable(p[0]);
	fd2 = p[0];

	pid = spawnvp(P_NOWAIT, av[0], av);
	dup2(oldfd0, 0);
	close(oldfd0);
	dup2(oldfd1, 1);
	close(oldfd1);
	dup2(oldfd2, 2);
	close(oldfd2);

	bits = max(fd1, fd2) + 1;
	zero = 1;
	wtodo = 0;
	wnext = 0;
	while (zero || fd1 || fd2) {
		if (zero && !wtodo) {
			if (zin) {
				line = fgetline(zin);
			} else {
				line = buf;
				if (getline(0, buf, sizeof(buf)) <= 0) line = 0;
			}
			unless (line) {
				fprintf(zout, "ERROR-no stdin information\n");
				goto out;
			}
			unless (sscanf(line, "@STDIN=%u@", &bytes) == 1) {
				fprintf(zout, "ERROR-bad input\n");
				goto out;
			}
			unless (bytes) {
				zero = 0;
				close(fd0);
				fd0 = 0;
				continue;
			}
			assert(bytes <= sizeof(wbuf));
			if (zin) {
				i = fread(wbuf, 1, bytes, zin);
			} else {
				i = readn(0, wbuf, bytes);
			}
			if (i < bytes) {
				fprintf(zout, "ERROR-input truncated\n");
				goto out;
			}
			wnext = wbuf;
			wtodo = bytes;
		}
		unless (fd1 || fd2) {
			/*
			 * bk is done, but we still need to consume stdin
			 * XXX this is kinda busted as the user might transfer a
			 *     bunch of data only to be given an error message.
			 *     Better for remote.c to duplex reading and writing so
			 *     it won't deadlock when the bkd sends and error and
			 *     stops reading.
			 */
			wtodo = 0;
			continue;
		}
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		if (wtodo) FD_SET(fd0, &wfds);
		if (fd1) FD_SET(fd1, &rfds);
		if (fd2) FD_SET(fd2, &rfds);
		if (select(bits, &rfds, &wfds, 0, 0) < 0) {
			perror("select");
died:			if (fd1) {
				close(fd1);
				fd1 = 0;
			}
			if (fd2) {
				close(fd2);
				fd2 = 0;
			}
			continue;
		}
		if (wtodo && FD_ISSET(fd0, &wfds)) {
			if ((i = write(fd0, wnext, wtodo)) > 0) {
				wtodo -= i;
				wnext += i;
			} else {
				perror("write");
				/*
				 * we failed to write to the child process
				 * so we are going to assume it died and
				 * follow the same path that select does
				 * at this point.
				 */
				goto died;
			}
		}
		if (FD_ISSET(fd1, &rfds)) {
			if ((i = read(fd1, buf, sizeof(buf))) > 0) {
				fprintf(zout, "@STDOUT=%u@\n", i);
				if (fwrite(buf, 1, i, zout) != i) {
					perror("fwrite");
					goto died;
				}
			} else {
				close(fd1);
				fd1 = 0;
			}
		}
		if (FD_ISSET(fd2, &rfds)) {
			if ((i = read(fd2, buf, sizeof(buf))) > 0) {
				fprintf(zout, "@STDERR=%u@\n", i);
				if (fwrite(buf, 1, i, zout) != i) {
					perror("fwrite");
					goto died;
				}
			} else {
				close(fd2);
				fd2 = 0;
			}
		}
	}
	/* make sure we are not hold any connections to child */
	if (fd0) close(fd0);
	if (fd1) close(fd1);
	if (fd2) close(fd2);
	if ((waitpid(pid, &status, 0) > 0) && WIFEXITED(status)) {
		rc = WEXITSTATUS(status);
	} else {
		rc = 3;
	}
	fprintf(zout, "@EXIT=%d@\n", rc);
out:	if (zin) {
		fclose(zin);
		fclose(fsave);
	}
	if (gzip & GZ_TOREMOTE) fclose(zout);
	return (rc);
}

int
cmd_rdlock(int ac, char **av)
{
	return (0);
}

int
cmd_rdunlock(int ac, char **av)
{
	return (0);
}

int
cmd_wrlock(int ac, char **av)
{
	return (0);
}

int
cmd_wrunlock(int ac, char **av)
{
	return (0);
}

/*
 * A useful debugging function.  It can be called remotely so it
 * must be secure.
 */
int
debugargs_main(int ac, char **av)
{
	int	i, n;
	char	*t;

	for (i = 0; av[i]; i++) {
		printf("%d: %s\n", i, shellquote(av[i]));
	}
	printf("cwd: %s\n", proj_cwd());
	if (start_cwd) printf("start_cwd: %s\n", start_cwd);
	if ((i > 1) && streq(av[i-1], "-")) {
		printf("stdin:\n");
		if (t = fgetline(stdin)) printf("%s\n", t);
		for (n = 0; fgetline(stdin); n++);
		if (n) printf("... %d more lines ...\n", n);
	}
	return (0);
}
