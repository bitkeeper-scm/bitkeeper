/*
 * Copyright 2004,2007,2016 BitMover, Inc
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

int
main(int ac, char **av)
{
	GZIP	*gz = gzopen(av[1], "wb");
	int	n;
	char	buf[BUFSIZ];

	setmode(0, _O_BINARY);
	while ((n = read(0, buf, sizeof(buf))) > 0) {
		gzwrite(gz, buf, n);
	}
	gzclose(gz);
	exit(0);
}
