// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// HLSL language parser
// Note: The input must already be pre-processed, no token starting with # is handled.
// The following limitations are currently known:
// - does not parse register specifiers with a target-profile
// - namespaces not supported
// - clipplanes VS attribute not supported
// - function specified for HS patchconstantfunc attribute must be declared before the attribute is used
// - structs cannot be forward declared or "inline" declared inside a variable declaration
// - max one attribute is supported for flow-control statements
// - functions and types may only be declared in the global scope (not inside functions or nested).
// - expression tokens may be tagged as swizzle when they are struct members with a swizzle-compatible name.
// - multiple var declarations in one statement are not supported outside functions (ie, in global or struct scope).
// - the scope associated with variables declared inside the condition of an if statement is the "if branch" statement, not the if statement itself.

#pragma once
#include <vector>
#include <string>
#include <boost/variant.hpp>

namespace HlslParser
{

enum EBaseType : uint8_t
{
	// Placeholder for invalid or missing types
	kBaseTypeInvalid,

	// Built-in types
	kBaseTypeVoid,         // Only allowed as function return type
	kBaseTypeBool,
	kBaseTypeInt,          // Also maps DWORD
	kBaseTypeUInt,         // Also maps dword
	kBaseTypeMin10Float,
	kBaseTypeMin16Float,
	kBaseTypeHalf,
	kBaseTypeFloat,        // Also maps FLOAT
	kBaseTypeDouble,
	kBaseTypeMin12Int,
	kBaseTypeMin16Int,
	kBaseTypeMin16UInt,
	kBaseTypeUInt64,       // uint64_t (SM6.0)
	kBaseTypeString,       // String type is used for annotations only
	kBaseTypeOverloadSet,  // Intermediate expressions only
	kBaseTypeUserDefined,  // Placeholder for structs and type-aliases (anything with a user-defined name)
	kBaseTypeMultiDim,     // Helper for multi-dimensional arrays

	// Sampler types
	kBaseTypeSampler1D,
	kBaseTypeSampler2D,
	kBaseTypeSampler3D,
	kBaseTypeSamplerCUBE,
	kBaseTypeSamplerState, // Also maps sampler_state
	kBaseTypeSamplerComparisonState,

	// Texture types
	kBaseTypeTexture1D,
	kBaseTypeTexture1DArray,
	kBaseTypeTexture2D,
	kBaseTypeTexture2DArray,
	kBaseTypeTexture2DMS,
	kBaseTypeTexture2DMSArray,
	kBaseTypeTexture3D,
	kBaseTypeTextureCube,
	kBaseTypeTextureCubeArray,

	// R/W texture types
	kBaseTypeRWTexture1D,
	kBaseTypeRWTexture1DArray,
	kBaseTypeRWTexture2D,
	kBaseTypeRWTexture2DArray,
	kBaseTypeRWTexture3D,

	// Rasterizer-ordered texture types
	kBaseTypeRasterizerOrderedTexture1D,
	kBaseTypeRasterizerOrderedTexture1DArray,
	kBaseTypeRasterizerOrderedTexture2D,
	kBaseTypeRasterizerOrderedTexture2DArray,
	kBaseTypeRasterizerOrderedTexture3D,

	// Buffer types
	kBaseTypeAppendStructuredBuffer,
	kBaseTypeBuffer,
	kBaseTypeByteAddressBuffer,
	kBaseTypeConsumeStructuredBuffer,
	kBaseTypeStructuredBuffer,

	// R/W buffer types
	kBaseTypeRWBuffer,
	kBaseTypeRWByteAddressBuffer,
	kBaseTypeRWStructuredBuffer,

	// Rasterizer-ordered buffer types
	kBaseTypeRasterizerOrderedBuffer,
	kBaseTypeRasterizerOrderedByteAddressBuffer,
	kBaseTypeRasterizerOrderedStructuredBuffer,

	// Other types
	kBaseTypeInputPatch,
	kBaseTypeOutputPatch,
	kBaseTypePointStream,
	kBaseTypeLineStream,
	kBaseTypeTriangleStream,
};

enum ESystemValue : uint8_t
{
	kSystemValueNone,
	kSystemValueClipDistance, kSystemValueClipDistance0 = kSystemValueClipDistance,
	kSystemValueClipDistance1,
	kSystemValueCullDistance, kSystemValueCullDistance0 = kSystemValueCullDistance,
	kSystemValueCullDistance1,
	kSystemValueCoverage,
	kSystemValueDepth,
	kSystemValueDepthGreaterEqual,
	kSystemValueDepthLessEqual,
	kSystemValueDispatchThreadId,
	kSystemValueDomainLocation,
	kSystemValueGroupId,
	kSystemValueGroupIndex,
	kSystemValueGroupThreadId,
	kSystemValueGsInstanceId,
	kSystemValueInnerCoverage,
	kSystemValueInsideTessFactor,
	kSystemValueInstanceId,
	kSystemValueIsFrontFace,
	kSystemValueOutputControlPointId,
	kSystemValuePosition,
	kSystemValuePrimitiveId,
	kSystemValueRenderTargetArrayIndex,
	kSystemValueSampleIndex,
	kSystemValueStencilRef,
	kSystemValueTarget, kSystemValueTarget0 = kSystemValueTarget,
	kSystemValueTarget1,
	kSystemValueTarget2,
	kSystemValueTarget3,
	kSystemValueTarget4,
	kSystemValueTarget5,
	kSystemValueTarget6,
	kSystemValueTarget7,
	kSystemValueTessFactor,
	kSystemValueVertexId,
	kSystemValueViewportArrayIndex,
	kSystemValueMax,
};

enum EStorageClass : uint8_t
{
	kStorageClassNone            = 0,
	kStorageClassExtern          = (1 << 0), // Excludes static, default for global, added to $Global constant-buffer.
	kStorageClassNoInterpolation = (1 << 1), // Not sure why this is not kInterpolationType
	kStorageClassPrecise         = (1 << 2),
	kStorageClassShared          = (1 << 3), // Compiler-hint, may be ignored
	kStorageClassGroupShared     = (1 << 4), // Compute-shader only
	kStorageClassStatic          = (1 << 5), // Excludes extern, missing initialization implies 0-initialized,
	kStorageClassUniform         = (1 << 6), // Default for global
	kStorageClassVolatile        = (1 << 7), // Compiler-hint, not sure why this is not kTypeModifier
};

enum EFunctionClass : uint8_t
{
	kFunctionClassNone    = 0,
	kFunctionClassInline  = (1 << 0),   // Optional, has no meaning as all functions are treated as inline
	kFunctionClassPrecise = (1 << 1),
};

enum ETypeModifier : uint8_t
{
	kTypeModifierNone        = 0,
	kTypeModifierConst       = (1 << 0), // Default for global, must be initialized
	kTypeModifierRowMajor    = (1 << 1), // Excludes column-major
	kTypeModifierColumnMajor = (1 << 2), // Excludes row-major
	kTypeModifierPointer     = (1 << 3), // Pointer-indirection
	kTypeModifierReference   = (1 << 4), // Reference-indirection
};

enum EInterpolationModifier : uint8_t
{
	kInterpolationModifierNone            = 0,
	kInterpolationModifierLinear          = (1 << 0), // Default for VS export
	kInterpolationModifierCentroid        = (1 << 1), // Must be combined with either linear or noperspective
	kInterpolationModifierNoInterpolation = (1 << 2), // Only valid option for integral types
	kInterpolationModifierNoPerspective   = (1 << 3),
	kInterpolationModifierSample          = (1 << 4),
};

enum EInputModifier : uint8_t
{
	kInputModifierNone    = 0,
	kInputModifierIn      = (1 << 1), // Default if not specified, for top-level function this implies uniform.
	kInputModifierOut     = (1 << 2), // On function return, the value is copied back to the caller.
	kInputModifierUniform = (1 << 3), // Cannot be combined with in or out, added to the $Param constant-buffer. For non-top-level function this is treated as in.
};

enum EGeometryType : uint8_t
{
	kGeometryTypeNone,
	kGeometryTypePoint,
	kGeometryTypeLine,
	kGeometryTypeTriangle,
	kGeometryTypeLineAdj,
	kGeometryTypeTriangleAdj,
};

enum EIntrinsic : uint8_t
{
	kIntrinsicAbort, kIntrinsicErrorF, kIntrinsicPrintF,
	kIntrinsicAbs, kIntrinsicSign,
	kIntrinsicACos, kIntrinsicASin, kIntrinsicATan, kIntrinsicATan2,
	kIntrinsicCos, kIntrinsicCosH, kIntrinsicSin, kIntrinsicSinCos, kIntrinsicSinH, kIntrinsicTan, kIntrinsicTanH,
	kIntrinsicAll, kIntrinsicAny,
	kIntrinsicAllMemoryBarrier, kIntrinsicAllMemoryBarrierWithGroupSync,
	kIntrinsicAsDouble, kIntrinsicAsFloat, kIntrinsicAsInt, kIntrinsicAsUInt,
	kIntrinsicCeil, kIntrinsicFloor, kIntrinsicRound,
	kIntrinsicCheckAccessFullyMapped,
	kIntrinsicClamp, kIntrinsicSaturate,
	kIntrinsicClip,
	kIntrinsicCountBits, kIntrinsicFirstBitHigh, kIntrinsicFirstBitLow, kIntrinsicReverseBits,
	kIntrinsicCross, kIntrinsicDot, kIntrinsicLength, kIntrinsicNormalize,
	kIntrinsicD3DCOLORtoUBYTE4,
	kIntrinsicDDX, kIntrinsicDDXCoarse, kIntrinsicDDXFine, kIntrinsicDDY, kIntrinsicDDYCoarse, kIntrinsicDDYFine,
	kIntrinsicDegrees, kIntrinsicRadians,
	kIntrinsicDeterminant,
	kIntrinsicDeviceMemoryBarrier, kIntrinsicDeviceMemoryBarrierWithGroupSync,
	kIntrinsicDistance, kIntrinsicDst,
	kIntrinsicEvaluateAttributeAtCentroid, kIntrinsicEvaluateAttributeAtSample, kIntrinsicEvaluateAttributeSnapped,
	kIntrinsicExp, kIntrinsicExp2, kIntrinsicPow,
	kIntrinsicF16ToF32, kIntrinsicF32ToF16,
	kIntrinsicFaceForward,
	kIntrinsicFMA, kIntrinsicMad, kIntrinsicMSad4,
	kIntrinsicFMod, kIntrinsicModF, kIntrinsicFWidth,
	kIntrinsicFrac, kIntrinsicFrExp, kIntrinsicLdExp,
	kIntrinsicGetRenderTargetSampleCount, kIntrinsicGetRenderTargetSamplePosition,
	kIntrinsicGroupMemoryBarrier, kIntrinsicGroupMemoryBarrierWithGroupSync,
	kIntrinsicInterlockedAdd, kIntrinsicInterlockedAnd, kIntrinsicInterlockedCompareExchange, kIntrinsicInterlockedCompareStore, kIntrinsicInterlockedExchange,
	kIntrinsicInterlockedMax, kIntrinsicInterlockedMin, kIntrinsicInterlockedOr, kIntrinsicInterlockedXor,
	kIntrinsicIsFinite, kIntrinsicIsInf, kIntrinsicIsNAN,
	kIntrinsicLerp, kIntrinsicSmoothStep, kIntrinsicStep,
	kIntrinsicLit,
	kIntrinsicLog, kIntrinsicLog10, kIntrinsicLog2,
	kIntrinsicMax, kIntrinsicMin,
	kIntrinsicMul, kIntrinsicTranspose,
	kIntrinsicNoise,
	kIntrinsicProcess2DQuadTessFactorsAvg, kIntrinsicProcess2DQuadTessFactorsMax, kIntrinsicProcess2DQuadTessFactorsMin,
	kIntrinsicProcessIsolineTessFactors,
	kIntrinsicProcessQuadTessFactorsAvg, kIntrinsicProcessQuadTessFactorsMax, kIntrinsicProcessQuadTessFactorsMin,
	kIntrinsicProcessTriTessFactorsAvg, kIntrinsicProcessTriTessFactorsMax, kIntrinsicProcessTriTessFactorsMin,
	kIntrinsicRcp, kIntrinsicRSqrt, kIntrinsicSqrt,
	kIntrinsicReflect, kIntrinsicRefract,
	kIntrinsicTex1D, kIntrinsicTex1DBias, kIntrinsicTex1DGrad, kIntrinsicTex1DLod, kIntrinsicTex1DProj,
	kIntrinsicTex2D, kIntrinsicTex2DBias, kIntrinsicTex2DGrad, kIntrinsicTex2DLod, kIntrinsicTex2DProj,
	kIntrinsicTex3D, kIntrinsicTex3DBias, kIntrinsicTex3DGrad, kIntrinsicTex3DLod, kIntrinsicTex3DProj,
	kIntrinsicTexCube, kIntrinsicTexCubeBias, kIntrinsicTexCubeGrad, kIntrinsicTexCubeLod, kIntrinsicTexCubeProj,
	kIntrinsicTrunc,
	// SM 6.0
	kIntrinsicWaveOnce, kIntrinsicWaveGetLaneCount, kIntrinsicWaveGetLaneIndex, kIntrinsicWaveIsHelperLane,
	kIntrinsicWaveAnyTrue, kIntrinsicWaveAllTrue, kIntrinsicWaveAllEqual, kIntrinsicWaveBallot,
	kIntrinsicWaveReadLaneAt, kIntrinsicWaveReadFirstLane,
	kIntrinsicWaveAllSum, kIntrinsicWaveAllProduct, kIntrinsicWaveAllBitAnd, kIntrinsicWaveAllBitOr, kIntrinsicWaveAllBitXor, kIntrinsicWaveAllMin, kIntrinsicWaveAllMax,
	kIntrinsicWavePrefixSum, kIntrinsicWavePrefixProduct,
	kIntrinsicWaveGetOrderedIndex, kIntrinsicGlobalOrderedCountIncrement,
	kIntrinsicQuadReadLaneAt, kIntrinsicQuadSwapX, kIntrinsicQuadSwapY,
};

enum EMemberIntrinsic : uint8_t
{
	kGenericIntrinsicGetDimensions, // (RW)Buffers, (RW)Textures
	kGenericIntrinsicLoad,          // Buffers, Textures
	kGenericIntrinsicIndexOperator, // (RW)Buffers, Textures, Patches
	kTextureIntrinsicCalculateLevelOfDetail, kTextureIntrinsicCalculateLevelOfDetailUnclamped,
	kTextureIntrinsicGather, kTextureIntrinsicGatherRed, kTextureIntrinsicGatherGreen, kTextureIntrinsicGatherBlue, kTextureIntrinsicGatherAlpha,
	kTextureIntrinsicGatherCmp, kTextureIntrinsicGatherCmpRed, kTextureIntrinsicGatherCmpGreen, kTextureIntrinsicGatherCmpBlue, kTextureIntrinsicGatherCmpAlpha,
	kTextureIntrinsicGetSamplePosition,
	kTextureIntrinsicMipsOperator, kTextureIntrinsicSampleOperator,  // mips[] and sample[]
	kTextureIntrinsicSample, kTextureIntrinsicSampleBias, kTextureIntrinsicSampleCmp, kTextureIntrinsicSampleCmpLevelZero, kTextureIntrinsicSampleGrad, kTextureIntrinsicSampleLevel,
	kBufferIntrinsicAppend, kBufferIntrinsicConsume, kBufferIntrinsicRestartStrip,
	kBufferIntrinsicLoad2, kBufferIntrinsicLoad3, kBufferIntrinsicLoad4,
	kBufferIntrinsicStore, kBufferIntrinsicStore2, kBufferIntrinsicStore3, kBufferIntrinsicStore4,
	kBufferIntrinsicIncrementCounter, kBufferIntrinsicDecrementCounter,
};

enum ESwizzle : uint8_t
{
	kSwizzleNone = 0,     // When packing 4 swizzle-pieces into an expression operand, kSwizzleNone is stored to indicate shorter than 4-element swizzle-expressions.

	// XYZW swizzles for vectors.
	kSwizzleX, kSwizzleY, kSwizzleZ, kSwizzleW,

	// RGBA swizzles for vectors.
	kSwizzleR, kSwizzleG, kSwizzleB, kSwizzleA,

	// Zero-based row/column swizzle for matrices.
	kSwizzleM00, kSwizzleM01, kSwizzleM02, kSwizzleM03,
	kSwizzleM10, kSwizzleM11, kSwizzleM12, kSwizzleM13,
	kSwizzleM20, kSwizzleM21, kSwizzleM22, kSwizzleM23,
	kSwizzleM30, kSwizzleM31, kSwizzleM32, kSwizzleM33,

	// One-based row/column swizzle for matrices.
	kSwizzle11, kSwizzle12, kSwizzle13, kSwizzle14,
	kSwizzle21, kSwizzle22, kSwizzle23, kSwizzle24,
	kSwizzle31, kSwizzle32, kSwizzle33, kSwizzle34,
	kSwizzle41, kSwizzle42, kSwizzle43, kSwizzle44,
};

struct SFloatLiteral
{
	std::string text;       // The value, as it appeared in the source text
	double      value;      // The parsed integral value
	EBaseType   type;       // The type indicated by the suffix (or kBaseTypeInvalid)
};

struct SIntLiteral
{
	std::string text;      // The value, as it appeared in the source text
	uint64_t    value;     // The parsed integral value
	EBaseType   type;      // The type indicated by the suffix (or kBaseTypeInvalid)
};

struct SStringLiteral
{
	std::string text;       // The value, as it appeared in the source text (ie, including quotes)
};

struct SBoolLiteral
{
	std::string text;  // The value, as it appeared in the source text (ie, true or false)
	bool        value; // The parsed boolean value
};

struct SType
{
	EBaseType base;                  // The float in float2x3, kBaseTypeUserDefined if not a simple type
	uint8_t   modifier;              // A combination of ETypeModifier flags
	uint16_t  vecDimOrTypeId;        // The 2 in float2x3, 0 if not present, or the type ID if base >= kBaseTypeUserDefined
	uint16_t  matDimOrSamples;       // The 3 in float2x3, 0 if not present, or the number of samples/patches in Texture2DMS<T, N> or OutputPatch<T, N>
	uint16_t  arrDim;                // The number of array elements, 0 if not an array
};

struct STypeName
{
	SType       type;       // The type being referenced
	std::string name;       // The identifier being introduced
};

struct SAnnotation
{
	SType       type;     // The type of the annotation
	std::string name;     // The identifier of the annotation
	uint32_t    expr;     // The expression ID assigned to the annotation
};

struct SRegister
{
	enum ERegisterType : uint8_t
	{
		kRegisterNone       = 0,   // No register found
		kRegisterB          = 'b', // Constant-buffer, subType
		kRegisterT          = 't', // Texture or texture-buffer
		kRegisterS          = 's', // Sampler
		kRegisterU          = 'u', // Unordered-access
		kRegisterC          = 'c', // Buffer offset (inside cbuffer/tbuffer only)
		kRegisterPackOffset = 'C', // Buffer offset (legacy keyword)
	};

	SRegister() : index(0), type(kRegisterNone), spaceOrElement(0) {}

	uint16_t      index;                // The index of the register
	ERegisterType type;                 // Type of register being specified
	uint8_t       spaceOrElement;       // If the type is C or PackOffset, a value [0, 3] to select the vector element, the SM5.1 space otherwise (default: 0)
};

struct SSemantic
{
	SSemantic() : sv(kSystemValueNone) {}

	std::string  name; // The name used, as it appeared in the source text.
	SRegister    reg;  // The register declaration (which may be kRegisterNone if not parsed).
	ESystemValue sv;   // The system value (which may be kSystemValueNone if not parsed).
};

struct SVariableDeclaration
{
	uint8_t                  storage;           // A combination of EStorageClass flags
	uint8_t                  interpolation;     // A combination of EInterpolationModifier flags
	uint8_t                  input;             // A combination of EInputModifier flags
	SType                    type;              // The type of the variable
	uint32_t                 initializer;       // The ExpressionId of the initializer or default argument value (or 0 if not present)
	std::string              name;              // The identifier of the variable
	SSemantic                semantic;          // The semantic applied to the variable (if any)
	std::vector<SAnnotation> annotations;       // The annotations applied to the variable (if any)
	uint32_t                 scope;             // The StatementId of the enclosing scope; 0: global scope; -1: function-forward-declaration.
};

struct SStruct
{
	std::string                       name;          // The name of the struct
	std::vector<SVariableDeclaration> members;       // The members of the struct
};

struct SBuffer
{
	enum EBufferType
	{
		kBufferTypeCBuffer,
		kBufferTypeTBuffer,
	}                                 type;         // The type of the buffer
	SSemantic                         semantic;     // The semantic applied to the buffer
	std::string                       name;         // The name of the buffer
	std::vector<SVariableDeclaration> members;      // The members of the buffer
};

struct SAttribute
{
	enum EType
	{
		kAttributeNone,                  // any: value has no meaning
		kAttributeDomain,                // HS, DS: EDomain value
		kAttributeEarlyDepthStencil,     // PS: value has no meaning
		kAttributeInstance,              // GS: integral value parameter
		kAttributeMaxTessFactor,         // HS: integral value parameter
		kAttributeMaxVertexCount,        // GS: integral value parameter
		kAttributeNumThreads,            // CS: packed fields, use GetNumThreads()
		kAttributeOutputControlPoints,   // HS: integral value parameter
		kAttributeOutputTopology,        // HS: EOutputTopology value
		kAttributePartitioning,          // HS: EPartitioning value
		kAttributePatchConstantFunc,     // HS: FunctionId value (index into SProgram::overloads)
		kAttributePatchSize,             // HS: integral value parameter
		kAttributeFlatten,               // switch, if: value has no meaning
		kAttributeBranch,                // switch, if: value has no meaning
		kAttributeForceCase,             // switch: value has no meaning
		kAttributeCall,                  // switch: value has no meaning
		kAttributeUnroll,                // while, for: integral value parameter or 0 if not specified
		kAttributeLoop,                  // while, for: value has no meaning
		kAttributeFastOpt,               // while, for: value has no meaning
		kAttributeAllowUavCondition,     // while, for: value has no meaning
	};

	enum EDomain   // Used when when type == kAttributeDomain
	{
		kDomainTri,
		kDomainQuad,
		kDomainIsoLine,
	};

	enum EOutputTopology   // Used when type == kAttributeOutputTopology
	{
		kOutputTopologyPoint,
		kOutputTopologyLine,
		kOutputTopologyTriangleCW,
		kOutputTopologyTriangleCCW,
	};

	enum EPartitioning   // Used when type == kAttributePartitioning
	{
		kPartitioningInteger,
		kPartitioningFractionalEven,
		kPartitioningFractionalOdd,
		kPartitioningPow2,
	};

	void GetNumThreads(uint32_t& x, uint32_t& y, uint32_t& z) const   // Used when type == kAttributeNumThreads
	{
		const uint32_t kMask = (1U << 12) - 1;
		x = (value & kMask) + 1U;
		y = ((value >> 12) & kMask) + 1U;
		z = (value >> 24) + 1U;
	}

	EType    type;      // Type of the attribute
	uint32_t value;     // Value meaning depends on attribute type, see EAttributeType
};

struct SFunctionDeclaration
{
	uint8_t                           functionClass; // Combination of EFunctionClass flags
	EGeometryType                     geometryType;  // A geometry type (valid only on GS entry-points)
	SType                             returnType;    // The return type of the function
	std::string                       name;          // The name of the function
	SSemantic                         semantic;      // The semantic (applied to the return type)
	std::vector<SVariableDeclaration> arguments;     // The arguments to the function
	std::vector<SAttribute>           attributes;    // The attributes applied to the function
	uint32_t                          definition;    // The function definition's StatementId (or 0, if this is only a declaration)
};

struct SExpression
{
	enum EOperator : uint32_t
	{
		// Placeholder for invalid operation
		kOpInvalid,

		// Unary terminals, operand[0] is not an ExpressionId, but rather an ...
		kTerminalVariable,      // ... index into SProgram::variables
		kTerminalConstant,      // ... index into SProgram::constants
		kTerminalFunction,      // ... index into SProgram::overloads
		kTerminalIntrinsic,     // ... value from EIntrinsic

		// Binary terminals, operand[1] is not an ExpressionId, but rather an ...
		kOpStructureAccess,     // 0.1  ... index into SProgram::constants
		kOpIndirectAccess,      // 0->1

		// Unary operators, operand[0] is an ExpressionId
		kOpPostfixIncrement,    // 0++
		kOpPostfixDecrement,    // 0--
		kOpPrefixIncrement,     // ++0
		kOpPrefixDecrement,     // --0
		kOpPlus,                // +0
		kOpMinus,               // -0
		kOpNot,                 // !0
		kOpComplement,          // ~0
		kOpSubExpression,       // (0)
		kOpSwizzle,             // 0.swizzle, use GetSwizzle() to retrieve the used swizzle

		// Binary operators, operand[0] and operand[1] are significant
		kOpArraySubscript,      // 0[1]
		kOpMultiply,            // 0*1
		kOpDivide,              // 0/1
		kOpModulo,              // 0%1
		kOpAdd,                 // 0+1
		kOpSubtract,            // 0-1
		kOpShiftLeft,           // 0<<1
		kOpShiftRight,          // 0>>1
		kOpCompareLess,         // 0<1
		kOpCompareLessEqual,    // 0<=1
		kOpCompareGreater,      // 0>1
		kOpCompareGreaterEqual, // 0>=1
		kOpCompareEqual,        // 0==1
		kOpCompareNotEqual,     // 0!=1
		kOpBitAnd,              // 0&1
		kOpBitXor,              // 0^1
		kOpBitOr,               // 0|1
		kOpLogicalAnd,          // 0&&1
		kOpLogicalOr,           // 0||1
		kOpAssign,              // 0=1
		kOpAssignAdd,           // 0+=1
		kOpAssignSubtract,      // 0-=1
		kOpAssignMultiply,      // 0*=1
		kOpAssignDivide,        // 0/=1
		kOpAssignModulo,        // 0%=1
		kOpAssignShiftLeft,     // 0<<=1
		kOpAssignShiftRight,    // 0>>=1
		kOpAssignBitAnd,        // 0&=1
		kOpAssignBitXor,        // 0^=1
		kOpAssignBitOr,         // 0|=1
		kOpComma,               // 0,1

		// Other operators
		// The items that have N child expressions (ie, those marked with "0: first child") can be retrieved using SProgram::GetExpressionList()
		kOpCompound,           // {E0,E1,...EN}, 0: first child
		kOpMemberIntrinsic,    // 0.1, where 1 is EMemberIntrinsic
		kOpFunctionCall,       // 0: first child, 1: function (ExpressionId to either kTerminalFunction, kTerminalIntrinsic or kOpMemberIntrinsic)
		kListIndirection,      // 0: actual child, 2: next child, this and items after this use the 3rd operand
		kOpTernary,            // 0?1:2
		kOpCast,               // (type)0, use GetType() to get the target type
		kOpConstruct,          // T(E0,E1,...EN), 0: first child, use GetType() to get the target type
	};

	EOperator op;             // The operator describing the meaning of the operands
	uint32_t  operand[3];     // ExpressionId of up to 3 operands (unless mentioned otherwise)

	SType GetType() const;                       // For kOpCast and kOpConstruct
	void  GetSwizzle(ESwizzle swizzle[4]) const; // For kOpSwizzle
};

struct SStatement
{
	enum EType
	{
		kEmpty,      // Empty statement: ;
		kVariable,   // Variable declaration: V;
		kExpression, // Expression: E;
		kBreak,      // Flow control: break;
		kContinue,   // Flow control: continue;
		kDiscard,    // Flow control: discard;
		kReturn,     // Flow control: return E;
		kSwitch,     // Flow control: switch (S0) S1
		kCase,       // Flow control: case E:
		kDefault,    // Flow control: default:
		kIf,         // Flow control: [A] if (S0) S1 else S2
		kFor,        // Loop: [A] for (S0; E; S1) S2 (E == 0: no expr)
		kDoWhile,    // Loop: [A] do S0 while(E);
		kWhile,      // Loop: [A] while (S0) S1
		kBlock,      // Block of statements: { S0;...;S1; } (S2 == -1: do not add {})
	};

	EType      type;           // Indicates the meaning of the other fields
	SAttribute attr;           // A
	uint32_t   exprIdOrVarId;  // E or V (E being an index into SProgram::expressions and V being an index into SProgram::variables)
	uint32_t   statementId[3]; // S0, S1, S2
};

enum EShaderType
{
	kShaderTypeVertex,
	kShaderTypePixel,
	kShaderTypeGeometry,
	kShaderTypeDomain,
	kShaderTypeHull,
	kShaderTypeCompute,
};

struct SProgram
{
	// Error message
	std::string errorMessage;

	// Collection of parsed objects (in order of appearance in the input)
	typedef boost::variant
	  <
	    STypeName,            // From typedef
	    SStruct,              // From struct declaration
	    SBuffer,              // From cbuffer/tbuffer keyword
	    SVariableDeclaration, // From global variable declaration
	    SFunctionDeclaration  // From global function declaration
	  >
	  TGlobalScopeObject;
	typedef std::vector<TGlobalScopeObject> TGlobalScopeObjectVector;
	TGlobalScopeObjectVector objects;

	// Collection of all known user-defined types.
	// User-defined SType's can use the TypeId field as an index into this collection.
	typedef boost::variant
	  <
	    STypeName, // From typedef
	    SStruct,   // From struct keyword
	    SType      // From multi-dimensional arrays
	  >
	  TTypeElement;
	typedef std::vector<TTypeElement> TTypeElementVector;
	TTypeElementVector userDefinedTypes;

	// Collection of declared variables.
	// VariableId fields can be used as an index into this collection.
	typedef std::vector<SVariableDeclaration> TVariableVector;
	TVariableVector variables;

	// Collection of constants.
	// Serve as terminals for expressions.
	typedef boost::variant
	  <
	    SFloatLiteral,
	    SIntLiteral,
	    SBoolLiteral,
	    SStringLiteral
	  >
	  TLiteral;
	typedef std::vector<TLiteral> TLiteralVector;
	TLiteralVector constants;

	// Collection of functions overload sets (ie, functions mapped by name).
	// An overload-set identifies instances of distinct SFunctionDeclaration objects in the objects vector.
	// Overload sets are used in function-call expressions and the patchconstantfunc attribute.
	typedef std::vector<uint32_t>     TOverloadSet;
	typedef std::vector<TOverloadSet> TOverloadSetVector;
	TOverloadSetVector overloads;

	// Storage for expressions.
	typedef std::vector<SExpression> TExpressionVector;
	TExpressionVector expressions;

	// Storage for statements.
	typedef std::vector<SStatement> TStatementVector;
	TStatementVector statements;

	// Create program object from given file
	static SProgram Create(const char* szFile, EShaderType shaderType, bool bDebug = false);

	// Expands a type into a base- or struct-type and array-dimensions.
	// The array-dimensions are added from outer-most (ie, largest, appears on the left in HLSL) to inner-most (ie, smallest, appears on the right in HLSL).
	// The resulting type will have arrDim == 0 and base != kBaseTypeMultiDim.
	SType ExpandType(const SType& type, std::vector<uint32_t>& arrayDims) const;

	// Tests if two types are identical, while ignoring the specified ETypeModifier flags (ie, pass None to also test all modifiers)
	bool IsSameType(const SType& type1, const SType& type2, uint8_t typeModifiers = kTypeModifierNone) const;

	// Queries the child expressions for an expression that doesn't have a fixed number of children.
	std::vector<const SExpression*> GetExpressionList(uint32_t listOperand) const;
};
}
