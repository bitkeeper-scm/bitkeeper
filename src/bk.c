/*
 * Copyright 2000-2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"
#include "bkd.h"
#include "cmd.h"
#include "tomcrypt.h"
#include "tomcrypt/randseed.h"
#include "nested.h"
#include "progress.h"
#include "cfg.h"

#define	BK "bk"

int	bk_no_repo_lock = 0;	/* BK_NO_REPO_LOCK is set, used in wrlocked() */
char	*editor = 0, *bin = 0;
char	*BitKeeper = "BitKeeper/";	/* XXX - reset this? */
char	**bk_environ;
jmp_buf	exit_buf;
int	spawn_tcl;		/* needed in crypto.c:bk_preSpawnHook() */
ltc_math_descriptor	ltc_mp;
char	*prog;			/* name of the bk command being run, like co */
char	*title;			/* if set, use this instead of prog for pbars */
char	*start_cwd;		/* if -R or -P, where did I start? */
unsigned int turnTransOff;	/* for transactions, see nested.h */
int	opt_parallel;		/* --parallel[=<n>] */

private	char	*buffer = 0;	/* copy of stdin */
char	*log_versions = "!@#$%^&*()-_=+[]{}|\\<>?/";	/* 25 of 'em */

private	void	bk_atexit(void);
private	int	bk_cleanup(int ret);
private	char	cmdlog_buffer[MAXPATH*4];
private	int	cmdlog_flags;
private	int	cmdlog_repolog;	/* if true log to repo_log in addition */
private	int	cmdlog_locks;
private int	cmd_run(char *prog, int is_bk, int ac, char **av);
private	void	showproc_start(char **av);
private	void	showproc_end(char *cmdlog_buffer, int ret);
private int	launch_L(char *script, char **av);

#define	MAXARGS	1024
#define	MAXPROCDEPTH	30	/* fallsafe, don't recurse deeper than this */

#ifdef	PROFILE
private void
save_gmon(void)
{
	char	buf[200];
	int	i = 0;

	do {
		sprintf(buf, "gmon-%s.%d", prog, i++);
	} while (exists(buf));
	rename("gmon.out", buf);
}
#endif

#if GCC_VERSION >= 40600
#pragma	GCC diagnostic push
#pragma	GCC diagnostic ignored "-Wclobbered"
#endif
int
main(int volatile ac, char **av, char **env)
{
	int	i, c, ret;
	int	is_bk = 0, dashr = 0, remote = 0, dash = 0;
	int	dashA = 0, dashU = 0, headers = 0;
	int	each_repo = 0, dashrdir = 0, fast_ok = 1, mp = 0;
	int	buf_stdin = 0;	/* -Bstdin */
	int	from_iterator = 0;
	char	*p, *locking = 0;
	char	*todir = 0;
	int	toroot = 0;	/* 1==cd2root 3==cd2product */
	char	*envargs = 0;
	char	**sopts = 0;
	char	**aliases = 0;
	char	**nav = 0;
	int	bk_isSubCmd = 0;	/* if 1, BK called us and sent seed */
	static longopt	lopts[] = {
		/* Note: remote_bk() won't like "option:" with a space */
		/* none of these are passed to --each commands */
		{ "title;", 300 },	// title for progress bar
		{ "cd;", 301 },		// cd to dir
		{ "headers", 303 },	// --headers for -s
		{ "from-iterator", 304 },
		{ "sigpipe", 305 },     // allow SIGPIPE
		{ "gfiles-opts;", 306 },// --gfiles-opts=vc
		{ "config;", 307 },	// override config options
		{ "ibuf;", 310 },
		{ "obuf;", 320 },
		{ "trace;", 330 },
		{ "files;", 331 },
		{ "funcs;", 332 },
		{ "progs;", 333 },
		{ "pids", 334 },
		{ "diffable", 335 },
		{ "trace-file;", 340 },
		{ "cold", 345 },
		{ "no-log", 350 },

		/* long aliases for some options */
		{ "all-files", 'A' },
		{ "user-files", 'U' },
		{ "each-repo", 'e' },
		{ "subset|", 's' },
		{ "parallel|", 'j' },
		{ 0, 0 },
	};

	/*
	 * On any reasonable OS this is unnecessary, ignored signals remain
	 * ignored across process creation.  So maybe it's overkill but we
	 * do it anyway, it's harmless (other than if we are on an OS that
	 * screws it up we have a small window where a signal could sneak
	 * in.  Not sure we can do much about that)
	 */
	if (getenv("_BK_IGNORE_SIGS")) sig_ignore();

	ltc_mp = ltm_desc;
	for (i = 3; i < 20; i++) close(i);
	reserveStdFds();
	spawn_preHook = bk_preSpawnHook;
	showproc_start(av);

	if (getenv("BK_REGRESSION") && exists("/build/die")) {
		fprintf(stderr, "Forced shutdown.\n");
		exit(1);
	}

#ifdef	WIN32
	/*
	 * See if the win32 layer needs to enable retry loops. This
	 * can be set to the number of times to try, but if it's set
	 * to anything other than a number (or zero) it means don't
	 * retry.
	 */
	if (p = getenv("BK_WIN_NORETRY")) win32_retry(strtol(p, 0, 10));
#endif

	unless (getenv("_BK_SIGPIPE")) signal(SIGPIPE, SIG_IGN);

	/*
	 * Support redirection via BK_STDIO=$HOST:$PORT.
	 * Similarly for STDERR except that it is write only.
	 */
	if ((p = getenv("BK_STDIO")) && !streq(p, "DONE")) {
		int	sock;

		if ((sock = tcp_connect(p, -1)) > 0) {
			dup2(sock, 0);
			dup2(sock, 1);
			close(sock);
		} else {
			fprintf(stderr, "Unable to reset stdio to %s\n", p);
		}
		putenv("BK_STDIO=DONE");
	}
	if ((p = getenv("BK_STDERR")) && !streq(p, "DONE")) {
		int	sock;

		if ((sock = tcp_connect(p, -1)) > 0) {
			dup2(sock, 2);
			close(sock);
		} else {
			fprintf(stderr, "Unable to reset stderr to %s\n", p);
		}
		putenv("BK_STDERR=DONE");
	}

	/*
	 * Windows seems to have a problem with stderr under rxvt's.
	 * Force unbuffered mode.
	 */
	setbuf(stderr, 0);

	cmdlog_buffer[0] = 0;
	cmdlog_flags = 0;
	if (i = setjmp(exit_buf)) {
		ret = (i >= 1000) ? (i - 1000) : 1;
		goto out;
	}
	atexit(bk_atexit);
	platformInit(av);
	bk_environ = env;

	i = rand_checkSeed();
	if (getenv("_BK_DEBUG_CHECKSEED")) {
		char	val[64];

		sprintf(val, "%d", i);
		cmdlog_addnote("checkseed", val);
	}
	bk_isSubCmd = !i;

	/*
	 * The goal of this is to cause bk to not call a different bk as a
	 * subprocess.  Coverity hit a problem where their bkd was still
	 * running after they had installed over it.  So they were calling
	 * a newer bk and something blew up.
	 */
	if (bk_isSubCmd &&
	    (p = getenv("_BK_VERSION")) && !streq(bk_vers, p) &&
	    !getenv("_BK_NO_VERSION_CHECK")) {
		error("bk: %s being called by %s not supported.\n",
		    bk_vers, p);
		if (getenv("_BK_IN_BKD")) drain();
		exit(1);
	}
	safe_putenv("_BK_VERSION=%s", bk_vers);

	unless (bin) {
		fprintf(stderr,
		    "Unable to find the BitKeeper bin directory, aborting\n");
		return (1);
	}
	fslayer_enable(1);
	unless (getenv("BK_TMP")) bktmpenv();

	/* stderr write(2) wrapper for progress bars */
	stderr->_write = progress_syswrite;

	/*
	 * Parse our options if called as "bk".
	 * We support most of the gfiles options.
	 */
	prog = basenm(av[0]);
	if (streq(prog, "sccs")) prog = "bk";
	if (streq(prog, "bk")) {
		if (av[1] && streq(av[1], "--help") && !av[2]) {
			system("bk help bk");
			return (0);
		}
		if (av[1] && streq(av[1], "--version") && !av[2]) {
			system("bk version");
			return (0);
		}
		is_bk = 1;
		nav = addLine(nav, strdup("bk"));
		/* adding options with args should update remote_bk() */
		while ((c = getopt(ac, av,
			"?;^@|1aAB;cdeDgGhj|L|lnpPqr|Rs|uUxz;", lopts)) != -1) {
			unless ((c == 'e') || (c == 's') || (c == '?') ||
			    (c >= 300)) {
				/* save options for --each */
				nav = bk_saveArg(nav, av, c);
			}
			switch (c) {
			    /* maybe sfiles, depends on nested or not */
			    case 'A': dashA = 1; break;
			    /* sfiles stuff */
			    case 'U':
				dashU = 1; /* nested -U mode */
				sopts = bk_saveArg(sopts, av, c);
				break;
			    case 'g': break; // ignore
			    case '1': case 'a': case 'c': case 'd':
			    case 'D': case 'G': case 'l':
			    case 'n': case 'p': case 'u': case 'x': case '^':
				if (c == 'c') {
					mp |= DS_EDITED;
				} else if ((c == 'p') || (c == 'l')) {
					mp |= DS_PENDING;
				} else {
					fast_ok = 0;
				}
				sopts = bk_saveArg(sopts, av, c);
				break;
			    case 306: // --gfiles-opts="-c --long-opt"
				fast_ok = 0;
				sopts = splitLine(optarg, " ", sopts);
				break;
			    case '?': envargs = optarg; break;
			    case '@': remote = 1; break;
			    case 'B':
				unless (streq(optarg, "stdin")) {
					fprintf(stderr, "bk: only -Bstdin\n");
					return (1);
				}
				buf_stdin = 1;
				break;
			    case 'e':
				each_repo = 1;
				break;
			    case 'q': break;	// noop, -q is the default
			    case 'j': /* --parallel */
				if (optarg) {
					opt_parallel = atoi(optarg);
				} else {
					opt_parallel = MAX(cpus()-1, 2);
				}
				break;
			    case 'L': locking = optarg; break;
			    case 'P':				/* doc 2.0 */
				toroot |= 3;
				break;
			    case 'r':				/* doc 2.0 */
				if (dashr) {
					fprintf(stderr,
					    "bk: Only one -r allowed\n");
					return (1);
				}
				dashr++;
				if (optarg) {
					if (todir) {
baddir:						fprintf(stderr,
						    "bk: only one --cd or "
						    "-r<dir> allowed\n");
						return (1);
					}
					dashrdir = 1;
					todir = optarg;
				}
				break;
			    case 'R':				/* doc 2.0 */
				toroot |= 1;
				break;
			    case 's':
				unless (optarg) {
					fprintf(stderr,
					    "bk -sALIAS: ALIAS cannot be omitted\n");
					return (1);
				}
				aliases = addLine(aliases, strdup(optarg));
				break;
			    case 'z': break;	/* remote will eat it */
			    case 300:	// --title
				title = optarg;
				break;
			    case 301:	// --cd
				if (todir) goto baddir;
				todir = optarg;
				break;
			    case 303:	// --headers
			    	headers = 1;
				break;
			    case 304:  // --from-iterator
				from_iterator = 1;
				break;
			    case 305:  // --sigpipe
				signal(SIGPIPE, SIG_DFL);
				break;
			    case 307: { // --config=key:val
				char	*key, *val;

				unless (val = strchr(optarg, ':')) usage();
				key = strndup(optarg, val-optarg);
				bk_setConfig(key, val+1);
				free(key);
				break;
			    }
			    case 310:   // --ibuf=line|<size>|0
			    case 320: { // --obuf=line|<size>|0
				FILE *f = (c == 310) ? stdin : stdout;
				if (streq(optarg, "line")) {
					setvbuf(f, 0, _IOLBF, 0);
				} else if (i = atoi(optarg)) {
					setvbuf(f, 0, _IOFBF, i);
				} else {
					setvbuf(f, 0, _IONBF, 0);
				}
				break;
			    }
			    // BK_TRACE=1 BK_TRACE_BITS=cmd,nested
			    case 330: /* --trace */
				unless (getenv("BK_TRACE")) {
				    putenv("BK_TRACE=1");
				}
				safe_putenv("BK_TRACE_BITS=%s", optarg);
				break;
			    case 331: /* --files */
				unless (getenv("BK_TRACE")) {
				    putenv("BK_TRACE=1");
				}
				safe_putenv("BK_TRACE_FILES=%s", optarg);
				break;
			    case 332: /* --funcs */
				unless (getenv("BK_TRACE")) {
				    putenv("BK_TRACE=1");
				}
				safe_putenv("BK_TRACE_FUNCS=%s", optarg);
				break;
			    case 333: /* --progs */
				unless (getenv("BK_TRACE")) {
				    putenv("BK_TRACE=1");
				}
				safe_putenv("BK_TRACE_PROGS=%s", optarg);
				break;
			    case 334: /* --pids */
				unless (getenv("BK_TRACE")) {
				    putenv("BK_TRACE=1");
				}
				safe_putenv("BK_TRACE_PIDS=1");
				break;
			    case 335: /* --diffable */
				unless (getenv("BK_DTRACE")) {
				    putenv("BK_DTRACE=1");
				}
				break;
			    case 340: /* --trace-file */
				unless ((p = getenv("BK_TRACE")) &&
				    !streq(p, "1")) {
					p = strchr(optarg, ':')
						? strdup(optarg)
						: fullname(optarg, 0);
					safe_putenv("BK_TRACE=%s", p);
					free(p);
				}
				break;
			    case 345: /* --cold */
				sopts = addLine(sopts, strdup("--cold"));
				break;
			    case 350: /* --no-log */
				putenv("_BK_CMD_NOLOG=1");
				break;
			    default: bk_badArg(c, av);
			}
		}
		unless (mp) fast_ok = 0;
		trace_init(av[0]);
		if (unlikely(bk_trace)) {
			char	**lines = 0;
			char	*buf;

			for (i = 0; av[i]; ++i) lines = addLine(lines, av[i]);
			buf = joinLines(" ", lines);
			T_CMD("%s", buf);
			free(buf);
			freeLines(lines, 0);
		}
		/* remember if we have a trailing dash for parallel below */
		for (i = 0; av[i]; ++i);
		if (streq(av[--i], "-")) dash = 1;

		/* if -r, then -U is only passed to sfiles */
		if (dashr && dashU) dashU = 0;

		// No -A w/ -r, -A is new.
		if (dashA && dashr) {
			fprintf(stderr, "bk: -A may not be combined with -r\n");
			return (1);
		}
		// --parallel requires -[AUr] or trailing -
		if (opt_parallel && !(dashA || dashU || dashr || dash)) {
			fprintf(stderr,
			   "bk: --parallel requires -A, -U, -r, or -\n");
			return (1);
		}
		if (each_repo && dashrdir) {
			fprintf(stderr,
			   "bk: -e may not be combined with -r<dir>\n");
			return (1);
		}
		if (todir && toroot) {
			fprintf(stderr,
			    "bk: --cd/-rDIR not allowed with -R or -P\n");
			return (1);
		}
		if (todir && remote) {
			/* protect from: bk --cd=/etc -xr -@URL cmd */
			fprintf(stderr,
			    "bk: --cd not allowed with -@<URL>\n");
			return (1);
		}

		/* -r implies cd2root unless combined with --cd */
		if (dashr && !each_repo && !todir) toroot |= 1;
		if (dashU) dashA = 1; /* from here on only dashA matters */

		if (each_repo && dashA) {
			fprintf(stderr, "bk: -A not allowed with -e\n");
			return (1);
		}

		if (aliases && !(each_repo || dashA)) {
			fprintf(stderr,
			    "bk: -sALIAS requires -e or -A\n");
			return (1);
		}

		if (toroot && each_repo) {
			fprintf(stderr,
			    "bk: -R/-P not allowed with -e\n");
			return (1);
		}

		if (av[optind]) {
			prog = av[optind];
			if (dashA && streq(prog, "check")) {
				fprintf(stderr,
				    "bk: -A/-U option cannot be used "
				    "with check\n");
				return (1);
			}
		}

		/* Trial versions of bk will expire in 3 weeks. */
		if (test_release && !bk_isSubCmd && !streq(prog, "upgrade") &&
		    !streq(prog, "bin") &&
		    !streq(prog, "pwd") &&
		    (getenv("_BK_EXPIRED_TRIAL") ||
			(time(0) > (time_t)bk_build_timet + test_release))) {
			char	*nav[] = {"version", 0};

			version_main(1, nav);
			exit(1);
		}

		/* -'?VAR=val&VAR2=val2' */
		if (envargs) {
			hash	*h = hash_new(HASH_MEMHASH);

			if (hash_fromStr(h, envargs)) return (1);
			EACH_HASH(h) {
				safe_putenv("%s=%s",
				    (char *)h->kptr, (char *)h->vptr);
			}
			hash_free(h);
		}
		if (remote) {
			if (streq(prog, "pull") ||
			    streq(prog, "push") ||
			    streq(prog, "resolve")) {
				fprintf(stderr, "Cannot run interactive commands "
				    "on a remote repository\n");
				return (1);
			}
			start_cwd = strdup(proj_cwd());
			cmdlog_start(av, 0);
			callstack_push(remote);
			ret = remote_bk(!headers, ac, av);
			goto out;
		}
		if (todir && chdir(todir)) {
			fprintf(stderr, "bk: Cannot chdir to %s\n", todir);
			return (1);
		}
		if ((toroot == 3) && proj_isComponent(0)) {
			if (proj_cd2product()) {
				fprintf(stderr,
				    "bk: Cannot find product root.\n");
				return(1);
			}
		} else if (toroot) {
			if (proj_cd2root()) {
				fprintf(stderr,
				    "bk: Cannot find product root.\n");
				return(1);
			}
		}
		if ((dashA || each_repo) && !proj_isEnsemble(0)) {
			/*
			 * Downgrade to be compat in standalone trees.
			 * bk -A => bk -r, but with cwd relative paths
			 * bk -U => bk -Ur, but with cwd relative paths
			 * bk -sHERE -A => bk -r
			 */
			if (dashA) {
				dashr = 1;
				sopts = addLine(sopts,
				    aprintf("--relpath=%s", proj_cwd()));
			}
			if (each_repo && proj_cd2root()) {
				fprintf(stderr,
				    "bk: Cannot find repository root.\n");
				return(1);
			}
			each_repo = 0;

			// -s|-sHERE|-s.|-sPRODUCT are fine.
			EACH(aliases) {
				unless (streq(aliases[i], ".") ||
				    strieq(aliases[i], "HERE") ||
				    strieq(aliases[i], "PRODUCT")) {
					fprintf(stderr,
					    "bk -s: illegal alias used in "
					    "traditional repository:\n\t%s\n",
					    aliases[i]);
					return (1);
				}
			}
			freeLines(aliases, free);
			aliases = 0;
		}
		start_cwd = strdup(proj_cwd());
		if (each_repo && !dashA) {
			nav = addLine(nav, strdup("--from-iterator"));
			for (i = optind; av[i]; i++) {
				nav = addLine(nav, strdup(av[i]));
			}
			callstack_push(0);
			if (!aliases && fast_ok) aliases = modified_pending(mp);
			ret = nested_each(!headers, nav, aliases);
			freeLines(aliases, free);
			callstack_pop();
			goto out;
		}
		if (locking) {
			int	waitsecs;

			if (locking[1]) {
				waitsecs = strtoul(locking+1, &p, 10);
				unless (*p == 0) {
bad_locking:				fprintf(stderr,
					    "bk: unknown option -L%s\n",
					    locking);
					ret = 1;
					goto out;
				}
			} else {
				waitsecs = cfg_int(0, CFG_LOCKWAIT);
			}
			while (1) {
				if (*locking == 'r') {
					unless (repository_rdlock(0)) break;
				} else if (*locking == 'w') {
					unless (repository_wrlock(0)) break;
				} else {
					goto bad_locking;
				}
				if (waitsecs == 0) {
					fprintf(stderr,
					    "%s: failed to get repository %s "
					    "lock.\n",
					    av[0],
					    (*locking == 'r') ?
					    "read" : "write");
					ret = 2;
					goto out;
				}
				--waitsecs;
				sleep(1);
			}
		}

		unless (prog = av[optind]) {
			prog = "bk"; /* for error messages */
			sopts = unshiftLine(sopts, strdup("gfiles"));
			if (dashr) {
				if (dashA) proj_cd2root();
				/* bk [opts] -r => bk -R gfiles [opts] */
				sopts = addLine(sopts, 0);
				av = &sopts[1];
				ac = nLines(sopts);
				prog = av[0];
				goto run;
			} else if (dashA) {
				/* bk -U [opts] => bk -s gfiles -U [opts] */
				sopts = unshiftLine(sopts, strdup("bk"));
				unless (mp & DS_PENDING) {
					/*
					 * With -p we want the
					 * component csets to be
					 * tested in the product.  All
					 * other cases we supress them
					 * so components are not
					 * listed twice.
					 */
					sopts = addLine(sopts, strdup("-h"));
				}
				sopts = addLine(sopts,
				    aprintf("--relpath=%s", start_cwd));
				callstack_push(0);
				if (!aliases && fast_ok) {
					aliases = modified_pending(mp);
				}
				ret = nested_each(!headers, sopts, aliases);
				freeLines(aliases, free);
				callstack_pop();
				goto out;
			} else if (from_iterator) {
				ret = 0;
				goto out;
			} else {
				usage();
			}
		}
		for (ac = 0; av[ac] = av[optind++]; ac++);
		if ((dashr || dashA) && !streq(prog, "gfiles")) {
			if (dashr) {
				sopts = unshiftLine(sopts, strdup("gfiles"));
				if (dashA) sopts = unshiftLine(sopts,
				    strdup("-R"));
			} else{
				sopts = unshiftLine(sopts, strdup("-A"));
				EACH(aliases) {
					sopts = unshiftLine(sopts,
					    aprintf("-s%s", aliases[i]));
				}
			}
			sopts = unshiftLine(sopts, strdup("bk"));
			sopts = addLine(sopts, 0);
			callstack_push(0);
			if (sfiles(sopts+1)) {
				callstack_pop();
				ret = 1;
				goto out;
			}
			callstack_pop();
			/* we have bk [-r...] cmd [opts] ... */
			/* we want cmd [opts] ... - */
			av[ac++] = "-";
			av[ac] = 0;
		}
	}

run:	trace_init(prog);	/* again 'cause we changed prog */

	if (dashr) nested_check();
	getoptReset();
#ifdef	PROFILE
	if (exists("gmon.out")) save_gmon();
#endif

#ifdef	WIN32
	/* This gets rid of an annoying message when sfiles is killed */
	nt_loadWinSock();
#endif
	cmdlog_start(av, 0);
	/*
	 * Update callstack and don't recurse too deep.
	 */
	callstack_push(0);

	if (buf_stdin) {
		buffer = bktmp(0);
		fd2file(0, buffer);
		close(0);
		open(buffer, O_RDONLY, 0);
	}
	ret = cmd_run(prog, is_bk, ac, av);
	if (locking && (locking[0] == 'r')) repository_rdunlock(0, 0);
	if (locking && (locking[0] == 'w')) repository_wrunlock(0, 0);
out:
	progress_restoreStderr();
	cmdlog_end(ret, 0);
	ret = bk_cleanup(ret);
	/* flush stdout/stderr, needed for bk-remote on windows */
	fflush(stdout);
	fflush(stderr);
	freeLines(sopts, free);
	freeLines(nav, free);
#ifdef	WIN32
	close(1);
#endif
	if (getenv("BK_CLOSEALL")) {
		/*
		 * The old code used to close stderr here.  Add check
		 * so that can be added back if it is needed for some
		 * odd reason.
		 * --lm3di
		 */
		close(1);
		close(2);
	}

	/* close stdin so that sfiles will bail out */
	close(0);

	/* belt and suspenders */
	sfilesDied(0, 1);	/* kill process, don't wait first */

	if (ret < 0) ret = 1;	/* win32 MUST have a positive return */
	return (ret);
}
#if GCC_VERSION >= 40600
#pragma	GCC diagnostic pop
#endif

private int
cmd_run_parallel(int ac, char **av)
{
	int	i, j, c;
	int	ret = 0, status;
	pid_t	pids[32] = {0};
	int	maxfd;
	char	*p;
	fd_set	wr, rd;
	size_t	len;
	FILE	*save;
	char	**argv = 0;
	int	fdin[32] = {0}, fdout[32] = {0};
	char	*frag[32] = {0}; /* end of line fragments */
	char	buf[MAXLINE];

	/*
	 * Make a bk command line for cmd. We already have any
	 * appropriate locks, so pass -?BK_NO_REPO_LOCK=YES.  Use this
	 * at your own risk! It doesn't guarantee that the cmds won't
	 * interfere with each other.
	 */
	argv = addLine(argv, "bk");
	argv = addLine(argv, "-?BK_NO_REPO_LOCK=YES");
	for (i = 0; av[i]; i++) argv = addLine(argv, av[i]);
	argv = addLine(argv, 0);

	/*
	 * Buffer the first few sfiles to see if we
	 * have fewer than opt_parallel, to avoid
	 * spawning more cmds than we have sfiles to process.
	 */
	if (opt_parallel > sizeof(pids)/sizeof(pids[0])) {
		opt_parallel = sizeof(pids)/sizeof(pids[0]);
	}
	save = fmem();
	for (i = 0; i < opt_parallel; ++i) {
		unless (p = fgetln(stdin, &len)) break;
		fwrite(p, 1, len, save);
	}
	opt_parallel = i;
	p = fmem_peek(save, &len);
	for (i = len; i > 0; --i) ungetc(p[i-1], stdin);
	fclose(save);

	/* Spawn opt_parallel cmds. */
	for (i = 0; i < opt_parallel; ++i) {
		pids[i] = spawnvpio(&fdin[i], &fdout[i], 0, argv + 1);
		if (pids[i] < 0) {
			ret = 126;
			for (j = 0; j <= i; ++j) {
				close(fdin[j]);
				close(fdout[j]);
			}
			goto out_parallel;
		}
	}
	freeLines(argv, 0);

	/*
	 * Fan out the sfiles list to the spawned cmds.  Use select()
	 * to find a child that's ready to read, and go through those
	 * round robin.
	 */
	while (1) {
		FD_ZERO(&wr);
		FD_ZERO(&rd);
		maxfd = 0;
		for (i = 0; i < opt_parallel; i++) {
			if (fdin[i]) {
				if (fdin[i] > maxfd) maxfd = fdin[i];
				FD_SET(fdin[i], &wr);
			}
			if (fdout[i]) {
				if (fdout[i] > maxfd) maxfd = fdout[i];
				FD_SET(fdout[i], &rd);
			}
		}
		unless (maxfd) break; /* done */
		if (select(maxfd+1, &rd, &wr, 0, 0) < 0) {
			perror("select");
			break;
		}
		for (i = 0; i < opt_parallel; i++) {
			if (FD_ISSET(fdin[i], &wr)) {
				/* send new line to process */
				if (p = fgetln(stdin, &len)) {
					(void)writen(fdin[i], p, len);
				} else {
					/* done writing */
					close(fdin[i]);
					fdin[i] = 0;
				}
			}
			if (FD_ISSET(fdout[i], &rd)) {
				/* read block of output data */
				c = 0;
				if (frag[i]) {
					/* load old frag to buf */
					strcpy(buf, frag[i]);
					c = strlen(buf);
					assert(c < sizeof(buf));
					FREE(frag[i]);
				}

				/* read new stuff to buf */
				j = read(fdout[i], buf+c, sizeof(buf)-c);
				if (j <= 0) { /* EOF */
					if (j < 0) perror("read");
					close(fdout[i]);
					fdout[i] = 0;
					j = 0;
				}
				c += j;

				/* print any lines */
				for (j = c; j > 0; j--) {
					if (buf[j-1] == '\n') break;
				}
				if (j) writen(1, buf, j);

				if (!fdout[i] || (!j && (c == sizeof(buf)))) {
					/* full buffer, punt */
					writen(1, buf+j, c - j);
				} else if (j < c) {
					/* save leftovers */
					frag[i] = strndup(buf+j, c - j);
				}
			}
		}
	}
out_parallel:
	/*
	 * This returns the last exit status that was non-zero.
	 */
	for (i = 0; i < opt_parallel; ++i) {
		if (pids[i] <= 0) continue;
		if (waitpid(pids[i], &status, 0) != pids[i]) {
			ret = 127;
		}
		if (status) {
			if (WIFEXITED(status)) {
				ret = WEXITSTATUS(status);
			} else if (WIFSIGNALED(status)) {
				ret = WTERMSIG(status);
			} else {
				ret = 128;
			}
		}
	}
	return (ret);
}

/*
 * The commands here needed to be spawned, not execed, so command
 * logging works.
 */
private int
cmd_run(char *prog, int is_bk, int ac, char **av)
{
	int	i, j;
	CMD	*cmd;
	char	cmd_path[MAXPATH];
	char	*argv[MAXARGS];

	cmd = cmd_lookup(prog, strlen(prog));

	/* Handle aliases */
	if (cmd && cmd->alias) {
		cmd = cmd_lookup(cmd->alias, strlen(cmd->alias));
		assert(cmd);
	}
	unless (is_bk || (cmd && cmd->fcn)) {
		fprintf(stderr, "%s is not a linkable command\n",  prog);
		return (1);
	}
	/* unknown commands fall through to bk.script */
	switch (cmd ? cmd->type : CMD_BK_SH) {
	    case CMD_INTERNAL:		/* handle internal command */
		assert(cmd->fcn);
		if ((ac == 2) && streq("--help", av[1])) {
			sys("bk", "help", prog, SYS);
			return (0);
		}
		if (opt_parallel) {
			return (cmd_run_parallel(ac, av));
		} else {
			return (cmd->fcn(ac, av));
		}
	    case CMD_GUI:		/* Handle Gui script */
		return (launch_wish(cmd->name, av+1));

	    case CMD_LSCRIPT:		/* Handle L scripts */
		return (launch_L(cmd->name, av+1));

	    case CMD_SHELL:		/* Handle shell scripts */
		argv[0] = shell();
		sprintf(cmd_path, "%s/%s", bin, prog);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) {
			if (i >= (MAXARGS-10)) {
				fprintf(stderr, "bk: too many args\n");
				return (1);
			}
			argv[i] = av[j];
		}
		argv[i] = 0;
		return (spawn_cmd(_P_WAIT, argv));

	    case CMD_CPROG:		/* Handle C programs */
		argv[0] = cmd->name;
		for (i = 1; av[i]; i++) {
			if (i >= (MAXARGS-10)) {
				fprintf(stderr, "bk: too many args\n");
				return(1);
			}
			argv[i] = av[i];
		}
		argv[i] = 0;
		return (spawn_cmd(_P_WAIT, argv));

	    case CMD_BK_SH:
		/* Handle GUI test */
		if (streq(prog, "guitest")) {
			sprintf(cmd_path, "%s/t/guitest", bin);
			return (launch_wish(cmd_path, av+1));
		}

		/*
		 * If we get here, it is a
		 * a) bk shell function
		 *    or
		 * b) external program/script
		 * XXX This is slow because we are going thru the shell
		 */
		argv[0] = shell();
		sprintf(cmd_path, "%s/bk.script", bin);
		argv[1] = cmd_path;
		for (i = 2, j = 0; av[j]; i++, j++) {
			if (i >= (MAXARGS-10)) {
				fprintf(stderr, "bk: too many args\n");
				return (1);
			}
			argv[i] = av[j];
		}
		argv[i] = 0;
		return (spawn_cmd(_P_WAIT, argv));
	    default:
		/* should never get here */
		fprintf(stderr, "bk: '%s' not setup correctly.\n", prog);
		return (1);
	}
}

#define	LOG_BADEXIT	-100000		/* some non-valid exit */

/*
 * A wrapper for exit() in bk.  It jumps to the cleanup code at the
 * end of main so that locks will be released, temp files will be
 * erased and the error will be logged in cmdlog.
 *
 * In the error case the cmdlog will be annotated with the file:line
 * that called exit.  Normal code should exit by returning from
 * cmd_main(), but any exit not by that path should be included in the
 * cmdlog.
 */
void
bk_exit(const char *file, int line, int ret)
{
	char	buf[MAXLINE];

	if (ret) {
		sprintf(buf, "%s:%d", file, line);
		cmdlog_addnote("exit", buf);
	}
	longjmp(exit_buf, (ret) + 1000);
}
/*
 * This function is installed as an atexit() handler.
 * In general, it shouldn't be needed but it is here as a fallback.
 *
 * On the normal exit path, both calls below will shortcircuit and
 * do nothing.
 *
 * But if exit() is called in libc or some other place that doesn't
 * include sccs.h and the macro replacing exit() with a longjmp, then
 * this function will make sure that the cmdlog is updated and bk
 * cleans up after itself.
 */
private void
bk_atexit(void)
{
	progress_restoreStderr();
	/*
	 * XXX While almost all bkd commands call this function on
	 * exit. (via the atexit() interface), there is one exception:
	 * on win32, the top level bkd service thread cannot process atexit()
	 * when the serice shutdown. (XP consider this an error)
	 * Fortunately, the bkd spawns a child process to process each
	 * new connection. The child process do follow the the normal
	 * exit path and process atexit().
	 */
	cmdlog_end(LOG_BADEXIT, 0);
	bk_cleanup(LOG_BADEXIT);
}

/*
 * Called before exiting, this function freed cached memory and looks for
 * other cleanups like stale lockfiles.
 */
private int
bk_cleanup(int ret)
{
	static	int	done = 0;

	if (done) return (ret);
	done = 1;

	/* this is attached to stdin and we have to clean it up or
	 * bktmpcleanup() will deadlock on windows.
	 */
	if (buffer) {
		close(0);
		unlink(buffer);
		free(buffer);
		buffer = 0;
	}
	notifier_flush();
	lockfile_cleanup();

	/*
	 * XXX TODO: We need to make win32 serivce child process send the
	 * the error log to via the serive log interface. (Service process
	 * cannot send messages to tty/desktop without special configuration).
	 */
	repository_lockcleanup(0);
	proj_reset(0);		/* flush data cached in proj struct */
	fslayer_enable(0);

	fslayer_cleanup();

#ifndef	NOPROC
	rmdir_findprocs();
#endif

#if	defined(__linux__)
	/*
	 * Test for filehandles left open at the end of regressions
	 * We only do with if bk exits successful as we can't fix
	 * all error paths.
	 * On linux if you comment out this section and run under
	 * valgrind these problems are easier to find.
	 * - put an abort() after the ttyprintf below
	 * - use --track-fds=yes --trace-children=yes -q
	 * 
	 * or just run with strace and see what was opened
	 */
	if ((ret == 0) && getenv("BK_REGRESSION")) {
		int	i, fd, len;
		char	buf[100];
		char	procf[100];

		for (i = 3; i < 20; i++) {
			/* if we can dup() it, then it is open */
			if ((fd = dup(i)) < 0) continue;
			close(fd);

			/* look for info in /proc */
			sprintf(procf, "/proc/%u/fd/%d", (u32)getpid(), i);
			if ((len = readlink(procf, buf, sizeof(buf))) < 0) {
				len = 0;
			}
			buf[len] = 0;
			ttyprintf("%s: warning fh %d left open %s\n",
			    prog, i, buf);
			unless (ret) ret = 1;
		}
	}
#endif
	bktmpcleanup();
	trace_free();
	return (ret);
}

/*
 * Special flags for various BK commands.  See sccs.h for definitions.
 */
private	struct {
	char	*name;
	int	flags;
} cmdtab[] = {
	{"abort", CMD_REPOLOG|CMD_COMPAT_NOSI},
	{"admin", CMD_WRLOCK},
	{"attach", CMD_REPOLOG},
	/* bam handled in bam.c */
	{"bk", CMD_COMPAT_NOSI}, /* bk-remote */
	{"check", CMD_COMPAT_NOSI},
	/* checksum is handled in checksum.c */
	{"clone", CMD_REPOLOG},		/* locking handled in clone.c */
	{"cmdlog", CMD_NOLOG},
	{"collapse", CMD_REPOLOG},	/* real locks in collapse.c */
	{"commit", CMD_REPOLOG},	/* real locks in commit.c */
	{"cp", CMD_WRLOCK},
	{"cset", CMD_REPOLOG},
	{"csetprune", CMD_WRLOCK},
	/* ci/delta locking handled in delta.c */
	{"_findcset", CMD_WRLOCK},
	{"fix", CMD_REPOLOG},
	{"get", CMD_COMPAT_NOSI},
	{"kill", CMD_NOREPO|CMD_COMPAT_NOSI},
	{"newroot", CMD_WRLOCK},
	{"pull", CMD_REPOLOG|CMD_BYTES}, /* real locks in pull.c */
	{"push", CMD_REPOLOG|CMD_BYTES}, /* real locks in push.c */
	{"rcs2bk", CMD_WRLOCK},
	{"remote abort", CMD_REPOLOG|CMD_COMPAT_NOSI|CMD_IGNORE_RESYNC},
	{"remote changes part1", CMD_REPOLOG|CMD_RDLOCK},
	{"remote changes part2", CMD_REPOLOG},	// unlocked internally
	{"remote clone",
	     CMD_REPOLOG|CMD_BYTES|CMD_RDLOCK|CMD_NESTED_RDLOCK},
	{"remote pull part1",
	     CMD_REPOLOG|CMD_BYTES|CMD_RDLOCK|CMD_NESTED_RDLOCK},
	{"remote pull part2", CMD_REPOLOG|CMD_BYTES|CMD_RDLOCK},
	{"remote push part1",
	     CMD_REPOLOG|CMD_BYTES|CMD_WRLOCK|CMD_NESTED_WRLOCK},
	{"remote push part2", CMD_REPOLOG|CMD_BYTES|CMD_WRLOCK},
	{"remote push part3", CMD_REPOLOG|CMD_BYTES|CMD_WRLOCK},
	{"remote rclone part1", CMD_REPOLOG|CMD_NOREPO|CMD_BYTES},
	{"remote rclone part2", CMD_REPOLOG|CMD_NOREPO|CMD_BYTES},
	{"remote rclone part3", CMD_REPOLOG|CMD_NOREPO|CMD_BYTES},
	{"remote quit", CMD_NOREPO|CMD_QUIT},
	{"remote rdlock", CMD_RDLOCK},
	{"remote wrlock", CMD_WRLOCK},
	{"renumber", CMD_WRLOCK},
	{"resolve", CMD_REPOLOG},
	{"rm", CMD_WRLOCK},
	// {"rmdir", CMD_WRLOCK}, // rmdir is a script in bk.sh
	{"mv", CMD_WRLOCK},
	{"mvdir", CMD_WRLOCK},
	{"sccs2bk", CMD_WRLOCK},
	{"stripdel", CMD_WRLOCK},
	{"synckeys", CMD_RDLOCK},
	{"tagmerge", CMD_WRLOCK|CMD_NESTED_WRLOCK},
	// takepatch locking is handled in takepatch.c
	{"_unbk", CMD_WRLOCK},
	{"undo", CMD_REPOLOG},  /* actual WRLOCK in undo.c */
	{"unpull", CMD_REPOLOG},
	{"xflags", CMD_WRLOCK},
	{ 0, 0 },
};

void
cmdlog_start(char **av, int bkd_cmd)
{
	int	i, len;
	char	*quoted;

	cmdlog_buffer[0] = 0;
	cmdlog_flags = 0;
	cmdlog_repolog = 0;
	for (i = 0; cmdtab[i].name; i++) {
		unless (strieq(cmdtab[i].name, av[0])) continue;

		cmdlog_flags = cmdtab[i].flags;
		if (bkd_cmd) {
			prog = av[0];
			T_CMD("%s", prog);
			callstack_push(1);
		}

		/*
		 * set the cmd_repolog flag when it's defined in the
		 * table above, an actual user command (indent() == 0)
		 * or when it's a remote command (indent() > 0)
		 * because it is spawned by a bkd)
		 */
		if ((cmdlog_flags & CMD_REPOLOG) &&
		    (indent() == 0) || strneq(av[0], "remote ", 7)) {
			cmdlog_repolog = 1;
		}
		break;
	}
	if (cmdlog_flags & CMD_NOLOG) putenv("_BK_CMD_NOLOG=1");
	if (cmdlog_flags && proj_root(0)) {
		/*
		 * When in http mode, each connection of push will be
		 * obtaining a separate repository lock.  For a BAM push a
		 * RESYNC dir is created in push_part2 and resolved at the end
		 * of push_part3.  The lock created at the start of push_part3
		 * needs to know that it is OK to ignore the existing RESYNC.
		 */
		if (getenv("_BKD_HTTP") && streq(av[0], "remote push part3")) {
			cmdlog_flags |= CMD_IGNORE_RESYNC;
		}

		/*
		 * Remote clone from a component without already having a
		 * nested lock means we're doing a populate, so skip
		 * nested locking.
		 */
		if (proj_isComponent(0) && streq(av[0], "remote clone")) {
			cmdlog_flags &= ~CMD_NESTED_RDLOCK;
		}

		/*
		 * Port is another command that just works at a component
		 * level, so we skip the nested locking.
		 */
		if (getenv("BK_PORT_ROOTKEY")) {
			cmdlog_flags &= ~CMD_NESTED_RDLOCK;
		}

		if (bkd_cmd) cmdlog_flags |= CMD_BKD_CMD;
		cmdlog_lock(cmdlog_flags);
	}

	unless (getenv("_BK_CMD_NOLOG")) {
		for (len = 1, i = 0; av[i]; i++) {
			quoted = shellquote(av[i]);
			len += strlen(quoted) + 1;
			if (len >= sizeof(cmdlog_buffer)) {
				free(quoted);
				break;
			}
			if (i) strcat(cmdlog_buffer, " ");
			strcat(cmdlog_buffer, quoted);
			free(quoted);
		}
		write_log("cmd_log", "%s", cmdlog_buffer);
	}
	if (cmdlog_flags & CMD_BYTES) save_byte_count(0); /* init to zero */

	if (bkd_cmd) {
		char	*repoID = getenv("BK_REPO_ID");
		if (repoID) cmdlog_addnote("rmtc", repoID);
	}
	if (bkd_cmd &&
	    (!(cmdlog_flags & CMD_COMPAT_NOSI) || bk_hasFeature(FEAT_SAMv3))) {
		/*
		 * COMPAT: Old bk's don't expect a serverInfo block
		 * before the error, but since we have the environment
		 * here, we can just check what version of bk we're
		 * talking to and either send the error before or
		 * after the serverInfo block
		 */
		if (sendServerInfo(cmdlog_flags)) {
			drain();
			repository_unlock(0, 0);
			exit(1);
		}
	}
}

/*
 * Handle locking issues for commands.
 * This is normally called automatically for commands out of
 * cmdlog_start() above, but commands can call it directly if locking
 * is conditional on factors not known yet.
 *
 * Global vars:  cmdlog_locks (read/write) cmdlog_flags (write)
 */
void
cmdlog_lock(int flags)
{
	int	i, do_lock = 1;
	char	*p, *repo1, *repo2, *nlid = 0;
	project	*proj = 0;
	char	*error_msg = 0;

	p = getenv("BK_NO_REPO_LOCK");
	if ((p && streq(p, "YES")) || bk_no_repo_lock) {
		/*
		 * We unset the env var here but set
		 * a flag var which gets checked by
		 * wrlocked() in slib.c
		 */
		bk_no_repo_lock = 1;
		putenv("BK_NO_REPO_LOCK=");
	}

	cmdlog_flags = flags;

	/*
	 * cmdlog_locks remember all the repository locks obtained by
	 * this process so if another command wants the same lock type
	 * we won't try to get it again.  This is used for example in
	 * pull part1,2,3 so with each http:// connection will
	 * reaquire the lock, but a normal bk:// connection will only
	 * get the lock once.
	 */
	cmdlog_flags &= ~cmdlog_locks;

	/*
	 * Provide a way to do nested repo operations.  Used by import
	 * which calls commit.
	 * Locking protocol is that BK_NO_REPO_LOCK=YES means we are already
	 * locked, skip it, but clear it to avoid inheritance.
	 */
	if (bk_no_repo_lock &&
	    (cmdlog_flags &
	    (CMD_WRLOCK|CMD_RDLOCK|CMD_NESTED_WRLOCK|CMD_NESTED_RDLOCK))) {

		T_LOCK("Skipping locking due to BK_NO_REPO_LOCK");
#if 0
		{
			int	i;
			i = repository_hasLocks(0, WRITER_LOCK_DIR);
			unless (i) T_LOCK("NO WRLOCK, ASSERTING");
			assert(i);
		}
#endif
		do_lock = 0;
	}

	T_LOCK("do_lock = %d:%x", do_lock, cmdlog_flags);
	if ((cmdlog_flags & CMD_BKD_CMD) &&
	    (cmdlog_flags & (CMD_WRLOCK|CMD_RDLOCK)) &&
	    (repo1 = getenv("BK_REPO_ID")) && (repo2 = proj_repoID(proj))) {
		i = streq(repo1, repo2);
		if (i) {
			error_msg = strdup("can't connect to same repo_id\n");
			if (getenv("BK_REGRESSION")) usleep(100000);
			goto out;
		}
	}

	if (do_lock && (cmdlog_flags & CMD_WRLOCK)) {
		if (cmdlog_flags & CMD_IGNORE_RESYNC) {
			putenv("_BK_IGNORE_RESYNC_LOCK=1");
		}
		T_LOCK("WRLOCK in %s", proj_cwd());
		i = repository_wrlock(proj);
		if (cmdlog_flags & CMD_IGNORE_RESYNC) {
			putenv("_BK_IGNORE_RESYNC_LOCK=");
		}
		if (i) {
			unless ((cmdlog_flags & CMD_BKD_CMD) ||
			    !proj_root(proj)) {
				if (proj_isEnsemble(proj)) {
					nested_printLockers(proj, 1,0, stderr);
				} else {
					repository_lockers(proj);
				}
			}
			switch (i) {
				/* Gross +6 is to skip ERROR- */
			    case LOCKERR_LOST_RACE:
				/* It would be nice if these went to stderr
				 * for local processes.
				 */
				error_msg = aprintf("%s\n", LOCK_WR_BUSY+6);
				break;
			    case LOCKERR_PERM:
				error_msg = aprintf("%s\n", LOCK_PERM+6);
				break;
			    default:
				error_msg = aprintf("%s\n", LOCK_UNKNOWN+6);
				break;
			}
			/*
			 * Eat message sent by bkd client. (e.g. push_part1) 
			 * We need this to make the bkd error message show up
			 * on the client side.
			 */
			goto out;
		}
		cmdlog_locks |= CMD_WRLOCK;
	}
	if (do_lock && (cmdlog_flags & CMD_RDLOCK)) {
		T_LOCK("RDLOCK in %s", proj_cwd());
		if (i = repository_rdlock(proj)) {
			unless ((cmdlog_flags & CMD_BKD_CMD) ||
			    !proj_root(proj)) {
				if (proj_isEnsemble(proj)) {
					nested_printLockers(proj, 1,0, stderr);
				} else {
					repository_lockers(proj);
				}
			}
			switch (i) {
			    case LOCKERR_LOST_RACE:
				error_msg = aprintf("%s\n", LOCK_RD_BUSY+6);
				break;
			    case LOCKERR_PERM:
				error_msg = aprintf("%s\n", LOCK_PERM+6);
				break;
			    default:
				error_msg = aprintf("%s\n", LOCK_UNKNOWN+6);
				break;
			}
			/*
			 * Eat message sent by bkd client. (e.g. pull_part1) 
			 * We need this to make the bkd error message show up
			 * on the client side.
			 */
			goto out;
		}
		cmdlog_locks |= CMD_RDLOCK;
	}
	if (do_lock &&
	    (cmdlog_flags & (CMD_NESTED_RDLOCK | CMD_NESTED_WRLOCK)) &&
	    proj_isEnsemble(proj)) {
again:		if (nlid = getenv("_BK_NESTED_LOCK")) {
			T_NUM(TR_LOCK|TR_NESTED, "checking: %s", nlid);
			unless (nested_mine(proj, nlid,
				(cmdlog_flags & CMD_NESTED_WRLOCK))) {
				if (nl_errno == NL_LOCK_FILE_NOT_FOUND) {
					/* relock */
					putenv("_BK_NESTED_LOCK=");
					goto again;
				} else {
					error_msg = nested_errmsg();
					goto out;
				}
			}
		} else if (cmdlog_flags & CMD_NESTED_WRLOCK) {
			unless (nlid = nested_wrlock(proj_product(proj))) {
				error_msg = nested_errmsg();
				goto out;
			}
			safe_putenv("_BK_NESTED_LOCK=%s", nlid);
			free(nlid);
			cmdlog_locks |= CMD_NESTED_WRLOCK;
			T_NUM(TR_LOCK|TR_NESTED, "%s", "NESTED_WRLOCK");
		} else if (cmdlog_flags & CMD_NESTED_RDLOCK) {
			unless (nlid = nested_rdlock(proj_product(proj))) {
				error_msg = nested_errmsg();
				goto out;
			}
			safe_putenv("_BK_NESTED_LOCK=%s", nlid);
			free(nlid);
			cmdlog_locks |= CMD_NESTED_RDLOCK;
			T_NUM(TR_LOCK|TR_NESTED, "NESTED_RDLOCK");
		} else {
			/* fail? */
		}
	}
out:
	if (error_msg) {
		error("%s", error_msg);
		free(error_msg);
		if (cmdlog_flags & CMD_BKD_CMD) {
			drain();
			repository_unlock(proj, 0);
		}
		exit (1);
	}
}

void
cmdlog_unlock(int flags)
{
	unless (cmdlog_locks & flags) return;
	if (flags & cmdlog_locks & (CMD_NESTED_WRLOCK|CMD_NESTED_RDLOCK)) {
		char	*nlid;

		nlid = getenv("_BK_NESTED_LOCK");
		assert(nlid);
		T_NUM(TR_LOCK|TR_NESTED, "nlid = %s", nlid);
		if (nested_unlock(0, nlid)) {
			error("%s", nested_errmsg());
		}
		cmdlog_locks &= ~(CMD_NESTED_WRLOCK|CMD_NESTED_RDLOCK);
	}
	if (flags & cmdlog_locks & (CMD_WRLOCK|CMD_RDLOCK)) {
		T_LOCK("UNLOCK %s", proj_cwd());
		repository_unlock(0, 0);
		cmdlog_locks &= ~(CMD_WRLOCK|CMD_RDLOCK);
	}
}

private	MDBM	*notes = 0;

void
cmdlog_addnote(const char *key, const char *val)
{
	unless (notes) notes = mdbm_mem();
	mdbm_store_str(notes, key, val, MDBM_REPLACE);
}

int
write_log(char *file, char *format, ...)
{
	struct timeval tv;
	FILE	*f;
	char	*root;
	char	path[MAXPATH], nformat[MAXPATH];
	va_list	ap;

	if (getenv("_BK_CMD_NOLOG")) return (0);
	unless (root = proj_root(0)) return (1);
	strcpy(path, root);
	if (proj_isResync(0)) concat_path(path, path, RESYNC2ROOT);
	concat_path(path, path, "/BitKeeper/log/");
	concat_path(path, path, file);
	unless (f = fopen(path, "a")) {
		concat_path(path, root, BKROOT);
		unless (exists(path)) return (1);
		concat_path(path, root, "/BitKeeper/log/");
		concat_path(path, path, file);
		unless (mkdirf(path)) return (1);
		unless (f = fopen(path, "a")) {
			fprintf(stderr, "Cannot open %s\n", path);
			return (1);
		}
	}
	setvbuf(f, NULL, _IONBF, 0);
	gettimeofday(&tv, 0);
	sprintf(nformat, "%c%s %lu.%06lu %s %s: %*s%s\n",
	    log_versions[LOGVER],
	    sccs_getuser(), tv.tv_sec, (unsigned long)tv.tv_usec,
	    milli(), bk_vers, indent(), "", format);
	va_start(ap, format);
	vfprintf(f, nformat, ap);
	va_end(ap);
	fclose(f);
	/* only rotate logs on command boundaries */
	if (indent() == 0) log_rotate(path);
	return (0);
}

int
cmdlog_end(int ret, int bkd_cmd)
{
	int	rc = cmdlog_flags & CMD_QUIT;
	char	*log;
	int	len, savelen;
	kvpair	kv;
	u64	rss;

	notifier_flush();
	unless (cmdlog_buffer[0]) goto out;

	/* add last minute notes */
	if (cmdlog_flags & CMD_BYTES) {
		char	buf[20];

		sprintf(buf, "%u", (u32)get_byte_count());
		cmdlog_addnote("xfered", buf);
	}
	/* if a process uses more than 10 meg, log it */
	rss = maxrss();
	if (rss > 10<<20) cmdlog_addnote("mem", psize(rss));

	showproc_end(cmdlog_buffer, ret);

	/* If we have no project root then bail out */
	unless (proj_root(0)) goto out;

	len = strlen(cmdlog_buffer) + 20;
	EACH_KV (notes) len += kv.key.dsize + kv.val.dsize;
	log = malloc(len);
	if (ret == LOG_BADEXIT) {
		sprintf(log, "%s = ?", cmdlog_buffer);
	} else {
		sprintf(log, "%s = %d", cmdlog_buffer, ret);
	}
	savelen = len;
	len = strlen(log);
	EACH_KV (notes) {
		log[len++] = ' ';
		strcpy(&log[len], kv.key.dptr);
		len += kv.key.dsize - 1;
		log[len++] = '=';
		strcpy(&log[len], kv.val.dptr);
		len += kv.val.dsize - 1;
	}
	assert(len < savelen);
	mdbm_close(notes);
	notes = 0;
	unless (getenv("_BK_CMD_NOLOG")) {
		write_log("cmd_log", "%s", log);
		if (cmdlog_repolog) {
			/*
			 * commands in the repolog table above get written
			 * to the repo_log in addition to the cmd_log and
			 * the repo_log is never rotated.
			 */
			write_log("repo_log", "%s", log);
		}
	}
	free(log);
	if ((!bkd_cmd || (bkd_cmd && ret ))) {
		cmdlog_unlock(CMD_NESTED_WRLOCK|CMD_NESTED_RDLOCK);
	}
	unless (bkd_cmd) cmdlog_unlock((CMD_WRLOCK|CMD_RDLOCK));
out:
	cmdlog_buffer[0] = 0;
	cmdlog_flags = 0;
	return (rc);
}

private int
launch_L(char *script, char **av)
{
	int	ret, j;
	int	i = 0;
	pid_t	pid;
	char	*argv[MAXARGS];
	char	tclcmd[MAXPATH];
	char	cmd[MAXPATH];

	sprintf(tclcmd, "%s/gui/bin/tclsh", bin);
	unless(executable(tclcmd)) {
		fprintf(stderr, "Cannot find the L interpreter.\n");
		exit(1);
	}

	sprintf(cmd, "%s/lscripts/%s.l", bin, script);

	argv[i++] = tclcmd;
	argv[i++] = "--L";
	argv[i++] = cmd;
	for (j = 0; av[j]; i++, j++) {
		if (i >= (MAXARGS-10)) {
			fprintf(stderr, "bk: too many args\n");
			return (1);
		}
		argv[i] = av[j];
	}
	argv[i] = 0;
	spawn_tcl = 1;
	if ((pid = spawnvp(_P_NOWAIT, argv[0], argv)) < 0) {
		fprintf(stderr, "bk: cannot spawn %s\n", argv[0]);
	}
	spawn_tcl = 0;
	if (waitpid(pid, &ret, 0) < 0) {
		return (126);
	} else if (!WIFEXITED(ret)) {
		return (127);
	} else {
		return (WEXITSTATUS(ret));
	}
}

int
launch_wish(char *script, char **av)
{
	char	*path;
	int	i, j, ac, ret, wret;
	pid_t	pid;
	struct	{
		char	*name;	/* -colormap */
		int	hasarg;	/* like -colormap name */
	}	wishargs[] = {
			{ "-colormap", 1 },
			{ "-display", 1 },
			{ "-geometry", 1 },
			{ "-name", 1 },
			{ "-sync", 0 },
			{ "-use", 1 },
			{ "-visual", 1 },
			{ 0, 0 }
		};
	char	*readwrite[] = {	// these all disable ^C
		"citool",
		"fmtool",
		"fm3tool",
		0
		};
	char	cmd_path[MAXPATH];
	char	*argv[MAXARGS];

	/* If they set this, they can set TCL_LIB/TK_LIB as well */
	unless ((path = getenv("BK_WISH")) && executable(path)) path = 0;
	unless (path) {
		if (gui_useAqua()) {
			path = aprintf(
			    "%s/gui/bin/BitKeeper.app/Contents/MacOS/BitKeeper",
			    bin);
		} else {
			path = aprintf("%s/gui/bin/bkgui", bin);
			if (executable(path)) {
				safe_putenv("TCL_LIBRARY=%s/gui/lib/tcl8.5",
				    bin);
				safe_putenv("TK_LIBRARY=%s/gui/lib/tk8.5",
				    bin);
			} else {
				free(path);
				path = 0;
			}
		}
	}
	unless (path) {
		fprintf(stderr, "Cannot find the graphical interpreter.\n");
		exit(1);
	}
	putenv("BK_GUI=YES");
	unless (gui_useDisplay()) {
		fprintf(stderr,
		    "Cannot find a display to use (set $DISPLAY?).\n");
		exit(1);
	}
	for (i = 0; readwrite[i]; i++) {
		if (streq(script, readwrite[i])) {
			sig_ignore();
			break;
		}
	}
	argv[ac=0] = path;
	if (strchr(script, '/')) {
		strcpy(cmd_path, script);
	} else {
                sprintf(cmd_path, "%s/gui/lib/%s", bin, script);
	}
	argv[++ac] = cmd_path;
	unless (exists(cmd_path)) {
		fprintf(stderr, "bk: %s command not found\n", script);
		exit(1);
	}

	/*
	 * Pass 1, get all the wish args first.
	 */
	for (i = 0; av[i]; i++) {
		if (ac >= (MAXARGS-10)) {
			fprintf(stderr, "bk: too many args\n");
			exit(1);
		}
		for (j = 0; wishargs[j].name; j++) {
			if (streq(wishargs[j].name, av[i])) {
				argv[++ac] = av[i];
				if (wishargs[j].hasarg) argv[++ac] = av[++i];
				break;
			}
		}
	}
	argv[++ac] = "--";
	/*
	 * Pass two, get all the other args.
	 */
	for (i = 0; av[i]; i++) {
		if (ac >= (MAXARGS-10)) {
			fprintf(stderr, "bk: too many args\n");
			exit(1);
		}
		for (j = 0; wishargs[j].name; j++) {
			if (streq(wishargs[j].name, av[i])) {
				break;
			}
		}
		if (wishargs[j].name) {
			if (wishargs[j].hasarg) i++;
			continue;
		}
		argv[++ac] = av[i];
	}
	argv[ac+1] = 0;
	if (streq(argv[ac], "--")) argv[ac] = 0;
	spawn_tcl = 1;
	if ((pid = spawnvp(_P_NOWAIT, argv[0], argv)) < 0) {
		fprintf(stderr, "bk: cannot spawn %s\n", argv[0]);
	}
	spawn_tcl = 0;
#ifdef	WIN32
	/*
	 * If we are about to call a GUI command hide the console
	 * since we won't be using it.  This is so that we don't have
	 * a unused console windows in the background of the GUIs.
	 * WARNING: after this we shouldn't try to do any console IO.
	 */
	FreeConsole();
#endif
	wret = waitpid(pid, &ret, 0);
	upgrade_maybeNag(0);
	if (wret < 0) {
		return (126);
	} else if (!WIFEXITED(ret)) {
		return (127);
	} else {
		return (WEXITSTATUS(ret));
	}
}

private void
showproc_start(char **av)
{
	int	i;
	char	*p;
	FILE	*f;

	// Make it so that SHOWTERSE works like SHOWPROC so you can use just 1
	if ((p = getenv("BK_SHOWTERSE")) && !getenv("BK_SHOWPROC")) {
		safe_putenv("BK_SHOWPROC=%s", p);
	}

	unless (f = efopen("BK_SHOWPROC")) return;
	unless (getenv("BK_SHOWTERSE")) {
		fprintf(f, "BK  (%5u %5s)%*s", getpid(), milli(), indent(), "");
	}
	for (i = 0; av[i]; ++i) fprintf(f, " %s", av[i]);
	fprintf(f, " [%s]", proj_cwd());
	fprintf(f, "\n");
	fclose(f);
}

private void
showproc_end(char *cmdlog_buffer, int ret)
{
	FILE	*f;
	kvpair	kv;

	/* We don't seem to have a matching lock event */
	unless (strneq(cmdlog_buffer, "\"remote nested\" unlock", 22)) {
		callstack_pop();
	}
	T_CMD("%s = %d", cmdlog_buffer, ret);
	unless (f = efopen("BK_SHOWPROC")) return;
	unless (getenv("BK_SHOWTERSE")) {
		fprintf(f, "END (%5u %7s)%*s", getpid(), milli(), indent(), "");
	}
	fprintf(f, " %s = %d", cmdlog_buffer, ret);
	if (notes) {
		fprintf(f, " (");
		EACH_KV(notes) fprintf(f, " %s=%s", kv.key.dptr, kv.val.dptr);
		fprintf(f, " )");
	}
	fprintf(f, " [%s]", proj_cwd());
	fprintf(f, "\n");
	fclose(f);
}

void
callstack_push(int remote)
{
	char *csp;
	char *at = (remote) ? "@" : "";

	if ((csp = getenv("_BK_CALLSTACK"))) {
		if (strcnt(csp, ':') >= MAXPROCDEPTH) {
			fprintf(stderr, "BK callstack: %s\n", csp);
			fprintf(stderr, "BK callstack too deep, aborting.\n");
			exit(1);
		}
		safe_putenv("_BK_CALLSTACK=%s:%s%s", csp, at, prog);
	} else {
		safe_putenv("_BK_CALLSTACK=%s%s", at, prog);
	}
}

void
callstack_pop(void)
{
	char	*p, *t;

	unless (p = getenv("_BK_CALLSTACK")) {
		if (getenv("_BK_DEVELOPER")) {
			fprintf(stderr,
			    "%s: pop of empty call stack\n", cmdlog_buffer);
		}
		return;
	}
	if (t = strrchr(p, ':')) {
		*t = 0;
	} else {
		p = "";
	}
	safe_putenv("_BK_CALLSTACK=%s", p);
}

void
log_rotate(char *path)
{
	struct stat	sb;
	int		size = 100*1024*1024;
	char		old[MAXPATH];

	if (stat(path, &sb)) return;

	if ((sb.st_mode & 0666) != 0666) chmod(path, 0666);
	if (streq(basenm(path), "cmd_log")) size = 5*1024*1024;
	if (sb.st_size > size) {
		sprintf(old, "%s.old", path);
		rename(path, old);
	}
}
