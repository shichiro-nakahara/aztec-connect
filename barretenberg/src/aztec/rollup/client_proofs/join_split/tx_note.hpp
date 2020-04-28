#pragma once
#include <ecc/curves/grumpkin/grumpkin.hpp>
#include <common/serialize.hpp>

namespace rollup {
namespace client_proofs {
namespace join_split {

using namespace barretenberg;

struct tx_note {
    grumpkin::g1::affine_element owner;
    uint32_t value;
    barretenberg::fr secret;
};

grumpkin::g1::affine_element encrypt_note(const tx_note& plaintext);
bool decrypt_note(grumpkin::g1::affine_element const& encrypted_note,
                  grumpkin::fr const& private_key,
                  fr const& viewing_key,
                  uint32_t& r);

inline bool operator==(tx_note const& lhs, tx_note const& rhs) {
    return lhs.owner == rhs.owner && lhs.value == rhs.value && lhs.secret == rhs.secret;
}

inline std::ostream& operator<<(std::ostream& os, tx_note const& note)
{
    os << "{ owner_x: " << note.owner.x << ", owner_y: " << note.owner.y << ", view_key: " << note.secret
       << ", value: " << note.value << " }";
    return os;
}

inline void read(uint8_t*& it, tx_note& note)
{
    read(it, note.owner);
    ::read(it, note.value);
    read(it, note.secret);
}

inline void write(std::vector<uint8_t>& buf, tx_note const& note)
{
    write(buf, note.owner);
    ::write(buf, note.value);
    write(buf, note.secret);
}

} // namespace join_split
} // namespace client_proofs
} // namespace rollup