#pragma once
#include <stdlib/types/turbo.hpp>
#include "witness_data.hpp"
#include "commit.hpp"

namespace rollup {
namespace proofs {
namespace notes {
namespace circuit {
namespace value {

using namespace plonk::stdlib::types::turbo;

struct value_note {
    point_ct owner;
    field_ct value;
    field_ct secret;
    field_ct asset_id;
    field_ct nonce;
    field_ct input_nullifier;
    field_ct commitment;
    field_ct creator_pubkey;
    bool_ct is_virtual;
    field_ct virtual_note_nonce;

    value_note(witness_data const& note)
        : owner(note.owner)
        , value(note.value)
        , secret(note.secret)
        , asset_id(note.asset_id)
        , nonce(note.nonce)
        , input_nullifier(note.input_nullifier)
        , commitment(value::commit(note))
        , creator_pubkey(note.creator_pubkey)
    {
        const auto loan_idx = MAX_NUM_ASSETS_BIT_LENGTH + 1;
        const auto sliced_asset_id = asset_id.slice(loan_idx + 1, loan_idx);
        is_virtual = sliced_asset_id[1] == 1;
        virtual_note_nonce = sliced_asset_id[0];
    }

    operator byte_array_ct() const { return byte_array_ct(commitment); }
};

} // namespace value
} // namespace circuit
} // namespace notes
} // namespace proofs
} // namespace rollup