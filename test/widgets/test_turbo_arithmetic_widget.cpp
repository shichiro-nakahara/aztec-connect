#include <gtest/gtest.h>

#include <barretenberg/waffle/proof_system/widgets/turbo_arithmetic_widget.hpp>
#include <barretenberg/waffle/composer/standard_composer.hpp>

#include <iostream>
#include <memory>

#include "../test_helpers.hpp"

using namespace barretenberg;
using namespace waffle;

TEST(turbo_arithmetic_widget, quotient_polynomial_satisfiability)
{
    const size_t num_gates = 4;
    waffle::CircuitFFTState circuit_state(num_gates);

    std::shared_ptr<program_witness> witness = std::make_shared<program_witness>();
    std::shared_ptr<proving_key> key = std::make_shared<proving_key>(num_gates);

    polynomial w_1(num_gates);
    polynomial w_2(num_gates);
    polynomial w_3(num_gates);
    polynomial w_4(num_gates);

    polynomial q_1(num_gates);
    polynomial q_2(num_gates);
    polynomial q_3(num_gates);
    polynomial q_4(num_gates);
    polynomial q_4_next(num_gates);
    polynomial q_m(num_gates);
    polynomial q_c(num_gates);
    polynomial q_arith(num_gates);

    for (size_t i = 0; i < num_gates; ++i)
    {
        w_1[i] = (fr::random_element());
        w_2[i] = (fr::random_element());
        w_3[i] = (fr::random_element());
        w_4[i] = (fr::random_element());

        q_1[i] = (fr::random_element());
        q_2[i] = (fr::random_element());
        q_3[i] = (fr::random_element());
        q_4[i] = (fr::random_element());
        q_m[i] = (fr::random_element());
        q_4_next[i] = (fr::zero);

        fr::field_t T0;
        fr::field_t T1;
        fr::__mul(w_1[i], w_2[i], T0);
        fr::__mul(T0, q_m[i], T0);

        fr::__mul(w_1[i], q_1[i], T1);
        fr::__add(T0, T1, T0);

        fr::__mul(w_2[i], q_2[i], T1);
        fr::__add(T0, T1, T0);

        fr::__mul(w_3[i], q_3[i], T1);
        fr::__add(T0, T1, T0);

        fr::__mul(w_4[i], q_4[i], T1);
        fr::__add(T0, T1, T0);

        fr::__neg(T0, T0);
        q_c[i] = (T0);
        q_arith[i] = fr::one;

    }

    polynomial w_1_fft = polynomial(w_1, 4 * num_gates + 4);
    polynomial w_2_fft = polynomial(w_2, 4 * num_gates + 4);
    polynomial w_3_fft = polynomial(w_3, 4 * num_gates + 4);
    polynomial w_4_fft = polynomial(w_4, 4 * num_gates + 4);

    w_1_fft.ifft(key->small_domain);
    w_2_fft.ifft(key->small_domain);
    w_3_fft.ifft(key->small_domain);
    w_4_fft.ifft(key->small_domain);

    w_1_fft.coset_fft(key->large_domain);
    w_2_fft.coset_fft(key->large_domain);
    w_3_fft.coset_fft(key->large_domain);
    w_4_fft.coset_fft(key->large_domain);

    w_1_fft.add_lagrange_base_coefficient(w_1_fft[0]);
    w_1_fft.add_lagrange_base_coefficient(w_1_fft[1]);
    w_1_fft.add_lagrange_base_coefficient(w_1_fft[2]);
    w_1_fft.add_lagrange_base_coefficient(w_1_fft[3]);
    w_2_fft.add_lagrange_base_coefficient(w_2_fft[0]);
    w_2_fft.add_lagrange_base_coefficient(w_2_fft[1]);
    w_2_fft.add_lagrange_base_coefficient(w_2_fft[2]);
    w_2_fft.add_lagrange_base_coefficient(w_2_fft[3]);
    w_3_fft.add_lagrange_base_coefficient(w_3_fft[0]);
    w_3_fft.add_lagrange_base_coefficient(w_3_fft[1]);
    w_3_fft.add_lagrange_base_coefficient(w_3_fft[2]);
    w_3_fft.add_lagrange_base_coefficient(w_3_fft[3]);
    w_4_fft.add_lagrange_base_coefficient(w_4_fft[0]);
    w_4_fft.add_lagrange_base_coefficient(w_4_fft[1]);
    w_4_fft.add_lagrange_base_coefficient(w_4_fft[2]);
    w_4_fft.add_lagrange_base_coefficient(w_4_fft[3]);

    witness->wires.insert({ "w_1", std::move(w_1) });
    witness->wires.insert({ "w_2", std::move(w_2) });
    witness->wires.insert({ "w_3", std::move(w_3) });
    witness->wires.insert({ "w_4", std::move(w_4) });

    witness->wire_ffts.insert({ "w_1_fft", std::move(w_1_fft) });
    witness->wire_ffts.insert({ "w_2_fft", std::move(w_2_fft) });
    witness->wire_ffts.insert({ "w_3_fft", std::move(w_3_fft) });
    witness->wire_ffts.insert({ "w_4_fft", std::move(w_4_fft) });

    polynomial q_1_fft(q_1, 4 * num_gates);
    polynomial q_2_fft(q_2, 4 * num_gates);
    polynomial q_3_fft(q_3, 4 * num_gates);
    polynomial q_4_fft(q_4, 4 * num_gates);
    polynomial q_4_next_fft(q_4_next, 4 * num_gates);
    polynomial q_m_fft(q_m, 4 * num_gates);
    polynomial q_c_fft(q_c, 4 * num_gates);
    polynomial q_arith_fft(q_arith, 4 * num_gates);

    q_1_fft.ifft(key->small_domain);
    q_2_fft.ifft(key->small_domain);
    q_3_fft.ifft(key->small_domain);
    q_4_fft.ifft(key->small_domain);
    q_4_next_fft.ifft(key->small_domain);
    q_m_fft.ifft(key->small_domain);
    q_c_fft.ifft(key->small_domain);
    q_arith_fft.ifft(key->small_domain);

    q_1_fft.coset_fft(key->large_domain);
    q_2_fft.coset_fft(key->large_domain);
    q_3_fft.coset_fft(key->large_domain);
    q_4_fft.coset_fft(key->large_domain);
    q_4_next_fft.coset_fft(key->large_domain);
    q_m_fft.coset_fft(key->large_domain);
    q_c_fft.coset_fft(key->large_domain);
    q_arith_fft.coset_fft(key->large_domain);

    key->constraint_selectors.insert({ "q_1", std::move(q_1) });
    key->constraint_selectors.insert({ "q_2", std::move(q_2) });
    key->constraint_selectors.insert({ "q_3", std::move(q_3) });
    key->constraint_selectors.insert({ "q_4", std::move(q_4) });
    key->constraint_selectors.insert({ "q_4_next", std::move(q_4_next) });
    key->constraint_selectors.insert({ "q_m", std::move(q_m) });
    key->constraint_selectors.insert({ "q_c", std::move(q_c) });
    key->constraint_selectors.insert({ "q_arith", std::move(q_arith) });

    key->constraint_selector_ffts.insert({ "q_1_fft", std::move(q_1_fft) });
    key->constraint_selector_ffts.insert({ "q_2_fft", std::move(q_2_fft) });
    key->constraint_selector_ffts.insert({ "q_3_fft", std::move(q_3_fft) });
    key->constraint_selector_ffts.insert({ "q_4_fft", std::move(q_4_fft) });
    key->constraint_selector_ffts.insert({ "q_4_next_fft", std::move(q_4_next_fft) });
    key->constraint_selector_ffts.insert({ "q_m_fft", std::move(q_m_fft) });
    key->constraint_selector_ffts.insert({ "q_c_fft", std::move(q_c_fft) });
    key->constraint_selector_ffts.insert({ "q_arith_fft", std::move(q_arith_fft) });


    waffle::ProverTurboArithmeticWidget widget(key.get(), witness.get());
    

    transcript::Transcript transcript = test_helpers::create_dummy_standard_transcript();

    circuit_state.quotient_large = polynomial(num_gates * 4);
    for (size_t i = 0; i < num_gates * 4; ++i)
    {
        circuit_state.quotient_large[i] = fr::zero;
    }
    widget.compute_quotient_contribution(fr::one, transcript, circuit_state);

    circuit_state.quotient_large.coset_ifft(key->large_domain);
    circuit_state.quotient_large.fft(key->large_domain);
    for (size_t i = 0; i < num_gates; ++i)
    {
        EXPECT_EQ(fr::eq(circuit_state.quotient_large[i * 4], fr::zero), true);
    }
}