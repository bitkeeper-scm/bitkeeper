/*
 * Copyright 1999-2001,2006 BitMover, Inc
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
#ifndef	_BK_STAT_H_
#define	_BK_STAT_H_

#define	S_IFLNK 0120000  /* nt does not have sym link, but we need this for portability */
#define	S_ISLNK(st_mode) ((st_mode & S_IFMT) == S_IFLNK)

#define S_IRGRP 0040
#define S_IROTH 0004

#define S_IWGRP 0020
#define S_IWOTH 0002

#define	S_IXGRP 0010
#define	S_IXOTH 0001


#endif /* _BK_STAT_H_ */

