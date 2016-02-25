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
#ifndef	_FTW_H_
#define	_FTW_H_
/* Unix ftw.h simulation, Andrew Chang 1998 */
/* TODO: match the defined values with Unix counter part */
#define FTW_NS  1
#define FTW_D   2
#define FTW_F   3
#define FTW_DNR 4

extern int ftw(const char *, int(*func)(const char *, struct stat *, int), int);
#endif /* _FTW_H_ */

