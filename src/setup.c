#include "system.h"
#include "sccs.h"

extern char *editor, *pager, *bin;

int
setup_main(int ac, char **av)
{
	int	force = 0, c;
	char	*project_name = 0, *project_path = 0, *config_path = 0;
	char	buf[1024], my_editor[1024], setup_files[MAXPATH];
	char 	s_config[MAXPATH] = "SCCS/s.config";
	FILE	*f;
	sccs	*s;

	while ((c = getopt(ac, av, "c:fn:")) != -1) {
		switch (c) {
		    case 'c':	localName2bkName(optarg, optarg);
				config_path = fullname(optarg, 0);
				break;
		    case 'f':	force = 1; break;
		    case 'n':	project_name = optarg; break;
		}
	}
	unless (project_path = av[optind]) {
		printf(
	"Usage: bk setup [-c<config file>] [-n <project name>] directory\n");
		exit (0);
	}
	if (exists(project_path)) {
		printf("bk: %s exists already, setup fails.\n", project_path);
		exit (1);
	}
	license();
	unless(force) {
		gethelp("setup_1", "", stdout);
		printf("Create new project? [no] ");
		if (fgets(buf, sizeof(buf), stdin) == NULL) buf[0] = 'n';
		if ((buf[0] != 'y') && (buf[0] != 'Y')) exit (0);
	}
	mkdirp(project_path);
	if (chdir(project_path) != 0) exit(1);
	unless(project_name) {
		gethelp("setup_2", "", stdout);
		while (1) {
			f = fopen("Description", "wb");
			fprintf(f,
				"Replace this with your project description");
			fclose(f);
			f = fopen("D.save", "wb");
			fprintf(f,
				"Replace this with your project description");
			fclose(f);
			printf("Editor to use [%s] ", editor);
			unless (fgets(my_editor, sizeof(my_editor), stdin)) {
				my_editor[0] = '\0';
			}
			chop(my_editor);
			if (my_editor[0] != 0) {
				sprintf(buf, "%s Description", my_editor);
			} else {
				sprintf(buf, "%s Description", editor);
			}
			system(buf);
			if (system("cmp -s D.save Description")) {
				break;
			} else {
				printf(
				"Sorry, you have to put something in there\n");
			}
		}
	} else {
		f = fopen("Description", "wb");
		fputs(project_name, f);
		fclose(f);
	}
	system("bk cset -siDescription .");

	f = fopen("Description", "rt");
	fgets(buf, sizeof(buf), f);
	fclose(f);
	unlink("Description"); unlink("D.save");
	if (chdir("BitKeeper/etc") != 0) {
		perror("cd BitKeeper/etc");
		exit (1);
	}
	if (config_path == NULL) {
		gethelp("setup_3", "", stdout);
		sprintf(buf, "cp %s/bitkeeper.config config", bin);
		system(buf);
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
			sprintf(buf, "cmp -s %s/bitkeeper.config config", bin);
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
                fprintf(stderr, "sendconfig: can not find project root.\n");
                exit(1);
        }                           
	sendConfig("setups@openlogging.org", 1, 0);
	return (0);
}
