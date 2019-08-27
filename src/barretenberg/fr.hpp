#pragma once

#include <stdint.h>
#include <stdio.h>

#include "types.hpp"

#ifdef NO_FUNNY_BUSINESS 
    #include "fr_impl_int128.hpp"
#else
    #include "fr_impl_asm.hpp"
#endif

namespace fr
{
constexpr field_t r_squared = {.data = {
    0x1BB8E645AE216DA7UL,
    0x53FE3AB1E35C59E3UL,
    0x8C49833D53BB8085UL,
    0x216D0B17F4E44A5UL}};

// lambda = curve root of unity modulo n, converted to montgomery form
constexpr field_t lambda = {.data = {
    0x93e7cede4a0329b3UL,
    0x7d4fdca77a96c167UL,
    0x8be4ba08b19a750aUL,
    0x1cbd5653a5661c25UL}};

constexpr field_t modulus = {.data = {
    0x43E1F593F0000001UL,
    0x2833E84879B97091UL,
    0xB85045B68181585DUL,
    0x30644E72E131A029UL}};

constexpr field_t modulus_plus_one = {.data = {
    0x43E1F593F0000002UL,
    0x2833E84879B97091UL,
    0xB85045B68181585DUL,
    0x30644E72E131A029UL}};

constexpr field_t one_raw = {.data = {1, 0, 0, 0}};

// compute a * b mod p, put result in r
inline void mul(const field_t &a, const field_t &b, field_t &r);

// compute a * b, put 512-bit result in r
inline void mul_512(const field_t &a, const field_t &b, const field_wide_t& r);

// compute a * a, put result in r
inline void sqr(const field_t &a, field_t &r);

// compute a + b, put result in r
inline void add(const field_t &a, const field_t &b, field_t &r);

// compute a - b, put result in r
inline void sub(const field_t &a, const field_t &b, field_t &r);

/**
 * copy src into dest. AVX implementation requires words to be aligned on 32 byte bounary
 **/ 
inline void copy(const field_t& src, field_t& dest);


inline bool gt(field_t& a, const field_t& b)
{
    bool t0 = a.data[3] > b.data[3];
    bool t1 = (a.data[3] == b.data[3]) && (a.data[2] > b.data[2]);
    bool t2 = (a.data[3] == b.data[3]) && (a.data[2] == b.data[2]) && (a.data[1] > b.data[1]);
    bool t3 = (a.data[3] == b.data[3]) && (a.data[2] == b.data[2]) && (a.data[1] == b.data[1]) && (a.data[0] > b.data[0]);
    return (t0 || t1 || t2 || t3);
}

/**
 * For short Weierstrass curves y^2 = x^3 + b mod r, if there exists a cube root of unity mod r,
 * we can take advantage of an enodmorphism to decompose a 254 bit scalar into 2 128 bit scalars.
 * \beta = cube root of 1, mod q (q = order of fq)
 * \lambda = cube root of 1, mod r (r = order of fr)
 * 
 * For a point P1 = (X, Y), where Y^2 = X^3 + b, we know that
 * the point P2 = (X * \beta, Y) is also a point on the curve
 * We can represent P2 as a scalar multiplication of P1, where P2 = \lambda * P1
 * 
 * For a generic multiplication of P1 by a 254 bit scalar k, we can decompose k
 * into 2 127 bit scalars (k1, k2), such that k = k1 - (k2 * \lambda) 
 * 
 * We can now represent (k * P1) as (k1 * P1) - (k2 * P2), where P2 = (X * \beta, Y).
 * As k1, k2 have half the bit length of k, we have reduced the number of loop iterations of our
 * scalar multiplication algorithm in half
 * 
 * To find k1, k2, We use the extended euclidean algorithm to find 4 short scalars [a1, a2], [b1, b2] such that
 * modulus = (a1 * b2) - (b1 * a2)
 * We then compube scalars c1 = round(b2 * k / r), c2 = round(b1 * k / r), where
 * k1 = (c1 * a1) + (c2 * a2), k2 = -((c1 * b1) + (c2 * b2))
 * We pre-compute scalars g1 = (2^256 * b1) / n, g2 = (2^256 * b2) / n, to avoid having to perform long division
 * on 512-bit scalars
 **/
inline void split_into_endomorphism_scalars(field_t &k, field_t &k1, field_t &k2)
{
    // uint64_t lambda_reduction[4] = { 0 };
    // to_montgomery_form(lambda, lambda_reduction);

    constexpr field_t g1 = { .data = {
        0x7a7bd9d4391eb18dUL,
        0x4ccef014a773d2cfUL,
        0x0000000000000002UL,
        0}};

    constexpr field_t g2 = { .data = {
        0xd91d232ec7e0b3d7UL,
        0x0000000000000002UL,
        0,
        0}};

    constexpr field_t minus_b1 = { .data = {
        0x8211bbeb7d4f1128UL,
        0x6f4d8248eeb859fcUL,
        0,
        0}};

    constexpr field_t b2 = { .data = {
        0x89d3256894d213e3UL,
        0,
        0,
        0}};

    field_wide_t c1;
    field_wide_t c2;

    // compute c1 = (g2 * k) >> 256
    mul_512(g2, k, c1);
    // compute c2 = (g1 * k) >> 256
    mul_512(g1, k, c2);
    // (the bit shifts are implicit, as we only utilize the high limbs of c1, c2

    field_wide_t q1;
    field_wide_t q2;
    // TODO remove data duplication
    field_t c1_hi = { .data = { c1.data[4], c1.data[5], c1.data[6], c1.data[7] }}; // *(field_t*)((uintptr_t)(&c1) + (4 * sizeof(uint64_t)));
    field_t c2_hi = { .data = { c2.data[4], c2.data[5], c2.data[6], c2.data[7] }}; // *(field_t*)((uintptr_t)(&c2) + (4 * sizeof(uint64_t)));

    // compute q1 = c1 * -b1
    mul_512(c1_hi, minus_b1, q1);
    // compute q2 = c2 * b2
    mul_512(c2_hi, b2, q2);

    field_t t1 = { .data = { 0, 0, 0, 0, }};
    field_t t2 = { .data = { 0, 0, 0, 0, }};
    // TODO: this doesn't have to be a 512-bit multiply...
    field_t q1_lo = { .data = { q1.data[0], q1.data[1], q1.data[2], q1.data[3] }}; // *(field_t*)((uintptr_t)(&q1) + (4 * sizeof(uint64_t)));
    field_t q2_lo = { .data = { q2.data[0], q2.data[1], q2.data[2], q2.data[3] }}; // *(field_t*)((uintptr_t)(&q2) + (4 * sizeof(uint64_t)));

    sub(q2_lo, q1_lo, t1);

    // to_montgomery_form(t1, t1);
    mul(t1, lambda, t2);
    // from_montgomery_form(t2, t2);
    add(k, t2, t2);

    k2.data[0] = t1.data[0];
    k2.data[1] = t1.data[1];
    k1.data[0] = t2.data[0];
    k1.data[1] = t2.data[1];
}

inline void normalize(field_t& a, field_t& r)
{
    r.data[0] = a.data[0];
    r.data[1] = a.data[1];
    r.data[2] = a.data[2];
    r.data[3] = a.data[3];
    while (gt(r, modulus_plus_one))
    {
        sub(r, modulus, r);
    }
}

inline void mul_lambda(field_t &a, field_t &r)
{
    mul(a, lambda, r);
}

/**
     * Negate field_t element `a`, mod `q`, place result in `r`
     **/
inline void neg(const field_t &a, field_t &r)
{
    sub(modulus, a, r);
}

/**
     * Convert a field element into montgomery form
     **/
inline void to_montgomery_form(const field_t &a, field_t &r)
{
    copy(a, r);
    while (gt(r, modulus_plus_one))
    {
        sub(r, modulus, r);
    }
    mul(r, r_squared, r);
}

/**
     * Convert a field element out of montgomery form by performing a modular
     * reduction against 1
     **/
inline void from_montgomery_form(const field_t &a, field_t &r)
{
    mul(a, one_raw, r);
    // while (gt(r, modulus_plus_one))
    // {
    //     sub(r, modulus, r);
    // }
}

/**
     * Get a random field element in montgomery form, place in `r`
     **/
inline void random_element(field_t &r)
{
    int got_entropy = getentropy((void *)r.data, 32);
    ASSERT(got_entropy == 0);
    to_montgomery_form(r, r);
}

/**
     * Set `r` to equal 1, in montgomery form
     **/
inline void one(field_t &r)
{
    to_montgomery_form(one_raw, r);
}

inline bool eq(field_t &a, field_t &b)
{
    return (a.data[0] == b.data[0]) && (a.data[1] == b.data[1]) && (a.data[2] == b.data[2]) && (a.data[3] == b.data[3]);
}

inline void print(field_t &a)
{
    printf("fr: [%lx, %lx, %lx, %lx]\n", a.data[0], a.data[1], a.data[2], a.data[3]);
}
} // namespace fr