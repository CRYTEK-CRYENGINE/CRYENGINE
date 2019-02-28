// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "HlslParser.inl"

#define BEGIN_SCOPE phx::bind(&SParserState::BeginScope, phx::ref(state))
#define END_SCOPE   phx::bind(&SParserState::EndScope, phx::ref(state))

namespace HlslParser
{

void SParserState::SGrammar::InitStatGrammar(SParserState& state)
{
	statement %=
	  statementHelper(false);
	statement.name("statement");

	statementHelper =
	  omit[lit(';')
	       [_val = phx::bind(&SParserState::AddStatement, phx::ref(state), SStatement::kEmpty, 0U, 0U, 0U, 0U)]]
	  | omit[(repo::distinct(alnum | '_')[lit("break")] > lit(';'))
	         [_val = phx::bind(&SParserState::AddStatement, phx::ref(state), SStatement::kBreak, 0U, 0U, 0U, 0U)]]
	  | omit[(repo::distinct(alnum | '_')[lit("continue")] > lit(';'))
	         [_val = phx::bind(&SParserState::AddStatement, phx::ref(state), SStatement::kContinue, 0U, 0U, 0U, 0U)]]
	  | omit[(repo::distinct(alnum | '_')[lit("discard")] > lit(';'))
	         [_val = phx::bind(&SParserState::AddStatement, phx::ref(state), SStatement::kDiscard, 0U, 0U, 0U, 0U)]]
	  | omit[(repo::distinct(alnum | '_')[lit("return")] > (opAssign | attr(0U)) > lit(';'))
	         [_val = phx::bind(&SParserState::AddStatement, phx::ref(state), SStatement::kReturn, _1, 0U, 0U, 0U)]]
	  | omit[((-statementSwitchAttr >> repo::distinct(alnum | '_')[lit("switch")])[BEGIN_SCOPE] > lit('(') > statementInner > lit(')') > statementHelper(true))
	         [_val = phx::bind(&SParserState::AddStatementAttr, phx::ref(state), SStatement::kSwitch, _1, 0U, _2, _3, 0U), END_SCOPE]]
	  | omit[(eps[_pass = _r1] > repo::distinct(alnum | '_')[lit("case")] > opComma > lit(':') > !lit(':'))
	         [_val = phx::bind(&SParserState::AddStatement, phx::ref(state), SStatement::kCase, _1, 0U, 0U, 0U)]]
	  | omit[(eps[_pass = _r1] > repo::distinct(alnum | '_')[lit("default")] > lit(':'))
	         [_val = phx::bind(&SParserState::AddStatement, phx::ref(state), SStatement::kDefault, 0U, 0U, 0U, 0U)]]
	  | omit[((-statementIfAttr >> repo::distinct(alnum | '_')[lit("if")])[BEGIN_SCOPE] > lit('(') > statementInner > lit(')') > statementHelper(_r1)[END_SCOPE] > (lit("else") > statementHelper(_r1) | attr(0U)))
	         [_val = phx::bind(&SParserState::AddStatementAttr, phx::ref(state), SStatement::kIf, _1, 0U, _2, _3, _4)]]
	  | omit[((-statementLoopAttr >> repo::distinct(alnum | '_')[lit("for")])[BEGIN_SCOPE] > lit('(') > statementInner > lit(';') > (opComma | attr(0U)) > lit(';') > (opComma | attr(0U)) > lit(')') > statementHelper(_r1))
	         [_val = phx::bind(&SParserState::AddStatement, phx::ref(state), phx::if_else(_4 == 0U, SStatement::kEmpty, SStatement::kExpression), _4, 0U, 0U, 0U),
	          _val = phx::bind(&SParserState::AddStatementAttr, phx::ref(state), SStatement::kFor, _1, _3, _2, _val, _5), END_SCOPE]]
	  | omit[((-statementLoopAttr >> repo::distinct(alnum | '_')[lit("do")])[BEGIN_SCOPE] > statementHelper(_r1) > lit("while")[END_SCOPE] > lit('(') > opComma > lit(')') > lit(';'))
	         [_val = phx::bind(&SParserState::AddStatementAttr, phx::ref(state), SStatement::kDoWhile, _1, _3, _2, 0U, 0U)]]
	  | omit[((-statementLoopAttr >> repo::distinct(alnum | '_')[lit("while")])[BEGIN_SCOPE] > lit('(') > statementInner > lit(')') > statementHelper(_r1))
	         [_val = phx::bind(&SParserState::AddStatementAttr, phx::ref(state), SStatement::kWhile, _1, 0U, _2, _3, 0U), END_SCOPE]]
	  | omit[(lit('{')[BEGIN_SCOPE] > *statementHelper(_r1) > lit('}'))
	         [_val = phx::bind(&SParserState::AddStatementBlock, phx::ref(state), _1), END_SCOPE]]
	  | omit[(opComma > lit(';'))
	         [_val = phx::bind(&SParserState::AddStatement, phx::ref(state), SStatement::kExpression, _1, 0U, 0U, 0U)]]
	  | omit[(statementVar > lit(';'))
	         [_val = _1]];
	statementHelper.name("statement_match");

	statementInner =
	  omit[
	    opComma[_val = phx::bind(&SParserState::AddStatement, phx::ref(state), SStatement::kExpression, _1, 0U, 0U, 0U)]
	    | statementVar[_val = _1]
	    | (&lit(';'))[_val = phx::bind(&SParserState::AddStatement, phx::ref(state), SStatement::kEmpty, 0U, 0U, 0U, 0U)]
	  ];
	statementInner.name("statement_inner");

	statementVar =
	  (varDeclaration(false, _b) > (lit('=') > opAssign | attr(0U)))
	  [_val = phx::bind(&SParserState::AddVariableStatement, phx::ref(state), _1, _2), _a = _1, _pass = _val != 0U]
	  > *omit[(lit(',') > varMultiDeclaration(_a, _b) > (lit('=') > opAssign | attr(0U)))
	          [_val = phx::bind(&SParserState::AppendVariableStatement, phx::ref(state), _1, _2), _pass = _val != 0]];
	statementVar.name("statement_variable");

	statementIfAttr %=
	  lit('[')
	  >> (lit("flatten") > attr(SAttribute::kAttributeFlatten)
	      | lit("branch") > attr(SAttribute::kAttributeBranch))
	  >> attr(0U)
	  >> lit(']');
	statementIfAttr.name("statement_if_attr");

	statementSwitchAttr %=
	  lit('[')
	  >> (lit("flatten") > attr(SAttribute::kAttributeFlatten)
	      | lit("branch") > attr(SAttribute::kAttributeBranch)
	      | lit("forcecase") > attr(SAttribute::kAttributeForceCase)
	      | lit("call") > attr(SAttribute::kAttributeCall))
	  >> attr(0U)
	  >> lit(']');
	statementSwitchAttr.name("statement_switch_attr");

	statementLoopAttr %=
	  lit('[')
	  >> (lit("unroll") > attr(SAttribute::kAttributeUnroll) > (lit('(') > uint_ > lit(')') | attr(0U))
	      | lit("loop") > attr(SAttribute::kAttributeLoop) > attr(0U)
	      | lit("fastopt") > attr(SAttribute::kAttributeFastOpt) > attr(0U)
	      | lit("allow_uav_condition") > attr(SAttribute::kAttributeAllowUavCondition) > attr(0U)
	      )
	  >> lit(']');
	statementLoopAttr.name("statement_loop_attr");

	// Here because of BEGIN_SCOPE/END_SCOPE macro usage
	funcHelper %=
	  *(lit('[') > funcAttribute > lit(']'))
	  >> funcModifiers
	  >> (funcVoidType | typeBase)[_a = &_1]
	  >> identifier
	  >> lit('(')[BEGIN_SCOPE]
	  >> (state.geometryType | attr(0U))
	  >> (funcArgument % lit(',') | attr(std::vector<SVariableDeclaration>()))
	  >> lit(')')
	  >> typeArrayTail(*_a)
	  >> ((lit(':') > semantic) | attr(SSemantic()))
	  >> ((lit(';') > attr(0U)) | statement)[END_SCOPE];
	funcHelper.name("function_match");
}
}

#undef BEGIN_SCOPE
#undef END_SCOPE
