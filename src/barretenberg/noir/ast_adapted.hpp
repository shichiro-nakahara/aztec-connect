#pragma once
#include "ast.hpp"
#include <boost/fusion/include/adapt_struct.hpp>

BOOST_FUSION_ADAPT_STRUCT(noir::ast::unary, operator_, operand_)

BOOST_FUSION_ADAPT_STRUCT(noir::ast::operation, operator_, operand_)

BOOST_FUSION_ADAPT_STRUCT(noir::ast::expression, first, rest)

BOOST_FUSION_ADAPT_STRUCT(noir::ast::variable_declaration, assign)

BOOST_FUSION_ADAPT_STRUCT(noir::ast::assignment, lhs, rhs)

// BOOST_FUSION_ADAPT_STRUCT(noir::ast::if_statement, condition, then, else_)

// BOOST_FUSION_ADAPT_STRUCT(noir::ast::while_statement, condition, body)
