#include "mycrypt.h"

int hash_memory(int hash, const unsigned char *data, unsigned long len, unsigned char *dst)
{
    hash_state md;

    if(hash_is_valid(hash) == CRYPT_ERROR) {
        return CRYPT_ERROR;
    }

    hash_descriptor[hash].init(&md);
    hash_descriptor[hash].process(&md, data, len);
    hash_descriptor[hash].done(&md, dst);
    return CRYPT_OK;
}

int hash_file(int hash, const char *fname, unsigned char *dst)
{
    hash_state md;
    FILE *in;
    unsigned char buf[512];
    int x;

    if(hash_is_valid(hash) == CRYPT_ERROR) {
        return CRYPT_ERROR;
    }

    in = fopen(fname, "rb");
    if (in == NULL) { 
       crypt_error = "Error opening file in hash_file()."; 
       return CRYPT_ERROR; 
    }
    hash_descriptor[hash].init(&md);
    do {
        x = fread(buf, 1, sizeof(buf), in);
        hash_descriptor[hash].process(&md, buf, x);
    } while (x == sizeof(buf));
    fclose(in);
    hash_descriptor[hash].done(&md, dst);
#ifdef CLEAN_STACK
    zeromem(buf, sizeof(buf));
#endif
    return CRYPT_OK;
}

static const char __attribute__((unused)) *ID_TAG = "hash.c"; 
 
