#include "compute_rollup_circuit_data.hpp"
#include "compute_inner_circuit_data.hpp"
#include "../client_proofs/join_split/join_split.hpp"
#include "rollup_circuit.hpp"

namespace rollup {
namespace rollup_proofs {

using namespace rollup::client_proofs::join_split;
using namespace plonk::stdlib::types::turbo;
using namespace rollup::rollup_proofs;

rollup_circuit_data compute_rollup_circuit_data(size_t rollup_size)
{
    auto inner = compute_inner_circuit_data();

    std::cerr << "Generating rollup circuit keys..." << std::endl;

    Composer composer = Composer("../srs_db/ignition");

    // Junk data required just to create keys.
    std::vector<waffle::plonk_proof> proofs(rollup_size, { std::vector<uint8_t>(inner.proof_size) });
    auto gibberish_data_path = fr_hash_path(32, std::make_pair(fr::random_element(), fr::random_element() ));
    auto gibberish_null_path = fr_hash_path(128, std::make_pair(fr::random_element(), fr::random_element() ));

    rollup_tx rollup = {
        0,
        (uint32_t)rollup_size,
        (uint32_t)inner.proof_size,
        0,
        std::vector(rollup_size, std::vector<uint8_t>(inner.proof_size)),
        fr::random_element(),
        fr::random_element(),
        std::vector(rollup_size * 2, std::make_pair(uint128_t(0), gibberish_data_path)),
        std::vector(rollup_size * 2, std::make_pair(uint128_t(0), gibberish_data_path)),
        fr::random_element(),
        fr::random_element(),
        std::vector(rollup_size * 2, std::make_pair(uint128_t(0), gibberish_null_path)),
        std::vector(rollup_size * 2, std::make_pair(uint128_t(0), gibberish_null_path)),
    };

    rollup_circuit(composer, rollup, inner.verification_key);

    std::cerr << "Circuit size: " << composer.get_num_gates() << std::endl;
    auto proving_key = composer.compute_proving_key();
    auto verification_key = composer.compute_verification_key();
    auto num_gates = composer.get_num_gates();
    std::cerr << "Done." << std::endl;

    return { proving_key, verification_key, num_gates, inner.proof_size, inner.verification_key };
}

} // namespace rollup_proofs
} // namespace rollup