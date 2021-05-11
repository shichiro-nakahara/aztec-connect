#include "claim_note.hpp"
#include "../constants.hpp"
#include <crypto/pedersen/pedersen.hpp>

// using namespace barretenberg;

namespace rollup {
namespace proofs {
namespace notes {
namespace native {

grumpkin::g1::affine_element encrypt_note(claim_note const& note)
{
    grumpkin::g1::element p_1 = crypto::pedersen::fixed_base_scalar_mul<NOTE_VALUE_BIT_LENGTH>(note.deposit_value, 0);
    grumpkin::g1::element p_2 = crypto::pedersen::fixed_base_scalar_mul<254>(note.bridge_id.to_field(), 1);

    // deposit value could be zero so we conditionally include its term in the 'sum'
    // bridge_id is always non-zero as it would always contain 'bridge_contract_address'
    // similarly, defi_interaction_nonce can be 0 so we add its term conditionally
    grumpkin::g1::element sum;
    if (note.deposit_value > 0) {
        sum = p_1 + p_2;
    } else {
        sum = p_2;
    }

    grumpkin::g1::affine_element p_3 =
        crypto::pedersen::compress_to_point_native(note.partial_state.x, note.partial_state.y, 2);
    sum += p_3;

    grumpkin::g1::element p_4 = crypto::pedersen::fixed_base_scalar_mul<32>((uint64_t)note.defi_interaction_nonce, 3);
    if (note.defi_interaction_nonce > 0) {
        sum += p_4;
    }
    sum = sum.normalize();

    return { sum.x, sum.y };
}

} // namespace native
} // namespace notes
} // namespace proofs
} // namespace rollup