#pragma once

#include <ecc/curves/bn254/fr.hpp>
#include <ecc/curves/bn254/fq.hpp>
#include <numeric/uint256/uint256.hpp>
#include <numeric/uintx/uintx.hpp>

#include "../field/field.hpp"

#include "../composers/composers_fwd.hpp"

namespace plonk {
namespace stdlib {

template <typename Composer, typename T> class bigfield {
  public:
    static constexpr uint64_t NUM_LIMB_BITS = 68;
    static constexpr uint64_t LOG2_BINARY_MODULUS = NUM_LIMB_BITS * 4;

    static constexpr uint512_t modulus_u512 =
        uint512_t(uint256_t(T::modulus_0, T::modulus_1, T::modulus_2, T::modulus_3));

    static constexpr uint64_t NUM_LAST_LIMB_BITS = modulus_u512.get_msb() + 1 - (NUM_LIMB_BITS * 3);

    static constexpr uint256_t DEFAULT_MAXIMUM_LIMB = (uint256_t(1) << NUM_LIMB_BITS) - uint256_t(1);
    static constexpr uint256_t DEFAULT_MAXIMUM_MOST_SIGNIFICANT_LIMB =
        (uint256_t(1) << NUM_LAST_LIMB_BITS) - uint256_t(1);

    struct Basis {
        uint512_t modulus;
        size_t num_bits;
    };
    struct Limb {
        Limb() {}
        Limb(const field_t<Composer>& input, const uint256_t max = uint256_t(0))
            : element(input)
        {
            if (input.witness_index == UINT32_MAX) {
                maximum_value = uint256_t(input.additive_constant);
            } else if (max != uint256_t(0)) {
                maximum_value = max;
            } else {
                maximum_value = DEFAULT_MAXIMUM_LIMB;
            }
        }
        Limb(const Limb& other) = default;
        Limb(Limb&& other) = default;
        Limb& operator=(const Limb& other) = default;
        Limb& operator=(Limb&& other) = default;

        field_t<Composer> element;
        uint256_t maximum_value;
    };

    static constexpr uint256_t prime_basis_maximum_limb =
        uint256_t(modulus_u512.slice(NUM_LIMB_BITS * 3, NUM_LIMB_BITS * 4));
    static constexpr Basis prime_basis{ uint512_t(barretenberg::fr::modulus), barretenberg::fr::modulus.get_msb() + 1 };
    static constexpr Basis binary_basis{ uint512_t(1) << LOG2_BINARY_MODULUS, LOG2_BINARY_MODULUS };
    static constexpr Basis target_basis{ modulus_u512, modulus_u512.get_msb() + 1 };

    static constexpr barretenberg::fr shift_1 = barretenberg::fr(uint256_t(1) << NUM_LIMB_BITS);
    static constexpr barretenberg::fr shift_2 = barretenberg::fr(uint256_t(1) << (NUM_LIMB_BITS * 2));
    static constexpr barretenberg::fr shift_3 = barretenberg::fr(uint256_t(1) << (NUM_LIMB_BITS * 3));
    static constexpr barretenberg::fr shift_right_1 = barretenberg::fr(1) / shift_1;
    static constexpr barretenberg::fr shift_right_2 = barretenberg::fr(1) / shift_2;

    static constexpr barretenberg::fr negative_prime_modulus_mod_binary_basis =
        -barretenberg::fr(uint256_t(modulus_u512));

    static constexpr uint512_t get_maximum_unreduced_value()
    {
        uint1024_t maximum_product = uint1024_t(binary_basis.modulus) * uint1024_t(prime_basis.modulus);
        // TODO: compute square root
        uint64_t maximum_product_bits = maximum_product.get_msb() - 1;
        return uint512_t(1) << (maximum_product_bits >> 1);
    }
    static constexpr uint256_t get_maximum_unreduced_limb_value() { return uint256_t(1) << 120; }

    static constexpr uint512_t negative_prime_modulus = binary_basis.modulus - target_basis.modulus;

    static constexpr uint256_t neg_modulus_limbs_u256[4]{
        uint256_t(negative_prime_modulus.slice(0, NUM_LIMB_BITS).lo),
        uint256_t(negative_prime_modulus.slice(NUM_LIMB_BITS, NUM_LIMB_BITS * 2).lo),
        uint256_t(negative_prime_modulus.slice(NUM_LIMB_BITS * 2, NUM_LIMB_BITS * 3).lo),
        uint256_t(negative_prime_modulus.slice(NUM_LIMB_BITS * 3, NUM_LIMB_BITS * 4).lo),
    };

    static constexpr barretenberg::fr neg_modulus_limbs[4]{
        barretenberg::fr(negative_prime_modulus.slice(0, NUM_LIMB_BITS).lo),
        barretenberg::fr(negative_prime_modulus.slice(NUM_LIMB_BITS, NUM_LIMB_BITS * 2).lo),
        barretenberg::fr(negative_prime_modulus.slice(NUM_LIMB_BITS * 2, NUM_LIMB_BITS * 3).lo),
        barretenberg::fr(negative_prime_modulus.slice(NUM_LIMB_BITS * 3, NUM_LIMB_BITS * 4).lo),
    };

    bigfield(const field_t<Composer>& low_bits, const field_t<Composer>& high_bits, const bool can_overflow = false);
    bigfield(Composer* parent_context = nullptr);
    bigfield(Composer* parent_context, const uint256_t& value);
    bigfield(const witness_t<Composer>& a,
             const witness_t<Composer>& b,
             const witness_t<Composer>& c,
             const witness_t<Composer>& d,
             const bool can_overflow = false);

    bigfield(const bigfield& other);
    bigfield(bigfield&& other);

    bigfield& operator=(const bigfield& other);
    bigfield& operator=(bigfield&& other);

    bigfield operator+(const bigfield& other) const;
    bigfield operator-(const bigfield& other) const;
    bigfield operator*(const bigfield& other) const;
    bigfield operator/(const bigfield& other) const;

    bigfield sqr() const;

    static void evaluate_product(const bigfield& left,
                                 const bigfield& right,
                                 const bigfield& quotient,
                                 const bigfield& remainder);

    static void evaluate_square(const bigfield& left, const bigfield& quotient, const bigfield& remainder);

    uint512_t get_value() const;
    uint512_t get_maximum_value() const;

    bool is_constant() const { return prime_basis_limb.witness_index == UINT32_MAX; }

    bigfield conditional_negate(const bool_t<Composer>& predicate) const;
    bigfield conditional_select(const bigfield& other, const bool_t<Composer>& predicate) const;

    void reduction_check() const;
    void self_reduce() const;

    static bigfield one()
    {
        bigfield result(nullptr, uint256_t(1));
        return result;
    }

    void assert_equal(const bigfield&)
    {
        // TODO fill this in...
    }
    // static constexpr bigfield neg_one()
    // {
    //     bigfield result(nullptr, prime_basis.modulus.lo - uint256_t(1));
    //     return result;
    // }

    Composer* context;
    mutable Limb binary_basis_limbs[4];
    mutable field_t<Composer> prime_basis_limb;
}; // namespace stdlib

template <typename C, typename T> inline std::ostream& operator<<(std::ostream& os, bigfield<T, C> const& v)
{
    return os << v.get_value();
}

} // namespace stdlib
} // namespace plonk

#include "bigfield_impl.hpp"