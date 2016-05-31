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

#include "system.h"
#include "pq.h"

private inline void
swap32(u32 *pq, int i, int j)
{
	u32	tmp = pq[i];

	pq[i] = pq[j];
	pq[j] = tmp;
}

private inline void
swim32(u32 *pq, int k)
{
	while ((k > 1) && (pq[k/2] < pq[k])) {
		swap32(pq, k/2, k);
		k = k/2;
	}
}

private inline void
sink32(u32 *pq, int k, int N)
{
	int	j;

	while (2*k <= N) {
		j = 2*k;
		if ((j < N) && (pq[j] < pq[j+1])) j++;
		unless (pq[k] < pq[j]) break;
		swap32(pq, k, j);
		k = j;
	}
}

void
pq32_insert(u32 **pq, u32 item)
{
	u32	i = item;

	addArray(pq, &i);
	swim32(*pq, nLines(*pq));
}

u32
pq32_delMax(u32 **pqp)
{
	u32	ret;
	u32	*pq = *pqp;
	int	N = nLines(pq);

	assert(N > 0);
	ret = pq[1];
	pq[1] = pq[N--];
	truncArray(pq, N);
	sink32(pq, 1, N);
	return ret;
}

// -------------------------------------------------

struct	pq {
	int	(*cmp)(void *a, void *b);
	char	**d;
};

private inline void
swap(PQ *pq, int i, int j)
{
	char	**d = pq->d;
	void	*tmp = d[i];

	d[i] = d[j];
	d[j] = tmp;
}

PQ *
pq_new(int (*cmp)(void *a, void *b))
{
	PQ	*ret;

	ret = new(PQ);
	ret->cmp = cmp;
	return (ret);
}

private inline void
swim(PQ *pq, int k)
{
	while ((k > 1) && (pq->cmp(pq->d[k/2], pq->d[k]) < 0)) {
		swap(pq, k/2, k);
		k = k/2;
	}
}

void
pq_insert(PQ *pq, void *item)
{
	pq->d = addLine(pq->d, item);
	swim(pq, nLines(pq->d));
}

private inline void
sink(PQ *pq, int k, int N)
{
	int	j;

	while (2*k <= N) {
		j = 2*k;
		if ((j < N) && (pq->cmp(pq->d[j], pq->d[j+1]) < 0)) j++;
		if (pq->cmp(pq->d[k], pq->d[j]) >= 0) break;
		swap(pq, k, j);
		k = j;
	}
}

/* remove max item according to comparison function */
void *
pq_pop(PQ *pq)
{
	void	*ret;
	int	N = nLines(pq->d);

	assert(N > 0);
	ret = pq->d[1];
	pq->d[1] = popLine(pq->d);
	sink(pq, 1, N-1);
	return ret;
}

void *
pq_peek(PQ *pq)
{
	return (nLines(pq->d) ? pq->d[1] : 0);
}

void
pq_free(PQ *pq)
{
	freeLines(pq->d, 0);
	free(pq);
}
