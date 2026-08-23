/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

#ifndef BN_H_
#define BN_H_

#include "tclInt.h"
#include "tclTomMathDecls.h"

#ifndef MODULE_SCOPE
#define MODULE_SCOPE extern
#endif

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdio.h>

#ifndef MP_32BIT
#define MP_32BIT
#endif
#ifndef MP_28BIT
#define MP_28BIT
#endif
#define MP_DIGIT_BIT 28

#ifndef MP_DIGIT_DECLARED
typedef uint32_t mp_digit;
#define MP_DIGIT_DECLARED
#endif
typedef uint64_t private_mp_word;
#define mp_word private_mp_word

#define MP_MASK ((((mp_digit)1)<<((mp_digit)MP_DIGIT_BIT))-((mp_digit)1))
#define MP_DIGIT_MAX MP_MASK
#define DIGIT_BIT MP_DIGIT_BIT

typedef int mp_sign;
#define MP_ZPOS 0
#define MP_NEG 1

typedef int mp_ord;
#define MP_LT -1
#define MP_EQ 0
#define MP_GT 1

typedef int mp_bool;
#define MP_YES 1
#define MP_NO 0

typedef int mp_err;
#define MP_OKAY 0
#define MP_ERR -1
#define MP_MEM -2
#define MP_VAL -3
#define MP_RANGE MP_VAL
#define MP_ITER -4
#define MP_BUF -5

typedef int mp_order;
#define MP_LSB_FIRST -1
#define MP_MSB_FIRST 1

typedef int mp_endian;
#define MP_LITTLE_ENDIAN -1
#define MP_NATIVE_ENDIAN 0
#define MP_BIG_ENDIAN 1

#define LTM_PRIME_BBS 0x0001
#define LTM_PRIME_SAFE 0x0002
#define LTM_PRIME_2MSB_ON 0x0008
#define MP_PRIME_BBS 0x0001
#define MP_PRIME_SAFE 0x0002
#define MP_PRIME_2MSB_ON 0x0008

#ifndef MP_WUR
#define MP_WUR
#endif
#ifndef MP_DEPRECATED
#define MP_DEPRECATED(x)
#endif
#ifndef MP_NORETURN
#define MP_NORETURN
#endif
#ifndef PRIVATE_MP_DEPRECATED_PRAGMA
#define PRIVATE_MP_DEPRECATED_PRAGMA(s)
#endif
#ifndef MP_DEPRECATED_PRAGMA
#define MP_DEPRECATED_PRAGMA(s)
#endif
#ifndef MP_NULL_TERMINATED
#define MP_NULL_TERMINATED
#endif

#if defined(BUILD_tcl) || !defined(_WIN32)
MODULE_SCOPE int KARATSUBA_MUL_CUTOFF,
           KARATSUBA_SQR_CUTOFF,
           TOOM_MUL_CUTOFF,
           TOOM_SQR_CUTOFF;
MODULE_SCOPE const mp_digit ltm_prime_tab[];
MODULE_SCOPE const char *const mp_s_rmap;
#endif

#ifndef MP_PREC
#define PRIVATE_MP_PREC 32
#define MP_PREC PRIVATE_MP_PREC
#endif

#ifdef MP_8BIT
# define PRIVATE_MP_PRIME_TAB_SIZE 31
#else
# define PRIVATE_MP_PRIME_TAB_SIZE 256
#endif
#define PRIME_SIZE PRIVATE_MP_PRIME_TAB_SIZE

#define PRIVATE_MP_WARRAY (int)(1uLL << (((CHAR_BIT * sizeof(private_mp_word)) - (2 * MP_DIGIT_BIT)) + 1))
#define MP_WARRAY PRIVATE_MP_WARRAY

#ifndef MP_INT_DECLARED
#define MP_INT_DECLARED
typedef struct mp_int mp_int;
#endif
struct mp_int {
    int used, alloc;
    mp_sign sign;
    mp_digit *dp;
};

typedef int private_mp_prime_callback(unsigned char *dst, int len, void *dat);
typedef private_mp_prime_callback ltm_prime_callback;

#define USED(m) ((m)->used)
#define DIGIT(m,k) ((m)->dp[(k)])
#define SIGN(m) ((m)->sign)

#define mp_iszero(a) (((a)->used == 0) ? MP_YES : MP_NO)
#define mp_isneg(a)  (((a)->sign != MP_ZPOS) ? MP_YES : MP_NO)

#define mp_read_raw(mp, str, len) mp_read_signed_bin((mp), (str), (len))
#define mp_raw_size(mp) mp_signed_bin_size(mp)
#define mp_toraw(mp, str) mp_to_signed_bin((mp), (str))
#define mp_read_mag(mp, str, len) mp_read_unsigned_bin((mp), (str), (len))
#define mp_mag_size(mp) mp_unsigned_bin_size(mp)
#define mp_tomag(mp, str) mp_to_unsigned_bin((mp), (str))

#define mp_tobinary(M, S) mp_toradix((M), (S), 2)
#define mp_tooctal(M, S) mp_toradix((M), (S), 8)
#define mp_todecimal(M, S) mp_toradix((M), (S), 10)
#define mp_tohex(M, S) mp_toradix((M), (S), 16)

#define s_mp_mul(a, b, c) s_mp_mul_digs(a, b, c, (a)->used + (b)->used + 1)
#define mp_prime_random(a, t, size, bbs, cb, dat) mp_prime_random_ex(a, t, ((size) * 8) + 1, (bbs==1)?LTM_PRIME_BBS:0, cb, dat)

#ifdef __cplusplus
extern "C" {
#endif

#ifndef USE_TCL_STUBS
const char *mp_error_to_string(mp_err code) MP_WUR;
mp_err mp_init(mp_int *a) MP_WUR;
void mp_clear(mp_int *a);
mp_err mp_init_multi(mp_int *mp, ...) MP_NULL_TERMINATED MP_WUR;
void mp_clear_multi(mp_int *mp, ...) MP_NULL_TERMINATED;
void mp_exch(mp_int *a, mp_int *b);
mp_err mp_shrink(mp_int *a) MP_WUR;
mp_err mp_grow(mp_int *a, int size) MP_WUR;
mp_err mp_init_size(mp_int *a, int size) MP_WUR;
void mp_zero(mp_int *a);
mp_bool mp_iseven(const mp_int *a) MP_WUR;
mp_bool mp_isodd(const mp_int *a) MP_WUR;
double mp_get_double(const mp_int *a) MP_WUR;
mp_err mp_set_double(mp_int *a, double b) MP_WUR;
int32_t mp_get_i32(const mp_int *a) MP_WUR;
void mp_set_i32(mp_int *a, int32_t b);
mp_err mp_init_i32(mp_int *a, int32_t b) MP_WUR;
#define mp_get_u32(a) ((uint32_t)mp_get_i32(a))
void mp_set_u32(mp_int *a, uint32_t b);
mp_err mp_init_u32(mp_int *a, uint32_t b) MP_WUR;
int64_t mp_get_i64(const mp_int *a) MP_WUR;
void mp_set_i64(mp_int *a, int64_t b);
mp_err mp_init_i64(mp_int *a, int64_t b) MP_WUR;
#define mp_get_u64(a) ((uint64_t)mp_get_i64(a))
void mp_set_u64(mp_int *a, uint64_t b);
mp_err mp_init_u64(mp_int *a, uint64_t b) MP_WUR;
uint32_t mp_get_mag_u32(const mp_int *a) MP_WUR;
uint64_t mp_get_mag_u64(const mp_int *a) MP_WUR;
unsigned long mp_get_mag_ul(const mp_int *a) MP_WUR;
unsigned long long mp_get_mag_ull(const mp_int *a) MP_WUR;
long mp_get_l(const mp_int *a) MP_WUR;
void mp_set_l(mp_int *a, long b);
mp_err mp_init_l(mp_int *a, long b) MP_WUR;
#define mp_get_ul(a) ((unsigned long)mp_get_l(a))
void mp_set_ul(mp_int *a, unsigned long b);
mp_err mp_init_ul(mp_int *a, unsigned long b) MP_WUR;
long long mp_get_ll(const mp_int *a) MP_WUR;
void mp_set_ll(mp_int *a, long long b);
mp_err mp_init_ll(mp_int *a, long long b) MP_WUR;
#define mp_get_ull(a) ((unsigned long long)mp_get_ll(a))
void mp_set_ull(mp_int *a, unsigned long long b);
mp_err mp_init_ull(mp_int *a, unsigned long long b) MP_WUR;
void mp_set(mp_int *a, mp_digit b);
mp_err mp_init_set(mp_int *a, mp_digit b) MP_WUR;
unsigned long mp_get_int(const mp_int *a) MP_WUR;
unsigned long mp_get_long(const mp_int *a) MP_WUR;
unsigned long long mp_get_long_long(const mp_int *a) MP_WUR;
mp_err mp_set_int(mp_int *a, unsigned long b);
mp_err mp_set_long(mp_int *a, unsigned long b);
mp_err mp_set_long_long(mp_int *a, unsigned long long b);
mp_err mp_init_set_int(mp_int *a, unsigned long b) MP_WUR;
mp_err mp_copy(const mp_int *a, mp_int *b) MP_WUR;
mp_err mp_init_copy(mp_int *a, const mp_int *b) MP_WUR;
void mp_clamp(mp_int *a);
mp_err mp_export(void *rop, size_t *countp, int order, size_t size, int endian, size_t nails, const mp_int *op) MP_WUR;
mp_err mp_import(mp_int *rop, size_t count, int order, size_t size, int endian, size_t nails, const void *op) MP_WUR;
mp_err mp_unpack(mp_int *rop, size_t count, mp_order order, size_t size, mp_endian endian, size_t nails, const void *op) MP_WUR;
size_t mp_pack_count(const mp_int *a, size_t nails, size_t size) MP_WUR;
mp_err mp_pack(void *rop, size_t maxcount, size_t *written, mp_order order, size_t size, mp_endian endian, size_t nails, const mp_int *op) MP_WUR;
void mp_rshd(mp_int *a, int b);
mp_err mp_lshd(mp_int *a, int b) MP_WUR;
mp_err mp_div_2d(const mp_int *a, int b, mp_int *c, mp_int *d) MP_WUR;
mp_err mp_div_2(const mp_int *a, mp_int *b) MP_WUR;
mp_err mp_div_3(const mp_int *a, mp_int *c, mp_digit *d) MP_WUR;
mp_err mp_mul_2d(const mp_int *a, int b, mp_int *c) MP_WUR;
mp_err mp_mul_2(const mp_int *a, mp_int *b) MP_WUR;
mp_err mp_mod_2d(const mp_int *a, int b, mp_int *c) MP_WUR;
mp_err mp_2expt(mp_int *a, int b) MP_WUR;
int mp_cnt_lsb(const mp_int *a) MP_WUR;
mp_err mp_rand(mp_int *a, int digits) MP_WUR;
mp_err mp_rand_digit(mp_digit *r) MP_WUR;
void mp_rand_source(mp_err(*source)(void *out, size_t size));
int mp_get_bit(const mp_int *a, int b) MP_WUR;
mp_err mp_tc_xor(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_xor(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_tc_or(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_or(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_tc_and(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_and(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_complement(const mp_int *a, mp_int *b) MP_WUR;
mp_err mp_tc_div_2d(const mp_int *a, int b, mp_int *c) MP_WUR;
mp_err mp_signed_rsh(const mp_int *a, int b, mp_int *c) MP_WUR;
mp_err mp_neg(const mp_int *a, mp_int *b) MP_WUR;
mp_err mp_abs(const mp_int *a, mp_int *b) MP_WUR;
mp_ord mp_cmp(const mp_int *a, const mp_int *b) MP_WUR;
mp_ord mp_cmp_mag(const mp_int *a, const mp_int *b) MP_WUR;
mp_err mp_add(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_sub(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_mul(const mp_int *a, const mp_int *b, mp_int *p) MP_WUR;
mp_err mp_sqr(const mp_int *a, mp_int *b) MP_WUR;
mp_err mp_div(const mp_int *a, const mp_int *b, mp_int *c, mp_int *d) MP_WUR;
mp_err mp_mod(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_incr(mp_int *a) MP_WUR;
mp_err mp_decr(mp_int *a) MP_WUR;
mp_ord mp_cmp_d(const mp_int *a, mp_digit b) MP_WUR;
mp_err mp_add_d(const mp_int *a, mp_digit b, mp_int *c) MP_WUR;
mp_err mp_sub_d(const mp_int *a, mp_digit b, mp_int *c) MP_WUR;
mp_err mp_mul_d(const mp_int *a, mp_digit b, mp_int *c) MP_WUR;
mp_err mp_div_d(const mp_int *a, mp_digit b, mp_int *c, mp_digit *d) MP_WUR;
mp_err mp_mod_d(const mp_int *a, mp_digit b, mp_digit *c) MP_WUR;
mp_err mp_addmod(const mp_int *a, const mp_int *b, const mp_int *c, mp_int *d) MP_WUR;
mp_err mp_submod(const mp_int *a, const mp_int *b, const mp_int *c, mp_int *d) MP_WUR;
mp_err mp_mulmod(const mp_int *a, const mp_int *b, const mp_int *c, mp_int *d) MP_WUR;
mp_err mp_sqrmod(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_invmod(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_gcd(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_exteuclid(const mp_int *a, const mp_int *b, mp_int *U1, mp_int *U2, mp_int *U3) MP_WUR;
mp_err mp_lcm(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
mp_err mp_root_u32(const mp_int *a, uint32_t b, mp_int *c) MP_WUR;
mp_err mp_n_root(const mp_int *a, mp_digit b, mp_int *c) MP_WUR;
mp_err mp_n_root_ex(const mp_int *a, mp_digit b, mp_int *c, int fast) MP_WUR;
mp_err mp_sqrt(const mp_int *arg, mp_int *ret) MP_WUR;
mp_err mp_sqrtmod_prime(const mp_int *n, const mp_int *prime, mp_int *ret) MP_WUR;
mp_err mp_is_square(const mp_int *arg, mp_bool *ret) MP_WUR;
mp_err mp_jacobi(const mp_int *a, const mp_int *n, int *c) MP_WUR;
mp_err mp_kronecker(const mp_int *a, const mp_int *p, int *c) MP_WUR;
mp_err mp_reduce_setup(mp_int *a, const mp_int *b) MP_WUR;
mp_err mp_reduce(mp_int *x, const mp_int *m, const mp_int *mu) MP_WUR;
mp_err mp_montgomery_setup(const mp_int *n, mp_digit *rho) MP_WUR;
mp_err mp_montgomery_calc_normalization(mp_int *a, const mp_int *b) MP_WUR;
mp_err mp_montgomery_reduce(mp_int *x, const mp_int *n, mp_digit rho) MP_WUR;
mp_bool mp_dr_is_modulus(const mp_int *a) MP_WUR;
void mp_dr_setup(const mp_int *a, mp_digit *d);
mp_err mp_dr_reduce(mp_int *x, const mp_int *n, mp_digit k) MP_WUR;
mp_bool mp_reduce_is_2k(const mp_int *a) MP_WUR;
mp_err mp_reduce_2k_setup(const mp_int *a, mp_digit *d) MP_WUR;
mp_err mp_reduce_2k(mp_int *a, const mp_int *n, mp_digit d) MP_WUR;
mp_bool mp_reduce_is_2k_l(const mp_int *a) MP_WUR;
mp_err mp_reduce_2k_setup_l(const mp_int *a, mp_int *d) MP_WUR;
mp_err mp_reduce_2k_l(mp_int *a, const mp_int *n, const mp_int *d) MP_WUR;
mp_err mp_exptmod(const mp_int *G, const mp_int *X, const mp_int *P, mp_int *Y) MP_WUR;
mp_err mp_prime_is_divisible(const mp_int *a, mp_bool *result) MP_WUR;
mp_err mp_prime_fermat(const mp_int *a, const mp_int *b, mp_bool *result) MP_WUR;
mp_err mp_prime_miller_rabin(const mp_int *a, const mp_int *b, mp_bool *result) MP_WUR;
int mp_prime_rabin_miller_trials(int size) MP_WUR;
mp_err mp_prime_strong_lucas_selfridge(const mp_int *a, mp_bool *result) MP_WUR;
mp_err mp_prime_frobenius_underwood(const mp_int *N, mp_bool *result) MP_WUR;
mp_err mp_prime_is_prime(const mp_int *a, int t, mp_bool *result) MP_WUR;
mp_err mp_prime_next_prime(mp_int *a, int t, int bbs_style) MP_WUR;
mp_err mp_prime_random_ex(mp_int *a, int t, int size, int flags, private_mp_prime_callback cb, void *dat) MP_WUR;
mp_err mp_prime_rand(mp_int *a, int t, int size, int flags) MP_WUR;
mp_err mp_log_u32(const mp_int *a, uint32_t base, uint32_t *c) MP_WUR;
mp_err mp_expt_u32(const mp_int *a, uint32_t b, mp_int *c) MP_WUR;
int mp_count_bits(const mp_int *a) MP_WUR;
int mp_unsigned_bin_size(const mp_int *a) MP_WUR;
mp_err mp_read_unsigned_bin(mp_int *a, const unsigned char *b, int c) MP_WUR;
mp_err mp_to_unsigned_bin(const mp_int *a, unsigned char *b) MP_WUR;
mp_err mp_to_unsigned_bin_n(const mp_int *a, unsigned char *b, unsigned long *outlen) MP_WUR;
int mp_signed_bin_size(const mp_int *a) MP_WUR;
mp_err mp_read_signed_bin(mp_int *a, const unsigned char *b, int c) MP_WUR;
mp_err mp_to_signed_bin(const mp_int *a,  unsigned char *b) MP_WUR;
mp_err mp_to_signed_bin_n(const mp_int *a, unsigned char *b, unsigned long *outlen) MP_WUR;
size_t mp_ubin_size(const mp_int *a) MP_WUR;
mp_err mp_from_ubin(mp_int *a, const unsigned char *buf, size_t size) MP_WUR;
mp_err mp_to_ubin(const mp_int *a, unsigned char *buf, size_t maxlen, size_t *written) MP_WUR;
size_t mp_sbin_size(const mp_int *a) MP_WUR;
mp_err mp_from_sbin(mp_int *a, const unsigned char *buf, size_t size) MP_WUR;
mp_err mp_to_sbin(const mp_int *a, unsigned char *buf, size_t maxlen, size_t *written) MP_WUR;
mp_err mp_read_radix(mp_int *a, const char *str, int radix) MP_WUR;
mp_err mp_toradix(const mp_int *a, char *str, int radix) MP_WUR;
mp_err mp_toradix_n(const mp_int *a, char *str, int radix, int maxlen) MP_WUR;
mp_err mp_to_radix(const mp_int *a, char *str, size_t maxlen, size_t *written, int radix) MP_WUR;
mp_err mp_radix_size(const mp_int *a, int radix, int *size) MP_WUR;
#ifndef MP_NO_FILE
mp_err mp_fread(mp_int *a, int radix, FILE *stream) MP_WUR;
mp_err mp_fwrite(const mp_int *a, int radix, FILE *stream) MP_WUR;
#endif
#endif /* !USE_TCL_STUBS */

#ifdef __cplusplus
}
#endif

#endif /* BN_H_ */
