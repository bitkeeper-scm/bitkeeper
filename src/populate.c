/*
 * Copyright 2008-2013,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sccs.h"
#include "nested.h"
#include "bkd.h"

private	int	unpopulate_check(popts *ops, comp *c);

/*
 * called with cp->alias set to which components should be populated
 * this function added or removes component such that the cp->present
 * matches cp->alias
 */
int
nested_populate(nested *n, popts *ops)
{
	int	i, j, k, status, rc;
	int	done = 1;
	int	flags = (ops->quiet ? SILENT : 0);
	retrc	retrc;
	char	**vp = 0;
	char	**checkfiles = 0;
	char	**list;
	remote	*r;
	char	*url;
	comp	*cp;
	int	transaction = (getenv("_BK_TRANSACTION") != 0);

	/* we assume pending components are marked */
	assert(n->pending);

	ops->n = n;

	/*
	 * See if removing components will cause a problem and fail
	 * early if it will.
	 */
	rc = 0;
	EACH_STRUCT(n->comps, cp, j) {
		if (!cp->alias && C_PRESENT(cp)) {
			if (nested_isPortal(0) && !transaction) {
				fprintf(stderr,
				    "Cannot remove components in a portal.\n");
				++rc;
				break;
			}
			if (nested_isGate(0) && !transaction) {
				fprintf(stderr,
				    "Cannot remove components in a gate.\n");
				++rc;
				break;
			}
		}
		if (!ops->force && !cp->alias && C_PRESENT(cp)) {
			if (C_PENDING(cp)) {
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
		if (cp->alias && !C_PRESENT(cp)) {
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
		unless (cp->alias && !C_PRESENT(cp)) continue;
		k = 0;
		while (url = urllist_find(n, cp, flags, &k)) {
			unless (flags & SILENT) {
				unless (ops->lasturl &&
				    streq(ops->lasturl, url)) {
					fprintf(stderr, "Source %s\n", url);
					ops->lasturl = url;
				}
			}
			unless (r = remote_parse(url, 0)) {
				fprintf(stderr,
				    "%s: cannot parse url '%s', skipping\n",
				    prog, url);
				continue;
			}
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
			vp = addLine(vp, aprintf("-j%d", ops->parallel));
			vp = addLine(vp,
			    aprintf("-r%s", cp->useLowerKey ?
				cp->lowerkey : C_DELTAKEY(cp)));
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
				cp->_present = 1;
				if (ops->runcheck) {
					checkfiles = addLine(checkfiles,
					    aprintf("%s/ChangeSet", cp->path));
				}
				break;
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
			}
			// failed to clone
			unless (url) break;
		}
		if (C_PRESENT(cp)) {
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
				if (C_PRESENT(t) &&
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
				if (C_PRESENT(cj)) nested_rmcomp(n, cj);
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
	/* deeply nested first */
	EACH_REVERSE(n->comps) {
		cp = (comp *)n->comps[i];
		if (C_PRESENT(cp) && !cp->alias) {
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
			cp->_present = 0;
		}
	}
	if (rc) goto out;

	EACH_STRUCT(n->comps, cp, i) {
		/* just check that the code above all worked */
		if (C_PRESENT(cp)) {
			assert(cp->alias);
		} else {
			assert(!cp->alias);
		}
	}

	/* do consistency check at end */
	unless (ops->leaveHERE) nested_writeHere(n);
	if (ops->runcheck) {
		rc |= run_check(ops->quiet, ops->verbose, checkfiles, "-u", 0);
		freeLines(checkfiles, free);
	}
out:	proj_reset(0);
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
	int	flags = 0;
	int	rc = -1;
	char	**av;
	char	**flist = 0;
	urlinfo *data;

	flags |= URLLIST_GATEONLY;  /* require gates */
	if (ops->quiet) flags |= SILENT;
	/*
	 * NOTE: Belts and suspenders - already checked in the caller
	 */
	if ((nested_isPortal(0) || nested_isGate(0)) &&
	    !getenv("_BK_TRANSACTION")) {
		return (-1);
	}
	if (C_PENDING(c)) return (-1);
	if (chdir(c->path)) {
		perror(c->path);
		goto out;
	}
	f = popen("bk gfiles -cxp -v", "r");
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
	unless (ops->noURLprobe || urllist_find(ops->n, c, flags, 0)) {
		/* print all the tried URLs that have this component */
		flist = 0;
		EACH_STRUCT(ops->n->urls, data, i) {
			unless (data->checked) continue;

			if ((t = hash_fetch(data->pcomps, &c, sizeof(comp *)))
			    && (*t == '0')) {
				flist = addLine(flist, data->url);
			}
		}
		if (nLines(flist) > 0) {
			av = addLine(0, "bk");
			av = addLine(av, "changes");
			av = addLine(av, "--standalone");
			av = addLine(av, "-LD");
			av = catLines(av, flist);
			freeLines(flist, 0);
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
 * Find best url to fetch component.
 *
 * Uses:
 *   n->urls
 */
char *
urllist_find(nested *n, comp *cp, int flags, int *idx)
{
	char	*t;
	int	i, j;
	int	gateonly = (flags & URLLIST_GATEONLY);
	FILE	*out;
	urlinfo	*data, *data2;

	/* In quiet mode, buffer to display on error */
	out = (flags & SILENT) ? fmem() : stderr;

	unless (n->urls) urlinfo_buildArray(n);

	i = idx ? (1 + *idx) : 1;
	EACH_START(i, n->urls, i) {
		data = (urlinfo *)n->urls[i];

		// skip urls that we don't think have the component
		unless ((t = hash_fetch(data->pcomps, &cp, sizeof(comp *))) &&
		    (*t == '1')) {
			continue;
		}

		// continue if we know it isn't a gate and they only want gates
		if (gateonly && (data->gate == 0)) continue;

		if (data->repoID) {
			/* don't talk to myself */
			if (streq(data->repoID, proj_repoID(n->proj))) continue;

			/*
			 * See if we have already passed a validated URL
			 * that points at the same repoID
			 * If so don't look here.
			 */
			EACH_STRUCT(n->urls, data2, j) {
				unless (j < i) break;

				if (data2->repoID && data2->checkedGood &&
				    streq(data->repoID, data2->repoID)) {
					break;
				}
			}
			if (j < i) continue;
		}

		// probe it, this updates the urlinfo struct
		unless (data->checked) urlinfo_probeURL(n, data->url, out);

		unless (data->checkedGood) continue; // probe failed

		if (gateonly && (data->gate != 1)) continue;

		if ((t = hash_fetch(data->pcomps, &cp, sizeof(comp *))) &&
		    (*t == '1')) {
			// now we know it has the component, so return it
			if (flags & SILENT) fclose(out);
			if (idx) *idx = i;	/* start where left off */
			return (data->url);
		}
	}
	if (flags & SILENT) {
		unless (flags & URLLIST_NOERRORS) {
			fputs(fmem_peek(out, 0), stderr);
		}
		fclose(out);
	}
	unless (flags & URLLIST_NOERRORS) {
		if (gateonly) {
			fprintf(stderr,
			    "%s: ./%s cannot be found at a gate\n",
			    prog, cp->path);
		} else {
			fprintf(stderr, "%s: No other sources for ./%s known\n",
			    prog, cp->path);
		}
	}
	if (idx) *idx = i;	/* start where left off */
	return (0);
}
