/*
 * Copyright 2013,2016 BitMover, Inc
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

/*
 * convert seconds to HH:MM:SS
 * Portable (for us) replacement for (gnu):
 *     TZ=GMT date --date=<seconds> +%T
 */
int
sec2hms_main(int ac, char **av)
{
	int	c, h, m, s, t = -1;

	while ((c = getopt(ac, av, "s:", 0)) != -1) {
		switch (c) {
		    case 's': t = atoi(optarg); break;
		    default: bk_badArg(c, av); return (1);
		}
	}
	if (t == -1) {
		unless (av[optind]) usage();
		t = atoi(av[optind]);
	}

	h = t / HOUR;
	m = (t % HOUR) / MINUTE;
	s = (t % MINUTE);

	printf("%02d:%02d:%02d\n", h, m, s);
	return (0);
}
