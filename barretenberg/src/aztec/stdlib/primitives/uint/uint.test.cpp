#include "../bool/bool.hpp"
#include "uint.hpp"
#include <gtest/gtest.h>
#include <numeric/random/engine.hpp>
#include <plonk/composer/turbo_composer.hpp>

using namespace barretenberg;
using namespace plonk;

namespace {
auto& engine = numeric::random::get_debug_engine();
}

namespace test_stdlib_uint32 {
typedef stdlib::bool_t<waffle::TurboComposer> bool_t;
typedef stdlib::uint8<waffle::TurboComposer> uint8;
typedef stdlib::uint32<waffle::TurboComposer> uint32;
typedef stdlib::witness_t<waffle::TurboComposer> witness_t;

std::vector<uint32_t> get_random_ints(size_t num)
{
    std::vector<uint32_t> result;
    for (size_t i = 0; i < num; ++i) {
        result.emplace_back(engine.get_random_uint32());
    }
    return result;
}

TEST(stdlib_uint32, test_create_from_wires)
{
    waffle::TurboComposer composer = waffle::TurboComposer();

    uint8 a = uint8(&composer,
                    std::vector<bool_t>{
                        bool_t(false),
                        bool_t(false),
                        bool_t(false),
                        bool_t(false),
                        bool_t(false),
                        bool_t(false),
                        bool_t(false),
                        witness_t(&composer, true),
                    });

    EXPECT_EQ(a.at(0).get_value(), false);
    EXPECT_EQ(a.at(7).get_value(), true);
    EXPECT_EQ(static_cast<uint32_t>(a.get_value()), 128U);
}

TEST(stdlib_uint32, test_add)
{
    waffle::TurboComposer composer = waffle::TurboComposer();

    witness_t first_input(&composer, 1U);
    witness_t second_input(&composer, 0U);

    uint32 a = first_input;
    uint32 b = second_input;
    uint32 c = a + b;
    for (size_t i = 0; i < 32; ++i) {
        b = a;
        a = c;
        c = a + b;
    }
    waffle::TurboProver prover = composer.create_prover();

    waffle::TurboVerifier verifier = composer.create_verifier();

    waffle::plonk_proof proof = prover.construct_proof();

    bool result = verifier.verify_proof(proof);
    EXPECT_EQ(result, true);
}

TEST(stdlib_uint32, test_add_with_constants)
{
    size_t n = 3;
    std::vector<uint32_t> witnesses = get_random_ints(3 * n);
    uint32_t expected[8];
    for (size_t i = 2; i < n; ++i) {
        expected[0] = witnesses[3 * i];
        expected[1] = witnesses[3 * i + 1];
        expected[2] = witnesses[3 * i + 2];
        expected[3] = expected[0] + expected[1];
        expected[4] = expected[1] + expected[0];
        // expected[5] = expected[1] + expected[2];
        // expected[6] = expected[3] + expected[4];
        // expected[7] = expected[4] + expected[5];
    }
    waffle::TurboComposer composer = waffle::TurboComposer();
    uint32 result[8];
    for (size_t i = 2; i < n; ++i) {
        result[0] = uint32(&composer, witnesses[3 * i]);
        result[1] = (witness_t(&composer, witnesses[3 * i + 1]));
        // result[2] = (witness_t(&composer, witnesses[3 * i + 2]));
        result[3] = result[0] + result[1];
        result[4] = result[1] + result[0];
        // result[5] = result[1] + result[2];
        result[6] = result[3] + result[4];
        // result[7] = result[4] + result[5];
    }

    // for (size_t i = 0; i < 8; ++i) {
    //     EXPECT_EQ(get_value(result[i]), expected[i]);
    // }
    waffle::TurboProver prover = composer.create_prover();

    waffle::TurboVerifier verifier = composer.create_verifier();

    waffle::plonk_proof proof = prover.construct_proof();

    bool proof_valid = verifier.verify_proof(proof);
    EXPECT_EQ(proof_valid, true);
}

TEST(stdlib_uint32, test_mul)
{
    uint32_t a_expected = 1U;
    uint32_t b_expected = 2U;
    uint32_t c_expected = a_expected + b_expected;
    for (size_t i = 0; i < 100; ++i) {
        b_expected = a_expected;
        a_expected = c_expected;
        c_expected = a_expected * b_expected;
    }

    waffle::TurboComposer composer = waffle::TurboComposer();

    witness_t first_input(&composer, 1U);
    witness_t second_input(&composer, 2U);

    uint32 a = first_input;
    uint32 b = second_input;
    uint32 c = a + b;
    for (size_t i = 0; i < 100; ++i) {
        b = a;
        a = c;
        c = a * b;
    }
    uint32_t c_result =
        static_cast<uint32_t>(composer.get_variable(c.get_witness_index()).from_montgomery_form().data[0]);
    EXPECT_EQ(c_result, c_expected);
    waffle::TurboProver prover = composer.create_prover();

    waffle::TurboVerifier verifier = composer.create_verifier();

    waffle::plonk_proof proof = prover.construct_proof();

    bool result = verifier.verify_proof(proof);
    EXPECT_EQ(result, true);
}

TEST(stdlib_uint32, test_xor)
{
    uint32_t a_expected = 0xa3b10422;
    uint32_t b_expected = 0xeac21343;
    uint32_t c_expected = a_expected ^ b_expected;
    for (size_t i = 0; i < 32; ++i) {
        b_expected = a_expected;
        a_expected = c_expected;
        c_expected = a_expected + b_expected;
        a_expected = c_expected ^ a_expected;
    }

    waffle::TurboComposer composer = waffle::TurboComposer();

    witness_t first_input(&composer, 0xa3b10422);
    witness_t second_input(&composer, 0xeac21343);

    uint32 a = first_input;
    uint32 b = second_input;
    uint32 c = a ^ b;
    for (size_t i = 0; i < 32; ++i) {
        b = a;
        a = c;
        c = a + b;
        a = c ^ a;
    }
    uint32_t a_result =
        static_cast<uint32_t>(composer.get_variable(a.get_witness_index()).from_montgomery_form().data[0]);
    EXPECT_EQ(a_result, a_expected);
    waffle::TurboProver prover = composer.create_prover();

    waffle::TurboVerifier verifier = composer.create_verifier();

    waffle::plonk_proof proof = prover.construct_proof();

    bool result = verifier.verify_proof(proof);
    EXPECT_EQ(result, true);
}

TEST(stdlib_uint32, test_xor_constants)
{
    waffle::TurboComposer composer = waffle::TurboComposer();

    uint32_t a_expected = 0xa3b10422;
    uint32_t b_expected = 0xeac21343;
    uint32_t c_expected = a_expected ^ b_expected;

    uint32 const_a(&composer, 0xa3b10422);
    uint32 const_b(&composer, 0xeac21343);
    uint32 c = const_a ^ const_b;
    c.get_witness_index();

    EXPECT_EQ(c.get_additive_constant(), uint256_t(c_expected));
}

TEST(stdlib_uint32, test_xor_more_constants)
{
    uint32_t a_expected = 0xa3b10422;
    uint32_t b_expected = 0xeac21343;
    uint32_t c_expected = a_expected ^ b_expected;
    for (size_t i = 0; i < 1; ++i) {
        b_expected = a_expected;
        a_expected = c_expected;
        c_expected = (a_expected + b_expected) ^ (0xa3b10422 ^ 0xeac21343);
    }

    waffle::TurboComposer composer = waffle::TurboComposer();

    witness_t first_input(&composer, 0xa3b10422);
    witness_t second_input(&composer, 0xeac21343);

    uint32 a = first_input;
    uint32 b = second_input;
    uint32 c = a ^ b;
    for (size_t i = 0; i < 1; ++i) {
        uint32 const_a = 0xa3b10422;
        uint32 const_b = 0xeac21343;
        b = a;
        a = c;
        c = (a + b) ^ (const_a ^ const_b);
    }
    uint32_t c_witness_index = c.get_witness_index();
    uint32_t c_result = static_cast<uint32_t>(composer.get_variable(c_witness_index).from_montgomery_form().data[0]);
    EXPECT_EQ(c_result, c_expected);
    waffle::TurboProver prover = composer.create_prover();

    waffle::TurboVerifier verifier = composer.create_verifier();

    waffle::plonk_proof proof = prover.construct_proof();

    bool result = verifier.verify_proof(proof);
    EXPECT_EQ(result, true);
}

TEST(stdlib_uint32, test_and_constants)
{
    uint32_t a_expected = 0xa3b10422;
    uint32_t b_expected = 0xeac21343;
    uint32_t c_expected = a_expected & b_expected;
    for (size_t i = 0; i < 1; ++i) {
        b_expected = a_expected;
        a_expected = c_expected;
        c_expected = (~a_expected & 0xa3b10422) + (b_expected & 0xeac21343);
        // c_expected = (a_expected + b_expected) & (0xa3b10422 & 0xeac21343);
    }

    waffle::TurboComposer composer = waffle::TurboComposer();

    witness_t first_input(&composer, 0xa3b10422);
    witness_t second_input(&composer, 0xeac21343);

    uint32 a = first_input;
    uint32 b = second_input;
    uint32 c = a & b;
    for (size_t i = 0; i < 1; ++i) {
        uint32 const_a = 0xa3b10422;
        uint32 const_b = 0xeac21343;
        b = a;
        a = c;
        c = (~a & const_a) + (b & const_b);
    }
    uint32_t c_witness_index = c.get_witness_index();
    uint32_t c_result = static_cast<uint32_t>(composer.get_variable(c_witness_index).from_montgomery_form().data[0]);
    EXPECT_EQ(c_result, c_expected);
    waffle::TurboProver prover = composer.create_prover();

    waffle::TurboVerifier verifier = composer.create_verifier();

    waffle::plonk_proof proof = prover.construct_proof();

    bool result = verifier.verify_proof(proof);
    EXPECT_EQ(result, true);
}

TEST(stdlib_uint32s, test_and)
{
    uint32_t a_expected = 0xa3b10422;
    uint32_t b_expected = 0xeac21343;
    uint32_t c_expected = a_expected + b_expected;
    for (size_t i = 0; i < 32; ++i) {
        b_expected = a_expected;
        a_expected = c_expected;
        c_expected = a_expected + b_expected;
        a_expected = c_expected & a_expected;
    }

    waffle::TurboComposer composer = waffle::TurboComposer();

    witness_t first_input(&composer, 0xa3b10422);
    witness_t second_input(&composer, 0xeac21343);

    uint32 a = first_input;
    uint32 b = second_input;
    uint32 c = a + b;
    for (size_t i = 0; i < 32; ++i) {
        b = a;
        a = c;
        c = a + b;
        a = c & a;
    }
    uint32_t a_result =
        static_cast<uint32_t>(composer.get_variable(a.get_witness_index()).from_montgomery_form().data[0]);
    EXPECT_EQ(a_result, a_expected);

    waffle::TurboProver prover = composer.create_prover();

    waffle::TurboVerifier verifier = composer.create_verifier();

    waffle::plonk_proof proof = prover.construct_proof();

    bool result = verifier.verify_proof(proof);
    EXPECT_EQ(result, true);
}

TEST(stdlib_uint32, test_or)
{
    uint32_t a_expected = 0xa3b10422;
    uint32_t b_expected = 0xeac21343;
    uint32_t c_expected = a_expected ^ b_expected;
    for (size_t i = 0; i < 32; ++i) {
        b_expected = a_expected;
        a_expected = c_expected;
        c_expected = a_expected + b_expected;
        a_expected = c_expected | a_expected;
    }

    waffle::TurboComposer composer = waffle::TurboComposer();

    witness_t first_input(&composer, 0xa3b10422);
    witness_t second_input(&composer, 0xeac21343);

    uint32 a = first_input;
    uint32 b = second_input;
    uint32 c = a ^ b;
    for (size_t i = 0; i < 32; ++i) {
        b = a;
        a = c;
        c = a + b;
        a = c | a;
    }
    uint32_t a_result =
        static_cast<uint32_t>(composer.get_variable(a.get_witness_index()).from_montgomery_form().data[0]);
    EXPECT_EQ(a_result, a_expected);

    waffle::TurboProver prover = composer.create_prover();

    waffle::TurboVerifier verifier = composer.create_verifier();

    waffle::plonk_proof proof = prover.construct_proof();

    bool result = verifier.verify_proof(proof);
    EXPECT_EQ(result, true);
}

TEST(stdlib_uint32, test_gt)
{
    const auto run_test = [](bool lhs_constant, bool rhs_constant, int type = 0) {
        uint32_t a_expected = 0xa3b10422;
        uint32_t b_expected;
        switch (type) {
        case 0: {
            b_expected = 0xbac21343; // a < b
            break;
        }
        case 1: {
            b_expected = 0x000affe2; // a > b
            break;
        }
        case 2: {
            b_expected = 0xa3b10422; // a = b
            break;
        }
        default: {
            b_expected = 0xbac21343; // a < b
        }
        }
        bool c_expected = a_expected > b_expected;

        waffle::TurboComposer composer = waffle::TurboComposer();

        uint32 a;
        uint32 b;
        if (lhs_constant) {
            a = uint32(nullptr, a_expected);
        } else {
            a = witness_t(&composer, a_expected);
        }
        if (rhs_constant) {
            b = uint32(nullptr, b_expected);
        } else {
            b = witness_t(&composer, b_expected);
        }
        // mix in some constant terms for good measure
        a *= uint32(&composer, 2);
        a += uint32(&composer, 0x00112233);
        b *= uint32(&composer, 2);
        b += uint32(&composer, 0x00112233);

        bool_t c = a > b;

        bool c_result = static_cast<bool>(c.get_value());
        EXPECT_EQ(c_result, c_expected);

        waffle::TurboProver prover = composer.create_prover();

        waffle::TurboVerifier verifier = composer.create_verifier();

        waffle::plonk_proof proof = prover.construct_proof();

        bool result = verifier.verify_proof(proof);
        EXPECT_EQ(result, true);
    };

    run_test(false, false, 0);
    run_test(false, true, 0);
    run_test(true, false, 0);
    run_test(true, true, 0);
    run_test(false, false, 1);
    run_test(false, true, 1);
    run_test(true, false, 1);
    run_test(true, true, 1);
    run_test(false, false, 2);
    run_test(false, true, 2);
    run_test(true, false, 2);
    run_test(true, true, 2);
}

uint32_t rotate(uint32_t value, size_t rotation)
{
    return rotation ? (value >> rotation) + (value << (32 - rotation)) : value;
}

TEST(stdlib_uint32, test_ror)
{
    uint32_t a_expected = 0xa3b10422;
    uint32_t b_expected = 0xeac21343;
    uint32_t c_expected = a_expected ^ b_expected;
    for (size_t i = 0; i < 32; ++i) {
        b_expected = a_expected;
        a_expected = c_expected;
        c_expected = a_expected + b_expected;
        a_expected = rotate(c_expected, i % 31) + rotate(a_expected, (i + 1) % 31);
    }

    waffle::TurboComposer composer = waffle::TurboComposer();

    witness_t first_input(&composer, 0xa3b10422);
    witness_t second_input(&composer, 0xeac21343);

    uint32 a = first_input;
    uint32 b = second_input;
    uint32 c = a ^ b;
    for (size_t i = 0; i < 32; ++i) {
        b = a;
        a = c;
        c = a + b;
        a = c.ror(static_cast<uint32_t>(i % 31)) + a.ror(static_cast<uint32_t>((i + 1) % 31));
    }
    uint32_t a_result =
        static_cast<uint32_t>(composer.get_variable(a.get_witness_index()).from_montgomery_form().data[0]);
    EXPECT_EQ(a_result, a_expected);

    waffle::TurboProver prover = composer.create_prover();

    waffle::TurboVerifier verifier = composer.create_verifier();

    waffle::plonk_proof proof = prover.construct_proof();

    bool result = verifier.verify_proof(proof);
    EXPECT_EQ(result, true);
}

uint32_t k_constants[64]{ 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
                          0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
                          0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
                          0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
                          0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
                          0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
                          0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
                          0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                          0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
                          0xc67178f2 };
uint32_t round_values[8]{
    0x01020304, 0x0a0b0c0d, 0x1a2b3e4d, 0x03951bd3, 0x0e0fa3fe, 0x01000000, 0x0f0eeea1, 0x12345678
};

// subtraction
// A - B
// = 2^{32} - B + A
// ...but only if constants are zero
// can also do (2^{32} - B + A) + (2^{32} - B.const)
// ...but what about multiplicative value? Um...erm...
TEST(stdlib_uint32, test_hash_rounds)
{
    uint32_t w_alt[64];

    for (size_t i = 0; i < 64; ++i) {
        w_alt[i] = static_cast<uint32_t>(barretenberg::fr::random_element().data[0]);
    }
    uint32_t a_alt = round_values[0];
    uint32_t b_alt = round_values[1];
    uint32_t c_alt = round_values[2];
    uint32_t d_alt = round_values[3];
    uint32_t e_alt = round_values[4];
    uint32_t f_alt = round_values[5];
    uint32_t g_alt = round_values[6];
    uint32_t h_alt = round_values[7];
    for (size_t i = 0; i < 64; ++i) {
        uint32_t S1_alt = rotate(e_alt, 7) ^ rotate(e_alt, 11) ^ rotate(e_alt, 25);
        uint32_t ch_alt = (e_alt & f_alt) ^ ((~e_alt) & g_alt);
        uint32_t temp1_alt = h_alt + S1_alt + ch_alt + k_constants[i % 64] + w_alt[i];

        uint32_t S0_alt = rotate(a_alt, 2) ^ rotate(a_alt, 13) ^ rotate(a_alt, 22);
        uint32_t maj_alt = (a_alt & b_alt) ^ (a_alt & c_alt) ^ (b_alt & c_alt);
        uint32_t temp2_alt = S0_alt + maj_alt;

        h_alt = g_alt;
        g_alt = f_alt;
        f_alt = e_alt;
        e_alt = d_alt + temp1_alt;
        d_alt = c_alt;
        c_alt = b_alt;
        b_alt = a_alt;
        a_alt = temp1_alt + temp2_alt;
    }
    waffle::TurboComposer composer = waffle::TurboComposer();

    std::vector<uint32> w;
    std::vector<uint32> k;
    for (size_t i = 0; i < 64; ++i) {
        w.emplace_back(uint32(witness_t(&composer, w_alt[i])));
        k.emplace_back(uint32(&composer, k_constants[i % 64]));
    }
    uint32 a = witness_t(&composer, round_values[0]);
    uint32 b = witness_t(&composer, round_values[1]);
    uint32 c = witness_t(&composer, round_values[2]);
    uint32 d = witness_t(&composer, round_values[3]);
    uint32 e = witness_t(&composer, round_values[4]);
    uint32 f = witness_t(&composer, round_values[5]);
    uint32 g = witness_t(&composer, round_values[6]);
    uint32 h = witness_t(&composer, round_values[7]);
    for (size_t i = 0; i < 64; ++i) {
        uint32 S1 = e.ror(7U) ^ e.ror(11U) ^ e.ror(25U);
        uint32 ch = (e & f) + ((~e) & g);
        uint32 temp1 = h + S1 + ch + k[i] + w[i];

        uint32 S0 = a.ror(2U) ^ a.ror(13U) ^ a.ror(22U);
        uint32 T0 = (b & c);
        uint32 T1 = (b - T0) + (c - T0);
        uint32 T2 = a & T1;
        uint32 maj = T2 + T0;
        uint32 temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    uint32_t a_result =
        static_cast<uint32_t>(composer.get_variable(a.get_witness_index()).from_montgomery_form().data[0]);
    uint32_t b_result =
        static_cast<uint32_t>(composer.get_variable(b.get_witness_index()).from_montgomery_form().data[0]);
    uint32_t c_result =
        static_cast<uint32_t>(composer.get_variable(c.get_witness_index()).from_montgomery_form().data[0]);
    uint32_t d_result =
        static_cast<uint32_t>(composer.get_variable(d.get_witness_index()).from_montgomery_form().data[0]);
    uint32_t e_result =
        static_cast<uint32_t>(composer.get_variable(e.get_witness_index()).from_montgomery_form().data[0]);
    uint32_t f_result =
        static_cast<uint32_t>(composer.get_variable(f.get_witness_index()).from_montgomery_form().data[0]);
    uint32_t g_result =
        static_cast<uint32_t>(composer.get_variable(g.get_witness_index()).from_montgomery_form().data[0]);
    uint32_t h_result =
        static_cast<uint32_t>(composer.get_variable(h.get_witness_index()).from_montgomery_form().data[0]);

    EXPECT_EQ(a_result, a_alt);
    EXPECT_EQ(b_result, b_alt);
    EXPECT_EQ(c_result, c_alt);
    EXPECT_EQ(d_result, d_alt);
    EXPECT_EQ(e_result, e_alt);
    EXPECT_EQ(f_result, f_alt);
    EXPECT_EQ(g_result, g_alt);
    EXPECT_EQ(h_result, h_alt);

    waffle::TurboProver prover = composer.create_prover();

    waffle::TurboVerifier verifier = composer.create_verifier();

    waffle::plonk_proof proof = prover.construct_proof();

    bool result = verifier.verify_proof(proof);
    EXPECT_EQ(result, true);
}

/**
 * @brief Make sure we prevent proving v / v = 0 by setting the divison remainder to be v.
 * TODO: This is lifted from the implementation. Should rewrite this test after introducing framework that separates
 * circuit construction from witness generation.

 */
TEST(stdlib_uint32, div_remainder_constraint)
{
    size_t width = 32; // will go away in refactor part of audit.
    waffle::TurboComposer composer = waffle::TurboComposer();

    uint32_t val = engine.get_random_uint32(); // will change in refactor part of audit

    uint32 a = witness_t(&composer, val);
    uint32 b = witness_t(&composer, val);

    const uint32_t dividend_idx = a.get_witness_index();
    const uint32_t divisor_idx = b.get_witness_index();

    const uint256_t divisor = b.get_value();

    const uint256_t q = 0;
    const uint256_t r = val;

    const uint32_t quotient_idx = composer.add_variable(q);
    const uint32_t remainder_idx = composer.add_variable(r);

    // In this example there are no additive constaints, so we just replace them by zero below.

    // constraint: qb + const_b q + 0 b - a + r - const_a == 0
    // i.e., a + const_a = q(b + const_b) + r
    const waffle::mul_quad division_gate{ .a = quotient_idx,  // q
                                          .b = divisor_idx,   // b
                                          .c = dividend_idx,  // a
                                          .d = remainder_idx, // r
                                          .mul_scaling = fr::one(),
                                          .a_scaling = b.get_additive_constant(),
                                          .b_scaling = fr::zero(),
                                          .c_scaling = fr::neg_one(),
                                          .d_scaling = fr::one(),
                                          .const_scaling = -a.get_additive_constant() };
    composer.create_big_mul_gate(division_gate);

    // set delta = (b + const_b - r)

    // constraint: b - r - delta + const_b == 0
    const uint256_t delta = divisor - r - 1;
    const uint32_t delta_idx = composer.add_variable(delta);

    const waffle::add_triple delta_gate{
        .a = divisor_idx,   // b
        .b = remainder_idx, // r
        .c = delta_idx,     // d
        .a_scaling = fr::one(),
        .b_scaling = fr::neg_one(),
        .c_scaling = fr::neg_one(),
        .const_scaling = b.get_additive_constant(),
    };

    composer.create_add_gate(delta_gate);

    // validate delta is in the correct range
    stdlib::field_t<waffle::TurboComposer>::from_witness_index(&composer, delta_idx).create_range_constraint(width);

    // normalize witness quotient and remainder
    // minimal bit range for quotient: from 0 (in case a = b-1) to width (when b = 1).
    uint32 quotient(&composer);
    composer.decompose_into_base4_accumulators(quotient_idx, width);

    // constrain remainder to lie in [0, 2^width-1]
    uint32 remainder(&composer);
    composer.decompose_into_base4_accumulators(remainder_idx, width);

    auto prover = composer.create_prover();
    auto verifier = composer.create_verifier();
    waffle::plonk_proof proof = prover.construct_proof();
    bool result = verifier.verify_proof(proof);
    EXPECT_EQ(result, false);
}

} // namespace test_stdlib_uint32