/*
 * Copyright 2016 BitMover, Inc
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

search
search_parse(char *str)
{
	int	errorcode;
	PCRE2_SIZE	poff;
	char	*p;
	search  s;
	PCRE2_UCHAR	perr[256];

	bzero(&s, sizeof (s));
	p = strrchr(str, '/');
	if (!p || ((p > str) && p[-1] == '\\')) {
		fprintf(stderr, "unterminated search pattern: /%s\n", str);
		exit(1);
	}
	*p++ = 0;
	while (*p) {
		switch (*p++) {
		    case 'i': s.ignorecase = 1; break;
		    case 'g': s.want_glob = 1; break;
		    case 'r': s.want_re = 1; break;
		    default:
			fprintf(stderr, "search: /%c not implemented\n", p[-1]);
			return (s);	/* pattern is null */
		}
	}
	if (s.want_glob && s.want_re) {
		fprintf(stderr, "search: /g or /r but not both\n");
		return (s);
	}
	str = s.pattern = strdup(str);
	if (s.want_glob) return (s);
	s.want_re = 1;
	if (s.ignorecase) for (p = str; (*p = tolower(*p)); p++);
	unless (s.re = pcre2_compile((PCRE2_SPTR)str, PCRE2_ZERO_TERMINATED, 0, &errorcode, &poff, 0)) {
		pcre2_get_error_message(errorcode, perr, sizeof(perr));
		fprintf(stderr, "search: bad regex \"%s\"\n", (char *)perr);
		free(str);
		s.pattern = 0;
		return (s);
	}
	return (s);
}

int
search_either(char *str, search s)
{
	return (s.want_re ? search_regex(str, s) : search_glob(str, s));
}

int
search_glob(char *str, search s)
{
	char	*glob;
	int	ret;

	unless (s.pattern) return (0);
	glob = aprintf("*%s*", s.pattern);
	ret = match_one(str, glob, s.ignorecase);
	free(glob);
	return (ret);
}

int
search_regex(char *str, search s)
{
	pcre2_match_data *md;
	int	ret;

	unless (s.pattern && s.want_re) return (0);
	md = pcre2_match_data_create_from_pattern(s.re, 0);
	if (s.ignorecase) {
		char	*p;

		str = strdup(str);
		for (p = str; (*p = tolower(*p)); p++);
		ret = (pcre2_match(s.re, (PCRE2_SPTR)str, strlen(str), 0, 0, md, 0) >= 0);
		free(str);
		pcre2_match_data_free(md);
		return (ret);
	}
	ret = (pcre2_match(s.re, (PCRE2_SPTR)str, strlen(str), 0, 0, md, 0) >= 0);
	pcre2_match_data_free(md);
	return (ret);
}

void
search_free(search search)
{
	if (search.re) pcre2_code_free(search.re);
	search.re = 0;
}
