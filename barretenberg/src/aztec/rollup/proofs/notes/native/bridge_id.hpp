#pragma once
#include <common/serialize.hpp>
#include "../constants.hpp"
#include <crypto/pedersen/pedersen.hpp>
#include <ecc/curves/grumpkin/grumpkin.hpp>

namespace rollup {
namespace proofs {
namespace notes {
namespace native {

struct bridge_id {
    uint256_t bridge_contract_address;
    uint32_t num_output_notes;
    uint32_t input_asset_id;
    uint32_t output_asset_id_a;
    uint32_t output_asset_id_b;

    barretenberg::fr to_field() const
    {
        // check if the address is 160 bits, num_output_notes is 2 bits and output_asset_id_b is 26 bits
        bool address_check = ((bridge_contract_address >> DEFI_BRIDGE_ADDRESS_BIT_LENGTH) == 0);
        bool num_output_notes_check = ((num_output_notes >> DEFI_BRIDGE_NUM_OUTPUT_NOTES_LEN) == 0);
        bool output_asset_id_b_check = ((output_asset_id_b >> DEFI_BRIDGE_OUTPUT_B_ASSET_ID_LEN) == 0);

        if (!(address_check && num_output_notes_check && output_asset_id_b_check)) {
            barretenberg::errors::throw_or_abort("Structure of the bridge_id incorrect!");
        }

        uint32_t num_output_notes_offset = DEFI_BRIDGE_ADDRESS_BIT_LENGTH;
        uint32_t input_asset_id_offset = num_output_notes_offset + DEFI_BRIDGE_NUM_OUTPUT_NOTES_LEN;
        uint32_t output_asset_id_a_offset = input_asset_id_offset + DEFI_BRIDGE_INPUT_ASSET_ID_LEN;
        uint32_t output_asset_id_b_offset = output_asset_id_a_offset + DEFI_BRIDGE_OUTPUT_A_ASSET_ID_LEN;

        uint256_t result = bridge_contract_address +
                           (static_cast<uint256_t>(num_output_notes) * (1UL << num_output_notes_offset)) +
                           (static_cast<uint256_t>(input_asset_id) * (1UL << input_asset_id_offset)) +
                           (static_cast<uint256_t>(output_asset_id_a) * (1UL << output_asset_id_a_offset)) +
                           (static_cast<uint256_t>(input_asset_id) * (1UL << output_asset_id_b_offset));

        return barretenberg::fr(result);
    }
};

inline bool operator==(bridge_id const& lhs, bridge_id const& rhs)
{
    return lhs.bridge_contract_address == rhs.bridge_contract_address && lhs.input_asset_id == rhs.input_asset_id &&
           lhs.num_output_notes == rhs.num_output_notes && lhs.output_asset_id_a == rhs.output_asset_id_a &&
           lhs.output_asset_id_b == rhs.output_asset_id_b;
}

inline std::ostream& operator<<(std::ostream& os, bridge_id const& bridge_id)
{
    os << "{\n"
       << "  bridge_contract_address: " << bridge_id.bridge_contract_address << ",\n"
       << "  num_output_notes: " << bridge_id.num_output_notes << ",\n"
       << "  input_asset_id: " << bridge_id.input_asset_id << ",\n"
       << "  output_asset_id_a: " << bridge_id.output_asset_id_a << ",\n"
       << "  output_asset_id_b: " << bridge_id.output_asset_id_a << "\n}";
    return os;
}

} // namespace native
} // namespace notes
} // namespace proofs
} // namespace rollup