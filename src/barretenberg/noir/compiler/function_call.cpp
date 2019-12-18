#include "function_call.hpp"
#include "function_statement_visitor.hpp"
#include "type_info_from.hpp"

namespace noir {
namespace code_gen {

ast::function_declaration const& function_lookup(CompilerContext& ctx,
                                                 std::string const& function_name,
                                                 size_t num_args)
{
    auto it = ctx.functions.find(function_name);

    if (it == ctx.functions.end()) {
        throw std::runtime_error("Function not found: " + function_name);
    }

    auto& func = (*it).second;

    if (num_args != func.args.size()) {
        throw std::runtime_error(
            format("Function call to %s has incorrect number of arguments. Expected %d, received %d.",
                   function_name,
                   func.args.size(),
                   num_args));
    }

    return func;
}

var_t function_call(CompilerContext& ctx, ast::function_declaration const& func, std::vector<var_t> const& args)
{
    ctx.symbol_table.push();

    for (size_t i = 0; i < func.args.size(); ++i) {
        var_t v = args[i];
        // Check type of function argument matches that of given variable.
        auto arg_type_info = type_info_from_type_id(func.args[i].type);
        if (v.type != arg_type_info) {
            std::string const& var_type = v.type.type_name();
            std::string const& arg_type = arg_type_info.type_name();
            throw std::runtime_error(format("Argument %d has incorrect type %s, expected %s.", i, var_type, arg_type));
        }
        ctx.symbol_table.declare(v, func.args[i].name);
    }

    auto return_ti = type_info_from_type_id(func.return_type);
    var_t result = FunctionStatementVisitor(ctx, return_ti)(func.statements.get());
    ctx.symbol_table.pop();
    return result;
}

var_t function_call(CompilerContext& ctx, std::string const& func_name, std::vector<var_t> const& args)
{
    return function_call(ctx, function_lookup(ctx, func_name, args.size()), args);
}

} // namespace code_gen
} // namespace noir