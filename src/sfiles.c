/*
 * TODO
 *	I want the directory/file stuff to be listed like ls in -v mode.
 *	Right now we do the dirs first and then the files.
 *	What I want is this:
 *	bk -R sfiles -gEv | sort +1
 */
/* Copyright (c) 2003 BitMover, Inc. */
#include "system.h"
#include "sccs.h"

#define	LSTATE	0	/* lock state: l,u */
#define	CSTATE	1	/* change state: c, ' ' */
#define	PSTATE	2	/* pending state: p,' ' */
#define	GSTATE	3	/* checked out state, 'G', ' ' */
#define	NSTATE	4	/* name state: n,' ' */
#define	DSTATE	5	/* directory state: d, D, ' ' */
#define	YSTATE	6	/* comments status: y, ' ' */

typedef struct winfo winfo;
typedef	char	STATE[8];

private	void	append_rev(MDBM *db, char *name, char *rev);
private int	chk_diffs(sccs *s);
private void	do_print(char state[6], char *file, char *rev);
private void	file(char *f);
private void	print_it(STATE state, char *file, char *rev);
private	void	print_summary(void);
private void	progress(int force);
private	void	sccsdir(winfo *wi);
private void	walk(char *dir);

typedef struct {
	u32     show_markers:1;		/* -v: show markers */
	u32     all:1;			/* -a: disable ignore list */
	u32	locked:1;		/* -l: list locked files */
	u32	unlocked:1;		/* -u: list unlocked files */
	u32	junk:1;			/* -j: list junk in SCCS dirs */
	u32	extras:1;		/* -x: list extra files */
	u32	changed:1;		/* -c: list changed files */
	u32	gotten:1;		/* -G: list checked out files */
	u32	names:1;		/* -n: list files in wrong path */
	u32	pending:1;		/* -p: list pending files */
	u32     gfile:1;     		/* print gfile name	*/
	u32     Aflg:1;			/* use with -p show	*/
					/* all pending deltas	*/
	u32     Cflg:1;     		/* want file<BK_FS>rev format	*/
	u32     dfile:1;   		/* use d.file to find 	*/
					/* pending delta	*/
	u32	dirs:1;			/* -d: list directories with BK files */
	u32	xdirs:1;		/* -D: list directories w/ no BK */
	u32	fixdfile:1;		/* fix up the dfile tag */
	u32	onelevel:1;		/* -1: this dir only */
	u32	progress:1;		/* if set, send progress to stdout */
	u32     summarize:1;     	/* summarize output only */
	u32     useronly:1;     	/* list user file only 	*/
	u32	timestamps:1;		/* whether to use the timestamp DB */
	u32	cfiles:1;		/* -y: list files with comments */
	FILE	*out;			/* send output here */
	char	*glob;			/* only files which match this */
} options;

private	jmp_buf	sfiles_exit;
private hash	*timestamps = 0;
private options	opts;
private char	**ignore, **dont_ignore;
private u32	d_count, s_count, x_count; /* progress counter */
private u32	s_last, x_last; /* progress counter */
private u32	D_count, C_count, c_count, n_count, p_count;

private char *
hasfile(char *file, char type, MDBM *sDB)
{
	file[0] = type;
	return (mdbm_fetch_str(sDB, file));
}

private inline sccs *
init(char *name, int flags, MDBM *sDB, MDBM *gDB)
{
        sccs    *s;

	if (sDB) {
		char *p = basenm(name);

		assert(gDB);
		flags |= INIT_NOSTAT;
		assert(*p == 's');
		if (hasfile(p, 'c', sDB)) flags |= INIT_HAScFILE;
		if (hasfile(p, 'p', sDB)) flags |= INIT_HASpFILE;
		if (hasfile(p, 'x', sDB)) flags |= INIT_HASxFILE;
		if (hasfile(p, 'z', sDB)) flags |= INIT_HASzFILE;
		if (mdbm_fetch_str(gDB, &p[2])) flags |= INIT_HASgFILE;
		*p = 's'; /* because hasfile() stomps */
	}
	if (strneq(name, "./", 2)) name += 2;
	s = sccs_init(name, flags);
        return (s);
}

private int
fastprint(char *file, struct stat *sb, void *data)
{
	puts(file + 2);
	return (0);
}

int
sfiles_main(int ac, char **av)
{
        int     c, i;
	char	*path, *s, buf[MAXPATH];

	if (setjmp(sfiles_exit)) return (1); /* error exit */

	if (ac == 1) {
		walksfiles(".", fastprint, 0);
		return (0);
	}

	while ((c = getopt(ac, av, "1acdDeEgGjlno:p|P|SuUvxy")) != -1) {
		switch (c) {
		    case '1':	opts.onelevel = 1; break;
		    case 'a':	opts.all = 1; break;		/* doc 2.0 */
		    case 'c':	opts.changed = opts.timestamps = 1;
		    		break;
		    case 'C':	opts.changed = 1;
				opts.timestamps = 0;
				break;
		    case 'd':	opts.dirs = 1; break;
		    case 'D':	opts.xdirs = 1; break;
		    case 'E':	opts.changed = opts.names = opts.pending = 1;
			    /* fall thru */
		    case 'e':	opts.dirs = opts.xdirs = opts.gotten =
				opts.junk = opts.locked = opts.unlocked =
				opts.show_markers = opts.extras =
				opts.cfiles = 1;
				break;
		    case 'g':	opts.gfile = 1; break;		/* doc 2.0 */
		    case 'G':	opts.gotten = 1; break;		/* doc */
		    case 'j':	opts.junk = 1; break;		/* doc 2.0 */
		    		/* XXX - should list BitKeeper/tmp stuff */
		    case 'l':   opts.locked = 1; break;		/* doc 2.0 */
		    case 'n':   opts.names = 1; break;		/* doc 2.0 */
		    case 'o':					/* doc 2.0 */
				unless (opts.out = fopen(optarg, "w")) {
		    			perror(optarg);
					exit(1);
				}
				opts.progress = 1;
				break;
		    case 'P':	opts.fixdfile = 1;	  	/* undoc 2.0 */
				/* fall thru */
		    case 'p':	opts.pending =1;		/* doc 2.0 */
				for (s = optarg; s && *s; s++) {
					if (*s == 'A') {
						opts.Aflg = 1;
						opts.Cflg = 1;
					} else if (*s == 'C') {
						opts.Cflg = 1;
					} else {
						goto usage;
					}
				}
				break;
		    case 'S':	opts.summarize = 1; break;
		    case 'u':	opts.unlocked = 1; break;	/* doc 2.0 */
		    case 'U':	opts.useronly = 1; break;	/* doc 2.0 */
		    case 'v':	opts.show_markers = 1; break;	/* doc 2.0 */
		    case 'x':					/* doc 2.0 */
				opts.extras = opts.junk = 1; break;
		    case 'y':	opts.cfiles = 1; break;		/* doc */
		    default:
usage:				system("bk help -s sfiles");
				return (1);
		}
	}
	/* backwards compat, remove in 4.0 */
	if (getenv("BK_NO_TIMESTAMPS")) {
		fprintf(stderr,
		    "Use bk sfiles -C instead of BK_NO_TIMESTAMPS\n");
		opts.timestamps = 0;
	}

	unless (opts.out) opts.out = stdout;
	fflush(opts.out); /* for win32 */
	C_count = c_count = p_count = d_count = s_count = x_count = 0;

	/*
	 * If user did not select any option,
	 * setup a default mode for them
	 */
	if (!opts.unlocked && !opts.locked && !opts.junk && !opts.extras &&
	    !opts.changed && !opts.pending && !opts.names &&
	    !opts.gotten && !opts.dirs && !opts.xdirs && !opts.cfiles) {
		opts.unlocked = 1;
		opts.locked = 1;
	}

	for (i = optind; av[i]; i++);
	i--;
	if (streq(av[i], "-")) i--;
	if ((i >= optind) && sfiles_glob(av[i])) {
		opts.glob = strdup(av[i]);
		if (av[i+1]) {
			av[i] = av[i+1];
			av[i+1] = 0;
		} else {
			av[i] = 0;
		}
	}
	if (!av[optind]) {
		path = ".";
		if (opts.names && !exists(BKROOT)) {
			fprintf(stderr,
			    "sfiles -n must be run at project root.\n");
			exit(1);
		}
		walk(path);
	} else if (streq("-", av[optind])) {
		setmode(0, _O_TEXT); /* read file list in text mode */
		while (fnext(buf, stdin)) {
			chop(buf);
			localName2bkName(buf, buf);
			path = buf;
                        if (isdir(path)) {
				walk(path);
			} else {
                                file(path);
			}
		}
	} else {
		if (opts.names && !exists(BKROOT)) {
			fprintf(stderr,
			    "sfiles -n must be run at project root.\n");
			exit(1);
		}
                for (i = optind; av[i]; ++i) {
                        localName2bkName(av[i], av[i]);
                        if (isdir(av[i])) {
                                path =  av[i];
                                walk(path);
                        } else {
                                path =  av[i];
                                file(path);
                        }
                }
	}
	if (opts.out) fclose(opts.out);
	if (opts.progress) progress(2);
	return (0);
}

private sccs *
chk_sfile(char *name, STATE state)
{
	char	*s;
	sccs	*sc = 0;

	s = strrchr(name, '/');

	if (s[1] == 's') {
		s[1] = 'c';
		if (opts.cfiles && exists(name)) state[YSTATE] = 'y';
		s[1] = 'p';
		if (exists(name)) {
			state[LSTATE] = 'l';
			s[1] = 's';
			if (opts.changed || opts.names) {
				sc = init(name, INIT_NOCKSUM, 0, 0);
			}
			if (opts.changed && sc && chk_diffs(sc)) {
				state[CSTATE] = 'c';
			}
			if (opts.names && sc) {
				delta	*d = sccs_top(sc);

				unless (d->pathname &&
				    patheq(sc->gfile, d->pathname)) {
					state[NSTATE] = 'n';
				}
			}
		} else {
			s[1] = 'z';
			if (exists(name)) {
				state[LSTATE] = 'l';
			} else {
				state[LSTATE] = 'u';
			}
			s[1] = 's';
			if (opts.names &&
			    (sc = init(name, INIT_NOCKSUM, 0, 0))) {
				delta	*d = sccs_top(sc);

				unless (d->pathname &&
					patheq(sc->gfile, d->pathname)) {
					state[NSTATE] = 'n';
				}
				sccs_free(sc);
				sc = 0;
			}
		}
	}
	return (sc);
}

private void
chk_pending(sccs *s, char *gfile, STATE state, MDBM *sDB, MDBM *gDB)
{
	delta	*d;
	int	local_s = 0, printed = 0;
	char	buf[MAXPATH], *dfile = 0, *p;


	if (opts.dfile) {
		if (sDB) {
			strcpy(buf, "d.");
			strcpy(&buf[2], basenm(gfile));
			unless (mdbm_fetch_str(sDB, buf)) {
				state[PSTATE] = ' ';
				do_print(state, gfile, 0);
				return;
			}
		} else {
			dfile = name2sccs(gfile);
			p = basenm(dfile);
			*p = 'd';
			unless (exists(dfile)) {
				free(dfile);
				state[PSTATE] = ' ';
				do_print(state, gfile, 0);
				return;
			}
		}
	}

	unless (s) {
		char *sfile = name2sccs(gfile);
		s = init(sfile, INIT_NOCKSUM, sDB, gDB);
		unless (s && HASGRAPH(s)) {
			fprintf(stderr, "sfiles: %s: bad sfile\n", sfile);
			free(sfile);
			if (s) sccs_free(s);
			if (dfile) free(dfile);
			return;
		}
		free(sfile);
		local_s = 1;
	}

	unless (dfile) {
		dfile = name2sccs(gfile);
		p = basenm(dfile);
		*p = 'd';
	}

	/*
	 * check for pending deltas
	 */
	state[PSTATE] = ' ';
	unless (d = sccs_top(s))  goto out;	
	if (d->flags & D_CSET) goto out;

	/*
	 * If it is out of view, we need to look at all leaves and see if
	 * there is a problem or not.
	 */
	if (s->defbranch && streq(s->defbranch, "1.0")) {
		for (d = s->table; d; d = d->next) {
			unless ((d->type == 'D') && sccs_isleaf(s, d)) {
				continue;
			}
			unless (d->flags & D_CSET) break;
		}
		unless (d) goto out;
		fprintf(stderr,
		    "Warning: not in view file %s skipped.\n", s->gfile);
		goto out;
	}

	assert(!(d->flags & D_CSET));
	state[PSTATE] = 'p';
	if (opts.Aflg) {
		do {
			do_print(state, gfile, d->rev);
			d = d->parent;
		} while (d && !(d->flags & D_CSET));
		printed = 1;
	} else if (opts.Cflg) {
		do_print(state, gfile, d->rev);
		printed = 1;
	}

out:	unless (printed) do_print(state, gfile, 0);
	/*
	 * Do not sccs_free() if it is passed in from outside
	 */
	if (local_s) sccs_free(s);
	if (opts.fixdfile) {
		/* No pending delta, remove redundant d.file */
		unless (state[PSTATE] == 'p') {
			unlink(dfile);
		} else {
			touch(dfile, 0666);
		}
	}
	if (dfile) free(dfile);
}

private void
file(char *f)
{
	char	name[MAXPATH], buf[MAXPATH];
	char    *s, *sfile;
	STATE	state = "       ";
	sccs	*sc = 0;

	if (strlen(f) >= sizeof(name)) {
		fprintf(stderr, "sfiles: name too long: [%s]\n", f);
		return;
	}
	strcpy(name, f);
	s = rindex(name, '/');
	/*
	 * There are three possible condition
	 * a) f is a sfile
	 * b) f is a regular or new gfile
	 * c) f is a junk file in the SCCS directory
	 */
	if (s && (name <= &s[-4]) && pathneq("SCCS", &s[-4], 4)) {
		/* this file is under a SCCS dir */
		unless (sccs_filetype(f) == 's') {
			state[CSTATE] = 'j';
			strcpy(buf, f);
		} else {
			sc = chk_sfile(f, state);
			s = sccs2name(f);
			strcpy(buf, s);
			free(s);
		}
	} else {
		/*
		 * TODO: we need to check for the case where
		 * the pwd is a SCCS dir
		 * This can fool the current code into wronly treating them
		 * as xtras.
		 */
		/* this file is not in SCCS/s. form */
		sfile = name2sccs(f);
		unless (exists(sfile)) {
			state[CSTATE] = 'x';
		} else {
			sc = chk_sfile(sfile, state);
		}
		free(sfile);
		strcpy(buf, f);
	}

	/*
	 * When we get here. buf contains the gname
	 * Now we check for pending deltas
	 */
	if (opts.pending && state[CSTATE] != 'x' &&  state[CSTATE] != 'j') {
		if (opts.gotten && exists(buf)) state[GSTATE] = 'G';
		chk_pending(sc, buf, state, 0, 0);
	} else  {
		if (state[CSTATE] == 'x' || state[CSTATE] == 'j') {
			if (exists(buf)) {
				state[LSTATE] = state[PSTATE] = ' ';
				do_print(state, buf, 0);
			}
		} else {
			state[PSTATE] = ' ';
			if (opts.gotten && exists(buf)) state[GSTATE] = 'G';
			do_print(state, buf, 0);
		}
	}
	if (sc) sccs_free(sc);
}

private void
print_summary(void)
{
	fprintf(opts.out, "%6d files under revision control.\n", s_count);
	if (opts.extras) {
		fprintf(opts.out,
		    "%6d files not under revision control.\n", x_count);
	}
	if (d_count) {
		fprintf(opts.out,
		    "%6d directories containing revision controlled files.\n",
		    d_count);
	}
	if (D_count) {
		fprintf(opts.out,
		    "%6d directories with no revision controlled files.\n",
		    D_count);
	}
	if (opts.gotten) {
		fprintf(opts.out, "%6d files checked out.\n", C_count);
	}
	if (opts.changed) {
		fprintf(opts.out,
		    "%6d files modified and not checked in.\n", c_count);
	}
	if (opts.pending) {
		fprintf(opts.out,
		    "%6d files with checked in, but not committed, deltas.\n",
		    p_count);
	}
	if (opts.names) {
		fprintf(opts.out,
		    "%6d files in incorrect locations.\n", n_count);
	}
}

struct winfo {
	char	**sfiles;
	MDBM	*sDB, *gDB;
	char	*sccsdir;
	int	sccsdirlen;
	project	*proj;
	int	seenfirst;
};

private void
add_to_winfo(winfo *wi, char *file, int sccs)
{
	char	*p;

	unless (wi->sDB) wi->sDB = mdbm_mem();
	unless (wi->gDB) wi->gDB = mdbm_mem();

	if (sccs) {
		if (pathneq("s.", file, 2)) {
			wi->sfiles = addLine(wi->sfiles, strdup(file));
		} else {
			if (pathneq("c.", file, 2)) {
				/*
				 * If there is no @rev, make sure we
				 * don't miss the case where we have
				 * both a c.file and c.file@rev
				 */
				if (p = strrchr(file, '@')) {
					*p++ = 0;
				} else {
					p = "";
				}
				/*
				 * Special handling for c.file@rev entry
				 * append the @rev part to the value field
				 * so we can print the correct file
				 * name if it turns out to be a junk file.
				 */
				append_rev(wi->sDB, file, p);
				return;
			}
		}
	}
	mdbm_store_str((sccs ? wi->sDB : wi->gDB), file, "", MDBM_INSERT);
}

private int
sfiles_walk(char *file, struct stat *sb, void *data)
{
	winfo	*wi = (winfo *)data;
	char	*p;
	int	n;
	int	nonsccs;

	if (S_ISDIR(sb->st_mode)) {
		/*
		 * Special processing for .bk_skip file
		 */
		if (sfiles_skipdir(file)) return (-1);

		n = strlen(file);

		/* Skip new repo */
		if (wi->sccsdir) {
			strcpy(&file[n], "/" BKROOT);
			if (exists(file)) return (-1);
			file[n] = 0;
		}

		/* if SCCS dir start new processing */
		p = 0;
		if ((p = strrchr(file, '/')) && patheq(p+1, "SCCS")) {
			if (wi->sccsdir) sccsdir(wi);
			wi->sccsdir = strdup(file);
			if (p = strrchr(wi->sccsdir, '/')) *++p = 0;
			wi->sccsdirlen = strlen(wi->sccsdir);
		} else {
			if (opts.dirs || opts.xdirs) {
				strcpy(&file[n], "/SCCS");
				nonsccs = !exists(file) || emptyDir(file);
				file[n] = 0;
				if (opts.xdirs && nonsccs) {
					D_count++;
					unless (opts.summarize) {
						print_it("     D ", file, 0);
					}
				}
				if (opts.dirs && !nonsccs && !opts.summarize) {
					print_it("     d ", file, 0);
				}
			}
			if (opts.onelevel) {
				if (wi->seenfirst) return (-1);
				wi->seenfirst = 1;
			}
		}
	} else {
		/* are we in the same SCCS dir? then save */
		if (wi->sccsdir &&
		    pathneq(file, wi->sccsdir, wi->sccsdirlen)) {
			int	sccs = 0;

			p = file + wi->sccsdirlen;
			if (pathneq(p, "SCCS/", 5)) {
				sccs = 1;
				p += 5;
			}
			if (!strchr(p, '/')) {
				add_to_winfo(wi, p, sccs);
				return (0);
			}
		}
		/* else just printit */
		do_print("xxxxxxx", file, 0);
	}
	return (0);
}

private void
walk(char *dir)
{
	char	tmp[MAXPATH];
	winfo	wi = {0};

	if (wi.proj = proj_init(dir)) {
		if (!opts.all) {
			FILE	*ignoref;

			sprintf(tmp, "%s/BitKeeper/etc/ignore",
			    proj_root(wi.proj));
			unless (exists(tmp)) get(tmp, SILENT, "-");
			if (ignoref = fopen(tmp, "rt")) {
				ignore = read_globs(ignoref, 0);
				fclose(ignoref);
			}
			dont_ignore = addLine(0,
			    strdup("BitKeeper/etc/gone"));
			dont_ignore = addLine(dont_ignore,
			    strdup("./BitKeeper/etc/gone"));
		}
		unless (opts.fixdfile) {
			sprintf(tmp, "%s/%s", proj_root(wi.proj), DFILE);
			opts.dfile = exists(tmp);
		}
	} else {
		/*
		 * Dir is not a BitKeeper repository,
		 * turn off all BitKeeper specific feature.
		 */
		opts.all = 1;
	}
#if 0
	/*
	 * XXX TODO: should we reset the progress counter ?
	 */
	s_count = x_count = 0;
#endif

	walkdir(dir, sfiles_walk, &wi);
	if (wi.sccsdir) sccsdir(&wi);

	if (ignore) free_globs(ignore);  ignore = 0;
	if (dont_ignore) free_globs(dont_ignore);  dont_ignore = 0;
	if (opts.timestamps && timestamps && wi.proj) {
		dumpTimestampDB(wi.proj, timestamps);
		hash_free(timestamps);
		timestamps = 0;
	}
	if (opts.summarize) print_summary();
	if (wi.proj) proj_free(wi.proj);

	/*
	 * We only enable fast scan mode if we started at the root
	 */
	if (opts.fixdfile && isdir("BitKeeper/etc/SCCS")) {
		enableFastPendingScan();
	}
}

private void
progress(int force)
{
	char	buf[100];

	if (!force && (s_last == s_count) && (x_last == x_count)) return;
	sprintf(buf, "%d %d %d %d\n", s_count, x_count, d_count, c_count);
	/* If we get an error, it usually means that we are to die */
	if (write(1, buf, strlen(buf)) != strlen(buf)) exit(1);
	s_last = s_count;
	x_last = x_count;
	if (force == 2) {
		usleep(300000);		/* let TK update */
	}
}

private int
chk_diffs(sccs *s)
{
	int different;

	if (!s) return (0);
	different = (sccs_hasDiffs(s, 0, 1) >= 1);
	if (timestamps) {
		updateTimestampDB(s, timestamps, different);
	}
	return (different);
}

private int
isIgnored(char *file)
{
	char *gfile, *p, *q, save;
	struct stat sbuf;
	int len;

	gfile =  strneq("./",  file, 2) ? &file[2] : file;
	unless (opts.all || match_globs(file, dont_ignore, 0)) {
		if (match_globs(file, ignore, 0)) {
			debug((stderr, "SKIP\t%s\n", file));
			return (1);
		}

		/*
		 * For backward compat with "bk sfiles"
		 * trimed "./" and match against ignore list.
		 */
		if ((gfile !=  file) && match_globs(gfile, ignore, 0)) {
			debug((stderr, "SKIP\t%s\n", gfile));
			return (1);
		}

		/*
		 * For backward compat with "bk sfiles"
		 * match basename/dirname against ignore list.
		 */
		p = gfile;
		while (p) {
			for (q = p; *q && (*q != '/'); q++);
			save = *q;
			*q = 0;
			if (match_globs(p, ignore, 0)) {
				*q = save;
				debug((stderr, "SKIP\t%s\n", gfile));
				return (1);
			}
			*q = save;
			if (save == 0) break;
			p = ++q;
		}

		/* ignore special file e.g. char/block/fifo file */
		if (lstat(gfile, &sbuf)) return (1);
		unless (sbuf.st_mode && S_IFREG|S_IFLNK) return (1);
	}

	/*
	 * HACK to hide stuff in the log and locks directory
	 * This assumes that sfiles is ran from project root
	 * If you run "bk sfiles" under <project root>/BitKeeper directory,
	 * these file will show up. It is probably OK.
	 */
	len = strlen(gfile);
	if ((len >= 14) && pathneq("BitKeeper/log/", gfile, 14)) return (1);
	if ((len >= 17) && pathneq("BitKeeper/writer/", gfile, 17)) return (1);
	if ((len >= 18) && pathneq("BitKeeper/readers/", gfile, 18)) return (1);
	if ((len >= 8) && pathneq("PENDING/", gfile, 8)) return (1);

	return (0);
}

private int
isTagFile(char *file)
{
	char *gfile;

	gfile =  strneq("./",  file, 2) ? &file[2] : file;
	return (pathneq("BitKeeper/etc/SCCS/x.", gfile, 21));
}

private int
isBkFile(char *gfile)
{
	if (streq(gfile, "ChangeSet") && isdir(BKROOT)) return (1);
	if (strneq(gfile, "BitKeeper/", 10) &&
	    !strneq(gfile, "BitKeeper/triggers/", 19) && isdir(BKROOT)) {
		return (1);
	}
	return (0);
}


private void
print_it(STATE state, char *file, char *rev)
{
	char	*sfile, *gfile;

	gfile =  strneq("./",  file, 2) ? &file[2] : file;
	if ((opts.useronly) && isBkFile(gfile)) return;

	if (opts.show_markers) {
		if (state[CSTATE] == 'j') {
			assert(streq(state, " j     "));
			if (fprintf(opts.out, "jjjjjjj ") != 8) {
error:				perror("output error");
				fflush(stderr);
				longjmp(sfiles_exit, 1); /* back to sfiles_main */
			}
		} else {
			STATE	tmp;
			char	*p;

			strcpy(tmp, state);
			for (p = tmp; *p; p++) if (*p == ' ') *p = '-';
			if (fprintf(opts.out, "%s ", tmp) != 8) {
				ttyprintf("STATE[%s]\n", tmp);
				goto error;
			}
		}
	}
	if (opts.gfile || (state[CSTATE] == 'x') || (state[CSTATE] == 'j') ||
	    (state[DSTATE] != ' '))  {
		if (fputs(gfile, opts.out) < 0) goto error;
	} else {
		sfile = name2sccs(gfile);
		if (fputs(sfile, opts.out) < 0) goto error;
		free(sfile);
	}
	if (rev) {
		int rlen = strlen(rev) + 1;
		if (fprintf(opts.out, "%c%s", BK_FS, rev) != rlen) goto error;
	}
	if (fputs("\n", opts.out) < 0) goto error;
}

private void
do_print(STATE state, char *file, char *rev)
{
	if (opts.glob) {
		char	*p;

		if (p = strrchr(file, '/')) {
			p++;
		} else {
			p = file;
		}
		/* XXX - make this conditional on !directory */
		if (streq("s.", p)) p += 2;
		unless (match_one(p, opts.glob, 0)) return;
	}
	if (state[PSTATE] == 'p') p_count++;
	if (state[NSTATE] == 'n') n_count++;
	if (state[GSTATE] == 'G') C_count++;
	switch (state[CSTATE]) {
	    case 'j': break;
	    case 'x': unless (isIgnored(file)) x_count++; break;
	    case 'c': C_count++; s_count++; c_count++; break;
	    case ' ': s_count++; break;
	    default: assert("state[CSTATE] unknown" == 0);
	}
	if (opts.progress &&
	    (((s_count - s_last) > 100) || ((x_count - x_last) > 100))) {
		progress(1);
	}
	if (opts.summarize) return; /* skip the detail */
	if ((state[PSTATE] == 'p') && opts.pending) goto print;

	switch (state[LSTATE]) {
	    case 'l':	if (opts.locked) goto print; break;
	    case 'u':	if (opts.unlocked) goto print; break;
	}

	switch (state[CSTATE]) {
	    case 'c':	if (opts.changed || opts.gotten) goto print; break;
	    case 'j':	if (opts.junk && !isTagFile(file)) goto print; break;
	    case 'x':	if (opts.extras && !isIgnored(file)) goto print; break;
	}

	if ((state[NSTATE] == 'n') && opts.names) goto print;
	if ((state[GSTATE] == 'G') && opts.gotten) goto print;
	if ((state[YSTATE] == 'y') && opts.cfiles) goto print;
	return;

print:	print_it(state, file, rev);
}

private void
append_rev(MDBM *db, char *name, char *rev)
{
	char	*buf = 0;
	char	*t;

	t = mdbm_fetch_str(db, name);
	if (t) {
		t = buf = aprintf("%s,%s", t, rev);
	} else {
		t = rev;
	}
	mdbm_store_str(db, name, t, MDBM_REPLACE);
	if (buf) free(buf);
}

/*
 * Called for each directory that has an SCCS subdirectory
 */
private void
sccsdir(winfo *wi)
{
	char	*dir = wi->sccsdir;
	char	**slist = wi->sfiles;
	MDBM	*gDB = wi->gDB;
	MDBM	*sDB = wi->sDB;
	char	*p, *gfile;
	datum	k;
	sccs	*s = 0;
	int	i;
	char	buf[MAXPATH];

	d_count++;
	if (opts.progress) progress(1);

	/*
	 * First eliminate as much as we can from SCCS dir;
	 * the leftovers in the gDB should be extras.
	 *
	 * Note: We use the fifo to loop thru the sfile
	 * because if we use mdbm_frist/next, we cannot delete
	 * entry while we are in the first/next loop, it screw up
	 * the mdbm internal index.
	 */
	EACH (slist) {
		char 	*file;
		STATE	state = "       ";

		p = slist[i];
		s = 0;
		file = p;
		gfile = &file[2];

		/*
		 * look for p.file,
		 */
		file[0] = 'p';
		if (mdbm_fetch_str(sDB, file)) {
			char *gfile;	/* a little bit of scope hiding ... */
			char *sfile;

			state[LSTATE] = 'l';
			file[0] = 's';
			concat_path(buf, dir, "SCCS");
			concat_path(buf, buf, file);
			sfile = buf;
			if (strneq(sfile, "./", 2)) sfile += 2;
			gfile = sccs2name(sfile);
			if (opts.changed && opts.timestamps &&
			    wi->proj && !timestamps) {
				timestamps = generateTimestampDB(wi->proj);
			}
			if (opts.changed &&
			    (!timestamps ||
				!timeMatch(wi->proj, gfile, sfile,
				    timestamps)) &&
			    (s = init(buf, INIT_NOCKSUM, sDB, gDB)) &&
			    chk_diffs(s)) {
				state[CSTATE] = 'c';
			}
			free(gfile);
		} else {
			file[0] = 'z';
			if (mdbm_fetch_str(sDB, file)) {
				state[LSTATE] = 'l';
			} else {
				state[LSTATE] = 'u';
			}
			file[0] = 's';
			concat_path(buf, dir, "SCCS");
			concat_path(buf, buf, file);
			if (opts.names &&
			    (s = init(buf, INIT_NOCKSUM, sDB, gDB))) {
				delta	*d = sccs_top(s);

				unless (d->pathname &&
				    patheq(s->gfile, d->pathname)) {
					state[NSTATE] = 'n';
				}
				sccs_free(s);
				s = 0;
			}
		}
		if (opts.gotten && mdbm_fetch_str(gDB, &file[2])) {
		    	state[GSTATE] = 'G';
		}
		if (opts.cfiles) {
			file[0] = 'c';
			if (mdbm_fetch_str(sDB, file)) state[YSTATE] = 'y';
			file[0] = 's';
		}
		concat_path(buf, dir, gfile);
		if (opts.pending) {
			/*
			 * check for pending deltas
			 */
			chk_pending(s, buf, state, sDB, gDB);
		} else {
			state[PSTATE] = ' ';
			do_print(state, buf, 0);
		}
		if (s) sccs_free(s);
	}
	freeLines(slist, free);

	/*
	 * Check the sDB for "junk" file
	 * XXX TODO: Do we consider the r.file and m.file "junk" file?
	 */
	if (opts.junk) {
		kvpair  kv;
		concat_path(buf, dir, "SCCS");
		EACH_KV (sDB) {
			char buf1[MAXPATH];

			/*
			 * Ignore x.files is the SCCS dir if the matching
			 * s.file exists.
			 */
			if (kv.key.dptr[1] == '.') {
				switch(kv.key.dptr[0]) {
				    case 's': continue;
				    case 'c': case 'd': case 'p':
				    case 'x': case 'z':
					strcpy(buf1, kv.key.dptr);
					buf1[0] = 's';
					if (mdbm_fetch_str(sDB, buf1)) {
						continue;
					}
				}
			}
			concat_path(buf1, buf, kv.key.dptr);
			if (kv.val.dsize == 0) {
				do_print(" j     ", buf1, 0);
			} else {
				/*
				 * We only get here when we get
				 * c.file@rev entries. Extract the @rev part
				 * from kv.val.ptr and append it to buf1 to
				 * reconstruct the correct file name.
				 * i.e c.file@rev
				 */
				p = kv.val.dptr;
				while (p) {
					char *q, *t, buf2[MAXPATH];

					q = strchr(p, ',');
					if (q) *q++ = 0;
					if (*p) {
						t = buf2;
						sprintf(buf2, "%s@%s", buf1, p);
					} else {
						t = buf1;
					}
					do_print(" j     ", t, 0);
					p = q;
				}
			}
		}
	}

	/*
	 * Everything left in the gDB is extra
	 */
	if (opts.extras) {
		for (k = mdbm_firstkey(gDB); k.dsize != 0;
						k = mdbm_nextkey(gDB)) {
			buf[0] = 's'; buf[1] = '.';
			strcpy(&buf[2], k.dptr);
			if (mdbm_fetch_str(sDB, buf)) continue;
			concat_path(buf, dir, k.dptr);
			do_print("xxxxxxx", buf, 0);
		}
	}
	mdbm_close(wi->gDB);
	wi->gDB = 0;
	mdbm_close(wi->sDB);
	wi->sDB = 0;
	free(wi->sccsdir);
	wi->sccsdir = 0;
	wi->sfiles = 0;
}

/*
 * Enable fast scan mode for pending file (base on d.file)
 * Caller must ensure we at at the project root
 */
void
enableFastPendingScan(void)
{
	touch(DFILE, 0666);
}

/*
 * Return true if we should skip this directory (it contains .bk_skip).
 * If there is an SCCS directory without a SCCS/.bk_skip then complain,
 * and return false, because that is likely to be a mistake.
 */
int
sfiles_skipdir(char *dir)
{
	char	*p;
	char	buf[MAXPATH];

	/* No .bk_skip? */
	snprintf(buf, sizeof(buf), "%s/%s", dir, BKSKIP);
	unless (exists(buf)) return (0);

	/* There is a .bk_skip; if we are in SCCS already then we skip */
	if ((p = strrchr(dir, '/')) && streq(p, "/SCCS")) return (1);

	/* We are not in SCCS; if there is no SCCS dir then skip */
	snprintf(buf, sizeof(buf), "%s/SCCS", dir);
	unless (isdir(buf)) return (1);

	/* We're not in SCCS, there is an SCCS, if SCCS/.bk_skip then skip */
	snprintf(buf, sizeof(buf), "%s/SCCS/%s", dir, BKSKIP);
	if (exists(buf)) return (1);

	/* OK, .bk_skip without SCCS/.bk_skip - flag it and keep going */
	getMsg("bk_skip_and_sccs", dir, 0, stderr);
	return (0);
}

typedef struct sinfo sinfo;
struct sinfo {
	walkfn	fn;
	void	*data;
	int	rootlen;
};

private int
findsfiles(char *file, struct stat *sb, void *data)
{
	char	*p = strrchr(file, '/');
	sinfo	*si = (sinfo *)data;

	unless (p) return (0);
	if (S_ISDIR(sb->st_mode)) {
		if (p - file > si->rootlen && patheq(p+1, "BitKeeper")) {
			/*
			 * Do not cross into other package roots
			 * (e.g. RESYNC).
			 */
			strcat(file, "/etc");
			if (exists(file)) return (-2);
		}
	} else {
		if ((p - file >= 6) && pathneq(p - 5, "/SCCS/s.", 8)) {
			return (si->fn(file, sb, si->data));
		} else if (patheq(p+1, BKSKIP)) {
			/*
			 * Skip directory containing a .bk_skip file
			 */
			p[1] = 0;
			if (sfiles_skipdir(file)) return (-2);
		}
	}
	return (0);
}

int
walksfiles(char *dir, walkfn fn, void *data)
{
	sinfo	si;

	si.fn = fn;
	si.data = data;
	si.rootlen = strlen(dir);
	return (walkdir(dir, findsfiles, &si));
}
