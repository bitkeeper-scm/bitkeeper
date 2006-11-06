void	rand_setSeed(int setpid);
int	rand_checkSeed(void);
int	rand_getPrng(prng_state **p);
void	rand_getBytes(unsigned char *buf, unsigned int len);
