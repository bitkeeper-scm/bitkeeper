/* Copyright (c) 2000 Andrew Chang */ 
#include "system.h"
#include "sccs.h"
 
WHATSTR("@(#)%K%");

typedef struct {
	u32     show_markers:1;		/* show markers		*/
	u32     aflg:1;			/* disable ignore list	*/
	u32     Aflg:1;			/* use with -p, show	*/
					/* all pending deltas	*/
	u32     jflg:1;			/* show SCCS/junk file 	*/
	u32     sflg:1;			/* show all sfile	*/
	u32     cflg:1;     		/* show changed files	*/
	u32     lflg:1;     		/* show locked files	*/
	u32     nflg:1;     		/* show no-changed file	*/
	u32     uflg:1;     		/* show unlocked files	*/
	u32     pflg:1;     		/* show pending files	*/
	u32     xflg:1;     		/* show xtra files	*/
	u32     dflg:1;     		/* show empty Dir	*/
	u32     gflg:1;     		/* print in gfile format*/
	u32     Cflg:1;     		/* file@rev format*/
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

private void do_print(char lock_state, char state, char pending, char *file);

void
enqueue(fifo *q, char *path)
{
	q_item *t = (q_item *) malloc(sizeof(q_item));
	
	assert(t);
	t->path = strdup(path);
	t->next = 0;
	if (q->first == NULL) {
		q->first = t;
		q->last = t;
	} else {
		q->last->next = t;
		q->last = t;
	}
}

char *
dequeue(fifo *q)
{
	char *t;
	q_item *i;

	if (q->first == NULL) return NULL;
	i = q->first;
	if (q->first == q->last) {
		q->first = NULL;
	} else {
		q->first = q->first->next;
	}
	t = i->path;
	free(i);
	return (t);
}

private inline sccs *
init(char *name, int flags)
{
        sccs    *s = sccs_init(name, flags|INIT_SAVEPROJ, proj);
 
        if (s && !proj) proj = s->proj;
        return (s);
}


sfind_main(int ac, char **av)
{
        int     c, i; 
	char	*root, *path, buf[MAXPATH];

	while ((c = getopt(ac, av, "aAcCdDeEgjklmpPrRsuvx")) != -1) {
		switch (c) {
		    case 'a':	opts.aflg = 1; break;
		    case 'A':	opts.Aflg = 1; break;
		    case 'c':	opts.cflg = 1; break;
		    case 'g':	opts.gflg = 1; break;
		    case 'j':	opts.jflg = 1; break;
		    case 'l':	opts.lflg = 1; break;
		    case 'n':	opts.nflg = 1; break;
		    case 'C':	opts.Cflg = 1; 
				opts.pflg = 1;
				break; /* backward compat */
		    case 'p':	opts.pflg = 1; break; /* replace old -C */
		    case 'u':	opts.uflg = 1; break; /* new semantic */
		    case 'x':	opts.xflg = 1; break;
		    case 'm':	opts.show_markers = 1; break;
		    case 'E':	opts.cflg = 1;	/* detail scan everything */
				opts.pflg = 1;
				/* fall thur */
		    case 'e':	opts.jflg = 1;	/* quick scan every */
				opts.lflg = 1;
				opts.nflg = 1;
				opts.xflg = 1;
				break;
		    case 's': 	opts.cflg = 1;
				opts.lflg = 1;
				opts.nflg = 1;
				break;
		    default: 	fprintf(stderr ,"usage: sfile2: ....\n");
				return(1);
		}
	}

	/*
	 * If user did not select any option,
	 * setup a default mode for them
	 */
	if (!opts.cflg && !opts.jflg && !opts.lflg && !opts.pflg &&
			    !opts.nflg && !opts.uflg && !opts.xflg) {
		opts.uflg = 1;
		opts.lflg = 1;
	}


	if (!opts.aflg && (root = sccs_root(0))) {
		FILE	*ignoref; 

		sprintf(buf, "%s/BitKeeper/etc/ignore", root);
		unless (exists(buf)) get(buf, SILENT, "-");
		if (ignoref = fopen(buf, "r")) {
			ignore = read_globs(ignoref, 0);
			fclose(ignoref);
		}
		free(root);
		root = 0;
	}           
	

	if (!av[optind]) {
		path = ".";
		walk(path, 0);
	} else if (streq("-", av[optind])) {
                char    buf[MAXPATH];

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

	if (ignore) free_globs(ignore);  
}

void 
chk_sfile(char *name, char *lock_state, char *state)
{
	char	*s;
	sccs	*sc = 0;

	s = strrchr(name, '/');

	if (s[1] == 's') {
		s[1] = 'p';
		if (exists(name)) {
			*lock_state = 'l';
			s[1] = 's';
			if (opts.cflg && 
			    (sc = init(name, INIT_NOCKSUM)) &&
			    chk_diffs(sc)) { 
				*state = 'c';
			} else {
				*state = 'n';
			}
		} else {
			s[1] = 'z';
			if (exists(name)) {
				*lock_state = 'l';
				*state = 'n';
			} else {
				*lock_state = 'u';
				*state = 'n';
			}
			s[1] = 's';
		}
	}
	if (sc) sccs_close(sc);
}

int
file(char *f)
{
	char	name[MAXPATH], buf[MAXPATH];
	char    *s, *sfile = 0, lock_state = ' ', state, pending;
	sccs	*sc;

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
			state = 'j';
			strcpy(buf, f);
		} else {
			chk_sfile(f, &lock_state, &state);
			s = sccs2name(f);
			strcpy(buf, s);
			sfile = name2sccs(s);
			free(s);
		}
	} else {
		/*
		 * TODO: we need to check for the caes where
		 * the pwd is a SCCS dir
		 * This can fool the current code into wronly treating them
		 * as xtras.
		 */
		/* this file is a gname */
		sfile = name2sccs(f); 
		unless (exists(sfile)) {
			state = 'x';
		} else {
			chk_sfile(sfile, &lock_state, &state);
		}
		strcpy(buf, f);
	}

	/*
	 * When we get here. buf contain the gname
	 * Now we check for pending deltas
	 */
	if (opts.pflg && state != 'x' &&  state != 'j') {
		delta *d;
		char tmp[MAXPATH];

		/*
		 * check for pending deltas
		 * TODO: need to handle LOD
		 */                                    
		sc = init(sfile, INIT_NOCKSUM);
		
		d = sccs_top(sc);
		assert(d);
		pending = (d->flags & D_CSET) ? ' ' : 'p';
		if (opts.Aflg) {
			while (d) {
				if (d->flags & D_CSET) break;
				sprintf(tmp, "%s@%s", buf, d->rev);
				do_print(lock_state, state, pending, tmp);
				d = d->parent;
			}
		} else {
			if (opts.Cflg) {
				sprintf(tmp, "%s@%s", buf, d->rev);
				do_print(lock_state, state, pending, tmp);
			} else {
				do_print(lock_state, state, pending, buf);
			}
		}
	} else  {
		if (state == 'x' || state == 'j') {
			if (exists(buf)) {
				do_print(' ', state, ' ', buf);
			}
		} else {
			do_print(lock_state, state, ' ', buf);
		}
	}
	if (sfile) free(sfile);
}



int
walk(char *path, int level)
{
	struct dirent   *e;
	DIR	*d;
	char	*p, buf[MAXPATH];
	fifo	dlist = {0, 0};
#ifndef WIN32
        ino_t	lastInode = 0;
#endif                                 

	assert(isdir(path));
	concat_path(buf, path, "SCCS");
	unless (exists(buf)) {
		/*
		 * TODO: we need to check for the caes where
		 * the pwd is a SCCS dir
		 * This can fool the current code into wronly treating them
		 * as xtras.
		 */
		if ((d = opendir(path)) == NULL) {
			perror(path);
			return;
		}
		while ((e = readdir(d)) != NULL) { 
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
			if (!opts.aflg && match_globs(e->d_name, ignore)) {
				debug((stderr, "SKIP\t%s\n", e->d_name));
				continue;
			}
			concat_path(buf, path, e->d_name);
			unless (isdir(buf)) {
				do_print(' ', 'x', ' ', buf);
			} else {
				enqueue(&dlist, buf);
			}
		}
		closedir(d);
		while (p = dequeue(&dlist)) {
			walk(p, level + 1);
			free(p);
		}
	} else {
		sccsdir(path, level);
	}
}

int
chk_diffs(sccs *s)
{
	if (!s) return (0);
	if (sccs_hasDiffs(s, 0) >= 1) return (1);
	return (0);  
}

private void
print_it(char lock_state, char state, char pending, char *file)
{
	char *sfile, *gfile;

	gfile =  strneq("./",  file, 2) ? &file[2] : file;
	if (strneq("BitKeeper/log/", gfile, 14)) return;

	if (opts.show_markers) printf("%c%c%c ", lock_state, state, pending);
	if (opts.gflg || (state == 'x') || (state == 'j')) {
		/* print in gfile format */
		printf("%s\n", gfile);
	} else {
		char *p;

		/* print in sfile format */
		p = strchr(gfile, '@');
		if (p) *p = 0;
		sfile = name2sccs(gfile);
		fputs(sfile, stdout);
		if (p) {
			*p = '@';
			fputs(p, stdout);
		}
		fputs("\n", stdout);
		free(sfile);
	}
}

private void
do_print(char lock_state,  char state, char pending, char *file)
{
	int show_it = 0;

	switch (lock_state) {
	    case 'l':	if (opts.lflg) show_it++; break;
	    case 'u':	if (opts.uflg) show_it++; break;
	}

	switch (state) {
	    case 'c':	if (opts.cflg) show_it++; break;
	    case 'n':	if (opts.nflg) show_it++; break;
	    case 'j':	if (opts.jflg) show_it++; break;
	    case 'x':	if (opts.xflg) show_it++; break;
	}
	if ((pending == 'p') && opts.pflg) show_it++;

	if (show_it) print_it(lock_state, state, pending, file);
}

/*
 * Called for each directory that has an SCCS subdirectory
 */
int
sccsdir(char *path, int level)
{
	MDBM	*gDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	MDBM	*sDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	fifo	dlist = {0, 0};
	fifo	slist = {0, 0};
	struct dirent   *e;
	DIR	*dir;
	char	*p, *gfile, buf[MAXPATH];
	datum   k, v;
	kvpair  kv;
	struct 	stat sb;
	sccs	*s = 0;
#ifndef WIN32
        ino_t	lastInode = 0;
#endif                                 

	 mdbm_set_alignment(gDB, sizeof(sb.st_mode));
	/*
	 * Get all the gfiles
	 */
	dir = opendir(path);
	while (e = readdir(dir)) {
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
		 * do not decent in to another project root. e.g RESYNC
		 */
		if ((level > 0)  && patheq(e->d_name, "BitKeeper")) {
			fprintf(stderr, "Warning: ignoring sub-root: %s\n", path);
			return;
		}

		if (!opts.aflg && match_globs(e->d_name, ignore)) {
			debug((stderr, "SKIP\t%s\n", e->d_name));
			continue;
		}

		// XXX TODO skip files with paths that are too long

		concat_path(buf, path, e->d_name);
		lstat(buf, &sb);
		if (S_ISDIR(sb.st_mode)) {
			enqueue(&dlist, buf);
		} else {
			k.dptr = e->d_name;
			k.dsize = strlen(e->d_name) + 1;
			v.dptr = (char *) &(sb.st_mode);
			v.dsize = sizeof(sb.st_mode);
			mdbm_store(gDB, k, v, MDBM_INSERT);
		}
	}
	closedir(dir);

	/*
	 * Get all the SCCS/?.files
	 * TODO compute sPath() for split root config
	 */
	concat_path(buf, path, "SCCS");
	dir = opendir(buf);
	while (e = readdir(dir)) {
#ifndef WIN32
		/*
		 * Linux 2.3.x NFS bug, skip repeats.
		 */
		if (lastInode == e->d_ino) continue;
		lastInode = e->d_ino;
#endif
		if (streq(e->d_name, ".") || streq(e->d_name, "..")) continue;
		// XXX TODO skip files with paths that are too long

		if (strneq("s.", e->d_name, 2)) {
			enqueue(&slist, e->d_name);
		} else {
			mdbm_store_str(sDB, e->d_name, "", MDBM_INSERT);
		}
	}
	closedir(dir);

	/*
	 * First eliminate as much we can from SCCS dir;
	 * the leftovers should be extras or directories in the g MDBM,
	 * and there should be NOTHING leftover in the s MDBM.
	 */
	while (p = dequeue(&slist)) {

		//char	file[MAXPATH];
		char	*file;
		char	lock_state, state, pending;
		delta	*d;

		s = 0;
		file = p;
		unless (strneq("s.", file, 2)) continue;
		gfile = &file[2];
		k.dptr = gfile;
		k.dsize = strlen(gfile) + 1;
		v = mdbm_fetch(gDB, k);
		if (v.dptr) {
			/* check that it is not a DIR, if it is that is an
			 * error because it is and s.file with a dir where a
			 * gfile wants to be.
			 */
			if (S_ISDIR(*((mode_t *)v.dptr))) {
				concat_path(buf, path, gfile);
				fprintf(stderr,
				    "%s should not be a directory\n", buf);
			}
			mdbm_delete(gDB, k);
		} 
		/*
		 * look for p.file,
		 */
		state = '?'; pending = '?';
		file[0] = 'p';
		if (mdbm_fetch_str(sDB, file)) {
			mdbm_delete_str(sDB, file); /* remove p.file entry */
			lock_state = 'l';
			file[0] = 's';
			concat_path(buf, path, "SCCS");
			concat_path(buf, buf, file);
			if (opts.cflg &&
			    (s = init(buf, INIT_NOCKSUM)) &&
			    chk_diffs(s)) {
				state = 'c';
			} else {
				//state = 'l';
				state = 'n';
			}
		} else {
			file[0] = 'z';
			if (mdbm_fetch_str(sDB, file)) {
				mdbm_delete_str(sDB, file);
				lock_state = 'l';
				//state = 'l';
				state = 'n';
			} else {
				lock_state = 'u';
				state = 'n';
			}
		}

		
		file[0] = 'x';
		mdbm_delete_str(sDB, file); /* remove x.file entry */
		
		/*
		 * check for pending deltas
		 * TODO: need to handle LOD
		 */
		concat_path(buf, path, gfile);
		if (opts.pflg) {
			char tmp[MAXPATH];

			file[0] = 's';
			unless (s) {
				concat_path(tmp, path, "SCCS");
				concat_path(tmp, tmp, file);
				s = init(tmp, INIT_NOCKSUM);
				assert(s);
			}
			assert(s && s->tree);
			d = sccs_top(s);
			assert(d);
			pending = (d->flags & D_CSET) ? ' ' : 'p';
			if (opts.Aflg) {
				while (d) {
					if (d->flags & D_CSET) break;
					sprintf(tmp, "%s@%s", buf, d->rev);
					do_print(lock_state, state,
								pending, tmp);
					d = d->parent;
				}
			} else {
				if (opts.Cflg) {
					sprintf(tmp, "%s@%s", buf, d->rev);
					do_print(lock_state, state,
								pending, tmp);
				} else {
					do_print(lock_state, state,
								pending, buf);
				}
			}
		} else {
			pending = ' ';
			do_print(lock_state, state, pending, buf);
		}

		if (s) sccs_close(s);
		free(p);
	}

	/*
	 * Check that s MDBM is empty at this point
	 * Anything in the array should be listed as a "junk" file
	 * XXX TODO: Do we consider the r.file and m.file "junk" file ?
	 */
	if (opts.jflg) {
		concat_path(buf, path, "SCCS");
		for (k = mdbm_firstkey(sDB); k.dsize != 0;
						    k = mdbm_nextkey(sDB)) {
			char buf1[MAXPATH];

			if (strneq("x.", k.dptr, 2)) continue;
			concat_path(buf1, buf, k.dptr);
			do_print(' ', 'j', ' ', buf1);
		}
	}
	mdbm_close(sDB);

	/*
	 * everything left in the g array is extra
	 */
	if (opts.xflg) {
		for (kv = mdbm_first(gDB); kv.key.dsize != 0;
							kv = mdbm_next(gDB)) {
			concat_path(buf, path, kv.key.dptr);
			do_print(' ', 'x', ' ', buf);
		}
	}
	mdbm_close(gDB);

	/*
	 * Process the directories
	 */
	while (p = dequeue(&dlist)) {
		walk(p, level + 1);
		free(p);
	}
}
