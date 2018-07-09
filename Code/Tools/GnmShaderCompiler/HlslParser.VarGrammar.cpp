// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "HlslParser.inl"

namespace HlslParser
{

void SParserState::SGrammar::InitVarGrammar(SParserState& state)
{
	// Matches a variable declaration in the given style (takes bool bIsArgument)
	// bIsArgument == true: [StorageClass] [InputModifiers] Type [Identifier] [ArrayDims] [: Semantic] [InterpolationModifier]
	// bIsArgument != true: [StorageClass] [InterpolationModifier] Type Identifier [ArrayDims] [: Semantic] [Annotations]
	varDeclaration %=
	  varDeclarationHelper(_r1, _r2)
	  > (eps(!_r1) | omit[varInterpolationModifier[phx::at_c < 1 > (_val) = _1]]); // This overwrites the interpolation modifier for arguments
	varDeclaration.name("variable_declaration");

	varMultiDeclaration =
	  eps[_val = _r1, phx::at_c < 4 > (_val) = 0U, phx::at_c < 3 > (_val) = _r2] // Copy previous declaration
	  >> identifier[phx::at_c < 5 > (_val) = _1]                                 // Assign new name
	  >> typeArrayTail(phx::at_c<3>(_val));                                      // Assign new type
	varMultiDeclaration.name("variable_declaration_multi");

	varDeclarationHelper %=
	  ((eps(_r1) > attr(uint8_t(0))) | varStorageClass)                // Storage class not allowed on arguments
	  >> ((eps(_r1) > attr(uint8_t(0))) | varInterpolationModifier)    // Interpolation modifiers matched in parent rule for arguments
	  >> ((eps(!_r1) > attr(uint8_t(0))) | varInputModifier)           // Input modifiers only on arguments
	  >> typeBase[_a = &_1, _r2 = _1]                                  // Type always required
	  >> attr(0U)                                                      // No initializer expression has been matched yet
	  >> (identifier | (eps(_r1) > attr(std::string())))               // Optional only for arguments
	  >> typeArrayTail(*_a)                                            // Array tail is optional in the sub-rule
	  >> ((lit(':') > semantic) | attr(SSemantic()))                   // Semantic always optional
	  >> ((eps(_r1) > attr(std::vector<SAnnotation>())) | annotation); // Annotation not allowed on arguments
	varDeclarationHelper.name("variable_declaration_match");

	varStorageClass =
	  eps[_a = kStorageClassNone]
	  > omit[*repo::distinct(alnum | '_')[state.storageClass][_pass = (_a & _1) == 0, _a |= _1]]
	  [_val = _a];
	varStorageClass.name("variable_storage_class");

	varInterpolationModifier =
	  eps[_a = kInterpolationModifierNone]
	  > omit[*repo::distinct(alnum | '_')[state.interpolationModifiers][_pass = (_a & _1) == 0, _a |= _1]]
	  [_val = _a];
	varInterpolationModifier.name("variable_interpolation_modifier");

	varInputModifier =
	  eps[_a = kInputModifierNone]
	  > omit[*repo::distinct(alnum | '_')[state.inputModifiers][_pass = (_a & _1) == 0, _a |= _1]]
	  [_val = _a];
	varInputModifier.name("variable_input_modifier");
}
}
