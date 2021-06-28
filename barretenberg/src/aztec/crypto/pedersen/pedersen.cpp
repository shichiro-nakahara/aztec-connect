#include "./pedersen.hpp"
#include <common/throw_or_abort.hpp>
#include <iostream>
#ifndef NO_MULTITHREADING
#include <omp.h>
#endif

namespace crypto {
namespace pedersen {

grumpkin::g1::element hash_single(const barretenberg::fr& in, generator_index_t const& index)
{
    auto gen_data = get_generator_data(index);
    barretenberg::fr scalar_multiplier = in.from_montgomery_form();

    constexpr size_t num_bits = 254;
    constexpr size_t num_quads_base = (num_bits - 1) >> 1;
    constexpr size_t num_quads = ((num_quads_base << 1) + 1 < num_bits) ? num_quads_base + 1 : num_quads_base;
    constexpr size_t num_wnaf_bits = (num_quads << 1) + 1;

    const crypto::pedersen::fixed_base_ladder* ladder = gen_data.get_hash_ladder(num_bits);

    barretenberg::fr scalar_multiplier_base = scalar_multiplier.to_montgomery_form();
    if ((scalar_multiplier.data[0] & 1) == 0) {
        barretenberg::fr two = barretenberg::fr::one() + barretenberg::fr::one();
        scalar_multiplier_base = scalar_multiplier_base - two;
    }
    scalar_multiplier_base = scalar_multiplier_base.from_montgomery_form();
    uint64_t wnaf_entries[num_quads + 2] = { 0 };
    bool skew = false;
    barretenberg::wnaf::fixed_wnaf<num_wnaf_bits, 1, 2>(&scalar_multiplier_base.data[0], &wnaf_entries[0], skew, 0);

    grumpkin::g1::element accumulator;
    accumulator = grumpkin::g1::element(ladder[0].one);
    if (skew) {
        accumulator += gen_data.aux_generator;
    }

    for (size_t i = 0; i < num_quads; ++i) {
        uint64_t entry = wnaf_entries[i + 1];
        ;
        const grumpkin::g1::affine_element& point_to_add =
            ((entry & 0xffffff) == 1) ? ladder[i + 1].three : ladder[i + 1].one;
        uint64_t predicate = (entry >> 31U) & 1U;
        accumulator.self_mixed_add_or_sub(point_to_add, predicate);
    }
    if (in == barretenberg::fr(0)) {
        accumulator.self_set_infinity();
    }
    return accumulator;
}

/**
 * Given a vector of fields, generate a pedersen commitment using the indexed generators.
 */
grumpkin::g1::affine_element commit_native(const std::vector<grumpkin::fq>& inputs, const size_t hash_index)
{
    ASSERT((inputs.size() < (1 << 16)) && "too many inputs for 16 bit index");
    std::vector<grumpkin::g1::element> out(inputs.size());

#ifndef NO_MULTITHREADING
    // Ensure generator data is initialized before threading...
    init_generator_data();
#pragma omp parallel for num_threads(inputs.size())
#endif
    for (size_t i = 0; i < inputs.size(); ++i) {
        generator_index_t index = { hash_index, i };
        out[i] = hash_single(inputs[i], index);
    }

    grumpkin::g1::element r = out[0];
    for (size_t i = 1; i < inputs.size(); ++i) {
        r = out[i] + r;
    }
    return r.is_point_at_infinity() ? grumpkin::g1::affine_element(0, 0) : grumpkin::g1::affine_element(r);
}

/**
 * The same as commit_native, but only return the resultant x coordinate (i.e. compress).
 */
grumpkin::fq compress_native(const std::vector<grumpkin::fq>& inputs, const size_t hash_index)
{
    return commit_native(inputs, hash_index).x;
}

/**
 * Given an arbitrary length of bytes, convert them to fields and compress the result using the default generators.
 */
grumpkin::fq compress_native_buffer_to_field(const std::vector<uint8_t>& input)
{
    const size_t num_bytes = input.size();
    const size_t bytes_per_element = 31;
    size_t num_elements = (num_bytes % bytes_per_element != 0) + (num_bytes / bytes_per_element);

    const auto slice = [](const std::vector<uint8_t>& data, const size_t start, const size_t slice_size) {
        uint256_t result(0);
        for (size_t i = 0; i < slice_size; ++i) {
            result = (result << uint256_t(8));
            result += uint256_t(data[i + start]);
        }
        return grumpkin::fq(result);
    };

    std::vector<grumpkin::fq> elements;
    for (size_t i = 0; i < num_elements; ++i) {
        size_t bytes_to_slice = 0;
        if (i == num_elements - 1) {
            bytes_to_slice = num_bytes - (i * bytes_per_element);
        } else {
            bytes_to_slice = bytes_per_element;
        }
        grumpkin::fq element = slice(input, i * bytes_per_element, bytes_to_slice);
        elements.emplace_back(element);
    }

    grumpkin::fq result_fq = compress_native(elements);
    return result_fq;
}

std::vector<uint8_t> compress_native(const std::vector<uint8_t>& input)
{
    const auto result_fq = compress_native_buffer_to_field(input);
    uint256_t result_u256(result_fq);
    const size_t num_bytes = input.size();

    bool is_zero = std::all_of(input.begin(), input.end(), [](auto e) { return e == 0; });
    if (is_zero) {
        result_u256 = num_bytes;
    }

    std::vector<uint8_t> result_buffer(32);
    for (size_t i = 0; i < 32; ++i) {
        const uint64_t shift = (31 - i) * 8;
        uint256_t shifted = result_u256 >> uint256_t(shift);
        result_buffer[i] = static_cast<uint8_t>(shifted.data[0]);
    }
    return result_buffer;
}

grumpkin::g1::affine_element compress_to_point_native(const grumpkin::fq& left,
                                                      const grumpkin::fq& right,
                                                      const size_t hash_index)
{
    generator_index_t index_1 = { hash_index, 0 };
    generator_index_t index_2 = { hash_index, 1 };
    grumpkin::g1::element first = hash_single(left, index_1);
    grumpkin::g1::element second = hash_single(right, index_2);
    first = first + second;
    first = first.normalize();
    return { first.x, first.y };
}

} // namespace pedersen
} // namespace crypto