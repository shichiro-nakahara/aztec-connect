#pragma once
#include "hash_path.hpp"
#include <stdlib/primitives/field/field.hpp>

namespace plonk {
namespace stdlib {
namespace merkle_tree {

using namespace barretenberg;

class LevelDbStore;

class LevelDbTree {
  public:
    typedef uint128_t index_t;
    typedef std::vector<uint8_t> value_t;

    LevelDbTree(LevelDbStore& store, size_t depth, std::string const& name = "main");
    LevelDbTree(LevelDbTree const& other) = delete;
    LevelDbTree(LevelDbTree&& other);
    ~LevelDbTree();

    fr_hash_path get_hash_path(index_t index);

    fr update_element(index_t index, value_t const& value);

    value_t get_element(index_t index);

    fr root() const;

    size_t depth() const { return depth_; }

    index_t size() const;

  private:
    void load_metadata();

    fr update_element(fr const& root, fr const& value, index_t index, size_t height);

    fr get_element(fr const& root, index_t index, size_t height);

    fr compute_zero_path_hash(size_t height, index_t index, fr const& value);

    fr binary_put(index_t a_index, fr const& a, fr const& b, size_t height);

    fr fork_stump(
        fr const& value1, index_t index1, fr const& value2, index_t index2, size_t height, size_t stump_height);

    void put(fr const& key, fr const& left, fr const& right);

    void put_stump(fr const& key, index_t index, fr const& value);

  private:
    static constexpr size_t LEAF_BYTES = 64;
    LevelDbStore& store_;
    std::vector<fr> zero_hashes_;
    size_t depth_;
    std::string name_;
};

} // namespace merkle_tree
} // namespace stdlib
} // namespace plonk