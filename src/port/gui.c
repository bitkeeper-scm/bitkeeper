/*
 * Copyright 2001-2002,2004-2007,2015-2016 BitMover, Inc
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

int
gui_useDisplay(void)
{
	char	*p;

	if ((p = getenv("BK_NO_GUI_PROMPT")) && *p) return (0);
	if (win32() || macosx()) return ((p = getenv("BK_GUI")) && *p);
	return (getenv("DISPLAY") && (p = getenv("BK_GUI")) && *p);
}

char *
gui_displayName(void)
{
#ifdef WIN32
	return ("monitor");
#else
	if (gui_useAqua()) {return ("monitor");}
	return (getenv("DISPLAY"));
#endif

}

int
gui_useAqua(void)
{
#ifdef	__APPLE__
	return (1);
#else
	return (0);
#endif
}
