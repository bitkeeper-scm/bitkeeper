CC=gcc
CPPFLAGS=-Isrc/headers -I../tommath -DLTC_SOURCE -DLTC_NO_ROLC
CFLAGS=-g -O3 -Wall -Wno-parentheses -Wno-char-subscripts -Wno-format-y2k -Wstrict-prototypes
AR=ar rc
RANLIB=ranlib

# Override Solaris make.rules
.SCCS_GET:
	$(Q)bk get -q $@

OBJS =	oldrsa.o \
	randseed.o \
	src/ciphers/aes/aes_enc.o \
	src/ciphers/aes/aes.o \
	src/misc/base64/base64_decode.o \
	src/misc/base64/base64_encode.o \
	src/hashes/helper/hash_file.o \
	src/hashes/helper/hash_filehandle.o \
	src/hashes/helper/hash_memory.o \
	src/hashes/helper/hash_memory_multi.o \
	src/hashes/md5.o \
	src/hashes/sha1.o \
	src/mac/hmac/hmac_done.o \
	src/mac/hmac/hmac_file.o \
	src/mac/hmac/hmac_init.o \
	src/mac/hmac/hmac_memory.o \
	src/mac/hmac/hmac_memory_multi.o \
	src/mac/hmac/hmac_process.o \
	src/mac/hmac/hmac_test.o \
	src/math/gmp_desc.o \
	src/math/ltm_desc.o \
	src/math/multi.o \
	src/math/rand_prime.o \
	src/math/tfm_desc.o \
	src/misc/burn_stack.o \
	src/misc/crypt/crypt.o \
	src/misc/crypt/crypt_argchk.o \
	src/misc/crypt/crypt_cipher_descriptor.o \
	src/misc/crypt/crypt_cipher_is_valid.o \
	src/misc/crypt/crypt_find_cipher.o \
	src/misc/crypt/crypt_find_cipher_any.o \
	src/misc/crypt/crypt_find_cipher_id.o \
	src/misc/crypt/crypt_find_hash.o \
	src/misc/crypt/crypt_find_hash_any.o \
	src/misc/crypt/crypt_find_hash_id.o \
	src/misc/crypt/crypt_find_hash_oid.o \
	src/misc/crypt/crypt_find_prng.o \
	src/misc/crypt/crypt_hash_descriptor.o \
	src/misc/crypt/crypt_hash_is_valid.o \
	src/misc/crypt/crypt_ltc_mp_descriptor.o \
	src/misc/crypt/crypt_prng_descriptor.o \
	src/misc/crypt/crypt_prng_is_valid.o \
	src/misc/crypt/crypt_register_cipher.o \
	src/misc/crypt/crypt_register_hash.o \
	src/misc/crypt/crypt_register_prng.o \
	src/misc/crypt/crypt_unregister_cipher.o \
	src/misc/crypt/crypt_unregister_hash.o \
	src/misc/crypt/crypt_unregister_prng.o \
	src/misc/error_to_string.o \
	src/misc/pkcs5/pkcs_5_1.o \
	src/misc/pkcs5/pkcs_5_2.o \
	src/misc/zeromem.o \
	src/modes/cfb/cfb_decrypt.o \
	src/modes/cfb/cfb_done.o \
	src/modes/cfb/cfb_encrypt.o \
	src/modes/cfb/cfb_getiv.o \
	src/modes/cfb/cfb_setiv.o \
	src/modes/cfb/cfb_start.o \
	src/modes/ctr/ctr_decrypt.o \
	src/modes/ctr/ctr_done.o \
	src/modes/ctr/ctr_encrypt.o \
	src/modes/ctr/ctr_getiv.o \
	src/modes/ctr/ctr_setiv.o \
	src/modes/ctr/ctr_start.o \
	src/pk/asn1/der/bit/der_decode_bit_string.o \
	src/pk/asn1/der/bit/der_encode_bit_string.o \
	src/pk/asn1/der/bit/der_length_bit_string.o \
	src/pk/asn1/der/boolean/der_decode_boolean.o \
	src/pk/asn1/der/boolean/der_encode_boolean.o \
	src/pk/asn1/der/boolean/der_length_boolean.o \
	src/pk/asn1/der/choice/der_decode_choice.o \
	src/pk/asn1/der/ia5/der_decode_ia5_string.o \
	src/pk/asn1/der/ia5/der_encode_ia5_string.o \
	src/pk/asn1/der/ia5/der_length_ia5_string.o \
	src/pk/asn1/der/integer/der_decode_integer.o \
	src/pk/asn1/der/integer/der_encode_integer.o \
	src/pk/asn1/der/integer/der_length_integer.o \
	src/pk/asn1/der/object_identifier/der_decode_object_identifier.o \
	src/pk/asn1/der/object_identifier/der_encode_object_identifier.o \
	src/pk/asn1/der/object_identifier/der_length_object_identifier.o \
	src/pk/asn1/der/octet/der_decode_octet_string.o \
	src/pk/asn1/der/octet/der_encode_octet_string.o \
	src/pk/asn1/der/octet/der_length_octet_string.o \
	src/pk/asn1/der/printable_string/der_decode_printable_string.o \
	src/pk/asn1/der/printable_string/der_encode_printable_string.o \
	src/pk/asn1/der/printable_string/der_length_printable_string.o \
	src/pk/asn1/der/sequence/der_decode_sequence_ex.o \
	src/pk/asn1/der/sequence/der_decode_sequence_flexi.o \
	src/pk/asn1/der/sequence/der_decode_sequence_multi.o \
	src/pk/asn1/der/sequence/der_encode_sequence_ex.o \
	src/pk/asn1/der/sequence/der_encode_sequence_multi.o \
	src/pk/asn1/der/sequence/der_length_sequence.o \
	src/pk/asn1/der/sequence/der_sequence_free.o \
	src/pk/asn1/der/set/der_encode_set.o \
	src/pk/asn1/der/set/der_encode_setof.o \
	src/pk/asn1/der/short_integer/der_decode_short_integer.o \
	src/pk/asn1/der/short_integer/der_encode_short_integer.o \
	src/pk/asn1/der/short_integer/der_length_short_integer.o \
	src/pk/asn1/der/utctime/der_decode_utctime.o \
	src/pk/asn1/der/utctime/der_encode_utctime.o \
	src/pk/asn1/der/utctime/der_length_utctime.o \
	src/pk/pkcs1/pkcs_1_i2osp.o \
	src/pk/pkcs1/pkcs_1_mgf1.o \
	src/pk/pkcs1/pkcs_1_oaep_decode.o \
	src/pk/pkcs1/pkcs_1_oaep_encode.o \
	src/pk/pkcs1/pkcs_1_os2ip.o \
	src/pk/pkcs1/pkcs_1_pss_decode.o \
	src/pk/pkcs1/pkcs_1_pss_encode.o \
	src/pk/rsa/rsa_decrypt_key.o \
	src/pk/rsa/rsa_encrypt_key.o \
	src/pk/rsa/rsa_export.o \
	src/pk/rsa/rsa_exptmod.o \
	src/pk/rsa/rsa_free.o \
	src/pk/rsa/rsa_import.o \
	src/pk/rsa/rsa_make_key.o \
	src/pk/rsa/rsa_sign_hash.o \
	src/pk/rsa/rsa_verify_hash.o \
	src/prngs/rng_get_bytes.o \
	src/prngs/rng_make_prng.o \
	src/prngs/sprng.o \
	src/prngs/yarrow.o 

SRCS = $(OBJS:%.o=%.c)
HDRS =  oldrsa.h \
	randseed.h \
	../tommath/tommath.h \
	../tommath/tommath_class.h \
	../tommath/tommath_superclass.h \
	src/headers/tomcrypt_cfg.h \
	src/headers/tomcrypt_mac.h \
	src/headers/tomcrypt_macros.h \
	src/headers/tomcrypt_custom.h \
	src/headers/tomcrypt_argchk.h \
	src/headers/tomcrypt_cipher.h \
	src/headers/tomcrypt_pk.h \
	src/headers/tomcrypt_hash.h \
	src/headers/tomcrypt_math.h \
	src/headers/tomcrypt_misc.h \
	src/headers/tomcrypt.h src/headers/tomcrypt_pkcs.h \
	src/headers/tomcrypt_prng.h \
	testprof/tomcrypt_test.h

all: libtomcrypt.a

libtomcrypt.a: $(OBJS)
	$(if $(Q),@echo AR libtomcrypt.a,)
	$(Q)$(RM) $@
	$(Q)$(AR) $@ $(OBJS)
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

$(OBJS): $(HDRS)

src/ciphers/aes/aes_enc.o: src/ciphers/aes/aes.c src/ciphers/aes/aes_tab.c
	$(if $(Q),@echo CC tomcrypt/aes_enc.c,)
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -DENCRYPT_ONLY -c $< -o $@

.PHONY: clean
clean:
	$(if $(Q),@echo Cleaning tomcrypt,)
	$(Q)rm -f $(OBJS) libtomcrypt.a tags.local

.PHONY: clobber
clobber: clean
	-$(Q)bk -r. clean

srcs: $(SRCS) $(HDRS)

tags.local: $(filter-out %/aes_enc.c,$(SRCS)) $(HDRS)
	cd ..;\
	ctags -f tomcrypt/$@ --file-tags=yes --c-types=d+f+s+t \
		$(patsubst %,tomcrypt/%,$^)

.c.o:
	$(if $(Q),@echo CC tomcrypt/$<,)
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@
