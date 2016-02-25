/*
 * Copyright 2005-2006,2009-2010,2014-2016 BitMover, Inc
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

private	int	rmtreewalk(char *file, char type, void *data);
private	int	rmtreewalktail(char *file, void *data);

int
rmtree(char *dir)
{
	int	ret;

	unless (exists(dir)) return (0);
	ret = walkdir(dir,
	    (walkfns){ .file = rmtreewalk, .tail = rmtreewalktail }, 0);
	if (ret) fprintf(stderr, "rmtree(%s) failed.\n", dir);
	return (ret);
}

private int
rmtreewalk(char *file, char type, void *data)
{
	int	ret;
	struct	stat sb;

	if (type == 'd') {
		if (ret = rmIfRepo(file)) return (ret);
		if (!lstat(file, &sb) && ((sb.st_mode & 0700) != 0700)) {
			chmod(file, sb.st_mode | 0700);
		}
	} else {
		if (unlink(file) && (errno != ENOENT)) {
			perror(file);
			return (1);
		}
	}
	return (0);
}

private int
rmtreewalktail(char *dir, void *data)
{
	if (rmdir(dir)) {
		perror(dir);
		return (1);
	}
	return (0);
}
