/*
 * Process the help pages to make the html pages.
 * Then process the summary pages to make indices.
 * Then make an index.html and put all the stuff in www/
 */
FILE	index;
string	first[] = { "All", "Overview", "Common" };
string	toplevels[] = { "All", "Overview", "Common", "Admin", "Compat",
			"File", "GUI-tools", "Nested", "Repository",
			"Utility" };
string	references[];
string	topics{string}[];
string	sections{string}[];

int
main(string av[])
{
	string	file, buf;
	int	i, skip;

	index = fopen("www/index.html", "w");
	fprintf(index, "<html>\n" .
	    "<body bgcolor=white>\n");
	for (buf = "", i = 0; i < 20; i++) buf .= "&nbsp;";
	fprintf(index, "<table>\n");
	fprintf(index, "<table><tr>" .
	    "<td style='width:6em;'><a href=index.html border=0>" .
	    "<img src=BitKeeper_SN_Blue.png width=50>" .
	    "</a></td>" .
	    "<td><b>BitKeeper Documentation</b></td></tr>\n",
	    buf, buf);

	build_nav_cache();

	foreach (file in getdir("../man2help", "*.fmt")) {
		html(file);
	}

	// Wayne wants these first, which seems reasonable
	foreach (file in first) {
		html("../man2help/" . file . ".done");
	}

	foreach (file in getdir("../man2help", "*.done")) {
		skip = 0;
		foreach (buf in first) {
			if (file == "../man2help/" . buf . ".done") {
				skip = 1;
			}
		}
		unless (skip) html(file);
	}

	fprintf(index, "</table>\n" .
	    "<p align=center><img src=BitKeeper_SN_SVC_Blue.png width=150>" .
	    "</p></body>\n</html>\n");
	fclose(index);

	i = 1;
	foreach (buf in references) {
		unless (exists("www/" . buf)) {
			if (i) {
				warn("\nWARNING some links are bad:\n");
				i = 0;
			}
			warn("%s\n", buf);
		}
	}
	return (0);
}

// Walk the toplevel sections and build up a list of topics in each one
// and then note which toplevel section each topic belongs to and keep
// track of that too.
void
build_nav_cache()
{
	FILE	fp;
	string	toplevel, file, line, topic;

	foreach (toplevel in toplevels) {
		file = "../man2help/${toplevel}.done";
		fp = fopen(file, "r");

		while (line = <fp>) {
			unless (line =~ /\s+bk(.*?) -/) continue;
			topic = trim($1);
			unless (length(topic)) topic = "bk";

			push(&topics{topic}, toplevel);
			push(&sections{toplevel}, topic);
		}
		fclose(fp);
	}
}

void
html(string file)
{
	FILE	in, out, body;
	string	buf, p, desc, topic, style;
	int	done = 0, see_also = 0;

	in = fopen("style.css", "r");
	read(in, &style);
	fclose(in);

	in = fopen(file, "r");
	assert(in);
	topic = basename(file);
	topic =~ s/^bk-//;
	topic =~ s/-1.fmt$//;
	if (topic =~ /\.done$/) {
		topic =~ s/.done$//;
		done = 1;
		desc = `cat ../man2help/${topic}.description`;
		desc =~ s/^.SH DESCRIPTION\n//;
		desc =~ s/\\\*\(BK/BitKeeper/;
	}
	buf = topic . ".html";
	out = fopen("www/" . "${topic}.html", "w+");
	body = fopen("www/" . "${topic}_body.html", "w+");
	assert(out);
	assert(body);
	warn("Htmlify %s\n", buf);
	buf =~ s/\.html//;
	if (done) {
		fprintf(index, "<tr><td>\n");
		fprintf(index, "<a href=\"%s.html\">%s</a><br>\n", buf, buf);
		fprintf(index, "</td><td>\n");
		fprintf(index, "%s\n\n", desc);
		fprintf(index, "</td></tr>\n");
	}
	buf =
	    <<EOF
	    <!DOCTYPE html>
	    <html>
	    <head>
	    <title>${topic} | BitKeeper Documentation</title>
	    <style>${style}</style>
	    </head>
	    <body>
	    EOF;

	puts(out, buf);
	nav(out, topic);

	puts(out, "<div id='content'>");
	puts(out, "<pre>");
	puts(body, "<pre>");

	while (buf = <in>) {
input:
		buf =~ s/</\&lt;/g;
		buf =~ s/>/\&gt;/g;
		switch (buf) {
		    case /^\$$/:
			break;  // skip
		    case m|^help://|:
			break;  // skip
		    case /(.*)(BitKeeper\s+User's\s+Manual)(.*)/:
			fprintf(out, "<b>%s</b>\n\n", buf);
			fprintf(body, "<b>%s</b>\n\n", buf);
			while (buf = <in>) {
				if (buf !~ /^\s*$/) goto input;
			}
			break;
		    case /^[a-zA-Z]+.*/:
			fprintf(out, "<strong>%s</strong>\n", buf);
			fprintf(body, "<strong>%s</strong>\n", buf);
			break;
		    case /bk help ([a-zA-Z0-9]+)/:
			if (see_also) {
			    for ( ;; ) {
				    p = <in>;
				    if (p =~ /^\s*$/) break;
				    buf .= " " . p;
			    }
			    page(out, buf);
			    page(body, buf);
			}
			break;
		    case /^  bk [a-zA-Z0-9\-]+ - /:
			section(out, buf);
			section(body, buf);
			break;
		    case /^  bk - /:
			section(out, buf);
			section(body, buf);
			break;
		    default:
			fprintf(out, "%s\n", buf);
			fprintf(body, "%s\n", buf);
		}
		if (buf =~ /SEE ALSO/) see_also++;
		if (buf =~ /^CATEGORY/) {
			while (buf = <in>) {
				if (buf =~ /^\s*$/) break;
				category(out, buf);
				category(body, buf);
			}
			fprintf(out, "\n");
			fprintf(body, "\n");
		}
	}
	fclose(in);

	puts(body, "</pre>");
	fclose(body);

	in = fopen("manpages.js", "r");
	read(in, &buf);
	fclose(in);

	puts(out, "</pre>");
	puts(out, buf);

	puts(out, "</div></body>\n</html>");
	fclose(out);
}

void
nav(FILE out, string topic)
{
	string	section;
	string	items[];

	if (lsearch(exact: toplevels, topic) > -1) {
		section = topic;
		items = sections{topic};
	} else {
		section = topics{topic}[END];
		unless (section) return;
		items = sections{section};
	}

	puts(out, "<div id='sidebar'>");
	puts(out, "<a href='index.html' border='0'>" .
	    "<img src='BitKeeper_SN_Blue.png' width='50'>" .
	    "</a>");
	puts(out, "<div><strong>${section}</strong></div>");
	puts(out, "<ul>");
	foreach (topic in items) {
		puts(out, "<li>");
		puts(out, "<a class='async' href='${topic}.html'>${topic}</a>");
		puts(out, "</li>");
	}
	puts(out, "</ul>");
	puts(out, "</div>");
}

void
page(FILE out, string buf)
{
	string	help[];
	string	indent;

	// in case the other guy changes indent
	buf =~ /^(\s+)/;
	indent = $1;

	buf =~ s/ \s+/ /g;
	if (buf =~ /bk\s+-\s+BitKeeper\s+configuration\s+management/) {
		help = { "bk" };
	} else {
		help = split(/bk help /, buf);
	}

	foreach (buf in help) {
		if (buf =~ /^\s*$/) continue;
		buf =~ s/\s+//g;
		buf =~ s/,//;

		if (fixup(&buf)) continue;

		fprintf(out,
		    "%s<a class='async' href=\"%s.html\">bk %s</a>\n",
		    indent, buf, buf);
		push(&references, buf . ".html");
	}
	fprintf(out, "\n");
}

void
category(FILE out, string buf)
{
	buf =~ /(\s+)(.*)/;
	if (fixup(&$2)) return;
	// warn("Category link '%s'\n", $2);
	fprintf(out, "%s<a href=\"%s.html\">%s</a>\n", $1, $2, $2);
	push(&references, $2 . ".html");
}

void
section(FILE out, string buf)
{
	if (buf =~ /^\s+bk\s+([a-zA-Z0-9\-]+)( \- .*)/) {
		if (fixup(&$1)) return;
		// warn("Section link '%s'\n", $1);
		fprintf(out,
		    "  <a class='async' href=\"%s.html\">bk %s</a>%s\n",
		    $1, $1, $2);
		push(&references, $1 . ".html");
	} else if (buf =~ /^\s+bk( \- .*)/) {
		// warn("Section link bk\n");
		fprintf(out, "  <a class='async' href=\"bk.html\">bk</a>%s\n",
		    $1);

		// no references here, we're good.
	} else {
		warn(buf);
		assert(0);
	}
}

int
fixup(string &buf)
{
	// Hack, by hand fixups
	buf =~ s/popu-late/populate/;
	buf =~ s/unpopulate/here/;	// that's the offical name
	buf =~ s/populate/here/;	// that's the offical name
	buf =~ s/tags/changes/;

	// junk, skip the link
	switch (buf) {
	    case /make\(1\)/:
	    case /diff\(1\)/:
	    case /diff3\(1\)/:
	    case m|notes/rfc934.txt|:
		return (1);
	}
	return (0);
}
