/* Copyright (c) 2000 Andrew Chang */ 
#include "system.h"
#include "sccs.h"

#define LSTATE	0	/* lock state	 */
#define CSTATE	1	/* change state	 */
#define PSTATE	2	/* pending state */
#define P2STATE 3	/* extended pending state */
 
WHATSTR("@(#)%K%");


private	char *sfind_usage = "\n\
usage: sfind [-aACeEgvS] [-s<select_sets>] [-o<file>] [directories]\n\n\
    -a		examine all files, even if listed in BitKeeper/etc/ignore\n\
    -A		for used with -s,,p, list all revs, not just the tip\n\
    -C		for used with -s,,p, list pending files in file@rev format\n\
    -e		list everything in quick scan mode\n\
    -E		list everything in detail scan mode\n\
    -g		list the gfile name, not the sfile name\n\
    -v		verbose mode: annotate the output with state markers\n\
    -o<file>	send the file list to <file>, and progress info to stdout\n\
    -S		summarize output\n\
    -U		list user file only (hide BitKeeper administrative file)\n\
";

typedef struct {
	u32     show_markers:1;		/* show markers		*/
	u32     aflg:1;			/* disable ignore list	*/
	u32	s1_lflg:1;
	u32	s1_uflg:1;
	u32	s1_jflg:1;
	u32	s1_xflg:1;
	u32	s2_cflg:1;
	u32	s2_nflg:1;
	u32	s3_pflg:1;
	u32	s3_cflg:1;
	u32     gflg:1;     		/* print gfile name	*/
	u32     Aflg:1;			/* use with -s,,p show	*/
					/* all pending deltas	*/
	u32     Cflg:1;     		/* want file@rev format	*/
	u32     splitRoot:1;   		/* split root mode	*/
	u32     dfile:1;   		/* use d.file to find 	*/
					/* pending delta	*/
	u32	progress:1;		/* if set, send progress to stdout */
	FILE	*out;			/* send output here */
	u32     Sflg:1;     		/* summarize output  	*/
	u32     Uflg:1;     		/* list user file only 	*/
} options;

typedef struct _q_item {
	char	*path;
	struct	_q_item *next;
} q_item;

typedef struct {
	q_item	*first;
	q_item	*last;
} fifo;

private project *proj;
private options	opts;
private globv	ignore; 
private u32	 d_count, s_count, x_count; /* progress counter */
private u32	 s_last, x_last; /* progress counter */
private unsigned int c_count, p_count;

private void do_print(char state[5], char *file, char *rev);
private void walk(char *dir, int level);
private void file(char *f);
private void sccsdir(char *dir, int level, DIR *sccs_dh, char buf[MAXPATH]);
private void progress(int force);
private int chk_diffs(sccs *s);


private void
enqueue(fifo *q, char *path)
{
	q_item	*t = (q_item *) malloc(sizeof(q_item));
	
	assert(t);
	t->path = strdup(path);
	t->next = 0;
	if (q->first == NULL) {
		q->first = t;
	} else {
		q->last->next = t;
	}
	q->last = t;
}

private char *
dequeue(fifo *q)
{
	char	*t;
	q_item	*i;

	if (q->first == NULL) return NULL;
	i = q->first;
	q->first = (q->first == q->last) ? NULL : q->first->next; 
	t = i->path;
	free(i);
	return (t);
}

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
	unless (opts.splitRoot) flags |= INIT_ONEROOT;
	s = sccs_init(name, flags|INIT_SAVEPROJ, proj);
        if (s && !proj) proj = s->proj;
        return (s);
}

private void
parse_select(char *sets)
{
	char *p;

	p = sets;
	/* prase set 1 */
	while (*p && (*p != ',')) {
		switch (*p) {
		    case 'l': opts.s1_lflg = 1; break;
		    case 'u': opts.s1_uflg = 1; break;
		    case 'j': opts.s1_jflg = 1; break;
		    case 'x': opts.s1_xflg = 1; break;
		    defualt: fprintf(stderr, "unknown set 1 flag %c\n", *p);
		}
		p++;
	}
	unless (*p++ == ',') return;
	/* prase set 2 */
	while (*p && (*p != ',')) {
		switch (*p) {
		    case 'c': opts.s2_cflg = 1; break;
		    case 'n': opts.s2_nflg = 1; break;
		    default: fprintf(stderr, "unknown set 2 flag %c\n", *p);
		}
		p++;
	}
	unless (*p++ == ',') return;
	/* prase set 3 */
	while (*p && (*p != ',')) {
		switch (*p) {
		    case 'p': opts.s3_pflg = 1; break;
		    case 'c': opts.s3_cflg = 1; //XXX FIXME
			      fprintf(stderr,
					"-s,,c is not implemented yet\n");
			      exit(1);
			      break; 
		    default: fprintf(stderr, "unknown set 3 flag %c\n", *p);
		}
		p++;
	}
	unless (*p++ == ',') return;
}


int
sfind_main(int ac, char **av)
{
        int     c, i, sfiles_compat = 0; 
	char	*root, *path, buf[MAXPATH];

	if ((ac > 1) && streq("--help", av[1])) {
usage:		fprintf(stderr, "%s", sfind_usage);
		return (0);
	}                

	while ((c = getopt(ac, av, "aAcCdDeEgklo:pPrRs:SuUvx")) != -1) {
		switch (c) {
		    case 'a':	opts.aflg = 1; break;
		    case 'A':	opts.s3_pflg = opts.Aflg = 1; break;
		    case 'c':	opts.s2_cflg = 1; break;
		    case 'g':	opts.gflg = 1; break;
		    case 'k':	sfiles_compat = 1; break;
		    case 'l':   opts.s1_lflg = 1; break;
		    case 'o':	unless (opts.out = fopen(optarg, "w")) {
		    			perror(optarg);
					exit(1);
				}
				opts.progress = 1;
				break;
		    case 'C':	opts.s3_pflg = opts.Cflg = 1; break;
		    case 'd':	sfiles_compat = 1; break;
		    case 'D':	sfiles_compat = 1; break;
		    case 'v':	opts.show_markers = 1; break;
		    case 'E':	opts.s2_cflg = 1;
				opts.s2_nflg = 1;
				opts.s3_pflg = 1;
				opts.s3_cflg = 1;
				/* fall thru */
		    case 'e':	opts.s1_jflg = 1;
				opts.s1_xflg = 1;
				opts.s1_lflg = 1;
				opts.s1_uflg = 1;
				break;
		    case 'p':	sfiles_compat = 1; break;
		    case 'P':	sfiles_compat = 1; break;
		    case 'r':	sfiles_compat = 1; break;
		    case 'R':	sfiles_compat = 1; break;
		    case 's':	parse_select(optarg);
				break;
		    case 'S':	opts.Sflg = 1; break; /* summarize output */	
		    case 'u':	opts.s1_uflg = 1; break;
		    case 'U':	opts.Uflg = 1; break;
		    case 'x':	opts.s1_xflg = opts.s1_jflg = 1; break;
		    default: 	goto usage;
		}
	}
	
	if (sfiles_compat) {
		getoptReset(); 
		return (sfiles_main(ac, av));
	}

	unless (opts.out) opts.out = stdout;
	c_count = p_count = d_count = s_count = x_count = 0;

	/*
	 * If user did not select any option,
	 * setup a default mode for them
	 */
	if (!opts.s1_uflg && !opts.s1_lflg && !opts.s1_jflg && !opts.s1_xflg &&
	    !opts.s2_cflg && !opts.s2_nflg && !opts.s3_pflg && !opts.s3_cflg) {
		opts.s1_uflg = 1;
		opts.s1_lflg = 1;
	}

	if (!av[optind]) {
		path = ".";
		walk(path, 0);
	} else if (streq("-", av[optind])) {
		setmode(0, _O_TEXT); /* read file list in text mode */
		while (fnext(buf, stdin)) {
			chop(buf);
			path = buf;
                        if (isdir(path)) {
				walk(path, 0);
			} else {
                                file(path);
			}
		}
	} else {
                for (i = optind; i < ac; ++i) {
                        localName2bkName(av[i], av[i]);
                        if (isdir(av[i])) {
                                path =  av[i];
                                walk(path, 0);
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
chk_sfile(char *name, char state[5])
{
	char	*s;
	sccs	*sc = 0;

	s = strrchr(name, '/');

	if (s[1] == 's') {
		s[1] = 'p';
		if (exists(name)) {
			state[LSTATE] = 'l';
			s[1] = 's';
			if (opts.s2_cflg && 
			    (sc = init(name, INIT_NOCKSUM, 0, 0)) &&
			    chk_diffs(sc)) { 
				state[CSTATE] = 'c';
			} else {
				state[CSTATE] = 'n';
			}
		} else {
			s[1] = 'z';
			if (exists(name)) {
				state[LSTATE] = 'l';
				state[CSTATE] = 'n';
			} else {
				state[LSTATE] = 'u';
				state[CSTATE] = 'n';
			}
			s[1] = 's';
		}
	}
	return (sc);
}

private void
chk_pending(sccs *s, char *gfile, char state[5], MDBM *sDB, MDBM *gDB)
{
	delta	*d;
	char	*rev;
	int	local_s = 0, printed = 0;
	char	buf[MAXPATH];

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
			char *dfile = name2sccs(gfile);
			char *p;

			p = basenm(dfile);
			*p = 'd';
			unless (exists(dfile)) {
				free(dfile);
				state[PSTATE] = ' ';
				do_print(state, gfile, 0);
				return;
			}
			free(dfile);
		}
	}

	unless (s) {
		char *sfile = name2sccs(gfile);
		s = init(sfile, INIT_NOCKSUM, sDB, gDB);
		free(sfile);
		assert(s);
		local_s = 1;
	}
	
	/*
	 * check for pending deltas
	 */                                    
	state[PSTATE] = ' ';
	unless (d = sccs_getrev(s, "+", 0, 0))  goto out;	
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
	 * Do not sccs_close() if it is passed in from outside 
	 */
	if (local_s) sccs_close(s);
	if (opts.dfile) {
		/* No pending delta, remove redundant d.file */
		unless (state[PSTATE] == 'p') {
			char *p, *dfile = name2sccs(gfile);

			p = basenm(dfile);
			*p = 'd';
			unlink(dfile);
			free(dfile);
		}
	}
}

private void
file(char *f)
{
	char	name[MAXPATH], buf[MAXPATH];
	char    *s, *sfile, state[5] = "    ";
	sccs	*sc = 0;

	strcpy(name, f);
	s = rindex(name, '/');
	/*
	 * There are tree possible condition
	 * a) f is a sfile
	 * b) f is a regular or new gfile
	 * c) f is a junk file in the SCCS directory
	 */
	if (s && (name <= &s[-4]) && pathneq("SCCS", &s[-4], 4)) {
		/* this file is under a SCCS dir */
		unless (sccs_filetype(f)) {
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
		/* this file is a gname */
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
	 * When we get here. buf contain the gname
	 * Now we check for pending deltas
	 */
	if (opts.s3_pflg && state[CSTATE] != 'x' &&  state[CSTATE] != 'j') {
		chk_pending(sc, buf, state, 0, 0);
	} else  {
		if (state[CSTATE] == 'x' || state[CSTATE] == 'j') {
			if (exists(buf)) {
				state[LSTATE] = state[PSTATE] = ' ';
				do_print(state, buf, 0);
			}
		} else {
			state[PSTATE] = ' ';
			do_print(state, buf, 0);
		}
	}
	if (sc) sccs_close(sc);
}

private void
print_summary()
{
	fprintf(opts.out, "%6d files under revision control.\n", s_count);
	if (opts.s1_xflg) {
		fprintf(opts.out,
		    "%6d files not under revision control.\n", x_count);
	}
	if (opts.s2_cflg) {
		fprintf(opts.out,
		    "%6d files modified and not checked in.\n", c_count);
	}
	if (opts.s3_pflg) {
		fprintf(opts.out,
		    "%6d files with checked in, but not committed, deltas.\n",
		    p_count);
	}
}



/*
 * This function returns NULL if it cannot find project root
 */
private char *
find_root(char *dir, char *root)
{
	return (_relativeName(dir, 1, 0, 1, 0, root));
}

private void
walk(char *dir, int level)
{
	struct dirent   *e;
	DIR	*dh, *sccs_dh;
	char	buf[MAXPATH], *p, *root;
	fifo	dlist = {0, 0};
#ifndef WIN32
        ino_t	lastInode = 0;
#endif                                 
	if (level == 0) {
		char tmp[MAXPATH];

		/*
		 * Find project root and put it in buf
		 */
		if (find_root(dir, buf)) {
			opts.splitRoot = hasRootFile(buf, tmp);
			if (!opts.aflg) {
				FILE	*ignoref; 

				sprintf(tmp, "%s/BitKeeper/etc/ignore", buf);
				unless (exists(tmp)) get(tmp, SILENT, "-");
				if (ignoref = fopen(tmp, "rt")) {
					ignore = read_globs(ignoref, 0);
					fclose(ignoref);
				}
			}           
			sprintf(tmp, "%s/BitKeeper/etc/SCCS/x.dfile", buf);
			opts.dfile = exists(tmp);
		} else {
			/*
			 * Dir is not a BitKeeper repository,
			 * turn off all BitKeeper specific feature.
			 */
			opts.aflg = 1;
		}
		assert(proj == 0);
#if 0
		/*
		 * XXX TODO: should we reset the progress counter ?
		 */
		s_count = x_count = 0;
#endif
	}

	concat_path(buf, dir, "SCCS");
	/*
	 * TODO: for better performance in split root mode,
	 * we should be able to better optimized the sPath() code
	 */
	sccs_dh =  opendir(opts.splitRoot ? sPath(buf, 1) : buf);
	unless (sccs_dh) {
		/*
		 * TODO: we need to check for the caes where
		 * the pwd is a SCCS dir
		 * This can fool the current code into wronly treating them
		 * as xtras.
		 */
		if ((dh = opendir(dir)) == NULL) {
			perror(dir);
			goto done;
		}
		while ((e = readdir(dh)) != NULL) { 
#ifndef WIN32
			/*
			 * Linux 2.3.x NFS bug, skip repeats.
			 */
			if (lastInode == e->d_ino) continue;
			lastInode = e->d_ino;
#endif
			if (streq(e->d_name, ".") || streq(e->d_name, "..")) {
				continue;
			}
			concat_path(buf, dir, e->d_name);
			unless (isdir(buf)) {
				do_print("xxxx", buf, 0);
			} else {
				enqueue(&dlist, buf);
			}
		}
		closedir(dh);
		while (p = dequeue(&dlist)) {
			walk(p, level + 1);
			free(p);
		}
	} else {
		sccsdir(dir, level, sccs_dh, buf);
	}

done:	if (level == 0) {
		if (ignore) free_globs(ignore);  ignore = 0;
		if (proj) proj_free(proj); proj = 0;
		if (opts.Sflg) print_summary();
	}
	if (opts.progress) progress(0);
}

private void
progress(int force)
{
	static	struct timeval tv;
	struct	timeval now;
	int	msec;
	char	buf[100];

/*
 * This is a good idea if you are displaying to a DSL line or a modem,
 * but sucks otherwise.  Sorry, DSL.
	if (force <= 1) {
		gettimeofday(&now, 0);
		msec = (now.tv_sec - tv.tv_sec) * 1000;
		msec += now.tv_usec / 1000;
		msec -= tv.tv_usec / 1000;
		if (tv.tv_sec && (msec < 11)) return;
		tv = now;
	}
*/
	if (!force && (s_last == s_count) && (x_last == x_count)) return;
	sprintf(buf, "%d %d %d\n", s_count, x_count, d_count);
	/* If we get an error, it usually means that we are to die */
	if (write(1, buf, strlen(buf)) != strlen(buf)) exit(1);
	s_last = s_count;
	x_last = x_count;
	if (force == 2) {
		usleep(300000);		/* let TK update */
	}
#ifdef WIN32
	else {
		usleep(0);		/* let TK update */
	}
#endif
}

private int
chk_diffs(sccs *s)
{
	if (!s) return (0);
	if (sccs_hasDiffs(s, 0, 1) >= 1) return (1);
	return (0);  
}

int
isIgnored(char *file)
{
	char *gfile, *p, *q, save;
	struct stat sbuf;
	int len;

	gfile =  strneq("./",  file, 2) ? &file[2] : file;
	unless (opts.aflg) {
		if (match_globs(file, ignore)) {
			debug((stderr, "SKIP\t%s\n", file));
			return (1);
		}

		/*
		 * For backward compat with "bk sfiles"
		 * trimed "./" and match against ignore list.
		 */
		if ((gfile !=  file) && match_globs(gfile, ignore)) {
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
			if (match_globs(p, ignore)) {
				*q = save;
				debug((stderr, "SKIP\t%s\n", gfile));
				return (1);
			}
			*q = save;
			if (save == 0) break;
			p = ++q;
		}

		/* ignore special file e.g. char/block/fifo file */
		if (lstat(gfile, &sbuf)) {
			perror(gfile);
			return (1);
		}
		unless (sbuf.st_mode && S_IFREG|S_IFLNK) return (1);
	}

	/*
	 * HACK to hide stuff in the log and locks directory
	 * This assumes that sfind is ran from project root
	 * If you run "bk sfind" under <project root>/BitKeeper directory,
	 * these file will show up. It is probably OK.
	 */
	len = strlen(gfile);
	if ((len >= 14) && strneq("BitKeeper/log/", gfile, 14)) return (1);
	if ((len >= 17) && strneq("BitKeeper/writer/", gfile, 17)) return (1);
	if ((len >= 18) && strneq("BitKeeper/readers/", gfile, 18)) return (1);
	
	return (0);
}

private int
isTagFile(char *file)
{
	char *gfile;

	gfile =  strneq("./",  file, 2) ? &file[2] : file;
	return (strneq("BitKeeper/etc/SCCS/x.", gfile, 21));
}


private void
print_it(char state[5], char *file, char *rev)
{
	char *sfile, *gfile;

	gfile =  strneq("./",  file, 2) ? &file[2] : file;
	if (opts.Uflg) {
		if (streq(gfile, "ChangeSet") ||
		    (strlen(gfile) > 10) && strneq(gfile, "BitKeeper/", 10)) {
			return;
		}
	}
	if (opts.show_markers) fprintf(opts.out, "%s ", state);
	if (opts.gflg || (state[CSTATE] == 'x') || (state[CSTATE] == 'j'))  {
		fputs(gfile, opts.out);	/* print gfile name */
	} else {
		sfile = name2sccs(gfile);
		/*
		 * TODO: for better performance in split root mode,
		 * we should be able to better optimized the sPath() code
		 * e.g. we could pass project struct into sPath()
		 */
		fputs(opts.splitRoot ? sPath(sfile, 0) : sfile, opts.out);
		free(sfile);
	}
	if (rev) fprintf(opts.out, "@%s", rev);
	fputs("\n", opts.out);
}

private void
do_print(char state[5], char *file, char *rev)
{

	if (state[PSTATE] == 'p') p_count++;
	switch (state[CSTATE]) {
	    case 'j': break;
	    case 'x': unless (isIgnored(file)) x_count++; break;
	    case 'c': c_count++;
	    	/* fall through */
	    default: s_count++; break;
	}
	if (opts.progress && 
	    (((s_count - s_last) > 100) || ((x_count - x_last) > 100))) { 
		progress(1);
	}
	if (opts.Sflg) return; /* user wants summary only, skip the detail */
	if ((state[PSTATE] == 'p') && opts.s3_pflg) goto print;

	switch (state[LSTATE]) {
	    case 'l':	if (opts.s1_lflg) goto print; break;
	    case 'u':	if (opts.s1_uflg) goto print; break;
	}

	switch (state[CSTATE]) {
	    case 'c':	if (opts.s2_cflg) goto print; break;
	    case 'n':	if (opts.s2_nflg) goto print; break;
	    case 'j':	if (opts.s1_jflg && !isTagFile(file)) goto print; break;
	    case 'x':	if (opts.s1_xflg && !isIgnored(file)) goto print; break;
	}
	return;

print:	print_it(state, file, rev);
}

char *
append_rev(MDBM *db, char *name, char *rev, char *buf)
{
	char *t;

	t = mdbm_fetch_str(db, name);
	unless (t) return (rev);
	sprintf(buf, "%s,%s", t, rev);
	assert(strlen(buf) < MAXPATH);
	return (buf);
}

/*
 * Called for each directory that has an SCCS subdirectory
 */
private void
sccsdir(char *dir, int level, DIR *sccs_dh, char buf[MAXPATH])
{
	MDBM	*gDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	MDBM	*sDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	fifo	dlist = {0, 0};
	fifo	slist = {0, 0};
	struct dirent   *e;
	DIR	*dh; /* dir handle */
	int 	dir_len = strlen(dir);
	char	*p, *gfile;
	datum	k;
	sccs	*s = 0;
	q_item	*i;
#ifndef WIN32
        ino_t	lastInode = 0;
#endif                                 

	/*
	 * Get all the SCCS/?.files
	 */
	while (e = readdir(sccs_dh)) {
#ifndef WIN32
		/*
		 * Linux 2.3.x NFS bug, skip repeats.
		 */
		if (lastInode == e->d_ino) continue;
		lastInode = e->d_ino;
#endif
		if (streq(e->d_name, ".") || streq(e->d_name, "..")) continue;
		/*
		 * Skip files with paths that are too long
		 * pad = "/SCCS/" = 6
		 */
		if ((dir_len + 6 + strlen(e->d_name)) >= MAXPATH) {
			fprintf(stderr,
			    "Warning: %s/%s name too long, skipped\n",
			    dir, e->d_name);
			continue;
		}

		if (strneq("s.", e->d_name, 2)) {
			enqueue(&slist, e->d_name);
		} else {
			if (strneq("c.", e->d_name, 2) &&
			    (p = strrchr(e->d_name, '@'))) {
				/*
				 * Special handling for c.file@rev entry
				 * append the @rev part to the value field
				 * so we can print the correct file
				 * name if it turns out to be a junk file. 
				 */
				*p++ = 0;
				p = append_rev(sDB, e->d_name, p, buf);
			} else {
				p = "";
			} 
			mdbm_store_str(sDB, e->d_name, p, MDBM_INSERT);
		}
	}
	closedir(sccs_dh);

	/*
	 * Get all the gfiles
	 */
	dh = opendir(dir);
	while (e = readdir(dh)) {
#ifndef WIN32
		/*
		 * Linux 2.3.x NFS bug, skip repeats.
		 */
		if (lastInode == e->d_ino) continue;
		lastInode = e->d_ino;
#endif
		if (streq(e->d_name, ".") || streq(e->d_name, "..")) continue;
		if (patheq(e->d_name, "SCCS")) continue;

		/*
		 * Do not descend into another project root. e.g RESYNC
		 */
		if ((level > 0)  && patheq(e->d_name, "BitKeeper")) {
			mdbm_close(gDB);
			mdbm_close(sDB);
			return;
		}

		/*
		 * Skip files with paths that are too long
		 * pad = "/SCCS/s." = 8
		 */
		if ((dir_len + 8 + strlen(e->d_name)) >= MAXPATH) {
			fprintf(stderr,
			    "Warning: %s/%s name too long, skipped\n",
			    dir, e->d_name);
			continue;
		}

		concat_path(buf, dir, e->d_name);
		if (isdir(buf)) {
			enqueue(&dlist, e->d_name);
		} else {
			mdbm_store_str(gDB, e->d_name, "", MDBM_INSERT);
		}
	}
	closedir(dh);
	d_count++;
	if (opts.progress) progress(1);

	/*
	 * First eliminate as much as we can from SCCS dir;
	 * the leftovers in the gDB should be extras.
	 *
	 * Note: We don't use fifo to loop thru the sfile
	 * because if we use mdbm_frist/next, we cannot delete
	 * entry while we are in the first/next loop, it screw up
	 * the mdbm internal index.
	 */
	while (p = dequeue(&slist)) {

		char 	*file;
		char	state[5] = "    ";

		s = 0;
		file = p;
		gfile = &file[2];

		/*
		 * look for p.file,
		 */
		file[0] = 'p';
		if (mdbm_fetch_str(sDB, file)) {
			state[LSTATE] = 'l';
			file[0] = 's';
			concat_path(buf, dir, "SCCS");
			concat_path(buf, buf, file);
			if (opts.s2_cflg &&
			    (s = init(buf, INIT_NOCKSUM, sDB, gDB)) &&
			    chk_diffs(s)) {
				state[CSTATE] = 'c';
			} else {
				state[CSTATE] = 'n';
			}
		} else {
			file[0] = 'z';
			if (mdbm_fetch_str(sDB, file)) {
				state[LSTATE] = 'l';
			} else {
				state[LSTATE] = 'u';
			}
			state[CSTATE] = 'n';
		}

		concat_path(buf, dir, gfile);
		if (opts.s3_pflg) {
			/*
			 * check for pending deltas
			 */
			chk_pending(s, buf, state, sDB, gDB);
		} else {
			state[PSTATE] = ' ';
			do_print(state, buf, 0);
		}

		file[0] = 'c';
		mdbm_delete_str(sDB, file); /* remove c.file entry */
		file[0] = 'd';
		mdbm_delete_str(sDB, file); /* remove d.file entry */
		file[0] = 'p';
		mdbm_delete_str(sDB, file); /* remove p.file entry */
		file[0] = 's';
		mdbm_delete_str(sDB, file); /* remove s.file entry */
		file[0] = 'x';
		mdbm_delete_str(sDB, file); /* remove x.file entry */
		file[0] = 'z';
		mdbm_delete_str(sDB, file); /* remove z.file entry */
		mdbm_delete_str(gDB, gfile);
		if (s) sccs_close(s);
		free(p);
	}

	/*
	 * Check the sDB for "junk" file
	 * XXX TODO: Do we consider the r.file and m.file "junk" file?
	 */
	if (opts.s1_jflg) {
		kvpair  kv;
		concat_path(buf, dir, "SCCS");
		for (kv = mdbm_first(sDB); kv.key.dsize != 0;
						    kv = mdbm_next(sDB)) {
			char buf1[MAXPATH];

			concat_path(buf1, buf, kv.key.dptr);
			if (kv.val.dsize -= 0) {
				do_print(" j ", buf1, 0);
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
					char *q;

					q = strchr(p, ',');
					if (q) *q++ = 0;
					concat_path(buf1, buf1, p);
					do_print("jjjj", buf1, 0);
					p = q;
				}
			}
		}
	}

	/*
	 * Everything left in the gDB is extra
	 */
	if (opts.s1_xflg) {
		for (k = mdbm_firstkey(gDB); k.dsize != 0;
						k = mdbm_nextkey(gDB)) {
			concat_path(buf, dir, k.dptr);
			do_print("xxxx", buf, 0);
		}
	}
	mdbm_close(gDB);

	/*
	 * Make sure the directory in the gdir does not have a s.file
	 */
	strcpy(buf, "s.");
	for (i = dlist.first; i; i = i->next) {
		strcpy(&buf[2], i->path);
		if (mdbm_fetch_str(sDB, buf)) {
			concat_path(buf, dir, i->path);
			fprintf(stderr,
			"Warning: %s should not be a directory\n", buf);
		}
	}
	mdbm_close(sDB);
	

	/*
	 * Process the directories
	 */
	while (p = dequeue(&dlist)) {
		concat_path(buf, dir, p);
		walk(buf, level + 1);
		free(p);
	}
}
