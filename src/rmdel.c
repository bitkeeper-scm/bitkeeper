/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");
char	*rmdel_help = "\n\
usage: rmdel [-DqsS] [-r<rev>] file\n\n\
    -D		destroy all information newer than this revision\n\
    -q		run quietly\n\
    -n		when used with -S, do not do it, just tell what would be done\n\
    -r<r>	rmdel revision <r>\n\
    -S		remove the set of specified deltas\n\
    -s		run quietly\n\
    -v		run more noisily (use with -S)\n\n";

void	rmcaches(void);
int	undo_main(int dont, int verbose);

/*
 * The weird setup is so that I can #include this file into sccssh.c
 */
int
rmdel_main(int ac, char **av, char *out)
{
	sccs	*s;
	int	c, flags = 0, errors = 0;
	int	verbose = 0, undo = 0, dont = 0;
	char	*name, *rev = 0;
	int	destroy = 0;
	int	invalidate = 0;
	delta	*d;
	char	lastname[MAXPATH];

	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr, rmdel_help);
		return (1);
	}
	while ((c = getopt(ac, av, "Dqr;sSv")) != -1) {
		switch (c) {
		    case 'D': destroy = 1; break;
		    case 'q': flags |= SILENT; break;
		    case 'r': rev = optarg; break;
		    case 'n': dont++; break;
		    case 's': flags |= SILENT; break;
		    case 'S': undo = 1; break;
		    case 'v': verbose++; break;
		    default:
			fprintf(stderr,
			    "rmdel: usage error, try rmdel --help\n");
			return (1);
		}
	}
	if (undo) {
		if (rev) {
			fprintf(stderr,
			    "rmdel: -S and -r<rev> are incompatible\n");
			exit(1);
		}
		if (destroy) {
			fprintf(stderr,
			    "rmdel: -S and -D are incompatible\n");
			exit(1);
		}
		unless (av[optind] && streq(av[optind], "-")) {
			fprintf(stderr,
			   "rmdel: -S mode accepts only files:revs on stdin\n");
			exit(1);
		}
		return (undo_main(dont, verbose));
	}
	name = sfileFirst("rmdel", &av[optind], 0);
	while (name) {
		unless (s = sccs_init(name, flags, 0)) {
			name = sfileNext();
			continue;
		}
		if (!s->tree) {
			if (!(s->state & S_SFILE)) {
				if (streq(lastname, s->sfile)) goto next;
				fprintf(stderr, "rmdel: %s doesn't exist.\n",
				    s->sfile);
			} else {
				perror(s->sfile);
			}
			goto next;
		}
		name = rev ? rev : sfileRev();
		unless (d = sccs_getrev(s, name, 0, 0)) {
			fprintf(stderr,
			    "rmdel: can't find %s:%s\n", s->gfile, name);
			goto next;
		}

		/*
		 * If they wanted to destroy the delta and it is the root
		 * delta, then blow the entire file away.
		 */
		if (destroy && (d == s->tree)) {
			if (sccs_clean(s, SILENT)) {
				fprintf(stderr,
				    "rmdel: can't remove edited %s\n",
				    s->gfile);
				errors = 1;
				goto next;
			}
			/* see ya! */
			verbose((stderr, "rmdel: remove %s\n", s->sfile));
			unlink(s->sfile);
			invalidate++;
			strcpy(lastname, s->sfile);
			goto next;
		}
		lastname[0] = 0;

		if (sccs_rmdel(s, d, destroy, flags)) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "rmdel of %s failed, skipping it.\n", name);
			}
			errors = 1;
		}
next:		sccs_free(s);
		name = sfileNext();
	}
	sfileDone();
#ifndef	NOPURIFY
	purify_list();
#endif
	if (invalidate) rmcaches();
	return (errors);
}

void
rmcaches()
{
	// XXX - needs to be updated when we move the cache to BitKeeper/caches
	if (sccs_cd2root(0, 0) == 0) {
		unlink(IDCACHE);
	}
}

int
main(int ac, char **av)
{
	return (rmdel_main(ac, av, "-"));
}

/*
 * undo - take a list of s.files and remove the specified deltas
 *
 * From undo.c:1.3 @(#)lm@lm.bitmover.com|src/undo.c|19990818003019
 *
 * XXX - there needs to be a way to lock the entire tree while this is
 * running.
 * XXX - this code assumes one LOD is operated on per line.  In other words,
 * changesets can not span LODs.
 */
int	undo_dont = 0;
int	undo_v = 0;

sccs	*copy(sccs *s);
void	init(void);
int	undo(sccs *s);
void	cleanup(void) NORETURN;
void	apply(void) NORETURN;
void	getfiles(sccs *s, char *rev);
int	applyfiles(char *file, char *lastrev);
void	done(int pass);

int
undo_main(int dont, int verbose)
{
	sccs	*s;
	char	*name;
	char	*Av[2];
	int	flags = SILENT;
	RANGE_DECL;

	if (verbose) flags = 0;
	undo_dont = dont;
	undo_v = verbose;
	init();
	Av[0] = "-";
	Av[1] = 0;
	for (name = sfileFirst("undo", Av, 0); name; name = sfileNext()) {
		/*
		 * For the undo command, it is an error to try and undo
		 * on anything that doesn't exist.
		 */
		unless ((s = sccs_init(name, flags, 0)) && s->tree) cleanup();
		RANGE("undo", s, 2, 1);
		undo(s);
next:		sccs_free(s);
	}
	sfileDone();
	apply();
	purify_list();
	return (0);
}

void
init(void)
{
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "undo: can't find BitKeeper root.\n");
		exit(1);
	}
	if (mkdir("UNDO", 0775) == -1) {
		perror("UNDO");
		exit(1);
	}
}

/*
 * Try to do the undo and then verify that the file is in a sane state.
 */
int
undo(sccs *s)
{
	delta	*d, *e = 0, *f;

	/* Figure out the "GCA". */
	for (d = s->table; d; d = d->next) {
		if (d->flags & D_SET) e = d;
		d->flags &= ~D_VISITED;		/* used below */
	}
	if (!e) {
		fprintf(stderr, "undo: no deltas marked in %s.\n", s->sfile);
		cleanup();
	}

	/*
	 * We want to reapply everything that is off on a branch because we
	 * need to renumber it.  So back up the "gca" until we are on the
	 * trunk.  This is the code that assumes that all deltas to be removed
	 * are within one LOD.
	 */
	while (e->r[2]) e = e->parent;
	if (e->flags & D_SET) e = e->next;

	/* Pick up the metadata deltas. */
	for (d = s->table; d != e; d = d->next) {
		unless (d->flags & D_SET) continue;
		for (f = d->parent; f && (f->type != 'D'); f = f->parent);
		unless (f && (f->flags & D_SET)) continue;
		for (f = d->parent; f && (f->type != 'D'); f = f->parent) {
			f->flags |= D_SET;
		}
	}

	if (IS_EDITED(s)) {
		// XXX - try cleaning it
		fprintf(stderr, "undo: %s is edited.\n", s->sfile);
		cleanup();
	}
	unless (sccs_lock(s, 'z')) {
		fprintf(stderr, "undo: can not lock %s\n", s->sfile);
		cleanup();
	}

	/*
	 * No ancestor should mean we are removing the entire file.
	 * We record that as a zero lengthed file.
	 */
	unless (e) {
		char	zero[MAXPATH];
		int	fd;

		for (e = s->table; e; e = e->next) {
			if (e->flags & D_SET) continue;
			fprintf(stderr,
			    "No ancestor for %s and unmarked deltas\n",
			    s->sfile);
			cleanup();
		}
		sprintf(zero, "UNDO/%s", s->sfile);
		fd = creat(zero, GROUP_MODE);
		if (fd == -1) mkdirp(zero);
		close(creat(zero, GROUP_MODE));
		assert(exists(zero) && (size(zero) == 0));
		if (undo_v > 1) fprintf(stderr, "%s: remove all\n", s->sfile);
		if (undo_v > 2) fprintf(stderr, "ZERO: %s\n", zero);
		return (0);
	}

	/* Convert s to copied file.  The other sccs * is freed above */
	unless (s = copy(s)) {
		sccs_unlock(s, 'z');
		cleanup();
	}
	getfiles(s, e->rev);
	if (applyfiles(s->sfile, e->rev)) {
		sccs_free(s);
		cleanup();
	}
	sccs_free(s);
	return (0);
}

/*
 * get all the init and diffs files.
 */
void
getfiles(sccs *s, char *rev)
{
	FILE	*f;
	int	n, i;
	delta	*d, *e, **list;
	char	*t;
	char	path[MAXPATH];

	/*
	 * Get a list of deltas which we will be putting back.
	 * We want them in oldest .. newest order and the table
	 * is in newest .. oldest order.
	 *
	 * Find the oldest delta marked for removal, then go back and
	 * find the ones not marked for removal.
	 */
	e = sccs_getrev(s, rev, 0, 0);
	for (n = 0, d = s->table; d != e; d = d->next) {
		unless (d->flags & D_SET) n++;
	}
	if (n == 0) return;
	list = calloc(n, sizeof(delta *));
	for (n = 0, d = s->table; d != e; d = d->next) {
		unless (d->flags & D_SET) {
			list[n++] = d;
		} else if (undo_v > 1) {
			fprintf(stderr, "%s: deleting %s\n", s->sfile, d->rev);
		}
	}
	i = n - 1;
	for (n = 0; i >= 0; i--, n++) {
		d = list[i];
		if (undo_v > 1) {
			fprintf(stderr, "%s: saving %s\n", s->sfile, d->rev);
		}
		sprintf(path, "%s-I%d", s->sfile, n);
		t = strrchr(path, '/');
		assert(t && (t[1] == 's'));
		t[1] = 'U';
		unless (f = fopen(path, "w")) {
			mkdirp(path);
			unless (f = fopen(path, "w")) {
				perror(path);
				cleanup();
			}
		}
		assert(d->parent);
		sccs_pdelta(s, d->parent, f);
		fprintf(f, "\n");
		s->rstop = s->rstart = d;
		sccs_prs(s, PRS_PATCH|SILENT, 0, NULL, f);
		fclose(f);
		sprintf(path, "%s-D%d", s->sfile, n);
		t = strrchr(path, '/');
		assert(t && (t[1] == 's'));
		t[1] = 'U';
		if (d->type == 'D') {
			sccs_getdiffs(s, d->rev, 0, path);
		} else {
			/* make a null file */
			close(creat(path, GROUP_MODE));
		}
	}
}

/*
 * Copy the file into the UNDO tree.
 */
sccs	*
copy(sccs *s)
{
	char	to[MAXPATH];
	sccs	*t;
	delta	*d, *e;

	sprintf(to, "UNDO/%s", s->sfile);
	if (fileCopy(s->sfile, to)) return (0);
	unless (t = sccs_init(to, INIT_NOCKSUM, 0)) return (0);
	for (d = s->table, e = t->table; d; d = d->next, e = e->next) {
		if (d->flags & D_SET) {
			e->flags |= D_SET;
		}
	}
	return (t);
}

/*
 * Init the file,
 * delete everything after the last rev,
 * and apply the saved deltas.
 */
int
applyfiles(char *file, char *lastrev)
{
	sccs	*s;
	char	path[MAXPATH];
	char	key[MAXPATH];
	FILE	*f;
	MMAP	*m;
	int	error = 0, n;
	delta	*d, *e;
	char	*t;

	unless (s = sccs_init(file, 0, 0)) {
		fprintf(stderr, "undo: can't init %s\n", file);
		return (-1);
	}
	unless (d = sccs_getrev(s, lastrev, 0, 0)) {
		fprintf(stderr, "undo: can't find %s in %s\n", lastrev, file);
		sccs_free(s);
		return (-1);
	}
	if (undo_v > 1) {
		fprintf(stderr,
		    "%s: removing all after %s\n", s->sfile, d->rev);
	}
	if (sccs_rmdel(s, sccs_next(s, d), 1, SILENT)) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr, "undo: can't rmdel %s..\n", lastrev);
		}
		return (-1);
	}
	sccs_free(s);
	unless (s = sccs_init(file, 0, 0)) {
		fprintf(stderr, "undo: can't init %s\n", file);
		return (-1);
	}
	for (n = 0; ; n++) {
		sprintf(path, "%s-I%d", file, n);
		t = strrchr(path, '/');
		assert(t && (t[1] == 's'));
		t[1] = 'U';
		unless (f = fopen(path, "r")) break;
		unless (fnext(key, f)) {
			perror("key read from init file");
			return (-1);
		}
		unless (d = sccs_findKey(s, key)) {
			fprintf(stderr,
			    "undo: can't find %s in %s\n", key, s->sfile);
			fprintf(stderr,
"This usually means that one of the deltas not removed is a child of one\n\
of the deltas which was removed.  That shouldn't happen unless you are\n\
running this command with a hand generated list of deltas.\n\
Please check the list and try again.\n");
			return (-1);
		}
		unless (sccs_restart(s)) { perror("restart"); exit(1); } 
		e = sccs_getInit(s, 0, f, 1, &error, 0);
		unless (e && !error) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr, "undo: can't init %s in %s\n",
				    path, s->sfile);
			}
			return (-1);
		}
		fclose(f);
		free(e->rev);
		e->rev = 0;
		unlink(path);
		sprintf(path, "%s-D%d", file, n);
		t = strrchr(path, '/');
		assert(t && (t[1] == 's'));
		t[1] = 'U';
		if (e->flags & D_META) {
			if (sccs_meta(s, d, path)) {
				perror("meta");
				return (-1);
			}
		} else {
			m = mopen(path);
			assert(f);
			if (sccs_get(s, d->rev,
			    0, 0, 0, SILENT|GET_SKIPGET|GET_EDIT, "-")) {
				perror("get");
				return (-1);
			}
			if (sccs_delta(s,
			    SILENT|DELTA_FORCE|DELTA_PATCH, e, 0, m)) {
				perror("delta");
				return (-1);
			}
			if (s->state & S_BAD_DSUM) return (-1);
		}
		unlink(path);
	}
	if (sccs_admin(s, ADMIN_BK, 0, 0, 0, 0, 0, 0, 0)) return (1);
	sccs_free(s);
	return (0);
}

int
mkdirp(char *file)
{
	char	*s;
	char	*t;
	char	buf[MAXPATH];

	strcpy(buf, file);	/* for !writable string constants */
	unless (s = strrchr(buf, '/')) return (0);
	*s = 0;
	if (isdir(buf)) return (0);
	for (t = buf; t < s; ) {
		if (t > buf) *t++ = '/';
		if (t < s) {
			while ((*t != '/') && (t < s)) t++;
			*t = 0;
		}
		mkdir(buf, 0775);
	}
	*s = '/';
	return (0);
}

int
fileCopy(char *from, char *to)
{
	char	buf[8192];
	int	n, from_fd, to_fd;
	struct	stat sb;

	mkdirp(to);
	if ((from_fd = open(from, 0, 0)) == -1) {
		perror(from);
		return (-1);
	}
	if (fstat(from_fd, &sb) == -1) {
		perror(from);
		return (-1);
	}
	if ((to_fd = creat(to, sb.st_mode & 0777)) == -1) {
		perror(to);
		return (-1);
	}
	while ((n = read(from_fd, buf, sizeof(buf))) > 0) {
		if (write(to_fd, buf, n) != n) {
			perror(to);
			return (-1);
		}
	}
	close(from_fd);
	close(to_fd);
	return (0);
}

#define	CHECK_PASS	1
#define	MOVE_PASS	2
#define	UNLOCK_PASS	4

/*
 * For each file in the UNDO tree, unlock the corresponding file.
 * Then blow away the UNDO tree.
 */
void
cleanup()
{
	unless (chdir("UNDO") == 0) {
		perror("chdir UNDO");
		exit(1);
	}
	done(UNLOCK_PASS);
	exit(1);
}

void
check()
{
	system("bk sfiles -P . | bk check -");
}

void
apply()
{
	/*
	 * Yes this multi pass approach is slow but I want everything
	 * checked before doing anything.  
	 */
	unless (chdir("UNDO") == 0) {
		perror("chdir UNDO");
		exit(1);
	}
	done(CHECK_PASS);
	check();
	done(MOVE_PASS|UNLOCK_PASS);
	exit(0);
}

void
Rename(char *old, char *new)
{
	mkdirp(new);
	if (exists(new) && unlink(new)) {
		perror("unlink");
		cleanup();
	}
	unless (rename(old, new)) {
		return;
	}
	if (errno != EBUSY) {
		perror("rename");
		cleanup();
	}

	/*
	 * NFS gives EBUSY under Linux sometimes, it's a Linux bug.
	 * Try copying and unlinking the source.
	 */
	if (fileCopy(old, new) == 0) {
		unlink(old);
		return;
	}
	fprintf(stderr, "Unable to rename(%s, %s)\n", old, new);
	cleanup();
}

void
done(int pass)
{
	FILE	*f;
	char	*t;
	sccs	*s;
	delta	*d;
	char	buf[MAXPATH]; 
	
	f = popen("bk sfiles .", "r");
	buf[0] = buf[1] = '.'; buf[2] = '/';
	while (fgets(&buf[3], MAXPATH-3, f)) {
		if (undo_v > 2) fprintf(stderr, "Processing %s", buf);
		chop(buf);
		if (pass & (MOVE_PASS|CHECK_PASS)) {
			char	*name;

			/*
			 * Remove the file if we have a zero sized file.
			 * We also remove the lock file because in the next
			 * pass this zero lengthed file is gone so we won't
			 * know to remove the lock file.
			 */
			if (size(&buf[3]) == 0) {
				if (pass & MOVE_PASS) {
					if (undo_v) {
						fprintf(stderr, "rm %s\n", buf);
					}
					unless (undo_dont) {
						unlink(buf);
						t = strrchr(buf, '/');
						assert(t && (t[1] == 's'));
						t[1] = 'z';
						if (unlink(buf)) {
							perror("unlink z.file");
						}
					}
				}
				goto un;
			}

			/* 
			 * Move the file to where it was as of TOT after the
			 * other stuff was removed.
			 * Make sure that there isn't anyone else in that
			 * spot - which shouldn't happen...
			 */
			s = sccs_init(&buf[3], INIT_NOCKSUM, 0);
			assert(s);
			s->state |= S_RANGE2;
			d = sccs_getrev(s, 0, 0, 0);
			assert(d);
			if (undo_v > 1) {
				fprintf(stderr,
				    "%s TOT %s\n", s->sfile, d->rev);
			}
			name = name2sccs(d->pathname);
			unless (streq(name, &buf[3])) {
				char	path[MAXPATH];

				sprintf(path, "../%s", name);
				if (exists(path)) {
					fprintf(stderr, "%s exists\n", name);
					cleanup();
				}
				if (pass & MOVE_PASS) {
					if (undo_v) {
						fprintf(stderr, "mv %s %s\n",
						    &buf[3], path);
					}
					unless (undo_dont) {
						Rename(&buf[3], path);
					}
					sprintf(path, "../%s", &buf[3]);
					if (undo_v) {
						fprintf(stderr,
						    "rm old %s\n", path);
					}
					unless (undo_dont) {
						unlink(path);
					}
				}
			} else if (pass & MOVE_PASS) {
				if (undo_v) {
					fprintf(stderr,
					    "mv %s %s\n", &buf[3], buf);
				}
				unless (undo_dont) {
					Rename(&buf[3], buf);
				}
			}
			sccs_free(s);
			free(name);
		}
un:		if (pass & UNLOCK_PASS) {
			t = strrchr(buf, '/');
			assert(t && (t[1] == 's'));
			t[1] = 'z';
			if (unlink(buf)) perror("unlink z.file");
		}
	}
	pclose(f);
	if (pass & CHECK_PASS) return;

	chdir("..");
	f = popen("find UNDO -depth -print", "r");
	while (fnext(buf, f)) {
		chop(buf);
		if (undo_v) fprintf(stderr, "remove %s\n", buf);
		if (rmdir(buf) && unlink(buf)) {
			perror(buf);
			exit(1);
		}
	}
}
