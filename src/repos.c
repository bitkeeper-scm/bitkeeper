#include "sccs.h"

/*
 * Update the list at `bk dotbk`/repos/`bk gethost -r`/path.log
 * with pathname to the current repository.
 *
 * Just skip it if there are problems updating the file.
 */
void
repos_update(sccs *cset)
{
	FILE	*f;
	char	*p, *lock;
	char	rfile[MAXPATH];
	char	buf[MAXPATH];
	char	path[MAXPATH];

	sprintf(rfile, "%s/repos/%s/path.log", getDotBk(), sccs_realhost());
	unless (exists(rfile)) {
		if (mkdirf(rfile)) return;
	}

	p = proj_root(cset->proj);
	if (proj_isCaseFoldingFS(cset->proj)) {
		getRealName(p, 0, path);
	} else {
		strcpy(path, p);
	}

	/* look for path in existing file */
	if (f = fopen(rfile, "r")) {
		while (fnext(buf, f)) {
			chomp(buf);
			if (streq(buf, path)) {
				fclose(f);
				return;
			}
		}
		fclose(f);
	}

	/* not found, so append to end */

	lock = aprintf("%s.lock", rfile);
	/* wait 10 seconds for lock and then bail */
	if (sccs_lockfile(lock, 10, 1)) {
		free(lock);
		return;
	}
	if (f = fopen(rfile, "a")) {
		fprintf(f, "%s\n", path);
		fclose(f);
	}
	sccs_unlockfile(lock);
	free(lock);
}
