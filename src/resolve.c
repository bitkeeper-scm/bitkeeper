/*
 * resolve.c - multi pass resolver for renames, contents, etc.
 *
 * a) pull everything out of RESYNC into RENAMES which has pending name changes
 * b) build a DBM of rootkeys true/false of all the files in the RESYNC dir
 *    (takepatch could do this and leave it in BitKeeper/tmp).
 * c) process the files in RENAMES, resolving any conflicts, and put the file
 *    in RESYNC - if there isn't a file there already.  If there is, ask if 
 *    they want to move it and if so, move it to RENAMES.
 * d) before putting the file in RESYNC, if the same pathname is taken in the
 *    repository and the rootkey of that file is not in our list, then ask
 *    the user if they want to move the other file out of the way or move
 *    this file somewhere else.  If somewhere else is chosen, repeat process
 *    using that name.  If move is chosen, then copy the file to RENAMES and
 *    put this file in RESYNC.
 * e) repeat RENAMES processing until RENAMES dir is empty.  Potentially all
 *    files could pass through RENAMES.
 * f) move each file from RESYNC to repository, making sure that if the file
 *    exists in the repository, we have either overwritten it or deleted it.
 *
 * Invariants:
 * opts->rootDB{key} = <path>.  If the key is there then the inode is in the
 * 	RESYNC directory under <path>.
 */
#include "resolve.h"
#include "comments.c"

int
main(int ac, char **av)
{
	int	c;
	static	opts opts;	/* so it is zero */

	opts.pass1 = opts.pass2 = opts.pass3 = opts.pass4 = 1;

	while ((c = getopt(ac, av, "l|m;acdq1234")) != -1) {
		switch (c) {
		    case 'l':
		    	if (optarg) {
				opts.log = fopen(optarg, "a");
			} else {
				opts.log = stderr;
			}
			break;
		    case 'a': opts.automerge = 1; break;
		    case 'd': opts.debug = 1; break;
		    case 'c': opts.noconflicts = 1; break;
		    case 'm': opts.mergeprog = optarg; break;
		    case 'q': opts.quiet = 1; break;
		    case '1': opts.pass1 = 0; break;
		    case '2': opts.pass2 = 0; break;
		    case '3': opts.pass3 = 0; break;
		    case '4': opts.pass4 = 0; break;
		}
    	}
	unless (opts.mergeprog) opts.mergeprog = "merge";
	if ((av[optind] != 0) && isdir(av[optind])) chdir(av[optind]);
	c = passes(&opts);
	return (c);
}

/*
 * Do the setup and then work through the passes.
 */
int
passes(opts *opts)
{
	sccs	*s;
	char	buf[MAXPATH];
	FILE	*p;

	/*
	 * Make sure we are where we think we are.  
	 */
	unless (exists("BitKeeper/etc")) sccs_cd2root(0, 0);
	unless (exists("BitKeeper/etc")) {
		fprintf(stderr, "resolve: can't find project root.\n");
		exit(1);
	}
	unless (exists(ROOT2RESYNC)) {
	    	fprintf(stderr,
		    "resolve: can't find RESYNC dir, nothing to resolve?\n");
		exit(0);
	}

	unless (opts->idDB = loadDB(IDCACHE, 0, DB_NODUPS)) {
		fprintf(stderr, "resolve: can't open %s\n", IDCACHE);
		exit(1);
	}
	opts->local_proj = sccs_initProject(0);
	chdir(ROOT2RESYNC);
	opts->resync_proj = sccs_initProject(0);

	/*
	 * Pass 1 - move files to RESYNC and/or build up rootDB
	 */
	unless (p = popen("bk sfiles .", "r")) {
		perror("popen of bk sfiles");
		return (1);
	}
	unless (opts->rootDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) {
		perror("mdbm_open");
		pclose(p);
		return (1);
	}
	if (opts->log) {
		fprintf(opts->log,
		    "==== Pass 1%s ====\n",
		    opts->pass1 ? "" : ": build DB for later passes");
	}
	while (fnext(buf, p)) {
		if (strneq("BitKeeper/RENAMES/SCCS/s.", buf, 25)) continue;
		chop(buf);
		unless (s = sccs_init(buf, INIT, opts->resync_proj)) continue;
		unless (opts->pass1) {
			char	path[MAXKEY];

			sccs_sdelta(s, sccs_ino(s), path);
			saveKey(opts, path, buf);
			sccs_free(s);
		} else {
			pass1_renames(opts, s);
		}
	}
	pclose(p);
	if (opts->pass1 && !opts->quiet) {
		fprintf(stdlog,
		    "resolve: found %d renames in pass 1\n", opts->renames);
	}

	/*
	 * If there are edited files, bail.
	 * This shouldn't happen, takepatch looks for them and 
	 * the repository is locked, but just in case...
	 */
	if (opts->edited) {
		SHOUT();
		fprintf(stderr,
"There are modified files in the repository which are also in the patch.\n\
BitKeeper is aborting this patch until you check in those files.\n\
You need to check in the edited files and run takepatch on the file\n\
in the PENDING directory.  Alternatively, you can rerun pull or resync\n\
that will work too, it just gets another patch.\n");
		resolve_cleanup(opts, CLEAN_RESYNC);
	}

	/*
	 * Pass 2 - move files back into RESYNC
	 */
	if (opts->pass2) {
		int	old, n = -1;

		if (opts->log) fprintf(opts->log, "==== Pass 2 ====\n");

		/*
		 * Keep going as long as we are reducing the list.
		 * We can hit cases where there is a conflict that would
		 * go away if we processed the whole list.
		 */
		do {
			old = n;
			n = pass2_renames(opts);
		} while (n && ((old == -1) || (n < old)));

		unless (n) {
			unless (opts->quiet) {
				fprintf(stdlog,
				    "resolve: resolved %d renames in pass 2\n",
				    opts->renames2);
			}
			goto pass3;
		}

		/*
		 * Now do the same thing, calling the resolver.
		 */
		opts->resolveNames = 1;
		n = -1;
		do {
			old = n;
			n = pass2_renames(opts);
		} while (n && ((old == -1) || (n < old)));
		if (n && opts->pass4) {
			fprintf(stderr,
			    "Did not resolve %d renames, abort\n", n);
			exit(1);
		}
		unless (opts->quiet) {
			fprintf(stdlog,
			    "resolve: resolved %d renames in pass 2\n",
			    opts->renames2);
		}
	}

	/*
	 * Pass 3 - resolve content/permissions/flags conflicts.
	 */
pass3:	if (opts->pass3) pass3_resolve(opts);

	/*
	 * Pass 4 - apply files to repository.
	 */
	if (opts->pass4) pass4_apply(opts);
	
	freeStuff(opts);

	/*
	 * Whoohooo...
	 */
	return (0);
}

/* ---------------------------- Pass1 stuff ---------------------------- */

/*
 * Save the key away so we know what we have in the RESYNC directory.
 * This is called for each file in the RESYNC dir.
 */
void
saveKey(opts *opts, char *key, char *file)
{
	if (mdbm_store_str(opts->rootDB, key, file, MDBM_INSERT)) {
		fprintf(stderr, "Duplicate key: %s\n", key);
		fprintf(stderr, "\twanted by %s\n", file);
		fprintf(stderr,
		    "\tused by %s\n", mdbm_fetch_str(opts->rootDB, key));
		exit(1);
	} else if (opts->debug) {
		fprintf(stdlog, "saveKey(%s)->%s\n", key, file);
	}
}

/*
 * If there is a file in the repository in the same location,
 * and it is the same file (root keys match),
 * then return true. 
 *
 * If there is no matching file in local (both path slot and key slot), then
 * return 0.
 */
int
nameOK(opts *opts, sccs *s)
{
	char	path[MAXPATH];
	char	buf[MAXPATH];
	sccs	*local;

	/*
	 * Same path slot and key?
	 */
	sprintf(path, "%s/%s", RESYNC2ROOT, s->sfile);
	if ((local = sccs_init(path, INIT, opts->local_proj)) &&
	    HAS_SFILE(local)) {
		if (IS_EDITED(local) && sccs_clean(local, SILENT)) {
			fprintf(stderr,
			    "Warning: %s is modified, will not overwrite it.\n",
			    local->gfile);
			opts->edited = 1;
		}
		sccs_sdelta(local, sccs_ino(local), path);
		sccs_sdelta(s, sccs_ino(s), buf);
		if (opts->debug) {
		    fprintf(stdlog,
			"nameOK(%s) = pathmatch %d\n",
			s->gfile, streq(path, buf));
		}
		sccs_free(local);
		return (streq(path, buf));
	}
	if (local) sccs_free(local);

	chdir(RESYNC2ROOT);
	sccs_sdelta(s, sccs_ino(s), buf);
	local = sccs_keyinit(buf, INIT, opts->local_proj, opts->idDB);
	if (local) {
		if (IS_EDITED(local) &&
		    sccs_clean(local, SILENT|CLEAN_SHUTUP)) {
			fprintf(stderr,
			    "Warning: %s is modified, will not overwrite it.\n",
			    local->gfile);
			opts->edited = 1;
		}
		sccs_free(local);
		chdir(ROOT2RESYNC);
		if (opts->debug) {
			fprintf(stdlog,
			    "nameOK(%s) = 0 because key match\n", s->gfile);
		}
		return (0);
	} else if (exists(s->gfile)) {
		chdir(ROOT2RESYNC);
		if (opts->debug) {
			fprintf(stdlog,
			    "nameOK(%s) = 0 because of gfile\n", s->gfile);
		}
		return (0);
	}
	chdir(ROOT2RESYNC);
	if (opts->debug) fprintf(stdlog, "nameOK(%s) = 1\n", s->gfile);
	return (1);
}

/*
 * Pass 1 - move to RENAMES and/or build up the DBM of root keys in RESYNC.
 */
void
pass1_renames(opts *opts, sccs *s)
{
	char	path[MAXPATH];
	static	int filenum;
	char	*mfile;
	char	*rfile;

	if (nameOK(opts, s)) {
		sccs_sdelta(s, sccs_ino(s), path);
		saveKey(opts, path, s->sfile);
		sccs_free(s);
		return;
	}

	unless (filenum) {
		mkdir("BitKeeper/RENAMES", 0775);
		mkdir("BitKeeper/RENAMES/SCCS", 0775);
	}
	do {
		sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", ++filenum);
	} while (exists(path));
	mfile = strdup(sccs_Xfile(s, 'm'));
	rfile = strdup(sccs_Xfile(s, 'r'));
	sccs_close(s);
	if (opts->debug) {
		fprintf(stdlog, "%s -> %s\n", s->sfile, path);
	}
	if (rename(s->sfile, path)) {
		fprintf(stderr, "Unable to rename(%s, %s)\n", s->sfile, path);
		exit(1);
	} else if (opts->log) {
		fprintf(opts->log, "rename(%s, %s)\n", s->sfile, path);
	}
	if (exists(mfile)) {
		sprintf(path, "BitKeeper/RENAMES/SCCS/m.%d", filenum);
		if (rename(mfile, path)) {
			fprintf(stderr,
			    "Unable to rename(%s, %s)\n", mfile, path);
			exit(1);
		} else if (opts->log) {
			fprintf(opts->log, "rename(%s, %s)\n", mfile, path);
		}
	}
	if (exists(rfile)) {
		sprintf(path, "BitKeeper/RENAMES/SCCS/r.%d", filenum);
		if (rename(rfile, path)) {
			fprintf(stderr,
			    "Unable to rename(%s, %s)\n", rfile, path);
			exit(1);
		} else if (opts->log) {
			fprintf(opts->log, "rename(%s, %s)\n", rfile, path);
		}
	}
	sccs_free(s);
	free(mfile);
	free(rfile);
	opts->renames++;
}

/* ---------------------------- Pass2 stuff ---------------------------- */

/*
 * pass2_renames - for each file in RENAMES, try to move it in place.
 * Return the number of files found in RENAMES.
 * This is called in two passes - the first pass just moves nonconflicting
 * files into place.  The second pass, indicated by opts->resolveNames,
 * handles all conflicts.
 * This routine may be called multiple times in each pass, until it gets to
 * a pass where it does no work.
 */
int
pass2_renames(opts *opts)
{
	char	path[MAXPATH];
	sccs	*s;
	delta	*e;
	names	*names = 0;
	DIR	*dir;
	struct dirent *d;
	int	n = 0;

	unless (dir = opendir("BitKeeper/RENAMES/SCCS")) return (0);
	while (d = readdir(dir)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		n++;
		unless (d->d_name[0] == 's') continue;
		sprintf(path, "BitKeeper/RENAMES/SCCS/%s", d->d_name);
		unless ((s = sccs_init(path, INIT, opts->resync_proj)) &&
		    s->tree) {
			if (s) sccs_free(s);
			fprintf(stderr, "Ignoring %s\n", path);
			continue;
		}
		d->d_name[0] = 'm';
		sprintf(path, "BitKeeper/RENAMES/SCCS/%s", d->d_name);
		names = getnames(path, 'm');
		unless (e = sccs_getrev(s, "+", 0, 0)) {
			/* should NEVER happen */
			fprintf(stderr, "Can't find TOT in %s\n", path);
			fprintf(stderr, "ERROR: File left in %s\n", path);
			sccs_free(s);
			freenames(names, 1);
			continue;
		}
		
		/*
		 * If we have a "names", then we have a conflict or a 
		 * rename, so do that.  Otherwise, this must be a create.
		 */
		if (names) {
			rename_file(opts, s, names, path);
			freenames(names, 1);
		} else {
			create(opts, s, e);
		}
		sccs_free(s);
	}
	closedir(dir);
	return (n);
}

/*
 * Extract the names from either a
 * m.file: rename local_name gca_name remote_name
 * or an
 * r.file: merge deltas 1.491 1.489 1.489.1.21 lm 00/01/08 10:39:11
 */
names	*
getnames(char	*path, int type)
{
	FILE	*f = fopen(path, "r");
	names	*names = 0;
	char	*s, *t;
	char	buf[MAXPATH*3];

	unless (f) return (0);
	unless (fnext(buf, f)) {
out:		fclose(f);
		freenames(names, 1);
		return (0);
	}
	unless (names = calloc(1, sizeof(*names))) goto out;
	if (type == 'm') {
		unless (strneq("rename ", buf, 7)) {
			fprintf(stderr, "BAD m.file: %s", buf);
			goto out;
		}
		s = &buf[7];
	} else {
		assert(type == 'r');
		unless (strneq("merge deltas ", buf, 13)) {
			fprintf(stderr, "BAD r.file: %s", buf);
			goto out;
		}
		s = &buf[13];
	}
	/* rename local_name gca_name remote_name
	 * merge deltas 1.491 1.489 1.489.1.21 lm 00/01/08 10:39:11
	 */
	t = s;
	unless (s = strchr(s, ' ')) goto out;
	*s++ = 0;
	unless (names->local = strdup(t)) goto out;
	t = s;
	unless (s = strchr(s, ' ')) goto out;
	*s++ = 0;
	unless (names->gca = strdup(t)) goto out;
	t = s;
	if (s = strchr(t, ' ')) {	/* r.file case */
		*s = 0;
	} else {			/* m.file case */
		chop(t);
	}
	unless (names->remote = strdup(t)) goto out;
	fclose(f);
	return (names);
}

/*
 * We think it is a create, but it might be a rename because an earlier
 * invocation of resolve already renamed it and threw away the m.file.
 * In that case, treat it like a non-conflicting rename with local & gca
 * the same and the remote side whatever it is in the RESYNC dir.
 *
 * It's a create if we can't find the file using the key and there is no
 * other file in that path slot.
 *
 * We do all the movement work here.  The lower level routines may make space
 * for us.
 */
int
create(opts *opts, sccs *s, delta *d)
{
	char	buf[MAXKEY];
	sccs	*local;
	char	*sfile = name2sccs(d->pathname);
	int	ret = 0;
	int	how;

	if (opts->debug) fprintf(stdlog, "create(%s)\n", d->pathname);

	/* 
	 * See if this name is taken in the repository.
	 */
	sccs_sdelta(s, sccs_ino(s), buf);
	chdir(RESYNC2ROOT);
again:	if (how = slotTaken(opts, sfile)) {
		chdir(ROOT2RESYNC);

		/* If we are just looking for space, skip this one */
		unless (opts->resolveNames) return (-1);

		if (opts->noconflicts) {
	    		fprintf(stderr, "resolve: can't process conflict\n");
	    		fprintf(stderr, "\t%s vs %s\n", s->sfile, sfile);
			opts->errors = opts->hadconflicts = 1;
			free(sfile);
			return (-1);
		}

		if (resolve_create(opts, s, how) == EAGAIN) goto again;
		return;
	}
	
	/*
	 * Mebbe we resolved this rename already.
	 */
	if (local = sccs_keyinit(buf, INIT, opts->local_proj, opts->idDB)) {
		names	*n;

		if (opts->debug) {
			fprintf(stdlog, "%s already renamed to %s\n",
			    local->gfile, d->pathname);
		}
		n = calloc(1, sizeof(*n));
		n->local = strdup(local->gfile);
		n->gca = strdup(local->gfile);
		n->remote = d->pathname;
		free(sfile);
		sccs_free(local);
		chdir(ROOT2RESYNC);
		return (rename_file(opts, s, n, 0));
	}

	/*
	 * OK, looking like a real create.
	 */
	sccs_close(s);
	chdir(ROOT2RESYNC);
	unless (ret) ret = rename(s->sfile, sfile);
	if (opts->debug) {
		fprintf(stdlog, "%s -> %s = %d\n", s->gfile, d->pathname, ret);
	}
	saveKey(opts, buf, sfile);
	free(sfile);
	opts->renames2++;
	return (ret);
}

void
freenames(names *names, int free_struct)
{
	unless (names) return;
	if (names->local) free(names->local);
	if (names->gca) free(names->gca);
	if (names->remote) free(names->remote);
	if (free_struct) free(names);
}

/*
 * Handle renames.
 */
int
rename_file(opts *opts, sccs *s, names *names, char *mfile)
{
	char	*sfile = name2sccs(names->remote);
	int	ret = 0;

	/*
	 * We can handle local or remote moves in no conflict mode,
	 * but not both.
	 */
	if (opts->noconflicts && 
	    (!streq(names->local, names->gca) &&
	    !streq(names->gca, names->remote))) {
	    	fprintf(stderr, "resolve: can't process rename conflict\n");
		fprintf(stderr,
		    "%s ->\n\tLOCAL:  %s\n\tGCA:    %s\n\tREMOTE: %s\n",
		    s->gfile, names->local, names->gca, names->remote);
		opts->hadconflicts = 1;
		return (-1);
	}

	/*
	 * For now, handle only remote updates.
	 */
	unless (streq(names->local, names->gca)) {
		fprintf(stderr, "Skipping local update XXX\n");
		fprintf(stderr,
		    "%s ->\n\tLOCAL:  %s\n\tGCA:    %s\n\tREMOTE: %s\n",
		    s->gfile, names->local, names->gca, names->remote);
		return (-1);
	}

	if (opts->debug) {
		fprintf(stdlog,
		    "%s ->\n\tLOCAL:  %s\n\tGCA:    %s\n\tREMOTE: %s\n",
		    s->gfile, names->local, names->gca, names->remote);
	}

	/*
	 * Make sure that the slot in the RESYNC dir isn't taken,
	 * then make sure the slot in the repository isn't taken.
	 */
	if (exists(sfile)) {
		fprintf(stderr, "%s (%s) exists in RESYNC\n", sfile, s->gfile);
		return (-1);
	}

	chdir(RESYNC2ROOT);
	if (slotTaken(opts, sfile)) {
		chdir(ROOT2RESYNC);
		return (-1);
	}
	chdir(ROOT2RESYNC);
	unless (ret = move_remote(opts, s, sfile)) {
		if (mfile) unlink(mfile);
	}
	free(sfile);
	return (ret);
}

/*
 * Move a file into the RESYNC dir in a new location and delta it such that
 * it has recorded the new location in the s.file.
 * If there is an associated r.file, update that to have the new tip[s].
 * Note that the sccs pointer is closed after this move and may be out of date.
 */
int
move_remote(opts *opts, sccs *s, char *sfile)
{
	int	ret;
	delta	*d;
	char	*rfile;
	char	*t;
	names	*names;
	char	buf[MAXKEY];

	if (opts->debug) {
		fprintf(stdlog, "move_remote(%s, %s)\n", s->gfile, sfile);
	}

	sccs_close(s);
	if (ret = rename(s->sfile, sfile)) return (ret);
	unless (opts->resolveNames) opts->renames2++;
	if (opts->log) fprintf(opts->log, "rename(%s, %s)\n", s->sfile, sfile);
	sccs_sdelta(s, sccs_ino(s), buf);
	saveKey(opts, buf, sfile);
	rfile = sccs_Xfile(s, 'r');
	strcpy(buf, rfile);		/* get a local copy */
	rfile = buf;
	if (exists(rfile)) {
		t = strrchr(sfile, '/');
		t[1] = 'r';
		if (ret = rename(rfile, sfile)) return (ret);
		if (opts->log) {
			fprintf(opts->log, "rename(%s, %s)\n", rfile, sfile);
		}
		strcpy(buf, sfile);	/* now rfile -> new r.file location */
		t[1] = 's';		/* and sfile is back to s.file */
		names = getnames(rfile, 'r');
		d = sccs_getrev(s, names->local, 0, 0);
		assert(d);
		t = name2sccs(d->pathname);
		unless (streq(t, sfile)) rev(opts, sfile, d, rfile, 1);
		free(t);
		d = sccs_getrev(s, names->remote, 0, 0);
		assert(d);
		t = name2sccs(d->pathname);
		unless (streq(t, sfile)) rev(opts, sfile, d, rfile, 2);
		free(t);
		freenames(names, 1);

	} else {
		d = sccs_getrev(s, "+", 0, 0);
		t = name2sccs(d->pathname);
		unless (streq(t, sfile)) rev(opts, sfile, d, rfile, 0);
		free(t);
	}
	/* Nota bene: *s is hopelessly out of date */
	return (0);
}

/*
 * Add a null rename delta to the specified tip.
 * If which is set, update the r.file with the new data.
 *	which == 1 means do local rev, 2 means do remote rev.
 */
rev(opts *opts, char *sfile, delta *d, char *rfile, int which)
{
	char	buf[MAXPATH+100];
	FILE	*f;
	char	*t;
	char	*r;

	if (opts->debug) {
		fprintf(stdlog, "rev(%s %s)\n", sfile, d->rev);
	}
	sprintf(buf, "bk get -e%s -r%s %s",
	    opts->log ? "" : "q", d->rev, sfile);
	system(buf);
	if (which) {
		t = strrchr(sfile, '/');
		assert(t && (t[1] == 's'));
		t[1] = 'p';
		f = fopen(sfile, "r");
		t[1] = 's';
		fnext(buf, f);
		fclose(f);
		fprintf(stderr, "%s\n", buf);
		r = strchr(buf, ' ');
		r++;
		t = strchr(r, ' ');
		*t = 0;
		r = strdup(r);
		f = fopen(rfile, "r");
		fnext(buf, f);
		fclose(f);
		f = fopen(rfile, "w");
		/* 0123456789012
		 * merge deltas 1.9 1.8 1.8.1.3 lm 00/01/15 00:25:18
		 */
		if (which == 2) {
			t = strchr(&buf[13], ' ');	/* after 1.9 */
			t = strchr(++t, ' ');		/* after 1.8 */
			*t = 0;
			fprintf(f, "%s %s", buf, r);
			t = strchr(++t, ' ');		/* after 1.8.1.3 */
			fprintf(f, t);
		} else {
			fprintf(f, "merge deltas %s ", r);
			t = strchr(&buf[13], ' ');
			fprintf(f, ++t);
		}
		fclose(f);
	}
	t = sccs2name(sfile);
	sprintf(buf, "bk delta %s -y'Rename: %s -> %s' %s",
	    opts->log ? "" : "-q", d->pathname, t, sfile);
	free(t);
	system(buf);
}

/*
 * Return 1 if the pathname in question is in use in the repository by
 * an SCCS file.  
 * Return 2 if the pathname in question is in use in the repository by
 * a file without an SCCS file.
 */
int
slotTaken(opts *opts, char *slot)
{
	if (opts->debug) fprintf(stdlog, "slotTaken(%s) = ", slot);

	if (exists(slot)) {
		char	buf2[MAXKEY];
		sccs	*local = sccs_init(slot, INIT, opts->local_proj);

		/*
		 * If we can find this key in the RESYNC dir then it is
		 * not a conflict, the file has bee successfully moved.
		 */
		sccs_sdelta(local, sccs_ino(local), buf2);
		sccs_free(local);
		unless (mdbm_fetch_str(opts->rootDB, buf2)) {
			if (opts->debug) {
				fprintf(stdlog,
				    "%s exists in local repository\n", slot);
			}
			if (opts->debug) fprintf(stdlog, "1\n");
			return (SFILE_CONFLICT);
		}
	} else {
		char	*gfile = sccs2name(slot);

		if (exists(gfile)) {
			free(gfile);
			if (opts->debug) fprintf(stdlog, "2\n");
			return (GFILE_CONFLICT);
		}
	}
	if (opts->debug) fprintf(stdlog, "0\n");
	return (0);
}

/* ---------------------------- Pass3 stuff ---------------------------- */

/*
 * Handle all the non-rename conflicts.
 */
int
pass3_resolve(opts *opts)
{
	char	buf[MAXPATH];
	FILE	*p;
	int	n = 0;

	if (opts->log) fprintf(opts->log, "==== Pass 3 ====\n");

	/*
	 * Make sure the renames are done first.
	 * XXX - doesn't look in RENAMES, should it?
	 */
	unless (p = popen("find . -type f -name 'm.*' -print", "r")) {
		perror("popen of find");
		exit (1);
	}
	while (fnext(buf, p)) {
		fprintf(stderr, "Needs rename: %s", &buf[2]);
		n++;
	}
	pclose(p);
	if (n) {
		fprintf(stderr,
"There are %d pending renames which need to be resolved before the conflicts\n\
can be resolved.  Please rerun resolve and fix these first.\n", n);
		exit(1);
	}

	/*
	 * Process any conflicts.
	 * Note: this counts on the rename resolution having bumped these
	 * revs if need be.
	 */
	unless (p = popen("find . -type f -name 'r.*' -print", "r")) {
		perror("popen of find");
		exit (1);
	}
	while (fnext(buf, p)) {
		chop(buf);
		conflict(opts, &buf[2]);
	}
	pclose(p);
	unless (opts->quiet) {
		fprintf(stdlog,
		    "resolve: resolved %d conflicts in pass 3\n",
		    opts->resolved);
	}

	if (opts->errors) {
		fprintf(stderr, "resolve: had errors, nothing is applied.\n");
		exit(1);
	}

	return (0);
}

/*
 * Read the r.file, get the versions, run the merge tool.
 */
void
conflict(opts *opts, char *rfile)
{
	names	*names;
	char	*name;
	char	*t;
	
	t = strrchr(rfile, '/');
	assert(t[1] == 'r');
	t[1] = 's';
	name = sccs2name(rfile);
	t[1] = 'r';

	names = getnames(rfile, 'r');
	if (opts->debug) {
		fprintf(stderr, "Conflict: %s %s %s %s\n",
		    rfile, names->local, names->gca, names->remote);
	}
	if (opts->noconflicts) {
	    	fprintf(stderr,
		    "resolve: can't process conflict in %s\n", name);
		free(name);
		freenames(names, 1);
		opts->hadconflicts = 1;
		return;
	}

	/*
	 * If we are automerging, try that.  The interface to the merge
	 * program is "merge left_vers gca_vers right_vers merge_vers"
	 * and the program must return as follows:
	 * 0 for no overlaps, 1 for some overlaps, 2 for errors.
	 */
	gotComment = 0; comment = 0;
	if (opts->automerge) {
		automerge(opts, name, rfile, names);
		free(name);
		freenames(names, 1);
		return;
	}
}

/*
 * Try to automerge.
 */
void
automerge(opts *opts, char *name, char *rfile, names *revs)
{
	char	cmd[MAXPATH*4];
	sccs	*s;
	char	*t;
	int	ret;
	names	tmp;
	int	flags = PRINT | (opts->quiet ? SILENT : 0);
	
	t = strrchr(rfile, '/');
	assert(t[1] == 'r');
	t[1] = 's';
	unless (s = sccs_init(rfile, INIT, opts->resync_proj)) {
		opts->errors = 1;
		t[1] = 'r';
		return;
	}
	t[1] = 'r';
	t = basenm(name);

	/*
	 * If it is the ChangeSet file, just edit it, we'll finish later.
	 */
	if (streq("SCCS/r.ChangeSet", rfile)) {
		(void)edit(opts, s, revs);
		sccs_free(s);
		unlink(rfile);
		return;
	}

	sprintf(cmd, "BitKeeper/tmp/%s@%s", t, revs->local);
	tmp.local = strdup(cmd);
	sprintf(cmd, "BitKeeper/tmp/%s@%s", t, revs->gca);
	tmp.gca = strdup(cmd);
	sprintf(cmd, "BitKeeper/tmp/%s@%s", t, revs->remote);
	tmp.remote = strdup(cmd);
	if (sccs_get(s, revs->local, 0, 0, 0, flags, tmp.local)) {
		fprintf(stderr, "Unable to get %s\n", tmp.local);
		opts->errors = 1;
		freenames(&tmp, 0);
		return;
	}
	if (sccs_get(s, revs->gca, 0, 0, 0, flags, tmp.gca)) {
		fprintf(stderr, "Unable to get %s\n", tmp.gca);
		opts->errors = 1;
		freenames(&tmp, 0);
		return;
	}
	if (sccs_get(s, revs->remote, 0, 0, 0, flags, tmp.remote)) {
		fprintf(stderr, "Unable to get %s\n", tmp.remote);
		opts->errors = 1;
		freenames(&tmp, 0);
		return;
	}
	sprintf(cmd, "bk %s %s %s %s %s",
	    opts->mergeprog, tmp.local, tmp.gca, tmp.remote, name);
	ret = system(cmd) & 0xffff;
	unlink(tmp.local);
	unlink(tmp.gca);
	unlink(tmp.remote);
	freenames(&tmp, 0);
	if (ret == 0) {
		unless (opts->quiet) {
			fprintf(stdlog, "automerge of %s OK\n", name);
		}
		if (edit(opts, s, revs)) {
			sccs_free(s);
			return;
		}
		comment = "automerge";
		gotComment = 1;
		sccs_restart(s);
		flags = DELTA_DONTASK|DELTA_FORCE|(opts->quiet ? SILENT : 0);
		if (sccs_delta(s, flags, 0, 0, 0, 0)) {
			sccs_whynot("delta", s);
			opts->errors = 1;
			return;
		}
	    	opts->resolved++;
		unlink(rfile);
		return;
	}
	sccs_free(s);
	if (ret == 0xff00) {
	    	fprintf(stderr, "Can not execute '%s'\n", cmd);
		opts->errors = 1;
		return;
	}
	if ((ret >> 8) == 1) {
		fprintf(stderr, "Conflicts during merge of %s\n", name);
		opts->hadconflicts = 1;
		return;
	}
	fprintf(stderr, "Automerge of %s failed for unknown reasons\n", name);
	opts->errors = 1;
	return;
}

/*
 * Figure out which delta is the branch one and merge it in.
 */
int
edit(opts *opts, sccs *s, names *revs)
{
	char	*branch;
	int	flags = GET_EDIT|GET_SKIPGET;

	branch = strchr(revs->local, '.');
	assert(branch);
	if (strchr(++branch, '.')) {
		branch = revs->local;
	} else {
		branch = revs->remote;
	}
	if (opts->quiet) flags |= SILENT;
	if (sccs_get(s, 0, branch, 0, 0, flags, "-")) {
		fprintf(stderr, "resolve: can not edit/merge %s\n", s->sfile);
		opts->errors = 1;
		return (1);
	}
	return (0);
}

/* ---------------------------- Pass4 stuff ---------------------------- */

int
pendingRenames()
{
	DIR	*dir;
	struct dirent *d;
	int	n = 0;

	unless (dir = opendir("BitKeeper/RENAMES/SCCS")) return (0);
	while (d = readdir(dir)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		fprintf(stderr,
		    "Pending: BitKeeper/RENAMES/SCCS/%s\n", d->d_name);
		n++;
	}
	closedir(dir);
	return (n);
}

/*
 * Return true if there are pending deltas.
 */
int
pending()
{
	FILE	*f = popen("bk sfiles -C", "r");
	int	ret = fgetc(f) != EOF;

	pclose(f);
	return (ret);
}

/*
 * Commit the changeset.
 *
 * XXX - need to check logging.
 */
void
commit(opts *opts)
{
	if (opts->quiet) {
		system("bk commit -sRFf -yMerge");
	} else {
		system("bk commit -RFf -yMerge");
	}
}

/*
 * Make sure there are no edited files, no RENAMES, and {r,m}.files and
 * apply all files.
 */
int
pass4_apply(opts *opts)
{
	sccs	*r, *l;
	char	buf[MAXPATH];
	char	key[MAXKEY];
	int	offset = strlen(ROOT2RESYNC) + 1;	/* RESYNC/ */
	FILE	*p;
	FILE	*get;
	int	n = 0;

	if (opts->log) fprintf(opts->log, "==== Pass 4 ====\n");

	if (pendingRenames()) exit(1);
	unless (p = popen("find . -type f -name '[mr].*' -print", "r")) {
		perror("popen of find");
		exit (1);
	}
	while (fnext(buf, p)) {
		fprintf(stderr, "Pending: %s", &buf[2]);
		n++;
	}
	pclose(p);
	if (n) exit(1);

	/*
	 * Pass 4b - create the cset.
	 * We need to do a commit if the ChangeSet file needs one and/or
	 * if there are any pending changes in the RESYNC dir.  We have
	 * go check for the pendings because we might have made them in
	 * an earlier run of this program.  Then this run wouldn't know
	 * that work has been done.
	 */
	if (exists("SCCS/p.ChangeSet") || pending()) {
		commit(opts);
	}

	/*
	 * Pass 4a - check for edited files and remove old files.
	 * This can leave us in a pretty bad state if there are
	 * edited files, but all files have been checked twice,
	 * once by takepatch and then once by nameOK() in pass1.
	 */
	chdir(RESYNC2ROOT);
	sprintf(key, "bk sfiles %s", ROOT2RESYNC);
	unless (p = popen(key, "r")) {
		perror("popen of bk sfiles");
		exit (1);
	}
	while (fnext(buf, p)) {
		chop(buf);
		unless (r = sccs_init(buf, INIT, opts->resync_proj)) {
			fprintf(stderr,
			    "resolve: can't init %s - abort.\n", buf);
			fprintf(stderr, "resolve: no files were applied.\n");
			exit(1);
		}
		sccs_sdelta(r, sccs_ino(r), key);
		if (l = sccs_keyinit(key, INIT, opts->local_proj, opts->idDB)) {
			/*
			 * This should not happen, the repository is locked.
			 */
			if (IS_EDITED(l)) {
				fprintf(stderr,
				    "Will not overwrite edited %s\n", l->gfile);
				exit(1);
			}
			sccs_close(l);
			unlink(l->sfile);
			if (opts->log) {
				fprintf(stdlog, "unlink(%s)\n", l->sfile);
			}
			sccs_free(l);
		}
		sccs_free(r);
	}
	pclose(p);

	/*
	 * Pass 4c - apply the files.
	 */
	sprintf(key, "bk sfiles %s", ROOT2RESYNC);
	unless (p = popen(key, "r")) {
		perror("popen of bk sfiles");
		exit (1);
	}
	unless (get = popen("get -s -", "w")) {
		perror("popen of get -");
		exit (1);
	}
	while (fnext(buf, p)) {
		chop(buf);
		/*
		 * We want to get the part of the path without the RESYNC.
		 */
		if (opts->log) {
			fprintf(stdlog, "rename(%s, %s)\n", buf, &buf[offset]);
		}
		if (rename(buf, &buf[offset])) {
			mkdirf(&buf[offset]);
			if (rename(buf, &buf[offset])) {
				perror("rename");
				fprintf(stderr, "rename(%s, %s) failed\n",
				    buf, &buf[offset]);
			} else {
				opts->applied++;
				fprintf(get, "%s\n", &buf[offset]);
			}
		} else {
			opts->applied++;
			fprintf(get, "%s\n", &buf[offset]);
		}
	}
	pclose(p);
	pclose(get);
	unless (opts->quiet) {
		fprintf(stdlog,
		    "resolve: applied %d files in pass 4\n", opts->applied);
		fprintf(stdlog, "resolve: rebuilding caches...\n");
	}
	system("bk sfiles -r");
	unless (opts->quiet) {
		fprintf(stdlog, "resolve: running consistency check...\n");
	}
	unless (system("bk -r check -a") == 0) {
		fprintf(stderr, "Failed.  Resolve not completed.\n");
		exit(1);
	}
	unless (opts->quiet) {
		fprintf(stdlog,
		    "resolve: consistency check passed, resolve complete.\n");
	}
	resolve_cleanup(opts, CLEAN_OK|CLEAN_RESYNC|CLEAN_PENDING);
	/* NOTREACHED */
	return (0);
}

void
freeStuff(opts *opts)
{
	/*
	 * Clean up allocations/files/dbms
	 */
	if (opts->local_proj) sccs_freeProject(opts->local_proj);
	if (opts->resync_proj) sccs_freeProject(opts->resync_proj);
	if (opts->log && (opts->log != stderr)) fclose(opts->log);
	if (opts->rootDB) mdbm_close(opts->rootDB);
	if (opts->idDB) mdbm_close(opts->idDB);
	purify_list();
}

void
resolve_cleanup(opts *opts, int what)
{
	char	buf[MAXPATH];
	char	pendingFile[MAXPATH];
	FILE	*f;

	unless (exists(ROOT2RESYNC)) chdir(RESYNC2ROOT);
	unless (exists(ROOT2RESYNC)) {
		fprintf(stderr, "cleanup: can't find RESYNC dir\n");
		fprintf(stderr, "cleanup: nothing removed.\n");
		exit(1);
	}

	/*
	 * Get the patch file name from RESYNC before deleting RESYNC.
	 */
	sprintf(buf, "%s/%s", ROOT2RESYNC, "BitKeeper/tmp/patch");
	unless (f = fopen(buf, "rb")) {
		fprintf(stderr, "Warning: no BitKeeper/tmp/patch\n");
		pendingFile[0] = 0;
	} else {
		fnext(pendingFile, f);
		chop(pendingFile);
	}
	fclose(f);

	/*
	 * If we are done, save the csets file.
	 */
	sprintf(buf, "%s/%s", ROOT2RESYNC, "BitKeeper/etc/csets");
	if ((what & CLEAN_OK) && exists(buf)) {
		rename(buf, "BitKeeper/etc/csets");
		if (opts->log) {
			fprintf(stdlog,
			    "rename(%s, BitKeeper/etc/csets)\n", buf);
		}
	}

	if (what & CLEAN_RESYNC) {
		char cmd[1024];
		assert(exists("RESYNC"));
		sprintf(cmd, "%s -rf RESYNC", RM);
		system(cmd);
	} else {
		fprintf(stderr, "resolve: RESYNC directory left intact.\n");
	}
	
	if (what & CLEAN_PENDING) {
		if (pendingFile[0]) {
			unlink(pendingFile);
		}
		rmdir(ROOT2PENDING);
	}

	freeStuff(opts);
	unless (what & CLEAN_OK) {
		SHOUT2();
		exit(1);
	}
	exit(0);
}
