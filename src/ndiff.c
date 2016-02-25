/*
 * Copyright 2012-2016 BitMover, Inc
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

#include "sccs.h"
#include "cfg.h"

/*
 * This struct is for diff -p, so we can save where
 * the function headers are.
 */
typedef	struct {
	int	lno;		/* line number */
	char	*s;		/* string to print */
} header;

typedef	struct {
	char	*data;
	int	len;
} line;

typedef struct {
	df_opt	dop;		/* copy of diff opts */
	u8	binary[2];	/* set if files are binary */
	header	*fn_defs;	/* function defs for diff -p */
	int	diffgap;	/* gap value for sdiff */
	line	*lines[2];
} filedf;

/* other functions */
private	int	external_diff(char *lfile, char *rfile, df_opt *dop, char *out);
private	void	saveline(line **lines, int side, char *data, int len);
private	char	*getData(int idx, int side, void *extra, int *len);
private	void	printStd(hunk *hunks, filedf *fop, FILE *out);
private	void	printRCS(hunk *hunks,
		    filedf *fop, FILE *out);
private	void	printIfDef(hunk *hunks, hunk *range,
		    filedf *fop, FILE *out, char *defstr);
private	void	printUnified(hunk *hunks, hunk *range,
		    filedf *fop, FILE *out);
private	void	printUBlock(hunk *hunks, hunk *range, filedf *fop, FILE *out);
private	void	printSdiff(hunk *hunks, hunk *range, filedf *fop, FILE *out);
private	void	printLines(char *left, int lenL,
		    char *right, int lenR, FILE *out, df_opt *dop);
private	void	printHunk(char *prefix, hunk *h, int side,
		    filedf *fop, FILE *out, u32 nlFlags);

enum {
	PRN_ADD_NL = 1,		/* print new line at end of diffs */
	PRN_WARN_NL = 2,	/* print warning (implies PRN_ADD_NL */
};

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

	rc = diff_files(av[optind], av[optind+1], &opts, "-");
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
diff_files(char *file1, char *file2, df_opt *dop, char *out)
{
	int	i, j;
	int	printAll = dop->out_define || dop->out_sdiff; /* whole file */
	int	lno[2];
	int	skip[2] = {0, 0};
	int	rc = 2;
	char	*files[2] = {file1, file2};
	char	*data[2] = {0, 0};
	df_cmp	*dcmp;
	df_hash	*dhash;
	header	*fh;
	filedf	fop = {{0}};
	pcre	*re = 0;
	FILE	*fout = 0;
	struct	stat sb[2];
	struct	tm	*tm;
	long	offset;
	hunk	*h, *hlist = 0, range;
	char	buf[1024];

	if (getenv("_BK_USE_EXTERNAL_DIFF")) {
		return (external_diff(file1, file2, dop, out));
	}

	unless (diff_cleanOpts(dop)) return (2);

	if (out) {
		fout = (streq(out, "-")) ? stdout : fopen(out, "w");
		unless (fout) return (2);
	}

	fop.dop = *dop;
	fop.diffgap = cfg_int(0, CFG_DIFFGAP);

	for (i = 0; i < 2; i++) {
		if (stat(files[i], &sb[i])) {
			unless (dop->new_is_null) {
				perror(files[i]);
				return (2);
			}
			skip[i] = 1;
		}
	}

	/* Maybe they gave us the same file? */
	if (!win32() && !printAll &&
	    (sb[DF_LEFT].st_dev == sb[DF_RIGHT].st_dev) &&
	    (sb[DF_LEFT].st_ino == sb[DF_RIGHT].st_ino)) {
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
	if (!printAll && (sb[DF_LEFT].st_size == sb[DF_RIGHT].st_size) &&
	    !memcmp(data[DF_LEFT], data[DF_RIGHT], sb[DF_LEFT].st_size)) {
		/* no diffs */
		rc = 0;
		goto out;
	}

	/*
	 * Set up the right functions for running the diff.
	 */
	if (dop->ignore_all_ws) {
		dcmp  = diff_cmpIgnoreWS;
		dhash = diff_hashIgnoreWS;
	} else if (dop->ignore_ws_chg) {
		dcmp  = diff_cmpIgnoreWSChg;
		dhash = diff_hashIgnoreWSChg;
	} else {
		dcmp  = diff_cmpLine;
		dhash = diff_hashLine;
	}

	re = dop->pattern;

	lno[DF_LEFT] = lno[DF_RIGHT] = 0;

	for (i = 0; i < 2; i++) {
		char	*s, *e, *t = 0;	/* start/end/tmp */

		s = e = data[i];
		for (j = 0; j < sb[i].st_size; j++) {
			if (*e == '\0') fop.binary[i] = 1;
			if (*e == '\n') {
				if (dop->strip_trailing_cr) {
					t = e;
					while ((e > s) && (*(e-1)=='\r')) e--;
					*e = '\n';
				}
				saveline(&fop.lines[i], i, s, e - s + 1);
				++lno[i];
				if (re && (i == 0)) {
					unless (pcre_exec(re, 0, s, e - s + 1,
						0, 0, 0, 0)) {
						fh = addArray(&fop.fn_defs,0);
						fh->lno = lno[DF_LEFT];
						fh->s = strndup(s, e - s + 1);
						chomp(fh->s);
						trim(fh->s);
					}
				}
				if (dop->strip_trailing_cr) e = t;
				s = e + 1;
			}
			e++;
		}
		if (j && (*(e-1) != '\n')) {
			if (dop->ignore_trailing_cr && (*(e-1) == '\r')) {
				t = e;
				while ((e > s) && (*(e-1) == '\r')) e--;
				*e = '\n';
			} else {
				e--;
			}
			saveline(&fop.lines[i], i, s, e - s + 1);
			++lno[i];
		}
	}

	if (fop.binary[DF_LEFT] || fop.binary[DF_RIGHT]) {
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
		dop->bin_files = 1;
		goto out;
	}

	/* Do the diff */
	range.start[DF_LEFT] = range.start[DF_RIGHT] = 1;
	range.len[DF_LEFT] = lno[DF_LEFT];
	range.len[DF_RIGHT] = lno[DF_RIGHT];
	hlist = diff_items(&range, dop->minimal,
	    getData, dcmp, dhash, diff_cost, &fop);

	rc = nLines(hlist) != 0;
	if (!out || (!rc && !printAll)) goto out;

	if (dop->out_diffstat) {
		dop->adds = dop->dels = dop->mods = 0;
		EACHP(hlist, h) {
			if (DLEN(h, DF_LEFT) == DLEN(h, DF_RIGHT)) {
				dop->mods += DLEN(h, DF_LEFT);
			} else {
				dop->dels += DLEN(h, DF_LEFT);
				dop->adds += DLEN(h, DF_RIGHT);
			}
		}
	}
	if (dop->out_unified) {
		/* print header */
		tm = localtimez(&sb[DF_LEFT].st_mtime, &offset);
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
		fprintf(fout, "--- %s\t%s %s\n",
		    files[DF_LEFT], buf, tzone(offset));
		tm = localtimez(&sb[DF_RIGHT].st_mtime, &offset);
		strftime(buf, 1024, "%Y-%m-%d %H:%M:%S", tm);
		fprintf(fout, "+++ %s\t%s %s\n",
		    files[DF_RIGHT], buf, tzone(offset));
		printUnified(hlist, &range, &fop, fout);
	} else if (dop->out_rcs) {
		printRCS(hlist, &fop, fout);
	} else if (dop->out_define) {
		printIfDef(
		    hlist, &range, &fop, fout, dop->out_define);
	} else if (dop->out_sdiff) {
		printSdiff(hlist, &range, &fop, fout);
	} else if (dop->out_print_hunks) {
		EACHP(hlist, h) {
			fprintf(fout, "%d,%d %d,%d\n",
			    DSTART(h, DF_LEFT), DLEN(h, DF_LEFT),
			    DSTART(h, DF_RIGHT), DLEN(h, DF_RIGHT));
		}
	} else {
		printStd(hlist, &fop, fout);
	}

out:	if (fout && (fout != stdout)) fclose(fout);
	FREE(data[DF_LEFT]);
	FREE(data[DF_RIGHT]);
	FREE(fop.lines[DF_LEFT]);
	FREE(fop.lines[DF_RIGHT]);
	FREE(hlist);
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

private	char *
getData(int idx, int side, void *extra, int *len)
{
	line	*data = &((filedf *)extra)->lines[side][idx];

	if (len) *len = data->len;
	return (data->data);
}

private	void
saveline(line **lines, int side, char *data, int len)
{
	line	*newline = addArray(lines, 0);

	newline->data = data;
	newline->len = len;
}

private	void
printSdiff(hunk *hunks, hunk *range, filedf *fop, FILE *out)
{
	hunk	*h, *from, common, hc;
	int	*plist, i, j, len;
	char	*strA, *strB;
	int	cmd;
	int	lenA, lenB;
	char	*str;

	from = 0;
	EACHP(hunks, h) {
		diff_mkCommon(&common, range, from, h);
		DFOREACH(&common, DF_LEFT, j) {
			str = getData(j, DF_LEFT, fop, &len);
			printLines(str, len, str, len, out, &fop->dop);
		}
		from = h;

		plist = diff_alignMods(h, getData, fop, fop->diffgap);
		hc = *h; // copy so we can modify
		EACH(plist) {
			cmd = plist[i];
			strA = strB = 0;
			lenA = lenB = 0;
			if (cmd != DF_RIGHT) {
				strA = getData(hc.start[DF_LEFT]++,
				    DF_LEFT, fop, &lenA);
				assert(hc.len[DF_LEFT]);
				hc.len[DF_LEFT]--;
			}
			if (cmd != DF_LEFT) {
				strB = getData(hc.start[DF_RIGHT]++,
				    DF_RIGHT, fop, &lenB);
				assert(hc.len[DF_RIGHT]);
				hc.len[DF_RIGHT]--;
			}
			printLines(strA, lenA, strB, lenB, out, &fop->dop);
		}
		assert(!hc.len[DF_LEFT] && !hc.len[DF_RIGHT]);
		free(plist);
	}
	diff_mkCommon(&common, range, from, 0);
	DFOREACH(&common, DF_LEFT, j) {
		str = getData(j, DF_LEFT, fop, &len);
		printLines(str, len, str, len, out, &fop->dop);
	}
}

private	void
printLines(char *left, int lenL, char *right, int lenR, FILE *out, df_opt *dop)
{
	int	i, j, n;
	char	sep;

	/* chop */
	if (lenL && (left[lenL-1] == '\n')) lenL--;
	if (lenR && (right[lenR-1] == '\n')) lenR--;

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
	n = (int)dop->out_sdiff;
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
		for (i = j = 0; (j < n) && (i < lenL); i++, j++) {
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
	if (right) fprintf(out, "%.*s", lenR, right);
	fputc('\n', out);
}

private	void
printStd(hunk *hunks, filedf *fop, FILE *out)
{
	hunk	*h;
	int	left, right;

	EACHP(hunks, h) {
		left = DLEN(h, DF_LEFT);
		right = DLEN(h, DF_RIGHT);

		fprintf(out, "%d", DAFTER(h, DF_LEFT));
		if (left > 1) {
			fprintf(out, ",%d", DLAST(h, DF_LEFT));
		}
		if (left && right) {
			fputc('c', out);
		} else if (left) {
			fputc('d', out);
		} else if (right) {
			fputc('a', out);
		}
		fprintf(out, "%d", DAFTER(h, DF_RIGHT));
		if (right > 1) {
			fprintf(out, ",%d", DLAST(h, DF_RIGHT));
		}
		fputc('\n', out);

		printHunk("< ", h, DF_LEFT, fop, out, PRN_WARN_NL);
		if (left && right) {
			fputs("---\n", out);
		}
		printHunk("> ", h, DF_RIGHT, fop, out, PRN_WARN_NL);
	}
}

private	void
printRCS(hunk *hunks, filedf *fop, FILE *out)
{
	hunk	*h;
	int	left, right;

	EACHP(hunks, h) {
		left = DLEN(h, DF_LEFT);
		right = DLEN(h, DF_RIGHT);

		if (left) {
			fprintf(out, "d%d %d\n", DSTART(h, DF_LEFT), left);
		}
		if (right) {
			fprintf(out, "a%d %d\n", DLAST(h, DF_LEFT), right);
			printHunk(0, h, DF_RIGHT, fop, out, 0);
		}
	}
}

private	void
printUnified(hunk *hunks, hunk *range, filedf *fop, FILE *out)
{
	hunk	*h, *set = 0;
	int	startCommon, endCommon;
	int	context = fop->dop.context;

	startCommon = DSTART(range, DF_LEFT);
	EACHP(hunks, h) {
		/* consider context as part of this hunk */
		endCommon = DSTART(h, DF_LEFT) - context;
		/* If any common remains, then hunks are separated: flush */
		if ((endCommon - startCommon > 1) && nLines(set)) {
			printUBlock(set, range, fop, out);
			truncArray(set, 0);
		}
		/* consider context as part of this hunk */
		startCommon = DEND(h, DF_LEFT) + context;
		addArray(&set, h);
	}
	if (nLines(set)) {
		printUBlock(set, range, fop, out);
	}
	free(set);
}

private	void
printUBlock(hunk *hunks, hunk *range, filedf *fop, FILE *out)
{
	hunk	*h, *from, common, bounds;
	hunk	*first = &hunks[1];
	hunk	*last = &hunks[nLines(hunks)];
	int	i, left, right, n, m, side;
	int	context = fop->dop.context;
	char	*p;

	assert(last != hunks);
	for (side = 0; side < 2; side++) {
		m = DSTART(first, side) - context;
		n = DSTART(range, side);
		DSTART(&bounds, side) = max(m, n);

		m = DEND(last, side) + context;
		n = DEND(range, side);
		DLEN(&bounds, side) = min(m, n) - DSTART(&bounds, side);
	}
	range = &bounds;

	left = range->len[DF_LEFT];
	right = range->len[DF_RIGHT];

	fprintf(out, "@@ -%d", DAFTER(range, DF_LEFT));
	if (left != 1) fprintf(out, ",%d", left);
	fprintf(out, " +%d", DAFTER(range, DF_RIGHT));
	if (right != 1) fprintf(out, ",%d", right);
	fputs(" @@", out);

	assert(nLines(hunks));
	if (fop->fn_defs) {
		/* XXX: different than diff; range->start[DF_LEFT] */
		int	idx = DSTART(&hunks[1], DF_LEFT);

		EACH_REVERSE(fop->fn_defs) {
			if (fop->fn_defs[i].lno < idx) {
				p = fop->fn_defs[i].s;
				fputc(' ', out);
				fprintf(out, "%.*s",
				    (int)min(40, strlen(p)), p);
				break;
			}
		}
	}
	fputc('\n', out);

	from = 0;
	EACHP(hunks, h) {
		diff_mkCommon(&common, range, from, h);
		printHunk(" ", &common, DF_LEFT, fop, out, PRN_WARN_NL);
		from = h;

		printHunk("-", h, DF_LEFT, fop, out, PRN_WARN_NL);
		printHunk("+", h, DF_RIGHT, fop, out, PRN_WARN_NL);
	}
	diff_mkCommon(&common, range, from, 0);
	printHunk(" ", &common, DF_LEFT, fop, out, PRN_WARN_NL);
}

private	void
printIfDef(hunk *hunks, hunk *range,
    filedf *fop, FILE *out, char *defstr)
{
	hunk	*h, *from, common;
	int	left, right;

	from = 0;
	EACHP(hunks, h) {
		diff_mkCommon(&common, range, from, h);
		printHunk(0, &common, DF_LEFT, fop, out, PRN_ADD_NL);
		from = h;

		left = DLEN(h, DF_LEFT);
		right = DLEN(h, DF_RIGHT);

		if (left) {
			fprintf(out, "#ifndef %s\n", defstr);
			printHunk(0, h, DF_LEFT, fop, out, PRN_ADD_NL);
		}
		if (right) {
			if (left) {
				fprintf(out, "#else /* %s */\n", defstr);
			} else {
				fprintf(out, "#ifdef %s\n", defstr);
			}
			printHunk(0, h, DF_RIGHT, fop, out, PRN_ADD_NL);
			fprintf(out, "#endif /* %s */\n", defstr);
		} else if (left) {
			fprintf(out, "#endif /* ! %s */\n", defstr);
		}
	}
	diff_mkCommon(&common, range, from, 0);
	printHunk(0, &common, DF_LEFT, fop, out, PRN_ADD_NL);
}

private	void
printHunk(char *prefix, hunk *h, int side, filedf *fop, FILE *out, u32 nlFlags)
{
	int	j, len;
	char	*p;

	DFOREACH(h, side, j) {
		p = getData(j, side, fop, &len);
		if (prefix) fputs(prefix, out);
		fwrite(p, len, 1, out);
		if (nlFlags && (!len || (p[len-1] != '\n'))) {
			fputc('\n', out);
			if (nlFlags & PRN_WARN_NL) {
				fputs("\\ No newline at end of file\n", out);
			}
		}
	}
}
