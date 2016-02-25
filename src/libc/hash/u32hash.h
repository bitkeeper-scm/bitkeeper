/*
 * Copyright 2014-2016 BitMover, Inc
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

hash	*u32hash_new(va_list ap);
int	u32hash_free(hash *h);

void	*u32hash_fetch(hash *h, void *kptr, int klen);
void	*u32hash_insert(hash *h, void *kptr, int klen, void *val, int vlen);
void	*u32hash_store(hash *h, void *kptr, int klen, void *val, int vlen);

void	*u32hash_first(hash *h);
void	*u32hash_next(hash *h);

int	u32hash_count(hash *h);
