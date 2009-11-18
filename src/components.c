#include "sccs.h"
#include "nested.h"
#include "bkd.h"

private	int	unpopulate_check(comp *c, char **urls);


int
components_main(int ac, char **av)
{
	int	c, i, j, k;
	clonerc	clonerc;
	int	keys = 0, add = 0, rm = 0, here = 0, quiet = 0, force = 0;
	int	set = 0;
	int	citool = 0, trim_noconnect = 0;
	int	from_clone = 0;
	char	**urls = 0;
	char	**aliases = 0;
	char	**vp = 0;
	char	**cav = 0;
	char	*checkfiles;
	char	**list;
	remote	*r;
	comp	*cp;
	FILE	*f;
	hash	*urllist;
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
	while ((c = getopt(ac, av, "@|CcE;fklq")) != -1) {
		switch(c) {
		    case '@':
			if (optarg && (optarg[0] == '@')) {
				unless (urls = file2Lines(urls, optarg+1)) {
					perror(optarg+1);
					return (1);
				}
			} else if (optarg) {
				urls = addLine(urls, strdup(optarg));
			} else {
				unless (list = parent_allp()) {
					fprintf(stderr, "%s: -@ failed as "
					    "repository has no parent\n",
					    prog);
					return (1);
				}
				EACH(list) urls = addLine(urls, list[i]);
				freeLines(list, 0);
			}
			break;
		    case 'C': from_clone = 1; break;
		    case 'c':
			if (streq(subcmd, "check")) {
				trim_noconnect = 1;
			} else {
				citool = 1;
			}
			break;
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
		fprintf(stderr, "%s: must be in a nested collection.\n", prog);
		return (1);
	}
	EACH(urls) {
		char	*u = urls[i];

		urls[i] = parent_normalize(u);
		free(u);
	}
	if (citool && proj_isComponent(0)) {
		first = strdup(proj_comppath(0));
	}
	(void)proj_cd2product();
	if ((here = streq(subcmd, "here")) || streq(subcmd, "missing")) {
		if (av[optind]) goto usage;
		assert(!citool || (here && !keys));
		if (first) printf("%s\n", first);
		if (citool) printf(".\n");
		n = nested_init(0, 0, 0, NESTED_PENDING);
		assert(n);
		aliases = nested_here(0);
		nested_aliases(n, n->tip, &aliases, 0, n->pending);
		EACH_STRUCT(n->comps, cp, i) {
			if (cp->product) continue;
			if (cp->present && !cp->alias) {
			    	fprintf(stderr,
				    "WARN: %s: %s is present but not in "
				    "HERE alias\n", prog, cp->path);
			} else if (!cp->present && cp->alias) {
			    	fprintf(stderr,
				    "WARN: %s: %s is not present but is in "
				    "HERE alias\n", prog, cp->path);
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
	} else if (streq(subcmd, "check")) {
		freeLines(cav, free);
		if (av[optind] || from_clone || force || keys) goto usage;
		n = nested_init(0, 0, 0, NESTED_PENDING);
		assert(n);
		rc = urllist_check(n, quiet, trim_noconnect, urls);
		nested_free(n);
		return (rc);
	} else if (streq(subcmd, "add")) {
		add = 1;
	} else if (streq(subcmd, "rm")) {
		rm = 1;
	} else if (streq(subcmd, "set")) {
		add = 1;
		set = 1;
	} else if (streq(subcmd, "where")) {
		unless (av[optind]) {
			urllist_dump(0);
		} else if (streq(av[optind], "rm")) {
			/* Perhaps _rm? or change the verb? */
			unlink(NESTED_URLLIST);
		} else {
			for ( ; av[optind]; optind++) {
				urllist_dump(av[optind]);
			}
		}
		return (0);
	} else {
		goto usage;
	}
	if (first) free(first);
	if (keys) goto usage;
	unless (av[optind]) goto usage;
	assert((add && !rm) || (rm && !add));
	n = nested_init(0, 0, 0, NESTED_PENDING);
	assert(n);
	unless (set) aliases = nested_here(0);
	urllist = hash_fromFile(hash_new(HASH_MEMHASH), NESTED_URLLIST);
	assert(urllist);
	if (from_clone) {
		char	*parent;

		prog = "clone";

		/*
		 * Special knowledge: at this time in a clone the parent
		 * will only have one entry in it -- the clone source.
		 *
		 * XXX: parent.c defines PARENT -- could move to a .h
		 * Could possibly make use of one of the interfaces in 
		 * parent.c which returns an addLines? I didn't dig there.
		 */
		parent = loadfile("BitKeeper/log/parent", 0);
		chomp(parent);

		/* expand pathnames in the urllist with the parent URL */
		urllist_normalize(urllist, parent);

		/*
		 * find out which components are present remotely and
		 * update URLLIST
		 */
		assert(!aliases);
		aliases = file2Lines(0, "BitKeeper/log/RMT_HERE");
		unlink("BitKeeper/log/RMT_HERE");
		EACH(aliases) {
			if (isKey(aliases[i]) &&
			    !nested_findKey(n, aliases[i])) {
				/* we can ignore extra rootkeys */
				removeLineN(aliases, i, free);
				i--;
			}
		}

		// XXX wrong, for clone -r, the meaning of HERE may have changed
		if (nested_aliases(n, n->tip, &aliases, 0, n->pending)) {
			/* It is OK if this fails, just tag everything */
			EACH_STRUCT(n->comps, cp, i) cp->alias = 1;
		}
		EACH_STRUCT(n->comps, cp, i) {
			if (cp->product || !cp->alias) continue;

			urllist_addURL(urllist, cp->rootkey, parent);
		}
		free(parent);

		/* just in case we exit early */
		if (hash_toFile(urllist, NESTED_URLLIST)) {
			perror(NESTED_URLLIST);
		}

		/* keep RMT_HERE for -sHERE */
		for (i = optind; av[i]; i++) {
			if (strieq(av[i], "here") || strieq(av[i], "there")) {
				break;
			}
		}
		unless (av[i]) {
			freeLines(aliases, free);
			aliases = 0;
		}
	}
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
	if (nested_aliases(n, n->tip, &aliases, 0, n->pending)) {
		/* we tested them above */
		assert("bad alias list" == 0);
	}

	/*
	 * Look to see if removed any components will cause a problem
	 * and fail early if we see any.
	 */
	rc = 0;
	EACH_STRUCT(n->comps, cp, j) {
		if (force) break; /* skip these checks */
		if (cp->present && !cp->alias) {
			char	**lurls = 0;

			/* try urls from cmd line first */
			EACH(urls) lurls = addLine(lurls, strdup(urls[i]));

			/* then ones remembered for this component */
			lurls = urllist_fetchURLs(urllist, cp->rootkey, lurls);

			if ((i = unpopulate_check(cp, lurls)) <= 0) {
				fprintf(stderr, "%s: unable to remove %s\n",
				    prog, cp->path);
				++rc;
			} else {
				urllist_addURL(urllist, cp->rootkey, lurls[i]);
			}
			freeLines(lurls, free);
		}
	}
	if (rc) return (1);
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
			char	**lurls = 0;
			char	**reasons = 0;

			/* try urls from cmd line first */
			EACH(urls) lurls = addLine(lurls, strdup(urls[i]));

			/* then ones remembered for this component */
			lurls = urllist_fetchURLs(urllist, cp->rootkey, lurls);
			EACH(lurls) {
				unless (r = remote_parse(lurls[i], 0)) continue;
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
					urllist_addURL(urllist,
					    cp->rootkey, lurls[i]);
					fprintf(f, "%s/ChangeSet\n", cp->path);
					break;
				} else if ((clonerc == CLONE_EXISTS) &&
				    exists(cp->path)) {
					/*
					 * failed: the dir was not empty
					 * no use trying other urls
					 */
					fprintf(stderr, "%s: %s not empty\n",
					    prog, cp->path);
					goto conflict;
				} else {
					char	*t;

					/* clone failed */
					if (exists(cp->path)) {
						/* failed and left crud */
						nested_rmcomp(n, cp);
					}
					switch(clonerc) {
					    case CLONE_CONNECT:
						t = "cannot connect";
						break;
					    case CLONE_CHDIR:
						t = "component not present";
						break;
					    case CLONE_BADREV:
						t = "component missing cset";
						break;
					    default:
						t = "unknown failure";
						break;
					}
					reasons = addLine(reasons, t);

					/*
					 * In some cases we know this
					 * URL is no good, so we can
					 * stop remembering it.
					 */
					if ((clonerc == CLONE_CHDIR) ||
					    (clonerc == CLONE_BADREV)) {
						urllist_rmURL(urllist,
						    cp->rootkey, lurls[i]);
					}
				}
			}
			unless (cp->present) {
				/* We tried all the urls and didn't break
				 * early
				 */
				fprintf(stderr,
				    "%s: failed to fetch component %s\n"
				    "URLs tried:\n",
				    prog, cp->path);
				assert(nLines(lurls) == nLines(reasons));
				EACH(lurls) {
					fprintf(stderr, "\t%s: %s\n",
					    lurls[i], reasons[i]);
				}
			}
conflict:
			freeLines(lurls, free);
			freeLines(reasons, 0);
			unless (cp->present) break;
		}
	}

	rc = 0;
	EACH_STRUCT(n->comps, cp, i) {
		/*
		 * If we failed to add all repos set exit status and remove
		 * other repos we cloned.
		 */
		if (!cp->present && cp->alias) {
			comp	*cj;
			int	j;

			EACH_STRUCT(list, cj, j) {
				if (cj->present) nested_rmcomp(n, cj);
			}
			rc = 1;
		}
	}
	freeLines(list, 0);

	unless (rc) {
		/*
		 * Now after we sucessfully cloned all new components
		 * we can remove the ones to be deleted.
		 */
		reverseLines(n->comps);	/* deeply nested first */
		EACH_STRUCT(n->comps, cp, j) {
			if (cp->present && !cp->alias) {
				if (nested_rmcomp(n, cp)) {
					fprintf(stderr,
					    "%s: remove of %s failed\n",
					    prog, cp->path);
					rc = 1;
					break;
				}
				cp->present = 0;
			}
		}
	}
	STOP_TRANSACTION();
	freeLines(cav, free);
	unless (rc) {
		EACH_STRUCT(n->comps, cp, i) {
			/* just check that the code above all worked */
			if (cp->present) {
				assert(cp->alias);
			} else {
				assert(!cp->alias);
			}
		}
	}
	nested_free(n);

	/* do consistency check at end */
	unless (rc) lines2File(aliases, "BitKeeper/log/HERE");
	if (hash_toFile(urllist, NESTED_URLLIST)) perror(NESTED_URLLIST);
	fclose(f);
	rc |= run_check(quiet, checkfiles, quiet ? 0 : "-v", 0);
	unlink(checkfiles);
	free(checkfiles);
	freeLines(aliases, free);
	hash_free(urllist);
	return (rc);
}

/*
 * Verify that it is OK to delete this component as it can be found in
 * one of these urls.  Also verify that the local copy doesn't have any local
 * work.
 *
 * Returns -1 one failure or index of url used as a backup if this
 * component can be deleted.
 */
private int
unpopulate_check(comp *c, char **urls)
{
	FILE	*f;
	char	*t;
	int	i, good = 0, errs;
	char	**av;
	char	out[MAXPATH];

	if (chdir(c->path)) {
		perror(c->path);
		return (-1);
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
	bktmp(out, 0);
	av = addLine(0, "bk");
	av = addLine(av, "changes");
	av = addLine(av, "-LD");
	EACH(urls) {
		unless (sysio(0, out, DEVNULL_WR,
			"bk", "changes", "-qLnd:REV:", urls[i], SYS)) {
			if (size(out) == 0) {
				/* found no diffs? */
				good = i;
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
			fprintf(stderr, "%s: No other sources for %s known\n",
			    prog, c->path);
		}
	}
	freeLines(av, 0);
out:	proj_cd2product();
	return (good);
}
