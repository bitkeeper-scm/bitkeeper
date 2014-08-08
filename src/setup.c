#include "system.h"
#include "sccs.h"
#include "logging.h"
#include "nested.h"

private int	mkconfig(FILE *out, MDBM *flist, int verbose);
private void	defaultFiles(int);
private MDBM	*addField(MDBM *flist, char *field);

int
setup_main(int ac, char **av)
{
	int	accept = 0, c;
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
	int	print = 0;
	int	product = 0;
	int	noCommit = 0;
	int	sccs_compat = 0;
	filecnt	nf;
	longopt	lopts[] = {
		{ "sccs-compat", 300 }, /* old compat option */
		{ "compat", 300 },	/* create compatible repo */
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "aCc:ePfF:p", lopts)) != -1) {
		switch (c) {
		    case 'a': accept = 1; break;
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
		    case 'F': flist = addField(flist, optarg); break;
		    case 'p': print = 1; break;
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
		mkconfig(stdout, flist, 0);
		if (flist) mdbm_close(flist);
		exit(0);
	}

	unless (package_path = av[optind]) package_path = ".";

	if (streq(package_path, "") || streq(package_path, "-")) {
		usage();
		return (1);
	}

	/*
	 * If in a repo, be it product, component or standalone, see
	 * that there are no sfiles from package_path on down to next repo.
	 */
	if ((proj = proj_init(package_path)) && isdir(package_path)) {
		FILE	*f;
		int	any = 0;

		sprintf(buf, "bk --cd='%s' sfiles -d", package_path);
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
	} else {
		features_set(0,
		    FEAT_BKFILE|FEAT_BWEAVE|FEAT_SCANDIRS, !sccs_compat);
	}

	unless (config_path) config_path = strdup(DEVNULL_RD);

	if (fileCopy(config_path, config)) exit(1);

	unless (m = loadConfig(in_prod, 1)) {
		fprintf(stderr, "No config file found\n");
		exit(1);
	}

	if (product && bk_notLicensed(0, LIC_SAM, 0)) goto err;

	/*
	 * When creating a new component we need a valid license, but
	 * while the component is being created it is a new
	 * repository, but not yet a component.  So the product's
	 * config file is not visible and if the only license is in
	 * the product bk won't see a license.  To work around this we
	 * make sure we have a valid write lease before proceeding.
	 * We don't check the exit status from lease renew as we may be
	 * disconnected with a valid write lease already.
	 */
	if (in_prod) {
		chdir(proj_root(in_prod));
		sys("bk", "lease", "renew", "-qw", SYS);
		chdir(here);
		chdir(package_path);

		/*
		 * make this appear to be a component from the start
		 * so the initial ChangeSet path will be correct
		 */
		nested_makeComponent(".");
		if (features_test(0, FEAT_BKFILE)) {
			unlink("BitKeeper/log/features");
		}
	}

	features_set(0, FEAT_REMAP, !proj_hasOldSCCS(0));
	if (cset_setup(SILENT)) goto err;
	unless (eula_accept(accept ? EULA_ACCEPT : EULA_PROMPT, 0)) exit(1);
	s = sccs_init(s_config, SILENT);
	assert(s);
	putenv("_BK_MV_OK=1");
	sccs_delta(s, SILENT|NEWFILE, 0, 0, 0, 0);
	s = sccs_restart(s);
	assert(s);
	sccs_get(s, 0, 0, 0, 0, SILENT|GET_EXPAND, 0);
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
	logChangeSet();
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
addField(MDBM *flist, char *field)
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
	mdbm_store_str(flist, field, p, MDBM_REPLACE);
	return (flist);
}

/*
 * Return a file pointer to the config template, if we can find one.
 */
private FILE	*
config_template(void)
{
	FILE	*f;
	char	*home = getenv("HOME");
	char	path[MAXPATH];

	/* don't look for templates during regressions, that will hose us */
	if (getenv("BK_REGRESSION")) return (0);

	if (home) {
		sprintf(path, "%s/.bk/config.template", home);
		if (f = fopen(path, "rt")) return (f);
	}
	sprintf(path, "%s/BitKeeper/etc/config.template", globalroot());
	if (f = fopen(path, "rt")) return (f);
	sprintf(path, "%s/etc/config.template", bin);
	if (f = fopen(path, "rt")) return (f);
	return (0);
}

private int
mkconfig(FILE *out, MDBM *flist, int verbose)
{
	FILE	*in;
	int	found = 0;
	int	first = 1;
	char	*p, *val;
	kvpair	kv;
	char	*key;
	u32	bits = 0;
	char	buf[1000], pattern[200];

	/* get a lease, but don't fail */
	if (key = lease_bkl(0, 0)) {
		bits = license_bklbits(key);
		free(key);
	}
	if (in = config_template()) {
		while (fnext(buf, in)) {
			if (buf[0] == '#') continue;
			unless (p = strrchr(buf, ':')) continue;
			chop(p);
			for (*p++ = 0; *p && isspace(*p); p++);
			unless (*p) continue;
			/* command line stuff overrides */
			if (flist && mdbm_fetch_str(flist, buf)) {
				continue;
			}
			p = aprintf("%s=%s", buf, p);
			flist = addField(flist, p);
			free(p);
		}
		fclose(in);
	}

	sprintf(buf, "%s/bkmsg.txt", bin);
	unless (in = fopen(buf, "rt")) {
		fprintf(stderr, "Unable to open %s\n", buf);
		return (-1);
	}
	if (verbose) {
		getMsgP("config_preamble", 0, "# ", 0, out);
		fputs("\n", out);
	}

	/*
	 * look for config template
	 */
	while (fgets(buf, sizeof(buf), in)) {
		if (streq("#config_template\n", buf)) {
			found = 1;
			break;
		}
	}
	unless (found) {
		fclose(in);
		return (-1);
	}

	val = flist ? mdbm_fetch_str(flist, "autofix") : 0;
	/* force autofix to default on */
	unless (val && *val) {
		char fld[] =  "autofix=yes";
		flist = addField(flist, fld);
	}

	val = flist ? mdbm_fetch_str(flist, "BAM") : 0;
	/* force BAM to default off, it's licensed */
	unless (val && *val) {
		char	fld[100];

		sprintf(fld, "%s", (bits & LIC_BAM) ? "BAM=on" : "BAM=off");
		flist = addField(flist, fld);
	}

	val = flist ? mdbm_fetch_str(flist, "checkout") : 0;
	/* force checkout to default edit */
	unless (val && *val) {
		char fld[] =  "checkout=get";
		flist = addField(flist, fld);
	}

	val = flist ? mdbm_fetch_str(flist, "clock_skew") : 0;
	/* force checkout to default edit */
	unless (val && *val) {
		char fld[] =  "clock_skew=on";
		flist = addField(flist, fld);
	}

	val = flist ? mdbm_fetch_str(flist, "compression") : 0;
	/* force compression to default on */
	unless (val && *val) {
		char fld[] =  "compression=gzip";
		flist = addField(flist, fld);
	}

	val = flist ? mdbm_fetch_str(flist, "partial_check") : 0;
	/* force compression to default on */
	unless (val && *val) {
		char fld[] =  "partial_check=on";
		flist = addField(flist, fld);
	}

	/*
	 * Now print the help message for each config entry
	 */
	while (fgets(buf, sizeof(buf), in)) {
		if (first && (buf[0] == '#')) continue;
		first = 0;
		if (streq("$\n", buf)) break;

		/*
		 * If we found a license somewhere just use that, it's
		 * probably what they want.  If that is wrong they can
		 * edit the config file later.
		 *
		 * Nota bene: we add some other config that starts w/ "lic"
		 * and this is busted.
		 */
		if (bits && strneq(buf, "lic", 3)) continue;

		chop(buf);
		if (verbose) {
			sprintf(pattern, "config_%s", buf);
			getMsgP(pattern, 0, "# ", 0, out);
		}
		if (flist && (val = mdbm_fetch_str(flist, buf))) {
			fprintf(out, "%s: %s\n", buf, val);
			mdbm_delete_str(flist, buf);
		} else {
			fprintf(out, "%s: \n", buf);
		}
	}
	fclose(in);

	unless (flist) return (0);

	/*
	 * Append user supplied field which have no overlap in template file
	 */
	for (kv = mdbm_first(flist); kv.key.dsize; kv = mdbm_next(flist)) {
		fprintf(out, "%s: %s\n", kv.key.dptr, kv.val.dptr);
	}
	return (0);
}
