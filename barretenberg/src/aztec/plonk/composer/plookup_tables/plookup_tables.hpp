#pragma once

#include "types.hpp"
#include "sha256.hpp"
#include "aes128.hpp"

namespace waffle {
namespace plookup {

const PLookupMultiTable& get_multi_table(const PLookupMultiTableId id);

PLookupReadData get_multi_table_values(const PLookupMultiTableId id, const barretenberg::fr& key);

inline PLookupTable create_table(const PLookupTableId id, const size_t index)
{
    switch (id) {
    case AES_SPARSE_MAP: {
        return aes128_tables::generate_aes_sparse_table(AES_SPARSE_MAP, index);
    }
    case AES_SBOX_MAP: {
        return aes128_tables::generate_aes_sbox_table(AES_SBOX_MAP, index);
    }
    case AES_SPARSE_NORMALIZE: {
        return aes128_tables::generate_aes_sparse_normalization_table(AES_SPARSE_NORMALIZE, index);
    }
    case SHA256_WITNESS_NORMALIZE: {
        return sha256_tables::generate_witness_extension_normalization_table(SHA256_WITNESS_NORMALIZE, index);
    }
    case SHA256_WITNESS_SLICE_3: {
        return sha256_tables::generate_witness_extension_table<16, 3, 0, 0>(SHA256_WITNESS_SLICE_3, index);
    }
    case SHA256_WITNESS_SLICE_7_ROTATE_4: {
        return sha256_tables::generate_witness_extension_table<16, 7, 4, 0>(SHA256_WITNESS_SLICE_7_ROTATE_4, index);
    }
    case SHA256_WITNESS_SLICE_8_ROTATE_7: {
        return sha256_tables::generate_witness_extension_table<16, 8, 7, 0>(SHA256_WITNESS_SLICE_8_ROTATE_7, index);
    }
    case SHA256_WITNESS_SLICE_14_ROTATE_1: {
        return sha256_tables::generate_witness_extension_table<16, 14, 1, 0>(SHA256_WITNESS_SLICE_14_ROTATE_1, index);
    }
    case SHA256_CH_NORMALIZE: {
        return sha256_tables::generate_choose_normalization_table(SHA256_CH_NORMALIZE, index);
    }
    case SHA256_MAJ_NORMALIZE: {
        return sha256_tables::generate_majority_normalization_table(SHA256_MAJ_NORMALIZE, index);
    }
    case SHA256_BASE28: {
        return sha256_tables::generate_sha256_sparse_table<28, 0>(SHA256_BASE28, index);
    }
    case SHA256_BASE28_ROTATE6: {
        return sha256_tables::generate_sha256_sparse_table<28, 6>(SHA256_BASE28_ROTATE6, index);
    }
    case SHA256_BASE28_ROTATE3: {
        return sha256_tables::generate_sha256_sparse_table<28, 3>(SHA256_BASE28_ROTATE3, index);
    }
    case SHA256_BASE16: {
        return sha256_tables::generate_sha256_sparse_table<16, 0>(SHA256_BASE16, index);
    }
    case SHA256_BASE16_ROTATE2: {
        return sha256_tables::generate_sha256_sparse_table<16, 2>(SHA256_BASE16_ROTATE2, index);
    }
    default: {
        throw;
    }
    }
}
} // namespace plookup
} // namespace waffle