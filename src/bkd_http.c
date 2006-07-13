#include "bkd.h"
#include "logging.h"
#include "range.h"
typedef void (*vfn)(char *page);

private char	*http_time(void);
private char	*http_expires(int secs);
private	char	*type(char *name);
private void	httphdr(char *file);
private void	http_error(int status, char *fmt, ...);
private void	http_index(char *page);
private void	http_changes(char *page);
private void	http_cset(char *page);
private void	http_anno(char *page);
private void	http_diffs(char *page);
private void	http_dir(char *page);
private void	http_prs(char *page);
private void	http_patch(char *page);
private void	http_stats(char *page);
private void	http_search(char *page);
private void	http_related(char *page);
private void	http_tags(char *page);
private void	http_robots(void);
private void	http_repos(char *page);
private	void	http_reserved(char *file);
private void	title(char *title, char *desc, char *color);
private void	pwd_title(char *t, char *color);
private void	header(char *color, char *title, char *header, ...);
private void	printnavbar(void);
private int	parseurl(char *);
private	void	mk_querystr(void);
private void	trailer(void);
private	void	detect_oldurls(char *url);


#define	COLOR		"lightblue"

#define	OUTER_TABLE	"<table width=100% bgcolor=black cellspacing=0 border=0 cellpadding=2><tr><td>\n"
#define INNER_TABLE	"<table width=100% cellpadding=3 cellspacing=1 border=0 class=\"sortable\" id=\"t1\">"
#define	INNER_END	"</table>"
#define OUTER_END	"</td></tr></table>\n"

#define BKWEB_SERVER_VERSION	"0.5"
#define	BKWWW		"/BitKeeper/www/"

private	char	*header_host;	/* hostname for http headers */
private	hash	*qin;		/* the query keys in current url */
private	hash	*qout;		/* query keys in urls on this page */
private char	*querystr;	/* str to added to urls on page */
private char	*user;		/* qin["USER"] */
private char	*prefix;	/* if user '$if(:USER:=user){' */
private char	*suffix;	/* if user '}' */
private	char	*root;		/* if set, path from bkd to repo root */
private	char	*fpath;		/* path to file on url from root */

private struct pageref {
	vfn	content;
	char	*page;
} pages[] = {
    { http_index,   "index" },
    { http_changes, "changes" },
    { http_cset,    "cset" },
    { http_dir,     "dir" },
    { http_prs,	    "prs" },
    { http_patch,   "patch"},
    { http_anno,    "anno" },
    { http_diffs,   "diffs" },
    { http_search,  "search" },
    { http_stats,   "stats"  },
    { http_related, "related" },
    { http_tags,    "tags" },
    { http_repos,   "repos" },
    { 0 },
};

int
cmd_httpget(int ac, char **av)
{
	char	*url, *key;
	char	*page;
	int 	i;
	char	buf[MAXPATH];

	/*
	 * Read http header (if any).
	 */
	if (ac > 2) {
		while (fnext(buf, stdin)) {
			if (strneq(buf, "Host:", 5)) {
				char	*s;
				s = &buf[5];
				while (isspace(*s)) s++;
				if (header_host) {
					free(header_host);
					header_host = 0;
				}
				/*
				 * Unused currently, but will be needed for
				 * virtual host support.
				 */
				header_host = strnonldup(s);
			} else if (buf[0] == '\r' || buf[0] == '\n') {
				break;
			}
			/* ignore everything else */
		}
	}
	unless (av[1]) http_error(404, "get what?\n");
	url = av[1];

	if (streq(url, "/robots.txt")) http_robots();

	qin = hash_new(HASH_MEMHASH);
	qout = hash_new(HASH_MEMHASH);

	detect_oldurls(url);

	/* expand url and extract arguments */
	if (parseurl(url)) http_error(503, "unable to parse URL");

	if (strneq(fpath, "BitKeeper/www/", 14)) {
		http_reserved(fpath + 14);
		exit(0);
	}

	/* check for a proper license */
	if (root) {
		unless (key = lease_bkl(0, 0)) {
			http_error(503,
		    "Cannot find BitKeeper license for this repository.");
		}
		unless (license_bklbits(key) & LIC_WEB) {
			http_error(503,
			    "BK/Web option has not been purchased.");
		}
	}
	page = hash_fetchStr(qin, "PAGE");
	for (i = 0; pages[i].content; i++) {
		if (streq(pages[i].page, page)) {
			pages[i].content(page);
			exit(0); /* NOT return */
		}
	}
	http_error(403, "illegal page request");
	exit(0);		/* NOT return */
}

private void
navbutton(char *title, char *url)
{
	if (!url) {
		printf("<font size=2 color=lightblue>%s</font>\n", title);
	} else {
		printf("<a style=\"text-decoration: none\" href=\"%s\">"
		    "<font size=2 color=yellow>%s</font></a>\n",
		    url, title);
	}
}

private void
navarrow(void) {
	printf("<font size=2 color=white>"
	    "<img src=" BKWWW "arrow.gif alt=&gt;&gt;>"
	    "</font>");
}

private void
printnavbar(void)
{
	char	*p, *dir, *file, *linkstr;
	char	*rev;
	FILE	*f;
	int	i;
	char	**items;
	char	buf[MAXLINE];

	/* put in a navigation bar */
	puts("<table width=100% cellpadding=1>\n"
	    "<tr bgcolor=black><td align=left>");

	navbutton("TOP", "/");

	p = buf;
	if (root) {
		/* in repo */
		dir = root;
	} else {
		dir = fpath;
	}
	dir = (root ? root : fpath);
	unless (streq(dir, ".")) {
		*p++ = '/';
		items = splitLine(dir, "/", 0);
		EACH(items) {
			p += sprintf(p, "%s/", items[i]);
			navarrow();
			navbutton(items[i], buf);
		}
		freeLines(items, free);
	}
	if (root && !streq(fpath, ".")) {
		items = splitLine(fpath, "/", 0);
		if (fpath[strlen(fpath)-1] != '/') {
			file = popLine(items);
		} else {
			file = 0;
		}
		EACH(items) {
			p += sprintf(p, "%s/", items[i]);
			navarrow();
			navbutton(items[i], buf);
		}
		freeLines(items, free);
		if (file) {
			navarrow();
			navbutton(file, 0);
			free(file);
		}
	}
	fputs("</td>", stdout);
	linkstr = hash_toStr(qin);
	if (root && (rev = hash_fetchStr(qin, "REV"))) {
		sprintf(buf, "bk -R prs -hnd:MD5KEY: -r'%s' -- '%s'",
		    rev, (fpath && !streq(fpath, ".")) ? fpath : "ChangeSet");
		f = popen(buf, "r");
		if (fnext(buf, f)) {
			chomp(buf);
		} else {
			buf[0] = 0;
		}
		if (pclose(f)) buf[0] = 0;
		if (buf[0]) {
			hash	*tmphash = hash_new(HASH_MEMHASH);

			hash_fromStr(tmphash, linkstr);
			hash_storeStr(tmphash, "REV", buf);
			free(linkstr);
			linkstr = hash_toStr(tmphash);
			hash_free(tmphash);
		}
	}
	printf(
	       "<td align=right><a style=\"text-decoration: none\" "
	       "href=\"?%s\">"
	       "<font size=2 color=white>[ link to this page ]</font>"
	       "</a></td>",
	       linkstr);
	free(linkstr);
	puts("</tr></table>");
}

private void
header(char *color, char *titlestr, char *headerstr, ...)
{
	char	*t, *fmt = 0;
	va_list	ptr;
	char	buf[MAXPATH];

	puts("<html><head>");
	if (titlestr) {
		va_start(ptr, headerstr);
		vsprintf(buf, titlestr, ptr);
		va_end(ptr);

		printf("<title>%s</title>\n", buf);

		if (headerstr) {
			va_start(ptr, headerstr);
			vsprintf(buf, headerstr, ptr);
			va_end(ptr);
		}

		fmt = buf;
	}
#ifdef SORTTABLE
	printf("<script type=\"text/javascript\" src=" BKWWW "sorttable.js>"
	    "</script>\n");
	printf("<style type=text/css>\n"
	    "table.sortable a.sortheader {\n"
	    "  background-color:#d0d0d0;\n"
	    "  font-weight: bold;\n"
	    "  text-decoration: none;\n"
	    "  display: block;\n"
	    "}\n"
	    "table.sortable span.sortarrow {\n"
	    "  color: black;\n"
	    "  text-decoration: none;\n"
	    "}\n"
	    "</style>\n");
#endif
	puts("</head>");

	puts("<body alink=black link=black bgcolor=white>");
	puts("<table width=100% bgcolor=black"
	    " cellspacing=0 border=0 cellpadding=1>\n"
	    "<tr><td>");

	printnavbar();

	t = proj_configval(0, "description");
	if (*t && (strlen(t) < 2000)) {
		title(fmt, t, color);
	} else {
		pwd_title(fmt, color);
	}
	puts("</td></tr></table>");
}

private void
httphdr(char *file)
{
	printf(
	    "HTTP/1.0 200 OK\r\n"
	    "Date: %s\r\n"
	    "Server: bkhttp/%s\r\n"
	    "Content-Type: %s\r\n"
	    "Last-Modified: %s\r\n"
	    "Expires: %s\r\n"
	    "\r\n",
	    http_time(),
	    BKWEB_SERVER_VERSION,
	    type(file),
	    http_time(),
	    http_expires(60));	/* expires in 60 seconds */
}

private void
http_changes(char *page)
{
	int	i;
	FILE	*f;
	char    *dspec;
	char	*rev = hash_fetchStr(qin, "REV");
	char	*av[100];
	char	buf[2048];

	httphdr(".html");
	header(COLOR, "ChangeSet Summaries", 0);

	puts(OUTER_TABLE INNER_TABLE
	    "<tr bgcolor=#d0d0d0>\n"
    	    "<th>Age</th><th>Author</th><th>Rev</th>"
	    "<th align=left>&nbsp;Comments</th></tr>");

	av[i=0] = "bk";
	av[++i] = "prs";
	av[++i] = "-h";
	if (rev) {
		sprintf(buf, "-r%s", rev);
		av[++i] = buf;
	}
	av[++i] = dspec = aprintf("-d%s"
	    "$if(:Li: -gt 0){<tr bgcolor=white>\n"
	    " <td align=right>:HTML_AGE:</td>\n"
	    " <td align=center>:USER:</td>\n"
	    " <td align=center$if(:TAG:){ bgcolor=yellow}>\n"
	    "|:REV:\n"
	    "$if(:TAG:){$each(:TAG:){<br>TAG: (:TAG:) }}"
	    "</td>\n"
	    " <td>:HTML_C:</td>\n"
	    "</tr>\n"
	    "}%s",
	    prefix, suffix);
	av[++i] = "ChangeSet";
	av[++i] = 0;
	f = popenvp(av, "r");
	free(dspec);

	hash_storeStr(qout, "PAGE", "cset");
	while (fnext(buf, f)) {
		if (buf[0] == '|') {
			chomp(buf);
			hash_storeStr(qout, "REV", buf+1);
			mk_querystr();
			printf("<a href=\"%s\">%s</a>\n", querystr, buf+1);
		} else {
			fputs(buf, stdout);
		}
	}
	pclose(f);
	puts(INNER_END OUTER_END);
	trailer();
}

private void
http_cset(char *page)
{
	char	*av[100];
	FILE	*f;
	int	i;
	char	*s, *t, *dspec;
	char	*rev = hash_fetchStr(qin, "REV");
	char	buf[2048];

	httphdr("cset.html");
	header(COLOR, "Changeset details for %s", 0, rev);

	puts("<table width=100% bgcolor=black cellspacing=0 border=0 "
	    "cellpadding=0><tr><td>\n"
	    "<table width=100% bgcolor=darkgray cellspacing=1 "
	    "border=0 cellpadding=4>");

	dspec = aprintf("-d%s"
	    "<tr bgcolor=#e0e0e0><td><font size=2>\n"
	    "#:GFILE:@:REV:&nbsp;&nbsp;:Dy:-:Dm:-:Dd: :T::TZ:&nbsp;&nbsp;:P:"
	    "$if(:DOMAIN:){@:DOMAIN:}</font><br>\n"
	    "</td></tr>\n"
	    "$if(:TAG:){"
	      "<tr bgcolor=yellow><td>"
	      "$each(:TAG:){TAG: (:TAG:)<br>}</td>\n</tr>\n"
	    "}"
	    "<tr bgcolor=white>\n"
	    "<td>:HTML_C:</td></tr>\n"
	    "%s",
	    prefix, suffix);

	av[i=0] = "bk";
	av[++i] = "changes";
	sprintf(buf, "-r%s", rev);
	av[++i] = buf;
	av[++i] = "-vn";
	av[++i] = dspec;
	av[++i] = 0;

	f = popenvp(av, "r");
	free(dspec);
	while (fnext(buf, f)) {
		if (buf[0] != '#') {
			fputs(buf, stdout);
			continue;
		}
		fputs(buf+1, stdout);

		t = strchr(buf, '@');
		*t++ = 0;
		s = strchr(t, '&');
		*s = 0;
		if (streq(buf+1, "ChangeSet")) {
			hash_storeStr(qout, "PAGE", "patch");
			hash_storeStr(qout, "REV", t);
			mk_querystr();
			printf("<a href=%s>\n"
			    "<font size=2 color=darkblue>all diffs</font></a>",
			    querystr);
		} else {
			hash_storeStr(qout, "PAGE", "prs");
			hash_deleteStr(qout, "REV");
			mk_querystr();
			printf("<a href=\"%s%s\">\n"
			    "<font size=2 color=darkblue>history</font></a>\n",
			    buf+1, querystr);
			hash_storeStr(qout, "PAGE", "anno");
			hash_storeStr(qout, "REV", t);
			mk_querystr();
			printf("&nbsp;&nbsp;"
			    "<a href=\"%s%s\">"
			    "<font size=2 color=darkblue>annotate</font>"
			    "</a>\n",
			    buf+1, querystr);
			hash_storeStr(qout, "PAGE", "diffs");
			mk_querystr();
			printf("&nbsp;&nbsp;"
			    "<a href=\"%s%s\">"
			    "<font size=2 color=darkblue>diffs</font></a>\n",
			    buf+1, querystr);
		}
	}
	pclose(f);

	fputs(INNER_END OUTER_END, stdout);
	trailer();
}

private void
title(char *title, char *desc, char *color)
{
	unless (title) return;

	printf("<tr><td>"
	    "<table width=100%% cellpadding=5 cellspacing=0 border=0>\n"
	    "<tr><td align=middle bgcolor=%s><font color=black>", color);
	if (desc) {
		puts(desc);
		puts("\n<hr size=1 noshade>\n");
	}
	puts(title);
	puts("</font></td></tr></table>\n");
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
trailer(void)
{
	puts("<p align=center>\n"
	    "<font color=black size=-2>"
	    "<a href=http://www.bitkeeper.com>"
	    "<img src=" BKWWW "trailer.gif alt=\"Learn more about BitKeeper\">"
	    "</a></font>\n"
	    "</p>");
	puts("</body></html>");
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
		} else if (*str == '&') {
			strcpy(&buf[h], "&amp;");
			h += 5;
		}
		else
			buf[h++] = *str;

		if (h > MAXPATH-10) {
			if (fwrite(buf, 1, h, stdout) != h) return 0;
			h = 0;
		}
	}
	if (h > 0 && fwrite(buf, 1, h, stdout) != h) return 0;

	return 1;
}

private void
http_prs(char *page)
{
	char	*revision = 0;
	FILE	*f;
	char	**items;
	int	i;
	char	*rev = hash_fetchStr(qin, "REV");
	char	*av[100];
	char	buf[MAXPATH*2];

	hash_storeStr(qout, "PAGE", "diffs");

	httphdr(".html");
	header(COLOR, "Revision history for %s", 0, fpath);

	puts(OUTER_TABLE INNER_TABLE
	    "<tr bgcolor=#d0d0d0>\n"
	    " <th>Age</th>\n"
	    " <th>Author</th>\n"
	    " <th>Rev</th>\n"
	    " <th align=left>Comments</th>\n"
	    "</tr>");

	av[i=0] = "bk";
	av[++i] = "prs";
	av[++i] = "-hn";
	av[++i] = "-d"
	    ":I:|"					   /* 1 rev */
	    "$if(:RENAME:){1}0|"			   /* 2 isrename */
	    ":HTML_AGE:|"				   /* 3 age */
	    ":USER:|"					   /* 4 user */
	    "$if(:TAG:){$each(:TAG:){<br>TAG: (:TAG:)}} |" /* 5 tags */
	    ":HTML_C:";					   /* 6 comments */
	if (rev) av[++i] = revision = aprintf("-r%s", rev);
	av[++i] = fpath;
	av[++i] = 0;

	f = popenvp(av, "r");
	while (fnext(buf, f)) {
		chomp(buf);

		items = splitLine(buf, "|", 0);
		if (streq(items[1], "1.1")) items[2][0] = ' ';

		hash_storeStr(qout, "REV", items[1]);
		mk_querystr();
		printf("%s<tr bgcolor=white>\n"
		    " <td align=right>%s</td>\n"
		    " <td align=center>%s</td>\n"
		    " <td align=center%s%s>"
		    "<a href=\"%s\">%s</a>"
		    "%s"
		    "</td>\n <td>%s</td>\n"
		    "</tr>\n%s",
		    prefix, items[3], items[4],
		    ((items[5][0] != ' ') ? " bgcolor=yellow" : ""),
		    ((items[2][0] == '1') ? " bgcolor=orange" : ""),
		    querystr, items[1], items[5], items[6],
		    suffix);
		freeLines(items, free);
	}
	fclose(f);

	puts(INNER_END OUTER_END);
	trailer();
	if (revision) free(revision);
}


private void
http_dir(char *page)
{
	char	*cmd, *file, *gfile, *rev, *enc;
	int	i;
	char	**d;
	FILE	*f;
	struct	stat sbuf;
	char	buf[MAXLINE];

	unless (d = getdir(fpath)) {
		http_error(500, "%s: %s", fpath, strerror(errno));
	}
	httphdr(".html");
	header(COLOR, "Source directory &lt;%s&gt;", 0,
	    fpath[1] ? fpath : "project root");

	puts(OUTER_TABLE INNER_TABLE
	    "<tr bgcolor=#d0d0d0>"
	    "<th>&nbsp;</th>\n"
	    "<th align=left>File&nbsp;name</th>\n"
	    "<th>Rev</th>\n"
	    "<th>&nbsp;</th>\n"
	    "<th>Age</th>\n"
	    "<th>Author</th>\n"
	    "<th align=left>&nbsp;Comments</th>\n"
	    "</tr>");

	hash_storeStr(qout, "PAGE", "dir");
	mk_querystr();
	EACH (d) {
		if (streq("SCCS", d[i])) continue;
		concat_path(buf, fpath, d[i]);
		if (lstat(buf, &sbuf) == -1) continue;
		unless (S_ISDIR(sbuf.st_mode)) continue;
		printf("<tr bgcolor=lightblue>"
		    "<td><img src=%s></td>"
		    "<td>&nbsp;<a href=\"%s/%s\">%s</a></td>"
		    "<td>&nbsp;</td>"			/* rev */
		    "<td>&nbsp;</td>"
		    "<td align=right>&nbsp;</td>"
		    "<td>&nbsp;</td>"			/* user */
		    "<td>&nbsp;</td></tr>\n",		/* comments */
		    BKWWW "dir.gif",
		    d[i], querystr,
		    d[i]);
	}
	freeLines(d, free);

	cmd = aprintf("bk prs -h -r+ -d'"
	    "%s<tr bgcolor=lightyellow>"
	    " <td><img src=" BKWWW "file.gif></td>\n"
	    "|:GFILE:|:REV:\n"
	    " <td align=right><font size=2>:HTML_AGE:</font></td>"
	    " <td align=center>:USER:</td>"
	    " <td>:HTML_C:&nbsp;</td>"
	    "</tr>\n%s' %s",
	    prefix, suffix, fpath);
	f = popen(cmd, "r");
	free(cmd);
	while (fnext(buf, f)) {
		if (buf[0] != '|') {
			fputs(buf, stdout);
			continue;
		}
		chomp(buf);
		file = buf + 1;
		gfile = basenm(file);
		rev = strchr(file, '|');
		*rev++ = 0;
		if (streq(file, "./ChangeSet")) {
			hash_storeStr(qout, "PAGE", "changes");
			hash_storeStr(qout, "REV", "+");
			mk_querystr();
			printf("<td><a href=%s>&nbsp;%s</a></td>",
			    querystr, gfile);
			hash_storeStr(qout, "PAGE", "cset");
			mk_querystr();
			printf("<td align=center>"
			    "<a href=%s>%s</a></td>"
			    "<td headers=csets>&nbsp;</td>",
			    querystr, rev);
		} else {
			hash_storeStr(qout, "PAGE", "prs");
			hash_deleteStr(qout, "REV");
			mk_querystr();
			enc = str_pullup(0,
			    webencode(0, gfile ,strlen(gfile)+1));
			printf("<td>"
			    "<a href=\"%s%s\">&nbsp;%s</a></td>",
			    enc, querystr, gfile);
			hash_storeStr(qout, "PAGE", "anno");
			hash_storeStr(qout, "REV", "+");
			mk_querystr();
			printf("<td align=center>"
			    "<a href=\"%s%s\">%s</a>"
			    "</td>",
			    enc, querystr, rev);
			hash_storeStr(qout, "PAGE", "related");
			hash_deleteStr(qout, "REV");
			mk_querystr();
			printf("<td align=center>"
			    "<a href=\"%s%s\">CSets</a></td>",
			    enc, querystr);
			free(enc);
		}
	}
	pclose(f);
	puts(INNER_END OUTER_END);
	trailer();
}


private void
http_anno(char *page)
{
	FILE	*f;
	char	buf[4096];
	int	n, empty = 1;
	char	*rev = hash_fetchStr(qin, "REV");

	httphdr(".html");
	header(COLOR, "Annotated listing of %s", 0, fpath);

	printf("<pre><font size=3>");
	sprintf(buf, "bk annotate -Aru -r'%s' %s", rev, fpath);

	/*
	 * Do not show the license key in config file
	 */
	if (streq(fpath, "BitKeeper/etc/config")) {
		strcat(buf,
		    " | sed -e's/| license:.*$/| license: XXXXXXXXXXXXX/'");
	}
	f = popen(buf, "r");
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		empty = 0;
		htmlify(buf, n);
	}
	pclose(f);
	if (empty) puts("\nEmpty file");
	puts("</pre>");
	trailer();
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
		fputs("<font color=black>", stdout);
		clr = 0;
		return;
	}
	if (c == '-') {
		if (clr != OLD) {
			fputs("</font><font color=gray>", stdout);
			clr = OLD;
		}
	} else if (c == '+') {
		if (clr != NEW) {
			fputs("</font><font color=blue>", stdout);
			clr = NEW;
		}
	} else if (c == 'd') {
		if (clr != BOLD) {
			fputs("</font><font color=black size=3>", stdout);
			clr = NEW;
		}
	} else {
		if (clr != BLACK) {
			fputs("</font><font color=black>", stdout);
			clr = BLACK;
		}
	}
}

private void
http_diffs(char *page)
{
	FILE	*f;
	char	*av[100];
	int	i;
	char	*rev = hash_fetchStr(qin, "REV");
	char	dspec[MAXPATH*2];
	char	argrev[100];
	char	buf[4096];

	httphdr(".html");
	header(COLOR, "Changes for %s", 0, fpath);

	hash_storeStr(qout, "PAGE", "anno");
	hash_storeStr(qout, "REV", rev);
	mk_querystr();

	i = snprintf(dspec, sizeof dspec,
		"-d%s<tr bgcolor=white>\n"
		" <td align=right>:HTML_AGE:</td>\n"
		" <td align=center>:USER:$if(:DOMAIN:){@:DOMAIN:}</td>\n"
		" <td align=center><a href=\"%s\">:I:</a></td>\n"
		" <td>:HTML_C:</td>\n"
		"</tr>\n%s", prefix, querystr, suffix);

	if (i == -1) {
		http_error(500, "buffer overflow (#1) in http_diffs");
	}

	if (snprintf(argrev, sizeof argrev, "-r%s", rev) == -1) {
		http_error(500, "buffer overflow (#2) in http_diffs");
	}

	puts(OUTER_TABLE INNER_TABLE
	    "<tr bgcolor=#d0d0d0>\n"
	    " <th>Age</th>\n"
	    " <th>Author</th>\n"
	    " <th>Annotate</th>\n"
	    " <th align=left>Comments</th>\n"
	    "</tr>");

	av[i=0] = "bk";
	av[++i] = "prs";
	av[++i] = "-h";
	av[++i] = argrev;
	av[++i] = dspec;
	av[++i] = fpath;
	av[++i] = 0;

	fflush(stdout);
	spawnvp(_P_WAIT, "bk", av);

	puts(INNER_END OUTER_END);

	if (strstr(rev, "..")) {
		sprintf(buf, "bk diffs -ur'%s' %s", rev, fpath);
	} else {
		sprintf(buf, "bk diffs -uR'%s' %s", rev, fpath);
	}
	f = popen(buf, "r");
	printf("<pre><font size=3>");
	color(0);
	fnext(buf, f);
	while (fnext(buf, f)) {
		color(buf[0]);
		htmlify(buf, strlen(buf));
	}
	pclose(f);
	printf("</font></pre>");
	trailer();
}

private void
http_patch(char *page)
{
	FILE	*f;
	char	*rev = hash_fetchStr(qin, "REV");
	char	buf[4096];

	httphdr(".html");
	header(COLOR, "All diffs for ChangeSet %s", 0, rev, querystr, rev);

	printf("<pre><font size=3>");
	sprintf(buf, "bk changes -vvr'%s'", rev);
	f = popen(buf, "r");
	color(0);
	while (fgets(buf, sizeof(buf), f)) {
		color(buf[0]);
		if ((buf[0] == '=') || (buf[0] == '#')) {
			printf("<table width=100%%>");
			printf("<tr bgcolor=#e0e0e0><td><font size=2>");
			chomp(buf);
		}
		htmlify(buf, strlen(buf));
		if ((buf[0] == '=') || (buf[0] == '#')) {
			puts("</font></td></tr></table>");
		}
	}
	pclose(f);
	puts("</pre>");
	trailer();
}

private void
http_stats(char *page)
{
	int	recent_cs, all_cs;
	char	c_user[80];
	int	ct;
	char	units[80];
	char	buf[200];
	FILE	*p;

	unless (p = popen("bk prs -h -d'$if(:Li: -gt 0){:USER: :AGE:\n}' ChangeSet | bk sort", "r"))
		http_error(500, "bk prs failed: %s", strerror(errno));

	c_user[0] = 0;
	recent_cs = all_cs = 0;

	httphdr(".html");
	header(COLOR, "User statistics", 0);

	puts(OUTER_TABLE INNER_TABLE
	    "<tr bgcolor=#d0d0d0>\n"
	    "<th>Author</th>\n"
	    "<th>Recent changesets</th>\n"
	    "<th>All changesets</th></tr>");

	while (fnext(buf,p)) {
		char	f_user[80];
		unless (sscanf(buf, " %79s %d %199s", f_user, &ct, units) == 3) {
			strtok(buf, "\n");
			printf("<!-- reject [%s] -->\n", buf);
			continue;
		}

		if (!streq(f_user, c_user)) {
			hash_storeStr(qout, "USER", c_user);
			mk_querystr();
			if (c_user[0]) {
				printf(
				    "<tr bgcolor=white>\n"
				    "<td align=center>"
				    "<a href=\"%s\">%s</a></td>\n"
				    "<td align=center>%d</td>\n"
				    "<td align=center>%d</td></tr>\n",
				    querystr, c_user,
				    recent_cs, all_cs);
			}
			strcpy(c_user, f_user);
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
		hash_storeStr(qout, "USER", c_user);
		mk_querystr();
		printf(
		    "<tr bgcolor=white>\n"
		    "<td align=center>"
		    "<a href=\"%s\">%s</a></td>\n"
		    "<td align=center>%d</td>\n"
		    "<td align=center>%d</td></tr>\n",
		    querystr, c_user,
		    recent_cs, all_cs);
	}
	puts(INNER_END OUTER_END);
	trailer();
}


private void
http_index(char *page)
{
	sccs	*s = sccs_init(CHANGESET, INIT_NOCKSUM|INIT_NOSTAT);
	delta	*d;
	time_t	now, t1h, t1d, t2d, t3d, t4d, t1w, t2w, t3w;
	time_t	t4w, t8w, t12w, t6m, t9m, t1y, t2y, t3y;
	int	c1h=0, c1d=0, c2d=0, c3d=0, c4d=0;
	int	c1w=0, c2w=0, c3w=0, c4w=0, c8w=0, c12w=0, c6m=0, c9m=0;
	int	c1y=0, c2y=0, c3y=0, c=0, cm=0;
	char	buf[MAXPATH*2];
	char	*email=0, *desc=0, *contact=0, *category=0;
	char	*bkweb=0, *master=0, *homepage=0;
	MDBM	*m;

	time(&now);
	t1h = now - HOUR;
	t1d = now - DAY;
	t2d = now - 2*DAY;
	t3d = now - 3*DAY;
	t4d = now - 4*DAY;
	t1w = now - WEEK;
	t2w = now - 2*WEEK;
	t3w = now - 3*WEEK;
	t4w = now - 4*WEEK;
	t8w = now - 8*WEEK;
	t12w = now - 12*WEEK;
	t6m = now - 6*MONTH;
	t9m = now - 9*MONTH;
	t1y = now - YEAR;
	t2y = now - 2*YEAR;
	t3y = now - 3*YEAR;
	for (d = s->table; d; d = d->next) {
		if (user && !streq(user, d->user)) continue;
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

	if (m = proj_config(0)) {
		desc = mdbm_fetch_str(m, "description");
		contact = mdbm_fetch_str(m, "contact");
		email = mdbm_fetch_str(m, "email");
		category = mdbm_fetch_str(m, "category");
		bkweb = mdbm_fetch_str(m, "bkweb");
		master = mdbm_fetch_str(m, "master");
		homepage = mdbm_fetch_str(m, "homepage");
	}

	httphdr(".html");
	/* don't use header() here; this is one place where the regular
	 * header.txt is not needed
	 */
	puts("<html><head><title>");
	puts(desc ? desc : "ChangeSet activity");
	puts("\n"
	    "</title></head>\n"
	    "<body alink=black link=black bgcolor=white>\n");
	puts("<table width=100% bgcolor=black"
	    " border=0 cellspacing=0 cellpadding=1>\n"
	    "<tr><td>");

	printnavbar();

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

	if (user) {
		char	*titlebar;
		titlebar = aprintf("ChangeSet activity for %s",
		    user);
		title(titlebar, buf, COLOR);
		free(titlebar);
	} else {
		title("ChangeSet activity", buf, COLOR);
	}
	puts("</td></tr></table>");

	puts("<p><table bgcolor=#e0e0e0 border=1 align=middle>");
	puts("<tr><td align=middle valign=top width=50%>");
	puts("<table cellpadding=3>");
	hash_storeStr(qout, "PAGE", "changes");
#define	DOIT(c, l, u, t) \
	if (c && (c != l)) { \
		hash_storeStr(qout, "REV", u); \
		mk_querystr(); \
		printf("<tr><td><a href=%s>", querystr); \
		printf( \
		    "%d&nbsp;Changesets&nbsp;in&nbsp;the&nbsp;last&nbsp;%s</a>", c, t); \
		printf("</td></tr>\n"); \
	}
	DOIT(c1h, 0, "-1h", "hour");
	DOIT(c1d, c1h, "-1d", "day");
	DOIT(c2d, c1d, "-2d", "two&nbsp;days");
	DOIT(c3d, c2d, "-3d", "three&nbsp;days");
	DOIT(c4d, c3d, "-4d", "four&nbsp;days");
	DOIT(c1w, c4d, "-7d", "week");
	DOIT(c2w, c1w, "-2w", "two&nbsp;weeks");
	DOIT(c3w, c2w, "-3w", "three&nbsp;weeks");
	DOIT(c4w, c3w, "-4w", "four&nbsp;weeks");
	DOIT(c8w, c4w, "-8w", "eight&nbsp;weeks");
	DOIT(c12w, c8w, "-12w", "twelve&nbsp;weeks");
	DOIT(c6m, c12w, "-6M", "six&nbsp;months");
	DOIT(c9m, c6m, "-9M", "nine&nbsp;months");
	DOIT(c1y, c9m, "-1y", "year");
	DOIT(c2y, c1y, "-2y", "two&nbsp;years");
	DOIT(c3y, c2y, "-3y", "three&nbsp;years");
	hash_deleteStr(qout, "REV");
	mk_querystr();
	printf("<tr><td><a href=%s>", querystr);
	if (cm) {
		printf("All %d changesets (%d empty merges)", c+cm, cm);
	} else {
		printf("All %d changesets", c+cm);
	}
	puts("</a></td></tr></table>");
	puts("</td><td align=middle valign=top width=50%>");
	puts("<table cellpadding=3>\n");
	unless (user) {
		puts("<tr><td align=middle>");
		hash_storeStr(qout, "PAGE", "stats");
		mk_querystr();
		printf("<a href=%s>User statistics</a></td></tr>\n",
		    querystr);
	}
	puts("<tr><td align=middle>");
	hash_storeStr(qout, "PAGE", "tags");
	mk_querystr();
	printf("<a href=%s>Tags</a></td></tr>\n", querystr);
	puts("<tr><td align=middle>");
	hash_storeStr(qout, "PAGE", "dir");
	mk_querystr();
	printf("<a href=%s>Browse the source tree</a></td></tr>", querystr);
	if (bkweb && !exists(WEBMASTER)) {
		puts("<tr><td align=middle>");
		printf("<a href=\"%s\">BK/Web site for this package</a>"
		    "</td></tr>\n", bkweb);
	}
	if (homepage) {
		puts("<tr><td align=middle>");
		printf("<a href=\"%s\">"
		    "Home page for this package</a></td></tr>\n",
		    homepage);
	}
	if (master) {
		puts("<tr><td align=middle>");
		printf("Master repository at %s</td></tr>\n", master);
	}
#ifdef notyet
	puts("<tr><td><a href=http://www.bitkeeper.com/bkweb/help.html>Help"
	    "</a></td></tr>\n");
#endif
	puts("<tr><td align=middle>"
	    "<form method=get>\n"
	    "<input type=hidden name=PAGE value=search>\n");
	puts("<INPUT size=26 type=text name=EXPR><br>\n"
	    "<table><tr><td><font size=2>\n"
	    "<input type=radio name=SEARCH "
	    "value=\"ChangeSet comments\" checked>"
	    "Changeset comments<br>\n"
	    "<input type=radio name=SEARCH value=\"file comments\">"
	    "File comments<br>\n");
	puts("<input type=radio name=SEARCH value=\"file contents\">"
	    "File contents<br>\n");
	puts("</td><td align=right>"
	    "<input type=submit value=Search><br>"
	    "<input type=reset value=\"Clear search\">\n"
	    "</font></td></tr></table></form>\n");
	puts("</table>");
	puts("</table>");
	trailer();
}

/*
 * rfc2616 3.3.1 (rfc1123)
 * "Tue, 28 Jan 1997 01:20:30 GMT";
 *  01234567890123456789012345678
 */
private char	*
http_time(void)
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
		buf[23] = t->tm_sec / 10 + '0';
		buf[24] = t->tm_sec % 10 + '0';
		save_tm.tm_sec = t->tm_sec;
		if (save_tm.tm_min == t->tm_min) return (buf);
	}
	save_tm = *t;
	strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", t);
	return(buf);
}

/*
 * "Tue, 28 Jan 1997 01:20:30 GMT";
 *  01234567890123456789012345678
 */
private char	*
http_expires(int secs)
{
	time_t	expires;
	struct	tm *t;
	static	char buf[100];

	time(&expires);
	expires += secs;
	t = gmtime(&expires);
	strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", t);
	return(buf);
}

private char	*
type(char *name)
{
	int	len = strlen(name);
	int	i, extlen;
	struct {
		char	*ext;
		char	*type;
	} table[] = {
		{"gif", "image/gif" },
		{"jpeg", "image/jpeg" },
		{"html", "text/html" },
		{"htm", "text/html" },
		{"js", "text/javascript" },
		{"css", "text/css" },
		{0,0}
	};

	for (i = 0; table[i].ext; i++) {
		extlen = strlen(table[i].ext);
		if ((name[len - extlen - 1] == '.') &&
		    streq(name + len - extlen, table[i].ext)) {
			return (table[i].type);
		}
	}
	return ("text/plain");
}


private void
http_error(int status, char *fmt, ...)
{
        char    buf[2048];
	va_list	ptr;
	int     size;
	int     ct;

	printf(
	    "HTTP/1.0 %d Error\r\n"
	    "Date: %s\r\n"
	    "Server: bkhttp/%s\r\n"
	    "Content-Type: text/html\r\n"
	    "\r\n",
	    status, http_time(), BKWEB_SERVER_VERSION);

	strcpy(buf, "<html><head><title>Error!</title></head>\n"
	    "<body alink=black link=black bgcolor=white>\n");
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
	puts("<center>\n"
            "<table>\n"
	    "<tr bgcolor=red fgcolor=white>\n"
	    "  <td align=center>\n");
	puts(buf);
	puts("  </td>\n"
	    "</tr>\n"
	    "</table>\n"
            "</center>\n");

	puts("<hr>\n"
	    "<table width=100%>\n"
	    "<tr>\n"
	    "  <th valign=top align=left>bkhttp/" BKWEB_SERVER_VERSION " server");
	puts("  </th>\n"
	    "  <td align=right><a href=http://www.bitkeeper.com>"
	    "<img src=" BKWWW "trailer.gif alt=\"Learn more about BitKeeper\">"
	    "  </a>"
	    "  </td>\n"
	    "</tr>\n"
	    "</table>\n");
	puts("</body>\n");
	exit(1);
}

private int
parseurl(char *url)
{
	char	*s, *cwd, *proot, *page;
	int	i, dir;
	project	*p;
	char	*newurl;

	unless (*url++ == '/') return (-1); /* skip leading slash */

	/* extract query_string */
	if (s = strrchr(url, '?')) {
		*s++ = 0;
		hash_fromStr(qin, s);
	}
	webdecode(url, &newurl, 0);
	url = newurl;

	if (user = hash_fetchStr(qin, "USER")) {
		hash_storeStr(qout, "USER", user);
		prefix = aprintf("$if(:USER:=%s){", user);
		suffix = strdup("}");
	} else {
		prefix = strdup("");
		suffix = strdup("");
	}
	mk_querystr();

	/*
	 * url should now be a relative path from the current directory,
	 * (not counting /BitKeeper/www)
	 * Handle directries without slashes.
	 */
	dir = (!*url || isdir(url));
	if (*url && (url[strlen(url)-1] != '/') && dir) {
		/* redirect with trailing / */
		printf("HTTP/1.0 301\r\n"
		    "Date: %s\r\n"
		    "Server: bkhttp/%s\r\n"
		    "Location: /%s/\r\n"
		    "\r\n",
		    http_time(), BKWEB_SERVER_VERSION,
		    url);
		exit(0);
	}

	if (p = proj_init(url)) {
		cwd = proj_cwd();
		i = strlen(cwd);
		proot = proj_root(p);

		if (streq(proot, cwd)) {
			root = strdup(".");
		} else {
			unless (strneq(proot, cwd, i) && (proot[i] == '/')) {
				/* followed symlink? */
				http_error(503, "Can't find project root");
			}
			root = strdup(proot + i + 1);
		}
		fpath = proj_relpath(p, url);
		chdir(proot);
		proj_free(p);
	} else {
		if (*url) {
			fpath = strdup(url);
			if (dir) chop(fpath);	/* kill trailing / */
		} else {
			fpath = ".";
		}
	}
	unless (page = hash_fetchStr(qin, "PAGE")) {
		if (root) {
			if (streq(fpath, ".")) {
				page = "index";
			} else {
				page = dir ? "dir" : "prs";
			}
		} else {
			page = dir ? "repos" : "cat";
		}
		hash_storeStr(qin, "PAGE", page);
	}
#if 0
	ttyprintf("---\n"
	    "url=%s\n"
	    "root=%s\n"
	    "fpath=%s\n",
	    url, root, fpath);
#endif

	return (0);
}

private void
mk_querystr(void)
{
	char	*p;

	if (querystr) free(querystr);
	if (p = hash_toStr(qout)) {
		querystr = malloc(strlen(p) + 2);
		sprintf(querystr, "?%s", p);
		free(p);
	} else {
		querystr = strdup("");
	}
}

private void
http_related(char *page)
{
	int	i;
	char	buf[2048];
	char    dspec[MAXPATH];
	FILE	*f;
	int	count = 0;
	delta	*d;
	sccs	*s = sccs_csetInit(INIT_NOCKSUM|INIT_NOSTAT);

	unless (s) http_error(500, "cannot initialize " CHANGESET);

	hash_storeStr(qout, "PAGE", "cset");
	mk_querystr();

	i = snprintf(dspec, sizeof dspec, "%s<tr bgcolor=white>\n"
			" <td align=right>:HTML_AGE:</td>\n"
			" <td align=center>:USER:</td>\n"
			" <td align=center"
			"$if(:TAG:){ bgcolor=yellow}>"
			"<a href=/%s/%s&REV=:I:>:I:</a>"
			"$if(:TAG:){$each(:TAG:){<br>TAG: (:TAG:)}}"
			"</td>\n"
			" <td>:HTML_C:</td>\n"
			"</tr>\n%s", prefix, root, querystr, suffix);

	if (i == -1)
		http_error(500, "buffer overflow in http_related");

	sprintf(buf, "bk f2csets %s", fpath);
	unless (f = popen(buf, "r"))
		http_error(500, "%s: %s", buf, strerror(errno));

	while (fnext(buf, f)) {
		chop(buf);
		if (d = sccs_findrev(s, buf)) d->flags |= D_SET;
	}
	s->state |= S_SET;
	pclose(f);

	httphdr(".html");
	header(COLOR, "Changesets that modify %s", 0, fpath);

	if (count) {
		puts(OUTER_TABLE INNER_TABLE
		    "<tr bgcolor=#d0d0d0>\n"
		    "<th>Age</th><th>Author</th><th>Rev</th>"
		    "<th align=left>&nbsp;Comments</th></tr>\n");

		sccs_prs(s, 0, 0, dspec, stdout);
		puts(INNER_END OUTER_END);
	}

	trailer();
	if (f) fclose(f);
}

private void
http_tags(char *page)
{
	char	*cmd;
	int	i;
	char	**tags;
	FILE	*f;
	char	buf[MAXLINE];

	httphdr(".html");
	header(COLOR, "Tags", 0);

	puts(OUTER_TABLE INNER_TABLE
	    "<tr bgcolor=#d0d0d0>\n"
	    "  <th>Age</th>\n"
	    "  <th>Tag</th>\n"
	    "  <th>&nbsp;</th>\n"
	    "  <th>&nbsp;</th>\n"
	    "  <th align=left>&nbsp;Comments</th>\n"
	    "</tr>\n");


	cmd = aprintf("bk changes -t -nd'%s"
	    "<tr bgcolor=white>\n"
	    "  <td align=right>:HTML_AGE:</td>\n"
	    "$each(:TAG:){|(:TAG:)}\n"
	    "  <td>:HTML_C:</td>\n"
	    "</tr>%s'", prefix, suffix);
	f = popen(cmd, "r");
	free(cmd);

	while (fnext(buf, f)) {
		if (buf[0] != '|') {
			fputs(buf, stdout);
			continue;
		}
		chomp(buf);
		printf("<td align=center bgcolor=yellow>\n");
		tags = splitLine(buf+1, "|", 0);
		EACH(tags) {
			hash_storeStr(qout, "PAGE", "cset");
			hash_storeStr(qout, "REV", tags[i]);
			mk_querystr();
			printf("<a href=\"%s\">%s</a><br>\n",
			    querystr, tags[i]);
		}
		printf("</a></td>\n");
		sprintf(buf, "..%s", tags[1]);
		hash_storeStr(qout, "PAGE", "changes");
		hash_storeStr(qout, "REV", buf);
		mk_querystr();
		printf("<td><a href=\"%s\">earlier CSets</a></td>\n",
		    querystr);
		sprintf(buf, "%s..", tags[1]);
		hash_storeStr(qout, "REV", buf);
		mk_querystr();
		printf("<td><a href=\"%s\">later CSets</a></td>\n",
		    querystr);
		freeLines(tags, free);
	}
	pclose(f);
	puts(INNER_END OUTER_END);

	trailer();
}

private void
http_search(char *page)
{
	char	*s, *base, *file, *rev, *text;
	int	which, i, first = 1;
	char	*expr;
	FILE	*f;
	char	buf[8<<10];

	expr = hash_fetchStr(qin, "EXPR");
	unless (expr && strlen(expr)) http_error(404, "Search for what?\n");

	s = hash_fetchStr(qin, "SEARCH");
#define	SEARCH_CSET	1
#define	SEARCH_COMMENTS	2
#define	SEARCH_CONTENTS	3
	if (streq(s, "ChangeSet comments")) {
		which = SEARCH_CSET;
	} else if (streq(s, "file comments")) {
		which = SEARCH_COMMENTS;
	} else if (streq(s, "file contents")) {
		which = SEARCH_CONTENTS;
	} else {
		which = 0;
		http_error(404, "Search for how?\n");
	}
	httphdr(".html");
	i = snprintf(buf,
	    sizeof(buf), "Search results for \"%s\" in %s\n", expr, s);
	if (i == -1) {
		header(COLOR, "Search results", 0);
	} else {
		header(COLOR, buf, 0);
	}

	switch (which) {
	    case SEARCH_CSET:
		sprintf(buf, "bk prs -h "
		    "-d'$each(:C:){:GFILE:::I:|(:C:)\n}' ChangeSet");
		break;
	    case SEARCH_CONTENTS:
		sprintf(buf, "bk -Ur grep -i -H -Ar '%s'", expr);
		break;
	    case SEARCH_COMMENTS:
		sprintf(buf, "bk -Ur prs -h "
		    "-d'$each(:C:){:GFILE:::I:|(:C:)\n}'");
		break;
	}
	unless (f = popen(buf, "r")) http_error(404, "grep failed?\n");

	while (fnext(buf, f)) {
		unless (text = strchr(buf, '|')) continue;
		s = text;
		*text++ = 0;
		while (isspace(s[-1])) *--s = 0; /* trail spaces from rev */
		unless (rev = strrchr(buf, ':')) continue;
		*rev++ = 0;
		file = buf;
		unless (base = strrchr(file, '/')) base = file; else base++;
		unless ((which == SEARCH_CONTENTS) || strcasestr(text, expr)) {
			continue;
		}
		if (first) {
			printf("<pre><strong>"
			    "Filename         Rev              Match</strong>"
			    "<hr size=1 noshade>");
			first = 0;
		}
		if (which == SEARCH_CSET) {
			hash_storeStr(qout, "PAGE", "cset");
		} else {
			hash_storeStr(qout, "PAGE", "anno");
		}
		hash_storeStr(qout, "REV", rev);
		mk_querystr();
		printf("<a href=\"%s%s\">%s</a>",
		    (which == SEARCH_CSET) ? "" : file,
		    querystr, base);
		for (i = strlen(base); i <= 16; ++i) putchar(' ');
		if (which == SEARCH_CSET) {
			hash_storeStr(qout, "PAGE", "patch");
		} else {
			hash_storeStr(qout, "PAGE", "diffs");
		}
		mk_querystr();
		printf("<a href=\"%s%s\">%s</a>",
		    (which == SEARCH_CSET) ? "" : file,
		    querystr, rev);
		for (i = strlen(rev); i <= 16; ++i) putchar(' ');
		htmlify(text, strlen(text));
	}
	pclose(f);
	if (first) {
		puts("<p><table align=middle><tr><td>"
		    "No matches found.</td></tr></table><p>");
	} else {
		puts("</pre>");
	}
	trailer();
}

private void
http_robots(void)
{
	httphdr("robots.txt");

	puts("User-agent: *\nDisallow: /");
}

private void
http_repos(char *page)
{
	char	**d;
	char	**repos = 0;
	int	i;
	char	*enc;
	time_t	now = time(0);
	struct	stat	sb;
	char	buf[MAXPATH];

	httphdr(".html");
	header(COLOR, "Repos", 0);

	puts(OUTER_TABLE INNER_TABLE
	    "<thead><tr bgcolor=#d0d0d0>\n"
	    "  <th>&nbsp;</th>\n"
	    "  <th align=left>Repo</th>\n"
	    "  <th>Age</th>\n"
	    "</tr></thead><tbody>");

	d = getdir(fpath);
	EACH(d) {
		concat_path(buf, fpath, d[i]);
		if (lstat(buf, &sb) == -1) continue;
		unless (S_ISDIR(sb.st_mode)) continue;
		concat_path(buf, buf, "BitKeeper/etc");
		if (isdir(buf)) {
			repos = addLine(repos, strdup(d[i]));
			continue;
		}
		enc = str_pullup(0, webencode(0, d[i], strlen(d[i])+1));
		printf("<tr bgcolor=lightblue>\n"
		    "<td><img src=" BKWWW "dir.gif></td>"
		    "<td>&nbsp;<a href=\"%s/%s\">%s</a></td>\n"
		    "<td>&nbsp;</td></tr>",
		    enc, querystr, d[i]);
		free(enc);
	}
	freeLines(d, free);
	EACH (repos) {
		concat_path(buf, fpath, repos[i]);
		concat_path(buf, buf, "SCCS/s.ChangeSet");
		if (lstat(buf, &sb) == -1) continue;
		enc = str_pullup(0, webencode(0, repos[i],strlen(repos[i])+1));
		printf("<tr bgcolor=lightyellow>\n"
		    "<td><img src=" BKWWW "repo.gif></td>"
		    "<td>&nbsp;<a href=\"%s/%s\">%s</a></td>\n"
		    "<td>%s</td></tr>",
		    enc, querystr, repos[i],
		    age(now - sb.st_mtime, "&nbsp;"));
		free(enc);
	}
	printf("</tbody>\n");
	puts(INNER_END OUTER_END);
	freeLines(repos, free);
}

/*
 * recognize urls matching one the the following regexs
 *
 * ROOT(/user=\w+)?/(ChangeSet@REV|cset@REV|patch@REV)
 * ROOT(/user=\w+)?/(hist|anno|diffs)/FILE(@REV)?
 *
 * return a 301 redirect to the new url
 */
private void
detect_oldurls(char *url)
{
	char	*p, *file = 0, *rev;
	char	*user;

	/* kill old navbar */
	unless (p = strstr(url, "?nav=")) return;
	*p = 0;

	if (p = strstr(url, "/ChangeSet@")) {
		hash_storeStr(qout, "PAGE", "changes");
		hash_storeStr(qout, "REV", p+11);
	} else if (p = strstr(url, "/cset@")) {
		hash_storeStr(qout, "PAGE", "cset");
		hash_storeStr(qout, "REV", p+6);
	} else if (p = strstr(url, "/patch@")) {
		hash_storeStr(qout, "PAGE", "patch");
		hash_storeStr(qout, "REV", p+7);
	} else if ((p = strstr(url, "/hist/")) ||
	    (p = strstr(url, "/anno/")) ||
	    (p = strstr(url, "/diffs/"))) {
		file = strchr(p+1, '/');
		*file++ = 0;
		if (streq(p+1, "hist")) {
			hash_storeStr(qout, "PAGE", "prs");
		} else {
			hash_storeStr(qout, "PAGE", p+1);
		}
		if (rev = strchr(file, '@')) {
			*rev++ = 0;
			hash_storeStr(qout, "REV", rev);
		}
	} else {
		http_error(503,
		    "unable to parse URL from old version of BK/Web");
	}

	*p = 0;
	if (user = strstr(url, "/user=")) {
		*user = 0;
		hash_storeStr(qout, "USER", user+6);
	}
	mk_querystr();

	/* redirect */
	printf("HTTP/1.0 301\r\n"
	    "Date: %s\r\n"
	    "Server: bkhttp/%s\r\n"
	    "Location: %s/%s%s\r\n"
	    "\r\n",
	    http_time(), BKWEB_SERVER_VERSION,
	    url,
	    (file ? file : ""),
	    querystr);
	exit(0);
}

private void
http_reserved(char *file)
{
	char	*path;
	FILE	*f;
	int	i;
	char	buf[MAXLINE];

	path = aprintf("%s/www/%s", bin, file);
	unless (f = fopen(path, "r")) http_error(404, "unknown file");
	free(path);
	httphdr(file);

	while ((i = fread(buf, 1, sizeof(buf), f)) > 0) {
		fwrite(buf, 1, i, stdout);
	}
	fclose(f);
}
