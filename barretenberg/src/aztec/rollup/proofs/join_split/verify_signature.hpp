#include "../notes/pedersen_note.hpp"
#include <stdlib/encryption/schnorr/schnorr.hpp>
#include <stdlib/hash/pedersen/pedersen.hpp>

namespace rollup {
namespace proofs {
namespace join_split {

using namespace notes;

bool verify_signature(std::array<public_note, 4> const& notes,
                      point_ct const& owner_pub_key,
                      schnorr::signature_bits const& signature)
{
    std::array<field_ct, 8> to_compress;
    for (size_t i = 0; i < 4; ++i) {
        to_compress[i * 2] = notes[i].ciphertext.x;
        to_compress[i * 2 + 1] = notes[i].ciphertext.y;
    }
    byte_array_ct message = pedersen::compress(to_compress);
    return verify_signature(message, owner_pub_key, signature);
}

} // namespace join_split
} // namespace proofs
} // namespace rollup