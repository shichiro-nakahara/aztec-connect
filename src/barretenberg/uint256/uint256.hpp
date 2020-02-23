/**
 * uint256_t
 * Copyright Aztec 2020
 *
 * An unsigned 256 bit integer type.
 *
 * Constructor and all methods are constexpr. Ideally, uint256_t should be able to be treated like any other literal
 *type.
 *
 * Not optimized for performance, this code doesn"t touch any of our hot paths when constructing PLONK proofs
 **/
#pragma once

#include <cstdint>
#include <iostream>

class uint256_t {
  public:
    constexpr uint256_t(const uint64_t a = 0)
        : data{ a, 0, 0, 0 }
    {}

    constexpr uint256_t(const uint64_t a, const uint64_t b, const uint64_t c, const uint64_t d)
        : data{ a, b, c, d }
    {}

    constexpr uint256_t(const uint256_t& other)
        : data{ other.data[0], other.data[1], other.data[2], other.data[3] }
    {}

    constexpr uint256_t& operator=(const uint256_t& other) = default;

    explicit constexpr operator bool() const { return static_cast<bool>(data[0]); };
    explicit constexpr operator uint8_t() const { return static_cast<uint8_t>(data[0]); };
    explicit constexpr operator uint16_t() const { return static_cast<uint16_t>(data[0]); };
    explicit constexpr operator uint32_t() const { return static_cast<uint32_t>(data[0]); };
    explicit constexpr operator uint64_t() const { return static_cast<uint64_t>(data[0]); };

    constexpr bool get_bit(const uint64_t bit_index) const;
    constexpr uint64_t get_msb() const;

    constexpr uint256_t operator+(const uint256_t& other) const;
    constexpr uint256_t operator-(const uint256_t& other) const;
    constexpr uint256_t operator-() const;

    constexpr uint256_t operator*(const uint256_t& other) const;
    constexpr uint256_t operator/(const uint256_t& other) const;
    constexpr uint256_t operator%(const uint256_t& other) const;

    constexpr uint256_t operator>>(const uint256_t& other) const;
    constexpr uint256_t operator<<(const uint256_t& other) const;

    constexpr uint256_t operator&(const uint256_t& other) const;
    constexpr uint256_t operator^(const uint256_t& other) const;
    constexpr uint256_t operator|(const uint256_t& other) const;
    constexpr uint256_t operator~() const;

    constexpr bool operator==(const uint256_t& other) const;
    constexpr bool operator!=(const uint256_t& other) const;
    constexpr bool operator!() const;

    constexpr bool operator>(const uint256_t& other) const;
    constexpr bool operator<(const uint256_t& other) const;
    constexpr bool operator>=(const uint256_t& other) const;
    constexpr bool operator<=(const uint256_t& other) const;

    constexpr uint256_t& operator+=(const uint256_t& other)
    {
        *this = *this + other;
        return *this;
    };
    constexpr uint256_t& operator-=(const uint256_t& other)
    {
        *this = *this - other;
        return *this;
    };
    constexpr uint256_t& operator*=(const uint256_t& other)
    {
        *this = *this * other;
        return *this;
    };
    constexpr uint256_t& operator/=(const uint256_t& other)
    {
        *this = *this / other;
        return *this;
    };
    constexpr uint256_t& operator%=(const uint256_t& other)
    {
        *this = *this % other;
        return *this;
    };

    constexpr uint256_t& operator++()
    {
        *this += uint256_t(1);
        return *this;
    };
    constexpr uint256_t& operator--()
    {
        *this -= uint256_t(1);
        return *this;
    };

    constexpr uint256_t& operator&=(const uint256_t& other)
    {
        *this = *this & other;
        return *this;
    };
    constexpr uint256_t& operator^=(const uint256_t& other)
    {
        *this = *this ^ other;
        return *this;
    };
    constexpr uint256_t& operator|=(const uint256_t& other)
    {
        *this = *this | other;
        return *this;
    };

    constexpr uint256_t& operator>>=(const uint256_t& other)
    {
        *this = *this >> other;
        return *this;
    };
    constexpr uint256_t& operator<<=(const uint256_t& other)
    {
        *this = *this << other;
        return *this;
    };

    constexpr uint256_t invmod(const uint256_t& modulus) const;

    constexpr std::pair<uint256_t, uint256_t> mul_512(const uint256_t& other) const;

    friend std::ostream& operator<<(std::ostream& os, const uint256_t& val);

    uint64_t data[4];

  private:
    struct uint64_pair {
        uint64_t data[2];
    };
    struct uint64_quad {
        uint64_t data[4];
    };
    struct divmod_output {
        uint64_quad quotient;
        uint64_quad remainder;
    };

    constexpr std::pair<uint64_t, uint64_t> mul_wide(const uint64_t a, const uint64_t b) const;
    constexpr std::pair<uint64_t, uint64_t> addc(const uint64_t a, const uint64_t b, const uint64_t carry_in) const;
    constexpr uint64_t addc_discard_hi(const uint64_t a, const uint64_t b, const uint64_t carry_in) const;
    constexpr uint64_t sbb_discard_hi(const uint64_t a, const uint64_t b, const uint64_t borrow_in) const;

    constexpr std::pair<uint64_t, uint64_t> sbb(const uint64_t a, const uint64_t b, const uint64_t borrow_in) const;
    constexpr uint64_t mac_discard_hi(const uint64_t a,
                                      const uint64_t b,
                                      const uint64_t c,
                                      const uint64_t carry_in) const;
    constexpr std::pair<uint64_t, uint64_t> mac(const uint64_t a,
                                                const uint64_t b,
                                                const uint64_t c,
                                                const uint64_t carry_in) const;
    constexpr divmod_output divmod(const uint256_t& a, const uint256_t& b) const;
};

#include "./uint256_impl.hpp"

inline std::ostream& operator<<(std::ostream& os, const uint256_t& val)
{
    os << "[" << val.data[0] << ", " << val.data[1] << ", " << val.data[2] << ", " << val.data[3] << "]";
    return os;
}