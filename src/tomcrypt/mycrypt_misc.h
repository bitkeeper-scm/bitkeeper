/* ---- BASE64 Routines ---- */
#ifdef BASE64
extern int base64_encode(const unsigned char *in,  unsigned long len, 
                               unsigned char *out, unsigned long *outlen);

extern int base64_decode(const unsigned char *in,  unsigned long len, 
                               unsigned char *out, unsigned long *outlen);
#endif

/* ---- COIN Flips ---- */
enum { 
    CF_HOST=0,  /* HOST  makes up the challenge */
    CF_GUEST,   /* GUEST sends their guess back to the HOST */
    CF_WIN,     /* A winning outcome */
    CF_LOSE     /* A losing outcome */
};

extern int coin_toss(const unsigned char *shared_secret, 
                     int secret_len, int whoami, int *result);


/* ---- MEM routines ---- */
extern void zeromem(void *dst, unsigned long len);
extern void burn_stack(unsigned long len);

/* ---- Additional ZLIB Functions ---- */
#ifdef GZIP
extern int pack_buffer(const unsigned char *in,  unsigned long inlen,
                             unsigned char *out, unsigned long *outlen);

extern int unpack_buffer(const unsigned char *in,  unsigned long inlen,
                               unsigned char *out, unsigned long *outlen);
#endif

extern const char *crypt_build_settings;
