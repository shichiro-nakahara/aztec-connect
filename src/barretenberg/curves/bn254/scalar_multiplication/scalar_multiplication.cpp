#include "./scalar_multiplication.hpp"

#include "../../../groups/wnaf.hpp"
#include "../fq.hpp"
#include "../fr.hpp"
#include "../g1.hpp"
#include "./mmu.hpp"
#include "./process_buckets.hpp"
#include "./scalar_multiplication.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#ifndef NO_MULTITHREADING
#include <omp.h>
#endif

namespace barretenberg {
namespace scalar_multiplication {

void generate_pippenger_point_table(g1::affine_element* points, g1::affine_element* table, size_t num_points)
{
    // iterate backwards, so that `points` and `table` can point to the same memory location
    for (size_t i = num_points - 1; i < num_points; --i) {
        table[i * 2] = points[i];
        table[i * 2 + 1].x = fq::field_t::beta * points[i].x;
        table[i * 2 + 1].y = -points[i].y;
    }
}

/**
 * Compute the windowed-non-adjacent-form versions of our scalar multipliers.
 *
 * We start by splitting our 254 bit scalars into 2 127-bit scalars, using the short weierstrass curve endomorphism
 * (for a point P \in \G === (x, y) \in \Fq, then (\beta x, y) = (\lambda) * P , where \beta = 1^{1/3} mod Fq and
 *\lambda = 1^{1/3} mod Fr) (which means we can represent a scalar multiplication (k * P) as (k1 * P + k2 * \lambda *
 *P), where k1, k2 have 127 bits) (see field::split_into_endomorphism_scalars for more details)
 *
 * Once we have our 127-bit scalar multipliers, we determine the optimal number of pippenger rounds, given the number of
 *points we're multiplying. Once we have the number of rounds, `m`, we need to split our scalar into `m` bit-slices.
 *Each pippenger round will work on one bit-slice.
 *
 * Pippenger's algorithm works by, for each round, iterating over the points we're multplying. For each point, we
 *examing the point's scalar multiplier and extract the bit-slice associated with the current pippenger round (we start
 *with the most significant slice). We then use the bit-slice to index a 'bucket', which we add the point into. For
 *example, if the bit slice is 01101, we add the corresponding point into bucket[13].
 *
 * At the end of each pippenger round we concatenate the buckets together. E.g. if we have 8 buckets, we compute:
 * sum = bucket[0] + 2 * bucket[1] + 3 * bucket[2] + 4 * bucket[3] + 5 * bucket[4] + 6 * bucket[5] + 7 * bucket[6] + 8 *
 *bucket[7].
 *
 * At the end of each pippenger round, the bucket sum will contain the scalar multiplication result for one bit slice.
 * For example, say we have 16 rounds, where each bit slice contains 8 bits (8 * 16 = 128, enough to represent our 127
 *bit scalars). At the end of the first round, we will have taken the 8 most significant bits from every scalar
 *multiplier. Our bucket sum will be the result of a mini-scalar-multiplication, where we have multiplied every point by
 *the 8 most significant bits of each point's scalar multiplier.
 *
 * We repeat this process for every pippenger round. In our example, this gives us 16 bucket sums.
 * We need to multiply the most significant bucket sum by 2^{120}, the second most significant bucket sum by 2^{112}
 *etc. Once this is done we can add the bucket sums together, to evaluate our scalar multiplication result.
 *
 * Pippenger has complexity O(n / logn), because of two factors at play: the number of buckets we need to concatenate
 *per round, and the number of points we need to add into buckets per round.
 *
 * To minimize the number of point additions per round, we want fewer rounds. But fewer rounds increases the number of
 *bucket concatenations. The more points we have, the greater the time saving when reducing the number of rounds, which
 *means we can afford to have more buckets per round.
 *
 * For a concrete example, with 2^20 points, the sweet spot is 2^15 buckets - with 2^15 buckets we can evaluate our 127
 *bit scalar multipliers in 8 rounds (we can represent b-bit windows with 2^{b-1} buckets, more on that below).
 *
 * This means that, for each round, we add 2^21 points into buckets (we've split our scalar multpliers into two
 *half-width multipliers, so each round has twice the number of points. This is the reason why the endormorphism is
 *useful here; without the endomorphism, we would need twice the number of buckets for each round).
 *
 * We also concatenate 2^15 buckets for each round. This requires 2^16 point additions.
 *
 * Meaning that the total number of point additions is (8 * 2^21) + (8 * 2^16) = 33 * 2^19 ~ 2^24 point additions.
 * If we were to use a simple Montgomery double-and-add ladder to exponentiate each point, we would need 2^27 point
 *additions (each scalar multiplier has ~2^7 non-zero bits, and there are 2^20 points).
 *
 * This makes pippenger 8 times faster than the naive O(n) equivalent. Given that a circuit with 1 million gates will
 *require 9 multiple-scalar-multiplications with 2^20 points, efficiently using Pippenger's algorithm is essential for
 *fast provers
 *
 * One additional efficiency gain is the use of 2^{b-1} buckets to represent b bits. To do this we represent our
 *bit-slices in non-adjacent form. Non-adjacent form represents values using a base, where each 'bit' can take the
 *values (-1, 0, 1). This is considerably more efficient than binary form for scalar multiplication, as inverting a
 *point can be done by negating the y-coordinate.
 *
 * We actually use a slightly different representation than simple non-adjacent form. To represent b bits, a bit slice
 *contains values from (-2^{b} - 1, ..., -1, 1, ..., 2^{b} - 1). i.e. we only have odd values. We do this to eliminate
 *0-valued windows, as having a conditional branch in our hot loop to check if an entry is 0 is somethin we want to
 *avoid.
 *
 * The above representation can be used to represent any binary number as long as we add a 'skew' factor. Each scalar
 *multiplier's `skew` tracks if the scalar multiplier is even or odd. If it's even, `skew = true`, and we add `1` to our
 *multiplier to make it odd.
 *
 * We then, at the end of the Pippenger algorithm, subtract a point from the total result, if that point's skew is
 *`true`.
 *
 * At the end of `compute_wnaf_states`, `state.wnaf_table` will contain our wnaf entries, but unsorted.
 **/
template <size_t num_initial_points>
inline void compute_wnaf_states(multiplication_runtime_state& state, fr::field_t* scalars)
{
    const size_t num_points = num_initial_points * 2;
    constexpr size_t SCALAR_MULTIPLICATION_MAX_ROUNDS = 256;
    const size_t num_rounds = get_num_rounds(num_points);
    constexpr size_t bits_per_bucket = get_optimal_bucket_width(num_initial_points);
    const size_t log2_num_points = static_cast<uint64_t>(internal::get_msb(static_cast<uint32_t>(num_points)));

    // fetch our wnaf table and skew table pointers from pre-allocated memory. This eliminates soft page faults when
    // writing to newly allocated memory. The page faults were adding up to 200 milliseconds onto the runtime of our
    // algorithm!
    state.wnaf_table = mmu::get_wnaf_pointer();
    state.skew_table = mmu::get_skew_pointer();

#ifndef NO_MULTITHREADING
    const size_t num_threads = static_cast<size_t>(omp_get_max_threads());
#else
    const size_t num_threads = 1;
#endif
    const size_t num_initial_points_per_thread = num_initial_points / num_threads;
    const size_t num_points_per_thread = num_points / num_threads;

#ifndef NO_MULTITHREADING
#pragma omp parallel for
#endif
    for (size_t i = 0; i < num_threads; ++i) {
        fr::field_t T0;
        uint64_t* wnaf_table = &state.wnaf_table[(2 * i) * num_initial_points_per_thread];
        fr::field_t* thread_scalars = &scalars[i * num_initial_points_per_thread];
        bool* skew_table = &state.skew_table[(2 * i) * num_initial_points_per_thread];
        uint64_t offset = i * num_points_per_thread;
        std::array<uint64_t, SCALAR_MULTIPLICATION_MAX_ROUNDS * 8> wnaf_entries{};

        // have our main loop work on 8 wnaf entries at a time. This ensures that, for
        // each iteration of the loop, each round's wnaf entries fit into a cache line.
        // This seems to improve performance, because we write our wnaf entries into memory
        // in a slightly unsequenced order (a given scalar will have `num_rounds` wnaf entries,
        // but these entries are not densely packed in memory - they are stored `num_points` apart from each other).
        // If we collect 8 wnaf entries together, we ensure that multiple iterations will not require the same cache
        // lines (wnaf entry = 8 bytes => 8 entries = 64 bytes)
        for (uint64_t j = 0; j < num_initial_points_per_thread; ++j) {
            T0 = thread_scalars[j].from_montgomery_form();
            fr::field_t::split_into_endomorphism_scalars(T0, T0, *(fr::field_t*)&T0.data[2]);

            wnaf::fixed_wnaf_packed<bits_per_bucket + 1>(
                &T0.data[0], &wnaf_entries[0], skew_table[j << 1ULL], ((j << 1ULL) + offset) << 32ULL);
            wnaf::fixed_wnaf_packed<bits_per_bucket + 1>(
                &T0.data[2], &wnaf_entries[num_rounds], skew_table[(j << 1UL) + 1], ((j << 1UL) + offset + 1) << 32UL);

            for (size_t k = 0; k < num_rounds; ++k) {
                wnaf_table[(k << log2_num_points) + (j << 1UL)] = wnaf_entries[k];
                wnaf_table[(k << log2_num_points) + (j << 1UL) + 1] = wnaf_entries[k + num_rounds];
            }
        }
    }
}

/**
 *  Sorts our wnaf entries in increasing bucket order (per round).
 *  We currently don't multi-thread the inner sorting algorithm, and just split our threads over the number of rounds.
 *  A multi-threaded sorting algorithm could be more efficient, but the total runtime of `organize_buckets` is <5% of
 *  pippenger's runtime, so not a priority.
 **/
template <size_t num_points> void organize_buckets(multiplication_runtime_state& state)
{
    const size_t num_rounds = get_num_rounds(num_points);
#ifndef NO_MULTITHREADING
#pragma omp parallel for
#endif
    for (size_t i = 0; i < num_rounds; ++i) {
        scalar_multiplication::process_buckets<num_points, get_optimal_bucket_width(num_points / 2)>(
            &state.wnaf_table[i * num_points]);
    }
}

inline void scalar_multiplication_round_inner(multiplication_thread_state& state,
                                              const size_t num_points,
                                              const uint64_t bucket_offset,
                                              g1::affine_element* points)
{
    g1::affine_element* current_point;
    g1::element* current_bucket;
    g1::affine_element* next_point = points + ((state.point_schedule[0]) >> 32UL);
    g1::element* next_bucket = state.buckets + (state.point_schedule[0] & 0x7fffffffUL) - bucket_offset;
    uint64_t current_negative;
    uint64_t next_negative = ((state.point_schedule[0] >> 31UL) & 1UL);

    for (size_t i = 1; i < num_points; ++i) {
        current_point = next_point;
        current_bucket = next_bucket;
        current_negative = next_negative;

        next_point = points + ((state.point_schedule[i]) >> 32UL);
        next_bucket = state.buckets + (state.point_schedule[i] & 0x7fffffffUL) - bucket_offset;
        next_negative = ((state.point_schedule[i] >> 31UL) & 1UL);

        __builtin_prefetch(next_point);

        (*current_bucket).self_mixed_add_or_sub(*current_point, current_negative);
    }

    (*next_bucket).self_mixed_add_or_sub(*next_point, next_negative);
}

inline g1::element scalar_multiplication_internal(multiplication_runtime_state& state,
                                                  g1::affine_element* points,
                                                  const size_t num_points)
{
    const size_t num_rounds = get_num_rounds(num_points);
#ifndef NO_MULTITHREADING
    const size_t num_threads = static_cast<size_t>(omp_get_max_threads());
#else
    const size_t num_threads = 1;
#endif
    const size_t bits_per_bucket = get_optimal_bucket_width(num_points / 2);
    const size_t num_points_per_thread = num_points / num_threads; // assume a power of 2

    g1::element* thread_accumulators = static_cast<g1::element*>(aligned_alloc(64, num_threads * sizeof(g1::element)));

    std::vector<uint64_t> bucket_offsets(num_threads);
    for (size_t j = 0; j < num_threads; ++j) {
        uint64_t max_buckets = 0;
        for (size_t i = 0; i < num_rounds; ++i) {
            const uint64_t* thread_point_schedule = &state.wnaf_table[(i * num_points) + j * num_points_per_thread];
            const uint64_t first_bucket = thread_point_schedule[0] & 0x7fffffffU;
            const uint64_t last_bucket = thread_point_schedule[(num_points_per_thread - 1)] & 0x7fffffffU;
            const uint64_t num_thread_buckets = (last_bucket - first_bucket) + 1;
            if (num_thread_buckets > max_buckets) {
                max_buckets = num_thread_buckets;
            }
        }
        bucket_offsets[j] = max_buckets;
    }
    for (size_t j = 1; j < num_threads; ++j) {
        bucket_offsets[j] += bucket_offsets[j - 1];
    }
    g1::element* buckets = mmu::get_bucket_pointer();
#ifndef NO_MULTITHREADING
#pragma omp parallel for
#endif
    for (size_t j = 0; j < num_threads; ++j) {
        thread_accumulators[j].self_set_infinity();

        g1::element* thread_buckets = buckets + (j == 0 ? 0 : bucket_offsets[j - 1]);
        for (size_t i = 0; i < num_rounds; ++i) {
            const uint64_t* thread_point_schedule = &state.wnaf_table[(i * num_points) + j * num_points_per_thread];
            const size_t first_bucket = thread_point_schedule[0] & 0x7fffffffU;
            const size_t last_bucket = thread_point_schedule[(num_points_per_thread - 1)] & 0x7fffffffU;
            const size_t num_thread_buckets = (last_bucket - first_bucket) + 1;

            for (size_t k = 0; k < num_thread_buckets; ++k) {
                thread_buckets[k].self_set_infinity();
            }
            multiplication_thread_state thread_state{ thread_buckets, thread_point_schedule };

            scalar_multiplication_round_inner(thread_state, num_points_per_thread, first_bucket, points);

            g1::element running_sum;
            g1::element accumulator;
            running_sum.self_set_infinity();
            accumulator.self_set_infinity();
            for (size_t k = num_thread_buckets - 1; k > 0; --k) {
                running_sum += thread_buckets[k];
                accumulator += running_sum;
            }
            running_sum += thread_buckets[0];
            accumulator.self_dbl();
            accumulator += running_sum;

            // we now need to scale up 'running sum' up to the value of the first bucket.
            // e.g. if first bucket is 0, no scaling
            // if first bucket is 1, we need to add (2 * running_sum)
            if (first_bucket > 0) {
                uint32_t multiplier = static_cast<uint32_t>(first_bucket << 1UL);
                size_t shift = internal::get_msb(multiplier);
                g1::element rolling_accumulator = g1::element::point_at_infinity;
                bool init = false;
                while (shift != static_cast<size_t>(-1)) {
                    if (init) {
                        rolling_accumulator.self_dbl();
                        if (((multiplier >> shift) & 1)) {
                            rolling_accumulator += running_sum;
                        }
                    } else {
                        rolling_accumulator += running_sum;
                    }
                    init = true;
                    shift -= 1;
                }
                accumulator += rolling_accumulator;
            }

            if (i == (num_rounds - 1)) {
                bool* skew_table = &state.skew_table[j * num_points_per_thread];
                g1::affine_element* point_table = &points[j * num_points_per_thread];
                g1::affine_element addition_temporary;
                for (size_t k = 0; k < num_points_per_thread; ++k) {
                    if (skew_table[k]) {
                        addition_temporary = -point_table[k];
                        accumulator += addition_temporary;
                    }
                }
            }

            if (i > 0) {
                for (size_t k = 0; k < bits_per_bucket + 1; ++k) {
                    thread_accumulators[j].self_dbl();
                }
            }
            thread_accumulators[j] += accumulator;
        }
    }

    g1::element result;
    result.self_set_infinity();
    for (size_t i = 0; i < num_threads; ++i) {
        result += thread_accumulators[i];
    }
    free(thread_accumulators);
    return result;
}

template <size_t num_initial_points>
inline g1::element pippenger_internal(g1::affine_element* points, fr::field_t* scalars)
{
    multiplication_runtime_state state;
    compute_wnaf_states<num_initial_points>(state, scalars);
    organize_buckets<num_initial_points * 2>(state);
    g1::element result = scalar_multiplication_internal(state, points, num_initial_points * 2);
    return result;
}

// TODO: this is a lot of code duplication, need to fix that once the method has stabilized
inline g1::element unsafe_scalar_multiplication_internal(multiplication_runtime_state& state,
                                                         g1::affine_element* points,
                                                         const size_t num_points)
{
    const size_t num_rounds = get_num_rounds(num_points);
#ifndef NO_MULTITHREADING
    const size_t num_threads = static_cast<size_t>(omp_get_max_threads());
#else
    const size_t num_threads = 1;
#endif
    const size_t bits_per_bucket = get_optimal_bucket_width(num_points / 2);
    const size_t num_points_per_thread = num_points / num_threads; // assume a power of 2

    g1::element* thread_accumulators = static_cast<g1::element*>(aligned_alloc(64, num_threads * sizeof(g1::element)));

#ifndef NO_MULTITHREADING
#pragma omp parallel for
#endif
    for (size_t j = 0; j < num_threads; ++j) {
        thread_accumulators[j].self_set_infinity();

        for (size_t i = 0; i < num_rounds; ++i) {
            uint64_t* thread_point_schedule = &state.wnaf_table[(i * num_points) + j * num_points_per_thread];
            const size_t first_bucket = thread_point_schedule[0] & 0x7fffffffU;
            const size_t last_bucket = thread_point_schedule[(num_points_per_thread - 1)] & 0x7fffffffU;
            const size_t num_thread_buckets = (last_bucket - first_bucket) + 1;

            affine_product_runtime_state product_state = mmu::get_affine_product_runtime_state(num_threads, j);
            product_state.num_points = static_cast<uint32_t>(num_points_per_thread);
            product_state.points = points;
            product_state.point_schedule = thread_point_schedule;
            product_state.num_buckets = static_cast<uint32_t>(num_thread_buckets);

            g1::affine_element* output_buckets = reduce_buckets(product_state, true);

            g1::element running_sum;
            g1::element accumulator;
            running_sum.self_set_infinity();
            accumulator.self_set_infinity();

            // one nice side-effect of the affine trick, is that half of the bucket concatenation
            // algorithm can use mixed addition formulae, instead of full addition formulae
            size_t output_it = product_state.num_points - 1;
            for (size_t k = num_thread_buckets - 1; k > 0; --k) {
                if (__builtin_expect(!product_state.bucket_empty_status[k], 1)) {
                    running_sum += (output_buckets[output_it]);
                    --output_it;
                }
                accumulator += running_sum;
            }
            running_sum += output_buckets[0];
            accumulator.self_dbl();
            accumulator += running_sum;

            // we now need to scale up 'running sum' up to the value of the first bucket.
            // e.g. if first bucket is 0, no scaling
            // if first bucket is 1, we need to add (2 * running_sum)
            if (first_bucket > 0) {
                uint32_t multiplier = static_cast<uint32_t>(first_bucket << 1UL);
                size_t shift = internal::get_msb(multiplier);
                g1::element rolling_accumulator = g1::element::point_at_infinity;
                bool init = false;
                while (shift != static_cast<size_t>(-1)) {
                    if (init) {
                        rolling_accumulator.self_dbl();
                        if (((multiplier >> shift) & 1)) {
                            rolling_accumulator += running_sum;
                        }
                    } else {
                        rolling_accumulator += running_sum;
                    }
                    init = true;
                    shift -= 1;
                }
                accumulator += rolling_accumulator;
            }

            if (i == (num_rounds - 1)) {
                bool* skew_table = &state.skew_table[j * num_points_per_thread];
                g1::affine_element* point_table = &points[j * num_points_per_thread];
                g1::affine_element addition_temporary;
                for (size_t k = 0; k < num_points_per_thread; ++k) {
                    if (skew_table[k]) {
                        addition_temporary = -point_table[k];
                        accumulator += addition_temporary;
                    }
                }
            }

            if (i > 0) {
                for (size_t k = 0; k < bits_per_bucket + 1; ++k) {
                    thread_accumulators[j].self_dbl();
                }
            }
            thread_accumulators[j] += accumulator;
        }
    }

    g1::element result;
    result.self_set_infinity();
    for (size_t i = 0; i < num_threads; ++i) {
        result += thread_accumulators[i];
    }
    free(thread_accumulators);
    return result;
}

template <size_t num_initial_points>
inline g1::element pippenger_unsafe_internal(g1::affine_element* points, fr::field_t* scalars)
{
    multiplication_runtime_state state;
    compute_wnaf_states<num_initial_points>(state, scalars);
    organize_buckets<num_initial_points * 2>(state);
    g1::element result = unsafe_scalar_multiplication_internal(state, points, num_initial_points * 2);
    return result;
}

inline g1::element pippenger(fr::field_t* scalars, g1::affine_element* points, const size_t num_initial_points)
{
    // our windowed non-adjacent form algorthm requires that each thread can work on at least 8 points.
    // If we fall below this theshold, fall back to the traditional scalar multiplication algorithm.
    // For 8 threads, this neatly coincides with the threshold where Strauss scalar multiplication outperforms Pippenger
#ifndef NO_MULTITHREADING
    const size_t threshold = std::max(static_cast<size_t>(omp_get_max_threads() * 8), 8UL);
#else
    const size_t threshold = 8UL;
#endif

    if (num_initial_points == 0) {
        g1::element out = g1::one;
        out.self_set_infinity();
        return out;
    }

    if (num_initial_points <= threshold) {
        std::vector<g1::element> exponentiation_results(num_initial_points);
        // might as well multithread this...
        // TODO: implement Strauss algorithm for small numbers of points.
#ifndef NO_MULTITHREADING
#pragma omp parallel for
#endif
        for (size_t i = 0; i < num_initial_points; ++i) {
            exponentiation_results[i] = g1::element(points[i * 2]) * scalars[i];
        }

        for (size_t i = num_initial_points - 1; i > 0; --i) {
            exponentiation_results[i - 1] += exponentiation_results[i];
        }
        return exponentiation_results[0];
    }

    const size_t log2_initial_points =
        std::min(static_cast<size_t>(internal::get_msb(static_cast<uint32_t>(num_initial_points))), 20UL);
    g1::element result;

    switch (log2_initial_points) {
    case 20:
        result = pippenger_internal<1 << 20>(points, scalars);
        break;
    case 19:
        result = pippenger_internal<1 << 19>(points, scalars);
        break;
    case 18:
        result = pippenger_internal<1 << 18>(points, scalars);
        break;
    case 17:
        result = pippenger_internal<1 << 17>(points, scalars);
        break;
    case 16:
        result = pippenger_internal<1 << 16>(points, scalars);
        break;
    case 15:
        result = pippenger_internal<1 << 15>(points, scalars);
        break;
    case 14:
        result = pippenger_internal<1 << 14>(points, scalars);
        break;
    case 13:
        result = pippenger_internal<1 << 13>(points, scalars);
        break;
    case 12:
        result = pippenger_internal<1 << 12>(points, scalars);
        break;
    case 11:
        result = pippenger_internal<1 << 11>(points, scalars);
        break;
    case 10:
        result = pippenger_internal<1 << 10>(points, scalars);
        break;
    case 9:
        result = pippenger_internal<1 << 9>(points, scalars);
        break;
    case 8:
        result = pippenger_internal<1 << 8>(points, scalars);
        break;
    case 7:
        result = pippenger_internal<1 << 7>(points, scalars);
        break;
    case 6:
        result = pippenger_internal<1 << 6>(points, scalars);
        break;
    case 5:
        result = pippenger_internal<1 << 5>(points, scalars);
        break;
    case 4:
        result = pippenger_internal<1 << 4>(points, scalars);
        break;
    case 3:
        result = pippenger_internal<1 << 3>(points, scalars);
        break;
    }

    if ((1UL << log2_initial_points) == num_initial_points) {
        return result;
    } else {
        result += pippenger(scalars + (1UL << log2_initial_points),
                            points + (1UL << (log2_initial_points + 1)),
                            num_initial_points - (1UL << log2_initial_points));
        return result;
    }
}

/**
 * It's pippenger! But this one has go-faster stripes and a prediliction for questionable life choices.
 * We use affine-addition formula in this method, which paradoxically is ~45% faster than the mixed addition formulae.
 * See `scalar_multiplication.cpp` for a more detailed description.
 *
 * It's...unsafe, because we assume that the incomplete addition formula exceptions are not triggered.
 * We don't bother to check for this to avoid conditional branches in a critical section of our code.
 * This is fine for situations where your bases are linearly independent (i.e. KZG10 polynomial commitments),
 * because triggering the incomplete addition exceptions is about as hard as solving the disrete log problem.
 *
 * This is ok for the prover, but GIANT RED CLAXON WARNINGS FOR THE VERIFIER
 * Don't use this in a verification algorithm! That would be a really bad idea.
 * Unless you're a malicious adversary, then it would be a great idea!
 *
 **/
g1::element pippenger_unsafe(fr::field_t* scalars, g1::affine_element* points, const size_t num_initial_points)
{
    // our windowed non-adjacent form algorthm requires that each thread can work on at least 8 points.
    // If we fall below this theshold, fall back to the traditional scalar multiplication algorithm.
    // For 8 threads, this neatly coincides with the threshold where Strauss scalar multiplication outperforms Pippenger
#ifndef NO_MULTITHREADING
    const size_t threshold = std::max(static_cast<size_t>(omp_get_max_threads() * 8), 8UL);
#else
    const size_t threshold = 8UL;
#endif

    if (num_initial_points == 0) {
        g1::element out = g1::one;
        out.self_set_infinity();
        return out;
    }

    if (num_initial_points <= threshold) {
        std::vector<g1::element> exponentiation_results(num_initial_points);
        // might as well multithread this...
        // TODO: implement Strauss algorithm for small numbers of points.
#ifndef NO_MULTITHREADING
#pragma omp parallel for
#endif
        for (size_t i = 0; i < num_initial_points; ++i) {
            exponentiation_results[i] = g1::element(points[i * 2]) * scalars[i];
        }

        for (size_t i = num_initial_points - 1; i > 0; --i) {
            exponentiation_results[i - 1] += exponentiation_results[i];
        }
        return exponentiation_results[0];
    }

    const size_t log2_initial_points =
        std::min(static_cast<size_t>(internal::get_msb(static_cast<uint32_t>(num_initial_points))), 20UL);
    g1::element result;

    switch (log2_initial_points) {
    case 20:
        result = pippenger_unsafe_internal<1 << 20>(points, scalars);
        break;
    case 19:
        result = pippenger_unsafe_internal<1 << 19>(points, scalars);
        break;
    case 18:
        result = pippenger_unsafe_internal<1 << 18>(points, scalars);
        break;
    case 17:
        result = pippenger_unsafe_internal<1 << 17>(points, scalars);
        break;
    case 16:
        result = pippenger_unsafe_internal<1 << 16>(points, scalars);
        break;
    case 15:
        result = pippenger_unsafe_internal<1 << 15>(points, scalars);
        break;
    case 14:
        result = pippenger_unsafe_internal<1 << 14>(points, scalars);
        break;
    case 13:
        result = pippenger_unsafe_internal<1 << 13>(points, scalars);
        break;
    case 12:
        result = pippenger_unsafe_internal<1 << 12>(points, scalars);
        break;
    case 11:
        result = pippenger_unsafe_internal<1 << 11>(points, scalars);
        break;
    case 10:
        result = pippenger_unsafe_internal<1 << 10>(points, scalars);
        break;
    case 9:
        result = pippenger_unsafe_internal<1 << 9>(points, scalars);
        break;
    case 8:
        result = pippenger_unsafe_internal<1 << 8>(points, scalars);
        break;
    case 7:
        result = pippenger_unsafe_internal<1 << 7>(points, scalars);
        break;
    case 6:
        result = pippenger_unsafe_internal<1 << 6>(points, scalars);
        break;
    case 5:
        result = pippenger_unsafe_internal<1 << 5>(points, scalars);
        break;
    case 4:
        result = pippenger_unsafe_internal<1 << 4>(points, scalars);
        break;
    case 3:
        result = pippenger_unsafe_internal<1 << 3>(points, scalars);
        break;
    }

    if ((1UL << log2_initial_points) == num_initial_points) {
        return result;
    } else {
        result += pippenger(scalars + (1UL << log2_initial_points),
                            points + (1UL << (log2_initial_points + 1)),
                            num_initial_points - (1UL << log2_initial_points));
        return result;
    }
}

template void compute_wnaf_states<1 << 2>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 3>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 4>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 5>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 6>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 7>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 8>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 9>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 10>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 11>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 12>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 13>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 14>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 15>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 16>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 17>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 18>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 19>(multiplication_runtime_state& state, fr::field_t* scalars);
template void compute_wnaf_states<1 << 20>(multiplication_runtime_state& state, fr::field_t* scalars);

template void organize_buckets<1 << 2>(multiplication_runtime_state& state);
template void organize_buckets<1 << 3>(multiplication_runtime_state& state);
template void organize_buckets<1 << 4>(multiplication_runtime_state& state);
template void organize_buckets<1 << 5>(multiplication_runtime_state& state);
template void organize_buckets<1 << 6>(multiplication_runtime_state& state);
template void organize_buckets<1 << 7>(multiplication_runtime_state& state);
template void organize_buckets<1 << 8>(multiplication_runtime_state& state);
template void organize_buckets<1 << 9>(multiplication_runtime_state& state);
template void organize_buckets<1 << 10>(multiplication_runtime_state& state);
template void organize_buckets<1 << 11>(multiplication_runtime_state& state);
template void organize_buckets<1 << 12>(multiplication_runtime_state& state);
template void organize_buckets<1 << 13>(multiplication_runtime_state& state);
template void organize_buckets<1 << 14>(multiplication_runtime_state& state);
template void organize_buckets<1 << 15>(multiplication_runtime_state& state);
template void organize_buckets<1 << 16>(multiplication_runtime_state& state);
template void organize_buckets<1 << 17>(multiplication_runtime_state& state);
template void organize_buckets<1 << 18>(multiplication_runtime_state& state);
template void organize_buckets<1 << 19>(multiplication_runtime_state& state);
template void organize_buckets<1 << 20>(multiplication_runtime_state& state);
template void organize_buckets<1 << 21>(multiplication_runtime_state& state);

} // namespace scalar_multiplication
} // namespace barretenberg