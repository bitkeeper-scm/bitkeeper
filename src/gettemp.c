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
		mkdirp(tmp);
		/* Don't allow pathnames with shell characters */
		if (strcspn(tmp, " \t\n\r\'\"><|`$&;[]*()\\") == strlen(tmp)) {
			tmpdirs[tmpdirs_len++] = (strdup)(tmp);
		}
		free(tmp);
	}

	/* /tmp on UNIX */
	tmpdirs[tmpdirs_len++] = TMP_PATH;

	assert(tmpdirs_len < sizeof(tmpdirs)/sizeof(char *));
	for (i = 0; i < tmpdirs_len; i++) {
		if (strlen(tmpdirs[i]) > tmpdirs_max) {
			tmpdirs_max = strlen(tmpdirs[i]);
		}
	}
}

char *
bktmp(char *buf, const char *template)
{
	int	i;

	unless (tmpdirs_len) setup_tmpdirs();
	unless (template) template = "none";
	unless (buf) buf = malloc(tmpdirs_max + strlen(template) + 12);

	for (i = 0; i < tmpdirs_len; i++) {
		int	fd;
		sprintf(buf, "%s/bk_%s_XXXXXX", tmpdirs[i], template);
		fd  = mkstemp(buf);
		if (fd != -1) {
			tmpfiles = addLine(tmpfiles, strdup(buf));
			close(fd);
			return(buf);
		}
	}
	perror("mkstemp() failed:");
	buf[0] = 0;
	return 0;
}

/*
 * Create a tempfile in BKTMP
 * This is used when the temporary file will be renamed to a BitKeeper
 * file and so MUST be in the bitkeeper tree.
 * Assumes we are in the project root.
 */
char	*
bktmp_local(char *buf, const char *template)
{
	int	fd;

	unless (template) template = "none";
	unless (buf) buf = malloc(strlen(BKTMP) + strlen(template) + 17);

	sprintf(buf, BKTMP "/bklocal_%s_XXXXXX", template);
	fd = mkstemp(buf);
	if (fd != -1) {
		tmpfiles = addLine(tmpfiles, strdup(buf));
		close(fd);
		return (buf);
	} else {
		char	*buf;
		buf = aprintf("bktmp_local(, %s), failed", template);
		perror(buf);
		free(buf);
	}
	return (0);
}

/*
 * To be called right before the program exits.  This verifies that
 * all the tempary files created during program execution are deleted.
 */
void
bktmpcleanup(void)
{
	int	i;

	unless (tmpfiles) return;
	EACH(tmpfiles) {
		if (exists(tmpfiles[i])) {
			fprintf(stderr, "WARNING: deleting orphan file %s\n",
				tmpfiles[i]);
			unlink(tmpfiles[i]);
		}
		free(tmpfiles[i]);
	}
	free(tmpfiles);
	tmpfiles = 0;
}
