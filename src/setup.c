#include "system.h"
#include "sccs.h"
#include "logging.h"

extern char *editor, *pager, *bin;
private int	mkconfig(FILE *out, MDBM *flist);
private void    usage(void);
private void	defaultIgnore(void);
private void	printField(FILE *out, MDBM *flist, char *field);
private MDBM	*addField(MDBM *flist, char *field);

int
setup_main(int ac, char **av)
{
	int	force = 0, allowNonEmptyDir = 0, ask = 1, c;
	char	*package_path = 0, *config_path = 0, *t;
	char	buf[MAXLINE], my_editor[1024], setup_files[MAXPATH];
	char	here[MAXPATH];
	char 	s_config[] = "BitKeeper/etc/SCCS/s.config";
	char 	config[] = "BitKeeper/etc/config";
	sccs	*s;
	MDBM	*m, *flist = 0;
	FILE	*f, *f1;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help setup");
		return (0);
	}
	while ((c = getopt(ac, av, "ac:efF:")) != -1) {
		switch (c) {
		    case 'c':					/* doc 2.0 */
		    	unless(exists(optarg)) {
				fprintf(stderr, 
				    "setup: %s doesn't exist. Exiting\n",
				    optarg);
				exit(1);
			}
			localName2bkName(optarg, optarg);
			config_path = fullname(optarg, 0);
			break;
		    case 'e':					/* to be doc*/
			allowNonEmptyDir = 1;
			break;
		    case 'f':					/* doc 2.0 */
			force = 1;
			break;
		    case 'F':
			flist = addField(flist, optarg);
			break;
		    case 'a':
			ask = 0;	/* don't ask */
			break;
		    default:
			usage();
		}
	}
	unless (package_path = av[optind]) {
		printf("Usage: bk setup [-c<config file>] directory\n");
		exit (0);
	}
	if (!allowNonEmptyDir && exists(package_path)) {
		printf("bk: %s exists already, setup fails.\n", package_path);
		exit (1);
	}
	license();
	unless(force) {
		getMsg("setup_1", 0, 0, stdout);
		printf("Create new package? [no] ");
		if (fgets(buf, sizeof(buf), stdin) == NULL) buf[0] = 'n';
		if ((buf[0] != 'y') && (buf[0] != 'Y')) exit (0);
	}
	if (!getcwd(here, sizeof here)) {
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
	if (config_path == NULL) {
		FILE 	*f;

		getMsg("setup_3", 0, 0, stdout);
		/* notepad.exe wants text mode */
		f = fopen("BitKeeper/etc/config", "wt");
		assert(f);
		mkconfig(f, flist);
		fclose(f);
		if (flist) mdbm_close(flist);
		chmod("BitKeeper/etc/config", 0664);
again:		printf("Editor to use [%s] ", editor);
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
		f = fopen(config, "wb");
		assert(f);
		while (fnext(buf, f1)) printField(f, flist, buf);
		fclose(f);
		fclose(f1);
	}

	unless (m = loadConfig(".")) {
		fprintf(stderr, "No config file found\n");
		exit(1);
	}
	unless (mdbm_fetch_str(m, "description")) {
		fprintf(stderr, "Setup: must provide a description.\n");
		if (config_path) {
err:			unlink("BitKeeper/etc/config");
			sccs_unmkroot("."); /* reverse  sccs_mkroot */
			unless (allowNonEmptyDir) {
				chdir(here);
				rmdir(package_path);
			}
			exit(1);
		}
		goto again;
	}

	unless (mdbm_fetch_str(m, "logging")) {
		fprintf(stderr, "Setup: must define logging policy.\n");
		if (config_path) goto err;
		goto again;
	}
	unless (t = mdbm_fetch_str(m, "email")) {
		fprintf(stderr, "Setup: must define email contact.\n");
		if (config_path) goto err;
		goto again;
	}
	unless (t = strchr(t, '@')) {
		fprintf(stderr, "Setup: must define a valid email contact.\n");
		if (config_path) goto err;
		goto again;
	}
	unless (t = strchr(t, '.')) {
		fprintf(stderr, "Setup: must define a valid email contact.\n");
		if (config_path) goto err;
		goto again;
	}
	if ((t = mdbm_fetch_str(m, "single_user")) && strchr(t, '@')) {
		fprintf(stderr, "Setup: single_user should not have a hostname.\n");
		if (config_path) goto err;
		goto again;
	}
	if ((mdbm_fetch_str(m, "single_user") != 0) ^
	    (mdbm_fetch_str(m, "single_host") != 0)) {
		fprintf(stderr, "Setup: single_user and single_host must appear together.\n");
		if (config_path) goto err;
		goto again;
	}		
#if 0	/* this makes setuptool appear to hang up when a non-approved
         * category is given.
	 */
	if ( (t = mdbm_fetch_str(m, "category")) && strlen(t) > 0) {
		gettemp(buf, "cat");
		if (f = fopen(buf, "wt")) {
			getMsg("setup_categories", 0, 0, f);
			fclose(f);
		}
		if (f = fopen(buf, "rt")) {
			cat = sccs_keys2mdbm(f);
			fclose(f);
			unlink(buf);

			if (cat) {
				unless (mdbm_fetch_str(cat, t)) {
					fprintf(stderr, "<%s> is not a known project category; use anyway[y/N]? ", t);
					unless (fgets(buf, sizeof buf, stdin)) buf[0] = 0;
				}
				else buf[0] = 'y';
				mdbm_close(cat);
				unless (buf[0] == 'y' || buf[0] == 'Y') goto again;
			}
		}
		else unlink(buf);
	}
#endif

	mdbm_close(m);

	if (cset_setup(SILENT, ask)) goto err;
	s = sccs_init(s_config, SILENT, NULL);
	assert(s);
	sccs_delta(s, SILENT|NEWFILE, 0, 0, 0, 0);
	s = sccs_restart(s);
	assert(s);
	sccs_get(s, 0, 0, 0, 0, SILENT|GET_EXPAND, 0);
	sccs_free(s);
	defaultIgnore();

	sprintf(setup_files, "%s/setup_files%d", TMP_PATH, getpid());
	sprintf(buf, "bk -R sfiles -pC > %s", setup_files);
	system(buf);
	sprintf(buf,
	    "bk cset -q -y\"Initial repository create\" -  < %s", setup_files);
	system(buf);
	unlink(setup_files);
 	if (sccs_cd2root(0, 0) == -1) {
                fprintf(stderr, "setup: cannot find package root.\n");
                exit(1);
        }                           
	mkdir(BKMASTER, 0775);
	enableFastPendingScan();
	sendConfig();
	return (0);
}

private void
defaultIgnore()
{
	int	fd = open("BitKeeper/etc/ignore", O_CREAT|O_RDWR, 0664);

	if (write(fd, "BitKeeper/*/*\n", 14) != 14) {
err:		perror("write");
		close(fd);
		return;
	}
	if (write(fd, "PENDING/*\n", 10) != 10) goto err;
	close(fd);
	system("bk new -Pq BitKeeper/etc/ignore");
}

private void
usage()
{
	system("bk help -s setup");
	exit(1);
}

private MDBM *
addField(MDBM *flist, char *field)
{
	char	*p;

	unless (flist) flist = mdbm_open(NULL, 0, 0, 1024);

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
 * Find field separator
 */
private char *
fieldSeparator(char *field)
{
	char	*p;

	/*
	 * Check for filter prefix
	 * e.g. [awc:/home/bk/bugfixes]checkout:get
	 */
	if (field[0] == '[')  {
		/* Skip over the possible colon inside the filter */
		p = strchr(field, ']');
		unless (p) return (NULL); /* not a proper filter */
		p = strchr(++p, ':');
	} else {
		p = strchr(field, ':');
	}
	return (p);
}

private void
printField(FILE *out, MDBM *flist, char *field)
{
	char	*p, *val;

	unless (flist) {
use_default:	fputs(field, out);
		return;
	}

	p = fieldSeparator(field);
	unless (p) goto use_default;
	*p = 0;
	val = mdbm_fetch_str(flist, field);
	unless (val) goto use_default;

	/*
	 * If we get here, user wants to override the default value
	 */
	fprintf(out, "%s: %s\n", field, val);
	mdbm_delete_str(flist, field);
}

private int
mkconfig(FILE *out, MDBM *flist)
{
	FILE	*in;
	int	found = 0;
	int	first = 1;
	char	confTemplate[MAXPATH], buf[200], pattern[200];
	char	*val = NULL;
	kvpair	kv;


	/*
	 * If there is a local config file template, use that
	 */
	sprintf(confTemplate, "%s/BitKeeper/etc/config.template", globalroot());
	if (in = fopen(confTemplate, "rt")) {
		while (fnext(buf, in))	printField(out, flist, buf);
		fclose(in);
		return (0);
	}

	sprintf(buf, "%s/bkmsg.txt", bin);
	unless (in = fopen(buf, "rt")) {
		fprintf(stderr, "Unable to open %s\n", buf);
		return (-1);
	}
	getMsg("config_preamble", 0, "# ", out);
	fputs("\n", out);

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

	if (flist) val = mdbm_fetch_str(flist, "compression");
	/* force "compression to default on */
	unless (val && *val) {
		char fld[] =  "compression=gzip";
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
		sprintf(pattern, "config_%s", buf);
		getMsg(pattern, 0, "# ", out);
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
