/*
 * Copyright 1999-2001,2016 BitMover, Inc
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

private int	do_chksum(int fd, int off, int *sump);

/*
 * Calculate the same checksum as is used in BitKeeper.
 *
 * %W% %@%
 */
int
chksum_main(int ac, char **av)
{
	int	sum, fd, i;
	int	off = 0;

	if (av[1] && (strncmp(av[1], "-o", 2) == 0)) {	/* -o doc 2.0 */
		if (av[1][2]) {
			off = atoi(&av[1][2]);
			av++, ac--;
		} else {
			off = atoi(av[2]);
			av += 2;
			ac -= 2;
		}
	}
	if (ac == 1) {
		if (do_chksum(0, off, &sum)) return (1);
		printf("%d\n", sum);
	} else for (i = 1; i < ac; ++i) {
		fd = open(av[i], 0, 0);
		if (fd == -1) {
			perror(av[i]);
		} else {
			if (do_chksum(fd, off, &sum)) {
				close(fd);
				return (1);
			}
			close(fd);
			printf("%-20s %d\n", av[i], sum);
		}
	}
	return (0);
}

private int
do_chksum(int fd, int off, int *sump)
{
	unsigned char buf[16<<10];
	register unsigned char *p;
	register int i;
	unsigned short sum = 0;

	while (off--) {
		if (read(fd, buf, 1) != 1) return (1);
	}
	while ((i = read(fd, buf, sizeof(buf))) > 0) {
		for (p = buf; i--; sum += *p++);
	}
	*sump = (int)sum;
	return (0);
}
