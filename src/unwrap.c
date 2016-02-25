/*
 * Copyright 2000-2001,2005-2006,2015-2016 BitMover, Inc
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
unwrap_main(int ac,  char **av)
{
	char	buf[MAXLINE];
	int	n;

	/* Has to be a getline because we don't want stdin eating part of
	 * the input that we (may) want to send to the unwrap child.
	 */
	while (getline(0, buf, sizeof(buf)) > 0) {
		if (strneq(buf, "# Patch vers:", 13)) {
			out("\n");
			out(buf);
			out("\n");
			while ((n = read(0, buf, sizeof(buf))) > 0) {
				writen(1, buf, n);
			}
			return (0);
		} else if (strneq(buf, "## Wrapped with", 15)) {
			char	wrap_path[MAXLINE], wrap[MAXPATH];

			unless (sscanf(&buf[16], "%s ##", wrap) == 1) {
				fprintf(stderr,  "cannot extract wrapper\n");
				exit(1);
			}
			sprintf(wrap_path, "%s/un%swrap", bin, wrap);
			if (executable(wrap_path)) {
				char	*av[2] = {wrap_path, 0};

				return (spawnvp(_P_WAIT, wrap_path,
				    av) ? 1 : 0);
			} else {
				FILE *f = fopen(DEV_TTY, "w");
				fprintf(f,
					"bk receive: don't have %s wrapper\n",
					wrap);
				fclose(f);
				return (1);
			}
		}
	}
	/* we should never get here */
	return (1);
}
