#pragma once

#include <vector>

#include "../../../polynomials/evaluation_domain.hpp"

namespace waffle {
barretenberg::fr compute_public_input_delta(const std::vector<barretenberg::fr>& inputs,
                                                     const barretenberg::fr& beta,
                                                     const barretenberg::fr& gamma,
                                                     const barretenberg::fr& subgroup_generator);
}