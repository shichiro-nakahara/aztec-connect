#ifndef POLYNOMIAL_HPP
#define POLYNOMIAL_HPP

#include "../types.hpp"

#include "./evaluation_domain.hpp"

namespace barretenberg
{
class polynomial
{
public:
    enum Representation
    {
        COEFFICIENT_FORM,
        ROOTS_OF_UNITY,
        COSET_ROOTS_OF_UNITY,
        NONE
    };

    // TODO: add a 'spill' factor when allocating memory - we sometimes needs to extend poly degree by 2/4,
    // if page size = power of two, will trigger unneccesary copies
    polynomial(const size_t initial_size = 0, const size_t initial_max_size_hint = DEFAULT_SIZE_HINT, const Representation repr = Representation::ROOTS_OF_UNITY);
    polynomial(const polynomial &other, const size_t target_max_size = 0);
    polynomial(polynomial &&other, const size_t target_max_size = 0);
    polynomial &operator=(polynomial &&other);
    polynomial &operator=(const polynomial &other);
    ~polynomial();

    // void copy(const polynomial &other, const size_t target_max_size = 0);
    barretenberg::fr::field_t* get_coefficients() const { return coefficients; };
    size_t get_size() const { return size; };
    size_t get_max_size() const { return max_size; };
    barretenberg::fr::field_t& at(size_t i) const { return coefficients[i]; };
    barretenberg::fr::field_t evaluate(const barretenberg::fr::field_t& z, const size_t target_size) const;

    void fft(const evaluation_domain &domain);
    void coset_fft(const evaluation_domain &domain);
    void coset_fft_with_constant(const evaluation_domain &domain, const barretenberg::fr::field_t &costant);
    void ifft(const evaluation_domain &domain);
    void ifft_with_constant(const evaluation_domain &domain, const barretenberg::fr::field_t &constant);
    void coset_ifft(const evaluation_domain &domain);
    // void coset_ifft_with_constant(const evaluation_domain &domain, const barretenberg::fr::field_t &constant);

    barretenberg::fr::field_t compute_kate_opening_coefficients(const barretenberg::fr::field_t &z);
    void add_lagrange_base_coefficient(const barretenberg::fr::field_t &coefficient);
    void add_coefficient(const barretenberg::fr::field_t &coefficient);

    void reserve(const size_t new_max_size);
    void resize(const size_t new_size);
    void resize_unsafe(const size_t new_size);
    void shrink_evaluation_domain(const size_t shrink_factor);

private:
    const static size_t DEFAULT_SIZE_HINT = 1 << 20;
    const static size_t DEFAULT_PAGE_SPILL = 20;
    void add_coefficient_internal(const barretenberg::fr::field_t &coefficient);
    void bump_memory(const size_t new_size);

    barretenberg::fr::field_t *coefficients;
    Representation representation;
    size_t size;
    size_t page_size;
    size_t max_size;
    size_t allocated_pages;
};
}
#endif