#include "bkd.h"
#include "logging.h"
typedef void (*vfn)(char *);

private char	*http_time(void);
private char	*http_expires(void);
private	char	*type(char *name);
private void	httphdr(char *file);
private	char	*url(char *path);
private void	http_error(int status, char *fmt, ...);
private void	http_page(char *page, vfn content, char *argument);
private void	http_file(char *file);
private void	http_index(char *page);
private void	http_changes(char *rev);
private void	http_cset(char *rev);
private void	http_anno(char *pathrev);
private void	http_both(char *pathrev);
private void	http_diffs(char *pathrev);
private void	http_src(char *pathrev);
private void	http_hist(char *pathrev);
private void	http_patch(char *rev);
private void	http_gif(char *path);
private void	http_stats(char *path);
private void	http_search(char *path);
private void	http_related(char *path);
private void	http_license(char *path);
private void	http_tags(char *path);
private void	title(char *title, char *desc, char *color);
private void	pwd_title(char *t, char *color);
private void	header(char *path, char *color, char *title, char *header, ...);
private void	printnavbar();
private char	*parseurl(char *);
private void	learn();
private void	trailer(char *path);
private char	*units(char *t);
private char	*findRoot(char *name);
private int	has_temp_license();
private	char	root[MAXPATH];
private int	embedded = 0;
private int	expires = 0;

#define	COLOR		"lightblue"

#define	OUTER_TABLE	"<table width=100% bgcolor=black cellspacing=0 border=0 cellpadding=2><tr><td>\n"
#define INNER_TABLE	"<table width=100% cellpadding=3 cellspacing=1 border=0>"
#define	INNER_END	"</table>"
#define OUTER_END	"</td></tr></table>\n"

#define BKWEB_SERVER_VERSION	"0.2"
#define	NOFUDGE(d)	(d->date - d->dateFudge)

private char	arguments[MAXPATH];
private char	navbar[MAXPATH];
private char	thisPage[MAXPATH];
private char	user[80];
private char	prefix[100];
private char	suffix[10];
private int	isLoggingTree;
private	char	*expr;

private struct pageref {
    vfn  content;
    char *page;
    char *name;
    int  size;
    int flags;
#define HAS_ARG 0x01
    char *arg;
} pages[] = {
    { http_index,   "index",     "index.html" },
    { http_changes, "changeset", "ChangeSet", 0, HAS_ARG, 0 },
    { http_changes, "changeset", "ChangeSet@", 10 },
    { http_src,     "source",    "src", 0, HAS_ARG, "." },
    { http_src,     "source",    "src/", 4 },
    { http_hist,    "hist",      "hist/", 5 },
    { http_cset,    "cset",      "cset@", 5 },
    { http_patch,   "patch",     "patch@", 6 },
    { http_both,    "both",      "both/", 5 },
    { http_anno,    "anno",      "anno/", 5 },
    { http_diffs,   "diffs",     "diffs/", 6 },
    { http_search,  "search",    "search/", 7 },
    { http_stats,   "stats",     "stats",  0, HAS_ARG, 0 },
    { http_related, "related",   "related/", 8 },
    { http_license, 0,           "license" },
    { http_tags,    "tags",      "tags" },
    { 0 },
};

int
cmd_httpget(int ac, char **av)
{
	char	buf[MAXPATH];
	char	*name = &av[1][1];
	int	state = 0;
	int 	ret, i;
	char	*s;

	/*
	 * Ignore the rest of the http header (if any), we don't care.
	 */
	if (ac > 2) {
		while (read(0, buf, 1) == 1) {
			if (buf[0] == '\r') {
				switch (state) {
				    case 0: case 2: state++; break;
				    default: state = 0;
				}
			} else if (buf[0] == '\n') {
				if (state == 1) state++;
				else if (state == 3) break;
				else state = 0;
			} else {
				state = 0;
			}
		}
	}

	unless (av[1]) http_error(404, "get what?\n");

	if ((strlen(name) + sizeof("BitKeeper/html") + 2) >= MAXPATH) {
		http_error(500, "path too long for bkweb");
	}

	/*
	 * Go find the project root.
	 * If they pass in //pathname/to/root/whatever, we'll do a cd first.
	 */
	if (*name == '/') {
		if (Opts.nocd) {
			http_error(403, "Absolute paths are not allowed");
		}
		unless (name = findRoot(name)) {
			http_error(503, "Can't find project root");
		}
	} else {
		strcpy(root, url(""));
	}

	name = parseurl(name);
	if (user[0]) sprintf(root+strlen(root), "user=%s/", user);
	unless (*name) name = "index.html";

	unless (bk_options()&BKOPT_WEB) {
		unless (streq(name, "license") || has_temp_license()) {
			http_error(503,
			    "bkWeb option is disabled: %s",
			    upgrade_msg);
		}
	}

	isLoggingTree = exists(LOG_TREE);
	for (i = 0; pages[i].content; i++) {
		if (pages[i].size == 0) {
			ret = streq(pages[i].name, name);
		} else {
			ret = strneq(pages[i].name, name, pages[i].size);
		}
		if (ret) {
			http_page(pages[i].page, pages[i].content,
			    (pages[i].flags & HAS_ARG) ? pages[i].arg
						       : name + pages[i].size);
			exit(0);
		}
	}

	sprintf(buf, "BitKeeper/html/%s", name);
	if (isreg(buf)) {
		http_file(buf);		/* XXX - doesn't respect base url */
	} else if ((s = strrchr(name, '.')) && streq(s, ".gif")) {
		http_gif(name);
	} else {
		http_error(404, "Page &lt;%s&gt; not found", name);
	}
	exit(0);
}


private void
whoami(char *fmt, ...)
{
	va_list ptr;

	va_start(ptr, fmt);
	vsprintf(thisPage, fmt, ptr);
	va_end(ptr);

	if (navbar[0]) {
		strcat(navbar, "|");
		strcat(navbar, thisPage);
	} else {
		strcpy(navbar, "?nav=");
		strcat(navbar, thisPage);
	}
}


private void
navbutton(int active, int tag, char *start, char *end)
{
	char *sep;
	char *p;
	char buf[MAXPATH];
	int ct;
	static int shiftroot=0;

#if 0
	for (sep = start; sep < end && *sep != '#'; ++sep)
	    ;
	if (sep < end && *sep == '#') ++sep;
#else
	sep = end;
#endif

	/* control field; not an actual button */
	if (start[0] == '!') {
		switch (start[1]) {
		    case '-':
			/* change base url to one that doesn't include user */
			for (p = &root[strlen(root)-2]; p > root && *p != '/';
			    --p) ;
			if (!shiftroot && p > root && strneq(p,"/user=",6)) {
				strncpy(buf, root, (p-root)+1);
				buf[p-root+1] = 0;
				out("<base href=");
				out(buf);
				out(">\n");
				shiftroot = 1;
			}
			break;
		    case '+':
			/* use normal base url */
			if (shiftroot) {
				out("<base href=");
				out(root);
				out(">\n");
				shiftroot = 0;
			}
			break;
		}
		return;
	}
	out("<a style=\"text-decoration: none\" ");
	if (tag) {
		ct = start-arguments;

		if (arguments[ct-1] == '|') --ct;

		sprintf(buf, "href=\"%.*s?%.*s\">",
		    sep-start, start, ct, arguments);
	} else {
		sprintf(buf, "href=\"%.*s\">", sep-start,start);
	}
	out(buf);
	out(active ? "<font size=2 color=lightblue>"
		   : "<font size=2 color=yellow>");

	if (sep < end) {
		sprintf(buf, "%.*s", end-sep, sep);
		out(buf);
		start = 0;
	} else if (strneq(start,"ChangeSet", 9)) {
		if (start[9] != '@') {
			out("All Changesets");
		} else if (start[10] == '+') {
			out("Latest ChangeSet");
		} else if (start[10] == '-') {
			ct = atoi(start+11);
			sprintf(buf,
			    "Changesets in the last %d %s",
			    ct, units(start+11));
			out(buf);
		} else if (start[10] == '.') {
			sprintf(buf, "Changesets before %s", start+12);
			out(buf);
		} else {
			int slen= strlen(start);

			if (start[slen-1] == '.' && start[slen-2] == '.')
				start[slen-2] = 0;
			sprintf(buf, "Changesets after %s", start+10);
			out(buf);
		}
		if (user[0] && !shiftroot) {
			out(" by ");
			out(user);
		}
		start = 0;
	} else if (strneq(start, "index.html", 10)) {
		if (user[0] && !shiftroot) {
			out("Statistics for ");
			out(user);
		} else {
			out("Home");
		}
		start = 0;
	} else if (strneq(start, "hist/", 5)) {
		out("History of ");
		start += 5;
	} else if (strneq(start, "anno/", 5)) {
		out("Annotations for ");
		start += 5;
	} else if (strneq(start, "diffs/", 6)) {
		out("Diffs for ");
		start += 6;
	} else if (strneq(start, "patch@", 6)) {
		out("All diffs for ");
		start += 6;
	} else if (strneq(start, "cset@", 5)) {
		out("ChangeSet ");
		start += 5;
	} else if (strneq(start, "stats", 5)) {
		out("Statistics");
		start = 0;
	} else if (strneq(start, "related/", 8)) {
		out("Changesets for ");
		start += 8;
	} else if (strneq(start, "src", 3)) {
		start += 3;
		if (sep-start == 2 && strneq(start, "/.", 2)) {
			out("Sources");
			start = 0;
		} else {
			for (p = sep; p > start && *p != '/'; --p)
				;
			start = p;
			if (*start == '/') ++start;
		}
	}
	if (start) {
		sprintf(buf, "%.*s", sep-start, start);
		out(buf);
	}
	out("</font></a>\n");
	unless (active) out("<font size=2 color=white>"
			    "<img src=arrow.gif alt=&gt;&gt;>"
			    "</font>\n");
}


private void
printnavbar()
{
	char *start, *end;
	int first = 1;

	/*
	out("<!-- [");out(navbar);out("] -->\n");
	*/
	/* put in a navigation bar */
	out("<table width=100% cellpadding=1>\n"
	    "<tr bgcolor=black><td align=left>\n");
	if (strneq(navbar, "?nav=", 5)) {
		for (start = arguments+5; *start; ++start) {
			for (end = start; *end && *end != '|'; ++end)
				;
			navbutton(0, !first,start,end);
			start = end;
			first = 0;
		}
	}
	navbutton(1, !first, thisPage, thisPage+strlen(thisPage));

	out("</td></tr></table>\n");
}


private void
header(char *path, char *color, char *titlestr, char *headerstr, ...)
{
	char buf[MAXPATH];
	va_list ptr;
	MDBM *m;
	char *t;
	char *fmt = 0;
	char *s;

	out("<html>");

	if (titlestr) {
		va_start(ptr, headerstr);
		vsprintf(buf, titlestr, ptr);
		va_end(ptr);

		out("<head><title>");
		out(buf);
		out("</title></head>\n");

		if (headerstr) {
			va_start(ptr, headerstr);
			vsprintf(buf, headerstr, ptr);
			va_end(ptr);
		}

		fmt = buf;
	}

	out("<body alink=black link=black bgcolor=white>\n");
	if (root && !streq(root, "")) {
		out("<base href=");
		out(root);
		out(">\n");
	}
	out("<table width=100% bgcolor=black"
	    " cellspacing=0 border=0 cellpadding=1>\n"
	    "<tr><td>\n");

	printnavbar();

	unless (include(path, "header.txt")) {
		m = loadConfig(".", 0);
		if (m && (t = mdbm_fetch_str(m, "description")) && strlen(t) < 2000) {
			title(fmt, t, color);
		} else {
			pwd_title(fmt, color);
		}
		if (m) mdbm_close(m);
	}
	out("</td></tr></table>\n");
}



/*
 * Given a pathname, try and find a BitKeeper/etc below.
 * We want the deepest one possible.
 */
private char *
findRoot(char *name)
{
	char	*s, *t;
	char	path[MAXPATH];
	int	tries = 256;

	sprintf(path, "%s/BitKeeper/etc", name);
	if (isdir(path)) {
		chdir(name);
		strcpy(root, url(name));
		if (bk_proj) proj_free(bk_proj);
		bk_proj = proj_init(0);
		return ("index.html");
	}
	for (s = strrchr(name, '/'); s && (s != name); ) {
		*s = 0;
		sprintf(path, "%s/BitKeeper/etc", name);
		unless (--tries) break;		/* just in case */
		if (isdir(path)) {
			chdir(name);
			strcpy(root, url(name));
			if (bk_proj) proj_free(bk_proj);
			bk_proj = proj_init(0);
			return (s + 1);
		}
		t = strrchr(name, '/');
		*s = '/';
		s = t;
	}
	return (0);
}

private void
httphdr(char *file)
{
	char	*buf = aprintf(
	    "HTTP/1.0 200 OK\r\n"
	    "%s\r\n"
	    "Server: bkhttp/%s\r\n"
	    "Content-Type: %s\r\n"
	    "Last-Modified: %s\r\n"
	    "Expires: %s\r\n"
	    "\r\n",
	    http_time(),
	    BKWEB_SERVER_VERSION,
	    type(file),
	    http_time(),
	    http_expires());
	out(buf);
	free(buf);
}

private void
http_file(char *file)
{
	int	fd, n;
	char	buf[BUFSIZ];

	unless ((fd = open(file, 0, 0)) >= 0) return;
	httphdr(file);
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		writen(1, buf, n);
	}
}

private void
http_changes(char *rev)
{
	char	*av[100];
	int	i;
	char	buf[2048];
	char    dspec[MAXPATH];
	MDBM	*m;
	char	*d;

	if (rev) {
		whoami("ChangeSet@%s", rev);
	} else {
		whoami("ChangeSet");
	}

	i = snprintf(dspec, sizeof dspec, "-d%s"
			"$if(:Li: -gt 0){<tr bgcolor=white>\n"
			" <td align=right>:HTML_AGE:</td>\n"
			" <td align=center>:USER:</td>\n"
			" <td align=center"
			"$if(:TAG:){ bgcolor=yellow}>"
			"<a href=cset@:I:%s>:I:</a>"
			"$if(:TAG:){<br>:TAG:}"
//			"$if(:TAG:){$each(:TAG:){<br>(:TAG:)}}"
			"</td>\n"
			" <td>:HTML_C:</td>\n"
			"</tr>\n"
			"}%s", prefix, navbar, suffix);

	if (i == -1) {
		http_error(500, "buffer overflow in http_changes");
	}

	if (!embedded) {
		httphdr(".html");
		header(0, COLOR, "ChangeSet Summaries", 0);
	}

	out(OUTER_TABLE INNER_TABLE
	    "<tr bgcolor=#d0d0d0>\n"
    	    "<th>Age</th><th>Author</th><th>Rev</th>"
	    "<th align=left>&nbsp;Comments</th></tr>\n");

	av[i=0] = "bk";
	av[++i] = "prs";
	av[++i] = "-h";
	if (rev) {
		sprintf(buf, "-r%s", rev);
		av[++i] = buf;
	}
	av[++i] = dspec;
	av[++i] = "ChangeSet";
	av[++i] = 0;
	putenv("BK_YEAR4=1");
	spawnvp_ex(_P_WAIT, "bk", av);
	out(INNER_END OUTER_END);
	if (!embedded) trailer("ChangeSet");
}

private void
http_cset(char *rev)
{
	char	*av[100];
	char	buf[2048];
	FILE	*f;
	MDBM	*m;
	int	i;
	int	fd;
	char	*d, **lines = 0;
	char	dspec[MAXPATH*2];
	pid_t	child;

	whoami("cset@%s", rev);

	i = snprintf(buf, sizeof(buf),
	    "-d%s"
	    "<tr bgcolor=#e0e0e0><td><font size=2>"
	    ":GFILE:@:I:&nbsp;&nbsp;:Dy:-:Dm:-:Dd: :T::TZ:&nbsp;&nbsp;:P:"
	    "$if(:DOMAIN:){@:DOMAIN:}</font><br>\n"
	    "%s"
	    "$if(:GFILE:!=ChangeSet){"
	      "<a href=hist/:GFILE:%s>\n"
	      "<font size=2 color=darkblue>history</font></a>\n"
	      "%s"
	    "}"
	    "</td></tr>\n"
	    "$if(:TAG:){<tr bgcolor=yellow>\n  <td>:TAG:</td>\n</tr>\n}"
	    "<tr bgcolor=white>\n"
	    "<td>:HTML_C:</td></tr>\n"
	    "%s",
	    prefix,
	    isLoggingTree ? "" :
	      "$if(:GFILE:=ChangeSet){"
	        "<a href=patch@:REV:%s>\n"
	        "<font size=2 color=darkblue>all diffs</font></a>\n"
	      "}",
	    navbar,
	    isLoggingTree ? "" :
	      "&nbsp;&nbsp;"
	      "<a href=anno/:GFILE:@:REV:%s>\n"
	      "<font size=2 color=darkblue>annotate</font></a>\n"
	      "&nbsp;&nbsp;"
	      "<a href=diffs/:GFILE:@:REV:%s>\n"
	      "<font size=2 color=darkblue>diffs</font></a>\n",
	    suffix);
	if (i == -1) http_error(500, "buffer overflow in http_cset");
	i = snprintf(dspec, sizeof(dspec), buf, navbar, navbar, navbar);
	if (i == -1) http_error(500, "buffer overflow in http_cset");

	putenv("BK_YEAR4=1");
	sprintf(buf, "bk cset -r%s", rev);
	unless (f = popen(buf, "r")) {
		http_error(500,
		    "bk cset -r%s failed: %s",
		    rev, strerror(errno));
	}

	while (fnext(buf, f)) {
		if (strneq("ChangeSet|", buf, 10)) continue;
		if ((d = strrchr(buf, BK_FS)) && streq(++d, "1.0\n")) continue;
		lines = addLine(lines, strdup(buf));
	}
	pclose(f);

	if (lines) {
		sortLines(lines);

		av[i=0] = "bk";
		av[++i] = "prs";
		av[++i] = "-h";
		av[++i] = dspec;
		av[++i] = "-";
		av[++i] = 0;

		if ((child=spawnvp_wPipe(av, &fd, MAXPATH)) == -1) {
			http_error(500,
			    "spawnvp_wPipe bk: %s", strerror(errno));
		}
	}

	if (!embedded) {
		httphdr("cset.html");
		header("cset", COLOR, "Changeset details for %s", 0, rev);
	}

	out("<table width=100% bgcolor=black cellspacing=0 border=0 "
	    "cellpadding=0><tr><td>\n"
	    "<table width=100% bgcolor=darkgray cellspacing=1 "
	    "border=0 cellpadding=4>\n");

	if (lines) {
		char	changeset[] = CHANGESET;
		sccs	*cset = sccs_init(changeset, 0, 0);
		delta	*d = findrev(cset, rev);

		cset->rstart = cset->rstop = d;
		sccs_prs(cset, 0, 0, &dspec[2], stdout);
		sccs_free(cset);
		fflush(stdout);
		EACH(lines) write(fd, lines[i], strlen(lines[i]));
		freeLines(lines);
		close(fd);
		waitpid(child, &i, 0);
	}

	out(INNER_END OUTER_END);
	if (!embedded) trailer("cset");
}

private void
cat2net(char *path)
{
	char	buf[4<<10];
	int	fd, n;

	unless ((fd = open(path, 0, 0)) >= 0) return;
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		writen(1, buf, n);
	}
	close(fd);
}

include(char *path, char *file)
{
	char	buf[MAXPATH];

	buf[0] = 0;
	if (!path) {
		sprintf(buf, "BitKeeper/html/%s", file);
		if (isreg(buf)) {
			cat2net(buf);
			return (1);
		}
	} else if (strlen(path) < 900) {
		sprintf(buf, "BitKeeper/html/%s/%s", path, file);
		if (isreg(buf)) {
			cat2net(buf);
			return (1);
		}
		/* Allow one header for everything */
		sprintf(buf, "BitKeeper/html/%s", file);
		if (isreg(buf)) {
			cat2net(buf);
			return (1);
		}
	}
	return (0);
}

private void
title(char *title, char *desc, char *color)
{
	unless (title) return;

	out("<tr><td><table width=100% cellpadding=5 cellspacing=0 border=0>\n"
	    "<tr><td align=middle bgcolor=");
	out(color);
	out("><font color=black>");
	if (desc) {
		out(desc);
		out("\n<hr size=1 noshade>\n");
	}
	out(title);
	out("</font></td></tr></table>\n");
}

private void
pwd_title(char *t, char *color)
{
	char	pwd[MAXPATH];

	pwd[0] = 0;
	getcwd(pwd, sizeof(pwd));
	title(t, pwd, color);
}

private void
trailer(char *path)
{
	include(path, "trailer.txt");

	if (isreg("BitKeeper/html/logo.gif")) {
		out("<font color=black size=-2>\n"
		    "<table border=0 bgcolor=white width=100%>\n"
		    "<tr>\n"
		    "<td align=left valign=bottom>"
		    "<img src=\"logo.gif\" alt=\"\"></img></td>\n"
		    "<td align=right valign=bottom>\n"
		    "<a href=http://www.bitkeeper.com>\n"
		    "<img src=trailer.gif alt=\"Learn more about BitKeeper\"></a>\n"
		    "</td></tr></table></font>\n");
	} else {
		out("<p align=center>\n"
		    "<font color=black size=-2>"
		    "<a href=http://www.bitkeeper.com>"
		    "<img src=trailer.gif alt=\"Learn more about BitKeeper\">"
		    "</a></font>\n"
		    "</p>");
		out("</body></html>\n");
	}
}

private int
htmlify(char *str, int len)
{
	char	*end = str + len;
	int	h;
	char	buf[MAXPATH];

	for (h = 0; str < end; str++) {
		if (*str == '<' || *str == '>') {
			buf[h++] = '&';
			buf[h++] = (*str == '<') ? 'l': 'g';
			buf[h++] = 't';
			buf[h++] = ';';
		}
		else
			buf[h++] = *str;

		if (h > MAXPATH-10) {
			if (writen(1, buf, h) != h) return 0;
			h = 0;
		}
	}
	if (h > 0 && writen(1, buf, h) != h) return 0;

	return 1;
}

/* pathname[@rev] */
private void
http_hist(char *pathrev)
{
	char	*av[100];
	char	revision[100];
	char	*s, *d;
	FILE	*f;
	MDBM	*m;
	int	i;
	char	dspec[MAXPATH*2];

	whoami("hist/%s", pathrev);

	i = snprintf(dspec, sizeof(dspec),
		"-d%s<tr bgcolor=white>\n"
		" <td align=right>:HTML_AGE:</td>\n"
		" <td align=center>:USER:</td>\n"
		" <td align=center"
		"$if(:TAG:){ bgcolor=yellow}"
		"$if(:RENAME:){$if(:I:!=1.1){ bgcolor=orange}}>"
		"%s%s%s%s%s"
		"$if(:TAG:){$each(:TAG:){<br>(:TAG:)}}"
		"</td>\n <td>:HTML_C:</td>\n"
		"</tr>\n%s",
		prefix,
		isLoggingTree ? "" : "<a href=\"diffs/:GFILE:@:I:",
		isLoggingTree ? "" : navbar,
		isLoggingTree ? "" : "\">",
		":I:",
		isLoggingTree ? "" : "</a>",
		suffix);
	if (i == -1) {
		http_error(500, "buffer overflow in http_hist");
	}

	if (!embedded) {
		httphdr(".html");
		header("hist", COLOR, "Revision history for %s", 0, pathrev);
	}

	av[i=0] = "bk";
	av[++i] = "prs";
	av[++i] = "-h";
	av[++i] = dspec;
	if (s = strrchr(pathrev, '@')) {
		*s++ = 0;
		sprintf(revision, "-r%s", s);
		av[++i] = revision;
	}
	av[++i] = pathrev;
	av[++i] = 0;
	out("<!-- dspec="); out(dspec); out(" -->\n");

	out(OUTER_TABLE INNER_TABLE
	    "<tr bgcolor=#d0d0d0>\n"
	    " <th>Age</th>\n"
	    " <th>Author</th>\n"
	    " <th>Rev</th>\n"
	    " <th align=left>Comments</th>\n"
	    "</tr>\n");

	putenv("BK_YEAR4=1");

	spawnvp_ex(_P_WAIT, "bk", av);

	out(INNER_END OUTER_END);
	if (!embedded) trailer("hist");
}


/* pathname */
private void
http_src(char *path)
{
	char	buf[32<<10], abuf[30];
	char	html[MAXPATH];
	char	**names = 0;
	int	i;
	MDBM	*m;
	DIR	*d;
	FILE	*f;
	char	*s, *t;
	struct	stat sbuf;
	struct	dirent *e;
	time_t	now;
	char 	dspec[MAXPATH*2];

	whoami("src/%s", path);

	i = snprintf(dspec, sizeof dspec,
	    "%s<tr bgcolor=lightyellow>"
	    " <td><img src=file.gif></td>"
	    " <td>"
	      "$if(:GFILE:=ChangeSet){<a href=ChangeSet@+%s>&nbsp;:G:</a>}"
	      "$if(:GFILE:!=ChangeSet){<a href=hist/:GFILE:%s>&nbsp;:G:</a>}"
	    " </td>"
	    " <td align=center>"
	      "$if(:GFILE:=ChangeSet){<a href=cset@:REV:%s>&nbsp;:REV:</a>}"
	      "$if(:GFILE:!=ChangeSet){<a href=anno/:GFILE:@:REV:%s>:REV:</a>}"
	    " </td>"
	    " <td align=center>"
	      "$if(:GFILE:=ChangeSet){&nbsp;}"
	      "$if(:GFILE:!=ChangeSet){<a href=related/:GFILE:%s>CSets</a>}"
	    " </td>"
	    " <td align=right><font size=2>:HTML_AGE:</font></td>"
	    " <td align=center>:USER:</td>"
	    " <td>:HTML_C:&nbsp;</td>"
	    "</tr>\n%s",
	    prefix,
	    navbar, navbar, navbar, navbar, navbar,
	    suffix);

	if (i == -1) {
		http_error(500, "buffer overflow in http_src");
	}

	if (!path || !*path) path = ".";
	unless (d = opendir(path)) {
		http_error(500, "%s: %s", path, strerror(errno));
	}

	if (!embedded) {
		httphdr(".html");
		header("src", COLOR, "Source directory &lt;%s&gt;", 0,
		    path[1] ? path : "project root");
	}

	out(OUTER_TABLE INNER_TABLE
	    "<tr bgcolor=#d0d0d0>"
	    "<th>&nbsp;</th><th align=left>File&nbsp;name</th>\n"
	    "<th>Rev</th>\n"
	    "<th>&nbsp;</th>\n"
	    "<th>Age</th>\n"
	    "<th>Author</th>\n"
	    "<th align=left>&nbsp;Comments</th>\n"
	    "</tr>\n");

	now = time(0);
	while (e = readdir(d)) {
		if (streq(".", e->d_name) || streq("SCCS", e->d_name) || streq("..", e->d_name)) continue;
		if (path[1]) {
			sprintf(buf, "%s/%s", path, e->d_name);
		} else {
			strcpy(buf, e->d_name);
		}
		if (lstat(buf, &sbuf) == -1) continue;
		if (path[1]) {
			sprintf(buf, "<a href=src/%s/%s%s>", path, e->d_name, navbar);
		} else {
			sprintf(buf, "<a href=src/%s%s>", e->d_name, navbar);
		}
		//s = age(now - sbuf.st_mtime, "&nbsp;");
		if (S_ISDIR(sbuf.st_mode)) {
			sprintf(html, 
			  "<tr bgcolor=lightblue>"
			  "<td><img src=%s></td><td>&nbsp;"
			  "%s%s</td><td>&nbsp;</td>"		/* rev */
			  "<td>&nbsp;</td>"
			  "<td align=right>&nbsp;</td>"
			  "<td>&nbsp;</td>"			/* user */
			  "<td>&nbsp;</td></tr>\n",		/* comments */
			  "dir.gif",
			  buf,
			  e->d_name);
			names = addLine(names, strdup(html));
		}
	}
	closedir(d);

	sprintf(buf,
	    "env BK_YEAR4=1 bk prs -hr+ -d'%s' %s", dspec, path[1] ? path : "");
	f = popen(buf, "r");
	while (fgets(buf, sizeof(buf), f)) {
		/*
		 * XXX - buffer problems, this screws up if we get a line
		 * bigger than sizeof buf.  Just looks ugly.
		 */
		names = addLine(names, strdup(buf));
	}
	pclose(f);
	sortLines(names);
	EACH(names) {
		out(names[i]);
	}
	freeLines(names);
	out(INNER_END OUTER_END);
	if (!embedded) trailer("src");
}


private void
http_anno(char *pathrev)
{
	FILE	*f;
	char	buf[4096];
	int	n, empty = 1;
	char	*s, *d;
	MDBM	*m;

	/* pick up the separator in the revision, but cut it apart after
	 * all the headers have been displayed
	 */
	unless (s = strrchr(pathrev, '@')) {
		http_error(503, "malformed rev %s", pathrev);
	}

	whoami("anno/%s", pathrev);

	if (!embedded) {
		httphdr(".html");
		header("anno",
		    COLOR, "Annotated listing of %s", 0, pathrev);
	}

	*s++ = 0;

	out("<pre><font size=2>");
	sprintf(buf, "bk annotate -uma -r%s %s", s, pathrev);

	/*
	 * Do not show the license key in config file
	 */
	if (streq(pathrev, "BitKeeper/etc/config")) {
		strcat(buf,
		    " | sed -e's/| license:.*$/| license: XXXXXXXXXXXXX/'");
	}
	f = popen(buf, "r");
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		empty = 0;
		htmlify(buf, n);
	}
	pclose(f);
	if (empty) {
		if (isLoggingTree) {
			out("\nThis is an Open Logging tree so there is "
			    "no data in this file.\n");
		} else {
			out("\nEmpty file\n");
		}
	}
	out("</pre>\n");
	if (!embedded) trailer("anno");
}

private void
http_both(char *pathrev)
{
	if (embedded) {
		out("Not implemented yet, check back soon");
	} else {
		header(0, "red", "Not implemented yet, check back soon", 0);
		out("<pre><font size=2>");
		out("</pre>\n");
		trailer(0);
	}
}

#define BLACK 1
#define	OLD 2
#define	NEW 3
#define	BOLD 4
private void
color(char c)
{
	static	int clr;

	unless (c) {
		out("<font color=black>");
		clr = 0;
		return;
	}
	if (c == '-') {
		if (clr != OLD) {
			out("</font><font color=gray>");
			clr = OLD;
		}
	} else if (c == '+') {
		if (clr != NEW) {
			out("</font><font color=blue>");
			clr = NEW;
		}
	} else if (c == 'd') {
		if (clr != BOLD) {
			out("</font><font color=black size=3>");
			clr = NEW;
		}
	} else {
		if (clr != BLACK) {
			out("</font><font color=black>");
			clr = BLACK;
		}
	}
}

private void
http_diffs(char *pathrev)
{
	FILE	*f;
	char	*av[100];
	char	*s;
	MDBM	*m;
	int	n;
	int	i;
	char	dspec[MAXPATH*2];
	char	argrev[100];
	char	buf[4096];


	whoami("diffs/%s", pathrev);

	i = snprintf(dspec, sizeof dspec,
		"-d%s<tr bgcolor=white>\n"
		" <td align=right>:HTML_AGE:</td>\n"
		" <td align=center>:USER:$if(:DOMAIN:){@:DOMAIN:}</td>\n"
		" <td align=center><a href=anno/:GFILE:@:I:%s>:I:</a></td>\n"
		" <td>:HTML_C:</td>\n"
		"</tr>\n%s", prefix, navbar, suffix);

	if (i == -1) {
		http_error(500, "buffer overflow (#1) in http_diffs");
	}

	unless (s = strrchr(pathrev, '@')) {
		http_error(503, "malformed rev %s", pathrev);
	}

	if (snprintf(argrev, sizeof argrev, "-r%s", 1+s) == -1) {
		http_error(500, "buffer overflow (#2) in http_diffs");
	}

	if (!embedded) {
		httphdr(".html");
		header("diffs", COLOR, "Changes for %s", 0, pathrev);
	}

	*s++ = 0;

	out(OUTER_TABLE INNER_TABLE
	    "<tr bgcolor=#d0d0d0>\n"
	    " <th>Age</th>\n"
	    " <th>Author</th>\n"
	    " <th>Annotate</th>\n"
	    " <th align=left>Comments</th>\n"
	    "</tr>\n");

	av[i=0] = "bk";
	av[++i] = "prs";
	av[++i] = "-h";
	av[++i] = argrev;
	av[++i] = dspec;
	av[++i] = pathrev;
	av[++i] = 0;

	spawnvp_ex(_P_WAIT, "bk", av);

	out(INNER_END OUTER_END);

	if (strstr(s, "..")) {
		sprintf(buf, "bk diffs -ur%s %s", s, pathrev);
	} else {
		sprintf(buf, "bk diffs -uR%s %s", s, pathrev);
	}
	f = popen(buf, "r");
	out("<pre>\n"
	    "<font size=2>\n");
	color(0);
	fnext(buf, f);
	while (fnext(buf, f)) {
		color(buf[0]);
		htmlify(buf, strlen(buf));
	}
	pclose(f);
	out("</font>\n"
	    "</pre>\n");
	if (!embedded) trailer("diffs");
}

private void
http_patch(char *rev)
{
	FILE	*f;
	char	buf[4096];
	int	n;
	char	*s;
	MDBM	*m;

	whoami("patch@%s", rev);

	if (!embedded) {
		httphdr(".html");
		header("rev", COLOR,
		    "All diffs for ChangeSet %s",
		    0,
		    rev, navbar, rev);
	}

	out("<pre><font size=2>");
	sprintf(buf, "bk export -T -h -x -tpatch -r%s", rev);
	f = popen(buf, "r");
	color(0);
	while (fgets(buf, sizeof(buf), f)) {
		color(buf[0]);
		if (buf[0] == 'd') {
			out("<table width=100%>\n");
			out("<tr bgcolor=lightblue><td>");
		}
		htmlify(buf, strlen(buf));
		if (buf[0] == 'd') out("</td></tr></table>");
	}
	pclose(f);
	out("</pre>\n");
	if (!embedded) trailer("patch");
}

private void
http_gif(char *name)
{
	extern	char bkpowered_gif[];
	extern	int bkpowered_len;
	extern	char file_gif[];
	extern	int file_len;
	extern	char dir_gif[];
	extern	int dir_len;
	extern	char back_gif[];
	extern	int back_len;
	extern	char arrow_gif[];
	extern	int arrow_len;

	if (*name == '/')  name++;
	if (streq(name, "bkpowered.gif") || streq(name, "trailer.gif")) {
		httphdr("BK.gif");
		writen(1, bkpowered_gif, bkpowered_len);
	} else if (streq(name, "file.gif")) {
		httphdr("BK.gif");
		writen(1, file_gif, file_len);
	} else if (streq(name, "dir.gif")) {
		httphdr("BK.gif");
		writen(1, dir_gif, dir_len);
	} else if (streq(name, "back.gif")) {
		httphdr("BK.gif");
		writen(1, back_gif, back_len);
	} else if (streq(name, "arrow.gif")) {
		httphdr("BK.gif");
		writen(1, arrow_gif, arrow_len);
	}
}

private char*
units(char *t)
{
	int	n = atoi(t);

	while (isdigit(*t)) t++;
	switch (*t) {
	    case 'm': return (n > 1 ? "minutes" : "minute");
	    case 'h': case 'H': return (n > 1 ? "hours" : "hour");
	    case 'd': case 'D': return (n > 1 ? "days" : "day");
	    case 'w': case 'W': return (n > 1 ? "weeks" : "week");
	    case 'M': return (n > 1 ? "months" : "month");
	    case 'y': case 'Y': return (n > 1 ? "years" : "year");
	    default: return ("unknown units");
    	}
}


private void
http_stats(char *page)
{
	int	recent_cs, all_cs;
	char	c_user[80];
	char	user[80];
	int	ct;
	char	units[80];
	char	buf[200];
	FILE	*p;

	unless (p = popen("bk prs -h -d'$if(:Li: -gt 0){:USER: :AGE:\n}' ChangeSet | bk _sort", "r"))
		http_error(500, "bk prs failed: %s", strerror(errno));

	c_user[0] = 0;
	recent_cs = all_cs = 0;

	/* don't use whoami, because I need to have absolute urls for
	 * the parent pages in the navbar
	 */
	sprintf(navbar, "?nav=!-|index.html|stats|!+", root, root);
	sprintf(thisPage, "stats");

	if (!embedded) {
		httphdr(".html");
		header("stats", COLOR,
		    "User statistics",
		    0);
	}


	out(OUTER_TABLE INNER_TABLE
	    "<tr bgcolor=#d0d0d0>\n"
	    "<th>Author</th>\n"
	    "<th>Recent changesets</th>\n"
	    "<th>All changesets</th></tr>\n");

	while (fnext(buf,p)) {
		unless (sscanf(buf, " %s %d %s", user, &ct, units) == 3) {
			strtok(buf, "\n");
			out("<!-- reject ["); out(buf); out("] -->\n");
			continue;
		}

		if (!streq(user, c_user)) {
			if (c_user[0]) {
				sprintf(buf,
				    "<tr bgcolor=white>\n"
				    "<td align=center>"
				    "<a href=\"user=%s%s\">%s</a></td>\n"
				    "<td align=center>%d</td>\n"
				    "<td align=center>%d</td></tr>\n",
				    c_user, navbar, c_user,
				    recent_cs, all_cs);
				out(buf);
			}
			strcpy(c_user, user);
			recent_cs = all_cs = 0;
		}
		if (strneq(units, "year", 4)) {
			ct *= 365*24;
		} else if (strneq(units, "month", 5)) {
			ct *= 30*24;
		} else if (strneq(units, "week", 4)) {
			ct *= 7*24;
		} else if (strneq(units, "day", 3)) {
			ct *= 24;
		}
		all_cs ++;
		if (ct <= 30*24) recent_cs ++;
	}
	pclose(p);
	if (all_cs > 0) {
		sprintf(buf,
		    "<tr bgcolor=white>\n"
		    "<td align=center>"
		    "<a href=\"user=%s%s\">%s</a></td>\n"
		    "<td align=center>%d</td>\n"
		    "<td align=center>%d</td></tr>\n",
		    c_user, navbar, c_user,
		    recent_cs, all_cs);
		out(buf);
	}
	out(INNER_END OUTER_END);
	if (!embedded) trailer("patch");
}


private void
http_index(char *page)
{
	sccs	*s = sccs_init(CHANGESET, INIT_NOCKSUM|INIT_NOSTAT, 0);
	delta	*d;
	time_t	now, t1h, t1d, t2d, t3d, t4d, t1w, t2w, t3w;
	time_t	t4w, t8w, t12w, t6m, t9m, t1y, t2y, t3y;
	int	c1h=0, c1d=0, c2d=0, c3d=0, c4d=0;
	int	c1w=0, c2w=0, c3w=0, c4w=0, c8w=0, c12w=0, c6m=0, c9m=0;
	int	c1y=0, c2y=0, c3y=0, c=0, cm=0;
	char	buf[MAXPATH*2];
	char	titlebar[200];
	char	*email=0, *desc=0, *contact=0, *category=0;
	char	*bkweb=0, *master=0, *homepage=0;
	MDBM	*m;

	time(&now);
	t1h = now - (60*60);
	t1d = now - (24*60*60);
	t2d = now - (2*24*60*60);
	t3d = now - (3*24*60*60);
	t4d = now - (4*24*60*60);
	t1w = now - (7*24*60*60);
	t2w = now - (14*24*60*60);
	t3w = now - (21*24*60*60);
	t4w = now - (31*24*60*60);
	t8w = now - (2*31*24*60*60);
	t12w = now - (3*31*24*60*60);
	t6m = now - (6*31*24*60*60);
	t9m = now - (9*31*24*60*60);
	t1y = now - (365*24*60*60);
	t2y = now - (2*365*24*60*60);
	t3y = now - (3*365*24*60*60);
	for (d = s->table; d; d = d->next) {
		if (user[0] && !streq(user, d->user)) continue;
		if (d->type == 'R') continue;
		unless (d->added > 0) {
			unless (d == s->tree) cm++;
			continue;
		}
		assert(d->type == 'D');
		if (NOFUDGE(d) >= t1h) c1h++;
		if (NOFUDGE(d) >= t1d) c1d++;
		if (NOFUDGE(d) >= t2d) c2d++;
		if (NOFUDGE(d) >= t3d) c3d++;
		if (NOFUDGE(d) >= t4d) c4d++;
		if (NOFUDGE(d) >= t1w) c1w++;
		if (NOFUDGE(d) >= t2w) c2w++;
		if (NOFUDGE(d) >= t3w) c3w++;
		if (NOFUDGE(d) >= t4w) c4w++;
		if (NOFUDGE(d) >= t8w) c8w++;
		if (NOFUDGE(d) >= t12w) c12w++;
		if (NOFUDGE(d) >= t6m) c6m++;
		if (NOFUDGE(d) >= t9m) c9m++;
		if (NOFUDGE(d) >= t1y) c1y++;
		if (NOFUDGE(d) >= t2y) c2y++;
		if (NOFUDGE(d) >= t3y) c3y++;
		c++;
	}
	sccs_free(s);

	whoami("index.html");

	if (embedded) {
	    out("<!-- ["); out(navbar); out("] -->\n");
	}
	else {
		httphdr(".html");
		/* don't use header() here; this is one place where the regular
		 * header.txt is not needed
		 */
		if (m = loadConfig(".", 0)) {
			desc = mdbm_fetch_str(m, "description");
			contact = mdbm_fetch_str(m, "contact");
			email = mdbm_fetch_str(m, "email");
			category = mdbm_fetch_str(m, "category");
			bkweb = mdbm_fetch_str(m, "bkweb");
			master = mdbm_fetch_str(m, "master");
			homepage = mdbm_fetch_str(m, "homepage");
		}

		out("<html><head><title>\n");
		out(desc ? desc : "ChangeSet activity");
		out("\n"
		    "</title></head>\n"
		    "<body alink=black link=black bgcolor=white>\n");
		if (root && !streq(root, "")) {
			out("<base href=");
			out(root);
			out(">\n");
		}
		out("<table width=100% bgcolor=black"
		    " border=0 cellspacing=0 cellpadding=1>\n"
		    "<tr><td>\n");

		printnavbar();

		unless (include(0, "homepage.txt")) {
			if (desc && strlen(desc) < MAXPATH) {
				sprintf(buf, "%s<br>", desc);
			} else {
				getcwd(buf, sizeof buf);
				strcat(buf, "<br>");
			}
			if (category && strlen(category) < MAXPATH) {
				sprintf(buf+strlen(buf), "(%s)<br>", category);
			}
			if (email && contact) {
				sprintf(buf+strlen(buf),
					"<a href=\"mailto:%s\">%s</a>",
					email, contact);
			}

			if (user[0] && snprintf(titlebar, sizeof titlebar,
			    "ChangeSet activity for %s", user) != -1) {
				title(titlebar, buf, COLOR);
			} else {
				title("ChangeSet activity", buf, COLOR);
			}
		}
		out("</td></tr></table>\n");
		if (m) mdbm_close(m);
	}

	out("<p><table bgcolor=#e0e0e0 border=1 align=middle>\n");
	out("<tr><td align=middle valign=top width=50%>\n");
	out("<table cellpadding=3>\n");
#define	DOIT(c, l, u, t) \
	if (c && (c != l)) { \
		sprintf(buf, "<tr><td><a href=ChangeSet@-%s%s>", u, navbar); \
		out(buf); \
		sprintf(buf, \
		    "%d&nbsp;Changesets&nbsp;in&nbsp;the&nbsp;last&nbsp;%s</a>", c, t); \
		out(buf); \
		out("</td></tr>\n"); \
	}
	DOIT(c1h, 0, "1h", "hour");
	DOIT(c1d, c1h, "1d", "day");
	DOIT(c2d, c1d, "2d", "two&nbsp;days");
	DOIT(c3d, c2d, "3d", "three&nbsp;days");
	DOIT(c4d, c3d, "4d", "four&nbsp;days");
	DOIT(c1w, c4d, "7d", "week");
	DOIT(c2w, c1w, "2w", "two&nbsp;weeks");
	DOIT(c3w, c2w, "3w", "three&nbsp;weeks");
	DOIT(c4w, c3w, "4w", "four&nbsp;weeks");
	DOIT(c8w, c4w, "8w", "eight&nbsp;weeks");
	DOIT(c12w, c8w, "12w", "twelve&nbsp;weeks");
	DOIT(c6m, c12w, "6M", "six&nbsp;months");
	DOIT(c9m, c6m, "9M", "nine&nbsp;months");
	DOIT(c1y, c9m, "1y", "year");
	DOIT(c2y, c1y, "2y", "two&nbsp;years");
	DOIT(c3y, c2y, "3y", "three&nbsp;years");
	sprintf(buf,"<tr><td><a href=ChangeSet%s>", navbar);
	out(buf);
	if (cm) {
		sprintf(buf, "All %d changesets (%d empty merges)", c+cm, cm);
	} else {
		sprintf(buf, "All %d changesets", c+cm);
	}
	out(buf);
	out("</a></td></tr></table>\n");
	out("</td><td align=middle valign=top width=50%>\n");
	out("<table cellpadding=3>\n\n");
	unless (user[0]) {
		out("<tr><td align=middle>");
		sprintf(buf,
		    "<a href=stats%s>User statistics</a></td></tr>\n", navbar);
		out(buf);
	}
	out("<tr><td align=middle>");
	sprintf(buf, "<a href=tags%s>Tags</a></td></tr>\n", navbar);
	out(buf);
	out("<tr><td align=middle>");
	sprintf(buf,
	    "<a href=src%s>Browse the source tree</a></td></tr>", navbar);
	out(buf);
	if (bkweb && !exists(WEBMASTER)) {
		out("<tr><td align=middle>");
		sprintf(buf,
		    "<a href=%s>BK/Web site for this package</a></td></tr>\n",
		    bkweb);
		out(buf);
	}
	if (homepage) {
		out("<tr><td align=middle>");
		sprintf(buf,
		    "<a href=%s>Home page for this package</a></td></tr>\n",
		    homepage);
		out(buf);
	}
	if (master) {
		out("<tr><td align=middle>");
		sprintf(buf, "Master repository at %s</td></tr>\n", master);
		out(buf);
	}
#ifdef notyet
	out("<tr><td><a href=http://www.bitkeeper.com/bkweb/help.html>Help"
	    "</a></td></tr>\n");
#endif
	out("<tr><td align=middle>"
	    "<form method=get action=search/>\n"
	    "<INPUT size=26 type=text name=expr><br>\n"
	    "<table><tr><td><font size=2>\n"
	    "<input type=radio name=search "
	    "value=\"ChangeSet comments\" checked>"
	    "Changeset comments<br>\n"
	    "<input type=radio name=search value=\"file comments\">"
	    "File comments<br>\n");
	unless (isLoggingTree) {
		out("<input type=radio name=search value=\"file contents\">"
		    "File contents<br>\n");
	}
	out("</td><td align=right>"
	    "<input type=submit value=Search><br>"
	    "<input type=reset value=\"Clear search\">\n"
	    "</font></td></tr></table></form>\n");
	out("</table>");
	out("</table>");
	if (!embedded) trailer(0);
}

/*
 * "Tue, 28 Jan 97 01:20:30 GMT";
 *  012345678901234567890123456
 */
private char	*
http_time()
{
	time_t	tt;
	static	time_t save_tt;
	struct	tm *t;
	static	struct tm save_tm;
	static	char buf[100];

	time(&tt);
	if (tt == save_tt) return (buf);
	save_tt = tt;
	t = gmtime(&tt);
	if (buf[0] && (tt - save_tt < 3600)) {
		buf[22] = t->tm_sec / 10 + '0';
		buf[21] = t->tm_sec % 10 + '0';
		save_tm.tm_sec = t->tm_sec;
		if (save_tm.tm_min == t->tm_min) return (buf);
	}
	save_tm = *t;
	strftime(buf, sizeof(buf), "%a, %d %b %y %H:%M:%S %Z", t);
	return(buf);
}

/*
 * "Tue, 28 Jan 97 01:20:30 GMT";
 *  012345678901234567890123456
 */
private char	*
http_expires()
{
	time_t	expires;
	struct	tm *t;
	static	char buf[100];

	time(&expires);
	expires += 60;
	t = gmtime(&expires);
	strftime(buf, sizeof(buf), "%a, %d %b %y %H:%M:%S %Z", t);
	return(buf);
}

private char	*
type(char *name)
{
	int	len = strlen(name);

	if (!strcmp(&name[len - 4], ".gif")) {
		return "image/gif";
	}
	if (!strcmp(&name[len - 5], ".jpeg")) {
		return "image/jpeg";
	}
	if (!strcmp(&name[len - 5], ".html")) {
		return "text/html";
	}
	return "text/plain";
}


private void
http_error(int status, char *fmt, ...)
{
        char    buf[2048];
	va_list	ptr;
	int     size;
	int     ct;

	if (embedded) {
		buf[0] = 0;
	} else {
		sprintf(buf,
		    "HTTP/1.0 %d Error\r\n"
		    "%s\r\n"
		    "Server: bkhttp/%s\r\n"
		    "Content-Type: text/html\r\n"
		    "\r\n",
		    status, http_time(), BKWEB_SERVER_VERSION);
		out(buf);

		strcpy(buf, "<html><head><title>Error!</title></head>\n"
			    "<body alink=black link=black bgcolor=white>\n");
	}
	sprintf(buf+strlen(buf), "<h2>Error %d</h2>\n", status);
	size = strlen(buf);

	va_start(ptr,fmt);
	ct = vsnprintf(buf+size, sizeof buf-size-1, fmt, ptr);
	va_end(ptr);

	if (ct == -1) {
		/* buffer overflow -- we didn't really need that message */
		buf[size] = 0;
	} else {
		strcat(buf, "\n");
	}
	out("<center>\n"
            "<table>\n"
	    "<tr bgcolor=red fgcolor=white>\n"
	    "  <td align=center>\n");
	out(buf);
	out("  </td>\n"
	    "</tr>\n"
	    "</table>\n"
            "</center>\n");

	if (embedded) return;

	out("<hr>\n"
	    "<table width=100%>\n"
	    "<tr>\n"
	    "  <th valign=top align=left>bkhttp/" BKWEB_SERVER_VERSION " server on ");
	out(url(0));
	out("  </th>\n"
	    "  <td align=right><a href=http://www.bitkeeper.com>"
	    "<img src=/trailer.gif alt=\"Learn more about BitKeeper\"></a>"
	    "  </td>\n"
	    "</tr>\n"
	    "</table>\n");
	out("</body>\n");
	exit(1);
}


/* if path is null, you get back a url suitable for placing in an error
 * message ``bkweb server on host:port'', and if path is not null, you
 * get back a real url suitable for placing into a href
 */
private char *
url(char *path)
{
	static char buf[MAXPATH*2];

	strcpy(buf, "http://");
	strcat(buf, sccs_gethost());
	if (Opts.port) sprintf(buf+strlen(buf), ":%d", Opts.port);
	if (path) {
		strcat(buf, "/");
		strcat(buf, path);
		unless (buf[strlen(buf)-1] == '/') strcat(buf, "/");
	}
	return buf;
}


private void
http_page(char *page, vfn content, char *argument)
{
    static char buf[MAXPATH];
    int arglen = strlen(arguments), navlen = strlen(navbar);
    int i = -1;
    FILE *f;

    if (page) i = snprintf(buf, sizeof buf, "BitKeeper/html/%s.html", page);

    if (i != -1 && isreg(buf) && (f = fopen(buf, "r")) != 0) {
	    embedded = 1;
	    httphdr(".html");
	    while (fgets(buf, sizeof buf, f)) {
		    if (strncmp(buf, ".CONTENT.", 9) == 0) {
			    arguments[arglen] = 0;
			    navbar[navlen] = 0;
			    (*content)(argument);
		    } else {
			    out(buf);
		    }
	    }
	    fclose(f);
    } else {
	    embedded = 0;
	    (*content)(argument);
    }
}

private int
has_temp_license()
{
	fd_set fds;
	struct timeval delay;
	char ack[5];
	int fd;
	int timeleft = 30;
#define PULL	0x01
#define	CLONE	0x02
	int need = PULL|CLONE;
	int i;

	extern int licenseServer[2];
	extern time_t licenseEnd;

	if (time(0) < licenseEnd) return 1;


	for (i=0; cmds[i].name; i++) {
		if (streq(cmds[i].name, "pull")) need &= ~PULL;
		if (streq(cmds[i].name, "clone")) need &= ~CLONE;
	}
	if (need) return 0;

	fd = licenseServer[0];

    again:
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	delay.tv_sec = 5;
	delay.tv_usec = 0;

	unless (select(fd+1, 0, &fds, 0, &delay) > 0 && FD_ISSET(fd, &fds))
		return 0;

	if (write(fd, "MMI?", 4) == 4) {
		ack[4] = 0;
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		delay.tv_sec = 1;
		delay.tv_usec = 0;

		unless (select(fd+1, &fds, 0, 0, &delay)) return 0;

		if (FD_ISSET(fd, &fds) && read(fd, ack, 4) == 4) {
			if (strneq(ack, "YES\0", 4)) return 1;

			if (strneq(ack, "NO\0\0", 4) && --timeleft > 0) {
				sleep(1);
				goto again;
			}

			return 0;
		}
	}

	return 0;
}



private char *
parseurl(char *url)
{
	char *s;

	thisPage[0] = arguments[0] = navbar[0] = user[0] = 0;
	prefix[0] = suffix[0] = 0;

	while (s = strrchr(url, '?')) {
		if (strneq(s, "?nav=", 5)) {
			strcpy(arguments, s);
			strcpy(navbar, s);
		} else if (strneq(s, "?expires=", 8)) {
			expires=atoi(s+9);
		} else if (strneq(s, "?expr=", 5)) {
			expr = strdup(&s[6]);
		}
		*s = 0;
	}
	if (strneq(url, "user=", 5)) {
		if (s = strchr(url, '/')) {
			*s++ = 0;
		} else {
			s = url;
		}
		strcpy(user, url+5);

		sprintf(prefix, "$if(:USER:=%s){", user);
		sprintf(suffix, "}");

		return (s == url) ? "" : s;
	}
	return url;
}


private void
http_related(char *file)
{
	char	*av[100];
	int	i, j;
	int     flen;
	char	buf[2048];
	char    dspec[MAXPATH];
	char	*d;
	char	*c;
	FILE	*f;
	int	count = 0;
	sccs	*s = sccs_init(CHANGESET, INIT_NOCKSUM|INIT_NOSTAT, 0);

	whoami("related/%s", file);

	unless (s) http_error(500, "cannot initialize " CHANGESET);

	i = snprintf(dspec, sizeof dspec, "%s<tr bgcolor=white>\n"
			" <td align=right>:HTML_AGE:</td>\n"
			" <td align=center>:USER:</td>\n"
			" <td align=center"
			"$if(:TAG:){ bgcolor=yellow}>"
			"<a href=cset@:I:%s>:I:</a>"
			"$if(:TAG:){$each(:TAG:){<br>(:TAG:)}}"
			"</td>\n"
			" <td>:HTML_C:</td>\n"
			"</tr>\n%s", prefix, navbar, suffix);

	if (i == -1)
		http_error(500, "buffer overflow in http_related");

	sprintf(buf, "bk f2csets %s", file);
	unless (f = popen(buf, "r"))
		http_error(500, "%s: %s", buf, strerror(errno));

	while (fnext(buf, f)) {
		chop(buf);
		if (rangeList(s, buf) == 0) count++;
	}
	pclose(f);

	if (!embedded) {
		httphdr(".html");
		header(0, COLOR, "Changesets that modify %s", 0, file);
	}

	if (count) {
		out(OUTER_TABLE INNER_TABLE
		    "<tr bgcolor=#d0d0d0>\n"
		    "<th>Age</th><th>Author</th><th>Rev</th>"
		    "<th align=left>&nbsp;Comments</th></tr>\n");

		if (f = fdopen(1, "w")) {
			sccs_prs(s, 0, 0, dspec, f);
			fflush(f);
		}

		out(INNER_END OUTER_END);
	}

	if (!embedded) trailer("related");
	if (f) fclose(f);
}

private void
http_license(char *page)
{
	char arg[5];
	extern int licenseServer[2];

	if (expires > 0 && expires < 480) {
		/* also check the ip address we're coming from */
		sprintf(arg, "S%03d", expires);
		write(licenseServer[0], arg, 4);
	}
	exit(0);
}


private void
http_tags(char *page)
{
	char	*av[100];
	int	i;
	char    dspec[MAXPATH];

	whoami("tags");

	i = snprintf(dspec, sizeof dspec, "-d%s$if(:TAG:!=''){"
	    "<tr bgcolor=white>\n"
	    "  <td align=right>:HTML_AGE:</td>\n"
	    "  <td align=center bgcolor=yellow>\n"
	    "      <a href=\"cset@:REV:%s\">:TAG:</a></td>\n"
	    "  <td><a href=\"ChangeSet@..:REV:%s\">earlier CSets</a></td>\n"
	    "  <td><a href=\"ChangeSet@:REV:..%s\">later CSets</a></td>\n"
	    "  <td>:HTML_C:</td>\n"
	    "</tr>}%s", prefix, navbar, navbar, navbar, suffix);

	if (i == -1)
		http_error(500, "buffer overflow in http_tags");

	if (!embedded) {
		httphdr(".html");
		header(0, COLOR, "Tags", 0);
	}

	out(OUTER_TABLE INNER_TABLE
	    "<tr bgcolor=#d0d0d0>\n"
	    "  <th>Age</th>\n"
	    "  <th>Tag</th>\n"
	    "  <th>&nbsp;</th>\n"
	    "  <th>&nbsp;</th>\n"
	    "  <th align=left>&nbsp;Comments</th>\n"
	    "</tr>\n");

	av[i=0] = "bk";
	av[++i] = "prs";
	av[++i] = "-h";
	av[++i] = dspec;
	av[++i] = CHANGESET;
	av[++i] = 0;

	spawnvp_ex(_P_WAIT, "bk", av);

	out(INNER_END OUTER_END);

	if (!embedded) trailer("tags");
}

private void
expand(char *s)
{
	char	*buf = malloc(strlen(s) + 1);
	char	*t = buf;

	while (*s) {
		unless (*s == '%') {
			*t++ = *s++;
			continue;
		}
		s++;
		*t++ = strtoul(s, 0, 16);
		while (isdigit(*s)) s++;
	}
	*t = 0;
	strcpy(expr, buf);
	free(buf);
}

private void
http_search(char *junk)
{
	char	*s, *base, *file, *rev, *text;
	int	which, i, first = 1;
	FILE	*f;
	char	buf[8<<10];

	whoami("index.html");

	/*
	 * More+than+one+word+%26tc&search=comments
	 * translates to
	 * More than one word &tc
	 */
#define	SEARCH_CSET	1
#define	SEARCH_COMMENTS	2
#define	SEARCH_CONTENTS	3
	which = SEARCH_CSET;
	s = "ChangeSet comments";
	if ((s = strrchr(expr, '&')) && strneq(s, "&search=", 8)) {
		*s = 0;
		s += 8;
		if (strchr(s, '+')) *strchr(s, '+') = ' ';
		if (streq(s, "file comments")) which = SEARCH_COMMENTS;
		if (streq(s, "file contents")) which = SEARCH_CONTENTS;
	}
	expand(expr);
	unless (strlen(expr)) http_error(404, "Search for what?\n");

	if (!embedded) {
		httphdr(".html");
		i = snprintf(buf,
		    sizeof(buf), "Search results for \"%s\" in %s\n", expr, s);
		if (i == -1) {
			header(0, COLOR, "Search results", 0);
		} else {
			header(0, COLOR, buf, 0);
		}
	}

	switch (which) {
	    case SEARCH_CSET:
		sprintf(buf, "bk prs -h "
		    "-d'$each(:C:){:GFILE:\t:I:\t(:C:)\n}' ChangeSet");
		break;
	    case SEARCH_CONTENTS:
		sprintf(buf, "bk -Ur grep -r+ -fm '%s'", expr);
		break;
	    case SEARCH_COMMENTS:
		sprintf(buf, "bk -Ur prs -h "
		    "-d'$each(:C:){:GFILE:\t:I:\t(:C:)\n}'");
		break;
	}
	unless (f = popen(buf, "r")) http_error(404, "grep failed?\n");
	
next:	while (fnext(buf, f)) {
		file = buf;
		unless (rev = strchr(buf, '\t')) continue;
		*rev++ = 0;
		unless (base = strrchr(file, '/')) base = file; else base++;
		unless (text = strchr(rev, '\t')) continue;
		*text++ = 0;
		unless ((which == SEARCH_CONTENTS) || strstr(text, expr)) {
			continue;
		}
		if (first) {
			out("<pre><strong>");
			out("Filename         Rev        Match</strong>\n");
			out("<hr size=1 noshade><br>");
			first = 0;
		}
		if (which == SEARCH_CSET) {
			out("<a href=cset");
		} else {
			out("<a href=anno/");
			out(file);
		}
		out("@");
		out(rev);
		out(">");
		out(base);
		out("</a>");
		for (i = strlen(base); i <= 16; ++i) out(" ");
		if (which == SEARCH_CSET) {
			out("<a href=patch");
		} else {
			out("<a href=diffs/");
			out(file);
		}
		out("@");
		out(rev);
		out(">");
		out(rev);
		out("</a>");
		for (i = strlen(rev); i <= 10; ++i) out(" ");
		htmlify(text, strlen(text));
	}
	pclose(f);
	if (first) {
		out("<p><table align=middle><tr><td>"
		    "No matches found.</td></tr></table><p>\n");
	} else {
		out("</pre>");
	}

	if (!embedded) trailer("search");
}
