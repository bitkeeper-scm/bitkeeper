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

/* %K% Copyright (c) 1999 Andrew Chang */
#ifndef	_DIRENT_H_
#define	_DIRENT_H_
/* unix dirent.h simulation, Andrew Chang 1998 */
#include "misc.h"
struct dirent
{
	char d_name[NBUF_SIZE];
};

struct dir_info {
	long	dh;
	char	first[1024];
};

typedef struct dir_info DIR;

extern DIR *opendir(const char *);
extern struct dirent *readdir(DIR *);
extern void closedir(DIR *);
#endif /*_DIRENT_H_ */

