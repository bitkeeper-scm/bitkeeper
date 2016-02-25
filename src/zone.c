/*
 * Copyright 1999-2002,2011,2016 BitMover, Inc
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
 * Return the local timezone as a string "[-]HH:MM"
 * The current GMT time_t is passed to because the zone changes
 * with daylight savings time.
 */
char	*
sccs_zone(time_t tt)
{
	static	char buf[8];
	long	offset;
	int	hwest, mwest;
	char	sign = '+';

	localtimez(&tt, &offset);
	/*
	 * What I want is to have 8 hours west of GMT to be -08:00.
	 */
	if (offset < 0) {
		sign = '-';
		offset = -offset;
	}
	hwest = offset / 3600;
	mwest = (offset % 3600) / 60;
	assert(offset - hwest * 3600 - mwest * 60 == 0);
	sprintf(buf, "%c%02d:%02d", sign, hwest, mwest);
	return (buf);
}

int
zone_main(int ac, char **av)
{
	printf("%s\n", sccs_zone(time(0)));
	return (0);
}
