int	oldrsa_encrypt_key(u8 *inkey, int inlen,
    u8 *outkey, unsigned long *outlen,
    prng_state *prng, int wprng, rsa_key *key);
int	oldrsa_decrypt_key(u8 *in, unsigned long *len,
    u8 *outkey, unsigned long *keylen,
    rsa_key *key);
int	oldrsa_sign_hash(u8 *hash, int hashlen, u8 *out, unsigned long *outlen,
    int hashid, rsa_key *key);
int	oldrsa_verify_hash(u8 *sig, int *siglen, u8 *hash, int hashlen,
    int *stat, int hashid, rsa_key *key);
int	oldrsa_import(u8 *in, rsa_key *key);
