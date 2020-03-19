#pragma once
#include "turbo_arithmetic_widget.hpp"

namespace waffle {
class VerifierTurboFixedBaseWidget : public VerifierBaseWidget {
  public:
    VerifierTurboFixedBaseWidget();

    static VerifierBaseWidget::challenge_coefficients append_scalar_multiplication_inputs(
        verification_key*,
        const challenge_coefficients& challenge,
        const transcript::Transcript& transcript,
        std::vector<barretenberg::g1::affine_element>& points,
        std::vector<barretenberg::fr>& scalars);

    static barretenberg::fr compute_batch_evaluation_contribution(verification_key*,
                                                                  barretenberg::fr&,
                                                                  const barretenberg::fr& nu_base,
                                                                  const transcript::Transcript&);

    static barretenberg::fr compute_quotient_evaluation_contribution(verification_key*,
                                                                     const barretenberg::fr& alpha_base,
                                                                     const transcript::Transcript& transcript,
                                                                     barretenberg::fr& t_eval,
                                                                     const bool use_lineraisation);
};

class ProverTurboFixedBaseWidget : public ProverTurboArithmeticWidget {
  public:
    ProverTurboFixedBaseWidget(proving_key* input_key, program_witness* input_witness);
    ProverTurboFixedBaseWidget(const ProverTurboFixedBaseWidget& other);
    ProverTurboFixedBaseWidget(ProverTurboFixedBaseWidget&& other);
    ProverTurboFixedBaseWidget& operator=(const ProverTurboFixedBaseWidget& other);
    ProverTurboFixedBaseWidget& operator=(ProverTurboFixedBaseWidget&& other);

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

    void compute_transcript_elements(transcript::Transcript& transcript, const bool use_linearisation) override;

    barretenberg::polynomial& q_ecc_1;
    barretenberg::polynomial& q_ecc_1_fft;
};
} // namespace waffle
