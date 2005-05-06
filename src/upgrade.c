#include "system.h"
#include "sccs.h"
#include "bkd.h"
#include "logging.h"

/*
 * TODO
 *  - update crypto manpage?
 *  - test http proxy password (changed base64 code)
 *
 *  - There should be a check for upgrades item in the Windows startup
 *    menu.
 *
 *  - change the installer to also take a -i option.
 */

#define	UPGRADEBASE	"http://upgrades.bitkeeper.com/upgrades"

private	int	noperms(char *target);
private	int	upgrade_fetch(char *name, char *file);

extern	char	*bin;

private	char	*urlbase = 0;
private	int	flags = 0;
private const char	key[] = {
	0x06, 0x0f, 0x0e, 0x02, 0x24, 0x1d, 0x29, 0x1d, 0x17, 0x7f,
	0x76, 0x32, 0x06, 0x1c, 0x29, 0x37, 0x44, 0x5d, 0x03, 0x2a,
	0x2e, 0x2e
};

int
upgrade_main(int ac, char **av)
{
	int	c;
	int	fetchonly = 0;
	int	install = 0;
	int	force = 0;
	int	obsolete = 0;
	char	*indexfn, *index;
	char	*p, *e;
	char	**data = 0;
	int	len;
	char	*want_codeline = 0;
	FILE	*f, *fout;
	int	rc = 2;
	char	*tmpbin = 0;
	char	salt[sizeof(key) + 1];
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "il:nfq")) != -1) {
		switch (c) {
		    case 'i': install = 1; break;
		    case 'l': want_codeline = optarg; break;
		    case 'n': fetchonly = 1; break;
		    case 'f': force = 1; break;
		    case 'q': flags |= SILENT; break;
		    default:
usage:			system("bk help -s upgrade");
			return (1);
		}
	}
	if (av[optind]) {
		if (av[optind+1]) goto usage;
		urlbase = av[optind];
	} else if ((p = user_preference("upgrade-url")) && *p) {
		urlbase = p;
	} else {
		urlbase = UPGRADEBASE;
	}
	if (install && fetchonly) {
		fprintf(stderr, "upgrade: -i or -n but not both.\n");
		goto out;
	}
	if (install && noperms(bin)) {
		notice("upgrade-badperms", bin, "-e");
		goto out;
	}
	unless (want_codeline) {
		if (!(p = strrchr(bk_vers, '-'))) {
			notice("upgrade-devbuild", 0, "-e");
			goto out;
		}
		want_codeline = strndup(bk_vers, p - bk_vers);
	}
	unless (streq(want_codeline, "bk-free") || lease_anycommercial()) {
		notice("upgrade-require-lease", 0, "-e");
		goto out;
	}
	if (!av[optind] && !proj_root(0)) {
		notice("upgrade-needrepo", 0, "-e");
		goto out;
	}
	indexfn = bktmp(0, "upgrade-idx");
	if (upgrade_fetch("INDEX", indexfn)) {
		fprintf(stderr, "upgrade: unable to fetch INDEX\n");
		free(indexfn);
		goto out;
	}
	index = loadfile(indexfn, &len);
	unlink(indexfn);
	free(indexfn);
	indexfn = 0;
	p = index + len - 1;
	*p = 0;
	while (p[-1] != '\n') --p;
	strcpy(buf, p);	/* hmac */
	*p = 0;
	makestring(salt, (char *)key, 'Q', sizeof(key));
 	p = secure_hashstr(index, strlen(index), salt);
	unless (streq(p, buf)) {
		fprintf(stderr, "upgrade: INDEX corrupted\n");
		free(index);
		goto out;
	}
	/* format:
	 * 1 filename
	 * 2 md5sum
	 * 3 version
	 * 4 utc
	 * 5 platform
	 * 6 codeline
	 * -- new fields ALWAYS go at the end! --
	 */
	p = index;
	while (*p) {
		if (e = strchr(p, '\n')) *e++ = 0;
		if (p[0] == '#') {
			/* comments */
		} else if (strneq(p, "old ", 4)) {
			if (streq(p + 4, bk_vers)) obsolete = 1;
		} else if (!data) {
			data = splitLine(p, ",", 0);
			unless ((nLines(data) == 6) &&
			    streq(data[5], bk_platform) &&
			    streq(data[6], want_codeline)) {
				freeLines(data, free);
				data = 0;
			};
		}
		p = e;
	}
	free(index);
	index = 0;
	/*
	 * Look to see if we already have the current version
	 * installed.  We compare UTC to catch releases that get
	 * tagged more than once. (like bk-3.2.3)
	 */
	if (data && streq(data[4], bk_utc) && !(fetchonly && force)) {
		freeLines(data, free);
		data = 0;
	}
	unless (data) {
		printf("upgrade: no new version of bitkeeper found\n");
		rc = 1;
		goto out;
	}
	if (!obsolete && !force) {
		fprintf(stderr,
"upgrade: A new version of BitKeeper is available (%s), but this\n"
"version of BitKeeper (%s) is not marked as being obsoleted by the\n"
"latested version so the upgrade is cancelled.  Rerun with the -f (force)\n"
"option to force the upgrade\n", data[3], bk_vers);
		goto out;
	}
	unless (fetchonly || install) {
		printf("BitKeeper version %s is available for download.\n",
		    data[3]);
		printf(
		     "rerun 'bk upgrade' with -n to fetch or -i to install\n");
		rc = 0;
		goto out;
	}
	tmpbin = aprintf("%s.tmp", data[1]);
	if (upgrade_fetch(data[1], tmpbin)) {
		fprintf(stderr, "upgrade: unable to fetch %s\n", data[1]);
		goto out;
	}

	/* find checksum of the file we just fetched */
	f = fopen(tmpbin, "r");
	p = hashstream(f);
	rewind(f);
	unless (streq(p, data[2])) {
		fprintf(stderr, "upgrade: file %s fails to match checksum\n",
		    data[1]);
		free(p);
 		goto out;
	}
	free(p);

	/* decrypt data */
	unlink(data[1]);
	fout = fopen(data[1], "w");
	assert(fout);
	if (upgrade_decrypt(f, fout)) goto out;
	fclose(f);
	fclose(fout);
	unlink(tmpbin);
	free(tmpbin);
	tmpbin = 0;

	chmod(data[1], 0500);
	if (fetchonly) {
		printf("New version of bk fetched: %s\n", data[1]);
		rc = 0;
		goto out;
	}
	putenv("BK_NOLINKS=1");
	sprintf(buf, "./%s -u", data[1]);
	if (system(buf)) {
		fprintf(stderr, "upgrade: install failed\n");
		goto out;
	}

	rc = 0;
 out:
	if (data) freeLines(data, free);
	if (tmpbin) {
		unlink(tmpbin);
		free(tmpbin);
	}
	return (rc);
}

/*
 * verify that the current user can replace all files at target
 */
private	int
noperms(char *target)
{
	struct	stat sb;

	/*
	 * If chmod works, I must have perms right?
	 * Assumes subdirs are ok.
	 */
	if (lstat(target, &sb)) return (1);
	if (chmod(target, 0770)) return (1);
	if (chmod(target, sb.st_mode)) return (1);
	return (0);
}

/*
 * Fetch filename from web using http.  If size is non-zero then a progress
 * bar will be printed unless flags&SILENT.
 * If localdir is set, then files will be copied from there instead.
 * Returns non-zero on error.
 */
private	int
upgrade_fetch(char *name, char *file)
{
	remote	*r;
	int	rc = 1;
	char	buf[MAXPATH];

	unlink(file);
	sprintf(buf, "%s/%s", urlbase, name);
	unless (strneq(buf, "http://", 7)) {
		/* urlbase might contain a local pathname */
		return (fileCopy(buf, file));
	}
	verbose((stderr, "Fetching %s\n", buf));
	r = remote_parse(buf);
	if (http_connect(r)) goto out;
	r->progressbar = 1;
	if (getenv("BK_NOTTY") || (flags & SILENT)) r->progressbar = 0;
	rc = http_fetch(r, buf, file);
out:
	remote_free(r);
	return (rc);
}
