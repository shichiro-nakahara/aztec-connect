#include <benchmark/benchmark.h>

using namespace benchmark;

#include <iostream>
#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <barretenberg/types.hpp>

#include <barretenberg/curves/bn254/fq.hpp>
#include <barretenberg/curves/bn254/fr.hpp>
#include <barretenberg/curves/bn254/g1.hpp>
#include <barretenberg/curves/bn254/g2.hpp>
#include <barretenberg/curves/bn254/pairing.hpp>
#include <barretenberg/curves/bn254/scalar_multiplication/scalar_multiplication.hpp>

#include <barretenberg/groups/wnaf.hpp>
#include <barretenberg/io/io.hpp>
#include <barretenberg/polynomials/polynomial_arithmetic.hpp>
#include <barretenberg/waffle/proof_system/preprocess.hpp>
#include <barretenberg/waffle/proof_system/prover/prover.hpp>
#include <barretenberg/waffle/proof_system/verifier/verifier.hpp>
#include <barretenberg/waffle/proof_system/widgets/arithmetic_widget.hpp>

using namespace barretenberg;

constexpr size_t MAX_GATES = 1 << 20;
constexpr size_t START = (1 << 20) >> 7;

#define CIRCUIT_STATE_SIZE(x) ((x * 17 * sizeof(fr::field_t)) + (x * 3 * sizeof(uint32_t)))
#define FFT_SIZE(x) (x * 22 * sizeof(fr::field_t))

struct global_vars {
    alignas(32) g1::affine_element g1_pair_points[2];
    alignas(32) g2::affine_element g2_pair_points[2];
    std::vector<waffle::Verifier> plonk_instances;
    waffle::plonk_proof plonk_proof;
    waffle::ReferenceString reference_string;
    std::vector<waffle::plonk_proof> plonk_proofs;
    fr::field_t* data;
    fr::field_t* scalars;
    fr::field_t* roots;
    fr::field_t* coefficients;
};

global_vars globals;

barretenberg::evaluation_domain evaluation_domains[10]{
    barretenberg::evaluation_domain(START),       barretenberg::evaluation_domain(START * 2),
    barretenberg::evaluation_domain(START * 4),   barretenberg::evaluation_domain(START * 8),
    barretenberg::evaluation_domain(START * 16),  barretenberg::evaluation_domain(START * 32),
    barretenberg::evaluation_domain(START * 64),  barretenberg::evaluation_domain(START * 128),
    barretenberg::evaluation_domain(START * 256), barretenberg::evaluation_domain(START * 512)
};

void generate_scalars(fr::field_t* scalars)
{
    fr::field_t T0 = fr::field_t::random_element();
    fr::field_t acc;
    fr::field_t::__copy(T0, acc);
    for (size_t i = 0; i < MAX_GATES; ++i) {
        acc *= T0;
        fr::field_t::__copy(acc, scalars[i]);
    }
}

void generate_pairing_points(g1::affine_element* p1s, g2::affine_element* p2s)
{
    p1s[0] = g1::affine_element(g1::element::random_element());
    p1s[1] = g1::affine_element(g1::element::random_element());
    p2s[0] = g2::affine_element(g2::element::random_element());
    p2s[1] = g2::affine_element(g2::element::random_element());
}

constexpr size_t MAX_ROUNDS = 9;
const auto init = []() {
    printf("generating test data\n");
    globals.reference_string = waffle::ReferenceString(MAX_GATES);
    globals.scalars = (fr::field_t*)(aligned_alloc(32, sizeof(fr::field_t) * MAX_GATES * MAX_ROUNDS));
    std::string my_file_path = std::string(BARRETENBERG_SRS_PATH);
    globals.data = (fr::field_t*)(aligned_alloc(32, sizeof(fr::field_t) * (8 * 17 * MAX_GATES)));

    generate_pairing_points(&globals.g1_pair_points[0], &globals.g2_pair_points[0]);
    for (size_t i = 0; i < MAX_ROUNDS; ++i) {
        generate_scalars(&globals.scalars[i * MAX_GATES]);
    }
    globals.plonk_instances.resize(8);
    globals.plonk_proofs.resize(8);
    for (size_t i = 0; i < 10; ++i) {
        evaluation_domains[i].compute_lookup_table();
    }
    printf("finished generating test data\n");
    return true;
}();

uint64_t rdtsc()
{
#ifdef __aarch64__
    uint64_t pmccntr;
    __asm__ __volatile__("mrs %0, pmccntr_el0" : "=r"(pmccntr));
    return pmccntr;
#elif __x86_64__
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#else
    return 0;
#endif
}

constexpr size_t NUM_SQUARINGS = 10000000;
inline fq::field_t fq_sqr_asm(fq::field_t& a, fq::field_t& r) noexcept
{
    for (size_t i = 0; i < NUM_SQUARINGS; ++i) {
        r = a.sqr();
    }
    DoNotOptimize(r);
    return r;
}

constexpr size_t NUM_MULTIPLICATIONS = 10000000;
inline fq::field_t fq_mul_asm(fq::field_t& a, fq::field_t& r) noexcept
{
    for (size_t i = 0; i < NUM_MULTIPLICATIONS; ++i) {
        r = a * r;
    }
    DoNotOptimize(r);
    return r;
}

void pippenger_bench(State& state) noexcept
{
    // uint64_t count = 0;
    // uint64_t i = 0;
    const uint64_t num_points = static_cast<uint64_t>(state.range(0));
    for (auto _ : state) {
        // uint64_t before = rdtsc();
        scalar_multiplication::pippenger(&globals.scalars[0], &globals.reference_string.monomials[0], num_points);
        // uint64_t after = rdtsc();
        // count += (after - before);
        // ++i;
    }
    // uint64_t avg_cycles = count / i;
    // printf("pippenger. %" PRIu64 " points. clock cycles = %" PRIu64 "\n", (num_points), (avg_cycles));
    // printf("pippenger clock cycles per mul = %" PRIu64 "\n", (avg_cycles / (MAX_GATES)));
}
BENCHMARK(pippenger_bench)->RangeMultiplier(2)->Range(START, MAX_GATES);

void unsafe_pippenger_bench(State& state) noexcept
{
    uint64_t count = 0;
    const uint64_t num_points = static_cast<uint64_t>(state.range(0));
    uint64_t i = 0;
    for (auto _ : state) {
        uint64_t before = rdtsc();
        scalar_multiplication::pippenger_unsafe(
            &globals.scalars[0], &globals.reference_string.monomials[0], num_points);
        uint64_t after = rdtsc();
        count += (after - before);
        ++i;
    }
    uint64_t avg_cycles = count / i;
    printf("unsafe pippenger. %" PRIu64 " points. clock cycles = %" PRIu64 "\n", (num_points), (avg_cycles));
    printf("unsafe pippenger clock cycles per mul = %" PRIu64 "\n", (avg_cycles / (MAX_GATES)));
}
BENCHMARK(unsafe_pippenger_bench)->RangeMultiplier(2)->Range(1 << 20, 1 << 20);

void new_plonk_scalar_multiplications_bench(State& state) noexcept
{
    uint64_t count = 0;
    uint64_t k = 0;
    for (auto _ : state) {
        uint64_t before = rdtsc();
        g1::element a =
            scalar_multiplication::pippenger(&globals.scalars[0], &globals.reference_string.monomials[0], MAX_GATES);
        g1::element b =
            scalar_multiplication::pippenger(&globals.scalars[1], &globals.reference_string.monomials[0], MAX_GATES);
        g1::element c =
            scalar_multiplication::pippenger(&globals.scalars[2], &globals.reference_string.monomials[0], MAX_GATES);
        g1::element d =
            scalar_multiplication::pippenger(&globals.scalars[3], &globals.reference_string.monomials[0], MAX_GATES);
        g1::element e =
            scalar_multiplication::pippenger(&globals.scalars[4], &globals.reference_string.monomials[0], MAX_GATES);
        g1::element f =
            scalar_multiplication::pippenger(&globals.scalars[5], &globals.reference_string.monomials[0], MAX_GATES);
        g1::element g =
            scalar_multiplication::pippenger(&globals.scalars[6], &globals.reference_string.monomials[0], MAX_GATES);
        g1::element h =
            scalar_multiplication::pippenger(&globals.scalars[7], &globals.reference_string.monomials[0], MAX_GATES);
        g1::element i =
            scalar_multiplication::pippenger(&globals.scalars[8], &globals.reference_string.monomials[0], MAX_GATES);
        uint64_t after = rdtsc();
        count += (after - before);
        ++k;
        g1::element out;
        out.self_set_infinity();
        out = a + out;
        out = b + out;
        out = c + out;
        out = d + out;
        out = e + out;
        out = f + out;
        out = g + out;
        out = h + out;
        out = i + out;
    }
    uint64_t avg_cycles = count / k;
    printf("plonk clock cycles = %" PRIu64 "\n", (avg_cycles));
    printf("pippenger clock cycles = %" PRIu64 "\n", (avg_cycles / 9));
    printf("pippenger clock cycles per scalar mul = %" PRIu64 "\n", (avg_cycles / (9 * MAX_GATES)));
}
BENCHMARK(new_plonk_scalar_multiplications_bench);

void coset_fft_bench_parallel(State& state) noexcept
{
    for (auto _ : state) {
        size_t idx = (size_t)log2(state.range(0)) - (size_t)log2(START);
        barretenberg::polynomial_arithmetic::coset_fft(globals.data, evaluation_domains[idx]);
    }
}
BENCHMARK(coset_fft_bench_parallel)->RangeMultiplier(2)->Range(START * 4, MAX_GATES * 4);

void alternate_coset_fft_bench_parallel(State& state) noexcept
{
    for (auto _ : state) {
        size_t idx = (size_t)log2(state.range(0)) - (size_t)log2(START);
        barretenberg::polynomial_arithmetic::coset_fft(
            globals.data, evaluation_domains[idx - 2], evaluation_domains[idx - 2], 4);
    }
}
BENCHMARK(alternate_coset_fft_bench_parallel)->RangeMultiplier(2)->Range(START * 4, MAX_GATES * 4);

void fft_bench_parallel(State& state) noexcept
{
    for (auto _ : state) {
        size_t idx = (size_t)log2(state.range(0)) - (size_t)log2(START);
        barretenberg::polynomial_arithmetic::fft(globals.data, evaluation_domains[idx]);
    }
}
BENCHMARK(fft_bench_parallel)->RangeMultiplier(2)->Range(START * 4, MAX_GATES * 4);

void fft_bench_serial(State& state) noexcept
{
    for (auto _ : state) {
        size_t idx = (size_t)log2(state.range(0)) - (size_t)log2(START);
        barretenberg::polynomial_arithmetic::fft_inner_serial(
            globals.data, evaluation_domains[idx].thread_size, evaluation_domains[idx].get_round_roots());
    }
}
BENCHMARK(fft_bench_serial)->RangeMultiplier(2)->Range(START * 4, MAX_GATES * 4);

void pairing_bench(State& state) noexcept
{
    uint64_t count = 0;
    uint64_t i = 0;
    for (auto _ : state) {
        uint64_t before = rdtsc();
        DoNotOptimize(pairing::reduced_ate_pairing(globals.g1_pair_points[0], globals.g2_pair_points[0]));
        uint64_t after = rdtsc();
        count += (after - before);
        ++i;
    }
    uint64_t avg_cycles = count / i;
    printf("single pairing clock cycles = %" PRIu64 "\n", (avg_cycles));
}
BENCHMARK(pairing_bench);

void pairing_twin_bench(State& state) noexcept
{
    uint64_t count = 0;
    uint64_t i = 0;
    for (auto _ : state) {
        uint64_t before = rdtsc();
        DoNotOptimize(pairing::reduced_ate_pairing_batch(&globals.g1_pair_points[0], &globals.g2_pair_points[0], 2));
        uint64_t after = rdtsc();
        count += (after - before);
        ++i;
    }
    uint64_t avg_cycles = count / i;
    printf("twin pairing clock cycles = %" PRIu64 "\n", (avg_cycles));
}
BENCHMARK(pairing_twin_bench);

constexpr size_t NUM_G1_ADDITIONS = 10000000;
void add_bench(State& state) noexcept
{
    uint64_t count = 0;
    uint64_t j = 0;
    g1::element a = g1::element::random_element();
    g1::element b = g1::element::random_element();
    for (auto _ : state) {
        uint64_t before = rdtsc();
        for (size_t i = 0; i < NUM_G1_ADDITIONS; ++i) {
            a += b;
        }
        uint64_t after = rdtsc();
        count += (after - before);
        ++j;
    }
    printf("g1 add number of cycles = %" PRIu64 "\n", count / (j * NUM_G1_ADDITIONS));
}
BENCHMARK(add_bench);

void mixed_add_bench(State& state) noexcept
{
    uint64_t count = 0;
    uint64_t j = 0;
    g1::element a = g1::element::random_element();
    g1::affine_element b = g1::affine_element(g1::element::random_element());
    for (auto _ : state) {
        uint64_t before = rdtsc();
        for (size_t i = 0; i < NUM_G1_ADDITIONS; ++i) {
            a += b;
        }
        uint64_t after = rdtsc();
        count += (after - before);
        ++j;
    }
    printf("g1 mixed add number of cycles = %" PRIu64 "\n", count / (j * NUM_G1_ADDITIONS));
    // printf("r_2 = [%" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "]\n", r_2[0], r_2[1], r_2[2], r_2[3]);
}
BENCHMARK(mixed_add_bench);

void fq_sqr_asm_bench(State& state) noexcept
{
    uint64_t count = 0;
    uint64_t i = 0;
    fq::field_t a{ 0x1122334455667788, 0x8877665544332211, 0x0123456701234567, 0x0efdfcfbfaf9f8f7 };
    fq::field_t r{ 1, 0, 0, 0 };
    for (auto _ : state) {
        size_t before = rdtsc();
        (DoNotOptimize(fq_sqr_asm(a, r)));
        size_t after = rdtsc();
        count += after - before;
        ++i;
    }
    printf("sqr number of cycles = %" PRIu64 "\n", count / (i * NUM_SQUARINGS));
    // printf("r_2 = [%" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "]\n", r_2[0], r_2[1], r_2[2], r_2[3]);
}
BENCHMARK(fq_sqr_asm_bench);

void fq_mul_asm_bench(State& state) noexcept
{
    uint64_t count = 0;
    uint64_t i = 0;
    fq::field_t a{ 0x1122334455667788, 0x8877665544332211, 0x0123456701234567, 0x0efdfcfbfaf9f8f7 };
    fq::field_t r{ 1, 0, 0, 0 };
    for (auto _ : state) {
        size_t before = rdtsc();
        (DoNotOptimize(fq_mul_asm(a, r)));
        size_t after = rdtsc();
        count += after - before;
        ++i;
    }
    printf("mul number of cycles = %" PRIu64 "\n", count / (i * NUM_MULTIPLICATIONS));
}
BENCHMARK(fq_mul_asm_bench);

BENCHMARK_MAIN();
// 21218750000
