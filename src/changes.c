#include "bkd.h"

typedef struct {
	u32     verbose:1;
	u32     tagOnly:1;
	u32     ldiff:1;	/* want the new local csets */
	u32     rdiff:1;	/* want the new remote csets */ 
	char	*rev;
	char	*srch;		/* search string against comments list */
	int	indent;
} opts;

private int	doit(opts, int dash);
private int	doit_remote(opts opts, char **av, int optind);

private void
usage()
{
	system("bk help -s changes");
    	exit(1);
}

int
changes_main(int ac, char **av)
{
	int	c;
	char	cmd[MAXLINE];
	opts	opts;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help changes");
		return (1);
	}   
	
	bzero(&opts, sizeof(opts));
	while ((c = getopt(ac, av, "tLRr|s;v|")) != -1) {
		switch (c) {
		    /*
		     * Note: do not add option 'k', it is reserved
		     * for internal use
		     */
		    case 'i': opts.indent = atoi(optarg);   	/* undoc? 2.0 */
			      break;
		    case 't': opts.tagOnly = 1; break;		/* doc 2.0 */
		    case 'v':					/* doc 2.0 */
			opts.verbose = 1;
			opts.indent = optarg ? atoi(optarg) : 2;
			break;
		    case 'r': opts.rev = optarg; break;		/* doc 2.0 */
		    case 's': opts.srch = optarg; break;
		    case 'L': opts.ldiff = 1; break;
		    case 'R': opts.rdiff = 1; break;
		    default:
			usage();
	    	}
	}
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "Can't find package root\n");
		exit(1);
	}

	if (!av[optind]) {
		return(doit(opts, 0));
	} else if (streq("-", av[optind])) {
		return(doit(opts, 1));
	} else if (opts.ldiff) {
		char	*new_av[20];
		int	i, j, wfd, fd1, status;
		pid_t	pid;
		
		if (opts.rdiff) {
			fprintf(stderr, "warning: -R option ignored\n");
		}

		/*
		 * What we want is: bk synckey -lk url | bk changes opts -
		 */
		new_av[0] = "bk";
		new_av[1] = "changes";
		for (i = 1, j = 2; i < optind; i++) {
	 	 	/* Copy av[], skip -L */
			if (streq(av[i], "-L"))  continue;
			new_av[j++] = av[i];
		}
		new_av[j++] = "-";
		new_av[j] = 0;
		assert(j < 20);
		pid = spawnvp_wPipe(new_av, &wfd, 0);

		/*
		 * Send "bk synckey" stdout into the pipe
		 */
		fd1 = dup(1); close(1);
		dup2(wfd, 1); close(wfd);
		sys("bk", "synckeys", "-lk", av[optind], SYS);
		close(1); dup2(fd1, 1); /* restore fd1, just in case */
		waitpid(pid, &status, 0);
		if (WIFEXITED(status)) return(WEXITSTATUS(status));
		return (1); /* interrupted */
	} else {
		return(doit_remote(opts, av, optind)); 
	}
}


/*
 * Convert a command line to a av[] vector 
 *
 * This function is copied from win32/uwtlib/wapi_intf.c
 * XXX TODO we should propably move this to util.c if used by
 * other code.
 */
private void
line2av(char *cmd, char **av)
{
	char	*p, *q, *s;
	int	i = 0;
#define isQuote(q) (strchr("\"\'", *q) && q[-1] != '\\')
#define isDelim(c) isspace(c)
#define	MAXARGS 100

	p = cmd;
	while (isspace(*p)) p++; 
	while (*p) {
		av[i++] = p;
		s = q = p;
		while (*q && !isDelim(*q)) {
			if (*q == '\\') {
				q++;
				*s++ = *q++;
			} else if (isQuote(q)) {
				q++; /* strip begin quote */
				while (!isQuote(q)) {
					*s++ = *q++;
				}
				q++; /* strip end quote */
			} else {
				*s++ = *q++;
			}
		}
		if (*q == 0) {
			*s = 0;
			break;
		}
		*s = 0;
		p = ++q;
		while (isspace(*p)) p++; 
	}
	av[i] = 0;
	assert(i < MAXARGS);
	return;
}

/*
 * XXX May need to change the @ to BK_FS in the following dspec
 */
#define	DSPEC	":DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\n$each(:C:){  (:C:)\n}$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}\n"
#define	TSPEC	"$if(:TAG:){:DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\n$each(:C:){  (:C:)\n}$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}\n}"

private int
doit(opts opts, int dash)
{
	FILE	*f;
	char	cmd[MAXKEY];
	char	tmpfile[MAXPATH];
	char	s_cset[] = CHANGESET;
	char	buf[100];
	char	*spec = opts.tagOnly ? TSPEC : DSPEC;
	char 	*end;
	pid_t	pid;
	extern	char *pager;
	char	*pager_av[MAXARGS];
	int	i, fd1, pfd;
	sccs	*s;
	delta	*d;

	s = sccs_init(s_cset, SILENT, 0);
	assert(s && s->tree);
	if (opts.rev) {
		if (opts.srch) {
			fprintf(stderr, "Warning: -s option ignored\n");
		}
		s->state |= S_SET;
		d = findrev(s, opts.rev);
		assert(d);
		if (d) d->flags |= D_SET;
		
	} else if (dash) {
		s->state |= S_SET;
		while (fgets(cmd, sizeof(cmd), stdin)) {
			/* ignore blank lines and comments */
			if ((*cmd == '#') || (*cmd == '\n')) continue;
			chomp(cmd);
			d = sccs_getrev(s, cmd, NULL, 0);
			unless (d) {
				fprintf(stderr, "Illegal line: %s", cmd);
				sccs_free(s);
				fclose(f);
				return (1);
			}
			while (d->type == 'R') {
				d = d->parent;
				assert(d);
			}
			d->flags |= D_SET;
		}
	} else if (opts.srch) {
		s->state |= S_SET;
		for (d = s->table; d; d = d->next) {
			if (d->type != 'D')  continue;
			EACH(d->comments) {
				if (match_one(d->comments[i], opts.srch, 1)) {
					d->flags |= D_SET;
				}
			}
		}
	} else {
		s->state &= ~S_SET; /* probably redundant */
	}

	/*
	 * What we want is: "bk prs opts | this process | pager
	 */
	signal(SIGPIPE, SIG_IGN);
	strcpy(tmpfile, pager); /* line2av stomp */
	line2av(tmpfile, pager_av); /* because pager is "less -E" on win32 */
	pid = spawnvp_wPipe(pager_av, &pfd, 0);
	fd1 = dup(1);
	dup2(pfd, 1);
	close(pfd);
	f = fdopen(1, "wb"); /* needed by sccs_prsdelta() below */

	gettemp(tmpfile, "changes");
	s->xflags |= X_YEAR4;
	for (d = s->table; d; d = d->next) {
		if (SET(s) && !(d->flags & D_SET)) continue;
		sccs_prsdelta(s, d, 0, spec, f);
		fflush(f);
		if (opts.verbose) {
			sprintf(cmd,
			    "bk cset -Hr%s | bk _sort | bk sccslog -i%d - > %s",
			    d->rev, opts.indent, tmpfile);
			system(cmd);
			if (cat(tmpfile)) break;
		}
	}
	sccs_free(s);
	close(1);
	waitpid(pid, 0, 0);
	unlink(tmpfile);
	dup2(fd1, 1); close(fd1);
	return (0);
}


private int
send_part1_msg(remote *r, opts opts, char **av, int optind)
{
	char	*cmd, buf[MAXPATH];
	int	rc, i;
	FILE 	*f;

	bktemp(buf);
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, NULL, r, 0);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "chg_part1");
	if (opts.rdiff) {
		/*
		 * When doing rdiff, the -v -t -r options are passed in 
		 * part2 of this command
	 	 */
		fputs(" -k", f); /* this enable the key sync code path */
	} else {
		/* Convert av[] vector to line format */
		for (i = 1; i < optind; i++) {
			assert(!streq(av[i], "-L") && !streq(av[i], "-R"));
			fprintf(f, " %s", av[i]);
		}
	}
	fputs("\n", f);
	fclose(f);

	cmd = aprintf("bk _probekey  >> %s", buf);
	system(cmd);
	free(cmd);

	rc = send_file(r, buf, 0, 0);	
	unlink(buf);
	return (rc);
}


private int
send_end_msg(remote *r, char *msg)
{
	char	msgfile[MAXPATH];
	FILE	*f;
	int	rc;
	int	gzip;

	bktemp(msgfile);
	f = fopen(msgfile, "wb");
	assert(f);
	sendEnv(f, NULL, r, 0);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in part 1
	 */
	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
	fprintf(f, "chg_part2");
	fputs("\n", f);

	fputs(msg, f);
	fclose(f);

	rc = send_file(r, msgfile, 0, 0);	
	unlink(msgfile);
	return (rc);
}

private int
send_part2_msg(remote *r, opts opts, char **av, int optind, char *key_list)
{
	int	rc, i;
	char	msgfile[MAXPATH], buf[MAXLINE];
	FILE	*f;

	bktemp(msgfile);
	f = fopen(msgfile, "wb");
	assert(f);
	sendEnv(f, NULL, r, 0);

	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
	fprintf(f, "chg_part2");
	/* convert av[] vector to line format */
	for (i = 1; i < optind; i++) {
		if (streq("-R", av[i]) || streq("-L", av[i])) continue;
		fprintf(f, " %s", av[i]);
	}
	fputs("\n", f);
	fclose(f);

	rc = send_file(r, msgfile, size(key_list), 0);	
	unlink(msgfile);
	f = fopen(key_list, "rt");
	assert(f);
	write_blk(r, "@KEY LIST@\n", 11);
	while (fnext(buf, f)) write_blk(r, buf, strlen(buf));
	write_blk(r, "@END@\n", 6);
	fclose(f);
	return (rc);
}

/*
 * TODO: this could be merged with synckeys() in synckeys.c
 */
private int
changes_part1(remote *r, opts opts, char **av, int optind, char *key_list)
{
	char	buf[MAXPATH], s_cset[] = CHANGESET;
	int	flags, fd, rc, n, rcsets = 0, rtags = 0;
	sccs	*s;
	delta	*d;

	if (bkd_connect(r, 0, 1)) return (-1);
	send_part1_msg(r, opts, av, optind);
	if (r->rfd < 0) return (-1);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if ((rc = remote_lock_fail(buf, 1))) {
		return (rc); /* -2 means locked */
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
		getline2(r, buf, sizeof(buf));
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
	}
	if (get_ok(r, buf, 1)) return (-1);

	if (opts.rdiff == 0) {
		getline2(r, buf, sizeof(buf));
		unless (streq("@CHANGES INFO@", buf)) {
			return (0); /* protocal error */
		}
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (streq("@END@", buf)) break;
			write(1, &buf[1], strlen(buf) - 1);
			write(1, "\n", 1);
		}
		return (0);
	}

	/*
	 * What we want is: "remote => bk _prunekey => keylist
	 */
	bktemp(key_list);
	fd = open(key_list, O_CREAT|O_WRONLY, 0644);
	s = sccs_init(s_cset, 0, 0);
	flags |= PK_REVPREFIX|PK_RKEY;
	rc = prunekey(s, r, fd, flags, 0, NULL, &rcsets, &rtags);
	if (rc < 0) {
		switch (rc) {
		    case -2:
			printf(
"You are trying to sync to an unrelated package. The root keys for the\n\
ChangeSet file do not match.  Please check the pathnames and try again.\n");
			close(fd);
			sccs_free(s);
			return (1); /* needed to force bkd unlock */
		    case -3:
			printf("You are syncing to an empty directory\n");
			sccs_free(s);
			return (1); /* empty dir */
			break;
		}
		close(fd);
		disconnect(r, 2);
		sccs_free(s);
		return (-1);
	}
	close(fd);
	sccs_free(s);
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	return (rcsets + rtags);
}

private int
changes_part2(remote *r, opts opts,
			char **av, int optind, char *key_list, int ret)
{
	int	rc = 0;
	char	buf[MAXLINE];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, 0, 0)) {
		return (1);
	}

	if (r->type == ADDR_HTTP) skip_http_hdr(r);

	if (ret == 0){
		send_end_msg(r, "@NOTHING TO SEND@\n");
		goto done;
	}
	send_part2_msg(r, opts, av, optind, key_list);

	getline2(r, buf, sizeof(buf));
	if (remote_lock_fail(buf, 0)) {
		rc = -1;
		goto done;
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
	}

	getline2(r, buf, sizeof(buf));
	unless (streq("@CHANGES INFO@", buf)) {
		rc = -1; /* protocal error */
		goto done;
	}
	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (streq("@END@", buf)) break;
		write(1, &buf[1], strlen(buf) - 1);
		write(1, "\n", 1);
	}
	
done:	unlink(key_list);
	disconnect(r, 2);
	return (rc);
}

private int
doit_remote(opts opts, char **av, int optind)
{
	char 	key_list[MAXPATH] = "";
	char 	*url = av[optind];
	int	rc;
	remote	*r;

	loadNetLib();
	has_proj("changes");
	r = remote_parse(url, 0);
	unless (r) {
		fprintf(stderr, "invalid url: %s\n", url);
		exit (1);
	}

	if (opts.rdiff && opts.rev) {
		fprintf(stderr, "warning: -r option ignored\n");
	}

	if ((bk_mode() == BK_BASIC) &&
		!isLocalHost(r->host) && exists(BKMASTER)) {
		fprintf(stderr,
			"Cannot sync from master repository: %s", upgrade_msg);
		exit(1);
	}

	rc = changes_part1(r, opts, av, optind, key_list);
	rc = changes_part2(r, opts, av, optind, key_list, rc);
	if (key_list[0]) unlink(key_list);
	return (rc);
}
