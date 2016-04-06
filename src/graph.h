/*
 * Copyright 2010-2012,2015-2016 BitMover, Inc
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

#ifndef	_GRAPH_H
#define	_GRAPH_H

typedef int	(*walkfcn)(sccs *s, ser_t d, void *token);

int	graph_v1(sccs *s);	/* when done, graph is in v1 form */
int	graph_v2(sccs *s);	/* when done, graph is in v2 form */

int	graph_fixMerge(sccs *s, ser_t first);
int	graph_convert(sccs *s, int fixpfile);
int	graph_check(sccs *s);

int	graph_hasDups(sccs *s, ser_t d, u8 *slist);
int	symdiff_expand(sccs *s, ser_t *leftlist, ser_t right, u8 *slist);
void	symdiff_compress(sccs *s, ser_t *leftlist, ser_t right,
	    u8 *slist, int count);
int	graph_kidwalk(sccs *s, walkfcn toTip, walkfcn toRoot, void *token);

#endif
