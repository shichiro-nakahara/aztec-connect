/**
 * Create reference strings given a path to a directory of transcript files.
 */
#pragma once
#include "reference_string.hpp"
#include <cstddef>
#include <ecc/curves/bn254/g1.hpp>
#include <ecc/curves/bn254/g2.hpp>

namespace barretenberg {
namespace pairing {
struct miller_lines;
}
} // namespace barretenberg

namespace waffle {

class VerifierFileReferenceString : public VerifierReferenceString {
  public:
    VerifierFileReferenceString(std::string const& path);
    ~VerifierFileReferenceString();

    barretenberg::g2::affine_element get_g2x() const { return g2_x; }

    barretenberg::pairing::miller_lines const* get_precomputed_g2_lines() const { return precomputed_g2_lines; }

  private:
    barretenberg::g2::affine_element g2_x;
    barretenberg::pairing::miller_lines* precomputed_g2_lines;
};

class FileReferenceString : public ProverReferenceString {
  public:
    FileReferenceString(const size_t num_points, std::string const& path);
    ~FileReferenceString();

    barretenberg::g1::affine_element* get_monomials() { return monomials; }

  private:
    barretenberg::g1::affine_element* monomials;
};

class FileReferenceStringFactory : public ReferenceStringFactory {
  public:
    FileReferenceStringFactory(std::string const& path)
        : path_(path)
    {}

    std::shared_ptr<ProverReferenceString> get_prover_crs(size_t degree)
    {
        return std::make_shared<FileReferenceString>(degree, path_);
    }

    std::shared_ptr<VerifierReferenceString> get_verifier_crs()
    {
        return std::make_shared<VerifierFileReferenceString>(path_);
    }

  private:
    std::string path_;
};

} // namespace waffle
