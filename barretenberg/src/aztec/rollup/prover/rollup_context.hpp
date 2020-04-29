#pragma once
#include <stdlib/merkle_tree/hash_path.hpp>
#include <stdlib/merkle_tree/leveldb_tree.hpp>
#include <stdlib/merkle_tree/leveldb_store.hpp>
#include <stdlib/types/turbo.hpp>

namespace rollup {
namespace prover {

using namespace plonk::stdlib::types::turbo;

typedef stdlib::merkle_tree::fr_hash_path fr_hash_path;
typedef stdlib::merkle_tree::hash_path<Composer> hash_path;
typedef stdlib::merkle_tree::LevelDbStore leveldb_store;
typedef stdlib::merkle_tree::LevelDbTree leveldb_tree;

struct rollup_context {
    Composer& composer;
    leveldb_tree data_db;
    leveldb_tree nullifier_db;
    field_ct data_size;
    field_ct data_root;
    field_ct nullifier_root;
};

} // namespace prover
} // namespace rollup