#include "bkd.h"
#include "range.h"

private struct {
	search	search;		/* -/pattern/[i] matches comments w/ pattern */
	u32	doSearch:1;	/* search the comments list */
	u32	forwards:1;	/* like prs -f */
	char	*date;		/* list this range of dates */
	char	*dspec;		/* override dspec */
	u32	noempty:1;	/* do not list empty merge deltas */
	u32	html:1;		/* do html style output */
	u32	keys:1;		/* just list the keys */
	u32	local:1;	/* want the new local csets */
	u32	nomerge:1;	/* do not list _any_ merge deltas */
	u32	newline:1;	/* add a newline after each record, like prs */
	char	*rev;		/* list this rev or range of revs */
	u32	remote:1;	/* want the new remote csets */
	u32	tagOnly:1;	/* only show items which are tagged */
	char	*user;		/* only from this user */
	u32	others:1;	/* -U<user> everyone except <user> */
	u32	verbose:1;	/* list the file checkin comments */

	/* not opts */
	FILE	*f;		/* global for recursion */
	sccs	*s;		/* global for recursion */
	char	*spec;		/* global for recursion */
} opts;

private int	doit(int dash);
private int	want(sccs *s, delta *e);
private int	send_part1_msg(remote *r, char **av);
private int	send_end_msg(remote *r, char *msg);
private int	send_part2_msg(remote *r, char **av, char *key_list);
private int	changes_part1(remote *r, char **av, char *key_list);
private int	changes_part2(remote *r, char **av, char *key_list, int ret);
private int	doit_remote(char **av, char *url);
private void	cset(sccs *s, FILE *f, char *dspec);

int
changes_main(int ac, char **av)
{
	int	c;
	char	*nav[30];
	int	nac = 0;
	char	*url;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help changes");
		return (1);
	}

	bzero(&opts, sizeof(opts));
	opts.noempty = 1;
	nav[nac++] = "bk";
	nav[nac++] = "changes";
	while ((c = getopt(ac, av, "c;d;efhkLmnRr|tu;U;v/;")) != -1) {
		unless (c == 'L' || c == 'R') {
			if (optarg) {
				nav[nac++] = aprintf("-%c%s", c, optarg);
			} else {
				nav[nac++] = aprintf("-%c", c);
			}
		}
		switch (c) {
		    /*
		     * Note: do not add option 'K', it is reserved
		     * for internal use
		     */
		    case 'c': opts.date = optarg; break;
		    case 'd': opts.dspec = optarg; break;
		    case 'e': opts.noempty = !opts.noempty; break;
		    case 'f': opts.forwards = 1; break;
		    case 'h': opts.html = 1; break;
		    case 'k': opts.keys = 1; opts.noempty = 0; break;
		    case 'm': opts.nomerge = 1; break;
		    case 'n': opts.newline = 1; break;
		    case 't': opts.tagOnly = 1; break;		/* doc 2.0 */
		    case 'U': opts.others = 1;
		    	/* fall through to u */
		    case 'u': opts.user = optarg; break;
		    case 'v': opts.verbose = 1; break;		/* doc 2.0 */
		    case 'r': opts.rev = optarg; break;		/* doc 2.0 */
		    case '/': opts.search = searchParse(optarg);
			      opts.doSearch = 1;
			      break;
		    case 'L': opts.local = 1; break;
		    case 'R': opts.remote = 1; break;
		    default:
usage:			system("bk help -s changes");
			exit(1);
		}
		optarg = 0;
	}
	if ((opts.local || opts.remote) && opts.rev) goto usage;
	if (opts.keys && (opts.verbose||opts.html||opts.dspec)) goto usage;
	if (sccs_cd2root(0, 0)) {
		if (!av[optind] || opts.local || opts.remote) {
			fprintf(stderr, "Can't find package root\n");
			exit(1);
		}
		/* otherwise we don't really care */
	}

	unless (opts.local || opts.remote || av[optind]) {
		for (c = 2; c < nac; c++) free(nav[c]);
		return (doit(0));
	} else if (av[optind] && streq("-", av[optind])) {
		if (opts.local || opts.remote) goto usage;
		for (c = 2; c < nac; c++) free(nav[c]);
		return (doit(1));
	}
	
	unless (url = av[optind]) {
		unless (url = getParent()) {
			fprintf(stderr, "No repository specified?!\n");
			goto out;
		}
	}

	if (opts.local) {
		int	wfd, fd1, status;
		pid_t	pid;

		if (opts.remote) {
			fprintf(stderr, "warning: -R option ignored\n");
		}

		/*
		 * What we want is: bk synckey -lk url | bk changes opts -
		 */
		nav[nac++] = strdup("-");
		assert(nac < 30);
		nav[nac] = 0;
		pid = spawnvp_wPipe(nav, &wfd, 0);

		/*
		 * Send "bk synckey" stdout into the pipe
		 */
		fd1 = dup(1); close(1);
		dup2(wfd, 1); close(wfd);
		sys("bk", "synckeys", "-lk", url, SYS);
		close(1); dup2(fd1, 1); /* restore fd1, just in case */
		waitpid(pid, &status, 0);
		if (WIFEXITED(status)) return (WEXITSTATUS(status));
out:		for (c = 2; c < nac; c++) free(nav[c]);
		unless (av[optind]) free(url);
		return (1); /* interrupted */
	} else {
		int	ret = doit_remote(&nav[1], url);

		for (c = 2; c < nac; c++) free(nav[c]);
		unless (av[optind]) free(url);
		return (ret);
	}
}

private int
pdelta(delta *e)
{
	if (feof(opts.f)) return (1); /* for early pager exit */
	if (e->type != 'D') return(0);
	unless (e->flags & D_SET) return(0);
	if (opts.keys) {
		sccs_pdelta(opts.s, e, opts.f);
		fputc('\n', opts.f);
	} else {
		sccs_prsdelta(opts.s, e, 0, opts.spec, opts.f);
		if (opts.newline) fputc('\n', opts.f);
	}
	fflush(opts.f);
	return (0);
}

private int
recurse(delta *d)
{
	if (d->next) {
		if (recurse(d->next)) return (1);
	}
	return (pdelta(d));
}

/*
 * XXX May need to change the @ to BK_FS in the following dspec
 */
#define	DSPEC	":DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\n$each(:C:){  (:C:)\n}$each(:SYMBOL:){  TAG: (:SYMBOL:)\n}\n"
#define	HSPEC	"<tr bgcolor=lightblue><td font size=4>" \
		"&nbsp;:Dy:-:Dm:-:Dd: :Th:::Tm:&nbsp;&nbsp;" \
		":P:@:HT:&nbsp;&nbsp;:I:</td></tr>\n" \
		"$if(:TAG:){<tr bgcolor=yellow><td>&nbsp;Tag: :TAG:" \
		"</td></tr>\n}" \
		"<tr bgcolor=white><td>" \
		"$each(:C:){&nbsp;(:C:)<br>\n}</td></tr>\n"
#define	HSPECV	"<tr bgcolor=" \
		"$if(:TYPE:=BitKeeper|ChangeSet){lightblue>}" \
		"$if(:TYPE:=BitKeeper){#f0f0f0>}" \
		"<td font size=4>&nbsp;" \
		"$if(:TYPE:=BitKeeper|ChangeSet){" \
		":Dy:-:Dm:-:Dd: :Th:::Tm:&nbsp;&nbsp;" \
		":P:@:HT:&nbsp;&nbsp;:I:}" \
		"$if(:TYPE:=BitKeeper){&nbsp;:DPN: :I:}" \
		"</td></tr>\n" \
		"$if(:TAG:){<tr bgcolor=yellow><td>&nbsp;Tag: :TAG:" \
		"</td></tr>\n}" \
		"<tr bgcolor=white><td>" \
		"$each(:C:){&nbsp;" \
		"$if(:TYPE:=BitKeeper){&nbsp;&nbsp;&nbsp;&nbsp;}" \
		"(:C:)<br>\n}" \
		"$unless(:C:){" \
		"$if(:TYPE:=BitKeeper){&nbsp;&nbsp;&nbsp;&nbsp;}" \
		"&lt;no comments&gt;}" \
		"</td></tr>\n"

private int
doit(int dash)
{
	FILE	*f;
	char	cmd[MAXKEY];
	char	s_cset[] = CHANGESET;
	char	*spec;
	pid_t	pid;
	sccs	*s;
	delta	*e;
	int	noisy = 0;
	int	expand = 1;
	RANGE_DECL;

	if (opts.dspec && !opts.html) {
		spec = opts.dspec;
	} else if (opts.html) {
		spec = opts.verbose ? HSPECV : HSPEC;
	} else {
		spec = DSPEC;
	}
	s = sccs_init(s_cset, SILENT, 0);
	assert(s && s->tree);
	if (opts.rev || opts.date) {
		if (opts.rev) {
			r[0] = notnull(opts.rev);
			things += tokens(r[0]);
		} else {
			d[0] = notnull(opts.date);
			things += tokens(d[0]);
		}
		rd = 1;
		RANGE("changes", s, expand, noisy)
		unless (SET(s)) {
			for (e = s->rstop; e; e = e->next) {
				if (want(s, e)) e->flags |= D_SET;
				if (e == s->rstart) break;
			}
			s->state |= S_SET;
		}
	} else if (dash) {
		while (fgets(cmd, sizeof(cmd), stdin)) {
			/* ignore blank lines and comments */
			if ((*cmd == '#') || (*cmd == '\n')) continue;
			chomp(cmd);
			e = sccs_getrev(s, cmd, NULL, 0);
			unless (e) {
				fprintf(stderr, "Illegal line: %s", cmd);
				sccs_free(s);
				return (1);
			}
			while (e->type == 'R') {
				e = e->parent;
				assert(e);
			}
			if (want(s, e)) e->flags |= D_SET;
		}
		s->state |= S_SET;
	} else {
		for (e = s->table; e; e = e->next) {
			if (e->type != 'D') continue;
			if (want(s, e)) e->flags |= D_SET;
		}
		s->state |= S_SET;
	}
	assert(SET(s));

	/*
	 * What we want is: this process | pager
	 */
	pid = mkpager();

	if (opts.html) {
		fputs("<html><body bgcolor=white>\n"
		    "<table align=center bgcolor=black cellspacing=0 "
		    "border=0 cellpadding=0><tr><td>\n"
		    "<table width=100% cellspacing=1 border=0 cellpadding=1>"
		    "<tr><td>\n", f);
		fflush(stdout);
	}
	s->xflags |= X_YEAR4;
	if (opts.verbose) {
		cset(s, stderr, spec == DSPEC ? 0 : spec);
	} else {
		opts.f = stdout;
		opts.s = s;
		opts.spec = spec;
		if (opts.forwards) {
			recurse(s->table);
		} else {
			for (e = s->table; e; e = e->next) {
				if (pdelta(e)) break;
			}
		}
	}
	if (opts.html) {
		fprintf(stdout, "</td></tr></table></table></body></html>\n");
	}
	sccs_free(s);
	fclose(stdout);
	if (pid >= 0) waitpid(pid, 0, 0);
	return (0);

next:	return (1);
}

private void
cset(sccs *s, FILE *f, char *dspec)
{
	char	*cmd;
	delta	*e;
	FILE	*p;

	if (dspec) {
		if (strchr(dspec, '\'')) {
			fprintf(stderr,
			    "Cannot have a single quote in dspec\n");
			return;
		}
		cmd = aprintf("bk cset -l - | bk sccslog %s %s -d'%s' - ",
		    opts.forwards ? "-f" : "",
		    opts.newline ? "-n" : "",
		    dspec);
	} else {
		cmd = aprintf("bk cset -l - | bk sccslog %s -i2 - ",
		    opts.forwards ? "-f" : "");
	}
	putenv("PAGER=cat");
	unless (p = popen(cmd, "w")) {
		perror(cmd);
		return;
	}
	for (e = s->table; e; e = e->next) {
		unless (e->flags & D_SET) continue;
		fprintf(p, "%s\n", e->rev);
	}
	pclose(p);
	free(cmd);
}

private int
want(sccs *s, delta *e)
{
	if (opts.tagOnly && !(e->flags & D_SYMBOLS)) return (0);
	if (opts.others) {
		if (streq(opts.user, e->user)) return (0);
	} else if (opts.user && !streq(opts.user, e->user)) {
		return (0);
	}
	if (opts.nomerge && e->merge) return (0);
	if (opts.noempty && e->merge && !sfind(s, e->merge)->added) return (0);
	if (opts.doSearch) {
		int	i;

		EACH(e->comments) {
			if (searchMatch(e->comments[i], opts.search)) {
				return (1);
			}
		}
		return (0);
	}
	return (1);
}


private int
send_part1_msg(remote *r, char **av)
{
	char	*cmd, buf[MAXPATH];
	int	rc, i;
	FILE 	*f;

	gettemp(buf, "changes");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, NULL, r, !opts.remote);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "chg_part1");
	if (opts.remote) {
		/*
		 * When doing remote, the -v -t -r options are passed in
		 * part2 of this command
		 */
		fputs(" -K", f); /* this enables the key sync code path */
	} else {
		/* Use the -L/-R cleaned options */
		for (i = 1; av[i]; i++) fprintf(f, " %s", av[i]);
	}
	fputs("\n", f);
	fclose(f);

	if (opts.remote) {
		cmd = aprintf("bk _probekey  >> %s", buf);
		system(cmd);
		free(cmd);
	}

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

	bktemp(msgfile);
	f = fopen(msgfile, "wb");
	assert(f);
	sendEnv(f, NULL, r, !opts.remote);

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
send_part2_msg(remote *r, char **av, char *key_list)
{
	int	rc, i;
	char	msgfile[MAXPATH], buf[MAXLINE];
	FILE	*f;

	bktemp(msgfile);
	f = fopen(msgfile, "wb");
	assert(f);
	sendEnv(f, NULL, r, !opts.remote);

	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
	fprintf(f, "chg_part2");
	/* Use the -L/-R cleaned options */
	for (i = 1; av[i]; i++) fprintf(f, " %s", av[i]);
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
changes_part1(remote *r, char **av, char *key_list)
{
	char	buf[MAXPATH], s_cset[] = CHANGESET;
	int	flags, fd, rc, rcsets = 0, rtags = 0;
	sccs	*s;

	if (bkd_connect(r, 0, 1)) return (-1);
	send_part1_msg(r, av);
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

	if (opts.remote == 0) {
		pid_t	pid;

		getline2(r, buf, sizeof(buf));
		unless (streq("@CHANGES INFO@", buf)) {
			return (0); /* protocal error */
		}
		pid = mkpager();
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (streq("@END@", buf)) break;

			/*
			 * Check for write error, in case
			 * our pager terminate early
			 */
			if (write(1, &buf[1], strlen(buf) - 1) < 0) break;
			if (write(1, "\n", 1) < 0) break;
		}
		fclose(stdout);
		if (pid > 0) waitpid(pid, 0, 0);
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
changes_part2(remote *r, char **av, char *key_list, int ret)
{
	int	rc = 0;
	char	buf[MAXLINE];
	pid_t	pid;

	if ((r->type == ADDR_HTTP) && bkd_connect(r, 0, 0)) {
		return (1);
	}

	if (r->type == ADDR_HTTP) skip_http_hdr(r);

	if (ret == 0){
		send_end_msg(r, "@NOTHING TO SEND@\n");
		goto done;
	}
	send_part2_msg(r, av, key_list);

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
	pid = mkpager();
	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (streq("@END@", buf)) break;
		if (write(1, &buf[1], strlen(buf) - 1) < 0) {
			break;
		}
		write(1, "\n", 1);
	}
	fclose(stdout);
	if (pid > 0) waitpid(pid, 0, 0);

done:	unlink(key_list);
	disconnect(r, 2);
	return (rc);
}

private int
doit_remote(char **av, char *url)
{
	char 	key_list[MAXPATH] = "";
	char	*tmp;
	int	rc;
	remote	*r;

	loadNetLib();
	if (opts.remote) has_proj("changes");
	r = remote_parse(url, 1);
	unless (r) {
		fprintf(stderr, "invalid url: %s\n", url);
		return (1);
	}

	if (opts.remote && opts.rev) {
		fprintf(stderr, "warning: -r option ignored\n");
	}

	/* Quote the dspec for the other side */
	for (rc = 0; av[rc]; ++rc) {
		unless (strneq("-d", av[rc], 2)) continue;
		tmp = aprintf("'-d%s'", &av[rc][2]);
		free(av[rc]);
		av[rc] = tmp;
	}
	rc = changes_part1(r, av, key_list);
	if (opts.remote) {
		rc = changes_part2(r, av, key_list, rc);
	}
	if (key_list[0]) unlink(key_list);
	return (rc);
}
