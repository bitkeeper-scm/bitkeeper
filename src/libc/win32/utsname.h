/*
 * Copyright 1999-2000,2016 BitMover, Inc
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

/* %K% Copyright (c) 1999 Andrew Chang */
#ifndef	_UTSNAME_H_
#define	_UTSNAME_H_
/* unix utsname.h simulation, Andrew Chang 1998 */

struct	utsname
{
	char sysname[NBUF_SIZE];
	char nodename[NBUF_SIZE];
};

extern int uname(struct utsname *);
#endif /* _USTNAME_H_ */

