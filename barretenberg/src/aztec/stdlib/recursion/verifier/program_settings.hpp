#pragma once

#include <plonk/proof_system/types/program_settings.hpp>
#include <stdlib/types/turbo.hpp>

#include "../transcript/transcript.hpp"

namespace plonk {
namespace stdlib {
namespace recursion {

class recursive_turbo_verifier_settings : public waffle::unrolled_turbo_settings {
  public:
    typedef plonk::stdlib::types::turbo::field_ct field_ct;
    typedef barretenberg::g1 g1;
    typedef Transcript<waffle::TurboComposer> transcript_ct;
    typedef waffle::VerifierPermutationWidget<field_ct, g1::affine_element, transcript_ct> PermutationWidget;
    typedef waffle::VerifierTurboFixedBaseWidget<field_ct, g1::affine_element, transcript_ct> TurboFixedBaseWidget;
    typedef waffle::VerifierTurboRangeWidget<field_ct, g1::affine_element, transcript_ct> TurboRangeWidget;
    typedef waffle::VerifierTurboLogicWidget<field_ct, g1::affine_element, transcript_ct> TurboLogicWidget;

    static constexpr size_t num_challenge_bytes = 16;
    static constexpr transcript::HashType hash_type = transcript::HashType::PedersenBlake2s;
    static constexpr bool use_linearisation = false;
    static field_ct append_scalar_multiplication_inputs(waffle::verification_key* key,
                                                        const field_ct& alpha_base,
                                                        const transcript_ct& transcript,
                                                        std::vector<g1::affine_element>& points,
                                                        std::vector<field_ct>& scalars)
    {
        auto updated_alpha = PermutationWidget::append_scalar_multiplication_inputs(
            key, alpha_base, transcript, points, scalars, use_linearisation);

        updated_alpha = TurboFixedBaseWidget::append_scalar_multiplication_inputs(
            key, updated_alpha, transcript, points, scalars, use_linearisation);

        updated_alpha = TurboRangeWidget::append_scalar_multiplication_inputs(
            key, updated_alpha, transcript, points, scalars, use_linearisation);

        updated_alpha = TurboLogicWidget::append_scalar_multiplication_inputs(
            key, updated_alpha, transcript, points, scalars, use_linearisation);
        return updated_alpha;
    }

    static void compute_batch_evaluation_contribution(waffle::verification_key* key,
                                                      field_ct& batch_eval,
                                                      const transcript_ct& transcript)
    {
        PermutationWidget::compute_batch_evaluation_contribution(key, batch_eval, transcript, use_linearisation);
        TurboFixedBaseWidget::compute_batch_evaluation_contribution(key, batch_eval, transcript, use_linearisation);
        TurboRangeWidget::compute_batch_evaluation_contribution(key, batch_eval, transcript, use_linearisation);
        TurboLogicWidget::compute_batch_evaluation_contribution(key, batch_eval, transcript, use_linearisation);
    }

    static field_ct compute_quotient_evaluation_contribution(waffle::verification_key* key,
                                                             const field_ct& alpha_base,
                                                             const transcript_ct& transcript,
                                                             field_ct& t_eval)
    {
        auto updated_alpha_base = PermutationWidget::compute_quotient_evaluation_contribution(
            key, alpha_base, transcript, t_eval, use_linearisation);
        updated_alpha_base = TurboFixedBaseWidget::compute_quotient_evaluation_contribution(
            key, updated_alpha_base, transcript, t_eval, use_linearisation);
        updated_alpha_base = TurboRangeWidget::compute_quotient_evaluation_contribution(
            key, updated_alpha_base, transcript, t_eval, use_linearisation);
        updated_alpha_base = TurboLogicWidget::compute_quotient_evaluation_contribution(
            key, updated_alpha_base, transcript, t_eval, use_linearisation);
        return updated_alpha_base;
    }
};
} // namespace recursion
} // namespace stdlib
} // namespace plonk
