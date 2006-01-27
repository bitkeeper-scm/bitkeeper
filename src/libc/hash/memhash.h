hash	*memhash_new(va_list ap);
int	memhash_free(hash *h);

void	*memhash_fetch(hash *h, void *kptr, int klen);
void	*memhash_insert(hash *h, void *kptr, int klen, void *val, int vlen);
void	*memhash_store(hash *h, void *kptr, int klen, void *val, int vlen);
int	memhash_delete(hash *h, void *kptr, int klen);

void	*memhash_first(hash *h);
void	*memhash_next(hash *h);

