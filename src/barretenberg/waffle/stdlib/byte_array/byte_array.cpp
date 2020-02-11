#include <algorithm>
#include <array>
#include <bitset>
#include <string>

#include "../../composer/bool_composer.hpp"
#include "../../composer/extended_composer.hpp"
#include "../../composer/mimc_composer.hpp"
#include "../../composer/standard_composer.hpp"
#include "../../composer/turbo_composer.hpp"
#include "byte_array.hpp"

namespace plonk {
namespace stdlib {

template <typename ComposerContext>
byte_array<ComposerContext>::byte_array(ComposerContext* parent_context)
    : context(parent_context)
{}

template <typename ComposerContext>
byte_array<ComposerContext>::byte_array(ComposerContext* parent_context, const size_t n)
    : context(parent_context)
    , values(std::vector<bool_t<ComposerContext>>(n))
{}

template <typename ComposerContext>
byte_array<ComposerContext>::byte_array(ComposerContext* parent_context, const std::string& input)
    : context(parent_context)
    , values(input.size() * 8)
{
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        std::bitset<8> char_bits = std::bitset<8>(static_cast<unsigned long long>(c));
        for (size_t j = 0; j < 8; ++j) {
            bool_t<ComposerContext> value(context, char_bits[7 - j]);
            values[(i * 8) + j] = value;
        }
    }
}

template <typename ComposerContext>
byte_array<ComposerContext>::byte_array(ComposerContext* parent_context, bits_t const& input)
    : context(parent_context)
    , values(input)
{}

template <typename ComposerContext>
byte_array<ComposerContext>::byte_array(ComposerContext* parent_context, bits_t&& input)
    : context(parent_context)
    , values(input)
{}

template <typename ComposerContext> byte_array<ComposerContext>::byte_array(const byte_array& other)
{
    context = other.context;
    std::copy(other.values.begin(), other.values.end(), std::back_inserter(values));
}

template <typename ComposerContext> byte_array<ComposerContext>::byte_array(byte_array&& other)
{
    context = other.context;
    values = std::move(other.values);
}

template <typename ComposerContext>
byte_array<ComposerContext>& byte_array<ComposerContext>::operator=(const byte_array& other)
{
    context = other.context;
    values = std::vector<bool_t<ComposerContext>>();
    std::copy(other.values.begin(), other.values.end(), std::back_inserter(values));
    return *this;
}

template <typename ComposerContext>
byte_array<ComposerContext>& byte_array<ComposerContext>::operator=(byte_array&& other)
{
    context = other.context;
    values = std::move(other.values);
    return *this;
}

template <typename ComposerContext> void byte_array<ComposerContext>::write(byte_array const& other)
{
    values.insert(values.end(), other.bits().begin(), other.bits().end());
}

template <typename ComposerContext> byte_array<ComposerContext> byte_array<ComposerContext>::slice(size_t offset) const
{
    ASSERT(offset < values.size());
    return byte_array(context, bits_t(values.begin() + (long)(offset * 8), values.end()));
}

template <typename ComposerContext>
byte_array<ComposerContext> byte_array<ComposerContext>::slice(size_t offset, size_t length) const
{
    ASSERT(offset < values.size());
    ASSERT(length < values.size() - offset);
    auto start = values.begin() + (long)(offset * 8);
    auto end = values.begin() + (long)((offset + length) * 8);
    return byte_array(context, bits_t(start, end));
}

template <typename ComposerContext> std::string byte_array<ComposerContext>::get_value() const
{
    size_t length = values.size();
    size_t num = (length / 8) + (length % 8 != 0);
    std::string bytes(num, 0);
    for (size_t i = 0; i < length; ++i) {
        size_t index = i / 8;
        char shift = static_cast<char>(7 - (i - index * 8));
        char value = static_cast<char>(values[i].get_value() << shift);
        bytes[index] |= value;
    }
    return bytes;
}

template class byte_array<waffle::StandardComposer>;
template class byte_array<waffle::BoolComposer>;
template class byte_array<waffle::MiMCComposer>;
template class byte_array<waffle::ExtendedComposer>;
template class byte_array<waffle::TurboComposer>;

} // namespace stdlib
} // namespace plonk