#pragma once
#include <stdlib/types/turbo.hpp>
#include "../native/defi_interaction/defi_interaction_note.hpp"
#include "witness_data.hpp"
#include "encrypt.hpp"

namespace rollup {
namespace proofs {
namespace notes {
namespace circuit {
namespace defi_interaction {

using namespace plonk::stdlib::types::turbo;

struct defi_interaction_note {

    // compress bridge_id to field
    field_ct bridge_id;

    // 32 bits
    field_ct interaction_nonce;

    // 252 bits
    field_ct total_input_value;

    // 252 bits
    field_ct total_output_a_value;

    // 252 bits. Force this to be 0 if bridge_id only uses 1 output note
    field_ct total_output_b_value;

    // if interaction failed, re-create original deposit note
    bool_ct interaction_result;

    // encrypted defi_interaction_note
    point_ct encrypted;

    defi_interaction_note(witness_data const& note)
        : bridge_id(note.bridge_id)
        , interaction_nonce(note.interaction_nonce)
        , total_input_value(note.total_input_value)
        , total_output_a_value(note.total_output_a_value)
        , total_output_b_value(note.total_output_b_value)
        , interaction_result(note.interaction_result)
        , encrypted(encrypt(note))
    {}
};

} // namespace defi_interaction
} // namespace circuit
} // namespace notes
} // namespace proofs
} // namespace rollup