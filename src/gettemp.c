/*
 * Copyright 1999-2000,2002-2006,2011,2013,2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"

private char	**tmpfiles = 0;

private char	*tmpdirs[4];
private int	tmpdirs_len = 0;
private int	tmpdirs_max = 0;

private void
setup_tmpdirs(void)
{
	int	i;
	char	*envtmp;

	/* Setup search path for where to put tempfiles */

	if (envtmp = getenv("TMPDIR")) tmpdirs[tmpdirs_len++] = envtmp;

	/*
	 * Make BKTMP use absolute pathname
	 */
	if (proj_root(0)) {
		char	*tmp;

		tmp = aprintf("%s/" BKTMP, proj_root(0));
		/* Don't allow pathnames with shell characters */
		if (strchrs(tmp, "\n\r\'\"><|`$&;[]*()\\\?")) {
			tmpdirs[tmpdirs_len++] = (strdup)(tmp);
		}
		free(tmp);
	}

	/* /tmp on UNIX */
	tmpdirs[tmpdirs_len++] = (char *)TMP_PATH;

	assert(tmpdirs_len < sizeof(tmpdirs)/sizeof(char *));
	for (i = 0; i < tmpdirs_len; i++) {
		if (strlen(tmpdirs[i]) > tmpdirs_max) {
			tmpdirs_max = strlen(tmpdirs[i]);
		}
	}
}

void
bktmpenv(void)
{
	char	*p;

	unless (tmpdirs_len) setup_tmpdirs();
	p = aprintf("BK_TMP=%s", tmpdirs[0]);
	putenv(p);
}

/*
 * Generate an unique tmpfile:
 *    <dir>/bk-<srcfile>;<lineno>_XXXXXX
 * ex: /tmp/bk_slib;3323_234643
 *
 * filename written to 'buf'
 * returns -1 on failure
 */
private int
getTempfile(char *dir, char *file, int line, char **buf)
{
	int	fd, len;
	char	*freeme = 0;
	char	bn[64];

	strncpy(bn, basenm(file), sizeof(bn));
	bn[sizeof(bn)-1] = 0;
	len = strlen(bn);
	if ((len > 2) && streq(bn+len-2, ".c")) bn[len-2] = 0;

	if (*buf) {
		sprintf(*buf, "%s/bk_%s;%d_XXXXXX", dir, bn, line);
	} else {
		*buf = freeme = aprintf("%s/bk_%s;%d_XXXXXX", dir, bn, line);
	}
	fd = mkstemp(*buf);
	if (fd >= 0) {
		close(fd);
		tmpfiles = addLine(tmpfiles, strdup(*buf));
		return (0);
	} else {
		*buf = 0;
		FREE(freeme);
		return (-1);
	}
}

/*
 * allocate a tmpfile is the default directory
 */
char *
_bktmp(char *file, int line, char *buf)
{
	int	i;

	unless (tmpdirs_len) setup_tmpdirs();
	for (i = 0; i < tmpdirs_len; i++) {
		unless (getTempfile(tmpdirs[i], file, line, &buf)) return (buf);
	}
	fprintf(stderr, "_bktmp(%s, %d) failed", file, line);
	return (0);
}

/*
 * Allocate a temporary directory
 */
char *
_bktmp_dir(char *file, int line, char *buf)
{
	int	i;

	unless (tmpdirs_len) setup_tmpdirs();
	for (i = 0; i < tmpdirs_len; i++) {
		unless (getTempfile(tmpdirs[i], file, line, &buf)) {
			unlink(buf);
			if (mkdir(buf, 0777)) break;
			return (buf);
		}
	}
	fprintf(stderr, "_bktmp_dir(%s, %d) failed", file, line);
	return (0);
}

/*
 * Create a tempfile in BKTMP
 * This is used when the temporary file will be renamed to a BitKeeper
 * file and so MUST be in the bitkeeper tree.
 * Assumes we are in the project root.
 */
char *
_bktmp_local(char *file, int line, char *buf)
{
	unless (getTempfile(BKTMP, file, line, &buf)) return (buf);
	fprintf(stderr, "_bktmp_local(%s, %d) failed", file, line);
	return (0);
}

/*
 * To be called right before the program exits.  This verifies that
 * all the temporary files created during program execution are deleted,
 * and warns the user about undeleted temporary directories.
 */
void
bktmpcleanup(void)
{
	int	i;

	/*
	 * If we were interrupted we may not have closed the files so let's
	 * try and close so winblows can delete them.
	 */
	for (i = 3; i < 20; ++i) {
		closesocket(i);
		close(i);
	}
	unless (tmpfiles) return;
	EACH(tmpfiles) {
		unless (exists(tmpfiles[i])) continue;
		if (isdir(tmpfiles[i])) {
			fprintf(stderr,
			    "WARNING: not deleting orphan directory %s\n",
			    tmpfiles[i]);
		} else {
			unlink(tmpfiles[i]);
		}
	}
	freeLines(tmpfiles, free);
	tmpfiles = 0;
}
