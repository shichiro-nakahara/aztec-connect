#pragma once
#include "base_widget.hpp"

namespace waffle {

class VerifierMiMCWidget : public VerifierBaseWidget {
  public:
    VerifierMiMCWidget();

    static VerifierBaseWidget::challenge_coefficients append_scalar_multiplication_inputs(
        verification_key*,
        const challenge_coefficients& challenge,
        const transcript::Transcript& transcript,
        std::vector<barretenberg::g1::affine_element>& points,
        std::vector<barretenberg::fr>& scalars);

    static barretenberg::fr compute_batch_evaluation_contribution(verification_key*,
                                                                  barretenberg::fr& batch_eval,
                                                                  const barretenberg::fr& nu_base,
                                                                  const transcript::Transcript& transcript);

    static barretenberg::fr compute_quotient_evaluation_contribution(verification_key* key,
                                                                     const barretenberg::fr& alpha_base,
                                                                     const transcript::Transcript& transcript,
                                                                     barretenberg::fr& t_eval,
                                                                     const bool use_linearisation);
};

class ProverMiMCWidget : public ProverBaseWidget {
  public:
    ProverMiMCWidget(proving_key* key, program_witness* witness);
    ProverMiMCWidget(const ProverMiMCWidget& other);
    ProverMiMCWidget(ProverMiMCWidget&& other);
    ProverMiMCWidget& operator=(const ProverMiMCWidget& other);
    ProverMiMCWidget& operator=(ProverMiMCWidget&& other);

    barretenberg::fr compute_quotient_contribution(const barretenberg::fr& alpha_base,
                                                   const transcript::Transcript& transcript);
    barretenberg::fr compute_linear_contribution(const barretenberg::fr& alpha_base,
                                                 const transcript::Transcript& transcript,
                                                 barretenberg::polynomial& r);
    barretenberg::fr compute_opening_poly_contribution(const barretenberg::fr& nu_base,
                                                       const transcript::Transcript& transcript,
                                                       barretenberg::fr* poly,
                                                       barretenberg::fr*,
                                                       const bool use_linerisation);

    void compute_transcript_elements(transcript::Transcript& transcript, const bool use_linearisation) override;

    barretenberg::polynomial& q_mimc_selector;
    barretenberg::polynomial& q_mimc_coefficient;

    barretenberg::polynomial& q_mimc_selector_fft;
    barretenberg::polynomial& q_mimc_coefficient_fft;
};
} // namespace waffle
