#pragma once
#include <barretenberg/misc_crypto/commitment/pedersen_note.hpp>
#include <barretenberg/misc_crypto/schnorr/schnorr.hpp>

namespace rollup {

struct join_split_tx {
    grumpkin::g1::affine_element owner_pub_key;
    uint32_t public_input;
    uint32_t public_output;
    uint32_t num_input_notes;
    uint32_t input_note_index[2];
    crypto::pedersen_note::private_note input_note[2];
    crypto::pedersen_note::private_note output_note[2];
    crypto::schnorr::signature signature;
};

inline std::ostream& operator<<(std::ostream& os, join_split_tx const& tx)
{
    return os << "public_input: " << tx.public_input << "\n"
              << "public_output: " << tx.public_output << "\n"
              << "in_value1: " << tx.input_note[0].value << "\n"
              << "in_value2: " << tx.input_note[1].value << "\n"
              << "out_value1: " << tx.output_note[0].value << "\n"
              << "out_value2: " << tx.output_note[1].value << "\n"
              << "num_input_notes: " << tx.num_input_notes << "\n"
              << "owner: " << tx.owner_pub_key.x << " " << tx.owner_pub_key.y << "\n";
}

join_split_tx hton(join_split_tx const& tx);
join_split_tx ntoh(join_split_tx const& tx);
std::ostream& write(std::ostream& os, join_split_tx const& tx);
std::istream& read(std::istream& is, join_split_tx& tx);

} // namespace rollup
