#include "rollup_circuit.hpp"
#include "../inner_proof_data.hpp"
#include <stdlib/merkle_tree/membership.hpp>
#include <common/throw_or_abort.hpp>

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
namespace rollup {
namespace proofs {
namespace rollup {

using namespace plonk::stdlib::types::turbo;
using namespace plonk::stdlib::recursion;
using namespace plonk::stdlib::merkle_tree;

void propagate_inner_proof_public_inputs(Composer& composer, std::vector<field_ct> const& public_inputs)
{
    for (size_t i = 0; i < InnerProofFields::NUM_PUBLISHED; ++i) {
        composer.set_public_input(public_inputs[i].witness_index);
    }
}

field_ct check_nullifiers_inserted(Composer& composer,
                                   std::vector<fr> const& new_null_roots,
                                   std::vector<fr_hash_path> const& old_null_paths,
                                   std::vector<fr_hash_path> const& new_null_paths,
                                   uint32_ct const& num_txs,
                                   field_ct latest_null_root,
                                   std::vector<field_ct> const& new_null_indicies,
                                   bool can_throw)
{

    auto new_nullifier_value = byte_array_ct(&composer, 64);
    new_nullifier_value.set_bit(511, 1);
    field_ct last_real_null_index;

    for (size_t i = 0; i < new_null_indicies.size(); ++i) {
        auto new_null_root = field_ct(witness_ct(&composer, new_null_roots[i]));
        // TODO: i should be able to be a constant, but causes things to fail :/
        auto is_real = num_txs > uint32_ct(witness_ct(&composer, i / 2));

        // This makes padding transactions act as noops.
        last_real_null_index = (new_null_indicies[i] * is_real) + (last_real_null_index * !is_real);
        auto old_nullifier_value = byte_array_ct(&composer, 64);
        old_nullifier_value.set_bit(511, !is_real);

        auto new_null_path = create_witness_hash_path(composer, new_null_paths[i]);
        auto old_null_path = create_witness_hash_path(composer, old_null_paths[i]);

        update_membership(composer,
                          new_null_root,
                          new_null_path,
                          new_nullifier_value,
                          latest_null_root,
                          old_null_path,
                          old_nullifier_value,
                          byte_array_ct(last_real_null_index));
        if (can_throw && composer.failed) {
            throw_or_abort("Failed nullifier update: " + std::to_string(i));
        }

        latest_null_root = new_null_root;
    }

    return latest_null_root;
}

void check_root_tree_updated(Composer& composer,
                             merkle_tree::hash_path const& new_data_roots_path,
                             merkle_tree::hash_path const& old_data_roots_path,
                             field_ct const& rollup_id,
                             field_ct const& new_data_root,
                             field_ct const& new_data_roots_root,
                             field_ct const& old_data_roots_root,
                             bool can_throw)
{

    auto empty_tree_value = byte_array_ct(&composer, 64);
    auto new_data_root_arr = byte_array_ct(new_data_root);
    auto one = field_ct(witness_ct(&composer, 1));
    auto index = byte_array_ct(rollup_id + one);
    update_membership(composer,
                      new_data_roots_root,
                      new_data_roots_path,
                      new_data_root_arr,
                      old_data_roots_root,
                      old_data_roots_path,
                      empty_tree_value,
                      index);
    if (can_throw && composer.failed) {
        throw_or_abort("Failed root tree update.");
    }
}

void check_data_tree_updated(Composer& composer,
                             size_t rollup_size,
                             merkle_tree::hash_path const& new_data_path,
                             merkle_tree::hash_path const& old_data_path,
                             std::vector<byte_array_ct> const& new_data_values,
                             //  field_ct const& rollup_root,
                             field_ct const& old_data_root,
                             field_ct const& new_data_root,
                             field_ct const& data_start_index,
                             bool can_throw)
{
    size_t height = numeric::get_msb(rollup_size) + 1;
    auto zero_subtree_root = field_ct(zero_hash_at_height(height));

    auto rollup_root = compute_tree_root(new_data_values);

    update_subtree_membership(composer,
                              new_data_root,
                              new_data_path,
                              rollup_root,
                              old_data_root,
                              old_data_path,
                              zero_subtree_root,
                              byte_array_ct(data_start_index),
                              height);
    if (can_throw && composer.failed) {
        throw_or_abort("Failed subtree update.");
    }
}

void check_accounts_not_nullified(Composer& composer,
                                  uint32_ct const& num_txs,
                                  field_ct const& null_root,
                                  std::vector<field_ct> const& account_null_indicies,
                                  std::vector<fr_hash_path> const& account_null_paths,
                                  bool can_throw)
{

    // Check that 0 exists at each of the account nullifier indicies.
    for (size_t i = 0; i < account_null_indicies.size(); ++i) {
        auto is_real = num_txs > uint32_ct(witness_ct(&composer, i));
        auto exists = check_membership(composer,
                                       null_root,
                                       create_witness_hash_path(composer, account_null_paths[i]),
                                       byte_array_ct(&composer, 64),
                                       byte_array_ct(account_null_indicies[i]));
        auto good = exists || !is_real;
        composer.assert_equal_constant(good.witness_index, 1);
        if (can_throw && composer.failed) {
            throw_or_abort("Failed account not nullified: " + std::to_string(i));
        }
    }
}

recursion_output<bn254> rollup_circuit(Composer& composer,
                                       rollup_tx const& rollup,
                                       std::vector<std::shared_ptr<waffle::verification_key>> const& verification_keys,
                                       size_t rollup_size,
                                       bool can_throw)
{
    auto rollup_id = field_ct(witness_ct(&composer, rollup.rollup_id));
    auto data_start_index = field_ct(witness_ct(&composer, rollup.data_start_index));
    auto old_data_root = field_ct(witness_ct(&composer, rollup.old_data_root));
    auto new_data_root = field_ct(witness_ct(&composer, rollup.new_data_root));
    auto old_null_root = field_ct(witness_ct(&composer, rollup.old_null_root));
    auto old_data_roots_root = field_ct(witness_ct(&composer, rollup.old_data_roots_root));
    auto new_data_roots_root = field_ct(witness_ct(&composer, rollup.new_data_roots_root));
    auto num_txs = uint32_ct(witness_ct(&composer, rollup.num_txs));

    auto new_data_values = std::vector<byte_array_ct>();
    auto new_null_indicies = std::vector<field_ct>();
    auto account_null_indicies = std::vector<field_ct>();
    auto recursive_manifest = Composer::create_unrolled_manifest(verification_keys[0]->num_public_inputs);

    recursion_output<bn254> recursion_output;
    std::vector<std::vector<field_ct>> inner_public_inputs;

    for (size_t i = 0; i < rollup_size; ++i) {
        // Pick verification key and check it's permitted.
        auto proof_id = from_buffer<uint32_t>(rollup.txs[i], InnerProofOffsets::PROOF_ID + 28);
        auto recursive_verification_key =
            plonk::stdlib::recursion::verification_key<bn254>::from_witness(&composer, verification_keys[proof_id]);
        recursive_verification_key->validate_key_is_in_set(verification_keys);

        // Verify the inner proof.
        recursion_output =
            verify_proof<bn254, recursive_turbo_verifier_settings<bn254>>(&composer,
                                                                          recursive_verification_key,
                                                                          recursive_manifest,
                                                                          waffle::plonk_proof{ rollup.txs[i] },
                                                                          recursion_output);
        if (can_throw && composer.failed) {
            throw_or_abort("Failed to verify proof: " + std::to_string(i));
        }

        // Add the proofs data values to the list. If this is a noop proof (padding), then the data values are zeros.
        // TODO: i should be able to be a constant, but causes things to fail :/
        auto is_real = num_txs > uint32_ct(witness_ct(&composer, i));
        auto public_inputs = recursion_output.public_inputs;
        new_data_values.push_back(byte_array_ct(&composer)
                                      .write(public_inputs[InnerProofFields::NEW_NOTE1_X] * is_real)
                                      .write(public_inputs[InnerProofFields::NEW_NOTE1_Y] * is_real));
        new_data_values.push_back(byte_array_ct(&composer)
                                      .write(public_inputs[InnerProofFields::NEW_NOTE2_X] * is_real)
                                      .write(public_inputs[InnerProofFields::NEW_NOTE2_Y] * is_real));

        // Check this proofs data root exists in the data root tree (unless a padding entry).
        auto data_root = public_inputs[InnerProofFields::MERKLE_ROOT];
        auto data_roots_path = create_witness_hash_path(composer, rollup.data_roots_paths[i]);
        auto data_root_index = uint32_ct(witness_ct(&composer, rollup.data_roots_indicies[i]));
        bool_ct valid = data_root_index <= rollup_id && check_membership(composer,
                                                                         old_data_roots_root,
                                                                         data_roots_path,
                                                                         byte_array_ct(data_root),
                                                                         byte_array_ct(data_root_index));
        composer.assert_equal(is_real.witness_index, valid.witness_index);
        if (can_throw && composer.failed) {
            throw_or_abort("Data root incorrect for proof: " + std::to_string(i));
        }

        new_null_indicies.push_back(public_inputs[InnerProofFields::NULLIFIER1]);
        new_null_indicies.push_back(public_inputs[InnerProofFields::NULLIFIER2]);
        account_null_indicies.push_back(public_inputs[InnerProofFields::ACCOUNT_NULLIFIER]);

        inner_public_inputs.push_back(public_inputs);
    }

    auto new_data_roots_path = create_witness_hash_path(composer, rollup.new_data_roots_path);
    auto old_data_roots_path = create_witness_hash_path(composer, rollup.old_data_roots_path);
    check_root_tree_updated(composer,
                            new_data_roots_path,
                            old_data_roots_path,
                            rollup_id,
                            new_data_root,
                            new_data_roots_root,
                            old_data_roots_root,
                            can_throw);

    // auto rollup_root = field_ct(witness_ct(&composer, rollup.rollup_root));
    auto new_data_path = create_witness_hash_path(composer, rollup.new_data_path);
    auto old_data_path = create_witness_hash_path(composer, rollup.old_data_path);
    check_data_tree_updated(composer,
                            rollup_size,
                            new_data_path,
                            old_data_path,
                            new_data_values,
                            old_data_root,
                            new_data_root,
                            data_start_index,
                            can_throw);

    auto new_null_root = check_nullifiers_inserted(composer,
                                                   rollup.new_null_roots,
                                                   rollup.old_null_paths,
                                                   rollup.new_null_paths,
                                                   num_txs,
                                                   old_null_root,
                                                   new_null_indicies,
                                                   can_throw);

    check_accounts_not_nullified(
        composer, num_txs, old_null_root, account_null_indicies, rollup.account_null_paths, can_throw);

    // Publish public inputs.
    composer.set_public_input(rollup_id.witness_index);
    public_witness_ct(&composer, rollup_size);
    composer.set_public_input(data_start_index.witness_index);
    composer.set_public_input(old_data_root.witness_index);
    composer.set_public_input(new_data_root.witness_index);
    composer.set_public_input(old_null_root.witness_index);
    composer.set_public_input(new_null_root.witness_index);
    composer.set_public_input(old_data_roots_root.witness_index);
    composer.set_public_input(new_data_roots_root.witness_index);
    composer.set_public_input(num_txs.get_witness_index());

    for (auto& inner : inner_public_inputs) {
        propagate_inner_proof_public_inputs(composer, inner);
    }

    // Publish pairing coords limbs as public inputs.
    for (auto coord :
         { &recursion_output.P0.x, &recursion_output.P0.y, &recursion_output.P1.x, &recursion_output.P1.y }) {
        for (size_t i = 0; i < 4; ++i) {
            composer.set_public_input(coord->binary_basis_limbs[i].element.witness_index);
        }
    }

    return recursion_output;
}

} // namespace rollup
} // namespace proofs
} // namespace rollup
