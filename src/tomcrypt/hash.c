#include "mycrypt.h"

int hash_memory(int hash, const unsigned char *data, unsigned long len, unsigned char *dst, unsigned long *outlen)
{
    hash_state md;

    _ARGCHK(data != NULL);
    _ARGCHK(dst != NULL);
    _ARGCHK(outlen != NULL);

    if (hash_is_valid(hash) == CRYPT_ERROR) {
        return CRYPT_ERROR;
    }

    if (*outlen < hash_descriptor[hash].hashsize) {
       crypt_error ="Invalid output size in hash_file().";
       return CRYPT_ERROR;
    }
    *outlen = hash_descriptor[hash].hashsize;

    hash_descriptor[hash].init(&md);
    hash_descriptor[hash].process(&md, data, len);
    hash_descriptor[hash].done(&md, dst);
    return CRYPT_OK;
}

int hash_filehandle(int hash, FILE *in, unsigned char *dst, unsigned long *outlen)
{
    hash_state md;
    unsigned char buf[512];
    int x;

    _ARGCHK(dst != NULL);
    _ARGCHK(outlen != NULL);

    if (hash_is_valid(hash) == CRYPT_ERROR) {
        return CRYPT_ERROR;
    }

    if (*outlen < hash_descriptor[hash].hashsize) {
       crypt_error ="Invalid output size in hash_file().";
       return CRYPT_ERROR;
    }
    *outlen = hash_descriptor[hash].hashsize;

    if (in == NULL) { 
       crypt_error = "Invalid file handle in hash_filehandle()."; 
       return CRYPT_ERROR; 
    }
    hash_descriptor[hash].init(&md);
    do {
        x = fread(buf, 1, sizeof(buf), in);
        hash_descriptor[hash].process(&md, buf, x);
    } while (x == sizeof(buf));
    hash_descriptor[hash].done(&md, dst);

#ifdef CLEAN_STACK
    zeromem(buf, sizeof(buf));
#endif

    return CRYPT_OK;
}


int hash_file(int hash, const char *fname, unsigned char *dst, unsigned long *outlen)
{
    FILE *in;

    _ARGCHK(fname != NULL);
    _ARGCHK(dst != NULL);
    _ARGCHK(outlen != NULL);

    if (hash_is_valid(hash) == CRYPT_ERROR) {
        return CRYPT_ERROR;
    }

    in = fopen(fname, "rb");
    if (in == NULL) { 
       crypt_error = "Error opening file in hash_file()."; 
       return CRYPT_ERROR; 
    }

    if (hash_filehandle(hash, in, dst, outlen) == CRYPT_ERROR) {
       return CRYPT_ERROR;
    }
    fclose(in);

    return CRYPT_OK;
}

