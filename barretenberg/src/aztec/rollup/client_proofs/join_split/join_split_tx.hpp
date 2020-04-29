#pragma once
#include "tx_note.hpp"
#include <crypto/schnorr/schnorr.hpp>
#include <stdlib/merkle_tree/hash_path.hpp>

namespace rollup {
namespace client_proofs {
namespace join_split {

using namespace plonk::stdlib;

struct join_split_tx {
    grumpkin::g1::affine_element owner_pub_key;
    uint32_t public_input;
    uint32_t public_output;
    uint32_t num_input_notes;
    std::array<uint32_t, 2> input_index;
    barretenberg::fr merkle_root;
    std::array<merkle_tree::fr_hash_path, 2> input_path;
    std::array<tx_note, 2> input_note;
    std::array<tx_note, 2> output_note;
    crypto::schnorr::signature signature;

    std::vector<uint8_t> to_buffer();
    static join_split_tx from_buffer(uint8_t const* buf);
};

void read(uint8_t const*& it, join_split_tx& tx);
void write(std::vector<uint8_t>& buf, join_split_tx const& tx);

bool operator==(join_split_tx const& lhs, join_split_tx const& rhs);
std::ostream& operator<<(std::ostream& os, join_split_tx const& tx);

} // namespace join_split
} // namespace client_proofs
} // namespace rollup