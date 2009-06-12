/* Copyright (c) 2003 BitMover, Inc. */
// LMXXX - The output should be stuffed into a lines array and sorted on a per
// directory basis.  We get it in the right order and then we screw it up.
// After beta.
#include "system.h"
#include "sccs.h"
#include "bam.h"
#include "range.h"

#define	TSTATE	0	/* type: d/D/i/j/r/s/x */
#define	LSTATE	1	/* lock state: l,u */
#define	CSTATE	2	/* change state: c, ' ' */
#define	PSTATE	3	/* pending state: p,' ' */
#define	GSTATE	4	/* checked out state, 'G', ' ' */
#define	NSTATE	5	/* name state: n,' ' */
#define	YSTATE	6	/* comments status: y, ' ' */

typedef struct winfo winfo;
typedef	char	STATE[8];

private	void	append_rev(MDBM *db, char *name, char *rev);
private int	chk_diffs(sccs *s);
private void	do_print(char state[6], char *gfile, char *rev);
private void	file(char *f);
private void	print_it(STATE state, char *file, char *rev);
private	void	print_summary(void);
private void	progress(int force);
private	void	sccsdir(winfo *wi);
private void	walk(char *dir);
private	void	load_project(char *dir);
private	void	free_project(void);
private void	load_ignore(project *p);
private	void	ignore_file(char *file);
private	void	print_components(void);

typedef struct {
	u32	Aflg:1;			/* -pA: show all pending deltas */
	u32	all:1;			/* -a: disable ignore list */
	u32	cfiles:1;		/* -y: list files with comments */
	u32	Cflg:1;			/* -pC: want file<BK_FS>rev format */
	u32	changed:1;		/* -c: list changed files */
	u32	dfile:1;		/* trust d.file for pending deltas */
	u32	dirs:1;			/* -d: list directories with BK files */
	u32	extras:1;		/* -x: list extra files */
	u32	fixdfile:1;		/* -P: fix up the dfile tag */
	u32	gfile:1;		/* -g: print gfile name */
	u32	gotten:1;		/* -G: list checked out files */
	u32	ignored:1;		/* -i: list ignored (extra) files */
	u32	junk:1;			/* -j: list junk in SCCS dirs */
	u32	locked:1;		/* -l: list locked files */
	u32	names:1;		/* -n: list files in wrong path */
	u32	null:1;			/* -0: put nulls after sfiles */
	u32	onelevel:1;		/* -1: this dir only, don't recurs */
	u32	pending:1;		/* -p: list pending files */
	u32	progress:1;		/* -o: send progress to stdout */
	u32	sfiles:1;		/* -s: we want sfiles */
	u32	verbose:1;		/* -v: show markers */
	u32	subrepos:1;		/* -R: we want subrepo roots */
	u32	summarize:1;		/* -S: summarize output only */
	u32	timestamps:1;		/* whether to use the timestamp DB */
	u32	unlocked:1;		/* -u: list unlocked files */
	u32	useronly:1;		/* -U: list user files only */
	u32	xdirs:1;		/* -D: list directories w/ no BK */
	u32	inverse:1;		/* -!: list the opposite */
	u32	skip_comps:1;		/* -h: list only files really "here" */
	u32	atRoot:1;		/* running at root of repo? */

	char	*prefix;		/* set from env for path prefix */
	FILE	*out;			/* -o<file>: send output here */
	char	*glob;			/* only files which match this */
} options;

/* don't use -z, bk.c needs it. */

private	jmp_buf	sfiles_exit;
private hash	*timestamps = 0;
private options	opts;
private	project	*proj, *prodproj;

private	char	**ignore;	/* list of file pathname globs to ignore */
private	char	**ignorebase;	/* list of file basename globs to ignore */
private	char	**prunedirs;	/* set of dirs to prune */
private	char	**components;	/* list of subcomponents */

private u32	d_count, s_count, x_count; /* progress counter */
private u32	s_last, x_last; /* progress counter */
private u32	R_count, D_count, C_count, c_count, n_count, p_count, i_count;

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

		flags |= INIT_NOSTAT;
		assert(*p == 's');
		if (hasfile(p, 'c', sDB)) flags |= INIT_HAScFILE;
		if (hasfile(p, 'p', sDB)) flags |= INIT_HASpFILE;
		if (hasfile(p, 'x', sDB)) flags |= INIT_HASxFILE;
		if (hasfile(p, 'z', sDB)) flags |= INIT_HASzFILE;
		if (gDB && mdbm_fetch_str(gDB, &p[2])) flags |= INIT_HASgFILE;
		*p = 's'; /* because hasfile() stomps */
	}
	if (strneq(name, "./", 2)) name += 2;
	s = sccs_init(name, flags);
        return (s);
}

private int
fastprint(char *file, struct stat *sb, void *data)
{
	int	len;

	file += 2;
	/* if (file =~ /\/SCCS\/s.ChangeSet$/) */
	len = strlen(file);
	if ((len > strlen(CHANGESET)) &&
	    streq(file + len - strlen(CHANGESET) - 1, "/" CHANGESET)) {
		/* pick out subcomponents */
		file[len - strlen(CHANGESET) - 1] = 0;
		components = addLine(components, strdup(file));
	} else {
		puts(file);
	}
	return (0);
}

int
sfiles_main(int ac, char **av)
{
        int     c, i;
	char	*path, *s, buf[MAXPATH];

	if (setjmp(sfiles_exit)) {
		return (1); /* error exit */
	}

	/* pass in path/to/component/ and note the trailing slash */
	opts.prefix = getenv("_BK_PREFIX");

	if ((ac == 1) && !opts.prefix)  {
		opts.sfiles = 1;
		walksfiles(".", fastprint, 0);
		load_project(".");
		print_components();
		free_project();
		return (0);
	}

	while ((c = getopt(ac, av, "^01acCdDeEgGhijlno:p|P|RsSuUvxy")) != -1) {
		switch (c) {
		    case '^':	opts.inverse = 1; break;
		    case '0':	opts.null = 1; break;		/* doc */
		    case '1':	opts.onelevel = 1; break;
		    case 'a':	opts.all = 1; break;		/* doc 2.0 */
		    case 'c':	opts.changed = opts.timestamps = 1;
		    		break;
		    case 'C':	opts.changed = 1;
				opts.timestamps = 0;
				break;
		    case 'd':	opts.dirs = 1; break;
		    case 'D':	opts.xdirs = 1; break;
		    case 'S':	opts.summarize = 1; /* fall through */
		    case 'e':	/* all of these do the same thing, which is */
		    case 'E':	/* to show everything in verbose format */
				opts.cfiles = opts.changed = opts.dirs =
				opts.extras = opts.gotten = opts.ignored =
				opts.junk = opts.locked = opts.names = 
				opts.pending = opts.sfiles = opts.subrepos =
				opts.unlocked = opts.xdirs = 1;
				/* fall through */
		    case 'v':	opts.verbose = 1; break;
		    case 'g':	opts.gfile = 1; break;		/* doc 2.0 */
		    case 'G':	opts.gotten = 1; break;		/* doc */
		    case 'h':	opts.skip_comps = 1; break;
		    case 'i':	opts.ignored = 1; break;
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
		    case 'R':	opts.subrepos = 1; break;
		    case 's':	opts.sfiles = 1; break;
		    case 'u':	opts.unlocked = 1; break;	/* doc 2.0 */
		    case 'U':	opts.useronly = 1; break;	/* doc 2.0 */
		    case 'x':	opts.extras = 1; break;		/* doc */
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
	if (opts.summarize) opts.verbose = 0;

	/*
	 * If user did not select any option,
	 * setup a default mode for them
	 */
#define	attributes	\
		(opts.cfiles || opts.changed || opts.gotten || opts.ignored || \
		opts.locked || opts.names || opts.pending || opts.unlocked)

	unless (attributes || opts.dirs ||
	    opts.extras || opts.junk || opts.subrepos || opts.xdirs) {
		opts.sfiles = 1;
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
	unless (av[optind]) {
		path = ".";
		/* flag running at the root of repo */
		opts.atRoot = isdir(BKROOT);
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
				/*
				 * Kind of hokey but we load the proj based on
				 * the first filename we see and hope that they
				 * give us a set of files all in the same proj.
				 */
				unless (proj) {
					s = dirname_alloc(path);
					load_project(s);
					free(s);
				}
                                file(path);
			}
		}
	} else {
                for (i = optind; av[i]; ++i) {
                        localName2bkName(av[i], av[i]);
                        if (isdir(av[i])) {
                                path = av[i];
                                walk(path);
                        } else {
                                path = av[i];
				unless (proj) {
					s = dirname_alloc(path);
					load_project(s);
					free(s);
				}
                                file(path);
                        }
                }
	}
	print_components();
	if (opts.out != stdout) fclose(opts.out);
	if (opts.progress) progress(2);
	free_project();
	return (0);
}

/* only called from file().  Used to get state of sfile */
private sccs *
chk_sfile(char *name, STATE state)
{
	char	*s, *relp;
	delta	*d;
	sccs	*sc = 0;

	s = strrchr(name, '/');

#define	DOIT	(opts.verbose || opts.changed || opts.names)
	if (s[1] == 's') {
// LMXXX - we could optimize this further.  If they are not looking for
// opts.sfiles or opts.changed or opts.locked or opts.names then we won't
// need to init the sfile.
		if (DOIT) {
			sc = init(name, INIT_NOCKSUM, 0, 0);
			unless (sc && HASGRAPH(sc)) {
				sccs_free(sc);
				sc = 0;
			}
		}
		s[1] = 'c';
		if (exists(name)) state[YSTATE] = 'y';
		s[1] = 'p';
		state[TSTATE] = 's';
		if (exists(name)) {
			state[LSTATE] = 'l';
			if (DOIT && sc && chk_diffs(sc)) {
				state[CSTATE] = 'c';
			}
		} else {
			s[1] = 'z';
			if (exists(name)) {
				state[LSTATE] = 'l';
			} else {
				state[LSTATE] = 'u';
			}
		}
		s[1] = 's';
		if (DOIT && sc) {
			d = sccs_top(sc);
			relp = proj_relpath(proj, name);
			unless (d->pathname && patheq(relp, d->pathname)) {
				state[NSTATE] = 'n';
			}
			free(relp);
		}
	}
	return (sc);
}

private int
pending_print(sccs *s, delta *d, void *token)
{
	char	**data = token;

	do_print(data[0], data[1], d->rev);
	return (0);
}

private void
chk_pending(sccs *s, char *gfile, STATE state, MDBM *sDB, MDBM *gDB)
{
	delta	*d;
	int	local_s = 0, printed = 0;
	char	buf[MAXPATH], *dfile = 0, *p;

	if (streq(gfile, "./ChangeSet")) {
		state[PSTATE] = ' ';
		do_print(state, gfile, 0);
		return;
	}
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
		char	*data[2] = {state, gfile};

		/* find latest cset mark */
		for (d = s->table; d; d = d->next) {
			if (d->flags & D_CSET) break;
		}
		/* and walk all revs not included in that... */
		range_walkrevs(s, d, 0, pending_print, data);
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

/*
 * process a file on the sfiles command line or read from stdin.
 * The global 'proj' should be set to the project that matches these files.
 */
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
	if (s = rindex(name, '/')) s -= 4;      /* point it at start of SCCS/ */
	unless (s >= name) s = 0;
	/*
	 * There are three possible condition
	 * a) f is a sfile
	 * b) f is a regular or new gfile
	 * c) f is a junk file in the SCCS directory
	 */
	if (s && strneq(s, "SCCS/", 5) && ((s == name) || (s[-1] == '/'))) {
		*s = 0;
		strcpy(buf, name);	/* not including SCCS/whatever */
		*s = 'S';
		s += 4;			/* points at '/' */

		switch (sccs_filetype(f)) {
		    case 'c':
			s[1] = 's';
			if (exists(name)) {
		    		state[YSTATE] = 'y';
				state[TSTATE] = 's';
			} else {
				state[TSTATE] = 'j';
			}
			s[1] = 'c';
			strcat(buf, s+1);
			break;
		    case 's':
			state[TSTATE] = 's';
			sc = chk_sfile(f, state);
			strcat(buf, s+3);
			break;
		    default:
			state[TSTATE] = 'j';
			strcpy(buf, f);
			break;
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
			state[TSTATE] = 'x';
			s = strrchr(sfile, '/');
			s[1] = 'c';
			if (exists(sfile)) state[YSTATE] = 'y';
		} else {
			sc = chk_sfile(sfile, state);
		}
		free(sfile);
		strcpy(buf, f);
	}
	if (sc && (sc->proj != proj)) {
		fprintf(stderr,
		    "sfiles: error file %s is from a different repository\n"
		    "than other files. %s vs %s\n",
		    sc->gfile, proj_root(sc->proj), proj_root(proj));
	}
	/*
	 * When we get here buf contains the gname (for the sfile case).
	 */
	if (state[TSTATE] == 's') {
		if (exists(buf)) state[GSTATE] = 'G';
		if (opts.pending || opts.verbose) {
			/* chk_pending calls do_print */
			chk_pending(sc, buf, state, 0, 0);
		} else {
			do_print(state, buf, 0);
		}
	} else  if (state[TSTATE] == 'x' || state[TSTATE] == 'j') {
		/* XXX - does not work with SCCS/c.* */
		if (exists(buf)) {
			state[LSTATE] = state[PSTATE] = ' ';
			do_print(state, buf, 0);
		}
	} else {
		state[PSTATE] = ' ';
		do_print(state, buf, 0);
	}
	if (sc) sccs_free(sc);
}

private void
print_summary(void)
{
	fprintf(opts.out, "%6d files under revision control.\n", s_count);
	if (R_count) {
		fprintf(opts.out,
		    "%6d subrepositories.\n", R_count);
	}
	if (opts.extras) {
		fprintf(opts.out,
		    "%6d files not under revision control (and not ignored).\n",
		    x_count);
	}
	if (i_count) {
		fprintf(opts.out,
		    "%6d ignored files not under revision control.\n", i_count);
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

/*
 * This struct contains the information that is passed sfiles_walk() by
 * walkdir() as it is traversing the respository.  Information from the
 * walkdir() is collected here and then processed once per directory with
 * a call to sccsdir().
 */
struct winfo {
	char	**sfiles;	/* The sfiles in the current directory */
	MDBM	*sDB;		/* all files in the SCCS subdirectory */
	MDBM	*gDB;		/* all files in the current directory */
	char	*sccsdir;	/* path to dir this data represents */
	int	seenfirst;	/* flag for -1 (one level) */
	int	rootlen;	/* length of directory passed to walkdir */
	char	*proj_prefix;	/* prefex needed to get sfile relpath */
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

private void
winfo_free(winfo *wi)
{
	mdbm_close(wi->gDB);
	wi->gDB = 0;
	mdbm_close(wi->sDB);
	wi->sDB = 0;
	freeLines(wi->sfiles, free);
	wi->sfiles = 0;
	free(wi->sccsdir);
	wi->sccsdir = 0;
}


private int
sfiles_walk(char *file, struct stat *sb, void *data)
{
	winfo	*wi = (winfo *)data;
	char	*p;
	int	sccs;
	int	n, nonsccs;
	char	buf[MAXPATH];

	p = strrchr(file, '/');
	if ((p - file) < wi->rootlen) p = 0;

	/* make 'buf' match the directory we are in */
	if (S_ISDIR(sb->st_mode)) {
		if (p) {
			strncpy(buf, file, p-file);
			buf[p-file] = 0;
		} else {
			strcpy(buf, file);
		}
	} else {
		assert(p);
		*p++ = 0;
		if ((p - file) > 6 && streq(p-6, "/SCCS")) {
			strncpy(buf, file, p-file-6);
			buf[p-file-6] = 0;
		} else {
			strcpy(buf, file);
		}
	}
	/* if directory has changed then process old directory */
	if (wi->sccsdir) {
		unless (patheq(buf, wi->sccsdir)) {
			sccsdir(wi);
			wi->sccsdir = strdup(buf);
		}
	}
	unless (wi->sccsdir) wi->sccsdir = strdup(buf);

	if (S_ISDIR(sb->st_mode)) {
		if (p && patheq(p, "/SCCS")) return (0);
		if (p &&((p-file) > wi->rootlen) && patheq(p+1, "BitKeeper")) {
			/*
			 * Do not cross into other package roots
			 * (e.g. RESYNC).
			 */
			concat_path(buf, file, "etc");
			if (exists(buf)) {
				*p = 0;
				do_print("R      ", file, 0);
				*p = '/';
				winfo_free(wi);
				return (-2);
			}
		}
		if (proj) {
			concat_path(buf, wi->proj_prefix,
			    p ? (file + wi->rootlen + 1) : "");
			if (match_globs(buf, prunedirs, 0) != 0) {
				return (-1);
			}
		}
		if (opts.dirs || opts.xdirs) {
			n = strlen(file);
			if (n > wi->rootlen) {
				concat_path(buf, file, BKROOT);
				if (exists(buf)) {
					do_print("R      ", file, 0);
					return (-1);
				}
			}
			strcpy(&file[n], "/SCCS");
			nonsccs = !exists(file) || emptyDir(file);
			file[n] = 0;
			if (opts.xdirs && nonsccs) {
				do_print("D      ", file, 0);
			}
			if (opts.dirs && !nonsccs) {
				do_print("d      ", file, 0);
			}
		}
		if (opts.onelevel) {
			if (wi->seenfirst) return (-1);
			wi->seenfirst = 1;
		}
	} else {
		if (patheq(p, BKSKIP)) {
			/* abort current dir */
			winfo_free(wi);
			return (-2);
		}
		if (((p - file) > 6) && patheq(p - 6, "/SCCS")) {
			sccs = 1;
			p[-6] = 0;
		} else {
			sccs = 0;
		}
		add_to_winfo(wi, p, sccs);
		p[-1] = '/';
		if (sccs) p[-6] = '/';
	}
	return (0);
}

private void
load_ignore(project *p)
{
	char	*file;

	ignore = ignorebase = prunedirs = 0;

	/* add default pruned dirs */
	prunedirs = addLine(prunedirs, strdup("/" BAM_ROOT));
	prunedirs = addLine(prunedirs, strdup("/BitKeeper/log"));
	prunedirs = addLine(prunedirs, strdup("/BitKeeper/tmp"));
	prunedirs = addLine(prunedirs, strdup("/BitKeeper/writer"));
	prunedirs = addLine(prunedirs, strdup("/BitKeeper/readers"));
	prunedirs = addLine(prunedirs, strdup("/PENDING"));

	/* add default ignore patterns */
	ignore = addLine(ignore, strdup("/BitKeeper/etc/level"));
	ignore = addLine(ignore, strdup("/BitKeeper/etc/csets-out"));
	ignore = addLine(ignore, strdup("/BitKeeper/etc/csets-in"));

	if (opts.all) return;


	file = aprintf("%s/BitKeeper/etc/ignore", proj_root(p));
	ignore_file(file);
	free(file);
	file = aprintf("%s/ignore", getDotBk());
	ignore_file(file);
	free(file);
}

private void
ignore_file(char *file)
{
	char	*pat;
	int	len, isbase, isprune;
	FILE	*ignoref;
	char	buf[MAXLINE];

	unless (exists(file)) get(file, SILENT, "-");
	ignoref = fopen(file, "r");
	unless (ignoref) return;
	while (fnext(buf, ignoref)) {
		chomp(buf);

		isbase = (strchr(buf, '/') == 0);

		pat = buf;
		if (*pat == '/') ++pat;	/* skip leading slash */
		if (strneq(pat, "./", 2)) pat += 2; /* skip leading ./ */

		unless (len = strlen(pat)) continue; /* blank lines */
		if ((len > 7) && streq(pat + len - 7, " -prune")) {
			pat[len-7] = 0;
			isprune = 1;
			isbase = 0;
		} else {
			isprune = 0;
		}
		if (isbase || (pat[0] == '*')) {
			pat = strdup(pat);
		} else {
			pat = aprintf("/%s", pat);
		}
		if (isprune) {
			prunedirs = addLine(prunedirs, pat);
		} else if (isbase) {
			ignorebase = addLine(ignorebase, pat);
		} else {
			ignore = addLine(ignore, pat);
		}
	}
	fclose(ignoref);
}

/*
 * maintain the global 'proj' pointer to point and the project for this
 * directory.
 */
private void
load_project(char *dir)
{
	project	*newproj;
	char	*p;
	char	tmp[MAXPATH];

	newproj = proj_init(dir);
	if (newproj != proj) {
		free_project();
		proj = newproj;
		prodproj = proj_product(proj);
		load_ignore(proj);
		unless (opts.fixdfile) {
			concat_path(tmp, proj_root(proj), DFILE);
			unless (opts.dfile = exists(tmp)) {
				if (p = getenv("_BK_SLOW_WALK")) touch(p, 0666);
			}
		}
	} else {
		if (newproj) proj_free(newproj);
	}
}

private void
free_project(void)
{
	if (proj) {
		proj_free(proj);
		proj = prodproj = 0;
		freeLines(ignore, free);
		freeLines(ignorebase, free);
		freeLines(prunedirs, free);
		ignore = ignorebase = prunedirs = 0;
	}
}

/* called on directories on stdin or the command line */
private void
walk(char *indir)
{
	char	*p;
	winfo	wi = {0};
	char	dir[MAXPATH];

	cleanPath(indir, dir);
	if (streq(dir, "/")) strcpy(dir, "/.");
	load_project(dir);
	unless (proj) {
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

	wi.rootlen = strlen(dir);
	if (proj) {
		p = proj_relpath(proj, dir);
		if (streq(p, ".")) {
			wi.proj_prefix = strdup("/");
		} else {
			wi.proj_prefix = aprintf("/%s", p);
		}
		free(p);
	}
	walkdir(dir, sfiles_walk, &wi);
	if (wi.sccsdir) sccsdir(&wi);
	if (proj) free(wi.proj_prefix);
	if (opts.timestamps && timestamps && proj) {
		dumpTimestampDB(proj, timestamps);
		hash_free(timestamps);
		timestamps = 0;
	}
	if (opts.summarize) print_summary();

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

	unless (s) return (0);
	different = (sccs_hasDiffs(s, 0, 1) >= 1);
	if (timestamps) {
		updateTimestampDB(s, timestamps, different);
	}
	return (different);
}

private int
isIgnored(char *file)
{
	char	*p;
	struct	stat sbuf;
	char	gfile[MAXPATH];

	/* if not in a repo, don't bother */
	unless (proj_root(proj)) return (0);

	/* If this file is not from our repo, then no */
	unless (p = proj_relpath(proj, file)) return (0);
	sprintf(gfile, "/%s", p);
	free(p);

	/* If this is a subcomponent, then no */
	if ((p = strrchr(file, '/')) && streq(p+1, "ChangeSet")) return (0);

	if (match_globs(gfile, ignore, 0) ||		 /* pathname match */
	    match_globs(basenm(gfile), ignorebase, 0) || /* basename match */
	    lstat(file, &sbuf) ||			 /* gone? */
 	    !(sbuf.st_mode && S_IFREG|S_IFLNK)) {	 /* not reg file */
		return (1);
	}
	if (getenv("BK_IGNOREDIRS")) {
		/* check if directory is ignored */
		while (p = strrchr(gfile, '/')) {
			*p = 0;
			if (match_globs(gfile, ignore, 0) ||
			    match_globs(basenm(gfile), ignorebase, 0)) {
				return (1);
			}
		}
	}
	return (0);
}

private int
hidden(char *file)
{
	return (strstr(file, "BitKeeper/etc/SCCS/x.") != 0);
}

private int
isBkFile(char *gfile)
{
	char	*rp = 0;
	int	ret;
	char	buf[MAXPATH];

	/* ChangeSet is reserved at the root of any repo */
	if (streq(basenm(gfile), "ChangeSet")) {
		strcpy(buf, gfile);
		concat_path(buf, dirname(buf), BKROOT); /* yes, this works */
		if (isdir(buf)) return (1);
	}
	if (opts.atRoot) {
		/* gfile is already a relative path from root */
	} else if (proj && strstr(gfile, "BitKeeper/")) {
		unless (rp = proj_relpath(proj, gfile)) return (0);
		gfile = rp;
	} else {
		/* not in root or no BitKeeper in pathname */
		return (0);
	}
	ret = (strneq(gfile, "BitKeeper/", 10) &&
	    !strneq(gfile, "BitKeeper/triggers/", 19));
	if (rp) free(rp);
	return (ret);
}

/* ONLY call this from do_print */
private void
print_it(STATE state, char *gfile, char *rev)
{
	char	*sfile;

	gfile =  strneq("./",  gfile, 2) ? &gfile[2] : gfile;
	if ((opts.useronly) && isBkFile(gfile)) return;

	if (opts.verbose) {
		if (state[TSTATE] == 'j') {
			assert(streq(state, "j      "));
			if (fprintf(opts.out, "j------ ") != 8) {
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
	/* if gfile or !sfile then print gfile style name */
	if (opts.prefix) fputs(opts.prefix, opts.out);
	if (opts.gfile || (state[TSTATE] != 's')) {
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
	if (fputc((opts.null ? '\0' : '\n'), opts.out) < 0) goto error;
}

private void
do_print(STATE buf, char *gfile, char *rev)
{
	STATE	state;
	int	doit;

	gfile =  strneq("./",  gfile, 2) ? &gfile[2] : gfile;
	strcpy(state, buf);	/* So we have writable strings */
	if (opts.glob) {
		char	*p;

		if (p = strrchr(gfile, '/')) {
			p++;
		} else {
			p = gfile;
		}
		/* XXX - make this conditional on !directory */
		if (streq("s.", p)) p += 2;
		unless (match_one(p, opts.glob, 0)) return;
	}

	if (state[PSTATE] == 'p') p_count++;
	if (state[NSTATE] == 'n') n_count++;
	if (state[GSTATE] == 'G') C_count++;
    	if (state[CSTATE] == 'c') c_count++; 

	switch (state[TSTATE]) {
	    case 'j': break;
	    case 'x':
	    	if (isIgnored(gfile)) {
			state[TSTATE] = 'i';
			i_count++;
		} else {
			x_count++;
		}
		break;
	    case 's': s_count++; break;
	    case 'd': d_count++; break;
	    case 'D': D_count++; break;
	    case 'R': R_count++; break;
	    default:
		fprintf(stderr, "TSTATE='%c'\n", state[TSTATE]);
		assert("state[TSTATE] unknown" == 0);
	}
	if (opts.progress &&
	    (((s_count - s_last) > 100) || ((x_count - x_last) > 100))) {
		progress(1);
	}
	if (opts.summarize) return; /* skip the detail */

	if ((state[TSTATE] == 'R') && prodproj && !opts.subrepos) {
		/*
		 * found a subrepo while walking nested tree when
		 * subrepos were not enabled.
		 */
		components =
		    addLine(components, proj_relpath(prodproj, gfile));
		/* won't print anything below */
	}
	doit = ((state[TSTATE] == 'd') && opts.dirs) ||
	    ((state[TSTATE] == 'D') && opts.xdirs) ||
	    ((state[TSTATE] == 'i') && opts.ignored) ||
	    ((state[TSTATE] == 'j') && opts.junk && !hidden(gfile)) ||
	    ((state[TSTATE] == 'R') && opts.subrepos) ||
	    ((state[TSTATE] == 's') && opts.sfiles) ||
	    ((state[TSTATE] == 'x') && opts.extras) ||
	    ((state[LSTATE] == 'l') && opts.locked) ||
	    ((state[LSTATE] == 'u') && opts.unlocked) ||
	    ((state[CSTATE] == 'c') && opts.changed) ||
	    ((state[PSTATE] == 'p') && opts.pending) ||
	    ((state[GSTATE] == 'G') && opts.gotten) ||
	    ((state[NSTATE] == 'n') && opts.names) ||
	    ((state[YSTATE] == 'y') && opts.cfiles);
	if (opts.inverse) doit = !doit;
	if (doit) print_it(state, gfile, rev);
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
 * Called for each directory traversed by sfiles_walk()
 */
private void
sccsdir(winfo *wi)
{
	char	*dir = wi->sccsdir;
	char	**slist = wi->sfiles;
	char	**gfiles = 0;
	MDBM	*gDB = wi->gDB;
	MDBM	*sDB = wi->sDB;
	char	*relp, *p, *gfile;
	kvpair  kv;
	sccs	*s = 0;
	delta	*d;
	int	i;
	char	buf[MAXPATH];
	char	buf1[MAXPATH];

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
	sortLines(slist, 0);	/* walkdir() has a funny sort */
	EACH (slist) {
		char 	*file;
		u32	flags = INIT_NOCKSUM;
		STATE	state = "       ";

		p = slist[i];
		s = 0;
		file = p;
		gfile = &file[2];

		state[TSTATE] = 's';
		if (mdbm_fetch_str(gDB, gfile)) {
			state[GSTATE] = 'G';
			flags = INIT_HASgFILE;
		}

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
// LMXXX - we could optimize this further.  If they are not looking for
// opts.sfiles or opts.changed or opts.locked then we won't need this info.
			if ((opts.verbose || opts.changed) && opts.timestamps &&
			    proj && !timestamps) {
				timestamps = generateTimestampDB(proj);
			}
			if ((opts.verbose || opts.changed) &&
			    (flags & INIT_HASgFILE) &&
			    (!timestamps ||
				!timeMatch(proj, gfile, sfile,
				    timestamps)) &&
			    (s = init(buf, flags, sDB, 0)) &&
			    chk_diffs(s)) {
				state[CSTATE] = 'c';
			}
			free(gfile);
			if (opts.names) {
				unless (s) s = init(buf, flags, sDB, 0);
				d = sccs_top(s);
				relp = proj_relpath(s->proj, s->gfile);
				unless (d->pathname &&
				    patheq(relp, d->pathname)) {
					state[NSTATE] = 'n';
				}
				free(relp);
			}
		} else {
			/*
			 * LMXXX - note that if we get here and the gfile
			 * is writable it may have diffs and we are not
			 * listing them.
			 */
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
			    (s = init(buf, flags, sDB, 0))) {
				d = sccs_top(s);
				relp = proj_relpath(s->proj, s->gfile);
				unless (d->pathname &&
				    patheq(relp, d->pathname)) {
					state[NSTATE] = 'n';
				}
				free(relp);
				sccs_free(s);
				s = 0;
			}
		}
		file[0] = 'c';
		if (mdbm_fetch_str(sDB, file)) state[YSTATE] = 'y';
		file[0] = 's';
		concat_path(buf, dir, gfile);
		if (opts.pending || opts.verbose) {
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

	/*
	 * Check the sDB for "junk" file
	 * XXX TODO: Do we consider the r.file and m.file "junk" file?
	 */
	if (opts.verbose || opts.junk) {
		concat_path(buf, dir, "SCCS");
		EACH_KV (sDB) {
			/*
			 * Ignore x.files is the SCCS dir if the matching
			 * s.file exists.
			 * Ignore SCCS/c.JUNK if JUNK exists.
			 * Ignore SCCS/c.JUNK if s.JUNK exists.
			 */
			if (kv.key.dptr[1] == '.') {
				switch(kv.key.dptr[0]) {
				    case 's': continue;
				    case 'd': case 'p': case 'x': case 'z':
					strcpy(buf1, kv.key.dptr);
					buf1[0] = 's';
					if (mdbm_fetch_str(sDB, buf1)) {
						continue;
					}
					break;
				    case 'c':
					strcpy(buf1, kv.key.dptr + 2);
				    	if (mdbm_fetch_str(gDB, buf1)) {
						/* We'll flag it below */
						continue;
					}
					sprintf(buf1, "s.%s", kv.key.dptr + 2);
				    	if (mdbm_fetch_str(sDB, buf1)) {
						/* We caught it above */
						continue;
					}
					break;
				}
			}
			concat_path(buf1, buf, kv.key.dptr);
			if (kv.val.dsize == 0) {
				do_print("j      ", buf1, 0);
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
					do_print("j      ", t, 0);
					p = q;
				}
			}
		}
	}

	/*
	 * Everything left in the gDB is extra
	 */
	if (opts.extras || opts.ignored || opts.verbose) {
		EACH_KV(gDB) gfiles = addLine(gfiles, kv.key.dptr);
		sortLines(gfiles, 0);
		EACH(gfiles) {
			buf[0] = 's'; buf[1] = '.';
			strcpy(&buf[2], gfiles[i]);
			if (mdbm_fetch_str(sDB, buf)) continue;
			concat_path(buf, dir, gfiles[i]);
			buf1[0] = 'c'; buf1[1] = '.';
			strcpy(&buf1[2], gfiles[i]);
			if (mdbm_fetch_str(sDB, buf1)) {
				do_print("x-----y", buf, 0);
			} else {
				do_print("x------", buf, 0);
			}
		}
		freeLines(gfiles, 0);
	}
	winfo_free(wi);
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
 * This struct contains the data that is passed to findsfiles() by walkdir()
 * when called by walksfiles().
 */
typedef struct sinfo sinfo;
struct sinfo {
	walkfn	fn;		/* call this function on each sfile */
	void	*data;		/* pass this to the fn() */
	int	rootlen;	/* the len of the dir passed to walksfiles() */
	char	*proj_prefix;	/* the prefix needed to make a relpath */
	u32	is_clone:1;	/* special clone walkfn */
	u32	is_modes:1;	/* -m in clone walkfn */
};

private int
findsfiles(char *file, struct stat *sb, void *data)
{
	char	*p = strrchr(file, '/');
	sinfo	*si = (sinfo *)data;
	char	buf[MAXPATH];

	unless (p) return (0);
	if (S_ISDIR(sb->st_mode)) {
		if (p - file > si->rootlen && patheq(p+1, "BitKeeper")) {
			/*
			 * Do not cross into other package roots
			 * (e.g. RESYNC).
			 */
			strcat(file, "/etc");
			if (exists(file)) {
				if (proj == prodproj) {  /* this is product */
					*p = 0;
					sprintf(p, "/SCCS/s.ChangeSet");
					si->fn(file, 0, si->data);
					*p = '/';
				}
				return (-2);
			}
		}
		if (si->is_clone && streq(file + 2, "BitKeeper/etc/SCCS")) {
			/*
			 * when doing a clone here are a couple more files to
			 * include from the BitKeeper/etc/SCCS dir
			 */
			p[5] = '/';
			strcpy(p+6, "x.cmark");
			if (exists(file)) si->fn(file, sb, si->data);
			strcpy(p+6, "x.dfile");
			if (exists(file)) si->fn(file, sb, si->data);
			strcpy(p+6, "x.id_cache");
			if (exists(file)) si->fn(file, sb, si->data);
		}
		if (si->is_clone && streq(file + 2, "BitKeeper/log")) {
			/*
			 * when doing a clone here are a couple more files to
			 * include from the BitKeeper/log dir
			 */
			p[4] = '/';
			strcpy(p+5, "PRODUCT");
			if (exists(file)) si->fn(file, sb, si->data);
			strcpy(p+5, "COMPONENT");
			if (exists(file)) si->fn(file, sb, si->data);
			strcpy(p+5, "CSETFILE");
			if (exists(file)) si->fn(file, sb, si->data);
			strcpy(p+5, "COMPONENTS");
			if (exists(file)) si->fn(file, sb, si->data);
			if (si->is_modes) {
				strcpy(p+5, "NFILES");
				if (exists(file)) si->fn(file, sb, si->data);
				strcpy(p+5, "ROOTKEY");
				if (exists(file)) si->fn(file, sb, si->data);
				strcpy(p+5, "TIP");
				if (exists(file)) si->fn(file, sb, si->data);
				strcpy(p+5, "checked");
				if (exists(file)) si->fn(file, sb, si->data);
			}
		}
		if (prunedirs) {
			concat_path(buf, si->proj_prefix,
			    file + si->rootlen + 1);
			if (match_globs(buf, prunedirs, 0)) return (-1);
		}
	} else {
		if ((p - file >= 6) && pathneq(p - 5, "/SCCS/s.", 8)) {
			return (si->fn(file, sb, si->data));
		} else if (si->is_clone &&
		    (p - file >= 6) && pathneq(p - 5, "/SCCS/d.", 8)) {
			/* clone includes d.files too */
			return (si->fn(file, sb, si->data));
		} else if (patheq(p+1, BKSKIP)) {
			/*
			 * Skip directory containing a .bk_skip file
			 */
			return (-2);
		} else if (si->is_clone &&
		    pathneq(file+2, BAM_ROOT, strlen(BAM_ROOT))) {
			/*
			 * lclone wants all the BAM files.  Under
			 * clone the default prunedirs will prune this
			 * directory, but lclone will traverse it.
			 */
			return (si->fn(file, sb, si->data));
		}
	}
	return (0);
}

int
walksfiles(char *dir, walkfn fn, void *data)
{
	char	*p;
	sinfo	si = {0};
	int	rc;

	si.fn = fn;
	si.data = data;
	si.rootlen = strlen(dir);
	load_project(dir);
	if (proj) {
		p = proj_relpath(proj, dir);
		if (streq(p, ".")) {
			si.proj_prefix = strdup("/");
		} else {
			si.proj_prefix = aprintf("/%s", p);
		}
		free(p);
	}
	rc = walkdir(dir, findsfiles, &si);
	if (proj) free(si.proj_prefix);
	free_project();
	return (rc);
}

/* generate a list of files to create the sfio in clone */
int
sfiles_clone_main(int ac, char **av)
{
	int	c;
	int	lclone = 0;
	int	modes = 0;	/* sfio sets modes so more stuff is ok */
	int	rc = 2;
	sinfo	si = {0};

	while ((c = getopt(ac, av, "Lm")) != -1) {
		switch (c) {
		    case 'L': lclone = 1; break;
		    case 'm': modes = 1; break;
		    default:
usage:			fprintf(stderr, "usage: _sfiles_clone [-L]\n");
			return (1);
		}
	}
	if (av[optind]) goto usage;
	load_project(".");
	/* in lclone mode decsend into BAM */
	if (lclone) removeLine(prunedirs, "/" BAM_ROOT, free);
	si.fn = fastprint;
	si.rootlen = 1;
	si.proj_prefix = "/";
	si.is_clone = 1;
	if (modes) si.is_modes = 1;
	rc = walkdir(".", findsfiles, &si);
	free_project();
	return (rc);
}

private void
print_components(void)
{
	project	*comp;
	int	i, cwd_len;
	char	*gfile, *freeme, *p, *cwd;
	STATE	state;
	char	buf[MAXPATH];

	if (opts.skip_comps) return;
	unless (proj && (proj == prodproj)) return;
	unless (opts.out) opts.out = stdout;

	cwd = proj_cwd();
	cwd_len = strlen(cwd);
	components = file2Lines(components,
	    proj_fullpath(prodproj, "BitKeeper/log/deep-nests"));
	uniqLines(components, free);
	EACH (components) {
		gfile = proj_fullpath(prodproj, components[i]);
		unless (pathneq(gfile, cwd, cwd_len)) continue;
		gfile += cwd_len + 1;
		concat_path(buf, gfile, CHANGESET);
		unless (exists(buf)) continue;
		// XXX
		comp = proj_init(gfile);
		strcpy(state, "       ");
		state[TSTATE] = 's';
		if (proj_isComponent(comp)) {
			if (opts.pending) {
				freeme = aprintf("%s/SCCS/s.ChangeSet", gfile);
				p = strrchr(freeme, '/');
				p[1] = 'd';
				if (exists(freeme)) {
					state[PSTATE] = 'p';
					free(freeme);
					freeme = aprintf("%s/ChangeSet", gfile);
					chk_pending(0, freeme, state, 0, 0);
					free(freeme);
					proj_free(comp);
					continue;
				}
				free(freeme);
				freeme = 0;
			}
			if (opts.names) {
				unless (patheq(components[i],
					proj_comppath(comp))) {
					state[NSTATE] = 'n';
				}
			}
		} else {
			state[TSTATE] = 'x';
		}
		gfile = aprintf("%s/%sChangeSet", gfile,
		    ((state[TSTATE] == 'x') && !opts.gfile) ? "SCCS/s." : "");
		do_print(state, gfile, 0);
		free(gfile);
		proj_free(comp);
	}
}
