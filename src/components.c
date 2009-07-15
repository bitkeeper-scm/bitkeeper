#include "sccs.h"
#include "nested.h"
#include "bkd.h"

private	int	unpopulate_check(comp *c, char **urls);


int
components_main(int ac, char **av)
{
	int	c, i, j, k, clonerc;
	int	keys = 0, add = 0, rm = 0, here = 0, quiet = 0, force = 0;
	int	citool = 0;
	char	**urls = 0;
	char	**aliases = 0;
	char	**vp = 0;
	char	**cav = 0;
	char	*checkfiles;
	char	**list;
	remote	*r;
	comp	*cp;
	FILE	*f;
	int	status, rc;
	nested	*n;
	char	*subcmd = 0;
	project	*prod;
	char	*first = 0;

	unless (start_cwd) start_cwd = strdup(proj_cwd());

	if (av[1]) {
		subcmd = av[1];
		++av;
		--ac;
	} else {
		subcmd = "here";
	}
	while ((c = getopt(ac, av, "@|cE;fklq")) != -1) {
		switch(c) {
		    case '@':
			list = 0;
			if (optarg && (optarg[0] == '@')) {
				unless (list = file2Lines(0, optarg+1)) {
					perror(optarg+1);
					return (1);
				}
			} else if (optarg) {
				urls = addLine(urls, strdup(optarg));
			} else {
				unless (list = parent_allp()) {
					fprintf(stderr, "%s: -@@ failed as "
					    "repository has no parent\n",
					    prog);
					return (1);
				}
			}
			EACH(list) urls = addLine(urls, list[i]);
			freeLines(list, 0);
			break;
		    case 'c': citool = 1; break;
		    case 'E':
			/* we just error check and pass through to clone */
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "%s: vars must start with BKU_\n", prog);
				return (1);
			}
			cav = addLine(cav, aprintf("-E%s", optarg));
			break;
		    case 'f': force = 1; break;
		    case 'k': keys = 1; break;
		    case 'l': cav = addLine(cav, strdup("-l")); break;
		    case 'q':
			quiet = 1;
			cav = addLine(cav, strdup("-q"));
			break;
		    default:
usage:			sys("bk", "help", "-s", prog, SYS);
			freeLines(aliases, free);
			if (first) free(first);
			return (1);
		}
	}
	unless (prod = proj_product(0)) {
		fprintf(stderr, "%s: must be in an nested collection.\n", prog);
		return (1);
	}
	if (citool && proj_isComponent(0)) {
		first = strdup(proj_comppath(0));
	}
	(void)proj_cd2product();
	aliases = nested_here(0);
	if ((here = streq(subcmd, "here")) || streq(subcmd, "missing")) {
		if (av[optind]) goto usage;
		assert(!citool || (here && !keys));
		if (first) printf("%s\n", first);
		if (citool) printf(".\n");
		n = nested_init(0, 0, 0, NESTED_PENDING);
		nested_aliases(n, n->tip, &aliases, 0, n->pending);
		EACH_STRUCT(n->comps, cp, i) {
			if (cp->product) continue;
			if (cp->present && !cp->alias) {
			    	fprintf(stderr,
				    "WARN: %s: %s is present but not in "
				    "in HERE alias\n", prog, cp->path);
			} else if (!cp->present && cp->alias) {
			    	fprintf(stderr,
				    "WARN: %s: %s is not present but is in "
				    "in HERE alias\n", prog, cp->path);
			}
			if (here && !cp->present) continue;
			if (!here && cp->present) continue;
			if (first && streq(first, cp->path)) continue;
			if (keys) {
				printf("%s\n", cp->rootkey);
			} else {
				printf("%s\n", cp->path);
			}
		}
		nested_free(n);
		freeLines(aliases, free);
		if (first) free(first);
		return (0);
	} else if (streq(subcmd, "add")) {
		add = 1;
	} else if (streq(subcmd, "rm")) {
		rm = 1;
	} else if (streq(subcmd, "set")) {
		freeLines(aliases, free);
		aliases = 0;
		add = 1;
	} else {
		goto usage;
	}
	if (first) free(first);
	if (keys) goto usage;
	unless (av[optind]) goto usage;
	assert((add && !rm) || (rm && !add));
	n = nested_init(0, 0, 0, NESTED_PENDING);
	assert(n);
	for ( ; av[optind]; optind++) {
		list = addLine(0, strdup(av[optind]));
		if (aliasdb_chkAliases(n, 0, &list, start_cwd)) return (1);
		EACH(list) {
			if (add) {
				/* no duplicates */
				removeLine(aliases, list[i], free);
				aliases = addLine(aliases, strdup(list[i]));
			} else {
				unless (removeLine(aliases, list[i], free)) {
					fprintf(stderr, "%s: can't remove "
					    "'%s' as it is not currently "
					    "populated.\n",
					    prog, av[optind]);
					return (1);
				}
			}
		}
		freeLines(list, free);
	}
	nested_aliases(n, n->tip, &aliases, 0, n->pending);
	n->product->alias = 1;

	/*
	 * Look to see if removed any components will cause a problem
	 * and fail early if we see any.
	 */
	i = 0;
	EACH_STRUCT(n->comps, cp, j) {
		if (force) break; /* skip these checks */
		if (cp->present && !cp->alias) {
			unless (urls || (urls = parent_allp())) {
				fprintf(stderr,
				    "%s: neither parent nor url provided.\n",
				    prog);
				goto usage;
			}
			if (unpopulate_check(cp, urls)) {
				fprintf(stderr, "%s: unable to remove %s\n",
				    prog, cp->path);
				++i;
			}
		}
	}
	if (i) return (1);
	checkfiles = bktmp(0, 0);
	f = fopen(checkfiles, "w");
	START_TRANSACTION();
	/*
	 * Now add all the new repos and keep a list of repos added
	 * so we can cleanup if needed.
	 */
	list = 0;		/* repos added */
	EACH_STRUCT(n->comps, cp, j) {
		if (!cp->present && cp->alias) {
			unless (urls || (urls = parent_allp())) {
				fprintf(stderr,
				    "%s: neither parent nor url provided.\n",
				    prog);
				goto usage;
			}
			EACH(urls) {
				unless (r = remote_parse(urls[i], 0)) continue;
				unless (r->params) {
					r->params = hash_new(HASH_MEMHASH);
				}
				hash_storeStr(r->params,
				    "ROOTKEY", cp->rootkey);

				vp = addLine(0, strdup("bk"));
				vp = addLine(vp, strdup("clone"));
				EACH_INDEX(cav, k) {
					vp = addLine(vp, strdup(cav[k]));
				}
				vp = addLine(vp, strdup("-p"));
				vp = addLine(vp, aprintf("-r%s", cp->deltakey));
				vp = addLine(vp, remote_unparse(r));
				vp = addLine(vp, strdup(cp->path));
				vp = addLine(vp, 0);
				status = spawnvp(_P_WAIT, "bk", vp + 1);
				freeLines(vp, free);
				clonerc =
				    WIFEXITED(status) ? WEXITSTATUS(status) : 1;
				if (clonerc == 0) {
					list = addLine(list, cp);
					cp->present = 1;
					fprintf(f, "%s/ChangeSet\n", cp->path);
					break;
				} else if (clonerc == 2) {
					/*
					 * failed: the dir was not empty
					 * no use trying other urls
					 */
					fprintf(stderr, "%s: %s not empty\n",
					    prog, cp->path);
					break;
				} else if (exists(cp->path)) {
					/* failed and left crud */
					nested_rmcomp(n, cp);
				}
			}
			unless (cp->present) break;
		}
	}
	/*
	 * If we failed to add all repos then cleanup and exit.
	 */
	rc = 0;
	EACH_STRUCT(n->comps, cp, i) {
		if (!cp->present && cp->alias) {
			fprintf(stderr, "%s: failed to fetch %s\n",
			    prog, cp->path);
			rc = 1;
		}
	}
	if (rc) {
		EACH_STRUCT(list, cp, i) {
			if (cp->present) nested_rmcomp(n, cp);
		}
		return (rc);
	}
	freeLines(list, 0);

	/*
	 * Now after we sucessfully cloned all new components we can
	 * remove the ones to be deleted.
	 */
	reverseLines(n->comps);	/* deeply nested first */
	EACH_STRUCT(n->comps, cp, j) {
		if (cp->present && !cp->alias) {
			if (nested_rmcomp(n, cp)) {
				fprintf(stderr, "%s: remove of %s failed\n",
				    prog, cp->path);
				return (1);
			}
			cp->present = 0;
		}
	}
	STOP_TRANSACTION();
	freeLines(cav, free);
	rc = 0;
	EACH_STRUCT(n->comps, cp, i) {
		if (!cp->present && cp->alias) {
			fprintf(stderr, "%s: failed to fetch %s\n",
			    prog, cp->path);
			rc = 1;
		}
		if (cp->present && !cp->alias) {
			fprintf(stderr, "%s: failed to unpopulate %s\n",
			    prog, cp->path);
			rc = 1;
		}
	}
	nested_free(n);

	/* do consistancy check at end */
	unless (rc) {
		lines2File(aliases, "BitKeeper/log/HERE");
		/* old repos break less */
		lines2File(aliases, "BitKeeper/log/COMPONENTS");
	}
	fclose(f);
	rc |= run_check(quiet, checkfiles, quiet ? 0 : "-v", 0);
	unlink(checkfiles);
	free(checkfiles);
	freeLines(aliases, free);
	return (rc);
}

private int
unpopulate_check(comp *c, char **urls)
{
	FILE	*f;
	char	*t;
	int	i, good, errs;
	char	**av;
	char	out[MAXPATH];

	if (chdir(c->path)) {
		perror(c->path);
		return (1);
	}
	f = popen("bk sfiles -gcxp -v", "r");
	errs = 0;
	while (t = fgetline(f)) {
		if (t[0] == 'x') {
			fprintf(stderr, "Extra file:         ");
		} else if (t[2] == 'c') {
			fprintf(stderr, "Modified file:      ");
		} else if (t[3] == 'p') {
			fprintf(stderr, "Non-committed file: ");
		} else {
			continue;
		}
		fprintf(stderr, "%s/%s\n", c->path, t + 8);
		++errs;
	}
	pclose(f);
	if (errs) goto out;
	good = 0;
	bktmp(out, 0);
	av = addLine(0, "bk");
	av = addLine(av, "changes");
	av = addLine(av, "-LD");
	EACH(urls) {
		unless (sysio(0, out, DEVNULL_WR,
			"bk", "changes", "-qLnd:REV:", urls[i], SYS)) {
			if (size(out) == 0) {
				/* found no diffs? */
				good = 1;
				break;
			} else {
				/* found diffs */
				av = addLine(av, urls[i]);
			}
		}
	}
	unlink(out);
	unless (good) {
		++errs;
		if (nLines(av) > 3) {
			fprintf(stderr, "Local changes to %s found:\n",
			    c->path);
			av = addLine(av, 0);
			/* send changes output to stderr */
			i = dup(1);
			dup2(2, 1);
			spawnvp(_P_WAIT, "bk", av+1);
			dup2(i, 1);
			close(i);
		} else {
			fprintf(stderr,
			    "No parent with %s to look for local changes\n",
			    c->path);
		}
	}
	freeLines(av, 0);
out:	proj_cd2product();
	if (errs) return (1);
	return (0);
}
