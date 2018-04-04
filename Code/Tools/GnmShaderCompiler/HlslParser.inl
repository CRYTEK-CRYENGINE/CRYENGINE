#pragma once
#include "HlslParser.h"

#include <fstream>
#include <iostream>

#define BOOST_SPIRIT_USE_PHOENIX_V3
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/repository/include/qi_distinct.hpp>
#include <boost/spirit/repository/include/qi_directive.hpp>
#include <boost/phoenix.hpp>
#include <boost/variant/get.hpp>

namespace HlslParser
{
// Shorthands
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phx = boost::phoenix;
namespace repo = boost::spirit::repository;
using namespace qi;
using ascii::char_;
using ascii::space;
using ascii::alnum;
using ascii::alpha;
using ascii::digit;

// Fixed iterator type for input
typedef boost::spirit::istream_iterator TIterator;

// Matches whitespace (includes C and C++ style comments)
struct SWhitespaceGrammar : qi::grammar<TIterator, void()>
{
	qi::rule<TIterator, void()> start;

	SWhitespaceGrammar();
};

// A table for symbol lookup, maps to an index into some real table
using TSymbolTable = qi::symbols<char, uint32_t>;

// A flag combined with system value enum to indicate it may be followed by a digit-sequence
// POSITION0 maps to SV_Position as well, but POSITION1 doesn't, so a special flag for that one
const uint32_t kSystemValueWithDigits = 0x80000000;
const uint32_t kSystemValueMaxDigitMask = 0x7F000000;
const uint32_t kSystemValueMaxDigitShift = 24;

// Combined with swizzle flags to reject mixed category swizzles.
const uint32_t kSwizzleCategoryXYZW = 0x100;
const uint32_t kSwizzleCategoryRGBA = 0x200;
const uint32_t kSwizzleCategory_MXX = 0x400;
const uint32_t kSwizzleCategory_XX = 0x800;
const uint32_t kSwizzleCategoryMask = 0xF00;

// Combined with member intrinsics for supported object types.
enum EMemberValidity : uint32_t
{
	kTexturesBase                  = sizeof(EMemberIntrinsic) * 8,
	kTexturesCount                 = kBaseTypeRWTexture3D - kBaseTypeTexture1D + 1,
	kBufferBase                    = kTexturesBase + kTexturesCount,
	kBufferCount                   = kBaseTypeRWStructuredBuffer - kBaseTypeAppendStructuredBuffer,
	kOtherBase                     = kBufferBase + kBufferCount,

	kMemberTexture1D               = (1 << (kTexturesBase + kBaseTypeTexture1D - kBaseTypeTexture1D)),
	kMemberTexture1DArray          = (1 << (kTexturesBase + kBaseTypeTexture1DArray - kBaseTypeTexture1D)),
	kMemberTexture2D               = (1 << (kTexturesBase + kBaseTypeTexture2D - kBaseTypeTexture1D)),
	kMemberTexture2DArray          = (1 << (kTexturesBase + kBaseTypeTexture2DArray - kBaseTypeTexture1D)),
	kMemberTexture2DMS             = (1 << (kTexturesBase + kBaseTypeTexture2DMS - kBaseTypeTexture1D)),
	kMemberTexture2DMSArray        = (1 << (kTexturesBase + kBaseTypeTexture2DMSArray - kBaseTypeTexture1D)),
	kMemberTexture3D               = (1 << (kTexturesBase + kBaseTypeTexture3D - kBaseTypeTexture1D)),
	kMemberTextureCube             = (1 << (kTexturesBase + kBaseTypeTextureCube - kBaseTypeTexture1D)),
	kMemberTextureCubeArray        = (1 << (kTexturesBase + kBaseTypeTextureCubeArray - kBaseTypeTexture1D)),
	kMemberRWTexture1D             = (1 << (kTexturesBase + kBaseTypeRWTexture1D - kBaseTypeTexture1D)),
	kMemberRWTexture1DArray        = (1 << (kTexturesBase + kBaseTypeRWTexture1DArray - kBaseTypeTexture1D)),
	kMemberRWTexture2D             = (1 << (kTexturesBase + kBaseTypeRWTexture2D - kBaseTypeTexture1D)),
	kMemberRWTexture2DArray        = (1 << (kTexturesBase + kBaseTypeRWTexture2DArray - kBaseTypeTexture1D)),
	kMemberRWTexture3D             = (1 << (kTexturesBase + kBaseTypeRWTexture3D - kBaseTypeTexture1D)),

	kMemberTexture1DAll            = kMemberTexture1D | kMemberTexture1DArray,
	kMemberTexture2DAllNoMS        = kMemberTexture2D | kMemberTexture2DArray,
	kMemberTexture2DAllMS          = kMemberTexture2DMS | kMemberTexture2DMSArray,
	kMemberTextureCubeAll          = kMemberTextureCube | kMemberTextureCubeArray,
	kMemberTexture2DLike           = kMemberTexture2DAllNoMS | kMemberTextureCubeAll,
	kMemberTextureAllNoMS          = kMemberTexture2DLike | kMemberTexture1DAll | kMemberTexture3D,
	kMemberTextureAll              = kMemberTextureAllNoMS | kMemberTexture2DAllMS,
	kMemberTextureRWAll            = kMemberRWTexture1D | kMemberRWTexture1DArray | kMemberRWTexture2D | kMemberRWTexture2DArray | kMemberRWTexture3D,

	kMemberAppendStructuredBuffer  = (1 << (kBufferBase + kBaseTypeAppendStructuredBuffer - kBaseTypeAppendStructuredBuffer)),
	kMemberBuffer                  = (1 << (kBufferBase + kBaseTypeBuffer - kBaseTypeAppendStructuredBuffer)),
	kMemberByteAddressBuffer       = (1 << (kBufferBase + kBaseTypeByteAddressBuffer - kBaseTypeAppendStructuredBuffer)),
	kMemberConsumeStructuredBuffer = (1 << (kBufferBase + kBaseTypeConsumeStructuredBuffer - kBaseTypeAppendStructuredBuffer)),
	kMemberStructuredBuffer        = (1 << (kBufferBase + kBaseTypeStructuredBuffer - kBaseTypeAppendStructuredBuffer)),
	kMemberRWBuffer                = (1 << (kBufferBase + kBaseTypeRWBuffer - kBaseTypeAppendStructuredBuffer)),
	kMemberRWByteAddressBuffer     = (1 << (kBufferBase + kBaseTypeRWByteAddressBuffer - kBaseTypeAppendStructuredBuffer)),
	kMemberRWStructuredBuffer      = (1 << (kBufferBase + kBaseTypeStructuredBuffer - kBaseTypeAppendStructuredBuffer)),

	kMemberBufferAll               = kMemberAppendStructuredBuffer | kMemberBuffer | kMemberByteAddressBuffer | kMemberConsumeStructuredBuffer | kMemberStructuredBuffer,
	kMemberBufferRWAll             = kMemberRWBuffer | kMemberRWByteAddressBuffer | kMemberRWStructuredBuffer,

	kMemberStreamOutput            = (1 << kOtherBase),
};

// Parser state filled during parsing
struct SParserState
{
	// Last error message
	std::stringstream errorMessage;

	// Output storage
	SProgram& result;

	// Fixed tables
	TSymbolTable keyWords;
	TSymbolTable systemValues;
	TSymbolTable storageClass;
	TSymbolTable interpolationModifiers;
	TSymbolTable inputModifiers;
	TSymbolTable object0Types;     // Not templated
	TSymbolTable object1Types;     // Templated on type
	TSymbolTable object2Types;     // Templated on type + sample/patch size
	TSymbolTable objectMembers;    // Built-in functions on objects
	TSymbolTable attrDomain;
	TSymbolTable attrOutputTopology;
	TSymbolTable attrPartitioning;
	TSymbolTable geometryType;
	TSymbolTable intrinsics;
	TSymbolTable swizzles;

	// Dynamic tables
	TSymbolTable typeNames;
	TSymbolTable variableNames;
	TSymbolTable functionNames;

	// Grammar rules
	struct SGrammar : qi::grammar<TIterator, SWhitespaceGrammar, SProgram::TGlobalScopeObjectVector()>
	{
		SGrammar(SParserState& state, bool bDebug);
		void InitHelpers(SParserState& state, bool bDebug);

		// Note: These are split over multiple CPP files to work around MSVC limitations
		// Specifically, object file size and memory usage during compilation are ridiculous.
		void InitBaseGrammar(SParserState& state);  // Implemented in HlslParser.BaseGrammar.cpp
		void InitTypeGrammar(SParserState& state);  // Implemented in HlslParser.TypeGrammar.cpp
		void InitOpGrammar(SParserState& state);    // Implemented in HlslParser.OpGrammar.cpp
		void InitStatGrammar(SParserState& state);  // Implemented in HlslParser.StatGrammar.cpp
		void InitVarGrammar(SParserState& state);   // Implemented in HlslParser.VarGrammar.cpp

		template<typename ... T> using TRule = qi::rule<TIterator, T ...>;
		template<typename ... T> using TWSRule = qi::rule<TIterator, SWhitespaceGrammar, T ...>;
		template<typename ... T> using TLoc = qi::locals<T ...>;

		enum EIntParseKind
		{
			kIntKindOctal       = 8,
			kIntKindDecimal     = 10,
			kIntKindHexadecimal = 16,
		};

		TWSRule<SType()>                                               typeBase;
		TWSRule<SType()>                                               typeRecurse;
		TRule<SType()>                                                 typeBuiltIn;
		TWSRule<SType()>                                               typeUserDefined;
		TWSRule<SType()>                                               typeVector;
		TWSRule<SType()>                                               typeMatrix;
		TRule<SType()>                                                 typeLegacyVector;
		TRule<SType()>                                                 typeLegacyMatrix;
		TRule<EBaseType()>                                             typeBuiltInBase;
		TRule<uint16_t()>                                              typeDim;
		TWSRule<uint8_t()>                                             typeModifiers;
		TWSRule<uint8_t()>                                             typeIndirection;
		TRule<SType()>                                                 typeObject0;
		TWSRule<SType()>                                               typeObject1;
		TWSRule<SType()>                                               typeObject2;
		TWSRule<void(SType&)>                                          typeArrayTail;
		TWSRule<SType(), TLoc<SType*>>                                 typeFull;
		TWSRule<uint32_t()>                                            opPrimary;
		TWSRule<uint32_t()>                                            opSpecial;
		TWSRule<uint32_t()>                                            opUnary;
		TWSRule<uint32_t()>                                            opMulDivMod;
		TWSRule<uint32_t()>                                            opAddSub;
		TWSRule<uint32_t()>                                            opShift;
		TWSRule<uint32_t()>                                            opCompareOther;
		TWSRule<SExpression::EOperator()>                              opCompareHelper;
		TWSRule<uint32_t()>                                            opCompareEq;
		TWSRule<uint32_t()>                                            opBitwiseAnd;
		TWSRule<uint32_t()>                                            opBitwiseXor;
		TWSRule<uint32_t()>                                            opBitwiseOr;
		TWSRule<uint32_t()>                                            opLogicalAnd;
		TWSRule<uint32_t()>                                            opLogicalOr;
		TWSRule<uint32_t()>                                            opTernary;
		TWSRule<uint32_t()>                                            opAssign;
		TWSRule<SExpression::EOperator()>                              opAssignHelper;
		TWSRule<uint32_t()>                                            opComma;
		TWSRule<STypeName()>                                           typeDef;
		TWSRule<STypeName(), TLoc<SType*>>                             typeDefMatch;
		TWSRule<SSemantic(), TLoc<ESystemValue, SRegister>>            semantic;
		TRule<ESystemValue()>                                          semanticSystemValue;
		TWSRule<SRegister()>                                           semanticPackOffset;
		TRule<uint8_t()>                                               semanticSwizzle;
		TRule<SRegister::ERegisterType()>                              semanticRegisterType;
		TWSRule<SRegister()>                                           semanticRegisterSpec;
		TWSRule<std::vector<SAnnotation>()>                            annotation;
		TWSRule<SType>                                                 annotationStringType;
		TWSRule<SAnnotation>                                           annotationOne;
		TWSRule<uint8_t(), TLoc<uint8_t>>                              varStorageClass;
		TWSRule<uint8_t(), TLoc<uint8_t>>                              varInterpolationModifier;
		TWSRule<uint8_t(), TLoc<uint8_t>>                              varInputModifier;
		TWSRule<SVariableDeclaration(bool, SType&)>                    varDeclaration;       // bIsArgument, outBaseType
		TWSRule<SVariableDeclaration(SVariableDeclaration&, SType&)>   varMultiDeclaration;  // mainDecl, baseType
		TWSRule<SVariableDeclaration(bool, SType&), TLoc<SType*>>      varDeclarationHelper; // bIsArgment, outBaseType
		TWSRule<SStruct()>                                             structRegistered;
		TWSRule<SStruct(), TLoc<SType>>                                structHelper;
		TWSRule<SBuffer()>                                             bufferRegistered;
		TWSRule<SBuffer(), TLoc<SType>>                                bufferHelper;
		TWSRule<SAttribute()>                                          funcAttribute;
		TWSRule<SType()>                                               funcVoidType;
		TWSRule<uint8_t()>                                             funcModifiers;
		TWSRule<SVariableDeclaration(), TLoc<SType>>                   funcArgument;
		TWSRule<uint32_t()>                                            funcPackNumThreads;
		TWSRule<SFunctionDeclaration()>                                funcRegistered;
		TWSRule<SFunctionDeclaration(), TLoc<SType*>>                  funcHelper;
		TWSRule<uint32_t()>                                            statement;
		TWSRule<uint32_t(bool)>                                        statementHelper;
		TWSRule<uint32_t>                                              statementInner;
		TWSRule<uint32_t(), TLoc<SVariableDeclaration, SType>>         statementVar;
		TWSRule<SAttribute()>                                          statementSwitchAttr;
		TWSRule<SAttribute()>                                          statementIfAttr;
		TWSRule<SAttribute()>                                          statementLoopAttr;
		TRule<std::string()>                                           identifier;
		TRule<SFloatLiteral(), TLoc<EBaseType>>                        floatLiteral;
		TRule<SIntLiteral(), TLoc<EIntParseKind, EBaseType, uint64_t>> intLiteral;
		TRule<SBoolLiteral(), TLoc<bool>>                              boolLiteral;
		TRule<SStringLiteral()>                                        stringLiteral;
		TWSRule<SVariableDeclaration(), TLoc<SType>>                   globalVarHelper;
		TWSRule<SProgram::TGlobalScopeObjectVector()>                  program;
	};

	// Scopes (for variables only right now)
	struct SScope
	{
		TSymbolTable previousTable; // The previous variable-name look-up table.
		uint32_t     scopeStart;    // The first index of variable in the new scope.
		uint32_t     firstStatId;   // The first valid statement ID for this scope.
	};
	std::vector<SScope> scopes;

	SParserState(SProgram& target, EShaderType shaderType);
	void                InitTables(EShaderType shaderType); // Implemented in HlslParser.Tables.cpp
	uint16_t            AddType(SType& type);
	bool                AddTypedef(STypeName& item);
	bool                AddStruct(SStruct& item);
	bool                AddVariable(SVariableDeclaration& item, uint32_t initializer);
	bool                AddBuffer(SBuffer& item);
	void                AddFunction(SFunctionDeclaration& item);
	uint32_t            AddExpr(SExpression::EOperator op, uint32_t operand0, uint32_t operand1, uint32_t operand2);
	uint32_t            AddTypedExpr(SExpression::EOperator op, uint32_t operand0, const SType& type);
	uint32_t            AddDotExpr(uint32_t lhs, const std::string& rhs, bool bArrow);
	uint32_t            AddListExpr(SExpression::EOperator op, uint32_t operand1, const std::vector<uint32_t>& elem);
	uint32_t            AddTypedListExpr(SExpression::EOperator op, const SType& type, const std::vector<uint32_t>& elem);
	uint32_t            AddConstExpr(SProgram::TLiteral lit);
	uint32_t            AddStatement(SStatement::EType type, uint32_t exprOrVar, uint32_t stat0, uint32_t stat1, uint32_t stat2);
	uint32_t            AddStatementAttr(SStatement::EType type, boost::optional<SAttribute> attr, uint32_t exprOrVar, uint32_t stat0, uint32_t stat1, uint32_t stat2);
	uint32_t            AddVariableStatement(SVariableDeclaration& decl, uint32_t initializer);
	uint32_t            AddStatementBlock(std::vector<uint32_t> stat);

	uint32_t            AppendVariableStatement(SVariableDeclaration& decl, uint32_t initializer);

	void                BeginScope();
	void                EndScope();

	uint32_t            GetLastRegisteredTypeId();

	uint16_t            RecurseApplyArrayTail(SType& target, uint16_t arrayDim);
	void                ApplyArrayTail(SType& target, uint16_t arrayDim);
	bool                ApplyArrayTailConst(SType& target, uint32_t varId);

	bool                EvalExpression(uint32_t exprId, double& result); // Attempts to evaluate the expression as a compile-time constant

	uint32_t            MakeExpressionList(uint32_t self, const std::vector<uint32_t>& list);
	static void         GetExpressionList(const std::vector<SExpression>& expressions, uint32_t item, std::vector<const SExpression*>& result);

	static uint32_t     PackNumThreads(uint32_t x, uint32_t y, uint32_t z);

	static ESystemValue MakeSystemValue(uint32_t tag, char digit);

	std::string         MakeErrorContext(TIterator pos, TIterator begin, TIterator end);
};

// Required for debug output
inline std::ostream& operator<<(std::ostream& out, const SRegister& reg)
{
	out << "{HlslParser::SRegister}";
	return out;
}
inline std::ostream& operator<<(std::ostream& out, const SType& type)
{
	out << "{HlslParser::SType}";
	return out;
}
inline std::ostream& operator<<(std::ostream& out, const SVariableDeclaration& varDecl)
{
	out << "{HlslParser::SVariableDeclaration}";
	return out;
}

}

// Fusion vector -> struct helpers
BOOST_FUSION_ADAPT_STRUCT(
  HlslParser::SFloatLiteral,
  (std::string, text),
  (double, value),
  (HlslParser::EBaseType, type)
  )

BOOST_FUSION_ADAPT_STRUCT(
  HlslParser::SIntLiteral,
  (std::string, text),
  (uint64_t, value),
  (HlslParser::EBaseType, type)
  )

BOOST_FUSION_ADAPT_STRUCT(
  HlslParser::SStringLiteral,
  (std::string, text)
  )

BOOST_FUSION_ADAPT_STRUCT(
  HlslParser::SBoolLiteral,
  (std::string, text)
    (bool, value)
  )

BOOST_FUSION_ADAPT_STRUCT(
  HlslParser::SType,
  (HlslParser::EBaseType, base),
  (uint8_t, modifier),
  (uint16_t, vecDimOrTypeId),
  (uint16_t, matDimOrSamples),
  (uint16_t, arrDim)
  );

BOOST_FUSION_ADAPT_STRUCT(
  HlslParser::STypeName,
  (HlslParser::SType, type),
  (std::string, name)
  );

BOOST_FUSION_ADAPT_STRUCT(
  HlslParser::SAnnotation,
  (HlslParser::SType, type),
  (std::string, name)
  );

BOOST_FUSION_ADAPT_STRUCT( // In parse order
  HlslParser::SRegister,
  (HlslParser::SRegister::ERegisterType, type),
  (uint16_t, index),
  (uint8_t, spaceOrElement)
  );

BOOST_FUSION_ADAPT_STRUCT(
  HlslParser::SSemantic,
  (std::string, name),
  (HlslParser::SRegister, reg),
  (HlslParser::ESystemValue, sv)
  );

BOOST_FUSION_ADAPT_STRUCT(
  HlslParser::SVariableDeclaration,
  (uint8_t, storage),
  (uint8_t, interpolation),
  (uint8_t, input),
  (HlslParser::SType, type),
  (uint32_t, initializer),
  (std::string, name),
  (HlslParser::SSemantic, semantic),
  (std::vector<HlslParser::SAnnotation>, annotations)
  );

BOOST_FUSION_ADAPT_STRUCT(
  HlslParser::SStruct,
  (std::string, name),
  (std::vector<HlslParser::SVariableDeclaration>, members)
  );

BOOST_FUSION_ADAPT_STRUCT(
  HlslParser::SBuffer,
  (HlslParser::SBuffer::EBufferType, type),
  (std::string, name),
  (HlslParser::SSemantic, semantic),
  (std::vector<HlslParser::SVariableDeclaration>, members)
  );

BOOST_FUSION_ADAPT_STRUCT(
  HlslParser::SAttribute,
  (HlslParser::SAttribute::EType, type),
  (uint32_t, value)
  );

BOOST_FUSION_ADAPT_STRUCT( // In parse order
  HlslParser::SFunctionDeclaration,
  (std::vector<HlslParser::SAttribute>, attributes),
  (uint8_t, functionClass),
  (HlslParser::SType, returnType),
  (std::string, name),
  (HlslParser::EGeometryType, geometryType),
  (std::vector<HlslParser::SVariableDeclaration>, arguments),
  (HlslParser::SSemantic, semantic),
  (uint32_t, definition)
  );
