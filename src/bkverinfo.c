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

hash	*data = 0;

int
bkver_main(int ac, char **av)
{
	int	c, i;
	int	keys = 0;
	char	**list = 0;
	longopt	lopts[] = {
		{ "keys", 300 },
		{0, 0}
	};

	while ((c = getopt(ac, av, "", lopts)) != -1) {
		switch(c) {
		    case 300:  // --keys
			keys = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (keys) {
		if (av[optind]) usage();
		bkver("");	/* for the side-effects */
		EACH_HASH(data) list = addLine(list, data->kptr);
		sortLines(list, 0);
		EACH(list) printf("%s\n", list[i]);
		freeLines(list, 0);
	} else {
		unless (av[optind]) usage();
		if (av[optind+1]) usage();
		printf("%s\n", bkver(av[optind]));
	}
	// don't try to free 'data', the atexit() code uses it
	return (0);
}

char *
bkver(char *key)
{
	FILE	*f;
	char	buf[MAXPATH];

	unless (data) {
		concat_path(buf, bin, "version");
		unless (f = fopen(buf, "r")) return ("NOFILE");
		data = hash_fromStream(0, f);
		fclose(f);
	}
	return (hash_fetchStr(data, key));
}
