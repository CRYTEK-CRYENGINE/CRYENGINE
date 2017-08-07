#include "internal_includes/toGLSLOperand.h"
#include "internal_includes/toGLSLDeclaration.h"
#include "bstrlib.h"
#include "hlslcc.h"
#include "internal_includes/debug.h"
#include "internal_includes/languages.h"

#include <float.h>
#include <stdlib.h>
#include <math.h>

#if defined(_MSC_VER) && _MSC_VER < 1900
#define isnan(x) _isnan(x)
#define isinf(x) (!_finite(x))
#endif

#define fpcheck(x) (isnan(x) || isinf(x))

extern const char* cComponentNames[];

extern void AddIndentation(HLSLCrossCompilerContext* psContext);
extern void AddAssignToDest(HLSLCrossCompilerContext* psContext, const Operand* psDest, SHADER_VARIABLE_TYPE eSrcType, uint32_t ui32SrcElementCount, int* pNeedsParenthesis);
extern void AddAssignPrologue(HLSLCrossCompilerContext *psContext, int numParenthesis);

uint32_t IsIntegerSigned(const SHADER_VARIABLE_TYPE eType)
{
	if (eType == SVT_INT || eType == SVT_INT16 || eType == SVT_INT12)
		return 1;
	return 0;
}

uint32_t IsIntegerUnsigned(const SHADER_VARIABLE_TYPE eType)
{
	if (eType == SVT_UINT || eType == SVT_UINT16)
		return 1;
	return 0;
}

uint32_t IsIntegerBoolean(const SHADER_VARIABLE_TYPE eType)
{
	if (eType == SVT_BOOL)
		return 1;
	return 0;
}

uint32_t IsFloat(const SHADER_VARIABLE_TYPE eType)
{
	if (eType == SVT_FLOAT || eType == SVT_FLOAT16 || eType == SVT_FLOAT10 || eType == SVT_FLOAT8 /*|| eType == SVT_DOUBLE*/)
		return 1;
	return 0;
}

uint32_t IsDouble(const SHADER_VARIABLE_TYPE eType)
{
	if (eType == SVT_DOUBLE)
		return 1;
	return 0;
}

uint32_t SVTTypeToFlag(const SHADER_VARIABLE_TYPE eType)
{
	if (IsIntegerUnsigned(eType))
		return TO_FLAG_UNSIGNED_INTEGER;
	if (IsIntegerSigned(eType))
		return TO_FLAG_INTEGER;
	if (IsIntegerBoolean(eType))
		return TO_FLAG_BOOL;
	if (IsDouble(eType))
		return TO_FLAG_DOUBLE;
	return TO_FLAG_NONE;
}

uint32_t SVTTypeToCast(const SHADER_VARIABLE_TYPE eType)
{
	if (IsIntegerUnsigned(eType))
		return TO_AUTO_BITCAST_TO_UINT;
	if (IsIntegerSigned(eType))
		return TO_AUTO_BITCAST_TO_INT;
	if (IsIntegerBoolean(eType))
		return TO_AUTO_BITCAST_TO_BOOL;
//	if (IsDouble(eType))
//		return TO_AUTO_BITCAST_TO_DOUBLE;
	return TO_FLAG_NONE;
}

SHADER_VARIABLE_TYPE TypeFlagsToSVTType(const uint32_t typeflags)
{
	if (typeflags & (TO_FLAG_INTEGER | TO_AUTO_BITCAST_TO_INT))
		return SVT_INT;
	if (typeflags & (TO_FLAG_UNSIGNED_INTEGER | TO_AUTO_BITCAST_TO_UINT))
		return SVT_UINT;
	if (typeflags & (TO_FLAG_BOOL | TO_AUTO_BITCAST_TO_BOOL))
		return SVT_BOOL;
	if (typeflags & (TO_FLAG_DOUBLE))
		return SVT_DOUBLE;
	return SVT_FLOAT;
}

// Returns 0 if the register used by the operand is per-vertex, or 1 if per-patch
int IsPatchConstantFromTypes(const Operand* psOperand, SHADER_TYPE eShaderType, uint32_t eShaderPhaseType)
{
	if (eShaderType != HULL_SHADER && eShaderType != DOMAIN_SHADER)
		return 0;

	if (eShaderType == HULL_SHADER && eShaderPhaseType == HS_CTRL_POINT_PHASE)
		return 0;

	if (eShaderType == DOMAIN_SHADER && psOperand->eType == OPERAND_TYPE_OUTPUT)
		return 0;

	if (psOperand->eType == OPERAND_TYPE_INPUT_CONTROL_POINT || psOperand->eType == OPERAND_TYPE_OUTPUT_CONTROL_POINT)
		return 0;

	return 1;
}

int IsPatchConstant(const HLSLCrossCompilerContext *psContext, const Operand* psOperand)
{
	return IsPatchConstantFromTypes(psOperand, psContext->psShader->eShaderType, psContext->currentPhase);
}

uint32_t GetOperandWriteMask(const Operand *psOperand)
{
	if (psOperand->eSelMode != OPERAND_4_COMPONENT_MASK_MODE || psOperand->ui32CompMask == 0)
		return OPERAND_4_COMPONENT_MASK_ALL;

	return psOperand->ui32CompMask;
}


const char * GetConstructorForType(const SHADER_VARIABLE_TYPE eType, const int components, const int usePrecisionQualifier)
{
	static const char * const uintTypes[] = { " ", "uint", "uvec2", "uvec3", "uvec4" };
	static const char * const intTypes[] = { " ", "int", "ivec2", "ivec3", "ivec4" };
	static const char * const floatTypes[] = { " ", "float", "vec2", "vec3", "vec4" };
	static const char * const doubleTypes[] = { " ", "double", "dvec2", "dvec3", "dvec4" };
	static const char * const boolTypes[] = { " ", "bool", "bvec2", "bvec3", "bvec4" };

	static const char * const uint16Types[] = { " ", "mediump uint", "mediump uvec2", "mediump uvec3", "mediump uvec4" };
	static const char * const int16Types[] = { " ", "mediump int", "mediump ivec2", "mediump ivec3", "mediump ivec4" };
	static const char * const int12Types[] = { " ", "lowp int", "lowp ivec2", "lowp ivec3", "lowp ivec4" };
	static const char * const float16Types[] = { " ", "mediump float", "mediump vec2", "mediump vec3", "mediump vec4" };
	static const char * const float10Types[] = { " ", "lowp float", "lowp vec2", "lowp vec3", "lowp vec4" };
	static const char * const float8Types[] = { " ", "lowp float", "lowp vec2", "lowp vec3", "lowp vec4" };

	if (components < 1 || components > 4)
		return "ERROR TOO MANY COMPONENTS IN VECTOR";

	if (usePrecisionQualifier)
		switch (eType)
		{
		case SVT_FLOAT16:
			return float16Types[components];
		case SVT_FLOAT10:
			return float10Types[components];
		case SVT_FLOAT8:
			return float8Types[components];
		case SVT_UINT16:
			return uint16Types[components];
		case SVT_INT16:
			return int16Types[components];
		case SVT_INT12:
			return int12Types[components];
		}

	switch (eType)
	{
	case SVT_UINT:
	case SVT_UINT16:
		return uintTypes[components];
	case SVT_INT:
	case SVT_INT16:
	case SVT_INT12:
		return intTypes[components];
	case SVT_FLOAT:
	case SVT_FLOAT16:
	case SVT_FLOAT10:
	case SVT_FLOAT8:
		return floatTypes[components];;
	case SVT_DOUBLE:
		return doubleTypes[components];
	case SVT_BOOL:
		return boolTypes[components];

	default:
		return "ERROR UNSUPPORTED TYPE";
	}
}

int GetMaxComponentFromComponentMask(const Operand* psOperand)
{
	if (psOperand->iWriteMaskEnabled &&
		psOperand->iNumComponents == 4)
	{
        //Component Mask
		if(psOperand->eSelMode == OPERAND_4_COMPONENT_MASK_MODE)
		{
			if(psOperand->ui32CompMask != 0 && psOperand->ui32CompMask != (OPERAND_4_COMPONENT_MASK_X|OPERAND_4_COMPONENT_MASK_Y|OPERAND_4_COMPONENT_MASK_Z|OPERAND_4_COMPONENT_MASK_W))
			{
				if(psOperand->ui32CompMask & OPERAND_4_COMPONENT_MASK_W)
				{
					return 4;
				}
				if(psOperand->ui32CompMask & OPERAND_4_COMPONENT_MASK_Z)
				{
					return 3;
				}
				if(psOperand->ui32CompMask & OPERAND_4_COMPONENT_MASK_Y)
				{
					return 2;
				}
				if(psOperand->ui32CompMask & OPERAND_4_COMPONENT_MASK_X)
				{
					return 1;
				}
			}
		}
		//Component Swizzle
		else if(psOperand->eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE)
		{
			return 4;
		}
		else if(psOperand->eSelMode == OPERAND_4_COMPONENT_SELECT_1_MODE)
		{
			return 1;
		}
	}

	return 4;
}

//Single component repeated
//e..g .wwww
uint32_t IsSwizzleReplicated(const Operand* psOperand)
{
	if(psOperand->iWriteMaskEnabled &&
		psOperand->iNumComponents == 4)
	{
		if(psOperand->eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE)
		{
			if (psOperand->ui32Swizzle == WWWW_SWIZZLE ||
				psOperand->ui32Swizzle == ZZZZ_SWIZZLE ||
				psOperand->ui32Swizzle == YYYY_SWIZZLE ||
				psOperand->ui32Swizzle == XXXX_SWIZZLE)
			{
				return 1;
			}
		}
	}
	return 0;
}

static uint32_t GetNumberBitsSet(uint32_t a)
{
	// Calculate number of bits in a
	// Taken from https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSet64
	// Works only up to 14 bits (we're only using up to 4)
	return (a * 0x200040008001ULL & 0x111111111111111ULL) % 0xf;
}

//e.g.
//.z = 1
//.x = 1
//.yw = 2
uint32_t GetNumSwizzleElements(const Operand* psOperand)
{
	return GetNumSwizzleElementsWithMask(psOperand, OPERAND_4_COMPONENT_MASK_ALL);
}

// Get the number of elements returned by operand, taking additional component mask into account
uint32_t GetNumSwizzleElementsWithMask(const Operand *psOperand, uint32_t ui32ComponentMask)
{
	uint32_t count = 0;

	switch (psOperand->eType)
	{
	case OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
		return 1; // TODO: does mask make any sense here?
	case OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP:
	case OPERAND_TYPE_INPUT_THREAD_ID:
	case OPERAND_TYPE_INPUT_THREAD_GROUP_ID:
		// Adjust component count and break to more processing
		((Operand *)psOperand)->iNumComponents = 3;
		break;
	case OPERAND_TYPE_IMMEDIATE32:
	case OPERAND_TYPE_IMMEDIATE64:
	case OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
	case OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL:
	case OPERAND_TYPE_OUTPUT_DEPTH:
		{
			// Translate numComponents into bitmask
			// 1 -> 1, 2 -> 3, 3 -> 7 and 4 -> 15
			uint32_t compMask = (1 << psOperand->iNumComponents) - 1;

			compMask &= ui32ComponentMask;
			// Calculate bits left in compMask
			return GetNumberBitsSet(compMask);
		}
	default:
		{
			break;
		}
	}

	if (psOperand->iWriteMaskEnabled &&
		psOperand->iNumComponents != 1)
	{
		//Component Mask
		if (psOperand->eSelMode == OPERAND_4_COMPONENT_MASK_MODE)
		{
			uint32_t mask;
			if (psOperand->ui32CompMask != 0)
				mask = psOperand->ui32CompMask & ui32ComponentMask;
			else
				mask = ui32ComponentMask;

			if (mask == OPERAND_4_COMPONENT_MASK_ALL)
				return 4;

			if (mask & OPERAND_4_COMPONENT_MASK_X)
			{
				count++;
			}
			if (mask & OPERAND_4_COMPONENT_MASK_Y)
			{
				count++;
			}
			if (mask & OPERAND_4_COMPONENT_MASK_Z)
			{
				count++;
			}
			if (mask & OPERAND_4_COMPONENT_MASK_W)
			{
				count++;
			}
		}
		else
			//Component Swizzle
			if (psOperand->eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE)
			{
				if (psOperand->ui32Swizzle != (NO_SWIZZLE))
				{
					uint32_t i;

					for (i = 0; i < 4; ++i)
					{
						if ((ui32ComponentMask & (1 << i)) == 0)
							continue;

						if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_X)
						{
							count++;
						}
						else if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_Y)
						{
							count++;
						}
						else if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_Z)
						{
							count++;
						}
						else if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_W)
						{
							count++;
						}
					}
				}
			}
			else
				if (psOperand->eSelMode == OPERAND_4_COMPONENT_SELECT_1_MODE) // ui32ComponentMask is ignored in this case
				{
					if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_X)
					{
						count++;
					}
					else if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_Y)
					{
						count++;
					}
					else if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_Z)
					{
						count++;
					}
					else if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_W)
					{
						count++;
					}
				}

		//Component Select 1
	}

	if(!count)
	{
		// Translate numComponents into bitmask
		// 1 -> 1, 2 -> 3, 3 -> 7 and 4 -> 15
		uint32_t compMask = (1 << psOperand->iNumComponents) - 1;

		compMask &= ui32ComponentMask;
		// Calculate bits left in compMask
		return GetNumberBitsSet(compMask);
	}

	return count;
}

void AddSwizzleUsingElementCount(HLSLCrossCompilerContext* psContext, uint32_t count)
{
	bstring glsl = *psContext->currentGLSLString;
	if (count == 4)
		return;

	if (count--)
	{
		bcatcstr(glsl, ".");
		bcatcstr(glsl, cComponentNames[0]);
		if (count--)
		{
			bcatcstr(glsl, cComponentNames[1]);
			if (count--)
			{
				bcatcstr(glsl, cComponentNames[2]);
				if (count--)
				{
					bcatcstr(glsl, cComponentNames[3]);
				}
			}
		}
	}
}

uint32_t ConvertOperandSwizzleToComponentMask(const Operand* psOperand)
{
	uint32_t mask = 0;

	if(psOperand->iWriteMaskEnabled &&
		psOperand->iNumComponents == 4)
	{
		//Component Mask
		if(psOperand->eSelMode == OPERAND_4_COMPONENT_MASK_MODE)
		{
			mask = psOperand->ui32CompMask;
		}
		else
			//Component Swizzle
			if(psOperand->eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE)
			{
				if(psOperand->ui32Swizzle != (NO_SWIZZLE))
				{
					uint32_t i;

					for(i=0; i< 4; ++i)
					{
						if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_X)
						{
							mask |= OPERAND_4_COMPONENT_MASK_X;
						}
						else if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_Y)
						{
							mask |= OPERAND_4_COMPONENT_MASK_Y;
						}
						else if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_Z)
						{
							mask |= OPERAND_4_COMPONENT_MASK_Z;
						}
						else if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_W)
						{
							mask |= OPERAND_4_COMPONENT_MASK_W;
						}
					}
				}
			}
			else
				if(psOperand->eSelMode == OPERAND_4_COMPONENT_SELECT_1_MODE)
				{
					if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_X)
					{
						mask |= OPERAND_4_COMPONENT_MASK_X;
					}
					else if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_Y)
					{
						mask |= OPERAND_4_COMPONENT_MASK_Y;
					}
					else if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_Z)
					{
						mask |= OPERAND_4_COMPONENT_MASK_Z;
					}
					else if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_W)
					{
						mask |= OPERAND_4_COMPONENT_MASK_W;
					}
				}

				//Component Select 1
	}

	return mask;
}

//Non-zero means the components overlap
int CompareOperandSwizzles(const Operand* psOperandA, const Operand* psOperandB)
{
	uint32_t maskA = ConvertOperandSwizzleToComponentMask(psOperandA);
	uint32_t maskB = ConvertOperandSwizzleToComponentMask(psOperandB);

	return maskA & maskB;
}


void TranslateOperandSwizzle(HLSLCrossCompilerContext* psContext, const Operand* psOperand)
{
	TranslateOperandSwizzleWithMask(psContext, psOperand, OPERAND_4_COMPONENT_MASK_ALL);
}

void TranslateOperandSwizzleWithMask(HLSLCrossCompilerContext* psContext, const Operand* psOperand, uint32_t ui32ComponentMask)
{
	bstring glsl = *psContext->currentGLSLString;

	if(psOperand->eType == OPERAND_TYPE_INPUT)
	{
		if(psContext->psShader->abScalarInput[psOperand->ui32RegisterNumber])
		{
			return;
		}
	}

	if (psOperand->eType == OPERAND_TYPE_OUTPUT)
	{
		if (psContext->psShader->abScalarInput[psOperand->ui32RegisterNumber])
		{
			return;
		}
	}

	if(psOperand->eType == OPERAND_TYPE_CONSTANT_BUFFER)
	{
		/*ConstantBuffer* psCBuf = NULL;
		ShaderVar* psVar = NULL;
		int32_t index = -1;
		GetConstantBufferFromBindingPoint(psOperand->aui32ArraySizes[0], &psContext->psShader->sInfo, &psCBuf);

		//Access the Nth vec4 (N=psOperand->aui32ArraySizes[1])
		//then apply the sizzle.

		GetShaderVarFromOffset(psOperand->aui32ArraySizes[1], psOperand->aui32Swizzle, psCBuf, &psVar, &index);

		bformata(glsl, ".%s", psVar->Name);
		if(index != -1)
		{
		bformata(glsl, "[%d]", index);
		}*/

		//return;
	}

	if (psOperand->iWriteMaskEnabled &&
		psOperand->iNumComponents != 1)
	{
		//Component Mask
		if (psOperand->eSelMode == OPERAND_4_COMPONENT_MASK_MODE)
		{
			uint32_t mask;
			if (psOperand->ui32CompMask != 0)
				mask = psOperand->ui32CompMask & ui32ComponentMask;
			else
				mask = ui32ComponentMask;

			if (mask != 0 && mask != OPERAND_4_COMPONENT_MASK_ALL)
			{
				bcatcstr(glsl, ".");
				if (mask & OPERAND_4_COMPONENT_MASK_X)
					bcatcstr(glsl, cComponentNames[0]);
				if (mask & OPERAND_4_COMPONENT_MASK_Y)
					bcatcstr(glsl, cComponentNames[1]);
				if (mask & OPERAND_4_COMPONENT_MASK_Z)
					bcatcstr(glsl, cComponentNames[2]);
				if (mask & OPERAND_4_COMPONENT_MASK_W)
					bcatcstr(glsl, cComponentNames[3]);
			}
		}
		//Component Swizzle
		else if (psOperand->eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE)
		{
			if (ui32ComponentMask != OPERAND_4_COMPONENT_MASK_ALL || !(
				psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_X &&
				psOperand->aui32Swizzle[1] == OPERAND_4_COMPONENT_Y &&
				psOperand->aui32Swizzle[2] == OPERAND_4_COMPONENT_Z &&
				psOperand->aui32Swizzle[3] == OPERAND_4_COMPONENT_W))
			{
				uint32_t i;

				bcatcstr(glsl, ".");

				for (i = 0; i < 4; ++i)
				{
					if (!(ui32ComponentMask & (OPERAND_4_COMPONENT_MASK_X << i)))
						continue;

					if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_X)
						bcatcstr(glsl, cComponentNames[0]);
					else if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_Y)
						bcatcstr(glsl, cComponentNames[1]);
					else if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_Z)
						bcatcstr(glsl, cComponentNames[2]);
					else if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_W)
						bcatcstr(glsl, cComponentNames[3]);
				}
			}
		}
		// ui32ComponentMask is ignored in this case
		else if (psOperand->eSelMode == OPERAND_4_COMPONENT_SELECT_1_MODE)
		{
			bcatcstr(glsl, ".");

			if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_X)
				bcatcstr(glsl, cComponentNames[0]);
			else if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_Y)
				bcatcstr(glsl, cComponentNames[1]);
			else if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_Z)
				bcatcstr(glsl, cComponentNames[2]);
			else if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_W)
				bcatcstr(glsl, cComponentNames[3]);
		}

		//Component Select 1
	}
}

int GetFirstOperandSwizzle(HLSLCrossCompilerContext* psContext, const Operand* psOperand)
{
	if (psOperand->eType == OPERAND_TYPE_INPUT)
	{
		if (psContext->psShader->abScalarInput[psOperand->ui32RegisterNumber])
		{
			return -1;
		}
	}

	if (psOperand->eType == OPERAND_TYPE_OUTPUT)
	{
		if (psContext->psShader->abScalarInput[psOperand->ui32RegisterNumber])
		{
			return -1;
		}
	}

	if (psOperand->iWriteMaskEnabled &&
		psOperand->iNumComponents == 4)
	{
		//Component Mask
		if (psOperand->eSelMode == OPERAND_4_COMPONENT_MASK_MODE)
		{
			if (psOperand->ui32CompMask != 0 && psOperand->ui32CompMask != (OPERAND_4_COMPONENT_MASK_X | OPERAND_4_COMPONENT_MASK_Y | OPERAND_4_COMPONENT_MASK_Z | OPERAND_4_COMPONENT_MASK_W))
			{
				if (psOperand->ui32CompMask & OPERAND_4_COMPONENT_MASK_X)
				{
					return 0;
				}
				if (psOperand->ui32CompMask & OPERAND_4_COMPONENT_MASK_Y)
				{
					return 1;
				}
				if (psOperand->ui32CompMask & OPERAND_4_COMPONENT_MASK_Z)
				{
					return 2;
				}
				if (psOperand->ui32CompMask & OPERAND_4_COMPONENT_MASK_W)
				{
					return 3;
				}
			}
		}
		else
			//Component Swizzle
			if (psOperand->eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE)
			{
				if (psOperand->ui32Swizzle != (NO_SWIZZLE))
				{
					uint32_t i;

					for (i = 0; i < 4; ++i)
					{
						if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_X)
						{
							return 0;
						}
						else if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_Y)
						{
							return 1;
						}
						else if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_Z)
						{
							return 2;
						}
						else if (psOperand->aui32Swizzle[i] == OPERAND_4_COMPONENT_W)
						{
							return 3;
						}
					}
				}
			}
			else
				if (psOperand->eSelMode == OPERAND_4_COMPONENT_SELECT_1_MODE)
				{

					if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_X)
					{
						return 0;
					}
					else if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_Y)
					{
						return 1;
					}
					else if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_Z)
					{
						return 2;
					}
					else if (psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_W)
					{
						return 3;
					}
				}

		//Component Select 1
	}

	return -1;
}

void TranslateOperandIndex(HLSLCrossCompilerContext* psContext, const Operand* psOperand, int index)
{
	int i = index;

	bstring glsl = *psContext->currentGLSLString;

	ASSERT(index < psOperand->iIndexDims);

	switch (psOperand->eIndexRep[i])
	{
	case OPERAND_INDEX_IMMEDIATE32:
		{
			bformata(glsl, "[%d]", psOperand->aui32ArraySizes[i]);
			break;
		}
	case OPERAND_INDEX_RELATIVE:
		{
			bcatcstr(glsl, "["); //Indexes must be integral.
			TranslateOperand(psContext, psOperand->psSubOperand[i], TO_FLAG_INTEGER | TO_AUTO_BITCAST_TO_INT);
			bcatcstr(glsl, "]");
			break;
		}
	case OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE:
		{
			bcatcstr(glsl, "["); //Indexes must be integral.
			TranslateOperand(psContext, psOperand->psSubOperand[i], TO_FLAG_INTEGER | TO_AUTO_BITCAST_TO_INT);
			bformata(glsl, " + %d]", psOperand->aui32ArraySizes[i]);
			break;
		}
	default:
		{
			break;
		}
	}
}

void TranslateOperandIndexMAD(HLSLCrossCompilerContext* psContext, const Operand* psOperand, int index, uint32_t multiply, uint32_t add)
{
	int i = index;
	int isGeoShader = psContext->psShader->eShaderType == GEOMETRY_SHADER ? 1 : 0;

	bstring glsl = *psContext->currentGLSLString;

	ASSERT(index < psOperand->iIndexDims);

	switch(psOperand->eIndexRep[i])
	{
	case OPERAND_INDEX_IMMEDIATE32:
		{
			if(i > 0 || isGeoShader)
			{
				bformata(glsl, "[%d*%d+%d]", psOperand->aui32ArraySizes[i], multiply, add);
			}
			else
			{
				bformata(glsl, "%d*%d+%d", psOperand->aui32ArraySizes[i], multiply, add);
			}
			break;
		}
	case OPERAND_INDEX_RELATIVE:
		{
			bcatcstr(glsl, "["); //Indexes must be integral.
			TranslateOperand(psContext, psOperand->psSubOperand[i], TO_FLAG_INTEGER | TO_AUTO_BITCAST_TO_INT);
			bformata(glsl, " * %d + %d]", multiply, add);
			break;
		}
	case OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE:
		{
			bcatcstr(glsl, "[("); //Indexes must be integral.
			TranslateOperand(psContext, psOperand->psSubOperand[i], TO_FLAG_INTEGER | TO_AUTO_BITCAST_TO_INT);
			bformata(glsl, " + %d) * %d + %d]", psOperand->aui32ArraySizes[i], multiply, add);
			break;
		}
	default:
		{
			break;
		}
	}
}

// Returns nonzero if a direct constructor can convert src->dest
int CanDoDirectCast(HLSLCrossCompilerContext *psContext, SHADER_VARIABLE_TYPE src, SHADER_VARIABLE_TYPE dst)
{
	// Only option on pre-SM4 stuff
	if (psContext->psShader->ui32MajorVersion < 4)
		return 1;

	// uint<->int<->bool conversions possible
	if ((IsIntegerSigned(src) || IsIntegerUnsigned(src) || IsIntegerBoolean(src)) &&
		(IsIntegerSigned(dst) || IsIntegerUnsigned(dst) || IsIntegerBoolean(dst)))
		return 1;

	// float<->double possible
	if ((IsFloat(src) || IsDouble(src)) &&
		(IsFloat(dst) || IsDouble(dst)))
		return 1;

	return 0;
}

const char *GetDirectCastOp(HLSLCrossCompilerContext *psContext, SHADER_VARIABLE_TYPE from, SHADER_VARIABLE_TYPE to, uint32_t numComponents)
{
	const int usePrec = psContext->flags & HLSLCC_FLAG_FORCE_PRECISION_QUALIFIERS;

	// uint<->uint conversions possible
	if (IsIntegerUnsigned(from) &&
		IsIntegerUnsigned(to  ))
		return "";

	// int<->int conversions possible
	if (IsIntegerSigned(from) &&
		IsIntegerSigned(to  ))
		return "";

	// float<->float possible
	if (IsFloat(from) &&
		IsFloat(to  ))
		return "";
	
	// double<->double possible
	if (IsDouble(from) &&
		IsDouble(to  ))
		return "";

	/*
	 * https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_shader_implicit_conversions.txt
	 * https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_gpu_shader5.txt
	 * https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_gpu_shader_fp64.txt
	 *
	 * Can be implicitly
	 *
	 *  Type of expression        converted to
	 *  -------------------- - ---------------- -
	 *  int                     uint, float
	 *  ivec2                   uvec2, vec2
	 *  ivec3                   uvec3, vec3
	 *  ivec4                   uvec4, vec4
	 *  uint                    float
	 *  uvec2                   vec2
	 *  uvec3                   vec3
	 *  uvec4                   vec4
	 *
	 *  Type of expression        converted to
	 *  ---------------------   -------------------
	 *  int                     uint(*), float, double
	 *  ivec2                   uvec2(*), vec2, dvec2
	 *  ivec3                   uvec3(*), vec3, dvec3
	 *  ivec4                   uvec4(*), vec4, dvec4
	 *  
	 *  uint                    float, double
	 *  uvec2                   vec2, dvec2
	 *  uvec3                   vec3, dvec3
	 *  uvec4                   vec4, dvec4
	 *  
	 *  float                   double
	 *  vec2                    dvec2
	 *  vec3                    dvec3
	 *  vec4                    dvec4
	 *  
	 *  mat2                    dmat2
	 *  mat3                    dmat3
	 *  mat4                    dmat4
	 *  mat2x3                  dmat2x3
	 *  mat2x4                  dmat2x4
	 *  mat3x2                  dmat3x2
	 *  mat3x4                  dmat3x4
	 *  mat4x2                  dmat4x2
	 *  mat4x3                  dmat4x3
	 */
	if ((numComponents >= 1) && IsFloat(from) && IsDouble(to))
		return "";
	if ((numComponents >= 1) && IsIntegerSigned(from) && (IsDouble(to) || IsFloat(to)))
		return "";
	if ((numComponents >= 1) && IsIntegerSigned(from) && IsIntegerUnsigned(to))
		return GetConstructorForType(to, numComponents, usePrec); // To prevent shift/comparison/etc. problems
	if ((numComponents >= 1) && IsIntegerUnsigned(from) && (IsDouble(to) || IsFloat(to)))
		return "";
	if ((numComponents >= 1) && IsIntegerUnsigned(from) && IsIntegerSigned(to))
		return GetConstructorForType(to, numComponents, usePrec); // Not possible otherwise

	if ((numComponents >= 1) && IsIntegerBoolean(from))
		return GetConstructorForType(to, numComponents, usePrec); // To prevent *~0u/etc. problems

	return "unknown";
}

const char *GetReinterpretCastOp(SHADER_VARIABLE_TYPE from, SHADER_VARIABLE_TYPE to)
{
	/**/ if ((IsFloat(to)) && (IsIntegerSigned(from) || IsIntegerBoolean(from)))
		return "intBitsToFloat";
	else if ((IsFloat(to)) && (IsIntegerUnsigned(from) || IsIntegerBoolean(from)))
		return "uintBitsToFloat";
	else if ((IsIntegerSigned(to) || IsIntegerBoolean(to)) && IsFloat(from))
		return "floatBitsToInt";
	else if ((IsIntegerUnsigned(to) || IsIntegerBoolean(to)) && IsFloat(from))
		return "floatBitsToUint";

	return "ERROR missing components in GetBitcastOp()";
}

// Helper function to print out a single 32-bit immediate value in desired format
static void printImmediate32(HLSLCrossCompilerContext *psContext, uint32_t value, SHADER_VARIABLE_TYPE eType)
{
	bstring glsl = *psContext->currentGLSLString;
	unsigned char* start = glsl->data + glsl->slen;
	int needsParenthesis = 0;

	// Print floats as bit patterns.
	if ((eType == SVT_FLOAT) && (psContext->psShader->ui32MajorVersion > 3) && (psContext->flags & HLSLCC_FLAG_FLOATS_AS_BITPATTERNS))
	{
		bcatcstr(glsl, "intBitsToFloat(");
		eType = SVT_INT;
		needsParenthesis = 1;
	}

	switch (eType)
	{
	default:
	case SVT_INT:
	case SVT_INT16:
	case SVT_INT12:
		// Need special handling for anything >= uint 0x3fffffff
		if (value < 65536)
			bformata(glsl, "%d", value);
		else if (value > 0x3ffffffe)
			bformata(glsl, "int(0x%Xu)", value);
		else
			bformata(glsl, "0x%X", value);
		break;
	case SVT_UINT:
	case SVT_UINT16:
		// Need special handling for anything >= uint 0x3fffffff
		if (value < 65536)
			bformata(glsl, "%uu", value);
		else if (value == 0xFFFFFFFF)
			bformata(glsl, "~0u");
		else
			bformata(glsl, "0x%Xu", value);
		break;
	case SVT_FLOAT:
		if ((value & 0x7F800000) == 0x7F800000)
			bformata(glsl, "uintBitsToFloat(0x%Xu /* %f */)", value, *((float *)(&value)));
		else if (floor(*((float *)(&value))) == *((float *)(&value)))
			bformata(glsl, "%d.0f", (int)(*((float *)(&value))));
		else if (floor(*((float *)(&value)) * 4096) == (*((float *)(&value)) * 4096))
			bformata(glsl, "%f", *((float *)(&value)));
		else
		{
			// Otherwise atan2/sin etc. will break because of imprecise factors
			bformata(glsl, "uintBitsToFloat(0x%Xu /* %f */)", value, *((float *)(&value)));
		//	bformata(glsl, "%g", *((float *)(&value)));
		//	if (strchr(start, '.'))
		//		bformata(glsl, "f");
		}
		break;
	case SVT_BOOL:
		if (value == 0)
			bcatcstr(glsl, "false");
		else
			bcatcstr(glsl, "true");
		break;
	}

	if (needsParenthesis)
		bcatcstr(glsl, ")");
}

static void TranslateVariableNameWithMask(HLSLCrossCompilerContext* psContext, const Operand* psOperand, uint32_t ui32TOFlag, uint32_t* pui32IgnoreSwizzle, uint32_t ui32CompMask)
{
	const int usePrec = psContext->flags & HLSLCC_FLAG_FORCE_PRECISION_QUALIFIERS;

	int numParenthesis = 0;
	int hasCtor = 0;
	bstring glsl = *psContext->currentGLSLString;
	SHADER_VARIABLE_TYPE requestedType = TypeFlagsToSVTType(ui32TOFlag);
	SHADER_VARIABLE_TYPE eType = GetOperandDataTypeEx(psContext, psOperand, requestedType);
	int numComponents = GetNumSwizzleElementsWithMask(psOperand, ui32CompMask);
	int requestedComponents = 0;

	if (ui32TOFlag & TO_AUTO_EXPAND_TO_VEC2)
		requestedComponents = 2;
	else if (ui32TOFlag & TO_AUTO_EXPAND_TO_VEC3)
		requestedComponents = 3;
	else if (ui32TOFlag & TO_AUTO_EXPAND_TO_VEC4)
		requestedComponents = 4;

	requestedComponents = max(requestedComponents, numComponents);
	*pui32IgnoreSwizzle = 0;

	if (!(ui32TOFlag & (TO_FLAG_DESTINATION | TO_FLAG_NAME_ONLY | TO_FLAG_DECLARATION_NAME)))
	{
		if (psOperand->eType == OPERAND_TYPE_IMMEDIATE32 || psOperand->eType == OPERAND_TYPE_IMMEDIATE64)
		{
			// Mark the operand type to match whatever we're asking for in the flags.
			((Operand *)psOperand)->aeDataType[0] = requestedType;
			((Operand *)psOperand)->aeDataType[1] = requestedType;
			((Operand *)psOperand)->aeDataType[2] = requestedType;
			((Operand *)psOperand)->aeDataType[3] = requestedType;
		}

		// OPERAND_TYPE_IMMEDIATE_CONSTANT_BUFFER has it's own conversion-macros
		if (eType != requestedType && psOperand->eType != OPERAND_TYPE_IMMEDIATE_CONSTANT_BUFFER)
		{
			// Simple path: types match.
			if (CanDoDirectCast(psContext, eType, requestedType))
			{
				const char *cast = GetDirectCastOp(psContext, eType, requestedType, numComponents);
				if (cast[0] != '\0')
				{
					bformata(glsl, "%s(", cast);
					numParenthesis++;
					hasCtor = 1;
				}
			}
			else
			{
				// Direct cast not possible, need to do bitcast.
				bformata(glsl, "%s(", GetReinterpretCastOp(eType, requestedType));
				numParenthesis++;
			}
		}

		// Add ctor if needed (upscaling)
		if (numComponents < requestedComponents && (hasCtor == 0))
		{
			ASSERT(numComponents == 1);
			bformata(glsl, "%s(", GetConstructorForType(requestedType, requestedComponents, usePrec));
			numParenthesis++;
			hasCtor = 1;
		}
	}

	switch(psOperand->eType)
	{
	case OPERAND_TYPE_IMMEDIATE32:
		{
			if(psOperand->iNumComponents == 1)
			{
				printImmediate32(psContext, *((unsigned int*)(&psOperand->afImmediates[0])), requestedType);
			}
			else
			{
				int i;
				int firstItemAdded = 0;
				if (hasCtor == 0)
				{
					bformata(glsl, "%s(", GetConstructorForType(requestedType, numComponents, usePrec));
					numParenthesis++;
					hasCtor = 1;
				}
				for (i = 0; i < 4; i++)
				{
					uint32_t uval;
					if (!(ui32CompMask & (1 << i)))
						continue;

					if (firstItemAdded)
						bcatcstr(glsl, ", ");
					uval = *((uint32_t*)(&psOperand->afImmediates[i]));
					printImmediate32(psContext, uval, requestedType);
					firstItemAdded = 1;
				}
				bcatcstr(glsl, ")");
				*pui32IgnoreSwizzle = 1;
				numParenthesis--;
			}
			break;
		}
	case OPERAND_TYPE_IMMEDIATE64:
		{
			if(psOperand->iNumComponents == 1)
			{
				bformata(glsl, "%e",
					psOperand->adImmediates[0]);
			}
			else
			{
				bformata(glsl, "dvec4(%e, %e, %e, %e)",
					psOperand->adImmediates[0],
					psOperand->adImmediates[1],
					psOperand->adImmediates[2],
					psOperand->adImmediates[3]);
				if(psOperand->iNumComponents != 4)
				{
					AddSwizzleUsingElementCount(psContext, psOperand->iNumComponents);
				}
			}
			break;
		}
	case OPERAND_TYPE_INPUT:
		{
			switch(psOperand->iIndexDims)
			{
			case INDEX_2D:
				{
					InOutSignature* psSignature = NULL;
					GetInputSignatureFromRegister(psOperand->ui32RegisterNumber, &psContext->psShader->sInfo, &psSignature);

					if ((psSignature->eSystemValueType == NAME_POSITION && psSignature->ui32SemanticIndex == 0) ||
						(!strcmp(psSignature->SemanticName, "POS") && psSignature->ui32SemanticIndex == 0) ||
						(!strcmp(psSignature->SemanticName, "SV_POSITION") && psSignature->ui32SemanticIndex == 0))
					{
						bcatcstr(glsl, "gl_in");
						TranslateOperandIndex(psContext, psOperand, 0);//Vertex index
						bcatcstr(glsl, ".gl_Position");
					}
					else
					{
						bstring inputName = bformat("Input%d", psOperand->aui32ArraySizes[1]);

						char* name = NULL;
						if (ui32TOFlag & TO_FLAG_DECLARATION_NAME)
							name = GetDeclaredInputName(psContext, psContext->psShader->eShaderType, psOperand);
						else
							name = bstr2cstr(inputName, '\0');

						bformata(glsl, "%s", name);
						TranslateOperandIndex(psContext, psOperand, 0);//Vertex index

						bdestroy(inputName);
						bcstrfree((char*)name);
					}
					break;
				}
			default:
				{
					if(psOperand->eIndexRep[0] == OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE)
					{
						bformata(glsl, "Input%d[", psOperand->ui32RegisterNumber);
						TranslateOperand(psContext, psOperand->psSubOperand[0], TO_FLAG_INTEGER | TO_AUTO_BITCAST_TO_INT);
						bcatcstr(glsl, "]");
					}
					else
					{
						if(psContext->psShader->aIndexedInput[psOperand->ui32RegisterNumber] != 0)
						{
							const uint32_t parentIndex = psContext->psShader->aIndexedInputParents[psOperand->ui32RegisterNumber];
							bformata(glsl, "Input%d[%d]", parentIndex, psOperand->ui32RegisterNumber - parentIndex);
						}
						else
						{
							bstring inputName = bformat("Input%d", psOperand->ui32RegisterNumber);

							char* name = NULL;
							if (ui32TOFlag & TO_FLAG_DECLARATION_NAME)
								name = GetDeclaredInputName(psContext, psContext->psShader->eShaderType, psOperand);
							else
								name = bstr2cstr(inputName, '\0');

							bformata(glsl, "%s", name);

							bdestroy(inputName);
							bcstrfree((char*)name);
						}
					}
					break;
				}
			}
			break;
		}
	case OPERAND_TYPE_INPUT_CONTROL_POINT:
		{
			InOutSignature* psSignature = NULL;
			GetInputSignatureFromRegister(psOperand->ui32RegisterNumber, &psContext->psShader->sInfo, &psSignature);

			if ((psSignature->eSystemValueType == NAME_POSITION && psSignature->ui32SemanticIndex == 0) ||
				(!strcmp(psSignature->SemanticName, "POS") && psSignature->ui32SemanticIndex == 0) ||
				(!strcmp(psSignature->SemanticName, "SV_POSITION") && psSignature->ui32SemanticIndex == 0))
			{
				bcatcstr(glsl, "gl_in");
				TranslateOperandIndex(psContext, psOperand, 0);//Vertex index
				bcatcstr(glsl, ".gl_Position");
			}
			else
			{
				bstring inputName = bformat("Input%d", psOperand->aui32ArraySizes[1]);

				char* name = NULL;
				if (ui32TOFlag & TO_FLAG_DECLARATION_NAME)
					name = GetDeclaredInputName(psContext, psContext->psShader->eShaderType, psOperand);
				else
					name = bstr2cstr(inputName, '\0');

				bformata(glsl, "%s", name);
				TranslateOperandIndex(psContext, psOperand, 0);//Vertex index

				bdestroy(inputName);
				bcstrfree((char*)name);
			}
			break;
		}
	case OPERAND_TYPE_INPUT_PATCH_CONSTANT:
		{
			bstring inputName = bformat("Constant%d", psOperand->aui32ArraySizes[0]);

			char* name = NULL;
			if (ui32TOFlag & TO_FLAG_DECLARATION_NAME)
				name = GetDeclaredPatchConstantName(psContext, psContext->psShader->eShaderType, psOperand);
			else
				name = bstr2cstr(inputName, '\0');

			bformata(glsl, "%s", name);
		//	TranslateOperandIndex(psContext, psOperand, 0);//Vertex index

			bdestroy(inputName);
			bcstrfree((char*)name);
			break;
		}
	case OPERAND_TYPE_OUTPUT:
		{
			if (!IsPatchConstant(psContext, psOperand))
			{
				bformata(glsl, "Output%d", psOperand->ui32RegisterNumber);
				if (psOperand->psSubOperand[0])
				{
					bcatcstr(glsl, "["); //Indexes must be integral.
					TranslateOperand(psContext, psOperand->psSubOperand[0], TO_FLAG_INTEGER | TO_AUTO_BITCAST_TO_INT);
					bcatcstr(glsl, "]");
				}
			}
			else
			{
				bstring inputName = bformat("Constant%d", psOperand->aui32ArraySizes[0]);

				char* name = NULL;
				if (ui32TOFlag & TO_FLAG_DECLARATION_NAME)
					name = GetDeclaredPatchConstantName(psContext, psContext->psShader->eShaderType, psOperand);
				else
					name = bstr2cstr(inputName, '\0');

				bformata(glsl, "%s", name);
				// Outputs are always indexed by oSEMANTIC?[gl_InvocationID]
				//	TranslateOperandIndex(psContext, psOperand, 0);//Vertex index

				bdestroy(inputName);
				bcstrfree((char*)name);
			}
			break;
		}
	case OPERAND_TYPE_OUTPUT_CONTROL_POINT:
		{
			bstring outputName = bformat("Output%d", psOperand->aui32ArraySizes[1]);
			int stream = 0;

			char* name = NULL;
			if (ui32TOFlag & TO_FLAG_DECLARATION_NAME)
				name = GetDeclaredOutputName(psContext, psContext->psShader->eShaderType, psOperand, &stream);
			else
				name = bstr2cstr(outputName, '\0');

			bformata(glsl, "%s", name);
			// Outputs are always indexed by oSEMANTIC?[gl_InvocationID]
			//	TranslateOperandIndex(psContext, psOperand, 0);//Vertex index

			bdestroy(outputName);
			bcstrfree((char*)name);
			break;
		}
	case OPERAND_TYPE_OUTPUT_DEPTH:
	case OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
	case OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL:
		{
			bcatcstr(glsl, "gl_FragDepth");
			break;
		}
	case OPERAND_TYPE_TEMP:
		{
			bcatcstr(glsl, "Temp");

			if (psContext->flags & HLSLCC_FLAG_AVOID_TEMP_REGISTER_ALIASING && psContext->psShader->eShaderType != HULL_SHADER)
			{
				// use global temps
				bformata(glsl, "%d", psOperand->ui32RegisterNumber);
			}
			else
			{
				if (psContext->flags & HLSLCC_FLAG_AVOID_TEMP_REGISTER_ALIASING)
					bformata(glsl, "_%s", GetConstructorForType(GetOperandDataType(psContext, psOperand), 1, usePrec));

				bformata(glsl, "[%d]", psOperand->ui32RegisterNumber);
			}

			break;
		}
	case OPERAND_TYPE_SPECIAL_IMMCONSTINT:
		{
			bformata(glsl, "IntImmConst%d", psOperand->ui32RegisterNumber);
			break;
		}
	case OPERAND_TYPE_SPECIAL_IMMCONST:
		{
			if(psOperand->psSubOperand[0] != NULL)
			{
				if (psContext->psShader->aui32Dx9ImmConstArrayRemap[psOperand->ui32RegisterNumber] != 0)
					bformata(glsl, "ImmConstArray[%d + ", psContext->psShader->aui32Dx9ImmConstArrayRemap[psOperand->ui32RegisterNumber]);
				else
					bcatcstr(glsl, "ImmConstArray[");
				TranslateOperandWithMask(psContext, psOperand->psSubOperand[0], TO_FLAG_INTEGER, OPERAND_4_COMPONENT_MASK_X);
				bcatcstr(glsl, "]");
			}
			else
			{
				bformata(glsl, "ImmConst%d", psOperand->ui32RegisterNumber);
			}
			break;
		}
	case OPERAND_TYPE_SPECIAL_OUTBASECOLOUR:
		{
			bcatcstr(glsl, "BaseColour");
			break;
		}
	case OPERAND_TYPE_SPECIAL_OUTOFFSETCOLOUR:
		{
			bcatcstr(glsl, "OffsetColour");
			break;
		}
	case OPERAND_TYPE_SPECIAL_POSITION:
		{
			bcatcstr(glsl, "gl_Position");
			break;
		}
	case OPERAND_TYPE_SPECIAL_FOG:
		{
			bcatcstr(glsl, "Fog");
			break;
		}
	case OPERAND_TYPE_SPECIAL_POINTSIZE:
		{
			bcatcstr(glsl, "gl_PointSize");
			break;
		}
	case OPERAND_TYPE_SPECIAL_ADDRESS:
		{
			bcatcstr(glsl, "Address");
			break;
		}
	case OPERAND_TYPE_SPECIAL_LOOPCOUNTER:
		{
			bcatcstr(glsl, "LoopCounter");
			pui32IgnoreSwizzle[0] = 1;
			break;
		}
	case OPERAND_TYPE_SPECIAL_TEXCOORD_INPUT:
	case OPERAND_TYPE_SPECIAL_TEXCOORD_OUTPUT:
		{
			bformata(glsl, "TexCoord%d", psOperand->ui32RegisterNumber);
			break;
		}
	case OPERAND_TYPE_CONSTANT_BUFFER:
		{
			ConstantBuffer* psCBuf = NULL;
			ShaderVarType* psVarType = NULL;
			GetConstantBufferFromBindingPoint(RGROUP_CBUFFER, psOperand->aui32ArraySizes[0], &psContext->psShader->sInfo, &psCBuf);

			if(ui32TOFlag & TO_FLAG_DECLARATION_NAME)
			{
				pui32IgnoreSwizzle[0] = 1;
			}

			// FIXME: With ES 3.0 the buffer name is often not prepended to variable names
			if(((psContext->flags & HLSLCC_FLAG_UNIFORM_BUFFER_OBJECT)!=HLSLCC_FLAG_UNIFORM_BUFFER_OBJECT) &&
				((psContext->flags & HLSLCC_FLAG_DISABLE_GLOBALS_STRUCT)!=HLSLCC_FLAG_DISABLE_GLOBALS_STRUCT))
			{
				if(psCBuf)
				{
					//$Globals.
					if(psCBuf->Name[0] == '$')
					{
						ConvertToUniformBufferName(glsl, psContext->psShader, "$Globals", psOperand->aui32ArraySizes[0]);
					}
					else
					{
						ConvertToUniformBufferName(glsl, psContext->psShader, psCBuf->Name, psOperand->aui32ArraySizes[0]);
					}
					if((ui32TOFlag & TO_FLAG_DECLARATION_NAME) != TO_FLAG_DECLARATION_NAME)
					{
						bcatcstr(glsl, ".");
					}
				}
				else
				{
					//bformata(glsl, "cb%d", psOperand->aui32ArraySizes[0]);
				}
			}

			if ((ui32TOFlag & TO_FLAG_DECLARATION_NAME) != TO_FLAG_DECLARATION_NAME)
			{
				//Work out the variable name. Don't apply swizzle to that variable yet.
				int32_t indices[16] = { -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1 };
				int32_t index = -1;
				int32_t rebase = 0;
				int relativeOffset = psOperand->psSubOperand[1] != NULL ? 1 : 0;

				if (psCBuf && !psCBuf->blob)
				{
					GetShaderVarFromOffset(psOperand->aui32ArraySizes[1], psOperand->aui32Swizzle, psCBuf, &psVarType, indices, &rebase);

					// Manage vSPI[floatBitsToInt(Temp0).x].vSPIAlphaTest.xw;
					// TODO:  vSPI[floatBitsToInt(Temp0).x].vSPIAlphaTest[arg].xw;
					if (psVarType->Parent != NULL)
						ShaderVarFullName(glsl, psContext->psShader, psVarType->Parent);
					else
						ShaderVarFullName(glsl, psContext->psShader, psVarType);

					index = indices[0];
				}
				else if (psCBuf)
				{
					ConvertToUniformBufferName(glsl, psContext->psShader, psCBuf->Name, psOperand->aui32ArraySizes[0]);
					bcatcstr(glsl, "_data");
					index = psOperand->aui32ArraySizes[1];
				}
				// We don't have a semantic for this variable, so try the raw dump approach.
				else
				{
					bformata(glsl, "cb%d.data", psOperand->aui32ArraySizes[0]);//
					index = psOperand->aui32ArraySizes[1];
				}

				// cbStruct[offset<reg>]
				// cbStruct[offset<imm>]
				if (psVarType->Parent != NULL && psVarType->Parent->Elements > 0)
				{
					if (relativeOffset)
					{
						SHADER_VARIABLE_TYPE eType = GetOperandDataType(psContext, psOperand->psSubOperand[1]);
						bcatcstr(glsl, "[");
						TranslateOperand(psContext, psOperand->psSubOperand[1], TO_FLAG_INTEGER | TO_AUTO_BITCAST_TO_INT);
						bcatcstr(glsl, "]");
					}
					else
					{
						bformata(glsl, "[%d]", index);
					}

					// turn off second level of []
					memmove(indices, indices + 1, 15);
					relativeOffset = 0;
					index = indices[0];
				}

				// cbStruct[offset].member
				if (psVarType->Parent != NULL)
				{
					bcatcstr(glsl, ".");
					ShaderVarName(glsl, psContext->psShader, psVarType->Name);
				}

				//Dx9 only?
				if (psOperand->psSubOperand[0] != NULL)
				{
					// Array of matrices is treated as array of vec4s in HLSL,
					// but that would mess up uniform types in GLSL. Do gymnastics.
					SHADER_VARIABLE_TYPE eType = GetOperandDataType(psContext, psOperand->psSubOperand[0]);
					uint32_t opFlags = TO_FLAG_INTEGER;
					if (eType != SVT_INT && eType != SVT_UINT && eType != SVT_BOOL)
						opFlags = TO_AUTO_BITCAST_TO_INT;

#if BOGUS // TODO
					if ((psVarType->Class == SVC_MATRIX_COLUMNS || psVarType->Class == SVC_MATRIX_ROWS) && (psVarType->Elements > 1))
					{
						// Special handling for matrix arrays
						bcatcstr(glsl, "[");
						TranslateOperand(psContext, psOperand->psSubOperand[0], opFlags);
						bformata(glsl, " / 4]");
						if (psContext->psShader->eTargetLanguage <= LANG_120)
						{
							bcatcstr(glsl, "[mod(float("); //Indexes must be integral.
							TranslateOperandWithMask(psContext, psOperand->psSubOperand[0], opFlags, OPERAND_4_COMPONENT_MASK_X);
							bformata(glsl, "), 4.0f)]");
						}
						else
					{
							bcatcstr(glsl, "["); //Indexes must be integral.
							TranslateOperandWithMask(psContext, psOperand->psSubOperand[0], opFlags, OPERAND_4_COMPONENT_MASK_X);
							bformata(glsl, " %% 4]");
						}
					}
					else
#endif
					{
						bcatcstr(glsl, "["); //Indexes must be integral.
						TranslateOperand(psContext, psOperand->psSubOperand[0], opFlags);
						bformata(glsl, "]");
					}
				}
				// cbStruct.member[offset+index]
				else if (index != -1 && relativeOffset)
				{
					// Array of matrices is treated as array of vec4s in HLSL,
					// but that would mess up uniform types in GLSL. Do gymnastics.
					SHADER_VARIABLE_TYPE eType = GetOperandDataType(psContext, psOperand->psSubOperand[1]);
					uint32_t opFlags = TO_FLAG_INTEGER;
					if (eType != SVT_INT && eType != SVT_UINT && eType != SVT_BOOL)
						opFlags = TO_AUTO_BITCAST_TO_INT;

#if BOGUS // TODO
					if ((psVarType->Class == SVC_MATRIX_COLUMNS || psVarType->Class == SVC_MATRIX_ROWS) && (psVarType->Elements > 1))
					{
						// Special handling for matrix arrays
						bcatcstr(glsl, "[(");
						TranslateOperand(psContext, psOperand->psSubOperand[1], opFlags);
						bformata(glsl, " + %d) / 4]", index);
						if (psContext->psShader->eTargetLanguage <= LANG_120)
						{
							bcatcstr(glsl, "[mod(float(");
							TranslateOperand(psContext, psOperand->psSubOperand[1], opFlags);
							bformata(glsl, " + %d), 4.0)]", index);
						}
						else
						{
							bcatcstr(glsl, "[(");
							TranslateOperand(psContext, psOperand->psSubOperand[1], opFlags);
							bformata(glsl, " + %d) %% 4]", index);
						}
					}
					else
#endif
					{
						bcatcstr(glsl, "[");
						TranslateOperand(psContext, psOperand->psSubOperand[1], opFlags);
						bformata(glsl, " + %d]", index);
					}
				}
				// cbStruct.member[index]
				// cbStruct[offset].member[index]
				else if (index != -1)
				{
#if BOGUS // TODO
					if ((psVarType->Class == SVC_MATRIX_COLUMNS || psVarType->Class == SVC_MATRIX_ROWS) && (psVarType->Elements > 1))
					{
						// Special handling for matrix arrays, open them up into vec4's
						size_t matidx = index / 4;
						size_t rowidx = index - (matidx * 4);
						bformata(glsl, "[%d][%d]", matidx, rowidx);
					}
					else
#endif
					if (index != -1)
					{
						bformata(glsl, "[%d]", index);
					}
				}
				else if (relativeOffset)
				{
					SHADER_VARIABLE_TYPE eType = GetOperandDataType(psContext, psOperand->psSubOperand[1]);
					bcatcstr(glsl, "[");
					TranslateOperand(psContext, psOperand->psSubOperand[1], TO_FLAG_INTEGER | TO_AUTO_BITCAST_TO_INT);
					bcatcstr(glsl, "]");
				}

				if (psVarType && psVarType->Class == SVC_VECTOR)
				{
					int e, elements = 0;

					// (rebase == 4), (psVarType->Columns == 2), ".xxyx", .x(GLSL) is .y(HLSL). .y(GLSL) is .z(HLSL)
					// (rebase == 4), (psVarType->Columns == 3), ".xxyz", .x(GLSL) is .y(HLSL). .y(GLSL) is .z(HLSL) .z(GLSL) is .w(HLSL)
					// (rebase == 8), (psVarType->Columns == 2), ".xxxy", .x(GLSL) is .z(HLSL). .y(GLSL) is .w(HLSL)
					// (rebase == 0), (psVarType->Columns == 2), ".xyxx", No rebase, but extend to vec4.
					// (rebase == 0), (psVarType->Columns == 3), ".xyzx", No rebase, but extend to vec4.

					if (rebase || (psVarType->Columns < ((uint32_t)requestedComponents)))
					{
						bcatcstr(glsl, ".");

						for (e = rebase / 4; rebase && (elements < 4); --e, elements += 1)
							bcatcstr(glsl, "x"); // dummy

						for (e = 0; (((uint32_t)e) < psVarType->Columns) && (elements < 4); ++e, elements += 1)
							bcatcstr(glsl, cComponentNames[e]);

						for (e = psVarType->Columns; (e < requestedComponents) && (elements < 4); ++e, elements += 1)
							bcatcstr(glsl, "x"); // dummy
					}

					// anticipate simple pass-through swizzle and prevent it
					*pui32IgnoreSwizzle = !rebase && (psVarType->Columns == ((uint32_t)requestedComponents)) &&
						(psOperand->eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE) &&
						(1 > numComponents || psOperand->aui32Swizzle[0] == OPERAND_4_COMPONENT_X) &&
						(2 > numComponents || psOperand->aui32Swizzle[1] == OPERAND_4_COMPONENT_Y) &&
						(3 > numComponents || psOperand->aui32Swizzle[2] == OPERAND_4_COMPONENT_Z) &&
						(4 > numComponents || psOperand->aui32Swizzle[3] == OPERAND_4_COMPONENT_W);
				}
				else if (psVarType && psVarType->Class == SVC_SCALAR)
				{
					*pui32IgnoreSwizzle = 1;
				}
			}
			break;
		}
	case OPERAND_TYPE_RESOURCE:
		{
		//	ResourceName(*psContext->currentGLSLString, psContext, RGROUP_TEXTURE, psOperand->ui32RegisterNumber, 0);
			TextureName(*psContext->currentGLSLString, psContext->psShader, psOperand->ui32RegisterNumber, MAX_RESOURCE_BINDINGS, 0);
			*pui32IgnoreSwizzle = 1;
			break;
		}
	case OPERAND_TYPE_SAMPLER:
		{
			bformata(glsl, "Sampler%d", psOperand->ui32RegisterNumber);
			*pui32IgnoreSwizzle = 1;
			break;
		}
	case OPERAND_TYPE_FUNCTION_BODY:
		{
			const uint32_t ui32FuncBody = psOperand->ui32RegisterNumber;
			const uint32_t ui32FuncTable = psContext->psShader->aui32FuncBodyToFuncTable[ui32FuncBody];
			//const uint32_t ui32FuncPointer = psContext->psShader->aui32FuncTableToFuncPointer[ui32FuncTable];
			const uint32_t ui32ClassType = psContext->psShader->sInfo.aui32TableIDToTypeID[ui32FuncTable];
			const char* ClassTypeName = &psContext->psShader->sInfo.psClassTypes[ui32ClassType].Name[0];
			const uint32_t ui32UniqueClassFuncIndex = psContext->psShader->ui32NextClassFuncName[ui32ClassType]++;

			bformata(glsl, "%s_Func%d", ClassTypeName, ui32UniqueClassFuncIndex);
			break;
		}
	case OPERAND_TYPE_INPUT_FORK_INSTANCE_ID:
		{
			bcatcstr(glsl, "forkInstanceID");
			*pui32IgnoreSwizzle = 1;
			return;
		}
	case OPERAND_TYPE_IMMEDIATE_CONSTANT_BUFFER:
		{
			if (IsIntegerSigned(requestedType))
				bcatcstr(glsl, "immediateConstBufferI");
			else if (IsIntegerUnsigned(requestedType))
				bcatcstr(glsl, "immediateConstBufferI");
			else
				bcatcstr(glsl, "immediateConstBufferF");

			if (psOperand->psSubOperand[0])
			{
				bcatcstr(glsl, "("); //Indexes must be integral.
				TranslateOperand(psContext, psOperand->psSubOperand[0], TO_FLAG_INTEGER | TO_AUTO_BITCAST_TO_INT);
				bcatcstr(glsl, ")");
			}
			break;
		}
	case OPERAND_TYPE_INPUT_DOMAIN_POINT:
		{
			bcatcstr(glsl, "gl_TessCoord");
			break;
		}
	case OPERAND_TYPE_NULL:
		{
			// Null register, used to discard results of operations
			bcatcstr(glsl, "//null");
			break;
		}
	case OPERAND_TYPE_OUTPUT_CONTROL_POINT_ID:
		{
			bcatcstr(glsl, "gl_InvocationID");
			*pui32IgnoreSwizzle = 1;
			break;
		}
	case OPERAND_TYPE_OUTPUT_COVERAGE_MASK:
		{
			bcatcstr(glsl, "gl_SampleMask[0]");
			*pui32IgnoreSwizzle = 1;
			break;
		}
	case OPERAND_TYPE_INPUT_COVERAGE_MASK:
		{
			bcatcstr(glsl, "gl_SampleMaskIn[0]");
			//Skip swizzle on scalar types.
			*pui32IgnoreSwizzle = 1;
			break;
		}
	case OPERAND_TYPE_INPUT_THREAD_ID://SV_DispatchThreadID
		{
			bcatcstr(glsl, "gl_GlobalInvocationID");
			break;
		}
	case OPERAND_TYPE_INPUT_THREAD_GROUP_ID://SV_GroupThreadID
		{
			bcatcstr(glsl, "gl_WorkGroupID");
			break;
		}
	case OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP://SV_GroupID
		{
			bcatcstr(glsl, "gl_LocalInvocationID");
			break;
		}
	case OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED://SV_GroupIndex
		{
			bcatcstr(glsl, "gl_LocalInvocationIndex");
			*pui32IgnoreSwizzle = 1; // No swizzle meaningful for scalar.
			break;
		}
	case OPERAND_TYPE_UNORDERED_ACCESS_VIEW:
		{
		//	ResourceName(*psContext->currentGLSLString, psContext, RGROUP_UAV, psOperand->ui32RegisterNumber, 0);
			UAVName(*psContext->currentGLSLString, psContext->psShader, psOperand->ui32RegisterNumber);
			break;
		}
	case OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY:
		{
			bformata(glsl, "TGSM%d", psOperand->ui32RegisterNumber);
			*pui32IgnoreSwizzle = 1;
			break;
		}
	case OPERAND_TYPE_INPUT_PRIMITIVEID:
		{
			bcatcstr(glsl, "gl_PrimitiveID");
			break;
		}
	case OPERAND_TYPE_INDEXABLE_TEMP:
		{
			bformata(glsl, "TempArray%d", psOperand->aui32ArraySizes[0]);
			if (psContext->flags & HLSLCC_FLAG_AVOID_TEMP_REGISTER_ALIASING)
				bformata(glsl, "_%s", GetConstructorForType(GetOperandDataType(psContext, psOperand), 1, usePrec));

			bcatcstr(glsl, "[");
			if (psOperand->aui32ArraySizes[1] != 0 || !psOperand->psSubOperand[1])
				bformata(glsl, "%d", psOperand->aui32ArraySizes[1]);

			if(psOperand->psSubOperand[1])
			{
				if (psOperand->aui32ArraySizes[1] != 0)
					bcatcstr(glsl, "+");
				TranslateOperand(psContext, psOperand->psSubOperand[1], TO_FLAG_UNSIGNED_INTEGER);

			}
			bcatcstr(glsl, "]");
			break;
		}
	case OPERAND_TYPE_STREAM:
		{
			bformata(glsl, "%d", psOperand->ui32RegisterNumber);
			break;
		}
	case OPERAND_TYPE_INPUT_GS_INSTANCE_ID:
		{
			// In HLSL the instance id is uint, so cast here.
			bcatcstr(glsl, "uint(gl_InvocationID)");
			break;
		}
	case OPERAND_TYPE_THIS_POINTER:
		{
			/*
			The "this" register is a register that provides up to 4 pieces of information:
			X: Which CB holds the instance data
			Y: Base element offset of the instance data within the instance CB
			Z: Base sampler index
			W: Base Texture index

			Can be different for each function call
			*/
			break;
		}
	default:
		{
			ASSERT(0);
			break;
		}
	}

	if (hasCtor && (*pui32IgnoreSwizzle == 0))
	{
		TranslateOperandSwizzleWithMask(psContext, psOperand, ui32CompMask);
		*pui32IgnoreSwizzle = 1;
	}

	while (numParenthesis != 0)
	{
		bcatcstr(glsl, ")");
		numParenthesis--;
	}

#if 0
	if (boolResult)
	{
		if (requestedType == SVT_UINT)
			bcatcstr(glsl, " * 1u");
		else
			bcatcstr(glsl, " * int(1u)");
	}
#endif
}

void TranslateVariableName(HLSLCrossCompilerContext* psContext, const Operand* psOperand, uint32_t ui32TOFlag, uint32_t* pui32IgnoreSwizzle)
{
	TranslateVariableNameWithMask(psContext, psOperand, ui32TOFlag, pui32IgnoreSwizzle, OPERAND_4_COMPONENT_MASK_ALL);
}

#if 0
void TranslateVariableName(HLSLCrossCompilerContext* psContext, const Operand* psOperand, uint32_t ui32TOFlag, uint32_t* pui32IgnoreSwizzle)
{
	int integerConstructor = 0;
	bstring glsl = *psContext->currentGLSLString;

	*pui32IgnoreSwizzle = 0;

	if(psOperand->eType != OPERAND_TYPE_IMMEDIATE32 &&
		psOperand->eType != OPERAND_TYPE_IMMEDIATE64)
	{
		const uint32_t swizCount = psOperand->iNumComponents;
		SHADER_VARIABLE_TYPE eType = GetOperandDataType(psContext, psOperand);

		if( (ui32TOFlag & (TO_FLAG_INTEGER|TO_FLAG_UNSIGNED_INTEGER)) == (TO_FLAG_INTEGER|TO_FLAG_UNSIGNED_INTEGER))
		{
			//Can be either int or uint
			if(eType != SVT_INT && eType != SVT_UINT)
			{
				if (eType == SVT_FLOAT)
					bformata(glsl, "floatBitsToInt(");
				else if(swizCount == 1)
					bformata(glsl, "int(");
				else
					bformata(glsl, "ivec%d(", swizCount);

				integerConstructor = 1;
			}
		}
		else
		{
			if((ui32TOFlag & (TO_FLAG_INTEGER|TO_FLAG_DESTINATION))==TO_FLAG_INTEGER &&
				eType != SVT_INT)
			{
				//Convert to int
				if (eType == SVT_FLOAT)
					bformata(glsl, "floatBitsToInt(");
				else if(swizCount == 1)
					bformata(glsl, "int(");
				else
					bformata(glsl, "ivec%d(", swizCount);

				integerConstructor = 1;
			}
			if((ui32TOFlag & (TO_FLAG_UNSIGNED_INTEGER|TO_FLAG_DESTINATION))==TO_FLAG_UNSIGNED_INTEGER &&
				eType != SVT_UINT)
			{
				//Convert to uint
				if (eType == SVT_FLOAT)
					bformata(glsl, "floatBitsToUint(");
				else if(swizCount == 1)
					bformata(glsl, "uint(");
				else
					bformata(glsl, "uvec%d(", swizCount);
				integerConstructor = 1;
			}
			if((ui32TOFlag & (TO_FLAG_FLOAT|TO_FLAG_DESTINATION))==TO_FLAG_FLOAT &&
				eType != SVT_FLOAT)
			{
				if (eType == SVT_UINT)
					bformata(glsl, "uintBitsToFloat(");
				else
				{
					ASSERT(eType == SVT_INT);
					bformata(glsl, "intBitsToFloat(");
				}

				integerConstructor = 1;
			}
		}
	}

	if (ui32TOFlag & TO_FLAG_COPY)
	{
		bcatcstr(glsl, "TempCopy");
		if ((psContext->flags & HLSLCC_FLAG_AVOID_TEMP_REGISTER_ALIASING) == 0)
		{
			SHADER_VARIABLE_TYPE eType = GetOperandDataType(psContext, psOperand);
			switch (eType)
			{
			case SVT_FLOAT:
				break;
			case SVT_INT:
				bcatcstr(glsl, "_int"); 
				break;
			case SVT_UINT:
				bcatcstr(glsl, "_uint");
				break;
			case SVT_DOUBLE:
				bcatcstr(glsl, "_double");
				break;
			default:
				ASSERT(0);
				break;
			}
		}
	}
	else
	{
		TranslateVariableNameByOperandType(psContext, psOperand, ui32TOFlag, pui32IgnoreSwizzle);
	}

	if(integerConstructor)
	{
		bcatcstr(glsl, ")");
	}
}
#endif

SHADER_VARIABLE_TYPE GetOperandDataType(HLSLCrossCompilerContext* psContext, const Operand* psOperand)
{
	return GetOperandDataTypeEx(psContext, psOperand, SVT_INT);
}

SHADER_VARIABLE_TYPE GetOperandDataTypeEx(HLSLCrossCompilerContext* psContext, const Operand* psOperand, SHADER_VARIABLE_TYPE ePreferredTypeForImmediates)
{
	// The min precision qualifier overrides all of the stuff below
	if (HavePrecisionQualifers(psContext->psShader->eTargetLanguage, psContext->flags))
	{
		switch (psOperand->eMinPrecision)
		{
		case OPERAND_MIN_PRECISION_ANY_16:
		case OPERAND_MIN_PRECISION_FLOAT_16:
			{
				return SVT_FLOAT16;
		}
		case OPERAND_MIN_PRECISION_ANY_10:
		case OPERAND_MIN_PRECISION_FLOAT_2_8:
			{
				return SVT_FLOAT10;
			}
		case OPERAND_MIN_PRECISION_SINT_16:
			{
				return SVT_INT16;
			}
		case OPERAND_MIN_PRECISION_UINT_16:
			{
				return SVT_UINT16;
			}
		default:
			{
				break;
			}
		}
	}

	switch(psOperand->eType)
	{
	case OPERAND_TYPE_TEMP:
		{
			SHADER_VARIABLE_TYPE eCurrentType;
			int i = 0;

			if (psContext->flags & HLSLCC_FLAG_AVOID_TEMP_REGISTER_ALIASING && psContext->psShader->eShaderType != HULL_SHADER)
				return psContext->psShader->aeCommonTempVecType[psOperand->ui32RegisterNumber];

			if(psOperand->eSelMode == OPERAND_4_COMPONENT_SELECT_1_MODE)
			{
				return psOperand->aeDataType[psOperand->aui32Swizzle[0]];
			}
			if(psOperand->eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE)
			{
				if(psOperand->ui32Swizzle == (NO_SWIZZLE))
				{
					return psOperand->aeDataType[0];
				}

				return psOperand->aeDataType[psOperand->aui32Swizzle[0]];
			}

			if(psOperand->eSelMode == OPERAND_4_COMPONENT_MASK_MODE)
			{
				uint32_t ui32CompMask = psOperand->ui32CompMask;
				if(!psOperand->ui32CompMask)
				{
					ui32CompMask = OPERAND_4_COMPONENT_MASK_ALL;
				}
				for(;i<4;++i)
				{
					if(ui32CompMask & (1<<i))
					{
						eCurrentType = psOperand->aeDataType[i];
						break;
					}
				}

#ifdef _DEBUG
				//Check if all elements have the same basic type.
				for(;i<4;++i)
				{
					if(psOperand->ui32CompMask & (1<<i))
					{
						if(eCurrentType != psOperand->aeDataType[i])
						{
							ASSERT(0);
						}
					}
				}
#endif
				return eCurrentType;
			}

			ASSERT(0);

			break;
		}
	case OPERAND_TYPE_OUTPUT:
		{
			const uint32_t ui32Register = psOperand->aui32ArraySizes[psOperand->iIndexDims-1];
			InOutSignature* psOut;
			
			//UINT in DX, INT in GL.
			switch (psOperand->eSpecialName)
			{
			case NAME_PRIMITIVE_ID:
			case NAME_VERTEX_ID:
			case NAME_INSTANCE_ID:
			case NAME_RENDER_TARGET_ARRAY_INDEX:
			case NAME_VIEWPORT_ARRAY_INDEX:
			case NAME_SAMPLE_INDEX:
				{
					return SVT_INT;
				}

			case NAME_IS_FRONT_FACE:
				{
					return SVT_BOOL;
				}

			case NAME_POSITION:
			case NAME_CLIP_DISTANCE:
				{
					return SVT_FLOAT;
				}

			default:
				{
					break;
				}
			}

			if (!IsPatchConstant(psContext, psOperand) /*psOperand->eType != OPERAND_TYPE_INPUT_PATCH_CONSTANT*/)
				GetOutputSignatureFromRegister(psContext->currentPhase, ui32Register, psOperand->ui32CompMask, 0, &psContext->psShader->sInfo, &psOut);
			else
				GetPatchConstantSignatureFromRegister(ui32Register, psOperand->ui32CompMask, &psContext->psShader->sInfo, &psOut);

			if (psOut)
			{
				//UINT in DX, INT in GL.
				switch (psOut->eSystemValueType)
				{
				case NAME_PRIMITIVE_ID:
				case NAME_VERTEX_ID:
				case NAME_INSTANCE_ID:
				case NAME_RENDER_TARGET_ARRAY_INDEX:
				case NAME_VIEWPORT_ARRAY_INDEX:
				case NAME_SAMPLE_INDEX:
					{
						return SVT_INT;
					}

				case NAME_IS_FRONT_FACE:
					{
						return SVT_BOOL;
					}

				case NAME_POSITION:
				case NAME_CLIP_DISTANCE:
					{
						return SVT_FLOAT;
					}

				default:
					{
						break;
					}
				}

				// This is HLSL side (may be different in GLSL)
				if (psOut->eComponentType == INOUT_COMPONENT_UINT32)
				{
					return SVT_UINT;
				}
				else if (psOut->eComponentType == INOUT_COMPONENT_SINT32)
				{
					return SVT_INT;
				}
			}
			break;
		}
	case OPERAND_TYPE_INPUT:
		{
			const uint32_t ui32Register = psOperand->aui32ArraySizes[psOperand->iIndexDims-1];
			InOutSignature* psIn;

			//UINT in DX, INT in GL.
			switch (psOperand->eSpecialName)
			{
			case NAME_PRIMITIVE_ID:
			case NAME_VERTEX_ID:
			case NAME_INSTANCE_ID:
			case NAME_RENDER_TARGET_ARRAY_INDEX:
			case NAME_VIEWPORT_ARRAY_INDEX:
			case NAME_SAMPLE_INDEX:
				{
					return SVT_INT;
				}

			case NAME_IS_FRONT_FACE:
				{
					return SVT_BOOL;
				}

			case NAME_POSITION:
			case NAME_CLIP_DISTANCE:
				{
					return SVT_FLOAT;
				}

			default:
				{
					break;
				}
			}

			if (!IsPatchConstant(psContext, psOperand) /*psOperand->eType != OPERAND_TYPE_INPUT_PATCH_CONSTANT*/)
				GetInputSignatureFromRegister(ui32Register, &psContext->psShader->sInfo, &psIn);
			else
				GetPatchConstantSignatureFromRegister(ui32Register, psOperand->ui32CompMask, &psContext->psShader->sInfo, &psIn);

			if (psIn)
			{
				//UINT in DX, INT in GL.
				switch (psIn->eSystemValueType)
				{
				case NAME_PRIMITIVE_ID:
				case NAME_VERTEX_ID:
				case NAME_INSTANCE_ID:
				case NAME_RENDER_TARGET_ARRAY_INDEX:
				case NAME_VIEWPORT_ARRAY_INDEX:
				case NAME_SAMPLE_INDEX:
					{
						return SVT_INT;
					}

				case NAME_IS_FRONT_FACE:
					{
						return SVT_BOOL;
					}

				case NAME_POSITION:
				case NAME_CLIP_DISTANCE:
					{
						return SVT_FLOAT;
					}

				default:
					{
						break;
					}
				}

				// This is HLSL side (may be different in GLSL)
				if (psIn->eComponentType == INOUT_COMPONENT_UINT32)
				{
					return SVT_UINT;
				}
				else if (psIn->eComponentType == INOUT_COMPONENT_SINT32)
				{
					return SVT_INT;
				}
			}
			break;
		}
	case OPERAND_TYPE_CONSTANT_BUFFER:
		{
			ConstantBuffer* psCBuf = NULL;
			ShaderVarType* psVarType = NULL;
			int32_t indices[16] = { -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1 };
			int32_t rebase = -1;

			GetConstantBufferFromBindingPoint(RGROUP_CBUFFER, psOperand->aui32ArraySizes[0], &psContext->psShader->sInfo, &psCBuf);

			if (psCBuf && !psCBuf->blob)
			{
				GetShaderVarFromOffset(psOperand->aui32ArraySizes[1], psOperand->aui32Swizzle, psCBuf, &psVarType, indices, &rebase);

				return psVarType->Type;
			}
			else
			{
				// Todo: this isn't correct yet.
				return SVT_FLOAT;
			}

			break;
		}
	case OPERAND_TYPE_IMMEDIATE32:
		{
			return ePreferredTypeForImmediates;
		}

	case OPERAND_TYPE_IMMEDIATE64:
		{
			return SVT_DOUBLE;
		}

	case OPERAND_TYPE_INPUT_THREAD_ID:
	case OPERAND_TYPE_INPUT_THREAD_GROUP_ID:
	case OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP:
	case OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
		{
			return SVT_UINT;
		}
	case OPERAND_TYPE_SPECIAL_ADDRESS:
	case OPERAND_TYPE_SPECIAL_LOOPCOUNTER:
		{
			return SVT_INT;
		}
	case OPERAND_TYPE_INPUT_GS_INSTANCE_ID:
		{
			return SVT_UINT;
		}
	case OPERAND_TYPE_OUTPUT_COVERAGE_MASK:
		{
			return SVT_INT;
	}
	case OPERAND_TYPE_INPUT_PRIMITIVEID:
	case OPERAND_TYPE_INPUT_FORK_INSTANCE_ID:
	case OPERAND_TYPE_OUTPUT_CONTROL_POINT_ID:
		{
			return SVT_INT;
		}
	default:
		{
			return SVT_FLOAT;
		}
	}

	return SVT_FLOAT;
}

void TranslateOperand(HLSLCrossCompilerContext* psContext, const Operand* psOperand, uint32_t ui32TOFlag)
{
	TranslateOperandWithMask(psContext, psOperand, ui32TOFlag, OPERAND_4_COMPONENT_MASK_ALL);
}

void TranslateOperandWithMask(HLSLCrossCompilerContext* psContext, const Operand* psOperand, uint32_t ui32TOFlag, uint32_t ui32ComponentMask)
{
	bstring glsl = *psContext->currentGLSLString;
	uint32_t ui32IgnoreSwizzle = 0;
	SHADER_VARIABLE_TYPE eType = GetOperandDataTypeEx(psContext, psOperand, TypeFlagsToSVTType(ui32TOFlag));

	if(psContext->psShader->ui32MajorVersion <=3)
	{
		ui32TOFlag &= ~(TO_AUTO_BITCAST_TO_FLOAT|TO_AUTO_BITCAST_TO_INT|TO_AUTO_BITCAST_TO_UINT|TO_AUTO_BITCAST_TO_BOOL);
	}

	if(ui32TOFlag & TO_FLAG_NAME_ONLY)
	{
		TranslateVariableName(psContext, psOperand, ui32TOFlag, &ui32IgnoreSwizzle);
		return;
	}

	switch(psOperand->eModifier)
	{
	case OPERAND_MODIFIER_NONE:
		{
			break;
		}
	case OPERAND_MODIFIER_NEG:
		{
			bcatcstr(glsl, "(-");
			break;
		}
	case OPERAND_MODIFIER_ABS:
		{
			bcatcstr(glsl, "abs(");
			break;
		}
	case OPERAND_MODIFIER_ABSNEG:
		{
			bcatcstr(glsl, "-abs(");
			break;
		}
	}

	TranslateVariableNameWithMask(psContext, psOperand, ui32TOFlag, &ui32IgnoreSwizzle, ui32ComponentMask);

	if (psContext->psShader->eShaderType == HULL_SHADER && psOperand->eType == OPERAND_TYPE_OUTPUT &&
		psOperand->ui32RegisterNumber != 0 && (0) && psOperand->eIndexRep[0] != OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE
		&& psContext->currentPhase == HS_CTRL_POINT_PHASE)
	{
		bcatcstr(glsl, "[gl_InvocationID]");
	}

	if(!ui32IgnoreSwizzle)
	{
		TranslateOperandSwizzleWithMask(psContext, psOperand, ui32ComponentMask);
	}

	switch(psOperand->eModifier)
	{
	case OPERAND_MODIFIER_NONE:
		{
			break;
		}
	case OPERAND_MODIFIER_NEG:
		{
			bcatcstr(glsl, ")");
			break;
		}
	case OPERAND_MODIFIER_ABS:
		{
			bcatcstr(glsl, ")");
			break;
		}
	case OPERAND_MODIFIER_ABSNEG:
		{
			bcatcstr(glsl, ")");
			break;
		}
	}
}

char ShaderTypePrefix(const Shader* psShader)
{
	switch (psShader->eShaderType)
	{
	default:
		ASSERT(0);
	case PIXEL_SHADER:    return 'p';
	case VERTEX_SHADER:   return 'v';
	case GEOMETRY_SHADER: return 'g';
	case HULL_SHADER:     return 'h';
	case DOMAIN_SHADER:   return 'd';
	case COMPUTE_SHADER:  return 'c';
	}
}

char ResourceGroupPrefix(ResourceGroup eResGroup)
{
	switch (eResGroup)
	{
	default:
		ASSERT(0);
	case RGROUP_CBUFFER: return 'c';
	case RGROUP_TEXTURE: return 't';
	case RGROUP_SAMPLER: return 's';
	case RGROUP_UAV:     return 'u';
	case RGROUP_BUFFER:  return 'b';
	}
}

const char* ResourceGroupName(ResourceGroup eResGroup)
{
	switch (eResGroup)
	{
	default:
		ASSERT(0);
	case RGROUP_CBUFFER: return "ConstantBuffer";
	case RGROUP_TEXTURE: return "Texture";
	case RGROUP_SAMPLER: return "Sampler";
	case RGROUP_UAV:     return "Unordered";
	case RGROUP_BUFFER:  return "Buffer";
	}
}

void ResourceName(bstring output, const Shader* psShader, const char* szName, ResourceGroup eGroup, const char* szSecondaryName, ResourceGroup eSecondaryGroup, /*uint32_t ui32ArrayOffset,*/ const uint32_t ui32Register, const uint32_t ui32SecondaryRegister, const char* szModifier)
{
	if ((psShader->flags & HLSLCC_FLAG_INOUT_SEMANTIC_NAMES))
	{
		bformata(output, "%c%d%s", ResourceGroupPrefix(eGroup), ui32Register, szName);
	}
	else
	{
	//	bconchar(output, ShaderTypePrefix(psShader));
		bformata(output, "dcl_%s%d", ResourceGroupName(eGroup), ui32Register);
	}

	if ((psShader->flags & HLSLCC_FLAG_INOUT_APPEND_SEMANTIC_NAMES))
	{
		bformata(output, "_%s", szName);
	}

//	if (ui32ArrayOffset)
//		bformata(output, "%d", ui32ArrayOffset);

	if (szSecondaryName != NULL)
	{
		if ((psShader->flags & HLSLCC_FLAG_INOUT_SEMANTIC_NAMES))
		{
			bformata(output, "_%s%c%d%s", szModifier, ResourceGroupPrefix(RGROUP_SAMPLER), ui32SecondaryRegister, szSecondaryName);
		}
		else
		{
			//	bconchar(output, ShaderTypePrefix(psShader));
			bformata(output, "_%sState%d", szModifier, ui32SecondaryRegister);
		}

		if ((psShader->flags & HLSLCC_FLAG_INOUT_APPEND_SEMANTIC_NAMES))
		{
			bformata(output, "_%s", szSecondaryName);
		}
	}
}

void TextureName(bstring output, const Shader* psShader, const uint32_t ui32TextureRegister, const uint32_t ui32SamplerRegister, const int bCompare)
{
	ResourceBinding* psTextureBinding = 0;
	ResourceBinding* psSamplerBinding = 0;
	int found;
	uint32_t ui32ArrayOffset = 0;
	const char* szModifier = bCompare ? "c" : "";

	uint32_t ui32SamplerRegisterNew = ui32SamplerRegister;
	if (ui32SamplerRegisterNew == ~0u)
	{
		uint32_t ui32SearchRegister;
		for (ui32SearchRegister = 0; ui32SearchRegister < psShader->sInfo.ui32NumSamplers; ++ui32SearchRegister)
			if (psShader->sInfo.asSamplers[ui32SearchRegister].sMask.ui10TextureBindPoint == ui32TextureRegister)
				if (psShader->sInfo.asSamplers[ui32SearchRegister].sMask.ui10SamplerBindPoint < MAX_RESOURCE_BINDINGS)
				{
					ui32SamplerRegisterNew = psShader->sInfo.asSamplers[ui32SearchRegister].sMask.ui10SamplerBindPoint;
					break;
				}
	}

	found = GetResourceFromBindingPoint(RGROUP_TEXTURE, ui32TextureRegister, &psShader->sInfo, &psTextureBinding);
	if (ui32SamplerRegisterNew < MAX_RESOURCE_BINDINGS) 
		found &= GetResourceFromBindingPoint(RGROUP_SAMPLER, ui32SamplerRegisterNew, &psShader->sInfo, &psSamplerBinding);

	ResourceGroup group = RGROUP_TEXTURE;
	if (psTextureBinding->eDimension == REFLECT_RESOURCE_DIMENSION_BUFFER)
		group = RGROUP_BUFFER;

	if (found)
		ResourceName(output, psShader, psTextureBinding->Name, group, psSamplerBinding ? psSamplerBinding->Name : NULL, RGROUP_SAMPLER, ui32TextureRegister/* - psTextureBinding->ui32BindPoint*/, ui32SamplerRegisterNew, szModifier);
	else if (ui32SamplerRegisterNew < MAX_RESOURCE_BINDINGS)
		bformata(output, "UnknownTexture%s_%d_%d", szModifier, ui32TextureRegister, ui32SamplerRegisterNew);
	else
		bformata(output, "UnknownTexture%s_%d", szModifier, ui32TextureRegister);
}

void UAVName(bstring output, const Shader* psShader, const uint32_t ui32RegisterNumber)
{
	ResourceBinding* psBinding = 0;
	int found;

	found = GetResourceFromBindingPoint(RGROUP_UAV, ui32RegisterNumber, &psShader->sInfo, &psBinding);

	if (found)
		ResourceName(output, psShader, psBinding->Name, RGROUP_UAV, NULL, RGROUP_COUNT, ui32RegisterNumber/* - psBinding->ui32BindPoint*/, MAX_RESOURCE_BINDINGS, "");
	else
		bformata(output, "UnknownUAV%d", ui32RegisterNumber);
}

ResourceType UAVType(bstring output, const Shader* psShader, const uint32_t ui32RegisterNumber)
{
	ResourceBinding* psBinding = 0;
	int found;

	found = GetResourceFromBindingPoint(RGROUP_UAV, ui32RegisterNumber, &psShader->sInfo, &psBinding);

	if (found)
		return psBinding->eType;
	else
		return RTYPE_COUNT;
}

void UniformBufferName(bstring output, const Shader* psShader, const uint32_t ui32RegisterNumber)
{
	ResourceBinding* psBinding = 0;
	int found;

	found = GetResourceFromBindingPoint(RGROUP_CBUFFER, ui32RegisterNumber, &psShader->sInfo, &psBinding);

	if (found)
		ResourceName(output, psShader, psBinding->Name, RGROUP_CBUFFER, NULL, RGROUP_COUNT, ui32RegisterNumber /*- psBinding->ui32BindPoint*/, MAX_RESOURCE_BINDINGS, "");
	else
		bformata(output, "UnknownUniformBuffer%d", ui32RegisterNumber);
}

void ShaderVarName(bstring output, const Shader* psShader, const char* OriginalName)
{
	bconchar(output, ShaderTypePrefix(psShader));
	bcatcstr(output, OriginalName);
}

void ShaderVarFullName(bstring output, const Shader* psContext, const ShaderVarType* psShaderVar)
{
	if (psShaderVar->Parent != NULL)
	{
		ShaderVarFullName(output, psContext, psShaderVar->Parent);
		bconchar(output, '.');
	}
	ShaderVarName(output, psContext, psShaderVar->Name);
}

void ConvertToTextureName(bstring output, const Shader* psShader, const char* szName, const char* szSamplerName, const uint32_t ui32RegisterNumber, const int bCompare)
{
	ResourceName(output, psShader, szName, RGROUP_TEXTURE, szSamplerName, RGROUP_SAMPLER, ui32RegisterNumber, MAX_RESOURCE_BINDINGS, bCompare ? "c" : "");
}

void ConvertToUAVName(bstring output, const Shader* psShader, const char* szOriginalUAVName, const uint32_t ui32RegisterNumber)
{
	ResourceName(output, psShader, szOriginalUAVName, RGROUP_UAV, NULL, RGROUP_COUNT, ui32RegisterNumber, MAX_RESOURCE_BINDINGS, "");
}

void ConvertToUniformBufferName(bstring output, const Shader* psShader, const char* szConstantBufferName, const uint32_t ui32RegisterNumber)
{
	ResourceName(output, psShader, szConstantBufferName, RGROUP_CBUFFER, NULL, RGROUP_COUNT, ui32RegisterNumber, MAX_RESOURCE_BINDINGS, "");
}