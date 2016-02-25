/*
 * Copyright 1999-2005,2010,2016 BitMover, Inc
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

#define	HELPURL	"help://"
#define	HSZ	7		/* number of chars, not counting NULL */

private int synopsis;

int
gethelp_main(int ac, char **av)
{
	int	c;
	char	*file = 0;

	while ((c = getopt(ac, av, "f:s", 0)) != -1) {
		switch (c) {
		    case 'f': file = optarg; break;
		    case 's': synopsis = 1; break;
		    default: bk_badArg(c, av);
	    	}
	}
	unless (av[optind]) usage();
	return (gethelp(file, av[optind], av[optind+1], 0, stdout));
}

/*
 * Usage: gethelp("name", arg, prefix, out)
 */
int
gethelp(char *helptxt, char *topic, char *bkarg, char *prefix, FILE *outf)
{
	char	buf[MAXLINE];
	FILE	*f;
	int	found = 0;
	char	*t;

	unless (helptxt) {
		sprintf(buf, "%s/bkhelp.txt", bin);
		helptxt = buf;
	}
	f = fopen(helptxt, "rt");
	unless (f) {
		fprintf(stderr, "Unable to locate help file %s\n", helptxt);
		exit(1);
	}
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
	unless (found) goto done;
	if (bkarg == NULL) bkarg = "";
	if (synopsis) {			/* print synopsis only */
		int	first = 1;

		found = 0;
		while (fgets(buf, sizeof(buf), f)) {
			if (strneq("SYNOPSIS", buf, 8)) {
				found = 1;
				break;
			}
			if (streq("$\n", buf)) break;
		}
		unless (found) goto done;
		fputs("usage:\t", outf);
		while (fgets(buf, sizeof(buf), f)) {
			if (streq("\n", buf)) break;
			if (streq("$\n", buf)) break;
			for (t = buf; isspace(*t); t++);
			if (!first && t && strneq("bk ", t, 3)) {
				fputc('\n', outf);
				fputc('\t', outf);
			} else unless (first) {
				fputc(' ', outf);
			}
			first = 0;
			for ( ; *t; t++) {
				if (*t == '\n') break;
				if (isspace(*t) && isspace(t[1])) continue;
				fputc(*t, outf);
			}
		}
		fputc('\n', outf);
	} else { /* print full man page */
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
			} else unless (p = strstr(buf, "#BKMOD#")) {
				fputs(buf, outf);
			} else {
				for (t = buf; p = strstr(t, "#BKMOD#"); ) {
					*p = 0;
					fputs(t, outf);
					t = &p[7];
					if (macosx()) {
				        	/* all the same size for roff 
				        	 *     #BKMOD#
						 */
						fputs("Command", outf);
					} else {
						fputs("Control", outf);
					}
				}
				if (t && *t) fputs(t, outf);
			}
		}
	}
done:	fclose(f);
	if (fflush(outf) == EOF) {
		fprintf(stderr, "gethelp: %s\n", strerror(errno));
		return (2);
	} else {
		return (found == 0);
	}
}

/*
 * Spit out the topic list in order.
 * The first section is "Summaries" and the next is "Alphabetical".
 * Finally, spit out the aliases.
 */
int
helptopics_main(int ac, char **av)
{
	char	buf[MAXLINE], name[MAXLINE];
	char	*t;
	int	c, first = 1, alias = 0, a = 0;
	MDBM	*aliases = mdbm_mem();
	MDBM	*dups = mdbm_mem();
	FILE	*f;
	kvpair	kv;
	char	*file = 0;

	while ((c = getopt(ac, av, "f:", 0)) != -1) {
		switch (c) {
		    case 'f':
			file = optarg; break;
		    default: bk_badArg(c, av);
	    	}
	}
	unless (file) {
		sprintf(buf, "%s/bkhelp.txt", bin);
		file = buf;
	}
	unless (f = fopen(file, "rt")) {
		fprintf(stderr, "Unable to locate help file %s\n", file);
		exit(1);
	}
	printf("Summaries\n");
	name[0] = 0;
	while (fgets(buf, sizeof(buf), f)) {
		unless (strneq(HELPURL, buf, HSZ)) {
			alias = 0;
			continue;
		}
		chop(buf);
		if (mdbm_store_str(dups, &buf[HSZ], "", MDBM_INSERT)) {
			fprintf(stderr, "Duplicate key: %s\n", &buf[HSZ]);
		}
		if (alias) {
			mdbm_store_str(aliases, &buf[HSZ], name, 0);
			a++;
			if (first &&
			    (t = strrchr(buf, '.')) && !streq(t, ".sum")) {
				first = 0;
				printf("Alphabetical\n");
			}
		} else {
			if (name[0]) printf(" %s\n", name);
			strcpy(name, &buf[HSZ]);
		}
		alias = 1;
	}
	if (name[0]) printf(" %s\n", name);
	fclose(f);
	if (a) {
		printf("Aliases\n");
		for (kv = mdbm_first(aliases);
		    kv.key.dsize; kv = mdbm_next(aliases)) {
			printf("%s\t%s\n", kv.key.dptr, kv.val.dptr);
		}
	}
	mdbm_close(aliases);
	mdbm_close(dups);
	exit (0);
}

int
helpsearch_main(int ac, char **av)
{
	char	p[500], lower[MAXPATH], buf[MAXPATH], name[200];
	int	substrings = 0, Long = 0, Debug = 0;
	int	c, len;
	char	*str, *word;
	FILE	*f;
	char	*file = 0;
	MDBM	*printed = mdbm_mem();

	while ((c = getopt(ac, av, "adf:l", 0)) != -1) {
		switch (c) {
		    case 'a': substrings++; break;
		    case 'd': Debug++; break;
		    case 'f': file = optarg; break;
		    case 'l': Long++; break;
		    default: bk_badArg(c, av);
	    	}
	}
	unless (word = av[optind]) usage();
	len = strlen(word);
	unless (file) {
		sprintf(buf, "%s/bkhelp.txt", bin);
		file = buf;
	}
	unless (f = fopen(file, "rt")) {
		fprintf(stderr, "Unable to locate help file %s\n", file);
		exit(1);
	}
	for (c = 0; word[c]; c++) word[c] = tolower(word[c]);
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
		for (c = 0; buf[c]; c++) lower[c] = tolower(buf[c]);
		lower[c] = 0;
		unless (str = strstr(lower, word)) continue;
		unless (substrings) {	/* otherwise whole words only */
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
		} else {
			sprintf(p, "%s\n", name);
		}
		if (mdbm_fetch_str(printed, p)) continue;
		fputs(p, stdout);
		mdbm_store_str(printed, p, "", 0);
	}
	fclose(f);
	mdbm_close(printed);
	return (0);
}

int
getmsg_main(int ac, char **av)
{
	char	b = 0;
	char	*tag;
	char	**args = 0;

	unless (av[1]) usage();
	if (av[1][0] && av[1][1] && !av[1][2]) {
		b = av[1][1];
		av++;
	}
	tag = av[1];
	av++;
	while (av[1]) {
		args = addLine(args, av[1]);
		av++;
	}
	return (getMsgv(tag, args, 0, b, stdout) == 0);
}
