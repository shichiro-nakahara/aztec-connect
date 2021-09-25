#include <gtest/gtest.h>

#include <numeric/random/engine.hpp>

#include <ecc/curves/bn254/fq.hpp>
#include <ecc/curves/bn254/fr.hpp>

#include "../bool/bool.hpp"
#include "../byte_array/byte_array.hpp"
#include "../field/field.hpp"
#include "./bigfield.hpp"

#include <stdlib/primitives/curves/bn254.hpp>
#include <plonk/proof_system/prover/prover.hpp>
#include <plonk/proof_system/verifier/verifier.hpp>

#include <memory>
#include <polynomials/polynomial_arithmetic.hpp>

namespace test_stdlib_bigfield {
using namespace barretenberg;
using namespace plonk;

/* A note regarding Plookup:
   stdlib_bigfield_plookup tests were present when this file was standardized
   to be more proving system-agnostic. Those tests are commented out  below, but modified
   in the following ways:
     - pbigfield_t was replaced by bn254::fq_ct;
     - pwitness_t  was replaced by bn254::witness_ct.
*/

namespace {
auto& engine = numeric::random::get_debug_engine();
}

template <typename Composer> class StdlibBigfield : public testing::Test {
    typedef stdlib::bn254<Composer> bn254;

    typedef typename bn254::fr_ct fr_ct;
    typedef typename bn254::fq_ct fq_ct;
    typedef typename bn254::public_witness_ct public_witness_ct;
    typedef typename bn254::witness_ct witness_ct;

  public:
    static void test_mul()
    {
        auto composer = Composer();
        size_t num_repetitions = 1;
        for (size_t i = 0; i < num_repetitions; ++i) {
            fq inputs[3]{ fq::random_element(), fq::random_element(), fq::random_element() };
            fq_ct a(witness_ct(&composer, fr(uint256_t(inputs[0]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct b(witness_ct(&composer, fr(uint256_t(inputs[1]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[1]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            uint64_t before = composer.get_num_gates();
            fq_ct c = a * b;
            uint64_t after = composer.get_num_gates();
            if (i == num_repetitions - 1) {
                std::cout << "num gates per mul = " << after - before << std::endl;
            }
            // uint256_t modulus{ Bn254FqParams::modulus_0,
            //                    Bn254FqParams::modulus_1,
            //                    Bn254FqParams::modulus_2,
            //                    Bn254FqParams::modulus_3 };

            fq expected = (inputs[0] * inputs[1]);
            expected = expected.from_montgomery_form();
            uint512_t result = c.get_value();

            EXPECT_EQ(result.lo.data[0], expected.data[0]);
            EXPECT_EQ(result.lo.data[1], expected.data[1]);
            EXPECT_EQ(result.lo.data[2], expected.data[2]);
            EXPECT_EQ(result.lo.data[3], expected.data[3]);
            EXPECT_EQ(result.hi.data[0], 0ULL);
            EXPECT_EQ(result.hi.data[1], 0ULL);
            EXPECT_EQ(result.hi.data[2], 0ULL);
            EXPECT_EQ(result.hi.data[3], 0ULL);
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_sqr()
    {
        auto composer = Composer();
        size_t num_repetitions = 10;
        for (size_t i = 0; i < num_repetitions; ++i) {
            fq inputs[3]{ fq::random_element(), fq::random_element(), fq::random_element() };
            fq_ct a(witness_ct(&composer, fr(uint256_t(inputs[0]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            uint64_t before = composer.get_num_gates();
            fq_ct c = a.sqr();
            uint64_t after = composer.get_num_gates();
            if (i == num_repetitions - 1) {
                std::cout << "num gates per mul = " << after - before << std::endl;
            }
            // uint256_t modulus{ Bn254FqParams::modulus_0,
            //                    Bn254FqParams::modulus_1,
            //                    Bn254FqParams::modulus_2,
            //                    Bn254FqParams::modulus_3 };

            fq expected = (inputs[0].sqr());
            expected = expected.from_montgomery_form();
            uint512_t result = c.get_value();

            EXPECT_EQ(result.lo.data[0], expected.data[0]);
            EXPECT_EQ(result.lo.data[1], expected.data[1]);
            EXPECT_EQ(result.lo.data[2], expected.data[2]);
            EXPECT_EQ(result.lo.data[3], expected.data[3]);
            EXPECT_EQ(result.hi.data[0], 0ULL);
            EXPECT_EQ(result.hi.data[1], 0ULL);
            EXPECT_EQ(result.hi.data[2], 0ULL);
            EXPECT_EQ(result.hi.data[3], 0ULL);
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_madd()
    {
        auto composer = Composer();
        size_t num_repetitions = 1;
        for (size_t i = 0; i < num_repetitions; ++i) {
            fq inputs[3]{ fq::random_element(), fq::random_element(), fq::random_element() };
            fq_ct a(witness_ct(&composer, fr(uint256_t(inputs[0]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct b(witness_ct(&composer, fr(uint256_t(inputs[1]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[1]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));

            fq_ct c(witness_ct(&composer, fr(uint256_t(inputs[2]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[2]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));

            uint64_t before = composer.get_num_gates();
            fq_ct d = a.madd(b, { c });
            uint64_t after = composer.get_num_gates();
            if (i == num_repetitions - 1) {
                std::cout << "num gates per mul = " << after - before << std::endl;
            }
            // uint256_t modulus{ Bn254FqParams::modulus_0,
            //                    Bn254FqParams::modulus_1,
            //                    Bn254FqParams::modulus_2,
            //                    Bn254FqParams::modulus_3 };

            fq expected = (inputs[0] * inputs[1]) + inputs[2];
            expected = expected.from_montgomery_form();
            uint512_t result = d.get_value();

            EXPECT_EQ(result.lo.data[0], expected.data[0]);
            EXPECT_EQ(result.lo.data[1], expected.data[1]);
            EXPECT_EQ(result.lo.data[2], expected.data[2]);
            EXPECT_EQ(result.lo.data[3], expected.data[3]);
            EXPECT_EQ(result.hi.data[0], 0ULL);
            EXPECT_EQ(result.hi.data[1], 0ULL);
            EXPECT_EQ(result.hi.data[2], 0ULL);
            EXPECT_EQ(result.hi.data[3], 0ULL);
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_dual_madd()
    {
        auto composer = Composer();
        size_t num_repetitions = 1;
        for (size_t i = 0; i < num_repetitions; ++i) {
            fq inputs[5]{ -1, -2, -3, -4, -5 };
            fq_ct a(witness_ct(&composer, fr(uint256_t(inputs[0]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct b(witness_ct(&composer, fr(uint256_t(inputs[1]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[1]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));

            fq_ct c(witness_ct(&composer, fr(uint256_t(inputs[2]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[2]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));

            fq_ct d(witness_ct(&composer, fr(uint256_t(inputs[3]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[3]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct e(witness_ct(&composer, fr(uint256_t(inputs[4]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[4]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));

            uint64_t before = composer.get_num_gates();
            typename fq_ct::cached_product temp;
            fq_ct f = fq_ct::dual_madd(a, b, c, d, { e }, temp);
            uint64_t after = composer.get_num_gates();
            if (i == num_repetitions - 1) {
                std::cout << "num gates per mul = " << after - before << std::endl;
            }
            // uint256_t modulus{ Bn254FqParams::modulus_0,
            //                    Bn254FqParams::modulus_1,
            //                    Bn254FqParams::modulus_2,
            //                    Bn254FqParams::modulus_3 };

            fq expected = (inputs[0] * inputs[1]) + (inputs[2] * inputs[3]) + inputs[4];
            expected = expected.from_montgomery_form();
            uint512_t result = f.get_value();

            EXPECT_EQ(result.lo.data[0], expected.data[0]);
            EXPECT_EQ(result.lo.data[1], expected.data[1]);
            EXPECT_EQ(result.lo.data[2], expected.data[2]);
            EXPECT_EQ(result.lo.data[3], expected.data[3]);
            EXPECT_EQ(result.hi.data[0], 0ULL);
            EXPECT_EQ(result.hi.data[1], 0ULL);
            EXPECT_EQ(result.hi.data[2], 0ULL);
            EXPECT_EQ(result.hi.data[3], 0ULL);
        }
        std::cout << "composer failed + err = " << composer.failed << " , " << composer.err << std::endl;
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_div()
    {
        auto composer = Composer();
        size_t num_repetitions = 10;
        for (size_t i = 0; i < num_repetitions; ++i) {
            fq inputs[3]{ fq::random_element(), fq::random_element(), fq::random_element() };
            inputs[0] = inputs[0].reduce_once().reduce_once();
            inputs[1] = inputs[1].reduce_once().reduce_once();
            fq_ct a(witness_ct(&composer, fr(uint256_t(inputs[0]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct b(witness_ct(&composer, fr(uint256_t(inputs[1]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[1]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct c = a / b;
            // uint256_t modulus{ Bn254FqParams::modulus_0,
            //                    Bn254FqParams::modulus_1,
            //                    Bn254FqParams::modulus_2,
            //                    Bn254FqParams::modulus_3 };

            fq expected = (inputs[0] / inputs[1]);
            expected = expected.reduce_once().reduce_once();
            expected = expected.from_montgomery_form();
            uint512_t result = c.get_value();

            EXPECT_EQ(result.lo.data[0], expected.data[0]);
            EXPECT_EQ(result.lo.data[1], expected.data[1]);
            EXPECT_EQ(result.lo.data[2], expected.data[2]);
            EXPECT_EQ(result.lo.data[3], expected.data[3]);
            EXPECT_EQ(result.hi.data[0], 0ULL);
            EXPECT_EQ(result.hi.data[1], 0ULL);
            EXPECT_EQ(result.hi.data[2], 0ULL);
            EXPECT_EQ(result.hi.data[3], 0ULL);
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_add_and_div()
    {
        auto composer = Composer();
        size_t num_repetitions = 1;
        for (size_t i = 0; i < num_repetitions; ++i) {
            fq inputs[4]{ fq::random_element(), fq::random_element(), fq::random_element() };
            fq_ct a(witness_ct(&composer, fr(uint256_t(inputs[0]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct b(witness_ct(&composer, fr(uint256_t(inputs[1]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[1]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct c(witness_ct(&composer, fr(uint256_t(inputs[2]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[2]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct d(witness_ct(&composer, fr(uint256_t(inputs[3]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[3]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct e = (a + b) / (c + d);
            // uint256_t modulus{ Bn254FqParams::modulus_0,
            //                    Bn254FqParams::modulus_1,
            //                    Bn254FqParams::modulus_2,
            //                    Bn254FqParams::modulus_3 };

            fq expected = (inputs[0] + inputs[1]) / (inputs[2] + inputs[3]);
            expected = expected.reduce_once().reduce_once();
            expected = expected.from_montgomery_form();
            uint512_t result = e.get_value();

            EXPECT_EQ(result.lo.data[0], expected.data[0]);
            EXPECT_EQ(result.lo.data[1], expected.data[1]);
            EXPECT_EQ(result.lo.data[2], expected.data[2]);
            EXPECT_EQ(result.lo.data[3], expected.data[3]);
            EXPECT_EQ(result.hi.data[0], 0ULL);
            EXPECT_EQ(result.hi.data[1], 0ULL);
            EXPECT_EQ(result.hi.data[2], 0ULL);
            EXPECT_EQ(result.hi.data[3], 0ULL);
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_add_and_mul()
    {
        auto composer = Composer();
        size_t num_repetitions = 10;
        for (size_t i = 0; i < num_repetitions; ++i) {
            fq inputs[4]{ fq::random_element(), fq::random_element(), fq::random_element(), fq::random_element() };

            fq_ct a(witness_ct(&composer, fr(uint256_t(inputs[0]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct b(witness_ct(&composer, fr(uint256_t(inputs[1]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[1]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct c(witness_ct(&composer, fr(uint256_t(inputs[2]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[2]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct d(witness_ct(&composer, fr(uint256_t(inputs[3]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[3]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct e = (a + b) * (c + d);
            fq expected = (inputs[0] + inputs[1]) * (inputs[2] + inputs[3]);
            expected = expected.from_montgomery_form();
            uint512_t result = e.get_value();

            EXPECT_EQ(result.lo.data[0], expected.data[0]);
            EXPECT_EQ(result.lo.data[1], expected.data[1]);
            EXPECT_EQ(result.lo.data[2], expected.data[2]);
            EXPECT_EQ(result.lo.data[3], expected.data[3]);
            EXPECT_EQ(result.hi.data[0], 0ULL);
            EXPECT_EQ(result.hi.data[1], 0ULL);
            EXPECT_EQ(result.hi.data[2], 0ULL);
            EXPECT_EQ(result.hi.data[3], 0ULL);
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_add_and_mul_with_constants()
    {
        auto composer = Composer();
        size_t num_repetitions = 10;
        for (size_t i = 0; i < num_repetitions; ++i) {
            fq inputs[2]{ fq::random_element(), fq::random_element() };
            uint256_t constants[2]{ engine.get_random_uint256() % fq_ct::modulus,
                                    engine.get_random_uint256() % fq_ct::modulus };
            fq_ct a(witness_ct(&composer, fr(uint256_t(inputs[0]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct b(&composer, constants[0]);
            fq_ct c(witness_ct(&composer, fr(uint256_t(inputs[1]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[1]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct d(&composer, constants[1]);
            fq_ct e = (a + b) * (c + d);
            fq expected = (inputs[0] + constants[0]) * (inputs[1] + constants[1]);
            expected = expected.from_montgomery_form();
            uint512_t result = e.get_value();

            EXPECT_EQ(result.lo.data[0], expected.data[0]);
            EXPECT_EQ(result.lo.data[1], expected.data[1]);
            EXPECT_EQ(result.lo.data[2], expected.data[2]);
            EXPECT_EQ(result.lo.data[3], expected.data[3]);
            EXPECT_EQ(result.hi.data[0], 0ULL);
            EXPECT_EQ(result.hi.data[1], 0ULL);
            EXPECT_EQ(result.hi.data[2], 0ULL);
            EXPECT_EQ(result.hi.data[3], 0ULL);
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_sub_and_mul()
    {
        auto composer = Composer();
        size_t num_repetitions = 10;
        for (size_t i = 0; i < num_repetitions; ++i) {
            fq inputs[4]{ fq::random_element(), fq::random_element(), fq::random_element(), fq::random_element() };

            fq_ct a(witness_ct(&composer, fr(uint256_t(inputs[0]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct b(witness_ct(&composer, fr(uint256_t(inputs[1]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[1]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct c(witness_ct(&composer, fr(uint256_t(inputs[2]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[2]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct d(witness_ct(&composer, fr(uint256_t(inputs[3]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[3]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct e = (a - b) * (c - d);
            fq expected = (inputs[0] - inputs[1]) * (inputs[2] - inputs[3]);
            expected = expected.from_montgomery_form();
            uint512_t result = e.get_value();

            EXPECT_EQ(result.lo.data[0], expected.data[0]);
            EXPECT_EQ(result.lo.data[1], expected.data[1]);
            EXPECT_EQ(result.lo.data[2], expected.data[2]);
            EXPECT_EQ(result.lo.data[3], expected.data[3]);
            EXPECT_EQ(result.hi.data[0], 0ULL);
            EXPECT_EQ(result.hi.data[1], 0ULL);
            EXPECT_EQ(result.hi.data[2], 0ULL);
            EXPECT_EQ(result.hi.data[3], 0ULL);
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_conditional_negate()
    {
        auto composer = Composer();
        size_t num_repetitions = 1;
        for (size_t i = 0; i < num_repetitions; ++i) {

            fq inputs[4]{ fq::random_element(), fq::random_element(), fq::random_element(), fq::random_element() };

            fq_ct a(witness_ct(&composer, fr(uint256_t(inputs[0]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            // fq_ct b(witness_ct(&composer,
            // fr(uint256_t(inputs[1]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
            //            witness_ct(&composer,
            //            fr(uint256_t(inputs[1]).slice(fq_ct::NUM_LIMB_BITS * 2,
            //            fq_ct::NUM_LIMB_BITS * 4))));

            typename bn254::bool_ct predicate_a(witness_ct(&composer, true));
            // bool_ct predicate_b(witness_ct(&composer, false));

            fq_ct c = a.conditional_negate(predicate_a);
            fq_ct d = a.conditional_negate(!predicate_a);
            fq_ct e = c + d;
            uint512_t c_out = c.get_value();
            uint512_t d_out = d.get_value();
            uint512_t e_out = e.get_value();

            fq result_c(c_out.lo);
            fq result_d(d_out.lo);
            fq result_e(e_out.lo);

            fq expected_c = (-inputs[0]);
            fq expected_d = inputs[0];

            EXPECT_EQ(result_c, expected_c);
            EXPECT_EQ(result_d, expected_d);
            EXPECT_EQ(result_e, fq(0));
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_group_operations()
    {
        auto composer = Composer();
        size_t num_repetitions = 1;
        for (size_t i = 0; i < num_repetitions; ++i) {
            g1::affine_element P1(g1::element::random_element());
            g1::affine_element P2(g1::element::random_element());

            fq_ct x1(
                witness_ct(&composer, fr(uint256_t(P1.x).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                witness_ct(&composer, fr(uint256_t(P1.x).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct y1(
                witness_ct(&composer, fr(uint256_t(P1.y).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                witness_ct(&composer, fr(uint256_t(P1.y).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct x2(
                witness_ct(&composer, fr(uint256_t(P2.x).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                witness_ct(&composer, fr(uint256_t(P2.x).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct y2(
                witness_ct(&composer, fr(uint256_t(P2.y).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                witness_ct(&composer, fr(uint256_t(P2.y).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            uint64_t before = composer.get_num_gates();

            fq_ct lambda = (y2 - y1) / (x2 - x1);
            fq_ct x3 = lambda.sqr() - (x2 + x1);
            fq_ct y3 = (x1 - x3) * lambda - y1;
            uint64_t after = composer.get_num_gates();
            std::cout << "added gates = " << after - before << std::endl;
            g1::affine_element P3(g1::element(P1) + g1::element(P2));
            fq expected_x = P3.x;
            fq expected_y = P3.y;
            expected_x = expected_x.from_montgomery_form();
            expected_y = expected_y.from_montgomery_form();
            uint512_t result_x = x3.get_value() % fq_ct::modulus_u512;
            uint512_t result_y = y3.get_value() % fq_ct::modulus_u512;
            EXPECT_EQ(result_x.lo.data[0], expected_x.data[0]);
            EXPECT_EQ(result_x.lo.data[1], expected_x.data[1]);
            EXPECT_EQ(result_x.lo.data[2], expected_x.data[2]);
            EXPECT_EQ(result_x.lo.data[3], expected_x.data[3]);
            EXPECT_EQ(result_y.lo.data[0], expected_y.data[0]);
            EXPECT_EQ(result_y.lo.data[1], expected_y.data[1]);
            EXPECT_EQ(result_y.lo.data[2], expected_y.data[2]);
            EXPECT_EQ(result_y.lo.data[3], expected_y.data[3]);
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_reduce()
    {
        auto composer = Composer();
        size_t num_repetitions = 10;
        for (size_t i = 0; i < num_repetitions; ++i) {

            fq inputs[4]{ fq::random_element(), fq::random_element(), fq::random_element(), fq::random_element() };

            fq_ct a(witness_ct(&composer, fr(uint256_t(inputs[0]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct b(witness_ct(&composer, fr(uint256_t(inputs[1]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[1]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));

            fq_ct c = a;
            fq expected = inputs[0];
            for (size_t i = 0; i < 16; ++i) {
                c = b * b + c;
                expected = inputs[1] * inputs[1] + expected;
            }
            // fq_ct c = a + a + a + a - b - b - b - b;
            c.self_reduce();
            fq result = fq(c.get_value().lo);
            EXPECT_EQ(result, expected);
            EXPECT_EQ(c.get_value().get_msb() < 254, true);
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_assert_is_in_field_success()
    {
        auto composer = Composer();
        size_t num_repetitions = 10;
        for (size_t i = 0; i < num_repetitions; ++i) {

            fq inputs[4]{ fq::random_element(), fq::random_element(), fq::random_element(), fq::random_element() };

            fq_ct a(witness_ct(&composer, fr(uint256_t(inputs[0]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));
            fq_ct b(witness_ct(&composer, fr(uint256_t(inputs[1]).slice(0, fq_ct::NUM_LIMB_BITS * 2))),
                    witness_ct(&composer,
                               fr(uint256_t(inputs[1]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS * 4))));

            fq_ct c = a;
            fq expected = inputs[0];
            for (size_t i = 0; i < 16; ++i) {
                c = b * b + c;
                expected = inputs[1] * inputs[1] + expected;
            }
            // fq_ct c = a + a + a + a - b - b - b - b;
            c.assert_is_in_field();
            uint256_t result = (c.get_value().lo);
            EXPECT_EQ(result, uint256_t(expected));
            EXPECT_EQ(c.get_value().get_msb() < 254, true);
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }

    static void test_byte_array_constructors()
    {
        auto composer = Composer();
        size_t num_repetitions = 10;
        for (size_t i = 0; i < num_repetitions; ++i) {

            fq inputs[4]{ fq::random_element(), fq::random_element(), fq::random_element(), fq::random_element() };
            std::vector<uint8_t> input_a(sizeof(fq));
            fq::serialize_to_buffer(inputs[0], &input_a[0]);
            std::vector<uint8_t> input_b(sizeof(fq));
            fq::serialize_to_buffer(inputs[1], &input_b[0]);

            stdlib::byte_array<Composer> input_arr_a(&composer, input_a);
            stdlib::byte_array<Composer> input_arr_b(&composer, input_b);

            fq_ct a(input_arr_a);
            fq_ct b(input_arr_b);

            fq_ct c = a * b;

            fq expected = inputs[0] * inputs[1];
            uint256_t result = (c.get_value().lo);
            EXPECT_EQ(result, uint256_t(expected));
        }
        auto prover = composer.create_prover();
        auto verifier = composer.create_verifier();
        waffle::plonk_proof proof = prover.construct_proof();
        bool proof_result = verifier.verify_proof(proof);
        EXPECT_EQ(proof_result, true);
    }
};

// Define types for which the above tests will be constructed.
typedef testing::Types<waffle::StandardComposer,
                       waffle::TurboComposer //,
                                             //    waffle::PlookupComposer
                       >
    ComposerTypes;
// Define the suite of tests.
TYPED_TEST_SUITE(StdlibBigfield, ComposerTypes);

TYPED_TEST(StdlibBigfield, Mul)
{
    TestFixture::test_mul();
}
TYPED_TEST(StdlibBigfield, Sqr)
{
    TestFixture::test_sqr();
}
TYPED_TEST(StdlibBigfield, Madd)
{
    TestFixture::test_madd();
}
TYPED_TEST(StdlibBigfield, DualMadd)
{
    TestFixture::test_dual_madd();
}
TYPED_TEST(StdlibBigfield, Div)
{
    TestFixture::test_div();
}
TYPED_TEST(StdlibBigfield, AddAndDiv)
{
    TestFixture::test_add_and_div();
}
TYPED_TEST(StdlibBigfield, AddAndMul)
{
    TestFixture::test_add_and_mul();
}
TYPED_TEST(StdlibBigfield, AddAndMulWithConstants)
{
    TestFixture::test_add_and_mul_with_constants();
}
TYPED_TEST(StdlibBigfield, SubAndMul)
{
    TestFixture::test_sub_and_mul();
}
TYPED_TEST(StdlibBigfield, ConditionalNegate)
{
    TestFixture::test_conditional_negate();
}
TYPED_TEST(StdlibBigfield, GroupOperations)
{
    TestFixture::test_group_operations();
}
TYPED_TEST(StdlibBigfield, Reduce)
{
    TestFixture::test_reduce();
}
TYPED_TEST(StdlibBigfield, AssertIsInFieldSuccess)
{
    TestFixture::test_assert_is_in_field_success();
}
TYPED_TEST(StdlibBigfield, ByteArrayConstructors)
{
    TestFixture::test_byte_array_constructors();
}

// // This test was disabled before the refactor to use TYPED_TEST's/
// TEST(StdlibBigfield, DISABLED_test_div_against_constants)
// {
//     auto composer = Composer();
//     size_t num_repetitions = 1;
//     for (size_t i = 0; i < num_repetitions; ++i) {
//         fq inputs[3]{ fq::random_element(), fq::random_element(), fq::random_element() };
//         fq_ct a(witness_ct(&composer, barretenberg::fr(uint256_t(inputs[0]).slice(0,
//         fq_ct::NUM_LIMB_BITS * 2))),
//                 witness_ct(
//                     &composer,
//                     barretenberg::fr(uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2,
//                     fq_ct::NUM_LIMB_BITS * 4))));
//         fq_ct b1(&composer, uint256_t(inputs[1]));
//         fq_ct b2(&composer, uint256_t(inputs[2]));
//         fq_ct c = a / (b1 - b2);
//         // uint256_t modulus{ barretenberg::Bn254FqParams::modulus_0,
//         //                    barretenberg::Bn254FqParams::modulus_1,
//         //                    barretenberg::Bn254FqParams::modulus_2,
//         //                    barretenberg::Bn254FqParams::modulus_3 };

//         fq expected = (inputs[0] / (inputs[1] - inputs[2]));
//         std::cout << "denominator = " << inputs[1] - inputs[2] << std::endl;
//         std::cout << "expected = " << expected << std::endl;
//         expected = expected.from_montgomery_form();
//         uint512_t result = c.get_value();

//         EXPECT_EQ(result.lo.data[0], expected.data[0]);
//         EXPECT_EQ(result.lo.data[1], expected.data[1]);
//         EXPECT_EQ(result.lo.data[2], expected.data[2]);
//         EXPECT_EQ(result.lo.data[3], expected.data[3]);
//         EXPECT_EQ(result.hi.data[0], 0ULL);
//         EXPECT_EQ(result.hi.data[1], 0ULL);
//         EXPECT_EQ(result.hi.data[2], 0ULL);
//         EXPECT_EQ(result.hi.data[3], 0ULL);
//     }
//     auto prover = composer.create_prover();
//     auto verifier = composer.create_verifier();
//     waffle::plonk_proof proof = prover.construct_proof();
//     bool proof_result = verifier.verify_proof(proof);
//     EXPECT_EQ(proof_result, true);
// }

// // PLOOKUP TESTS
// TEST(stdlib_bigfield_plookup, test_mul)
// {
//     waffle::PlookupComposer composer = waffle::PlookupComposer();
//     size_t num_repetitions = 1;
//     for (size_t i = 0; i < num_repetitions; ++i) {
//         fq inputs[3]{ fq::random_element(), fq::random_element(), fq::random_element() };
//         fq_ct a(
//             witness_ct(&composer, barretenberg::fr(uint256_t(inputs[0]).slice(0,
//             fq_ct::NUM_LIMB_BITS * 2))), witness_ct(&composer,
//                        barretenberg::fr(
//                            uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS *
//                            4))));
//         fq_ct b(
//             witness_ct(&composer, barretenberg::fr(uint256_t(inputs[1]).slice(0,
//             fq_ct::NUM_LIMB_BITS * 2))), witness_ct(&composer,
//                        barretenberg::fr(
//                            uint256_t(inputs[1]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS *
//                            4))));
//         std::cout << "starting mul" << std::endl;
//         uint64_t before = composer.get_num_gates();
//         fq_ct c = a * b;
//         uint64_t after = composer.get_num_gates();
//         if (i == num_repetitions - 1) {
//             std::cout << "num gates per mul = " << after - before << std::endl;
//         }

//         fq expected = (inputs[0] * inputs[1]);
//         expected = expected.from_montgomery_form();
//         uint512_t result = c.get_value();

//         EXPECT_EQ(result.lo.data[0], expected.data[0]);
//         EXPECT_EQ(result.lo.data[1], expected.data[1]);
//         EXPECT_EQ(result.lo.data[2], expected.data[2]);
//         EXPECT_EQ(result.lo.data[3], expected.data[3]);
//         EXPECT_EQ(result.hi.data[0], 0ULL);
//         EXPECT_EQ(result.hi.data[1], 0ULL);
//         EXPECT_EQ(result.hi.data[2], 0ULL);
//         EXPECT_EQ(result.hi.data[3], 0ULL);
//     }
//     composer.process_range_lists();
//     waffle::PlookupProver prover = composer.create_prover();
//     waffle::PlookupVerifier verifier = composer.create_verifier();
//     waffle::plonk_proof proof = prover.construct_proof();
//     bool proof_result = verifier.verify_proof(proof);
//     EXPECT_EQ(proof_result, true);
// }

// TEST(stdlib_bigfield_plookup, test_sqr)
// {
//     waffle::PlookupComposer composer = waffle::PlookupComposer();
//     size_t num_repetitions = 10;
//     for (size_t i = 0; i < num_repetitions; ++i) {
//         fq inputs[3]{ fq::random_element(), fq::random_element(), fq::random_element() };
//         fq_ct a(
//             witness_ct(&composer, barretenberg::fr(uint256_t(inputs[0]).slice(0,
//             fq_ct::NUM_LIMB_BITS * 2))), witness_ct(&composer,
//                        barretenberg::fr(
//                            uint256_t(inputs[0]).slice(fq_ct::NUM_LIMB_BITS * 2, fq_ct::NUM_LIMB_BITS *
//                            4))));
//         uint64_t before = composer.get_num_gates();
//         fq_ct c = a.sqr();
//         uint64_t after = composer.get_num_gates();
//         if (i == num_repetitions - 1) {
//             std::cout << "num gates per sqr = " << after - before << std::endl;
//         }

//         fq expected = (inputs[0].sqr());
//         expected = expected.from_montgomery_form();
//         uint512_t result = c.get_value();

//         EXPECT_EQ(result.lo.data[0], expected.data[0]);
//         EXPECT_EQ(result.lo.data[1], expected.data[1]);
//         EXPECT_EQ(result.lo.data[2], expected.data[2]);
//         EXPECT_EQ(result.lo.data[3], expected.data[3]);
//         EXPECT_EQ(result.hi.data[0], 0ULL);
//         EXPECT_EQ(result.hi.data[1], 0ULL);
//         EXPECT_EQ(result.hi.data[2], 0ULL);
//         EXPECT_EQ(result.hi.data[3], 0ULL);
//     }
//     composer.process_range_lists();
//     waffle::PlookupProver prover = composer.create_prover();
//     waffle::PlookupVerifier verifier = composer.create_verifier();
//     waffle::plonk_proof proof = prover.construct_proof();
//     bool proof_result = verifier.verify_proof(proof);
//     EXPECT_EQ(proof_result, true);
// }
} // namespace test_stdlib_bigfield