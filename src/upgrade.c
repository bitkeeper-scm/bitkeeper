/*
 * Copyright 2004-2007,2009-2016 BitMover, Inc
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
#include "bkd.h"
#include "cfg.h"

/*
 * TODO
 *  - test http proxy password (changed base64 code)
 *
 *  - There should be a check for upgrades item in the Windows startup
 *    menu.
 *
 *  - change the installer to also take a -i option.
 */

#define	UPGRADEBASE	"http://www.bitkeeper.org/downloads/latest"
#define	UPGRADETRIAL	UPGRADEBASE

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
	int	show_latest = 0;
	int	force = 0;
	int	obsolete = 0;
	char	*oldversion;
	char	*indexfn, *index;
	char	*p, *e;
	char	*platform = 0, *version = 0;
	char	**platforms, **bininstaller = 0;
	char	**data = 0;
	int	len;
	FILE	*f;
	int	rc = 2;
	char	*bundle = 0;
	mode_t	myumask;
	static longopt	lopts[] = {
		{"show-latest", 300 },
		{ 0, 0 }
	};
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "a|cdfinq", lopts)) != -1) {
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
		    case 300: // --show-latest
			show_latest = 1;
			fetchonly = 1;
			install = 0;
			flags |= SILENT;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) {
		if (av[optind+1]) usage();
		urlbase = av[optind];

		if (platform && streq(platform, "?") && !strchr(urlbase,'/')) {
			fprintf(stderr, "upgrade: did you mean to say -a%s\n",
			    urlbase);
			exit(1);
		}
	} else if (p = cfg_str(0, CFG_UPGRADE_URL)) {
		urlbase = p;
	} else if (test_release) {
		urlbase = UPGRADETRIAL;
	} else {
		urlbase = UPGRADEBASE;
	}
	if (streq(bk_platform, "powerpc-macosx") &&
	    (!platform || streq(platform, "x86-macosx"))) {
		/*
		 * Check to see if they are running a powerpc bk on an
		 * intel mac under rosetta.
		 */
		if ((p = backtick("uname -p", 0)) && streq(p, "i386")) {
			bk_platform = "x86-macosx";
		}
	}
	if (platform) {
		if (install && !streq(platform, bk_platform)) {
			notice("upgrade-install-other-platform", 0, "-e");
			goto out;
		}
	} else if (p = getenv("BK_UPGRADE_PLATFORM")) {
		/*
		 * This is mainly useful for development machines that
		 * are using a platform that we don't actually release
		 * to customers.
		 */
		platform = p;
	} else {
		platform = bk_platform;
	}
	if (macosx()) {
		/* figure out if we're in a bundle or not */
		bundle = fullname(bin, 0);
		if (p = strstr(bundle, "BitKeeper.app")) {
			/* we know the app name, we want the dir where
			 * it goes */
			*(p+13) = 0; /* NULL at end of BitKeeper.app */
		} else {
			bundle = 0;
		}
	}
	if (win32() && (p = getenv("OSTYPE"))
	    && streq(p, "msys") && (fetchonly || install)
	    && !getenv("BK_REGRESSION")) {
		notice("upgrade-nomsys", 0, "-e");
		goto out;
	}
	if (!macosx() && install && noperms(bin)) {
		notice("upgrade-badperms", bin, "-e");
		goto out;
	}
	indexfn = bktmp(0);
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
	p = hashstr(index, strlen(index));
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
			if (show_latest) {
				// bk_ver,bk_utc
				printf("%s,%s\n", data[3], data[4]);
				rc = 0;
				goto out;
			}
			unless (version) version = strdup(data[3]);
			if (streq(data[5], platform)) {
				/* found this platform */
				if (macosx()) {
					/* if we hit a .bin, skip it. we
					 * want the .pkg */
					p = strrchr(data[1], '.');
					if (streq(p , ".bin")) {
						/* we want to replicate data */
						p = joinLines(",", data);
						bininstaller = splitLine(p,
						    ",", 0);
						free(p);
						goto next;
					}
				}
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
	if (show_latest) goto out;

	if (platforms) {	/* didn't find this platform */
		uniqLines(platforms, free);
		if (streq(platform, "?")) {
			printf("Available architectures for %s:\n", version);
			EACH(platforms) printf("  %s\n", platforms[i]);
			rc = 0;
		} else if (bininstaller) {
			if (fetchonly || !bundle) {
				/* it's ok to just fetch the old installer or
				 * to use an old installer on an old install */
				freeLines(data, free);
				data = bininstaller;
				goto proceed;
			}
			notice("upgrade-pre-bundle", bundle, "-e");
			rc = 1;
		} else {
			fprintf(stderr,
			    "No upgrade for the arch %s found. "
			    "Available architectures for %s:\n",
			    platform, version);
			EACH(platforms) fprintf(stderr, "  %s\n", platforms[i]);
			rc = 2;
		}
		freeLines(platforms, free);
		goto out;
	}
proceed:
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
	oldversion = bk_vers;
	bk_vers = data[3];

	bk_vers = oldversion;
	unless (fetchonly || install) {
		printf("BitKeeper version %s is available for download.\n",
		    data[3]);
		printf("Run\n"
		    "\tbk upgrade\t# to download and install the new bk\n"
		    "\tbk upgrade -d\t# to download bk without installing\n");
		rc = 0;
		goto out;
	}

	if (upgrade_fetch(data[1], data[1])) {
		fprintf(stderr, "upgrade: unable to fetch %s\n", data[1]);
		goto out;
	}

	/* find checksum of the file we just fetched */
	f = fopen(data[1], "r");
	p = hashstream(fileno(f));
	assert(p);
	rewind(f);
	unless (streq(p, data[2])) {
		fprintf(stderr, "upgrade: file %s fails to match checksum\n",
		    data[1]);
		fclose(f);
		free(p);
		goto out;
	}
	fclose(f);
	free(p);

	myumask = umask(0);
	umask(myumask);
	chmod(data[1], 0555 & ~myumask);

	if (fetchonly) {
		printf("New version of bk fetched: %s\n", data[1]);
		rc = 0;
		goto out;
	}
	putenv("BK_NOLINKS=1");	/* XXX -u already does this */
#ifdef WIN32
	if (runas(data[1], "-u", 0)) {
		fprintf(stderr, "upgrade: install failed\n");
		goto out;
	}
#else
	if (macosx()) {
		sprintf(buf, "/usr/bin/open -W %s", data[1]);
	} else {
		sprintf(buf, "./%s -u", data[1]);
	}
	if (system(buf)) {
		fprintf(stderr, "upgrade: install failed\n");
		goto out;
	}
#endif
	unlink(data[1]);
	rc = 0;
 out:
	if (version) free(version);
	if (bundle) free(bundle);
	if (data) freeLines(data, free);
	return (rc);
}

/*
 * verify that the current user can replace all files at target
 */
private	int
noperms(char *target)
{
	struct	stat sb, sbdir;
	char	*test_file;
	int	rc = 1;

	/*
	 * Assumes subdirs are ok.
	 */
	sbdir.st_mode = 0;
	unless (test_file = aprintf("%s/upgrade_test.tmp", target)) return (1);
	if (touch(test_file, 0644)) {
		/* can't create file in dir, try change dir perms */
		if (lstat(target, &sbdir)) goto out;
		if (chmod(target, 0775)) goto out;
		if (touch(test_file, 0644)) goto out;
	}
	if (lstat(test_file, &sb)) goto out;
	if (unlink(test_file)) goto out;
	rc = 0;
out:
	if (sbdir.st_mode) chmod(target, sbdir.st_mode); /* restore perms */
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

/*
 * Every day look at the official upgrade area and find the latest bk version.
 * Store the result in `bk dotbk`/latest-bkver
 *
 * Returns -1 if check failed.
 */
int
upgrade_latestVersion(char *new_vers, char *new_utc)
{
	FILE	*f;
	char	*t, *p;
	int	rc = -1;
	char	buf[MAXPATH], new[MAXPATH];

	*new_vers = *new_utc = 0;
	concat_path(buf, getDotBk(), "latest-bkver");
	sprintf(new, "%s.tmp", buf);
	if (time(0) - mtime(buf) > DAY) {
		if (sysio(0, new, DEVNULL_WR,
			"bk", "upgrade", "--show-latest", SYS)) {
			unlink(new);
			return (-1);
		}
		if (rename(new, buf)) perror(new);
	}
	if (f = fopen(buf, "r")) {
		if ((t = fgetline(f)) && (p = strchr(t, ','))) {
			*p++ = 0;
			assert(strlen(t) < 32);
			assert(strlen(p) < 16);
			strcpy(new_vers, t);
			strcpy(new_utc, p);
			rc = 0;
		}
		fclose(f);
	}
	return (rc);
}

/*
 * Tell the user about new versions of bk.
 */
void
upgrade_maybeNag(char *out)
{
	FILE	*f;
	char	*t, *new_age, *bk_age;
	int	same, i;
	time_t	now = time(0);
	char	*av[] = {
		"bk", "prompt", "-io", 0, 0
	};
	int	ac = 3;	/* first 0 above */
	char	new_vers[65];
	char	new_utc[16];
	char	buf[MAXLINE];

	/*
	 * bk help may go through here twice, if we are in a GUI, skip
	 * this the first time.
	 */
	if (out && getenv("BK_GUI")) return;


	/*
	 * Give people a way to give customers to disable this
	 */
	if (getenv("BK_NEVER_NAG")) return;
	if (cfg_bool(0, CFG_UPGRADE_NONAG)) return;

	/* a new bk is out */
	if (upgrade_latestVersion(new_vers, new_utc)) return;
	if (getenv("_BK_ALWAYS_NAG")) goto donag;
	if (strcmp(new_utc, bk_utc) <= 0) return;

	/*
	 * Wait for the new bk to be out for a while, unless we are a
	 * beta version.
	 */
	if (!strstr(bk_vers, "-beta-") &&
	    ((now - sccs_date2time(bk_utc, 0)) > MONTH) &&
	    ((now - sccs_date2time(new_utc, 0)) < MONTH)) {
		return;
	}

	/* We can only nag once a month */
	concat_path(buf, getDotBk(), "latest-bkver.nag");
	if ((now - mtime(buf)) < MONTH) {
		/* make sure we nagged for the same thing */
		t = loadfile(buf, 0);
		sprintf(buf, "%s,%s\n", bk_utc, new_utc);
		same = streq(buf, t);
		free(t);
		if (same) return;
	}

	/* looks like we need to nag */

	/* remember that we did */
	concat_path(buf, getDotBk(), "latest-bkver.nag");
	Fprintf(buf, "%s,%s\n", bk_utc, new_utc);


donag:	/* okay, nag */

	/* age uses a staic buffer */
	new_age = strdup(age(now - sccs_date2time(new_utc, 0), " "));
	bk_age = strdup(age(now - sccs_date2time(bk_utc, 0), " "));
	av[ac] = aprintf("BitKeeper %s (%s) is out, it was released %s ago.\n"
	    "You are running version %s (%s) released %s ago.\n\n"
	    "If you want to upgrade, please run bk upgrade.\n"
	    "Or set upgrade_nonag:yes config to not see these messages",
	    new_vers, new_utc, new_age,
	    bk_vers, bk_utc, bk_age);
	if (out) {
		if (f = fopen(out, "w")) {
			fprintf(f, "%s\n", av[ac]);
			for (i = 0; i < 79; ++i) fputc('=', f);
			fputc('\n', f);
			fclose(f);
		}
	} else {
		putenv("BK_NEVER_NAG=1");
		spawnvp(_P_DETACH, av[0], av);
	}
	free(av[ac]);
	free(new_age);
	free(bk_age);
	return;
}
