#include "sccs.h"
#include "ensemble.h"

private hash*	hash_setDifference(hash *A, hash *B);

int
populate_main(int ac, char **av)
{
	int	c, i;
	int	quiet = 0;
	int	repair = 0;
	char	**urls = 0;
	char	*url;
	char	**aliases = 0;
	char	**vp = 0;
	char	**cav = 0;
	char	*checkfiles;
	FILE	*f;
	int	status, rc;
	sccs	*s;
	eopts	opts = {0};
	repos	*repos;
	hash	*done;

	unless (start_cwd) start_cwd = strdup(proj_cwd());
	while ((c = getopt(ac, av, "dE;lqrs;")) != -1) {
		unless ((c == 'r') || (c == 's')) {
			if (optarg) {
				cav = addLine(cav, aprintf("-%c%s", c, optarg));
			} else {
				cav = addLine(cav, aprintf("-%c",c));
			}
		}
		switch(c) {
		    case 'd': break;
		    case 'E':
			/* we just error check and pass through to clone */
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "populate: vars must start with BKU_\n");
				return (1);
			}
			break;
		    case 'l': break;
		    case 'q': quiet = 1; break;
		    case 'r': repair = 1; break;
		    case 's':
			aliases = addLine(aliases, strdup(optarg));
			break;
		    default:
usage:			sys("bk", "help", "-s", "populate", SYS);
			return (1);
		}
		optarg = 0;
	}
	unless (proj_isEnsemble(0)) {
		fprintf(stderr, "populate: must be in an ensemble.\n");
		return (1);
	}
	proj_cd2product();
	for (i = 0; av[optind + i]; i++) {
		urls = addLine(urls, strdup(av[optind +i]));
	}
	unless (urls) {
		unless (urls = parent_pullp()) {
			fprintf(stderr,
			    "populate: neither parent nor url provided.\n");
			goto usage;
		}
	}

	if (repair) {
		if (aliases) {
			fprintf(stderr, "populate: -r or -s but not both.\n");
			exit(1);
		}
		aliases = file2Lines(0, "BitKeeper/log/COMPONENTS");
	} else unless (aliases) {
		aliases = addLine(0, strdup("default"));
	}
	s = sccs_csetInit(SILENT);
	if (aliases) {
		unless (opts.aliases = alias_list(aliases, s)) return (1);
	}
	opts.rev = "+";
	opts.sc = s;
	opts.deeplast = 1;
	repos = ensemble_list(opts);
	putenv("_BK_TRANSACTION=1");
	done = hash_new(HASH_MEMHASH);
	EACH (urls) {
		url = urls[i];
		EACH_REPO (repos) {
			if (repos->present) {
				unless (quiet) {
					fprintf(stderr,
					    "populate: %s is already here.\n",
					    repos->path);
				    	}
				continue;
			}
			if (hash_fetchStr(done, repos->path)) continue;
			vp = addLine(0, strdup("bk"));
			vp = addLine(vp, strdup("clone"));
			EACH(cav) vp = addLine(vp, strdup(cav[i]));
			vp = addLine(vp, aprintf("-r%s", repos->deltakey));
			vp = addLine(vp, aprintf("%s?ROOTKEY=%s",
				url, repos->rootkey));
			vp = addLine(vp, strdup(repos->path));
			vp = addLine(vp, 0);
			status = spawnvp(_P_WAIT, "bk", vp + 1);
			freeLines(vp, free);
			if (WIFEXITED(status) ? WEXITSTATUS(status) : 1) {
				/* failed */
				rmtree(repos->path);
			} else {
				hash_storeStr(done, repos->path, 0);
			}
		}
	}
	freeLines(cav, free);
	rc = 0;
	EACH_REPO (repos) {
		if (repos->present) continue;
		if (hash_fetchStr(done, repos->path)) continue;
		fprintf(stderr, "populate: failed to fetch %s\n", repos->path);
		rc = 1;
	}
	ensemble_free(repos);
	sccs_free(s);

	if (hash_first(done)) {
		/* do consistancy check at end */
		checkfiles = bktmp(0, "clonechk");
		f = fopen(checkfiles, "w");
		assert(f);
		EACH_HASH (done) {
			fprintf(f, "%s/ChangeSet\n", done->kptr);
		}
		fclose(f);
		hash_free(done);
		i = run_check(quiet, checkfiles, quiet ? "-f" : "-fv", 0);
		rc += i;
		unlink(checkfiles);
		free(checkfiles);
		if (i) {
			fprintf(stderr, "Consistency check failed, "
			    "repository left locked.\n");
		} else if (aliases) {
			char	**p;

			/*
			 * Add aliases fetched to the aliases
			 * file. This is needed for unpopulate to
			 * work.
			 */
			p = file2Lines(0, "BitKeeper/log/COMPONENTS");
			EACH (aliases) {
				p = addLine(p, strdup(aliases[i]));
			}
			uniqLines(p, free);
			if (lines2File(p, "BitKeeper/log/COMPONENTS")) {
				perror("BitKeeper/log/COMPONENTS");
			}
			freeLines(p, free);
		}
	} else {
		unless (quiet) {
			fprintf(stderr, "populate: no components fetched\n");
		}
	}
	return (rc);
}

int
unpopulate_main(int ac, char **av)
{
	int	c, i;
	char	*buf, *cmd;
	char	**list = 0, **new_alias = 0;
	hash	*h;
	hash	*alias_unwanted = 0, *alias_wanted = 0, *alias_keep = 0;
	hash	*comps_wanted = 0, *comps_unwanted = 0, *comps_delete = 0;
	FILE	*f;
	eopts	op = {0};
	repos	*comps = 0;
	sccs	*s = 0;
	int	quiet = 0, force = 0, rc = 1;

	alias_unwanted = hash_new(HASH_MEMHASH);
	while ((c = getopt(ac, av, "A;fq")) != -1) {
		switch (c) {
		    case 'A':
			hash_insertStr(alias_unwanted, optarg, "");
			break;
		    case 'f': force = 1; break;
		    case 'q': quiet = 1; break;
		    default:
			sys("bk", "help", "-s", "unpopulate", SYS);
			goto out;
		}
	}
	if (proj_cd2product()) {
		fprintf(stderr, "%s: must be called in a product.\n", av[0]);
		goto out;
	}

	unless (hash_first(alias_unwanted)) {
		hash_insertStr(alias_unwanted, "default", "");
	}

	alias_wanted = hash_new(HASH_MEMHASH);
	if (f = fopen("BitKeeper/log/ALIASES", "r")) {
		while (buf=fgetline(f)) hash_insertStr(alias_wanted, buf, "");
		fclose(f);
	} else {
		hash_insertStr(alias_wanted, "default", "");
	}

	/*
	 * This next 'if' says that you cannot unpopulate aliases that
	 * are not in the ALIASES file. It is an arbitrary restriction that
	 * I think is unnecessary. lm3di :)
	 */
	h = hash_setDifference(alias_unwanted, alias_wanted);
	if (hash_first(h)) {
		fprintf(stderr, "%s: cannot remove ", av[0]);
		i = 0;
		EACH_HASH(h) {
			fprintf(stderr, "'%s' ", h->kptr);
			i++;
		}
		fprintf(stderr, "because %s not populated.\n",
		    (i > 1)? "they are" : "it is");
		goto out;
	}

	unless (s = sccs_csetInit(SILENT)) goto out;

	alias_keep = hash_setDifference(alias_wanted, alias_unwanted);
	if (hash_first(alias_keep)) {
		/* turn to keys */
		new_alias = 0;
		EACH_HASH(alias_keep) {
			new_alias=addLine(new_alias, (char*)alias_keep->kptr);
		}
		comps_wanted = alias_list(new_alias, s);
		unless (comps_wanted) goto out;
	} else {
		/* we want nothing */
		comps_wanted = hash_new(HASH_MEMHASH);
	}

	list = 0;
	EACH_HASH(alias_unwanted) {
		list = addLine(list, (char*)alias_unwanted->kptr);
	}
	comps_unwanted = alias_list(list, s);
	freeLines(list, 0);
	list = 0;
	unless (comps_unwanted) goto out;

	comps_delete = hash_setDifference(comps_unwanted, comps_wanted);

	op.sc = s;
	op.rev = "+";
	op.deepfirst = 1;
	op.aliases = comps_delete;

	comps = ensemble_list(op);
	putenv("_BK_TRANSACTION=1");
	/*
	 * Two passes, the first one to see if it would work, the second
	 * one to actually remove them.
	 */
	unless (force) {
		EACH_REPO(comps) {
			unless (comps->present) continue;
			if (chdir(comps->path)) {
				perror(comps->path);
				continue; /* maybe error? */
			}
			buf = bktmp(0, av[0]);
			cmd = aprintf("bk superset > '%s'", buf);
			if (system(cmd)) {
				fprintf(stderr, "%s: component '%s' "
				    "has local changes, not removing.\n",
				    av[0], comps->path);
				cat(buf);
				unlink(buf);
				free(buf);
				free(cmd);
				goto out;
			}
			unlink(buf);
			free(buf);
			free(cmd);
			proj_cd2product();
		}
	}
	i = 0;
	EACH_REPO(comps) {
		unless (comps->present) {
			unless (quiet) {
				fprintf(stderr, "%s: %s not present.\n",
				    av[0], comps->path);
			}
			continue;
		}
		unless (quiet) printf("Removing '%s'.\n", comps->path);
		i++;
		if (rmtree(comps->path)) goto out;
	}
	unless (i) {
		unless (quiet) printf("%s: no components removed.\n", av[0]);
	}
	/* update ALIAS file */
	sortLines(new_alias, 0);
	if (lines2File(new_alias, "BitKeeper/log/ALIASES")) {
		perror("BitKeeper/log/ALIASES");
	}
	putenv("_BK_TRANSACTION=");
	rc = 0;
out:	if (alias_unwanted) hash_free(alias_unwanted);
	if (alias_wanted) hash_free(alias_wanted);
	if (h) hash_free(h);
	if (s) sccs_free(s);
	if (alias_keep) hash_free(alias_keep);
	if (new_alias) freeLines(new_alias, 0);
	if (comps_wanted) hash_free(comps_wanted);
	if (comps_unwanted) hash_free(comps_unwanted);
	if (comps_delete) hash_free(comps_delete);
	if (comps) ensemble_free(comps);
	return (rc);
}


private hash*
hash_setDifference(hash *A, hash *B)
{
	hash	*ret;

	unless (A) return (0);
	ret = hash_new(HASH_MEMHASH);
	EACH_HASH(A) {
		unless (hash_fetchStr(B, A->kptr)) {
			hash_insertStr(ret, A->kptr, A->vptr);
		}
	}
	return (ret);
}
