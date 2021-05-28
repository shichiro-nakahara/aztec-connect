#pragma once
#include <stdlib/types/turbo.hpp>
#include "../../native/defi_interaction/defi_interaction_note.hpp"
#include "../bridge_id.hpp"

namespace rollup {
namespace proofs {
namespace notes {
namespace circuit {
namespace defi_interaction {

using namespace plonk::stdlib::types::turbo;

struct witness_data {
    bridge_id bridge_id_data;
    field_ct interaction_nonce;
    field_ct total_input_value;
    field_ct total_output_a_value;
    field_ct total_output_b_value;
    bool_ct interaction_result;

    witness_data(Composer& composer, native::defi_interaction::defi_interaction_note const& note_data)
    {
        bridge_id_data = bridge_id::from_uint256_t(composer, note_data.bridge_id);
        interaction_nonce = witness_ct(&composer, note_data.interaction_nonce);
        total_input_value = witness_ct(&composer, note_data.total_input_value);
        total_output_a_value = witness_ct(&composer, note_data.total_output_a_value);
        total_output_b_value = witness_ct(&composer, note_data.total_output_b_value);
        interaction_result = witness_ct(&composer, note_data.interaction_result);

        composer.create_range_constraint(interaction_nonce.witness_index, 32);
        composer.create_range_constraint(total_input_value.witness_index, NOTE_VALUE_BIT_LENGTH);
        composer.create_range_constraint(total_output_a_value.witness_index, NOTE_VALUE_BIT_LENGTH);
        composer.create_range_constraint(total_output_b_value.witness_index, NOTE_VALUE_BIT_LENGTH);
        composer.create_range_constraint(interaction_result.witness_index, 1);
    }
};

} // namespace defi_interaction
} // namespace circuit
} // namespace notes
} // namespace proofs
} // namespace rollup