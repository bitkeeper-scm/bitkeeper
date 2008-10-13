#include "sccs.h"
#include "ensemble.h"

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
	while ((c = getopt(ac, av, "A;dE;lqr")) != -1) {
		switch (c) {
		    case 'A':
		    case 'r':
		    	break;
		    default:
			if (optarg) {
				cav = addLine(cav, aprintf("-%c%s", c, optarg));
			} else {
				cav = addLine(cav, aprintf("-%c",c));
			}
		}
		switch(c) {
		    case 'A':
			aliases = addLine(aliases, strdup(optarg));
			break;
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
			fprintf(stderr, "populate: -r or -A but not both.\n");
			exit(1);
		}
		aliases = file2Lines(0, "BitKeeper/log/ALIASES");
	} else unless (aliases) {
		aliases = addLine(0, strdup("default"));
	}
	s = sccs_csetInit(SILENT);
	if (aliases) {
		unless (opts.aliases = alias_list(aliases, s)) return (1);
	}
	opts.rev = "+";
	opts.sc = s;
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
			 * populate adds the aliases we have requested to
			 * the ALIASES file, but if there isn't a aliases
			 * file already then don't create one as that already
			 * implies: fetch everything.
			 */
			if (p = file2Lines(0, "BitKeeper/log/ALIASES")) {
				EACH (aliases) {
					p = addLine(p, strdup(aliases[i]));
				}
				uniqLines(p, free);
				if (lines2File(p, "BitKeeper/log/ALIASES")) {
					perror("BitKeeper/log/ALIASES");
				}
				freeLines(p, free);
			}
		}
	} else {
		unless (quiet) {
			fprintf(stderr, "populate: no components fetched\n");
		}
	}
	return (rc);
}
