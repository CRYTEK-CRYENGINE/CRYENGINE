// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "HlslParser.inl"

#define TYPE_FIELD(placeHolder, fieldName) phx::at_c<fieldName>(placeHolder)

namespace HlslParser
{

// A number of rules deal with packing/modifying SType via Phoenix at_c
// These are the IDs of those fields so the code is more readable.
enum ETypeField
{
	kTypeFieldBase,
	kTypeFieldModifier,
	kTypeFieldTypeId,
	kTypeFieldVecDim = kTypeFieldTypeId,
	kTypeFieldSamples,
	kTypeFieldMatDim = kTypeFieldSamples,
	kTypeFieldArrDim,
};

void SParserState::SGrammar::InitTypeGrammar(SParserState& state)
{
	// Matches a floating point literal (but no sign)
	// TODO: Parse value
	floatLiteral %=
	  raw[
	    no_case[
	      (
	        (+digit >> &lit('e'))            // Match ddd (followed by E, or it would be integral)
	        | (*digit >> lit('.') >> +digit) // Or match [ddd].ddd
	        | (+digit >> lit('.'))           // Or match ddd.
	      )
	      > (                             // Optionally match the whole exponent
	        lit('e')                      // Match E
	        > (lit('-') | lit('+') | eps) // Optional exponent sign
	        > +digit                      // Exponent value
	        | eps
	        )
	      > (
	        lit('h')[_a = kBaseTypeHalf]
	        | lit('f')[_a = kBaseTypeFloat]
	        | lit('l')[_a = kBaseTypeDouble]
	        | eps[_a = kBaseTypeInvalid]
	        )
	    ]] > attr(0.0) > attr(_a);
	floatLiteral.name("float_literal");

	// Matches an integral literal (but no sign)
	intLiteral %=
	  raw[
	    (eps[_a = kIntKindDecimal, _b = kBaseTypeInvalid, _c = 0U]                                    // Initialize locals
	     >> -(lit('0') > ((lit('x') | lit('X'))[_a = kIntKindHexadecimal] | eps[_a = kIntKindOctal])) // Classify kind
	     >> ((eps[_pass = _a == kIntKindHexadecimal] > hex[_c = _1])                                  // Parse hex digits
	         | repeat(_a != kIntKindOctal, inf)[(digit[_c = (_c * _a) + (_1 - '0')])])                // Add octal/decimal digits
	     >> !(lit('e') | lit('E') | lit('.')))                                                        // Refuse float literals
	    > -((lit('u') | lit('U'))[_b = kBaseTypeUInt] | (lit('l') | lit('L'))[_b = kBaseTypeInt])     // Type suffix
	  ] > attr(_c) > attr(_b);
	intLiteral.name("int_literal");

	// Matches a string literal (including quotes)
	stringLiteral %=
	  as_string[raw[
	              lit('\"')                          // opening quote
	              > *((lit('\\') > (print | +space)) // any escape sequence, including newline
	                  | (!lit('\"') > print))        // anything that's not a quote
	              > lit('\"')                        // closing quote
	            ]];
	stringLiteral.name("string_literal");

	// Matches a boolean literal
	boolLiteral %=
	  (string("true")[_a = true] | string("false")[_a = false])
	  > attr(_a);
	boolLiteral.name("bool_literal");

	// Matches any type (built-in or user-defined), except the array specifier.
	// See also: typeFull and typeArrayTail.
	typeBase %=
	  omit[
	    typeModifiers[TYPE_FIELD(_val, kTypeFieldModifier) = _1]
	  ] >> (
	    typeBuiltIn
	    | typeUserDefined
	    | typeVector
	    | typeMatrix
	    | typeLegacyVector
	    | typeLegacyMatrix
	    | typeObject0
	    | typeObject1
	    | typeObject2
	    )
	  [TYPE_FIELD(_val, kTypeFieldArrDim) = 0U]
	  >> omit[
	    typeIndirection[TYPE_FIELD(_val, kTypeFieldModifier) |= _1]
	  ];
	typeBase.name("type_base");

	// Object1 and Object2 types recurse into this rule to match the inner type.
	// We have to reject ObjectX types inside them, so this is a smaller matching set than the main rule.
	typeRecurse %=
	  omit[
	    typeModifiers[TYPE_FIELD(_val, kTypeFieldModifier) = _1]
	  ] >> (
	    typeBuiltIn
	    | typeUserDefined
	    | typeVector
	    | typeMatrix
	    | typeLegacyVector
	    | typeLegacyMatrix
	    )
	  [TYPE_FIELD(_val, kTypeFieldArrDim) = 0U]
	  >> omit[
	    typeIndirection[TYPE_FIELD(_val, kTypeFieldModifier) |= _1]
	  ];
	typeRecurse.name("type_recurse");

	// Matches types like floatNxN
	typeBuiltIn =
	  repo::distinct(alnum | '_')[
	    typeBuiltInBase
	    [TYPE_FIELD(_val, kTypeFieldBase) = _1]
	    >> typeDim
	    [TYPE_FIELD(_val, kTypeFieldVecDim) = _1]
	    >> ((lit('x') >> typeDim) | attr(uint16_t(0)))
	    [TYPE_FIELD(_val, kTypeFieldMatDim) = _1]
	  ];
	typeBuiltIn.name("type_builtin");

	// Matches previously or inline declared user-defined-types
	typeUserDefined =
	  (
	    (omit[structRegistered] > attr(phx::bind(&SParserState::GetLastRegisteredTypeId, phx::ref(state))))
	    | (repo::distinct(alnum | '_')[state.typeNames])
	  )
	  [TYPE_FIELD(_val, kTypeFieldBase) = kBaseTypeUserDefined]
	  [TYPE_FIELD(_val, kTypeFieldTypeId) = _1]
	  [TYPE_FIELD(_val, kTypeFieldMatDim) = 0U];
	typeUserDefined.name("type_user_defined");

	// Matches vector<T, N>
	typeVector =
	  (lit("vector") >> lit('<'))
	  > typeBuiltInBase
	  [TYPE_FIELD(_val, kTypeFieldBase) = _1]
	  > lit(',')
	  > typeDim
	  [TYPE_FIELD(_val, kTypeFieldVecDim) = _1]
	  [TYPE_FIELD(_val, kTypeFieldMatDim) = 0U]
	  [_pass = (_1 != 0U)]
	  > lit('>');
	typeVector.name("type_vector_template");

	// Matches matrix<T, N, N>
	typeMatrix =
	  (lit("matrix") >> lit('<'))
	  > typeBuiltInBase
	  [TYPE_FIELD(_val, kTypeFieldBase) = _1]
	  > lit(',')
	  > typeDim
	  [TYPE_FIELD(_val, kTypeFieldVecDim) = _1]
	  [_pass = (_1 != 0U)]
	  > lit(',')
	  > typeDim
	  [TYPE_FIELD(_val, kTypeFieldMatDim) = _1]
	  [_pass = (_1 != 0U)]
	  > lit('>');
	typeMatrix.name("type_matrix_template");

	// Matches upper-case VECTOR for legacy
	typeLegacyVector =
	  lit("VECTOR")
	  [TYPE_FIELD(_val, kTypeFieldBase) = kBaseTypeFloat]
	  [TYPE_FIELD(_val, kTypeFieldVecDim) = 4U]
	  [TYPE_FIELD(_val, kTypeFieldMatDim) = 0U];
	typeLegacyVector.name("type_legacy_vector");

	// Matches upper-case MATRIX for legacy
	typeLegacyMatrix =
	  lit("MATRIX")
	  [TYPE_FIELD(_val, kTypeFieldBase) = kBaseTypeFloat]
	  [TYPE_FIELD(_val, kTypeFieldVecDim) = 4U]
	  [TYPE_FIELD(_val, kTypeFieldMatDim) = 4U];
	typeLegacyMatrix.name("type_legacy_matrix");

	// Matches non-templated object types (ie, SamplerState, ByteAddressBuffer)
	typeObject0 =
	  repo::distinct(alnum | '_')[state.object0Types]
	  [TYPE_FIELD(_val, kTypeFieldBase) = phx::static_cast_ < EBaseType > (_1)]
	  [TYPE_FIELD(_val, kTypeFieldTypeId) = 0U]
	  [TYPE_FIELD(_val, kTypeFieldSamples) = 0U];
	typeObject0.name("type_object_0");

	// Matches optionally-templated object types (ie, Texture2D, StructuredBuffer) with 1 template parameter.
	// Note that we leave the TypeId field at "invalid placeholder" here, if no template is specified, it would require some trickery to get the default type as it may depend on the matched object.
	// We just leave this as an exercise for the user of the parsed data for now.
	typeObject1 =
	  repo::distinct(alnum | '_')[state.object1Types]
	  [TYPE_FIELD(_val, kTypeFieldBase) = phx::static_cast_ < EBaseType > (_1)]
	  [TYPE_FIELD(_val, kTypeFieldTypeId) = 0U]
	  [TYPE_FIELD(_val, kTypeFieldSamples) = 0U]
	  > -(lit('<')
	      > typeRecurse[TYPE_FIELD(_val, kTypeFieldTypeId) = phx::bind(&SParserState::AddType, phx::ref(state), _1)]
	      > lit('>'));
	typeObject1.name("type_object_1");

	// Matches required-templated object types (ie, Texture2DMS, OutputPatch) with 2 template parameters.
	// Note: Starting with SM5.0, it's allowed to drop the samples field, in this case we store a 0.
	typeObject2 =
	  repo::distinct(alnum | '_')[state.object2Types]
	  [TYPE_FIELD(_val, kTypeFieldBase) = phx::static_cast_ < EBaseType > (_1)]
	  > lit('<')
	  > typeRecurse[TYPE_FIELD(_val, kTypeFieldTypeId) = phx::bind(&SParserState::AddType, phx::ref(state), _1)]
	  > ((lit(',') > uint_parser<uint16_t>() | attr(uint16_t(0)))[TYPE_FIELD(_val, kTypeFieldSamples) = _1])
	  > lit('>');
	typeObject2.name("type_object_2");

	// Matches a base-type
	typeBuiltInBase =
	  lit("bool")
	  [_val = kBaseTypeBool]
	  | (lit("int") | lit("DWORD"))             // The (legacy) uppercase DWORD is signed int
	  [_val = kBaseTypeInt]
	  | (lit("uint") | lit("dword"))            // The lowercase dword is unsigned int
	  [_val = kBaseTypeUInt]
	  | (lit("unsigned int") | lit("unsigned")) // C style
	  [_val = kBaseTypeUInt]
	  | lit("half")
	  [_val = kBaseTypeHalf]
	  | (lit("float") | lit("FLOAT"))           // Legacy uppercase FLOAT
	  [_val = kBaseTypeFloat]
	  | lit("double")
	  [_val = kBaseTypeDouble]
	  | lit("min16float")
	  [_val = kBaseTypeMin16Float]
	  | lit("min10float")
	  [_val = kBaseTypeMin10Float]
	  | lit("min16int")
	  [_val = kBaseTypeMin16Int]
	  | lit("min12int")
	  [_val = kBaseTypeMin12Int]
	  | lit("min16uint")
	  [_val = kBaseTypeMin16UInt]
	  | lit("uint64_t")
	  [_val = kBaseTypeUInt64];
	typeBuiltInBase.name("type_base");

	typeDim =
	  lit('1')
	  [_val = 1U]
	  | lit('2')
	  [_val = 2U]
	  | lit('3')
	  [_val = 3U]
	  | lit('4')
	  [_val = 4U]
	  | eps
	  [_val = 0U];
	typeDim.name("type_dim");

	typeModifiers =
	  eps[_val = kTypeModifierNone]
	  >> (-(lit("const")[_val |= kTypeModifierConst])                   // Optionally match const (but no volatile, as that is a storage-class)
	      ^ (lit("row_major")[_val |= kTypeModifierRowMajor]            // Optionally match row_major (before or after optional const, and exclude column_major)
	         | lit("column_major")[_val |= kTypeModifierColumnMajor])); // Optionally match column_major (before or after optional const, and exclude row_major)
	typeModifiers.name("type_modifiers");

	typeIndirection =
	  eps[_val = kTypeModifierNone]
	  >> -lit('*')[_val |= kTypeModifierPointer]
	  >> -lit('&')[_val |= kTypeModifierReference];
	typeIndirection.name("type_indirection");

	// Matches any number of array specifiers at the tail of a declaration
	// This modifies the type previously declared on the front of the declaration.
	typeArrayTail =
	  *(lit('[')
	    > opComma[_pass = phx::bind(&SParserState::ApplyArrayTailConst, phx::ref(state), _r1, _1)]
	    > lit(']')
	    );
	typeArrayTail.name("type_array_tail");

	// Matches any type, including an array specifier
	// See also: typeBase, typeArrayTail
	typeFull %=
	  typeBase[_a = &_1]
	  > typeArrayTail(*_a);
	typeFull.name("type_full");

	// Here because TYPE_FIELD macro
	annotationStringType =
	  lit("string")
	  [TYPE_FIELD(_val, kTypeFieldBase) = kBaseTypeString]
	  [TYPE_FIELD(_val, kTypeFieldVecDim) = 0U]
	  [TYPE_FIELD(_val, kTypeFieldMatDim) = 0U]
	  [TYPE_FIELD(_val, kTypeFieldArrDim) = 0U]
	  [TYPE_FIELD(_val, kTypeFieldModifier) = kTypeModifierNone];
	annotationStringType.name("annotation_string");

	// Here because TYPE_FIELD macro
	funcVoidType =
	  lit("void")
	  [TYPE_FIELD(_val, kTypeFieldBase) = kBaseTypeVoid]
	  [TYPE_FIELD(_val, kTypeFieldModifier) = kTypeModifierNone]
	  [TYPE_FIELD(_val, kTypeFieldVecDim) = 0U]
	  [TYPE_FIELD(_val, kTypeFieldMatDim) = 0U]
	  [TYPE_FIELD(_val, kTypeFieldArrDim) = 0U];
	funcVoidType.name("function_type_void");
}
}

#undef TYPE_FIELD
