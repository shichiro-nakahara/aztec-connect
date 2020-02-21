#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "../bool/bool.hpp"
#include "../byte_array/byte_array.hpp"
#include "../common.hpp"
#include "../uint32/uint32.hpp"

namespace waffle {
class StandardComposer;
class BoolComposer;
class MiMCComposer;
class TurboComposer;
class TurboComposer;
} // namespace waffle

namespace plonk {
namespace stdlib {

template <typename ComposerContext> class bitarray {
  public:
    bitarray(ComposerContext* parent_context, const size_t n);
    bitarray(ComposerContext* parent_context, const std::string& input);
    bitarray(ComposerContext* parent_context, const std::vector<uint8_t>& input);
    bitarray(const std::vector<uint32<ComposerContext>>& input);
    bitarray(uint32<ComposerContext> const& input);
    bitarray(byte_array<ComposerContext> const& input)
        : context(input.get_context())
        , length(input.size() * 8)
        , values(input.bits())
    {
        std::reverse(values.begin(), values.end());
    }

    template <size_t N> bitarray(const std::array<uint32<ComposerContext>, N>& input)
    {
        context = nullptr;
        for (const auto& x : input) {
            if (x.get_context() != nullptr) {
                context = x.get_context();
                break;
            }
        }

        size_t num_words = static_cast<size_t>(N);
        values.resize(num_words * 32);
        for (size_t i = 0; i < num_words; ++i) {
            size_t input_index = num_words - 1 - i;
            for (size_t j = 0; j < 32; ++j) {
                values[i * 32 + j] = input[input_index].at(j);
            }
        }
        length = num_words * 32;
    }

    bitarray(const bitarray& other);
    bitarray(bitarray&& other);

    bitarray& operator=(const bitarray& other);
    bitarray& operator=(bitarray&& other);

    bool_t<ComposerContext>& operator[](const size_t idx);
    bool_t<ComposerContext> operator[](const size_t idx) const;

    operator byte_array<ComposerContext>()
    {
        typename byte_array<ComposerContext>::bits_t bits(values);
        std::reverse(bits.begin(), bits.end());
        return byte_array(context, bits);
    };

    template <size_t N> operator std::array<uint32<ComposerContext>, N>()
    {
        ASSERT(N * 32 == length);
        std::array<uint32<ComposerContext>, N> output;
        for (size_t i = 0; i < N; ++i) {
            std::array<bool_t<ComposerContext>, 32> bools;
            size_t end;
            size_t start;
            start = ((N - i) * 32) - 32;
            end = start + 32 > length ? length : start + 32;
            for (size_t j = start; j < end; ++j) {
                bools[j - start] = values[j];
            }
            if (start + 32 > length) {
                for (size_t j = end; j < start + 32; ++j) {
                    bools[j - start] = bool_t<ComposerContext>(context, false);
                }
            }
            output[i] = uint32<ComposerContext>(context, bools);
        }
        return output;
    }

    std::vector<uint32<ComposerContext>> to_uint32_vector();

    template <size_t N>
    void populate_uint32_array(const size_t starting_index, std::array<uint32<ComposerContext>, N>& output)
    {
        // ASSERT(N * 32 == (length - starting_index));

        size_t num_uint32s = (length / 32) + (length % 32 != 0);
        size_t num_selected_uint32s = N;

        size_t count = 0;
        for (size_t i = (0); i < num_selected_uint32s; ++i) {
            std::array<bool_t<ComposerContext>, 32> bools;
            size_t end;
            size_t start;
            start = ((num_uint32s - i) * 32) - 32;
            end = start + 32 > length ? length : start + 32;
            for (size_t j = start; j < end; ++j) {
                bools[j - start] = values[j - starting_index];
            }
            if (start + 32 > length) {
                for (size_t j = end; j < start + 32; ++j) {
                    bools[j - start] = bool_t<ComposerContext>(context, false);
                }
            }

            output[count] = uint32<ComposerContext>(context, bools);
            ++count;
        }
    }

    std::string get_witness_as_string() const;

    size_t size() const { return length; }

    ComposerContext* get_context() const { return context; }

    void print() const
    {
        size_t num_ulongs = (length / 32) + (length % 32 != 0);
        std::vector<uint32_t> ulong_vector(num_ulongs, 0);
        for (size_t i = 0; i < length; ++i) {
            size_t ulong_index = i / 32;
            uint32_t shift = static_cast<uint32_t>(i - (ulong_index * 32));
            ulong_vector[num_ulongs - 1 - ulong_index] =
                ulong_vector[num_ulongs - 1 - ulong_index] + (static_cast<uint32_t>(values[i].get_value()) << shift);
        }
        printf("[");
        for (size_t i = 0; i < num_ulongs; ++i) {
            printf(" %x", (ulong_vector[i]));
        }
        printf(" ]\n");
    }

  private:
    ComposerContext* context;
    size_t length;
    std::vector<bool_t<ComposerContext>> values;
};

extern template class bitarray<waffle::StandardComposer>;
extern template class bitarray<waffle::BoolComposer>;
extern template class bitarray<waffle::MiMCComposer>;
extern template class bitarray<waffle::TurboComposer>;

} // namespace stdlib
} // namespace plonk
