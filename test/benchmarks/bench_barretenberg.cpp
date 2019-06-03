#include <benchmark/benchmark.h>

using namespace benchmark;

#include <gmp.h>
#include <iostream>
#include <time.h>
#include <libff/algebra/fields/fp.hpp>
#include <libff/algebra/curves/alt_bn128/alt_bn128_init.hpp>
#include <libff/algebra/curves/alt_bn128/alt_bn128_g1.hpp>
#include <libff/algebra/scalar_multiplication/multiexp.hpp>

#include <barretenberg/g1.hpp>
#include <barretenberg/fq.hpp>
#include <barretenberg/fr.hpp>
#include <barretenberg/scalar_multiplication.hpp>

struct multiplication_data
{
    g1::affine_element* points;
    uint64_t* scalars;
    std::vector<libff::alt_bn128_G1> libff_points;
    std::vector<libff::alt_bn128_Fr> libff_scalars; 
};

constexpr size_t NUM_POINTS = 10000000;

void generate_points(multiplication_data& data, size_t num_points)
{
    data.scalars = (uint64_t*)aligned_alloc(32, sizeof(uint64_t) * 4 * NUM_POINTS);
    data.points = (g1::affine_element*)aligned_alloc(32, sizeof(g1::affine_element) * NUM_POINTS * 2);

    data.libff_points.reserve(num_points);
    data.libff_scalars.reserve(num_points);

    g1::element small_table[10000];
    for (size_t i = 0; i < 10000; ++i)
    {
        small_table[i] = g1::random_element();
    }
    g1::element current_table[10000];
    for (size_t i = 0; i < (num_points / 10000); ++i)
    {
        for (size_t j = 0; j < 10000; ++j)
        {
            g1::add(small_table[i], small_table[j], current_table[j]);
        }
        g1::batch_normalize(&current_table[0], 10000);
        for (size_t j = 0; j < 10000; ++j)
        {
            libff::alt_bn128_G1 libff_pt = libff::alt_bn128_G1::one();
            fq::copy(current_table[j].x, data.points[i * 10000 + j].x);
            fq::copy(current_table[j].y, data.points[i * 10000 + j].y);
            fq::copy(current_table[j].x, libff_pt.X.mont_repr.data);
            fq::copy(current_table[j].y, libff_pt.Y.mont_repr.data);
            data.libff_points.emplace_back(libff_pt);
        }
    }
    g1::batch_normalize(small_table, 10000);
    size_t rounded = (num_points / 10000) * 10000;
    size_t leftovers = num_points - rounded;
    for (size_t j = 0;  j < leftovers; ++j)
    {
            libff::alt_bn128_G1 libff_pt = libff::alt_bn128_G1::one();
            fq::copy(small_table[j].x, data.points[rounded + j].x);
            fq::copy(small_table[j].y, data.points[rounded + j].y);
        
            fq::copy(small_table[j].x, libff_pt.X.mont_repr.data);
            fq::copy(small_table[j].y, libff_pt.Y.mont_repr.data);
            data.libff_points.emplace_back(libff_pt);
    }

    for (size_t i = 0; i < num_points; ++i)
    {
        fr::random_element(&data.scalars[i * 4]);
        libff::alt_bn128_Fr libff_scalar;
        fq::copy(&data.scalars[i * 4], libff_scalar.mont_repr.data);
        data.libff_scalars.emplace_back(libff_scalar);
    }
    scalar_multiplication::generate_pippenger_point_table(data.points, data.points, num_points);
}

multiplication_data point_data;

const auto init = []() {
    libff::init_alt_bn128_params();
    printf("generating point data\n");
    generate_points(point_data, NUM_POINTS);
    printf("generated point data\n");
    return true;
}();

uint64_t rdtsc(){
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}


inline uint64_t fq_sqr_asm(uint64_t* a, uint64_t* r) noexcept
{
    for (size_t i = 0; i < 10000000; ++i)
    {
        fq::sqr(a, r);
    }
    return 1;
}


inline uint64_t fq_mul_asm(uint64_t* a, uint64_t* r) noexcept
{
    for (size_t i = 0; i < 10000000; ++i)
    {
        fq::mul(a, r, r);
    }
    return 1;
}

inline uint64_t fq_mul_libff(libff::alt_bn128_Fq& a, libff::alt_bn128_Fq& r)
{
    for (size_t i = 0; i < 10000000; ++i)
    {
        r = a * r;
    }
    return 1;
}



void pippenger_bench(State& state) noexcept
{
    for (auto _ : state)
    {
        DoNotOptimize(scalar_multiplication::pippenger(&point_data.scalars[0], &point_data.points[0], NUM_POINTS, 21));
    }
}
BENCHMARK(pippenger_bench);

void libff_pippenger_bench(State &state) noexcept
{
    for (auto _ : state)
    {
        DoNotOptimize(libff::multi_exp<libff::alt_bn128_G1, libff::alt_bn128_Fr, libff::multi_exp_method_BDLO12>(
            point_data.libff_points.begin(),
            point_data.libff_points.end(),
            point_data.libff_scalars.begin(),
            point_data.libff_scalars.end(),
            1));
    }
}
BENCHMARK(libff_pippenger_bench);


void dbl_bench(State& state) noexcept
{
    // uint64_t count = 0;
    // uint64_t i = 0;
    g1::element a = g1::random_element();
    for (auto _ : state)
    {
        for (size_t i = 0; i < 10000000; ++i)
        {
            g1::dbl(a, a);
        }
    }
    // printf("number of cycles = %lu\n", count / i);
    // printf("r_2 = [%lu, %lu, %lu, %lu]\n", r_2[0], r_2[1], r_2[2], r_2[3]);
}
BENCHMARK(dbl_bench);


void dbl_libff_bench(State& state) noexcept
{
    // uint64_t count = 0;
    // uint64_t i = 0;
    libff::init_alt_bn128_params();
    libff::alt_bn128_G1 a = libff::alt_bn128_G1::random_element();

    for (auto _ : state)
    {
        for (size_t i = 0; i < 10000000; ++i)
        {
            a = a.dbl();
        }
    }
    // printf("number of cycles = %lu\n", count / i);
    // printf("r_2 = [%lu, %lu, %lu, %lu]\n", r_2[0], r_2[1], r_2[2], r_2[3]);
}
BENCHMARK(dbl_libff_bench);

void add_bench(State& state) noexcept
{
    // uint64_t count = 0;
    // uint64_t i = 0;
    g1::element a = g1::random_element();
    g1::element b = g1::random_element();
    for (auto _ : state)
    {
        for (size_t i = 0; i < 10000000; ++i)
        {
            g1::add(a, b, a);
        }
    }
    // printf("number of cycles = %lu\n", count / i);
    // printf("r_2 = [%lu, %lu, %lu, %lu]\n", r_2[0], r_2[1], r_2[2], r_2[3]);
}
BENCHMARK(add_bench);


void add_libff_bench(State& state) noexcept
{
    // uint64_t count = 0;
    // uint64_t i = 0;
    libff::init_alt_bn128_params();
    libff::alt_bn128_G1 a = libff::alt_bn128_G1::random_element();
    libff::alt_bn128_G1 b = libff::alt_bn128_G1::random_element();

    for (auto _ : state)
    {
        for (size_t i = 0; i < 10000000; ++i)
        {
            a = a + b;
        }
    }
    // printf("number of cycles = %lu\n", count / i);
    // printf("r_2 = [%lu, %lu, %lu, %lu]\n", r_2[0], r_2[1], r_2[2], r_2[3]);
}
BENCHMARK(add_libff_bench);

void mixed_add_bench(State& state) noexcept
{
    // uint64_t count = 0;
    // uint64_t i = 0;
    g1::element a = g1::random_element();
    g1::affine_element b = g1::random_affine_element();
    for (auto _ : state)
    {
        for (size_t i = 0; i < 10000000; ++i)
        {
            g1::mixed_add(a, b, a);
        }
    }
    // printf("number of cycles = %lu\n", count / i);
    // printf("r_2 = [%lu, %lu, %lu, %lu]\n", r_2[0], r_2[1], r_2[2], r_2[3]);
}
BENCHMARK(mixed_add_bench);


void mixed_add_libff_bench(State& state) noexcept
{
    // uint64_t count = 0;
    // uint64_t i = 0;
    libff::init_alt_bn128_params();
    libff::alt_bn128_G1 a;
    a = libff::alt_bn128_G1::random_element();
    libff::alt_bn128_G1 b;
    b.X = a.X;
    b.Y = a.Y;
    b.Z = libff::alt_bn128_Fq::one();

    for (auto _ : state)
    {
        for (size_t i = 0; i < 10000000; ++i)
        {
            a = a.mixed_add(b);
        }
    }
    // printf("number of cycles = %lu\n", count / i);
    // printf("r_2 = [%lu, %lu, %lu, %lu]\n", r_2[0], r_2[1], r_2[2], r_2[3]);
}
BENCHMARK(mixed_add_libff_bench);

void fq_sqr_asm_bench(State& state) noexcept
{
    // uint64_t count = 0;
    // uint64_t i = 0;
    uint64_t a[4] = { 0x1122334455667788, 0x8877665544332211, 0x0123456701234567, 0x0efdfcfbfaf9f8f7 };
    uint64_t r[4] = { 1, 0, 0, 0 };
    for (auto _ : state)
    {
        (DoNotOptimize(fq_sqr_asm(&a[0], &r[0])));
        // ++i;
    }
    // printf("number of cycles = %lu\n", count / i);
    // printf("r_2 = [%lu, %lu, %lu, %lu]\n", r_2[0], r_2[1], r_2[2], r_2[3]);
}
BENCHMARK(fq_sqr_asm_bench);

void fq_mul_asm_bench(State& state) noexcept
{
    // uint64_t count = 0;
    // uint64_t i = 0;
    uint64_t a[4] = { 0x1122334455667788, 0x8877665544332211, 0x0123456701234567, 0x0efdfcfbfaf9f8f7 };
    uint64_t r[4] = { 1, 0, 0, 0 };
    for (auto _ : state)
    {
        (DoNotOptimize(fq_mul_asm(&a[0], &r[0])));
        // ++i;
    }
    // printf("number of cycles = %lu\n", count / i);
    // printf("r_2 = [%lu, %lu, %lu, %lu]\n", r_2[0], r_2[1], r_2[2], r_2[3]);
}
BENCHMARK(fq_mul_asm_bench);


void fq_mul_libff_bench(State& state) noexcept
{
    // uint64_t count = 0;
    // uint64_t i = 0;
    libff::init_alt_bn128_params();
    libff::alt_bn128_Fq a = libff::alt_bn128_Fq::one();
    libff::alt_bn128_Fq r = libff::alt_bn128_Fq::one();
    
    a.mont_repr.data[0] = 0x1122334455667788;
    a.mont_repr.data[1] = 0x8877665544332211;
    a.mont_repr.data[2] = 0x0123456701234567;
    a.mont_repr.data[3] = 0x0efdfcfbfaf9f8f7;
    r.mont_repr.data[0] = 1;
    r.mont_repr.data[1] = 0;
    r.mont_repr.data[2] = 0;
    r.mont_repr.data[3] = 0;

    for (auto _ : state)
    {
        (DoNotOptimize(fq_mul_libff(a, r)));
        // ++i;
    }
}
BENCHMARK(fq_mul_libff_bench);

BENCHMARK_MAIN();
// 21218750000