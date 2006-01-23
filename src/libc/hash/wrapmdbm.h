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
