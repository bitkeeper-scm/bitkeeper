/*
 * Simple TCP server.
 */
#include "bkd.h"
#include "cmd.h"

int
cmd_eof(int ac, char **av)
{
	out("OK-Goodbye\n");
	exit(0);
	return (0);	/* lint */
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

	unless (av[1]) return (1);
	unless (p = strchr(av[1], '=')) return (1);

	var = strdup_tochar(av[1], '=');
	/*
	 * We also disallow anything not starting with one of
	 * _BK_, BK_, or BKD_.  Not sure we need the BKD_, but hey.
	 * Also disable BK_HOST, this screws up the locks.
	 */
	if (streq("BK_SEED_OK", var)) return (1);
	if (streq("BK_NO_TRIGGERS", var)) return (1);
	if (streq("BK_HOST", var)) return (1);
	unless (strneq("BK_", var, 3) || strneq("BKU_", var, 4) ||
	    strneq("BKD_", var, 4) || strneq("_BK_", var, 4)) {
	    	return (1);
	}
	oldenv = getenv(var);
	unless (oldenv && streq(oldenv, p+1)) {
		if (streq(var, "_BK_USER")) {
			sccs_resetuser();
			putenv(&av[1][1]);	/* convert to BK_USER */
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

	/*
	 * Handle BK_SEED processing
	 */
	if (streq(var, "BK_SEED")) {
		char	*seed, *oldseed = 0, *newseed;
		int	i;

		seed = av[1] + 8;
		if (strchr(seed, '|')) {
			oldseed = bkd_restoreSeed(getenv("BK_REPOID"));
		}
		i = bkd_seed(oldseed, seed, &newseed);
		if (oldseed) {
			/* in part2 */
			safe_putenv("BK_SEED_OK=%d", i);
		} else {
			bkd_saveSeed(getenv("BK_REPOID"), newseed);
		}
		safe_putenv("BKD_SEED=%s", newseed);
	}
	free(var);

	return (0);
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
	status = system("bk -r check -a 2>&1");
	rc = WEXITSTATUS(status); 
	fputc(BKD_NUL, stdout);
	fputc('\n', stdout);
	if (rc) printf("%c%d\n", BKD_RC, rc);
	fflush(stdout);
	out("@END@\n");
	return (rc);
}

#define GZ_FROMREMOTE	1	/* ungzip the stdin we get */
#define GZ_TOREMOTE	2	/* gzip the stdout we send back */

int
cmd_bk(int ac, char **av)
{
	int	status, fd, i, bytes = 0;
	int	gzip = GZ_FROMREMOTE|GZ_TOREMOTE;
	char	*tmp = 0;
	int	rc = 1;
	int	oldfd0, oldfd1, oldfd2;
	int	fd1, fd2;
	int	p[2];
	pid_t	pid;
	fd_set	rfds;
	CMD	*cmd;
	zgetbuf	*zin = 0;
	zputbuf *zout = 0;
	char	*line;
	char	hdr[64];
	char	buf[8192];	/* must match remote.c:doit()/buf */

	for (i = 1; av[i]; i++) {
		if (av[i][0] != '-') break;
		if (streq(av[i], "-z0")) gzip = 0;
		if (streq(av[i], "-zi0")) gzip &= ~GZ_FROMREMOTE;
		if (streq(av[i], "-zo0")) gzip &= ~GZ_TOREMOTE;
	}
	if (line = getenv("_BK_REMOTEGZIP")) gzip = atoi(line);
	if (gzip & GZ_FROMREMOTE) zin = zgets_initCustom(0, stdin);
	if (gzip & GZ_TOREMOTE) zout = zputs_init(0, stdout);

	/*
	 * We only allow remote commands if the bkd told us that was OK.
	 */
	unless (Opts.unsafe) {
		for (i = 1; av[i] && (av[i][0] == '-'); i++);
		unless (av[i] &&
		    (cmd = cmd_lookup(av[i], strlen(av[i]))) &&
		    cmd->remote) {
			strcpy(buf,
			    "ERROR-remote commands are not enabled.\n");
err:			if (zout) {
				zputs(zout, buf, strlen(buf));
			} else {
				fputs(buf, stdout);
			}
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
	unless (isdir(BKROOT)) {
		strcpy(buf, "ERROR-not at repository root\n");
		goto err;
	}
	putenv("BK_NO_CMD_FALL_THROUGH=1");
	putenv("BKD_DAEMON=");	/* allow new bkd connections */

	setmode(0, _O_BINARY);
	tmp = bktmp(0, "stdin");
	fd = open(tmp, O_CREAT|O_RDWR, 0600);
	assert(fd != -1);
	while (1) {
		if (zin) {
			line = zgets(zin);
		} else {
			line = fnext(buf, stdin) ? buf : 0;
		}
		unless (line) {
			strcpy(buf, "ERROR-no stdin information\n");
			close(fd);
			goto err;
		}
		unless (sscanf(line, "@STDIN=%u@", &bytes) == 1) {
			strcpy(buf, "ERROR-bad input\n");
			close(fd);
			goto err;
		}
		unless (bytes) break;
		while (1) {
			i = min(sizeof(buf), bytes);
			if (zin) {
				i = zread(zin, buf, i);
			} else {
				i = fread(buf, 1, i, stdin);
			}
			if (i <= 0) break;
			writen(fd, buf, i);
			bytes -= i;
		}
		if (bytes) {
			strcpy(buf, "ERROR-input truncated\n");
			close(fd);
			goto err;
		}
	}
	lseek(fd, 0, SEEK_SET);
	/* setup stdin */
	oldfd0 = dup(0);
	dup2(fd, 0);
	close(fd);

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

	while (fd1 || fd2) {
		FD_ZERO(&rfds);
		if (fd1) FD_SET(fd1, &rfds);
		if (fd2) FD_SET(fd2, &rfds);
		if (select(max(fd1, fd2)+1, &rfds, 0, 0, 0) < 0) break;
		if (FD_ISSET(fd1, &rfds)) {
			if ((i = read(fd1, buf, sizeof(buf))) > 0) {
				sprintf(hdr, "@STDOUT=%u@\n", i);
				if (zout) {
					zputs(zout, hdr, strlen(hdr));
					zputs(zout, buf, i);
				} else {
					fputs(hdr, stdout);
					fwrite(buf, 1, i, stdout);
				}
			} else {
				close(fd1);
				fd1 = 0;
			}
		}
		if (FD_ISSET(fd2, &rfds)) {
			if ((i = read(fd2, buf, sizeof(buf))) > 0) {
				sprintf(hdr, "@STDERR=%u@\n", i);
				if (zout) {
					zputs(zout, hdr, strlen(hdr));
					zputs(zout, buf, i);
				} else {
					fputs(hdr, stdout);
					fwrite(buf, 1, i, stdout);
				}
			} else {
				close(fd2);
				fd2 = 0;
			}
		}
	}
	if ((waitpid(pid, &status, 0) > 0) && WIFEXITED(status)) {
		rc = WEXITSTATUS(status);
	} else {
		rc = 3;
	}
	sprintf(hdr, "@EXIT=%d@\n", rc);
	if (zout) {
		zputs(zout, hdr, strlen(hdr));
	} else {
		fputs(hdr, stdout);
	}
out:	if (zin) zgets_done(zin);
	if (zout) zputs_done(zout);
	fflush(stdout);
	if (tmp) {
		unlink(tmp);
		free(tmp);
	}
	/* 
	 * For some reason when bk is run from a service on Windows we
 	 * these shutdowns to flush all data that is written to the 
	 * sockets.  This is not fully understood.
	 */
	shutdown(1, 1);
	shutdown(2, 1);
	return (rc);
}

/*
 * A useful debugging function.  It can be called remotely so it
 * must be secure.
 */
int
debugargs_main(int ac, char **av)
{
	int	i;

	for (i = 0; av[i]; i++) {
		printf("%d: %s\n", i, shellquote(av[i]));
	}
	return (0);
}
