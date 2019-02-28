// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "HlslParser.inl"

namespace HlslParser
{

void SParserState::SGrammar::InitBaseGrammar(SParserState& state)
{
	// Matches an identifier
	identifier %= !(repo::distinct(alnum | '_')[state.keyWords])    // can't be a keyword
	              >> raw[(alpha | '_') >> *(alnum | '_')];
	identifier.name("identifier");

	// Matches a typedef, including the semi-colon.
	// The typedef is stored as a user-defined type.
	typeDef %= lit("typedef")
	           > typeDefMatch
	           [_pass = phx::bind(&SParserState::AddTypedef, phx::ref(state), _1)];
	typeDef.name("typedef");

	// TODO: Inline above
	typeDefMatch %= typeBase[_a = &_1]
	                > identifier
	                > typeArrayTail(*_a)
	                > lit(';');
	typeDefMatch.name("typedef_match");

	// Matches any semantic (or otherwise meta-specifier introduced with :, such as register identifier)
	semantic %=
	  raw[semanticPackOffset[_b = _1] | semanticRegisterSpec[_b = _1] | semanticSystemValue[_a = _1] | identifier]
	  > attr(_b)
	  > attr(_a);
	semantic.name("semantic");

	semanticSystemValue = no_case[state.systemValues > (digit | attr('0'))][_val = phx::bind(&SParserState::MakeSystemValue, _1, _2), _pass = _val != kSystemValueMax];
	semanticSystemValue.name("semantic_system_value");

	semanticPackOffset %=
	  lit("packoffset")
	  > lit('(')
	  > attr(SRegister::kRegisterPackOffset)
	  > (lit('c') | lit('C'))
	  > no_skip[uint_parser < uint16_t > ()]
	  > ((lit('.') > semanticSwizzle) | attr((uint8_t)0))
	  > lit(')');
	semanticPackOffset.name("semantic_packoffset");

	// Note: There is no reference on what kind of "sub-element" and "element" expression a register(c0???) may have.
	// We're just assuming it can have the same swizzle as packoffset.
	semanticRegisterSpec %= lit("register")
	                        > lit('(')
	                        > semanticRegisterType
	                        > no_skip[uint_parser < uint16_t > ()]
	                        > ((lit('.') > semanticSwizzle) | (lit(',') > lexeme[lit("space") > uint_parser < uint8_t > ()]) | attr((uint8_t)0))
	                        > lit(')');
	semanticRegisterSpec.name("semantic_register");

	// Single-element swizzle for offset
	semanticSwizzle =
	  (lit('r') | lit('x'))
	  [_val = 0]
	  | (lit('g') | lit('y'))
	  [_val = 1]
	  | (lit('b') | lit('z'))
	  [_val = 2]
	  | (lit('a') | lit('w'))
	  [_val = 3];
	semanticSwizzle.name("semantic_swizzle");

	// Register type, either case accepted
	semanticRegisterType =
	  (lit('b') | lit('B'))
	  [_val = SRegister::kRegisterB]
	  | (lit('t') | lit('T'))
	  [_val = SRegister::kRegisterT]
	  | (lit('s') | lit('S'))
	  [_val = SRegister::kRegisterS]
	  | (lit('u') | lit('U'))
	  [_val = SRegister::kRegisterU]
	  | (lit('c') | lit('C'))
	  [_val = SRegister::kRegisterC];
	semanticRegisterType.name("semantic_register_type");

	// Matches a set of annotation, including the <> around them
	annotation %=
	  (lit('<') > *annotationOne > lit('>'))
	  | attr(std::vector<SAnnotation>());
	annotation.name("annotation_set");

	annotationOne %=
	  (typeBase | annotationStringType)
	  > identifier
	  > lit('=')
	  > omit[+~char_(';')] // TODO: Actual expression parse here?
	  > lit(';');
	annotationOne.name("annotation_item");

	// Matches a (potentially anonymous) struct declaration, and registers as user-defined-type.
	// Does not match a semicolon at the end of the declaration.
	structRegistered %=
	  structHelper[_pass = phx::bind(&SParserState::AddStruct, phx::ref(state), _1)];
	structRegistered.name("struct");

	structHelper %= lit("struct")
	                >> (identifier | attr(""))
	                >> lit('{')
	                >> *(varDeclaration(false, _a) >> lit(';'))
	                >> lit('}');
	structHelper.name("struct_match");

	// Matches a cbuffer/tbuffer, including the semicolon at the end of the declaration.
	bufferRegistered %=
	  bufferHelper[_pass = phx::bind(&SParserState::AddBuffer, phx::ref(state), _1)];
	bufferRegistered.name("cbuffer");

	bufferHelper %=
	  ((lit("cbuffer") > attr(SBuffer::kBufferTypeCBuffer)) | (lit("tbuffer") > attr(SBuffer::kBufferTypeTBuffer)))
	  > (identifier | attr(""))
	  > ((lit(':') > semantic) | attr(SSemantic()))
	  > lit('{')
	  > *(varDeclaration(false, _a) > lit(';'))
	  > lit('}')
	  > lit(';');
	bufferHelper.name("cbuffer_match");

	// Matches a function signature or declaration
	// TODO: Currently matches semi-colon or body
	funcRegistered %=
	  funcHelper[phx::bind(&SParserState::AddFunction, phx::ref(state), _1)];
	funcRegistered.name("function_declaration");

	funcAttribute %=
	  (lit("domain") > attr(SAttribute::kAttributeDomain) > lit('(') > state.attrDomain > lit(')'))
	  | (lit("earlydepthstencil") > attr(SAttribute::kAttributeEarlyDepthStencil) > attr(0U))
	  | (lit("instance") > attr(SAttribute::kAttributeInstance) > lit('(') > opTernary > lit(')'))
	  | (lit("maxtessfactor") > attr(SAttribute::kAttributeMaxTessFactor) > lit('(') > opTernary > lit(')'))
	  | (lit("maxvertexcount") > attr(SAttribute::kAttributeMaxVertexCount) > lit('(') > opTernary > lit(')'))
	  | (lit("numthreads") > attr(SAttribute::kAttributeNumThreads) > lit('(') > funcPackNumThreads > lit(')'))
	  | (lit("outputcontrolpoints") > attr(SAttribute::kAttributeOutputControlPoints) > lit('(') > opTernary > lit(')'))
	  | (lit("outputtopology") > attr(SAttribute::kAttributeOutputTopology) > lit('(') > state.attrOutputTopology > lit(')'))
	  | (lit("partitioning") > attr(SAttribute::kAttributePartitioning) > lit('(') > state.attrPartitioning > lit(')'))
	  | (lit("patchconstantfunc") > attr(SAttribute::kAttributePatchConstantFunc) > lit('(') > repo::confix("\"", "\"")[state.functionNames] > lit(')'))
	  | (lit("patchsize") > attr(SAttribute::kAttributePatchSize) > lit('(') > opTernary > lit(')'));
	funcAttribute.name("function_attribute");

	funcPackNumThreads =
	  (uint_parser<uint16_t>()
	   > lit(',')
	   > uint_parser<uint16_t>()
	   > lit(',')
	   > uint_parser<uint16_t>())
	  [_val = phx::bind(&SParserState::PackNumThreads, _1, _2, _3)];
	funcPackNumThreads.name("function_num_threads");

	funcModifiers =
	  eps[_val = kFunctionClassNone]
	  >> *(lit("inline")[_val |= kFunctionClassInline] | lit("precise")[_val |= kFunctionClassPrecise]);
	funcModifiers.name("function_modifiers");

	funcArgument =
	  (varDeclaration(true, _a)[_val = _1] > omit[-(lit('=') > opTernary)[phx::at_c < 4 > (_val) = _1]])
	  [_pass = phx::bind(&SParserState::AddVariable, phx::ref(state), _val, 0U)];
	funcArgument.name("function_argument");

	globalVarHelper =
	  varDeclaration(false, _a)[_val = _1]
	  >> -(lit('=') >> opAssign[phx::at_c < 4 > (_val) = _1])
	  >> lit(';')[_pass = phx::bind(&SParserState::AddVariable, phx::ref(state), _val, 0U)];
	globalVarHelper.name("global_var");

	program %= *(
	  // Note: Cheap to reject
	  (structRegistered >> lit(';'))
	  | bufferRegistered
	  | typeDef
	  // Note: Expensive to reject
	  | funcRegistered
	  | globalVarHelper
	  );
	program.name("program");
}
}
