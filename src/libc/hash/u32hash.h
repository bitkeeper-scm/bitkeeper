hash	*u32hash_new(va_list ap);
int	u32hash_free(hash *h);

void	*u32hash_fetch(hash *h, void *kptr, int klen);
void	*u32hash_insert(hash *h, void *kptr, int klen, void *val, int vlen);
void	*u32hash_store(hash *h, void *kptr, int klen, void *val, int vlen);

void	*u32hash_first(hash *h);
void	*u32hash_next(hash *h);

int	u32hash_count(hash *h);
