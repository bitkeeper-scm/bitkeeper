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

typedef struct {
	char	*pkey;		/* product key backpointer */
	u32	oldtime;	/* oldest time in range */
	char	*ekey;		/* range endpoint key */
	char	*emkey;		/* range endpoint merge key */
} cmark;

#define	IS_POLYPATH(p)	(strneq(p, "BitKeeper/etc/poly/", 19))

cmark	*poly_check(sccs *cset, ser_t d);


