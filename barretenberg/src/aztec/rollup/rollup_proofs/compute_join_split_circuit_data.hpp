#pragma once
#include <plonk/proof_system/proving_key/proving_key.hpp>
#include <plonk/proof_system/verification_key/verification_key.hpp>
#include <rollup/client_proofs/join_split/join_split_tx.hpp>

namespace rollup {
namespace rollup_proofs {

rollup::client_proofs::join_split::join_split_tx noop_tx();

struct join_split_circuit_data {
    std::shared_ptr<waffle::proving_key> proving_key;
    std::shared_ptr<waffle::verification_key> verification_key;
    size_t num_gates;
    std::vector<uint8_t> padding_proof;
};

join_split_circuit_data compute_join_split_circuit_data(std::string const& srs_path);

join_split_circuit_data compute_or_load_join_split_circuit_data(std::string const& srs_path,
                                                                std::string const& key_path);

} // namespace rollup_proofs
} // namespace rollup
