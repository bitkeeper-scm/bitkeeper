/*
 * Copyright 2000-2003,2005,2016 BitMover, Inc
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

/*
 * This works even if there isn't a gfile.
 */
int
unedit_main(int ac, char **av)
{
	sccs	*s = 0;
	int	sflags = SF_NODIREXPAND;
	int	ret = 0;
	char	*name;

	/*
	 * Too dangerous to unedit everything automagically,
	 * make 'em spell it out.
	 */
	optind = 1;
	unless (name = sfileFirst("unedit", &av[optind], sflags)) {
		fprintf(stderr,
		  "unedit: must have explicit list when discarding changes.\n");
			return(1);
	}
	while (name) {
		s = sccs_init(name, SILENT|INIT_NOCKSUM);
		if (s) {
			if (sccs_unedit(s, SILENT)) ret = 1;
			sccs_free(s);
		}
		name = sfileNext();
	}
	if (sfileDone()) ret = 1;
	return (ret);
}
