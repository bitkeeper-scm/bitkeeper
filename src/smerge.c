#include "system.h"
#include "sccs.h"

private int	do_merge(int start, int end, FILE *inputs[3]);
private void	usage(void);
private char	*fagets(FILE *fh);
private int	resolve_conflict(char **lines[3]);
private int	fdiff_resolve_conflict(char **lines[3]);
private char	**unidiff(char **gca, char **new);
private void	show_examples(void);
private	int	parse_range(char *range, int *start, int *end);

enum {
	MODE_GCA,
	MODE_2WAY,
	MODE_3WAY,
	MODE_NEWONLY
};

enum {
	LOCAL,
	GCA, 
	REMOTE
};

private	char	*revs[3];
private	char	*file;
private	int	automerge = 0;
private	int	mode = MODE_GCA;
private	int	show_seq;
private	int	merge_common = 1;
private	int	fdiff;
private	FILE	*fl, *fr;

int
smerge_main(int ac, char **av)
{
	int	c;
	int	i;
	char	buf[MAXPATH];
	FILE	*inputs[3];
	int	start = 0, end = 0;
	int	ret;

	while ((c = getopt(ac, av, "23Aaefhnr:s")) != -1) {
		switch (c) {
		    case '2': /* 2 way format (like diff3) */
			mode = MODE_2WAY;
			break;
		    case '3': /* 3 way format (shows gca) */
			mode = MODE_3WAY;
			break;
		    case 'n': /* newonly (like -2 except marks added lines) */
			mode = MODE_NEWONLY;
			break;
		    case 'a': /* automerge non-overlapping changes */
			automerge = 1;
			break;
		    case 'f': /* fdiff output mode */
			fdiff = 1;
			break;
		    case 'A': /* do not collapse same changes on both sides */
			merge_common = 0;
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
	if (mode == MODE_3WAY && fdiff) {
		fprintf(stderr, "3way mode is not legal with fdiff output\n");
		return (2);
	}
	file = av[optind + 3];
	for (i = 0; i < 3; i++) {
		revs[i] = av[optind + i];
		sprintf(buf, "bk get -qkpOr%s %s", revs[i], file);
		inputs[i] = popen(buf, "r");
		assert(inputs[i]);
	}
	if (fdiff) {
		char *left = "/tmp/left";
		char *right = "/tmp/right";

		puts(left);
		puts(right);
		fl = fopen(left, "w");
		fr = fopen(right, "w");
		assert(fl && fr);
	}
	ret = do_merge(start, end, inputs);
	for (i = 0; i < 3; i++) {
		int	status;
		status = pclose(inputs[i]);
		if (status) {
			fprintf(stderr, 
				"Fetch of revision %s of file %s failed.\n",
				revs[i], file);
			ret = 2;
		}
	}
	if (fdiff) {
		fclose(fl);
		fclose(fr);
	}
	return (ret);
}

private void
usage(void)
{
	system("bk help -s smerge");
}

private void
printline(char *line, int preserve_char)
{
	if (preserve_char) {
		assert(!fdiff);
		fputc(line[0], stdout);
		++line;
	}
	unless (show_seq) {
		line = strchr(line, '\t');
		assert(line);
		++line;
	}
	if (fdiff) {
		fputc(' ', fl);
		fputs(line, fl);
		fputc(' ', fr);
		fputs(line, fr);
	} else {
		fputs(line, stdout);
	}
}
			
private int
do_merge(int start, int end, FILE *inputs[3])
{
	char	**lines[3];
	u32	seq[3];
	int	i;
	char	*curr[3];
	int	ret = 0;
	
#define NEWLINE(fh) curr[i] = fagets(fh); \
		seq[i] = curr[i] ? atoi(curr[i]) : ~0;

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
					if (fdiff) {
						if (fdiff_resolve_conflict(lines)) {
							ret = 1;
						}
					} else {
						if (resolve_conflict(lines)) {
							ret = 1;
						}
					}
				}
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
		if (len + 1 < size) break;
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

	if (a == b) return (1);
	unless (a && b) return (0);
	EACH(a) {
		char	*al, *bl;
		unless (b[i]) return (0);
		al = strchr(a[i], '\t');
		bl = strchr(b[i], '\t');
		assert(al && bl);
		unless (streq(al, bl)) return (0);
	}
	if (b[i]) return (0);
	return (1);
}

private int
do_automerge(char **lines[3])
{
	int	i;

	if (merge_common && sameLines(lines[LOCAL], lines[REMOTE])) {
		EACH(lines[LOCAL]) printline(lines[LOCAL][i], 0);
		return (1);
	}
	if (automerge) {
		if (sameLines(lines[LOCAL], lines[GCA])) {
			EACH(lines[REMOTE]) printline(lines[REMOTE][i], 0);
			return (1);
		}
		if (sameLines(lines[REMOTE], lines[GCA])) {
			EACH(lines[LOCAL]) printline(lines[LOCAL][i], 0);
			return (1);
		}
	}
	return (0);
}

private int
resolve_conflict(char **lines[3])
{
	int	i;
	char	**diffs;

	if (do_automerge(lines)) return (0);

	switch (mode) {
	    case MODE_GCA:
		unless (sameLines(lines[LOCAL], lines[GCA])) {
			printf("<<< local %s %s vs %s\n", 
			       file, revs[GCA], revs[LOCAL]);
			diffs = unidiff(lines[GCA], lines[LOCAL]);
			EACH(diffs) printline(diffs[i], 1);
			freeLines(diffs);
		}
		unless (sameLines(lines[REMOTE], lines[GCA])) {
			printf("<<< remote %s %s vs %s\n", 
			       file, revs[GCA], revs[REMOTE]);
			diffs = unidiff(lines[GCA], lines[REMOTE]);
			EACH(diffs) printline(diffs[i], 1);
			freeLines(diffs);
		}
		printf(">>>\n");
		break;
	    case MODE_2WAY:
		printf("<<< local %s %s\n", file, revs[LOCAL]);
		EACH(lines[LOCAL]) printline(lines[LOCAL][i], 0);
		printf("<<< remote %s %s\n", file, revs[REMOTE]);
		EACH(lines[REMOTE]) printline(lines[REMOTE][i], 0);
		printf(">>>\n");
		break;
	    case MODE_3WAY:
		printf("<<< gca %s %s\n", file, revs[GCA]);
		EACH(lines[GCA]) printline(lines[GCA][i], 0);
		printf("<<< local %s %s\n", file, revs[LOCAL]);
		EACH(lines[LOCAL]) printline(lines[LOCAL][i], 0);
		printf("<<< remote %s %s\n", file, revs[REMOTE]);
		EACH(lines[REMOTE]) printline(lines[REMOTE][i], 0);
		printf(">>>\n");
		break;
	    case MODE_NEWONLY:
		unless (sameLines(lines[LOCAL], lines[GCA])) {
			printf("<<< local %s %s vs %s\n", 
			       file, revs[GCA], revs[LOCAL]);
			diffs = unidiff(lines[GCA], lines[LOCAL]);
			EACH(diffs) {
				if (diffs[i][0] != '-') printline(diffs[i], 1);
			}
			freeLines(diffs);
		}
		unless (sameLines(lines[REMOTE], lines[GCA])) {
			printf("<<< remote %s %s vs %s\n", 
			       file, revs[GCA], revs[REMOTE]);
			diffs = unidiff(lines[GCA], lines[REMOTE]);
			EACH(diffs) {
				if (diffs[i][0] != '-') printline(diffs[i], 1);
			}
			freeLines(diffs);
		}
		printf(">>>\n");
		break;
	}
	return (1);
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
		char	*trail = strchr(lines[1], '\t') + 1;
		if (mode == MODE_2WAY) {
			ret = trail;
		} else {
			ret = malloc(strlen(lines[1]));
			ret[0] = lines[1][0];
			strcpy(ret + 1, trail);
			free(lines[1]);
		}
	}
	for (i = 1; lines[i]; i++) lines[i] = lines[i + 1];
	return(ret);
}
	

private int
fdiff_resolve_conflict(char **lines[3])
{
	int	i;
	char	**left = 0, **right = 0;
	char	**diffs;
	int	sl, sr;

	if (do_automerge(lines)) return (0);

	switch (mode) {
	    case MODE_GCA:
		left = unidiff(lines[GCA], lines[LOCAL]);
		right = unidiff(lines[GCA], lines[REMOTE]);
		break;
	    case MODE_2WAY:
		EACH(lines[LOCAL]) left = addLine(left, lines[LOCAL][i]);
		EACH(lines[REMOTE]) right = addLine(right, lines[REMOTE][i]);
		break;
	    case MODE_NEWONLY:
		diffs = unidiff(lines[GCA], lines[LOCAL]);
		EACH(diffs) {
			if (diffs[i][0] != '-') {
				left = addLine(left, diffs[i]);
			}
		}
		free(diffs);
		diffs = unidiff(lines[GCA], lines[REMOTE]);
		EACH(diffs) {
			if (diffs[i][0] != '-') {
				right = addLine(right, diffs[i]);
			}
		}
		free(diffs);
		break;
	}
	sl = left ? seq(left[1]) : 0;
	sr = right ? seq(right[1]) : 0;
	while (sl && sr) {
		while (sl < sr) {
			fputs(popLine(left), fl);
			fputs("s\n", fr);
			sl = seq(left[1]);
			unless (sl) goto done;
		}
 		while (sl > sr) {
			fputs("s\n", fl);
			fputs(popLine(right), fr);
			sr = seq(right[1]);
			unless(sr) goto done;
		}
		while (sl == sr) {
			fputs(popLine(left), fl);
			fputs(popLine(right), fr);
			sl = seq(left[1]);
			sr = seq(right[1]);
			unless (sl && sr) goto done;
		}
	}
 done:
	while (sl) {
		fputs(popLine(left), fl);
		fputs("s\n", fr);
		sl = seq(left[1]);
	}
	while (sr) {
		fputs("s\n", fl);
		fputs(popLine(right), fr);
		sr = seq(right[1]);
	}
	freeLines(left);
	freeLines(right);
	return (1);
}

private char **
unidiff(char **gca, char **new)
{
	char	gcafile[MAXPATH], newfile[MAXPATH];
	char	buf[128];
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
	
	cmd = aprintf("diff --rcs %s %s", gcafile, newfile);
	f = popen(cmd, "r");
	line = 0;
	while (fnext(buf, f)) {
		char	*n;
		int	l, cnt;
		assert(buf[0] == 'd' || buf[0] == 'a');
		l = strtol(buf + 1, &n, 10);
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
				fnext(buf, f);
				n = malloc(strlen(buf) + 2);
				n[0] = '+';
				strcpy(n + 1, buf);
				out = addLine(out, n);
			}
		}
	}
	while (gca && gcaline < (int)gca[0] && gca[gcaline]) {
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
"Summary of bk smerge output formats
 default
    <<< local slib.c 1.642.1.6 vs 1.645
    		   sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);
    -             assert(sc->tree);
    -             sccs_sdelta(sc, sc->tree, file);
    +             assert(HASGRAPH(sc));
    +             sccs_sdelta(sc, sccs_ino(sc), file);
    <<< remote slib.c 1.642.1.6 vs 1.642.2.1
    -             sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);
    +             sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, p);
    		  assert(sc->tree);
    		  sccs_sdelta(sc, sc->tree, file);
    >>>

 -2  (2 way format (like diff3))
    <<< local slib.c 1.645
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);
    		  assert(HASGRAPH(sc));
    		  sccs_sdelta(sc, sccs_ino(sc), file);
    <<< remote slib.c 1.642.2.1
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, p);
    		  assert(sc->tree);
    		  sccs_sdelta(sc, sc->tree, file);
    >>>

 -3  (3 way format (shows gca)      
    <<< gca slib.c 1.642.1.6
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);
    		  assert(sc->tree);
    		  sccs_sdelta(sc, sc->tree, file);
    <<< local slib.c 1.645
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);
    		  assert(HASGRAPH(sc));
    		  sccs_sdelta(sc, sccs_ino(sc), file);
    <<< remote slib.c 1.642.2.1
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, p);
    		  assert(sc->tree);
    		  sccs_sdelta(sc, sc->tree, file);
    >>>

 -n  ( newonly (like -2 except marks added lines))
    <<< local slib.c 1.642.1.6 vs 1.645
    		  sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, s->proj);
    +             assert(HASGRAPH(sc));
    +             sccs_sdelta(sc, sccs_ino(sc), file);
    <<< remote slib.c 1.642.1.6 vs 1.642.2.1
    +             sc = sccs_init(file, INIT_NOCKSUM|INIT_SAVEPROJ, p);
    		  assert(sc->tree);
    		  sccs_sdelta(sc, sc->tree, file);
    >>>
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
	
	

