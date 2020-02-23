#include <gtest/gtest.h>

#include <barretenberg/polynomials/polynomial.hpp>
#include <barretenberg/polynomials/polynomial_arithmetic.hpp>
#include <barretenberg/uint256/uint256.hpp>
#include <barretenberg/waffle/proof_system/permutation.hpp>
#include <barretenberg/waffle/proof_system/prover/prover.hpp>
#include <barretenberg/waffle/proof_system/public_inputs/public_inputs.hpp>
#include <barretenberg/waffle/proof_system/widgets/arithmetic_widget.hpp>

#include <barretenberg/curves/bn254/g1.hpp>
#include <barretenberg/curves/bn254/scalar_multiplication/scalar_multiplication.hpp>

#include <barretenberg/transcript/manifest.hpp>

#include <random>

using namespace barretenberg;

namespace {
std::mt19937 engine;
std::uniform_int_distribution<uint64_t> dist{ 0ULL, UINT64_MAX };

const auto init = []() {
    // std::random_device rd{};
    std::seed_seq seed2{ 1, 2, 3, 4, 5, 6, 7, 8 };
    engine = std::mt19937(seed2);
    return 1;
}();

fr::field_t get_pseudorandom_element()
{
    return uint256_t(fr::field_t{ dist(engine), dist(engine), dist(engine), dist(engine) });
}
} // namespace

/*
```
elliptic curve point addition on a short weierstrass curve.

circuit has 9 gates, I've added 7 dummy gates so that the polynomial degrees are a power of 2

input points: (x_1, y_1), (x_2, y_2)
output point: (x_3, y_3)
intermediate variables: (t_1, t_2, t_3, t_4, t_5, t_6, t_7)

Variable assignments:
t_1 = (y_2 - y_1)
t_2 = (x_2 - x_1)
t_3 = (y_2 - y_1) / (x_2 - x_1)
x_3 = t_3*t_3 - x_2 - x_1
y_3 = t_3*(x_1 - x_3) - y_1
t_4 = (x_3 + x_1)
t_5 = (t_4 + x_2)
t_6 = (y_3 + y_1)
t_7 = (x_1 - x_3)

Constraints:
(y_2 - y_1) - t_1 = 0
(x_2 - x_1) - t_2 = 0
(x_1 + x_2) - t_4 = 0
(t_4 + x_3) - t_5 = 0
(y_3 + y_1) - t_6 = 0
(x_1 - x_3) - t_7 = 0
 (t_3 * t_2) - t_1 = 0
-(t_3 * t_3) + t_5 = 0
-(t_3 * t_7) + t_6 = 0

Wire polynomials:
w_l = [y_2, x_2, x_1, t_4, y_3, x_1, t_3, t_3, t_3, 0, 0, 0, 0, 0, 0, 0]
w_r = [y_1, x_1, x_2, x_3, y_1, x_3, t_2, t_3, t_7, 0, 0, 0, 0, 0, 0, 0]
w_o = [t_1, t_2, t_4, t_5, t_6, t_7, t_1, t_5, t_6, 0, 0, 0, 0, 0, 0, 0]

Gate polynomials:
q_m = [ 0,  0,  0,  0,  0,  0,  1, -1, -1, 0, 0, 0, 0, 0, 0, 0]
q_l = [ 1,  1,  1,  1,  1,  1,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0]
q_r = [-1, -1,  1,  1,  1, -1,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0]
q_o = [-1, -1, -1, -1, -1, -1, -1,  1,  1, 0, 0, 0, 0, 0, 0, 0]
q_c = [ 0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0]

Permutation polynomials:
s_id = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]
sigma_1 = [1, 3+n, 6, 3+2n, 5, 2+n, 8, 9, 8+n, 10, 11, 12, 13, 14, 15, 16]
sigma_2 = [5+n, 3, 2, 6+n, 1+n, 4+n, 2+2n, 7, 6+2n, 10+n, 11+n, 12+n, 13+n, 14+n, 15+n, 16+n]
sigma_3 = [7+2n, 7+n, 4, 8+2n, 9+2n, 9+n, 1+2n, 4+2n, 5+2n, 10+2n, 11+2n, 12+2n, 13+2n, 14+2n, 15+2n]

(for n = 16, permutation polynomials are)
sigma_1 = [1, 19, 6, 35, 5, 18, 8, 9, 24, 10, 11, 12, 13, 14, 15, 16]
sigma_2 = [21, 3, 2, 22, 17, 20, 34, 7, 38, 26, 27, 28, 29, 30, 31, 32]
sigma_3 = [39, 23, 4, 40, 41, 25, 33, 36, 37, 42, 43, 44, 45, 46, 47, 48]
```
*/
using namespace barretenberg;
using namespace waffle;

namespace {

// transcript::Manifest create_manifest(const size_t num_public_inputs = 0)
// {
//     constexpr size_t g1_size = 64;
//     constexpr size_t fr_size = 32;
//     const size_t public_input_size = fr_size * num_public_inputs;
//     const transcript::Manifest output = transcript::Manifest(
//         { transcript::Manifest::RoundManifest({ { "circuit_size", 4, false } }, "init"),
//           transcript::Manifest::RoundManifest({ { "public_inputs", public_input_size, false },
//                                                 { "W_1", g1_size, false },
//                                                 { "W_2", g1_size, false },
//                                                 { "W_3", g1_size, false } },
//                                               "beta"),
//           transcript::Manifest::RoundManifest({ {} }, "gamma"),
//           transcript::Manifest::RoundManifest({ { "Z", g1_size, false } }, "alpha"),
//           transcript::Manifest::RoundManifest(
//               { { "T_1", g1_size, false }, { "T_2", g1_size, false }, { "T_3", g1_size, false } }, "z"),
//           transcript::Manifest::RoundManifest({ { "w_1", fr_size, false },
//                                                 { "w_2", fr_size, false },
//                                                 { "w_3", fr_size, false },
//                                                 { "z_omega", fr_size, false },
//                                                 { "sigma_1", fr_size, false },
//                                                 { "sigma_2", fr_size, false },
//                                                 { "r", fr_size, false },
//                                                 { "t", fr_size, true } },
//                                               "nu"),
//           transcript::Manifest::RoundManifest({ { "PI_Z", g1_size, false }, { "PI_Z_OMEGA", g1_size, false } },
//                                               "separator") });
//     return output;
// }

// waffle::Prover generate_test_data(const size_t n, const size_t num_public_inputs)
// {
//     // state.widgets.emplace_back(std::make_unique<waffle::ProverArithmeticWidget>(n));

//     // create some constraints that satisfy our arithmetic circuit relation
//     fr::field_t T0;

//     // even indices = mul gates, odd incides = add gates

//     std::shared_ptr<proving_key> key = std::make_shared<proving_key>(n);
//     std::shared_ptr<program_witness> witness = std::make_shared<program_witness>();

//     polynomial w_l;
//     polynomial w_r;
//     polynomial w_o;
//     polynomial q_l;
//     polynomial q_r;
//     polynomial q_o;
//     polynomial q_c;
//     polynomial q_m;

//     w_l.resize(n);
//     w_r.resize(n);
//     w_o.resize(n);
//     q_l.resize(n);
//     q_r.resize(n);
//     q_o.resize(n);
//     q_m.resize(n);
//     q_c.resize(n);

//     for (size_t i = 0; i < n / 4; ++i) {
//         w_l.at(2 * i) = fr::random_element();
//         w_r.at(2 * i) = fr::random_element();
//         fr::__mul(w_l.at(2 * i), w_r.at(2 * i), w_o.at(2 * i));
//         fr::__add(w_o[2 * i], w_l[2 * i], w_o[2 * i]);
//         fr::__add(w_o[2 * i], w_r[2 * i], w_o[2 * i]);
//         fr::__add(w_o[2 * i], fr::one, w_o[2 * i]);
//         fr::__copy(fr::one, q_l.at(2 * i));
//         fr::__copy(fr::one, q_r.at(2 * i));
//         fr::__copy(fr::neg_one(), q_o.at(2 * i));
//         fr::__copy(fr::one, q_c.at(2 * i));
//         fr::__copy(fr::one, q_m.at(2 * i));

//         w_l.at(2 * i + 1) = fr::random_element();
//         w_r.at(2 * i + 1) = fr::random_element();
//         w_o.at(2 * i + 1) = fr::random_element();

//         fr::__add(w_l.at(2 * i + 1), w_r.at(2 * i + 1), T0);
//         fr::__add(T0, w_o.at(2 * i + 1), q_c.at(2 * i + 1));
//         fr::__neg(q_c.at(2 * i + 1), q_c.at(2 * i + 1));
//         q_l.at(2 * i + 1) = fr::one;
//         q_r.at(2 * i + 1) = fr::one;
//         q_o.at(2 * i + 1) = fr::one;
//         q_m.at(2 * i + 1) = fr::zero;
//     }
//     size_t shift = n / 2;
//     polynomial_arithmetic::copy_polynomial(&w_l.at(0), &w_l.at(shift), shift, shift);
//     polynomial_arithmetic::copy_polynomial(&w_r.at(0), &w_r.at(shift), shift, shift);
//     polynomial_arithmetic::copy_polynomial(&w_o.at(0), &w_o.at(shift), shift, shift);
//     polynomial_arithmetic::copy_polynomial(&q_m.at(0), &q_m.at(shift), shift, shift);
//     polynomial_arithmetic::copy_polynomial(&q_l.at(0), &q_l.at(shift), shift, shift);
//     polynomial_arithmetic::copy_polynomial(&q_r.at(0), &q_r.at(shift), shift, shift);
//     polynomial_arithmetic::copy_polynomial(&q_o.at(0), &q_o.at(shift), shift, shift);
//     polynomial_arithmetic::copy_polynomial(&q_c.at(0), &q_c.at(shift), shift, shift);

//     std::vector<uint32_t> sigma_1_mapping;
//     std::vector<uint32_t> sigma_2_mapping;
//     std::vector<uint32_t> sigma_3_mapping;
//     // create basic permutation - second half of witness vector is a copy of the first half
//     sigma_1_mapping.resize(n);
//     sigma_2_mapping.resize(n);
//     sigma_3_mapping.resize(n);

//     for (size_t i = 0; i < n / 2; ++i) {
//         sigma_1_mapping[i] = (uint32_t)i + (1U << 30U);
//         sigma_2_mapping[shift + i] = (uint32_t)i + (1U << 30U);
//         sigma_3_mapping[shift + i] = (uint32_t)i + (1U << 31U);
//         sigma_1_mapping[i] = (uint32_t)(i + shift);
//         sigma_2_mapping[i] = (uint32_t)(i + shift) + (1U << 30U);
//         sigma_3_mapping[i] = (uint32_t)(i + shift) + (1U << 31U);
//     }
//     // make last permutation the same as identity permutation
//     sigma_1_mapping[shift - 1] = (uint32_t)shift - 1 + (1U << 30U);
//     sigma_2_mapping[shift - 1] = (uint32_t)shift - 1 + (1U << 30U);
//     sigma_3_mapping[shift - 1] = (uint32_t)shift - 1 + (1U << 31U);
//     sigma_1_mapping[n - 1] = (uint32_t)n - 1 + (1U << 30U);
//     sigma_2_mapping[n - 1] = (uint32_t)n - 1 + (1U << 30U);
//     sigma_3_mapping[n - 1] = (uint32_t)n - 1 + (1U << 31U);

//     polynomial sigma_1(key->n);
//     polynomial sigma_2(key->n);
//     polynomial sigma_3(key->n);

//     waffle::compute_permutation_lagrange_base_single<standard_settings>(sigma_1, sigma_1_mapping, key->small_domain);
//     waffle::compute_permutation_lagrange_base_single<standard_settings>(sigma_2, sigma_2_mapping, key->small_domain);
//     waffle::compute_permutation_lagrange_base_single<standard_settings>(sigma_3, sigma_3_mapping, key->small_domain);

//     polynomial sigma_1_lagrange_base(sigma_1, key->n);
//     polynomial sigma_2_lagrange_base(sigma_2, key->n);
//     polynomial sigma_3_lagrange_base(sigma_3, key->n);

//     key->permutation_selectors_lagrange_base.insert({ "sigma_1", std::move(sigma_1_lagrange_base) });
//     key->permutation_selectors_lagrange_base.insert({ "sigma_2", std::move(sigma_2_lagrange_base) });
//     key->permutation_selectors_lagrange_base.insert({ "sigma_3", std::move(sigma_3_lagrange_base) });

//     sigma_1.ifft(key->small_domain);
//     sigma_2.ifft(key->small_domain);
//     sigma_3.ifft(key->small_domain);
//     constexpr size_t width = 4;
//     polynomial sigma_1_fft(sigma_1, key->n * width);
//     polynomial sigma_2_fft(sigma_2, key->n * width);
//     polynomial sigma_3_fft(sigma_3, key->n * width);

//     sigma_1_fft.coset_fft(key->large_domain);
//     sigma_2_fft.coset_fft(key->large_domain);
//     sigma_3_fft.coset_fft(key->large_domain);

//     key->permutation_selectors.insert({ "sigma_1", std::move(sigma_1) });
//     key->permutation_selectors.insert({ "sigma_2", std::move(sigma_2) });
//     key->permutation_selectors.insert({ "sigma_3", std::move(sigma_3) });

//     key->permutation_selector_ffts.insert({ "sigma_1_fft", std::move(sigma_1_fft) });
//     key->permutation_selector_ffts.insert({ "sigma_2_fft", std::move(sigma_2_fft) });
//     key->permutation_selector_ffts.insert({ "sigma_3_fft", std::move(sigma_3_fft) });

//     w_l.at(n - 1) = fr::zero;
//     w_r.at(n - 1) = fr::zero;
//     w_o.at(n - 1) = fr::zero;
//     q_c.at(n - 1) = fr::zero;
//     q_l.at(n - 1) = fr::zero;
//     q_r.at(n - 1) = fr::zero;
//     q_o.at(n - 1) = fr::zero;
//     q_m.at(n - 1) = fr::zero;

//     w_l.at(shift - 1) = fr::zero;
//     w_r.at(shift - 1) = fr::zero;
//     w_o.at(shift - 1) = fr::zero;
//     q_c.at(shift - 1) = fr::zero;

//     witness->wires.insert({ "w_1", std::move(w_l) });
//     witness->wires.insert({ "w_2", std::move(w_r) });
//     witness->wires.insert({ "w_3", std::move(w_o) });

//     q_l.ifft(key->small_domain);
//     q_r.ifft(key->small_domain);
//     q_o.ifft(key->small_domain);
//     q_m.ifft(key->small_domain);
//     q_c.ifft(key->small_domain);

//     polynomial q_1_fft(q_l, n * 2);
//     polynomial q_2_fft(q_r, n * 2);
//     polynomial q_3_fft(q_o, n * 2);
//     polynomial q_m_fft(q_m, n * 2);
//     polynomial q_c_fft(q_c, n * 2);

//     q_1_fft.coset_fft(key->mid_domain);
//     q_2_fft.coset_fft(key->mid_domain);
//     q_3_fft.coset_fft(key->mid_domain);
//     q_m_fft.coset_fft(key->mid_domain);
//     q_c_fft.coset_fft(key->mid_domain);

//     key->constraint_selectors.insert({ "q_1", std::move(q_l) });
//     key->constraint_selectors.insert({ "q_2", std::move(q_r) });
//     key->constraint_selectors.insert({ "q_3", std::move(q_o) });
//     key->constraint_selectors.insert({ "q_m", std::move(q_m) });
//     key->constraint_selectors.insert({ "q_c", std::move(q_c) });

//     key->constraint_selector_ffts.insert({ "q_1_fft", std::move(q_1_fft) });
//     key->constraint_selector_ffts.insert({ "q_2_fft", std::move(q_2_fft) });
//     key->constraint_selector_ffts.insert({ "q_3_fft", std::move(q_3_fft) });
//     key->constraint_selector_ffts.insert({ "q_m_fft", std::move(q_m_fft) });
//     key->constraint_selector_ffts.insert({ "q_c_fft", std::move(q_c_fft) });
//     std::unique_ptr<waffle::ProverArithmeticWidget> widget =
//         std::make_unique<waffle::ProverArithmeticWidget>(key.get(), witness.get());

//     waffle::Prover state = waffle::Prover(key, witness, create_manifest(num_public_inputs));
//     state.widgets.emplace_back(std::move(widget));
//     return state;
// }
// } // namespace

// TEST(test_public_inputs, compute_delta)
// {
//     constexpr uint32_t circuit_size = 256;
//     constexpr size_t num_public_inputs = 7;

//     waffle::Prover state = generate_test_data(circuit_size, num_public_inputs);

//     polynomial& sigma_1 = state.key->permutation_selectors.at("sigma_1");
//     polynomial& wires = state.witness->wires.at("w_1");
//     polynomial& wires2 = state.witness->wires.at("w_2");
//     polynomial& wires3 = state.witness->wires.at("w_3");

//     std::vector<fr::field_t> public_inputs;

//     fr::field_t work_root = fr::one;
//     for (size_t i = 0; i < num_public_inputs; ++i) {
//         sigma_1[i] = work_root;
//         fr::__mul(work_root, state.key->small_domain.root, work_root);
//         public_inputs.push_back(wires[i]);
//     }

//     state.execute_preamble_round();
//     state.execute_first_round();
//     state.execute_second_round();

//     fr::field_t beta = fr::serialize_from_buffer(state.transcript.get_challenge("beta").begin());
//     fr::field_t gamma = fr::serialize_from_buffer(state.transcript.get_challenge("gamma").begin());

//     state.key->z.fft(state.key->small_domain);
//     fr::field_t target_delta =
//         waffle::compute_public_input_delta(public_inputs, beta, gamma, state.key->small_domain.root);

//     fr::field_t T0;
//     fr::field_t T1;
//     fr::field_t T2;

//     fr::__invert(target_delta, target_delta);
//     fr::__mul(target_delta, state.key->z[circuit_size - 1], T0);
//     fr::print(fr::from_montgomery_form(T0));
//     // check that the max degree of our quotient polynomial is 3n
//     EXPECT_EQ(fr::eq(target_delta, state.key->z[circuit_size - 1]), true);
// }

TEST(test_public_inputs, compute_delta)
{
    constexpr uint32_t circuit_size = 256;
    constexpr size_t num_public_inputs = 7;

    evaluation_domain domain(circuit_size);

    std::vector<fr::field_t> left;
    std::vector<fr::field_t> right;
    std::vector<fr::field_t> sigma_1;
    std::vector<fr::field_t> sigma_2;

    fr::field_t work_root = fr::one;
    for (size_t i = 0; i < circuit_size; ++i) {
        fr::field_t temp = get_pseudorandom_element();
        left.push_back(temp);
        right.push_back(temp);
        sigma_1.push_back(fr::mul(fr::coset_generators[0], work_root));
        sigma_2.push_back(work_root);
        work_root = fr::mul(work_root, domain.root);
    }

    fr::field_t beta = get_pseudorandom_element();
    fr::field_t gamma = get_pseudorandom_element();
    fr::field_t root = domain.root;
    const auto compute_grand_product = [root, beta, gamma](std::vector<fr::field_t>& left,
                                                           std::vector<fr::field_t>& right,
                                                           std::vector<fr::field_t>& sigma_1,
                                                           std::vector<fr::field_t>& sigma_2) {
        fr::field_t numerator = fr::one;
        fr::field_t denominator = fr::one;
        fr::field_t work_root = fr::one;
        for (size_t i = 0; i < circuit_size; ++i) {
            fr::field_t T0 = fr::add(left[i], gamma);
            fr::field_t T1 = fr::add(right[i], gamma);

            fr::field_t T2 = fr::mul(work_root, beta);
            fr::field_t T3 = fr::mul(fr::coset_generators[0], T2);

            fr::field_t T4 = fr::add(T0, T2);
            fr::field_t T5 = fr::add(T1, T3);
            fr::field_t T6 = fr::mul(T4, T5);

            numerator = fr::mul(numerator, T6);

            fr::field_t T7 = fr::add(T0, fr::mul(sigma_1[i], beta));
            fr::field_t T8 = fr::add(T1, fr::mul(sigma_2[i], beta));
            fr::field_t T9 = fr::mul(T7, T8);
            denominator = fr::mul(denominator, T9);
            work_root = fr::mul(work_root, root);
        }

        fr::__invert(denominator, denominator);

        fr::field_t product = fr::mul(numerator, denominator);
        return product;
    };

    fr::field_t init_result = compute_grand_product(left, right, sigma_1, sigma_2);

    EXPECT_EQ(fr::eq(init_result, fr::one), true);

    work_root = fr::one;
    for (size_t i = 0; i < num_public_inputs; ++i) {
        sigma_1[i] = work_root;
        work_root = fr::mul(work_root, domain.root);
    }

    fr::field_t modified_result = compute_grand_product(left, right, sigma_1, sigma_2);

    std::vector<fr::field_t> public_inputs;
    for (size_t i = 0; i < num_public_inputs; ++i) {
        public_inputs.push_back(left[i]);
    }
    fr::field_t target_delta = waffle::compute_public_input_delta(public_inputs, beta, gamma, domain.root);

    EXPECT_EQ(fr::eq(modified_result, target_delta), true);
}
} // namespace