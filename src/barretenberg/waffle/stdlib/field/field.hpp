#pragma once

#include "../../../curves/bn254/fr.hpp"
#include "../bool/bool.hpp"
#include "../common.hpp"

namespace plonk {
namespace stdlib {

template <typename ComposerContext> class field_t {
  public:
    field_t(ComposerContext* parent_context = nullptr);
    field_t(ComposerContext* parent_context, const barretenberg::fr::field_t& value);
    field_t(const uint64_t value);
    field_t(const witness_t<ComposerContext>& value);
    field_t(const field_t& other);
    field_t(field_t&& other);

    field_t(const bool_t<ComposerContext>& other);
    operator bool_t<ComposerContext>();

    field_t& operator=(const field_t& other);
    field_t& operator=(field_t&& other);

    field_t operator+(const field_t& other) const;
    field_t operator-(const field_t& other) const;
    field_t operator*(const field_t& other) const;
    field_t operator/(const field_t& other) const;

    bool_t<ComposerContext> operator==(const field_t& other) const;

    field_t normalize() const;

    barretenberg::fr::field_t get_value() const;

    bool is_constant() const { return witness_index == static_cast<uint32_t>(-1); }

    mutable ComposerContext* context = nullptr;
    mutable barretenberg::fr::field_t additive_constant;
    mutable barretenberg::fr::field_t multiplicative_constant;
    mutable uint32_t witness_index = static_cast<uint32_t>(-1);
};

template <typename ComposerContext> inline std::ostream& operator<<(std::ostream& os, field_t<ComposerContext> const& v)
{
    barretenberg::fr::field_t a = v.get_value();
    os << std::hex << "field: [" << a.data[0] << ", " << a.data[1] << ", " << a.data[2] << ", " << a.data[3] << "]";
    return os;
}

} // namespace stdlib
} // namespace plonk

#include "./field.tcc"