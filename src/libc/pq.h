/*
 * Copyright 2002-2007,2009-2016 BitMover, Inc
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

#ifndef	_LIB_PQ_H
#define	_LIB_PQ_H

void	pq32_insert(u32 **pq, u32 item);
u32	pq32_delMax(u32 **pq);
#define	pq32_isEmpty(pq)	(nLines(pq) == 0)

typedef	struct	pq	PQ;
PQ	*pq_new(int (*cmp)(void *a, void *b));
void	pq_insert(PQ *, void *);
void	*pq_pop(PQ *);
void	*pq_peek(PQ *);
void	pq_free(PQ *);

#endif	/* _LIB_PQ_H */
