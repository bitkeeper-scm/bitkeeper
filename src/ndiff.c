#include "system.h"
#include "sccs.h"
#include "regex.h"


/*
 * This struct is for diff -p, so we can save where
 * the function headers are.
 */
typedef	struct header {
	int	lno;		/* line number */
	char	*s;		/* string to print */
} header;

typedef	struct file_t {
	char	*name;
	int	binary;
	struct stat sb;
	int	nl;		/* does it have a newline at the end? */
	char	*data;
} file_t;

typedef struct filediff {
	file_t	files[2];		/* the two files to diff */
	header	*fn_defs;		/* function defs for diff -p */
	diffopt *dop;			/* options for diff */
} filediff;

private	int	cleanOpts(diffopt *opts);

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

/* other functions */
private int	algn_ws(void *data, int len, void *extra);

int
ndiff_main(int ac, char **av)
{
	int		c;
	diffopt		opts;
	longopt		lopts[] = {
		{ "ignore-trailing-cr", 310 },
		{ "show-function-line", 'F'},
		{ "strip-trailing-cr", 320 },
		{ "print-hunks", 330},
		{ 0, 0 }
	};
	int	rc = -1;

	bzero(&opts, sizeof(diffopt));
	while ((c = getopt(ac, av, "bdD:F:puwnN", lopts)) != -1) {
		switch (c) {
		    case 'b': opts.ignore_ws_chg = 1; break;
		    case 'd': opts.minimal = 1; break;
		    case 'D': opts.out_define = strdup(optarg);	break;
		    case 'p': opts.out_show_c_func = 1; break;
		    case 'u': opts.out_unified = 1; break;
		    case 'w': opts.ignore_all_ws = 1; break;
		    case 'n': opts.out_rcs = 1; break;
		    case 'N': opts.new_is_null = 1; break;
		    case 'F': opts.pattern = strdup(optarg); break;
		    case 310:	/* --ignore-trailing-cr */
			opts.ignore_trailing_cr = 1;
			break;
		    case 320:   /* --strip-trailing-cr */
			opts.strip_trailing_cr = 1;
			break;
		    case 330:
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
	    opts.out_print_hunks) > 1) {
		fprintf(stderr, "diff: conflicting output style options\n");
		goto out;
	}
	if (opts.out_show_c_func && opts.pattern) {
		fprintf(stderr, "diff: only one of -p or -F allowed\n");
		goto out;
	}

	rc = diff_files(av[optind], av[optind+1], &opts, 0, stdout);
out:	if (opts.out_define) FREE(opts.out_define);
	if (opts.pattern) FREE(opts.pattern);
	return (rc);
}

private	int
cleanOpts(diffopt *opts)
{
	/*
	 * Make sure we don't have conflicting output styles
	 */
	if ((opts->out_unified + !!opts->out_define + opts->out_rcs +
		opts->out_print_hunks) > 1) return (0);
	/*
	 * --ignore-trailing-cr implies --strip-tailing-cr
	 */
	if (opts->ignore_trailing_cr) opts->strip_trailing_cr = 1;

	/*
	 * show_c_func uses and patter is invalid
	 */
	if (opts->out_show_c_func && opts->pattern) return (0);
	return (1);
}

/*
 * Return:
 *   0  same
 *   1  differ
 *   2  error
 */
int
diff_files(char *file1, char *file2, diffopt *dopts, diffctx **odc, FILE *out)
{
	int		i, j, n;
	int		firstDiff;
	int		lno[2];
	int		skip[2] = {0, 0};
	int		fd;
	diffcmp		dcmp = 0;
	diffhash	dhash = 0;
	header		*fh;
	int		rc = 2;
	diffctx		*dc = 0;
	filediff	*o;

	unless (cleanOpts(dopts)) return (2);

	o = new(filediff);
	o->files[0].name = file1;
	o->files[1].name = file2;
	o->dop = new(diffopt);
	memcpy(o->dop, dopts, sizeof(diffopt));

	for (i = 0; i < 2; i++) {
		if (stat(o->files[i].name, &o->files[i].sb)) {
			unless (o->dop->new_is_null) {
				perror(o->files[i].name);
				return (2);
			}
			skip[i] = 1;
		}
	}

	/* Maybe they gave us the same file? */
	if (!win32() && (o->files[0].sb.st_dev == o->files[1].sb.st_dev) &&
	    (o->files[0].sb.st_ino == o->files[1].sb.st_ino)) {
		/* no diffs */
		return (0);
	}

	/* Load the two files into memory. */
	for (i = 0; i < 2; i++) {
		if (skip[i]) continue;
		unless ((fd = open(o->files[i].name, O_RDONLY, 0)) >= 0) {
			perror(o->files[i].name);
			rc = 2;
			goto out;
		}
		o->files[i].data = malloc(o->files[i].sb.st_size + 1);
		assert(o->files[i].data);
		n = o->files[i].sb.st_size;
		unless (n == read(fd, o->files[i].data, n)) {
			perror(o->files[1].name);
			close(fd);
			rc = 2;
			goto out;
		}
		close(fd);
	}

	/*
	 * If the two files are the same size, or if we are just
	 * seeing if they differ and don't care about the actual
	 * diffs, then do a quick comparison.
	 */
	if ((o->files[0].sb.st_size == o->files[1].sb.st_size) &&
	    !memcmp(o->files[0].data, o->files[1].data, o->files[0].sb.st_size)) {
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

	dc = diff_new(dcmp, dhash, algn_ws, o);

	if (o->dop->pattern) {
		re_comp(o->dop->pattern);
	} else if (o->dop->out_show_c_func) {
		o->dop->pattern = "^[A-Za-z_][A-Za-z0-9_]*[ \t]*(";
		re_comp(o->dop->pattern);
	}

	lno[0] = lno[1] = 0;

	for (i = 0; i < 2; i++) {
		char	*s, *e, *t = 0;	/* start/end/tmp */

		s = e = o->files[i].data;
		for (j = 0; j < o->files[i].sb.st_size; j++) {
			if (*e == '\0') o->files[i].binary = 1;
			if (*e == '\n') {
				if (o->dop->strip_trailing_cr) {
					t = e;
					while ((e > s) && (*(e-1)=='\r')) e--;
					*e = '\n';
				}
				diff_addItem(dc, i, s, e - s + 1);
				lno[i]++;
				if (o->dop->pattern && (i == 0)) {
					if (re_exec(s)) {
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
				while ((e > s) && (*(e-1) == '\r')) e--;
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
		fprintf(out, "Binary files %s and %s differ\n",
		    o->files[0].name, o->files[1].name);
		rc = 1;
		goto out;
	}

	/*
	 * This is an optimization where we just compare as much as we
	 * can to avoid hashing.
	 */
	n = min(o->files[0].sb.st_size, o->files[1].sb.st_size);
	firstDiff = 0;
	for (i = 0; i < n; i++) {
		if (o->files[0].data[i] != o->files[1].data[i]) break;
		if (o->files[0].data[i] == '\n') firstDiff++;
	}

	/* Do the diff */
	diff_items(dc, firstDiff, o->dop->minimal);
	unless (nLines(diff_hunks(dc))) {
		rc = 0;
		goto out;
	}
	rc = 1;			/* difference found */
	if (odc) {
		*odc = dc;
		return (rc);
	}

	unless (out) goto out;

	/* Print the diffs. */
	if (o->dop->out_unified) {
		diff_printUnified(dc,
		    o->files[0].name, &o->files[0].sb.st_mtime,
		    o->files[1].name, &o->files[1].sb.st_mtime,
		    printLine, o->dop->pattern ? printHeader : 0, out);
	} else if (o->dop->out_rcs) {
		diff_printRCS(dc, printLine, out);
	} else if (o->dop->out_define) {
		diff_printIfDef(dc, o->dop->out_define, printLine, out);
	} else if (o->dop->out_print_hunks) {
		hunk	*h, *hlist = diff_hunks(dc);

		EACHP(hlist, h) {
			fprintf(out, "%d,%d %d,%d\n", h->li, h->ll, h->ri, h->rl);
		}
	} else {
		diff_print(dc, printLine, out);
	}
	if (o->dop->out_show_c_func) o->dop->pattern = 0;
out:	FREE(o->files[0].data);
	FREE(o->files[1].data);
	FREE(o->dop);
	FREE(o);
	diff_free(dc);
	return (rc);
}

/*
 * Is this string just whitespace?
 */
private int
algn_ws(void *data, int len, void *extra)
{
	int	i;
	char	*s = (char *)data;

	for (i = 0; i < len; i++) {
		unless (isspace(s[i])) return (0);
	}
	return (1);
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
		while (a[i] && isspace(a[i])) i++;
		while (b[j] && isspace(b[j])) j++;
		unless (a[i] == b[j]) break;
	}
	return ((i == lena) && (j == lenb));
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
			while (a[i] && (sa = isspace(a[i]))) i++;
		}
		if ((j > 0) && isspace(b[j-1])) {
			while (b[j] && (sb = isspace(b[j]))) j++ ;
		}
		if (sa && sb) {
			i++, j++;
			continue;
		}
		unless (a[i] == b[j]) break;
	}
	return ((i == lena) && (j == lenb));
}

/* HASH FUNCTIONS */

/*
 * Adapted from libc/mdbm/hash.c:mdbm_hash1
 */
#define	PRIME1		37
#define	PRIME2		1048583

/*
 * Hash data.
 */
private	u32
hash_identical(void *data, int len, int side, int last, void *extra)
{
	u32	h = 0;
	int	i;
	filediff *o = (filediff *)extra;
	char	*buf = (char *)data;

	if (last && !o->files[side].nl) h = 1;
	for (i = 0; i < len; i++) h = h * PRIME1 ^ (buf[i] - ' ');
	h %= PRIME2;
	return (h);
}

/*
 * Hash data ignoring changes in white space (diff -b)
 */
private	u32
hash_ignore_ws_chg(void *data, int len, int side, int last, void *extra)
{
	u32	h = 0;
	int	i;
	filediff	*o = (filediff *)extra;
	char	*buf = (char *)data;

	if (last && !o->files[side].nl && o->dop->strip_trailing_cr) {
		h = 1;
	}
	for (i = 0; i < len; i++) {
		if (isspace(buf[i])) {
			while ((i < len) && isspace(buf[i+1])) i++;
			h = h * PRIME1 ^ 0 ; /* buf[i] - ' ' == 0 */
			continue;
		}
		h = h * PRIME1 ^ (buf[i] - ' ');
	}
	h %= PRIME2;
	return (h);
}

/*
 * Hash data ignoring all white space (diff -w)
 */
private	u32
hash_ignore_ws(void *data, int len, int side, int last, void *extra)
{
	u32	h = 0;
	int	i;
	filediff	*o = (filediff *)extra;
	char	*buf = (char *)data;

	if (last && !o->files[side].nl && o->dop->strip_trailing_cr) {
		h = 1;
	}
	for (i = 0; i < len; i++) {
		if (isspace(buf[i])) {
			while ((i < len) && isspace(buf[i+1])) i++;
			continue;
		}
		h = h * PRIME1 ^ (buf[i] - ' ');
	}
	h %= PRIME2;
	return (h);
}

private void
printLine(void *data, int len, int side, int last, void *extra, FILE *out)
{
	filediff	*o = (filediff *)extra;

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
	filediff	*o = (filediff *)extra;

	EACH_REVERSE(o->fn_defs) {
		if (o->fn_defs[i].lno < lno) {
			fprintf(out, "%.*s",
			    (int)min(40, strlen(o->fn_defs[i].s)),
			    o->fn_defs[i].s);
			return;
		}
	}
}

