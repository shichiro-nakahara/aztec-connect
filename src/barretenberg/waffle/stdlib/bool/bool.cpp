#include "./bool.hpp"
#include "../../composer/composer_base.hpp"

#include "../../../curves/bn254/fr.hpp"

#include "../../composer/standard_composer.hpp"
#include "../../composer/bool_composer.hpp"
#include "../../composer/mimc_composer.hpp"
#include "../../composer/turbo_composer.hpp"

using namespace barretenberg;

namespace plonk {
namespace stdlib {

template <typename ComposerContext>
bool_t<ComposerContext>::bool_t(const bool value)
    : context(nullptr)
    , witness_bool(value)
    , witness_inverted(false)
    , witness_index(static_cast<uint32_t>(-1))
{}

template <typename ComposerContext>
bool_t<ComposerContext>::bool_t(ComposerContext* parent_context)
    : context(parent_context)
{
    witness_bool = false;
    witness_inverted = false;
    witness_index = static_cast<uint32_t>(-1);
}

template <typename ComposerContext>
bool_t<ComposerContext>::bool_t(const witness_t<ComposerContext>& value)
    : context(value.context)
{
    ASSERT(barretenberg::fr::eq(value.witness, barretenberg::fr::zero) ||
           barretenberg::fr::eq(value.witness, barretenberg::fr::one));
    witness_index = value.witness_index;
    context->create_bool_gate(witness_index);
    witness_bool = barretenberg::fr::eq(value.witness, barretenberg::fr::one);
    witness_inverted = false;
}

template <typename ComposerContext>
bool_t<ComposerContext>::bool_t(ComposerContext* parent_context, const bool value)
    : context(parent_context)
{
    context = parent_context;
    witness_index = static_cast<uint32_t>(-1);
    witness_bool = value;
    witness_inverted = false;
}

template <typename ComposerContext>
bool_t<ComposerContext>::bool_t(const bool_t<ComposerContext>& other)
    : context(other.context)
{
    witness_index = other.witness_index;
    witness_bool = other.witness_bool;
    witness_inverted = other.witness_inverted;
}

template <typename ComposerContext>
bool_t<ComposerContext>::bool_t(bool_t<ComposerContext>&& other)
    : context(other.context)
{
    witness_index = other.witness_index;
    witness_bool = other.witness_bool;
    witness_inverted = other.witness_inverted;
}

template <typename ComposerContext> bool_t<ComposerContext>& bool_t<ComposerContext>::operator=(const bool other)
{
    context = nullptr;
    witness_index = static_cast<uint32_t>(-1);
    witness_bool = other;
    witness_inverted = false;
    return *this;
}

template <typename ComposerContext> bool_t<ComposerContext>& bool_t<ComposerContext>::operator=(const bool_t& other)
{
    context = other.context;
    witness_index = other.witness_index;
    witness_bool = other.witness_bool;
    witness_inverted = other.witness_inverted;
    return *this;
}

template <typename ComposerContext> bool_t<ComposerContext>& bool_t<ComposerContext>::operator=(bool_t&& other)
{
    context = other.context;
    witness_index = other.witness_index;
    witness_bool = other.witness_bool;
    witness_inverted = other.witness_inverted;
    return *this;
}

template <typename ComposerContext>
bool_t<ComposerContext>& bool_t<ComposerContext>::operator=(const witness_t<ComposerContext>& other)
{
    ASSERT(barretenberg::fr::eq(other.witness, barretenberg::fr::one) ||
           barretenberg::fr::eq(other.witness, barretenberg::fr::zero));
    context = other.context;
    witness_bool = barretenberg::fr::eq(other.witness, barretenberg::fr::zero) ? false : true;
    witness_index = other.witness_index;
    witness_inverted = false;
    context->create_bool_gate(witness_index);
    return *this;
}

template <typename ComposerContext>
bool_t<ComposerContext> bool_t<ComposerContext>::operator&(const bool_t& other) const
{
    bool_t<ComposerContext> result(context == nullptr ? other.context : context);
    bool left = witness_inverted ^ witness_bool;
    bool right = other.witness_inverted ^ other.witness_bool;

    ASSERT(result.context ||
           (witness_index == static_cast<uint32_t>(-1) && other.witness_index == static_cast<uint32_t>(-1)));
    if (witness_index != static_cast<uint32_t>(-1) && other.witness_index != static_cast<uint32_t>(-1)) {
        result.witness_bool = left & right;
        barretenberg::fr::field_t value = result.witness_bool ? barretenberg::fr::one : barretenberg::fr::zero;
        result.witness_index = context->add_variable(value);
        result.witness_inverted = false;
        // (a.b)
        // (b.(1-a))
        // (a.(1-b))
        // (1-a).(1-b)
        const waffle::poly_triple gate_coefficients{
            witness_index,
            other.witness_index,
            result.witness_index,
            (witness_inverted ^ other.witness_inverted) ? barretenberg::fr::neg_one() : barretenberg::fr::one,
            other.witness_inverted ? barretenberg::fr::one : barretenberg::fr::zero,
            witness_inverted ? barretenberg::fr::one : barretenberg::fr::zero,
            barretenberg::fr::neg_one(),
            (witness_inverted & other.witness_inverted) ? barretenberg::fr::one : barretenberg::fr::zero
        };
        context->create_poly_gate(gate_coefficients);
    } else if (witness_index != static_cast<uint32_t>(-1) && other.witness_index == static_cast<uint32_t>(-1)) {
        if (other.witness_bool ^ other.witness_inverted) {
            result = bool_t<ComposerContext>(*this);
        } else {
            result.witness_bool = false;
            result.witness_inverted = false;
            result.witness_index = static_cast<uint32_t>(-1);
        }
    } else if (witness_index == static_cast<uint32_t>(-1) && other.witness_index != static_cast<uint32_t>(-1)) {
        if (witness_bool ^ witness_inverted) {
            result = bool_t<ComposerContext>(other);
        } else {
            result.witness_bool = false;
            result.witness_inverted = false;
            result.witness_index = static_cast<uint32_t>(-1);
        }
    } else {
        result.witness_bool = left & right;
        result.witness_index = static_cast<uint32_t>(-1);
        result.witness_inverted = false;
    }
    return result;
}

template <typename ComposerContext>
bool_t<ComposerContext> bool_t<ComposerContext>::operator|(const bool_t& other) const
{
    bool_t<ComposerContext> result(context == nullptr ? other.context : context);

    ASSERT(result.context ||
           (witness_index == static_cast<uint32_t>(-1) && other.witness_index == static_cast<uint32_t>(-1)));

    result.witness_bool = (witness_bool ^ witness_inverted) | (other.witness_bool ^ other.witness_inverted);
    barretenberg::fr::field_t value = result.witness_bool ? barretenberg::fr::one : barretenberg::fr::zero;
    result.witness_inverted = false;
    if ((other.witness_index != static_cast<uint32_t>(-1)) && (witness_index != static_cast<uint32_t>(-1))) {
        result.witness_index = context->add_variable(value);
        // result = a + b - ab
        // (1 - a) + (1 - b) - (1 - a)(1 - b) = 2 - a - b - ab - 1 + a + b = 1 - ab
        // (1 - a) + b - (1 - a)(b) = 1 - a + b - b +ab = 1 - a + ab
        // a + (1 - b) - (a)(1 - b) = a - b + ab - a + 1 = 1 - b + ab
        barretenberg::fr::field_t multiplicative_coefficient;
        barretenberg::fr::field_t left_coefficient;
        barretenberg::fr::field_t right_coefficient;
        barretenberg::fr::field_t constant_coefficient;
        if (witness_inverted && !other.witness_inverted) {
            multiplicative_coefficient = barretenberg::fr::one;
            left_coefficient = barretenberg::fr::neg_one();
            right_coefficient = barretenberg::fr::zero;
            constant_coefficient = barretenberg::fr::one;
        } else if (!witness_inverted && other.witness_inverted) {
            multiplicative_coefficient = barretenberg::fr::one;
            left_coefficient = barretenberg::fr::zero;
            right_coefficient = barretenberg::fr::neg_one();
            constant_coefficient = barretenberg::fr::one;
        } else if (witness_inverted && other.witness_inverted) {
            multiplicative_coefficient = barretenberg::fr::neg_one();
            left_coefficient = barretenberg::fr::zero;
            right_coefficient = barretenberg::fr::zero;
            constant_coefficient = barretenberg::fr::one;
        } else {
            multiplicative_coefficient = barretenberg::fr::neg_one();
            left_coefficient = barretenberg::fr::one;
            right_coefficient = barretenberg::fr::one;
            constant_coefficient = barretenberg::fr::zero;
        }
        const waffle::poly_triple gate_coefficients{
            witness_index,    other.witness_index, result.witness_index,        multiplicative_coefficient,
            left_coefficient, right_coefficient,   barretenberg::fr::neg_one(), constant_coefficient
        };
        context->create_poly_gate(gate_coefficients);
    } else if (witness_index != static_cast<uint32_t>(-1) && other.witness_index == static_cast<uint32_t>(-1)) {
        if (other.witness_bool ^ other.witness_inverted) {
            result.witness_index = static_cast<uint32_t>(-1);
            result.witness_bool = true;
            result.witness_inverted = false;
        } else {
            result = bool_t<ComposerContext>(*this);
        }
    } else if (witness_index == static_cast<uint32_t>(-1) && other.witness_index != static_cast<uint32_t>(-1)) {
        if (witness_bool ^ witness_inverted) {
            result.witness_index = static_cast<uint32_t>(-1);
            result.witness_bool = true;
            result.witness_inverted = false;
        } else {
            result = bool_t<ComposerContext>(other);
        }
    } else {
        result.witness_inverted = false;
        result.witness_index = static_cast<uint32_t>(-1);
    }
    return result;
}

template <typename ComposerContext>
bool_t<ComposerContext> bool_t<ComposerContext>::operator^(const bool_t& other) const
{
    bool_t<ComposerContext> result(context == nullptr ? other.context : context);

    ASSERT(result.context ||
           (witness_index == static_cast<uint32_t>(-1) && other.witness_index == static_cast<uint32_t>(-1)));

    result.witness_bool = (witness_bool ^ witness_inverted) ^ (other.witness_bool ^ other.witness_inverted);
    barretenberg::fr::field_t value = result.witness_bool ? barretenberg::fr::one : barretenberg::fr::zero;
    result.witness_inverted = false;

    if ((other.witness_index != static_cast<uint32_t>(-1)) && (witness_index != static_cast<uint32_t>(-1))) {
        result.witness_index = context->add_variable(value);
        // norm a, norm b: a + b - 2ab
        // inv  a, norm b: (1 - a) + b - 2(1 - a)b = 1 - a - b + 2ab
        // norm a, inv  b: a + (1 - b) - 2(a)(1 - b) = 1 - a - b + 2ab
        // inv  a, inv  b: (1 - a) + (1 - b) - 2(1 - a)(1 - b) = a + b - 2ab
        barretenberg::fr::field_t multiplicative_coefficient;
        barretenberg::fr::field_t left_coefficient;
        barretenberg::fr::field_t right_coefficient;
        barretenberg::fr::field_t constant_coefficient;
        if ((witness_inverted && other.witness_inverted) || (!witness_inverted && !other.witness_inverted)) {
            multiplicative_coefficient =
                barretenberg::fr::add(barretenberg::fr::neg_one(), barretenberg::fr::neg_one());
            left_coefficient = barretenberg::fr::one;
            right_coefficient = barretenberg::fr::one;
            constant_coefficient = barretenberg::fr::zero;
        } else {
            multiplicative_coefficient = barretenberg::fr::add(barretenberg::fr::one, barretenberg::fr::one);
            left_coefficient = barretenberg::fr::neg_one();
            right_coefficient = barretenberg::fr::neg_one();
            constant_coefficient = barretenberg::fr::one;
        }
        const waffle::poly_triple gate_coefficients{
            witness_index,    other.witness_index, result.witness_index,        multiplicative_coefficient,
            left_coefficient, right_coefficient,   barretenberg::fr::neg_one(), constant_coefficient
        };
        context->create_poly_gate(gate_coefficients);
    } else if (witness_index != static_cast<uint32_t>(-1) && other.witness_index == static_cast<uint32_t>(-1)) {
        // witness ^ 1 = !witness
        if (other.witness_bool ^ other.witness_inverted) {
            result = !bool_t<ComposerContext>(*this);
        } else {
            result = bool_t<ComposerContext>(*this);
        }
    } else if (witness_index == static_cast<uint32_t>(-1) && other.witness_index != static_cast<uint32_t>(-1)) {
        if (witness_bool ^ witness_inverted) {
            result = !bool_t<ComposerContext>(other);
        } else {
            result = bool_t<ComposerContext>(other);
        }
    } else {
        result.witness_inverted = false;
        result.witness_index = static_cast<uint32_t>(-1);
    }
    return result;
}

template <typename ComposerContext> bool_t<ComposerContext> bool_t<ComposerContext>::operator!() const
{
    bool_t<ComposerContext> result(*this);
    result.witness_inverted = !result.witness_inverted;
    return result;
}

template <typename ComposerContext>
bool_t<ComposerContext> bool_t<ComposerContext>::operator==(const bool_t& other) const
{
    ASSERT(context || other.context ||
           (witness_index == static_cast<uint32_t>(-1) && other.witness_index == static_cast<uint32_t>(-1)));
    if ((other.witness_index == static_cast<uint32_t>(-1)) && (witness_index == static_cast<uint32_t>(-1))) {
        bool_t<ComposerContext> result(context == nullptr ? other.context : context);
        result.witness_bool = (witness_bool ^ witness_inverted) == (other.witness_bool ^ other.witness_inverted);
        result.witness_index = static_cast<uint32_t>(-1);
        return result;
    } else if ((witness_index != static_cast<uint32_t>(-1)) && (other.witness_index == static_cast<uint32_t>(-1))) {
        if (other.witness_bool ^ other.witness_inverted) {
            return (*this);
        } else {
            return !(*this);
        }
    } else if ((witness_index == static_cast<uint32_t>(-1)) && (other.witness_index != static_cast<uint32_t>(-1))) {
        if (witness_bool ^ witness_inverted) {
            return other;
        } else {
            return !(other);
        }
    } else {
        bool_t<ComposerContext> result(context == nullptr ? other.context : context);
        result.witness_bool = (witness_bool ^ witness_inverted) == (other.witness_bool ^ other.witness_inverted);
        barretenberg::fr::field_t value = result.witness_bool ? barretenberg::fr::one : barretenberg::fr::zero;
        result.witness_index = context->add_variable(value);
        // norm a, norm b or both inv: 1 - a - b + 2ab
        // inv a or inv b = a + b - 2ab
        barretenberg::fr::field_t multiplicative_coefficient;
        barretenberg::fr::field_t left_coefficient;
        barretenberg::fr::field_t right_coefficient;
        barretenberg::fr::field_t constant_coefficient;
        if ((witness_inverted && other.witness_inverted) || (!witness_inverted && !other.witness_inverted)) {
            multiplicative_coefficient = barretenberg::fr::add(barretenberg::fr::one, barretenberg::fr::one);
            left_coefficient = barretenberg::fr::neg_one();
            right_coefficient = barretenberg::fr::neg_one();
            constant_coefficient = barretenberg::fr::one;
        } else {
            multiplicative_coefficient =
                barretenberg::fr::add(barretenberg::fr::neg_one(), barretenberg::fr::neg_one());
            left_coefficient = barretenberg::fr::one;
            right_coefficient = barretenberg::fr::one;
            constant_coefficient = barretenberg::fr::zero;
        }
        const waffle::poly_triple gate_coefficients{
            witness_index,    other.witness_index, result.witness_index,        multiplicative_coefficient,
            left_coefficient, right_coefficient,   barretenberg::fr::neg_one(), constant_coefficient
        };
        context->create_poly_gate(gate_coefficients);
        return result;
    }
}

template <typename ComposerContext> bool_t<ComposerContext> bool_t<ComposerContext>::operator!=(const bool_t<ComposerContext>& other) const
{
    return operator^(other);
}

template <typename ComposerContext> bool_t<ComposerContext> bool_t<ComposerContext>::operator&&(const bool_t<ComposerContext>& other) const
{
    return operator&(other);
}

template <typename ComposerContext> bool_t<ComposerContext> bool_t<ComposerContext>::operator||(const bool_t<ComposerContext>& other) const
{
    return operator|(other);
}


template <typename ComposerContext> bool_t<ComposerContext> bool_t<ComposerContext>::normalize() const
{
    bool is_constant = (witness_index == static_cast<uint32_t>(-1));
    if (is_constant)
    {
        return *this;
    }

    barretenberg::fr::field_t value = witness_bool ^ witness_inverted ? barretenberg::fr::one : barretenberg::fr::zero;

    uint32_t new_witness = context->add_variable(value);
    uint32_t new_value = witness_bool ^ witness_inverted;

    barretenberg::fr::field_t q_l;
    barretenberg::fr::field_t q_c;

    q_l = witness_inverted ? barretenberg::fr::neg_one() : barretenberg::fr::one;
    q_c = witness_inverted ? barretenberg::fr::one : barretenberg::fr::zero;

    barretenberg::fr::field_t q_o = barretenberg::fr::neg_one();
    barretenberg::fr::field_t q_m = barretenberg::fr::zero;
    barretenberg::fr::field_t q_r = barretenberg::fr::zero;

    const waffle::poly_triple gate_coefficients{
        witness_index, witness_index, new_witness, q_m, q_l, q_r, q_o, q_c
    };

    context->create_poly_gate(gate_coefficients);

    witness_index = new_witness;
    witness_bool = new_value;
    witness_inverted = false;
    return *this;
}

template class bool_t<waffle::StandardComposer>;
template class bool_t<waffle::BoolComposer>;
template class bool_t<waffle::MiMCComposer>;
template class bool_t<waffle::TurboComposer>;

} // namespace stdlib
} // namespace plonk
