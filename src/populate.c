#include "sccs.h"
#include "nested.h"

extern	char	*prog;

int
populate_main(int ac, char **av)
{
	int	c, i, j, clonerc;
	int	quiet = 0;
	int	repair = 0;
	char	**urls = 0;
	char	*url;
	char	**aliases = 0;
	char	**vp = 0;
	char	**cav = 0;
	char	*checkfiles;
	comp	*cp;
	FILE	*f;
	int	status, rc;
	nested	*n;
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
	n = nested_init(0, 0, 0, 0);
	if (aliasdb_chkAliases(n, 0, &aliases, proj_cwd())) return (1);
	nested_filterAlias(n, 0, aliases);
	START_TRANSACTION();
	done = hash_new(HASH_MEMHASH);
	EACH (urls) {
		url = urls[i];
		EACH_STRUCT(n->comps, cp) {
			if (cp->product) continue;
			if (cp->present) {
				unless (quiet) {
					fprintf(stderr,
					    "populate: %s is already here.\n",
					    cp->path);
				    	}
				continue;
			}
			unless (cp->alias) continue;
			if (hash_fetchStr(done, cp->path)) continue;
			vp = addLine(0, strdup("bk"));
			vp = addLine(vp, strdup("clone"));
			EACH_INDEX(cav, j) vp = addLine(vp, strdup(cav[j]));
			vp = addLine(vp, aprintf("-r%s", cp->deltakey));
			vp = addLine(vp, aprintf("%s?ROOTKEY=%s",
				url, cp->rootkey));
			vp = addLine(vp, strdup(cp->path));
			vp = addLine(vp, 0);
			status = spawnvp(_P_WAIT, "bk", vp + 1);
			freeLines(vp, free);
			clonerc = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
			if (clonerc == 0) {
				hash_storeStr(done, cp->path, 0);
			} else if (clonerc == 2) {
				/* failed because the dir was not empty */
			} else {
				/* failed and left crud */
				nested_rmtree(cp->path);
			}
		}
	}
	freeLines(cav, free);
	rc = 0;
	EACH_STRUCT(n->comps, cp) {
		if (cp->present || !cp->alias) continue;
		if (hash_fetchStr(done, cp->path)) continue;
		fprintf(stderr, "populate: failed to fetch %s\n", cp->path);
		rc = 1;
	}
	nested_free(n);

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
	return (0);
}
