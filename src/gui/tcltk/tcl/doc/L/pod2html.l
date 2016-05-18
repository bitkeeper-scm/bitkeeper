#!/usr/bin/bk tclsh
/*
 * Copyright 2009-2016 BitMover, Inc
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
string	body[];
string	template;

int
main(string av[])
{
	FILE	f;
	int	i, ul;
	int	space = 0, dd = 0, p = 0, pre = 0, tr = 0, TOC = 0;
	string	c, buf, tmp, title, trim, all[], toc[], contents[];
	string	lopts[] = {
		"TOC",
		"title:",
		"template:"
	};

	/*
	 * -t<title> or --title=<title>
	 * --template=<templateFile>
	 */
	while (c = getopt(av, "t:", lopts)) {
		switch (c) {
		    case "TOC":
		    	TOC = 1;
			break;
		    case "t":
		    case "title":
			title = optarg;
			break;
		    case "template":
			template = optarg;
			break;
		}
	}

	unless (av[optind] && (f = fopen(av[optind], "r"))) {
		die("usage: ${av[0]} <filename>");
	}
	unless (title) title = av[optind];

	if (template == "") {
		header(title);
	}
	
	/*
	 * Load up the whole file in all[] and spit out the index.
	 */
	push(&toc, "<ul id=\"toc\">");
	ul = 1;
	while (buf = fgetline(f)) {
		// support shell comments (for the license)
		if (buf =~ /^#/) continue;

		push(&all, buf);
		if (buf =~ /^=head(\d+)\s+(.*)/) {
			i = (int)$1;
			for (buf = ""; --i; ) {
				buf .= "    ";
			}
			buf .= $2;
			push(&contents, buf);
				
			i = (int)$1;
			while (ul > i) {
				push(&toc, "</ul>");
				ul--;
			}
			while (i > ul) {
				push(&toc, "<ul>");
				ul++;
			}
			tmp = $2;
			tmp =~ s/\s+/_/g;
			buf =~ s/^=head(\d+)\s+//;
			push(&toc, "<li class=\"tocitem\">"
			    "<a href=\"#${tmp}\">${buf}</a></li>");
		}
	}
	while (ul--) push(&toc, "</ul>");
	fclose(f);

	if (TOC) {
		foreach (buf in contents) {
			puts(buf);
		}
		exit(0);
	}

	/*
	 * Now walk all[] and process the markup.  We currently handle:
	 * =head%d title
	 * =over 
	 * =item name
	 * =proto return_type func(args)
	 * =back
	 * <blank line>
	 * B<bold this>
	 * C<some code>
	 * I<italics>
	 */
	for (i = 0; i <= length(all); i++) {
		buf = inline(all[i]);
		if (buf =~ /^=toc/) {
			output(join("\n", toc));
		} else if (buf =~ /^=head(\d+)\s+(.*)/) {
			if ((int)$1 == 1) output("<hr>");
			tmp = $2;
			tmp =~ s/\s+/_/g;
			tmp = sprintf("<h%d><a name=\"%s\">%s</a></h%d>\n",
			    $1, tmp, $2, $1);
			output(tmp);
		} else if (buf =~ /^=over/) {
			output("<dl>");
		} else if (buf =~ /^=item\s+(.*)/) {
			if (dd) {
				output("</dd>");
				dd--;
			}
			output("<dt><strong>${$1}</strong></dt><dd>");
			dd++;
		} else if (buf =~ /^=options$/) {
			output("<table>");
		} else if (buf =~ /^=option\s+(.*)/) {
			if (tr) {
				output("</td>");
				output("</tr>");
				tr--;
			}
			output("<tr>");
			output("<td><strong>${$1}</strong></td>");
			output("<td>");
			tr++;
		} else if (buf =~ /^=options_end$/) {
			if (tr) {
				output("</td>");
				output("</tr>");
				tr--;
			}
			output("</table>");
		} else if (buf =~ /^=proto\s+([^ \t]+)\s+(.*)/) {
			if (dd) {
				output("</dd>");
				dd--;
			}
			output("<dt><b>${$1} ${$2}</b></dt><dd>");
			dd++;
		} else if (buf =~ /^=back/) {
			if (dd) {
				output("</dd>");
				dd--;
			}
			output("</dl>");
		} else if (buf =~ /^=include\s+(.*)/) {
			string	file = $1;

			unless (exists(file)) {
				puts(stderr, "file not found: ${file}");
				exit(1);
			}

			f = fopen(file, "r");
			read(f, &tmp);
			fclose(f);
			output(tmp);
		} else if (buf =~ /^\s*$/) {
			if (p) {
				output("</p>");
				p = 0;
			}
			if (pre) {
				/*
				 * If we see a blank line in a preformatted
				 * block, we don't want to stop the pre
				 * unless the next line is not indented.
				 * So peek ahead.
				 */
				if ((buf = all[i+1]) && (buf =~ /^\s/)) {
					output("");
					continue;
				}
				output("</pre>");
				pre = 0;
				trim = undef;
			}
			space = 1;
		} else {
			if (space) {
				if (buf =~ /^(\s+)[^ \t]+/) {
					trim = $1;
					output("<pre class=\"code\">");
					pre = 1;
				} else {
					output("<p>");
					p = 1;
				}
				space = 0;
			}
			if (trim) buf =~ s/^${trim}//;
			output(buf);
		}
	}

	if (template == "") {
		output("</body></html>");
	} else {
		string	t, map[];

		f = fopen(template, "r");
		read(f, &t);
		fclose(f);

		push(&map, "%TITLE%");
		push(&map, title);
		push(&map, "%TOC%");
		push(&map, join("\n", toc));
		push(&map, "%BODY%");
		push(&map, join("\n", body));
		puts(String_map(map, t));
	}

	return (0);
}

void
output(string s)
{
	if (template == "") {
		puts(s);
	} else {
		push(&body, s);
	}
}

/*
 * header and style sheet
 */
void
header(string title)
{
	string	head = <<EOF
<html>
<head>
<title>${title}</title>
<style>
pre {
	background: #eeeedd;
	border-width: 1px;
	border-style: solid solid solid solid;
	border-color: #ccc;
	padding: 5px 5px 5px 5px;
	font-family: monospace;
	font-weight: bolder;
}
dt {
	font-size: large;
}
</style>
</head>
<body>
EOF
	puts(head);
}

/*
 * Process B<bold>, C<code>, I<italic>, F<italic>, L<link>, S<non-breaking>.
 * This will handle nested stuff like C<if (!I<condition>)>
 * but dies if there are nested ones of the same type, i.e.,
 * C<whatever C<some more>>
 */
string
inline(string buf)
{
	string	c, prev = undef, result, link, stack[];
	int	B = 0, C = 0, I = 0, L = 0, S = 0;

	// sleazy fixes that sort of work
	buf =~ s/<</\&lt;\&lt;/;
	unless (buf =~ /[BCFILS]<.+>/) {
		buf =~ s/</\&lt;/g;
		buf =~ s/>/\&gt;/g;
		return (buf);
	}
	foreach (c in buf) {
		if ((c == "<") && prev) {
			if (prev == "B") {
				if (B++) die("Nested B<> unsupported: ${buf}");
				result[END] = "";
				result .= "<b>";
				push(&stack, "B");
			} else if (prev == "C") {
				if (C++) die("Nested C<> unsupported: ${buf}");
				result[END] = "";
				result .= "<code>";
				push(&stack, "CODE");
			} else if (prev == "I" || prev == "F") {
				if (I++) die("Nested I<> unsupported: ${buf}");
				result[END] = "";
				result .= "<i>";
				push(&stack, "I");
			} else if (prev == "L") {
				if (L++) die("Nested L<> unsupported: ${buf}");
				result[END] = "";
				result .= "<a href=\"";
				link = "";
				push(&stack, "L");
			} else if (prev == "S") {
				if (S++) die("Nested S<> unsupported: ${buf}");
				result[END] = "";
				push(&stack, "S");
			} else {
				result .= c;
				prev = c;
			}
		} else if ((c == ">") && length(stack)) {
			c = pop(&stack);
			if (c == "B") {
				B--;
			} else if (c == "CODE") {
				C--;
			} else if (c == "I") {
				I--;
			} else if (c == "L") {
				L--;
				result .= "\">${link}</a>";
				c = undef;
			} else {
				S--;
				c = undef;
			}
			if (defined(c)) {
				result .= "</" . String_tolower(c) . ">";
			}
			prev = undef;
		} else {
			if (S && isspace(c)) {
				result .= "&nbsp;";
			} else {
				result .= c;
			}
			if (L) link .= c;
			prev = c;
		}
	}
	return (result);
}
