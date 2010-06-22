#include "sccs.h"
#include "nested.h"
#include "bkd.h"

private	int	unpopulate_check(comp *c, char **urls);

/*
 * called with cp->alias set to which components should be populated
 * this function added or removes component such that the cp->present
 * matches cp->alias
 */
int
nested_populate(nested *n, char **urls, int force, popts *ops)
{
	int	i, j, status, rc;
	int	done = 1;
	clonerc	clonerc;
	char	**vp = 0;
	char	*checkfiles = 0;
	char	**list;
	remote	*r;
	comp	*cp;
	FILE	*f = 0;
	hash	*urllist;

	urllist = hash_fromFile(hash_new(HASH_MEMHASH), NESTED_URLLIST);
	assert(urllist);
	/*
	 * Look to see if removed any components will cause a problem
	 * and fail early if we see any.
	 */
	rc = 0;
	EACH_STRUCT(n->comps, cp, j) {
		if (!force && cp->present && !cp->alias) {
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
		if (!cp->present && cp->alias) {
			/* see if the namespace is not taken */
			if (exists(cp->path) && !nested_emptyDir(n, cp->path)){
				fprintf(stderr, "%s: %s not empty\n",
				    prog, cp->path);
				++rc;
			}
			ops->comps++;
		}
	}
	if (rc) return (1);
	if (ops->runcheck) {
		checkfiles = bktmp(0, 0);
		f = fopen(checkfiles, "w");
	}
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
				unless (r = remote_parse(lurls[i], 0)) {
					reasons = addLine(reasons, "bad url");
					continue;
				}
				unless (r->params) {
					r->params = hash_new(HASH_MEMHASH);
				}
				hash_storeStr(r->params,
				    "ROOTKEY", cp->rootkey);

				vp = addLine(0, strdup("bk"));
				vp = addLine(vp, strdup("clone"));
				if (ops->debug) vp = addLine(vp, strdup("-d"));
				if (ops->quiet) vp = addLine(vp, strdup("-q"));
				if (ops->verbose) {
					vp = addLine(vp, strdup("-v"));
				}
				if (ops->no_lclone) {
					vp = addLine(vp,
					    strdup("--no-hardlink"));
				}
				vp = addLine(vp, strdup("-p"));
				vp = addLine(vp, aprintf("-r%s", cp->deltakey));
				vp = addLine(vp,
				    aprintf("-P%u/%u %s",
				    done, ops->comps, cp->path));
				vp = addLine(vp, remote_unparse(r));
				vp = addLine(vp, strdup(cp->path));
				vp = addLine(vp, 0);
				status = spawnvp(_P_WAIT, "bk", vp + 1);
				freeLines(vp, free);
				clonerc =
				    WIFEXITED(status) ? WEXITSTATUS(status) : 1;
				if (clonerc == 0) {
					done++;
					list = addLine(list, cp);
					cp->present = 1;
					urllist_addURL(urllist,
					    cp->rootkey, lurls[i]);
					if (ops->runcheck) {
						fprintf(f, "%s/ChangeSet\n",
						    cp->path);
					}
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
			/*
			 * If populating this component made another
			 * component deeply nested, then I need to update
			 * deep-nests
			 */
			EACH_START(j+1, n->comps, i) {
				comp	*t = (comp *)n->comps[i];
				FILE	*dn;

				unless (strneq(
				    cp->path, t->path, strlen(cp->path))) {
				    	break;
				}
				if (t->present && 
				    t->path[strlen(cp->path)] == '/') {
					dn = fopen("BitKeeper/log/deep-nests",
					    "a");
					fprintf(dn, "%s\n", t->path);
					fclose(dn);
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

	/* do consistency check at end */
	unless (rc) nested_writeHere(n);
	urllist_write(urllist);
	if (ops->runcheck) {
		fclose(f);
		rc |= run_check(ops->verbose,
		    checkfiles, ops->quiet ? 0 : "-v", 0);
		unlink(checkfiles);
		free(checkfiles);
	}
	hash_free(urllist);
	return (rc);
}

/*
 * Verify that it is OK to delete this component as it can be found in
 * one of these urls.  Also verify that the local copy doesn't have any local
 * work.
 *
 * Returns -1 on failure or index of url used as a backup if this
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

	if (nested_isPortal(0)) {
		fprintf(stderr, "Cannot remove components in a portal.\n");
		return (-1);
	}
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
			"bk", "changes", "-qaLnd:REV:", urls[i], SYS)) {
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
