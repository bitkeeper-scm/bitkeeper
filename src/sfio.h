/*
 * Copyright 2012-2013,2016 BitMover, Inc
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

#ifndef	SFIO_BSIZ
#define SFIO_BSIZ       (16<<10)
#define SFIO_NOMODE     "SFIO v 1.4"    /* must be 10 bytes exactly */
#define SFIO_MODE       "SFIO vm1.4"    /* must be 10 bytes exactly */
#define SFIO_VERS(m)    (m ? SFIO_MODE : SFIO_NOMODE)

/* error returns, don't use 1, that's generic */
#define SFIO_LSTAT      2
#define SFIO_READLINK   3
#define SFIO_OPEN       4
#define SFIO_SIZE       5
#define SFIO_LOOKUP     6
#define SFIO_MORE       7       /* another sfio follows */
#define SFIO_SFILES	8

#define M_IN    1
#define M_OUT   2
#define M_LIST  3
#endif
