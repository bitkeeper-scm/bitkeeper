#include "sccs.h"
#include "nested.h"
#include "bkd.h"

private	int	unpopulate_check(popts *ops, comp *c);
private	char	*locateComp(popts *ops, comp *cp, char ***flist);


/*
 * called with cp->alias set to which components should be populated
 * this function added or removes component such that the cp->present
 * matches cp->alias
 */
int
nested_populate(nested *n, char **urls, popts *ops)
{
	int	i, j, status, rc;
	int	done = 1;
	int	flags = (ops->quiet ? SILENT : 0);
	retrc	retrc;
	char	**vp = 0;
	char	**checkfiles = 0;
	char	**list;
	remote	*r;
	char	*url;
	comp	*cp;

	ops->n = n;
	ops->urllist = hash_fromFile(hash_new(HASH_MEMHASH), NESTED_URLLIST);
	assert(ops->urllist);
	ops->urls = urls;
	ops->seen = hash_new(HASH_MEMHASH);

	if (ops->last) {
		/* clone has already choosen the URL for some components */
		url = ops->last;
		if (strneq(url, "file://", 7)) {
			url = strdup(url + 7);
			free(ops->last);
			ops->last = url;
		}

		hash_storeStr(ops->seen, url, 0);

		EACH_STRUCT(n->comps, cp, j) {
			if (cp->remotePresent) cp->data = strdup(url);
		}
	}

	/*
	 * Look to see if removed any components will cause a problem
	 * and fail early if we see any.
	 */
	rc = 0;
	EACH_STRUCT(n->comps, cp, j) {
		if (!ops->force && cp->present && !cp->alias) {
			if (nested_isPortal(0)) {
				fprintf(stderr,
				    "Cannot remove components in a portal.\n");
				++rc;
				break;
			}
			if (nested_isGate(0)) {
				fprintf(stderr,
				    "Cannot remove components in a gate.\n");
				++rc;
				break;
			}
			if (cp->pending) {
				fprintf(stderr,
				    "%s: unable to remove ./%s, it contains "
				    "csets not committed in product.\n",
				    prog, cp->path);
				++rc;
			} else if (unpopulate_check(ops, cp)) {
				fprintf(stderr, "%s: unable to remove ./%s\n",
				    prog, cp->path);
				++rc;
			}
		}
		if (!cp->present && cp->alias) {
			/* see if the namespace is not taken */
			if (exists(cp->path) && !nested_emptyDir(n, cp->path)){
				fprintf(stderr, "%s: ./%s not empty\n",
				    prog, cp->path);
				++rc;
			}
			ops->comps++;
		}
	}
	if (rc) goto out;
	START_TRANSACTION();
	/*
	 * Now add all the new repos and keep a list of repos added
	 * so we can cleanup if needed.
	 */
	list = 0;		/* repos added */
	EACH_STRUCT(n->comps, cp, j) {
		unless (!cp->present && cp->alias) continue;
again:		if (url = locateComp(ops, cp, 0)) {
			unless ((flags & SILENT) ||
			    (ops->last && streq(ops->last, cp->data))) {
				if (ops->last) free(ops->last);
				ops->last = strdup(cp->data);
				fprintf(stderr, "Source %s\n", (char *)cp->data);
			}
			r = remote_parse(url, 0);
			assert(r);
			unless (r->params) r->params = hash_new(HASH_MEMHASH);
			hash_storeStr(r->params, "ROOTKEY", cp->rootkey);

			vp = addLine(0, strdup("bk"));
			vp = addLine(vp, strdup("clone"));
			if (ops->debug) vp = addLine(vp, strdup("-d"));
			if (ops->quiet) vp = addLine(vp, strdup("-q"));
			if (ops->verbose) vp = addLine(vp, strdup("-v"));
			if (ops->no_lclone) {
				vp = addLine(vp, strdup("--no-hardlinks"));
			}
			vp = addLine(vp, strdup("-p"));
			vp = addLine(vp, aprintf("-r%s", cp->deltakey));
			vp = addLine(vp,
			    aprintf("-P%u/%u %s", done, ops->comps, cp->path));
			vp = addLine(vp, remote_unparse(r));
			vp = addLine(vp, strdup(cp->path));
			vp = addLine(vp, 0);
			status = spawnvp(_P_WAIT, "bk", vp + 1);
			freeLines(vp, free);
			retrc = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
			if (retrc == 0) {
				done++;
				list = addLine(list, cp);
				cp->present = 1;
				if (ops->runcheck) {
					checkfiles = addLine(checkfiles,
					    aprintf("%s/ChangeSet", cp->path));
				}
			} else if ((retrc == RET_EXISTS) && exists(cp->path)) {
				/*
				 * failed: the dir was not empty no use trying
				 * other urls
				 */
				fprintf(stderr, "%s: ./%s not empty\n",
				    prog, cp->path);
			} else {
				char	*t;

				/* clone failed */
				if (exists(cp->path)) {
					/* failed and left crud */
					nested_rmcomp(n, cp);
				}
				switch(retrc) {
				    case RET_CONNECT:
					t = "cannot connect";
					break;
				    case RET_CHDIR:
					t = "component not present";
					break;
				    case RET_BADREV:
					t = "component missing cset";
					break;
				    default:
					t = "unknown failure";
					break;
				}
				fprintf(stderr,
				    "%s: failed to fetch "
				    "component ./%s from %s: %s\n",
				    prog, cp->path, url, t);
				free(cp->data);
				cp->data = 0;
				cp->remotePresent = 0;
				goto again;
			}
		}
		if (cp->present) {
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
		} else {
			comp	*cj;
			int	j;

			/*
			 * If we failed to add all repos set exit
			 * status and remove other repos we cloned.
			 */
			EACH_STRUCT(list, cj, j) {
				if (cj->present) nested_rmcomp(n, cj);
			}
			rc = 1;
			break;
		}
	}
	freeLines(list, 0);
	STOP_TRANSACTION();
	if (rc) goto out;

	/*
	 * Now after we sucessfully cloned all new components we can
	 * remove the ones to be deleted.
	 */
	reverseLines(n->comps);	/* deeply nested first */
	EACH_STRUCT(n->comps, cp, j) {
		if (cp->present && !cp->alias) {
			verbose((stderr, "%s: removing ./%s...",
				prog, cp->path));
			if (nested_rmcomp(n, cp)) {
				verbose((stderr, "failed\n"));
				fprintf(stderr,
				    "%s: remove of ./%s failed\n",
				    prog, cp->path);
				rc = 1;
				break;
			}
			verbose((stderr, "done\n"));
			cp->present = 0;
		}
	}
	reverseLines(n->comps);	/* restore order */
	if (rc) goto out;

	EACH_STRUCT(n->comps, cp, i) {
		/* just check that the code above all worked */
		if (cp->present) {
			assert(cp->alias);
		} else {
			assert(!cp->alias);
		}
	}

	/* do consistency check at end */
	unless (ops->leaveHERE) nested_writeHere(n);
	if (ops->runcheck) {
		rc |= run_check(ops->verbose,
		    checkfiles, ops->quiet ? 0 : "-v", 0);
		freeLines(checkfiles, free);
	}
out:	urllist_write(ops->urllist);
	EACH_STRUCT(n->comps, cp, i) {
		if (cp->data) free(cp->data);
	}
	hash_free(ops->urllist);
	hash_free(ops->seen);
	if (ops->last) {
		free(ops->last);
		ops->last = 0;
	}
	return (rc);
}

/*
 */
private int
unpopulate_check(popts *ops, comp *c)
{
	FILE	*f;
	char	*t;
	int	i, errs;
	int	rc = -1;
	char	**av;
	char	**flist = 0;

	if (nested_isPortal(0) || nested_isGate(0)) return (-1);
	if (c->pending) return (-1);
	if (chdir(c->path)) {
		perror(c->path);
		goto out;
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
	if (!locateComp(ops, c, &flist)) {
		/* flist is all the URLs that have this component */
		if (nLines(flist) > 0) {
			av = addLine(0, "bk");
			av = addLine(av, "changes");
			av = addLine(av, "--standalone");
			av = addLine(av, "-LD");
			av = catLines(av, flist);
			fprintf(stderr, "Local changes to ./%s found:\n",
			    c->path);
			av = addLine(av, 0);
			/* send changes output to stderr */
			i = dup(1);
			dup2(2, 1);
			spawnvp(_P_WAIT, "bk", av+1);
			dup2(i, 1);
			close(i);
			freeLines(av, 0);
		}
		goto out;
	}
	rc = 0;
out:	proj_cd2product();
	return (rc);
}

/*
 * find best url to fetch component
 *
 * if search failed returns list of urls that
 * have component for error messages
 */
private char *
locateComp(popts *ops, comp *cp, char ***flist)
{
	comp	*c;
	FILE	*f;
	char	*t, *p, *url;
	int	i, j, rc;
	char	**lurls = 0;
	char	**missing = 0;
	int	flags = (ops->quiet ? SILENT : 0);
	char	keylist[MAXPATH];
	char	buf[MAXLINE];

	if (cp->remotePresent && cp->data) goto out;

	cp->remotePresent = 0;
	if (cp->data) free(cp->data);
	cp->data = 0;

	bktmp(keylist, 0);

	/* try urls from cmd line first */
	EACH(ops->urls) lurls = addLine(lurls, strdup(ops->urls[i]));

	/* then ones remembered for this component */
	lurls = urllist_fetchURLs(ops->urllist, cp->rootkey, lurls);

	/* and then the parents */
	lurls = catLines(lurls, parent_allp());

	EACH(lurls) {
		/* try each url once */
		url = lurls[i];
		if (strneq(url, "file://", 7)) url += 7;
		unless (hash_insertStr(ops->seen, url, 0)) continue;

		verbose((stderr, "%s: searching %s...", prog, url));

		f = fopen(keylist, "w");
		/* save just the components we haven't found yet */
		EACH_STRUCT(ops->n->comps, c, j) {
			if (c->product) continue;
			if (c->present == c->alias) continue;
			if (c->remotePresent) continue;

			fprintf(f, "%s %s\n", c->rootkey, c->deltakey);
		}
		fclose(f);
		sprintf(buf, "bk -q@'%s' -Lr -Bstdin havekeys -FD - "
		    "< '%s' 2> " DEVNULL_WR,
		    url, keylist);
		f = popen(buf, "r");
		assert(f);
		while (t = fgetline(f)) {
			if (p = separator(t)) *p = 0;
			unless (c = nested_findKey(ops->n, t)) {
				if (p) p[-1] = ' ';
				verbose((stderr, "failed\n"));
				fprintf(stderr,
				    "%s: bad data from '%s'\n-> %s\n",
				    prog, buf, t);
				pclose(f);
				unlink(keylist);
				goto out;
			}
			unless (p) {
				/* has component, but missing this key */
				missing = addLine(missing, strdup(url));
				continue;
			}
			c->data = strdup(url);
			c->remotePresent = 1;
			urllist_addURL(ops->urllist, c->rootkey, url);
		}
		/*
		 * We have 4 possible exit status values to consider from
		 * havekeys (see comment before remote_bk() for more info):
		 *  0   the connection worked and we captured the data
		 *  16  we connected to the bkd fine, but the repository
		 *      is not there.  This URL is bogus and can be ignored.
		 *  8   The bkd_connect() failed
		 *  33	havekeys says this is myself
		 *  other  Another failure.
		 *
		 */
		rc = pclose(f);
		unlink(keylist);
		rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 1;
		if (rc == 16) {
			/* no repo at that pathname? */
			/* remove URL */
			urllist_rmURL(ops->urllist, 0, url);
			verbose((stderr, "repo gone\n"));
		} else if (rc == 8) {
			/* connect failure */
			verbose((stderr, "connect failure\n"));
		} else if (rc == 33) {
			/* talking to myself */
			urllist_rmURL(ops->urllist, 0, url);
			verbose((stderr, "link to myself\n"));
		} else if (rc != 0) {
			/* some other failure */
			verbose((stderr, "unknown failure\n"));
		} else {
			/* havekeys worked
			 * remove this url from urllist of repos that are
			 * still not found
			 */
			EACH_STRUCT(ops->n->comps, c, j) {
				if (c->product) continue;
				if (c->present == c->alias) continue;
				if (c->remotePresent) continue;

				urllist_rmURL(ops->urllist,
				    c->rootkey, url);
			}
			verbose((stderr, "ok\n"));
		}
		if (cp->remotePresent) break;
	}
out:	freeLines(lurls, free);
	if (cp->remotePresent) {
		freeLines(missing, free);
		if (flist) *flist = 0;
		return (cp->data);
	}
	if (flist) {
		*flist = missing;
	} else {
		freeLines(missing, free);
	}
	fprintf(stderr, "%s: No other sources for ./%s known\n",
	    prog, cp->path);
	return (0);
}
