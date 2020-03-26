#include "mem_reference_string.hpp"
#include <ecc/curves/bn254/pairing.hpp>
#include <ecc/curves/bn254/scalar_multiplication/scalar_multiplication.hpp>
#include <srs/io.hpp>
#include <sstream>
#include <common/streams.hpp>

namespace waffle {

VerifierMemReferenceString::VerifierMemReferenceString(char const* buffer)
{
    barretenberg::io::read_g2_elements_from_buffer(&g2_x, buffer, 128);

    precomputed_g2_lines =
        (barretenberg::pairing::miller_lines*)(aligned_alloc(64, sizeof(barretenberg::pairing::miller_lines) * 2));

    barretenberg::pairing::precompute_miller_lines(barretenberg::g2::one, precomputed_g2_lines[0]);
    barretenberg::pairing::precompute_miller_lines(g2_x, precomputed_g2_lines[1]);
}

VerifierMemReferenceString::~VerifierMemReferenceString()
{
    aligned_free(precomputed_g2_lines);
}

MemReferenceString::MemReferenceString(const size_t num_points, char const* buffer, size_t )
{
    monomials = (barretenberg::g1::affine_element*)(aligned_alloc(
        64, sizeof(barretenberg::g1::affine_element) * (2 * num_points + 2)));

    monomials[0] = barretenberg::g1::affine_one;

    barretenberg::io::read_g1_elements_from_buffer(&monomials[1], buffer, num_points*64);
    barretenberg::scalar_multiplication::generate_pippenger_point_table(monomials, monomials, num_points);
}

MemReferenceString::~MemReferenceString()
{
    aligned_free(monomials);
}

} // namespace waffle