#include "sccs.h"
#include <dirent.h>
#ifdef WIN32
#include "uwtlib/ftw.h"
#else
#include <ftw.h>
#endif
WHATSTR("%W% %@%");

/*
 * sfiles - find SCCS files
 *
 * TODO - make it find only files locked by a particular user
 */
char *sfiles_usage = "\n\
usage: sfiles [-acCdglpPx] [-r[root]] [directories]\n\n\
    -a		when used with -C, list all revs, not just the tip\n\
    -c		list only changed files (locked and modified)\n\
    -C		list leaves which are not in a changeset as file:1.3\n\
    -d		list directories under SCCS control (have SCCS subdir)\n\
    -g		list the gfile name, not the sfile name\n\
    -l		list only locked files\n\
    -p		(paranoid) opens each file to make sure it is an SCCS file\n\
		but only if the pathname to the file is not ../SCCS/s.*\n\
    -P		(paranoid) opens each file to make sure it is an SCCS file\n\
    -r[root]	rebuild the id to pathname cache\n\
    -x		list files which have no revision control files\n\
    		Note: revision control files must look like SCCS/s.*, not\n\
		foo/bar/blech/s.*\n\
\n\
    The -i and -r options can take an optional project root but not any other\n\
    directories.\n\
\n";

int	aFlg, cFlg, Cflg, dFlg, gFlg, lFlg, pFlg, PFlg, rFlg, vFlg, xFlg;
int	error;
FILE	*pending_cache;
FILE	*id_cache;
MDBM	*idDB;
MDBM	*csetDB;	/* database of {file if, tip id} */
int	dups;
int	file(char *f, int (*func)());
int	func(const char *filename, const struct stat *sb, int flag);
int	hasDiffs(char *file);
int	isSccs(char *s);
void	rebuild(void);
int	caches(const char *filename, const struct stat *sb, int flag);
char	*name(char *);

int
main(int ac, char **av)
{
	int	c, i;
	char	*root = 0;
	
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "%s", sfiles_usage);
		exit(0);
	}
	while ((c = getopt(ac, av, "acCdglpPr|vx")) != -1) {
		switch (c) {
		    case 'a': aFlg++; break;
		    case 'c': cFlg++; break;
		    case 'C': Cflg++; rFlg++; break;
		    case 'd': dFlg++; break;
		    case 'g': gFlg++; break;
		    case 'l': lFlg++; break;
		    case 'p': pFlg++; break;
		    case 'P': PFlg++; break;
		    case 'r': rFlg++; root = optarg; break;
		    case 'v': vFlg++; break;
		    case 'x': xFlg++; break;
		    default: goto usage;
		}
	}
	if (rFlg || Cflg) {
		if (av[optind]) {
			fprintf(stderr, "%s: -r must be stand alone.\n", av[0]);
			exit(1);
		}
		/* perror is in sccs_root, don't do it twice */
		unless (sccs_cd2root(0, 0) == 0) {
			purify_list();
			exit(1);
		}
		rebuild();
		purify_list();
		exit(dups ? 1 : 0);
	}
	if (!av[optind]) {
		lftw(".", func, 15);
	} else {
		for (i = optind; i < ac; ++i) {
			if (isdir(av[i])) {
				lftw(av[i], func, 15);
			} else {
				file(av[i], func);
			}
		}
	}
	purify_list();
	exit(0);
}

/*
 * Handle a single file.
 * Convert to s.file first, if possible.
 */
int
file(char *f, int (*func)())
{
	struct	stat sb;
	char	*sfile, *gfile;

	sfile = name2sccs(f);
	gfile = sccs2name(sfile);
	if (lstat(sfile, &sb) == -1) {
		if ((lstat(gfile, &sb) == 0) && xFlg) {
			printf("%s\n", f);
		}
		goto out;
	} else if (!xFlg) {
		func(sfile, &sb);
	}
out:	free(sfile);
	free(gfile);
	return (0);
}

int
func(const char *filename, const struct stat *sb, int flag)
{
	register char *file = (char *)filename;
	register char *s;
	char	*sfile, *gfile;

	if ((file[0] == '.') && (file[1] == '/')) file += 2;
	if (dFlg) {
		if (S_ISDIR(sb->st_mode)) {
			char	buf[MAXPATH];

			sprintf(buf, "%s/SCCS", file);
			if (exists(buf)) printf("%s\n", file);
		}
		return (0);
	}
	if (S_ISDIR(sb->st_mode)) return (0);
	if (xFlg) {
		sfile = name2sccs(file);
		gfile = sccs2name(sfile);
		if (streq(gfile, file) && !exists(sfile)) {
			printf("%s\n", gfile);
		}
		free(sfile);
		free(gfile);
		return (0);
	}
	for (s = file; *s; s++);
	for ( ; s > file; s--) if (s[-1] == '/') break;		/* CSTYLED */
	if (!s || (s[0] != 's') || (s[1] != '.')) return (0);
	if (lFlg || cFlg) {
		*s = 'p';
		if (exists(file)) {
			*s = 's';
			if ((cFlg && hasDiffs(file)) || lFlg) {
				printf("%s\n", name(file));
			}
		}
		*s = 's';
		return (0);
	}
	if (PFlg) {
		if (!isSccs(file)) return (0);
	} else if (pFlg) {
		if ((s-file < 5) || (s[-2] != 'S') || (s[-3] != 'C') ||
		    (s[-4] != 'C') || (s[-5] != 'S')) {
			if (!isSccs(file)) return (0);
		}
	}
	printf("%s\n", name(file));
	return (0);
}

char	*
name(char *sfile)
{
	static	char	buf[MAXPATH];

	if (!gFlg) return (sfile);
	strcpy(buf, sfile);
	sfile = strrchr(buf, '/');
	assert(sfile);
	assert(sfile[1] == 's');
	assert(sfile[2] == '.');
	if ((sfile[-1] == 'S') && (sfile[-2] == 'C') && (sfile > buf + 3)) {
		strcpy(&sfile[-4], &sfile[3]);
	} else {
		strcpy(&sfile[1], &sfile[3]);
	}
	return (buf);
}

int
hasDiffs(char *file)
{
	sccs	*s = sccs_init(file, 0);

	if (!s) return (0);
	if (sccs_hasDiffs(s, 0) >= 1) {
		sccs_free(s);
		return (1);
	}
	sccs_free(s);
	return (0);
}

int
isSccs(char *s)
{
	int	fd = open(s, 0, 0);
	char	buf[8];

	if ((fd == -1) || (read(fd, buf, 8) != 8)) return (0);
	close(fd);
	return ((buf[0] == '\001') && (buf[1] == 'h') &&
	    isdigit(buf[2]) && isdigit(buf[3]) && isdigit(buf[4]) &&
	    isdigit(buf[5]) && isdigit(buf[6]) && (buf[7] == '\n'));
}

/*
 * rebuild the caches.
 */
void
rebuild()
{
	int	i;
	sccs	*cset;
	MDBM	*csetIds();

	unless (cset = sccs_init("SCCS/s.ChangeSet", 0)) {
		perror("sfiles: can't init ChangeSet");
		exit(1);
	}
	unless (HAS_SFILE(cset) && cset->cksumok) {
		perror("sfiles: can't init ChangeSet");
		sccs_free(cset);
		exit(1);
	}

	if (Cflg) {
		unless (csetDB = csetIds(cset, 0, 1)) {
			perror("sfiles: can't init ChangeSet DB");
			sccs_free(cset);
			exit(1);
		}
	}

	unless (rFlg) goto c;

	unless ((i = open("SCCS/z.id_cache", O_CREAT|O_EXCL, 0600)) > 0) {
		fprintf(stderr, "sfiles: can't lock id cache\n");
		exit(1);
	}
	close(i);	/* unlink it when we are done */
	unless (id_cache = fopen("SCCS/x.id_cache", "w")) {
		perror("SCCS/x.id_cache");
		exit(1);
	}
	fprintf(id_cache, "\
# This is a BitKeeper cache file.\n\
# If you suspect that the file is corrupted, simply remove it and \n\
# and it will be rebuilt as needed.  \n\
# The format of the file is <ID> <PATHNAME>\n\
# The file is used for performance during makepatch/takepatch commands.\n");
	idDB = mdbm_open(NULL, 0, 0, 4096);
	assert(idDB);
	mdbm_pre_split(idDB, 1<<10);
c:	lftw(".", caches, 15);
	if (rFlg) {
		fclose(id_cache);
		unlink("SCCS/z.id_cache");
	}
	sccs_free(cset);
}

visit(delta *d)
{
	d->flags |= D_VISITED;
	if (d->parent) visit(d->parent);
}

/*
 * Print out everything leading from start to d, not including start.
 * XXX - stolen from cset.c - both copies need to go in slib.c.
 */
void
csetDeltas(sccs *sc, delta *start, delta *d)
{
	int	i;
	delta	*sfind(sccs *, ser_t);

	unless (d) return;
	if ((d == start) || (d->flags & D_VISITED)) return;
	d->flags |= D_VISITED;
	/*
	 * We don't need the merge pointer, it is part of the include list.
	 * if (d->merge) csetDeltas(sc, start, sfind(sc, d->merge));
	 */
	EACH(d->include) {
		delta	*e = sfind(sc, d->include[i]);

		csetDeltas(sc, e->parent, e);
	}
	csetDeltas(sc, start, d->parent);
	// XXX - fixme - removed deltas not done.
	// Is this an issue?  I think makepatch handles them.
	if (d->type == 'D') printf("%s:%s\n", sc->sfile, d->rev);
}

int
caches(const char *filename, const struct stat *sb, int flag)
{
	register char *file = (char *)filename;
	register char *s;
	sccs	*sc;
	register delta *d, *e;
	char	*path;
	datum	k, v;
	char	buf[MAXPATH*2];

	if ((file[0] == '.') && (file[1] == '/')) file += 2;
	for (s = file; *s; s++);
	for ( ; s > file; s--) if (s[-1] == '/') break;		/* CSTYLED */
	if ((s[0] != 's') || (s[1] != '.')) return (0);
	unless (sc = sccs_init(file, SHUTUP)) return (0);
	unless (HAS_SFILE(sc) && sc->cksumok) {
		sccs_free(sc);
		return (0);
	}
	if (vFlg) printf("%s\n", sc->gfile);

	if (rFlg) {
		/* update the id cache */
		sccs_sdelta(buf, sccs_ino(sc));
		if (sc->tree->pathname &&
		    streq(sc->tree->pathname, sc->gfile)) {
			path = sc->tree->pathname;
		} else {
			path = sc->gfile;
		}
		fprintf(id_cache, "%s %s\n", buf, path);
		k.dptr = buf;
		k.dsize = strlen(buf) + 1;
		v.dptr = path;
		v.dsize = strlen(path) + 1;
		if (mdbm_store(idDB, k, v, MDBM_INSERT)) {
			v = mdbm_fetch(idDB, k);
			fprintf(stderr,
		    	"Duplicate id '%s' for %s\n  Used by %s\n",
		    	buf, path, v.dptr);
				dups++;
		}
	}

	unless (Cflg) {
		sccs_free(sc);
		return (0);
	}

	/* find the leaf of the current LOD and check it */
	sc->state |= RANGE2;
	unless (d = sccs_getrev(sc, 0, 0, 0)) {
		sccs_free(sc);
		return (0);
	}

	/* This currently always fails */
	if (d->cset) {
		sccs_free(sc);
		return (0);
	}
	
	/* Go look for it in the cset */
	sccs_sdelta(buf, sccs_ino(sc));
	k.dptr = buf;
	k.dsize = strlen(buf) + 1;
	v = mdbm_fetch(csetDB, k);
	if (v.dsize) {
		if (e = sccs_findKey(sc, v.dptr)) {
			while (e && (e->type != 'D')) e = e->parent;
			if (e != d) {
				if (aFlg) {
					visit(e);
					csetDeltas(sc, e, d);
				} else {
					printf("%s:%s\n", sc->sfile, d->rev);
				}
			}
		}
	} else {	/* this is a new file */
		if (aFlg) {
			csetDeltas(sc, 0, d);
		} else {
			printf("%s:%s\n", sc->sfile, d->rev);
		}
	}
	sccs_free(sc);
	return (0);
}




_ftw_get_flag(const char *dir, struct stat *sb)
{

	if (lstat(dir, sb) != 0) return FTW_NS;
	if ((S_ISDIR(sb->st_mode))) return FTW_D;

	/* everything else is considered a file */
	return FTW_F;
}


/*
 * Walk a directory tree recusivly, 
 * A tree pointed to by a sym-link is ignored
 * TODO: handle the "depth" parameter
 */
int
lftw(const char *dir,
	int(*func)(const char *file, struct stat *sb, int flag),
	int depth)
{
	DIR	*d;
	struct	dirent *e;
	struct	stat sbuf;
	char	tmp_buf[MAXPATH];
	char	*slash = "/";
	int	flag, rc = 0, first_time = 1;

	flag = _ftw_get_flag(dir, &sbuf);

	if ((rc = (*func)(dir, &sbuf, flag)) != 0) return rc;

	if (flag != FTW_D) return 0;

	/*
	 * if we get here, the top level node is a directory
	 * now we process its children
	 */
	/* CSTYLED */
	for (;;) {
		if (first_time) {
			int len = strlen(dir);
			first_time = 0;
			if ((len > 1) && (dir[len -1] == '/')) slash = "";
			if ((d = opendir(dir)) == NULL) {
				perror(dir);
				goto done;
			}
		}
		e = readdir(d);
		if (e == NULL) goto done;
		if ((strcmp(e->d_name, ".") == 0) ||
		    (strcmp(e->d_name, "..") == 0))
			continue;  /* skip "." && ".." */

		/* now we do the real work */
		sprintf(tmp_buf, "%s%s%s", dir, slash, e->d_name);
		flag = _ftw_get_flag(tmp_buf, &sbuf);
		if (S_ISDIR(sbuf.st_mode)) {
			lftw(tmp_buf, func, 0);
		} else {
			if ((rc = (*func)(tmp_buf, &sbuf, flag)) != 0) {
				goto done;
			}
		}

	}
done:
	closedir(d);
	return rc;
}

