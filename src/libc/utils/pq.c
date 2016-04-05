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
swap(u32 *pq, int i, int j)
{
	u32	tmp = pq[i];

	pq[i] = pq[j];
	pq[j] = tmp;
}

private inline void
swim(u32 *pq, int k)
{
	while ((k > 1) && (pq[k/2] < pq[k])) {
		swap(pq, k/2, k);
		k = k/2;
	}
}

private inline void
sink(u32 *pq, int k, int N)
{
	int	j;

	while (2*k <= N) {
		j = 2*k;
		if ((j < N) && (pq[j] < pq[j+1])) j++;
		unless (pq[k] < pq[j]) break;
		swap(pq, k, j);
		k = j;
	}
}

void
pq_insert(u32 **pq, u32 item)
{
	u32	i = item;

	addArray(pq, &i);
	swim(*pq, nLines(*pq));
}

u32
pq_delMax(u32 **pqp)
{
	u32	ret;
	u32	*pq = *pqp;
	int	N = nLines(pq);

	assert(N > 0);
	ret = pq[1];
	pq[1] = pq[N--];
	truncArray(pq, N);
	sink(pq, 1, N);
	return ret;
}
