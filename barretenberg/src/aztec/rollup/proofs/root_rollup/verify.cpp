#include <common/container.hpp>
#include "verify.hpp"
#include "root_rollup_circuit.hpp"
#include "create_root_rollup_tx.hpp"
#include "root_rollup_proof_data.hpp"

namespace rollup {
namespace proofs {
namespace root_rollup {

using namespace barretenberg;
using namespace plonk::stdlib::types::turbo;

bool pairing_check(recursion_output<bn254> recursion_output,
                   std::shared_ptr<waffle::VerifierReferenceString> const& srs)
{
    g1::affine_element P[2];
    P[0].x = barretenberg::fq(recursion_output.P0.x.get_value().lo);
    P[0].y = barretenberg::fq(recursion_output.P0.y.get_value().lo);
    P[1].x = barretenberg::fq(recursion_output.P1.x.get_value().lo);
    P[1].y = barretenberg::fq(recursion_output.P1.y.get_value().lo);
    barretenberg::fq12 inner_proof_result =
        barretenberg::pairing::reduced_ate_pairing_batch_precomputed(P, srs->get_precomputed_g2_lines(), 2);
    return inner_proof_result == barretenberg::fq12::one();
}

verify_result verify_internal(Composer& composer,
                              root_rollup_tx& tx,
                              circuit_data const& circuit_data,
                              bool skip_pairing)
{
    verify_result result = { false, false, {}, {}, {}, recursion_output<bn254>() };

    if (!circuit_data.inner_rollup_circuit_data.verification_key) {
        info("Inner verification key not provided.");
        return result;
    }

    if (circuit_data.inner_rollup_circuit_data.padding_proof.size() == 0) {
        info("Inner padding proof not provided.");
        return result;
    }

    if (!circuit_data.verifier_crs) {
        info("Verifier crs not provided.");
        return result;
    }

    // Pad the rollup if necessary.
    pad_rollup_tx(tx, circuit_data);

    auto circuit_result = root_rollup_circuit(composer,
                                              tx,
                                              circuit_data.inner_rollup_circuit_data.rollup_size,
                                              circuit_data.rollup_size,
                                              circuit_data.inner_rollup_circuit_data.verification_key);

    result.recursion_output_data = circuit_result.recursion_output;
    result.broadcast_data = circuit_result.broadcast_data;
    result.public_inputs = composer.get_public_inputs();

    if (composer.failed) {
        info("Circuit logic failed: " + composer.err);
        return result;
    }

    if (!skip_pairing && !pairing_check(result.recursion_output_data, circuit_data.verifier_crs)) {
        info("Native pairing check failed.");
        return result;
    }

    result.logic_verified = true;
    return result;
}

verify_result verify_logic(root_rollup_tx& tx, circuit_data const& circuit_data)
{
    Composer composer = Composer(circuit_data.proving_key, circuit_data.verification_key, circuit_data.num_gates);
    return verify_internal(composer, tx, circuit_data, false);
}

verify_result verify_proverless(root_rollup_tx& tx, circuit_data const& circuit_data)
{
    Composer composer = Composer(circuit_data.proving_key, circuit_data.verification_key, circuit_data.num_gates);
    auto result = verify_internal(composer, tx, circuit_data, true);

    if (!result.logic_verified) {
        return result;
    }

    auto pub_input_buf = to_buffer(result.public_inputs);
    result.proof_data = join({ pub_input_buf, slice(circuit_data.padding_proof, pub_input_buf.size()) });
    result.verified = true;
    return result;
}

verify_result verify(root_rollup_tx& tx, circuit_data const& circuit_data)
{
    Composer composer = Composer(circuit_data.proving_key, circuit_data.verification_key, circuit_data.num_gates);
    auto result = verify_internal(composer, tx, circuit_data, false);

    if (!result.logic_verified) {
        return result;
    }

    circuit_data.proving_key->reset();

    auto prover = composer.create_unrolled_prover();
    auto proof = prover.construct_proof();
    result.proof_data = proof.proof_data;

    auto verifier = composer.create_unrolled_verifier();
    result.verified = verifier.verify_proof(proof);

    if (!result.verified) {
        info("Proof validation failed.");
        return result;
    }

    return result;
}

} // namespace root_rollup
} // namespace proofs
} // namespace rollup
