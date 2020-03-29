#include "verifier.hpp"
#include <gtest/gtest.h>

#include <ecc/curves/bn254/fr.hpp>
#include <ecc/curves/bn254/g1.hpp>

#include <plonk/transcript/transcript.hpp>
#include <stdlib/types/turbo.hpp>

#include <ecc/curves/bn254/fq12.hpp>
#include <ecc/curves/bn254/pairing.hpp>

#include "../../hash/pedersen/pedersen.hpp"
#include "../../hash/blake2s/blake2s.hpp"

#include "program_settings.hpp"

using namespace plonk;

using namespace plonk::stdlib::types::turbo;

void create_inner_circuit(waffle::TurboComposer& composer)
{
    field_ct a(witness_ct(&composer, barretenberg::fr::random_element()));
    field_ct b(witness_ct(&composer, barretenberg::fr::random_element()));
    for (size_t i = 0; i < 32; ++i) {
        a = (a * b) + b + a;
        a = a.madd(b, a);
    }
    stdlib::pedersen::compress(a, b);
    byte_array_ct to_hash(&composer, "nonsense test data");
    stdlib::blake2s(to_hash);

    barretenberg::fr bigfield_data = fr::random_element();
    barretenberg::fr bigfield_data_a{ bigfield_data.data[0], bigfield_data.data[1], 0, 0 };
    barretenberg::fr bigfield_data_b{ bigfield_data.data[2], bigfield_data.data[3], 0, 0 };

    fq_ct big_a(field_ct(witness_ct(&composer, bigfield_data_a.to_montgomery_form())),
                field_ct(witness_ct(&composer, 0)));
    fq_ct big_b(field_ct(witness_ct(&composer, bigfield_data_b.to_montgomery_form())),
                field_ct(witness_ct(&composer, 0)));
    big_a* big_b;
}

// Ok, so we need to create a recursive circuit...
stdlib::recursion::recursion_output<group_ct> create_outer_circuit(waffle::TurboComposer& inner_composer,
                                                                   waffle::TurboComposer& outer_composer)
{
    waffle::UnrolledTurboProver prover = inner_composer.create_unrolled_prover();

    std::shared_ptr<waffle::verification_key> verification_key = inner_composer.compute_verification_key();

    waffle::plonk_proof recursive_proof = prover.construct_proof();
    transcript::Manifest recursive_manifest =
        waffle::TurboComposer::create_unrolled_manifest(prover.key->num_public_inputs);

    stdlib::recursion::recursion_output<group_ct> output =
        stdlib::recursion::verify_proof<waffle::TurboComposer,
                                        plonk::stdlib::recursion::recursive_turbo_verifier_settings>(
            &outer_composer, verification_key, recursive_manifest, recursive_proof);
    return output;
}

TEST(stdlib_verifier, test_recursive_proof_composition)
{
    waffle::TurboComposer inner_composer = waffle::TurboComposer();
    waffle::TurboComposer outer_composer = waffle::TurboComposer();
    create_inner_circuit(inner_composer);
    stdlib::recursion::recursion_output<group_ct> output = create_outer_circuit(inner_composer, outer_composer);

    printf("composer gates = %zu\n", outer_composer.get_num_gates());

    std::cout << "creating prover" << std::endl;
    waffle::TurboProver prover = outer_composer.create_prover();
    std::cout << "created prover" << std::endl;
    g1::affine_element P[2];
    P[0].x = barretenberg::fq(output.P0.x.get_value().lo);
    P[0].y = barretenberg::fq(output.P0.y.get_value().lo);
    P[1].x = barretenberg::fq(output.P1.x.get_value().lo);
    P[1].y = barretenberg::fq(output.P1.y.get_value().lo);

    barretenberg::fq12 inner_proof_result = barretenberg::pairing::reduced_ate_pairing_batch_precomputed(
        P, prover.key->reference_string.precomputed_g2_lines, 2);

    EXPECT_EQ(inner_proof_result, barretenberg::fq12::one());

    std::cout << "creating verifier" << std::endl;
    waffle::TurboVerifier verifier = outer_composer.create_verifier();
    std::cout << "created verifier" << std::endl;

    std::cout << "creating proof" << std::endl;
    waffle::plonk_proof proof = prover.construct_proof();
    std::cout << "created proof" << std::endl;

    bool result = verifier.verify_proof(proof);
    EXPECT_EQ(result, true);
}