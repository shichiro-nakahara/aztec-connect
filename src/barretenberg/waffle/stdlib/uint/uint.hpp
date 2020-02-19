#pragma once

#include <vector>

#include "../byte_array/byte_array.hpp"
#include "../../../uint256/uint256.hpp"

#include "../common.hpp"

namespace waffle
{
    class StandardComposer;
    class TurboComposer;
    class BoolComposer;
    class MiMCComposer;
    class TurboComposer;
}

namespace plonk {
namespace stdlib {

template <typename Composer> class bool_t;
template <typename Composer> class field_t;

template <typename Composer, typename Native> class uint {
    public:
    static constexpr size_t width = sizeof(Native) * 8;

    uint(const witness_t<Composer>& other);
    uint(const field_t<Composer>& other);
    uint(const uint256_t& value = 0);
    uint(Composer* composer, const uint256_t& value = 0);
    uint(const byte_array<Composer>& other);
    uint(Composer* parent_context, const std::vector<bool_t<Composer>>& wires);
    uint(Composer* parent_context, const std::array<bool_t<Composer>, width>& wires)
        : uint<Composer, Native>(parent_context, std::vector<bool_t<Composer>>(wires.begin(), wires.end()))
    {}

    uint(const Native v)
        : uint(static_cast<uint256_t>(v))
    {}

    // uint(const char v)
    //     : uint(uint256_t((uint8_t)v))
    // {}
    // uint(uint64_t v)
    //     : uint(static_cast<uint256_t>(v))
    // {}

    uint(const uint& other);
    uint(uint&& other);

    uint& operator=(const uint& other);
    uint& operator=(uint&& other);

    operator byte_array<Composer>() const;
    operator field_t<Composer>() const;

    uint operator+(const uint& other) const;
    uint operator-(const uint& other) const;
    uint operator*(const uint& other) const;
    uint operator/(const uint& other) const;
    uint operator%(const uint& other) const;

    uint operator&(const uint& other) const;
    uint operator^(const uint& other) const;
    uint operator|(const uint& other) const;
    uint operator~() const;

    uint operator>>(const uint64_t shift) const;
    uint operator<<(const uint64_t shift) const;

    uint ror(const uint64_t target_rotation) const;
    uint rol(const uint64_t target_rotation) const;
    uint ror(const uint256_t target_rotation) const { return ror(target_rotation.data[0]); }
    uint rol(const uint256_t target_rotation) const { return rol(target_rotation.data[0]); }

    bool_t<Composer> operator>(const uint& other) const;
    bool_t<Composer> operator<(const uint& other) const;
    bool_t<Composer> operator>=(const uint& other) const;
    bool_t<Composer> operator<=(const uint& other) const;
    bool_t<Composer> operator==(const uint& other) const;
    bool_t<Composer> operator!=(const uint& other) const;
    bool_t<Composer> operator!() const;

    uint operator+=(const uint& other) { *this = operator+(other); return *this; }
    uint operator-=(const uint& other) { *this = operator-(other); return *this; }
    uint operator*=(const uint& other) { *this = operator*(other); return *this; }
    uint operator/=(const uint& other) { *this = operator/(other); return *this; }
    uint operator%=(const uint& other) { *this = operator%(other); return *this; }

    uint operator&=(const uint& other) { *this = operator&(other); return *this; }
    uint operator^=(const uint& other) { *this = operator^(other); return *this; }
    uint operator|=(const uint& other) { *this = operator|(other); return *this; }

    uint operator>>=(const uint64_t shift) { *this = operator>>(shift); return *this; }
    uint operator<<=(const uint64_t shift) { *this = operator<<(shift); return *this; }

    uint normalize() const;

    uint256_t get_value() const;

    bool is_constant() const { return witness_index == UINT32_MAX; }
    Composer* get_context() const { return context; }

    bool_t<Composer> at(const size_t bit_index) const;

    size_t get_width() const { return width; }

    uint32_t get_witness_index() const { return witness_index; }

    uint256_t get_additive_constant() const { return additive_constant; }

protected:
    Composer* context;

    enum WitnessStatus {
        OK,
        NOT_NORMALIZED,
        WEAK_NORMALIZED
    };

    mutable uint256_t additive_constant;
    mutable WitnessStatus witness_status;
    mutable std::vector<uint32_t> accumulators;
    mutable uint32_t witness_index;

    static constexpr uint256_t CIRCUIT_UINT_MAX_PLUS_ONE = (uint256_t(1) << width);
    static constexpr uint256_t MASK = CIRCUIT_UINT_MAX_PLUS_ONE - 1;
private:
    enum LogicOp{
        AND,
        XOR,
    };
    
    std::pair<uint, uint> divmod(const uint& other) const;
    uint logic_operator(const uint& other, const LogicOp op_type) const;
    uint weak_normalize() const;

    uint256_t get_unbounded_value() const;
};

template <typename T, typename w> inline std::ostream& operator<<(std::ostream& os, uint<T, w> const& v)
{
    return os << v.get_value();
}

extern template class uint<waffle::TurboComposer, uint8_t>;
extern template class uint<waffle::TurboComposer, uint16_t>;
extern template class uint<waffle::TurboComposer, uint32_t>;
extern template class uint<waffle::TurboComposer, uint64_t>;

extern template class uint<waffle::StandardComposer, uint8_t>;
extern template class uint<waffle::StandardComposer, uint16_t>;
extern template class uint<waffle::StandardComposer, uint32_t>;
extern template class uint<waffle::StandardComposer, uint64_t>;

extern template class uint<waffle::BoolComposer, uint8_t>;
extern template class uint<waffle::BoolComposer, uint16_t>;
extern template class uint<waffle::BoolComposer, uint32_t>;
extern template class uint<waffle::BoolComposer, uint64_t>;

extern template class uint<waffle::MiMCComposer, uint8_t>;
extern template class uint<waffle::MiMCComposer, uint16_t>;
extern template class uint<waffle::MiMCComposer, uint32_t>;
extern template class uint<waffle::MiMCComposer, uint64_t>;

}
}