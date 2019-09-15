#include <gtest/gtest.h>

#include <barretenberg/waffle/waffle.hpp>


namespace
{
void generate_test_data(waffle::circuit_state& state, fr::field_t* data)
{
    size_t n = state.n;

    state.w_l = &data[0];
    state.w_r = &data[n];
    state.w_o = &data[2 * n];
    state.z_1 = &data[3 * n];
    state.z_2 = &data[4 * n + 1];
    state.q_c = &data[5 * n + 2];
    state.q_l = &data[6 * n + 2];
    state.q_r = &data[7 * n + 2];
    state.q_o = &data[8 * n + 2];
    state.q_m = &data[9 * n + 2];
    state.sigma_1 = &data[10 * n + 2];
    state.sigma_2 = &data[11 * n + 2];
    state.sigma_3 = &data[12 * n + 2];
    state.s_id = &data[13 * n + 2];
    state.t = &data[14 * n + 2];

    state.product_1 = &data[17 * n + 5];
    state.product_2 = &data[18 * n + 6];
    state.product_3 = &data[19 * n + 7];
    state.permutation_product = &data[20 * n + 8];
    state.w_l_lagrange_base = state.t;
    state.w_r_lagrange_base = &state.t[n + 1];
    state.w_o_lagrange_base = &state.t[2 * n + 2];

    fr::random_element(state.beta);
    fr::random_element(state.gamma);
    fr::random_element(state.alpha);
    fr::sqr(state.alpha, state.alpha_squared);
    fr::mul(state.alpha_squared, state.alpha, state.alpha_cubed);
    // create some constraints that satisfy our arithmetic circuit relation
    fr::field_t one;
    fr::field_t zero;
    fr::field_t minus_one;
    fr::one(one);
    fr::neg(one, minus_one);
    fr::zero(zero);
    fr::field_t T0;
    // even indices = mul gates, odd incides = add gates
    for (size_t i = 0; i < n / 4; ++i)
    {
        fr::random_element(state.w_l[2 * i]);
        fr::random_element(state.w_r[2 * i]);
        fr::mul(state.w_l[2 * i], state.w_r[2 * i], state.w_o[2 * i]);
        fr::copy(zero, state.q_l[2 * i]);
        fr::copy(zero, state.q_r[2 * i]);
        fr::copy(minus_one, state.q_o[2 * i]);
        fr::copy(zero, state.q_c[2 * i]);
        fr::copy(one, state.q_m[2 * i]);

        fr::random_element(state.w_l[2 * i + 1]);
        fr::random_element(state.w_r[2 * i + 1]);
        fr::random_element(state.w_o[2 * i + 1]);

        fr::add(state.w_l[2 * i + 1], state.w_r[2 * i + 1], T0);
        fr::add(T0, state.w_o[2 * i + 1], state.q_c[2 * i + 1]);
        fr::neg(state.q_c[2 * i + 1], state.q_c[2 * i + 1]);
        fr::one(state.q_l[2 * i + 1]);
        fr::one(state.q_r[2 * i + 1]);
        fr::one(state.q_o[2 * i + 1]);
        fr::zero(state.q_m[2 * i + 1]);
    }

    size_t shift = n / 2;
    polynomials::copy_polynomial(state.w_l, state.w_l + shift, shift, shift);
    polynomials::copy_polynomial(state.w_r, state.w_r + shift, shift, shift);
    polynomials::copy_polynomial(state.w_o, state.w_o + shift, shift, shift);
    polynomials::copy_polynomial(state.q_m, state.q_m + shift, shift, shift);
    polynomials::copy_polynomial(state.q_l, state.q_l + shift, shift, shift);
    polynomials::copy_polynomial(state.q_r, state.q_r + shift, shift, shift);
    polynomials::copy_polynomial(state.q_o, state.q_o + shift, shift, shift);
    polynomials::copy_polynomial(state.q_c, state.q_c + shift, shift, shift);


    fr::field_t T1 = { .data = { shift + 1, 0, 0, 0 } };
    fr::field_t n_mont = { .data = { n, 0, 0, 0 } } ;
    fr::one(T0);
    fr::to_montgomery_form(T1, T1);
    fr::to_montgomery_form(n_mont, n_mont);
    for (size_t i = 0; i < n / 2; ++i)
    {
        fr::copy(T0, state.sigma_1[shift + i]);
        fr::copy(T0, state.sigma_2[shift + i]);
        fr::copy(T0, state.sigma_3[shift + i]);

        fr::add(state.sigma_2[shift + i], n_mont, state.sigma_2[shift + i]);
        fr::add(state.sigma_3[shift + i], n_mont, state.sigma_3[shift + i]);
        fr::add(state.sigma_3[shift + i], n_mont, state.sigma_3[shift + i]);

        fr::copy(T1, state.sigma_1[i]);
        fr::copy(T1, state.sigma_2[i]);
        fr::copy(T1, state.sigma_3[i]);

        fr::add(state.sigma_2[i], n_mont, state.sigma_2[i]);
        fr::add(state.sigma_3[i], n_mont, state.sigma_3[i]);
        fr::add(state.sigma_3[i], n_mont, state.sigma_3[i]);

        fr::add(T0, one, T0);
        fr::add(T1, one, T1);
    }

    fr::zero(state.w_l[n-1]);
    fr::zero(state.w_r[n-1]);
    fr::zero(state.w_o[n-1]);
    fr::zero(state.q_c[n-1]);
    fr::zero(state.w_l[shift-1]);
    fr::zero(state.w_r[shift-1]);
    fr::zero(state.w_o[shift-1]);
    fr::zero(state.q_c[shift-1]);

    fr::field_t T2 = { .data = { shift, 0, 0, 0 } };
    fr::to_montgomery_form(T2, T2);
    fr::copy(T2, state.sigma_1[shift-1]);
    fr::copy(T2, state.sigma_2[shift-1]);
    fr::copy(T2, state.sigma_3[shift-1]);
    fr::add(state.sigma_2[shift-1], n_mont, state.sigma_2[shift-1]);
    fr::add(state.sigma_3[shift-1], n_mont, state.sigma_3[shift-1]);
    fr::add(state.sigma_3[shift-1], n_mont, state.sigma_3[shift-1]);

    fr::zero(state.sigma_1[n-1]);
    fr::zero(state.sigma_2[n-1]);
    fr::zero(state.sigma_3[n-1]);
    fr::zero(state.q_l[n - 1]);
    fr::zero(state.q_r[n - 1]);
    fr::zero(state.q_o[n - 1]);
    fr::zero(state.q_m[n - 1]);

    // fr::zero(state.q_c[n-1]);
}
}

// polynomials::evaluation_domain glob_domain = polynomials::get_domain(n);


TEST(waffle, compute_quotient_polynomial_new)
{
    size_t n = 256;

    waffle::circuit_state state;
    state.small_domain = polynomials::get_domain(n);
    state.mid_domain = polynomials::get_domain(2 * n);
    state.large_domain = polynomials::get_domain(4 * n);
    state.n = n;
    fr::random_element(state.beta);
    fr::random_element(state.gamma);
    fr::random_element(state.alpha);
    fr::field_t data[28 * n + 2];

    // fr::field_t scratch_space[19 * n + 8];
    // fr::field_t scratch_space[70 * n + 4];
    fr::field_t* scratch_space = (fr::field_t*)(aligned_alloc(32, sizeof(fr::field_t) * (70 * n + 8)));

    waffle::fft_pointers ffts;
    ffts.scratch_memory = scratch_space;
    generate_test_data(state, data);

    fr::field_t x;
    fr::random_element(x);
    srs::plonk_srs srs;
    g1::affine_element monomials[6 * n + 2];
    monomials[0] = g1::affine_one();

    for (size_t i = 1; i < 3 * n; ++i)
    {
        monomials[i] = g1::group_exponentiation(monomials[i-1], x);
    }
    scalar_multiplication::generate_pippenger_point_table(monomials, monomials, 3 * n);
    srs.monomials = monomials;
    srs.degree = n;

    waffle::compute_quotient_polynomial_new(state, ffts, srs);
    

    // for (size_t i = 0; i < 4 * n; ++i)
    // {
    //     fr::print(ffts.quotient_poly[i]);
    // }
    // check that the max degree of our quotient polynomial is 3n
    for (size_t i = 3 * n; i < 4 * n; ++i)
    {
        for (size_t j = 0; j < 4; ++j)
        {
            EXPECT_EQ(ffts.quotient_poly[i].data[j], 0);
        }
    }

    free(scratch_space);
}

TEST(waffle, compute_quotient_polynomial_new_two)
{
    size_t n = 256;

    waffle::circuit_state state;
    state.small_domain = polynomials::get_domain(n);
    state.mid_domain = polynomials::get_domain(2 * n);
    state.large_domain = polynomials::get_domain(4 * n);
    state.n = n;
    fr::random_element(state.beta);
    fr::random_element(state.gamma);
    fr::random_element(state.alpha);
    fr::field_t data[28 * n + 2];

    // fr::field_t scratch_space[19 * n + 8];
    // fr::field_t scratch_space[70 * n + 4];
    fr::field_t* scratch_space = (fr::field_t*)(aligned_alloc(32, sizeof(fr::field_t) * (70 * n + 8)));

    waffle::fft_pointers ffts;
    ffts.scratch_memory = scratch_space;
    generate_test_data(state, data);

    fr::field_t x;
    fr::random_element(x);
    srs::plonk_srs srs;
    g1::affine_element monomials[6 * n + 2];
    monomials[0] = g1::affine_one();

    for (size_t i = 1; i < 3 * n; ++i)
    {
        monomials[i] = g1::group_exponentiation(monomials[i-1], x);
    }
    scalar_multiplication::generate_pippenger_point_table(monomials, monomials, 3 * n);
    srs.monomials = monomials;
    srs.degree = n;

    waffle::compute_quotient_polynomial_new(state, ffts, srs);
    

    // for (size_t i = 0; i < 4 * n; ++i)
    // {
    //     fr::print(ffts.quotient_poly[i]);
    // }
    // check that the max degree of our quotient polynomial is 3n
    for (size_t i = 3 * n; i < 4 * n; ++i)
    {
        for (size_t j = 0; j < 4; ++j)
        {
            EXPECT_EQ(ffts.quotient_poly[i].data[j], 0);
        }
    }

    free(scratch_space);
}

TEST(waffle, compute_z_coefficients)
{
    size_t n = 16;

    fr::field_t data[16 * n + 2];
    waffle::circuit_state state;
    state.small_domain = polynomials::get_domain(n);
    state.mid_domain = polynomials::get_domain(2 * n);
    state.large_domain = polynomials::get_domain(4 * n);

    state.w_l = &data[0];
    state.w_r = &data[n];
    state.w_o = &data[2 * n];
    state.z_1 = &data[3 * n];
    state.z_2 = &data[4 * n + 1];
    state.sigma_1 = &data[5 * n + 2];
    state.sigma_2 = &data[6 * n + 2];
    state.sigma_3 = &data[7 * n + 2];
    state.s_id = &data[8 * n + 2];
    state.product_1 = &data[9 * n + 2];
    state.product_2 = &data[10 * n + 2];
    state.product_3 = &data[11 * n + 2];
    state.w_l_lagrange_base = &data[12 * n + 2];
    state.w_r_lagrange_base = &data[13 * n + 2];
    state.w_o_lagrange_base = &data[14 * n + 2];
    state.permutation_product = &data[15 * n + 2];
    state.n = n;

    fr::one(state.gamma);
    fr::add(state.gamma, state.gamma, state.beta);

    fr::field_t i_mont;
    fr::field_t one;
    fr::zero(i_mont);
    fr::one(one);
    for (size_t i = 0; i < n; ++i)
    {
        fr::copy(i_mont, state.w_l[i]);
        fr::copy(state.w_l[i], state.w_l_lagrange_base[i]);
        fr::add(state.w_l[i], state.w_l[i], state.w_r[i]);
        fr::copy(state.w_r[i], state.w_r_lagrange_base[i]);
        fr::add(state.w_l[i], state.w_r[i], state.w_o[i]);
        fr::copy(state.w_o[i], state.w_o_lagrange_base[i]);
        fr::add(i_mont, one, i_mont);

        fr::one(state.sigma_1[i]);
        fr::add(state.sigma_1[i], state.sigma_1[i], state.sigma_2[i]);
        fr::add(state.sigma_1[i], state.sigma_2[i], state.sigma_3[i]);
    }

    fr::field_t scratch_space[8 * n + 4];
    waffle::fft_pointers ffts;
    ffts.z_1_poly = &scratch_space[0];
    ffts.z_2_poly = &scratch_space[4 * n + 4];
    waffle::compute_z_coefficients(state, ffts);

    size_t z_1_evaluations[n];
    size_t z_2_evaluations[n];
    z_1_evaluations[0] = 1;
    z_2_evaluations[0] = 1;
    for (size_t i = 0; i < n - 1; ++i)
    {
        uint64_t product_1 = i + 3;
        uint64_t product_2 = (2 * i) + 5;
        uint64_t product_3 = (3 * i) + 7;

        uint64_t s_id = i + 1;
        uint64_t id_1 = i + (2 * s_id) + 1;
        uint64_t id_2 = (2 * i) + (2 * s_id) + 1 + (2 * n);
        uint64_t id_3 = (3 * i) + (2 * s_id) + 1 + (4 * n);
        uint64_t id_product = id_1 * id_2 * id_3;
        uint64_t sigma_product = product_1 * product_2 * product_3;

        z_1_evaluations[i + 1] = z_1_evaluations[i] * id_product;
        z_2_evaluations[i + 1] = z_2_evaluations[i] * sigma_product;
        // fr::field_t product_1_result;
        // fr::field_t product_2_result;
        // fr::field_t product_3_result;
        // fr::from_montgomery_form(state.product_1[i], product_1_result);
        // fr::from_montgomery_form(state.product_2[i], product_2_result);
        // fr::from_montgomery_form(state.product_3[i], product_3_result);
        // EXPECT_EQ(product_1_result.data[0], product_1);
        // EXPECT_EQ(product_2_result.data[0], product_2);
        // EXPECT_EQ(product_3_result.data[0], product_3);
    }

    fr::field_t work_root;
    fr::field_t z_1_expected;
    fr::field_t z_2_expected;
    fr::one(work_root);

    for (size_t i = 0; i < n; ++i)
    {
        polynomials::eval(state.z_1, work_root, n, z_1_expected);
        polynomials::eval(state.z_2, work_root, n, z_2_expected);
        fr::from_montgomery_form(z_1_expected, z_1_expected);
        fr::from_montgomery_form(z_2_expected, z_2_expected);
        fr::mul(work_root, state.small_domain.root, work_root);
        EXPECT_EQ(z_1_expected.data[0], z_1_evaluations[i]);
        EXPECT_EQ(z_2_expected.data[0], z_2_evaluations[i]);
    }

    fr::field_t z_coeffs[4 * n];
    fr::field_t z_coeffs_copy[4 * n + 4];
    polynomials::copy_polynomial(state.z_1, z_coeffs, n, 4 * n);
    polynomials::fft(z_coeffs, state.large_domain);
    polynomials::copy_polynomial(z_coeffs, z_coeffs_copy, 4 * n, 4 * n);

    fr::copy(z_coeffs_copy[0], z_coeffs_copy[4 * n]);
    fr::copy(z_coeffs_copy[1], z_coeffs_copy[4 * n + 1]);
    fr::copy(z_coeffs_copy[2], z_coeffs_copy[4 * n + 2]);
    fr::copy(z_coeffs_copy[3], z_coeffs_copy[4 * n + 3]);

    polynomials::ifft(z_coeffs, state.large_domain);

    fr::field_t* shifted_z = &z_coeffs_copy[4];

    polynomials::ifft(shifted_z, state.large_domain);


    fr::field_t x;
    fr::field_t shifted_x;
    fr::random_element(x);
    fr::mul(x, state.small_domain.root, shifted_x);

    fr::field_t z_eval;
    fr::field_t shifted_z_eval;
    polynomials::eval(z_coeffs, shifted_x, state.small_domain.size, z_eval);
    polynomials::eval(shifted_z, x, state.small_domain.size, shifted_z_eval);

    for (size_t i = 0; i < 4; ++i)
    {
        EXPECT_EQ(z_eval.data[i], shifted_z_eval.data[i]);
    }

}

TEST(waffle, compute_wire_coefficients)
{
    size_t n = 256;
    fr::field_t data[13 * n];

    waffle::circuit_state state;
    state.small_domain = polynomials::get_domain(n);
    state.mid_domain = polynomials::get_domain(2 * n);
    state.large_domain = polynomials::get_domain(4 * n);

    state.w_l = &data[0];
    state.w_r = &data[n];
    state.w_o = &data[2 * n];
    state.z_1 = &data[6 * n];
    state.z_2 = &data[7 * n];
    state.t = &data[8 * n];
    state.sigma_1 = &data[3 * n];
    state.sigma_2 = &data[4 * n];
    state.sigma_3 = &data[5 * n];
    state.w_l_lagrange_base = &data[9 * n];
    state.w_r_lagrange_base = &data[10 * n];
    state.w_o_lagrange_base = &data[11 * n];
    state.permutation_product = &data[12 * n];
    state.n = n;

    fr::field_t w_l_reference[n];
    fr::field_t w_r_reference[n];
    fr::field_t w_o_reference[n];
    fr::field_t i_mont;
    fr::field_t one;
    fr::zero(i_mont);
    fr::one(one);
    for (size_t i = 0; i < n; ++i)
    {
        fr::copy(i_mont, state.w_l[i]);
        fr::add(state.w_l[i], state.w_l[i], state.w_r[i]);
        fr::add(state.w_l[i], state.w_r[i], state.w_o[i]);
        fr::add(i_mont, one, i_mont);

        fr::one(state.sigma_1[i]);
        fr::add(state.sigma_1[i], state.sigma_1[i], state.sigma_2[i]);
        fr::add(state.sigma_1[i], state.sigma_2[i], state.sigma_3[i]);

        fr::copy(state.w_l[i], w_l_reference[i]);
        fr::copy(state.w_r[i], w_r_reference[i]);
        fr::copy(state.w_o[i], w_o_reference[i]);
    }

    fr::field_t scratch_space[24 * n + 8];
    waffle::fft_pointers ffts;
    ffts.w_l_poly = &scratch_space[0];
    ffts.w_r_poly = &scratch_space[4 * n];
    ffts.w_o_poly = &scratch_space[8 * n];
    ffts.identity_poly = &scratch_space[12 * n];
    ffts.z_1_poly = &scratch_space[16 * n];
    ffts.z_2_poly = &scratch_space[20 * n + 4];
    waffle::compute_wire_coefficients(state, ffts);

    fr::field_t work_root;
    fr::field_t w_l_expected;
    fr::field_t w_r_expected;
    fr::field_t w_o_expected;
    fr::one(work_root);

    for (size_t i = 0; i < n; ++i)
    {
        polynomials::eval(state.w_l, work_root, n, w_l_expected);
        polynomials::eval(state.w_r, work_root, n, w_r_expected);
        polynomials::eval(state.w_o, work_root, n, w_o_expected);
        fr::mul(work_root, state.small_domain.root, work_root);
        for (size_t j = 0; j < 4; ++j)
        {
            EXPECT_EQ(w_l_reference[i].data[j], w_l_expected.data[j]);
            EXPECT_EQ(w_r_reference[i].data[j], w_r_expected.data[j]);
            EXPECT_EQ(w_o_reference[i].data[j], w_o_expected.data[j]);
        }
    }
}

TEST(waffle, compute_wire_commitments)
{
    size_t n = 256;

    waffle::circuit_state state;
    state.small_domain = polynomials::get_domain(n);
    state.mid_domain = polynomials::get_domain(2 * n);
    state.large_domain = polynomials::get_domain(4 * n);
    state.n = n;
    fr::random_element(state.beta);
    fr::random_element(state.gamma);
    fr::random_element(state.alpha);
    fr::field_t data[25 * n];

    fr::field_t scratch_space[12 * n];

    waffle::fft_pointers ffts;
    ffts.scratch_memory = scratch_space;
    ffts.w_l_poly = &ffts.scratch_memory[0];
    ffts.w_r_poly = &ffts.scratch_memory[4 * n];
    ffts.w_o_poly = &ffts.scratch_memory[8 * n];
    generate_test_data(state, data);

    fr::field_t x;
    fr::random_element(x);
    srs::plonk_srs srs;
    g1::affine_element monomials[2 * n + 1];
    monomials[0] = g1::affine_one();

    for (size_t i = 1; i < n; ++i)
    {
        monomials[i] = g1::group_exponentiation(monomials[i-1], x);
    }
    scalar_multiplication::generate_pippenger_point_table(monomials, monomials, n);
    srs.monomials = monomials;
    srs.degree = n;

    waffle::compute_wire_coefficients(state, ffts);

    fr::field_t w_l_copy[n];
    fr::field_t w_r_copy[n];
    fr::field_t w_o_copy[n];
    polynomials::copy_polynomial(state.w_l, w_l_copy, n, n);
    polynomials::copy_polynomial(state.w_r, w_r_copy, n, n);
    polynomials::copy_polynomial(state.w_o, w_o_copy, n, n);

    fr::field_t w_l_eval;
    fr::field_t w_r_eval;
    fr::field_t w_o_eval;

    // TODO: our scalar mul algorithm currently doesn't convert values out of montgomery representation
    // do we want to do this? is expensive, can probably work around and leave everything in mont form?
    // we can 'normalize' our evaluation check by adding an extra factor of R to each scalar
    for (size_t i = 0; i < n; ++i)
    {
        fr::to_montgomery_form(w_l_copy[i], w_l_copy[i]);
        fr::to_montgomery_form(w_r_copy[i], w_r_copy[i]);
        fr::to_montgomery_form(w_o_copy[i], w_o_copy[i]);
    }

    polynomials::eval(w_l_copy, x, n, w_l_eval);
    polynomials::eval(w_r_copy, x, n, w_r_eval);
    polynomials::eval(w_o_copy, x, n, w_o_eval);


    waffle::compute_wire_commitments(state, srs);

    g1::affine_element generator = g1::affine_one();
    g1::affine_element expected_w_l = g1::group_exponentiation(generator, w_l_eval);
    g1::affine_element expected_w_r = g1::group_exponentiation(generator, w_r_eval);
    g1::affine_element expected_w_o = g1::group_exponentiation(generator, w_o_eval);

    for (size_t i = 0; i < 1; ++i)
    {
        EXPECT_EQ(state.W_L.x.data[i], expected_w_l.x.data[i]);
        EXPECT_EQ(state.W_L.y.data[i], expected_w_l.y.data[i]);
        EXPECT_EQ(state.W_R.x.data[i], expected_w_r.x.data[i]);
        EXPECT_EQ(state.W_R.y.data[i], expected_w_r.y.data[i]);
        EXPECT_EQ(state.W_O.x.data[i], expected_w_o.x.data[i]);
        EXPECT_EQ(state.W_O.y.data[i], expected_w_o.y.data[i]);
    }
}

TEST(waffle, compute_z_commitments)
{
    size_t n = 256;

    waffle::circuit_state state;
    state.small_domain = polynomials::get_domain(n);
    state.mid_domain = polynomials::get_domain(2 * n);
    state.large_domain = polynomials::get_domain(4 * n);
    state.n = n;
    fr::random_element(state.beta);
    fr::random_element(state.gamma);
    fr::random_element(state.alpha);

    fr::field_t* data = (fr::field_t*)(aligned_alloc(32, sizeof(fr::field_t) * (28 * n + 2)));
    fr::field_t* scratch_space = (fr::field_t*)(aligned_alloc(32, sizeof(fr::field_t) * (70 * n + 8)));
    g1::affine_element* monomials = (g1::affine_element*)(aligned_alloc(32, sizeof(g1::affine_element) * (6 * n + 2)));

    // fr::field_t data[25 * n];

    // fr::field_t scratch_space[20 * n];

    waffle::fft_pointers ffts;
    ffts.scratch_memory = scratch_space;
    ffts.w_l_poly = &ffts.scratch_memory[0];
    ffts.w_r_poly = &ffts.scratch_memory[4 * n];
    ffts.w_o_poly = &ffts.scratch_memory[8 * n];
    ffts.z_1_poly = &ffts.scratch_memory[12 * n];
    ffts.z_2_poly = &ffts.scratch_memory[16 * n];
    generate_test_data(state, data);

    fr::field_t x;
    fr::random_element(x);
    srs::plonk_srs srs;
    // g1::affine_element monomials[2 * n + 1];
    monomials[0] = g1::affine_one();

    for (size_t i = 1; i < n; ++i)
    {
        monomials[i] = g1::group_exponentiation(monomials[i-1], x);
    }
    scalar_multiplication::generate_pippenger_point_table(monomials, monomials, n);
    srs.monomials = monomials;
    srs.degree = n;

    waffle::compute_wire_coefficients(state, ffts);
    waffle::compute_z_coefficients(state, ffts);

    fr::field_t z_1_copy[n];
    fr::field_t z_2_copy[n];
    polynomials::copy_polynomial(state.z_1, z_1_copy, n, n);
    polynomials::copy_polynomial(state.z_2, z_2_copy, n, n);

    fr::field_t z_1_eval;
    fr::field_t z_2_eval;

    // TODO: our scalar mul algorithm currently doesn't convert values out of montgomery representation
    // do we want to do this? is expensive, can probably work around and leave everything in mont form?
    // we can 'normalize' our evaluation check by adding an extra factor of R to each scalar
    for (size_t i = 0; i < n; ++i)
    {
        fr::to_montgomery_form(z_1_copy[i], z_1_copy[i]);
        fr::to_montgomery_form(z_2_copy[i], z_2_copy[i]);
    }

    polynomials::eval(z_1_copy, x, n, z_1_eval);
    polynomials::eval(z_2_copy, x, n, z_2_eval);

    waffle::compute_z_commitments(state, srs);

    g1::affine_element generator = g1::affine_one();
    g1::affine_element expected_z_1 = g1::group_exponentiation(generator, z_1_eval);
    g1::affine_element expected_z_2 = g1::group_exponentiation(generator, z_2_eval);

    for (size_t i = 0; i < 1; ++i)
    {
        EXPECT_EQ(state.Z_1.x.data[i], expected_z_1.x.data[i]);
        EXPECT_EQ(state.Z_1.y.data[i], expected_z_1.y.data[i]);
        EXPECT_EQ(state.Z_2.x.data[i], expected_z_2.x.data[i]);
        EXPECT_EQ(state.Z_2.y.data[i], expected_z_2.y.data[i]);
    }

    free(monomials);
    free(scratch_space);
    free(data);
}

// TEST(waffle, compute_quotient_polynomial)
// {
//     size_t n = 256;
//     polynomials::evaluation_domain domain = polynomials::get_domain(n);

//     waffle::circuit_state state;
//     state.n = n;
//     fr::random_element(state.beta);
//     fr::random_element(state.gamma);
//     fr::random_element(state.alpha);
//     fr::field_t data[20 * n + 2];

//     // fr::field_t scratch_space[19 * n + 8];
//     fr::field_t scratch_space[70 * n + 8];

//     waffle::fft_pointers ffts;
//     ffts.scratch_memory = scratch_space;
//     generate_test_data(state, data);

//     fr::field_t x;
//     fr::random_element(x);
//     srs::plonk_srs srs;
//     g1::affine_element monomials[6 * n + 1];
//     monomials[0] = g1::affine_one();

//     for (size_t i = 1; i < 3 * n; ++i)
//     {
//         monomials[i] = g1::group_exponentiation(monomials[i-1], x);
//     }
//     scalar_multiplication::generate_pippenger_point_table(monomials, monomials, 3 * n);
//     srs.monomials = monomials;
//     srs.degree = n;

//     waffle::compute_quotient_polynomial(state, domain, ffts, srs);
    

//     // for (size_t i = 0; i < 4 * n; ++i)
//     // {
//     //     fr::print(ffts.quotient_poly[i]);
//     // }
//     // check that the max degree of our quotient polynomial is 3n
//     for (size_t i = 3 * n; i < 4 * n; ++i)
//     {
//         for (size_t j = 0; j < 4; ++j)
//         {
//             EXPECT_EQ(ffts.quotient_poly[i].data[j], 0);
//         }
//     }
// }

TEST(waffle, compute_linearisation_coefficients)
{
    size_t n = 256;

    waffle::circuit_state state;
    state.small_domain = polynomials::get_domain(n);
    state.mid_domain = polynomials::get_domain(2 * n);
    state.large_domain = polynomials::get_domain(4 * n);

    state.n = n;

    // fr::field_t data[28 * n + 2];
    fr::field_t* data = (fr::field_t*)(aligned_alloc(32, sizeof(fr::field_t) * (28 * n + 2)));
    fr::field_t* scratch_space = (fr::field_t*)(aligned_alloc(32, sizeof(fr::field_t) * (70 * n + 8)));
    g1::affine_element* monomials = (g1::affine_element*)(aligned_alloc(32, sizeof(g1::affine_element) * (6 * n + 2)));

   // fr::field_t scratch_space[18 * n + 8];
    //fr::field_t* =  scratch_space[70 * n + 8];
    waffle::fft_pointers ffts;
    ffts.scratch_memory = scratch_space;
    // generate_simple_test_data(state, data);
    generate_test_data(state, data);

    fr::field_t x;
    fr::random_element(x);
    srs::plonk_srs srs;
    // g1::affine_element monomials[6 * n + 1];
    monomials[0] = g1::affine_one();

    for (size_t i = 1; i < 3 * n; ++i)
    {
        monomials[i] = g1::group_exponentiation(monomials[i-1], x);
    }
    scalar_multiplication::generate_pippenger_point_table(monomials, monomials, 3 * n);
    srs.monomials = monomials;
    srs.degree = n;

    waffle::compute_quotient_polynomial_new(state, ffts, srs);
    fr::random_element(state.z);
    // fr::field_t foobar[4 * n];
    state.linear_poly = &ffts.scratch_memory[4 * n];

    waffle::compute_linearisation_coefficients(state, ffts);

    polynomials::lagrange_evaluations lagrange_evals = polynomials::get_lagrange_evaluations(state.z, state.small_domain);

    fr::field_t rhs;
    fr::field_t lhs;
    fr::field_t T0;
    fr::field_t T1;
    fr::field_t T2;
    fr::mul(lagrange_evals.l_n_minus_1, state.alpha_squared, T0);
    fr::mul(T0, state.alpha_cubed, T0);

    fr::sub(state.z_1_shifted_eval, state.z_2_shifted_eval, T1);
    fr::mul(T0, T1, T0);

    fr::mul(state.z_1_shifted_eval, state.alpha_squared, T1);
    fr::mul(state.z_2_shifted_eval, state.alpha_cubed, T2);
    fr::add(T1, T2, T1);
    fr::sub(T0, T1, T0);
    fr::add(T0, state.linear_eval, rhs);

    fr::mul(state.t_eval, lagrange_evals.vanishing_poly, lhs);

    for (size_t i = 0; i < 4; ++i)
    {
        EXPECT_EQ(lhs.data[i], rhs.data[i]);
    }
    free(scratch_space);
    free(data);
    free(monomials);
}

