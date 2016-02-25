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

#include "system.h"

void
diefn(int seppuku, char *file, int line, char *fmt, ...)
{
	va_list	ap;
	int	len;
	char	*format;

	len = strlen(fmt);
	if (len && (fmt[len-1] == '\n')) {
		format = aprintf("%s", fmt);
	} else {
		format = aprintf("%s at %s line %d.\n", fmt, file, line);
	}
	va_start(ap, fmt);
	vfprintf(stderr, format, ap);
	va_end(ap);
	free(format);
	if (seppuku) exit(1);
}
