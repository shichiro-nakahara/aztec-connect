#include "./sequential_widget.hpp"

#include "../../../types.hpp"

#include "../../../curves/bn254/scalar_multiplication/scalar_multiplication.hpp"
#include "../../../polynomials/evaluation_domain.hpp"
#include "../../../transcript/transcript.hpp"

#include "../proving_key/proving_key.hpp"
#include "../verification_key/verification_key.hpp"

using namespace barretenberg;

namespace waffle {
ProverSequentialWidget::ProverSequentialWidget(proving_key* input_key, program_witness* input_witness)
    : ProverBaseWidget(input_key, input_witness)
    , q_3_next(key->constraint_selectors.at("q_3_next"))
    , q_3_next_fft(key->constraint_selector_ffts.at("q_3_next_fft"))
{}

ProverSequentialWidget::ProverSequentialWidget(const ProverSequentialWidget& other)
    : ProverBaseWidget(other)
    , q_3_next(key->constraint_selectors.at("q_3_next"))
    , q_3_next_fft(key->constraint_selector_ffts.at("q_3_next_fft"))
{}

ProverSequentialWidget::ProverSequentialWidget(ProverSequentialWidget&& other)
    : ProverBaseWidget(other)
    , q_3_next(key->constraint_selectors.at("q_3_next"))
    , q_3_next_fft(key->constraint_selector_ffts.at("q_3_next_fft"))
{}

ProverSequentialWidget& ProverSequentialWidget::operator=(const ProverSequentialWidget& other)
{
    ProverBaseWidget::operator=(other);

    q_3_next = key->constraint_selectors.at("q_3_next");

    q_3_next_fft = key->constraint_selector_ffts.at("q_3_next_fft");
    return *this;
}

ProverSequentialWidget& ProverSequentialWidget::operator=(ProverSequentialWidget&& other)
{
    ProverBaseWidget::operator=(other);

    q_3_next = key->constraint_selectors.at("q_3_next");

    q_3_next_fft = key->constraint_selector_ffts.at("q_3_next_fft");
    return *this;
}

fr::field_t ProverSequentialWidget::compute_quotient_contribution(const barretenberg::fr::field_t& alpha_base,
                                                                  const transcript::Transcript& transcript)
{
    fr::field_t alpha = fr::serialize_from_buffer(&transcript.get_challenge("alpha")[0]);

    barretenberg::fr::field_t old_alpha = barretenberg::fr::mul(alpha_base, alpha.invert());
    polynomial& w_3_fft = key->wire_ffts.at("w_3_fft");
    polynomial& quotient_mid = key->quotient_mid;
    ITERATE_OVER_DOMAIN_START(key->mid_domain);
    fr::field_t T0;
    fr::__mul(w_3_fft.at(2 * i + 4), q_3_next_fft[i], T0); // w_l * q_m = rdx
    T0.self_mul(old_alpha);
    quotient_mid[i].self_add(T0);
    ITERATE_OVER_DOMAIN_END;

    return alpha_base;
}

fr::field_t ProverSequentialWidget::compute_linear_contribution(const fr::field_t& alpha_base,
                                                                const transcript::Transcript& transcript,
                                                                polynomial& r)
{
    fr::field_t w_o_shifted_eval = fr::serialize_from_buffer(&transcript.get_element("w_3_omega")[0]);
    fr::field_t alpha = fr::serialize_from_buffer(&transcript.get_challenge("alpha")[0]);

    barretenberg::fr::field_t old_alpha = alpha_base * alpha.invert();
    ITERATE_OVER_DOMAIN_START(key->small_domain);
    fr::field_t T0;
    fr::__mul(w_o_shifted_eval, q_3_next[i], T0);
    T0.self_mul(old_alpha);
    r[i].self_add(T0);
    ITERATE_OVER_DOMAIN_END;
    return alpha_base;
}

// ###

VerifierSequentialWidget::VerifierSequentialWidget()
    : VerifierBaseWidget()
{
}

VerifierBaseWidget::challenge_coefficients VerifierSequentialWidget::append_scalar_multiplication_inputs(
    verification_key* key,
    const challenge_coefficients& challenge,
    const transcript::Transcript& transcript,
    std::vector<barretenberg::g1::affine_element>& points,
    std::vector<barretenberg::fr::field_t>& scalars)
{
    fr::field_t w_o_shifted_eval = fr::serialize_from_buffer(&transcript.get_element("w_3_omega")[0]);

    barretenberg::fr::field_t old_alpha =
        barretenberg::fr::mul(challenge.alpha_base, barretenberg::fr::invert(challenge.alpha_step));

    // Q_M term = w_l * w_r * challenge.alpha_base * nu
    fr::field_t q_o_next_term;
    fr::__mul(w_o_shifted_eval, old_alpha, q_o_next_term);
    q_o_next_term.self_mul(challenge.linear_nu);

    if (g1::on_curve(key->constraint_selectors.at("Q_3_NEXT"))) {
        points.push_back(key->constraint_selectors.at("Q_3_NEXT"));
        scalars.push_back(q_o_next_term);
    }

    return VerifierBaseWidget::challenge_coefficients{
        challenge.alpha_base, challenge.alpha_step, challenge.nu_base, challenge.nu_step, challenge.linear_nu
    };
}
} // namespace waffle