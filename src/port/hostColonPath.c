/*
 * Copyright 2001,2015-2016 BitMover, Inc
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

#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifndef WIN32
char *
isHostColonPath(char *path)
{
	char *s;

	unless (s = strchr(path, ':')) return (0);
	return (s); /* s point to colon */
}
#else
char *
isHostColonPath(char *path)
{
	char *s;

	/*
	 * Win32 note:
	 * There are some ambiguity in the X:Y path format;
	 * X:Y could be host:path or dirve:path
	 * If X is only one character long, we assume X is a drive name
	 * If X is longer than one charatctr we assume X is a host name
	 *
	 * XXX This mean we do not support host name which is only
	 * one character long on win32 platform.
	 */
	unless ((s = strchr(path, ':')) && (s != &path[1])) {
		return (0);
	}
	return (s); /* s point to colon */
}
#endif
