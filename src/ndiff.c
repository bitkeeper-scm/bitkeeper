#include "system.h"
#include "sccs.h"

/*
 * This struct is for diff -p, so we can save where
 * the function headers are.
 */
typedef	struct {
	int	lno;		/* line number */
	char	*s;		/* string to print */
} header;

/*
 * One of these for each file we are diffing.
 */
typedef	struct {
	u32	nl:1;		/* does it have a newline at the end? */
	u32	binary:1;	/* set if binary */
} file;

typedef struct {
	file	files[2];	/* the two files to diff */
	header	*fn_defs;	/* function defs for diff -p */
	char	**bl[2];	/* # of lines in left/right for sdiff */
	int	state;		/* for sdiff */
	df_opt	*dop;		/* copy of diff opts */
} filedf;


/* comparison funcs */
private	int	cmp_identical(void *a, int lena,
    void *b, int lenb, int last, void *extra);
private	int	cmp_ignore_ws(void *va, int lena,
    void *vb, int lenb, int last, void *extra);
private	int	cmp_ignore_ws_chg(void *va, int lena,
    void *vb, int lenb, int last, void *extra);

/* hashing funcs */
private	u32	hash_identical(void *data, int len,
    int side, int last, void *extra);
private	u32	hash_ignore_ws(void *buf, int len,
    int side, int last, void *extra);
private	u32	hash_ignore_ws_chg(void *data, int len,
    int side, int last, void *extra);

private void	printLine(void *data, int len, int side, int last,
    void *extra, FILE *out);
private	void	printHeader(int lno, void *extra, FILE *out);
private	void	printDeco(u32 where, void *extra, FILE *out);

private void	sdPrint(void *data, int len, int side, int last,
    void *extra, FILE *out);
private void	sdState(u32 where, void *extra, FILE *out);

/* other functions */
private u32	align(void *data, int len, int pos, void *extra);

private	int	external_diff(char *lfile, char *rfile, df_opt *dop, char *out);

private void	printLines(filedf *o, char *left, char *right, FILE *out);
private void	alignMods(filedf *o, char **A, char **B, FILE *out);
private int	lcs(char *a, int alen, char *b, int blen);

int
ndiff_main(int ac, char **av)
{
	int	c, i;
	char	*p, *pattern = 0;
	const	char *perr;
	int	poff;
	df_opt	opts;
	longopt	lopts[] = {
		{ "ignore-trailing-cr", 310 },
		{ "sdiff|", 315},
		{ "show-function-line", 'F'},
		{ "strip-trailing-cr", 320 },
		{ "print-hunks", 330},
		{ 0, 0 }
	};
	int	rc = -1;

	bzero(&opts, sizeof(df_opt));
	while ((c = getopt(ac, av, "bdD:F:pu|wnN", lopts)) != -1) {
		switch (c) {
		    case 'b': opts.ignore_ws_chg = 1; break;
		    case 'd': opts.minimal = 1; break;
		    case 'D': opts.out_define = strdup(optarg);	break;
		    case 'p': opts.out_show_c_func = 1; break;
		    case 'u':
			opts.out_unified = 1;
			if (optarg && isdigit(optarg[0])) {
				i = strtoul(optarg, &p, 10);
				opts.context = i ? i : -1; /* -1 means zero */
				if (*p) getoptConsumed(p - optarg + 1);
			} else if (optarg) {
				getoptConsumed(1);
			}
			break;
		    case 'w': opts.ignore_all_ws = 1; break;
		    case 'n': opts.out_rcs = 1; break;
		    case 'N': opts.new_is_null = 1; break;
		    case 'F': pattern = strdup(optarg); break;
		    case 310:	/* --ignore-trailing-cr */
			opts.ignore_trailing_cr = 1;
			break;
		    case 315:	/* --sdiff */
			opts.out_sdiff = optarg? strtoul(optarg, 0, 10) : 130;
			break;
		    case 320:   /* --strip-trailing-cr */
			opts.strip_trailing_cr = 1;
			break;
		    case 330:	/* --print-hunks */
			opts.out_print_hunks = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	unless (ac - optind == 2) {
		fprintf(stderr, "diff: missing operand after `%s'\n",
		    av[ac-1]);
		goto out;
	}
	if ((opts.out_unified + !!opts.out_define + opts.out_rcs +
	    !!opts.out_sdiff + opts.out_print_hunks) > 1) {
		fprintf(stderr, "diff: conflicting output style options\n");
		goto out;
	}
	if (opts.out_show_c_func && pattern) {
		fprintf(stderr, "diff: only one of -p or -F allowed\n");
		goto out;
	}

	if (pattern &&
	    !(opts.pattern = pcre_compile(pattern, 0, &perr, &poff, 0))) {
		fprintf(stderr, "diff: bad regexp '%s': %s\n", pattern, perr);
		goto out;
	}

	rc = diff_files(av[optind], av[optind+1], &opts, 0, "-");
out:	if (opts.out_define) FREE(opts.out_define);
	if (opts.pattern) free(opts.pattern);
	if (pattern) FREE(pattern);
	return (rc);
}

int
diff_cleanOpts(df_opt *opts)
{
	const	char *perr;
	int	poff;

	/*
	 * Make sure we don't have conflicting output styles
	 */
	if ((opts->out_unified + !!opts->out_define + opts->out_rcs +
		!!opts->out_sdiff + opts->out_print_hunks) > 1) return (0);
	/*
	 * --ignore-trailing-cr implies --strip-tailing-cr
	 */
	if (opts->ignore_trailing_cr) opts->strip_trailing_cr = 1;

	/*
	 * provide a default pattern if they didn't give us one.
	 */
	if (opts->out_show_c_func && !opts->pattern) {
		opts->pattern = pcre_compile("^[[:alpha:]$_]",
		    0, &perr, &poff, 0);
		assert(opts->pattern);
	}

	if (opts->context) {
		if (opts->context < 0) opts->context = 0; /* -1 means zero */
	} else {
		/* default is 3 lines of context */
		opts->context = 3;
	}
	return (1);
}

/*
 * Return:
 *   0  same
 *   1  differ
 *   2  error
 */
int
diff_files(char *file1, char *file2, df_opt *dop, df_ctx **odc, char *out)
{
	int	i, j, n;
	int	firstDiff;
	int	lno[2];
	int	skip[2] = {0, 0};
	int	rc = 2, hasctrlr = 0;
	char	*files[2] = {file1, file2};
	char	*data[2] = {0, 0};
	df_cmp	dcmp = 0;
	df_hash	dhash = 0;
	header	*fh;
	df_ctx	*dc = 0;
	filedf	*o;
	pcre	*re = 0;
	FILE	*fout = 0;
	struct	stat sb[2];

	if (getenv("_BK_USE_EXTERNAL_DIFF")) {
		return (external_diff(file1, file2, dop, out));
	}

	unless (diff_cleanOpts(dop)) return (2);

	if (out) {
		fout = (streq(out, "-")) ? stdout : fopen(out, "w");
		unless (fout) return (2);
	}

	o = new(filedf);
	o->dop = new(df_opt);
	*o->dop = *dop;

	for (i = 0; i < 2; i++) {
		if (stat(files[i], &sb[i])) {
			unless (o->dop->new_is_null) {
				perror(files[i]);
				return (2);
			}
			skip[i] = 1;
		}
	}

	/* Maybe they gave us the same file? */
	if (!win32() &&
	    (sb[0].st_dev == sb[1].st_dev) && (sb[0].st_ino == sb[1].st_ino)) {
		/* no diffs */
		return (0);
	}

	/* Load the two files into memory. */
	for (i = 0; i < 2; i++) {
		if (skip[i]) continue;
		unless (data[i] = loadfile(files[i], 0)) {
			perror(files[i]);
			rc = 2;
			goto out;
		}
	}

	/*
	 * If the two files are the same size, or if we are just
	 * seeing if they differ and don't care about the actual
	 * diffs, then do a quick comparison.
	 */
	if ((sb[0].st_size == sb[1].st_size) &&
	    !memcmp(data[0], data[1], sb[0].st_size)) {
		/* no diffs */
		rc = 0;
		goto out;
	}

	/*
	 * Set up the right functions for running the diff.
	 */
	if (o->dop->ignore_all_ws) {
		dcmp  = cmp_ignore_ws;
		dhash = hash_ignore_ws;
	} else if (o->dop->ignore_ws_chg) {
		dcmp  = cmp_ignore_ws_chg;
		dhash = hash_ignore_ws_chg;
	} else {
		dcmp  = cmp_identical;
		dhash = hash_identical;
	}

	dc = diff_new(dcmp, dhash, align, o);

	re = o->dop->pattern;

	lno[0] = lno[1] = 0;

	for (i = 0; i < 2; i++) {
		char	*s, *e, *t = 0;	/* start/end/tmp */

		s = e = data[i];
		for (j = 0; j < sb[i].st_size; j++) {
			if (*e == '\0') o->files[i].binary = 1;
			if (*e == '\n') {
				if (o->dop->strip_trailing_cr) {
					t = e;
					while ((e > s) && (*(e-1)=='\r')) e--;
					if (e != t) hasctrlr = 1;
					*e = '\n';
				}
				diff_addItem(dc, i, s, e - s + 1);
				lno[i]++;
				if (re && (i == 0)) {
					unless (pcre_exec(re, 0, s, e - s + 1,
						0, 0, 0, 0)) {
						fh = addArray(&o->fn_defs,0);
						fh->lno = lno[0];
						fh->s = strndup(s, e - s + 1);
						chomp(fh->s);
						trim(fh->s);
					}
				}
				if (o->dop->strip_trailing_cr) e = t;
				s = e + 1;
			}
			e++;
		}
		o->files[i].nl = 1;
		if (j && (*(e-1) != '\n')) {
			if (o->dop->ignore_trailing_cr && (*(e-1) == '\r')) {
				t = e;
				while ((e > s) && (*(e-1) == '\r')) e--;
				if (e != t) hasctrlr = 1;
				*e = '\n';
			} else {
				e--;
				o->files[i].nl = 0;
			}
			diff_addItem(dc, i, s, e - s + 1);
			lno[i]++;
		}
	}

	if (o->files[0].binary || o->files[1].binary) {
		/*
		 * We already know the files can't be identical as
		 * we've checked for same size and equal contents
		 * above.
		 */
		if (fout) {
			fprintf(fout,
			    "Binary files %s and %s differ\n", file1, file2);
		}
		rc = 1;
		goto out;
	}

	/*
	 * This is an optimization where we just compare as much as we
	 * can to avoid hashing.
	 */
	n = min(sb[0].st_size, sb[1].st_size);
	firstDiff = 0;
	unless (hasctrlr) {
		for (i = 0; i < n; i++) {
			if (data[0][i] != data[1][i]) break;
			if (data[0][i] == '\n') firstDiff++;
		}
	}
	assert((firstDiff <= lno[0]) && (firstDiff <= lno[1]));
	/* Do the diff */
	diff_items(dc, firstDiff, o->dop->minimal);
	unless (nLines(diff_hunks(dc))) {
		rc = 0;
		goto out;
	}
	rc = 1;			/* difference found */
	if (odc) *odc = dc;
	unless (out) goto out;

	/* Print the diffs. */
	if (o->dop->out_unified) {
		diff_printUnified(dc,
		    file1, &sb[0].st_mtime,
		    file2, &sb[1].st_mtime,
		    o->dop->context,
		    printLine, o->dop->pattern ? printHeader : 0, fout);
	} else if (o->dop->out_rcs) {
		diff_printRCS(dc, printLine, fout);
	} else if (o->dop->out_define) {
		diff_printDecorated(dc, printLine, printDeco, fout);
	} else if (o->dop->out_sdiff) {
		diff_printDecorated(dc, sdPrint, sdState, fout);
	} else if (o->dop->out_print_hunks) {
		hunk	*h, *hlist = diff_hunks(dc);

		EACHP(hlist, h) {
			fprintf(fout,
			    "%d,%d %d,%d\n", h->li, h->ll, h->ri, h->rl);
		}
	} else {
		diff_print(dc, printLine, fout);
	}
out:	if (fout && (fout != stdout)) fclose(fout);
	FREE(data[0]);
	FREE(data[1]);
	FREE(o->dop);
	FREE(o);
	unless (odc) diff_free(dc);
	return (rc);
}

private int
external_diff(char *lfile, char *rfile, df_opt *dop, char *out)
{
	int	fd1 = -1;
	int	status = -1, i;
	int	ret;
	char	*av[40];
	char	def[MAXLINE];

	unless (streq("-", out)) {
		int	fd;

		fd1 = dup(1);
		close(1); /* this confused purify, it expect a fclose */
		fd = open(out, O_CREAT|O_WRONLY, 0644);
		if (fd == -1) {
			perror(out);
			return (2);
		}
		assert(fd == 1);
		debug((stderr, "diff: out = %s\n", out));
	}

	av[i = 0] = "diff";
	if (dop->ignore_ws_chg) av[++i] = "-b";
	if (dop->out_show_c_func) av[++i] = "-p";
	if (dop->ignore_all_ws) av[++i] = "-w";
	if (dop->new_is_null) av[++i] = "-N";
	if (dop->out_define) {
		sprintf(def, "-D%s", dop->out_define);
		av[++i] = def;
	}
	if (dop->out_rcs) av[++i] = "-n";
	if (dop->out_unified) av[++i] = "-u";

	av[++i] = "--ignore-trailing-cr";
	av[++i] = lfile;
	av[++i] = rfile;
	av[++i] = 0;
	assert(i < sizeof(av)/sizeof(char*));

	/* Needed because we normally ignore it in sccs_init() but we may
	 * be diffing no inited files.
	 */
	signal(SIGPIPE, SIG_IGN);

	status = spawnvp(_P_WAIT, av[0], av);
	unless (WIFEXITED(status))  {
		fprintf(stderr,
		    "diff(): spawnvp returned 0x%x, errno=%d, PATH=%s\n",
		    status, errno, getenv("PATH"));
		for (i = 0; av[i]; ++i) fprintf(stderr, "av[%d]=%s\n",i, av[i]);
	}
	ret = WEXITSTATUS(status);

	if (fd1 != -1) { close(1); dup2(fd1, 1); close(fd1); }
	return (ret);
}

private u32
align(void *data, int len, int pos, void *extra)
{
	char	*line = data;
	int	i, j, c;
	int	price;
	struct {
		char	*match;		/* pattern to match */
		int	len;		/* size of match */
		int	price[3];	/* price for BEG, END, MID */
	} menu[] = {
		{"", 0, {2, 1, 3}},	/* empty line */
		{"/*", 2, {1, 2, 3}},	/* start comment */
		{"*/", 2, {2, 1, 3}},	/* end comment */
		{"{", 1, {1, 2, 3}},	/* start block */
		{"}", 1, {2, 1, 3}},	/* end block */
		{0, 0, {0, 0, 0}}
	};
	/* remove final newline */
	if ((len > 0) && (line[len-1] == '\n')) --len;

	/* skip whitespace at start of line */
	for (i = 0; i < len; i++) {
		unless ((line[i] == ' ') || (line[i] == '\t')) break;
	}

	/* handle blank line case */
	if (i == len) return (menu[0].price[pos]);

	/* look for other cases */
	price = 0;
	for (j = 1; menu[j].match; j++) {
		c = menu[j].len;
		if (((len - i) >= c) && strneq(line+i, menu[j].match, c)) {
			price = menu[j].price[pos];
			i += c;
			break;
		}
	}
	if (price) {
		/* make sure all that's left is whitespace */
		for (/* i */; i < len; i++) {
			unless ((line[i] == ' ') || (line[i] == '\t')) break;
		}
		if (i == len) return (price);
	}
	return (0);
}

/* Comparison functions for the various diff options */

/*
 * Just see if two lines are identical
 */
private	int
cmp_identical(void *va, int lena, void *vb, int lenb, int last, void *extra)
{
	if (lena != lenb) return (lena - lenb);
	return (memcmp(va, vb, lena));
}

/*
 * Compare ignoring all white space. (diff -w)
 */
private	int
cmp_ignore_ws(void *va, int lena, void *vb, int lenb, int last, void *extra)
{
	int	i, j;
	int	sa, sb;
	char	*a = (char *)va;
	char	*b = (char *)vb;

	i = j = 0;
	while (i < lena || j < lenb) {
		if (a[i] == b[j]) {
			i++, j++;
			continue;
		}
		sa = isspace(a[i]);
		sb = isspace(b[j]);
		unless (sa || sb) break; /* real difference */
		while ((i < lena) && isspace(a[i])) i++;
		while ((j < lenb) && isspace(b[j])) j++;
		unless (a[i] == b[j]) break;
	}
	return (!((i == lena) && (j == lenb)));
}

/*
 * Compare ignoring changes in white space (diff -b).
 */
private	int
cmp_ignore_ws_chg(void *va, int lena, void *vb, int lenb, int last, void *extra)
{
	int	i, j;
	int	sa, sb;
	char	*a = (char *)va;
	char	*b = (char *)vb;

	i = j = 0;
	while ((i < lena) || (j < lenb)) {
		if (a[i] == b[j]) {
			i++, j++;
			continue;
		}
		sa = isspace(a[i]);
		sb = isspace(b[j]);
		unless (sa || sb) break; /* real difference */
		if ((i > 0) && isspace(a[i-1])) {
			while ((i < lena) && (sa = isspace(a[i]))) i++;
		}
		if ((j > 0) && isspace(b[j-1])) {
			while ((j < lenb) && (sb = isspace(b[j]))) j++ ;
		}
		if (sa && sb) {
			i++, j++;
			continue;
		}
		unless (a[i] == b[j]) break;
	}
	return (!((i == lena) && (j == lenb)));
}

/* HASH FUNCTIONS */

/*
 * Hash data, use crc32c for speed
 */
private	u32
hash_identical(void *data, int len, int side, int last, void *extra)
{
	u32	h = 0;
	filedf	*o = (filedf *)extra;
	char	*buf = (char *)data;

	if (last && !o->files[side].nl) h = 1;
	return (crc32c(h, buf, len));
}

/*
 * Hash data ignoring changes in white space (diff -b)
 */
private	u32
hash_ignore_ws_chg(void *data, int len, int side, int last, void *extra)
{
	u32	h = 0;
	int	i, j = 0;
	filedf	*o = (filedf *)extra;
	char	*buf = (char *)data;
	char	copy[len];

	if (last && !o->files[side].nl && o->dop->strip_trailing_cr) {
		h = 1;
	}
	for (i = 0; i < len; i++) {
		if (isspace(buf[i])) {
			while ((i < len) && isspace(buf[i+1])) i++;
			copy[j++] = ' ';
			continue;
		}
		copy[j++] = buf[i];
	}
	return (crc32c(h, copy, j));
}

/*
 * Hash data ignoring all white space (diff -w)
 */
private	u32
hash_ignore_ws(void *data, int len, int side, int last, void *extra)
{
	u32	h = 0;
	int	i, j = 0;
	filedf	*o = (filedf *)extra;
	char	*buf = (char *)data;
	char	copy[len];

	if (last && !o->files[side].nl && o->dop->strip_trailing_cr) {
		h = 1;
	}
	for (i = 0; i < len; i++) {
		if (isspace(buf[i])) {
			while ((i < len) && isspace(buf[i+1])) i++;
			continue;
		}
		copy[j++] = buf[i];
	}
	return (crc32c(h, copy, j));
}

private void
printLine(void *data, int len, int side, int last, void *extra, FILE *out)
{
	filedf	*o = (filedf *)extra;

	fwrite(data, len, 1, out);
	if (last && !o->files[side].nl) {
		fputc('\n', out);
		unless (o->dop->out_define) {
			fputs("\\ No newline at end of file\n", out);
		}
	}
}

private	void
printHeader(int lno, void *extra, FILE *out)
{
	int	i;
	filedf	*o = (filedf *)extra;

	EACH_REVERSE(o->fn_defs) {
		if (o->fn_defs[i].lno <= lno) {
			fprintf(out, "%.*s",
			    (int)min(40, strlen(o->fn_defs[i].s)),
			    o->fn_defs[i].s);
			return;
		}
	}
}

private void
printDeco(u32 where, void *extra, FILE *out)
{
	filedf	*o = (filedf *)extra;
	char	*defstr = o->dop->out_define;

	if (where & DF_LEFT_START) {
		fprintf(out, "#ifndef %s\n", defstr);
	} else if ((where & (DF_LEFT_END|DF_RIGHT_START)) ==
	    (DF_LEFT_END|DF_RIGHT_START)) {
		fprintf(out, "#else /* %s */\n", defstr);
	} else if (where & DF_LEFT_END) {
		fprintf(out, "#endif /* ! %s */\n", defstr);
	} else if (where & DF_RIGHT_START) {
		fprintf(out, "#ifdef %s\n", defstr);
	} else if (where & DF_RIGHT_END) {
		fprintf(out, "#endif /* %s */\n", defstr);
	}
}

private void
sdPrint(void *data, int len, int side, int last, void *extra, FILE *out)
{
	filedf	*o = (filedf *)extra;
	char	*buf = (char *)data;
	char	*str;

	if (buf[len-1] == '\n') len--;
	str = strndup(buf, len);
	switch (o->state) {
	    case DF_MOD_START:
		o->bl[side] = addLine(o->bl[side], str);
		return;
	    case DF_COMMON_START: printLines(o, str, str, out); break;
	    case DF_LEFT_START:	  printLines(o, str, 0, out); break;
	    case DF_RIGHT_START:  printLines(o, 0, str, out); break;
	    default:		assert(0);
	}
	FREE(str);
}

private void
sdState(u32 where, void *extra, FILE *out)
{
	filedf	*o = (filedf *)extra;

	switch (where) {
	    case DF_MOD_START:
	    case DF_COMMON_START:
		o->state = where;
		break;
	    case DF_LEFT_START:
	    case DF_RIGHT_START:
		if (o->state != DF_MOD_START) o->state = where;
		break;
	    case DF_MOD_END:
		alignMods(o, o->bl[0], o->bl[1], out);
		freeLines(o->bl[0], free);
		freeLines(o->bl[1], free);
		o->bl[0] = o->bl[1] = 0;
		o->state = 0;
	    default: break;
	}
}

private void
printLines(filedf *o, char *left, char *right, FILE *out)
{
	int	i, j, n;
	char	sep;

	/*
	 * If they pass the same pointer we assume it's not a diff,
	 * different pointers but both present means it's a diff where
	 * left was changed to right.
	 */
	if (left && right) {
		sep = (left == right) ? ' ' : '|';
	} else if (left) {
		sep = '<';
	} else {
		sep = '>';
	}
	n = (int)o->dop->out_sdiff;
	if (n <= 1) {
		fputc(sep, out);
		fputc('\n', out);
		return;
	}
	if ((n == 2) || (n == 3)) {
		fputc(' ', out);
		fputc(sep, out);
		if (n == 3) fputc(' ', out);
		fputc('\n', out);
		return;
	}
	n = (n - 3) / 2;
	/*
	 * We need to do the prints by hand since a tab accounts for 8
	 * chars and printf's %.-* and %.*s formats don't account for
	 * that.
	 */
	if (left) {
		for (i = j = 0; (j < n) && (i < strlen(left)); i++, j++) {
			fputc(left[i], out);
			if (left[i] == '\t') j += 7 - (j % 8);
		}
		fprintf(out, "%-*s", n - j, "");
	} else {
		fprintf(out, "%-*s", n, "");
	}
	fputc(' ', out);
	fputc(sep, out);
	fputc(' ', out);
	if (right) fprintf(out, "%.*s", n, right);
	fputc('\n', out);
}

/*
 * This implements the Needleman–Wunsch algorithm for finding
 * the best alignment of diff block.
 * See http://en.wikipedia.org/wiki/Needleman–Wunsch_algorithm
 */
private void
alignMods(filedf *o, char **A, char **B, FILE *out)
{
	int	i, j, k;
	int	match, delete, insert;
	int	score, scoreDiag, scoreUp, scoreLeft;
	int	lenA, lenB;
	int	n = nLines(A), m = nLines(B);
	int	d = 0;	/* gap penalty */
	int	**F;
	int	*algnA;
	int	*algnB;

	if ((n * m) > 100000) {
		/*
		 * Punt if the problem is too large since the
		 * algorithm is O(n^2).  The rationale is that the
		 * line alignment only helps if you're looking at
		 * smallish regions. Once you've gone over a few
		 * screenfuls you're just reading new code so no point
		 * in working hard to align lines.
		 *
		 */
		for (i = 1; (i <= n) && (i <= m); i++) {
			printLines(o, A[i], B[i], out);
		}
		while (i <= n) printLines(o, A[i++], 0, out);
		while (i <= m) printLines(o, 0, B[i++], out);
		return;
	}
	F = malloc((n+1) * sizeof(int *));
	algnA = calloc(n+m+1, sizeof(int));
	algnB = calloc(n+m+1, sizeof(int));
	for (i = 0; i <= n; i++) {
		F[i] = malloc((m+1) * sizeof(int));
		F[i][0] = d * i;
	}
	for (j = 0; j <= m; j++) F[0][j] = d * j;
	for (i = 1; i <= n; i++) {
		lenA = strlen(A[i]);
		for (j = 1; j <= m; j++) {
			lenB = strlen(B[j]);
			match  = F[i-1][j-1] + lcs(A[i], lenA, B[j], lenB);
			delete = F[i-1][j] + d;
			insert = F[i][j-1] + d;
			F[i][j] = max(match, max(delete, insert));
		}
	}
	/*
	 * F now has all the alignments, walk it back to find
	 * the best one.
	 */
	k = n + m;
	i = nLines(A);
	j = nLines(B);
	while ((i > 0) && (j > 0)) {
		score     = F[i][j];
		scoreDiag = F[i-1][j-1];
		scoreUp   = F[i][j-1];
		scoreLeft = F[i-1][j];
		assert(k > 0);
		lenA = strlen(A[i]);
		lenB = strlen(B[j]);
		if (score == (scoreDiag + lcs(A[i], lenA, B[j], lenB))) {
			algnA[k] = i;
			algnB[k] = j;
			i--; j--; k--;
		} else if (score == (scoreLeft + d)) {
			algnA[k] = i;
			algnB[k] = -1;
			i--; k--;
		} else if (score == (scoreUp + d)) {
			algnA[k] = -1;
			algnB[k] = j;
			j--; k--;
		}
	}
	while (i > 0) {
		assert(k > 0);
		algnA[k] = i;
		algnB[k] = -1;
		i--; k--;
	}
	while (j > 0) {
		assert(k > 0);
		algnA[k] = -1;
		algnB[k] = j;
		j--; k--;
	}
	/* Now print the output */
	for (i = k + 1; i <= (n + m); i++) {
		assert((algnA[i] != -1) || (algnB[i] != -1));
		if (algnA[i] == -1) {
			printLines(o, 0, B[algnB[i]], out);
		} else if (algnB[i] == -1) {
			printLines(o, A[algnA[i]], 0, out);
		} else {
			printLines(o, A[algnA[i]], B[algnB[i]], out);
		}
	}
	for (i = 0; i <= n; i++) free(F[i]);
	free(F);
	free(algnA);
	free(algnB);
}

private int
lcs(char *a, int alen, char *b, int blen)
{
	int	i, j;
	int	ret;
	int	**d;

	d = calloc(alen+1, sizeof(int *));
	for (i = 0; i <= alen; i++) d[i] = calloc(blen+1, sizeof(int));

	for (i = 1; i <= alen; i++) {
		for (j = 1; j <= blen; j++) {
			if (a[i] == b[j]) {
				d[i][j] = d[i-1][j-1] + 1;
			} else {
				d[i][j] = max(d[i][j-1], d[i-1][j]);
			}
		}
	}
	ret = d[alen][blen];
	for (i = 0; i <= alen; i++) free(d[i]);
	free(d);
	return (ret);
}
