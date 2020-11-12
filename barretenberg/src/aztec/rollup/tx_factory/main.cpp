#include "../proofs/join_split/join_split.hpp"
#include "../proofs/join_split/join_split_tx.hpp"
#include "../proofs/join_split/create_noop_join_split_proof.hpp"
#include "../proofs/rollup/create_rollup.hpp"
#include "../proofs/rollup/rollup_tx.hpp"
#include "../constants.hpp"
#include <common/streams.hpp>
#include <iostream>
#include <stdlib/merkle_tree/leveldb_store.hpp>
#include <stdlib/merkle_tree/merkle_tree.hpp>
#include <stdlib/types/turbo.hpp>

using namespace rollup::proofs::join_split;
using namespace rollup::proofs::rollup;
using namespace plonk::stdlib::merkle_tree;
using namespace plonk::stdlib::types::turbo;

int main(int argc, char** argv)
{
    MemoryStore store;
    MerkleTree<MemoryStore> data_tree(store, rollup::DATA_TREE_DEPTH, 0);
    MerkleTree<MemoryStore> null_tree(store, rollup::NULL_TREE_DEPTH, 1);
    MerkleTree<MemoryStore> root_tree(store, rollup::ROOT_TREE_DEPTH, 2);

    std::vector<std::string> args(argv, argv + argc);

    if (args.size() < 3) {
        std::cerr << "usage: " << args[0] << " <num_txs> <rollup_size>" << std::endl;
        return -1;
    }

    auto data_root = to_buffer(data_tree.root());
    root_tree.update_element(0, data_root);

    const uint32_t num_txs = static_cast<uint32_t>(std::stoul(args[1]));
    const uint32_t rollup_size = static_cast<uint32_t>(std::stoul(args[2]));

    auto join_split_circuit_data = compute_join_split_circuit_data("../srs_db/ignition");

    std::cerr << "Generating a " << rollup_size << " rollup with " << num_txs << " txs..." << std::endl;
    auto proofs = std::vector<std::vector<uint8_t>>(num_txs);
    for (size_t i = 0; i < num_txs; ++i) {
        proofs[i] = create_noop_join_split_proof(join_split_circuit_data, data_tree.root());
    }
    rollup_tx rollup =
        create_rollup(0, proofs, data_tree, null_tree, root_tree, rollup_size, join_split_circuit_data.padding_proof);

    write(std::cout, rollup);

    return 0;
}
