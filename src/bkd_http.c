/*
 * Copyright 2000-2016 BitMover, Inc
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

#include "bkd.h"
#include "range.h"
#include "cfg.h"

typedef void (*vfn)(char *page);

private	char	*any2rev(char *any, char *file);
private char	*http_time(void);
private char	*http_expires(int secs);
private	char	*type(char *name);
private void	httphdr(char *file);
private void	http_error(int status, char *fmt, ...)
#ifdef __GNUC__
     __attribute__((format (__printf__, 2, 3)))
#endif
     ;
private void	http_index(char *page);
private	void	http_cat(char *page);
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
private void	http_title(char *desc);
private void	pwd_title(void);
private void	header(char *title, char *header, ...);
private void	printnavbar(char *current_page);
private int	parseurl(char *);
private	void	mk_querystr(void);
private void	trailer(void);
private	void	detect_oldurls(char *url);
private void	flushExit(int status);
private char	*dl_link(void);
private void	show_readme(char *rev);

#define INNER_TABLE	"<table class='table table-bordered table-condensed table-striped sortable'>"
#define	INNER_END	"</table>\n"

#define BKWEB_SERVER_VERSION	"0.6"
#define	BKWWW		"/BitKeeper/www/"
#define BASE_CSS        BKWWW "css/bootstrap.min.css"
#define BK_CSS          BKWWW "css/bk.css"

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
    { http_cat,	    "cat" },
    { http_changes, "changes" },
    { http_cset,    "cset" },
    { http_dir,     "dir" },
    { http_prs,	    "prs" },
    { http_prs,	    "history" },
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

private struct {
	int	secs;
	char	*cmd;
	char	*name;
} dates[] = {
	{ HOUR,    "1h", "hour" },
	{ DAY,     "1d", "day" },
	{ 2*DAY,   "2d", "two days" },
	{ 3*DAY,   "3d", "three days" },
	{ 4*DAY,   "4d", "four days" },
	{ WEEK,    "1w", "week" },
	{ 2*WEEK,  "2w", "two weeks" },
	{ 3*WEEK,  "3w", "three weeks" },
	{ 4*WEEK,  "4w", "four weeks" },
	{ 8*WEEK,  "8w", "eight weeks" },
	{ 12*WEEK, "12w", "twelve weeks" },
	{ 6*MONTH, "6M", "six months" },
	{ 9*MONTH, "9M", "nine months" },
	{ YEAR,    "1y", "year" },
	{ 2*YEAR,  "2y", "two years" },
	{ 3*YEAR,  "3y", "three years" },
	{ 0 }
};

int
cmd_httpget(int ac, char **av)
{
	char	*url;
	char	*p, *page;
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
				header_host = strnonldup(s);
				if ((p = strrchr(s, ':')) && isdigit(p[1])) {
					*p = 0;
				}
				safe_putenv("BK_VHOST=%s", s);
				p = vpath_translate(Opts.vhost_dirpath);
				if (chdir(p)) {
					http_error(404,
					    "Unable to chdir to '%s'", p);
				}
				free(p);
			} else if (buf[0] == '\r' || buf[0] == '\n') {
				break;
			}
			/* ignore everything else */
		}
	}
	unless (av[1]) http_error(404, "get what?\n");
	url = av[1];

	if (streq(url, "/robots.txt")) {
		http_robots();
		flushExit(0);
	}

	qin = hash_new(HASH_MEMHASH);
	qout = hash_new(HASH_MEMHASH);

	detect_oldurls(url);

	/* expand url and extract arguments */
	if (parseurl(url)) http_error(503, "unable to parse URL");

	page = hash_fetchStr(qin, "PAGE");
	for (i = 0; pages[i].content; i++) {
		if (streq(pages[i].page, page)) {
			pages[i].content(page);
			flushExit(0); /* NOT return */
		}
	}
	http_error(403, "illegal page request");
	flushExit(0);		/* NOT return */
	return (0);		/* keep the compiler happy */
}

private void
navbutton(char *title, char *url)
{
	if (!url) {
                printf("<li class='crumb'>%s</li>", title);
	} else {
                printf("<li class='crumb'><a href='%s'>%s</a></li>",url,title);
	}
}

private void
printnavbar(char *current_page)
{
	char	*p, *file = 0;
	int	i;
	char	**items = 0;
	char	buf[MAXLINE];

	/* put in a navigation bar */
        puts("<ol class='breadcrumb'>");

	navbutton("TOP", "/");

	p = buf;
	*p++ = '/';
	if (root && !streq(root, ".")) items = splitLine(root, "/", 0);
	if (!streq(fpath, ".")) {
		items = splitLine(fpath, "/", items);
		if (fpath[strlen(fpath)-1] != '/') file = popLine(items);
	}
	EACH(items) {
		p += sprintf(p, "%s/", items[i]);
		navbutton(items[i], buf);
	}
	freeLines(items, free);
	if (file) {
		navbutton(file, 0);
		free(file);
	}
	if (current_page != NULL) navbutton(current_page, 0);
        puts("</ol>");
}

private void
header(char *titlestr, char *headerstr, ...)
{
	va_list	ptr;
	char	*t;
	char	buf[MAXPATH];
	char	*label = headerstr;

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
			label = buf;
		}
	}
	printf("<script type=\"text/javascript\" src=" BKWWW "sorttable.js>"
	    "</script>\n");
	printf("<script type=\"text/javascript\" src=" BKWWW "markdown.min.js>"
	    "</script>\n");
	puts("<meta name='viewport' content='width=device-width'>");
	puts("<meta name='viewport' content='initial-scale=1.0'>");
        puts("<link rel='stylesheet' href='" BASE_CSS "' />");
        puts("<link rel='stylesheet' href='" BK_CSS "' />");
	puts("</head>");

	puts("<body class='container-fluid'>");
	printnavbar(label);

	if ((t = cfg_str(0, CFG_DESCRIPTION)) && (strlen(t) < 2000)) {
		http_title(t);
	} else {
		pwd_title();
	}
}

private void
httphdr(char *file)
{
	if (getenv("GATEWAY_INTERFACE")) {
		/* looks like a CGI context */
		printf("Status: 200 OK\r\n");
	} else {
		/* we are the web server */
	    	printf("HTTP/1.0 200 OK\r\n");
	}
	printf(
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
	char	*date = hash_fetchStr(qin, "DATE");
	char	*av[100];
	char	buf[2048];

	sprintf(buf, "Changes");
	if (date && (*date == '-') && ends_with(date, "..")) {
		char	match[8];

		strcpy(match, date+1);
		match[strlen(match)-2] = 0;
		for (i = 0; dates[i].secs; i++) {
			if (streq(dates[i].cmd, match)) {
				if (user) {
					sprintf(buf,
					    "Changes by %s in the last %s",
					    user, dates[i].name);
				} else {
					sprintf(buf,
					    "Changes in the last %s",
					    dates[i].name);
				}
				break;
			}
		}
	} else if (user) {
		sprintf(buf, "All changes by %s", user);
	}

	httphdr(".html");
	header("ChangeSet Summaries", buf);

	puts("<div id='changes-view' class='view'>");

	puts(INNER_TABLE
	    "<tr>\n"
    	    "<th class='age'>Age</th>"
	    "<th class='author'>Author</th>"
	    "<th class='rev'>Rev</th>"
	    "<th class='comments'>Comments</th></tr>");

	av[i=0] = "bk";
	av[++i] = "prs";
	av[++i] = "-h";
	if (rev) {
		sprintf(buf, "-r%s", rev);
		av[++i] = buf;
	} else if (date) {
		sprintf(buf, "-c%s", date);
		av[++i] = buf;
	}
	av[++i] = dspec = aprintf("-d%s"
	    "$if(:Li: -gt 0){<tr>\\n"
	    " <td class='age'>:HTML_AGE:</td>\\n"
	    " <td class='author'>:USER:</td>\\n"
	    " <td$if(:TAG:){ class='rev tag'}$else{ class='rev'}>\\n"
	    "|:REV:|:MD5KEY:\\n"
	    "$if(:TAG:){$each(:TAG:){<br>TAG: (:TAG:) }}"
	    "</td>\\n"
	    " <td class='comments'>:HTML_C:</td>\\n"
	    "</tr>\\n"
	    "}%s",
	    prefix, suffix);
	av[++i] = "ChangeSet";
	av[++i] = 0;
	f = popenvp(av, "r");
	free(dspec);

	hash_storeStr(qout, "PAGE", "cset");
	/*
	 * read the prs output and create links on the | lines.
	 * Note: It doesn't matter if :HTML_C: exceeds sizeof(buf) because
	 *       this code still works.
	 */
	while (fnext(buf, f)) {
		char	*p;
		if (buf[0] == '|') {
			chomp(buf);
			p = strrchr(buf+1, '|');
			assert(p);
			hash_storeStr(qout, "REV", p+1);
			mk_querystr();
			*p = 0;
			printf("<a href=\"%s\">%s</a>\n", querystr, buf+1);
		} else {
			fputs(buf, stdout);
		}
	}
	pclose(f);
	puts(INNER_END);
	puts("</div>");
	trailer();
}

private void
http_cset(char *page)
{
	char	*av[100];
	FILE	*f;
	int	i;
	char	*p, *s, *t, *dspec, *md5key;
	char	*rev = hash_fetchStr(qin, "REV");
	int	didhead = 0;
	char	*buf = malloc(2048);

	dspec = aprintf("-d%s"
	    "##:REV:\\n"
	    "<tr><td class='cset-heading'>\\n"
	    "#:MD5KEY:@:GFILE:@:REV:&nbsp;&nbsp;:Dy:-:Dm:-:Dd: :T::TZ:&nbsp;&nbsp;:P:"
	    "$if(:DOMAIN:){@:DOMAIN:}<br>\\n"
	    "</td></tr>\\n"
	    "$if(:TAG:){"
	      "<tr class='tag'><td class='tag'>"
	      "$each(:TAG:){TAG: (:TAG:)<br>}</td>\\n</tr>\\n"
	    "}"
	    "<tr>\\n"
	    "<td class='comments'>:HTML_C:</td></tr>\\n"
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
	free(buf);
	while (buf = fgetline(f)) {
		if (buf[0] != '#') {
			fputs(buf, stdout);
			continue;
		}
		if (buf[1] == '#') {
			if (didhead) continue;
			httphdr("cset.html");
			header("ChangeSet details for %s", buf+2, buf+2);
			puts("<div id='cset-view' class='view'>");
			puts("<table class='table table-bordered"
			    " table-condensed table-striped'>");
			didhead = 1;
			continue;
		}
		md5key = buf+1;
		p = strchr(md5key, '@');
		*p++ = 0;
		fputs(p, stdout);

		t = strchr(p, '@');
		*t++ = 0;
		s = strchr(t, '&');
		*s = 0;
		if (streq(p, "ChangeSet")) {
			hash_storeStr(qout, "PAGE", "patch");
			hash_storeStr(qout, "REV", md5key);
			mk_querystr();
			printf("<a class='function' href='%s'>all diffs</a>",
			    querystr);
		} else {
			hash_storeStr(qout, "PAGE", "history");
			hash_deleteStr(qout, "REV");
			mk_querystr();
			printf("<a class='function' href='%s%s'>history</a>\n",
			    p, querystr);
			hash_storeStr(qout, "PAGE", "anno");
			hash_storeStr(qout, "REV", md5key);
			mk_querystr();
			printf("<a class='function' href='%s%s'>annotate</a>\n",
			    p, querystr);
			hash_storeStr(qout, "PAGE", "diffs");
			mk_querystr();
			printf("<a class='function' href='%s%s'>diffs</a>\n",
			    p, querystr);
		}
	}
	pclose(f);

	fputs(INNER_END, stdout);
	puts("</div>");
	trailer();
}

private void
http_title(char *desc)
{
        puts("<div class='row page-title'>");
        puts("<div class='col-xs-12'>");
	puts(desc);
        puts("</div>");
        puts("</div>");
}

private void
pwd_title(void)
{
	char	pwd[MAXPATH];

	strcpy(pwd, proj_cwd());
	http_title(pwd);
}

private void
trailer(void)
{
	puts("<div class='row footer'>");
	puts("<div class='col-xs-12 text-center'>");
	puts("<hr/>");
	puts("<a href='http://www.bitkeeper.com'>");
	puts("<img src='" BKWWW "trailer.gif'>");
	puts("</div>");
	puts("</div>");
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
	char	*buf;

	hash_storeStr(qout, "PAGE", "diffs");

	httphdr(".html");
	header("Revision history for %s", "History", fpath);

	puts("<div id='prs-view' class='view'>");

	puts(INNER_TABLE
	    "<tr>\n"
	    " <th class='age'>Age</th>\n"
	    " <th class='author'>Author</th>\n"
	    " <th class='rev'>Rev</th>\n"
	    " <th class='comments'>Comments</th>\n"
	    "</tr>");

	av[i=0] = "bk";
	av[++i] = "prs";
	av[++i] = "-hn";
	av[++i] = "-d"
		":MD5KEY:|"				   /* 1 md5key */
	    ":I:|"					   /* 2 rev */
	    "$if(:RENAME:){1}0|"			   /* 3 isrename */
	    ":HTML_AGE:|"				   /* 4 age */
	    ":USER:|"					   /* 5 user */
	    "$if(:TAG:){$each(:TAG:){<br>TAG: (:TAG:)}} |" /* 6 tags */
	    ":HTML_C:";					   /* 7 comments */
	if (rev) av[++i] = revision = aprintf("-r%s", rev);
	av[++i] = fpath;
	av[++i] = 0;

	f = popenvp(av, "r");
	while (buf = fgetline(f)) {
		items = splitLine(buf, "|", 0);
		if (streq(items[2], "1.1")) items[3][0] = ' ';

		hash_storeStr(qout, "REV", items[1]);
		mk_querystr();
		printf("%s<tr>\n"
		    " <td class='age'>%s</td>\n"
		    " <td class='author'>%s</td>\n"
		    " <td class='rev%s%s'>"
		    "<a href='%s'>%s</a>"
		    "%s"
		    "</td>\n <td>%s</td>\n"
		    "</tr>\n%s",
		    prefix, items[4], items[5],
		    ((items[6][0] != ' ') ? " tag tags" : ""),
		    ((items[3][0] == '1') ? " rename" : ""),
		    querystr, items[2], items[6], items[7],
		    suffix);
		freeLines(items, free);
	}
	pclose(f);

	puts(INNER_END);

	puts("</div>");

	trailer();
	if (revision) free(revision);
}


private void
http_dir(char *page)
{
	char	*cmd, *file, *dpn, *fn, *lnkfn, *rev, *enc, *md5key;
	char	*t;
	int	c;
	FILE	*d;
	FILE	*f, *out;
	char	*tmpf;
	FILE	*ftmp;
	char	buf[MAXLINE];

	hash_storeStr(qout, "PAGE", "dir");
	if (rev = hash_fetchStr(qin, "REV")) {
		hash_storeStr(qout, "REV", rev);
	} else {
		rev = "+";
	}
	mk_querystr();
	tmpf = bktmp(0);
	sprintf(buf, "bk rset -S -hl'%s' --dir='%s' --subdirs 2> '%s'",
	    rev, fpath, tmpf);
	d = popen(buf, "r");
	if ((c = fgetc(d)) > 0) {
		/* geting data */
		ungetc(c, d);
	} else {
		/* error */
		pclose(d);
		buf[0] = 0;
		if (f = fopen(tmpf, "r")) {
			if (t = fgetline(f)) strcpy(buf, t);
			fclose(f);
		}
		http_error(500, "%s: %s", fpath, buf[0] ? buf : "");
	}

	httphdr(".html");
	header("Source directory &lt;%s&gt;", "Browse Source",
	    fpath[1] ? fpath : "project root");

	puts("<div id='dir-view' class='view'>");

	puts(INNER_TABLE
	    "<tr>"
	    "<th class='icon'></th>\n"
	    "<th class='filename'>File or folder name</th>\n"
	    "<th class='rev'>Rev</th>\n"
	    "<th class='function'></th>\n"
	    "<th class='age'>Age</th>\n"
	    "<th class='author'>Author</th>\n"
	    "<th class='comments'>Comments</th>\n"
	    "</tr>");

	ftmp = fopen(tmpf, "w");
	while (t = fgetline(d)) {
		if (t[0] == '|') {
			/* display directories */
			++t;
			printf("<tr>"
			    "<td class='icon'><a href='%s/%s'><img src='%s'></a></td>"
			    "<td class='filename'><a href='%s/%s'>%s</a></td>"
			    "<td class='rev'></td>"		/* rev */
			    "<td class='function'></td>"
			    "<td class='age'></td>"		/* age */
			    "<td class='author'></td>"		/* user */
			    "<td class='comments'></td></tr>\n",/* comments */
			    t, querystr,
			    BKWWW "folder_plain.png",
			    t, querystr,
			    t);
		} else {
			char	**items = splitLine(t, "|", 0);

			/* save info for files */
			fprintf(ftmp, "%s|%s\n", items[1], items[3]);
			freeLines(items, free);
		}
	}
	pclose(d);
	fclose(ftmp);

	cmd = aprintf("bk prs -h -d'"
	    "%s<tr>\\n"
	    "|:GFILE:|:DPN:|:REV:|:MD5KEY:\\n"
	    " <td class='age'>:HTML_AGE:</td>"
	    " <td class='author'>:USER:</td>"
	    " <td class='comments'>:HTML_C:</td>"
	    "</tr>\\n%s' - < '%s'",
	    prefix, suffix, tmpf);
	out = fmem();
	f = popen(cmd, "r");
	free(cmd);
	while (fnext(buf, f)) {
		if (buf[0] != '|') {
			fputs(buf, stdout);
			continue;
		}
		chomp(buf);
		file = buf + 1;
		dpn = strchr(file, '|');
		*dpn++ = 0;
		rev = strchr(dpn, '|');
		*rev++ = 0;
		md5key = strchr(rev, '|');
		*md5key++ = 0;
		fn = basenm(dpn);	/* historical file name */
		lnkfn = file;		/* current sfile path */
		if (streq(file, "ChangeSet")) {
			hash_storeStr(qout, "PAGE", "changes");
			hash_storeStr(qout, "REV", md5key);
			mk_querystr();
			printf("<td class='icon'><a href=\"%s\"><img src='"
			    BKWWW "document_delta.png'></a></td>"
			    "<td class='filename'><a href='%s'>%s</a></td>",
			    querystr,
			    querystr, fn);
			hash_storeStr(qout, "PAGE", "cset");
			mk_querystr();
			printf("<td class='rev'>"
			    "<a href='%s'>%s</a></td>"
			    "<td class='function'></td>",
			    querystr, rev);
		} else {
			hash_storeStr(qout, "PAGE", "history");
			hash_deleteStr(qout, "REV");
			mk_querystr();
			ftrunc(out, 0);
			webencode(out, root, strlen(root)+1);
			fputc('/', out);
			webencode(out, lnkfn, strlen(lnkfn)+1);
			enc = fmem_peek(out, 0);
			printf("<td class='icon'><a href='/%s%s'><img src='"
			    BKWWW "document_delta.png'></a></td>",
			    enc, querystr);
			hash_storeStr(qout, "PAGE", "anno");
			hash_storeStr(qout, "REV", md5key);
			mk_querystr();
			printf("<td class='filename'>"
			    "<a href='/%s%s'>%s</a></td>",
			    enc, querystr, fn);
			hash_storeStr(qout, "PAGE", "diffs");
			hash_storeStr(qout, "REV", md5key);
			mk_querystr();
			printf("<td class='rev'>"
			    "<a href='/%s%s'>%s</a>"
			    "</td>",
			    enc, querystr, rev);
			hash_storeStr(qout, "PAGE", "related");
			hash_deleteStr(qout, "REV");
			mk_querystr();
			printf("<td class='function'>"
			    "<a href='/%s%s'>csets</a></td>",
			    enc, querystr);
		}
	}
	pclose(f);
	fclose(out);
	unlink(tmpf);
	free(tmpf);

	puts(INNER_END);

	show_readme(rev);

	puts("</div>");

	trailer();
}


private void
http_anno(char *page)
{
	FILE	*f;
	char	*t;
	char	buf[4096];
	int	empty = 1;
	char	*rev = hash_fetchStr(qin, "REV");
	char	*freeme;

	httphdr(".html");
	/*
	 * Second param is title, third is header, both use same
	 * varargs list, so use hack of %.0s to have it skip string
	 */
	header("Annotated listing of %s%.0s", 
	    "%.0s%s &lt;annotated&gt;", fpath, freeme = dl_link());
	free(freeme);

	puts("<div id='anno-view' class='view'>");

	printf("<pre class='code annotated'>");
	unless (rev) rev = "+";
	sprintf(buf, "bk annotate -Aru -r'%s' '%s'", rev, fpath);

	/*
	 * Do not show the license key in config file
	 */
	if (streq(fpath, "BitKeeper/etc/config")) {
		strcat(buf,
		    " | sed -e's/| license:.*$/| license: XXXXXXXXXXXXX/'");
	}
	f = popen(buf, "r");
	while (t = fgetline(f)) {
		empty = 0;
		htmlify(t, strlen(t));
		putchar('\n');
	}
	pclose(f);
	if (empty) puts("\nEmpty file");
	puts("</pre>");

	puts("</div>");

	trailer();
}

private char *
dl_link(void)
{
	FILE	*f = fmem();
	char	*base = basenm(fpath);

	fputs("<a title='View file contents' href=\"/", f);
	webencode(f, root, strlen(root)+1);
	putc('/', f);
	webencode(f, fpath, strlen(fpath)+1);
	fputs("?PAGE=cat", f);
	if (hash_fetchStr(qin, "REV")) {
		fputs("&REV=", f);
		webencode(f, qin->vptr, qin->vlen);
	}
	fputs("\">", f);
	webencode(f, base, strlen(base)+1);
	fputs("</a>", f);
	return (fmem_close(f, 0));
}

#define BLACK 1
#define	OLD 2
#define	NEW 3
private void
color(char c)
{
	static	int clr;

	unless (c) {
		fputs("<div class='same'>", stdout);
		clr = 0;
		return;
	}
	if (c == '-') {
		if (clr != OLD) {
			fputs("</div><div class='old'>", stdout);
			clr = OLD;
		}
	} else if (c == '+') {
		if (clr != NEW) {
			fputs("</div><div class='new'>", stdout);
			clr = NEW;
		}
	} else {
		if (clr != BLACK) {
			fputs("</div><div class='same'>", stdout);
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
	char	*freeme;

	httphdr(".html");
	/*
	 * Second param is title, third is header, both use same
	 * varargs list, so use hack of %.0s to have it skip string
	 */
	header("Changes for %s%.0s", 
	    "%.0s%s &lt;changes&gt;", fpath, freeme = dl_link());
	free(freeme);

	puts("<div id='diffs-view' class='view'>");

	hash_storeStr(qout, "PAGE", "anno");
	mk_querystr();

	i = snprintf(dspec, sizeof dspec,
		"-d%s<tr>\\n"
		" <td class='age'>:HTML_AGE:</td>\\n"
		" <td class='author'>:USER:$if(:DOMAIN:){@:DOMAIN:}</td>\\n"
		" <td class='rev'><a href=\"%s&REV=:MD5KEY:\">:I:</a></td>\\n"
		" <td class='comments'>:HTML_C:</td>\\n"
		"</tr>\\n%s", prefix, querystr, suffix);

	if (i == -1) {
		http_error(500, "buffer overflow (#1) in http_diffs");
	}

	if (snprintf(argrev, sizeof argrev, "-r%s", rev) == -1) {
		http_error(500, "buffer overflow (#2) in http_diffs");
	}

	puts(INNER_TABLE
	    "<tr>\n"
	    " <th class='age'>Age</th>\n"
	    " <th class='author'>Author</th>\n"
	    " <th class='rev'>Annotate</th>\n"
	    " <th class='comments'>Comments</th>\n"
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

	puts(INNER_END);

	if (strstr(rev, "..")) {
		sprintf(buf, "bk diffs -ur'%s' '%s'",
		    any2rev(rev, fpath), fpath);
	} else {
		sprintf(buf, "bk diffs -uR'%s' '%s'",
		    any2rev(rev, fpath), fpath);
	}
	f = popen(buf, "r");
	printf("<pre class='code diffs'>");
	color(0);
	fnext(buf, f);
	while (fnext(buf, f)) {
		color(buf[0]);
		htmlify(buf, strlen(buf));
	}
	pclose(f);
	printf("</div>"); /* Close the opening <div> from color() */
	printf("</pre>");

	puts("</div>");

	trailer();
}

private void
http_patch(char *page)
{
	FILE	*f;
	char	*rev = hash_fetchStr(qin, "REV");
	char	buf[4096];
	char	*s;

	httphdr(".html");
	s = any2rev(rev, "ChangeSet");
	header("All diffs for ChangeSet %s", "%s changes",
	    s, querystr, rev);
	free(s);

	puts("<div id='patch-view' class='view'>");

	sprintf(buf, "bk changes -vvr'%s'", rev);
	f = popen(buf, "r");
	while (fgets(buf, sizeof(buf), f)) {
		color(buf[0]);
		if ((buf[0] == '#')) {
			puts("</pre>");
			puts("</div>");
			puts("<div class='panel panel-default'>");
			puts("<div class='panel-heading cset-heading'>");
			puts("<h3 class='panel-title'>");
			printf("<div class='file-heading'>");
		}
		if ((buf[0] == '=')) {
			puts("</pre>");
			puts("</div>");
			puts("<div class='panel panel-default'>");
			puts("<div class='panel-heading cset-heading'>");
			puts("<h3 class='panel-title'>");
		}
		htmlify(buf, strlen(buf));
		if ((buf[0] == '=') || (buf[0] == '#')) {
			puts("</h3>");
			puts("</div>");
			puts("<pre class='code diffs patch'>");
			color(0);
		}
	}
	pclose(f);
	printf("</div>"); /* Close the opening <div> from color() */
	printf("</pre>");

	puts("</div>");

	trailer();
}

private void
http_stats(char *page)
{
	int	recent_cs, all_cs;
	char	c_user[80];
	int	ct;
	char	units[200];
	char	buf[200];
	FILE	*p;

	unless (p = popen("bk prs -h -d'$if(:Li: -gt 0){:USER: :AGE:\\n}' ChangeSet | bk sort", "r"))
		http_error(500, "bk prs failed: %s", strerror(errno));

	c_user[0] = 0;
	recent_cs = all_cs = 0;

	httphdr(".html");
	header("User statistics", "User Statistics");

	puts("<div id='stats-view' class='view'>");

	puts(INNER_TABLE
	    "<tr>\n"
	    "<th class='author'>Author</th>\n"
	    "<th class='recent-csets'>Recent changesets</th>\n"
	    "<th class='all-csets'>All changesets</th></tr>");

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
				    "<tr>\n"
				    "<td class='author'>"
				    "<a href=\"%s\">%s</a></td>\n"
				    "<td class='recent-csets'>%d</td>\n"
				    "<td class='all-csets'>%d</td></tr>\n",
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
		    "<tr>\n"
		    "<td class='author'>"
		    "<a href=\"%s\">%s</a></td>\n"
		    "<td class='recent-csets'>%d</td>\n"
		    "<td class='all-csets'>%d</td></tr>\n",
		    querystr, c_user,
		    recent_cs, all_cs);
	}
	puts(INNER_END);

	puts("</div>");

	trailer();
}


private void
http_index(char *page)
{
	sccs	*s;
	ser_t	d;
	time_t	now;
	int	i;
	int	c=0, cm=0;
	//char	*email=0;
	char	*desc=0;
	//char	*contact=0;
	//char	*category=0;
	char	*bkweb=0, *master=0, *homepage=0;
	int	cnt[20] = {0};

	unless (s  = sccs_csetInit(INIT_NOCKSUM|INIT_NOSTAT|INIT_MUSTEXIST)) {
		http_error(404, "Unable to find ChangeSet file");
		return;
	}
	time(&now);
	for (d = TABLE(s); d >= TREE(s); d--) {
		if (user && !streq(user, USER(s, d))) continue;
		if (TAG(s, d)) continue;
		unless (ADDED(s, d) > 0) {
			unless (d == TREE(s)) cm++;
			continue;
		}
		for (i = 0; dates[i].secs; i++) {
			assert(i < sizeof(cnt)/sizeof(cnt[0]));
			if (NOFUDGE(s, d) >= now - dates[i].secs) cnt[i]++;
		}
		c++;
	}
	sccs_free(s);

	desc	 = cfg_str(0, CFG_DESCRIPTION);
	//contact  = cfg_str(0, CFG_CONTACT);
	//email	 = cfg_str(0, CFG_EMAIL);
	//category = cfg_str(0, CFG_CATEGORY);
	bkweb	 = cfg_str(0, CFG_BKWEB);
	master	 = cfg_str(0, CFG_MASTER);
	homepage = cfg_str(0, CFG_HOMEPAGE);

	httphdr(".html");
	/* don't use header() here; this is one place where the regular
	 * header.txt is not needed
	 */
	puts("<html><head><title>");
	puts(desc ? desc : "ChangeSet activity");
	puts("</title>\n");
	puts("<meta name='viewport' content='width=device-width'>");
	puts("<meta name='viewport' content='initial-scale=1.0'>");
        puts("<link rel='stylesheet' href='" BASE_CSS "' />");
        puts("<link rel='stylesheet' href='" BK_CSS "' />");
	printf("<script type=\"text/javascript\" src=" BKWWW "markdown.min.js>"
	    "</script>\n");
        puts("</head>\n");
        puts("<body class='container-fluid'>");
	printnavbar(user);

	puts("<div id='index-view' class='view'>");

	puts("<div class='row'>");
	puts("<div class='col-xs-12 col-sm-5 col-md-4 col-lg-4'>");
	puts("<label>ChangeSet History</label>");
	hash_storeStr(qout, "PAGE", "changes");
	for (i = 0; dates[i].secs; i++) {
		char	t[10];
		if (!cnt[i] || (i && (cnt[i] == cnt[i-1]))) continue;
		sprintf(t, "-%s..", dates[i].cmd);
		hash_storeStr(qout, "DATE", t);
		mk_querystr();
		printf("<div><a href=%s>", querystr);
		printf(
		    "%d&nbsp;ChangeSets&nbsp;in&nbsp;the&nbsp;last&nbsp;%s</a>",
		    cnt[i], dates[i].name);
		printf("</div>\n");
	}
	hash_deleteStr(qout, "DATE");
	mk_querystr();
	printf("<div><a href=%s>", querystr);
	if (cm) {
		printf("All %d changesets (%d empty merges)", c+cm, cm);
	} else {
		printf("All %d changesets", c+cm);
	}
	puts("</a></div>");
	puts("</div>");

	puts("<div class='col-xs-12 col-sm-7 col-md-8 col-lg-8'>");
	puts("<label>Search History</label>");

	puts("<div>");
	hash_storeStr(qout, "PAGE", "tags");
	mk_querystr();
	printf("<a href=%s>Tags</a></div>", querystr);

	unless (user) {
		puts("<div>");
		hash_storeStr(qout, "PAGE", "stats");
		mk_querystr();
		printf("<a href='%s'>User statistics</a>", querystr);
		puts("</div>");
	}

	puts("<div>");
	hash_storeStr(qout, "PAGE", "dir");
	mk_querystr();
	printf("<a href=%s>Browse the source tree</a></div>", querystr);

	if (bkweb && !exists(WEBMASTER)) {
		puts("<div>");
		printf("<a href=\"%s\">BK/Web site for this package</a>"
		    "</div>\n", bkweb);
	}

	if (homepage) {
		puts("<div>");
		printf("<a href=\"%s\">"
		    "Home page for this package</a></div>\n",
		    homepage);
	}

	if (master) {
		puts("<div>");
		printf("Master repository at %s</div>\n", master);
	}

	puts("<div class='search-form'>"
	    "<form class='form-inline' method='GET'>\n"
	    "<input type='hidden' name='PAGE' value='search'>\n");

	puts("<div class='form-group'>");
	puts("<div class='input-group'>");
	puts("<div class='input-group-addon'>");
	puts("<span class='glyphicon glyphicon-search'></span>");
	puts("</div>");
	puts("<input type='search' name='EXPR' class='form-control'>");
	puts("</div>");
	puts("</div>");

	puts("<button type='submit' class='btn btn-default'>Search</button>");
	puts("<br/>");

	puts("<label class='radio-inline'>");
	puts("<input type='radio' name='SEARCH' value='ChangeSet comments'"
		" checked>ChangeSet comments</label>");
	puts("<label class='radio-inline'>");
	puts("<input type='radio' name='SEARCH' value='file comments'>"
		"File comments</label>");
	puts("<label class='radio-inline'>");
	puts("<input type='radio' name='SEARCH' value='file contents'>"
		"File contents</label>");

	puts("</form>");
	puts("</div>");
	puts("</div>");
	puts("</div>");

	puts("<br/>");
	show_readme("+");

	puts("</div>");

	trailer();
}

private int
format_readme(char *readme, int markdown, char *rev)
{
	FILE	*f;
	int	c;
	char	buf[MAXLINE];

	sprintf(buf, "bk get -kqpr'%s' '%s/%s'", rev, fpath, readme);
	unless (f = popen(buf, "r")) return (-1);
	if ((c = fgetc(f)) <= 0) {
		pclose(f);
		return (-1);
	}
	ungetc(c, f);

	puts("<div class='row'>");
	puts("<div class='col-xs-12'>");
	puts("<div class='panel panel-default'>");
	printf("<div class='panel-heading'>"
	    "<span class='glyphicon glyphicon-file'></span>"
	    " %s</div>", readme);
	if (markdown) {
		printf("<div id='markdown' class='panel-body'>");
	} else {
		printf("<div id='readme' class='panel-body'>");
		while ((c = fgetc(f)) > 0) {
			switch (c) {
			    case '<': fputs("&lt;", stdout); break;
			    case '>': fputs("&gt;", stdout); break;
			    case '&': fputs("&amp;", stdout); break;
			    case '"': fputs("&quot;", stdout); break;
			    case '\'': fputs("&#39;", stdout); break;
			    default: putc(c, stdout); break;
			}
		}
	}
	puts("</div>");
	puts("</div>");
	puts("</div>");
	puts("</div>");
	if (markdown) {
		puts("<script>");
		fputs("var data = \"", stdout);
		while ((c = fgetc(f)) > 0) {
			switch (c) {
			    case '"': fputs("\\\"", stdout); break;
			    case '\\': fputs("\\\\", stdout); break;
			    case '\n': fputs("\\n\\\n", stdout); break;
			    case '\r': fputs("\\r", stdout); break;
			    case '\t': fputs("\\t", stdout); break;
			    default: putc(c, stdout); break;
			}
		}
		puts("\";");
		puts("var md = document.getElementById('markdown');");
		puts("md.innerHTML = markdown.toHTML(data);");
		puts("</script>");
	}
	pclose(f);
	return (0);
}

private void
show_readme(char *rev)
{
	// stop on the first version that succeeds
	unless (format_readme("README.md", 1, rev)) return;
	unless (format_readme("README.txt", 0, rev)) return;
	unless (format_readme("README", 0, rev)) return;
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
		{"css", "text/css" },
		{"gif", "image/gif" },
		{"html", "text/html" },
		{"htm", "text/html" },
		{"jpeg", "image/jpeg" },
		{"js", "text/javascript" },
		{"png", "image/png" },
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

	if (getenv("GATEWAY_INTERFACE")) {
		/* looks like a CGI context */
		printf("Status: %d Error\r\n", status);
	} else {
		printf("HTTP/1.0 %d Error\r\n", status);
	}
	printf(
	    "Date: %s\r\n"
	    "Server: bkhttp/%s\r\n"
	    "Content-Type: text/html\r\n"
	    "\r\n",
	    http_time(), BKWEB_SERVER_VERSION);

	puts("<html><head><title>Error!</title></head>\n"
	    "<body alink=black link=black bgcolor=white>");

	size = sizeof(buf);
	va_start(ptr,fmt);
	ct = vsnprintf(buf, size, fmt, ptr);
	va_end(ptr);

	if (ct == -1) {
		/* vsnprintf hit a problem other than truncation */
		strcpy(buf, "unknown error");
	}
	puts("<center>\n"
            "<table>\n"
	    "<tr bgcolor=red fgcolor=white>\n"
	    "  <td align=center>");
	printf("<br>\n<h2>Error %d</h2>\n", status);
	puts(buf);
	puts("  </td>\n"
	    "</tr>\n"
	    "</table>\n"
            "</center>");

	puts("<hr>\n"
	    "<table width=100%>\n"
	    "<tr>\n"
	    "  <th valign=top align=left>bkhttp/" BKWEB_SERVER_VERSION " server");
	puts("  </th>\n"
	    "  <td align=right><a href=http://www.bitkeeper.com>"
	    "<img src='" BKWWW "trailer.gif' alt='Learn more about BitKeeper'>"
	    "  </a>"
	    "  </td>\n"
	    "</tr>\n"
	    "</table>");
	puts("</body></html>");
	flushExit(1);
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

	unless (bkd_isSafe(url)) {
		http_error(403,
		    "BK/Web cannot access data outside of "
		    "the directory where the bkd was started.");
	}
	if (*url && (url[strlen(url)-1] != '/') && dir) {
		/* redirect with trailing / */
		printf("HTTP/1.0 301\r\n"
		    "Date: %s\r\n"
		    "Server: bkhttp/%s\r\n"
		    "Location: /%s/\r\n"
		    "\r\n",
		    http_time(), BKWEB_SERVER_VERSION,
		    url);
		flushExit(0);
	}

	if (p = proj_init(url)) {
		cwd = proj_cwd();
		i = strlen(cwd);
		proot = proj_root(p);

		if (streq(proot, cwd)) {
			root = strdup(".");
		} else if ((i == 1) && streq(cwd, "/")) {
			root = strdup(proot);
		} else {
			unless (strneq(proot, cwd, i) && (proot[i] == '/')) {
				unless (Opts.symlink_ok) {
err:					http_error(503,
					    "Can't find project root");
				}
				page = fullLink(url, 0, 0);
				unless ((strneq(page, cwd, i)
				    && (page[i] == '/'))) {
					free(page);
					goto err;
				}
				free(page);
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
				page = dir ? "dir" : "cat";
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
	char    dspec[MAXPATH];

	hash_storeStr(qout, "PAGE", "cset");
	mk_querystr();

	i = snprintf(dspec, sizeof dspec, "%s<tr>\\n"
			" <td tag=\"age\">:HTML_AGE:</td>\\n"
			" <td tag=\"author\">:USER:</td>\\n"
			" <td"
			"$if(:TAG:){ class=\"rev tag\"}$else{ class=\"rev\"}>"
			"<a href=/%s/%s&REV=:MD5KEY:>:I:</a>"
			"$if(:TAG:){$each(:TAG:){<br>TAG: (:TAG:)}}"
			"</td>\\n"
			" <td class=\"comments\">:HTML_C:</td>\\n"
			"</tr>\\n%s", prefix, root, querystr, suffix);

	if (i == -1)
		http_error(500, "buffer overflow in http_related");

	httphdr(".html");
	header("ChangeSets that modify %s", 0, fpath);

	puts("<div id='related-view' class='view'>");

	puts(INNER_TABLE
	    "<tr>\n"
	    "<th class='age'>Age</th>"
	    "<th class='author'>Author</th>"
	    "<th class='rev'>Rev</th>"
	    "<th class='comments'>Comments</th></tr>\n");
	fflush(stdout);
	systemf("bk changes -i'%s' -nd'%s'", fpath, dspec);
	puts(INNER_END);

	puts("</div>");

	trailer();
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
	header("Tags", "Tags");

	puts("<div id='tags-view' class='view'>");

	puts(INNER_TABLE
	    "<tr>\n"
	    "  <th class='age'>Age</th>\n"
	    "  <th class='tag'>Tag</th>\n"
	    "  <th class='function'></th>\n"
	    "  <th class='function'></th>\n"
	    "  <th class='comments'>Comments</th>\n"
	    "</tr>\n");


	cmd = aprintf("bk changes -t -nd'%s"
	    "<tr>\\n"
	    "  <td class='age'>:HTML_AGE:</td>\\n"
	    "$each(:TAG:){|(:TAG:)}\\n"
	    "  <td class='comments'>:HTML_C:</td>\\n"
	    "</tr>%s'", prefix, suffix);
	f = popen(cmd, "r");
	free(cmd);

	while (fnext(buf, f)) {
		if (buf[0] != '|') {
			fputs(buf, stdout);
			continue;
		}
		chomp(buf);
		printf("<td class='tag tags'>\n");
		tags = splitLine(buf+1, "|", 0);
		EACH(tags) {
			hash_storeStr(qout, "PAGE", "cset");
			hash_storeStr(qout, "REV", tags[i]);
			mk_querystr();
			printf("<a href=\"%s\">%s</a><br>\n",
			    querystr, tags[i]);
		}
		printf("</td>\n");
		sprintf(buf, "..%s", tags[1]);
		hash_storeStr(qout, "PAGE", "changes");
		hash_storeStr(qout, "REV", buf);
		mk_querystr();
		printf("<td class='function'>"
		    "<a href='%s'>earlier csets</a></td>\n", querystr);
		sprintf(buf, "%s..", tags[1]);
		hash_storeStr(qout, "REV", buf);
		mk_querystr();
		printf("<td class='function'>"
		    "<a href='%s'>later csets</a></td>\n", querystr);
		freeLines(tags, free);
	}
	pclose(f);
	puts(INNER_END);

	puts("</div>");

	trailer();
}

private void
http_search(char *page)
{
	char	*s, *base, *file, *rev, *text, *md5key;
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
		header("Search Results", "Search Results");
	} else {
		header(buf, "Search Results");
	}

	puts("<div id='search-view' class='view'>");

	switch (which) {
	    case SEARCH_CSET:
		sprintf(buf, "bk prs -h "
		    "-d'$each(:C:){:GFILE:::I:::MD5KEY:|(:C:)\\n}' ChangeSet");
		break;
	    case SEARCH_CONTENTS:
		sprintf(buf, "bk -Ur grep -i -H -Ar '%s'", expr);
		break;
	    case SEARCH_COMMENTS:
		sprintf(buf, "bk -Ur prs -h "
		    "-d'$each(:C:){:GFILE:::I:::MD5KEY:|(:C:)\\n}'");
		break;
	}
	unless (f = popen(buf, "r")) http_error(404, "grep failed?\n");

	while (fnext(buf, f)) {
		unless (text = strchr(buf, '|')) continue;
		s = text;
		*text++ = 0;
		while (isspace(s[-1])) *--s = 0; /* trail spaces from rev */
		unless (md5key = strrchr(buf, ':')) continue;
		*md5key++ = 0;
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
		hash_storeStr(qout, "REV", md5key);
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

	puts("</div>");

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
	int	i;
	char	*enc;
	time_t	now = time(0);
	struct	stat	sb;
	FILE	*f = fmem();
	char	buf[MAXPATH];
	char	buf2[MAXPATH];

	if (chdir(fpath)) http_error(404, "unknown directory");

	httphdr(".html");
	header("Repos", "Repos");

	puts("<div id='repos-view' class='view'>");

	puts(INNER_TABLE
	    "<tr>\n"
	    "  <th class='icon'></th>\n"
	    "  <th>Repository or Folder</th>\n"
	    "  <th data-sort='date'>Age</th>\n"
	    "</tr>");

	d = getdir(".");
	EACH(d) {
		if (d[i][0] == '.') continue; /* skip DOT directories */
		strcpy(buf, d[i]);
		if (lstat(buf, &sb) == -1) continue;
		unless (S_ISDIR(sb.st_mode)) continue;
		concat_path(buf2, buf, "BitKeeper/etc");
		ftrunc(f, 0);
		webencode(f, d[i], strlen(d[i])+1);
		enc = fmem_peek(f, 0);
		if (isdir(buf2)) {
			concat_path(buf, buf, "SCCS/s.ChangeSet");
			if (lstat(buf, &sb) == -1) continue;
			printf("<tr class='repo'>\n"
			    "<td class='icon'><a href='%s/%s'>"
			    "<img src='" BKWWW "folder_delta.png'></a></td>"
			    "<td><a href='%s/%s'>%s</a></td>\n"
			    "<td>%s</td></tr>",
			    enc, querystr,
			    enc, querystr, d[i],
			    age(now - sb.st_mtime, "&nbsp;"));
		} else {
			printf("<tr class='directory'>"
			    "<td class='icon'><a href='%s/%s'>"
			    "<img src='" BKWWW "folder_plain.png'></a></td>"
			    "<td><a href='%s/%s'>%s</a></td>\n"
			    "<td>%s</td></tr>",
			    enc, querystr,
			    enc, querystr, d[i],
			    age(now - sb.st_mtime, "&nbsp;"));
		}
	}
	fclose(f);
	freeLines(d, free);
	puts(INNER_END);

	puts("</div>");

	trailer();
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
			hash_storeStr(qout, "PAGE", "history");
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
	flushExit(0);
}

private void
http_cat(char *page)
{
	FILE	*f;
	int	i;
	char	*cmd, *file;
	int	hdr = 0;
	char	buf[MAXLINE];
	char	*rev = hash_fetchStr(qin, "REV");

	/* handle reserved namespace */
	// XXX search both dirs? which order?
	if (strneq(fpath, "BitKeeper/www/", 14)) {
		file = aprintf("%s/www/%s", bin, fpath + 14);
	} else {
		file = fpath;
	}
	if (rev) {
		cmd = aprintf("bk get -qkpr'%s' '%s'", rev, file);
	} else {
		cmd = aprintf("bk cat '%s'", file);
	}
	f = popen(cmd, "r");
	free(cmd);
	unless (f) http_error(404, "unknown file");

	while ((i = fread(buf, 1, sizeof(buf), f)) > 0) {
		unless (hdr) {
			httphdr(file);
			hdr = 1;
		}
		fwrite(buf, 1, i, stdout);
	}
	if (pclose(f) && !hdr) http_error(404, "unknown file");
	unless (hdr) httphdr(file); /* empty file */
}

private void
flushExit(int status)
{
	fflush(stdout);
	/*
	 * Without an explicit shutdown, bkd on windows sometimes
	 * doesn't flush pending data and do a graceful shutdown.
	 */
	shutdown(fileno(stdout), 1);
	exit(status);
}

private char *
any2rev(char *any, char *file)
{
	sccs	*s;
	ser_t	d;
	char	*buf;
	char	*sfile;

	if (strchr(any, '.')) return (strdup(any));
	sfile = name2sccs(file);
	unless (s = sccs_init(sfile, INIT_NOCKSUM)) return (strdup(any));
	unless (d = sccs_findMD5(s, any)) {
		unless (d = sccs_findrev(s, any)) {
			sccs_free(s);
			free(sfile);
			return (strdup(any));
		}
	}
	buf = strdup(REV(s, d));
	sccs_free(s);
	free(sfile);
	return (buf);
}
