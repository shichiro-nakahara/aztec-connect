#pragma once

#include <cstdint>
#include <iostream>

#include "../uint256/uint256.hpp"
#include "../utils.hpp"

#if defined(__SIZEOF_INT128__)
__extension__ using uint128_t = unsigned __int128;
#endif

namespace barretenberg {
template <class Params> struct field {
  public:
    field() noexcept {}

    constexpr field(const uint256_t& input) noexcept
        : data{ input.data[0], input.data[1], input.data[2], input.data[3] }
    {
        self_to_montgomery_form();
    }

    constexpr field(const uint64_t input) noexcept
        : data{ input, 0, 0, 0 }
    {
        self_to_montgomery_form();
    }
    constexpr field(const uint64_t a, const uint64_t b, const uint64_t c, const uint64_t d) noexcept
        : data{ a, b, c, d } {};

    constexpr operator uint256_t() const noexcept
    {
        field out = from_montgomery_form();
        return uint256_t(out.data[0], out.data[1], out.data[2], out.data[3]);
    }

    constexpr field(const field& other) = default;
    constexpr field& operator=(const field& other) = default;

    alignas(32) uint64_t data[4];

    static constexpr field modulus = field(Params::modulus_0, Params::modulus_1, Params::modulus_2, Params::modulus_3);
    static constexpr field twice_modulus =
        field(Params::twice_modulus_0, Params::twice_modulus_1, Params::twice_modulus_2, Params::twice_modulus_3);
    static constexpr field two_inv = field(Params::two_inv_0, Params::two_inv_1, Params::two_inv_2, Params::two_inv_3);
    static constexpr field beta =
        field(Params::cube_root_0, Params::cube_root_1, Params::cube_root_2, Params::cube_root_3);
    static constexpr field multiplicative_generator = field(Params::multiplicative_generator_0,
                                                            Params::multiplicative_generator_1,
                                                            Params::multiplicative_generator_2,
                                                            Params::multiplicative_generator_3);
    static constexpr field multiplicative_generator_inverse = field(Params::multiplicative_generator_inverse_0,
                                                                    Params::multiplicative_generator_inverse_1,
                                                                    Params::multiplicative_generator_inverse_2,
                                                                    Params::multiplicative_generator_inverse_3);
    static constexpr field one = field(Params::one_mont_0, Params::one_mont_1, Params::one_mont_2, Params::one_mont_3);
    static constexpr field zero = field(0ULL, 0ULL, 0ULL, 0ULL);
    static constexpr field neg_one = zero - one;
    static constexpr field coset_generators[15] = { field(Params::coset_generators_0[0],
                                                          Params::coset_generators_1[0],
                                                          Params::coset_generators_2[0],
                                                          Params::coset_generators_3[0]),
                                                    field(Params::coset_generators_0[1],
                                                          Params::coset_generators_1[1],
                                                          Params::coset_generators_2[1],
                                                          Params::coset_generators_3[1]),
                                                    field(Params::coset_generators_0[2],
                                                          Params::coset_generators_1[2],
                                                          Params::coset_generators_2[2],
                                                          Params::coset_generators_3[2]),
                                                    field(Params::coset_generators_0[3],
                                                          Params::coset_generators_1[3],
                                                          Params::coset_generators_2[3],
                                                          Params::coset_generators_3[3]),
                                                    field(Params::coset_generators_0[4],
                                                          Params::coset_generators_1[4],
                                                          Params::coset_generators_2[4],
                                                          Params::coset_generators_3[4]),
                                                    field(Params::coset_generators_0[5],
                                                          Params::coset_generators_1[5],
                                                          Params::coset_generators_2[5],
                                                          Params::coset_generators_3[5]),
                                                    field(Params::coset_generators_0[6],
                                                          Params::coset_generators_1[6],
                                                          Params::coset_generators_2[6],
                                                          Params::coset_generators_3[6]),
                                                    field(Params::coset_generators_0[7],
                                                          Params::coset_generators_1[7],
                                                          Params::coset_generators_2[7],
                                                          Params::coset_generators_3[7]),
                                                    field(Params::coset_generators_0[8],
                                                          Params::coset_generators_1[8],
                                                          Params::coset_generators_2[8],
                                                          Params::coset_generators_3[8]),
                                                    field(Params::coset_generators_0[9],
                                                          Params::coset_generators_1[9],
                                                          Params::coset_generators_2[9],
                                                          Params::coset_generators_3[9]),
                                                    field(Params::coset_generators_0[10],
                                                          Params::coset_generators_1[10],
                                                          Params::coset_generators_2[10],
                                                          Params::coset_generators_3[10]),
                                                    field(Params::coset_generators_0[11],
                                                          Params::coset_generators_1[11],
                                                          Params::coset_generators_2[11],
                                                          Params::coset_generators_3[11]),
                                                    field(Params::coset_generators_0[12],
                                                          Params::coset_generators_1[12],
                                                          Params::coset_generators_2[12],
                                                          Params::coset_generators_3[12]),
                                                    field(Params::coset_generators_0[13],
                                                          Params::coset_generators_1[13],
                                                          Params::coset_generators_2[13],
                                                          Params::coset_generators_3[13]),
                                                    field(Params::coset_generators_0[14],
                                                          Params::coset_generators_1[14],
                                                          Params::coset_generators_2[14],
                                                          Params::coset_generators_3[14]) };

    // static constexpr std::array<field, 15> coset_generators = compute_coset_generators();

    // constexpr field() noexcept
    // {
    //     if constexpr (unstd::is_constant_evaluated()) {
    //         data[0] = 0;
    //         data[1] = 1;
    //         data[2] = 2;
    //         data[3] = 3;
    //     }
    // }

    // constexpr field() = default;

    // constexpr field(uint64_t* inputs) noexcept
    //     : data{ inputs }
    // {}

    // constexpr field(const field& other) = default;
    // constexpr field(field&& other) = default;

    // constexpr field& operator=(const field& other) = default;
    // constexpr field& operator=(field&& other) = default;
    // // field() noexcept {}
    // constexpr field() noexcept
    //     : data()
    // {}
    // constexpr field(const uint64_t a, const uint64_t b, const uint64_t c, const uint64_t d) noexcept
    //     : data{ a, b, c, d }
    // {}

    // constexpr field(const std::array<uint64_t, 4>& input) noexcept
    //     : data{ input[0], input[1], input[2], input[3] }
    // {}

    // constexpr field(const field& other) noexcept
    //     : data{ other.data[0], other.data[1], other.data[2], other.data[3] }
    // {}

    // constexpr field& operator=(const field& other) = default;
    // constexpr field& operator=(field&& other) = default;

    BBERG_INLINE constexpr field operator*(const field& other) const noexcept;
    BBERG_INLINE constexpr field operator+(const field& other) const noexcept;
    BBERG_INLINE constexpr field operator-(const field& other) const noexcept;
    BBERG_INLINE constexpr field operator-() const noexcept;
    constexpr field operator/(const field& other) const noexcept;

    BBERG_INLINE constexpr field operator*=(const field& other) noexcept;
    BBERG_INLINE constexpr field operator+=(const field& other) noexcept;
    BBERG_INLINE constexpr field operator-=(const field& other) noexcept;
    constexpr field operator/=(const field& other) noexcept;

    BBERG_INLINE constexpr bool operator>(const field& other) const noexcept;
    BBERG_INLINE constexpr bool operator<(const field& other) const noexcept;
    BBERG_INLINE constexpr bool operator==(const field& other) const noexcept;
    BBERG_INLINE constexpr bool operator!=(const field& other) const noexcept;

    BBERG_INLINE constexpr field to_montgomery_form() const noexcept;
    BBERG_INLINE constexpr field from_montgomery_form() const noexcept;

    BBERG_INLINE constexpr field sqr() const noexcept;
    BBERG_INLINE constexpr void self_sqr() noexcept;

    BBERG_INLINE constexpr field pow(const field& exponent) const noexcept;
    BBERG_INLINE constexpr field pow(const uint64_t exponent) const noexcept;
    constexpr field invert() const noexcept;
    static void batch_invert(field* coeffs, const size_t n) noexcept;
    constexpr field sqrt() const noexcept;

    BBERG_INLINE constexpr void self_neg() noexcept;

    BBERG_INLINE constexpr void self_to_montgomery_form() noexcept;
    BBERG_INLINE constexpr void self_from_montgomery_form() noexcept;

    BBERG_INLINE constexpr void self_conditional_negate(const uint64_t predicate) noexcept;

    BBERG_INLINE constexpr field reduce_once() const noexcept;
    BBERG_INLINE constexpr void self_reduce_once() noexcept;

    BBERG_INLINE constexpr uint64_t get_msb() const noexcept;
    BBERG_INLINE constexpr void self_set_msb() noexcept;
    BBERG_INLINE constexpr bool is_msb_set() const noexcept;
    BBERG_INLINE constexpr uint64_t is_msb_set_word() const noexcept;
    BBERG_INLINE constexpr bool get_bit(const uint64_t bit_index) const noexcept;

    BBERG_INLINE constexpr bool is_zero() const noexcept;

    static constexpr field get_root_of_unity(const size_t degree) noexcept;

    static void serialize_to_buffer(const field& value, uint8_t* buffer)
    {
        const field input = value.from_montgomery_form();
        for (size_t j = 0; j < 4; ++j) {
            for (size_t i = 0; i < 8; ++i) {
                uint8_t byte = static_cast<uint8_t>(input.data[3 - j] >> (56 - (i * 8)));
                buffer[j * 8 + i] = byte;
            }
        }
    }

    static field serialize_from_buffer(const uint8_t* buffer)
    {
        field result = zero;
        for (size_t j = 0; j < 4; ++j) {
            for (size_t i = 0; i < 8; ++i) {
                uint8_t byte = buffer[j * 8 + i];
                result.data[3 - j] = result.data[3 - j] | (static_cast<uint64_t>(byte) << (56 - (i * 8)));
            }
        }
        return result.to_montgomery_form();
    }

    struct wide_array {
        uint64_t data[8];
    };
    BBERG_INLINE constexpr wide_array mul_512(const field& other) const noexcept;
    BBERG_INLINE constexpr wide_array sqr_512() const noexcept;

    BBERG_INLINE constexpr field conditionally_subtract_from_double_modulus(const uint64_t predicate) const noexcept
    {
        if (predicate) {
            return twice_modulus - *this;
        } else {
            return *this;
        }
    }
    /**
     * For short Weierstrass curves y^2 = x^3 + b mod r, if there exists a cube root of unity mod r,
     * we can take advantage of an enodmorphism to decompose a 254 bit scalar into 2 128 bit scalars.
     * \beta = cube root of 1, mod q (q = order of fq)
     * \lambda = cube root of 1, mod r (r = order of fr)
     *
     * For a point P1 = (X, Y), where Y^2 = X^3 + b, we know that
     * the point P2 = (X * \beta, Y) is also a point on the curve
     * We can represent P2 as a scalar multiplication of P1, where P2 = \lambda * P1
     *
     * For a generic multiplication of P1 by a 254 bit scalar k, we can decompose k
     * into 2 127 bit scalars (k1, k2), such that k = k1 - (k2 * \lambda)
     *
     * We can now represent (k * P1) as (k1 * P1) - (k2 * P2), where P2 = (X * \beta, Y).
     * As k1, k2 have half the bit length of k, we have reduced the number of loop iterations of our
     * scalar multiplication algorithm in half
     *
     * To find k1, k2, We use the extended euclidean algorithm to find 4 short scalars [a1, a2], [b1, b2] such that
     * modulus = (a1 * b2) - (b1 * a2)
     * We then compube scalars c1 = round(b2 * k / r), c2 = round(b1 * k / r), where
     * k1 = (c1 * a1) + (c2 * a2), k2 = -((c1 * b1) + (c2 * b2))
     * We pre-compute scalars g1 = (2^256 * b1) / n, g2 = (2^256 * b2) / n, to avoid having to perform long division
     * on 512-bit scalars
     **/
    BBERG_INLINE static void split_into_endomorphism_scalars(field& k, field& k1, field& k2)
    {
        field input = k.reduce_once();
        // uint64_t lambda_reduction[4] = { 0 };
        // __to_montgomery_form(lambda, lambda_reduction);

        // TODO: these parameters only work for the bn254 coordinate field.
        // Need to shift into Params and calculate correct constants for the subgroup field
        constexpr field endo_g1 = { Params::endo_g1_lo, Params::endo_g1_mid, Params::endo_g1_hi, 0 };

        constexpr field endo_g2 = { Params::endo_g2_lo, Params::endo_g2_mid, 0, 0 };

        constexpr field endo_minus_b1 = { Params::endo_minus_b1_lo, Params::endo_minus_b1_mid, 0, 0 };

        constexpr field endo_b2 = { Params::endo_b2_lo, Params::endo_b2_mid, 0, 0 };

        // compute c1 = (g2 * k) >> 256
        wide_array c1 = endo_g2.mul_512(input);
        // compute c2 = (g1 * k) >> 256
        wide_array c2 = endo_g1.mul_512(input);

        // (the bit shifts are implicit, as we only utilize the high limbs of c1, c2

        // TODO remove data duplication
        field c1_hi = {
            c1.data[4], c1.data[5], c1.data[6], c1.data[7]
        }; // *(field*)((uintptr_t)(&c1) + (4 * sizeof(uint64_t)));
        field c2_hi = {
            c2.data[4], c2.data[5], c2.data[6], c2.data[7]
        }; // *(field*)((uintptr_t)(&c2) + (4 * sizeof(uint64_t)));

        // compute q1 = c1 * -b1
        wide_array q1 = c1_hi.mul_512(endo_minus_b1);
        // compute q2 = c2 * b2
        wide_array q2 = c2_hi.mul_512(endo_b2);

        // TODO: this doesn't have to be a 512-bit multiply...
        field q1_lo{ q1.data[0], q1.data[1], q1.data[2], q1.data[3] };
        field q2_lo{ q2.data[0], q2.data[1], q2.data[2], q2.data[3] };

        field t1 = (q2_lo - q1_lo).reduce_once();
        field t2 = (t1 * beta + input).reduce_once();
        k2.data[0] = t1.data[0];
        k2.data[1] = t1.data[1];
        k1.data[0] = t2.data[0];
        k1.data[1] = t2.data[1];
    }

    static void compute_coset_generators(const size_t n, const size_t subgroup_size, field* result)
    {
        if (n > 0) {
            result[0] = multiplicative_generator;
        }
        field work_variable = multiplicative_generator + one;

        size_t count = 1;
        while (count < n) {
            // work_variable contains a new field element, and we need to test that, for all previous vector
            // elements, result[i] / work_variable is not a member of our subgroup
            field work_inverse = work_variable.invert();
            bool valid = true;
            for (size_t j = 0; j < count; ++j) {
                field target_element = result[j] * work_inverse;
                field subgroup_check = target_element.pow(subgroup_size);
                if (subgroup_check == one) {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                result[count] = (work_variable);
                ++count;
            }
            work_variable = work_variable + one;
        }
    }

    // static constexpr auto coset_generators = compute_coset_generators();
    // static constexpr std::array<field, 15> coset_generators = compute_coset_generators((1 << 30U));

    friend std::ostream& operator<<(std::ostream& os, const field& a)
    {
        field out = a.reduce_once().reduce_once();
        std::ios_base::fmtflags f(os.flags());
        os << std::hex << "0x" << std::setfill('0') << std::setw(16) << out.data[3] << std::setw(16) << out.data[2]
           << std::setw(16) << out.data[1] << std::setw(16) << out.data[0];
        os.flags(f);
        return os;
    }

    BBERG_INLINE static void __copy(const field& a, field& r) noexcept { r = a; }
    BBERG_INLINE static void __swap(field& src, field& dest) noexcept
    {
        field T = dest;
        dest = src;
        src = T;
    }

    static field random_element(std::mt19937_64* engine = nullptr,
                                std::uniform_int_distribution<uint64_t>* dist = nullptr) noexcept;

  private:
    struct wnaf_table {
        uint8_t windows[64];

        constexpr wnaf_table(const field& target)
            : windows{ (uint8_t)(target.data[0] & 15),         (uint8_t)((target.data[0] >> 4) & 15),
                       (uint8_t)((target.data[0] >> 8) & 15),  (uint8_t)((target.data[0] >> 12) & 15),
                       (uint8_t)((target.data[0] >> 16) & 15), (uint8_t)((target.data[0] >> 20) & 15),
                       (uint8_t)((target.data[0] >> 24) & 15), (uint8_t)((target.data[0] >> 28) & 15),
                       (uint8_t)((target.data[0] >> 32) & 15), (uint8_t)((target.data[0] >> 36) & 15),
                       (uint8_t)((target.data[0] >> 40) & 15), (uint8_t)((target.data[0] >> 44) & 15),
                       (uint8_t)((target.data[0] >> 48) & 15), (uint8_t)((target.data[0] >> 52) & 15),
                       (uint8_t)((target.data[0] >> 56) & 15), (uint8_t)((target.data[0] >> 60) & 15),
                       (uint8_t)(target.data[1] & 15),         (uint8_t)((target.data[1] >> 4) & 15),
                       (uint8_t)((target.data[1] >> 8) & 15),  (uint8_t)((target.data[1] >> 12) & 15),
                       (uint8_t)((target.data[1] >> 16) & 15), (uint8_t)((target.data[1] >> 20) & 15),
                       (uint8_t)((target.data[1] >> 24) & 15), (uint8_t)((target.data[1] >> 28) & 15),
                       (uint8_t)((target.data[1] >> 32) & 15), (uint8_t)((target.data[1] >> 36) & 15),
                       (uint8_t)((target.data[1] >> 40) & 15), (uint8_t)((target.data[1] >> 44) & 15),
                       (uint8_t)((target.data[1] >> 48) & 15), (uint8_t)((target.data[1] >> 52) & 15),
                       (uint8_t)((target.data[1] >> 56) & 15), (uint8_t)((target.data[1] >> 60) & 15),
                       (uint8_t)(target.data[2] & 15),         (uint8_t)((target.data[2] >> 4) & 15),
                       (uint8_t)((target.data[2] >> 8) & 15),  (uint8_t)((target.data[2] >> 12) & 15),
                       (uint8_t)((target.data[2] >> 16) & 15), (uint8_t)((target.data[2] >> 20) & 15),
                       (uint8_t)((target.data[2] >> 24) & 15), (uint8_t)((target.data[2] >> 28) & 15),
                       (uint8_t)((target.data[2] >> 32) & 15), (uint8_t)((target.data[2] >> 36) & 15),
                       (uint8_t)((target.data[2] >> 40) & 15), (uint8_t)((target.data[2] >> 44) & 15),
                       (uint8_t)((target.data[2] >> 48) & 15), (uint8_t)((target.data[2] >> 52) & 15),
                       (uint8_t)((target.data[2] >> 56) & 15), (uint8_t)((target.data[2] >> 60) & 15),
                       (uint8_t)(target.data[3] & 15),         (uint8_t)((target.data[3] >> 4) & 15),
                       (uint8_t)((target.data[3] >> 8) & 15),  (uint8_t)((target.data[3] >> 12) & 15),
                       (uint8_t)((target.data[3] >> 16) & 15), (uint8_t)((target.data[3] >> 20) & 15),
                       (uint8_t)((target.data[3] >> 24) & 15), (uint8_t)((target.data[3] >> 28) & 15),
                       (uint8_t)((target.data[3] >> 32) & 15), (uint8_t)((target.data[3] >> 36) & 15),
                       (uint8_t)((target.data[3] >> 40) & 15), (uint8_t)((target.data[3] >> 44) & 15),
                       (uint8_t)((target.data[3] >> 48) & 15), (uint8_t)((target.data[3] >> 52) & 15),
                       (uint8_t)((target.data[3] >> 56) & 15), (uint8_t)((target.data[3] >> 60) & 15) }
        {}
    };

    template <size_t idx, field::wnaf_table window>
    constexpr field exponentiation_round([[maybe_unused]] const field* lookup_table) noexcept;

    static constexpr field modulus_plus_one =
        field(Params::modulus_0 + 1ULL, Params::modulus_1, Params::modulus_2, Params::modulus_3);
    static constexpr field modulus_minus_two =
        field(Params::modulus_0 - 2ULL, Params::modulus_1, Params::modulus_2, Params::modulus_3);
    static constexpr field sqrt_exponent =
        field(Params::sqrt_exponent_0, Params::sqrt_exponent_1, Params::sqrt_exponent_2, Params::sqrt_exponent_3);
    static constexpr field r_squared =
        field(Params::r_squared_0, Params::r_squared_1, Params::r_squared_2, Params::r_squared_3);
    static constexpr field root_of_unity =
        field(Params::primitive_root_0, Params::primitive_root_1, Params::primitive_root_2, Params::primitive_root_3);

    BBERG_INLINE static constexpr std::pair<uint64_t, uint64_t> mul_wide(const uint64_t a, const uint64_t b) noexcept;
    BBERG_INLINE static constexpr std::pair<uint64_t, uint64_t> mac(const uint64_t a,
                                                                    const uint64_t b,
                                                                    const uint64_t c,
                                                                    const uint64_t carry_in) noexcept;
    BBERG_INLINE static constexpr std::pair<uint64_t, uint64_t> mac_mini(const uint64_t a,
                                                                         const uint64_t b,
                                                                         const uint64_t c) noexcept;
    BBERG_INLINE static constexpr uint64_t mac_discard_lo(const uint64_t a,
                                                          const uint64_t b,
                                                          const uint64_t c,
                                                          const uint64_t carry_in) noexcept;
    BBERG_INLINE static constexpr uint64_t mac_discard_hi(const uint64_t a,
                                                          const uint64_t b,
                                                          const uint64_t c,
                                                          const uint64_t carry_in) noexcept;

    BBERG_INLINE static constexpr std::pair<uint64_t, uint64_t> addc(const uint64_t a,
                                                                     const uint64_t b,
                                                                     const uint64_t carry_in) noexcept;

    BBERG_INLINE static constexpr std::pair<uint64_t, uint64_t> addc_mini(const uint64_t a, const uint64_t b) noexcept;

    BBERG_INLINE static constexpr uint64_t addc_discard_hi(const uint64_t a,
                                                           const uint64_t b,
                                                           const uint64_t carry_in) noexcept;

    BBERG_INLINE static constexpr std::pair<uint64_t, uint64_t> sbb(const uint64_t a,
                                                                    const uint64_t b,
                                                                    const uint64_t borrow_in) noexcept;
    BBERG_INLINE static constexpr uint64_t sbb_discard_hi(const uint64_t a,
                                                          const uint64_t b,
                                                          uint64_t borrow_in) noexcept;

    BBERG_INLINE constexpr field subtract(const field& other) const noexcept;
    BBERG_INLINE constexpr field subtract_coarse(const field& other) const noexcept;
    BBERG_INLINE static constexpr field montgomery_reduce(const wide_array& r) noexcept;

#ifndef DISABLE_SHENANIGANS
    BBERG_INLINE static field asm_mul(const field& a, const field& b) noexcept;
    BBERG_INLINE static field asm_sqr(const field& a) noexcept;
    BBERG_INLINE static field asm_add(const field& a, const field& b) noexcept;
    BBERG_INLINE static field asm_sub(const field& a, const field& b) noexcept;
    BBERG_INLINE static field asm_mul_with_coarse_reduction(const field& a, const field& b) noexcept;
    BBERG_INLINE static field asm_sqr_with_coarse_reduction(const field& a) noexcept;
    BBERG_INLINE static field asm_add_with_coarse_reduction(const field& a, const field& b) noexcept;
    BBERG_INLINE static field asm_sub_with_coarse_reduction(const field& a, const field& b) noexcept;
    BBERG_INLINE static field asm_add_without_reduction(const field& a, const field& b) noexcept;

    BBERG_INLINE static void asm_self_mul(const field& a, const field& b) noexcept;
    BBERG_INLINE static void asm_self_sqr(const field& a) noexcept;
    BBERG_INLINE static void asm_self_add(const field& a, const field& b) noexcept;
    BBERG_INLINE static void asm_self_sub(const field& a, const field& b) noexcept;
    BBERG_INLINE static void asm_self_mul_with_coarse_reduction(const field& a, const field& b) noexcept;
    BBERG_INLINE static void asm_self_sqr_with_coarse_reduction(const field& a) noexcept;
    BBERG_INLINE static void asm_self_add_with_coarse_reduction(const field& a, const field& b) noexcept;
    BBERG_INLINE static void asm_self_sub_with_coarse_reduction(const field& a, const field& b) noexcept;
    BBERG_INLINE static void asm_self_add_without_reduction(const field& a, const field& b) noexcept;

    BBERG_INLINE static void asm_conditional_negate(field& a, const uint64_t predicate) noexcept;
    BBERG_INLINE static field asm_reduce_once(const field& a) noexcept;
    BBERG_INLINE static void asm_self_reduce_once(const field& a) noexcept;
    static constexpr uint64_t zero_reference = 0x00ULL;
#endif
    constexpr field tonelli_shanks_sqrt() const noexcept;
#if defined(__SIZEOF_INT128__) && !defined(__wasm__)
    static constexpr uint128_t lo_mask = 0xffffffffffffffffUL;
#endif
};
} // namespace barretenberg

#include "./field_impl.hpp"
