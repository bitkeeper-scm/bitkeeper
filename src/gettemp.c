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

char *
bktmp(char *buf, const char *template)
{
	int	i;
	char	*tmp, *p;

	unless (tmpdirs_len) setup_tmpdirs();
	unless (template) template = "none";
	tmp = strdup(template);
	if (strlen(tmp) > 64) tmp[64] = 0;
	while (p = strchr(tmp, '?')) *p = '_';
	unless (buf) buf = malloc(tmpdirs_max + strlen(tmp) + 12);

	for (i = 0; i < tmpdirs_len; i++) {
		int	fd;
		sprintf(buf, "%s/bk_%s_XXXXXX", tmpdirs[i], tmp);
		fd  = mkstemp(buf);
		if (fd != -1) {
			tmpfiles = addLine(tmpfiles, strdup(buf));
			close(fd);
			free(tmp);
			return(buf);
		}
	}
	perror("mkstemp() failed");
	buf[0] = 0;
	free(tmp);
	return (0);
}

char *
bktmpdir(char *buf, const char *template)
{
	int	i, c, pid = getpid();

	unless (tmpdirs_len) setup_tmpdirs();
	unless (template) template = "none";
	unless (buf) buf = malloc(tmpdirs_max + strlen(template) + 12);

	for (i = 0; i < tmpdirs_len; i++) {
		for(c=0; c < INT_MAX; c++) {
			sprintf(buf, "%s/bk_%s_%d%d",
					tmpdirs[i], template, pid, c);
			if (mkdir(buf, 0777) == -1) {
				if (errno == EEXIST) {
					continue;
				} else {
					break;
				}
			}
			tmpfiles = addLine(tmpfiles, strdup(buf));
			return (buf);
		}
	}
	perror("mkdir() failed");
	buf[0] = 0;
	return (0);
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
	char	*tmp;

	unless (template) template = "none";
	tmp = strdup(template);
	if (strlen(tmp) > 64) tmp[64] = 0;
	unless (buf) buf = malloc(strlen(BKTMP) + strlen(tmp) + 17);
	sprintf(buf, BKTMP "/bklocal_%s_XXXXXX", tmp);
	free(tmp);
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
