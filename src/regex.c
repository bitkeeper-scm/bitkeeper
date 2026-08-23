/*
 * Copyright 2004,2010,2014-2016 BitMover, Inc
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

int
regex_main(int ac, char **av)
{
	pcre2_code	*re;
	int	i;
	int	errorcode;
	PCRE2_SIZE	off;
	int	matched = 0;
	pcre2_match_data *md = 0;

	unless (av[1] && av[2]) usage();
	unless (re = pcre2_compile((PCRE2_SPTR)av[1], PCRE2_ZERO_TERMINATED, 0, &errorcode, &off, 0)) {
		PCRE2_UCHAR error[256];
		pcre2_get_error_message(errorcode, error, sizeof(error));
		fprintf(stderr, "pcre_compile returned 0: %s\n", (char *)error);
		return(1);
	}
	md = pcre2_match_data_create_from_pattern(re, 0);
	for (i = 2; av[i]; i++) {
		if (pcre2_match(re, (PCRE2_SPTR)av[i], strlen(av[i]), 0, 0, md, 0) >= 0) {
			printf("%s matches.\n", av[i]);
			matched = 1;
		}
	}
	unless (matched) printf("No match.\n");
	pcre2_match_data_free(md);
	pcre2_code_free(re);
	return (matched ? 0 : 1);
}
