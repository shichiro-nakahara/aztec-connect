#pragma once

#include "../../primitives/field/field.hpp"
#include "../../primitives/bool/bool.hpp"
#include "../../primitives/bigfield/bigfield.hpp"
#include "../../primitives/biggroup/biggroup.hpp"

#include "../transcript/transcript.hpp"

#include <plonk/proof_system/utils/linearizer.hpp>
#include <plonk/proof_system/public_inputs/public_inputs.hpp>

#include <plonk/proof_system/widgets/turbo_fixed_base_widget.hpp>

#include <polynomials/polynomial_arithmetic.hpp>

#include <ecc/curves/bn254/fq12.hpp>
#include <ecc/curves/bn254/pairing.hpp>

namespace plonk {
namespace stdlib {
namespace recursion {

template <typename Group> struct recursion_output {
    Group P0;
    Group P1;
};

template <typename Composer> struct lagrange_evaluations {
    field_t<Composer> l_1;
    field_t<Composer> l_n_minus_1;
    field_t<Composer> vanishing_poly;
};

template <typename Composer>
lagrange_evaluations<Composer> get_lagrange_evaluations(const field_t<Composer>& z, const evaluation_domain& domain)
{
    using field_pt = field_t<Composer>;
    field_pt z_pow = z;
    for (size_t i = 0; i < domain.log2_size; ++i) {
        z_pow *= z_pow;
    }
    field_pt numerator = z_pow - field_pt(1);

    lagrange_evaluations<Composer> result;
    result.vanishing_poly = numerator / (z - domain.root_inverse);
    numerator *= domain.domain_inverse;
    result.l_1 = numerator / (z - field_pt(1));
    result.l_n_minus_1 = numerator / ((z * domain.root.sqr()) - field_pt(1));

    barretenberg::polynomial_arithmetic::get_lagrange_evaluations(z.get_value(), domain);
    return result;
}

template <typename Composer, typename program_settings>
recursion_output<
    element<Composer, bigfield<Composer, barretenberg::Bn254FqParams>, field_t<Composer>, barretenberg::Bn254G1Params>>
verify_proof(Composer* context,
             std::shared_ptr<waffle::verification_key> key,
             const transcript::Manifest& manifest,
             const waffle::plonk_proof& proof)
{
    using bool_pt = bool_t<Composer>;
    using field_pt = field_t<Composer>;
    using fq_pt = bigfield<Composer, barretenberg::Bn254FqParams>;
    using group_pt = element<Composer, fq_pt, field_pt, barretenberg::Bn254G1Params>;

    Transcript<Composer> transcript = Transcript<Composer>(context, proof.proof_data, manifest);

    std::array<group_pt, program_settings::program_width> T;
    std::array<group_pt, program_settings::program_width> W;
    std::array<group_pt, program_settings::program_width> sigmas;
    constexpr size_t num_sigma_evaluations =
        (program_settings::use_linearisation ? program_settings::program_width - 1 : program_settings::program_width);

    std::array<field_pt, program_settings::program_width> wire_evaluations;
    std::array<field_pt, num_sigma_evaluations> sigma_evaluations;

    for (size_t i = 0; i < program_settings::program_width; ++i) {
        std::string index = std::to_string(i + 1);
        T[i] = transcript.get_group_element("T_" + index);
        W[i] = transcript.get_group_element("W_" + index);
        wire_evaluations[i] = transcript.get_field_element("w_" + index);
        std::cout << "sigmas = " << key->permutation_selectors.at("SIGMA_" + std::to_string(i + 1)) << std::endl;
        sigmas[i] =
            Transcript<Composer>::convert_g1(context, key->permutation_selectors.at("SIGMA_" + std::to_string(i + 1)));
    }

    for (size_t i = 0; i < num_sigma_evaluations; ++i) {
        std::string index = std::to_string(i + 1);
        sigma_evaluations[i] = transcript.get_field_element("sigma_" + index);
    }

    group_pt Z_1 = transcript.get_group_element("Z");
    group_pt PI_Z = transcript.get_group_element("PI_Z");
    group_pt PI_Z_OMEGA = transcript.get_group_element("PI_Z_OMEGA");

    field_pt z_1_shifted_eval = transcript.get_field_element("z_omega");

    bool_pt inputs_valid = T[0].on_curve() && Z_1.on_curve() && PI_Z.on_curve();
    for (size_t i = 0; i < program_settings::program_width; ++i) {
        inputs_valid = inputs_valid && sigmas[i].on_curve();
        inputs_valid = inputs_valid && !(sigma_evaluations[i] == field_pt(0));
    }
    if constexpr (program_settings::use_linearisation) {
        field_pt linear_eval = transcript.get_field_element("r");
        inputs_valid = inputs_valid && !(linear_eval == field_pt(0)());
    }
    context->assert_equal_constant(inputs_valid.witness_index, barretenberg::fr(1));

    field_pt alpha_pow[4];

    field_t circuit_size(stdlib::witness_t(context, barretenberg::fr(key->n)));
    field_t public_input_size(stdlib::witness_t(context, barretenberg::fr(key->num_public_inputs)));

    transcript.add_field_element("circuit_size", circuit_size);
    transcript.add_field_element("public_input_size", public_input_size);

    transcript.apply_fiat_shamir("init");
    transcript.apply_fiat_shamir("beta");
    transcript.apply_fiat_shamir("alpha");
    transcript.apply_fiat_shamir("z");

    field_pt beta = transcript.get_challenge_field_element("beta");
    field_pt alpha = transcript.get_challenge_field_element("alpha");
    field_pt z_challenge = transcript.get_challenge_field_element("z");
    field_pt gamma = transcript.get_challenge_field_element("beta", 1);

    field_pt t_eval = field_pt(0);

    lagrange_evaluations<Composer> lagrange_evals = get_lagrange_evaluations(z_challenge, key->domain);

    waffle::plonk_linear_terms<field_pt> linear_terms =
        waffle::compute_linear_terms<field_pt, Transcript<waffle::TurboComposer>, program_settings>(transcript,
                                                                                                    lagrange_evals.l_1);

    // reconstruct evaluation of quotient polynomial from prover messages
    field_pt T0;
    field_pt T1;
    field_pt T2;
    alpha_pow[0] = alpha;
    for (size_t i = 1; i < 4; ++i) {
        alpha_pow[i] = alpha_pow[i - 1] * alpha_pow[0];
    }

    field_pt sigma_contribution(context, barretenberg::fr(1));

    for (size_t i = 0; i < program_settings::program_width - 1; ++i) {
        sigma_contribution *= (sigma_evaluations[i] * beta + wire_evaluations[i] + gamma);
    }

    std::vector<field_pt> public_inputs = (transcript.get_field_element_vector("public_inputs"));

    // TODO fix:
    field_pt public_input_delta =
        waffle::compute_public_input_delta<field_pt>(public_inputs, beta, gamma, key->domain.root);
    T0 = wire_evaluations[program_settings::program_width - 1] + gamma;
    sigma_contribution *= T0;
    sigma_contribution *= z_1_shifted_eval;
    sigma_contribution *= alpha_pow[0];

    T1 = z_1_shifted_eval - public_input_delta;
    T1 *= lagrange_evals.l_n_minus_1;
    T1 *= alpha_pow[1];

    T2 = lagrange_evals.l_1 * alpha_pow[2];
    T1 -= T2;
    T1 -= sigma_contribution;

    if constexpr (program_settings::use_linearisation) {
        field_pt linear_eval = transcript.get_field_element("r");
        T1 += linear_eval;
    }
    t_eval += T1;

    field_pt alpha_base = alpha.sqr().sqr();

    alpha_base = program_settings::compute_quotient_evaluation_contribution(key.get(), alpha_base, transcript, t_eval);

    if constexpr (!program_settings::use_linearisation) {
        field_pt z_eval = transcript.get_field_element("z");
        t_eval += (linear_terms.z_1 * z_eval);
        t_eval += (linear_terms.sigma_last * sigma_evaluations[program_settings::program_width - 1]);
    }

    t_eval = t_eval / lagrange_evals.vanishing_poly;
    std::cout << "rverifier t_eval = " << t_eval << std::endl;
    transcript.add_field_element("t", t_eval);

    transcript.apply_fiat_shamir("nu");
    transcript.apply_fiat_shamir("separator");

    std::vector<field_pt> nu_challenges;
    for (size_t i = 0; i < transcript.get_num_challenges("nu"); ++i) {
        nu_challenges.emplace_back(transcript.get_challenge_field_element("nu", i));
    }
    field_pt u = transcript.get_challenge_field_element("separator");

    field_pt batch_evaluation = t_eval;
    constexpr size_t nu_offset = program_settings::use_linearisation ? 1 : 0;
    if constexpr (program_settings::use_linearisation) {
        field_pt linear_eval = transcript.get_field_element("r");
        T0 = nu_challenges[0] * linear_eval;
        batch_evaluation += T0;
    } else {
        field_pt z_eval = transcript.get_field_element("z");
        T0 = z_eval * nu_challenges[2 * program_settings::program_width];
        batch_evaluation += T0;
        T0 = nu_challenges[2 * program_settings::program_width - 1] *
             sigma_evaluations[program_settings::program_width - 1];
        batch_evaluation += T0;
    }
    for (size_t i = 0; i < program_settings::program_width; ++i) {
        T0 = nu_challenges[i + nu_offset] * wire_evaluations[i];
        batch_evaluation += T0;
    }

    for (size_t i = 0; i < program_settings::program_width - 1; ++i) {
        T0 = nu_challenges[program_settings::program_width + i + nu_offset] * sigma_evaluations[i];
        batch_evaluation += T0;
    }

    constexpr size_t nu_z_offset = (program_settings::use_linearisation) ? 2 * program_settings::program_width
                                                                         : 2 * program_settings::program_width + 1;

    T0 = nu_challenges[nu_z_offset] * u;
    T0 *= z_1_shifted_eval;
    batch_evaluation += T0;

    size_t nu_ptr = nu_z_offset + 1;

    for (size_t i = 0; i < program_settings::program_width; ++i) {
        if (program_settings::requires_shifted_wire(program_settings::wire_shift_settings, i)) {
            field_pt wire_shifted_eval = transcript.get_field_element("w_" + std::to_string(i + 1) + "_omega");
            T0 = wire_shifted_eval * nu_challenges[nu_ptr++];
            T0 *= u;
            batch_evaluation += T0;
        }
    }

    program_settings::compute_batch_evaluation_contribution(key.get(), batch_evaluation, nu_ptr, transcript);

    batch_evaluation = -batch_evaluation;

    field_pt z_omega_scalar;
    z_omega_scalar = z_challenge * key->domain.root;
    z_omega_scalar *= u;

    std::vector<field_pt> scalars;
    std::vector<group_pt> elements;

    elements.emplace_back(Z_1);
    if constexpr (program_settings::use_linearisation) {
        linear_terms.z_1 *= nu_challenges[0];
        linear_terms.z_1 += (nu_challenges[nu_z_offset] * u);
        scalars.emplace_back(linear_terms.z_1);
    } else {
        T0 = nu_challenges[nu_z_offset] * u + nu_challenges[2 * program_settings::program_width];
        scalars.emplace_back(T0);
    }

    nu_ptr = nu_z_offset + 1;
    for (size_t i = 0; i < program_settings::program_width; ++i) {
        // TODO sort this to remove branch
        if (W[i].on_curve().get_value()) {
            elements.emplace_back(W[i]);
            if (program_settings::requires_shifted_wire(program_settings::wire_shift_settings, i)) {
                T0 = nu_challenges[nu_ptr] * u;
                T0 += nu_challenges[i + nu_offset];
                scalars.emplace_back(T0);
            } else {
                scalars.emplace_back(nu_challenges[i + nu_offset]);
            }
        }
        if (program_settings::requires_shifted_wire(program_settings::wire_shift_settings, i)) {
            ++nu_ptr;
        }
    }

    for (size_t i = 0; i < program_settings::program_width - 1; ++i) {
        elements.emplace_back(sigmas[i]);
        scalars.emplace_back(nu_challenges[program_settings::program_width + i + nu_offset]);
    }

    if constexpr (program_settings::use_linearisation) {
        elements.emplace_back(sigmas[program_settings::program_width - 1]);
        linear_terms.sigma_last *= nu_challenges[0];
        scalars.emplace_back(linear_terms.sigma_last);
    } else {
        elements.emplace_back(sigmas[program_settings::program_width - 1]);
        scalars.emplace_back(nu_challenges[2 * program_settings::program_width - 1]);
    }

    elements.emplace_back(group_pt::one(context));
    scalars.emplace_back(batch_evaluation);

    // TODO FIX
    if (PI_Z_OMEGA.on_curve().get_value()) {
        elements.emplace_back(PI_Z_OMEGA);
        scalars.emplace_back(z_omega_scalar);
    }

    elements.emplace_back(PI_Z);
    scalars.emplace_back(z_challenge);

    // TODO FIX
    field_pt z_pow_n = z_challenge;
    const size_t log2_n = numeric::get_msb(static_cast<uint64_t>(key->n));
    for (size_t j = 0; j < log2_n; ++j) {
        z_pow_n = z_pow_n.sqr();
    }
    field_pt z_power = z_pow_n;
    for (size_t i = 1; i < program_settings::program_width; ++i) {
        if (T[i].on_curve().get_value()) {
            elements.emplace_back(T[i]);
            scalars.emplace_back(z_power);
        }

        z_power *= z_pow_n;
    }

    waffle::VerifierBaseWidget::challenge_coefficients<field_pt> coeffs{ alpha.sqr().sqr(), alpha, nu_ptr, 0 };

    std::vector<barretenberg::g1::affine_element> g1_inputs;
    program_settings::append_scalar_multiplication_inputs(key.get(), coeffs, transcript, g1_inputs, scalars);
    for (size_t i = 0; i < g1_inputs.size(); ++i) {
        elements.push_back(Transcript<waffle::TurboComposer>::convert_g1(context, g1_inputs[i]));
    }

    std::cout << "mark a" << std::endl;
    group_pt rhs = group_pt::batch_mul(elements, scalars);
    std::cout << "mark b" << std::endl;
    rhs = (rhs + T[0]).normalize();
    std::cout << "mark c" << std::endl;
    group_pt lhs = PI_Z_OMEGA * u;
    std::cout << "mark d" << std::endl;
    lhs = lhs + PI_Z;
    std::cout << "mark e" << std::endl;
    lhs = (-lhs).normalize();
    std::cout << "mark f" << std::endl;

    return recursion_output<group_pt>{ rhs, lhs };
    /*
    std::cout << "g1 rhs points = " << elements.size() << std::endl;
    barretenberg::g1::element rhs = barretenberg::g1::one;
    rhs.self_set_infinity();
    for (size_t i = 0; i < elements.size(); ++i) {
        group_pt input = elements[i];
        uint256_t x = input.x.get_value().lo;
        uint256_t y = input.y.get_value().lo;
        barretenberg::g1::element point(barretenberg::fq(x), barretenberg::fq(y), barretenberg::fq(1));
        barretenberg::fr scalar(scalars[i].get_value());
        rhs += (point * scalar);
    }

    barretenberg::g1::element lhs;
    lhs.x = barretenberg::fq(PI_Z_OMEGA.x.get_value().lo);
    lhs.y = barretenberg::fq(PI_Z_OMEGA.y.get_value().lo);
    lhs.z = barretenberg::fq(1);
    lhs = lhs * u.get_value();

    barretenberg::g1::element to_add;
    to_add.x = barretenberg::fq(PI_Z.x.get_value().lo);
    to_add.y = barretenberg::fq(PI_Z.y.get_value().lo);
    to_add.z = barretenberg::fq(1);

    lhs += to_add;
    lhs = -lhs;

    to_add.x = barretenberg::fq(T[0].x.get_value().lo);
    to_add.y = barretenberg::fq(T[0].y.get_value().lo);
    to_add.z = barretenberg::fq(1);
    rhs += to_add;

    lhs = lhs.normalize();
    rhs = rhs.normalize();

    g1::affine_element P_affine[2];
    barretenberg::fq::__copy(lhs.x, P_affine[1].x);
    barretenberg::fq::__copy(lhs.y, P_affine[1].y);
    barretenberg::fq::__copy(rhs.x, P_affine[0].x);
    barretenberg::fq::__copy(rhs.y, P_affine[0].y);

    barretenberg::fq12 tresult = barretenberg::pairing::reduced_ate_pairing_batch_precomputed(
        P_affine, key->reference_string.precomputed_g2_lines, 2);

    std::cout << "result = " << (tresult == barretenberg::fq12::one()) << std::endl;
    // recursion_output<group_pt> result{
    //     (PI_Z_OMEGA * u) + PI_Z,
    //     group_pt::batch_mul(scalars, elements),
    // };
    recursion_output<group_pt> result{
        group_pt::one(context),
        group_pt::one(context),
    };

    return result;
    */
}

} // namespace recursion
} // namespace stdlib
} // namespace plonk