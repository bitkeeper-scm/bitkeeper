#include "system.h"
#include "sccs.h"
#include "logging.h"

private int	mkconfig(FILE *out, MDBM *flist, int verbose);
private void    usage(void);
private void	defaultFiles(int);
private void	printField(FILE *out, MDBM *flist, char *field);
private MDBM	*addField(MDBM *flist, char *field);
private	int	licensebad(MDBM *m);

int
setup_main(int ac, char **av)
{
	int	force = 0, allowNonEmptyDir = 0, accept = 0, c;
	char	*package_path = 0, *config_path = 0;
	char	buf[MAXLINE], my_editor[1024];
	char	here[MAXPATH];
	char 	s_config[] = "BitKeeper/etc/SCCS/s.config";
	char 	config[] = "BitKeeper/etc/config";
	project	*in_prod = 0;
	sccs	*s;
	MDBM	*m = 0, *flist = 0;
	FILE	*f, *f1;
	int	status;
	int	print = 0;
	int	product = 0;
	int	noCommit = 0;

	while ((c = getopt(ac, av, "aCc:ePfF:p")) != -1) {
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
			config_path = strdup(fullname(optarg));
			break;
		    case 'P': product = 1; break;
		    case 'e': allowNonEmptyDir = 1; break;
		    case 'f': force = 1; break;
		    case 'F': flist = addField(flist, optarg); break;
		    case 'p': print = 1; break;
		    default: usage();
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

	unless (package_path = av[optind]) {
		printf("Usage: bk setup [-c<config file>] directory\n");
		exit (0);
	}
	if (exists(package_path) && !allowNonEmptyDir) {
		printf("bk: %s exists already, setup fails.\n", package_path);
		exit (1);
	}
	unless (force) {
		getMsg("setup_1", 0, '-', stdout);
		printf("Create new package? [no] ");
		if (fgets(buf, sizeof(buf), stdin) == NULL) buf[0] = 'n';
		if ((buf[0] != 'y') && (buf[0] != 'Y')) exit (0);
	}
	unless (getcwd(here, sizeof here)) {
		perror("getcwd");
		exit(1);
	}
	if (mkdirp(package_path)) {
		perror(package_path);
		exit(1);
	}
	if (chdir(package_path) != 0) {
		perror(package_path);
		exit(1);
	}
	if (allowNonEmptyDir && exists(BKROOT)) {
		printf("bk: %s repository exists already, setup fails.\n",
		package_path);
	}
	sccs_mkroot(".");
	unless (product) {
		/*
		 * in_prod will be used later - it is only set if
		 * creating a component inside of a product.
		 */
		if (in_prod = proj_product(0)) {
			/*
			 * null config file is okay if in product
			 * as the component will use the product's config
			 * Create an empty one instead of none, both for
			 * sane, and for limiting converge operations.
			 */
			unless (config_path) config_path = strdup(DEVNULL_RD);
		}
	}
	if (config_path == 0) {
		getMsg("setup_3", 0, '-', stdout);
		/* notepad.exe wants text mode */
		f = fopen("BitKeeper/etc/config", "wt");
		assert(f);
		mkconfig(f, flist, 1);
		fclose(f);
		if (flist) mdbm_close(flist);
		chmod("BitKeeper/etc/config", 0664);
again:		
		/*
		 * Yes, I know this is ugly but it replaces a zillion error
		 * path gotos below.
		 */
		if (config_path) goto err;
		printf("Editor to use [%s] ", editor);
		unless (fgets(my_editor, sizeof(my_editor), stdin)) {
			my_editor[0] = '\0';
		}
		chop(my_editor);
		if (my_editor[0] != 0) {
			sprintf(buf, "%s BitKeeper/etc/config", my_editor);
		} else {
			sprintf(buf, "%s BitKeeper/etc/config", editor);
		}
		system(buf);
	} else {
		unless (f1 = fopen(config_path, "rt")) {
			fprintf(stderr, "setup: can't open %s\n", config_path);
			fprintf(stderr, "You need to use a fullpath\n");
			exit(1);
	    	}
		f = fopen(config, "w");
		assert(f);
		while (fnext(buf, f1)) printField(f, flist, buf);
		fclose(f);
		fclose(f1);
	}

	unless (m = loadConfig(in_prod, 1)) {
		fprintf(stderr, "No config file found\n");
		exit(1);
	}

	/* When eula_name() stopped returning bkl on invalid signatures
	 * we needed this to force a good error message.
	 */
	if (mdbm_fetch_str(m, "license") && licensebad(m)) {
		if (config_path) {
err:			unlink("BitKeeper/etc/config");
			unlink("BitKeeper/log/cmd_log");
			if (m) mdbm_close(m);
			sccs_unmkroot("."); /* reverse  sccs_mkroot */
			unless (allowNonEmptyDir) {
				chdir(here);
				rmdir(package_path);
			}
			exit(1);
		}
		goto again;
	}
	mdbm_close(m);
	m = 0;

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
	}

	if (cset_setup(SILENT)) goto err;
	unless (eula_accept(accept ? EULA_ACCEPT : EULA_PROMPT, 0)) exit(1);
	s = sccs_init(s_config, SILENT);
	assert(s);
	sccs_delta(s, SILENT|NEWFILE, 0, 0, 0, 0);
	s = sccs_restart(s);
	assert(s);
	sccs_get(s, 0, 0, 0, 0, SILENT|GET_EXPAND, 0);
	sccs_free(s);
	defaultFiles(product);

	status = sys("bk", "commit", "-qFyInitial repository create", SYS);
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
	if (in_prod) {		/* we are a new component, attach ourself */
		status = sys("bk", "attach",
		    noCommit ? "-qC" : "-q", ".", SYS);
		unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
			fprintf(stderr, "setup: bk attach failed.\n");
			return (1);
		}
	}
	if (config_path) free(config_path);
	return (0);
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
		f = fopen("BitKeeper/etc/aliases", "w");
		fprintf(f, "@default\nall\n");
		fclose(f);
		system("bk new -Pq BitKeeper/etc/aliases");
		f = fopen("BitKeeper/log/COMPONENTS", "w");
		fprintf(f, "default\n");
		fclose(f);
		touch("BitKeeper/log/PRODUCT", 0444);
	}
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

private void
usage(void)
{
	system("bk help -s setup");
	exit(1);
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

private void
printField(FILE *out, MDBM *flist, char *field)
{
	char	*p, *val;

	unless (flist) {
use_default:	fputs(field, out);
		return;
	}

	unless (p = strrchr(field, ':')) goto use_default;
	*p = 0;
	unless (val = mdbm_fetch_str(flist, field)) goto use_default;

	/*
	 * If we get here, user wants to override the default value
	 */
	fprintf(out, "%s: %s\n", field, val);
	mdbm_delete_str(flist, field);
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
	char	buf[1000], pattern[200];

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

	val = flist ? mdbm_fetch_str(flist, "compression") : 0;
	/* force compression to default on */
	unless (val && *val) {
		char fld[] =  "compression=gzip";
		flist = addField(flist, fld);
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
		char fld[] =  "BAM=no";
		flist = addField(flist, fld);
	}

	/*
	 * Now print the help message for each config entry
	 */
	while (fgets(buf, sizeof(buf), in)) {
		if (first && (buf[0] == '#')) continue;
		first = 0;
		if (streq("$\n", buf)) break;
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

private int
licensebad(MDBM *m)
{
	hash	*req = hash_new(HASH_MEMHASH);
	char	*err;
	int	rc = !getlicense(m, req);

	if (err = hash_fetchStr(req, "ERROR")) lease_printerr(err);
	hash_free(req);

	return (rc);
}
