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
		dflags |= NEWFILE;  /* XXX: like poly, should add |DELTA_DB; */
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
	 * LICENSE
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

	fputs("@LICENSE\n", f);
	/*
	 * Save the license used to create this cset.
	 *  XXX what to put here?
	 * proj_bkl(proj) returns the license key in the current lease,
	 * but that changes all the time and isn't really important.
	 *
	 * We use 'license' from the config file, but that might not actually
	 * be the license key that was used to fetch the curent lease.
	 * Also with licenseurl, we may not even have a license.
	 *
	 * Unfortunately we don't save the original license key in a
	 * lease.
	 */
	unless (incomp) {
		t = cfg_str(proj, CFG_LICENSE);
		fprintf(f, "%s\n", notnull(t));
	}

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
