#include "plookup_composer.hpp"

#include <ecc/curves/bn254/scalar_multiplication/scalar_multiplication.hpp>
#include <numeric/bitop/get_msb.hpp>
#include <plonk/proof_system/widgets/permutation_widget.hpp>
#include <plonk/proof_system/widgets/turbo_arithmetic_widget.hpp>
#include <plonk/proof_system/widgets/turbo_fixed_base_widget.hpp>
#include <plonk/proof_system/widgets/turbo_logic_widget.hpp>
#include <plonk/proof_system/widgets/turbo_range_widget.hpp>
#include <plonk/proof_system/widgets/plookup_widget.hpp>
#include <plonk/reference_string/file_reference_string.hpp>

#include "plookup_tables/plookup_tables.hpp"
#include "plookup_tables/aes128.hpp"
#include "plookup_tables/sha256.hpp"

using namespace barretenberg;

namespace waffle {

PLookupComposer::PLookupComposer()
    : PLookupComposer("../srs_db", 0)
{}

PLookupComposer::PLookupComposer(std::string const& crs_path, const size_t size_hint)
    : PLookupComposer(std::unique_ptr<ReferenceStringFactory>(new FileReferenceStringFactory(crs_path)), size_hint){};

PLookupComposer::PLookupComposer(std::unique_ptr<ReferenceStringFactory>&& crs_factory, const size_t size_hint)
    : ComposerBase(std::move(crs_factory))
{
    w_l.reserve(size_hint);
    w_r.reserve(size_hint);
    w_o.reserve(size_hint);
    w_4.reserve(size_hint);
    q_m.reserve(size_hint);
    q_1.reserve(size_hint);
    q_2.reserve(size_hint);
    q_3.reserve(size_hint);
    q_4.reserve(size_hint);
    q_arith.reserve(size_hint);
    q_c.reserve(size_hint);
    q_5.reserve(size_hint);
    q_ecc_1.reserve(size_hint);
    q_range.reserve(size_hint);
    q_logic.reserve(size_hint);
    q_lookup_index.reserve(size_hint);
    q_lookup_type.reserve(size_hint);

    zero_idx = put_constant_variable(fr::zero());
    // zero_idx = add_variable(barretenberg::fr::zero());
}

PLookupComposer::PLookupComposer(std::shared_ptr<proving_key> const& p_key,
                                 std::shared_ptr<verification_key> const& v_key,
                                 size_t size_hint)
    : ComposerBase(p_key, v_key)
{
    w_l.reserve(size_hint);
    w_r.reserve(size_hint);
    w_o.reserve(size_hint);
    w_4.reserve(size_hint);
    q_m.reserve(size_hint);
    q_1.reserve(size_hint);
    q_2.reserve(size_hint);
    q_3.reserve(size_hint);
    q_4.reserve(size_hint);
    q_arith.reserve(size_hint);
    q_c.reserve(size_hint);
    q_5.reserve(size_hint);
    q_ecc_1.reserve(size_hint);
    q_range.reserve(size_hint);
    q_logic.reserve(size_hint);
    q_lookup_index.reserve(size_hint);
    q_lookup_type.reserve(size_hint);

    zero_idx = put_constant_variable(fr::zero());
};

void PLookupComposer::create_dummy_gate()
{
    gate_flags.push_back(0);
    uint32_t idx = add_variable(fr{ 1, 1, 1, 1 }.to_montgomery_form());
    w_l.emplace_back(idx);
    w_r.emplace_back(idx);
    w_o.emplace_back(idx);
    w_4.emplace_back(idx);
    q_arith.emplace_back(fr::zero());
    q_4.emplace_back(fr::zero());
    q_5.emplace_back(fr::zero());
    q_ecc_1.emplace_back(fr::zero());
    q_m.emplace_back(fr::zero());
    q_1.emplace_back(fr::zero());
    q_2.emplace_back(fr::zero());
    q_3.emplace_back(fr::zero());
    q_c.emplace_back(fr::zero());
    q_range.emplace_back(fr::zero());
    q_logic.emplace_back(fr::zero());
    q_lookup_index.emplace_back(fr::zero());
    q_lookup_type.emplace_back(fr::zero());

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };
    epicycle fourth{ static_cast<uint32_t>(n), WireType::FOURTH };

    wire_epicycles[static_cast<size_t>(idx)].emplace_back(left);
    wire_epicycles[static_cast<size_t>(idx)].emplace_back(right);
    wire_epicycles[static_cast<size_t>(idx)].emplace_back(out);
    wire_epicycles[static_cast<size_t>(idx)].emplace_back(fourth);

    ++n;
}

void PLookupComposer::create_add_gate(const add_triple& in)
{
    gate_flags.push_back(0);
    w_l.emplace_back(in.a);
    w_r.emplace_back(in.b);
    w_o.emplace_back(in.c);
    w_4.emplace_back(zero_idx);
    q_m.emplace_back(fr::zero());
    q_1.emplace_back(in.a_scaling);
    q_2.emplace_back(in.b_scaling);
    q_3.emplace_back(in.c_scaling);
    q_c.emplace_back(in.const_scaling);
    q_arith.emplace_back(fr::one());
    q_4.emplace_back(fr::zero());
    q_5.emplace_back(fr::zero());
    q_ecc_1.emplace_back(fr::zero());
    q_range.emplace_back(fr::zero());
    q_logic.emplace_back(fr::zero());
    q_lookup_index.emplace_back(fr::zero());
    q_lookup_type.emplace_back(fr::zero());

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };

    ASSERT(wire_epicycles.size() > in.a);
    ASSERT(wire_epicycles.size() > in.b);
    ASSERT(wire_epicycles.size() > in.c);

    wire_epicycles[static_cast<size_t>(in.a)].emplace_back(left);
    wire_epicycles[static_cast<size_t>(in.b)].emplace_back(right);
    wire_epicycles[static_cast<size_t>(in.c)].emplace_back(out);

    ++n;
}

void PLookupComposer::create_big_add_gate(const add_quad& in)
{
    gate_flags.push_back(0);
    w_l.emplace_back(in.a);
    w_r.emplace_back(in.b);
    w_o.emplace_back(in.c);
    w_4.emplace_back(in.d);
    q_m.emplace_back(fr::zero());
    q_1.emplace_back(in.a_scaling);
    q_2.emplace_back(in.b_scaling);
    q_3.emplace_back(in.c_scaling);
    q_c.emplace_back(in.const_scaling);
    q_arith.emplace_back(fr::one());
    q_4.emplace_back(in.d_scaling);
    q_5.emplace_back(fr::zero());
    q_ecc_1.emplace_back(fr::zero());
    q_range.emplace_back(fr::zero());
    q_logic.emplace_back(fr::zero());
    q_lookup_index.emplace_back(fr::zero());
    q_lookup_type.emplace_back(fr::zero());

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };
    epicycle fourth{ static_cast<uint32_t>(n), WireType::FOURTH };

    ASSERT(wire_epicycles.size() > in.a);
    ASSERT(wire_epicycles.size() > in.b);
    ASSERT(wire_epicycles.size() > in.c);
    ASSERT(wire_epicycles.size() > in.d);

    wire_epicycles[static_cast<size_t>(in.a)].emplace_back(left);
    wire_epicycles[static_cast<size_t>(in.b)].emplace_back(right);
    wire_epicycles[static_cast<size_t>(in.c)].emplace_back(out);
    wire_epicycles[static_cast<size_t>(in.d)].emplace_back(fourth);

    ++n;
}

void PLookupComposer::create_big_add_gate_with_bit_extraction(const add_quad& in)
{
    gate_flags.push_back(0);
    w_l.emplace_back(in.a);
    w_r.emplace_back(in.b);
    w_o.emplace_back(in.c);
    w_4.emplace_back(in.d);
    q_m.emplace_back(fr::zero());
    q_1.emplace_back(in.a_scaling);
    q_2.emplace_back(in.b_scaling);
    q_3.emplace_back(in.c_scaling);
    q_c.emplace_back(in.const_scaling);
    q_arith.emplace_back(fr::one() + fr::one());
    q_4.emplace_back(in.d_scaling);
    q_5.emplace_back(fr::zero());
    q_ecc_1.emplace_back(fr::zero());
    q_range.emplace_back(fr::zero());
    q_logic.emplace_back(fr::zero());
    q_lookup_index.emplace_back(fr::zero());
    q_lookup_type.emplace_back(fr::zero());

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };
    epicycle fourth{ static_cast<uint32_t>(n), WireType::FOURTH };

    ASSERT(wire_epicycles.size() > in.a);
    ASSERT(wire_epicycles.size() > in.b);
    ASSERT(wire_epicycles.size() > in.c);
    ASSERT(wire_epicycles.size() > in.d);

    wire_epicycles[static_cast<size_t>(in.a)].emplace_back(left);
    wire_epicycles[static_cast<size_t>(in.b)].emplace_back(right);
    wire_epicycles[static_cast<size_t>(in.c)].emplace_back(out);
    wire_epicycles[static_cast<size_t>(in.d)].emplace_back(fourth);

    ++n;
}

void PLookupComposer::create_big_mul_gate(const mul_quad& in)
{
    gate_flags.push_back(0);
    w_l.emplace_back(in.a);
    w_r.emplace_back(in.b);
    w_o.emplace_back(in.c);
    w_4.emplace_back(in.d);
    q_m.emplace_back(in.mul_scaling);
    q_1.emplace_back(in.a_scaling);
    q_2.emplace_back(in.b_scaling);
    q_3.emplace_back(in.c_scaling);
    q_c.emplace_back(in.const_scaling);
    q_arith.emplace_back(fr::one());
    q_4.emplace_back(in.d_scaling);
    q_5.emplace_back(fr::zero());
    q_ecc_1.emplace_back(fr::zero());
    q_range.emplace_back(fr::zero());
    q_logic.emplace_back(fr::zero());
    q_lookup_index.emplace_back(fr::zero());
    q_lookup_type.emplace_back(fr::zero());

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };
    epicycle fourth{ static_cast<uint32_t>(n), WireType::FOURTH };

    ASSERT(wire_epicycles.size() > in.a);
    ASSERT(wire_epicycles.size() > in.b);
    ASSERT(wire_epicycles.size() > in.c);
    ASSERT(wire_epicycles.size() > in.d);

    wire_epicycles[static_cast<size_t>(in.a)].emplace_back(left);
    wire_epicycles[static_cast<size_t>(in.b)].emplace_back(right);
    wire_epicycles[static_cast<size_t>(in.c)].emplace_back(out);
    wire_epicycles[static_cast<size_t>(in.d)].emplace_back(fourth);

    ++n;
}

// Creates a width-4 addition gate, where the fourth witness must be a boolean.
// Can be used to normalize a 32-bit addition
void PLookupComposer::create_balanced_add_gate(const add_quad& in)
{
    gate_flags.push_back(0);
    w_l.emplace_back(in.a);
    w_r.emplace_back(in.b);
    w_o.emplace_back(in.c);
    w_4.emplace_back(in.d);
    q_m.emplace_back(fr::zero());
    q_1.emplace_back(in.a_scaling);
    q_2.emplace_back(in.b_scaling);
    q_3.emplace_back(in.c_scaling);
    q_c.emplace_back(in.const_scaling);
    q_arith.emplace_back(fr::one());
    q_4.emplace_back(in.d_scaling);
    q_5.emplace_back(fr::one());
    q_ecc_1.emplace_back(fr::zero());
    q_range.emplace_back(fr::zero());
    q_logic.emplace_back(fr::zero());
    q_lookup_index.emplace_back(fr::zero());
    q_lookup_type.emplace_back(fr::zero());

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };
    epicycle fourth{ static_cast<uint32_t>(n), WireType::FOURTH };

    ASSERT(wire_epicycles.size() > in.a);
    ASSERT(wire_epicycles.size() > in.b);
    ASSERT(wire_epicycles.size() > in.c);
    ASSERT(wire_epicycles.size() > in.d);

    wire_epicycles[static_cast<size_t>(in.a)].emplace_back(left);
    wire_epicycles[static_cast<size_t>(in.b)].emplace_back(right);
    wire_epicycles[static_cast<size_t>(in.c)].emplace_back(out);
    wire_epicycles[static_cast<size_t>(in.d)].emplace_back(fourth);

    ++n;
}

void PLookupComposer::create_mul_gate(const mul_triple& in)
{
    gate_flags.push_back(0);
    add_gate_flag(gate_flags.size() - 1, GateFlags::FIXED_LEFT_WIRE);
    add_gate_flag(gate_flags.size() - 1, GateFlags::FIXED_RIGHT_WIRE);
    w_l.emplace_back(in.a);
    w_r.emplace_back(in.b);
    w_o.emplace_back(in.c);
    w_4.emplace_back(zero_idx);
    q_m.emplace_back(in.mul_scaling);
    q_1.emplace_back(fr::zero());
    q_2.emplace_back(fr::zero());
    q_3.emplace_back(in.c_scaling);
    q_c.emplace_back(in.const_scaling);
    q_arith.emplace_back(fr::one());
    q_4.emplace_back(fr::zero());
    q_5.emplace_back(fr::zero());
    q_ecc_1.emplace_back(fr::zero());
    q_range.emplace_back(fr::zero());
    q_logic.emplace_back(fr::zero());
    q_lookup_index.emplace_back(fr::zero());
    q_lookup_type.emplace_back(fr::zero());

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };

    ASSERT(wire_epicycles.size() > in.a);
    ASSERT(wire_epicycles.size() > in.b);
    ASSERT(wire_epicycles.size() > in.c);
    ASSERT(wire_epicycles.size() > zero_idx);

    wire_epicycles[static_cast<size_t>(in.a)].emplace_back(left);
    wire_epicycles[static_cast<size_t>(in.b)].emplace_back(right);
    wire_epicycles[static_cast<size_t>(in.c)].emplace_back(out);

    ++n;
}

void PLookupComposer::create_bool_gate(const uint32_t variable_index)
{
    gate_flags.push_back(0);
    add_gate_flag(gate_flags.size() - 1, GateFlags::FIXED_LEFT_WIRE);
    add_gate_flag(gate_flags.size() - 1, GateFlags::FIXED_RIGHT_WIRE);
    w_l.emplace_back(variable_index);
    w_r.emplace_back(variable_index);
    w_o.emplace_back(variable_index);
    w_4.emplace_back(zero_idx);
    q_arith.emplace_back(fr::one());
    q_4.emplace_back(fr::zero());
    q_5.emplace_back(fr::zero());
    q_ecc_1.emplace_back(fr::zero());
    q_range.emplace_back(fr::zero());

    q_m.emplace_back(fr::one());
    q_1.emplace_back(fr::zero());
    q_2.emplace_back(fr::zero());
    q_3.emplace_back(fr::neg_one());
    q_c.emplace_back(fr::zero());
    q_logic.emplace_back(fr::zero());
    q_lookup_index.emplace_back(fr::zero());
    q_lookup_type.emplace_back(fr::zero());

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };

    ASSERT(wire_epicycles.size() > variable_index);
    wire_epicycles[static_cast<size_t>(variable_index)].emplace_back(left);
    wire_epicycles[static_cast<size_t>(variable_index)].emplace_back(right);
    wire_epicycles[static_cast<size_t>(variable_index)].emplace_back(out);

    ++n;
}

void PLookupComposer::create_poly_gate(const poly_triple& in)
{
    gate_flags.push_back(0);
    add_gate_flag(gate_flags.size() - 1, GateFlags::FIXED_LEFT_WIRE);
    add_gate_flag(gate_flags.size() - 1, GateFlags::FIXED_RIGHT_WIRE);
    w_l.emplace_back(in.a);
    w_r.emplace_back(in.b);
    w_o.emplace_back(in.c);
    w_4.emplace_back(zero_idx);
    q_m.emplace_back(in.q_m);
    q_1.emplace_back(in.q_l);
    q_2.emplace_back(in.q_r);
    q_3.emplace_back(in.q_o);
    q_c.emplace_back(in.q_c);
    q_range.emplace_back(fr::zero());
    q_logic.emplace_back(fr::zero());

    q_arith.emplace_back(fr::one());
    q_4.emplace_back(fr::zero());
    q_5.emplace_back(fr::zero());
    q_ecc_1.emplace_back(fr::zero());
    q_lookup_index.emplace_back(fr::zero());
    q_lookup_type.emplace_back(fr::zero());

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };

    ASSERT(wire_epicycles.size() > in.a);
    ASSERT(wire_epicycles.size() > in.b);
    ASSERT(wire_epicycles.size() > in.c);
    ASSERT(wire_epicycles.size() > zero_idx);

    wire_epicycles[static_cast<size_t>(in.a)].emplace_back(left);
    wire_epicycles[static_cast<size_t>(in.b)].emplace_back(right);
    wire_epicycles[static_cast<size_t>(in.c)].emplace_back(out);

    ++n;
}

void PLookupComposer::create_fixed_group_add_gate(const fixed_group_add_quad& in)
{
    gate_flags.push_back(0);
    w_l.emplace_back(in.a);
    w_r.emplace_back(in.b);
    w_o.emplace_back(in.c);
    w_4.emplace_back(in.d);

    q_arith.emplace_back(fr::zero());
    q_4.emplace_back(fr::zero());
    q_5.emplace_back(fr::zero());
    q_m.emplace_back(fr::zero());
    q_c.emplace_back(fr::zero());
    q_range.emplace_back(fr::zero());
    q_logic.emplace_back(fr::zero());

    q_1.emplace_back(in.q_x_1);
    q_2.emplace_back(in.q_x_2);
    q_3.emplace_back(in.q_y_1);
    q_ecc_1.emplace_back(in.q_y_2);
    q_lookup_index.emplace_back(fr::zero());
    q_lookup_type.emplace_back(fr::zero());

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };
    epicycle fourth{ static_cast<uint32_t>(n), WireType::FOURTH };

    ASSERT(wire_epicycles.size() > in.a);
    ASSERT(wire_epicycles.size() > in.b);
    ASSERT(wire_epicycles.size() > in.c);
    ASSERT(wire_epicycles.size() > in.d);

    wire_epicycles[static_cast<size_t>(in.a)].emplace_back(left);
    wire_epicycles[static_cast<size_t>(in.b)].emplace_back(right);
    wire_epicycles[static_cast<size_t>(in.c)].emplace_back(out);
    wire_epicycles[static_cast<size_t>(in.d)].emplace_back(fourth);

    ++n;
}

void PLookupComposer::create_fixed_group_add_gate_with_init(const fixed_group_add_quad& in,
                                                            const fixed_group_init_quad& init)
{
    gate_flags.push_back(0);
    w_l.emplace_back(in.a);
    w_r.emplace_back(in.b);
    w_o.emplace_back(in.c);
    w_4.emplace_back(in.d);

    q_arith.emplace_back(fr::zero());
    q_4.emplace_back(init.q_x_1);
    q_5.emplace_back(init.q_x_2);
    q_m.emplace_back(init.q_y_1);
    q_c.emplace_back(init.q_y_2);
    q_range.emplace_back(fr::zero());
    q_logic.emplace_back(fr::zero());

    q_1.emplace_back(in.q_x_1);
    q_2.emplace_back(in.q_x_2);
    q_3.emplace_back(in.q_y_1);
    q_ecc_1.emplace_back(in.q_y_2);
    q_lookup_index.emplace_back(fr::zero());
    q_lookup_type.emplace_back(fr::zero());

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };
    epicycle fourth{ static_cast<uint32_t>(n), WireType::FOURTH };

    ASSERT(wire_epicycles.size() > in.a);
    ASSERT(wire_epicycles.size() > in.b);
    ASSERT(wire_epicycles.size() > in.c);
    ASSERT(wire_epicycles.size() > in.d);

    wire_epicycles[static_cast<size_t>(in.a)].emplace_back(left);
    wire_epicycles[static_cast<size_t>(in.b)].emplace_back(right);
    wire_epicycles[static_cast<size_t>(in.c)].emplace_back(out);
    wire_epicycles[static_cast<size_t>(in.d)].emplace_back(fourth);

    ++n;
}

void PLookupComposer::fix_witness(const uint32_t witness_index, const barretenberg::fr& witness_value)
{
    gate_flags.push_back(0);

    w_l.emplace_back(witness_index);
    w_r.emplace_back(zero_idx);
    w_o.emplace_back(zero_idx);
    w_4.emplace_back(zero_idx);
    q_m.emplace_back(fr::zero());
    q_1.emplace_back(fr::one());
    q_2.emplace_back(fr::zero());
    q_3.emplace_back(fr::zero());
    q_c.emplace_back(-witness_value);
    q_arith.emplace_back(fr::one());
    q_4.emplace_back(fr::zero());
    q_5.emplace_back(fr::zero());
    q_ecc_1.emplace_back(fr::zero());
    q_range.emplace_back(fr::zero());
    q_logic.emplace_back(fr::zero());
    q_lookup_index.emplace_back(fr::zero());
    q_lookup_type.emplace_back(fr::zero());

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };

    ASSERT(wire_epicycles.size() > witness_index);
    ASSERT(wire_epicycles.size() > zero_idx);
    ASSERT(wire_epicycles.size() > zero_idx);
    wire_epicycles[static_cast<size_t>(witness_index)].emplace_back(left);

    ++n;
}

std::vector<uint32_t> PLookupComposer::create_range_constraint(const uint32_t witness_index, const size_t num_bits)
{
    ASSERT(static_cast<uint32_t>(variables.size()) > witness_index);
    ASSERT(((num_bits >> 1U) << 1U) == num_bits);

    /*
     * The range constraint accumulates base 4 values into a sum.
     * We do this by evaluating a kind of 'raster scan', where we compare adjacent elements
     * and validate that their differences map to a base for value  *
     * Let's say that we want to perform a 32-bit range constraint in 'x'.
     * We can represent x via 16 constituent base-4 'quads' {q_0, ..., q_15}:
     *
     *      15
     *      ===
     *      \          i
     * x =  /    q  . 4
     *      ===   i
     *     i = 0
     *
     * In program memory, we place an accumulating base-4 sum of x {a_0, ..., a_15}, where
     *
     *         i
     *        ===
     *        \                  j
     * a   =  /    q         .  4
     *  i     ===   (15 - j)
     *       j = 0
     *
     *
     * From this, we can use our range transition constraint to validate that
     *
     *
     *  a      - 4 . a  ϵ [0, 1, 2, 3]
     *   i + 1        i
     *
     *
     * We place our accumulating sums in program memory in the following sequence:
     *
     * +-----+-----+-----+-----+
     * |  A  |  B  |  C  |  D  |
     * +-----+-----+-----+-----+
     * | a3  | a2  | a1  | 0   |
     * | a7  | a6  | a5  | a4  |
     * | a11 | a10 | a9  | a8  |
     * | a15 | a14 | a13 | a12 |
     * | --- | --- | --- | a16 |
     * +-----+-----+-----+-----+
     *
     * Our range transition constraint on row 'i'
     * performs our base-4 range check on the follwing pairs:
     *
     * (D_{i}, C_{i}), (C_{i}, B_{i}), (B_{i}, A_{i}), (A_{i}, D_{i+1})
     *
     * We need to start our raster scan at zero, so we simplify matters and just force the first value
     * to be zero.
     *
     * The output will be in the 4th column of an otherwise unused row. Assuming this row can
     * be used for a width-3 standard gate, the total number of gates for an n-bit range constraint
     * is (n / 8) gates
     *
     **/

    const fr witness_value = variables[witness_index].from_montgomery_form();

    // one gate accmulates 4 quads, or 8 bits.
    // # gates = (bits / 8)
    size_t num_quad_gates = (num_bits >> 3);

    num_quad_gates = (num_quad_gates << 3 == num_bits) ? num_quad_gates : num_quad_gates + 1;

    // hmm
    std::vector<uint32_t>* wires[4]{ &w_4, &w_o, &w_r, &w_l };

    // hmmm
    WireType wire_types[4]{ WireType::FOURTH, WireType::OUTPUT, WireType::RIGHT, WireType::LEFT };

    const size_t num_quads = (num_quad_gates << 2);
    const size_t forced_zero_threshold = 1 + (((num_quads << 1) - num_bits) >> 1);
    std::vector<uint32_t> accumulators;
    fr accumulator = fr::zero();

    for (size_t i = 0; i < num_quads + 1; ++i) {
        const size_t gate_index = n + (i / 4);
        uint32_t accumulator_index;
        if (i < forced_zero_threshold) {
            accumulator_index = zero_idx;
        } else {
            const size_t bit_index = (num_quads - i) << 1;
            const uint64_t quad = static_cast<uint64_t>(witness_value.get_bit(bit_index)) +
                                  2ULL * static_cast<uint64_t>(witness_value.get_bit(bit_index + 1));
            const fr quad_element = fr{ quad, 0, 0, 0 }.to_montgomery_form();
            accumulator += accumulator;
            accumulator += accumulator;
            accumulator += quad_element;

            accumulator_index = add_variable(accumulator);
            accumulators.emplace_back(accumulator_index);
        }

        // hmmmm
        (*(wires + (i & 3)))->emplace_back(accumulator_index);
        const size_t wire_index = i & 3;

        wire_epicycles[accumulator_index].emplace_back(
            epicycle(static_cast<uint32_t>(gate_index), wire_types[wire_index]));
    }
    size_t used_gates = (num_quads + 1) / 4;

    // TODO: handle partially used gates. For now just set them to be zero
    if (used_gates * 4 != (num_quads + 1)) {
        ++used_gates;
    }

    for (size_t i = 0; i < used_gates; ++i) {
        q_m.emplace_back(fr::zero());
        q_1.emplace_back(fr::zero());
        q_2.emplace_back(fr::zero());
        q_3.emplace_back(fr::zero());
        q_c.emplace_back(fr::zero());
        q_arith.emplace_back(fr::zero());
        q_4.emplace_back(fr::zero());
        q_5.emplace_back(fr::zero());
        q_ecc_1.emplace_back(fr::zero());
        q_logic.emplace_back(fr::zero());
        q_range.emplace_back(fr::one());
        q_lookup_index.emplace_back(fr::zero());
        q_lookup_type.emplace_back(fr::zero());
    }

    q_range[q_range.size() - 1] = fr::zero();

    w_l.emplace_back(zero_idx);
    w_r.emplace_back(zero_idx);
    w_o.emplace_back(zero_idx);

    assert_equal(accumulators[accumulators.size() - 1], witness_index);
    accumulators[accumulators.size() - 1] = witness_index;

    n += used_gates;
    return accumulators;
}

waffle::accumulator_triple PLookupComposer::create_logic_constraint(const uint32_t a,
                                                                    const uint32_t b,
                                                                    const size_t num_bits,
                                                                    const bool is_xor_gate)
{
    ASSERT(static_cast<uint32_t>(variables.size()) > a);
    ASSERT(static_cast<uint32_t>(variables.size()) > b);
    ASSERT(((num_bits >> 1U) << 1U) == num_bits); // no odd number of bits! bad! only quads!

    /*
     * The LOGIC constraint accumulates 3 base-4 values (a, b, c) into a sum, where c = a & b OR c = a ^ b
     *
     * In program memory, we place an accumulating base-4 sum of a, b, c {a_0, ..., a_15}, where
     *
     *         i
     *        ===
     *        \                  j
     * a   =  /    q         .  4
     *  i     ===   (15 - j)
     *       j = 0
     *
     *
     * From this, we can use our logic transition constraint to validate that
     *
     *
     *  a      - 4 . a  ϵ [0, 1, 2, 3]
     *   i + 1        i
     *
     *
     *
     *
     *  b      - 4 . b  ϵ [0, 1, 2, 3]
     *   i + 1        i
     *
     *
     *
     *
     *                    /                 \          /                 \
     *  c      - 4 . c  = | a      - 4 . a  | (& OR ^) | b      - b . a  |
     *   i + 1        i   \  i + 1        i /          \  i + 1        i /
     *
     *
     * We also need the following temporary, w, stored in program memory:
     *
     *      /                 \   /                 \
     * w  = | a      - 4 . a  | * | b      - b . a  |
     *  i   \  i + 1        i /   \  i + 1        i /
     *
     *
     * w is needed to prevent the degree of our quotient polynomial from blowing up
     *
     * We place our accumulating sums in program memory in the following sequence:
     *
     * +-----+-----+-----+-----+
     * |  A  |  B  |  C  |  D  |
     * +-----+-----+-----+-----+
     * | 0   | 0   | w1  | 0   |
     * | a1  | b1  | w2  | c1  |
     * | a2  | b2  | w3  | c2  |
     * |  :  |  :  |  :  |  :  |
     * | an  | bn  | --- | cn  |
     * +-----+-----+-----+-----+
     *
     * Our transition constraint extracts quads by taking the difference between two accumulating sums,
     * so we need to start the chain with a row of zeroes
     *
     * The total number of gates required to evaluate an AND operation is (n / 2) + 1,
     * where n = max(num_bits(a), num_bits(b))
     *
     * One additional benefit of this constraint, is that both our inputs and output are in 'native' uint32 form.
     * This means we *never* have to decompose a uint32 into bits and back in order to chain together
     * addition and logic operations.
     *
     **/

    const fr left_witness_value = variables[a].from_montgomery_form();
    const fr right_witness_value = variables[b].from_montgomery_form();

    // one gate accmulates 1 quads, or 2 bits.
    // # gates = (bits / 2)
    const size_t num_quads = (num_bits >> 1);

    waffle::accumulator_triple accumulators;
    fr left_accumulator = fr::zero();
    fr right_accumulator = fr::zero();
    fr out_accumulator = fr::zero();

    // Step 1: populare 1st row accumulators with zero
    w_l.emplace_back(zero_idx);
    w_r.emplace_back(zero_idx);
    w_4.emplace_back(zero_idx);

    wire_epicycles[zero_idx].emplace_back(epicycle(static_cast<uint32_t>(n), WireType::LEFT));
    wire_epicycles[zero_idx].emplace_back(epicycle(static_cast<uint32_t>(n), WireType::RIGHT));
    wire_epicycles[zero_idx].emplace_back(epicycle(static_cast<uint32_t>(n), WireType::FOURTH));

    // w_l, w_r, w_4 should now point to 1 gate ahead of w_o
    for (size_t i = 0; i < num_quads; ++i) {
        const size_t gate_index = n + i + 1;
        uint32_t left_accumulator_index;
        uint32_t right_accumulator_index;
        uint32_t out_accumulator_index;
        uint32_t product_index;

        const size_t bit_index = (num_quads - 1 - i) << 1;
        const uint64_t left_quad = static_cast<uint64_t>(left_witness_value.get_bit(bit_index)) +
                                   2ULL * static_cast<uint64_t>(left_witness_value.get_bit(bit_index + 1));

        const uint64_t right_quad = static_cast<uint64_t>(right_witness_value.get_bit(bit_index)) +
                                    2ULL * static_cast<uint64_t>(right_witness_value.get_bit(bit_index + 1));
        const fr left_quad_element = fr{ left_quad, 0, 0, 0 }.to_montgomery_form();
        const fr right_quad_element = fr{ right_quad, 0, 0, 0 }.to_montgomery_form();
        fr out_quad_element;
        if (is_xor_gate) {
            out_quad_element = fr{ left_quad ^ right_quad, 0, 0, 0 }.to_montgomery_form();
        } else {
            out_quad_element = fr{ left_quad & right_quad, 0, 0, 0 }.to_montgomery_form();
        }

        const fr product_quad_element = fr{ left_quad * right_quad, 0, 0, 0 }.to_montgomery_form();

        left_accumulator += left_accumulator;
        left_accumulator += left_accumulator;
        left_accumulator += left_quad_element;

        right_accumulator += right_accumulator;
        right_accumulator += right_accumulator;
        right_accumulator += right_quad_element;

        out_accumulator += out_accumulator;
        out_accumulator += out_accumulator;
        out_accumulator += out_quad_element;

        left_accumulator_index = add_variable(left_accumulator);
        accumulators.left.emplace_back(left_accumulator_index);

        right_accumulator_index = add_variable(right_accumulator);
        accumulators.right.emplace_back(right_accumulator_index);

        out_accumulator_index = add_variable(out_accumulator);
        accumulators.out.emplace_back(out_accumulator_index);

        product_index = add_variable(product_quad_element);

        w_l.emplace_back(left_accumulator_index);
        w_r.emplace_back(right_accumulator_index);
        w_4.emplace_back(out_accumulator_index);
        w_o.emplace_back(product_index);

        wire_epicycles[left_accumulator_index].emplace_back(
            epicycle(static_cast<uint32_t>(gate_index), WireType::LEFT));
        wire_epicycles[right_accumulator_index].emplace_back(
            epicycle(static_cast<uint32_t>(gate_index), WireType::RIGHT));
        wire_epicycles[out_accumulator_index].emplace_back(
            epicycle(static_cast<uint32_t>(gate_index), WireType::FOURTH));
        wire_epicycles[product_index].emplace_back(epicycle(static_cast<uint32_t>(gate_index - 1), WireType::OUTPUT));
    }

    w_o.emplace_back(zero_idx);

    for (size_t i = 0; i < num_quads + 1; ++i) {
        q_m.emplace_back(fr::zero());
        q_1.emplace_back(fr::zero());
        q_2.emplace_back(fr::zero());
        q_3.emplace_back(fr::zero());
        q_arith.emplace_back(fr::zero());
        q_4.emplace_back(fr::zero());
        q_5.emplace_back(fr::zero());
        q_ecc_1.emplace_back(fr::zero());
        q_range.emplace_back(fr::zero());
        if (is_xor_gate) {
            q_c.emplace_back(fr::neg_one());
            q_logic.emplace_back(fr::neg_one());
        } else {
            q_c.emplace_back(fr::one());
            q_logic.emplace_back(fr::one());
        }
        q_lookup_index.emplace_back(fr::zero());
        q_lookup_type.emplace_back(fr::zero());
    }
    q_c[q_c.size() - 1] = fr::zero();         // last gate is a noop
    q_logic[q_logic.size() - 1] = fr::zero(); // last gate is a noop

    assert_equal(accumulators.left[accumulators.left.size() - 1], a);
    accumulators.left[accumulators.left.size() - 1] = a;

    assert_equal(accumulators.right[accumulators.right.size() - 1], b);
    accumulators.right[accumulators.right.size() - 1] = b;

    n += (num_quads + 1);
    return accumulators;
}

waffle::accumulator_triple PLookupComposer::create_and_constraint(const uint32_t a,
                                                                  const uint32_t b,
                                                                  const size_t num_bits)
{
    return create_logic_constraint(a, b, num_bits, false);
}

waffle::accumulator_triple PLookupComposer::create_xor_constraint(const uint32_t a,
                                                                  const uint32_t b,
                                                                  const size_t num_bits)
{
    return create_logic_constraint(a, b, num_bits, true);
}

uint32_t PLookupComposer::put_constant_variable(const barretenberg::fr& variable)
{
    if (constant_variables.count(variable) == 1) {
        return constant_variables.at(variable);
    } else {
        uint32_t variable_index = add_variable(variable);
        fix_witness(variable_index, variable);
        constant_variables.insert({ variable, variable_index });
        return variable_index;
    }
}

void PLookupComposer::add_lookup_selector(polynomial& small, const std::string& tag)
{
    polynomial lagrange_base(small, circuit_proving_key->small_domain.size + 1);
    small.ifft(circuit_proving_key->small_domain);
    polynomial large(small, circuit_proving_key->n * 4 + 4);
    large.coset_fft(circuit_proving_key->large_domain);

    large.add_lagrange_base_coefficient(large[0]);
    large.add_lagrange_base_coefficient(large[1]);
    large.add_lagrange_base_coefficient(large[2]);
    large.add_lagrange_base_coefficient(large[3]);

    circuit_proving_key->permutation_selectors.insert({ tag, std::move(small) });
    circuit_proving_key->permutation_selectors_lagrange_base.insert({ tag, std::move(lagrange_base) });
    circuit_proving_key->permutation_selector_ffts.insert({ tag + "_fft", std::move(large) });
}

std::shared_ptr<proving_key> PLookupComposer::compute_proving_key()
{
    if (computed_proving_key) {
        return circuit_proving_key;
    }
    create_dummy_gate();
    ASSERT(wire_epicycles.size() == variables.size());
    ASSERT(n == q_m.size());
    ASSERT(n == q_1.size());
    ASSERT(n == q_2.size());
    ASSERT(n == q_3.size());
    ASSERT(n == q_3.size());
    ASSERT(n == q_4.size());
    ASSERT(n == q_5.size());
    ASSERT(n == q_arith.size());
    ASSERT(n == q_ecc_1.size());
    ASSERT(n == q_range.size());
    ASSERT(n == q_logic.size());
    ASSERT(n == q_lookup_index.size());
    ASSERT(n == q_lookup_type.size());

    size_t tables_size = 0;
    size_t lookups_size = 0;
    for (const auto& table : lookup_tables) {
        tables_size += table.size;
        lookups_size += table.lookup_gates.size();
    }

    const size_t filled_gates = n + public_inputs.size();
    const size_t total_num_gates = std::max(filled_gates, tables_size + lookups_size);

    size_t log2_n = static_cast<size_t>(numeric::get_msb(total_num_gates + 1));
    if ((1UL << log2_n) != (total_num_gates + 1)) {
        ++log2_n;
    }
    size_t new_n = 1UL << log2_n;

    for (size_t i = filled_gates; i < new_n; ++i) {
        q_m.emplace_back(fr::zero());
        q_1.emplace_back(fr::zero());
        q_2.emplace_back(fr::zero());
        q_3.emplace_back(fr::zero());
        q_c.emplace_back(fr::zero());
        q_4.emplace_back(fr::zero());
        q_5.emplace_back(fr::zero());
        q_arith.emplace_back(fr::zero());
        q_ecc_1.emplace_back(fr::zero());
        q_range.emplace_back(fr::zero());
        q_logic.emplace_back(fr::zero());
        q_lookup_index.emplace_back(fr::zero());
        q_lookup_type.emplace_back(fr::zero());
    }

    for (size_t i = 0; i < public_inputs.size(); ++i) {
        epicycle left{ static_cast<uint32_t>(i - public_inputs.size()), WireType::LEFT };
        epicycle right{ static_cast<uint32_t>(i - public_inputs.size()), WireType::RIGHT };

        std::vector<epicycle>& old_epicycles = wire_epicycles[static_cast<size_t>(public_inputs[i])];

        std::vector<epicycle> new_epicycles;

        new_epicycles.emplace_back(left);
        new_epicycles.emplace_back(right);
        for (size_t i = 0; i < old_epicycles.size(); ++i) {
            new_epicycles.emplace_back(old_epicycles[i]);
        }
        old_epicycles = new_epicycles;
    }
    auto crs = crs_factory_->get_prover_crs(new_n);
    circuit_proving_key = std::make_shared<proving_key>(new_n, public_inputs.size(), crs);

    polynomial poly_q_m(new_n);
    polynomial poly_q_c(new_n);
    polynomial poly_q_1(new_n);
    polynomial poly_q_2(new_n);
    polynomial poly_q_3(new_n);
    polynomial poly_q_4(new_n);
    polynomial poly_q_5(new_n);
    polynomial poly_q_arith(new_n);
    polynomial poly_q_ecc_1(new_n);
    polynomial poly_q_range(new_n);
    polynomial poly_q_logic(new_n);
    polynomial poly_q_lookup_index(new_n + 1);
    polynomial poly_q_lookup_type(new_n + 1);

    for (size_t i = 0; i < public_inputs.size(); ++i) {
        poly_q_m[i] = fr::zero();
        poly_q_1[i] = fr::one();
        poly_q_2[i] = fr::zero();
        poly_q_3[i] = fr::zero();
        poly_q_4[i] = fr::zero();
        poly_q_5[i] = fr::zero();
        poly_q_arith[i] = fr::zero();
        poly_q_ecc_1[i] = fr::zero();
        poly_q_c[i] = fr::zero();
        poly_q_range[i] = fr::zero();
        poly_q_logic[i] = fr::zero();
        poly_q_lookup_index[i] = fr::zero();
        poly_q_lookup_type[i] = fr::zero();
    }

    for (size_t i = public_inputs.size(); i < new_n; ++i) {
        poly_q_m[i] = q_m[i - public_inputs.size()];
        poly_q_1[i] = q_1[i - public_inputs.size()];
        poly_q_2[i] = q_2[i - public_inputs.size()];
        poly_q_3[i] = q_3[i - public_inputs.size()];
        poly_q_c[i] = q_c[i - public_inputs.size()];
        poly_q_4[i] = q_4[i - public_inputs.size()];
        poly_q_5[i] = q_5[i - public_inputs.size()];
        poly_q_arith[i] = q_arith[i - public_inputs.size()];
        poly_q_ecc_1[i] = q_ecc_1[i - public_inputs.size()];
        poly_q_range[i] = q_range[i - public_inputs.size()];
        poly_q_logic[i] = q_logic[i - public_inputs.size()];
        poly_q_lookup_index[i] = q_lookup_index[i - public_inputs.size()];
        poly_q_lookup_type[i] = q_lookup_type[i - public_inputs.size()];
    }

    add_selector(poly_q_1, "q_1");
    add_selector(poly_q_2, "q_2", true);
    add_selector(poly_q_3, "q_3");
    add_selector(poly_q_4, "q_4");
    add_selector(poly_q_5, "q_5");
    add_selector(poly_q_m, "q_m", true);
    add_selector(poly_q_c, "q_c", true);
    add_selector(poly_q_arith, "q_arith");
    add_selector(poly_q_ecc_1, "q_ecc_1");
    add_selector(poly_q_range, "q_range");
    add_selector(poly_q_logic, "q_logic");

    polynomial poly_q_table_1(new_n + 1);
    polynomial poly_q_table_2(new_n + 1);
    polynomial poly_q_table_3(new_n + 1);
    polynomial poly_q_table_4(new_n + 1);
    size_t offset = new_n - tables_size;

    for (size_t i = 0; i < offset; ++i) {
        poly_q_table_1[i] = fr::zero();
        poly_q_table_2[i] = fr::zero();
        poly_q_table_3[i] = fr::zero();
        poly_q_table_4[i] = fr::zero();
    }

    for (const auto& table : lookup_tables) {
        const fr table_index(table.table_index);

        for (size_t i = 0; i < table.size; ++i) {
            poly_q_table_1[offset] = table.column_1[i];
            poly_q_table_2[offset] = table.column_2[i];
            poly_q_table_3[offset] = table.column_3[i];
            poly_q_table_4[offset] = table_index;
            ++offset;
        }
    }

    add_lookup_selector(poly_q_table_1, "table_value_1");
    add_lookup_selector(poly_q_table_2, "table_value_2");
    add_lookup_selector(poly_q_table_3, "table_value_3");
    add_lookup_selector(poly_q_table_4, "table_value_4");
    add_lookup_selector(poly_q_lookup_index, "table_index");
    add_lookup_selector(poly_q_lookup_type, "table_type");

    polynomial z_lookup_fft(new_n * 4 + 4, new_n * 4 + 4);
    polynomial s_fft(new_n * 4 + 4, new_n * 4 + 4);
    circuit_proving_key->wire_ffts.insert({ "z_lookup_fft", std::move(z_lookup_fft) });
    circuit_proving_key->wire_ffts.insert({ "s_fft", std::move(s_fft) });

    // auto& lookup_mapping = circuit_proving_key->lookup_mapping;
    // auto& table_indices = circuit_proving_key->table_indices;

    // lookup_mapping.resize(new_n);
    // table_indices.resize(new_n);
    // for (size_t i = 0; i < new_n; ++i) {
    //     lookup_mapping[i] = LookupType::NONE;
    // }

    // for (const auto& table : lookup_tables) {
    //     for (const auto& lookup_entry : table.lookup_gates) {
    //         lookup_mapping[lookup_entry.first] = lookup_entry.second;
    //         table_indices[lookup_entry.first] = table.table_index;
    //     }
    // }

    circuit_proving_key->num_lookup_tables = lookup_tables.size();

    compute_sigma_permutations<4>(circuit_proving_key.get());
    computed_proving_key = true;
    return circuit_proving_key;
}

std::shared_ptr<verification_key> PLookupComposer::compute_verification_key()
{
    if (computed_verification_key) {
        return circuit_verification_key;
    }
    if (!computed_proving_key) {
        compute_proving_key();
    }

    std::array<fr*, 21> poly_coefficients;
    poly_coefficients[0] = circuit_proving_key->constraint_selectors.at("q_1").get_coefficients();
    poly_coefficients[1] = circuit_proving_key->constraint_selectors.at("q_2").get_coefficients();
    poly_coefficients[2] = circuit_proving_key->constraint_selectors.at("q_3").get_coefficients();
    poly_coefficients[3] = circuit_proving_key->constraint_selectors.at("q_4").get_coefficients();
    poly_coefficients[4] = circuit_proving_key->constraint_selectors.at("q_5").get_coefficients();
    poly_coefficients[5] = circuit_proving_key->constraint_selectors.at("q_m").get_coefficients();
    poly_coefficients[6] = circuit_proving_key->constraint_selectors.at("q_c").get_coefficients();
    poly_coefficients[7] = circuit_proving_key->constraint_selectors.at("q_arith").get_coefficients();
    poly_coefficients[8] = circuit_proving_key->constraint_selectors.at("q_ecc_1").get_coefficients();
    poly_coefficients[9] = circuit_proving_key->constraint_selectors.at("q_range").get_coefficients();
    poly_coefficients[10] = circuit_proving_key->constraint_selectors.at("q_logic").get_coefficients();

    poly_coefficients[11] = circuit_proving_key->permutation_selectors.at("sigma_1").get_coefficients();
    poly_coefficients[12] = circuit_proving_key->permutation_selectors.at("sigma_2").get_coefficients();
    poly_coefficients[13] = circuit_proving_key->permutation_selectors.at("sigma_3").get_coefficients();
    poly_coefficients[14] = circuit_proving_key->permutation_selectors.at("sigma_4").get_coefficients();

    poly_coefficients[15] = circuit_proving_key->permutation_selectors.at("table_value_1").get_coefficients();
    poly_coefficients[16] = circuit_proving_key->permutation_selectors.at("table_value_2").get_coefficients();
    poly_coefficients[17] = circuit_proving_key->permutation_selectors.at("table_value_3").get_coefficients();
    poly_coefficients[18] = circuit_proving_key->permutation_selectors.at("table_value_4").get_coefficients();
    poly_coefficients[19] = circuit_proving_key->permutation_selectors.at("table_index").get_coefficients();
    poly_coefficients[20] = circuit_proving_key->permutation_selectors.at("table_type").get_coefficients();

    std::vector<barretenberg::g1::affine_element> commitments;
    commitments.resize(21);

    for (size_t i = 0; i < 21; ++i) {
        commitments[i] =
            g1::affine_element(scalar_multiplication::pippenger(poly_coefficients[i],
                                                                circuit_proving_key->reference_string->get_monomials(),
                                                                circuit_proving_key->n,
                                                                circuit_proving_key->pippenger_runtime_state));
    }

    auto crs = crs_factory_->get_verifier_crs();
    circuit_verification_key =
        std::make_shared<verification_key>(circuit_proving_key->n, circuit_proving_key->num_public_inputs, crs);

    circuit_verification_key->constraint_selectors.insert({ "Q_1", commitments[0] });
    circuit_verification_key->constraint_selectors.insert({ "Q_2", commitments[1] });
    circuit_verification_key->constraint_selectors.insert({ "Q_3", commitments[2] });
    circuit_verification_key->constraint_selectors.insert({ "Q_4", commitments[3] });
    circuit_verification_key->constraint_selectors.insert({ "Q_5", commitments[4] });
    circuit_verification_key->constraint_selectors.insert({ "Q_M", commitments[5] });
    circuit_verification_key->constraint_selectors.insert({ "Q_C", commitments[6] });
    circuit_verification_key->constraint_selectors.insert({ "Q_ARITHMETIC_SELECTOR", commitments[7] });
    circuit_verification_key->constraint_selectors.insert({ "Q_FIXED_BASE_SELECTOR", commitments[8] });
    circuit_verification_key->constraint_selectors.insert({ "Q_RANGE_SELECTOR", commitments[9] });
    circuit_verification_key->constraint_selectors.insert({ "Q_LOGIC_SELECTOR", commitments[10] });

    circuit_verification_key->permutation_selectors.insert({ "SIGMA_1", commitments[11] });
    circuit_verification_key->permutation_selectors.insert({ "SIGMA_2", commitments[12] });
    circuit_verification_key->permutation_selectors.insert({ "SIGMA_3", commitments[13] });
    circuit_verification_key->permutation_selectors.insert({ "SIGMA_4", commitments[14] });

    circuit_verification_key->permutation_selectors.insert({ "TABLE_1", commitments[15] });
    circuit_verification_key->permutation_selectors.insert({ "TABLE_2", commitments[16] });
    circuit_verification_key->permutation_selectors.insert({ "TABLE_3", commitments[17] });
    circuit_verification_key->permutation_selectors.insert({ "TABLE_4", commitments[18] });

    circuit_verification_key->permutation_selectors.insert({ "TABLE_INDEX", commitments[19] });
    circuit_verification_key->permutation_selectors.insert({ "TABLE_TYPE", commitments[20] });

    computed_verification_key = true;
    return circuit_verification_key;
}

std::shared_ptr<program_witness> PLookupComposer::compute_witness()
{
    if (computed_witness) {
        return witness;
    }

    size_t tables_size = 0;
    size_t lookups_size = 0;
    for (const auto& table : lookup_tables) {
        tables_size += table.size;
        lookups_size += table.lookup_gates.size();
    }

    const size_t filled_gates = n + public_inputs.size();
    const size_t total_num_gates = std::max(filled_gates, tables_size + lookups_size);

    size_t log2_n = static_cast<size_t>(numeric::get_msb(total_num_gates + 1));
    if ((1UL << log2_n) != (total_num_gates + 1)) {
        ++log2_n;
    }
    size_t new_n = 1UL << log2_n;

    for (size_t i = filled_gates; i < new_n; ++i) {
        w_l.emplace_back(zero_idx);
        w_r.emplace_back(zero_idx);
        w_o.emplace_back(zero_idx);
        w_4.emplace_back(zero_idx);
    }

    polynomial poly_w_1(new_n);
    polynomial poly_w_2(new_n);
    polynomial poly_w_3(new_n);
    polynomial poly_w_4(new_n);
    polynomial s_1(new_n);
    polynomial s_2(new_n);
    polynomial s_3(new_n);
    polynomial s_4(new_n);
    polynomial z_lookup(new_n + 1);
    for (size_t i = 0; i < public_inputs.size(); ++i) {
        poly_w_1[i] = fr(0);
        poly_w_2[i] = variables[public_inputs[i]];
        poly_w_3[i] = fr(0);
        poly_w_4[i] = fr(0);
    }
    for (size_t i = public_inputs.size(); i < new_n; ++i) {
        poly_w_1[i] = variables[w_l[i - public_inputs.size()]];
        poly_w_2[i] = variables[w_r[i - public_inputs.size()]];
        poly_w_3[i] = variables[w_o[i - public_inputs.size()]];
        poly_w_4[i] = variables[w_4[i - public_inputs.size()]];
    }

    size_t count = new_n - tables_size - lookups_size;
    for (size_t i = 0; i < count; ++i) {
        s_1[i] = fr::zero();
        s_2[i] = fr::zero();
        s_3[i] = fr::zero();
        s_4[i] = fr::zero();
    }

    for (auto& table : lookup_tables) {
        const fr table_index(table.table_index);
        auto& lookup_gates = table.lookup_gates;
        for (size_t i = 0; i < table.size; ++i) {
            if (table.use_twin_keys) {
                lookup_gates.push_back({
                    {
                        table.column_1[i].from_montgomery_form().data[0],
                        table.column_2[i].from_montgomery_form().data[0],
                    },
                    {
                        table.column_3[i],
                        fr(0),
                    },
                });
            } else {
                lookup_gates.push_back({
                    {
                        table.column_1[i].from_montgomery_form().data[0],
                        0,
                    },
                    {
                        table.column_2[i],
                        table.column_3[i],
                    },
                });
            }
        }

        std::sort(lookup_gates.begin(), lookup_gates.end());

        for (const auto& entry : lookup_gates) {
            const auto components = entry.to_sorted_list_components(table.use_twin_keys);
            s_1[count] = components[0];
            s_2[count] = components[1];
            s_3[count] = components[2];
            s_4[count] = table_index;
            ++count;
        }
    }

    witness = std::make_shared<program_witness>();
    witness->wires.insert({ "w_1", std::move(poly_w_1) });
    witness->wires.insert({ "w_2", std::move(poly_w_2) });
    witness->wires.insert({ "w_3", std::move(poly_w_3) });
    witness->wires.insert({ "w_4", std::move(poly_w_4) });
    witness->wires.insert({ "s", std::move(s_1) });
    witness->wires.insert({ "s_2", std::move(s_2) });
    witness->wires.insert({ "s_3", std::move(s_3) });
    witness->wires.insert({ "s_4", std::move(s_4) });
    witness->wires.insert({ "z_lookup", std::move(z_lookup) });

    computed_witness = true;
    return witness;
}

PLookupProver PLookupComposer::create_prover()
{
    compute_proving_key();
    compute_witness();

    PLookupProver output_state(circuit_proving_key, witness, create_manifest(public_inputs.size()));

    std::unique_ptr<ProverPermutationWidget<4>> permutation_widget =
        std::make_unique<ProverPermutationWidget<4>>(circuit_proving_key.get(), witness.get());
    std::unique_ptr<ProverPLookupWidget> plookup_widget =
        std::make_unique<ProverPLookupWidget>(circuit_proving_key.get(), witness.get());
    std::unique_ptr<ProverTurboFixedBaseWidget> fixed_base_widget =
        std::make_unique<ProverTurboFixedBaseWidget>(circuit_proving_key.get(), witness.get());
    std::unique_ptr<ProverTurboRangeWidget> range_widget =
        std::make_unique<ProverTurboRangeWidget>(circuit_proving_key.get(), witness.get());
    std::unique_ptr<ProverTurboLogicWidget> logic_widget =
        std::make_unique<ProverTurboLogicWidget>(circuit_proving_key.get(), witness.get());

    output_state.widgets.emplace_back(std::move(permutation_widget));
    output_state.widgets.emplace_back(std::move(fixed_base_widget));
    output_state.widgets.emplace_back(std::move(range_widget));
    output_state.widgets.emplace_back(std::move(logic_widget));
    output_state.widgets.emplace_back(std::move(plookup_widget));
    return output_state;
}

UnrolledPLookupProver PLookupComposer::create_unrolled_prover()
{
    compute_proving_key();
    compute_witness();

    UnrolledPLookupProver output_state(circuit_proving_key, witness, create_unrolled_manifest(public_inputs.size()));

    std::unique_ptr<ProverPermutationWidget<4>> permutation_widget =
        std::make_unique<ProverPermutationWidget<4>>(circuit_proving_key.get(), witness.get());
    std::unique_ptr<ProverPLookupWidget> plookup_widget =
        std::make_unique<ProverPLookupWidget>(circuit_proving_key.get(), witness.get());
    std::unique_ptr<ProverTurboFixedBaseWidget> fixed_base_widget =
        std::make_unique<ProverTurboFixedBaseWidget>(circuit_proving_key.get(), witness.get());
    std::unique_ptr<ProverTurboRangeWidget> range_widget =
        std::make_unique<ProverTurboRangeWidget>(circuit_proving_key.get(), witness.get());
    std::unique_ptr<ProverTurboLogicWidget> logic_widget =
        std::make_unique<ProverTurboLogicWidget>(circuit_proving_key.get(), witness.get());

    output_state.widgets.emplace_back(std::move(permutation_widget));
    output_state.widgets.emplace_back(std::move(fixed_base_widget));
    output_state.widgets.emplace_back(std::move(range_widget));
    output_state.widgets.emplace_back(std::move(logic_widget));
    output_state.widgets.emplace_back(std::move(plookup_widget));

    return output_state;
}

PLookupVerifier PLookupComposer::create_verifier()
{
    compute_verification_key();

    PLookupVerifier output_state(circuit_verification_key, create_manifest(public_inputs.size()));

    return output_state;
}

UnrolledPLookupVerifier PLookupComposer::create_unrolled_verifier()
{
    compute_verification_key();

    UnrolledPLookupVerifier output_state(circuit_verification_key, create_unrolled_manifest(public_inputs.size()));

    return output_state;
}

void PLookupComposer::initialize_precomputed_table(
    const PLookupTableId id,
    bool (*generator)(std::vector<fr>&, std ::vector<fr>&, std::vector<fr>&),
    std::array<fr, 2> (*get_values_from_key)(const std::array<uint64_t, 2>))
{
    for (auto table : lookup_tables) {
        ASSERT(table.id != id);
    }
    PLookupTable new_table;
    new_table.id = id;
    new_table.table_index = lookup_tables.size() + 1;
    new_table.use_twin_keys = generator(new_table.column_1, new_table.column_2, new_table.column_3);
    new_table.size = new_table.column_1.size();
    new_table.get_values_from_key = get_values_from_key;
    lookup_tables.emplace_back(new_table);
}

PLookupTable& PLookupComposer::get_table(const PLookupTableId id)
{
    for (PLookupTable& table : lookup_tables) {
        if (table.id == id) {
            return table;
        }
    }
    // Hmm. table doesn't exist! try to create it
    switch (id) {
    case AES_SPARSE_MAP: {
        lookup_tables.emplace_back(
            std::move(aes128_tables::generate_aes_sparse_table(AES_SPARSE_MAP, lookup_tables.size())));
        return get_table(id);
    }
    case AES_SBOX_MAP: {
        lookup_tables.emplace_back(
            std::move(aes128_tables::generate_aes_sbox_table(AES_SBOX_MAP, lookup_tables.size())));
        return get_table(id);
    }
    case AES_SPARSE_NORMALIZE: {
        lookup_tables.emplace_back(std::move(
            aes128_tables::generate_aes_sparse_normalization_table(AES_SPARSE_NORMALIZE, lookup_tables.size())));
        return get_table(id);
    }
    case SHA256_WITNESS_NORMALIZE: {
        lookup_tables.emplace_back(std::move(sha256_tables::generate_witness_extension_normalization_table(
            SHA256_WITNESS_NORMALIZE, lookup_tables.size())));
        return get_table(id);
    }
    case SHA256_WITNESS_SLICE_3: {
        lookup_tables.emplace_back(std::move(sha256_tables::generate_witness_extension_table<16, 3, 0, 0>(
            SHA256_WITNESS_SLICE_3, lookup_tables.size())));
        return get_table(id);
    }
    case SHA256_WITNESS_SLICE_7_ROTATE_4: {
        lookup_tables.emplace_back(std::move(sha256_tables::generate_witness_extension_table<16, 7, 4, 0>(
            SHA256_WITNESS_SLICE_7_ROTATE_4, lookup_tables.size())));
        return get_table(id);
    }
    case SHA256_WITNESS_SLICE_8_ROTATE_7: {
        lookup_tables.emplace_back(std::move(sha256_tables::generate_witness_extension_table<16, 8, 7, 0>(
            SHA256_WITNESS_SLICE_8_ROTATE_7, lookup_tables.size())));
        return get_table(id);
    }
    case SHA256_WITNESS_SLICE_14_ROTATE_1: {
        lookup_tables.emplace_back(std::move(sha256_tables::generate_witness_extension_table<16, 14, 1, 0>(
            SHA256_WITNESS_SLICE_14_ROTATE_1, lookup_tables.size())));
        return get_table(id);
    }
    case SHA256_CH_NORMALIZE: {
        lookup_tables.emplace_back(
            std::move(sha256_tables::generate_choose_normalization_table(SHA256_CH_NORMALIZE, lookup_tables.size())));
        return get_table(id);
    }
    case SHA256_MAJ_NORMALIZE: {
        lookup_tables.emplace_back(std::move(
            sha256_tables::generate_majority_normalization_table(SHA256_MAJ_NORMALIZE, lookup_tables.size())));
        return get_table(id);
    }
    case SHA256_BASE28: {
        lookup_tables.emplace_back(
            std::move(sha256_tables::generate_sha256_sparse_table<28, 0>(SHA256_BASE28, lookup_tables.size())));
        return get_table(id);
    }
    case SHA256_BASE28_ROTATE6: {
        lookup_tables.emplace_back(
            std::move(sha256_tables::generate_sha256_sparse_table<28, 6>(SHA256_BASE28_ROTATE6, lookup_tables.size())));
        return get_table(id);
    }
    case SHA256_BASE28_ROTATE3: {
        lookup_tables.emplace_back(
            std::move(sha256_tables::generate_sha256_sparse_table<28, 3>(SHA256_BASE28_ROTATE3, lookup_tables.size())));
        return get_table(id);
    }
    case SHA256_BASE16: {
        lookup_tables.emplace_back(
            std::move(sha256_tables::generate_sha256_sparse_table<16, 0>(SHA256_BASE16, lookup_tables.size())));
        return get_table(id);
    }
    case SHA256_BASE16_ROTATE2: {
        lookup_tables.emplace_back(
            std::move(sha256_tables::generate_sha256_sparse_table<16, 2>(SHA256_BASE16_ROTATE2, lookup_tables.size())));
        return get_table(id);
    }
    default: {
        throw;
    }
    }
}

PLookupMultiTable& PLookupComposer::get_multi_table(const PLookupMultiTableId id)
{
    for (PLookupMultiTable& table : lookup_multi_tables) {
        if (table.id == id) {
            return table;
        }
    }
    // Hmm. table doesn't exist! try to create it
    switch (id) {
    case SHA256_CH_INPUT: {
        lookup_multi_tables.emplace_back(std::move(sha256_tables::get_choose_input_table(id)));
        return get_multi_table(id);
    }
    case SHA256_MAJ_INPUT: {
        lookup_multi_tables.emplace_back(std::move(sha256_tables::get_majority_input_table(id)));
        return get_multi_table(id);
    }
    case SHA256_WITNESS_INPUT: {
        lookup_multi_tables.emplace_back(std::move(sha256_tables::get_witness_extension_input_table(id)));
        return get_multi_table(id);
    }
    case SHA256_CH_OUTPUT: {
        lookup_multi_tables.emplace_back(std::move(sha256_tables::get_choose_output_table(id)));
        return get_multi_table(id);
    }
    case SHA256_MAJ_OUTPUT: {
        lookup_multi_tables.emplace_back(std::move(sha256_tables::get_majority_output_table(id)));
        return get_multi_table(id);
    }
    case SHA256_WITNESS_OUTPUT: {
        lookup_multi_tables.emplace_back(std::move(sha256_tables::get_witness_extension_output_table(id)));
        return get_multi_table(id);
    }
    default: {
        throw;
    }
    }
}

void PLookupComposer::validate_lookup(const PLookupTableId id, const std::array<uint32_t, 3> indices)
{
    PLookupTable& table = get_table(id);

    table.lookup_gates.push_back({ {
                                       variables[indices[0]].from_montgomery_form().data[0],
                                       variables[indices[1]].from_montgomery_form().data[0],
                                   },
                                   {
                                       variables[indices[2]],
                                       fr(0),
                                   } });

    q_lookup_type.emplace_back(fr::one());
    q_lookup_index.emplace_back(fr(table.table_index));
    w_l.emplace_back(indices[0]);
    w_r.emplace_back(indices[1]);
    w_o.emplace_back(indices[2]);
    w_4.emplace_back(zero_idx);
    q_1.emplace_back(fr(0));
    q_2.emplace_back(fr(0));
    q_3.emplace_back(fr(0));
    q_m.emplace_back(fr(0));
    q_c.emplace_back(fr(0));
    q_arith.emplace_back(fr(0));
    q_4.emplace_back(fr(0));
    q_5.emplace_back(fr(0));
    q_ecc_1.emplace_back(fr(0));
    q_range.emplace_back(fr(0));
    q_logic.emplace_back(fr(0));

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };

    ASSERT(wire_epicycles.size() > indices[0]);
    ASSERT(wire_epicycles.size() > indices[1]);
    ASSERT(wire_epicycles.size() > indices[2]);

    wire_epicycles[static_cast<size_t>(indices[0])].emplace_back(left);
    wire_epicycles[static_cast<size_t>(indices[1])].emplace_back(right);
    wire_epicycles[static_cast<size_t>(indices[2])].emplace_back(out);

    ++n;
}

uint32_t PLookupComposer::read_from_table(const PLookupTableId id,
                                          const uint32_t first_key_idx,
                                          const uint32_t second_key_idx)
{
    const std::array<uint32_t, 2> key_indices{
        first_key_idx,
        second_key_idx == UINT32_MAX ? zero_idx : second_key_idx,
    };

    const std::array<uint64_t, 2> keys{
        variables[key_indices[0]].from_montgomery_form().data[0],
        variables[key_indices[1]].from_montgomery_form().data[0],
    };

    PLookupTable& table = get_table(id);

    const auto values = table.get_values_from_key(keys);

    const uint32_t value_index = add_variable(table.get_values_from_key(keys)[0]);

    table.lookup_gates.push_back({ keys, values });

    q_lookup_type.emplace_back(fr::one());
    q_lookup_index.emplace_back(fr(table.table_index));
    w_l.emplace_back(key_indices[0]);
    w_r.emplace_back(key_indices[1]);
    w_o.emplace_back(value_index);
    w_4.emplace_back(zero_idx);
    q_1.emplace_back(fr(0));
    q_2.emplace_back(fr(0));
    q_3.emplace_back(fr(0));
    q_m.emplace_back(fr(0));
    q_c.emplace_back(fr(0));
    q_arith.emplace_back(fr(0));
    q_4.emplace_back(fr(0));
    q_5.emplace_back(fr(0));
    q_ecc_1.emplace_back(fr(0));
    q_range.emplace_back(fr(0));
    q_logic.emplace_back(fr(0));

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };

    ASSERT(wire_epicycles.size() > key_indices[0]);
    ASSERT(wire_epicycles.size() > key_indices[1]);
    ASSERT(wire_epicycles.size() > value_index);

    wire_epicycles[static_cast<size_t>(key_indices[0])].emplace_back(left);
    wire_epicycles[static_cast<size_t>(key_indices[1])].emplace_back(right);
    wire_epicycles[static_cast<size_t>(value_index)].emplace_back(out);

    ++n;

    return value_index;
}

std::array<uint32_t, 2> PLookupComposer::read_from_table(const PLookupTableId id, const uint32_t key_idx)
{
    const std::array<uint32_t, 2> key_indices{
        key_idx,
        zero_idx,
    };

    const std::array<uint64_t, 2> keys{
        get_variable(key_indices[0]).from_montgomery_form().data[0],
        0,
    };

    PLookupTable& table = get_table(id);

    const auto values = table.get_values_from_key(keys);
    const std::array<uint32_t, 2> value_indices{
        add_variable(table.get_values_from_key(keys)[0]),
        add_variable(table.get_values_from_key(keys)[1]),
    };

    table.lookup_gates.push_back({ keys, values });

    q_lookup_type.emplace_back(fr::one());
    q_lookup_index.emplace_back(fr(table.table_index));
    w_l.emplace_back(key_indices[0]);
    w_r.emplace_back(value_indices[0]);
    w_o.emplace_back(value_indices[1]);
    w_4.emplace_back(zero_idx);
    q_1.emplace_back(fr(0));
    q_2.emplace_back(fr(0));
    q_3.emplace_back(fr(0));
    q_m.emplace_back(fr(0));
    q_c.emplace_back(fr(0));
    q_arith.emplace_back(fr(0));
    q_4.emplace_back(fr(0));
    q_5.emplace_back(fr(0));
    q_ecc_1.emplace_back(fr(0));
    q_range.emplace_back(fr(0));
    q_logic.emplace_back(fr(0));

    epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
    epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
    epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };

    ASSERT(wire_epicycles.size() > key_indices[0]);
    ASSERT(wire_epicycles.size() > value_indices[0]);
    ASSERT(wire_epicycles.size() > value_indices[1]);

    wire_epicycles[static_cast<size_t>(key_indices[0])].emplace_back(left);
    wire_epicycles[static_cast<size_t>(value_indices[0])].emplace_back(right);
    wire_epicycles[static_cast<size_t>(value_indices[1])].emplace_back(out);

    ++n;

    return value_indices;
}

std::array<std::vector<uint32_t>, 3> PLookupComposer::read_sequence_from_table(const PLookupTableId id,
                                                                               const uint32_t key_index_a,
                                                                               const uint32_t key_index_b,
                                                                               const size_t num_lookups)
{
    PLookupTable& table = get_table(id);

    const uint64_t base_a = uint256_t(table.column_1_step_size).data[0];
    const uint64_t base_b = uint256_t(table.column_2_step_size).data[0];

    const auto slice_input = [num_lookups](const uint256_t input, const uint64_t base) {
        uint256_t target = input;
        std::vector<uint64_t> slices;

        for (size_t i = 0; i < num_lookups; ++i) {
            if (target == 0) {
                slices.push_back(0);
            } else {
                const uint64_t slice = (target % base).data[0];
                slices.push_back(slice);
                target -= slice;
                target /= base;
            }
        }
        return slices;
    };

    const bool has_key_b = key_index_b != UINT32_MAX;
    const auto input_sequence_a = slice_input(get_variable(key_index_a), base_a);
    const auto input_sequence_b = has_key_b ? slice_input(get_variable(key_index_b), base_b) : std::vector<uint64_t>();

    ASSERT(input_sequence_a.size() == input_sequence_b.size());
    std::vector<fr> column_1_values(num_lookups);
    std::vector<fr> column_2_values(num_lookups);
    std::vector<fr> column_3_values(num_lookups);

    const auto values = table.get_values_from_key(
        { input_sequence_a[num_lookups - 1], has_key_b ? input_sequence_b[num_lookups - 1] : 0 });
    column_1_values[num_lookups - 1] = (input_sequence_a[num_lookups - 1]);
    column_2_values[num_lookups - 1] = has_key_b ? input_sequence_b[num_lookups - 1] : values[0];
    column_3_values[num_lookups - 1] = has_key_b ? values[0] : values[1];

    table.lookup_gates.push_back({
        {
            input_sequence_a[num_lookups - 1],
            has_key_b ? input_sequence_b[num_lookups - 1] : 0,
        },
        {
            values[0],
            values[1],
        },
    });

    for (size_t i = 1; i < num_lookups; ++i) {
        const uint64_t key = input_sequence_a[num_lookups - 1 - i];
        const auto values = table.get_values_from_key({ key, has_key_b ? input_sequence_b[num_lookups - 1 - i] : 0 });

        const std::array<fr, 3> previous{
            column_1_values[num_lookups - i] * table.column_1_step_size,
            column_2_values[num_lookups - i] * table.column_2_step_size,
            column_3_values[num_lookups - i] * table.column_3_step_size,
        };

        const std::array<fr, 3> current{
            fr(key),
            (has_key_b ? input_sequence_b[num_lookups - 1 - i] : values[0]),
            (has_key_b ? values[0] : values[1]),
        };

        table.lookup_gates.push_back({
            {
                key,
                has_key_b ? input_sequence_b[num_lookups - 1 - i] : 0,
            },
            {
                values[0],
                values[1],
            },
        });

        const auto first = previous[0] + current[0];
        const auto second = previous[1] + current[1];
        const auto third = previous[2] + current[2];

        column_1_values[num_lookups - 1 - i] = first;
        column_2_values[num_lookups - 1 - i] = second;
        column_3_values[num_lookups - 1 - i] = third;
    }

    ASSERT(column_1_values[0] == get_variable(key_index_a));
    ASSERT(key_index_b == UINT32_MAX || column_2_values[0] == get_variable(key_index_b));

    std::array<std::vector<uint32_t>, 3> column_indices;
    for (size_t i = 0; i < num_lookups; ++i) {
        const auto first_idx = (i == 0) ? key_index_a : add_variable(column_1_values[i]);
        const auto second_idx = (i == 0 && has_key_b) ? key_index_b : add_variable(column_2_values[i]);
        const auto third_idx = add_variable(column_3_values[i]);

        column_indices[0].push_back(first_idx);
        column_indices[1].push_back(second_idx);
        column_indices[2].push_back(third_idx);

        q_lookup_type.emplace_back(fr(1));
        q_lookup_index.emplace_back(fr(table.table_index));
        w_l.emplace_back(first_idx);
        w_r.emplace_back(second_idx);
        w_o.emplace_back(third_idx);
        w_4.emplace_back(zero_idx);
        q_1.emplace_back(fr(0));
        q_2.emplace_back((i == (num_lookups - 1) ? fr(0) : -table.column_1_step_size));
        q_3.emplace_back(fr(0));
        q_m.emplace_back((i == (num_lookups - 1) ? fr(0) : -table.column_2_step_size));
        q_c.emplace_back((i == (num_lookups - 1) ? fr(0) : -table.column_3_step_size));
        q_arith.emplace_back(fr(0));
        q_4.emplace_back(fr(0));
        q_5.emplace_back(fr(0));
        q_ecc_1.emplace_back(fr(0));
        q_range.emplace_back(fr(0));
        q_logic.emplace_back(fr(0));

        epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
        epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
        epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };

        ASSERT(wire_epicycles.size() > first_idx);
        ASSERT(wire_epicycles.size() > second_idx);
        ASSERT(wire_epicycles.size() > third_idx);

        wire_epicycles[static_cast<size_t>(first_idx)].emplace_back(left);
        wire_epicycles[static_cast<size_t>(second_idx)].emplace_back(right);
        wire_epicycles[static_cast<size_t>(third_idx)].emplace_back(out);

        ++n;
    }

    return column_indices;
}

std::vector<uint32_t> PLookupComposer::read_sequence_from_table(const PLookupTableId id,
                                                                const std::vector<std::array<uint32_t, 2>>& key_indices)
{
    const size_t num_lookups = key_indices.size();

    PLookupTable& table = get_table(id);

    if (num_lookups == 0) {
        return std::vector<uint32_t>();
    }
    std::vector<uint32_t> value_indices;

    std::vector<std::array<uint64_t, 2>> keys;
    keys.reserve(key_indices.size());

    std::array<uint64_t, 2> previous_key{
        variables[key_indices[0][0]].from_montgomery_form().data[0],
        variables[key_indices[0][1]].from_montgomery_form().data[0],
    };

    const uint64_t step_1 = table.column_1_step_size.from_montgomery_form().data[0];
    const uint64_t step_2 = table.column_2_step_size.from_montgomery_form().data[0];

    std::vector<fr> lookup_values;
    lookup_values.resize(num_lookups);

    for (size_t i = 0; i < num_lookups; ++i) {
        std::array<uint64_t, 2> difference_key{};
        std::array<uint64_t, 2> key{};
        fr value;

        if (i == num_lookups - 1) {
            difference_key = previous_key;
            key = previous_key;
        } else {
            difference_key = {
                variables[key_indices[i + 1][0]].from_montgomery_form().data[0],
                variables[key_indices[i + 1][1]].from_montgomery_form().data[0],
            };
            key = {
                previous_key[0] - difference_key[0] * step_1,
                previous_key[1] - difference_key[1] * step_2,
            };
        }

        value = table.get_values_from_key(key)[0];
        lookup_values[num_lookups - 1 - i] = (value);

        previous_key = difference_key;

        table.lookup_gates.push_back({
            key,
            { value, fr(0) },
        });
    }

    for (size_t i = num_lookups - 2; i < num_lookups; --i) {
        lookup_values[i] += table.column_3_step_size * lookup_values[i + 1];
    }

    for (size_t i = 0; i < num_lookups; ++i) {
        const uint32_t value_idx = add_variable(lookup_values[i]);
        value_indices.push_back(value_idx);

        q_lookup_type.emplace_back(fr(1));
        q_lookup_index.emplace_back(fr(table.table_index));
        w_l.emplace_back(key_indices[i][0]);
        w_r.emplace_back(key_indices[i][1]);
        w_o.emplace_back(value_idx);
        w_4.emplace_back(zero_idx);
        q_1.emplace_back(fr(0));
        q_2.emplace_back((i == (num_lookups - 1) ? fr(0) : -table.column_1_step_size));
        q_3.emplace_back(fr(0));
        q_m.emplace_back((i == (num_lookups - 1) ? fr(0) : -table.column_2_step_size));
        q_c.emplace_back((i == (num_lookups - 1) ? fr(0) : -table.column_3_step_size));
        q_arith.emplace_back(fr(0));
        q_4.emplace_back(fr(0));
        q_5.emplace_back(fr(0));
        q_ecc_1.emplace_back(fr(0));
        q_range.emplace_back(fr(0));
        q_logic.emplace_back(fr(0));

        epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
        epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
        epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };

        ASSERT(wire_epicycles.size() > key_indices[i][0]);
        ASSERT(wire_epicycles.size() > key_indices[i][1]);
        ASSERT(wire_epicycles.size() > value_idx);

        wire_epicycles[static_cast<size_t>(key_indices[i][0])].emplace_back(left);
        wire_epicycles[static_cast<size_t>(key_indices[i][1])].emplace_back(right);
        wire_epicycles[static_cast<size_t>(value_idx)].emplace_back(out);

        ++n;
    }
    return value_indices;
}

PLookupReadData PLookupComposer::get_multi_table_values(const PLookupMultiTableId id, const barretenberg::fr key)
{
    const auto& multi_table = get_multi_table(id);

    const size_t num_lookups = multi_table.lookup_ids.size();

    PLookupReadData result;

    result.column_1_step_sizes.emplace_back(fr(1));
    result.column_2_step_sizes.emplace_back(fr(1));
    result.column_3_step_sizes.emplace_back(fr(1));

    std::vector<barretenberg::fr> coefficient_inverses(multi_table.column_1_coefficients.begin(),
                                                       multi_table.column_1_coefficients.end());
    std::copy(multi_table.column_2_coefficients.begin(),
              multi_table.column_2_coefficients.end(),
              std::back_inserter(coefficient_inverses));
    std::copy(multi_table.column_3_coefficients.begin(),
              multi_table.column_3_coefficients.end(),
              std::back_inserter(coefficient_inverses));

    fr::batch_invert(&coefficient_inverses[0], num_lookups * 3);

    for (size_t i = 1; i < num_lookups; ++i) {
        result.column_1_step_sizes.emplace_back(multi_table.column_1_coefficients[i] * coefficient_inverses[i - 1]);
        result.column_2_step_sizes.emplace_back(multi_table.column_2_coefficients[i] *
                                                coefficient_inverses[num_lookups + i - 1]);
        result.column_3_step_sizes.emplace_back(multi_table.column_3_coefficients[i] *
                                                coefficient_inverses[2 * num_lookups + i - 1]);
    }

    const auto keys = numeric::slice_input(key, multi_table.slice_sizes);

    std::vector<fr> column_1_raw_values;
    std::vector<fr> column_2_raw_values;
    std::vector<fr> column_3_raw_values;

    for (size_t i = 0; i < num_lookups; ++i) {
        PLookupTable& table = get_table(multi_table.lookup_ids[i]);

        const auto values = table.get_values_from_key({ keys[i], 0 });
        column_1_raw_values.emplace_back(keys[i]);
        column_2_raw_values.emplace_back(values[0]);
        column_3_raw_values.emplace_back(values[1]);

        const PLookupTable::KeyEntry key_entry{ { keys[i], 0 }, values };
        result.key_entries.emplace_back(key_entry);
    }
    result.column_1_accumulator_values.resize(num_lookups);
    result.column_2_accumulator_values.resize(num_lookups);
    result.column_3_accumulator_values.resize(num_lookups);

    result.column_1_accumulator_values[num_lookups - 1] = column_1_raw_values[num_lookups - 1];
    result.column_2_accumulator_values[num_lookups - 1] = column_2_raw_values[num_lookups - 1];
    result.column_3_accumulator_values[num_lookups - 1] = column_3_raw_values[num_lookups - 1];

    for (size_t i = 1; i < num_lookups; ++i) {
        const auto& previous_1 = result.column_1_accumulator_values[num_lookups - i];
        const auto& previous_2 = result.column_2_accumulator_values[num_lookups - i];
        const auto& previous_3 = result.column_3_accumulator_values[num_lookups - i];

        auto& current_1 = result.column_1_accumulator_values[num_lookups - 1 - i];
        auto& current_2 = result.column_2_accumulator_values[num_lookups - 1 - i];
        auto& current_3 = result.column_3_accumulator_values[num_lookups - 1 - i];

        const auto& raw_1 = column_1_raw_values[num_lookups - 1 - i];
        const auto& raw_2 = column_2_raw_values[num_lookups - 1 - i];
        const auto& raw_3 = column_3_raw_values[num_lookups - 1 - i];

        current_1 = raw_1 + previous_1 * result.column_1_step_sizes[num_lookups - i];
        current_2 = raw_2 + previous_2 * result.column_2_step_sizes[num_lookups - i];
        current_3 = raw_3 + previous_3 * result.column_3_step_sizes[num_lookups - i];
    }
    return result;
}

std::array<std::vector<uint32_t>, 3> PLookupComposer::read_sequence_from_multi_table(const PLookupMultiTableId& id,
                                                                                     const PLookupReadData& read_values,
                                                                                     const uint32_t key_index)

{
    const auto& multi_table = get_multi_table(id);
    const size_t num_lookups = read_values.column_1_accumulator_values.size();
    std::array<std::vector<uint32_t>, 3> column_indices;
    for (size_t i = 0; i < num_lookups; ++i) {
        auto& table = get_table(multi_table.lookup_ids[i]);

        table.lookup_gates.emplace_back(read_values.key_entries[i]);

        const auto first_idx = (i == 0) ? key_index : add_variable(read_values.column_1_accumulator_values[i]);
        const auto second_idx = add_variable(read_values.column_2_accumulator_values[i]);
        const auto third_idx = add_variable(read_values.column_3_accumulator_values[i]);

        column_indices[0].push_back(first_idx);
        column_indices[1].push_back(second_idx);
        column_indices[2].push_back(third_idx);

        q_lookup_type.emplace_back(fr(1));
        q_lookup_index.emplace_back(fr(table.table_index));
        w_l.emplace_back(first_idx);
        w_r.emplace_back(second_idx);
        w_o.emplace_back(third_idx);
        w_4.emplace_back(zero_idx);
        q_1.emplace_back(fr(0));
        q_2.emplace_back((i == (num_lookups - 1) ? fr(0) : -read_values.column_1_step_sizes[i + 1]));
        q_3.emplace_back(fr(0));
        q_m.emplace_back((i == (num_lookups - 1) ? fr(0) : -read_values.column_2_step_sizes[i + 1]));
        q_c.emplace_back((i == (num_lookups - 1) ? fr(0) : -read_values.column_3_step_sizes[i + 1]));
        q_arith.emplace_back(fr(0));
        q_4.emplace_back(fr(0));
        q_5.emplace_back(fr(0));
        q_ecc_1.emplace_back(fr(0));
        q_range.emplace_back(fr(0));
        q_logic.emplace_back(fr(0));

        epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
        epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
        epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };

        ASSERT(wire_epicycles.size() > first_idx);
        ASSERT(wire_epicycles.size() > second_idx);
        ASSERT(wire_epicycles.size() > third_idx);

        wire_epicycles[static_cast<size_t>(first_idx)].emplace_back(left);
        wire_epicycles[static_cast<size_t>(second_idx)].emplace_back(right);
        wire_epicycles[static_cast<size_t>(third_idx)].emplace_back(out);

        ++n;
    }
    return column_indices;
}

std::array<std::vector<uint32_t>, 3> PLookupComposer::read_sequence_from_multi_table(
    const PLookupMultiTable& multi_table, const uint32_t key_index)

{
    const size_t num_lookups = multi_table.lookup_ids.size();

    std::vector<barretenberg::fr> column_1_step_sizes{ 1 };
    std::vector<barretenberg::fr> column_2_step_sizes{ 1 };
    std::vector<barretenberg::fr> column_3_step_sizes{ 1 };

    std::vector<barretenberg::fr> coefficient_inverses(multi_table.column_1_coefficients.begin(),
                                                       multi_table.column_1_coefficients.end());
    std::copy(multi_table.column_2_coefficients.begin(),
              multi_table.column_2_coefficients.end(),
              std::back_inserter(coefficient_inverses));
    std::copy(multi_table.column_3_coefficients.begin(),
              multi_table.column_3_coefficients.end(),
              std::back_inserter(coefficient_inverses));

    fr::batch_invert(&coefficient_inverses[0], num_lookups * 3);

    for (size_t i = 1; i < num_lookups; ++i) {
        column_1_step_sizes.emplace_back(multi_table.column_1_coefficients[i] * coefficient_inverses[i - 1]);
        column_2_step_sizes.emplace_back(multi_table.column_2_coefficients[i] *
                                         coefficient_inverses[num_lookups + i - 1]);
        column_3_step_sizes.emplace_back(multi_table.column_3_coefficients[i] *
                                         coefficient_inverses[2 * num_lookups + i - 1]);
    }

    const auto value = get_variable(key_index);

    const auto keys = numeric::slice_input(value, multi_table.slice_sizes);

    std::vector<fr> column_1_raw_values;
    std::vector<fr> column_2_raw_values;
    std::vector<fr> column_3_raw_values;

    for (size_t i = 0; i < num_lookups; ++i) {
        PLookupTable& table = get_table(multi_table.lookup_ids[i]);

        const auto values = table.get_values_from_key({ keys[i], 0 });

        column_1_raw_values.emplace_back(keys[i]);
        column_2_raw_values.emplace_back(values[0]);
        column_3_raw_values.emplace_back(values[1]);

        table.lookup_gates.push_back({ { keys[i], 0 }, values });
    }

    std::vector<fr> column_1_values(num_lookups);
    std::vector<fr> column_2_values(num_lookups);
    std::vector<fr> column_3_values(num_lookups);

    column_1_values[num_lookups - 1] = column_1_raw_values[num_lookups - 1];
    column_2_values[num_lookups - 1] = column_2_raw_values[num_lookups - 1];
    column_3_values[num_lookups - 1] = column_3_raw_values[num_lookups - 1];

    for (size_t i = 1; i < num_lookups; ++i) {
        const auto& previous_1 = column_1_values[num_lookups - i];
        const auto& previous_2 = column_2_values[num_lookups - i];
        const auto& previous_3 = column_3_values[num_lookups - i];

        auto& current_1 = column_1_values[num_lookups - 1 - i];
        auto& current_2 = column_2_values[num_lookups - 1 - i];
        auto& current_3 = column_3_values[num_lookups - 1 - i];

        const auto& raw_1 = column_1_raw_values[num_lookups - 1 - i];
        const auto& raw_2 = column_2_raw_values[num_lookups - 1 - i];
        const auto& raw_3 = column_3_raw_values[num_lookups - 1 - i];

        current_1 = raw_1 + previous_1 * column_1_step_sizes[num_lookups - i];
        current_2 = raw_2 + previous_2 * column_2_step_sizes[num_lookups - i];
        current_3 = raw_3 + previous_3 * column_3_step_sizes[num_lookups - i];
    }

    std::array<std::vector<uint32_t>, 3> column_indices;

    for (size_t i = 0; i < num_lookups; ++i) {
        PLookupTable& table = get_table(multi_table.lookup_ids[i]);

        const auto first_idx = (i == 0) ? key_index : add_variable(column_1_values[i]);
        const auto second_idx = add_variable(column_2_values[i]);
        const auto third_idx = add_variable(column_3_values[i]);

        column_indices[0].push_back(first_idx);
        column_indices[1].push_back(second_idx);
        column_indices[2].push_back(third_idx);

        q_lookup_type.emplace_back(fr(1));
        q_lookup_index.emplace_back(fr(table.table_index));
        w_l.emplace_back(first_idx);
        w_r.emplace_back(second_idx);
        w_o.emplace_back(third_idx);
        w_4.emplace_back(zero_idx);
        q_1.emplace_back(fr(0));
        q_2.emplace_back((i == (num_lookups - 1) ? fr(0) : -column_1_step_sizes[i + 1]));
        q_3.emplace_back(fr(0));
        q_m.emplace_back((i == (num_lookups - 1) ? fr(0) : -column_2_step_sizes[i + 1]));
        q_c.emplace_back((i == (num_lookups - 1) ? fr(0) : -column_3_step_sizes[i + 1]));
        q_arith.emplace_back(fr(0));
        q_4.emplace_back(fr(0));
        q_5.emplace_back(fr(0));
        q_ecc_1.emplace_back(fr(0));
        q_range.emplace_back(fr(0));
        q_logic.emplace_back(fr(0));

        epicycle left{ static_cast<uint32_t>(n), WireType::LEFT };
        epicycle right{ static_cast<uint32_t>(n), WireType::RIGHT };
        epicycle out{ static_cast<uint32_t>(n), WireType::OUTPUT };

        ASSERT(wire_epicycles.size() > first_idx);
        ASSERT(wire_epicycles.size() > second_idx);
        ASSERT(wire_epicycles.size() > third_idx);

        wire_epicycles[static_cast<size_t>(first_idx)].emplace_back(left);
        wire_epicycles[static_cast<size_t>(second_idx)].emplace_back(right);
        wire_epicycles[static_cast<size_t>(third_idx)].emplace_back(out);

        ++n;
    }

    return column_indices;
}

} // namespace waffle