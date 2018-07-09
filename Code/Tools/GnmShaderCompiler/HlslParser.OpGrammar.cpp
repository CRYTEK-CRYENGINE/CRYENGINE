// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "HlslParser.inl"

namespace HlslParser
{

void SParserState::SGrammar::InitOpGrammar(SParserState& state)
{
	// Matches terminals and ()/{} sub-expressions
	// Also matches C++ style functional cast (allowed in HLSL)
	// Note: All opXXX are ordered, such that each operator precedence group is one rule
	opPrimary =
	  (lit('(') > opComma > lit(')'))[_val = phx::bind(&SParserState::AddExpr, phx::ref(state), SExpression::kOpSubExpression, _1, 0U, 0U)]
	  | (lit('{') > (((opAssign % lit(',')) >> -lit(',')) | attr(std::vector<uint32_t>())) > lit('}'))[_val = phx::bind(&SParserState::AddListExpr, phx::ref(state), SExpression::kOpCompound, 0U, _1)]
	  | (boolLiteral | floatLiteral | intLiteral)[_val = phx::bind(&SParserState::AddConstExpr, phx::ref(state), _1)]
	  | repo::distinct(alnum | '_')[state.functionNames][_val = phx::bind(&SParserState::AddExpr, phx::ref(state), SExpression::kTerminalFunction, _1, 0U, 0U)]
	  | repo::distinct(alnum | '_')[state.variableNames][_val = phx::bind(&SParserState::AddExpr, phx::ref(state), SExpression::kTerminalVariable, _1, 0U, 0U)]
	  | repo::distinct(alnum | '_')[state.intrinsics][_val = phx::bind(&SParserState::AddExpr, phx::ref(state), SExpression::kTerminalIntrinsic, _1, 0U, 0U)]
	  | ((typeFull >> lit('(')) > (opAssign % lit(',') | attr(std::vector<uint32_t>())) > lit(')'))[_val = phx::bind(&SParserState::AddTypedListExpr, phx::ref(state), SExpression::kOpConstruct, _1, _2)];
	opPrimary.name("expression_terminal");

	// Postfix expression (ie, ++/--/[]/.) and function calls
	opSpecial =
	  opPrimary[_val = _1] >
	  *((lit("++") > attr(SExpression::kOpPostfixIncrement)
	     | lit("--") > attr(SExpression::kOpPostfixDecrement)
	     )[_val = phx::bind(&SParserState::AddExpr, phx::ref(state), _1, _val, 0U, 0U)]
	    | (lit('[') > opComma > lit(']'))[_val = phx::bind(&SParserState::AddExpr, phx::ref(state), SExpression::kOpArraySubscript, _val, _1, 0U)]
	    | (lit('.') > identifier)[_val = phx::bind(&SParserState::AddDotExpr, phx::ref(state), _val, _1, false), _pass = _val != 0U]
	    | (lit("->") > identifier)[_val = phx::bind(&SParserState::AddDotExpr, phx::ref(state), _val, _1, true), _pass = _val != 0U]
	    | (lit('(') > (opAssign % lit(',') | attr(std::vector<uint32_t>())) > lit(')'))[_val = phx::bind(&SParserState::AddListExpr, phx::ref(state), SExpression::kOpFunctionCall, _val, _1)]
	    );
	opSpecial.name("expression_special");

	// Prefix expressions (ie, ++/--/!/+/-/~)
	// Also C-style casts
	opUnary =
	  ((lit("++") > attr(SExpression::kOpPrefixIncrement)
	    | lit("--") > attr(SExpression::kOpPrefixDecrement)
	    | lit('!') > attr(SExpression::kOpNot)
	    | lit('+') > attr(SExpression::kOpPlus)
	    | lit('-') > attr(SExpression::kOpMinus)
	    | lit('~') > attr(SExpression::kOpComplement)
	    ) > opUnary[_val = _1])
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), _1, _2, 0U, 0U)]
	  | ('(' >> typeFull >> ')' >> opUnary[_val = _1])
	  [_val = phx::bind(&SParserState::AddTypedExpr, phx::ref(state), SExpression::kOpCast, _2, _1)]
	  | opSpecial[_val = _1];
	opUnary.name("expression_unary");

	// Binary operators * / %
	opMulDivMod =
	  opUnary[_val = _1] >> *(repo::distinct('=')[lit('*') > attr(SExpression::kOpMultiply) | lit('/') > attr(SExpression::kOpDivide) | lit('%') > attr(SExpression::kOpModulo)] > opUnary)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), _1, _val, _2, 0U)];
	opMulDivMod.name("expression_mul_div_mod");

	// Binary operators + -
	opAddSub =
	  opMulDivMod[_val = _1] >> *(repo::distinct('=')[lit('+') > attr(SExpression::kOpAdd) | lit('-') > attr(SExpression::kOpSubtract)] > opMulDivMod)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), _1, _val, _2, 0U)];
	opAddSub.name("expression_add_sub");

	// Binary operators >> <<
	opShift =
	  opAddSub[_val = _1] >> *((lit("<<") > attr(SExpression::kOpShiftLeft) | lit(">>") > attr(SExpression::kOpShiftRight)) > opAddSub)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), _1, _val, _2, 0U)];
	opShift.name("expression_shift");

	// Comparison binary operators <= < >= >
	opCompareOther =
	  opShift[_val = _1] >> *(opCompareHelper > opShift)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), _1, _val, _2, 0U)];
	opCompareOther.name("expression_compare_rel");

	// Helper to determine whcih comparison operator is used
	// TODO: inline above
	opCompareHelper %=
	  lit('<') >> ((lit('=') > attr(SExpression::kOpCompareLessEqual)) | attr(SExpression::kOpCompareLess))
	  | (lit('>') >> ((lit('=') > attr(SExpression::kOpCompareGreaterEqual)) | attr(SExpression::kOpCompareGreater)));
	opCompareHelper.name("expression_compare_type");

	// Comparison binary operators == !=
	opCompareEq =
	  opCompareOther[_val = _1] >> *((lit("==") > attr(SExpression::kOpCompareEqual) | lit("!=") > attr(SExpression::kOpCompareNotEqual)) > opCompareOther)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), _1, _val, _2, 0U)];
	opCompareEq.name("expression_compare_eq");

	// Binary &
	opBitwiseAnd =
	  opCompareEq[_val = _1] >> *(repo::distinct(char_("&="))[lit('&')] > opCompareEq)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), SExpression::kOpBitAnd, _val, _1, 0U)];
	opBitwiseAnd.name("expression_bit_and");

	// Binary ^
	opBitwiseXor =
	  opBitwiseAnd[_val = _1] >> *(repo::distinct('=')[lit('^')] > opBitwiseAnd)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), SExpression::kOpBitXor, _val, _1, 0U)];
	opBitwiseXor.name("expression_bit_xor");

	// Binary |
	opBitwiseOr =
	  opBitwiseXor[_val = _1] >> *(repo::distinct(char_("|="))[lit('|')] > opBitwiseXor)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), SExpression::kOpBitOr, _val, _1, 0U)];
	opBitwiseXor.name("expression_bit_or");

	// Binary &&
	opLogicalAnd =
	  opBitwiseOr[_val = _1] >> *(lit("&&") > opBitwiseOr)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), SExpression::kOpLogicalAnd, _val, _1, 0U)];
	opLogicalAnd.name("expression_logical_and");

	// Binary ||
	opLogicalOr =
	  opLogicalAnd[_val = _1] >> *(lit("||") > opLogicalAnd)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), SExpression::kOpLogicalOr, _val, _1, 0U)];
	opLogicalOr.name("expression_logical_or");

	// Ternary operator ?:
	opTernary =
	  opLogicalOr[_val = _1] >> *(lit('?') > opComma > lit(':') > opTernary)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), SExpression::kOpTernary, _val, _1, _2)];
	opTernary.name("expression_ternary");

	// Assignment = += -= *= /= %= &= ^= |= <<= >>=
	opAssign =
	  opTernary[_val = _1] >> *(opAssignHelper > opAssign)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), _1, _val, _2, 0U)];
	opAssign.name("expression_assign");

	// Helper to determine actual assignment operator type
	opAssignHelper %=
	  (lit('+') > attr(SExpression::kOpAssignAdd)
	   | lit('-') > attr(SExpression::kOpAssignSubtract)
	   | lit('*') > attr(SExpression::kOpAssignMultiply)
	   | lit('/') > attr(SExpression::kOpAssignDivide)
	   | lit('%') > attr(SExpression::kOpAssignModulo)
	   | lit('&') > attr(SExpression::kOpAssignBitAnd)
	   | lit('^') > attr(SExpression::kOpAssignBitXor)
	   | lit('|') > attr(SExpression::kOpAssignBitOr)
	   | lit("<<") > attr(SExpression::kOpAssignShiftLeft)
	   | lit(">>") > attr(SExpression::kOpAssignShiftRight)
	   | eps > attr(SExpression::kOpAssign)
	  ) >> lit('=') >> !lit('=');
	opAssignHelper.name("expression_assign_helper");

	// Comma operator
	// Note: When comma operator should not be matched (ie, function call, compound initializer, function-cast etc), use opAssign instead.
	// Note: Lowest precedence class
	opComma =
	  opAssign[_val = _1] >> *(lit(',') > opAssign)
	  [_val = phx::bind(&SParserState::AddExpr, phx::ref(state), SExpression::kOpComma, _val, _1, 0U)];
	opComma.name("expression_comma");
}
}
