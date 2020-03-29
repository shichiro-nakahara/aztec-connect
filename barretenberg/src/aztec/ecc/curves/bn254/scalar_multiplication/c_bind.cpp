#include "./scalar_multiplication.hpp"
#include <srs/io.hpp>
#include <common/log.hpp>

using namespace barretenberg;

#define WASM_EXPORT __attribute__((visibility("default")))

extern "C" {

WASM_EXPORT void* bbmalloc(size_t size) {
    return aligned_alloc(64, size);
}

WASM_EXPORT void bbfree(void* ptr) {
    aligned_free(ptr);
}

WASM_EXPORT void pippenger_unsafe(fr* scalars, size_t from, size_t range, uint8_t* points, size_t num_points, g1::element* result)
{
    static size_t monomials_size = 0;
    static g1::affine_element* monomials = 0;

    if (monomials_size < num_points) {
        aligned_free(monomials);
        monomials = (barretenberg::g1::affine_element*)(aligned_alloc(
            64, sizeof(barretenberg::g1::affine_element) * (2 * num_points + 2)));

        monomials[0] = barretenberg::g1::affine_one;

        barretenberg::io::read_g1_elements_from_buffer(&monomials[1], (char*)points, num_points * 64);
        barretenberg::scalar_multiplication::generate_pippenger_point_table(monomials, monomials, num_points);
    }

    scalar_multiplication::unsafe_pippenger_runtime_state state(range);
    *result = scalar_multiplication::pippenger_unsafe(scalars, monomials + from * 2, range, state);
}

WASM_EXPORT void g1_sum(g1::element* points, const size_t num_points, g1::element* result) {
    result->self_set_infinity();
    *result = std::accumulate(points, points + num_points, *result);
}

}