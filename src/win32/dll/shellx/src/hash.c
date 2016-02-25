/*
 * Copyright 2006,2008,2016 BitMover, Inc
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
#include "memhash.h"

struct hashops	ops[] = {
	{	"memhash",	/* type 1 */
		memhash_new,
		0,		/* open */
		0,		/* close */
		memhash_free,
		memhash_fetch,
		memhash_store,
		memhash_insert,
		memhash_del,
		memhash_first,
		memhash_next,
		0,		/* last */
		0,		/* prev */
	},
};

/*
 * Calls the initialize function for the underlying hash
 * function and returns a pointer to it.
 * Any arguments after 'type' are passed to the creation methods.
 *
 * methods:
 *   memhash_new wrapmdbm_new
 */
hash *
hash_new(int type, ...)
{
	hash	*ret;
	va_list	ap;

	assert((type >= 0) && (type < (sizeof(ops)/sizeof(ops[0]))));
	va_start(ap, type);
	ret = ops[type].hashnew(ap);
	va_end(ap);
	if (ret) ret->ops = &ops[type];
	return (ret);
}

/*
 * Creates a new file-backed hash
 *
 * methods:
 *   wrapmdbm_open
 */
hash *
hash_open(int type, char *file, int flags, mode_t mode, ...)
{
	hash	*ret;
	va_list	ap;

	assert((type >= 0) && (type < (sizeof(ops)/sizeof(ops[0]))));
	assert(ops[type].hashopen);
	va_start(ap, mode);
	ret = ops[type].hashopen(file, flags, mode, ap);
	va_end(ap);
	if (ret) ret->ops = &ops[type];
	return (ret);
}

/*
 * Closes a file-backed hash
 *
 * methods:
 *   wrapmdbm_close
 */
int
hash_close(hash *h)
{
	int	ret;

	assert(h->ops->hashclose);
	ret = h->ops->hashclose(h);
	free(h);
	return (ret);
}

/*
 * Free a hash.  Should only be called on hash's created with hash_new()
 *
 * methods:
 *   memhash_free wrapmdbm_free
 */
int
hash_free(hash *h)
{
	int	ret;

	ret = h->ops->free(h);
	free(h);
	return (ret);
}
