/*
 * Copyright (c) 2000, Andrew Chang & Larry McVoy
 */    
#include "bkd.h"

typedef	struct {
	u32	automerge:1;		/* -i: turn off automerge */
	u32	list:1;			/* -l: long listing */
	u32	dont:1;			/* -n: do not actually do it */
	u32	quiet:1;		/* -q: shut up */
	u32	metaOnly:1;		/* -e empty patch */
	u32	noresolve:1;		/* -r: don't run resolve at all */
	u32	textOnly:1;		/* -t: don't pass -t to resolve */
	u32	debug:1;		/* -d: debug */
	int	gzip;			/* -z[level] compression */
	u32	in, out;		/* stats */
} opts;

private int	pull(char **av, opts opts, remote *r, char **envVar);
private	int	resolve(opts opts, remote *r);
private	int	takepatch(opts opts, int gzip, remote *r);

int
pull_main(int ac, char **av)
{
	int	c, rc, i = 1;
	int	try = -1; /* retry forever */
	opts	opts;
	remote	*r;
	char	**envVar = 0;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help pull");
		return (0);
	}
	bzero(&opts, sizeof(opts));
	opts.gzip = 6;
	opts.automerge = 1;
	while ((c = getopt(ac, av, "c:deE:ilnqrtz|")) != -1) {
		switch (c) {
		    case 'i': opts.automerge = 0; break;
		    case 'l': opts.list = 1; break;
		    case 'n': opts.dont = 1; break;
		    case 'q': opts.quiet = 1; break;
		    case 'r': opts.noresolve = 1; break;
		    case 't': opts.textOnly = 1; break;
		    case 'd': opts.debug = 1; break;
		    case 'e': opts.metaOnly = 1; break;
		    case 'E': envVar = addLine(envVar, strdup(optarg)); break;
		    case 'c': try = atoi(optarg); break;
		    case 'z':
			opts.gzip = optarg ? atoi(optarg) : 6;
			if (opts.gzip < 0 || opts.gzip > 9) opts.gzip = 6;
			break;
		    default:
usage:			system("bk help -s push");
			return (1);
		}
	}

	loadNetLib();
	r = remote_parse(av[optind], 0);
	unless (r) goto usage;
	if (opts.debug) r->trace = 1;
	for (;;) {
		rc = pull(av, opts, r, envVar);
		if (rc != 2) break;
		if (try == 0) break;
		if (bk_mode() == BK_BASIC) {
			if (try > 0) {
				fprintf(stderr,
			    "pull: retry request detected: %s", upgrade_msg);
			}
			break;
		}
		if (try != -1) --try;
		unless(opts.quiet) {
			fprintf(stderr,
				"pull: remote locked, trying again...\n");
		}
		sleep(min((i++ * 2), 10));
	}
	if (rc == 2) rc = 1; /* if retry failed, rest exit code to 1 */
	remote_free(r);
	freeLines(envVar);
	return (rc);
}


private int
send_part1_msg(opts opts, remote *r, char probe_list[], char **envVar)
{
	char	buf[MAXPATH];
	FILE    *f;
	int	rc;

	bktemp(buf);
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "pull_part1");
	if (opts.gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.metaOnly) fprintf(f, " -e");
	if (opts.dont) fprintf(f, " -n");
	if (opts.list) fprintf(f, " -l");
	if (opts.quiet) fprintf(f, " -q");
	if (opts.debug) fprintf(f, " -d");
	fputs("\n", f);
	fclose(f);

	rc = send_file(r, buf, 0, opts.gzip);	
	unlink(buf);
	return (rc);
}

private int
pull_part1(opts opts, remote *r, char probe_list[], char **envVar)
{
	char	buf[MAXPATH];
	int	n, fd;

	if (send_part1_msg(opts, r, probe_list, envVar)) return (-1);

	if (r->httpd) skip_http_hdr(r);
	if (getline2(r, buf, sizeof (buf)) <= 0) return (-1);
	if (streq(buf, "ERROR-Can't get read lock on the repository")) {
		if (!opts.quiet) fprintf(stderr, "%s\n", buf);
		return (-1);
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
		getline2(r, buf, sizeof(buf));
	} else {
#ifdef BKD_VERSION1_2
		drain_bkd1_2_msg(r, buf, sizeof(buf));
#endif
	}
	if (get_ok(r, buf, !opts.quiet)) {
		disconnect(r, 2);
		return (1);
	}
	bktemp(probe_list);
	fd = open(probe_list, O_CREAT|O_WRONLY, 0644);
	assert(fd >= 0);
	if (opts.gzip) gzip_init(opts.gzip);
	while ((n = getline2(r, buf, sizeof(buf))) > 0) {
		write(fd, buf, n);
		write(fd, "\n", 1);
		if (streq("@END@", buf)) break;
	}
	if (opts.gzip) gzip_done();
	close(fd);
	if (r->httpd) disconnect(r, 2);
	return (0);
}

private int
send_keys_msg(opts opts, remote *r, char probe_list[], char **envVar)
{
	char	msg_file[MAXPATH], buf[MAXPATH * 2];
	FILE	*f;
	MMAP    *m;
	int	status, rc;

	bktemp(msg_file);
	f = fopen(msg_file, "w");
	assert(f);
	sendEnv(f, envVar);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "pull_part2");
	if (opts.gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.metaOnly) fprintf(f, " -e");
	if (opts.dont) fprintf(f, " -n");
	if (opts.list) fprintf(f, " -l");
	if (opts.quiet) fprintf(f, " -q");
	if (opts.debug) fprintf(f, " -d");
	fputs("\n", f);
	fclose(f);

	sprintf(buf, "bk _listkey -q < %s >> %s", probe_list, msg_file);
	status = system(buf); 
	rc = WEXITSTATUS(status);
	if (opts.debug) fprintf(stderr, "listkey returned %d\n", rc);
	switch (rc) {
	    case 0:	break;
	    case 1:	fprintf(stderr,
			    "You are trying to pull from an unrelated package\n"
			    "Please check the pathnames and try again.\n");
			break;
	    default:	unlink(msg_file);
			return (-1);
	}

	rc = send_file(r, msg_file, 0, opts.gzip);	
	unlink(msg_file);
	return (rc);
}

int
pull_part2(char **av, opts opts, remote *r, char probe_list[], char **envVar)
{
	char	buf[MAXPATH * 2];
	int	rc = 0, n, i;

	if (send_keys_msg(opts, r, probe_list, envVar)) {
		rc = 1;
		goto done;
	}

	if (r->httpd) skip_http_hdr(r);
	getline2(r, buf, sizeof (buf));
	if (streq(buf, "ERROR-Can't get read lock on the repository")) {
		if (!opts.quiet) fprintf(stderr, "%s\n", buf);
		return (-1);
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
		getline2(r, buf, sizeof(buf));
	}
	if (get_ok(r, buf, !opts.quiet)) {
		rc = 1;
		goto done;
	}

	/*
	 * Read the verbose status if we asked for it
	 */
	getline2(r, buf, sizeof(buf));
	if (streq(buf, "@REV LIST@")) {
		i = n = 0;
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (streq(buf, "@END@")) break;
			unless (i++) {
				if (opts.dont) {
					fprintf(stderr, "%s\n",
"---------------------- Would send the following csets ----------------------");
				} else {
					fprintf(stderr, "%s\n",
"----------------------- Sending the following csets ------------------------");
				}
			}
			n += strlen(buf) + 1;
			fputs(&buf[1], stderr);
			if (n > 72) {
				n = 0;
				fputs("\n", stderr);
			} else {
				fputs(" ", stderr);
			}
		}
		if (i) fputs("\n", stderr);

		getline2(r, buf, sizeof(buf));
		if (streq(buf, "@CHANGE LIST@")) {
			while (getline2(r, buf, sizeof(buf)) > 0) {
				if (streq(buf, "@END@")) break;
				fprintf(stderr, "%s\n", &buf[1]);
			}
			getline2(r, buf, sizeof(buf));
		}
		if (i) {
			fprintf(stderr, "%s\n",
"----------------------------------------------------------------------------");
		} else {
			fprintf(stderr, "Nothing to pull.\n");
		}
	}

	/*
	 * check remote trigger
	 */
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, !opts.quiet)) {
			rc = 1;
			goto done;
		}
		getline2(r, buf, sizeof (buf));
	}

	if (opts.dont) {
		unlink(probe_list);
		rc = 0;
		goto done;
	}

	if (streq(buf, "@PATCH@")) {
		if (takepatch(opts, opts.gzip, r)) {
			rc = 1;
			goto done;
		}
		/*
		 * We are about to run resolve, fire pre trigger
		 */
		if (!opts.metaOnly && trigger(av, "pre")) {
			rc = 1;
			goto done;
		}
		unless (opts.noresolve) {
			if (resolve(opts, r)) {
				rc = 1;
				goto done;
			}
		}
		rc = 0;
		putenv("BK_INCOMING=OK");
	}  else if (streq(buf, "@NOTHING TO SEND@")) {
		putenv("BK_INCOMING=NOTHING");
		rc = 0;
	} else {
		fprintf(stderr, "protocol error: <%s>\n", buf);
		while (getline2(r, buf, sizeof(buf)) > 0) {
			fprintf(stderr, "protocol error: <%s>\n", buf);
		}
		rc = 1;
	}

done:	if (rc) putenv("BK_INCOMING=CONFLICT");
	unless (opts.metaOnly) trigger(av, "post");
	unlink(probe_list);
	/*
	 * Wait for remote to disconnect
	 * This is important when trigger/error condition 
	 * short circuit the code path
	 */
	wait_eof(r, opts.debug);
	disconnect(r, 2);
	return (rc);
}

private int
pull(char **av, opts opts, remote *r, char **envVar)
{
	sccs	*cset;
	char	csetFile[MAXPATH] = CHANGESET;
	char	key_list[MAXPATH];
	char	buf[MAXPATH];
	char	*root;
	int	gzip;

	unless (r) usage();
	gzip = opts.gzip && r->port;
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "pull: cannot find package root.\n");
		exit(1);
	}
	if ((bk_mode() == BK_BASIC) && exists("BitKeeper/etc/.master")) {
		fprintf(stderr, "Cannot pull from master repository: %s",
			upgrade_msg);
		exit(1);
	}
	if (repository_lockers(0)) {
		fprintf(stderr, "Cannot do pull into locked repository.\n");
		exit(1);
	}
	unless (cset = sccs_init(csetFile, 0, 0)) {
		fprintf(stderr, "pull: no ChangeSet file found.\n");
		exit(1);
	}
	sccs_sdelta(cset, sccs_ino(cset), buf);
	sccs_free(cset);  /* for win32 */
	root = strdup(buf);

	if (pull_part1(opts, r, key_list, envVar)) return (1); /* fail */
	if (pull_part2(av, opts, r, key_list, envVar)) return (1); /* fail */
	return (0);
}

private	int
takepatch(opts opts, int gzip, remote *r)
{
	int	n, status, pfd;
	pid_t	pid;
	char	*cmds[10];
	char	buf[4096];

	cmds[n = 0] = "bk";
	cmds[++n] = "takepatch";
	unless (opts.quiet) cmds[++n] = "-mvv";
	cmds[++n] = 0;
	pid = spawnvp_wPipe(cmds, &pfd, BIG_PIPE);
	gunzipAll2fd(r->rfd, pfd, gzip, &(opts.in), &(opts.out));
	close(pfd);

	n = waitpid(pid, &status, 0);
	if (n != pid) {
		perror("WAITPID");
		fprintf(stderr, "Waiting for %d\n", pid);
	}

	if (gzip && !opts.quiet) {
		fprintf(stderr,
		    "%u bytes uncompressed to %u, ",
		    opts.in, opts.out);
		fprintf(stderr,
		    "%.2fX expansion\n",
		    (double)opts.out/opts.in);
	}
	if (WIFEXITED(status)) return (WEXITSTATUS(status));
#ifndef	WIN32
	if (WIFSIGNALED(status)) return (-WTERMSIG(status));
#endif
	return (100);
}

private	int
resolve(opts opts, remote *r)
{
	char	*cmd[20];
	int	i;
	char	buf[MAXPATH*2];
	char	pwd[MAXPATH];
	char	from[MAXPATH];
	char	*f;
	char	*h = sccs_gethost();
	int	status;

	cmd[i = 0] = "bk";
	cmd[++i] = "resolve";
	if (opts.quiet) cmd[++i] = "-q";
	if (opts.textOnly) cmd[++i] = "-t";
	if (opts.automerge) cmd[++i] = "-a";
	if (opts.debug) cmd[++i] = "-d";
	getcwd(pwd, sizeof(pwd));
	if (r->host) {
		f = remote_unparse(r);
	} else {
		sprintf(from, "%s:%s", h, r->path);
		f = from;
	}
	sprintf(buf, "-yMerge %s into %s:%s", f, h ? h : "?", pwd);
	if (strlen(buf) > 74) {
		sprintf(buf, "-yMerge %s\ninto %s:%s", f, h ? h : "?", pwd);
	}
	if (r->host) free(f);
	cmd[++i] = buf;
	cmd[++i] = 0;
	unless (opts.quiet) {
		fprintf(stderr, "Running resolve to apply new work ...\n");
	}
	status = spawnvp_ex(_P_WAIT, "bk", cmd);
	unless (WIFEXITED(status)) return (100);
	return (WEXITSTATUS(status));
	return (0);
}
