#pragma once
#include "base_widget.hpp"

namespace waffle {
/**
 * ProverBoolWidget : constraint that constrains left and right wire values to be booleans
 *
 **/

class VerifierBoolWidget : public VerifierBaseWidget {
  public:
    VerifierBoolWidget();

    static barretenberg::fr compute_quotient_evaluation_contribution(
        verification_key*, const barretenberg::fr&, const transcript::Transcript&, barretenberg::fr&, const bool);

    static barretenberg::fr compute_batch_evaluation_contribution(verification_key*,
                                                                  barretenberg::fr& batch_eval,
                                                                  const barretenberg::fr& nu_base,
                                                                  const transcript::Transcript& transcript,
                                                                  const bool use_linearisation);

    static VerifierBaseWidget::challenge_coefficients append_scalar_multiplication_inputs(
        verification_key* key,
        const challenge_coefficients& challenge,
        const transcript::Transcript& transcript,
        std::vector<barretenberg::g1::affine_element>& points,
        std::vector<barretenberg::fr>& scalars,
        const bool use_linearisation);
};

class ProverBoolWidget : public ProverBaseWidget {
  public:
    ProverBoolWidget(proving_key* input_key, program_witness* input_witness);
    ProverBoolWidget(const ProverBoolWidget& other);
    ProverBoolWidget(ProverBoolWidget&& other);
    ProverBoolWidget& operator=(const ProverBoolWidget& other);
    ProverBoolWidget& operator=(ProverBoolWidget&& other);

    barretenberg::fr compute_quotient_contribution(const barretenberg::fr& alpha_base,
                                                   const transcript::Transcript& transcript);
    barretenberg::fr compute_linear_contribution(const barretenberg::fr& alpha_base,
                                                 const transcript::Transcript& transcript,
                                                 barretenberg::polynomial& r);

    barretenberg::fr compute_opening_poly_contribution(const barretenberg::fr& nu_base,
                                                       const transcript::Transcript&,
                                                       barretenberg::fr*,
                                                       barretenberg::fr*,
                                                       const bool);

    void compute_transcript_elements(transcript::Transcript&, const bool) override;

    barretenberg::polynomial& q_bl;
    barretenberg::polynomial& q_br;
    barretenberg::polynomial& q_bo;

    barretenberg::polynomial& q_bl_fft;
    barretenberg::polynomial& q_br_fft;
    barretenberg::polynomial& q_bo_fft;
};
} // namespace waffle
