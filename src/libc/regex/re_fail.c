/*
 * Copyright 2003,2012,2016 BitMover, Inc
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
#include "regex.h"

/* 
 * re_fail:
 *	default internal error handler for re_exec.
 *
 *	should probably do something like a longjump to recover
 *	gracefully.
 */ 
void	
re_fail(char *s, char c)
{
	fprintf(stderr, "%s [opcode %o]\n", s, c);
	exit(1);
}
