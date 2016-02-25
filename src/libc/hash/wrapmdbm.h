/*
 * Copyright 2006,2016 BitMover, Inc
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

#ifndef	_WRAPMDBM_H
#define	_WRAPMDBM_H

hash	*wrapmdbm_new(va_list ap);
hash	*wrapmdbm_open(char *file, int flags, mode_t mode, va_list ap);
int	wrapmdbm_close(hash *h);
int	wrapmdbm_free(hash *h);

void	*wrapmdbm_fetch(hash *h, void *kptr, int klen);
void	*wrapmdbm_insert(hash *h, void *kptr, int klen, void *val, int vlen);
void	*wrapmdbm_store(hash *h, void *kptr, int klen, void *val, int vlen);
int	wrapmdbm_delete(hash *h, void *kptr, int klen);

void	*wrapmdbm_first(hash *h);
void	*wrapmdbm_next(hash *h);
void	*wrapmdbm_last(hash *h);
void	*wrapmdbm_prev(hash *h);

#endif	/* _WRAPMDBM_H */
