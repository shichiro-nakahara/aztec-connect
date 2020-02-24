#include "./bool_widget.hpp"

#include "../../../curves/bn254/scalar_multiplication/scalar_multiplication.hpp"
#include "../../../transcript/transcript.hpp"
#include "../../../types.hpp"

#include "../proving_key/proving_key.hpp"
#include "../verification_key/verification_key.hpp"

using namespace barretenberg;

namespace waffle {
ProverBoolWidget::ProverBoolWidget(proving_key* input_key, program_witness* input_witness)
    : ProverBaseWidget(input_key, input_witness)
    , q_bl(key->constraint_selectors.at("q_bl"))
    , q_br(key->constraint_selectors.at("q_br"))
    , q_bo(key->constraint_selectors.at("q_bo"))
    , q_bl_fft(key->constraint_selector_ffts.at("q_bl_fft"))
    , q_br_fft(key->constraint_selector_ffts.at("q_br_fft"))
    , q_bo_fft(key->constraint_selector_ffts.at("q_bo_fft"))
{}

ProverBoolWidget::ProverBoolWidget(const ProverBoolWidget& other)
    : ProverBaseWidget(other)
    , q_bl(key->constraint_selectors.at("q_bl"))
    , q_br(key->constraint_selectors.at("q_br"))
    , q_bo(key->constraint_selectors.at("q_bo"))
    , q_bl_fft(key->constraint_selector_ffts.at("q_bl_fft"))
    , q_br_fft(key->constraint_selector_ffts.at("q_br_fft"))
    , q_bo_fft(key->constraint_selector_ffts.at("q_bo_fft"))
{}

ProverBoolWidget::ProverBoolWidget(ProverBoolWidget&& other)
    : ProverBaseWidget(other)
    , q_bl(key->constraint_selectors.at("q_bl"))
    , q_br(key->constraint_selectors.at("q_br"))
    , q_bo(key->constraint_selectors.at("q_bo"))
    , q_bl_fft(key->constraint_selector_ffts.at("q_bl_fft"))
    , q_br_fft(key->constraint_selector_ffts.at("q_br_fft"))
    , q_bo_fft(key->constraint_selector_ffts.at("q_bo_fft"))
{}

ProverBoolWidget& ProverBoolWidget::operator=(const ProverBoolWidget& other)
{
    ProverBaseWidget::operator=(other);

    q_bl = key->constraint_selectors.at("q_bl");
    q_br = key->constraint_selectors.at("q_br");
    q_bo = key->constraint_selectors.at("q_bo");

    q_bl_fft = key->constraint_selectors.at("q_bl_fft");
    q_br_fft = key->constraint_selectors.at("q_br_fft");
    q_bo_fft = key->constraint_selectors.at("q_bo_fft");
    return *this;
}

ProverBoolWidget& ProverBoolWidget::operator=(ProverBoolWidget&& other)
{
    ProverBaseWidget::operator=(other);

    q_bl = key->constraint_selectors.at("q_bl");
    q_br = key->constraint_selectors.at("q_br");
    q_bo = key->constraint_selectors.at("q_bo");

    q_bl_fft = key->constraint_selectors.at("q_bl_fft");
    q_br_fft = key->constraint_selectors.at("q_br_fft");
    q_bo_fft = key->constraint_selectors.at("q_bo_fft");
    return *this;
}

fr::field_t ProverBoolWidget::compute_quotient_contribution(const fr::field_t& alpha_base,
                                                            const transcript::Transcript& transcript)
{
    fr::field_t alpha = fr::field_t::serialize_from_buffer(transcript.get_challenge("alpha").begin());

    polynomial& w_1_fft = key->wire_ffts.at("w_1_fft");
    polynomial& w_2_fft = key->wire_ffts.at("w_2_fft");
    polynomial& w_3_fft = key->wire_ffts.at("w_3_fft");

    polynomial& quotient_mid = key->quotient_mid;

    fr::field_t alpha_a = alpha_base * alpha;
    fr::field_t alpha_b = alpha_a * alpha;
    ITERATE_OVER_DOMAIN_START(key->mid_domain);
    fr::field_t T0 = (w_1_fft[i * 2].sqr() - w_1_fft[i * 2]) * q_bl_fft[i] * alpha_base;
    fr::field_t T1 = (w_2_fft[i * 2].sqr() - w_2_fft[i * 2]) * q_br_fft[i] * alpha_a;
    fr::field_t T2 = (w_3_fft[i * 3].sqr() - w_3_fft[i * 2]) * q_bo_fft[i] * alpha_b;

    quotient_mid[i].self_add(T0 + T1 + T2);
    ITERATE_OVER_DOMAIN_END;

    return alpha_base * alpha_b * alpha;
}

fr::field_t ProverBoolWidget::compute_linear_contribution(const fr::field_t& alpha_base,
                                                          const transcript::Transcript& transcript,
                                                          polynomial& r)
{
    fr::field_t alpha = fr::field_t::serialize_from_buffer(transcript.get_challenge("alpha").begin());
    fr::field_t w_l_eval = fr::field_t::serialize_from_buffer(&transcript.get_element("w_1")[0]);
    fr::field_t w_r_eval = fr::field_t::serialize_from_buffer(&transcript.get_element("w_2")[0]);
    fr::field_t w_o_eval = fr::field_t::serialize_from_buffer(&transcript.get_element("w_3")[0]);

    fr::field_t left = (w_l_eval.sqr() - w_l_eval) * alpha_base;
    fr::field_t right = (w_r_eval.sqr() - w_r_eval) * alpha_base * alpha;
    fr::field_t out = (w_o_eval.sqr() - w_o_eval) * alpha_base * alpha.sqr();

    ITERATE_OVER_DOMAIN_START(key->small_domain);
    r[i].self_add((left * q_bl[i]) + (right * q_br[i]) + (out * q_bo[i]));
    ITERATE_OVER_DOMAIN_END;

    return alpha_base * alpha.sqr() * alpha;
}

fr::field_t ProverBoolWidget::compute_opening_poly_contribution(const fr::field_t& nu_base,
                                                                const transcript::Transcript&,
                                                                fr::field_t*,
                                                                fr::field_t*)
{
    return nu_base;
}

// ###

VerifierBoolWidget::VerifierBoolWidget()
    : VerifierBaseWidget()
{}

fr::field_t VerifierBoolWidget::compute_quotient_evaluation_contribution(verification_key*,
                                                                         const fr::field_t& alpha_base,
                                                                         const transcript::Transcript& transcript,
                                                                         fr::field_t&)
{
    return alpha_base * fr::field_t::serialize_from_buffer(transcript.get_challenge("alpha").begin());
}

fr::field_t VerifierBoolWidget::compute_batch_evaluation_contribution(verification_key*,
                                                                      fr::field_t&,
                                                                      const fr::field_t& nu_base,
                                                                      const transcript::Transcript&)
{
    return nu_base;
};

VerifierBaseWidget::challenge_coefficients VerifierBoolWidget::append_scalar_multiplication_inputs(
    verification_key* key,
    const challenge_coefficients& challenge,
    const transcript::Transcript& transcript,
    std::vector<g1::affine_element>& points,
    std::vector<fr::field_t>& scalars)
{
    fr::field_t w_l_eval = fr::field_t::serialize_from_buffer(&transcript.get_element("w_1")[0]);
    fr::field_t w_r_eval = fr::field_t::serialize_from_buffer(&transcript.get_element("w_2")[0]);
    fr::field_t w_o_eval = fr::field_t::serialize_from_buffer(&transcript.get_element("w_3")[0]);

    fr::field_t left_bool_multiplier = (w_l_eval.sqr() - w_l_eval) * challenge.alpha_base * challenge.linear_nu;
    fr::field_t right_bool_multiplier =
        (w_r_eval.sqr() - w_r_eval) * challenge.alpha_base * challenge.alpha_step * challenge.linear_nu;
    fr::field_t output_bool_multiplier =
        (w_o_eval.sqr() - w_o_eval) * challenge.alpha_base * challenge.alpha_step.sqr() * challenge.linear_nu;

    if (g1::on_curve(key->constraint_selectors.at("Q_BL"))) {
        points.push_back(key->constraint_selectors.at("Q_BL"));
        scalars.push_back(left_bool_multiplier);
    }
    if (g1::on_curve(key->constraint_selectors.at("Q_BR"))) {
        points.push_back(key->constraint_selectors.at("Q_BR"));
        scalars.push_back(right_bool_multiplier);
    }
    if (g1::on_curve(key->constraint_selectors.at("Q_BO"))) {
        points.push_back(key->constraint_selectors.at("Q_BO"));
        scalars.push_back(output_bool_multiplier);
    }

    return VerifierBaseWidget::challenge_coefficients{ challenge.alpha_base * challenge.alpha_step.sqr() *
                                                           challenge.alpha_step,
                                                       challenge.alpha_step,
                                                       challenge.nu_base,
                                                       challenge.nu_step,
                                                       challenge.linear_nu };
}
} // namespace waffle