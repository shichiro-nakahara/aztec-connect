#pragma once
#include "compute_rollup_circuit_data.hpp"
#include "create_noop_join_split_proof.hpp"
#include "verify_rollup.hpp"
#include <stdlib/merkle_tree/leveldb_tree.hpp>
#include <stdlib/merkle_tree/memory_store.hpp>
#include <stdlib/merkle_tree/memory_tree.hpp>

namespace rollup {
namespace rollup_proofs {

template <typename Tree>
rollup_tx create_rollup(
    size_t num_txs, std::vector<std::vector<uint8_t>> const& txs, Tree& data_tree, Tree& null_tree, size_t rollup_size)
{
    size_t rollup_tree_depth = numeric::get_msb(rollup_size) + 1;

    // Compute data tree data.
    MemoryTree rollup_tree(rollup_tree_depth);

    auto old_data_root = data_tree.root();
    auto old_data_path = data_tree.get_hash_path(data_tree.size());

    std::vector<uint128_t> nullifier_indicies;
    std::vector<uint8_t> zero_value(64, 0);

    for (size_t i = 0; i < rollup_size; ++i) {
        auto is_real = num_txs > i;
        auto proof_data = txs[i];
        auto data_value1 =
            is_real ? std::vector(proof_data.begin() + 2 * 32, proof_data.begin() + 2 * 32 + 64) : zero_value;
        auto data_value2 =
            is_real ? std::vector(proof_data.begin() + 4 * 32, proof_data.begin() + 4 * 32 + 64) : zero_value;

        data_tree.update_element(i * 2, data_value1);
        data_tree.update_element(i * 2 + 1, data_value2);
        rollup_tree.update_element(i * 2, data_value1);
        rollup_tree.update_element(i * 2 + 1, data_value2);

        nullifier_indicies.push_back(from_buffer<uint128_t>(proof_data.data(), 7 * 32 + 16));
        nullifier_indicies.push_back(from_buffer<uint128_t>(proof_data.data(), 8 * 32 + 16));
    }

    // Compute nullifier tree data.
    auto old_null_root = null_tree.root();
    std::vector<fr> new_null_roots;
    std::vector<fr_hash_path> old_null_paths;
    std::vector<fr_hash_path> new_null_paths;

    auto nullifier_value = std::vector<uint8_t>(64, 0);
    nullifier_value[63] = 1;

    for (size_t i = 0; i < nullifier_indicies.size(); ++i) {
        auto is_real = num_txs > i / 2;
        old_null_paths.push_back(null_tree.get_hash_path(nullifier_indicies[i]));
        null_tree.update_element(nullifier_indicies[i], is_real ? nullifier_value : zero_value);
        new_null_paths.push_back(null_tree.get_hash_path(nullifier_indicies[i]));
        new_null_roots.push_back(null_tree.root());
    }

    // Compose our rollup.
    rollup_tx rollup = {
        0,
        (uint32_t)num_txs,
        (uint32_t)txs[0].size(),
        0,
        txs,
        rollup_tree.root(),
        old_data_root,
        data_tree.root(),
        old_data_path,
        data_tree.get_hash_path(0),
        old_null_root,
        new_null_roots,
        old_null_paths,
        new_null_paths,
    };

    return rollup;
}

} // namespace rollup_proofs
} // namespace rollup