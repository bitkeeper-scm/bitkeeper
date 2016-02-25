/*
 * Copyright 2010-2011,2014-2016 BitMover, Inc
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
#include "cfg.h"

/*
 * Update the BitKeeper/etc/attr file with information for the current
 * repository.
 * If something changed, then return 1
 * If an error, then return -1
 * If unchanged: return 0.
 */
int
attr_update(void)
{
	sccs	*s;
	int	rc;
	int	iflags = SILENT;
	int	gflags = GET_EDIT|GET_SKIPGET|SILENT;
	int	dflags = DELTA_AUTO|DELTA_DONTASK|SILENT;

	if (getenv("_BK_NO_ATTR")) return (0);

	/*
	 * We have seen problems with 4.x merging the attr file; don't
	 * record it unless they ask for it or we are nested.
	 */
	unless (getenv("BK_ATTR") || proj_isEnsemble(0)) return (0);

	attr_write(ATTR);
	s = sccs_init(SATTR, iflags);
	assert(s);
	if (HASGRAPH(s)) {
		/* edit the file */
		unless (HAS_PFILE(s)) {
			rc = sccs_get(s, 0, 0, 0, 0, gflags, 0, 0);
			//assert(rc == 0);
		}
	} else {
		dflags |= DELTA_NEWFILE;  /* XXX: like poly, should add |DELTA_DB; */
	}
	/* attr files in the wild can have keywords; not useful */
	s->xflags &= ~(X_RCS|X_EXPAND1|X_SCCS);

	rc = sccs_delta(s, dflags, 0, 0, 0, 0);
	if (rc == -2) {
		/* no delta if no diffs in file */
		xfile_delete(s->gfile, 'p');
		unlink(s->gfile);
		rc = 0;
	} else if (rc) {
		rc = -1;	/* fold all other errors */
	} else {
		rc = 1;
	}
	sccs_free(s);
	return (rc);
}

/*
 * Write a new version of BitKeeper/etc/attr
 */
int
attr_write(char *file)
{
	FILE	*f;
	char	*t;
	char	**lines;
	int	i;
	int	incomp;
	project	*proj = proj_isResync(0);

	/*
	 * We have seen problems with 4.x merging the attr file; don't
	 * record it unless they ask for it or we are nested.
	 */
	unless (getenv("BK_ATTR") || proj_isEnsemble(0)) return (0);

	/*
	 * Note: we don't merge into existing attributes.  If we did then
	 * we would need to figure out how to merge conflicts in keys we
	 * don't understand.
	 * Just nuke the file and write it fresh.
	 */
	unlink(file);
	unless (f = fopen(file, "wb")) {
		perror(file);
		return (-1);
	}

	/*
	 * We don't bother tracking data for components while they are
	 * attached because it is mostly redundant with the product
	 * data.  We clear the fields so the wrong data can't be
	 * returned and we still print the keys so the weave will
	 * likely use those lines as anchor points.
	 */
	incomp = proj_isComponent(proj);

	/*
	 * Keep keys sorted
	 * HERE
	 * ID
	 * TEST
	 * VERSION
	 */

	if (proj_isProduct(proj)) {
		fputs("@HERE\n", f);
		lines = nested_here(proj);
		uniqLines(lines, free);
		EACH(lines) fprintf(f, "%s\n", lines[i]);
		fprintf(f, "\n"); /* so final newline is included */
		freeLines(lines, free);
	}

	fputs("@ID\n", f);
	fprintf(f, "%s\n", proj_rootkey(proj));


	if (getenv("BK_REGRESSION") && (t = getenv("_BK_ATTR_TEST"))) {
		fprintf(f, "@TEST\n%s\n", t);
	}

	fputs("@VERSION\n", f);
	unless (incomp) {
		/* match version -s alg */
		t = bk_vers;
		if (strneq(t, "bk-", 3)) t += 3;
		fprintf(f, "%s\n", t);
	}

	fclose(f);

	return (0);
}
