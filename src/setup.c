#include "system.h"
#include "sccs.h"

extern char *editor, *pager, *bin;

int
setup_main(int ac, char **av)
{
	int	force = 0, c;
	char	*package_name = 0, *package_path = 0, *config_path = 0;
	char	buf[MAXLINE], my_editor[1024], setup_files[MAXPATH];
	char 	s_config[MAXPATH] = "SCCS/s.config";
	FILE	*f, *f1;
	sccs	*s;
	char	*edit[] = {my_editor, "Description", 0};

	while ((c = getopt(ac, av, "c:fn:")) != -1) {
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
		    case 'n':
			package_name = optarg;
			break;
		}
	}
	unless (package_path = av[optind]) {
		printf(
	"Usage: bk setup [-c<config file>] [-n <package name>] directory\n");
		exit (0);
	}
	if (exists(package_path)) {
		printf("bk: %s exists already, setup fails.\n", package_path);
		exit (1);
	}
	license();
	unless(force) {
		gethelp("setup_1", "", 0, stdout);
		printf("Create new package? [no] ");
		if (fgets(buf, sizeof(buf), stdin) == NULL) buf[0] = 'n';
		if ((buf[0] != 'y') && (buf[0] != 'Y')) exit (0);
	}
	mkdirp(package_path);
	if (chdir(package_path) != 0) exit(1);
	unless(package_name) {
		gethelp("setup_2", "", 0, stdout);
		while (1) {
			/*
			 * win32 note: notepate.exe wants text mode
			 */
			f = fopen("Description", "wt");
			fprintf(f,
				"Replace this with your package description\n");
			fclose(f);
			f = fopen("D.save", "wb");
			fprintf(f,
				"Replace this with your package description\n");
			fclose(f);
			printf("Editor to use [%s] ", editor);
			unless (fgets(my_editor, sizeof(my_editor), stdin)) {
				my_editor[0] = '\0';
			}
			chop(my_editor);
			edit[0] = (my_editor[0] != 0) ? my_editor : editor;
			if (spawnvp_ex(_P_WAIT, edit[0], edit) != 0) continue;
			if (my_editor[0] != 0) editor = strdup(my_editor);
			if (system("cmp -s D.save Description")) {
				break;
			} else {
				printf(
				"Sorry, you have to put something in there\n");
			}
		}
	} else {
		f = fopen("Description", "wb");
		fputs(package_name, f);
		fclose(f);
	}
	system("bk cset -snDescription .");

	f = fopen("Description", "rt");
	fgets(buf, sizeof(buf), f);
	fclose(f);
	unlink("Description"); unlink("D.save");
	if (chdir("BitKeeper/etc") != 0) {
		perror("cd BitKeeper/etc");
		exit (1);
	}
	if (config_path == NULL) {
		gethelp("setup_3", "", 0, stdout);
		f1 = fopen("config", "wt"); /* notepad.exe wants text mode */
		assert(f1);
		mkconfig(f1);
		fclose(f1);
		system("cp config D.config");
		chmod("config", 0664);
		while (1) {
			printf("Editor to use [%s] ", editor);
			unless (fgets(my_editor, sizeof(my_editor), stdin)) {
				my_editor[0] = '\0';
			}
			chop(my_editor);
			if (my_editor[0] != 0) {
				sprintf(buf, "%s config", my_editor);
			} else {
				sprintf(buf, "%s config", editor);
			}
			system(buf);
			sprintf(buf, "cmp -s D.config config", bin);
			if (system(buf)) {
				break;
			} else {
				printf("Sorry, you have to fill this out.\n");
			}
		}
	} else {
		sprintf(buf, "cp %s config", config_path);
		system(buf);
	}
	unlink("D.config");
	s = sccs_init(s_config, SILENT, NULL);
	assert(s);
	sccs_delta(s, SILENT|NEWFILE, 0, 0, 0, 0);
	s = sccs_restart(s);
	assert(s);
	sccs_get(s, 0, 0, 0, 0, SILENT|GET_EXPAND, 0);
	sccs_free(s);

	sprintf(setup_files, "%s/setup_files%d", TMP_PATH, getpid());
	sprintf(buf, "bk sfiles -C > %s", setup_files);
	system(buf);
	sprintf(buf,
	    "bk cset -q -y\"Initial repository create\" -  < %s", setup_files);
	system(buf);
	unlink(setup_files);
 	if (sccs_cd2root(0, 0) == -1) {
                fprintf(stderr, "setup: can not find package root.\n");
                exit(1);
        }                           
	sendConfig("setups@openlogging.org");
	return (0);
}
