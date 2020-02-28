#pragma once

#include "../types.hpp"
#include "../uint256/uint256.hpp"

namespace barretenberg {
namespace group_elements {
template <typename Fq, typename Fr, typename Params> class alignas(64) affine_element {
  public:
    static constexpr affine_element one{ Params::one_x, Params::one_y };

    affine_element() noexcept {}

    constexpr affine_element(const Fq& a, const Fq& b) noexcept;

    constexpr affine_element(const affine_element& other) noexcept;

    constexpr affine_element(affine_element&& other) noexcept;

    explicit constexpr affine_element(const uint256_t& compressed) noexcept;

    constexpr affine_element& operator=(const affine_element& other) noexcept;

    constexpr affine_element& operator=(affine_element&& other) noexcept;

    explicit constexpr operator uint256_t() const noexcept;

    constexpr affine_element set_infinity() const noexcept;
    constexpr void self_set_infinity() noexcept;

    constexpr bool is_point_at_infinity() const noexcept;

    constexpr bool on_curve() const noexcept;

    static affine_element hash_to_curve(const uint64_t seed) noexcept;

    constexpr bool operator==(const affine_element& other) const noexcept;

    constexpr affine_element operator-() const noexcept { return { x, -y }; }

    static void serialize_to_buffer(const affine_element& value, uint8_t* buffer)
    {
        Fq::serialize_to_buffer(value.y, buffer);
        Fq::serialize_to_buffer(value.x, buffer + sizeof(Fq));
        if (!value.on_curve()) {
            buffer[0] = buffer[0] | (1 << 7);
        }
    }

    static affine_element serialize_from_buffer(uint8_t* buffer)
    {
        affine_element result;
        result.y = Fq::serialize_from_buffer(buffer);
        result.x = Fq::serialize_from_buffer(buffer + sizeof(Fq));
        if (((buffer[0] >> 7) & 1) == 1) {
            result.self_set_infinity();
        }
        return result;
    }
    Fq x;
    Fq y;
};
} // namespace group_elements
} // namespace barretenberg

#include "./affine_element_impl.hpp"