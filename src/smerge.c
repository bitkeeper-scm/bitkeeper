#include "system.h"
#include "sccs.h"
#include <string.h>

private	char	*getrevs(int i, char **av);
private int	do_merge(int start, int end, FILE *inputs[3]);
private void	usage(void);
private char	*fagets(FILE *fh);
private int	resolve_conflict(char **lines[3], u32 start, u32 end);
private char	**unidiff(char **gca, char **new);
private void	show_examples(void);
private	int	parse_range(char *range, int *start, int *end);

typedef struct region region;
private	void	user_conflict(region *r);
private	void	user_conflict_fdiff(region *r);


/* automerge functions */
private	void	enable_mergefcns(char *list, int enable);
private	void	mergefcns_help(void);

private int	merge_same_changes(region *r);
private int	merge_only_one(region *r);
private int	merge_content(region *r);
private int	merge_common_header(region *r);
private int	merge_common_footer(region *r);
private	int	merge_common_deletes(region *r);

enum {
	MODE_GCA,
	MODE_2WAY,
	MODE_3WAY,
	MODE_NEWONLY
};
 
enum {
	LEFT,
	GCA, 
	RIGHT
};

#define VALID(lines, i) ((lines) && (i) < (int)(lines)[0] && (lines)[i])

private	char	*revs[3];
private	char	*file;
private	int	mode;
private	int	show_seq;
private	int	fdiff;
private	char	*anno = 0;
private	int	tostdout = 0;
private	FILE	*outf;

int
smerge_main(int ac, char **av)
{
	int	c;
	int	i;
	char	buf[MAXPATH];
	FILE	*inputs[3];
	int	start = 0, end = 0;
	int	ret;
	int	identical = 1;
	int	status;

	mode = MODE_3WAY;
	while ((c = getopt(ac, av, "2A:a:efghI:npr:s")) != -1) {
		switch (c) {
		    case '2': /* 2 way format (like diff3) */
			mode = MODE_2WAY;
			break;
		    case 'g': /* gca format */
			mode = MODE_GCA;
			break;
		    case 'n': /* newonly (like -2 except marks added lines) */
			mode = MODE_NEWONLY;
			break;
		    case 'I': /* Add annotations (Info) */
		    	anno = optarg;
			break;
		    case 'A':
		    case 'a':
			enable_mergefcns(optarg, c == 'a');
			break;
		    case 'p':
			tostdout = 1;
			break;
		    case 'f': /* fdiff output mode */
			fdiff = 1;
			if (mode == MODE_3WAY) mode = MODE_GCA;
			break;
		    case 'e': /* show examples */
			show_examples();
			return(2);
		    case 'r': /* show output in the range <r> */
			if (parse_range(optarg, &start, &end)) {
				usage();
				return (2);
			}
			break;
		    case 's': /* show sequence numbers */
			show_seq = 1;
			break;
		    case 'h': /* help */
		    default:
			usage();
			return (2);
		}
	}
	if (ac - optind != 4) {
		usage();
		return (2);
	}
	if (fdiff) assert(mode != MODE_3WAY);

	file = av[optind + 3];
	for (i = 0; i < 3; i++) revs[i] = getrevs(i, &av[optind]);
	
	/*
	 * check to see if both local and remote versions are identical.
	 */
	sprintf(buf, "bk get -qkp -r%s %s", revs[LEFT], file);
	inputs[LEFT] = popen(buf, "r");
	sprintf(buf, "bk get -qkp -r%s %s", revs[RIGHT], file);
	inputs[RIGHT] = popen(buf, "r");
	do {
		int	len;
		char	buf1[MAXPATH], buf2[MAXPATH];
		
		len = fread(buf1, 1, sizeof(buf1), inputs[LEFT]);
		if (len != fread(buf2, 1, sizeof(buf2), inputs[RIGHT])
		    || memcmp(buf1, buf2, len) != 0) {
			identical = 0;
			break;
		}
	} while (!feof(inputs[LEFT]));
	unless (feof(inputs[RIGHT])) identical = 0;
	status = pclose(inputs[LEFT]);
	if (status) {
		fprintf(stderr, 
		    "Fetch of revision %s of file %s failed.\n",
		    revs[LEFT], file);
		ret = 2;
	}
	status = pclose(inputs[RIGHT]);
	if (status) {
		fprintf(stderr, 
		    "Fetch of revision %s of file %s failed.\n",
		    revs[RIGHT], file);
		ret = 2;
	}
	if (ret) return (ret);
	if (identical) {
		free(revs[RIGHT]);
		revs[RIGHT] = strdup(revs[LEFT]);
	}
	
	
	for (i = 0; i < 3; i++) {
		sprintf(buf, "bk get %s%s -qkpO -r%s %s",
		    anno ? "-a" : "",
		    anno ? anno : "",
		    revs[i], 
		    file);
		inputs[i] = popen(buf, "r");
		assert(inputs[i]);
	}
	if (tostdout) {
		outf = stdout;
	} else {
		outf = fopen(file, "w");
		unless (outf) {
			fprintf(stderr, "Can't open %s for writing\n", file);
			exit(2);
		}
	}
	ret = do_merge(start, end, inputs);
	for (i = 0; i < 3; i++) {
		status = pclose(inputs[i]);
		if (status) {
			fprintf(stderr, 
			    "Fetch of revision %s of file %s failed.\n",
			    revs[i], file);
			ret = 2;
		}
		free(revs[i]);
	}
	unless (tostdout) fclose(outf);
	return (ret);
}

/*
 * convert the version string for the input fine into arguments that
 * can be passed to get.
 *   from 1.7+2.1,2.2-1.6
 *     to 1.7 -i2.1,2.2 -x1.6
 */
private char *
getrevs(int i, char **av)
{
	static	const	char	*envnames[] = {
		"MERGE_REVS_LOCAL", 
		"MERGE_REVS_GCA", 
		"MERGE_REVS_REMOTE"
	};
	char	*ret = 0;
	char	*verstr;
	char	*v;
	char	*p;
	int	len;

	verstr = getenv(envnames[i]);
	unless (verstr) verstr = av[i];
	/* Allocate buffer for return string */
	ret = malloc(strlen(verstr) + 16);
	p = ret;
	v = verstr;
	len = strspn(v, "0123456789.");
	unless (len) goto err;
	strncpy(p, v, len);
	p += len;
	v += len;
	while (*v == '+' || *v == '-') {
		p += sprintf(p, " -%c", (*v == '+') ? 'i' : 'x');
		++v;
		len = strspn(v, "0123456789.,");
		unless (len) goto err;
		strncpy(p, v, len);
		p += len;
		v += len;
	}
	if (*v) {
err:		fprintf(stderr, "Unable to parse version number: %s\n", verstr);
		exit(2);
	}
		
	*p = 0;
	assert(strlen(ret) < strlen(verstr) + 16);
	return (ret);
}
	

private void
usage(void)
{
	system("bk help -s smerge");
	mergefcns_help();
}

private char *
skipseq(char *p)
{
	if (anno) {
		++p;  /* skip first char */
		while (isspace(*p)) ++p;
		while (isdigit(*p)) ++p;
		assert(*p == ' ');
		++p;
	} else {
		p = strchr(p, '\t');
		assert(p);
		p++;
	}
	return (p);
}

private void
printline(char *line, int preserve_char)
{
	if (preserve_char) {
		assert(!fdiff);
		fputc(line[0], outf);
		++line;
	}
	unless (show_seq) line = skipseq(line);
	if (fdiff) fputc(' ', outf);
	fputs(line, outf);
}
			
private int
do_merge(int start, int end, FILE *inputs[3])
{
	char	**lines[3];
	u32	seq[3];
	int	i;
	char	*curr[3];
	int	ret = 0;
	u32	last_match = 0;
	
#define NEWLINE(fh) (seq[i] = (curr[i] = fagets(fh)) ? atoi(curr[i]) : ~0)
	for (i = 0; i < 3; i++) {
		NEWLINE(inputs[i]);
		lines[i] = 0;
	}
	while (1) {
		u32	max = 0;
		for (i = 0; i < 3; i++) max = seq[i] > max ? seq[i] : max;
		
		for (i = 0; i < 3; i++) {
			while (seq[i] < max) {
				lines[i] = addLine(lines[i], curr[i]);
				NEWLINE(inputs[i]);
			}
		}
		if (seq[0] == max &&
		    seq[1] == max &&
		    seq[2] == max) {
			if (max > start) {
				if (lines[0] || lines[1] || lines[2]) {
					if (resolve_conflict(lines, 
					    last_match, max)) {
						ret = 1;
					}
					lines[0] = lines[1] = lines[2] = 0;
				}
				last_match = max;
				if (end && max >= end) break;
				if (max == ~0) break;
				printline(curr[0], 0);
			}
			for (i = 0; i < 3; i++) {
				if (lines[i]) freeLines(lines[i]);
				lines[i] = 0;
				if (curr[i]) free(curr[i]);
				NEWLINE(inputs[i]);
			}
		}
	}
#undef	NEWLINE
	return (ret);
}

private char *
fagets(FILE *fh)
{
	int	size;
	int	len;
	char	*ret;

	size = 128;
	ret = malloc(size);
	len = 0;

	/*
	 * Read a line from the file with NO limit on linesize
	 */
	while (1) {
		if (!fgets(ret + len, size - len, fh)) {
			if (len) return (ret);
			free(ret);
			return 0;
		}
		len += strlen(ret + len);
		if (ret[len-1] == '\n') break;  /* stop on complete line */
		size <<= 1;
#ifdef	HAVE_REALLOC
		ret = realloc(ret, size);
#else		
		{
			/* cheap realloc()  bah! */
			char	*new;
			new = malloc(size);
			strcpy(new, ret);
			free(ret);
			ret = new;
		}
#endif
	}
	return (ret);
}


/*
 * return true if lines are the same
 */
private int
sameLines(char **a, char **b)
{
	int	i;

	EACH(a) {
		char	*al, *bl;
		unless (VALID(b, i)) return (0);
		al = strchr(a[i], anno ? '|' : '\t');
		bl = strchr(b[i], anno ? '|' : '\t');
		assert(al && bl);
		unless (streq(al, bl)) return (0);
	}
	if (VALID(b, i)) return (0);
	return (1);
}

struct region {
	char	**left;
	char	**gca;
	char	**right;
	char	**merged;
	int	automerged;
	u32	start;
	u32	end;
	region	*next;
};
private region *regions = 0;

private region *
pop_region(void)
{
	if (regions) {
		region	*ret;

		ret = regions;
		regions = regions->next;
		return (ret);
	} else {
		return (0);
	}
}

private void
push_region(region *r)
{	
	r->next = regions;
	regions = r;
}

/*
 * Define of set of autoresolve function for processing
 * a conflict each function takes an array of 3 groups of lines
 * for the local, gca, and remote.  And looks at them and
 * possiblibly prints some output with printline() and then 
 * returns. 
 * Return values:
 *    0  printed nothing, made no changes
 *    1  made some change
 *
 *   The functions can make calls to push_region() to schedule some
 *   lines to be processed after the current section is finished.
 *   The functions are not allowed to modify or delete the lines that
 *   are passed in, but they are allowed to split them into multiple
 *   regions.
 *
 * The numbers used below, once shipped, must always mean the same thing.
 * If you evolve this code, use new numbers.
 */
struct mergefcns {
	char	*name;
	int	enable;
	int	(*fcn)(region *r);
	char	*help;
} mergefcns[] = {
	{"1",	1, merge_same_changes,
	"Merge identical changes made by both sides"},
	{"2",	1, merge_only_one,
	"Merge when only one side changes"},
	{"3",	1, merge_content,
	"Merge adjacent non-overlapping modifications on both sides"},
	{"4",	1, merge_common_header,
	"Merge identical changes at the start of a conflict"},
	{"5",	1, merge_common_footer,
	"Merge identical changes at the end of a conflict"},
	{"6",	1, merge_common_deletes,
	"Merge identical deletions made by both sides"},
};
#define	N_MERGEFCNS (sizeof(mergefcns)/sizeof(struct mergefcns))

private void
enable_mergefcns(char *list, int enable)
{
	int	i;
	int	len;

	while (*list) {
		list += strspn(list, ", ");
		len = strcspn(list, ", ");
		unless (len) break;

		if (len == 3 && strneq(list, "all", 3)) {
			for (i = 0; i < N_MERGEFCNS; i++) {
				mergefcns[i].enable = enable;
			}
		} else {
			for (i = 0; i < N_MERGEFCNS; i++) {
				if (strlen(mergefcns[i].name) == len &&
				    strneq(list, mergefcns[i].name, len)) {
					mergefcns[i].enable = enable;
					break;
				}
			}
			if (i == N_MERGEFCNS) {
				fprintf(stderr, 
					"ERROR: unknown merge function '%*s'\n",
					len, list);
				exit(2);
			}
		}
		list += len;
	}
}

private void
mergefcns_help(void)
{
	int	i;

	fprintf(stderr,
"The following is a list of merge algorthms which may be enabled or disabled\n\
to change the way smerge will automerge.  Starred entries are on by default.\n");
	for (i = 0; i < N_MERGEFCNS; i++) {
		fprintf(stderr, "%2s%c %s\n", 
		    mergefcns[i].name, 
		    mergefcns[i].enable ? '*' : ' ',
		    mergefcns[i].help);
	}
}

private int
resolve_conflict(char **lines[3], u32 start, u32 end)
{
	int	i;
	region	*r;
	int	ret = 0;

	new(r);
	r->left = lines[LEFT];
	r->gca = lines[GCA];
	r->right = lines[RIGHT];
	r->start = start;
	r->end = end;
	push_region(r);
	while (regions) {
		int	changed;
		r = pop_region();
		do {
			changed = 0;
			for (i = 0; i < N_MERGEFCNS; i++) {
				unless (mergefcns[i].enable) continue;
				changed |= mergefcns[i].fcn(r);
				if (r->automerged) break;
			}
		} while (changed && !r->automerged);
		if (r->automerged) {
			/* This region was automerged */
			if (fdiff) {
				user_conflict_fdiff(r);
				fputs("Merge\n", outf);
			} 
			EACH(r->merged) {
				printline(r->merged[i], 0);
			}
			freeLines(r->merged);
		} else {
			/* found a conflict that needs to be resolved 
			 * by the user 
			 */
			ret = 1;
			if (fdiff) {
				user_conflict_fdiff(r);
			} else {
				user_conflict(r);
			}
		}
		if (fdiff) {
			fputs("End", outf);
			if (r->end) {
				fprintf(outf, " %d", r->end);
			}
			fputc('\n', outf);
		}
		freeLines(r->left);
		freeLines(r->gca);
		freeLines(r->right);
		free(r);
	}
	return (ret);
}

private void
user_conflict(region *r)
{
	int	i;
	char	**diffs;

	switch (mode) {
	    case MODE_GCA:
		unless (sameLines(r->left, r->gca)) {
			fprintf(outf, "<<<<<<< local %s %s vs %s\n", 
			    file, revs[GCA], revs[LEFT]);
			diffs = unidiff(r->gca, r->left);
			EACH(diffs) printline(diffs[i], 1);
			freeLines(diffs);
		}
		unless (sameLines(r->right, r->gca)) {
			fprintf(outf, "<<<<<<< remote %s %s vs %s\n", 
			    file, revs[GCA], revs[RIGHT]);
			diffs = unidiff(r->gca, r->right);
			EACH(diffs) printline(diffs[i], 1);
			freeLines(diffs);
		}
		fprintf(outf, ">>>>>>>\n");
		break;
	    case MODE_2WAY:
		fprintf(outf, "<<<<<<< local %s %s\n", file, revs[LEFT]);
		EACH(r->left) printline(r->left[i], 0);
		fprintf(outf, "<<<<<<< remote %s %s\n", file, revs[RIGHT]);
		EACH(r->right) printline(r->right[i], 0);
		fprintf(outf, ">>>>>>>\n");
		break;
	    case MODE_3WAY:
		fprintf(outf, "<<<<<<< gca %s %s\n", file, revs[GCA]);
		EACH(r->gca) printline(r->gca[i], 0);
		fprintf(outf, "<<<<<<< local %s %s\n", file, revs[LEFT]);
		EACH(r->left) printline(r->left[i], 0);
		fprintf(outf, "<<<<<<< remote %s %s\n", file, revs[RIGHT]);
		EACH(r->right) printline(r->right[i], 0);
		fprintf(outf, ">>>>>>>\n");
		break;
	    case MODE_NEWONLY:
		unless (sameLines(r->left, r->gca)) {
			fprintf(outf, "<<<<<<< local %s %s vs %s\n", 
			    file, revs[GCA], revs[LEFT]);
			diffs = unidiff(r->gca, r->left);
			EACH(diffs) {
				if (diffs[i][0] != '-') {
					printline(diffs[i], 1);
				}
			}
			freeLines(diffs);
		}
		unless (sameLines(r->right, r->gca)) {
			fprintf(outf, "<<<<<<< remote %s %s vs %s\n", 
			    file, revs[GCA], revs[RIGHT]);
			diffs = unidiff(r->gca, r->right);
			EACH(diffs) {
				if (diffs[i][0] != '-') {
					printline(diffs[i], 1);
				}
			}
			freeLines(diffs);
		}
		fprintf(outf, ">>>>>>>\n");
		break;
	}
}

private u32
seq(char *line)
{
	return (line ? atoi(line + 1) : 0);
}

private char *
popLine(char **lines)
{
	char	*ret;
	int	i;

	assert(lines && lines[1]);
	if (show_seq) {
		ret = lines[1];
	} else {
		char	*trail = skipseq(lines[1]);
		if (mode == MODE_2WAY) {
			ret = trail;
		} else {
			ret = malloc(strlen(lines[1]));
			ret[0] = lines[1][0];
			strcpy(ret + 1, trail);
			free(lines[1]);
		}
	}
	/* shift array down by one */
	i = 1;
	while (VALID(lines, i+1)) {
		lines[i] = lines[i + 1];
		++i;
	}
	lines[i] = 0;
	return(ret);
}
	

private void
user_conflict_fdiff(region *r)
{
	int	i;
	char	**left = 0, **right = 0;
	char	**diffs;
	char	**out_right = 0;
	u32	sl, sr;

	switch (mode) {
	    case MODE_GCA:
		left = unidiff(r->gca, r->left);
		right = unidiff(r->gca, r->right);
		break;
	    case MODE_2WAY:
		EACH(r->left) left = addLine(left, r->left[i]);
		EACH(r->right) right = addLine(right, r->right[i]);
		break;
	    case MODE_NEWONLY:
		diffs = unidiff(r->gca, r->left);
		EACH(diffs) {
			if (diffs[i][0] != '-') {
				left = addLine(left, diffs[i]);
			}
		}
		free(diffs);
		diffs = unidiff(r->gca, r->right);
		EACH(diffs) {
			if (diffs[i][0] != '-') {
				right = addLine(right, diffs[i]);
			}
		}
		free(diffs);
		break;
	}
	fputs("Left", outf);
	if (r->start) {
		printf(" %d", r->start);
	}
	fputc('\n', outf);
	sl = left ? seq(left[1]) : 0;
	sr = right ? seq(right[1]) : 0;
	while (sl && sr) {
		while (sl < sr) {
			char	*line = popLine(left);
			fputs(line, outf);
			free(line);
			out_right = addLine(out_right, strdup("s\n"));
			sl = seq(left[1]);
			unless (sl) goto done;
		}
		while (sl > sr) {
			fputs("s\n", outf);
			out_right = addLine(out_right, popLine(right));
			sr = seq(right[1]);
			unless(sr) goto done;
		}
		
		while (sl == sr) {
			char	*line = popLine(left);
			fputs(line, outf);
			free(line);
			out_right = addLine(out_right, popLine(right));
			sl = seq(left[1]);
			sr = seq(right[1]);
			unless (sl && sr) goto done;
		}
	}
 done:
	while (sl) {
		char	*line = popLine(left);
		fputs(line, outf);
		free(line);
		out_right = addLine(out_right, strdup("s\n"));
		sl = seq(left[1]);
	}
	while (sr) {
		fputs("s\n", outf);
		out_right = addLine(out_right, popLine(right));
		sr = seq(right[1]);
	}
	fputs("Right\n", outf);
	EACH (out_right) fputs(out_right[i], outf);
	freeLines(out_right);
	freeLines(left);
	freeLines(right);
}

private char **
unidiff(char **gca, char **new)
{
	char	gcafile[MAXPATH], newfile[MAXPATH];
	char	*buf;
	char	*cmd;
	FILE	*f;
	int	i;
	int	line;
	char	**out = 0;
	int	gcaline = 1;

	gettemp(gcafile, "gca");
	f = fopen(gcafile, "w");
	EACH(gca) fputs(gca[i], f);
	fclose(f);

	/* 
	 * XXX we should be able to send this command to stdin of the command
	 * below.
	 */
	gettemp(newfile, "new");
	f = fopen(newfile, "w");
	EACH(new) fputs(new[i], f);
	fclose(f);
	
	cmd = aprintf("diff --rcs --minimal %s %s", gcafile, newfile);
	f = popen(cmd, "r");
	line = 0;
	while (buf = fagets(f)) {
		char	*n;
		int	l, cnt;
		assert(buf[0] == 'd' || buf[0] == 'a');
		l = strtol(buf + 1, &n, 10);
		assert(n != buf + 1);
		cnt = strtol(n, 0, 10);
		if (buf[0] == 'd') {
			while (line < l - 1) {
				n = malloc(strlen(gca[gcaline])+2);
				n[0] = ' ';
				strcpy(n + 1, gca[gcaline]);
				++gcaline;
				out = addLine(out, n);
				++line;
			}
			while (cnt--) {
				n = malloc(strlen(gca[gcaline])+2);
				n[0] = '-';
				strcpy(n + 1, gca[gcaline]);
				++gcaline;
				out = addLine(out, n);
				++line;
			}
		} else if (buf[0] == 'a') {
			while (line < l) {
				n = malloc(strlen(gca[gcaline])+2);
				n[0] = ' ';
				strcpy(n + 1, gca[gcaline]);
				++gcaline;
				out = addLine(out, n);
				++line;
			}
			while (cnt--) {
				char	*data = fagets(f);
				assert(data);
				n = malloc(strlen(data) + 2);
				n[0] = '+';
				strcpy(n + 1, data);
				free(data);
				out = addLine(out, n);
			}
		}
		free(buf);
	}
	while (VALID(gca, gcaline)) {
		char	*n;
		n = malloc(strlen(gca[gcaline]) + 2);
		n[0] = ' ';
		strcpy(n + 1, gca[gcaline]);
		++gcaline;
		out = addLine(out, n);
	}
	pclose(f);
	unlink(gcafile);
	unlink(newfile);
	return (out);
}

private void
show_examples(void)
{
	fputs(
"Summary of bk smerge output formats\n\
default		(3 way format (shows gca))\n\
    <<<<<<< gca slib.c 1.642.1.6\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);\n\
    		  assert(sc->tree);\n\
    		  sccs_sdelta(sc, sc->tree, file);\n\
    <<<<<<< local slib.c 1.645\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);\n\
    		  assert(HASGRAPH(sc));\n\
    		  sccs_sdelta(sc, sccs_ino(sc), file);\n\
    <<<<<<< remote slib.c 1.642.2.1\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, p);\n\
    		  assert(sc->tree);\n\
    		  sccs_sdelta(sc, sc->tree, file);\n\
    >>>>>>>\n\
\n\
-g	(Shows local and remove files as a diff from the GCA)\n\
    <<<<<<< local slib.c 1.642.1.6 vs 1.645\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);\n\
    -             assert(sc->tree);\n\
    -             sccs_sdelta(sc, sc->tree, file);\n\
    +             assert(HASGRAPH(sc));\n\
    +             sccs_sdelta(sc, sccs_ino(sc), file);\n\
    <<<<<<< remote slib.c 1.642.1.6 vs 1.642.2.1\n\
    -             sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);\n\
    +             sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, p);\n\
    		  assert(sc->tree);\n\
    		  sccs_sdelta(sc, sc->tree, file);\n\
    >>>>>>>\n\
", stdout);
	fputs(
"-2	(2 way format (like diff3))\n\
    <<<<<<< local slib.c 1.645\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);\n\
    		  assert(HASGRAPH(sc));\n\
    		  sccs_sdelta(sc, sccs_ino(sc), file);\n\
    <<<<<<< remote slib.c 1.642.2.1\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, p);\n\
    		  assert(sc->tree);\n\
    		  sccs_sdelta(sc, sc->tree, file);\n\
    >>>>>>>\n\
\n\
-n	(newonly (like -2 except marks added lines))\n\
    <<<<<<< local slib.c 1.642.1.6 vs 1.645\n\
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);\n\
    +             assert(HASGRAPH(sc));\n\
    +             sccs_sdelta(sc, sccs_ino(sc), file);\n\
    <<<<<<< remote slib.c 1.642.1.6 vs 1.642.2.1\n\
    +             sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, p);\n\
    		  assert(sc->tree);\n\
    		  sccs_sdelta(sc, sc->tree, file);\n\
    >>>>>>>\n\
", stdout);
}

private	int
parse_range(char *range, int *start, int *end)
{
	if (isdigit(range[0])) {
		*start = strtol(range, &range, 10);
	}
	if (range[0] == 0 || range[0] != '.' || range[1] != '.') {
		return (-1);
	}
	range += 2;
	if (isdigit(range[0])) {
		*end = strtol(range, 0, 10);
	}
	return(0);
}

/*--------------------------------------------------------------------
 * Automerge functions
 *--------------------------------------------------------------------
 */

/*
 * All the lines on both sides are identical.
 */
private int
merge_same_changes(region *r)
{
	int	i;

	if (sameLines(r->left, r->right)) {
		EACH(r->left) {
			/* XXX add char to indicate 'both' */
			r->merged = addLine(r->merged, strdup(r->left[i]));
		}
		r->automerged = 1;
		return (1);
	}
	return (0);
}

/*
 * Only one side make changes
 */
private int
merge_only_one(region *r)
{
	int	i;
	if (sameLines(r->left, r->gca)) {
		EACH(r->right) {
			/* add state to indicate 'right' ? */
			r->merged = addLine(r->merged, strdup(r->right[i]));
		}
		r->automerged = 1;
		return (1);
	}
	if (sameLines(r->right, r->gca)) {
		EACH(r->left) {
			/* add state to indicate 'left' ? */
			r->merged = addLine(r->merged, strdup(r->left[i]));
		}
		r->automerged = 1;
		return (1);
	}
	return (0);
}



private char **
lines_modified(char **diff)
{
	char	**out = 0;
	int	i;
	int	saw_deletes = 0;
	int	saw_adds = 0;

	EACH (diff) {
		switch (diff[i][0]) {
		    case ' ': break;
		    case '-':
			if (saw_adds) goto bad;
			saw_deletes = 1;
			out = addLine(out, strdup(diff[i]));
			break;
		    case '+':
			unless (saw_deletes) goto bad;
			saw_adds = 1;
			break;
		}
	}
	return (out);
 bad:
	freeLines(out);
	return (0);
}

private int
are_unmodified(char **diff, char **lines)
{
	int	i;
	int	lcnt = 1;
	u32	s;

	s = seq(lines[lcnt]);
	EACH (diff) {
		if (s < seq(diff[i])) return (0);
		if (s == seq(diff[i])) {
			if (diff[i][0] != ' ') return (0);
			++lcnt;
			s = seq(lines[lcnt]);
			unless (s) return (1);
		}
	}
	return (0);
}
	
private int
merge_content(region *r)
{
	char	**left, **right;
	char	**modified;
	int	i;
	int	ret = 0;
	int	rline;
	int	ok;

	left = unidiff(r->gca, r->left);
	right = unidiff(r->gca, r->right);
	
	modified = lines_modified(left);
	unless (modified) goto bad;
	ok = are_unmodified(right, modified);
	freeLines(modified);
	unless (ok) goto bad;

	modified = lines_modified(right);
	unless (modified) goto bad;
	ok = are_unmodified(left, modified);
	freeLines(modified);
	unless (ok) goto bad;
	
	/* we are good to go, do merge */
	rline = 1;
	EACH(left) {
		while (VALID(right, rline) && 
		       seq(right[rline]) < seq(left[i])) {
			if (right[i][0] == '+') {
				/* XXX add char to indicate 'right' ? */
				r->merged = addLine(r->merged, 
						    strdup(right[rline] + 1));
			}
			++rline;
		}
		if (VALID(right, rline) && seq(right[rline]) == seq(left[i])) {
			/* deleted line, ignore */
			if (!((left[i][0] == '-' && right[rline][0] == ' ') ||
			      (left[i][0] == ' ' && right[rline][0] == '-'))) {
				fprintf(stderr, "ERROR:\n\t%s\t%s", 
				    left[i], right[rline]);
				exit(2);
			}
			rline++;
		} else {
			assert(left[i][0] == '+');
			/* XXX add char to indicate 'left' ? */
			r->merged = addLine(r->merged, 
					    strdup(left[i] + 1));
		}
	}
	while (VALID(right, rline)) {
		if (right[rline][0] == '+') {
			/* XXX add char to indicate 'right' ? */
			r->merged = addLine(r->merged, 
					    strdup(right[rline] + 1));
		}
		++rline;
	}
	r->automerged = 1;
	ret = 1;
 bad:
	freeLines(left);
	freeLines(right);
	return (ret);
}

private int
merge_common_header(region *r)
{
	char	**a = r->left;
	char	**b = r->right;
	int	i;
	int	save_idx;
	int	last_seq;
	region	*newr;

	EACH(a) {
		char	*al, *bl;
		unless (VALID(b, i)) break;
		al = strchr(a[i], anno ? '|' : '\t');
		bl = strchr(b[i], anno ? '|' : '\t');
		assert(al && bl);
		unless (streq(al, bl)) break;
	}
	if (i == 1) return (0);
	save_idx = i;
	last_seq = seq(a[i-1]);

	/* save common header and move rest to a new region */
	new(newr);
	EACH (a) {
		if (i >= save_idx) {
			newr->left = addLine(newr->left, a[i]);
			a[i] = 0;
		}
	}
	EACH (b) {
		if (i >= save_idx) {
			newr->right = addLine(newr->right, b[i]);
			b[i] = 0;
		}
	}
	EACH (r->gca) {
		if (seq(r->gca[i]) > last_seq) {
			newr->gca = addLine(newr->gca, r->gca[i]);
			r->gca[i] = 0;
		}
	}
	newr->end = r->end;
	r->end = 0;
	push_region(newr);

	return (1);
}

private int
merge_common_footer(region *r)
{
	char	**a = r->left;
	char	**b = r->right;
	int	i;
	int	end_a, end_b;
	int	ca, cb;
	region	*newr;
	u32	last_seq;

	EACH (a);
	ca = end_a = i - 1;
	EACH (b);
	cb = end_b = i - 1;

	unless (ca > 1 && cb > 1) return (0);
	
	while (ca > 0 && cb > 0 && VALID(a, ca) && VALID(b, cb)) {
		char	*al, *bl;
		al = strchr(a[ca], anno ? '|' : '\t');
		bl = strchr(b[cb], anno ? '|' : '\t');
		assert(al && bl);
		unless (streq(al, bl)) break;
		--ca;
		--cb;
	}
	unless (ca < end_a && cb < end_b) return (0);

	/* push common lines to new region */
	new(newr);
	for (i = ca + 1; VALID(a, i); ++i) {
		newr->left = addLine(newr->left, a[i]);
		a[i] = 0;
	}
	for (i = cb + 1; VALID(b, i); ++i) {
		newr->right = addLine(newr->right, b[i]);
		b[i] = 0;
	}
	/* move lines from GCA that are after these lines to new region */
	last_seq = min(seq(newr->left[1]), seq(newr->right[1]));
	EACH(r->gca) {
		if (seq(r->gca[i]) >= last_seq) {
			newr->gca = addLine(newr->gca, r->gca[i]);
			r->gca[i] = 0;
		}
	}
	newr->end = r->end;
	r->end = 0;
	push_region(newr);

	return (1);
}
	
private int
merge_common_deletes(region *r)
{
	char	**left, **right;
	int	i, j;
	int	ret = 0;
	int	cnt;
	int	len;
	region	*newr;

	left = unidiff(r->gca, r->left);
	right = unidiff(r->gca, r->right);

#if 0	
	/* 
	 * remove matching deletes at beginning
	 */
	EACH (left) if (left[i][0] != '-') break;
	cnt = i - 1;
	EACH (right) if (right[i][0] != '-') break;
	cnt = min(cnt, i - 1);
	
	if (cnt > 0) {
		ret = 1;
		lines_trim_head(r->gca, cnt);
	}
#endif
	
	/*
	 * move matching deletes at end to new region
	 */
	EACH (left);
	len = i - 1;
	for (j = len; j >= 1; --j) {
		if (left[j][0] != '-') break;
	}
	cnt = len - j;
	EACH (right);
	len = i - 1;
	for (j = len; j >= 1; --j) {
		if (right[j][0] != '-') break;
	}
	cnt = min(cnt, len - j);

	if (cnt > 0) {
		EACH (r->gca);
		len = i - 1;
		if (cnt < len) {
			new(newr);
			while (cnt > 0) {
				--i;
				--cnt;
				newr->gca = addLine(newr->gca,
						    r->gca[i]);
				r->gca[i] = 0;
			}
			newr->end = r->end;
			r->end = 0;
			push_region(newr);
			ret = 1;
		}
	}

	freeLines(left);
	freeLines(right);
	return (ret);
}

