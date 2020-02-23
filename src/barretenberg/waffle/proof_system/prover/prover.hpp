#pragma once

#include <memory>

#include "../../../polynomials/polynomial.hpp"
#include "../../../types.hpp"

#include "../../reference_string/reference_string.hpp"
#include "../../waffle_types.hpp"
#include "../program_settings.hpp"
#include "../widgets/base_widget.hpp"

#include "../../../transcript/manifest.hpp"
#include "../../../transcript/transcript.hpp"
#include "../proving_key/proving_key.hpp"

namespace waffle {

template <typename settings> class ProverBase {
    typedef barretenberg::fr fr;
    typedef barretenberg::g1 g1;
    typedef barretenberg::polynomial polynomial;

  public:
    ProverBase(std::shared_ptr<proving_key> input_key = nullptr,
               std::shared_ptr<program_witness> input_witness = nullptr,
               const transcript::Manifest& manifest = transcript::Manifest({}));
    ProverBase(ProverBase&& other);
    ProverBase(const ProverBase& other) = delete;
    ProverBase& operator=(const ProverBase& other) = delete;
    ProverBase& operator=(ProverBase&& other);

    void execute_preamble_round();
    void execute_first_round();
    void execute_second_round();
    void execute_third_round();
    void execute_fourth_round();
    void execute_fifth_round();

    void compute_permutation_lagrange_base_full();
    void compute_wire_coefficients();
    void compute_z_coefficients();
    void compute_wire_commitments();
    void compute_z_commitment();
    void compute_quotient_commitment();
    void compute_permutation_grand_product_coefficients();
    void compute_arithmetisation_coefficients();
    void init_quotient_polynomials();
    void compute_opening_elements();

    barretenberg::fr::field_t compute_linearisation_coefficients();
    waffle::plonk_proof construct_proof();
    void reset();

    size_t n;

    std::vector<uint32_t> sigma_1_mapping;
    std::vector<uint32_t> sigma_2_mapping;
    std::vector<uint32_t> sigma_3_mapping;

    // Hmm, mixing runtime polymorphism and zero-knowledge proof generation. This seems fine...
    // TODO: note from future self: totally not fine. Replace with template parameters
    std::vector<std::unique_ptr<ProverBaseWidget>> widgets;
    ReferenceString reference_string;
    transcript::Transcript transcript;

    std::shared_ptr<proving_key> key;
    std::shared_ptr<program_witness> witness;

    bool uses_quotient_mid;
};

extern template class ProverBase<standard_settings>;
extern template class ProverBase<extended_settings>;
extern template class ProverBase<turbo_settings>;

typedef ProverBase<standard_settings> Prover;
typedef ProverBase<extended_settings> ExtendedProver;
typedef ProverBase<turbo_settings> TurboProver;

} // namespace waffle
