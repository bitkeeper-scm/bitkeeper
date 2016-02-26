/*
 * Copyright 1999-2000,2006 BitMover, Inc
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
#ifndef	_TIMES_H_
#define	_TIMES_H_
/* unix times.h simulation, Andrew Chang 1998 */

struct timezone
{
	int not_used;
};

extern void gettimeofday(struct timeval *, struct timezone *);
#endif /* _TIMES_H_ */

