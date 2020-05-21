#pragma once
#include "compute_rollup_circuit_data.hpp"
#include "create_noop_join_split_proof.hpp"
#include "verify_rollup.hpp"
#include <rollup/client_proofs/join_split/join_split_data.hpp>
#include <stdlib/merkle_tree/leveldb_tree.hpp>
#include <stdlib/merkle_tree/memory_store.hpp>
#include <stdlib/merkle_tree/memory_tree.hpp>

namespace rollup {
namespace rollup_proofs {

using namespace rollup::client_proofs::join_split;

template <typename Tree>
rollup_tx create_rollup(uint32_t rollup_id,
                        std::vector<std::vector<uint8_t>> const& txs,
                        Tree& data_tree,
                        Tree& null_tree,
                        Tree& root_tree,
                        size_t rollup_size,
                        std::vector<uint8_t> padding_proof)
{
    size_t rollup_tree_depth = numeric::get_msb(rollup_size) + 1;
    MemoryTree rollup_tree(rollup_tree_depth);

    // Compute data tree data.
    auto num_txs = (uint32_t)txs.size();
    auto data_start_index = (uint32_t)data_tree.size();
    auto old_data_root = data_tree.root();
    auto old_data_path = data_tree.get_hash_path(data_start_index);
    auto data_roots_index = rollup_id;
    auto data_roots_path = root_tree.get_hash_path(data_roots_index);
    auto root_tree_root = root_tree.root();

    std::vector<fr_hash_path> data_roots_paths;
    std::vector<uint32_t> data_roots_indicies;
    std::vector<uint128_t> nullifier_indicies;
    std::vector<uint8_t> zero_value(64, 0);

    for (size_t i = 0; i < num_txs; ++i) {
        auto proof_data = txs[i];
        auto struct_data = join_split_data(proof_data);
        auto data_value1 = struct_data.new_note1;
        auto data_value2 = struct_data.new_note2;

        data_tree.update_element(data_start_index + i * 2, data_value1);
        data_tree.update_element(data_start_index + i * 2 + 1, data_value2);
        rollup_tree.update_element(i * 2, data_value1);
        rollup_tree.update_element(i * 2 + 1, data_value2);

        data_roots_paths.push_back(data_roots_path);
        data_roots_indicies.push_back((uint32_t)data_roots_index);

        nullifier_indicies.push_back(struct_data.nullifier1);
        nullifier_indicies.push_back(struct_data.nullifier2);
    }

    auto data_root = to_buffer(data_tree.root());
    root_tree.update_element(rollup_id + 1, data_root);

    // Compute nullifier tree data.
    auto old_null_root = null_tree.root();
    std::vector<fr> new_null_roots;
    std::vector<fr_hash_path> old_null_paths;
    std::vector<fr_hash_path> new_null_paths;

    auto nullifier_value = std::vector<uint8_t>(64, 0);
    nullifier_value[63] = 1;

    for (size_t i = 0; i < nullifier_indicies.size(); ++i) {
        old_null_paths.push_back(null_tree.get_hash_path(nullifier_indicies[i]));
        null_tree.update_element(nullifier_indicies[i], nullifier_value);
        new_null_paths.push_back(null_tree.get_hash_path(nullifier_indicies[i]));
        new_null_roots.push_back(null_tree.root());
    }

    // Compose our rollup.
    rollup_tx rollup = {
        rollup_id,
        num_txs,
        data_start_index,
        txs,
        rollup_tree.root(),
        old_data_root,
        data_tree.root(),
        old_data_path,
        data_tree.get_hash_path(data_start_index),
        old_null_root,
        new_null_roots,
        old_null_paths,
        new_null_paths,
        root_tree_root,
        data_roots_paths,
        data_roots_indicies,
    };

    // Add padding data if necessary.
    rollup.txs.resize(rollup_size, padding_proof);
    rollup.new_null_roots.resize(rollup_size * 2, rollup.new_null_roots.back());
    rollup.old_null_paths.resize(rollup_size * 2, rollup.new_null_paths.back());
    rollup.new_null_paths.resize(rollup_size * 2, rollup.new_null_paths.back());
    auto zero_roots_path = root_tree.get_hash_path(0);
    rollup.data_roots_paths.resize(rollup_size, zero_roots_path);
    rollup.data_roots_indicies.resize(rollup_size, 0);

    return rollup;
}

} // namespace rollup_proofs
} // namespace rollup
