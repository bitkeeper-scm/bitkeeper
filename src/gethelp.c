#include "system.h"
#include "sccs.h"

extern char     *bin;

int
gethelp_main(int ac, char **av)
{
	unless (av[1]) {
		fprintf(stderr, "usage: gethelp help_name bkarg\n");
		exit(1);
	}
	return (gethelp(av[1], av[2], 0, stdout) == 0);
}

int
gethelp(char *help_name, char *bkarg, char *prefix, FILE *outf)
{
	char	buf[MAXLINE], pattern[MAXLINE];
	FILE	*f;
	int	found = 0;
	int	first = 1;

	if (bkarg == NULL) bkarg = "";
	sprintf(buf, "%s/bkhelp.txt", bin);
	f = fopen(buf, "rt");
	unless (f) {
		fprintf(stderr, "Unable to locate help file %s\n", buf);
		exit(1);
	}
	sprintf(pattern, "#%s\n", help_name);
	while (fgets(buf, sizeof(buf), f)) {
		if (streq(pattern, buf)) {
			found = 1;
			break;
		}
	}
	while (fgets(buf, sizeof(buf), f)) {
		char	*p;

		if (first && (buf[0] == '#')) continue;
		first = 0;
		if (streq("$\n", buf)) break;
		if (prefix) fputs(prefix, outf);
		p = strstr(buf, "#BKARG#");
		if (p) {
			*p = 0;
			fputs(buf, outf);
			fputs(bkarg, outf);
			fputs(&p[7], outf);
		} else {
			fputs(buf, outf);
		}
	}
	fclose(f);
	return (found);
}

int
helpaliases_main(int ac, char **av)
{
	char	buf[MAXLINE], name[MAXLINE];
	FILE	*f;

	sprintf(buf, "%s/bkhelp.txt", bin);
	unless (f = fopen(buf, "rt")) {
		fprintf(stderr, "Unable to locate help file %s\n", buf);
		exit(1);
	}
	name[0] = 0;
	while (fgets(buf, sizeof(buf), f)) {
		unless (strneq("#help_", buf, 6)) {
			name[0] = 0;
			continue;
		}
		chop(buf);
		if (name[0]) {
			printf("%s\t%s\n", &buf[6], &name[6]);
		} else {
			strcpy(name, buf);
		}
	}
	fclose(f);
	exit (0);
}

/*
 * Generate the topic list.
 * Each help topic is preceeded by a #section_section name or it goes in
 * Misc.
 *
 * XXX - need sections for
 * File operations
 * ChangeSet
 * Administrative commands
 * Misc
 */
int
helptopiclist_main(int ac, char **av)
{
	char	buf[MAXLINE], name[MAXLINE];
#define	GENERAL		0
#define	LICENSE		1
#define	BASIC		2
#define	REPOSITORY	3
#define	GUI_TOOLS	4
#define	FILES		5
#define	ADMIN		6
#define	COMPATIBILITY	7
#define	MISC		8
#define	OBSOLETE	9
#define	LAST		9
	char	**sections[LAST+1];
	int	a = 0, i = -1, j;
	MDBM	*aliases = mdbm_mem();
	MDBM	*dups = mdbm_mem();
	FILE	*f;
	kvpair	kv;

	bzero(sections, sizeof(sections));
	sprintf(buf, "%s/bkhelp.txt", bin);
	unless (f = fopen(buf, "rt")) {
		fprintf(stderr, "Unable to locate help file %s\n", buf);
		exit(1);
	}
	name[0] = 0;
	while (fgets(buf, sizeof(buf), f)) {
		chop(buf);
#define	streqcase	!strcasecmp
		if (streqcase("#section General", buf)) {
			i = GENERAL;
			continue;
		} else if (streqcase("#section Licensing", buf)) {
			i = LICENSE;
			continue;
		} else if (streqcase("#section Basic", buf)) {
			i = BASIC;
			continue;
		} else if (streqcase("#section Repository", buf)) {
			i = REPOSITORY;
			continue;
		} else if (streqcase("#section GUI tools", buf)) {
			i = GUI_TOOLS;
			continue;
		} else if (streqcase("#section Compatibility", buf)) {
			i = COMPATIBILITY;
			continue;
		} else if (streqcase("#section File", buf)) {
			i = FILES;
			continue;
		} else if (streqcase("#section Admin", buf)) {
			i = ADMIN;
			continue;
		} else if (streqcase("#section Obsolete", buf)) {
			i = OBSOLETE;
			continue;
		} else if (strneq("#section", buf, 8)) {
			fprintf(stderr, "WARNING: unknown section %s\n", buf);
			i = MISC;
			continue;
		}
		unless (strneq("#help_", buf, 6)) {
			name[0] = 0;
			i = -1;
			continue;
		}
		if (mdbm_store_str(dups, &buf[6], "", MDBM_INSERT)) {
			fprintf(stderr, "Duplicate key: %s\n", &buf[6]);
		}
		if (name[0]) {
			mdbm_store_str(aliases, &buf[6], name, 0);
			a++;
		} else {
			strcpy(name, &buf[6]);
			if (i == -1) i = MISC;
			sections[i] = addLine(sections[i], strdup(name));
		}
	}
	fclose(f);
	for (j = 0; j <= LAST; ++j) {
		unless (sections[j]) continue;
		switch (j) {
		    case GENERAL: printf("General\n"); break;
		    case LICENSE: printf("License\n"); break;
		    case BASIC: printf("Basic\n"); break;
		    case REPOSITORY: printf("Repository\n"); break;
		    case GUI_TOOLS: printf("GUI tools\n"); break;
		    case COMPATIBILITY: printf("Compatibility\n"); break;
		    case FILES: printf("File\n"); break;
		    case ADMIN: printf("Administration\n"); break;
		    case MISC: printf("Misc\n"); break;
		    case OBSOLETE: printf("Obsolete\n"); break;
		}
		sortLines(sections[j]);
		EACH(sections[j]) {
			if (isdigit(sections[j][i][0])) {
				printf("  %s\n", &sections[j][i][1]);
			} else {
				printf("  %s\n", sections[j][i]);
			}
		}
	}
	if (a) {
		printf("Aliases\n");
		for (kv = mdbm_first(aliases);
		    kv.key.dsize; kv = mdbm_next(aliases)) {
			if (isdigit(*kv.val.dptr)) kv.val.dptr++;
			printf("%s\t%s\n", kv.key.dptr, kv.val.dptr);
		}
	}
	exit (0);
}

int
helpsearch_main(int ac, char **av)
{
	char	p[500], buf[200], name[200], sect[200];
	int	Help, All, Long, Debug, Sections, Offsets;
	int	offset = 0, c, len;
	char	*str, *word;
	FILE	*f;
	MDBM	*printed = mdbm_mem();

	Help = All = Long = Debug = Sections = Offsets = 0;
	while ((c = getopt(ac, av, "adhlos")) != -1) {
		switch (c) {
		    case 'a': All++; break;
		    case 'd': Debug++; break;
		    case 'h': Help++; break;
		    case 'l': Long++; break;
		    case 's': Sections++; break;
		    case 'o': Offsets++; break;
		    default:
			goto usage;
	    	}
	}
	unless (word = av[optind]) {
usage:		fprintf(stderr, "usage: bk helpsearch [-also] word\n");
		exit(1);
	}
	len = strlen(word);
	sprintf(buf, "%s/bkhelp.txt", bin);
	unless (f = fopen(buf, "rt")) {
		fprintf(stderr, "Unable to locate help file %s\n", buf);
		exit(1);
	}
	name[0] = 0;
	while (fgets(buf, sizeof(buf), f)) {
		chop(buf);
		if (strneq("#help_", buf, 6)) {
			unless (name[0]) strcpy(name, &buf[6]);
			continue;
		} else if (strneq("#section ", buf, 9)) {
			name[0] = 0;
			offset = 0;
			strcpy(sect, &buf[9]);
			continue;
		}
		if (buf[0] == '#') continue;
		offset++;
		unless (str = strstr(buf, word)) continue;
		unless (All) {
			unless ((str == buf) || isspace(str[-1])) {
				if (Debug) printf("SKIP1 %s\n", buf);
				continue;
			}
			unless (isspace(str[len]) ||
			    !str[len] || ispunct(str[len])) {
				if (Debug) printf("SKIP2 %s\n", buf);
				continue;
			}
		}
		if (Long) {
			if (Offsets) {
				sprintf(p,
				    "%s/%s/%d\t%s\n", sect, name, offset, buf);
			} else {
				if (Help) {
					sprintf(p, "%s\t%s\n", name, buf);
				} else {
					sprintf(p,
					    "%s/%s\t%s\n", sect, name, buf);
				}
			}
		} else if (Sections) {
			sprintf(p, "%s\n", sect);
		} else {
			if (Offsets) {
				sprintf(p, "%s/%s/%d\n", sect, name, offset);
			} else {
				sprintf(p, "%s/%s\n", sect, name);
			}
		}
		if (mdbm_fetch_str(printed, p)) continue;
		fputs(p, stdout);
		mdbm_store_str(printed, p, "", 0);
	}
	fclose(f);
	mdbm_close(printed);
}

int
getmsg_main(int ac, char **av)
{
	unless (av[1]) {
		fprintf(stderr, "usage: getmsg msg_name bkarg\n");
		exit(1);
	}
	return (getmsg(av[1], av[2], 0, stdout) == 0);
}
