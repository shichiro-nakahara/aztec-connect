#include "escape_hatch.hpp"
#include "../../constants.hpp"
#include "../join_split/join_split_circuit.hpp"
#include "../rollup/rollup_circuit.hpp"
#include "../root_rollup/root_rollup_circuit.hpp"
#include <common/map.hpp>

// #pragma GCC diagnostic ignored "-Wunused-variable"
// #pragma GCC diagnostic ignored "-Wunused-parameter"
namespace rollup {
namespace proofs {
namespace escape_hatch {

using namespace plonk::stdlib::types::turbo;
using namespace plonk::stdlib::merkle_tree;
using namespace join_split;
using namespace proofs::notes::circuit;

void escape_hatch_circuit(Composer& composer, escape_hatch_tx const& tx)
{
    join_split_inputs inputs = {
        witness_ct(&composer, tx.js_tx.public_input),
        witness_ct(&composer, tx.js_tx.public_output),
        witness_ct(&composer, tx.js_tx.asset_id),
        witness_ct(&composer, tx.js_tx.num_input_notes),
        witness_ct(&composer, tx.js_tx.input_index[0]),
        witness_ct(&composer, tx.js_tx.input_index[1]),
        value::witness_data(composer, tx.js_tx.input_note[0]),
        value::witness_data(composer, tx.js_tx.input_note[1]),
        value::witness_data(composer, tx.js_tx.output_note[0]),
        value::witness_data(composer, tx.js_tx.output_note[1]),
        claim::claim_note_tx_witness_data(composer, tx.js_tx.claim_note),
        { witness_ct(&composer, tx.js_tx.signing_pub_key.x), witness_ct(&composer, tx.js_tx.signing_pub_key.y) },
        stdlib::schnorr::convert_signature(&composer, tx.js_tx.signature),
        witness_ct(&composer, tx.js_tx.old_data_root),
        merkle_tree::create_witness_hash_path(composer, tx.js_tx.input_path[0]),
        merkle_tree::create_witness_hash_path(composer, tx.js_tx.input_path[1]),
        witness_ct(&composer, tx.js_tx.account_index),
        merkle_tree::create_witness_hash_path(composer, tx.js_tx.account_path),
        witness_ct(&composer, tx.js_tx.input_owner),
        witness_ct(&composer, tx.js_tx.output_owner),
        witness_ct(&composer, static_cast<fr>(tx.js_tx.account_private_key)),
        witness_ct(&composer, tx.js_tx.alias_hash),
        witness_ct(&composer, tx.js_tx.nonce),
    };

    auto outputs = join_split_circuit_component(composer, inputs);
    outputs.tx_fee.assert_is_zero("tx_fee not zero");

    auto one = uint32_ct(1);
    auto rollup_id = field_ct(witness_ct(&composer, tx.rollup_id));
    auto old_data_root = field_ct(witness_ct(&composer, tx.js_tx.old_data_root));
    auto new_data_root = field_ct(witness_ct(&composer, tx.new_data_root));
    auto old_data_roots_root = field_ct(witness_ct(&composer, tx.old_data_roots_root));
    auto new_data_roots_root = field_ct(witness_ct(&composer, tx.new_data_roots_root));
    auto old_null_root = field_ct(witness_ct(&composer, tx.old_null_root));
    auto data_start_index = field_ct(witness_ct(&composer, tx.data_start_index));
    const auto new_null_roots = map(tx.new_null_roots, [&](auto& r) { return field_ct(witness_ct(&composer, r)); });
    const auto old_null_paths = map(tx.old_null_paths, [&](auto& p) { return create_witness_hash_path(composer, p); });

    auto new_null_root = rollup::check_nullifiers_inserted(
        composer, new_null_roots, old_null_paths, one, old_null_root, { outputs.nullifier1, outputs.nullifier2 });

    root_rollup::check_root_tree_updated(composer,
                                         create_witness_hash_path(composer, tx.old_data_roots_path),
                                         rollup_id,
                                         new_data_root,
                                         new_data_roots_root,
                                         old_data_roots_root);

    rollup::check_data_tree_updated(
        composer,
        1,
        create_witness_hash_path(composer, tx.old_data_path),
        { byte_array_ct(&composer).write(outputs.output_note1.x).write(outputs.output_note1.y),
          byte_array_ct(&composer).write(outputs.output_note2.x).write(outputs.output_note2.y) },
        old_data_root,
        new_data_root,
        data_start_index);

    // Public inputs mimick a 1 rollup, minus the pairing point at the end.
    composer.set_public_input(rollup_id.witness_index);
    public_witness_ct(&composer, 0); // rollup_size. 0 implies escape hatch.
    composer.set_public_input(data_start_index.witness_index);
    composer.set_public_input(old_data_root.witness_index);
    composer.set_public_input(new_data_root.witness_index);
    composer.set_public_input(old_null_root.witness_index);
    composer.set_public_input(new_null_root.witness_index);
    composer.set_public_input(old_data_roots_root.witness_index);
    composer.set_public_input(new_data_roots_root.witness_index);
    public_witness_ct(&composer, 0); // old_defi_root.
    public_witness_ct(&composer, 0); // new_defi_root.
    for (size_t i = 0; i < NUM_BRIDGE_CALLS_PER_BLOCK; ++i) {
        auto zero_bridge_id = public_witness_ct(&composer, 0);
        composer.assert_equal_constant(zero_bridge_id.witness_index, 0);
    }
    for (size_t i = 0; i < NUM_BRIDGE_CALLS_PER_BLOCK; ++i) {
        auto zero_deposit_sum = public_witness_ct(&composer, 0);
        composer.assert_equal_constant(zero_deposit_sum.witness_index, 0);
    }
    for (size_t j = 0; j < NUM_ASSETS; ++j) {
        auto zero_fee = public_witness_ct(&composer, 0);
        composer.assert_equal_constant(zero_fee.witness_index, 0);
    }

    // "Inner proof".
    public_witness_ct(&composer, 0); // proof_id.
    composer.set_public_input(inputs.public_input.witness_index);
    composer.set_public_input(inputs.public_output.witness_index);
    composer.set_public_input(inputs.asset_id.witness_index);
    composer.set_public_input(outputs.output_note1.x.witness_index);
    composer.set_public_input(outputs.output_note1.y.witness_index);
    composer.set_public_input(outputs.output_note2.x.witness_index);
    composer.set_public_input(outputs.output_note2.y.witness_index);
    composer.set_public_input(outputs.nullifier1.witness_index);
    composer.set_public_input(outputs.nullifier2.witness_index);
    public_witness_ct(&composer, tx.js_tx.input_owner);
    public_witness_ct(&composer, tx.js_tx.output_owner);

    for (size_t i = 0; i < NUM_BRIDGE_CALLS_PER_BLOCK; ++i) {
        auto empty_interaction_note_x = public_witness_ct(&composer, 0);
        auto empty_interaction_note_y = public_witness_ct(&composer, 0);
        composer.assert_equal_constant(empty_interaction_note_x.witness_index, 0);
        composer.assert_equal_constant(empty_interaction_note_y.witness_index, 0);
    }
    public_witness_ct(&composer, 0); // previous_defi_interaction_hash
}

} // namespace escape_hatch
} // namespace proofs
} // namespace rollup
