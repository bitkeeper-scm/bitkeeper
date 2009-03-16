#include "sccs.h"
#include "nested.h"
#include "bkd.h"

extern	char	*prog;

int
components_main(int ac, char **av)
{
	int	c, i, j, k, clonerc;
	int	quiet = 0;
	int	keys = 0, paths = 0, add = 0, rm = 0;
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

	unless (start_cwd) start_cwd = strdup(proj_cwd());

	if (av[1]) {
		subcmd = av[1];
		++av;
		--ac;
	} else {
		subcmd = "here";
		paths = 1;
	}
	while ((c = getopt(ac, av, "@|E;klpq")) != -1) {
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
		    case 'E':
			/* we just error check and pass through to clone */
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "%s: vars must start with BKU_\n", prog);
				return (1);
			}
			cav = addLine(cav, aprintf("-E%s", optarg));
			break;
		    case 'k': keys = 1; break;
		    case 'l': cav = addLine(cav, strdup("-l")); break;
		    case 'p': paths = 1; break;
		    case 'q':
			quiet = 1;
			cav = addLine(cav, strdup("-q"));
			break;
		    default:
usage:			sys("bk", "help", "-s", prog, SYS);
			return (1);
		}
	}
	if (proj_cd2product()) {
		fprintf(stderr, "%s: must be in an ensemble.\n", prog);
		return (1);
	}
	aliases = file2Lines(0, "BitKeeper/log/COMPONENTS");
	if (streq(subcmd, "here")) {
		if (paths) return (system("bk _nested -h")); /* XXX hack */
		printf("bk components here: NYI!\n");
		return (0);
	} else if (streq(subcmd, "missing")) {
		printf("bk components missing: NYI!\n");
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
	if (keys || paths) goto usage;
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
	nested_filterAlias(n, 0, aliases);
	checkfiles = bktmp(0, 0);
	f = fopen(checkfiles, "w");
	START_TRANSACTION();
	EACH_STRUCT_INDEX(n->comps, cp, j) {
		if (cp->product) continue;
		if (!cp->present && cp->alias) {
			unless (urls || (urls = parent_pullp())) {
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
				vp = addLine(vp, aprintf("-r%s", cp->deltakey));
				vp = addLine(vp, remote_unparse(r));
				vp = addLine(vp, strdup(cp->path));
				vp = addLine(vp, 0);
				status = spawnvp(_P_WAIT, "bk", vp + 1);
				freeLines(vp, free);
				clonerc =
				    WIFEXITED(status) ? WEXITSTATUS(status) : 1;
				if (clonerc == 0) {
					cp->present = 1;
					fprintf(f, "%s/ChangeSet\n", cp->path);
					break;
				} else if (clonerc == 2) {
					/* failed because the dir was not empty */
				} else {
					/* failed and left crud */
					nested_rmtree(cp->path);
				}
			}
		}
		if (cp->present && !cp->alias) {
			fprintf(stderr, "%s: need to unpopulate %s\n",
			    prog, cp->path);
		}
	}
	STOP_TRANSACTION();
	freeLines(cav, free);
	rc = 0;
	EACH_STRUCT(n->comps, cp) {
		if (cp->product) continue;
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
	unless (rc) lines2File(aliases, "BitKeeper/log/COMPONENTS");
	fclose(f);
	rc |= run_check(quiet, checkfiles, quiet ? 0 : "-v", 0);
	unlink(checkfiles);
	free(checkfiles);
	freeLines(aliases, free);
	return (rc);
}
