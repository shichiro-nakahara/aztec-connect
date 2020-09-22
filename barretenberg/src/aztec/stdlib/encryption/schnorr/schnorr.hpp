#pragma once
#include <crypto/schnorr/schnorr.hpp>
#include "../../primitives/field/field.hpp"
#include "../../primitives/bool/bool.hpp"
#include "../../primitives/witness/witness.hpp"
#include "../../primitives/bit_array/bit_array.hpp"
#include "../../primitives/byte_array/byte_array.hpp"
#include "../../primitives/point/point.hpp"

namespace plonk {
namespace stdlib {
namespace schnorr {

template <typename C> struct signature_bits {
    bit_array<C> s;
    bit_array<C> e;
};
template <typename C> point<C> variable_base_mul(const point<C>& pub_key, const bit_array<C>& scalar);

template <typename C>
bool verify_signature(const bit_array<C>& message, const point<C>& pub_key, const signature_bits<C>& sig);

template <typename C> signature_bits<C> convert_signature(C* context, const crypto::schnorr::signature& sig);

template <typename C> bit_array<C> convert_message(C* context, const std::string& message_string);

template <typename C>
bool verify_signature(const byte_array<C>& message, const point<C>& pub_key, const signature_bits<C>& sig);

extern template point<waffle::TurboComposer> variable_base_mul<waffle::TurboComposer>(
    const point<waffle::TurboComposer>&, const bit_array<waffle::TurboComposer>&);
extern template point<waffle::PLookupComposer> variable_base_mul<waffle::PLookupComposer>(
    const point<waffle::PLookupComposer>&, const bit_array<waffle::PLookupComposer>&);

extern template bool verify_signature<waffle::TurboComposer>(const bit_array<waffle::TurboComposer>&,
                                                             const point<waffle::TurboComposer>&,
                                                             const signature_bits<waffle::TurboComposer>&);
extern template bool verify_signature<waffle::PLookupComposer>(const bit_array<waffle::PLookupComposer>&,
                                                               const point<waffle::PLookupComposer>&,
                                                               const signature_bits<waffle::PLookupComposer>&);

extern template bool verify_signature<waffle::TurboComposer>(const byte_array<waffle::TurboComposer>&,
                                                             const point<waffle::TurboComposer>&,
                                                             const signature_bits<waffle::TurboComposer>&);
extern template bool verify_signature<waffle::PLookupComposer>(const byte_array<waffle::PLookupComposer>&,
                                                               const point<waffle::PLookupComposer>&,
                                                               const signature_bits<waffle::PLookupComposer>&);

extern template signature_bits<waffle::TurboComposer> convert_signature<waffle::TurboComposer>(
    waffle::TurboComposer*, const crypto::schnorr::signature&);
extern template signature_bits<waffle::PLookupComposer> convert_signature<waffle::PLookupComposer>(
    waffle::PLookupComposer*, const crypto::schnorr::signature&);

extern template bit_array<waffle::TurboComposer> convert_message<waffle::TurboComposer>(waffle::TurboComposer*,
                                                                                        const std::string&);
extern template bit_array<waffle::PLookupComposer> convert_message<waffle::PLookupComposer>(waffle::PLookupComposer*,
                                                                                            const std::string&);

} // namespace schnorr
} // namespace stdlib
} // namespace plonk
