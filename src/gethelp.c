#include "system.h"
#include "sccs.h"

#define	HELPURL	"help://"
#define	HSZ	7		/* number of chars, not counting NULL */

extern char     *bin;

int
gethelp_main(int ac, char **av)
{
	unless (av[1]) {
		fprintf(stderr, "usage: gethelp [section/]topic bkarg\n");
		exit(1);
	}
	return (gethelp(av[1], av[2], 0, stdout) == 0);
}

/*
 * Usage: gethelp("[section/]name", arg, prefix, out)
 */
int
gethelp(char *topic, char *bkarg, char *prefix, FILE *outf)
{
	char	buf[MAXLINE], pattern[MAXLINE];
	FILE	*f;
	int	found = 0;
	char	*t;

	if (bkarg == NULL) bkarg = "";
	sprintf(buf, "%s/bkhelp.txt", bin);
	f = fopen(buf, "rt");
	unless (f) {
		fprintf(stderr, "Unable to locate help file %s\n", buf);
		exit(1);
	}
	if (strchr(topic, '/')) {
		sprintf(pattern, "%s%s\n", HELPURL, topic);
		while (fgets(buf, sizeof(buf), f)) {
			if (streq(pattern, buf)) {
				found = 1;
				break;
			}
		}
	} else {
		/*
		 * Take the first one that matches.
		 * XXX - may be others.
		 */
		while (fgets(buf, sizeof(buf), f)) {
			unless (strneq(HELPURL, buf, HSZ)) continue;
			t = strrchr(buf, '/');
			t++;
			chop(buf);
			if (streq(t, topic)) {
				found = 1;
				break;
			}
		}
	}
	unless (found) return (0);
	while (fgets(buf, sizeof(buf), f)) {
		char	*p;

		if (strneq(HELPURL, buf, HSZ)) continue;
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
		unless (strneq(HELPURL, buf, HSZ)) {
			name[0] = 0;
			continue;
		}
		chop(buf);
		if (name[0]) {
			printf("%s\t%s\n", &buf[HSZ], &name[HSZ]);
		} else {
			strcpy(name, buf);
		}
	}
	fclose(f);
	exit (0);
}

/*
 * Generate the topic list.
 */
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
int
helptopiclist_main(int ac, char **av)
{
	char	buf[MAXLINE], name[MAXLINE];
	char	*t, **sections[LAST+1];
	int	alias = 0, a = 0, i = -1, j;
	MDBM	*aliases = mdbm_mem();
	MDBM	*dups = mdbm_mem();
	FILE	*f;
	int	full = (ac > 1) && streq(av[1], "-f");
	kvpair	kv;

	bzero(sections, sizeof(sections));
	sprintf(buf, "%s/bkhelp.txt", bin);
	unless (f = fopen(buf, "rt")) {
		fprintf(stderr, "Unable to locate help file %s\n", buf);
		exit(1);
	}
	name[0] = 0;
	while (fgets(buf, sizeof(buf), f)) {
		unless (strneq(HELPURL, buf, HSZ)) {
			alias = 0;
			continue;
		}
		chop(buf);
		t = strrchr(buf, '/');
		*t = 0;
#define	streqcase	!strcasecmp
		if (streqcase("General", &buf[HSZ])) {
			i = GENERAL;
		} else if (streqcase("Licensing", &buf[HSZ])) {
			i = LICENSE;
		} else if (streqcase("Basics", &buf[HSZ])) {
			i = BASIC;
		} else if (streqcase("Repository", &buf[HSZ])) {
			i = REPOSITORY;
		} else if (streqcase("GUI-tools", &buf[HSZ])) {
			i = GUI_TOOLS;
		} else if (streqcase("Compat", &buf[HSZ])) {
			i = COMPATIBILITY;
		} else if (streqcase("File", &buf[HSZ])) {
			i = FILES;
		} else if (streqcase("Admin", &buf[HSZ])) {
			i = ADMIN;
		} else if (streqcase("Obsolete", &buf[HSZ])) {
			i = OBSOLETE;
		} else {
			fprintf(stderr, "WARNING: unknown section %s\n", buf);
			i = MISC;
		}
		*t = '/';
		if (mdbm_store_str(dups, &buf[HSZ], "", MDBM_INSERT)) {
			fprintf(stderr, "Duplicate key: %s\n", &buf[HSZ]);
		}
		if (alias) {
			mdbm_store_str(aliases, &buf[HSZ], name, 0);
			a++;
		} else {
			strcpy(name, &buf[HSZ]);
			sections[i] = addLine(sections[i], strdup(t+1));
		}
		alias = 1;
	}
	fclose(f);
	for (j = 0; j <= LAST; ++j) {
		unless (sections[j]) continue;
		switch (j) {
		    case GENERAL:	t = "General"; break;
		    case LICENSE:	t = "Licensing"; break;
		    case BASIC:		t = "Basics"; break;
		    case REPOSITORY:	t = "Repository"; break;
		    case GUI_TOOLS:	t = "GUI-tools"; break;
		    case COMPATIBILITY: t = "Compat"; break;
		    case FILES:		t = "File"; break;
		    case ADMIN: 	t = "Admin"; break;
		    case MISC: 		t = "Misc"; break;
		    case OBSOLETE:	t = "Obsolete"; break;
		    default: 		t = "???"; break;
		}
		printf("%s\n", t);
		if (full) {
			sprintf(buf, "  %s/", t);
			t = buf;
		} else {
			t = "  ";
		}
		sortLines(sections[j]);
		EACH(sections[j]) {
			unless (streq(sections[j][i], "Introduction")) continue;
			printf("%s%s\n", t, sections[j][i]);
		}
		EACH(sections[j]) {
			if (streq(sections[j][i], "Introduction")) continue;
			printf("%s%s\n", t, sections[j][i]);
		}
	}
	if (a) {
		printf("Aliases\n");
		for (kv = mdbm_first(aliases);
		    kv.key.dsize; kv = mdbm_next(aliases)) {
			printf("%s\t%s\n", kv.key.dptr, kv.val.dptr);
		}
	}
	exit (0);
}

int
helpsearch_main(int ac, char **av)
{
	char	p[500], buf[200], name[200];
	int	All = 0, Long = 0, Debug = 0, Sections = 0;
	int	c, len;
	char	*t, *str, *word;
	FILE	*f;
	MDBM	*printed = mdbm_mem();

	while ((c = getopt(ac, av, "adls")) != -1) {
		switch (c) {
		    case 'a': All++; break;
		    case 'd': Debug++; break;
		    case 'l': Long++; break;
		    case 's': Sections++; break;
		    default:
			goto usage;
	    	}
	}
	unless (word = av[optind]) {
usage:		fprintf(stderr, "usage: bk helpsearch [-als] word\n");
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
		if (strneq(HELPURL, buf, HSZ)) {
			unless (name[0]) strcpy(name, &buf[HSZ]);
			continue;
		}
		if (streq(buf, "$")) {
			name[0] = 0;
			continue;
		}
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
			sprintf(p, "%s\t%s\n", name, buf);
		} else if (Sections) {
			t = strchr(name, '/');
			*t = 0;
			sprintf(p, "%s\n", name);
		} else {
			sprintf(p, "%s\n", name);
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
