/*
 * Copyright 2010,2016 BitMover, Inc
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

#define   FSLAYER_NODEFINES
#include "system.h"

/*
 * This is only called by libc/fslayer/fslayer_rmIfRepo_stub.c
 * which is a stub function loaded by the installer.  The point
 * is in a non-remapped environment, like the installer, the callback
 * is a NOP, so rmtree() works as a normal 'rm -rf'.
 * BK never calls this.  It uses src/fslayer.c:fslayer_rmIfRepo().
 */
int
rmIfRepo(char *dir)
{
	return (0);
}
