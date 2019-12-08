#pragma once

namespace noir {
namespace code_gen {

struct EqualityVisitor : boost::static_visitor<var_t> {
    template <typename T> var_t operator()(std::vector<T> const&, std::vector<T> const&) const
    {
        throw std::runtime_error("No array equality.");
    }
    template <typename T> var_t operator()(T const& lhs, T const& rhs) const { return lhs == rhs; }
    template <typename T, typename U> var_t operator()(T const&, U const&) const
    {
        throw std::runtime_error("Cannot compare differing types.");
    }
};

struct BitwiseOrVisitor : boost::static_visitor<var_t> {
    template <typename T> var_t operator()(std::vector<T> const&, std::vector<T> const&) const
    {
        throw std::runtime_error("No array support.");
    }
    template <typename T> var_t operator()(T const& lhs, T const& rhs) const { return lhs | rhs; }
    template <typename T, typename U> var_t operator()(T const&, U const&) const
    {
        throw std::runtime_error("Cannot OR differing types.");
    }
};

struct BitwiseAndVisitor : boost::static_visitor<var_t> {
    template <typename T> var_t operator()(std::vector<T> const&, std::vector<T> const&) const
    {
        throw std::runtime_error("No array support.");
    }
    template <typename T> var_t operator()(T const& lhs, T const& rhs) const { return lhs & rhs; }
    template <typename T, typename U> var_t operator()(T const&, U const&) const
    {
        throw std::runtime_error("Cannot AND differing types.");
    }
};

struct BitwiseXorVisitor : boost::static_visitor<var_t> {
    template <typename T> var_t operator()(std::vector<T> const&, std::vector<T> const&) const
    {
        throw std::runtime_error("No array support.");
    }
    template <typename T> var_t operator()(T const& lhs, T const& rhs) const { return lhs ^ rhs; }
    template <typename T, typename U> var_t operator()(T const&, U const&) const
    {
        throw std::runtime_error("Cannot XOR differing types.");
    }
};

struct NegVis : boost::static_visitor<var_t> {
    template <typename T> var_t operator()(std::vector<T> const&) const
    {
        throw std::runtime_error("No array support.");
    }
    var_t operator()(bool_t const&) const { throw std::runtime_error("Cannot neg bool."); }
    var_t operator()(uint32 const&) const { throw std::runtime_error("Cannot neg uint32."); }
};

struct NotVis : boost::static_visitor<var_t> {
    template <typename T> var_t operator()(std::vector<T> const&) const
    {
        throw std::runtime_error("No array support.");
    }
    var_t operator()(bool_t const& var) const { return !var; }
    var_t operator()(uint32 const&) const { throw std::runtime_error("Cannot NOT a uint."); }
};

struct BitwiseNotVisitor : boost::static_visitor<var_t> {
    template <typename T> var_t operator()(std::vector<T> const&) const
    {
        throw std::runtime_error("No array support.");
    }
    var_t operator()(bool_t const& var) const { return ~var; }
    var_t operator()(uint32 const& var) const { return ~var; }
};

struct IndexVisitor : boost::static_visitor<var_t> {
    template <typename T> var_t operator()(std::vector<T>& lhs, unsigned int i) const
    {
        std::cout << "indexing " << i << " " << lhs[i] << std::endl;
        return lhs[i];
    }
    template <typename T> var_t operator()(T&, unsigned int) const
    {
        throw std::runtime_error("Can only index arrays.");
    }
};

struct AssignVisitor : boost::static_visitor<var_t> {
    template <typename T> var_t operator()(std::vector<T> const&, std::vector<T> const&) const
    {
        throw std::runtime_error("No array assign support (yet).");
    }
    template <typename T> var_t operator()(T& lhs, T& rhs) const { return lhs = rhs; }
    template <typename T, typename U> var_t operator()(T const&, U const&) const
    {
        throw std::runtime_error("Cannot assign differing types.");
    }
};

struct IndexedAssignVisitor : boost::static_visitor<> {
    IndexedAssignVisitor(unsigned int i)
        : i(i)
    {}

    template <typename T> void operator()(std::vector<T>& lhs, const T& rhs) const
    {
        std::cout << "indexed assign " << i << " " << lhs[i] << "->" << rhs << std::endl;
        lhs[i] = rhs;
    }

    template <typename T, typename U> void operator()(T const&, U const&) const
    {
        throw std::runtime_error("Not array or differing types in indexed assign.");
    }

    unsigned int i;
};

} // namespace code_gen
} // namespace noir