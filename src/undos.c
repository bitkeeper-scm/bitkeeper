/*
 * Copyright 1999-2002,2005,2010,2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"

private	void	undos(FILE *f);
private	void	redos(FILE *f);

private	int	auto_new_line = 1;

int
undos_main(int ac, char **av)
{
	FILE	*f;
	int	c;
	int	do_redos = 0;
	int	rc = 0;

	while ((c = getopt(ac, av, "nr", 0)) != -1) {
		switch (c) {
		    case 'n': auto_new_line = 0; break;		/* doc 2.0 */
		    case 'r': do_redos = 1; break;		/* doc */
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind]) {
		if (do_redos) {
			redos(stdin);
		} else {
			undos(stdin);
		}
		return (0);
	}
	for (; av[optind]; optind++) {
		unless (f = fopen(av[optind], "r")) {
			perror(av[optind]);
			rc = 1;
			break;
		}
		if (do_redos) {
			redos(f);
		} else {
			undos(f);
		}
		fclose(f);
	}
	return (rc);
}

/*
 * Both of these routines strip out \r's.
 * If you have "text\r More text" and you have auto_new_line on then
 * it does a s|\r|\n| otherwise it does a s|\r||.
 */
private void
undos(FILE *f)
{
	int	c, lastc = 0;

	while (lastc != EOF) {
		c = getc(f);
		if (c == '\032') c = EOF; /* ^Z DOS end of file marker */
		switch (c) {
		    case '\r': break; /* skip CRs */
		    case '\n': putchar(c); break;
		    default:
			if (lastc == '\r') {
				putchar(auto_new_line ? '\n' : '\r');
				lastc = '\n';
			}
			if (c == EOF) {
				if ((lastc != '\n') && auto_new_line) {
					putchar('\n');
				}
			} else {
				putchar(c);
			}
			break;
		}
		lastc = c;
	}
}

/*
 * Make out have CRLF if it doesn't already
 */
private void
redos(FILE *f)
{
	int	c, lastc = 0;

	while (lastc != EOF) {
		c = getc(f);
		switch (c) {
		    case '\r': break; /* skip CRs */
		    case '\n': fputs("\r\n", stdout); break;
		    default:
			if (lastc == '\r') {
				fputs("\r\n", stdout);
				lastc = '\n';
			}
			if (c == EOF) {
				if ((lastc != '\n') && auto_new_line) {
					fputs("\r\n", stdout);
				}
			} else {
				putchar(c);
			}
			break;
		}
		lastc = c;
	}
}
