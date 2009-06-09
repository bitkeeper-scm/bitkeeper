#include "system.h"
#include "sccs.h"
#include "bkd.h"
#include "logging.h"

/*
 * TODO
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

private	char	*urlbase = 0;
private	int	flags = 0;

int
upgrade_main(int ac, char **av)
{
	int	c, i;
	int	fetchonly = 0;
	int	install = 1;
	int	force = 0;
	int	obsolete = 0;
	char	*oldversion, *errs = 0, *lic;
	char	*indexfn, *index;
	char	*p, *e;
	char	*platform = 0, *version = 0;
	char	**platforms;
	char	**data = 0;
	int	len;
	FILE	*f, *fout;
	MDBM	*configDB = proj_config(0);
	char	*licf;
	int	rc = 2;
	char	*tmpbin = 0;
	mode_t	myumask;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "a|cdfinq")) != -1) {
		switch (c) {
		    case 'a':
		    	unless (platform = optarg) {
				platform = "?";
				install = 0;
				flags |= SILENT;
			}
			break;
		    case 'c': install = 0; break;	/* check only */
		    case 'f': force = 1; break;		/* force */
		    case 'i': install = 1; break;	/* now default, noop */
		    case 'd':				/* download only */
		    case 'n':				// obsolete, for compat
			install = 0; fetchonly = 1; break;
		    case 'q': flags |= SILENT; break;
		    default:
usage:			system("bk help -s upgrade");
			return (1);
		}
	}
	if (av[optind]) {
		if (av[optind+1]) goto usage;
		urlbase = av[optind];

		if (platform && streq(platform, "?") && !strchr(urlbase,'/')) {
			fprintf(stderr, "upgrade: did you mean to say -a%s\n",
			    urlbase);
			exit(1);
		}
	} else if ((p = proj_configval(0, "upgrade-url")) && *p) {
		urlbase = p;
	} else {
		urlbase = UPGRADEBASE;
	}
	if (platform) {
		if (install && !streq(platform, bk_platform)) {
			notice("upgrade-install-other-platform", 0, "-e");
			goto out;
		}
	} else {
		platform = bk_platform;
	}
	if (win32() && (p = getenv("OSTYPE"))
	    && streq(p, "msys") && (fetchonly || install)
	    && !getenv("BK_REGRESSION")) {
		notice("upgrade-nomsys", 0, "-e");
		goto out;
	}
	if (install && noperms(bin)) {
		notice("upgrade-badperms", bin, "-e");
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
 	p = secure_hashstr(index, strlen(index), makestring(KEY_UPGRADE));
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
	 * 6 unused
	 * -- new fields ALWAYS go at the end! --
	 */
	platforms = allocLines(20);
	p = index;
	while (*p) {
		if (e = strchr(p, '\n')) *e++ = 0;
		if (p[0] == '#') {
			/* comments */
		} else if (strneq(p, "old ", 4)) {
			if (streq(p + 4, bk_vers)) obsolete = 1;
		} else if (!data) {
			data = splitLine(p, ",", 0);
			if (nLines(data) < 6) goto next;
			if (platforms) { /* remember platforms */
				platforms =
				    addLine(platforms, strdup(data[5]));
			}
			unless (version) version = strdup(data[3]);
			if (streq(data[5], platform)) {
				/* found this platform */
				freeLines(platforms, free);
				platforms = 0;
			} else {
next:				freeLines(data, free);
				data = 0;
			}
		}
		p = e;
	}
	free(index);
	index = 0;

	if (platforms) {	/* didn't find this platform */
		uniqLines(platforms, free);
		unless (streq(platform, "?")) {
			fprintf(stderr,
			    "No upgrade for the arch %s found. "
			    "Available architectures for %s:\n",
			    platform, version);
			EACH(platforms) fprintf(stderr, "  %s\n", platforms[i]);
			rc = 2;
		} else {
			printf("Available architectures for %s:\n", version);
			EACH(platforms) printf("  %s\n", platforms[i]);
			rc = 0;
		}
		freeLines(platforms, free);
		goto out;
	}
	/*
	 * Look to see if we already have the current version
	 * installed.  We compare UTC to catch releases that get
	 * tagged more than once. (like bk-3.2.3)
	 */
	if (data && getenv("BK_REGRESSION")) {
		/* control matches for regressions */
		data[4] = strdup(getenv("BK_UPGRADE_FORCEMATCH") ? bk_utc : "");
	}
	if (data && streq(data[4], bk_utc) && !fetchonly) {
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
"latest version so the upgrade is cancelled.  Rerun with the -f (force)\n"
"option to force the upgrade\n", data[3], bk_vers);
		goto out;
	}
	/* obtain new lease here */
	oldversion = bk_vers;
	bk_vers = data[3];
	unless (lic = lease_bkl(0, &errs)) {
		lease_printerr(errs);
		free(errs);
		notice("upgrade-require-license", 0, "-e");
		rc = 2;
		goto out;
	}
	bk_vers = oldversion;
	free(lic);

	unless (fetchonly || install) {
		printf("BitKeeper version %s is available for download.\n",
		    data[3]);
		printf("Run\n"
		    "\tbk upgrade\t# to download and install the new bk\n"
		    "\tbk upgrade -d\t# to download bk without installing\n");
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
	assert(p);
	rewind(f);
	unless (streq(p, data[2])) {
		fprintf(stderr, "upgrade: file %s fails to match checksum\n",
		    data[1]);
		fclose(f);
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

	/* embed bk license, we know it is valid because we got a lease above */
	licf = bktmp(0, 0);
	f = fopen(licf, "w");
	if ((p = getenv("BK_LICENSEURL")) ||
	    (p = mdbm_fetch_str(configDB, "licenseurl"))) {
		fprintf(f, "licenseurl: %s\n", p);
	} else if (p = mdbm_fetch_str(configDB, "license")) {
		fprintf(f, "license: %s\n", p);
		i = 1;
		do {
			sprintf(buf, "licsign%d", i++);
			if (p = mdbm_fetch_str(configDB, buf)) {
				fprintf(f, "%s: %s\n", buf, p);
			}
		} while (p);
	} else {
		fprintf(stderr, "upgrade: can't find license to embed\n");
		goto out;
	}
	fclose(f);
	unless (getenv("_BK_UPGRADE_NOINSKEYS")) {
		rc = inskeys(data[1], licf);
		unlink(licf);
		free(licf);
		if (rc) goto out;
		rc = 2;
	}
	myumask = umask(0);
	umask(myumask);
	chmod(data[1], 0555 & ~myumask);

	if (fetchonly) {
		printf("New version of bk fetched: %s\n", data[1]);
		rc = 0;
		goto out;
	}
	putenv("BK_NOLINKS=1");	/* XXX -u already does this */
	sprintf(buf, "./%s -u", data[1]);
	if (system(buf)) {
		fprintf(stderr, "upgrade: install failed\n");
		goto out;
	}
	unlink(data[1]);
	rc = 0;
 out:
	if (version) free(version);
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
	char	*test_file;
	int	rc = 1;

	/*
	 * Assumes subdirs are ok.
	 */
	unless (test_file = aprintf("%s/upgrade_test.tmp", target)) return (1);
	if (touch(test_file, 0644) < 0) goto out;
	if (lstat(test_file, &sb)) goto out;
	if (unlink(test_file)) goto out;
	rc = 0;
out:
	free(test_file);
	return (rc);
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
	concat_path(buf, urlbase, name);
	unless (strneq(buf, "http://", 7)) {
		/* urlbase might contain a local pathname */
		return (fileCopy(buf, file));
	}
	verbose((stderr, "Fetching %s\n", buf));
	r = remote_parse(buf, 0);
	if (http_connect(r)) goto out;
	r->progressbar = 1;
	if (getenv("BK_NOTTY") || (flags & SILENT)) r->progressbar = 0;
	rc = http_fetch(r, file);
out:
	remote_free(r);
	return (rc);
}
