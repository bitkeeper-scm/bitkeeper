#include "system.h"
#include "sccs.h"

extern char *editor, *pager, *bin;
private void    usage(void);
private void	defaultIgnore();

int
setup_main(int ac, char **av)
{
	int	force = 0, c;
	char	*package_path = 0, *config_path = 0, *t;
	char	buf[MAXLINE], my_editor[1024], setup_files[MAXPATH];
	char 	s_config[MAXPATH] = "BitKeeper/etc/SCCS/s.config";
	sccs	*s;
	MDBM	*m;

	while ((c = getopt(ac, av, "c:f")) != -1) {
		switch (c) {
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
		    case 'f':
			force = 1;
			break;
		    default:
			usage();
		}
	}
	unless (package_path = av[optind]) {
		printf("Usage: bk setup [-c<config file>] directory\n");
		exit (0);
	}
	if (exists(package_path)) {
		printf("bk: %s exists already, setup fails.\n", package_path);
		exit (1);
	}
	license();
	unless(force) {
		getmsg("setup_1", 0, 0, stdout);
		printf("Create new package? [no] ");
		if (fgets(buf, sizeof(buf), stdin) == NULL) buf[0] = 'n';
		if ((buf[0] != 'y') && (buf[0] != 'Y')) exit (0);
	}
	if (mkdirp(package_path)) {
		perror(package_path);
		exit(1);
	}
	if (chdir(package_path) != 0) {
		perror(package_path);
		exit(1);
	}
	sccs_mkroot(".");
	if (config_path == NULL) {
		FILE 	*f;

		getmsg("setup_3", 0, 0, stdout);
		/* notepad.exe wants text mode */
		f = fopen("BitKeeper/etc/config", "wt");
		assert(f);
		mkconfig(f);
		fclose(f);
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
		unless (exists(config_path)) {
			fprintf(stderr, "setup: can't open %s\n", config_path);
			fprintf(stderr, "You need to use a fullpath\n");
			exit(1);
	    	}
		sprintf(buf, "cp %s BitKeeper/etc/config", config_path);
		system(buf);
	}

	unless (m = loadConfig(".", 0)) {
		fprintf(stderr, "No config file found\n");
		exit(1);
	}
	unless (mdbm_fetch_str(m, "description")) {
		fprintf(stderr, "Setup: must provide a description.\n");
		if (config_path) exit(1);
		goto again;
	}
	unless (mdbm_fetch_str(m, "logging")) {
		fprintf(stderr, "Setup: must define logging policy.\n");
		if (config_path) exit(1);
		goto again;
	}
	unless (t = mdbm_fetch_str(m, "email")) {
		fprintf(stderr, "Setup: must define email contact.\n");
		if (config_path) exit(1);
		goto again;
	}
	unless (t = strchr(t, '@')) {
		fprintf(stderr, "Setup: must define a valid email contact.\n");
		if (config_path) exit(1);
		goto again;
	}
	unless (t = strchr(t, '.')) {
		fprintf(stderr, "Setup: must define a valid email contact.\n");
		if (config_path) exit(1);
		goto again;
	}
	mdbm_close(m);

	if (cset_setup(SILENT)) return (1);
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
	mkdir("BitKeeper/etc/.master", 0775);
	sendConfig("setups@openlogging.org", "1.0");
	return (0);
}

private void
defaultIgnore()
{
	int	fd = open("BitKeeper/etc/ignore", O_CREAT|O_RDWR, 0664);

	if (write(fd, "BitKeeper/*/*\n", 14) != 14) {
		perror("write");
		close(fd);
		return;
	}
	close(fd);
	system("bk new -Pq BitKeeper/etc/ignore");
}

private void
usage()
{
	fprintf(stderr,
		    "\n\
usage: setup [-f] [-c<config file>] directory\n\
\n\
	-f               Don't ask for confirmation.\n\
	-c<config file>  Configuration file to use for setup.\n\n");
	exit(1);
}
