/*
 * Copyright 1999-2011,2013-2016 BitMover, Inc
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
#include "nested.h"
#include "cfg.h"

private MDBM	*mkconfig(FILE *out, MDBM *flist, int verbose);
private void	defaultFiles(int);
private MDBM	*addField(MDBM *flist, char *field, int replace);

int
setup_main(int ac, char **av)
{
	int	c;
	char	*package_path = 0, *config_path = 0;
	char	buf[MAXLINE];
	char	here[MAXPATH];
	char 	s_config[] = "BitKeeper/etc/SCCS/s.config";
	char 	config[] = "BitKeeper/etc/config";
	project	*in_prod = 0;
	project	*proj;
	sccs	*s;
	MDBM	*m = 0, *flist = 0;
	int	status;
	int	print = 0, product = 0, noCommit = 0,
		sccs_compat = 0, verbose = 0;
	filecnt	nf;
	longopt	lopts[] = {
		{ "sccs-compat", 300 }, /* old compat option */
		{ "compat", 300 },	/* create compatible repo */
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "aCc:ePfF:pv", lopts)) != -1) {
		switch (c) {
		    case 'a': break; /* ignored */
		    case 'C':
			noCommit = 1;
			break;
		    case 'c':
			unless(exists(optarg)) {
				fprintf(stderr,
				    "setup: %s doesn't exist. Exiting\n",
				    optarg);
				exit(1);
			}
			localName2bkName(optarg, optarg);
			config_path = fullname(optarg, 0);
			break;
		    case 'P': product = 1; break;
		    case 'e': break;
		    case 'f': break;
		    case 'F': flist = addField(flist, optarg, 1); break;
		    case 'p': print = 1; break;
		    case 'v': verbose = 1; break;
		    case 300: sccs_compat = 1;  break;
		    default: bk_badArg(c, av);
		}
	}
	if (noCommit && product) {
		fprintf(stderr, "setup: can't use -C and -P together.\n");
		exit(1);
	}
	if (print) {
		if (config_path) {
			fprintf(stderr, "setup: can't mix -c and -p.\n");
			exit(1);
		}
		if (flist = mkconfig(stdout, flist, verbose)) {
			mdbm_close(flist);
			flist = 0;
		}
		exit(0);
	}

	unless (package_path = av[optind]) package_path = ".";

	if (streq(package_path, "") || streq(package_path, "-")) {
		usage();
		return (1);
	}
	unless (getenv("BK_REGRESSION")) sccs_compat = 0;

	/*
	 * If in a repo, be it product, component or standalone, see
	 * that there are no sfiles from package_path on down to next repo.
	 */
	if ((proj = proj_init(package_path)) && isdir(package_path)) {
		FILE	*f;
		int	any = 0;

		sprintf(buf, "bk --cd='%s' gfiles -d", package_path);
		if (f = popen(buf, "r")) {
			while (fread(buf, 1, sizeof(buf), f)) any = 1;
			pclose(f); /* ignore errors */
		}
		if (any) {
			fprintf(stderr,
			    "The directory '%s' has existing "
			    "version control files in or under it.\n"
			    "Aborting\n", package_path);
			proj_free(proj);
			return (1);
		}
	}
	if (sccs_compat) {
		if (!product && proj_product(proj)) {
			fprintf(stderr,
			    "%s: can't use --compat inside a product.\n",
			    prog);
			exit(1);
		}
		proj_remapDefault(0);
	}

	unless (product) {
		/*
		 * in_prod will be used later - it is only set if
		 * creating a component inside of a product.
		 */
		if (proj && (in_prod = proj_product(proj))) {
			/*
			 * null config file is okay if in product
			 * as the component will use the product's config
			 * Create an empty one instead of none, both for
			 * sane, and for limiting converge operations.
			 */
			unless (config_path) config_path = strdup(DEVNULL_RD);
			/*
			 * attach will fail if not a portal, catch this early
			 * and clean up
			 */
			unless(nested_isPortal(in_prod)) {
				fprintf(stderr, "New components can only be "
				    "created in a portal. "
				    "See 'bk help portal'.\n");
				return (1);
			}
		}
	}
	strcpy(here, proj_cwd());
	if (mkdirp(package_path)) {
		perror(package_path);
		exit(1);
	}
	/*
	 * work around wrlock assert since we don't have a repo to
	 * lock yet
	 */
	safe_putenv("_BK_WR_LOCKED=%u", getpid());
	sccs_mkroot(package_path);
	if (chdir(package_path) != 0) {
		perror(package_path);
		exit(1);
	}
	if (in_prod) {
		/* add local features file until the product file takes over */
		features_set(0, FEAT_BKFILE,
		    features_test(in_prod, FEAT_BKFILE));
		features_set(0, FEAT_BWEAVE,
		    features_test(in_prod, FEAT_BWEAVE));
		features_set(0, FEAT_BWEAVEv2,
		    features_test(in_prod, FEAT_BWEAVEv2));
		features_set(0, FEAT_BKMERGE,
		    features_test(in_prod, FEAT_BKMERGE));
	} else {
		if (sccs_compat) {
			features_set(0, FEAT_FILEFORMAT|FEAT_SCANDIRS, 0);
		} else {
			features_set(0,
			    (FEAT_FILEFORMAT & ~FEAT_BWEAVEv2) | FEAT_SCANDIRS,
			    1);
		}
	}

	if (config_path) {
		if (fileCopy(config_path, config)) exit(1);
	} else {
		FILE	*f;

		f = fopen(config, "w");
		assert(f);
		if (flist = mkconfig(f, flist, 1)) {
			mdbm_close(flist);
			flist = 0;
		}
		fclose(f);
	}
	unless (m = loadConfig(in_prod, 1)) {
		fprintf(stderr, "No config file found\n");
		exit(1);
	}

	if (in_prod) {
		chdir(package_path);

		/*
		 * make this appear to be a component from the start
		 * so the initial ChangeSet path will be correct
		 */
		nested_makeComponent(".");
	}

	features_set(0, FEAT_REMAP, !proj_hasOldSCCS(0));
	if (cset_setup(SILENT)) goto err;
	s = sccs_init(s_config, SILENT);
	assert(s);
	putenv("_BK_MV_OK=1");
	sccs_delta(s, SILENT|DELTA_NEWFILE, 0, 0, 0, 0);
	s = sccs_restart(s);
	assert(s);
	sccs_get(s, 0, 0, 0, 0, SILENT|GET_EXPAND, s->gfile, 0);
	sccs_free(s);
	defaultFiles(product);

	status = sys("bk", "commit", "-S", "-qFyInitial repository create", SYS);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		fprintf(stderr, "setup: bk commit failed.\n");
		return (1);
	}
	if (proj_cd2root()) {
		fprintf(stderr, "setup: cannot find package root.\n");
		return (1);
	}
	enableFastPendingScan();
	nf.tot = product ? NFILES_PROD : NFILES_SA;
	nf.usr = 1;
	if (in_prod) {		/* we are a new component, attach ourself */
		unlink("BitKeeper/log/COMPONENT");
		status = sys("bk", "attach",
		    noCommit ? "-qC" : "-q", "-N", ".", SYS);
		unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
			fprintf(stderr, "setup: bk attach failed.\n");
			return (1);
		}
		nf.tot = NFILES_COMP;
	}
	proj_reset(0);	/* acts as proj_free(proj) */
	repo_nfilesUpdate(&nf);	/* update product level nfiles */
	if (config_path) free(config_path);
	return (0);
err:	unlink("BitKeeper/etc/config");
	unlink("BitKeeper/log/cmd_log");
	unlink("BitKeeper/log/features");
	if (m) mdbm_close(m);
	if (flist) mdbm_close(flist);
	sccs_unmkroot("."); /* reverse  sccs_mkroot */
	proj_reset(0);	/* acts as proj_free(proj) */
	unless (streq(package_path, "."))  {
		chdir(here);
		if (rmtree(package_path)) perror(package_path);
	}
	return (1);
}

private void
defaultFiles(int product)
{
	FILE	*f;

	f = fopen("BitKeeper/etc/ignore", "w");
	fprintf(f, "\n");
	fclose(f);
	system("bk new -Pq BitKeeper/etc/ignore");
	if (product) {
		touch(ALIASES, 0664);
		system("bk new -Pq " ALIASES);
		f = fopen("BitKeeper/log/HERE", "w");
		fprintf(f, "ALL\nPRODUCT\n");
		fclose(f);
		touch("BitKeeper/log/PRODUCT", 0444);
		system("bk portal -q .");
		system("bk gate -q .");
		proj_reset(0);		/* created product */
	}
	attr_update();	/* XXX: if < 0, failed, but just smile and go on */
	unless (getenv("_BK_SETUP_NOGONE")) {
		f = fopen("BitKeeper/etc/gone", "w");
		fprintf(f, "\n");
		fclose(f);
		system("bk new -Pq BitKeeper/etc/gone");
	}
	f = fopen(COLLAPSED, "w");
	fprintf(f, "\n");
	fclose(f);
	system("bk new -Pq " COLLAPSED);
}

private MDBM *
addField(MDBM *flist, char *field, int replace)
{
	char	*p;

	unless (flist) flist = mdbm_mem();

	p = strchr(field, '=');
	unless (p) {
		fprintf(stderr,
		    "setup: Cannot find assignment operator \"=\"\n"
		    "Bad -F option \"%s\", ignored\n", field);
		return (flist);
	}
	*p++ = 0;
	if (strchr(field, '[')) {
		p[-1] = '=';
		fprintf(stderr,
		    "setup: filters not supported; \"%s\" ignored\n", field);
		return (flist);
	}
	field = cfg_alias(field);
	mdbm_store_str(flist, field, p, replace ? MDBM_REPLACE : MDBM_INSERT);
	return (flist);
}

/*
 * Return a file pointer to the config template, if we can find one.
 */
private FILE	*
config_template(void)
{
	FILE	*f;
	char	*dotbk = getDotBk();
	char	path[MAXPATH];

	if (dotbk) {
		sprintf(path, "%s/config.template", dotbk);
		if (f = fopen(path, "rt")) return (f);
	}
	sprintf(path, "%s/BitKeeper/etc/config.template", globalroot());
	if (f = fopen(path, "rt")) return (f);
	sprintf(path, "%s/etc/config.template", bin);
	if (f = fopen(path, "rt")) return (f);
	return (0);
}

private MDBM *
mkconfig(FILE *out, MDBM *flist, int verbose)
{
	FILE	*in;
	char	*p;
	char	**keys = 0;
	datum	k;
	int	i, idx;
	char	*def;
	char	buf[1000], pattern[200];

	if (in = config_template()) {
		while (fnext(buf, in)) {
			/*
			 * XXX: parseConfigKV(buf, 1, *key, &p)
			 * might do a better job of parsing
			 */
			if (buf[0] == '#') continue;
			unless (p = strrchr(buf, ':')) continue;
			chop(p);
			for (*p++ = 0; *p && isspace(*p); p++);
			unless (*p) continue;
			p = aprintf("%s=%s", buf, p);
			flist = addField(flist, p, 0);
			free(p);
		}
		fclose(in);
	}

	if (verbose) {
		getMsgP("config_preamble", 0, "# ", 0, out);
	}


	unless (flist) flist = mdbm_mem();
	cfg_loadSetup(flist);

	/*
	 * Now print the help message for each config entry
	 */
	EACH_KEY(flist) keys = addLine(keys, k.dptr);
	sortLines(keys, stringcase_sort);
	EACH(keys) {
		char	*key = keys[i];
		char	*val = mdbm_fetch_str(flist, key);
		char	**bkargs = 0;
		char	*comment = "";

		if (verbose) {
			fputc('\n', out);
			def = 0;
			comment = "";
			if ((idx = cfg_findVar(key)) >= 0) {
				def = cfg_def(idx);
			}
			unless (def) {
				def = "empty";
				comment = "# ";
			}
			sprintf(pattern, "config_%s", key);
			unless (getMsgP(pattern, def, "# ", 0, out)) {
				bkargs = addLine(bkargs, key);
				bkargs = addLine(bkargs, def);
				getMsgv("config_undoc", bkargs, "# ", 0, out);
				freeLines(bkargs, 0);
				bkargs = 0;
			}
		}
		fprintf(out, "%s%s: %s\n", comment, key, val);
	}
	freeLines(keys, 0);
	return (flist);
}
