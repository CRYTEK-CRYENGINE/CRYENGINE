// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "PsslGenerator.h"
#include <sstream>

#define LEGACY_SAMPLER         "__LegacyWrappedSampler"
#define LEGACY_SAMPLER_AFFIX_S "_S"
#define LEGACY_SAMPLER_AFFIX_T "_T"

struct SContext
{
	const HlslParser::SProgram& hlsl;
	const SPsslConversion&      options;
	std::stringstream           output;
	uint32_t                    indent;
	mutable uint32_t            legacySamplerUsed;
	const HlslParser::SStruct*  pOutputStruct;
	std::vector<std::string>    emittedStructs;

	SContext(const HlslParser::SProgram& hlsl, const SPsslConversion& options)
		: hlsl(hlsl)
		, options(options)
		, indent(0)
		, legacySamplerUsed(0)
		, pOutputStruct(nullptr)
	{}

	std::string Indent(uint32_t extra = 0) const
	{
		return std::string((indent + extra) * (options.bIndentWithTabs ? 1 : 4), options.bIndentWithTabs ? '\t' : ' ');
	}

	enum ETypeFlags
	{
		kTypeForceStorage       = (1 << 0),
		kTypeAllowVoid          = (1 << 1),
		kTypeAllowBool          = (1 << 2),
		kTypeAllowSmallFloat    = (1 << 3),
		kTypeAllowDouble        = (1 << 4),
		kTypeAllowSmallInt      = (1 << 5),
		kTypeAllowConst         = (1 << 6),
		kTypeAllowUserDefined   = (1 << 7),
		kTypeAllowSampler       = (1 << 8),
		kTypeAllowTexture       = (1 << 9),
		kTypeAllowBuffer        = (1 << 10),
		kTypeAllowPatch         = (1 << 11),
		kTypeAllowStream        = (1 << 12),
		kTypeAllowFloat         = (1 << 13),
		kTypeAllowInt           = (1 << 14),
		kTypeAllowArray         = (1 << 15),
		kTypeHalfAsFloat        = (1 << 16), // Half (and smaller) floats are written out as float
		kTypeSmallAsInt         = (1 << 17), // 8 and 16 bit integrals are written out as 32-bit integrals
		kTypeBoolAsInt          = (1 << 18), // bool is written out as 32-bit int
		kTypeIgnoreConst        = (1 << 19), // const keyword, if any, is discarded
		kTypeForceConst         = (1 << 20), // const keyword is added, if not already present
		kTypeAllowLegacySampler = (1 << 21), // sampler1D, sampler2D, sampler3D, samplerCUBE types
		kTypeAllowPointer       = (1 << 22), // pointer-indirection
		kTypeAllowReference     = (1 << 23), // reference-indirection

		kTypeAllowAllIntegral   = kTypeAllowBool | kTypeAllowSmallInt | kTypeAllowInt,
		kTypeAllowAllFloating   = kTypeAllowSmallFloat | kTypeAllowFloat | kTypeAllowDouble,

		kTypeStruct             = kTypeAllowAllIntegral | kTypeAllowAllFloating | kTypeAllowUserDefined | kTypeAllowArray | kTypeAllowUserDefined | kTypeAllowSampler | kTypeAllowBuffer | kTypeAllowTexture | kTypeAllowPointer | kTypeForceStorage,
		kTypeBuffer             = kTypeAllowInt | kTypeAllowFloat | kTypeSmallAsInt | kTypeHalfAsFloat | kTypeAllowArray | kTypeForceStorage | kTypeAllowConst | kTypeAllowUserDefined,
		kTypeGlobal             = kTypeAllowAllIntegral | kTypeAllowAllFloating | kTypeAllowUserDefined | kTypeAllowArray | kTypeAllowConst | kTypeForceStorage,
		kTypeResource           = kTypeAllowSampler | kTypeAllowTexture | kTypeAllowBuffer | kTypeForceStorage | kTypeAllowArray,
		kTypeLocal              = kTypeAllowAllIntegral | kTypeAllowAllFloating | kTypeAllowSampler | kTypeAllowTexture | kTypeAllowBuffer | kTypeAllowConst | kTypeAllowUserDefined | kTypeAllowArray | kTypeAllowLegacySampler | kTypeForceStorage,
		kTypeArgument           = kTypeLocal | kTypeAllowPatch | kTypeAllowStream | kTypeAllowLegacySampler | kTypeForceStorage,
		kTypeFunc               = kTypeLocal | kTypeAllowVoid | kTypeAllowLegacySampler | kTypeForceStorage,
	};

	std::vector<uint32_t> AddType(const HlslParser::SType& type, int flags, std::stringstream& output) const
	{
		if (!options.bSupportLegacySamplers)
		{
			flags &= ~kTypeAllowLegacySampler;
		}
		if (!options.bSupportSmallTypes)
		{
			if (flags & kTypeAllowSmallFloat)
			{
				flags |= kTypeHalfAsFloat;
			}
			if (flags & kTypeAllowSmallInt)
			{
				flags |= kTypeSmallAsInt;
			}
		}
		std::vector<uint32_t> dims;
		HlslParser::SType base = hlsl.ExpandType(type, dims);
		if ((flags & kTypeAllowArray) == 0 && !dims.empty())
		{
			throw std::runtime_error("Array type not allowed in this context");
		}
		if (((base.modifier & HlslParser::kTypeModifierConst) || (flags & kTypeForceConst)) && (flags & kTypeIgnoreConst) == 0)
		{
			if (flags & (kTypeAllowConst | kTypeForceConst))
			{
				output << "const ";
			}
			else
			{
				std::runtime_error("Const type not allowed to appear in this context");
			}
		}
		if (base.matDimOrSamples
		    && base.base != HlslParser::kBaseTypeTexture2DMS
		    && base.base != HlslParser::kBaseTypeTexture2DMSArray
		    && base.base != HlslParser::kBaseTypeInputPatch
		    && base.base != HlslParser::kBaseTypeOutputPatch)
		{
			if ((flags & kTypeForceStorage) && (base.modifier & (HlslParser::kTypeModifierRowMajor | HlslParser::kTypeModifierColumnMajor)) == 0)
			{
				base.modifier |= (options.bMatrixStorageRowMajor ? HlslParser::kTypeModifierRowMajor : HlslParser::kTypeModifierColumnMajor);
			}
			if (base.modifier & HlslParser::kTypeModifierRowMajor)
			{
				output << "row_major ";
			}
			else if (base.modifier & HlslParser::kTypeModifierColumnMajor)
			{
				output << "column_major ";
			}
		}
		bool bBasic = false;
		bool bTemplate = false;
		bool bSamples = false;
		switch (base.base)
		{
		case HlslParser::kBaseTypeVoid:
			if (flags & kTypeAllowVoid)
			{
				output << "void";
			}
			else
			{
				throw std::runtime_error("Void type not allowed to appear in this context");
			}
			break;
		case HlslParser::kBaseTypeBool:
			if ((flags & kTypeBoolAsInt) == 0)
			{
				if (flags & kTypeAllowBool)
				{
					output << "bool";
					bBasic = true;
					break;
				}
				else
				{
					throw std::runtime_error("Boolean type not allowed to appear in this context");
				}
			}
		// Fall through
		case HlslParser::kBaseTypeMin12Int:
		case HlslParser::kBaseTypeMin16Int:
		case HlslParser::kBaseTypeMin16UInt:
			if ((flags & kTypeSmallAsInt) != 0 && base.base != HlslParser::kBaseTypeBool)
			{
				if (flags & kTypeAllowSmallInt)
				{
					output << (base.base == HlslParser::kBaseTypeMin16UInt ? "ushort" : "short");
					bBasic = true;
				}
				else
				{
					throw std::runtime_error("Small-integral type not allowed in this context");
				}
				break;
			}
		// Fall through
		case HlslParser::kBaseTypeInt:
		case HlslParser::kBaseTypeUInt:
		case HlslParser::kBaseTypeUInt64:
			if (flags & kTypeAllowInt)
			{

				output << (base.base == HlslParser::kBaseTypeUInt64 ? "ulong" : base.base != HlslParser::kBaseTypeUInt && base.base != HlslParser::kBaseTypeMin16UInt ? "int" : "uint");
				bBasic = true;
			}
			else
			{
				throw std::runtime_error("Integral type not allowed to appear in this context");
			}
			break;
		case HlslParser::kBaseTypeMin10Float:
		case HlslParser::kBaseTypeMin16Float:
		case HlslParser::kBaseTypeHalf:
			if ((flags & kTypeHalfAsFloat) == 0)
			{
				if (flags & kTypeAllowSmallFloat)
				{
					output << "half";
					bBasic = true;
					break;
				}
				else
				{
					throw std::runtime_error("Small-float type not allowed to appear in this context");
				}
			}
		// Fall through
		case HlslParser::kBaseTypeFloat:
			if (flags & kTypeAllowFloat)
			{
				output << "float";
				bBasic = true;
			}
			else
			{
				throw std::runtime_error("Float type not allowed to appear in this context");
			}
			break;
		case HlslParser::kBaseTypeDouble:
			if (flags & kTypeAllowDouble)
			{
				output << "double";
				bBasic = true;
			}
			else
			{
				throw std::runtime_error("Double-precision float not allowed to appear in this context");
			}
			break;
		case HlslParser::kBaseTypeUserDefined:
			if (flags & kTypeAllowUserDefined)
			{
				const auto& udt = hlsl.userDefinedTypes.at(base.vecDimOrTypeId);
				if (auto* pTypeDef = boost::get<HlslParser::STypeName>(&udt))
				{
					output << pTypeDef->name;
				}
				else if (auto* pStruct = boost::get<HlslParser::SStruct>(&udt))
				{
					if (std::find(emittedStructs.begin(), emittedStructs.end(), pStruct->name) != emittedStructs.end())
					{
						assert(!pStruct->name.empty());
						output << pStruct->name;
					}
					else
					{
						AddStruct(*pStruct, true, output);
						output << " ";
					}
				}
				else
				{
					throw std::runtime_error("Cannot emit user-defined-type");
				}
			}
			else
			{
				throw std::runtime_error("User-defined type not allowed to appear in this context");
			}
			break;
		case HlslParser::kBaseTypeSampler1D:
		case HlslParser::kBaseTypeSampler2D:
		case HlslParser::kBaseTypeSampler3D:
		case HlslParser::kBaseTypeSamplerCUBE:
			if ((flags & kTypeAllowSampler) && (flags & kTypeAllowLegacySampler))
			{
				if (base.base == HlslParser::kBaseTypeSamplerCUBE)
				{
					output << LEGACY_SAMPLER "Cube";
				}
				else
				{
					output << LEGACY_SAMPLER;
					output << (char)(base.base - HlslParser::kBaseTypeSampler1D + '1');
					output << 'D';
				}
				legacySamplerUsed |= 1 << (base.base - HlslParser::kBaseTypeSampler1D);
			}
			else
			{
				throw std::runtime_error("Legacy sampler type not allowed in this context");
			}
			break;
		case HlslParser::kBaseTypeSamplerState:
		case HlslParser::kBaseTypeSamplerComparisonState:
			if (flags & kTypeAllowSampler)
			{
				output << (base.base == HlslParser::kBaseTypeSamplerState ? "SamplerState" : "SamplerComparisonState");
			}
			else
			{
				throw std::runtime_error("Sampler type not allowed to appear in this context");
			}
			break;
		case HlslParser::kBaseTypeTexture2DMS:
		case HlslParser::kBaseTypeTexture2DMSArray:
			bSamples = true;
		// Fall through
		case HlslParser::kBaseTypeTexture1D:
		case HlslParser::kBaseTypeTexture1DArray:
		case HlslParser::kBaseTypeTexture2D:
		case HlslParser::kBaseTypeTexture2DArray:
		case HlslParser::kBaseTypeTexture3D:
		case HlslParser::kBaseTypeTextureCube:
		case HlslParser::kBaseTypeTextureCubeArray:
		case HlslParser::kBaseTypeRWTexture1D:
		case HlslParser::kBaseTypeRWTexture1DArray:
		case HlslParser::kBaseTypeRWTexture2D:
		case HlslParser::kBaseTypeRWTexture2DArray:
		case HlslParser::kBaseTypeRWTexture3D:
			if (flags & kTypeAllowTexture)
			{
				static const char* szTexTypes[] =
				{
					"Texture1D",
					"Texture1D_Array",
					"Texture2D",
					"Texture2D_Array",
					"MS_Texture2D",
					"MS_Texture2D_Array",
					"Texture3D",
					"TextureCube",
					"TextureCube_Array",
					"RW_Texture1D",
					"RW_Texture1D_Array",
					"RW_Texture2D",
					"RW_Texture2D_Array",
					"RW_Texture3D"
				};
				output << szTexTypes[base.base - HlslParser::kBaseTypeTexture1D];
				bTemplate = true;
			}
			else
			{
				throw std::runtime_error("Texture type not allowed to appear in this context");
			}
			break;
		case HlslParser::kBaseTypeAppendStructuredBuffer:
		case HlslParser::kBaseTypeBuffer:
		case HlslParser::kBaseTypeByteAddressBuffer:
		case HlslParser::kBaseTypeConsumeStructuredBuffer:
		case HlslParser::kBaseTypeStructuredBuffer:
		case HlslParser::kBaseTypeRWBuffer:
		case HlslParser::kBaseTypeRWByteAddressBuffer:
		case HlslParser::kBaseTypeRWStructuredBuffer:
			if (flags & kTypeAllowBuffer)
			{
				static const char* szBufTypes[] =
				{
					"AppendRegularBuffer",
					"RegularBuffer",
					"ByteBuffer",
					"ConsumeRegularBuffer",
					"RegularBuffer",
					"RW_RegularBuffer",
					"RW_ByteBuffer",
					"RW_RegularBuffer",
				};
				output << szBufTypes[base.base - HlslParser::kBaseTypeAppendStructuredBuffer];
				bTemplate = (base.base != HlslParser::kBaseTypeByteAddressBuffer && base.base != HlslParser::kBaseTypeRWByteAddressBuffer);
			}
			else
			{
				throw std::runtime_error("Buffer type not allowed to appear in this context");
			}
			break;
		case HlslParser::kBaseTypeInputPatch:
		case HlslParser::kBaseTypeOutputPatch:
			if (flags & kTypeAllowPatch)
			{
				output << (base.base == HlslParser::kBaseTypeInputPatch ? "InputPatch" : "OutputPatch");
				bTemplate = true;
				bSamples = true;
			}
			else
			{
				throw std::runtime_error("Patch type not allowed to appear in this context");
			}
			break;
		case HlslParser::kBaseTypePointStream:
		case HlslParser::kBaseTypeLineStream:
		case HlslParser::kBaseTypeTriangleStream:
			if (flags & kTypeAllowStream)
			{
				output << (base.base == HlslParser::kBaseTypePointStream ? "PointBuffer" : base.base == HlslParser::kBaseTypeLineStream ? "LineBuffer" : "TriangleBuffer");
				bTemplate = true;
			}
			else
			{
				throw std::runtime_error("Stream type not allowed to appear in this context");
			}
			break;
		default:
			throw std::runtime_error("The given type is not supported by PSSL");
		}

		if (bBasic)
		{
			if (base.vecDimOrTypeId != 0)
			{
				output << std::to_string(base.vecDimOrTypeId);
				if (base.matDimOrSamples != 0)
				{
					output << 'x' << std::to_string(base.matDimOrSamples);
				}
			}
		}
		else if (bTemplate)
		{
			output << "<";
			HlslParser::SType innerType = boost::get<HlslParser::SType>(hlsl.userDefinedTypes.at(base.vecDimOrTypeId));
			if (innerType.base == HlslParser::kBaseTypeInvalid)
			{
				innerType.base = HlslParser::kBaseTypeFloat;
				innerType.vecDimOrTypeId = 4;
			}
			AddType(innerType, kTypeAllowFloat | kTypeAllowInt | kTypeAllowUserDefined | kTypeForceStorage, output);
			if (bSamples && base.matDimOrSamples != 0)
			{
				output << ", ";
				output << base.matDimOrSamples;
			}
			output << ">";
		}

		if (base.modifier & HlslParser::kTypeModifierPointer)
		{
			if (flags & kTypeAllowPointer)
			{
				output << "*";
			}
			else
			{
				throw std::runtime_error("pointer type not allowed in this context");
			}
		}

		if (base.modifier & HlslParser::kTypeModifierReference)
		{
			if (flags & kTypeAllowReference)
			{
				output << "&";
			}
			else
			{
				throw std::runtime_error("reference type not allowed in this context");
			}
		}

		return dims;
	}

	std::vector<uint32_t> AddType(const HlslParser::SType& type, int flags)
	{
		return AddType(type, flags, output);
	}

	void AddDims(const std::vector<uint32_t>& dims, std::stringstream& output) const
	{
		for (auto dim : dims)
		{
			output << "[" << dim << "]";
		}
	}

	void AddDims(const std::vector<uint32_t>& dims)
	{
		AddDims(dims, output);
	}

	void AddSemantic(const HlslParser::SSemantic& semantic, int flags, std::stringstream& output) const
	{
		if (semantic.sv != HlslParser::kSystemValueNone)
		{
			if (flags & kVarAllowSystemValue)
			{
				output << " : ";

				bool bSkip = false;
				if (!(flags & kVarAllowLegacyColorDepthSVs))
				{
					if (_strnicmp(semantic.name.c_str(), "color", 5) == 0 || _strnicmp(semantic.name.c_str(), "depth", 5) == 0)
					{
						// This is a depth, color or position semantic that has been "upgraded" to a SV semantic
						// However, this upgrade should NOT be done in this context, so instead emit the un-upgraded string.
						output << semantic.name;
						bSkip = true;
					}
				}
				if (!(flags & kVarAllowLegacyPositionSVs))
				{
					if (_strnicmp(semantic.name.c_str(), "position", 8) == 0)
					{
						// This is a position semantic that has been changed to SV_POSITION.
						// However, on input to VS this should remain a user semantic.
						output << semantic.name;
						bSkip = true;
					}
				}
				if (!bSkip)
				{
					AddSystemValue(semantic.sv, output);
				}
			}
			else
			{
				throw std::runtime_error("System value not allowed");
			}
		}
		else if (semantic.reg.type != HlslParser::SRegister::kRegisterNone)
		{
			if (semantic.reg.type == HlslParser::SRegister::kRegisterC || semantic.reg.type == HlslParser::SRegister::kRegisterPackOffset)
			{
				if (flags & kVarAllowPacking) // C register spec treated as packoffset
				{
					static const char registerId[] = { 'x', 'y', 'z', 'w' };
					output << " : packoffset(c" << semantic.reg.index << "." << registerId[semantic.reg.spaceOrElement] << ")";
				}
				else
				{
					throw std::runtime_error("Packing not allowed");
				}
			}
			else
			{
				if ((flags & kVarAllowResourceRegister) && (semantic.reg.spaceOrElement == 0)) // SM5.1 spaces not supported in PSSL
				{
					output << " : register(" << (char)semantic.reg.type << semantic.reg.index << ")";
				}
				else
				{
					throw std::runtime_error("Resource register not allowed");
				}
			}
		}
		else if (!semantic.name.empty())
		{
			if (semantic.name.compare("SV_UserData") == 0)
			{
				output << " : " << "S_SRT_DATA";
			}
			else if (semantic.name.find("S_", 0) == 0)
			{
				throw std::runtime_error("Semantic names starting with S_ are reserved by PSSL");
			}
			else if (flags & kVarAllowUserSemantic)
			{
				output << " : " << semantic.name;
			}
			else
			{
				throw std::runtime_error("User semantic not allowed");
			}
		}
	}

	void AddInterpolationModifier(uint8_t modifier, bool bForce, bool bIntegral, std::stringstream& output) const
	{
		if ((modifier& HlslParser::kInterpolationModifierLinear) || (bForce && modifier == 0 && !bIntegral))
		{
			output << "linear ";
		}
		if (modifier & HlslParser::kInterpolationModifierCentroid)
		{
			output << "centroid ";
		}
		if ((modifier& HlslParser::kInterpolationModifierNoInterpolation) || (bForce && modifier == 0 && bIntegral))
		{
			output << "nointerp ";
		}
		if (modifier & HlslParser::kInterpolationModifierNoPerspective)
		{
			output << "nopersp ";
		}
		if (modifier & HlslParser::kInterpolationModifierSample)
		{
			output << "sample ";
		}
	}

	void AddSystemValue(HlslParser::ESystemValue systemValue, std::stringstream& output) const
	{
		switch (systemValue)
		{
		case HlslParser::kSystemValueClipDistance0:
			output << "S_CLIP_DISTANCE0";
			break;
		case HlslParser::kSystemValueClipDistance1:
			output << "S_CLIP_DISTANCE1";
			break;
		case HlslParser::kSystemValueCullDistance0:
			output << "S_CULL_DISTANCE0";
			break;
		case HlslParser::kSystemValueCullDistance1:
			output << "S_CULL_DISTANCE1";
			break;
		case HlslParser::kSystemValueCoverage:
			output << "S_COVERAGE";
			break;
		case HlslParser::kSystemValueDepth:
			output << "S_DEPTH_OUTPUT";
			break;
		case HlslParser::kSystemValueDepthGreaterEqual:
			output << "S_DEPTH_GE_OUTPUT";
			break;
		case HlslParser::kSystemValueDepthLessEqual:
			output << "S_DEPTH_LE_OUTPUT";
			break;
		case HlslParser::kSystemValueDispatchThreadId:
			output << "S_DISPATCH_THREAD_ID";
			break;
		case HlslParser::kSystemValueDomainLocation:
			output << "S_DOMAIN_LOCATION";
			break;
		case HlslParser::kSystemValueGroupId:
			output << "S_GROUP_ID";
			break;
		case HlslParser::kSystemValueGroupIndex:
			output << "S_GROUP_INDEX";
			break;
		case HlslParser::kSystemValueGroupThreadId:
			output << "S_GROUP_THREAD_ID";
			break;
		case HlslParser::kSystemValueGsInstanceId:
			output << "S_GSINSTANCE_ID";
			break;
		case HlslParser::kSystemValueInsideTessFactor:
			output << "S_INSIDE_TESS_FACTOR";
			break;
		case HlslParser::kSystemValueInstanceId:
			output << "S_INSTANCE_ID";
			break;
		case HlslParser::kSystemValueIsFrontFace:
			output << "S_FRONT_FACE";
			break;
		case HlslParser::kSystemValueOutputControlPointId:
			output << "S_OUTPUT_CONTROL_POINT_ID";
			break;
		case HlslParser::kSystemValuePosition:
			output << "S_POSITION";
			break;
		case HlslParser::kSystemValuePrimitiveId:
			output << "S_PRIMITIVE_ID";
			break;
		case HlslParser::kSystemValueRenderTargetArrayIndex:
			output << "S_RENDER_TARGET_INDEX";
			break;
		case HlslParser::kSystemValueSampleIndex:
			output << "S_SAMPLE_INDEX";
			break;
		case HlslParser::kSystemValueStencilRef:
			output << "S_STENCIL_VALUE";
			break;
		case HlslParser::kSystemValueTarget0:
		case HlslParser::kSystemValueTarget1:
		case HlslParser::kSystemValueTarget2:
		case HlslParser::kSystemValueTarget3:
		case HlslParser::kSystemValueTarget4:
		case HlslParser::kSystemValueTarget5:
		case HlslParser::kSystemValueTarget6:
		case HlslParser::kSystemValueTarget7:
			output << "S_TARGET_OUTPUT" << (systemValue - HlslParser::kSystemValueTarget0);
			break;
		case HlslParser::kSystemValueTessFactor:
			output << "S_EDGE_TESS_FACTOR";
			break;
		case HlslParser::kSystemValueVertexId:
			output << "S_VERTEX_ID";
			break;
		case HlslParser::kSystemValueViewportArrayIndex:
			output << "S_VIEWPORT_INDEX";
			break;
		default:
			throw std::runtime_error("System value not supported in PSSL");
		}
	}

	void AddAttribute(const HlslParser::SAttribute& attr)
	{
		static const char* const szDomain[] =
		{
			"\"tri\"",
			"\"quad\"",
			"\"isoline\"",
		};

		static const char* const szTopology[] =
		{
			nullptr, // PSSL does not support POINT topology
			"\"line\"",
			"\"triangle_cw\"",
			"\"triangle_ccw\"",
		};

		static const char* const szPartitioning[] =
		{
			"\"integer\"",
			"\"fractional_even\"",
			"\"fractional_odd\"",
			"\"pow2\"",
		};

		switch (attr.type)
		{
		case HlslParser::SAttribute::kAttributeDomain:
			if (attr.value >= sizeof(szDomain) / sizeof(*szDomain))
			{
				throw std::runtime_error("Unsupported domain attribute value");
			}
			output << "[DOMAIN_PATCH_TYPE(" << szDomain[attr.value] << ")]";
			break;
		case HlslParser::SAttribute::kAttributeEarlyDepthStencil:
			output << "[FORCE_EARLY_DEPTH_STENCIL]";
			break;
		case HlslParser::SAttribute::kAttributeInstance:
			output << "[INSTANCE(" << GetExpressionRecursive(attr.value) << ")]";
			break;
		case HlslParser::SAttribute::kAttributeMaxTessFactor:
			output << "[MAX_TESS_FACTOR(" << GetExpressionRecursive(attr.value) << ")]";
			break;
		case HlslParser::SAttribute::kAttributeMaxVertexCount:
			output << "[MAX_VERTEX_COUNT(" << GetExpressionRecursive(attr.value) << ")]";
			break;
		case HlslParser::SAttribute::kAttributeNumThreads:
			{
				uint32_t x, y, z;
				attr.GetNumThreads(x, y, z);
				output << "[NUM_THREADS(" << x << ", " << y << ", " << z << ")]";
			}
			break;
		case HlslParser::SAttribute::kAttributeOutputControlPoints:
			output << "[OUTPUT_CONTROL_POINTS(" << GetExpressionRecursive(attr.value) << ")]";
			break;
		case HlslParser::SAttribute::kAttributeOutputTopology:
			if (attr.value == 0 || attr.value >= sizeof(szTopology) / sizeof(*szTopology))
			{
				throw std::runtime_error("Unsupported topology attribute value");
			}
			output << "[OUTPUT_TOPOLOGY_TYPE(" << szTopology[attr.value] << ")]";
			break;
		case HlslParser::SAttribute::kAttributePartitioning:
			if (attr.value >= sizeof(szPartitioning) / sizeof(*szPartitioning))
			{
				throw std::runtime_error("Unsupported partitioning attribute value");
			}
			output << "[PARTITIONING_TYPE(" << szPartitioning[attr.value] << ")]";
			break;
		case HlslParser::SAttribute::kAttributePatchConstantFunc:
			output << "[PATCH_CONSTANT_FUNC(\"" << boost::get<HlslParser::SFunctionDeclaration>(hlsl.objects[hlsl.overloads[attr.value].front()]).name << "\")]";
			break;
		case HlslParser::SAttribute::kAttributeFlatten:
			output << "[flatten]";
			break;
		case HlslParser::SAttribute::kAttributeForceCase: // Not supported on PSSL, treat as branch
		case HlslParser::SAttribute::kAttributeCall:      // Not supported on PSSL, treat as branch
			output << "/*unsupported HLSL attribute converted to:*/";
		case HlslParser::SAttribute::kAttributeBranch:
			output << "[branch]";
			break;
		case HlslParser::SAttribute::kAttributeUnroll:
			output << "[unroll]";
			break;
		case HlslParser::SAttribute::kAttributeLoop:
			output << "[loop]";
			break;
		case HlslParser::SAttribute::kAttributeFastOpt: // Emit a comment for this
			output << "/*unsupported HLSL attribute: [fast_opt]*/";
			break;
		case HlslParser::SAttribute::kAttributeAllowUavCondition: // Emit a comment for this
			output << "/*unsupported HLSL attribute: [allow_uav_condition]*/";
			break;
		default:
			throw std::runtime_error("Unsupported attribute used");
		}
	}

	static const char* MapIntrinsic(HlslParser::EIntrinsic intrinsic)
	{
		switch (intrinsic)
		{
		case HlslParser::kIntrinsicAbort:
			throw std::runtime_error("abort intrinsic not supported");
		case HlslParser::kIntrinsicAbs:
			return "abs";
		case HlslParser::kIntrinsicACos:
			return "acos";
		case HlslParser::kIntrinsicAll:
			return "all";
		case HlslParser::kIntrinsicAllMemoryBarrier:
			return "/*All*/MemoryBarrier";
		case HlslParser::kIntrinsicAllMemoryBarrierWithGroupSync:
			return "/*All*/MemoryBarrierSync";
		case HlslParser::kIntrinsicAny:
			return "any";
		case HlslParser::kIntrinsicAsDouble:
			return "asdouble";
		case HlslParser::kIntrinsicAsFloat:
			return "asfloat";
		case HlslParser::kIntrinsicASin:
			return "asin";
		case HlslParser::kIntrinsicAsInt:
			return "asint";
		case HlslParser::kIntrinsicAsUInt:
			return "asuint";
		case HlslParser::kIntrinsicATan:
			return "atan";
		case HlslParser::kIntrinsicATan2:
			return "atan2";
		case HlslParser::kIntrinsicCeil:
			return "ceil";
		case HlslParser::kIntrinsicCheckAccessFullyMapped:
			throw std::runtime_error("CheckAccessFullyMapped intrinsic not supported, use Sparse_Texture objects instead");
		case HlslParser::kIntrinsicClamp:
			return "clamp";
		case HlslParser::kIntrinsicClip:
			return "clip";
		case HlslParser::kIntrinsicCos:
			return "cos";
		case HlslParser::kIntrinsicCosH:
			return "cosh";
		case HlslParser::kIntrinsicCountBits:
			return "/*countbits*/CountSetBits";
		case HlslParser::kIntrinsicCross:
			return "cross";
		case HlslParser::kIntrinsicD3DCOLORtoUBYTE4:
			return "/*D3DCOLORtoUBYTE4*/FloatColorsToRGBA8";
		case HlslParser::kIntrinsicDDX:
			return "ddx";
		case HlslParser::kIntrinsicDDXCoarse:
			return "ddx_coarse";
		case HlslParser::kIntrinsicDDXFine:
			return "ddx_fine";
		case HlslParser::kIntrinsicDDY:
			return "ddy";
		case HlslParser::kIntrinsicDDYCoarse:
			return "ddy_coarse";
		case HlslParser::kIntrinsicDDYFine:
			return "ddy_fine";
		case HlslParser::kIntrinsicDegrees:
			return "degrees";
		case HlslParser::kIntrinsicDeterminant:
			return "determinant";
		case HlslParser::kIntrinsicDeviceMemoryBarrier:
			return "/*Device*/SharedMemoryBarrier";
		case HlslParser::kIntrinsicDeviceMemoryBarrierWithGroupSync:
			return "/*Device*/SharedMemoryBarrierSync";
		case HlslParser::kIntrinsicDistance:
			return "distance";
		case HlslParser::kIntrinsicDot:
			return "dot";
		case HlslParser::kIntrinsicDst:
			return "dst";
		case HlslParser::kIntrinsicErrorF:
			throw std::runtime_error("errorf intrinsic not supported");
		case HlslParser::kIntrinsicEvaluateAttributeAtCentroid:
			return "EvaluateAttributeAtCentroid";
		case HlslParser::kIntrinsicEvaluateAttributeAtSample:
			return "EvaluateAttributeAtSample";
		case HlslParser::kIntrinsicEvaluateAttributeSnapped:
			return "EvaluateAttributeSnapped";
		case HlslParser::kIntrinsicExp:
			return "exp";
		case HlslParser::kIntrinsicExp2:
			return "exp2";
		case HlslParser::kIntrinsicF16ToF32:
			return "f16tof32";
		case HlslParser::kIntrinsicF32ToF16:
			return "f32tof16";
		case HlslParser::kIntrinsicFaceForward:
			return "faceforward";
		case HlslParser::kIntrinsicFirstBitHigh:
			return "/*firstbithigh*/FirstSetBit_Hi_MSB";
		case HlslParser::kIntrinsicFirstBitLow:
			return "/*firstbitlow*/FirstSetBit_Lo";
		case HlslParser::kIntrinsicFloor:
			return "floor";
		case HlslParser::kIntrinsicFMA:
			return "fma";
		case HlslParser::kIntrinsicFMod:
			return "fmod";
		case HlslParser::kIntrinsicFWidth:
			return "fwidth";
		case HlslParser::kIntrinsicFrac: // Listed only in PSSL index, has no separate page
			return "frac";
		case HlslParser::kIntrinsicFrExp:
			return "frexp";
		case HlslParser::kIntrinsicGetRenderTargetSampleCount:
		case HlslParser::kIntrinsicGetRenderTargetSamplePosition:
			throw std::runtime_error("GetRenderTargetSampleXXX intrinsic not supported");
		case HlslParser::kIntrinsicInterlockedAdd:
			return "/*Interlocked*/AtomicAdd";
		case HlslParser::kIntrinsicInterlockedAnd:
			return "/*Interlocked*/AtomicAnd";
		case HlslParser::kIntrinsicInterlockedCompareExchange:
			return "/*Interlocked*/AtomicCmpExchange";
		case HlslParser::kIntrinsicInterlockedCompareStore:
			return "/*Interlocked*/AtomicCmpStore";
		case HlslParser::kIntrinsicInterlockedExchange:
			return "/*Interlocked*/AtomicExchange";
		case HlslParser::kIntrinsicInterlockedMax:
			return "/*Interlocked*/AtomicMax";
		case HlslParser::kIntrinsicInterlockedMin:
			return "/*Interlocked*/AtomicMin";
		case HlslParser::kIntrinsicInterlockedOr:
			return "/*Interlocked*/AtomicOr";
		case HlslParser::kIntrinsicInterlockedXor:
			return "/*Interlocked*/AtomicXor";
		case HlslParser::kIntrinsicIsFinite:
			return "isfinite";
		case HlslParser::kIntrinsicIsInf:
			return "isinf";
		case HlslParser::kIntrinsicIsNAN:
			return "isnan";
		case HlslParser::kIntrinsicLerp:
			return "lerp";
		case HlslParser::kIntrinsicLit:
			return "lit";
		case HlslParser::kIntrinsicLog:
			return "log";
		case HlslParser::kIntrinsicLog10:
			return "log10";
		case HlslParser::kIntrinsicLog2:
			return "log2";
		case HlslParser::kIntrinsicMax:
			return "max";
		case HlslParser::kIntrinsicMin:
			return "min";
		case HlslParser::kIntrinsicMul:
			return "mul";
		case HlslParser::kIntrinsicGroupMemoryBarrier:
			return "ThreadGroupMemoryBarrier";
		case HlslParser::kIntrinsicGroupMemoryBarrierWithGroupSync:
			return "ThreadGroupMemoryBarrierSync";
		case HlslParser::kIntrinsicLdExp:
			return "ldexp";
		case HlslParser::kIntrinsicLength:
			return "length";
		case HlslParser::kIntrinsicMad:
			return "mad";
		case HlslParser::kIntrinsicModF:
			return "modf";
		case HlslParser::kIntrinsicMSad4:
			throw std::runtime_error("MSAD4 intrinsic not supported, use mqsad() instead");
		case HlslParser::kIntrinsicNoise:
			throw std::runtime_error("noise intrinsic not supported");
		case HlslParser::kIntrinsicNormalize:
			return "normalize";
		case HlslParser::kIntrinsicPrintF:
			throw std::runtime_error("printf intrinsic not supported");
		case HlslParser::kIntrinsicPow:
			return "pow";
		case HlslParser::kIntrinsicProcess2DQuadTessFactorsAvg:
		case HlslParser::kIntrinsicProcess2DQuadTessFactorsMax:
		case HlslParser::kIntrinsicProcess2DQuadTessFactorsMin:
		case HlslParser::kIntrinsicProcessIsolineTessFactors:
		case HlslParser::kIntrinsicProcessQuadTessFactorsAvg:
		case HlslParser::kIntrinsicProcessQuadTessFactorsMax:
		case HlslParser::kIntrinsicProcessQuadTessFactorsMin:
		case HlslParser::kIntrinsicProcessTriTessFactorsAvg:
		case HlslParser::kIntrinsicProcessTriTessFactorsMax:
		case HlslParser::kIntrinsicProcessTriTessFactorsMin:
			throw std::runtime_error("ProcessXXXTessFactors intrinsic not supported");
		case HlslParser::kIntrinsicRadians:
			return "radians";
		case HlslParser::kIntrinsicRcp:
			return "rcp";
		case HlslParser::kIntrinsicReverseBits:
			return "ReverseBits";
		case HlslParser::kIntrinsicRound:
			return "round";
		case HlslParser::kIntrinsicRSqrt:
			return "rsqrt";
		case HlslParser::kIntrinsicReflect:
			return "reflect";
		case HlslParser::kIntrinsicRefract:
			return "refract";
		case HlslParser::kIntrinsicSaturate:
			return "saturate";
		case HlslParser::kIntrinsicSign:
			return "sign";
		case HlslParser::kIntrinsicSin:
			return "sin";
		case HlslParser::kIntrinsicSinCos:
			return "sincos";
		case HlslParser::kIntrinsicSinH:
			return "sinh";
		case HlslParser::kIntrinsicSmoothStep:
			return "smoothstep";
		case HlslParser::kIntrinsicSqrt:
			return "sqrt";
		case HlslParser::kIntrinsicStep:
			return "step";
		case HlslParser::kIntrinsicTan:
			return "tan";
		case HlslParser::kIntrinsicTanH:
			return "tanh";
		case HlslParser::kIntrinsicTranspose:
			return "transpose";
		case HlslParser::kIntrinsicTrunc:
			return "trunc";
		case HlslParser::kIntrinsicWaveOnce:
			throw std::runtime_error("WaveOnce not yet implemented");
		case HlslParser::kIntrinsicWaveGetLaneCount:
			throw std::runtime_error("WaveGetLaneCount not yet implemented");
		case HlslParser::kIntrinsicWaveGetLaneIndex:
			throw std::runtime_error("WaveGetLaneIndex not yet implemented");
		case HlslParser::kIntrinsicWaveIsHelperLane:
			throw std::runtime_error("WaveIsHelperLane not yet implemented");
		case HlslParser::kIntrinsicWaveAnyTrue:
			throw std::runtime_error("WaveAnyTrue not yet implemented");
		case HlslParser::kIntrinsicWaveAllTrue:
			throw std::runtime_error("WaveAllTrue not yet implemented");
		case HlslParser::kIntrinsicWaveAllEqual:
			throw std::runtime_error("WaveAllEqual not yet implemented");
		case HlslParser::kIntrinsicWaveBallot:
			return "/*Wave*/ballot";
		case HlslParser::kIntrinsicWaveReadLaneAt:
			return "/*Wave*/ReadLane/*At*/";
		case HlslParser::kIntrinsicWaveReadFirstLane:
			return "/*Wave*/ReadFirstLane";
		case HlslParser::kIntrinsicWaveAllSum:
			return "/*WaveAllSum*/CrossLaneAdd";
		case HlslParser::kIntrinsicWaveAllProduct:
			throw std::runtime_error("WaveAllProduct not yet implemented");
		case HlslParser::kIntrinsicWaveAllBitAnd:
			return "/*WaveAll*/CrossLaneAnd";
		case HlslParser::kIntrinsicWaveAllBitOr:
			return "/*WaveAll*/CrossLaneOr";
		case HlslParser::kIntrinsicWaveAllBitXor:
			throw std::runtime_error("WaveAllBitXor not yet implemented");
		case HlslParser::kIntrinsicWaveAllMin:
			return "/*WaveAll*/CrossLaneMin";
		case HlslParser::kIntrinsicWaveAllMax:
			return "/*WaveAll*/CrossLaneMax";
		case HlslParser::kIntrinsicWavePrefixSum:
			throw std::runtime_error("WavePrefixSum not yet implemented");
		case HlslParser::kIntrinsicWavePrefixProduct:
			throw std::runtime_error("WavePrefixProduct not yet implemented");
		case HlslParser::kIntrinsicWaveGetOrderedIndex:
			throw std::runtime_error("WaveGetOrderedIndex not yet implemented");
		case HlslParser::kIntrinsicGlobalOrderedCountIncrement:
			throw std::runtime_error("GlobalOrderedCountIncrement not yet implemented");
		case HlslParser::kIntrinsicQuadReadLaneAt:
			throw std::runtime_error("QuadReadLaneAt not yet implemented");
		case HlslParser::kIntrinsicQuadSwapX:
			throw std::runtime_error("QuadSwapX not yet implemented");
		case HlslParser::kIntrinsicQuadSwapY:
			throw std::runtime_error("QuadSwapY not yet implemented");
		default:
			throw std::runtime_error("Unsupported intrinsic");
		}
	}

	static const char* MapIntrinsic(HlslParser::EMemberIntrinsic intrinsic)
	{
		switch (intrinsic)
		{
		case HlslParser::kGenericIntrinsicGetDimensions:
			return "GetDimensions";
		case HlslParser::kGenericIntrinsicLoad:
			return "Load";
		case HlslParser::kTextureIntrinsicCalculateLevelOfDetail:
			return "/*CalculateLevelOfDetail*/GetLOD";
		case HlslParser::kTextureIntrinsicCalculateLevelOfDetailUnclamped:
			return "/*CalculateLevelOfDetail*/GetLODUnclamped";
		case HlslParser::kTextureIntrinsicGather:
			return "Gather";
		case HlslParser::kTextureIntrinsicGatherRed:
			return "GatherRed";
		case HlslParser::kTextureIntrinsicGatherGreen:
			return "GatherGreen";
		case HlslParser::kTextureIntrinsicGatherBlue:
			return "GatherBlue";
		case HlslParser::kTextureIntrinsicGatherAlpha:
			return "GatherAlpha";
		case HlslParser::kTextureIntrinsicGatherCmp:
			return "GatherCmp";
		case HlslParser::kTextureIntrinsicGatherCmpRed:
			return "GatherCmpRed";
		case HlslParser::kTextureIntrinsicGatherCmpGreen:
			return "GatherCmpGreen";
		case HlslParser::kTextureIntrinsicGatherCmpBlue:
			return "GatherCmpBlue";
		case HlslParser::kTextureIntrinsicGatherCmpAlpha:
			return "GatherCmpAlpha";
		case HlslParser::kTextureIntrinsicGetSamplePosition:
			return "GetSamplePoint";
		case HlslParser::kTextureIntrinsicSample:
			return "Sample";
		case HlslParser::kTextureIntrinsicSampleBias:
			return "SampleBias";
		case HlslParser::kTextureIntrinsicSampleCmp:
			return "SampleCmp";
		case HlslParser::kTextureIntrinsicSampleCmpLevelZero:
			return "/*SampleCmpLevelZero*/SampleCmpLOD0";
		case HlslParser::kTextureIntrinsicSampleGrad:
			return "/*SampleGrad*/SampleGradient";
		case HlslParser::kTextureIntrinsicSampleLevel:
			return "/*SampleLevel*/SampleLOD";
		case HlslParser::kBufferIntrinsicAppend:
			return "Append";
		case HlslParser::kBufferIntrinsicConsume:
			return "Consume";
		case HlslParser::kBufferIntrinsicRestartStrip:
			return "RestartStrip";
		case HlslParser::kBufferIntrinsicLoad2:
			return "Load2";
		case HlslParser::kBufferIntrinsicLoad3:
			return "Load3";
		case HlslParser::kBufferIntrinsicLoad4:
			return "Load4";
		case HlslParser::kBufferIntrinsicStore:
			return "Store";
		case HlslParser::kBufferIntrinsicStore2:
			return "Store2";
		case HlslParser::kBufferIntrinsicStore3:
			return "Store3";
		case HlslParser::kBufferIntrinsicStore4:
			return "Store4";
		case HlslParser::kBufferIntrinsicIncrementCounter:
			return "IncrementCount";
		case HlslParser::kBufferIntrinsicDecrementCounter:
			return "DecrementCount";
		default:
			throw std::runtime_error("Member intrinsic not supported");
		}
	}

	std::string GetExpressionRecursive(uint32_t exprId) const
	{
		return GetExpressionRecursive(hlsl.expressions.at(exprId));
	}

	// A hack to support legacy samplers.
	// During generation we will split the global scope legacy samplers into _S and _T objects.
	// In any expressions that refers such a split sampler, we need to create a temporary with that specific purpose.
	bool IsLegacySamplerAccess(const HlslParser::SVariableDeclaration& varDecl) const
	{
		if (varDecl.type.base >= HlslParser::kBaseTypeSampler1D && varDecl.type.base <= HlslParser::kBaseTypeSamplerCUBE)
		{
			return varDecl.scope == 0; // Only global scope samplers should be considered for wrapped access.
		}
		return false;
	}

	std::string GetExpressionRecursive(const HlslParser::SExpression& expr) const
	{
		switch (expr.op)
		{
		case HlslParser::SExpression::kTerminalVariable:
			{
				auto& varDecl = hlsl.variables.at(expr.operand[0]);
				if (options.bSupportLegacySamplers && IsLegacySamplerAccess(varDecl))
				{
					static const char* const szAffix[] = { "1D", "2D", "3D", "Cube" };
					return std::string(LEGACY_SAMPLER) + szAffix[varDecl.type.base - HlslParser::kBaseTypeSampler1D] + "(" + varDecl.name + LEGACY_SAMPLER_AFFIX_S ", " + varDecl.name + LEGACY_SAMPLER_AFFIX_T ")";
				}
				return varDecl.name;
			}
		case HlslParser::SExpression::kTerminalConstant:
			{
				auto& constant = hlsl.constants.at(expr.operand[0]);
				if (auto* pBoolConstant = boost::get<HlslParser::SBoolLiteral>(&constant))
				{
					return pBoolConstant->text;
				}
				if (auto* pIntConstant = boost::get<HlslParser::SIntLiteral>(&constant))
				{
					return pIntConstant->text;
				}
				if (auto* pFloatConstant = boost::get<HlslParser::SFloatLiteral>(&constant))
				{
					return pFloatConstant->text;
				}
				throw std::runtime_error("Unknown constant type used");
			}
		case HlslParser::SExpression::kTerminalFunction:
			{
				auto& overloadSet = hlsl.overloads.at(expr.operand[0]);
				auto& funcDecl = boost::get<HlslParser::SFunctionDeclaration>(hlsl.objects[overloadSet.front()]);
				return funcDecl.name;
			}
		case HlslParser::SExpression::kTerminalIntrinsic:
			{
				const HlslParser::EIntrinsic intrinsic = static_cast<HlslParser::EIntrinsic>(expr.operand[0]);
				return MapIntrinsic(intrinsic);
			}
		case HlslParser::SExpression::kOpMemberIntrinsic:
			{
				std::string result = GetExpressionRecursive(expr.operand[0]);
				const HlslParser::EMemberIntrinsic intrinsic = static_cast<HlslParser::EMemberIntrinsic>(expr.operand[1]);
				result += ".";
				result += MapIntrinsic(intrinsic);
				return std::move(result);
			}
		case HlslParser::SExpression::kOpStructureAccess:
		case HlslParser::SExpression::kOpIndirectAccess:
			{
				std::string result = GetExpressionRecursive(expr.operand[0]);
				result += (expr.op == HlslParser::SExpression::kOpIndirectAccess) ? "->" : ".";

				auto& constant = hlsl.constants.at(expr.operand[1]); // TODO: This will not stay forever a constant hopefully
				if (auto* pStringConstant = boost::get<HlslParser::SStringLiteral>(&constant))
				{
					result += pStringConstant->text;
				}
				else
				{
					throw std::runtime_error("Bad constant for literal, expected string");
				}
				return std::move(result);
			}
		case HlslParser::SExpression::kOpSwizzle:
			{
				std::string result = GetExpressionRecursive(expr.operand[0]);
				result += ".";

				static const char* const szSwizzle[] =
				{
					"",
					"x",   "y",     "z",    "w",
					"r",   "g",     "b",    "a",
					"_m00","_m01",  "_m02", "_m03",
					"_m10","_m11",  "_m12", "_m13",
					"_m20","_m21",  "_m22", "_m23",
					"_m30","_m31",  "_m32", "_m33",
					"_11", "_12",   "_13",  "_14",
					"_21", "_22",   "_23",  "_24",
					"_31", "_32",   "_33",  "_34",
					"_41", "_42",   "_43",  "_44",
				};

				HlslParser::ESwizzle swizzle[4];
				expr.GetSwizzle(swizzle);
				for (int i = 0; i < 4; ++i)
				{
					result += szSwizzle[swizzle[i]];
				}
				return std::move(result);
			}
		case HlslParser::SExpression::kOpCompound:
			{
				std::string result = "{ ";
				bool bComma = false;
				for (auto* pChildExpr : hlsl.GetExpressionList(expr.operand[0]))
				{
					if (bComma)
					{
						result += ", ";
					}
					bComma = true;
					result += GetExpressionRecursive(*pChildExpr);
				}
				result += " }";
				return std::move(result);
			}
		case HlslParser::SExpression::kOpFunctionCall:
			{
				auto childExpressions = hlsl.GetExpressionList(expr.operand[0]);

				// We need to find and adjust calls to legacy texture sampling intrinsics.
				// In this case, we would expect to find an expression like: texXXX(legacySamplerExpr, ...).
				// We need to transform it to legacySamplerExpr.t.SampleFunc(legacySamplerExpr.s, ...) instead.
				if (options.bSupportLegacySamplers
				    && childExpressions.size() >= 2
				    && hlsl.expressions.at(expr.operand[1]).op == HlslParser::SExpression::kTerminalIntrinsic
				    && hlsl.expressions.at(expr.operand[1]).operand[0] >= HlslParser::kIntrinsicTex1D
				    && hlsl.expressions.at(expr.operand[1]).operand[0] <= HlslParser::kIntrinsicTexCubeProj)
				{
					// Look deeper into the first parameter, to see if it is directly expressing a legacy sampler object
					// In this case, we don't need to form a wrapper.
					std::string wrappedS, wrappedT;
					if (childExpressions[0]->op == HlslParser::SExpression::kTerminalVariable)
					{
						auto& varDecl = hlsl.variables.at(childExpressions[0]->operand[0]);
						if (IsLegacySamplerAccess(varDecl))
						{
							wrappedS = varDecl.name + LEGACY_SAMPLER_AFFIX_S;
							wrappedT = varDecl.name + LEGACY_SAMPLER_AFFIX_T;
						}
					}
					if (wrappedS.empty())
					{
						const std::string wrapper = GetExpressionRecursive(*childExpressions[0]);
						wrappedS = wrapper + ".s";
						wrappedT = wrapper + ".t";
					}
					std::string result = wrappedT + ".";

					int splitParam = 0;
					bool bDivide = false;
					bool bGradient = false;
					switch (hlsl.expressions.at(expr.operand[1]).operand[0])
					{
					case HlslParser::kIntrinsicTex1D:
					case HlslParser::kIntrinsicTex2D:
					case HlslParser::kIntrinsicTex3D:
					case HlslParser::kIntrinsicTexCube:
						if (childExpressions.size() < 4)
						{
							result += MapIntrinsic(HlslParser::kTextureIntrinsicSample);
							break;
						}
					// Have DDX/DDY parameter, so should map to SampleGradient instead, fall through
					case HlslParser::kIntrinsicTex1DGrad:
					case HlslParser::kIntrinsicTex2DGrad:
					case HlslParser::kIntrinsicTex3DGrad:
					case HlslParser::kIntrinsicTexCubeGrad:
						bGradient = true;
						result += MapIntrinsic(HlslParser::kTextureIntrinsicSampleGrad);
						break;

					case HlslParser::kIntrinsicTex3DLod:
					case HlslParser::kIntrinsicTexCubeLod:
						splitParam++; // XYZ and W
					case HlslParser::kIntrinsicTex2DLod:
						splitParam++; // XY and W
					case HlslParser::kIntrinsicTex1DLod:
						splitParam++; // X and W
						result += MapIntrinsic(HlslParser::kTextureIntrinsicSampleLevel);
						break;

					case HlslParser::kIntrinsicTex3DBias:
					case HlslParser::kIntrinsicTexCubeBias:
						splitParam++; // XYZ and W
					case HlslParser::kIntrinsicTex2DBias:
						splitParam++; // XY and W
					case HlslParser::kIntrinsicTex1DBias:
						splitParam++; // X and W
						result += MapIntrinsic(HlslParser::kTextureIntrinsicSampleBias);
						break;

					case HlslParser::kIntrinsicTex3DProj:
					case HlslParser::kIntrinsicTexCubeProj:
						splitParam++; // XYZ and W
					case HlslParser::kIntrinsicTex2DProj:
						splitParam++; // XY and W
					case HlslParser::kIntrinsicTex1DProj:
						splitParam++; // X and W
						bDivide = true;
						result += MapIntrinsic(HlslParser::kTextureIntrinsicSample);
						break;

					default:
						throw std::runtime_error("Unsupported legacy sampler intrinsic");
					}
					result += "(" + wrappedS + ", ";

					std::string coord = GetExpressionRecursive(*childExpressions[1]);
					const char* const szSelect[] = { ".x", ".xy", ".xyz" };
					if (bDivide)
					{
						result += coord + szSelect[splitParam - 1] + " / " + coord + ".w";
					}
					else if (splitParam)
					{
						result += coord + szSelect[splitParam - 1] + ", " + coord + ".w";
					}
					else
					{
						result += coord; // This should be the appropriate vector length already.
					}

					if (bGradient && childExpressions.size() >= 4)
					{
						result += ", " + GetExpressionRecursive(*childExpressions[2]);
						result += ", " + GetExpressionRecursive(*childExpressions[3]);
					}
					result += ")";
					return std::move(result);
				}

				// Transform "obj.Load(param)" into "obj[param]", since PSSL complains about RegularBuffer types not having Load member function.
				if (childExpressions.size() == 1)
				{
					const HlslParser::SExpression& expression = hlsl.expressions.at(expr.operand[1]);
					if (expression.op == HlslParser::SExpression::kOpMemberIntrinsic && expression.operand[1] == HlslParser::kGenericIntrinsicLoad)
					{
						std::string result = GetExpressionRecursive(expression.operand[0]);
						result += "/*.Load*/[";
						result += GetExpressionRecursive(*childExpressions.at(0));
						result += "]";
						return std::move(result);
					}
				}

				std::string result = GetExpressionRecursive(expr.operand[1]);
				result += "(";
				bool bComma = false;
				for (auto* pChildExpr : childExpressions)
				{
					if (bComma)
					{
						result += ", ";
					}
					bComma = true;
					result += GetExpressionRecursive(*pChildExpr);
				}
				result += ")";
				return std::move(result);
			}
		case HlslParser::SExpression::kOpTernary:
			{
				std::string result = GetExpressionRecursive(expr.operand[0]);
				result += " ? ";
				result += GetExpressionRecursive(expr.operand[1]);
				result += " : ";
				result += GetExpressionRecursive(expr.operand[2]);
				return std::move(result);
			}
		case HlslParser::SExpression::kOpCast:
			{
				const HlslParser::SType type = expr.GetType();
				std::stringstream result;
				result << "(";
				auto dims = AddType(type, kTypeStruct, result);
				AddDims(dims, result);
				result << ")";
				result << GetExpressionRecursive(expr.operand[0]);
				return result.str();
			}
		case HlslParser::SExpression::kOpConstruct:
			{
				const HlslParser::SType type = expr.GetType();
				std::stringstream result;
				auto dims = AddType(type, kTypeStruct, result);
				AddDims(dims, result);
				result << "(";
				bool bComma = false;
				for (auto* pChildExpr : hlsl.GetExpressionList(expr.operand[0]))
				{
					if (bComma)
					{
						result << ", ";
					}
					bComma = true;
					result << GetExpressionRecursive(*pChildExpr);
				}
				result << ")";
				return result.str();
			}
		default:
			if (expr.op < HlslParser::SExpression::kOpPostfixIncrement || expr.op > HlslParser::SExpression::kOpComma)
			{
				throw std::runtime_error("Unsupported expression operator");
			}
		}

		// Unary operators
		const std::string oper1 = GetExpressionRecursive(expr.operand[0]);
		switch (expr.op)
		{
		case HlslParser::SExpression::kOpPostfixIncrement:
			return oper1 + "++";
		case HlslParser::SExpression::kOpPostfixDecrement:
			return oper1 + "--";
		case HlslParser::SExpression::kOpPrefixIncrement:
			return "++" + oper1;
		case HlslParser::SExpression::kOpPrefixDecrement:
			return "--" + oper1;
		case HlslParser::SExpression::kOpPlus:
			return "+" + oper1;
		case HlslParser::SExpression::kOpMinus:
			return "-" + oper1;
		case HlslParser::SExpression::kOpNot:
			return "!" + oper1;
		case HlslParser::SExpression::kOpComplement:
			return "~" + oper1;
		case HlslParser::SExpression::kOpSubExpression:
			return "(" + oper1 + ")";
		}

		// Binary operators
		const std::string oper2 = GetExpressionRecursive(expr.operand[1]);
		switch (expr.op)
		{
		case HlslParser::SExpression::kOpArraySubscript:
			return oper1 + "[" + oper2 + "]";
		case HlslParser::SExpression::kOpMultiply:
			return oper1 + " * " + oper2;
		case HlslParser::SExpression::kOpDivide:
			return oper1 + " / " + oper2;
		case HlslParser::SExpression::kOpModulo:
			return oper1 + " % " + oper2;
		case HlslParser::SExpression::kOpAdd:
			return oper1 + " + " + oper2;
		case HlslParser::SExpression::kOpSubtract:
			return oper1 + " - " + oper2;
		case HlslParser::SExpression::kOpShiftLeft:
			return oper1 + " << " + oper2;
		case HlslParser::SExpression::kOpShiftRight:
			return oper1 + " >> " + oper2;
		case HlslParser::SExpression::kOpCompareLess:
			return oper1 + " < " + oper2;
		case HlslParser::SExpression::kOpCompareLessEqual:
			return oper1 + " <= " + oper2;
		case HlslParser::SExpression::kOpCompareGreater:
			return oper1 + " > " + oper2;
		case HlslParser::SExpression::kOpCompareGreaterEqual:
			return oper1 + " >= " + oper2;
		case HlslParser::SExpression::kOpCompareEqual:
			return oper1 + " == " + oper2;
		case HlslParser::SExpression::kOpCompareNotEqual:
			return oper1 + " != " + oper2;
		case HlslParser::SExpression::kOpBitAnd:
			return oper1 + " & " + oper2;
		case HlslParser::SExpression::kOpBitXor:
			return oper1 + " ^ " + oper2;
		case HlslParser::SExpression::kOpBitOr:
			return oper1 + " | " + oper2;
		case HlslParser::SExpression::kOpLogicalAnd:
			return oper1 + " && " + oper2;
		case HlslParser::SExpression::kOpLogicalOr:
			return oper1 + " || " + oper2;
		case HlslParser::SExpression::kOpAssign:
			return oper1 + " = " + oper2;
		case HlslParser::SExpression::kOpAssignAdd:
			return oper1 + " += " + oper2;
		case HlslParser::SExpression::kOpAssignSubtract:
			return oper1 + " -= " + oper2;
		case HlslParser::SExpression::kOpAssignMultiply:
			return oper1 + " *= " + oper2;
		case HlslParser::SExpression::kOpAssignDivide:
			return oper1 + " /= " + oper2;
		case HlslParser::SExpression::kOpAssignModulo:
			return oper1 + " %= " + oper2;
		case HlslParser::SExpression::kOpAssignShiftLeft:
			return oper1 + " <<= " + oper2;
		case HlslParser::SExpression::kOpAssignShiftRight:
			return oper1 + " >>= " + oper2;
		case HlslParser::SExpression::kOpAssignBitAnd:
			return oper1 + " &= " + oper2;
		case HlslParser::SExpression::kOpAssignBitXor:
			return oper1 + " ^= " + oper2;
		case HlslParser::SExpression::kOpAssignBitOr:
			return oper1 + " |= " + oper2;
		case HlslParser::SExpression::kOpComma:
			return oper1 + ", " + oper2;
		}

		throw std::runtime_error("Unexpected expression");
	}

	void AddExpression(const HlslParser::SExpression& expr, std::stringstream& output) const
	{
		output << GetExpressionRecursive(expr);
	}

	enum EVarFlags
	{
		kVarAllowInputModifier         = (1 << 0),
		kVarAllowInterpolationModifier = (1 << 1),
		kVarAllowStorageExtern         = (1 << 2),
		kVarAllowStoragePrecise        = (1 << 3),
		kVarAllowStorageLDS            = (1 << 4),
		kVarAllowStorageStatic         = (1 << 5),
		kVarAllowStorageUniform        = (1 << 6),
		kVarAllowStorageVolatile       = (1 << 7),
		kVarUniformAsIn                = (1 << 8),
		kVarIgnoreUniform              = (1 << 9),
		kVarForceExternOrStatic        = (1 << 10),
		kVarIgnoreVolatile             = (1 << 12),
		kVarSharedAsGDS                = (1 << 13),
		kVarForceInputModifier         = (1 << 14),
		kVarAllowSystemValue           = (1 << 15),
		kVarAllowUserSemantic          = (1 << 16),
		kVarAllowResourceRegister      = (1 << 17),
		kVarAllowPacking               = (1 << 18),
		kVarAllowInitializer           = (1 << 19),
		kVarAllowLegacyColorDepthSVs   = (1 << 20), // Those are only allowed on PS return or output type
		kVarAllowLegacyPositionSVs     = (1 << 21), // Those are only allowed on VS input type
		kVarStaticAsConst              = (1 << 22), // Static is not allowed within function in PSSL. Ideally we should make these globals, but that is a bit tricky.

		// Bitwise combinations of flags for each location where variable declarations can appear
		kVarStruct   = kVarAllowInterpolationModifier | kVarIgnoreVolatile | kVarAllowSystemValue | kVarAllowUserSemantic,
		kVarBuffer   = kVarAllowStorageExtern | kVarIgnoreVolatile | kVarAllowPacking | kVarAllowStorageUniform | kVarIgnoreUniform,
		kVarGlobal   = kVarAllowStorageStatic | kVarAllowStorageExtern | kVarForceExternOrStatic | kVarAllowStorageLDS | kVarSharedAsGDS | kVarAllowStorageUniform | kVarIgnoreUniform | kVarIgnoreVolatile | kVarAllowInitializer,
		kVarResource = kVarAllowStorageExtern | kVarAllowStorageUniform | kVarIgnoreUniform | kVarIgnoreVolatile | kVarAllowResourceRegister,
		kVarLocal    = kVarAllowStorageStatic | kVarAllowStorageVolatile | kVarAllowInitializer | kVarStaticAsConst,
		kVarArgument = kVarAllowInputModifier | kVarAllowInterpolationModifier | kVarAllowStoragePrecise | kVarAllowSystemValue | kVarAllowUserSemantic | kVarForceInputModifier | kVarAllowInitializer,
		kVarFunc     = kVarAllowStoragePrecise | kVarAllowStorageVolatile | kVarIgnoreVolatile | kVarAllowSystemValue | kVarAllowUserSemantic,
	};

	void AddVarDecl(const HlslParser::SVariableDeclaration& varDecl, int flags, int typeFlags, std::stringstream& output) const
	{
		uint8_t storage = varDecl.storage;
		uint8_t interpolation = varDecl.interpolation;

		if (storage & HlslParser::kStorageClassNoInterpolation)
		{
			// This for some reason is a storage class modified in HLSL according to the docs (that may be a doc bug)
			// For now, we will just treat it as-if the interpolation modifier is NoInterpolation instead.
			if (interpolation & ~HlslParser::kInterpolationModifierNoInterpolation)
			{
				throw std::runtime_error("Conflicting storage class and interpolation modifier");
			}
			storage &= ~HlslParser::kStorageClassNoInterpolation;
			interpolation = HlslParser::kInterpolationModifierNoInterpolation;
		}

		// Memory type (roughly picks LDS for groupshared, GDS for shared, K$ for extern and SGPR for static)
		bool bAppVisible = false;
		if ((storage & (HlslParser::kStorageClassStatic | HlslParser::kStorageClassExtern | HlslParser::kStorageClassGroupShared | HlslParser::kStorageClassShared)) || (flags & kVarForceExternOrStatic))
		{
			if ((flags & kVarAllowStorageLDS) && (storage & HlslParser::kStorageClassGroupShared))
			{
				output << "thread_group_memory ";
				storage &= ~(HlslParser::kStorageClassGroupShared | HlslParser::kStorageClassStatic);
			}
			else if ((flags & kVarSharedAsGDS) && (storage & HlslParser::kStorageClassShared))
			{
				output << "global_memory ";
				storage &= ~(HlslParser::kStorageClassShared | HlslParser::kStorageClassShared);
			}
			else if ((flags & kVarAllowStorageStatic) && (storage & HlslParser::kStorageClassStatic))
			{
				if ((flags & kVarStaticAsConst) == 0)
				{
					output << "static ";
				}
				else
				{
					typeFlags |= kTypeIgnoreConst; // Const already emitted instead of static
					output << "/*static*/ const ";
				}
				storage &= ~HlslParser::kStorageClassStatic;
			}
			else if ((flags & kVarForceExternOrStatic) || ((flags & kVarAllowStorageExtern) && (storage & HlslParser::kStorageClassExtern)))
			{
				output << "extern ";
				storage &= ~HlslParser::kStorageClassExtern;
				bAppVisible = true;
			}
		}

		// Other storage qualifiers
		if (flags & kVarIgnoreUniform)
		{
			storage &= ~HlslParser::kStorageClassUniform;
		}
		else if ((storage& HlslParser::kStorageClassUniform) && (flags & kVarAllowStorageUniform))
		{
			output << "uniform ";
			storage &= ~HlslParser::kStorageClassUniform;
		}
		if ((storage& HlslParser::kStorageClassPrecise) && (flags & kVarAllowStoragePrecise))
		{
			output << "precise ";
			storage &= ~HlslParser::kStorageClassPrecise;
		}
		if (flags & kVarIgnoreVolatile)
		{
			storage &= ~HlslParser::kStorageClassVolatile;
		}
		else if ((storage& HlslParser::kStorageClassVolatile) && (flags & kVarAllowStorageVolatile))
		{
			output << "volatile ";
			storage &= ~HlslParser::kStorageClassVolatile;
		}
		if (storage)
		{
			throw std::runtime_error("Unsupported storage class used");
		}

		// Interpolation
		if (flags & kVarAllowInterpolationModifier)
		{
			AddInterpolationModifier(interpolation, false, false, output);
		}
		else if (interpolation)
		{
			throw std::runtime_error("Unsupported interpolation modifier used");
		}

		// Input modifier
		bool bOutput = true;
		if (flags & kVarAllowInputModifier)
		{
			bOutput = false;
			if (varDecl.input & HlslParser::kInputModifierUniform)
			{
				if (flags & kVarUniformAsIn)
				{
					output << "in ";
				}
				else
				{
					output << "uniform ";
				}
			}
			else if (varDecl.input & HlslParser::kInputModifierOut)
			{
				if (varDecl.input & HlslParser::kInputModifierIn)
				{
					output << "inout ";
				}
				else
				{
					bOutput = true;
					output << "out ";
				}
			}
			else if ((varDecl.input & HlslParser::kInputModifierIn) || (flags & kVarForceInputModifier))
			{
				output << "in ";
			}
		}
		else if (varDecl.input)
		{
			throw std::runtime_error("Unsupported input modifier");
		}
		if (!bOutput)
		{
			flags &= ~(kVarAllowLegacyColorDepthSVs | kVarAllowLegacyPositionSVs);
		}

		auto dims = AddType(varDecl.type, typeFlags, output);
		output << " " << varDecl.name;
		AddDims(dims, output);

		AddSemantic(varDecl.semantic, flags, output);

		if (varDecl.initializer)
		{
			if (flags & kVarAllowInitializer)
			{
				output << " = ";
				AddExpression(hlsl.expressions.at(varDecl.initializer), output);
			}
			else
			{
				throw std::runtime_error("Initializer expression not allowed here");
			}
		}
	}

	void AddVarDecl(const HlslParser::SVariableDeclaration& varDecl, int flags, int typeFlags)
	{
		AddVarDecl(varDecl, flags, typeFlags, output);
	}

	void AddTypedef(const HlslParser::STypeName& typeDef)
	{
		output << Indent() << "typedef ";
		auto dims = AddType(typeDef.type, kTypeArgument);
		output << typeDef.name;
		AddDims(dims);
		output << std::endl << std::endl;
	}

	void AddStruct(const HlslParser::SStruct& structDef, bool bInline, std::stringstream& output) const
	{
		if (!bInline)
		{
			output << Indent();
		}
		output << "struct " << structDef.name << std::endl;
		output << Indent() << "{" << std::endl;

		int flags = kVarStruct;
		if (options.shaderType == HlslParser::kShaderTypePixel && pOutputStruct && structDef.name == pOutputStruct->name)
		{
			flags |= kVarAllowLegacyColorDepthSVs;
		}
		else if (options.shaderType != HlslParser::kShaderTypeVertex || (pOutputStruct && structDef.name == pOutputStruct->name))
		{
			flags |= kVarAllowLegacyPositionSVs;
		}

		for (auto& item : structDef.members)
		{
			output << Indent(1);
			AddVarDecl(item, flags, kTypeStruct, output);
			output << ";" << std::endl;
		}

		output << Indent() << "}";
		if (!bInline)
		{
			output << ";" << std::endl << std::endl;
		}
	}

	void AddStruct(const HlslParser::SStruct& structDef)
	{
		emittedStructs.push_back(structDef.name);
		AddStruct(structDef, false, output);
	}

	void AddBuffer(const HlslParser::SBuffer& bufferDef)
	{
		if (bufferDef.members.empty())
		{
			return; // HLSL accepts empty buffers, but PSSL doesn't
		}

		// In PSSL, either all members or no members need to be packed
		bool bAnyPacked = false;
		bool bAllPacked = true;
		for (auto& member : bufferDef.members)
		{
			if (member.semantic.reg.type != HlslParser::SRegister::kRegisterNone)
			{
				bAnyPacked = true;
			}
			else
			{
				bAllPacked = false;
			}
		}

		HlslParser::SRegister::ERegisterType expectedType;
		switch (bufferDef.type)
		{
		case HlslParser::SBuffer::kBufferTypeTBuffer:
			output << Indent() << "TextureBuffer ";
			expectedType = HlslParser::SRegister::kRegisterT;
			break;
		case HlslParser::SBuffer::kBufferTypeCBuffer:
			output << Indent() << "ConstantBuffer ";
			expectedType = HlslParser::SRegister::kRegisterB;
			break;
		default:
			throw std::runtime_error("Unsupported buffer type");
		}

		if (bufferDef.name.empty())
		{
			static int bufId = 0;
			output << "_UnnamedBuffer_" << (++bufId); // PSSL doesn't accept anonymous buffers
		}
		else
		{
			output << bufferDef.name;
		}

		AddSemantic(bufferDef.semantic, kVarAllowResourceRegister, output);

		if (bAnyPacked && !bAllPacked)
		{
			output << " /*buffer was partially packed in HLSL*/";
		}
		output << std::endl;
		output << Indent() << "{" << std::endl;
		++indent;

		uint32_t offset = 0;
		HlslParser::SVariableDeclaration copy;
		for (auto& member : bufferDef.members)
		{
			// Deal with partially packed constant buffers
			auto* pMember = &member;
			if (bAnyPacked && !bAllPacked)
			{
				if (member.type.base < HlslParser::kBaseTypeBool || member.type.base > HlslParser::kBaseTypeMin16UInt)
				{
					throw new std::runtime_error("Could not auto-pack a constant buffer that has some (but not all) items explicitly packed, because an unsupported type was in the buffer");
				}
				uint32_t itemDim = std::max(member.type.vecDimOrTypeId, (uint16_t)1) * std::max(member.type.matDimOrSamples, (uint16_t)1);
				bool bStradles4 = ((offset >> 2) != ((offset + itemDim - 1) >> 2));
				uint32_t totalDim = itemDim;
				if (member.type.arrDim)
				{
					itemDim = (totalDim + 3U) & ~3U; // MSDN says each array item starts on it's own 4-element constant
					bStradles4 |= (offset & 3) != 0; // Therefore, if we are at any non-zero offset, treat as straddling
					totalDim = itemDim * member.type.arrDim;
				}
				if (member.semantic.reg.type == HlslParser::SRegister::kRegisterNone)
				{
					if (bStradles4)
					{
						offset = (offset + 3U) & ~3U;
					}
					copy = member;
					copy.semantic.reg.type = HlslParser::SRegister::kRegisterPackOffset;
					copy.semantic.reg.spaceOrElement = offset & 3U;
					copy.semantic.reg.index = offset >> 2;
					pMember = &copy;
				}
				else
				{
					// Item was already packed, compute new offset for next element.
					offset = member.semantic.reg.index * 4 + member.semantic.reg.spaceOrElement;
				}
				offset += totalDim;
			}

			output << Indent();
			AddVarDecl(*pMember, kVarBuffer, kTypeBuffer);
			output << ";";
			if (pMember != &member)
			{
				output << " /*packed by GnmShaderCompiler*/";
			}
			output << std::endl;
		}

		--indent;
		output << Indent() << "};" << std::endl << std::endl;
	}

	void AddVariable(const HlslParser::SVariableDeclaration& varDecl)
	{
		if (varDecl.type.base >= HlslParser::kBaseTypeSampler1D)
		{
			if (varDecl.type.base < HlslParser::kBaseTypeSamplerState)
			{
				// Legacy sampler global declaration
				static const HlslParser::EBaseType texTypes[] = { HlslParser::kBaseTypeTexture1D, HlslParser::kBaseTypeTexture2D, HlslParser::kBaseTypeTexture3D, HlslParser::kBaseTypeTextureCube };
				output << "/*DX9-style sampler declaration split here*/" << std::endl;

				// Emit a SamplerState
				HlslParser::SVariableDeclaration copy = varDecl;
				std::string wrappedS = copy.name + LEGACY_SAMPLER_AFFIX_S;
				std::swap(wrappedS, copy.name);
				copy.type.base = HlslParser::kBaseTypeSamplerState;
				AddVarDecl(copy, kVarResource, kTypeResource);
				output << ";" << std::endl;

				// Emit a TextureXXX with a T register
				copy.name = wrappedS + LEGACY_SAMPLER_AFFIX_T;
				copy.type.base = texTypes[varDecl.type.base - HlslParser::kBaseTypeSampler1D];
				if (copy.semantic.reg.type == HlslParser::SRegister::kRegisterS)
				{
					copy.semantic.reg.type = HlslParser::SRegister::kRegisterT;
				}
				AddVarDecl(copy, kVarResource, kTypeResource);
			}
			else
			{
				AddVarDecl(varDecl, kVarResource, kTypeResource);
			}
		}
		else
		{
			AddVarDecl(varDecl, kVarGlobal, kTypeGlobal);
		}
		output << ";" << std::endl << std::endl;
	}

	void AddStatement(const HlslParser::SStatement& statement, bool bInline = false, bool bIndentMoreIfNotBlock = false, bool bFromElse = false)
	{
		// Else followed by if, special case to make "else if" on one line
		if (bFromElse)
		{
			if (statement.type == HlslParser::SStatement::kIf)
			{
				output << " ";
				bInline = true;
			}
			else
			{
				output << std::endl;
			}
		}

		// Otherwise nicer formatting for single-line conditionals
		if (!bInline)
		{
			if (bIndentMoreIfNotBlock && statement.type != HlslParser::SStatement::kBlock)
			{
				++indent;
			}
			output << Indent();
			if (bIndentMoreIfNotBlock && statement.type != HlslParser::SStatement::kBlock)
			{
				--indent;
			}
		}

		// Actual statement generation
		switch (statement.type)
		{
		case HlslParser::SStatement::kEmpty:
			if (!bInline)
			{
				output << ";" << std::endl;
			}
			return;
		case HlslParser::SStatement::kVariable:
			AddVarDecl(hlsl.variables.at(statement.exprIdOrVarId), kVarLocal, kTypeLocal);
			if (!bInline)
			{
				output << ";" << std::endl;
			}
			return;
		case HlslParser::SStatement::kExpression:
			AddExpression(hlsl.expressions.at(statement.exprIdOrVarId), output);
			if (!bInline)
			{
				output << ";" << std::endl;
			}
			return;
		case HlslParser::SStatement::kBreak:
			output << "break;" << std::endl;
			return;
		case HlslParser::SStatement::kContinue:
			output << "continue;" << std::endl;
			return;
		case HlslParser::SStatement::kDiscard:
			output << "discard;" << std::endl;
			return;
		case HlslParser::SStatement::kReturn:
			output << "return ";
			if (statement.exprIdOrVarId != 0)
			{
				AddExpression(hlsl.expressions.at(statement.exprIdOrVarId), output);
			}
			else
			{
				output << "/*void*/";
			}
			output << ";" << std::endl;
			return;
		case HlslParser::SStatement::kSwitch:
			output << "switch (";
			AddStatement(hlsl.statements.at(statement.statementId[0]));
			output << ")" << std::endl;
			return;
		case HlslParser::SStatement::kCase:
			output << "case ";
			AddExpression(hlsl.expressions.at(statement.exprIdOrVarId), output);
			output << ":" << std::endl;
			return;
		case HlslParser::SStatement::kDefault:
			output << "default:" << std::endl;
			return;
		case HlslParser::SStatement::kIf:
			if (statement.attr.type != HlslParser::SAttribute::kAttributeNone)
			{
				AddAttribute(statement.attr);
				output << std::endl << Indent();
			}
			output << "if (";
			AddStatement(hlsl.statements.at(statement.statementId[0]), true);
			output << ")" << std::endl;
			AddStatement(hlsl.statements.at(statement.statementId[1]), false, true);
			if (statement.statementId[2])
			{
				if (!bInline || bFromElse)
				{
					output << Indent();
				}
				output << "else";
				AddStatement(hlsl.statements.at(statement.statementId[2]), false, true, true);
			}
			return;
		case HlslParser::SStatement::kFor:
			if (statement.attr.type != HlslParser::SAttribute::kAttributeNone)
			{
				AddAttribute(statement.attr);
				output << std::endl << Indent();
			}
			output << "for (";
			AddStatement(hlsl.statements.at(statement.statementId[0]), true);
			output << "; ";
			if (statement.exprIdOrVarId)
			{
				AddExpression(hlsl.expressions.at(statement.exprIdOrVarId), output);
			}
			output << "; ";
			AddStatement(hlsl.statements.at(statement.statementId[1]), true);
			output << ")" << std::endl;
			AddStatement(hlsl.statements.at(statement.statementId[2]), false, true);
			return;
		case HlslParser::SStatement::kDoWhile:
			if (statement.attr.type != HlslParser::SAttribute::kAttributeNone)
			{
				AddAttribute(statement.attr);
				output << std::endl << Indent();
			}
			output << "do" << std::endl;
			AddStatement(hlsl.statements.at(statement.statementId[0]));
			output << Indent() << "while (";
			AddExpression(hlsl.expressions.at(statement.exprIdOrVarId), output);
			output << ");" << std::endl;
			return;
		case HlslParser::SStatement::kWhile:
			if (statement.attr.type != HlslParser::SAttribute::kAttributeNone)
			{
				AddAttribute(statement.attr);
				output << std::endl << Indent();
			}
			output << "while (";
			AddStatement(hlsl.statements.at(statement.statementId[0]), true);
			output << ")" << std::endl;
			AddStatement(hlsl.statements.at(statement.statementId[1]));
			return;
		case HlslParser::SStatement::kBlock:
			{
				const bool bIndent = statement.statementId[2] != 0xFFFFFFFFU;
				const uint32_t previousIndent = indent;
				if (bIndent)
				{
					output << "{" << std::endl;
					++indent;
				}
				else
				{
					indent = 0;
				}
				const HlslParser::SStatement* pBlock = &statement;
				while (pBlock)
				{
					for (uint32_t statId = pBlock->statementId[0]; statId <= pBlock->statementId[1]; ++statId)
					{
						AddStatement(hlsl.statements.at(statId));
						if (!bIndent)
						{
							indent = previousIndent;
						}
					}
					pBlock = pBlock->statementId[2] + 1U >= 2U ? &hlsl.statements.at(pBlock->statementId[2]) : nullptr;
				}
				if (bIndent)
				{
					--indent;
					output << Indent() << "}" << std::endl;
				}
			}
			return;
		}
		throw std::runtime_error("Statement not supported");
	}

	void AddFunction(const HlslParser::SFunctionDeclaration& funcDecl)
	{
		output << Indent();

		for (auto attr : funcDecl.attributes)
		{
			AddAttribute(attr);
			output << std::endl << Indent();
		}

		if (funcDecl.functionClass & HlslParser::kFunctionClassPrecise)
		{
			output << "precise ";
		}

		auto dims = AddType(funcDecl.returnType, kTypeFunc);
		output << " " << funcDecl.name << "(";

		if (options.shaderType == HlslParser::kShaderTypeGeometry && options.entryPoint == funcDecl.name)
		{
			switch (funcDecl.geometryType)
			{
			case HlslParser::kGeometryTypePoint:
				output << "Point ";
				break;
			case HlslParser::kGeometryTypeLine:
				output << "Line ";
				break;
			case HlslParser::kGeometryTypeTriangle:
				output << "Triangle ";
				break;
			case HlslParser::kGeometryTypeLineAdj:
				output << "AdjacentLine ";
				break;
			case HlslParser::kGeometryTypeTriangleAdj:
				output << "AdjacentTriangle ";
				break;
			default:
				throw std::runtime_error("GeometryType required on GS entrypoint function");
			}
		}
		else if (funcDecl.geometryType != HlslParser::kGeometryTypeNone)
		{
			throw std::runtime_error("GeometryType not supported on this function");
		}

		bool bComma = false;
		for (auto arg : funcDecl.arguments)
		{
			if (bComma)
			{
				output << ", ";
			}
			int flags = kVarArgument;
			if (options.shaderType == HlslParser::kShaderTypePixel && funcDecl.name == options.entryPoint)
			{
				flags |= kVarAllowLegacyColorDepthSVs;
			}
			else if (options.shaderType != HlslParser::kShaderTypeVertex || funcDecl.name == options.entryPoint)
			{
				flags |= kVarAllowLegacyPositionSVs;
			}
			AddVarDecl(arg, flags, kTypeArgument);
			bComma = true;
		}
		output << ")";
		AddDims(dims);

		int flags = kVarFunc;
		if (options.shaderType == HlslParser::kShaderTypePixel && funcDecl.name == options.entryPoint)
		{
			flags |= kVarAllowLegacyColorDepthSVs;
		}
		else if (options.shaderType != HlslParser::kShaderTypeVertex || funcDecl.name == options.entryPoint)
		{
			flags |= kVarAllowLegacyColorDepthSVs;
		}
		AddSemantic(funcDecl.semantic, flags, output);

		if (funcDecl.definition)
		{
			output << std::endl;
			AddStatement(hlsl.statements.at(funcDecl.definition));
		}
		else
		{
			output << ";" << std::endl;
		}
		output << std::endl;
	}
};

std::string ToPssl(const HlslParser::SProgram& hlsl, const SPsslConversion& conversion)
{
	SContext context(hlsl, conversion);
	struct SVisitor
	{
		typedef void result_type;
		SContext& context;
		void operator()(const HlslParser::STypeName& typeDef)
		{
			context.AddTypedef(typeDef);
		}
		void operator()(const HlslParser::SStruct& structDef)
		{
			context.AddStruct(structDef);
		}
		void operator()(const HlslParser::SBuffer& bufferDef)
		{
			context.AddBuffer(bufferDef);
		}
		void operator()(const HlslParser::SVariableDeclaration& varDecl)
		{
			context.AddVariable(varDecl);
		}
		void operator()(const HlslParser::SFunctionDeclaration& funcDecl)
		{
			context.AddFunction(funcDecl);
		}
	} visitor = { context };
	try
	{
		// Pre-pass to identify the output struct (if any).
		// This is important since HlslParser may parse certain legacy semantics (COLOR/DEPTH) as SV.
		// However, if those are used anywhere but the output of a PS, they should be converted back.
		for (auto& object : hlsl.objects)
		{
			if (auto* pFuncDecl = boost::get<HlslParser::SFunctionDeclaration>(&object))
			{
				if (pFuncDecl->name == conversion.entryPoint)
				{
					if (pFuncDecl->returnType.base == HlslParser::kBaseTypeUserDefined)
					{
						context.pOutputStruct = boost::get<HlslParser::SStruct>(&hlsl.userDefinedTypes.at(pFuncDecl->returnType.vecDimOrTypeId));
					}
				}
			}
		}

		for (auto& object : hlsl.objects)
		{
			object.apply_visitor(visitor);
		}
	}
	catch (const std::exception& ex)
	{
		return std::string("error (PsslGen): ") + ex.what();
	}

	// Print headers
	std::stringstream result;
	result << "/*HLSL-to-PSSL conversion using GnmShaderCompiler*/" << std::endl;
	result << "/*GnmShaderCompiler built at " __DATE__ " " __TIME__ "*/" << std::endl;
	if (!context.options.comment.empty())
	{
		result << context.options.comment << std::endl << std::endl;
	}
	if (context.legacySamplerUsed)
	{
		std::stringstream legacy;
		legacy << "/*The following wrappers were generated for DX9-style-samplers used in the source*/" << std::endl;
		if (context.legacySamplerUsed & (1U << 0))
		{
			legacy << "struct " LEGACY_SAMPLER "1D   { SamplerState s; Texture1D<float4>   t; };" << std::endl;
		}
		if (context.legacySamplerUsed & (1U << 1))
		{
			legacy << "struct " LEGACY_SAMPLER "2D   { SamplerState s; Texture2D<float4>   t; };" << std::endl;
		}
		if (context.legacySamplerUsed & (1U << 2))
		{
			legacy << "struct " LEGACY_SAMPLER "3D   { SamplerState s; Texture3D<float4>   t; };" << std::endl;
		}
		if (context.legacySamplerUsed & (1U << 3))
		{
			legacy << "struct " LEGACY_SAMPLER "Cube { SamplerState s; TextureCube<float4> t; };" << std::endl;
		}
		legacy << std::endl;
		result << legacy.str();
	}
	result << context.output.str();
	return result.str();
}
