/*
 * Copyright 2001-2013,2015-2016 BitMover, Inc
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
#include "bam.h"
#include "nested.h"

typedef	struct {
	u32	debug:1;
	u32	verbose:1;
	u32	product:1;
	u32	detach:1;
	int	gzip;
	int	jobs;
	char    *rev;
	char	*bam_url;
	char	**aliases;
} opts;

private int	getsfio(int jobs);
private int	rclone_end(opts *opts);

private char *
rclone_common(int ac, char **av, opts *opts)
{
	int	c;
	char	*p;

	bzero(opts, sizeof(*opts));
	while ((c = getopt(ac, av, "B;dDj;Pr;s;Tvz|", 0)) != -1) {
		switch (c) {
		    case 'B': opts->bam_url = optarg; break;
		    case 'd': opts->debug = 1; break;
		    case 'D': opts->detach = 1; break;
		    case 'j': opts->jobs = atoi(optarg); break; 
		    case 'r': opts->rev = optarg; break; 
		    case 's':
			opts->aliases = addLine(opts->aliases, strdup(optarg));
			break;
		    case 'P': opts->product = 1; /* fall through */
		    case 'T': START_TRANSACTION(); break;
		    case 'v': opts->verbose = 1; break;
		    case 'z':
			opts->gzip = optarg ? atoi(optarg) : Z_BEST_SPEED;
			if (opts->gzip < 0 || opts->gzip > 9) opts->gzip = Z_BEST_SPEED;
			break;
		    default: bk_badArg(c, av);
		}
	}

	setmode(0, _O_BINARY); /* needed for gzip mode */
	unless (p = getenv("BK_REMOTE_PROTOCOL")) p = "";
	unless (streq(p, BKD_VERSION)) {
		out("ERROR-protocol version mismatch, want: ");
		out(BKD_VERSION);
		out(", got ");
		out(p);
		out("\n");
		return (NULL);
	}

	safe_putenv("BK_CSETS=..%s", opts->rev ? opts->rev : "+");
	unless (av[optind])  return (strdup("."));
	return (strdup(av[optind]));
}

private int
isEmptyDir(char *dir)
{
	int	i;
	char	**d;

	unless (d = getdir(dir)) return (0);
	EACH (d) {
		/*
		 * Ignore .ssh directory, for the "hostme" environment
		 */
		if (streq(d[i], ".ssh")) continue;
		freeLines(d, free);
		return (0);
	}
	freeLines(d, free);
	return (1);
}

int
cmd_rclone_part1(int ac, char **av)
{
	opts	opts;
	char	*path, *p;
	char	buf[MAXPATH];

	unless (path = rclone_common(ac, av, &opts)) return (1);

	/*
	 * No check for NESTED here because the other side has to send -P
	 * and if they did then they obviously know about nested.
	 */

	if (((p = getenv("BK_BAM")) && streq(p, "YES")) &&
	    !bk_hasFeature(FEAT_BAMv2)) {
		out("ERROR-please upgrade your BK to a BAMv2 aware version "
		    "(4.1.1 or later)\n");
		return (1);
	}
	if (Opts.safe_cd || getenv("BKD_DAEMON")) {
		char	cwd[MAXPATH];
		char	new[MAXPATH];

		fullname(path, new);
		localName2bkName(new, new);
		strcpy(cwd, proj_cwd());
		unless ((strlen(new) >= strlen(cwd)) &&
		    pathneq(cwd, new, strlen(cwd))) {
			out("ERROR-illegal cd command\n");
			free(path);
			return (1);
		}
	}
	if (global_locked()) {
		out("ERROR-all repositories on this host are locked.\n");
		return (1);
	}
	if (exists(path)) {
		if (isdir(path)) {
			if  (!isEmptyDir(path)) {
				p = aprintf("ERROR-path \"%s\" is not empty\n",
					path);
err:				out(p);
				free(p);
				free(path);
				return (1);
			}
		} else {
			p = aprintf("ERROR-path \"%s\" is not a directory\n",
				path);
				goto err;
		}
	} else if (mkdirp(path)) {
		p = aprintf(
			"ERROR-cannot make directory %s: %s\n",
			path, strerror(errno));
		goto err;
	}
	if (opts.bam_url) {
		if (streq(opts.bam_url, ".")) {
			/* handled in part2 */
		} else if (!streq(opts.bam_url, "none")) {
			unless (p = bp_serverURL2ID(opts.bam_url)) {
				rmdir(path);
				p = aprintf(
		    "ERROR-BAM server URL \"%s\" is not valid\n",
				opts.bam_url);
				goto err;
			}
			concat_path(buf, path, "BitKeeper/log");
			mkdirp(buf);
			bp_setBAMserver(path, opts.bam_url, p);
			free(p);
		}
	} else if ((p = getenv("BK_BAM_SERVER_URL")) && streq(p, ".")) {
		rmdir(path);
		p = aprintf("ERROR-must pass -B to clone BAM server\n");
		goto err;
	}

	out("@OK@\n");
	free(path);
	return (0);
}

int
cmd_rclone_part2(int ac, char **av)
{
	opts	opts;
	char	buf[MAXPATH];
	char	*path, *p;
	int	fd2, jobs, rc = 0;
	project	*proj;
	u64	sfio;

	unless (path = rclone_common(ac, av, &opts)) return (1);
	if (unsafe_cd(path)) {
		p = aprintf("ERROR-cannot chdir to \"%s\"\n", path);
		out(p);
		free(p);
		free(path);
		return (1);
	}
	jobs = opts.jobs ? opts.jobs : parallel(".", WRITER);

	getline(0, buf, sizeof(buf));
	if (!streq(buf, "@SFIO@")) {
		fprintf(stderr, "expect @SFIO@, got <%s>\n", buf);
		rc = 1;
		goto done;
	}

	unless (getenv("BK_REMAP")) proj_remapDefault(0);
	sccs_mkroot(".");
	if (opts.product) {
		touch("BitKeeper/log/PRODUCT", 0644);
		proj_reset(0);
	}
	putenv("_BK_NEWPROJECT=YES");
	if (sane(0, 0)) return (-1);
	repository_wrlock(0);
	if (getenv("BK_LEVEL")) {
		setlevel(atoi(getenv("BK_LEVEL")));
	}
	if (opts.bam_url) {
		if (streq(opts.bam_url, ".")) {
			bp_setBAMserver(path, ".", proj_repoID(0));
		}
	} else if (p = getenv("BK_BAM_SERVER_URL")) {
		bp_setBAMserver(0, p, getenv("BK_BAM_SERVER_ID"));
	}
	free(path);
	if ((p = getenv("BK_FEATURES_USED")) ||
	     (p = getenv("BK_FEATURES_REQUIRED"))) {
		char	**list = splitLine(p, ",", 0);

		lines2File(list, "BitKeeper/log/features");
		freeLines(list, free);
		proj_reset(0);
	}
	features_set(0, FEAT_REMAP, !proj_hasOldSCCS(0));
	printf("@SFIO INFO@\n");

	/* Arrange to have stderr go to stdout */
	fflush(stdout);
	fd2 = dup(2); dup2(1, 2);
	if (rc = getsfio(jobs)) goto err;
	/* clone needs the remote HERE as RMT_HERE; rclone doesn't */
	if (opts.product) unlink("BitKeeper/log/HERE");
	if (opts.detach) unlink("BitKeeper/log/COMPONENT");
	proj_reset(0);
	if (proj_isComponent(0)) {
		unlink("BitKeeper/log/features");
		proj_reset(0);
	}
	cset_updatetip();

	/*
	 * After unpacking sfio we need to reset proj because it might
	 * have become a component.
	 */
	proj = proj_init(".");
	proj_reset(proj);
	proj_free(proj);

	getline(0, buf, sizeof(buf));
	if (!streq("@END@", buf)) {
		fprintf(stderr, "cmd_rclone: warning: lost end marker\n");
	}

	/*
	 * When the source and destination of a clone are remapped
	 * differently, then the id_cache may appear in the wrong location.
	 * Here we move the correct idcache into position before
	 * continuing.
	 */
	if (proj_hasOldSCCS(0)) {
		if (getenv("BK_REMAP")) {
			/* clone remapped -> non-remapped */
			rename("BitKeeper/log/x.id_cache", IDCACHE);
		}
	} else {
		unless (getenv("BK_REMAP")) {
			/* clone non-remapped -> remapped */
			rename("BitKeeper/etc/SCCS/x.id_cache", IDCACHE);
		}
	}

	unless (rc || getenv("BK_BAM")) {
		rc = rclone_end(&opts);
	}

	/* restore original stderr */
err:	dup2(fd2, 2); close(fd2);
	fputc(BKD_NUL, stdout);
	if (rc) {
		printf("%c%d\n", BKD_RC, rc);
	} else {
		/*
		 * Send any BAM keys we need.
		 */
		if (getenv("BK_BAM")) {
			touch(BAM_MARKER, 0666);
			putenv("BKD_DAEMON="); /* allow new bkd connections */
			printf("@BAM@\n");
			p = aprintf("-r..'%s'", opts.rev ? opts.rev : "");
			rc = bp_sendkeys(stdout, p, &sfio, opts.gzip);
			free(p);
			// XXX - rc != 0?
			printf("@DATASIZE=%s@\n", psize(sfio));
			fflush(stdout);
			return (0);
		}
	}
done:
	fputs("@END@\n", stdout); /* end SFIO INFO block */
	fflush(stdout);
	unless (rc) {
		putenv("BK_STATUS=OK");
	} else {
		putenv("BK_STATUS=FAILED");
	}
	safe_putenv("BK_CSETS=..%s", opts.rev ? opts.rev : "+");
	trigger(av[0], "post");
	fputs("@END@\n", stdout);
	repository_unlock(0, 0);
	putenv("BK_CSETS=");
	return (rc);
}

/*
 * complete an rclone of BAM data
 */
int
cmd_rclone_part3(int ac, char **av)
{
	int	fd2, pfd, rc = 0;
	int	status;
	int	inbytes, outbytes;
	pid_t	pid;
	char	*path, *p;
	opts	opts;
	FILE	*f;
	char	*sfio[] = {"bk", "sfio", "-iqB", "-", 0};
	char	buf[4096];

	unless (path = rclone_common(ac, av, &opts)) return (1);
	if (unsafe_cd(path)) {
		p = aprintf("ERROR-cannot chdir to \"%s\"\n", path);
		out(p);
		free(p);
		free(path);
		return (1);
	}
	free(path);

	buf[0] = 0;
	getline(0, buf, sizeof(buf));
	/* Arrange to have stderr go to stdout */
	fd2 = dup(2); dup2(1, 2);
	if (streq(buf, "@BAM@")) {
		/*
		 * Do sfio
		 */
		pid = spawnvpio(&pfd, 0, 0, sfio);
		inbytes = outbytes = 0;
		f = fdopen(pfd, "wb");
		/* stdin needs to be unbuffered when calling sfio */
		assert(!Opts.use_stdio);
		gunzipAll2fh(0, f, &inbytes, &outbytes);
		fclose(f);
		getline(0, buf, sizeof(buf));
		if (!streq("@END@", buf)) {
			fprintf(stderr,
			    "cmd_rclone: warning: lost end marker\n");
		}

		if ((rc = waitpid(pid, &status, 0)) != pid) {
			perror("sfio subprocess");
			rc = 254;
		}
		if (WIFEXITED(status)) {
			rc =  WEXITSTATUS(status);
		} else {
			rc = 253;
		}
	} else unless (streq(buf, "@NOBAM@")) {
		fprintf(stderr, "expect @BAM@, got <%s>\n", buf);
		rc = 1;
	}
	unless (rc) {
		rc = rclone_end(&opts);
	}
	/* restore original stderr */
	dup2(fd2, 2); close(fd2);
	fputc(BKD_NUL, stdout);
	if (rc) printf("%c%d\n", BKD_RC, rc);

	fputs("@END@\n", stdout);
	fflush(stdout);

	if (rc) {
		putenv("BK_STATUS=FAILED");
	} else {
		putenv("BK_STATUS=OK");
	}
	safe_putenv("BK_CSETS=..%s", opts.rev ? opts.rev : "+");
	trigger(av[0], "post");
	fputs("@END@\n", stdout);
	repository_unlock(0, 0);
	putenv("BK_CSETS=");
	return (rc);
}

private int
rclone_end(opts *opts)
{
	int	rc = 0;
	int	partial = 1;	/* we may not know, so assume worst case */
	int	quiet = !opts->verbose;
	char	**checkfiles = 0;

	/* remove any uncommited stuff */
	sccs_rmUncommitted(quiet, &checkfiles);

	if (opts->detach) {
		/*
		 * bp_dataroot() using $BK_ROOTKEY in a bkd to remember the
		 * current ROOTKEY, but we are about the change that so we
		 * need to clear this.
		 */
		putenv("BK_ROOTKEY=");
		if (detach(quiet, 0)) return (-1);
	}
	putenv("_BK_DEVELOPER="); /* don't whine about fixing formats */
	/* remove any later stuff */
	if (opts->rev) {
		rc = after(quiet, 0, opts->rev);
		if (rc == UNDO_SKIP) {
			rc = 0;
			goto docheck;
		}
	} else {
docheck:	/* undo already runs check so we only need this case */
		if (checkfiles || full_check()) {
			rc = run_check(quiet,
			    0, checkfiles, "-fT", &partial);
		}
	}
	freeLines(checkfiles, free);
	if (partial && bp_hasBAM() &&
	    (proj_checkout(0) & (CO_BAM_GET|CO_BAM_EDIT))) {
		system("bk _sfiles_bam | bk checkout -Tq -");
	}

	/*
	 * Save the HERE file.  chmod because sfio w/o perms doesn't
	 * leave it RW.
	 */
	if (opts->aliases) {
		chmod("BitKeeper/log/HERE", 0666);
		opts->aliases = nested_fixHere(opts->aliases);
		if (lines2File(opts->aliases, "BitKeeper/log/HERE")) {
			perror("BitKeeper/log/HERE");
		}
	}
	return (rc);
}


private int
getsfio(int jobs)
{
	int	status, pfd;
	u32	in, out;
	FILE	*f;
	char	*cmds[10] =
		    {"bk", "sfio", "-iq", "--checkout", "--mark-no-dfiles", 0};
	char	j[20];
	pid_t	pid;

	if (jobs) {
		sprintf(j, "-j%d", jobs);
		assert(cmds[5] == 0);
		cmds[5] = j;
		cmds[6] = 0;
	}
	pid = spawnvpio(&pfd, 0, 0, cmds);
	if (pid == -1) {
		fprintf(stderr, "Cannot spawn %s %s\n", cmds[0], cmds[1]);
		return (1);
	}
	f = fdopen(pfd, "wb");
	/* stdin needs to be unbuffered here */
	assert(!Opts.use_stdio);
	gunzipAll2fh(0, f, &in, &out);
	fclose(f);
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	}
	return (100);
}
