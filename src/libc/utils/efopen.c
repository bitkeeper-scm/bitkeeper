/*
 * Copyright 2006,2009,2016 BitMover, Inc
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

FILE *
efopen(char *env)
{
	char	*t, *p;
	int	port, sock;
	FILE	*f = 0;

	unless (t = getenv(env)) return (0);
	if (IsFullPath(t)) {
		f = fopen(t, "a");
	} else if ((p = strchr(t, ':')) && ((port = atoi(p+1)) > 0)) {
		*p = 0;
		sock = tcp_connect(t, port);
		*p = ':';
		if (sock >= 0) f = fdopen(sock, "w");
	} else {
		unless (f = fopen(DEV_TTY, "w")) f = fdopen(2, "w");
	}
	if (f) setvbuf(f, 0, _IONBF, 0);
	return (f);
}

int
efprintf(char *env, char *fmt, ...)
{
	va_list	ap;
	int	ret = -1;
	FILE	*f;

	va_start(ap, fmt);
	if (f = efopen(env)) {
		ret = vfprintf(f, fmt, ap);
		fclose(f);
	}
	va_end(ap);
	return (ret);
}
