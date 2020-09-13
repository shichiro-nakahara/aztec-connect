#pragma once
#include <stdlib/types/turbo.hpp>

namespace rollup {
namespace proofs {
namespace pedersen_note {

using namespace plonk::stdlib::types::turbo;

constexpr size_t NOTE_VALUE_BIT_LENGTH = 252;

struct public_note {
    point_ct ciphertext;
};

struct private_note {
    point_ct owner;
    // note value must be 252 bits or smaller - we assume this is checked elsewhere
    field_ct value;
    // this secret must be 250 bits or smaller - it cannot be taken from the entire field_ct range
    field_ct secret;
};

typedef std::pair<private_note, public_note> note_pair;

public_note encrypt_note(const private_note& plaintext);

// template <size_t num_bits> note_triple fixed_base_scalar_mul(const field_ct& in, const size_t generator_index);
// extern template note_triple fixed_base_scalar_mul<32>(const field_ct& in, const size_t generator_index);
// extern template note_triple fixed_base_scalar_mul<250>(const field_ct& in, const size_t generator_index);

} // namespace pedersen_note
} // namespace proofs
} // namespace rollup