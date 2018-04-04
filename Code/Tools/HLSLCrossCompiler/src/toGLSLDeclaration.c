#include "hlslcc.h"
#include "internal_includes/toGLSLDeclaration.h"
#include "internal_includes/toGLSLOperand.h"
#include "internal_includes/languages.h"
#include "bstrlib.h"
#include "internal_includes/debug.h"
#include "internal_includes/hlslcc_malloc.h"
#include <math.h>
#include <float.h>

#if defined(_MSC_VER) && _MSC_VER < 1900
#define isnan(x) _isnan(x)
#define isinf(x) (!_finite(x))
#endif

#define fpcheck(x) (isnan(x) || isinf(x))

extern const char* cComponentNames[];

typedef enum {
	GLVARTYPE_FLOAT,
	GLVARTYPE_INT,
	GLVARTYPE_FLOAT4,
} GLVARTYPE;

extern void AddIndentation(HLSLCrossCompilerContext* psContext);
extern void AddAssignToDest(HLSLCrossCompilerContext* psContext, const Operand* psDest, SHADER_VARIABLE_TYPE eSrcType, uint32_t ui32SrcElementCount, int* pNeedsParenthesis);
extern void AddAssignPrologue(HLSLCrossCompilerContext *psContext, int numParenthesis);
extern uint32_t AddImport(HLSLCrossCompilerContext* psContext, SYMBOL_TYPE eType, uint32_t ui32ID, uint32_t ui32Default);
extern uint32_t AddExport(HLSLCrossCompilerContext* psContext, SYMBOL_TYPE eType, uint32_t ui32ID, uint32_t ui32Value);
const char* GetMangleSuffix(const SHADER_TYPE eShaderType);

const char* GetPrecisionAttributeFromOperand(OPERAND_MIN_PRECISION eMinPrecision)
{
	switch (eMinPrecision)
	{
	case OPERAND_MIN_PRECISION_DEFAULT:
		{
			return "highp ";
			break;
		}
	case OPERAND_MIN_PRECISION_ANY_16:
	case OPERAND_MIN_PRECISION_FLOAT_16:
	case OPERAND_MIN_PRECISION_SINT_16:
	case OPERAND_MIN_PRECISION_UINT_16:
		{
			return "mediump ";
			break;
		}
	case OPERAND_MIN_PRECISION_ANY_10:
	case OPERAND_MIN_PRECISION_FLOAT_2_8:
		{
			return "lowp ";
			break;
		}
	}

	return "";
}

const char* GetPrecisionAttributeFromType(SHADER_VARIABLE_TYPE eType)
{
	switch (eType)
	{
//	case SVT_FLOATUNORM:
//	case SVT_FLOATSNORM:

	case SVT_FLOAT:
	case SVT_UINT:
	case SVT_INT:
		{
			return "highp ";
			break;
		}
	case SVT_FLOAT16:
	case SVT_UINT16:
	case SVT_INT16:
		{
			return "mediump ";
			break;
		}
	case SVT_FLOAT10:
	case SVT_FLOAT8:
	case SVT_INT12:
		{
			return "lowp ";
			break;
		}
	}

	return "";
}

const char* GetTypeString(GLVARTYPE eType)
{
	switch(eType)
	{
	case GLVARTYPE_FLOAT:
		{
			return "float";
		}
	case GLVARTYPE_INT:
		{
			return "int";
		}
	case GLVARTYPE_FLOAT4:
		{
			return "vec4";
		}
	default:
		{
			return "";
		}
	}
}

const uint32_t GetTypeElementCount(GLVARTYPE eType)
{
	switch(eType)
	{
	case GLVARTYPE_FLOAT:
	case GLVARTYPE_INT:
		{
			return 1;
		}
	case GLVARTYPE_FLOAT4:
		{
			return 4;
		}
	default:
		{
			return 0;
		}
	}
}

#define ALIGNMENT_STANDARD	0
#define ALIGNMENT_LAYOUT	1
#define ALIGNMENT_DIVISIBLE	2

static const uint32_t customUBO = ALIGNMENT_LAYOUT;
static const uint32_t packedUBO = 0;
static const uint32_t sharedUBO = 0;

static const uint32_t customStruct = ALIGNMENT_DIVISIBLE;
static const uint32_t packedStruct = 0;
static const uint32_t sharedStruct = 0;

void GetSTDLayout(ShaderVarType* pType, uint32_t* puAlignment, uint32_t* puSize, uint32_t alignment, uint32_t version)
{
	uint32_t puStride = 0;
	*puSize           = 0;
	*puAlignment      = 1;

	// 1.  If the member is a scalar consuming N basic machine units, the base alignment is N
	switch (pType->Type)
	{
	case SVT_UINT16:
	case SVT_INT12:
	case SVT_INT16:
	case SVT_FLOAT16:
	case SVT_FLOAT10:
		puStride     = 2;
		*puSize      = 2;
		*puAlignment = 2;
		break;
	case SVT_BOOL:
	case SVT_UINT:
	case SVT_INT:
	case SVT_FLOAT:
		puStride     = 4;
		*puSize      = 4;
		*puAlignment = 4;
		break;
	case SVT_DOUBLE:
		puStride     = 8;
		*puSize      = 8;
		*puAlignment = 4;
		break;
	case SVT_VOID:
		break;
	default:
		ASSERT(0);
		break;
	}

	switch (pType->Class)
	{
	case SVC_SCALAR:
		break;
	case SVC_MATRIX_ROWS:
	case SVC_MATRIX_COLUMNS:
		// 5.  If the member is a column - major matrix with C columns and R rows, the
		//     matrix is stored identically to an array of C column vectors with R
		//     components each, according to rule(4).
		// 6.  If the member is an array of S column - major matrices with C columns and
		//     R rows, the matrix is stored identically to a row of S × C column vectors
		//     with R components each, according to rule(4).
		// 7.  If the member is a row - major matrix with C columns and R rows, the matrix
		//     is stored identically to an array of R row vectors with C components each,
		//     according to rule(4).
		// 8.  If the member is an array of S row - major matrices with C columns and
		//     R rows, the matrix is stored identically to a row of S × R row vectors with
		//     C components each, according to rule(4).

		// Matrices are translated to arrays of vectors
		*puSize *= pType->Rows;
	case SVC_VECTOR:
		// type is made up from floats and has no alignment
		if (alignment != ALIGNMENT_STANDARD)
		{
			puStride     *= pType->Columns;
			*puSize      *= pType->Columns;
			*puAlignment *= (alignment == ALIGNMENT_DIVISIBLE ? 1 : pType->Columns);
			break;
		}

		// 2.  If the member is a two - or four - component vector with components consuming N
		//     basic machine units, the base alignment is 2N or 4N, respectively.
		// 3.  If the member is a three - component vector with components consuming N
		//     basic machine units, the base alignment is 4N
		switch (pType->Columns)
		{
		case 2:
			puStride     *= 2;
			*puSize      *= 2;
			*puAlignment *= 2;
			break;
		case 3:
			puStride     *= 3;
			*puSize      *= 3;
			*puAlignment *= 4;
			break;
		case 4:
			puStride     *= 4;
			*puSize      *= 4;
			*puAlignment *= 4;
			break;
		}
		break;
	case SVC_STRUCT:
		{
			// 9.  If the member is a structure, the base alignment of the structure is N, where N
			//     is the largest base alignment value of any of its members, and rounded
			//     up to the base alignment of a vec4. The individual members of this sub -
			//     structure are then assigned offsets by applying this set of rules recursively,
			//     where the base offset of the first member of the sub - structure is equal to the
			//     aligned offset of the structure.The structure may have padding at the end;
			//     the base offset of the member following the sub - structure is rounded up to
			//     the next multiple of the base alignment of the structure.
			// 10. If the member is an array of S structures, the S elements of the array are laid
			//     out in order, according to rule(9).
			uint32_t uMember;
			for (uMember = 0; uMember < pType->MemberCount; ++uMember)
			{
				uint32_t uMemberAlignment, uMemberSize;

				GetSTDLayout(pType->Members + uMember, &uMemberAlignment, &uMemberSize, alignment ? customStruct : ALIGNMENT_STANDARD, version);

				puStride    += uMemberSize;
				*puSize     += uMemberSize;
				*puAlignment = *puAlignment > uMemberAlignment ? *puAlignment : uMemberAlignment;
			}
		}
		break;
	default:
		ASSERT(0);
		break;
	}

	// When using the std430 storage layout, shader storage blocks will be laid out in buffer storage
	// identically to uniform and shader storage blocks using the std140 layout, except
	// that the base alignment and stride of arrays of scalars and vectors in rule 4 and of
	// structures in rule 9 are not rounded up a multiple of the base alignment of a vec4
	if (version != 430)
	{
		// 4.  If the member is an array of scalars or vectors, the base alignment and array
		//     stride are set to match the base alignment of a single array element, according
		//     to rules(1), (2), and (3), and rounded up to the base alignment of a vec4.The
		//     array may have padding at the end; the base offset of the member following
		//     the array is rounded up to the next multiple of the base alignment.
		if (pType->Elements > 1 || pType->Class == SVC_MATRIX_ROWS || pType->Class == SVC_MATRIX_COLUMNS || pType->Class == SVC_STRUCT)
			puStride     = (puStride     + 0x0000000F) & 0xFFFFFFF0,
			*puAlignment = (*puAlignment + 0x0000000F) & 0xFFFFFFF0,
			*puSize      = (*puSize      + 0x0000000F) & 0xFFFFFFF0;
	}

	if (pType->Elements > 1)
		*puSize = puStride * pType->Elements;
}

void AddToDx9ImmConstIndexableArray(HLSLCrossCompilerContext* psContext, const Operand* psOperand)
{
	bstring* savedStringPtr = psContext->currentGLSLString;

	psContext->currentGLSLString = &psContext->earlyMain;
	psContext->indent++;
	AddIndentation(psContext);
	psContext->psShader->aui32Dx9ImmConstArrayRemap[psOperand->ui32RegisterNumber] = psContext->psShader->ui32NumDx9ImmConst;
	bformata(psContext->earlyMain, "ImmConstArray[%d] = ", psContext->psShader->ui32NumDx9ImmConst);
	TranslateOperand(psContext, psOperand, TO_FLAG_NONE);
	bcatcstr(psContext->earlyMain, ";\n");
	psContext->indent--;
	psContext->psShader->ui32NumDx9ImmConst++;

	psContext->currentGLSLString = savedStringPtr;
}

static void DeclareConstBufferShaderVariable(HLSLCrossCompilerContext* psContext, const char* RName, const char* Name, ShaderVarType* psType, uint32_t alignment, uint32_t offsetVar, int unsizedArray)
{
	bstring glsl = *psContext->currentGLSLString;
	const char* varPrecison = HavePrecisionQualifers(psContext->psShader->eTargetLanguage, psContext->flags) ? GetPrecisionAttributeFromType(psType->Type) : "";
	const int usePrec = psContext->flags & HLSLCC_FLAG_FORCE_PRECISION_QUALIFIERS;
	uint32_t column;

	bcatcstr(glsl, "\t");
	if (alignment == ALIGNMENT_LAYOUT)
		bformata(glsl, "layout(offset = %4d) ", offsetVar);
	else
		bformata(glsl, "/* hlsl ofs = %4d */ ", offsetVar);

	if (psType->Class == SVC_STRUCT)
	{
		ShaderVarName(glsl, psContext->psShader, Name);
		bcatcstr(glsl, "_Type ");
		ShaderVarName(glsl, psContext->psShader, Name);
		if (psType->Elements > 1)
			bformata(glsl, "[%d]", psType->Elements);
	}
	else if (psType->Class == SVC_MATRIX_COLUMNS || psType->Class == SVC_MATRIX_ROWS)
	{
		bformata(glsl, "%s ", GetConstructorForType(psType->Type, psType->Columns, usePrec));
		ShaderVarName(glsl, psContext->psShader, Name);

		bformata(glsl, "[%d", psType->Rows);
		if (psType->Elements > 1)
			bformata(glsl, " * %d", psType->Elements);
		bformata(glsl, "]");
	}
	else if (psType->Class == SVC_VECTOR && (alignment != ALIGNMENT_DIVISIBLE || unsizedArray))
	{
		bformata(glsl, "%s ", GetConstructorForType(psType->Type, psType->Columns, usePrec));
		ShaderVarName(glsl, psContext->psShader, Name);

		if (psType->Elements > 1)
			bformata(glsl, "[%d]", psType->Elements);
	}
	else if (psType->Class == SVC_VECTOR && (alignment == ALIGNMENT_DIVISIBLE && !unsizedArray))
	{
		char* memberName;
		char* memberGroup;
		bstring memberNameB = bfromcstralloc(MAX_SHADER_VARS, "");
		bstring memberGroupB = bfromcstralloc(MAX_SHADER_VARS, "");
		btrunc(memberNameB, 0);
		btrunc(memberGroupB, 0);

		ShaderVarName(memberNameB, psContext->psShader, Name);
		for (column = 0; column < psType->Columns; ++column)
		{
			ShaderVarName(memberGroupB, psContext->psShader, Name);
			bformata(memberGroupB, "%s%s", cComponentNames[column], column == (psType->Columns - 1) ? "" : ", ");
		}

		memberName = bstr2cstr(memberNameB, '\0');
		memberGroup = bstr2cstr(memberGroupB, '\0');

		bdestroy(memberNameB);
		bdestroy(memberGroupB);

		bformata(glsl, "%s %s", GetConstructorForType(psType->Type, 1, usePrec), memberGroup);
		bformata(glsl, ";\n");
//		bformata(glsl, "#define %s %s(%s)\n", memberName, GetConstructorForType(psType->Type, psType->Columns, usePrec), memberGroup);

		if (psType->Elements > 1)
			bformata(glsl, "#error Member %s.%s is an array and can't be made divisible automagically\n", RName, memberName);

		bcstrfree(memberName);
		bcstrfree(memberGroup);

		// mark as unaligned, needs manual resolve when accessing the variable
		psType->Unaligned = 1;
		return;
	}
	else if (psType->Class == SVC_SCALAR)
	{
#if 0
			//Use int instead of bool.
			//Allows implicit conversions to integer and
			//bool consumes 4-bytes in HLSL and GLSL anyway.
			bformata(glsl, "int ");
			// Also change the definition in the type tree.
			((ShaderVarType *)psType)->Type = SVT_INT;
#endif

		bformata(glsl, "%s ", GetConstructorForType(psType->Type, 1, usePrec));
		ShaderVarName(glsl, psContext->psShader, Name);

		if (psType->Elements > 1)
			bformata(glsl, "[%d]", psType->Elements);
	}

	if (unsizedArray)
		bformata(glsl, "[]");
	bformata(glsl, ";\n");
}

//In GLSL embedded structure definitions are not supported.
static void PreDeclareStructType(HLSLCrossCompilerContext* psContext, const char* RName, const char* Name, ShaderVarType* psType, uint32_t alignment, uint32_t version, int unsizedArray)
{
	bstring glsl = *psContext->currentGLSLString;
	uint32_t i, implicitOffset, alignedOffset;
	uint32_t
		*uStructAlignment = &psType->Members[psType->MemberCount].ParentCount,
		*uStructSize = &psType->Members[psType->MemberCount].Offset;

	for (i = 0; i < psType->MemberCount; ++i)
	{
		ShaderVarType* psVar = &psType->Members[i];

		if (psVar->Class == SVC_STRUCT)
		{
			PreDeclareStructType(psContext, RName, psVar->Name, psVar, customStruct, version, 0);
		}
	}

	if (psType->Class == SVC_STRUCT)
	{
		uint32_t unnamed_struct = strcmp(Name, "$Element") == 0 ? 1 : 0;
		uint32_t compliantStruct = 0;

		//Not supported at the moment
		ASSERT(!unnamed_struct);

		bcatcstr(glsl, "struct ");
		ShaderVarName(glsl, psContext->psShader, Name);
		bcatcstr(glsl, "_Type {\n");

		// Stuff the aligned struct's size into the the last memberElement
		GetSTDLayout(psType, uStructAlignment, uStructSize, ALIGNMENT_STANDARD, version);

		alignedOffset = *uStructSize;

		GetSTDLayout(psType, uStructAlignment, uStructSize, customStruct, version);

		compliantStruct =
			(alignedOffset == *uStructSize) &&
			(!(unsizedArray)); // Work around GLSL spec bug, vec3[] will be read as vec4[] even under 430, this also infects structures

		implicitOffset = 0;
		for (i = 0; i < psType->MemberCount; ++i)
		{
			ShaderVarType* psVar = &psType->Members[i];
			uint32_t compliant = 0;

			ASSERT(psType->Members != 0);

			if (!packedStruct && !sharedStruct)
			{
				if (customStruct != ALIGNMENT_LAYOUT)
				{
					uint32_t uVarAlignment, uVarSize;
					GetSTDLayout(psVar, &uVarAlignment, &uVarSize, ALIGNMENT_STANDARD, version);

					alignedOffset  = implicitOffset;
					alignedOffset += uVarAlignment - 1;
					alignedOffset -= alignedOffset % uVarAlignment;

					compliant =
						(alignedOffset + 0        == (psVar + 0)->Offset) &&
						(alignedOffset + uVarSize == (psVar + 1)->Offset) &&
						(compliantStruct || uVarAlignment <= *uStructAlignment) &&
						(!(psVar->Class == SVC_VECTOR && psVar->Columns == 3 && psVar->Elements > 0));

					GetSTDLayout(psVar, &uVarAlignment, &uVarSize, customStruct, version);

					// fill in padding variable to produce offsets, instead of the "layout(offset=...)" way
					// works only if DirectX's alignment is always larger than OpenGL's alignment
					if ((implicitOffset + 4 - 1) / 4 < psVar->Offset / 4)
					{
						// if we'd use arrays we'd fall into the std140 alignment trap again, thus we use individual floats
						int32_t  nNumPaddingFloats = psVar->Offset / 4 - (implicitOffset + 4 - 1) / 4;
						while (--nNumPaddingFloats >= 0)
							bformata(glsl, "\tfloat padding_%s_%d_%d;\n", RName, psVar->Offset, nNumPaddingFloats);

						implicitOffset = psVar->Offset - psVar->Offset % 4;
					}

					implicitOffset += uVarAlignment - 1;
					implicitOffset -= implicitOffset % uVarAlignment;

					if (implicitOffset != psVar->Offset)
						bformata(glsl, "#error Position of member %s.%s is %d in HLSL, but should be %d\n", RName, psVar->Name, psVar->Offset, implicitOffset);

					implicitOffset += uVarSize;
				}
			}

			DeclareConstBufferShaderVariable(psContext, RName, psVar->Name, psVar, compliant ? ALIGNMENT_STANDARD : customStruct, psVar->Offset, 0);
		}

		bformata(glsl, "};\n");
	}
}

char* GetDeclaredInputName(const HLSLCrossCompilerContext* psContext, const SHADER_TYPE eShaderType, const Operand* psOperand)
{
	bstring inputName;
	char* cstr;
	InOutSignature* psIn;
	int found = GetInputSignatureFromRegister(
		psOperand->ui32RegisterNumber,
		&psContext->psShader->sInfo,
		&psIn);

	if ((psContext->flags & HLSLCC_FLAG_INOUT_SEMANTIC_NAMES) && found)
	{
		inputName = bformat("i%d%s%d", psOperand->ui32RegisterNumber, psIn->SemanticName, psIn->ui32SemanticIndex);
	}
	else if (eShaderType == GEOMETRY_SHADER)
	{
		inputName = bformat("VtxOutput%d", psOperand->ui32RegisterNumber);
	}
	else if (eShaderType == HULL_SHADER)
	{
		inputName = bformat("VtxGeoOutput%d", psOperand->ui32RegisterNumber);
	}
	else if (eShaderType == DOMAIN_SHADER)
	{
		inputName = bformat("HullOutput%d", psOperand->ui32RegisterNumber);
	}
	else if (eShaderType == PIXEL_SHADER)
	{
		if (psContext->flags & HLSLCC_FLAG_TESS_ENABLED)
		{
			inputName = bformat("DomOutput%d", psOperand->ui32RegisterNumber);
		}
		else
		{
			inputName = bformat("VtxGeoOutput%d", psOperand->ui32RegisterNumber);
		}
	}
	else
	{
		ASSERT(eShaderType == VERTEX_SHADER);
		inputName = bformat("dcl_Input%d", psOperand->ui32RegisterNumber);
	}

	if ((psContext->flags & HLSLCC_FLAG_INOUT_APPEND_SEMANTIC_NAMES) && found)
	{
		bformata(inputName, "_%s%d", psIn->SemanticName, psIn->ui32SemanticIndex);
	}

	cstr = bstr2cstr(inputName, '\0');
	bdestroy(inputName);
	return cstr;
}

char* GetDeclaredPatchConstantName(const HLSLCrossCompilerContext* psContext, const SHADER_TYPE eShaderType, const Operand* psOperand)
{
	bstring inputName;
	char* cstr;
	InOutSignature* psIn;
	int found = GetPatchConstantSignatureFromRegister(
		psOperand->ui32RegisterNumber,
		psOperand->ui32CompMask,
		&psContext->psShader->sInfo,
		&psIn);

	if ((psContext->flags & HLSLCC_FLAG_INOUT_SEMANTIC_NAMES) && found)
	{
		inputName = bformat("p%d%s%d", psOperand->ui32RegisterNumber, psIn->SemanticName, psIn->ui32SemanticIndex);
	}
	else if (eShaderType == GEOMETRY_SHADER)
	{
		inputName = bformat("VtxConstant%d", psOperand->ui32RegisterNumber);
	}
	else if (eShaderType == HULL_SHADER)
	{
		inputName = bformat("VtxGeoConstant%d", psOperand->ui32RegisterNumber);
	}
	else if (eShaderType == DOMAIN_SHADER)
	{
		inputName = bformat("HullConstant%d", psOperand->ui32RegisterNumber);
	}
	else if (eShaderType == PIXEL_SHADER)
	{
		if (psContext->flags & HLSLCC_FLAG_TESS_ENABLED)
		{
			inputName = bformat("DomConstant%d", psOperand->ui32RegisterNumber);
		}
		else
		{
			inputName = bformat("VtxGeoConstant%d", psOperand->ui32RegisterNumber);
		}
	}
	else
	{
		ASSERT(eShaderType == VERTEX_SHADER);
		inputName = bformat("dcl_Constant%d", psOperand->ui32RegisterNumber);
	}

	if ((psContext->flags & HLSLCC_FLAG_INOUT_APPEND_SEMANTIC_NAMES) && found)
	{
		bformata(inputName, "_%s%d", psIn->SemanticName, psIn->ui32SemanticIndex);
	}

	cstr = bstr2cstr(inputName, '\0');
	bdestroy(inputName);
	return cstr;
}

char* GetDeclaredOutputName(const HLSLCrossCompilerContext* psContext, const SHADER_TYPE eShaderType, const Operand* psOperand, int* piStream)
{
	bstring outputName;
	char* cstr;
	InOutSignature* psOut;

	int foundOutput = GetOutputSignatureFromRegister(
		psContext->currentPhase,
		psOperand->ui32RegisterNumber,
		psOperand->ui32CompMask,
		psContext->psShader->ui32CurrentVertexOutputStream,
		&psContext->psShader->sInfo,
		&psOut);

	ASSERT(foundOutput);

	if (psContext->flags & HLSLCC_FLAG_INOUT_SEMANTIC_NAMES)
	{
		outputName = bformat("o%d%s%d", psOperand->ui32RegisterNumber, psOut->SemanticName, psOut->ui32SemanticIndex);
	}
	else if (eShaderType == GEOMETRY_SHADER)
	{
		if (psOut->ui32Stream != 0)
		{
			outputName = bformat("VtxGeoOutput%d_S%d", psOperand->ui32RegisterNumber, psOut->ui32Stream);
			piStream[0] = psOut->ui32Stream;
		}
		else
		{
			outputName = bformat("VtxGeoOutput%d", psOperand->ui32RegisterNumber);
		}

	}
	else if (eShaderType == DOMAIN_SHADER)
	{
		outputName = bformat("DomOutput%d", psOperand->ui32RegisterNumber);
	}
	else if (eShaderType == VERTEX_SHADER)
	{
		if (psContext->flags & HLSLCC_FLAG_GS_ENABLED)
		{
			outputName = bformat("VtxOutput%d", psOperand->ui32RegisterNumber);
		}
		else
		{
			outputName = bformat("VtxGeoOutput%d", psOperand->ui32RegisterNumber);
		}
	}
	else if (eShaderType == PIXEL_SHADER)
	{
		outputName = bformat("PixOutput%d", psOperand->ui32RegisterNumber);
	}
	else
	{
		ASSERT(eShaderType == HULL_SHADER);
		outputName = bformat("HullOutput%d", psOperand->ui32RegisterNumber);
	}

	if (psContext->flags & HLSLCC_FLAG_INOUT_APPEND_SEMANTIC_NAMES)
	{
		bformata(outputName, "_%s%d", psOut->SemanticName, psOut->ui32SemanticIndex);
	}

	cstr = bstr2cstr(outputName, '\0');
	bdestroy(outputName);
	return cstr;
}

static const char* GetInterpolationString(INTERPOLATION_MODE eMode, GLLang lang)
{
	switch(eMode)
	{
	case INTERPOLATION_CONSTANT:
		{
			return "flat ";
		}
	case INTERPOLATION_LINEAR:
		{
			return "";
		}
	case INTERPOLATION_LINEAR_CENTROID:
		{
			return "centroid ";
		}
	case INTERPOLATION_LINEAR_NOPERSPECTIVE:
		{
			return lang <= LANG_ES_310 ? "" : "noperspective ";
			break;
		}
	case INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID:
		{
			return  lang <= LANG_ES_310 ? "centroid " : "noperspective centroid ";
		}
	case INTERPOLATION_LINEAR_SAMPLE:
		{
			return "sample ";
		}
	case INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE:
		{
			return  lang <= LANG_ES_310 ? "" : "noperspective sample ";
		}
	default:
		{
			return "";
		}
	}
}

static void DeclareInput(
	HLSLCrossCompilerContext* psContext,
	const Declaration* psDecl,
	const char* Interpolation, const char* StorageQualifier, const char* Precision, int iNumComponents, OPERAND_INDEX_DIMENSION eIndexDim, const char* InputName, const uint32_t ui32CompMask)
{
	Shader* psShader = psContext->psShader;
	bstring glsl = *psContext->currentGLSLString;
	uint32_t ui32Reg = psDecl->asOperands[0].ui32RegisterNumber;

	// This falls within the specified index ranges. The default is 0 if no input range is specified
	if(psShader->aIndexedInput[ui32Reg] == -1)
		return;

//	if ((ui32CompMask & ~psShader->aiInputDeclaredSize[ui32Reg]) != 0)
	if (                 psShader->aiInputDeclaredSize[ui32Reg]  == 0)
	{
		const char* vecType = "vec";
		const char* scalarType = "float";
		InOutSignature* psSignature = NULL;

		if (GetInputSignatureFromRegister(ui32Reg, &psShader->sInfo, &psSignature))
		{
			switch(psSignature->eComponentType)
			{
			case INOUT_COMPONENT_UINT32:
				{
					vecType = "uvec";
					scalarType = "uint";
					break;
				}
			case INOUT_COMPONENT_SINT32:
				{
					vecType = "ivec";
					scalarType = "int";
					break;
				}
			case INOUT_COMPONENT_FLOAT32:
				{
					break;
				}
			}
		}

		if (psContext->psDependencies)
		{
			if (psShader->eShaderType == PIXEL_SHADER)
			{
				psContext->psDependencies->aePixelInputInterpolation[ui32Reg] = psDecl->value.eInterpolation;
			}
		}

		if (HaveInOutLocationQualifier(psContext->psShader->eTargetLanguage, psContext->psShader->extensions, psContext->flags) ||
			HaveLimitedInOutLocationQualifier(psContext->psShader->eTargetLanguage, psShader->eShaderType, 1, psContext->flags))
		{
			bformata(glsl, "layout(location = %d) ", ui32Reg);
		}

		if (iNumComponents == 1)
		{
			psContext->psShader->abScalarInput[ui32Reg] = -1;
		}

		switch(eIndexDim)
		{
		case INDEX_2D:
			{
				if (psShader->eShaderType == HULL_SHADER)
				{
					if (iNumComponents == 1)
					{
						bformata(glsl, "%s %s %s %s[/*gl_MaxPatchVertices*/];\n", StorageQualifier, Precision, scalarType, InputName, psDecl->asOperands[0].aui32ArraySizes[0]);
						bformata(glsl, "%s1 Input%d;\n", vecType, ui32Reg);
					}
					else
					{
						bformata(glsl, "%s %s %s%d %s[/*gl_MaxPatchVertices*/];\n", StorageQualifier, Precision, vecType, iNumComponents, InputName);
						bformata(glsl, "%s%d Input%d[gl_MaxPatchVertices];\n", vecType, iNumComponents, ui32Reg);
					}
				}
				else if (psDecl->asOperands[0].eType == OPERAND_TYPE_INPUT_CONTROL_POINT)
				{
					if (iNumComponents == 1)
					{
						bformata(glsl, "%s %s %s %s[/*%d*/];\n", StorageQualifier, Precision, scalarType, InputName, psDecl->asOperands[0].aui32ArraySizes[0]);
						bformata(glsl, "%s1 Input%d;\n", vecType, ui32Reg);
					}
					else
					{
						bformata(glsl, "%s %s %s%d %s[/*%d*/];\n", StorageQualifier, Precision, vecType, iNumComponents, InputName, psDecl->asOperands[0].aui32ArraySizes[0]);
						bformata(glsl, "%s%d Input%d[%d];\n", vecType, iNumComponents, ui32Reg, psDecl->asOperands[0].aui32ArraySizes[0]);
					}
				}
				else 
				{
					if (iNumComponents == 1)
					{
						bformata(glsl, "%s %s %s %s[%d];\n", StorageQualifier, Precision, scalarType, InputName, psDecl->asOperands[0].aui32ArraySizes[0]);
						bformata(glsl, "%s1 Input%d;\n", vecType, ui32Reg);
					}
					else
					{
						bformata(glsl, "%s %s %s%d %s[%d];\n", StorageQualifier, Precision, vecType, iNumComponents, InputName, psDecl->asOperands[0].aui32ArraySizes[0]);
						bformata(glsl, "%s%d Input%d[%d];\n", vecType, iNumComponents, ui32Reg, psDecl->asOperands[0].aui32ArraySizes[0]);
					}
				}

			//	psShader->aiInputDeclaredSize[ui32Reg] = psSignature->ui32Mask;
				psShader->aiInputDeclaredSize[ui32Reg] = psDecl->asOperands[0].aui32ArraySizes[0];
				break;
			}
		default:
			{

				if(psDecl->asOperands[0].eType == OPERAND_TYPE_SPECIAL_TEXCOORD_INPUT)
				{
					InputName = "TexCoord";
				}

				if (iNumComponents == 1)
				{
					bformata(glsl, "%s %s %s %s %s;\n", Interpolation, StorageQualifier, Precision, scalarType, InputName);
					bformata(glsl, "%s1 Input%d;\n", vecType, ui32Reg);

					psShader->aiInputDeclaredSize[ui32Reg] = -1;
				}
				else if (psShader->aIndexedInput[ui32Reg] > 0)
				{
					bformata(glsl, "%s %s %s %s%d %s[%d];", Interpolation, StorageQualifier, Precision, vecType, iNumComponents, InputName, psShader->aIndexedInput[ui32Reg]);
					bformata(glsl, "%s%d Input%d[%d];\n", vecType, iNumComponents, ui32Reg, psShader->aIndexedInput[ui32Reg]);

					psShader->aiInputDeclaredSize[ui32Reg] = psShader->aIndexedInput[ui32Reg];
				}
				else
				{
					bformata(glsl, "%s %s %s %s%d %s;\n", Interpolation, StorageQualifier, Precision, vecType, iNumComponents, InputName);
					bformata(glsl, "%s%d Input%d;\n", vecType, iNumComponents, ui32Reg);

					psShader->aiInputDeclaredSize[ui32Reg] = -1;
				}
				break;
			}
		}
	}

	if(psShader->abInputReferencedByInstruction[ui32Reg])
	{
		psContext->currentGLSLString = &psContext->earlyMain;
		psContext->indent++;

		if(psShader->aiInputDeclaredSize[ui32Reg] == -1) //Not an array
		{
			AddIndentation(psContext);
			bformata(psContext->earlyMain, "Input%d = %s;\n", ui32Reg, InputName);
		}
		else
		{
			int arrayIndex = psShader->aiInputDeclaredSize[ui32Reg];

			while(arrayIndex)
			{
				AddIndentation(psContext);
				bformata(psContext->earlyMain, "Input%d[%d] = %s[%d];\n", ui32Reg, arrayIndex - 1, InputName, arrayIndex - 1);

				arrayIndex--;
			}
		}
		psContext->indent--;
		psContext->currentGLSLString = &psContext->glsl;
	}
}

static void DeclareInputPatchConstant(
	HLSLCrossCompilerContext* psContext,
	const Declaration* psDecl,
	const char* Interpolation, const char* StorageQualifier, const char* Precision, int iNumComponents, OPERAND_INDEX_DIMENSION eIndexDim, const char* InputName, const uint32_t ui32CompMask)
{
	Shader* psShader = psContext->psShader;
	bstring glsl = *psContext->currentGLSLString;
	uint32_t ui32Reg = psDecl->asOperands[0].ui32RegisterNumber;

	// This falls within the specified index ranges. The default is 0 if no input range is specified
	if (psShader->aIndexedInput[ui32Reg] == -1)
		return;

//	if ((ui32CompMask & ~psShader->aiConstantDeclaredSize[ui32Reg]) != 0)
	if (                 psShader->aiConstantDeclaredSize[ui32Reg]  == 0)
	{
		const char* vecType = "vec";
		const char* scalarType = "float";
		InOutSignature* psSignature = NULL;

		if (GetPatchConstantSignatureFromRegister(ui32Reg, psDecl->asOperands[0].ui32CompMask, &psShader->sInfo, &psSignature))
		{
			switch (psSignature->eComponentType)
			{
			case INOUT_COMPONENT_UINT32:
				{
					vecType = "uvec";
					scalarType = "uint";
					break;
				}
			case INOUT_COMPONENT_SINT32:
				{
					vecType = "ivec";
					scalarType = "int";
					break;
				}
			case INOUT_COMPONENT_FLOAT32:
				{
					break;
				}
			}
		}

		if (psContext->psDependencies)
		{
			if (psShader->eShaderType == PIXEL_SHADER)
			{
				psContext->psDependencies->aePixelInputInterpolation[ui32Reg] = psDecl->value.eInterpolation;
			}
		}

		bformata(glsl, "patch ", ui32Reg);

		if (iNumComponents == 1)
		{
			psContext->psShader->abScalarInput[ui32Reg] = -1;
		}

		switch (eIndexDim)
		{
		case INDEX_2D:
			{
				if (psShader->eShaderType == HULL_SHADER)
				{
					if (iNumComponents == 1)
					{
						bformata(glsl, "%s %s %s %s[/*gl_MaxPatchVertices*/];\n", StorageQualifier, Precision, scalarType, InputName, psDecl->asOperands[0].aui32ArraySizes[0]);
						bformata(glsl, "%s1 Constant%d;\n", vecType, ui32Reg);
					}
					else
					{
						bformata(glsl, "%s %s %s%d %s[/*gl_MaxPatchVertices*/];\n", StorageQualifier, Precision, vecType, iNumComponents, InputName);
						bformata(glsl, "%s%d Constant%d[gl_MaxPatchVertices];\n", vecType, iNumComponents, ui32Reg);
					}
				}
				else if (psDecl->asOperands[0].eType == OPERAND_TYPE_INPUT_CONTROL_POINT)
				{
					if (iNumComponents == 1)
					{
						bformata(glsl, "%s %s %s %s[/*%d*/];\n", StorageQualifier, Precision, scalarType, InputName, psDecl->asOperands[0].aui32ArraySizes[0]);
						bformata(glsl, "%s1 Constant%d;\n", vecType, ui32Reg);
					}
					else
					{
						bformata(glsl, "%s %s %s%d %s[/*%d*/];\n", StorageQualifier, Precision, vecType, iNumComponents, InputName, psDecl->asOperands[0].aui32ArraySizes[0]);
						bformata(glsl, "%s%d Constant%d[%d];\n", vecType, iNumComponents, ui32Reg, psDecl->asOperands[0].aui32ArraySizes[0]);
					}
				}
				else
				{
					if (iNumComponents == 1)
					{
						bformata(glsl, "%s %s %s %s[%d];\n", StorageQualifier, Precision, scalarType, InputName, psDecl->asOperands[0].aui32ArraySizes[0]);
						bformata(glsl, "%s1 Constant%d;\n", vecType, ui32Reg);
					}
					else
					{
						bformata(glsl, "%s %s %s%d %s[%d];\n", StorageQualifier, Precision, vecType, iNumComponents, InputName, psDecl->asOperands[0].aui32ArraySizes[0]);
						bformata(glsl, "%s%d Constant%d[%d];\n", vecType, iNumComponents, ui32Reg, psDecl->asOperands[0].aui32ArraySizes[0]);
					}
				}

			//	psShader->aiConstantDeclaredSize[ui32Reg] = psSignature->ui32Mask;
				psShader->aiConstantDeclaredSize[ui32Reg] = psDecl->asOperands[0].aui32ArraySizes[0];
				break;
			}
		default:
			{

				if (psDecl->asOperands[0].eType == OPERAND_TYPE_SPECIAL_TEXCOORD_INPUT)
				{
					InputName = "TexCoord";
				}

				if (iNumComponents == 1)
				{
					bformata(glsl, "%s %s %s %s %s;\n", Interpolation, StorageQualifier, Precision, scalarType, InputName);
					bformata(glsl, "%s1 Constant%d;\n", vecType, ui32Reg);

					psShader->aiConstantDeclaredSize[ui32Reg] = -1;
				}
				else if (psShader->aIndexedInput[ui32Reg] > 0)
				{
					bformata(glsl, "%s %s %s %s%d %s[%d];", Interpolation, StorageQualifier, Precision, vecType, iNumComponents, InputName, psShader->aIndexedInput[ui32Reg]);
					bformata(glsl, "%s%d Constant%d[%d];\n", vecType, iNumComponents, ui32Reg, psShader->aIndexedInput[ui32Reg]);

					psShader->aiConstantDeclaredSize[ui32Reg] = psShader->aIndexedInput[ui32Reg];
				}
				else
				{
					bformata(glsl, "%s %s %s %s%d %s;\n", Interpolation, StorageQualifier, Precision, vecType, iNumComponents, InputName);
					bformata(glsl, "%s%d Constant%d;\n", vecType, iNumComponents, ui32Reg);

					psShader->aiConstantDeclaredSize[ui32Reg] = -1;
				}
				break;
			}
		}
	}

	if (psShader->abInputReferencedByInstruction[ui32Reg])
	{
		psContext->currentGLSLString = &psContext->earlyMain;
		psContext->indent++;

		if (psShader->aiConstantDeclaredSize[ui32Reg] == -1) //Not an array
		{
			AddIndentation(psContext);
			bformata(psContext->earlyMain, "Constant%d = %s;\n", ui32Reg, InputName);
		}
		else
		{
			int arrayIndex = psShader->aiConstantDeclaredSize[ui32Reg];

			while (arrayIndex)
			{
				AddIndentation(psContext);
				bformata(psContext->earlyMain, "Constant%d[%d] = %s[%d];\n", ui32Reg, arrayIndex - 1, InputName, arrayIndex - 1);

				arrayIndex--;
			}
		}
		psContext->indent--;
		psContext->currentGLSLString = &psContext->glsl;
	}
}

void AddBuiltinInput(HLSLCrossCompilerContext* psContext, const Declaration* psDecl, const char* builtinName, uint32_t uNumComponents)
{
	bstring glsl = *psContext->currentGLSLString;
	Shader* psShader = psContext->psShader;

	if(psShader->aiInputDeclaredSize[psDecl->asOperands[0].ui32RegisterNumber] == 0)
	{
		SHADER_VARIABLE_TYPE eType = GetOperandDataType(psContext, &psDecl->asOperands[0]);
		switch(eType)
		{
		case SVT_INT:
			bformata(glsl, "ivec4 ");
			break;
		case SVT_UINT:
			bformata(glsl, "uvec4 ");
			break;
		case SVT_BOOL:
			bformata(glsl, "bvec4 ");
			break;
		default:
			bformata(glsl, "vec4 ");
			break;
		}
		TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NAME_ONLY);
		bformata(glsl, ";\n");

		psShader->aiInputDeclaredSize[psDecl->asOperands[0].ui32RegisterNumber] = 1;
	}
	else
	{
		//This register has already been declared. The HLSL bytecode likely looks
		//something like this then:
		//  dcl_input_ps constant v3.x
		//  dcl_input_ps_sgv v3.y, primitive_id

		//GLSL does not allow assignment to a varying!
	}

	psContext->currentGLSLString = &psContext->earlyMain;
	psContext->indent++;
	AddIndentation(psContext);
	TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_DESTINATION);

	bformata(psContext->earlyMain, " = %s", builtinName);

	switch(psDecl->asOperands[0].eSpecialName)
	{
	case NAME_POSITION:
		TranslateOperandSwizzle(psContext, &psDecl->asOperands[0]);
		// Invert w coordinate if necessary to be the same as SV_Position
		if (psContext->psShader->eShaderType == PIXEL_SHADER)
		{
			if (psDecl->asOperands[0].eSelMode == OPERAND_4_COMPONENT_MASK_MODE &&
				psDecl->asOperands[0].eType == OPERAND_TYPE_INPUT)
			{
				if (psDecl->asOperands[0].ui32CompMask & OPERAND_4_COMPONENT_MASK_Z)
				{
					uint32_t ui32IgnoreSwizzle;
					bcatcstr(psContext->earlyMain, ";\n#ifdef EMULATE_DEPTH_CLAMP\n");
					AddIndentation(psContext);
					TranslateVariableName(psContext, &psDecl->asOperands[0], TO_FLAG_NONE, &ui32IgnoreSwizzle);
					bcatcstr(psContext->earlyMain, ".z = unclampedDepth;\n");
					bcatcstr(psContext->earlyMain, "#endif\n");
				}
				if (psDecl->asOperands[0].ui32CompMask & OPERAND_4_COMPONENT_MASK_W)
				{
					uint32_t ui32IgnoreSwizzle;
					bcatcstr(psContext->earlyMain, ";\n");
					AddIndentation(psContext);
					TranslateVariableName(psContext, &psDecl->asOperands[0], TO_FLAG_NONE, &ui32IgnoreSwizzle);
					bcatcstr(psContext->earlyMain, ".w = 1.0f / ");
					TranslateVariableName(psContext, &psDecl->asOperands[0], TO_FLAG_NONE, &ui32IgnoreSwizzle);
					bcatcstr(psContext->earlyMain, ".w;\n");
				}
			}
			else
			{
				ASSERT(0);
			}
		}

		break;
	default:
		//Scalar built-in. Don't apply swizzle.
		break;
	}
	bcatcstr(psContext->earlyMain, ";\n");

	psContext->indent--;
	psContext->currentGLSLString = &psContext->glsl;
}

int PatchConstantNeedsDeclaring(HLSLCrossCompilerContext* psContext, const Operand* psOperand, const int count)
{
	Shader* psShader = psContext->psShader;
	const uint32_t declared = ((psContext->currentPhase + 1) << 3) | psShader->ui32CurrentVertexOutputStream;

	if (psShader->aiConstantDeclared[psOperand->ui32RegisterNumber] != declared)
	{
		int offset;

		for (offset = 0; offset < count; offset++)
		{
			psShader->aiConstantDeclared[psOperand->ui32RegisterNumber + offset] = declared;
		}
		return 1;
	}

	return 0;
}

int BuiltinOutputNeedsDeclaring(HLSLCrossCompilerContext* psContext, const Operand* psOperand, const int count)
{
	Shader* psShader = psContext->psShader;
	const uint32_t declared = ((psContext->currentPhase + 1) << 3) | psShader->ui32CurrentVertexOutputStream;

	if (psShader->eShaderType == PIXEL_SHADER)
	{
		if (psOperand->eType == OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL ||
			psOperand->eType == OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL)
		{
			return 1;
		}
	}

	if (psShader->aiOutputDeclared[psOperand->ui32RegisterNumber] != declared)
	{
		int offset;

		for (offset = 0; offset < count; offset++)
		{
			psShader->aiOutputDeclared[psOperand->ui32RegisterNumber + offset] = declared;
		}
		return 1;
	}

	return 0;
}

int OutputNeedsDeclaring(HLSLCrossCompilerContext* psContext, const Operand* psOperand, const int count)
{
	Shader* psShader = psContext->psShader;
	const uint32_t declared = ((psContext->currentPhase + 1) << 3) | 0;

	if (psShader->aiOutputDeclared[psOperand->ui32RegisterNumber] != declared)
	{
		psShader->aiOutputDeclared[psOperand->ui32RegisterNumber] = declared;

		return 1;
	}

	return 0;
}

static void AddBuiltinOutput(HLSLCrossCompilerContext* psContext, const Declaration* psDecl, const GLVARTYPE type, int arrayElements, const char* builtinName)
{
	bstring glsl = *psContext->currentGLSLString;
	Shader* psShader = psContext->psShader;

	psContext->havePostShaderCode[psContext->currentPhase] = 1;

	if (BuiltinOutputNeedsDeclaring(psContext, &psDecl->asOperands[0], arrayElements ? arrayElements : 1))
	{
		InOutSignature* psSignature = NULL;
		bstring phaseNameB = bfromcstralloc(32, "");
		bformata(phaseNameB, "phase%di%d", psContext->currentPhase, psContext->currentInst);
		char* phaseName = bstr2cstr(phaseNameB, '\0');

		GetOutputSignatureFromRegister(
			psContext->currentPhase,
			psDecl->asOperands[0].ui32RegisterNumber,
			psDecl->asOperands[0].ui32CompMask,
			0,
			&psShader->sInfo, &psSignature);

		bcatcstr(glsl, "#undef ");
		TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NAME_ONLY);
		bcatcstr(glsl, "\n");

		bcatcstr(glsl, "#define ");
		TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NAME_ONLY);
		bformata(glsl, " %s_", phaseName);
		TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NAME_ONLY);
		bcatcstr(glsl, "\n");

		switch (type)
		{
		case GLVARTYPE_INT:
			bcatcstr(glsl, "ivec4 ");
			break;
		default:
			bcatcstr(glsl, "vec4 ");
		}

		bformata(glsl, " %s_", phaseName);
		TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NAME_ONLY);
		if(arrayElements && (type == GLVARTYPE_FLOAT4))
			bformata(glsl, "[%d];\n", arrayElements);
		else
			bcatcstr(glsl, ";\n");

		psContext->currentGLSLString = &psContext->postShaderCode[psContext->currentPhase];
		glsl = *psContext->currentGLSLString;
		psContext->indent++;
		if(arrayElements && (type == GLVARTYPE_FLOAT4))
		{
			int elem;
			for(elem = 0; elem < arrayElements; elem++)
			{
				AddIndentation(psContext);
				bformata(glsl, "%s[%d] = %s(%s_", builtinName, elem, GetTypeString(type), phaseName);
				TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NAME_ONLY);
				bformata(glsl, "[%d]", elem);
				TranslateOperandSwizzle(psContext, &psDecl->asOperands[0]);
				bformata(glsl, ");\n");
			}
		}
		else if (arrayElements)
		{
			int elem;
			for (elem = 0; elem < arrayElements; elem++)
			{
				AddIndentation(psContext);
				bformata(glsl, "%s[%d] = %s(%s_", builtinName, elem, GetTypeString(type), phaseName);
				TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NAME_ONLY);
				TranslateOperandSwizzle(psContext, &psDecl->asOperands[0]);
				bformata(glsl, ");\n");
			}
		}
		else
		{

			if(psDecl->asOperands[0].eSpecialName == NAME_CLIP_DISTANCE)
			{
				int max = GetMaxComponentFromComponentMask(&psDecl->asOperands[0]);

				int applySiwzzle = GetNumSwizzleElements(&psDecl->asOperands[0]) > 1 ? 1 : 0;
				int index;
				int i;
				int multiplier = 1;

				ASSERT(psSignature!=NULL);

				index = psSignature->ui32SemanticIndex;

				//Clip distance can be spread across 1 or 2 outputs (each no more than a vec4).
				//Some examples:
				//float4 clip[2] : SV_ClipDistance; //8 clip distances
				//float3 clip[2] : SV_ClipDistance; //6 clip distances
				//float4 clip : SV_ClipDistance; //4 clip distances
				//float clip : SV_ClipDistance; //1 clip distance.

				//In GLSL the clip distance built-in is an array of up to 8 floats.
				//So vector to array conversion needs to be done here.
				if(index == 1)
				{
					InOutSignature* psFirstClipSignature;
					if(GetOutputSignatureFromSystemValue(NAME_CLIP_DISTANCE, 1, &psShader->sInfo, &psFirstClipSignature))
					{
						if(psFirstClipSignature->ui32Mask & (1 << 3))
						{
							multiplier = 4;
						}
						else if (psFirstClipSignature->ui32Mask & (1 << 2))
						{
							multiplier = 3;
						}
						else if (psFirstClipSignature->ui32Mask & (1 << 1))
						{
							multiplier = 2;
						}
					}
				}

				for(i=0; i<max; ++i)
				{
					AddIndentation(psContext);
					bformata(glsl, "%s[%d] = (%s_", builtinName, i + multiplier*index, phaseName);
					TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NONE);

					if (applySiwzzle)
						bformata(glsl, ").%s;\n", cComponentNames[i]);
					else
						bformata(glsl, ");\n");
				}
			}
			else
			{
				uint32_t elements = GetNumSwizzleElements(&psDecl->asOperands[0]);

				if(elements != GetTypeElementCount(type))
				{
					//This is to handle float3 position seen in control point phases
					//struct HS_OUTPUT
					//{
					//	float3 vPosition : POSITION;
					//}; -> dcl_output o0.xyz
					//gl_Position is vec4.
					AddIndentation(psContext);
					bformata(glsl, "%s = %s(%s_", builtinName, GetTypeString(type), phaseName);
					TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NONE);
					bformata(glsl, ", 1);\n");
				}
				else
				{
					AddIndentation(psContext);
					bformata(glsl, "%s = %s(%s_", builtinName, GetTypeString(type), phaseName);
					TranslateOperand(psContext, &psDecl->asOperands[0], type == GLVARTYPE_INT ? TO_FLAG_INTEGER : TO_FLAG_NONE);
					bformata(glsl, ");\n");
				}
			}

			if (psShader->eShaderType == VERTEX_SHADER && psDecl->asOperands[0].eSpecialName == NAME_POSITION)
			{
				if (psContext->flags & HLSLCC_FLAG_INVERT_CLIP_SPACE_Y)
				{
					AddIndentation(psContext);
					bformata(glsl, "gl_Position.y = -gl_Position.y;\n");
				}

				if (EmulateDepthClamp(psContext->psShader->eTargetLanguage))
				{
					bcatcstr(glsl, "#ifdef EMULATE_DEPTH_CLAMP\n");
					bcatcstr(glsl, "#if EMULATE_DEPTH_CLAMP == 1\n");
					AddIndentation(psContext);
					bcatcstr(glsl, "unclampedDepth = gl_DepthRange.near + gl_DepthRange.diff * gl_Position.z / gl_Position.w;\n");
					bcatcstr(glsl, "#elif EMULATE_DEPTH_CLAMP == 2\n");
					AddIndentation(psContext);
					bcatcstr(glsl, "unclampedZ = gl_DepthRange.diff * gl_Position.z;\n");
					bcatcstr(glsl, "#endif\n");
					AddIndentation(psContext);
					bcatcstr(glsl, "gl_Position.z = 0.0;\n");
				}

				if (psContext->flags & HLSLCC_FLAG_CONVERT_CLIP_SPACE_Z)
				{
					if (EmulateDepthClamp(psContext->psShader->eTargetLanguage))
						bcatcstr(glsl, "#else\n");

					AddIndentation(psContext);
					bcatcstr(glsl, "gl_Position.z = gl_Position.z * 2.0 - gl_Position.w;\n");
				}

				if (EmulateDepthClamp(psContext->psShader->eTargetLanguage))
					bcatcstr(glsl, "#endif\n");
			}
		}
		psContext->indent--;
		psContext->currentGLSLString = &psContext->glsl;
		bdestroy(phaseNameB);
		bcstrfree(phaseName);
	}
}

static void DeclareOutput(HLSLCrossCompilerContext* psContext, const Declaration* psDecl)
{
	bstring glsl = *psContext->currentGLSLString;
	Shader* psShader = psContext->psShader;

	if (OutputNeedsDeclaring(psContext, &psDecl->asOperands[0], 1))
	{
		const Operand* psOperand = &psDecl->asOperands[0];
		const char* Precision = "";
		const char* type = "vec";

		InOutSignature* psSignature = NULL;

		if (GetOutputSignatureFromRegister(
			psContext->currentPhase,
			psDecl->asOperands[0].ui32RegisterNumber,
			psDecl->asOperands[0].ui32CompMask,
			psShader->ui32CurrentVertexOutputStream,
			&psShader->sInfo,
			&psSignature))
		{
			switch (psSignature->eComponentType)
			{
				case INOUT_COMPONENT_UINT32:
				{
					type = "uvec";
					break;
				}
				case INOUT_COMPONENT_SINT32:
				{
					type = "ivec";
					break;
				}
				case INOUT_COMPONENT_FLOAT32:
				{
					break;
				}
			}
		}

		if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
		{
			Precision = GetPrecisionAttributeFromOperand(psOperand->eMinPrecision);
		}

		switch(psShader->eShaderType)
		{
		case PIXEL_SHADER:
			{
				switch(psDecl->asOperands[0].eType)
				{
				case OPERAND_TYPE_OUTPUT_COVERAGE_MASK:
				case OPERAND_TYPE_OUTPUT_DEPTH:
					{

						break;
					}
				case OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
					{
						bcatcstr(glsl, "#ifdef GL_ARB_conservative_depth\n");
						bcatcstr(glsl, "#extension GL_ARB_conservative_depth : enable\n");
						bcatcstr(glsl, "layout (depth_greater) out float gl_FragDepth;\n");
						bcatcstr(glsl, "#endif\n");
						break;
					}
				case OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL:
					{
						bcatcstr(glsl, "#ifdef GL_ARB_conservative_depth\n");
						bcatcstr(glsl, "#extension GL_ARB_conservative_depth : enable\n");
						bcatcstr(glsl, "layout (depth_less) out float gl_FragDepth;\n");
						bcatcstr(glsl, "#endif\n");
						break;
					}
				default:
					{
						if(WriteToFragData(psContext->psShader->eTargetLanguage))
						{
							bformata(glsl, "#define Output%d gl_FragData[%d]\n", psDecl->asOperands[0].ui32RegisterNumber, psDecl->asOperands[0].ui32RegisterNumber);
						}
						else
						{
							int stream = 0;
							char* OutputName = GetDeclaredOutputName(psContext, PIXEL_SHADER, psOperand, &stream);

							if (HaveInOutLocationQualifier(psContext->psShader->eTargetLanguage, psContext->psShader->extensions, psContext->flags) ||
								HaveLimitedInOutLocationQualifier(psContext->psShader->eTargetLanguage, PIXEL_SHADER, 2, psContext->flags))
							{
								uint32_t index = 0;
								uint32_t renderTarget = psDecl->asOperands[0].ui32RegisterNumber;

								if((psContext->flags & HLSLCC_FLAG_DUAL_SOURCE_BLENDING) && DualSourceBlendSupported(psContext->psShader->eTargetLanguage))
								{
									if(renderTarget > 0)
									{
										renderTarget = 0;
										index = 1;
									}
									bformata(glsl, "layout(location = %d, index = %d) ", renderTarget, index);
								}
								else
								{
									bformata(glsl, "layout(location = %d) ", renderTarget);
								}
							}

							bformata(glsl, "out %s %s4 %s;\n", Precision, type, OutputName);
							if(stream)
							{
								bformata(glsl, "#define Output%d_S%d %s\n", psDecl->asOperands[0].ui32RegisterNumber, stream, OutputName);
							}
							else
							{
								bformata(glsl, "#define Output%d %s\n", psDecl->asOperands[0].ui32RegisterNumber, OutputName);
							}
							bcstrfree(OutputName);
						}
						break;
					}
				}
				break;
			}
		case VERTEX_SHADER:
			{
				int iNumComponents = 4;//GetMaxComponentFromComponentMask(&psDecl->asOperands[0]);
				const char* Interpolation = "";
				int stream = 0;
				char* OutputName = GetDeclaredOutputName(psContext, VERTEX_SHADER, psOperand, &stream);

				if (psContext->psDependencies)
				{
					if (psShader->eShaderType == VERTEX_SHADER)
					{
						Interpolation = GetInterpolationString(psContext->psDependencies->aePixelInputInterpolation[psDecl->asOperands[0].ui32RegisterNumber], psShader->eTargetLanguage);
					}
				}

				if (HaveInOutLocationQualifier(psContext->psShader->eTargetLanguage, psContext->psShader->extensions, psContext->flags))
				{
					bformata(glsl, "layout(location = %d) ", psDecl->asOperands[0].ui32RegisterNumber);
				}

				if (InOutSupported(psContext->psShader->eTargetLanguage))
				{
					bformata(glsl, "%s out %s %s%d %s;\n", Interpolation, Precision, type, iNumComponents, OutputName);
				}
				else
				{
					bformata(glsl, "%s varying %s %s%d %s;\n", Interpolation, Precision, type, iNumComponents, OutputName);
				}
				bformata(glsl, "#define Output%d %s\n", psDecl->asOperands[0].ui32RegisterNumber, OutputName);
				bcstrfree(OutputName);

				break;
			}
		case GEOMETRY_SHADER:
			{
				int stream = 0;
				char* OutputName = GetDeclaredOutputName(psContext, GEOMETRY_SHADER, psOperand, &stream);

				if (HaveInOutLocationQualifier(psContext->psShader->eTargetLanguage, psContext->psShader->extensions, psContext->flags))
				{
					bformata(glsl, "layout(location = %d) ", psDecl->asOperands[0].ui32RegisterNumber);
				}

				bformata(glsl, "out %s4 %s;\n", type, OutputName);
				if(stream)
				{
					bformata(glsl, "#define Output%d_S%d %s\n", psDecl->asOperands[0].ui32RegisterNumber, stream, OutputName);
				}
				else
				{
					bformata(glsl, "#define Output%d %s\n", psDecl->asOperands[0].ui32RegisterNumber, OutputName);
				}
				bcstrfree(OutputName);
				break;
			}
		case HULL_SHADER:
			{
				int stream = 0;
				char* OutputName = GetDeclaredOutputName(psContext, HULL_SHADER, psOperand, &stream);

				ASSERT(psDecl->asOperands[0].ui32RegisterNumber!=0);//Reg 0 should be gl_out[gl_InvocationID].gl_Position.

				if(psContext->currentPhase == HS_JOIN_PHASE)
				{
					bformata(glsl, "out patch %s4 %s[];\n", type, OutputName);
				}
				else
				{
					if (HaveInOutLocationQualifier(psContext->psShader->eTargetLanguage, psContext->psShader->extensions, psContext->flags))
					{
						bformata(glsl, "layout(location = %d) ", psDecl->asOperands[0].ui32RegisterNumber);
					}
					bformata(glsl, "out %s4 %s[];\n", type, OutputName);
				}
				bformata(glsl, "#define Output%d %s[gl_InvocationID]\n", psDecl->asOperands[0].ui32RegisterNumber, OutputName);
				bcstrfree(OutputName);
				break;
			}
		case DOMAIN_SHADER:
			{
				int stream = 0;
				char* OutputName = GetDeclaredOutputName(psContext, DOMAIN_SHADER, psOperand, &stream);
				if (HaveInOutLocationQualifier(psContext->psShader->eTargetLanguage, psContext->psShader->extensions, psContext->flags))
				{
					bformata(glsl, "layout(location = %d) ", psDecl->asOperands[0].ui32RegisterNumber);
				}
				bformata(glsl, "out %s4 %s;\n", type, OutputName);
				bformata(glsl, "#define Output%d %s\n", psDecl->asOperands[0].ui32RegisterNumber, OutputName);
				bcstrfree(OutputName);
				break;
			}
		}
	}

	// This works just fine if there is no interpolator involved
	//
	// TEXCOORD                 0   xy          1     NONE   float   xy  
	// TEXCOORD                 1     z         1     NONE   float     z 
	//
	// dcl_output o1.xy
	// dcl_output o1.z
	//
	// add o1.z, r0.y, cb5[2].x
	// mov o1.xy, v1.xyxx
#if NOTNEEDEDSOFAR
	else
	{
		/*
		Multiple outputs can be packed into one register. e.g.
		// Name                 Index   Mask Register SysValue  Format   Used
		// -------------------- ----- ------ -------- -------- ------- ------
		// FACTOR                   0   x           3     NONE     int   x   
		// MAX                      0    y          3     NONE     int    y  

		We want unique outputs to make it easier to use transform feedback.

		out  ivec4 FACTOR0;
		#define Output3 FACTOR0
		out  ivec4 MAX0;

		MAIN SHADER CODE. Writes factor and max to Output3 which aliases FACTOR0.

		MAX0.x = FACTOR0.y;

		This unpacking of outputs is only done when using HLSLCC_FLAG_INOUT_SEMANTIC_NAMES/HLSLCC_FLAG_INOUT_APPEND_SEMANTIC_NAMES.
		When not set the application will be using HLSL reflection information to discover
		what the input and outputs mean if need be.
		*/

		//

		if((psContext->flags & (HLSLCC_FLAG_INOUT_SEMANTIC_NAMES|HLSLCC_FLAG_INOUT_APPEND_SEMANTIC_NAMES)) && (psDecl->asOperands[0].eType == OPERAND_TYPE_OUTPUT))
		{
			const Operand* psOperand = &psDecl->asOperands[0];
			InOutSignature* psSignature = NULL;
			const char* type = "vec";
			int stream = 0;
			char* OutputName = GetDeclaredOutputName(psContext, psShader->eShaderType, psOperand, &stream);

			GetOutputSignatureFromRegister(
				psContext->currentPhase,
				psOperand->ui32RegisterNumber,
				psOperand->ui32CompMask,
				0,
				&psShader->sInfo,
				&psSignature);

			if HaveInOutLocationQualifier(psContext->psShader->eTargetLanguage,psContext->psShader->extensions,psContext->flags))
			{
				bformata(glsl, "layout(location = %d) ", psDecl->asOperands[0].ui32RegisterNumber);
			}

			switch(psSignature->eComponentType)
			{
			case INOUT_COMPONENT_UINT32:
				{
					type = "uvec";
					break;
				}
			case INOUT_COMPONENT_SINT32:
				{
					type = "ivec";
					break;
				}
			case INOUT_COMPONENT_FLOAT32:
				{
					break;
				}
			}
			bformata(glsl, "out %s4 %s;\n", type, OutputName);

			psContext->havePostShaderCode[psContext->currentPhase] = 1;

			psContext->currentGLSLString = &psContext->postShaderCode[psContext->currentPhase];
			glsl = *psContext->currentGLSLString;

			bcatcstr(glsl, OutputName);
			bcstrfree(OutputName);
			AddSwizzleUsingElementCount(psContext, GetNumSwizzleElements(psOperand));
			bformata(glsl, " = Output%d", psOperand->ui32RegisterNumber);
			TranslateOperandSwizzle(psContext, psOperand);
			bcatcstr(glsl, ";\n");

			psContext->currentGLSLString = &psContext->glsl;
			glsl = *psContext->currentGLSLString;
		}
	}
#endif
}

static void DeclareOutputPatchConstant(HLSLCrossCompilerContext* psContext, const Declaration* psDecl)
{
	bstring glsl = *psContext->currentGLSLString;
	Shader* psShader = psContext->psShader;

	psContext->havePostShaderCode[psContext->currentPhase] = 1;

	if (PatchConstantNeedsDeclaring(psContext, &psDecl->asOperands[0], 1))
	{
		const Operand* psOperand = &psDecl->asOperands[0];
		const int usePrec = psContext->flags & HLSLCC_FLAG_FORCE_PRECISION_QUALIFIERS;
		const char* Precision = "";
		const char* type = "vec";
		int arrayElements = 0;
		char *customName = GetDeclaredPatchConstantName(psContext, psShader->eShaderType, psOperand);
		SHADER_VARIABLE_TYPE shaderType = SVT_FLOAT;
		uint32_t typeFlag = TO_FLAG_NONE;
		uint32_t destElemCount = GetNumSwizzleElements(psOperand);

		InOutSignature* psSignature = NULL;
		bstring phaseNameB = bfromcstralloc(32, "");
		bformata(phaseNameB, "phase%di%d", psContext->currentPhase, psContext->currentInst);
		char* phaseName = bstr2cstr(phaseNameB, '\0');

		if (GetPatchConstantSignatureFromRegister(
			psOperand->ui32RegisterNumber,
			psOperand->ui32CompMask,
			&psShader->sInfo,
			&psSignature))
		{
			switch (psSignature->eComponentType)
			{
				case INOUT_COMPONENT_UINT32:
				{
					shaderType = SVT_UINT;
					typeFlag = TO_FLAG_UNSIGNED_INTEGER | TO_AUTO_BITCAST_TO_UINT | TO_AUTO_EXPAND_TO_VEC4;
					type = "uvec";
					break;
				}
				case INOUT_COMPONENT_SINT32:
				{
					shaderType = SVT_INT;
					typeFlag = TO_FLAG_INTEGER | TO_AUTO_BITCAST_TO_INT | TO_AUTO_EXPAND_TO_VEC4;
					type = "ivec";
					break;
				}
				case INOUT_COMPONENT_FLOAT32:
				{
					break;
				}
			}
		}

		if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
		{
			Precision = GetPrecisionAttributeFromOperand(psOperand->eMinPrecision);
		}

		bcatcstr(glsl, "#undef ");
		TranslateOperand(psContext, psOperand, TO_FLAG_NAME_ONLY);
		bcatcstr(glsl, "\n");

		bcatcstr(glsl, "#define ");
		TranslateOperand(psContext, psOperand, TO_FLAG_NAME_ONLY);
		bformata(glsl, " %s_", phaseName);
		TranslateOperand(psContext, psOperand, TO_FLAG_NAME_ONLY);
		bcatcstr(glsl, "\n");

		bformata(glsl, "%s ", GetConstructorForType(shaderType, 4, usePrec));

		bformata(glsl, "%s_", phaseName);
		TranslateOperand(psContext, psOperand, TO_FLAG_NAME_ONLY);
		if (arrayElements)
			bformata(glsl, "[%d];\n", arrayElements);
		else
			bcatcstr(glsl, ";\n");

		/* TODO: prevent redefining:
		 *
		 *	//fork_phase declarations
		 *	patch out vec4 pTEXCOORD0;
		 *	void fork_phase2()
		 *	//join_phase declarations
		 *	patch out vec4 pTEXCOORD0;
		 *	void join_phase0()
		 */
		bformata(glsl, "patch out %s %s;\n", GetConstructorForType(shaderType, 4, usePrec), customName);

		psContext->currentGLSLString = &psContext->postShaderCode[psContext->currentPhase];
		glsl = *psContext->currentGLSLString;
		psContext->indent++;
		if (arrayElements)
		{
			int elem;
			for (elem = 0; elem < arrayElements; elem++)
			{
				AddIndentation(psContext);
				bformata(glsl, "%s[%d] = %s(%s_", customName, elem, GetConstructorForType(shaderType, 4, usePrec), phaseName);
				TranslateOperand(psContext, psOperand, TO_FLAG_NAME_ONLY);
				bformata(glsl, "[%d]", elem);
				TranslateOperandSwizzle(psContext, psOperand);
				bformata(glsl, ");\n");
			}
		}
		else
		{
			{
			//	if (elements != GetTypeElementCount(type))
			//	{
			//		//This is to handle float3 position seen in control point phases
			//		//struct HS_OUTPUT
			//		//{
			//		//	float3 vPosition : POSITION;
			//		//}; -> dcl_output o0.xyz
			//		//gl_Position is vec4.
			//		AddIndentation(psContext);
			//		bformata(glsl, "%s = %s(%s_", customName, type, phaseName);
			//		TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NONE);
			//		bformata(glsl, ", 1);\n");
			//	}
			//	else
				{
					//     pMY_PATCH_ID0 = int( phase3i1_ivec4(Constant0.y));
					AddIndentation(psContext);
					bformata(glsl, "%s = %s(%s_", customName, GetConstructorForType(shaderType, 4, usePrec), phaseName);
					TranslateOperand(psContext, psOperand, TO_FLAG_NAME_ONLY);
					TranslateOperandSwizzleWithMask(psContext, psOperand, OPERAND_4_COMPONENT_SWIZZLE_MODE);
					bformata(glsl, ");\n");
				}
			}
		}
		psContext->indent--;
		psContext->currentGLSLString = &psContext->glsl;
		bdestroy(phaseNameB);
		bcstrfree(phaseName);
	}
}

static void DeclareUBOConstants(HLSLCrossCompilerContext* psContext, const uint32_t ui32BindingPoint, ConstantBuffer* psCBuf)
{
	bstring glsl = *psContext->currentGLSLString;

	const char* NameSuffix = "";
	uint32_t i, implicitOffset, alignedOffset;
	const char* Name = psCBuf->Name;
	uint32_t auiSortedVars[MAX_SHADER_VARS];

	Name += (psCBuf->Name[0] == '$'); //For $Globals

	// Sort by offset
	if (psCBuf->ui32NumVars > 0)
	{
		uint32_t bSorted = 1;
		auiSortedVars[0] = 0;
		for (i = 1; i < psCBuf->ui32NumVars; ++i)
		{
			auiSortedVars[i] = i;
			bSorted = bSorted && psCBuf->asVars[i - 1].ui32StartOffset <= psCBuf->asVars[i].ui32StartOffset;
		}
		while (!bSorted)
		{
			bSorted = 1;
			for (i = 1; i < psCBuf->ui32NumVars; ++i)
			{
				if (psCBuf->asVars[auiSortedVars[i - 1]].ui32StartOffset > psCBuf->asVars[auiSortedVars[i]].ui32StartOffset)
				{
					uint32_t uiTemp = auiSortedVars[i];
					auiSortedVars[i] = auiSortedVars[i - 1];
					auiSortedVars[i - 1] = uiTemp;
					bSorted = 0;
				}
			}
		}
		auiSortedVars[psCBuf->ui32NumVars] = psCBuf->ui32NumVars;
	}

	for (i = 0; i < psCBuf->ui32NumVars; ++i)
	{
		ShaderVar* psVar = psCBuf->asVars + auiSortedVars[i];

		PreDeclareStructType(psContext, psCBuf->Name, psVar->sType.Name, &psVar->sType, customUBO, 140, 0);
	}

	/* [layout (location = X)] uniform vec4 HLSLConstantBufferName[numConsts]; */
	if (packedUBO)
		if (HaveUniformBindingsAndLocations(psContext->psShader->eTargetLanguage, psContext->psShader->extensions,psContext->flags) && (psContext->flags & HLSLCC_FLAG_AVOID_RESOURCE_BINDINGS_AND_LOCATIONS) == 0)
			bformata(glsl, "layout(packed, binding = %d) ", ui32BindingPoint);
		else
			bformata(glsl, "layout(packed) ");
	else if (sharedUBO)
		if (HaveUniformBindingsAndLocations(psContext->psShader->eTargetLanguage, psContext->psShader->extensions,psContext->flags) && (psContext->flags & HLSLCC_FLAG_AVOID_RESOURCE_BINDINGS_AND_LOCATIONS) == 0)
			bformata(glsl, "layout(shared, binding = %d) ", ui32BindingPoint);
		else
			bformata(glsl, "layout(shared) ");
	else
		if (HaveUniformBindingsAndLocations(psContext->psShader->eTargetLanguage, psContext->psShader->extensions,psContext->flags) && (psContext->flags & HLSLCC_FLAG_AVOID_RESOURCE_BINDINGS_AND_LOCATIONS) == 0)
			bformata(glsl, "layout(std140, binding = %d) ", ui32BindingPoint);
		else
			bformata(glsl, "layout(std140) ");

	if ((psContext->flags & HLSLCC_FLAG_MANGLE_IDENTIFIERS_PER_STAGE) != 0)
		NameSuffix = GetMangleSuffix(psContext->psShader->eShaderType);

	bformata(glsl, "uniform ");
	ConvertToUniformBufferName(glsl, psContext->psShader, psCBuf->Name, ui32BindingPoint);
	bformata(glsl, " {\n ");

	// Stuff the aligned CB's size into the the last asVars
	psCBuf->asVars[psCBuf->ui32NumVars].ui32StartOffset = psCBuf->ui32TotalSizeInBytes;

	implicitOffset = 0;
	for (i = 0; i < psCBuf->ui32NumVars; ++i)
	{
		ShaderVar* psVar = psCBuf->asVars + auiSortedVars[i];
		uint32_t compliant = 0;

		if (!packedUBO && !sharedUBO)
		{
			if (customUBO != ALIGNMENT_LAYOUT)
			{
				uint32_t uVarAlignment, uVarSize;
				GetSTDLayout(&psVar->sType, &uVarAlignment, &uVarSize, ALIGNMENT_STANDARD, 140);

				alignedOffset = implicitOffset;
				alignedOffset += uVarAlignment - 1;
				alignedOffset -= alignedOffset % uVarAlignment;

				compliant =
					(implicitOffset + 0        == (psCBuf->asVars + auiSortedVars[i + 0])->ui32StartOffset) &&
					(implicitOffset + uVarSize == (psCBuf->asVars + auiSortedVars[i + 1])->ui32StartOffset);

				GetSTDLayout(&psVar->sType, &uVarAlignment, &uVarSize, customUBO, 140);

				// fill in padding variable to produce offsets, instead of the "layout(offset=...)" way
				// works only if DirectX's alignment is always larger than OpenGL's alignment
				if ((implicitOffset + 4 - 1) / 4 < psVar->ui32StartOffset / 4)
				{
					// if we'd use arrays we'd fall into the std140 alignment trap again, thus we use individual floats
					int32_t  nNumPaddingFloats = psVar->ui32StartOffset / 4 - (implicitOffset + 4 - 1) / 4;
					while (--nNumPaddingFloats >= 0)
						bformata(glsl, "\tfloat padding_%s_%d_%d;\n", Name, psVar->ui32StartOffset, nNumPaddingFloats);

					implicitOffset = psVar->ui32StartOffset - psVar->ui32StartOffset % 4;
				}

				implicitOffset += uVarAlignment - 1;
				implicitOffset -= implicitOffset % uVarAlignment;

				if (implicitOffset != psVar->ui32StartOffset)
					bformata(glsl, "#error Position of member %s.%s is %d in HLSL, but should be %d\n", Name, psVar->Name, psVar->ui32StartOffset, implicitOffset);

				implicitOffset += uVarSize;
			}
		}

		DeclareConstBufferShaderVariable(psContext, psCBuf->Name, psVar->sType.Name, &psVar->sType, compliant ? ALIGNMENT_STANDARD : customUBO, psVar->ui32StartOffset, 0);
	}

	bcatcstr(glsl, "};\n");
}

void DeclareBufferVariable(HLSLCrossCompilerContext* psContext, const uint32_t ui32BindingPoint, ConstantBuffer* psCBuf, const Operand* psOperand, const uint32_t ui32GloballyCoherentAccess, const ResourceType eResourceType)
{
	const char* Name = psCBuf->Name;
	bstring StructName;
	uint32_t unnamed_struct = strcmp(psCBuf->asVars[0].Name, "$Element") == 0 ? 1 : 0;
	bstring glsl = *psContext->currentGLSLString;
	uint32_t compliant = 0;

	ASSERT(psCBuf->ui32NumVars == 1);
	ASSERT(unnamed_struct);

	StructName = bfromcstr("");

	//TranslateOperand(psContext, psOperand, TO_FLAG_NAME_ONLY);
	uint32_t location = ui32BindingPoint;
	uint32_t ui32BindingPointNew = ui32BindingPoint;
	if(psOperand->eType == OPERAND_TYPE_RESOURCE && eResourceType == RTYPE_STRUCTURED)
	{
		// Constant buffer locations start at 0. Resource locations start at ui32NumConstantBuffers.
		ui32BindingPointNew = psContext->psShader->sInfo.ui32NumConstantBuffers + ui32BindingPoint;
		bformata(StructName, "StructuredRes%d", psOperand->ui32RegisterNumber);
	}
	else if(psOperand->eType == OPERAND_TYPE_RESOURCE && eResourceType == RTYPE_UAV_RWBYTEADDRESS)
	{
		// Constant buffer locations start at 0. UAV locations start at ui32NumConstantBuffers. UAV locations start at ui32NumConstantBuffers + ui32NumSamplers.
		ui32BindingPointNew = psContext->psShader->sInfo.ui32NumConstantBuffers + psContext->psShader->sInfo.ui32NumSamplers + ui32BindingPoint;
		bformata(StructName, "RawRes%d", psOperand->ui32RegisterNumber);
	}
	else
	{
		// Constant buffer locations start at 0. UAV locations start at ui32NumConstantBuffers. UAV locations start at ui32NumConstantBuffers + ui32NumSamplers.
		ui32BindingPointNew = psContext->psShader->sInfo.ui32NumConstantBuffers + psContext->psShader->sInfo.ui32NumSamplers + ui32BindingPoint;
		//ResourceName(StructName, psContext, RGROUP_UAV, psOperand->ui32RegisterNumber, 0);
		bformata(StructName, "UAV%d", psOperand->ui32RegisterNumber);
	}

	PreDeclareStructType(psContext, Name, bstr2cstr(StructName, '\0'), &psCBuf->asVars[0].sType, customStruct, 430, 1);

	/* [layout (binding = X)] uniform vec4 HLSLConstantBufferName[numConsts]; */
	if (HaveUniformBindingsAndLocations(psContext->psShader->eTargetLanguage,psContext->psShader->extensions,psContext->flags) && (psContext->flags & HLSLCC_FLAG_AVOID_RESOURCE_BINDINGS_AND_LOCATIONS) == 0)
		bformata(glsl, "layout(std430, binding = %d) ", ui32BindingPointNew);
	else
		bformata(glsl, "layout(std430) ");

	if (ui32GloballyCoherentAccess & GLOBALLY_COHERENT_ACCESS)
	{
		bcatcstr(glsl, "coherent ");
	}

	if (eResourceType == RTYPE_STRUCTURED)
	{
		bcatcstr(glsl, "readonly ");
	}

	bcatcstr(glsl, "buffer ");
	if (eResourceType == RTYPE_STRUCTURED)
		ConvertToTextureName(glsl, psContext->psShader, Name, NULL, location, 0);
	else
		ConvertToUAVName(glsl, psContext->psShader, Name, location);
	bcatcstr(glsl, " {\n ");

	DeclareConstBufferShaderVariable(psContext, Name, bstr2cstr(StructName, '\0'), &psCBuf->asVars[0].sType, compliant ? ALIGNMENT_STANDARD : customStruct, psCBuf->asVars[0].ui32StartOffset, 1);

	bcatcstr(glsl, "};\n");

	bdestroy(StructName);
}


static void DeclareStructConstants(HLSLCrossCompilerContext* psContext, const uint32_t ui32BindingPoint, ConstantBuffer* psCBuf, const Operand* psOperand)
{
	const char* Name = psCBuf->Name;
	bstring glsl = *psContext->currentGLSLString;
	uint32_t i;
	int useGlobalsStruct = 1;
	uint32_t compliant = 0;

	if (psContext->flags & HLSLCC_FLAG_DISABLE_GLOBALS_STRUCT && psCBuf->Name[0] == '$')
		useGlobalsStruct = 0;

	if (useGlobalsStruct)
	{
		for (i = 0; i < psCBuf->ui32NumVars; ++i)
		{
			PreDeclareStructType(psContext, Name, psCBuf->asVars[i].sType.Name, &psCBuf->asVars[i].sType, customStruct, 140, 0);
		}
	}

	/* [layout (binding = X)] uniform HLSLConstantBufferName { numMembers }; */
	if (HaveUniformBindingsAndLocations(psContext->psShader->eTargetLanguage,psContext->psShader->extensions,psContext->flags) && (psContext->flags & HLSLCC_FLAG_AVOID_RESOURCE_BINDINGS_AND_LOCATIONS) == 0)
		bformata(glsl, "layout(std140, binding = %d) ", ui32BindingPoint);
	else
		bformata(glsl, "layout(std140) ");
	
	if (useGlobalsStruct)
	{
		bcatcstr(glsl, "uniform struct ");
		TranslateOperand(psContext, psOperand, TO_FLAG_DECLARATION_NAME);
		bcatcstr(glsl, "_Type {\n");
	}

	for (i = 0; i < psCBuf->ui32NumVars; ++i)
	{
		if (!useGlobalsStruct)
			bcatcstr(glsl, "uniform ");

		DeclareConstBufferShaderVariable(psContext, Name, psCBuf->asVars[i].sType.Name, &psCBuf->asVars[i].sType, compliant ? ALIGNMENT_STANDARD : customStruct, psCBuf->asVars[i].ui32StartOffset, 0);
	}

	if (useGlobalsStruct)
	{
		bcatcstr(glsl, "} ");

		TranslateOperand(psContext, psOperand, TO_FLAG_DECLARATION_NAME);

		bcatcstr(glsl, ";\n");
	}
}

char* GetSamplerType(HLSLCrossCompilerContext* psContext, const RESOURCE_DIMENSION eDimension, const uint32_t ui32RegisterNumber)
{
	ResourceBinding* psBinding = 0;
	RESOURCE_RETURN_TYPE eType = RETURN_TYPE_UNORM;
	char* prefix = "";

	int found = GetResourceFromBindingPoint(RGROUP_TEXTURE, ui32RegisterNumber, &psContext->psShader->sInfo, &psBinding);
	if(found)
	{
		eType = (RESOURCE_RETURN_TYPE)psBinding->ui32ReturnType;
	}

	switch (eType)
	{
	case RETURN_TYPE_UINT:
		prefix = "u";
		break;
	case RETURN_TYPE_SINT:
		prefix = "i";
		break;
	default:
		break;
	}

	switch(eDimension)
	{
		case RESOURCE_DIMENSION_BUFFER:
		{
			switch(eType)
			{
				case RETURN_TYPE_SINT:
					return "isamplerBuffer";
				case RETURN_TYPE_UINT:
					return "usamplerBuffer";
				default:
					return "samplerBuffer";
			}
			break;
		}

		case RESOURCE_DIMENSION_TEXTURE1D:
		{
			switch(eType)
			{
				case RETURN_TYPE_SINT:
					return "isampler1D";
				case RETURN_TYPE_UINT:
					return "usampler1D";
				default:
					return "sampler1D";
			}
			break;
		}

		case RESOURCE_DIMENSION_TEXTURE2D:
		{
			switch(eType)
			{
				case RETURN_TYPE_SINT:
					return "isampler2D";
				case RETURN_TYPE_UINT:
					return "usampler2D";
				default:
					return "sampler2D";
			}
			break;
		}

		case RESOURCE_DIMENSION_TEXTURE2DMS:
		{
			switch(eType)
			{
				case RETURN_TYPE_SINT:
					return "isampler2DMS";
				case RETURN_TYPE_UINT:
					return "usampler2DMS";
				default:
					return "sampler2DMS";
			}
			break;
		}

		case RESOURCE_DIMENSION_TEXTURE3D:
		{
			switch(eType)
			{
				case RETURN_TYPE_SINT:
					return "isampler3D";
				case RETURN_TYPE_UINT:
					return "usampler3D";
				default:
					return "sampler3D";
			}
			break;
		}

		case RESOURCE_DIMENSION_TEXTURECUBE:
		{
			switch(eType)
			{
				case RETURN_TYPE_SINT:
					return "isamplerCube";
				case RETURN_TYPE_UINT:
					return "usamplerCube";
				default:
					return "samplerCube";
			}
			break;
		}

		case RESOURCE_DIMENSION_TEXTURE1DARRAY:
		{
			switch(eType)
			{
				case RETURN_TYPE_SINT:
					return "isampler1DArray";
				case RETURN_TYPE_UINT:
					return "usampler1DArray";
				default:
					return "sampler1DArray";
			}
			break;
		}

		case RESOURCE_DIMENSION_TEXTURE2DARRAY:
		{
			switch(eType)
			{
				case RETURN_TYPE_SINT:
					return "isampler2DArray";
				case RETURN_TYPE_UINT:
					return "usampler2DArray";
				default:
					return "sampler2DArray";
			}
			break;
		}

		case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
		{
			switch(eType)
			{
				case RETURN_TYPE_SINT:
					return "isampler2DMSArray";
				case RETURN_TYPE_UINT:
					return "usampler2DMSArray";
				default:
					return "sampler2DMSArray";
			}
			break;
		}

		case RESOURCE_DIMENSION_TEXTURECUBEARRAY:
		{
			switch(eType)
			{
				case RETURN_TYPE_SINT:
					return "isamplerCubeArray";
				case RETURN_TYPE_UINT:
					return "usamplerCubeArray";
				default:
					return "samplerCubeArray";
			}
			break;
		}
	}

	return "sampler2D";
}

static void TranslateResourceTexture(HLSLCrossCompilerContext* psContext, const Declaration* psDecl, uint32_t samplerCanDoShadowCmp)
{
	const int combineTextureSampler = (psContext->flags & HLSLCC_FLAG_COMBINE_TEXTURE_SAMPLERS) && psDecl->ui32SamplerUsedCount;
	bstring glsl = *psContext->currentGLSLString;
	Shader* psShader = psContext->psShader;
	uint32_t i;

	const char* samplerTypeName = GetSamplerType(psContext, psDecl->value.eResourceDimension, psDecl->asOperands[0].ui32RegisterNumber);
	const char* samplerPrecison = HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags) ? GetPrecisionAttributeFromType(psDecl->eReturnType) : "";

	if (combineTextureSampler)
	{
		if (samplerCanDoShadowCmp && psDecl->ui32IsShadowTex)
		{
			for (i = 0; i < psDecl->ui32SamplerUsedCount; i++)
			{
				bformata(glsl, "uniform %s", samplerPrecison);
				bcatcstr(glsl, samplerTypeName);
				bcatcstr(glsl, "Shadow ");
				TextureName(glsl, psContext->psShader, psDecl->asOperands[0].ui32RegisterNumber, psDecl->ui32SamplerUsed[i], 1);
				bcatcstr(glsl, ";\n");
			}
		}
		for (i = 0; i < psDecl->ui32SamplerUsedCount; i++)
		{
			bformata(glsl, "uniform %s", samplerPrecison);
			bcatcstr(glsl, samplerTypeName);
			bcatcstr(glsl, " ");
			TextureName(glsl, psContext->psShader, psDecl->asOperands[0].ui32RegisterNumber, psDecl->ui32SamplerUsed[i], 0);
			bcatcstr(glsl, ";\n");
		}
	}
	else
	{
		if (samplerCanDoShadowCmp && psDecl->ui32IsShadowTex)
		{
			//Create shadow and non-shadow sampler.
			//HLSL does not have separate types for depth compare, just different functions.

			bformata(glsl, "uniform %s", samplerPrecison);
			bcatcstr(glsl, samplerTypeName);
			bcatcstr(glsl, "Shadow ");
			TextureName(glsl, psContext->psShader, psDecl->asOperands[0].ui32RegisterNumber, MAX_RESOURCE_BINDINGS, 1);
			bcatcstr(glsl, ";\n");
		}

		bformata(glsl, "uniform %s", samplerPrecison);
		bcatcstr(glsl, samplerTypeName);
		bcatcstr(glsl, " ");
		TextureName(glsl, psContext->psShader, psDecl->asOperands[0].ui32RegisterNumber, MAX_RESOURCE_BINDINGS, 0);
		bcatcstr(glsl, ";\n");
	}
}

void TranslateDeclaration(HLSLCrossCompilerContext* psContext, const Declaration* psDecl)
{
	bstring glsl = *psContext->currentGLSLString;
	Shader* psShader = psContext->psShader;

	switch(psDecl->eOpcode)
	{
	case OPCODE_DCL_INPUT_SGV:
	case OPCODE_DCL_INPUT_PS_SGV:
		{
			const SPECIAL_NAME eSpecialName = psDecl->asOperands[0].eSpecialName;
			switch(eSpecialName)
			{
			case NAME_POSITION:
				{
					AddBuiltinInput(psContext, psDecl, "gl_Position", 4);
					break;
				}
			case NAME_RENDER_TARGET_ARRAY_INDEX:
				{
					AddBuiltinInput(psContext, psDecl, "gl_Layer", 1);
					break;
				}
			case NAME_CLIP_DISTANCE:
				{
					AddBuiltinInput(psContext, psDecl, "gl_ClipDistance", 4);
					break;
				}
			case NAME_VIEWPORT_ARRAY_INDEX:
				{
					AddBuiltinInput(psContext, psDecl, "gl_ViewportIndex", 1);
					break;
				}
			case NAME_INSTANCE_ID:
				{
					AddBuiltinInput(psContext, psDecl, "gl_InstanceID", 1);
					break;
				}
			case NAME_IS_FRONT_FACE:
				{
					AddBuiltinInput(psContext, psDecl, "gl_FrontFacing", 1);
					break;
				}
			case NAME_SAMPLE_INDEX:
				{
					AddBuiltinInput(psContext, psDecl, "gl_SampleID", 1);
					break;
				}
			case NAME_VERTEX_ID:
				{
					AddBuiltinInput(psContext, psDecl, "gl_VertexID", 1);
					break;
				}
			case NAME_PRIMITIVE_ID:
				{
					AddBuiltinInput(psContext, psDecl, "gl_PrimitiveID", 1);
					break;
				}
			default:
				{
					bformata(glsl, "in vec4 %s;\n", psDecl->asOperands[0].pszSpecialName);

					bcatcstr(glsl, "#define ");
					TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NONE);
					bformata(glsl, " %s\n", psDecl->asOperands[0].pszSpecialName);
					break;
				}
			}
			break;
		}

	case OPCODE_DCL_INPUT_PS_SIV:
		{
			switch(psDecl->asOperands[0].eSpecialName)
			{
			case NAME_POSITION:
				{
					AddBuiltinInput(psContext, psDecl, "gl_FragCoord", 4);
					break;
				}
			case NAME_RENDER_TARGET_ARRAY_INDEX:
				{
					AddBuiltinInput(psContext, psDecl, "gl_Layer", 1);
					break;
				}
			}
			break;
		}

	case OPCODE_DCL_OUTPUT_SIV:
		{
			switch(psDecl->asOperands[0].eSpecialName)
			{
			case NAME_POSITION:
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT4, 0, "gl_Position");
					break;
				}
			case NAME_RENDER_TARGET_ARRAY_INDEX:
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_INT, 0, "gl_Layer");
					break;
				}
			case NAME_CLIP_DISTANCE:
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 0, "gl_ClipDistance");
					break;
				}
			case NAME_VIEWPORT_ARRAY_INDEX:
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_INT, 0, "gl_ViewportIndex");
					break;
				}
			case NAME_VERTEX_ID:
				{
					ASSERT(0); //VertexID is not an output
					break;
				}
			case NAME_PRIMITIVE_ID:
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_INT, 0, "gl_PrimitiveID");
					break;
				}
			case NAME_INSTANCE_ID:
				{
					ASSERT(0); //InstanceID is not an output
					break;
				}
			case NAME_IS_FRONT_FACE:
				{
					ASSERT(0); //FrontFacing is not an output
					break;
				}
			case NAME_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR:
				{
					if(psContext->psShader->aIndexedOutput[psDecl->asOperands[0].ui32RegisterNumber])
					{
						AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 4, "gl_TessLevelOuter");
					}
					else
					{
						AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 0, "gl_TessLevelOuter[0]");
					}
					break;
				}
			case NAME_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR: 
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 0, "gl_TessLevelOuter[1]");
					break;
				}
			case NAME_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR: 
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 0, "gl_TessLevelOuter[2]");
					break;
				}
			case NAME_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR:
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 0, "gl_TessLevelOuter[3]");
					break;
				}
			case NAME_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR:
				{
					if(psContext->psShader->aIndexedOutput[psDecl->asOperands[0].ui32RegisterNumber])
					{
						AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 3,"gl_TessLevelOuter");
					}
					else
					{
						AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 0, "gl_TessLevelOuter[0]");
					}
					break;
				}
			case NAME_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR:
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 0, "gl_TessLevelOuter[1]");
					break;
				}
			case NAME_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR:
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 0, "gl_TessLevelOuter[2]");
					break;
				}
			case NAME_FINAL_LINE_DENSITY_TESSFACTOR:
				{
					if(psContext->psShader->aIndexedOutput[psDecl->asOperands[0].ui32RegisterNumber])
					{
						AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 2, "gl_TessLevelOuter");
					}
					else
					{
						AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 0, "gl_TessLevelOuter[0]");
					}
					break;
				}
			case NAME_FINAL_LINE_DETAIL_TESSFACTOR:
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 0, "gl_TessLevelOuter[1]");
					break;
				}
			case NAME_FINAL_TRI_INSIDE_TESSFACTOR:
			case NAME_FINAL_QUAD_U_INSIDE_TESSFACTOR:
				{
					if(psContext->psShader->aIndexedOutput[psDecl->asOperands[0].ui32RegisterNumber])
					{
						AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 2, "gl_TessLevelInner");
					}
					else
					{
						AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 0, "gl_TessLevelInner[0]");
					}
					break;
				}
			case NAME_FINAL_QUAD_V_INSIDE_TESSFACTOR:
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT, 0, "gl_TessLevelInner[1]");
					break;
				}
			default:
				{
					bformata(glsl, "out vec4 %s;\n", psDecl->asOperands[0].pszSpecialName);

					bcatcstr(glsl, "#define ");
					TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NONE);
					bformata(glsl, " %s\n", psDecl->asOperands[0].pszSpecialName);
					break;
				}
			}
			break;
		}
	case OPCODE_DCL_INPUT:
		{
			const Operand* psOperand = &psDecl->asOperands[0];
			int iNumComponents = 4;//GetMaxComponentFromComponentMask(psOperand);
			const char* StorageQualifier = "attribute";
			char* InputName;
			const char* Precision = "";

			if ((psOperand->eType == OPERAND_TYPE_INPUT_DOMAIN_POINT)||
				(psOperand->eType == OPERAND_TYPE_OUTPUT_CONTROL_POINT_ID)||
				(psOperand->eType == OPERAND_TYPE_INPUT_COVERAGE_MASK)||
				(psOperand->eType == OPERAND_TYPE_INPUT_THREAD_ID)||
				(psOperand->eType == OPERAND_TYPE_INPUT_THREAD_GROUP_ID)||
				(psOperand->eType == OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP)||
				(psOperand->eType == OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED) ||
				(psOperand->eType == OPERAND_TYPE_INPUT_FORK_INSTANCE_ID))
			{
				break;
			}

#if BOGUS
			// hs_fork_phase
			// dcl_input vicp[3][0].xyz

			// No need to declare patch constants read again by the hull shader.
			if ((psOperand->eType == OPERAND_TYPE_INPUT_PATCH_CONSTANT) && psContext->psShader->eShaderType == HULL_SHADER)
			{
				break;
			}
			// ...or control points
			if ((psOperand->eType == OPERAND_TYPE_INPUT_CONTROL_POINT) && psContext->psShader->eShaderType == HULL_SHADER)
			{
				break;
			}
#endif

			// Also skip position input to domain shader
			if ((psOperand->eType == OPERAND_TYPE_INPUT_CONTROL_POINT) && psContext->psShader->eShaderType == DOMAIN_SHADER)
			{
				InOutSignature* psSignature = NULL;
				GetInputSignatureFromRegister(psOperand->ui32RegisterNumber, &psContext->psShader->sInfo, &psSignature);

				if ((psSignature->eSystemValueType == NAME_POSITION && psSignature->ui32SemanticIndex == 0) ||
					(!strcmp(psSignature->SemanticName, "POS") && psSignature->ui32SemanticIndex == 0) ||
					(!strcmp(psSignature->SemanticName, "SV_POSITION") && psSignature->ui32SemanticIndex == 0))
				{
					break;
				}
			}
			else if (psOperand->eType == OPERAND_TYPE_INPUT_PRIMITIVEID)
			{
				// Always defined
			//	AddBuiltinPatchConstant(psContext, psDecl, "gl_PrimitiveID", 1);
				break;
			}
			else if (psOperand->eType == OPERAND_TYPE_INPUT_PATCH_CONSTANT)
			{
				InOutSignature* psSignature = NULL;
				GetPatchConstantSignatureFromRegister(psOperand->ui32RegisterNumber, psOperand->ui32CompMask, &psContext->psShader->sInfo, &psSignature);
				
				int unhandled = 1;

				const SPECIAL_NAME eSystemValueType = psSignature->eSystemValueType;
				switch(eSystemValueType)
				{
				case NAME_POSITION:
					{
						AddBuiltinInput(psContext, psDecl, "gl_Position", 4);
						break;
					}
				case NAME_RENDER_TARGET_ARRAY_INDEX:
					{
						AddBuiltinInput(psContext, psDecl, "gl_Layer", 1);
						break;
					}
				case NAME_CLIP_DISTANCE:
					{
						AddBuiltinInput(psContext, psDecl, "gl_ClipDistance", 4);
						break;
					}
				case NAME_VIEWPORT_ARRAY_INDEX:
					{
						AddBuiltinInput(psContext, psDecl, "gl_ViewportIndex", 1);
						break;
					}
				case NAME_VERTEX_ID:
					{
						AddBuiltinInput(psContext, psDecl, "gl_VertexID", 1);
						break;
					}
				case NAME_INSTANCE_ID:
					{
						AddBuiltinInput(psContext, psDecl, "gl_InstanceID", 1);
						break;
					}
				case NAME_IS_FRONT_FACE:
					{
						AddBuiltinInput(psContext, psDecl, "gl_FrontFacing", 1);
						break;
					}
				case NAME_PRIMITIVE_ID:
					{
						AddBuiltinInput(psContext, psDecl, "gl_PrimitiveID", 1);
						break;
					}
				case NAME_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR:
				case NAME_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR:
				case NAME_FINAL_LINE_DENSITY_TESSFACTOR:
					{
						if (psContext->psShader->aIndexedOutput[psDecl->asOperands[0].ui32RegisterNumber])
							AddBuiltinInput(psContext, psDecl, "gl_TessLevelOuter", 4);
						else
							AddBuiltinInput(psContext, psDecl, "gl_TessLevelOuter[0]", 1);
						break;
					}
				case NAME_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR:
				case NAME_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR:
				case NAME_FINAL_LINE_DETAIL_TESSFACTOR:
					{
						AddBuiltinInput(psContext, psDecl, "gl_TessLevelOuter[1]", 1);
						break;
					}
				case NAME_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR:
				case NAME_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR:
					{
						AddBuiltinInput(psContext, psDecl, "gl_TessLevelOuter[2]", 1);
						break;
					}
				case NAME_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR:
					{
						AddBuiltinInput(psContext, psDecl, "gl_TessLevelOuter[3]", 1);
						break;
					}
				case NAME_FINAL_TRI_INSIDE_TESSFACTOR:
				case NAME_FINAL_QUAD_U_INSIDE_TESSFACTOR:
					{
						if (psContext->psShader->aIndexedOutput[psDecl->asOperands[0].ui32RegisterNumber])
							AddBuiltinInput(psContext, psDecl, "gl_TessLevelInner", 4);
						else
							AddBuiltinInput(psContext, psDecl, "gl_TessLevelInner", 1);
						break;
					}
				case NAME_FINAL_QUAD_V_INSIDE_TESSFACTOR:
					{
						AddBuiltinInput(psContext, psDecl, "gl_TessLevelInner[1]", 1);
						break;
					}
				default:
					{
						unhandled = 0;
					}
				}

				if (unhandled)
				{
					break;
				}
			}

			// Already declared as part of an array.
			if(psShader->aIndexedInput[psDecl->asOperands[0].ui32RegisterNumber] == -1)
			{
				break;
			}

			if (!IsPatchConstant(psContext, psOperand) /*psOperand->eType != OPERAND_TYPE_INPUT_PATCH_CONSTANT*/)
				InputName = GetDeclaredInputName(psContext, psShader->eShaderType, psOperand);
			else
				InputName = GetDeclaredPatchConstantName(psContext, psShader->eShaderType, psOperand);

			if (InOutSupported(psContext->psShader->eTargetLanguage))
			{
				StorageQualifier = "in";
			}

			if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
			{
				Precision = GetPrecisionAttributeFromOperand(psOperand->eMinPrecision);
			}

			if (!IsPatchConstant(psContext, psOperand) /*psOperand->eType != OPERAND_TYPE_INPUT_PATCH_CONSTANT*/)
				DeclareInput(psContext, psDecl, "", StorageQualifier, Precision, iNumComponents, (OPERAND_INDEX_DIMENSION)psOperand->iIndexDims, InputName, psOperand->ui32CompMask);
			else
				DeclareInputPatchConstant(psContext, psDecl, "", StorageQualifier, Precision, iNumComponents, (OPERAND_INDEX_DIMENSION)psOperand->iIndexDims, InputName, psOperand->ui32CompMask);

			bcstrfree(InputName);

			break;
		}
	case OPCODE_DCL_OUTPUT:
		{
			const Operand* psOperand = &psDecl->asOperands[0];
			int iNumComponents = 4;//GetMaxComponentFromComponentMask(psOperand);
			const char* StorageQualifier = "attribute";
			char* OutputName;
			const char* Precision = "";
			int stream = 0;

			if ((psOperand->eType == OPERAND_TYPE_OUTPUT_DEPTH) ||
				(psOperand->eType == OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL) ||
				(psOperand->eType == OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL) ||
				(psOperand->eType == OPERAND_TYPE_OUTPUT_CONTROL_POINT_ID))
			{
				break;
			}

			// Also skip position input to domain shader
			{
				InOutSignature* psSignature = NULL;
				int foundOutput = GetOutputSignatureFromRegister(
					psContext->currentPhase,
					psOperand->ui32RegisterNumber,
					psOperand->ui32CompMask,
					psContext->psShader->ui32CurrentVertexOutputStream,
					&psContext->psShader->sInfo,
					&psSignature);

				if ((psSignature->eSystemValueType == NAME_POSITION && psSignature->ui32SemanticIndex == 0) ||
					(!strcmp(psSignature->SemanticName, "POS") && psSignature->ui32SemanticIndex == 0) ||
					(!strcmp(psSignature->SemanticName, "SV_POSITION") && psSignature->ui32SemanticIndex == 0))
				{
					AddBuiltinOutput(psContext, psDecl, GLVARTYPE_FLOAT4, 0, "gl_out[gl_InvocationID].gl_Position");
					break;
				}
			}
		//	if (psOperand->eType == OPERAND_TYPE_INPUT_PATCH_CONSTANT)
			{
				if (IsPatchConstant(psContext, psOperand) /*psOperand->eType != OPERAND_TYPE_INPUT_PATCH_CONSTANT*/)
				{
					InOutSignature* psSignature = NULL;
					GetPatchConstantSignatureFromRegister(psOperand->ui32RegisterNumber, psOperand->ui32CompMask, &psContext->psShader->sInfo, &psSignature);
				
					int unhandled = 1;

					const SPECIAL_NAME eSystemValueType = psSignature->eSystemValueType;
					switch(eSystemValueType)
					{
						// They are all in OUTPUT_SIV
					default:
						{
							unhandled = 0;
						}
					}

					if (unhandled)
					{
						break;
					}
				}

				if (!IsPatchConstant(psContext, psOperand) /*psOperand->eType != OPERAND_TYPE_INPUT_PATCH_CONSTANT*/)
					OutputName = GetDeclaredOutputName(psContext, psShader->eShaderType, psOperand, &stream);
				else
					OutputName = GetDeclaredPatchConstantName(psContext, psShader->eShaderType, psOperand);

				if (InOutSupported(psContext->psShader->eTargetLanguage))
				{
					StorageQualifier = "out";
				}

				if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
				{
					Precision = GetPrecisionAttributeFromOperand(psOperand->eMinPrecision);
				}

				if (!IsPatchConstant(psContext, psOperand) /*psOperand->eType != OPERAND_TYPE_INPUT_PATCH_CONSTANT*/)
					DeclareOutput(psContext, psDecl);
				else
					DeclareOutputPatchConstant(psContext, psDecl);//, "", StorageQualifier, Precision, iNumComponents, (OPERAND_INDEX_DIMENSION)psOperand->iIndexDims, OutputName, psOperand->ui32CompMask);
			}
			break;
		}
	case OPCODE_DCL_INPUT_SIV:
		{
            if(psShader->eShaderType == PIXEL_SHADER && psContext->psDependencies)
			{
                psContext->psDependencies->aePixelInputInterpolation[psDecl->asOperands[0].ui32RegisterNumber] = psDecl->value.eInterpolation;
			}
			break;
		}
	case OPCODE_DCL_INPUT_PS:
		{
			const Operand* psOperand = &psDecl->asOperands[0];
			int iNumComponents = 4;//GetMaxComponentFromComponentMask(psOperand);
			const char* StorageQualifier = "varying";
			const char* Precision = "";
			char* InputName = GetDeclaredInputName(psContext, PIXEL_SHADER, psOperand);
			const char* Interpolation = "";

			//Already declared as part of an array.
			if (psShader->aIndexedInput[psDecl->asOperands[0].ui32RegisterNumber] == -1)
			{
				break;
			}

			if (InOutSupported(psContext->psShader->eTargetLanguage))
			{
				StorageQualifier = "in";
			}

			switch(psDecl->value.eInterpolation)
			{
			case INTERPOLATION_CONSTANT:
				{
					Interpolation = "flat";
					break;
				}
			case INTERPOLATION_LINEAR:
				{
					break;
				}
			case INTERPOLATION_LINEAR_CENTROID:
				{
					Interpolation = "centroid";
					break;
				}
			case INTERPOLATION_LINEAR_NOPERSPECTIVE:
				{
					Interpolation = "noperspective";
					break;
				}
			case INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID:
				{
					Interpolation = "noperspective centroid";
					break;
				}
			case INTERPOLATION_LINEAR_SAMPLE:
				{
					Interpolation = "sample";
					break;
				}
			case INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE:
				{
					Interpolation = "noperspective sample";
					break;
				}
			}

			if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
			{
				Precision = GetPrecisionAttributeFromOperand(psOperand->eMinPrecision);
			}

			DeclareInput(psContext, psDecl, Interpolation, StorageQualifier, Precision, iNumComponents, INDEX_1D, InputName, psOperand->ui32CompMask);
			bcstrfree(InputName);

			break;
		}
	case OPCODE_DCL_TEMPS:
		{
			const int usePrec = psContext->flags & HLSLCC_FLAG_FORCE_PRECISION_QUALIFIERS;

			uint32_t i = 0; 
			const uint32_t ui32NumTemps = psDecl->value.ui32NumTemps;

			if (psContext->flags & HLSLCC_FLAG_AVOID_TEMP_REGISTER_ALIASING && psContext->psShader->eShaderType != HULL_SHADER)
				break;

			if(ui32NumTemps > 0)
			{
				bformata(glsl, "%s Temp_%s[%d];\n", GetConstructorForType(SVT_FLOAT, 4, usePrec), GetConstructorForType(SVT_FLOAT, 1, 0), ui32NumTemps);
				bformata(glsl, "%s Temp_%s[%d];\n", GetConstructorForType(SVT_INT, 4, usePrec), GetConstructorForType(SVT_INT, 1, 0), ui32NumTemps);
				bformata(glsl, "%s Temp_%s[%d];\n", GetConstructorForType(SVT_BOOL, 4, usePrec), GetConstructorForType(SVT_BOOL, 1, 0), ui32NumTemps);
				if (psContext->psShader->bUseTempCopy)
				{
					bformata(glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_FLOAT, 4, usePrec), GetConstructorForType(SVT_FLOAT, 1, 0));
					bformata(glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_INT, 4, usePrec), GetConstructorForType(SVT_INT, 1, 0));
					bformata(glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_BOOL, 4, usePrec), GetConstructorForType(SVT_BOOL, 1, 0));
				}

				if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
				{
					bformata(glsl, "%s Temp_m%s[%d];\n", GetConstructorForType(SVT_INT16, 4, usePrec), GetConstructorForType(SVT_INT, 1, 0), ui32NumTemps);
					bformata(glsl, "%s Temp_l%s[%d];\n", GetConstructorForType(SVT_INT12, 4, usePrec), GetConstructorForType(SVT_INT, 1, 0), ui32NumTemps);
					bformata(glsl, "%s Temp_m%s[%d];\n", GetConstructorForType(SVT_FLOAT16, 4, usePrec), GetConstructorForType(SVT_FLOAT, 1, 0), ui32NumTemps);
					bformata(glsl, "%s Temp_l%s[%d];\n", GetConstructorForType(SVT_FLOAT10, 4, usePrec), GetConstructorForType(SVT_FLOAT, 1, 0), ui32NumTemps);
					if (psContext->psShader->bUseTempCopy)
					{
						bformata(glsl, "%s TempCopy_m%s;\n", GetConstructorForType(SVT_INT16, 4, usePrec), GetConstructorForType(SVT_INT, 1, 0));
						bformata(glsl, "%s TempCopy_l%s;\n", GetConstructorForType(SVT_INT12, 4, usePrec), GetConstructorForType(SVT_INT, 1, 0));
						bformata(glsl, "%s TempCopy_m%s;\n", GetConstructorForType(SVT_FLOAT16, 4, usePrec), GetConstructorForType(SVT_FLOAT, 1, 0));
						bformata(glsl, "%s TempCopy_l%s;\n", GetConstructorForType(SVT_FLOAT10, 4, usePrec), GetConstructorForType(SVT_FLOAT, 1, 0));
					}
				}

				if (HaveUVec(psShader->eTargetLanguage))
				{
					bformata(glsl, "%s Temp_%s[%d];\n", GetConstructorForType(SVT_UINT, 4, usePrec), GetConstructorForType(SVT_UINT, 1, 0), ui32NumTemps);
					if (psContext->psShader->bUseTempCopy)
						bformata(glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_UINT, 4, usePrec), GetConstructorForType(SVT_UINT, 1, 0));

					if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
					{
						bformata(glsl, "%s Temp_m%s[%d];\n", GetConstructorForType(SVT_UINT16, 4, usePrec), GetConstructorForType(SVT_UINT, 1, 0), ui32NumTemps);
						if (psContext->psShader->bUseTempCopy)
							bformata(glsl, "%s TempCopy_m%s;\n", GetConstructorForType(SVT_UINT16, 4, usePrec), GetConstructorForType(SVT_UINT, 1, 0));
					}
				}

				if (psShader->fp64)
				{
					bformata(glsl, "%s Temp_%s[%d];\n", GetConstructorForType(SVT_DOUBLE, 4, usePrec), GetConstructorForType(SVT_DOUBLE, 1, 0), ui32NumTemps);
					if (psContext->psShader->bUseTempCopy)
						bformata(glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_DOUBLE, 4, usePrec), GetConstructorForType(SVT_DOUBLE, 1, 0));
				}
			}

			break;
		}
	case OPCODE_SPECIAL_DCL_IMMCONST:
		{
			const Operand* psDest = &psDecl->asOperands[0];
			const Operand* psSrc = &psDecl->asOperands[1];

			ASSERT(psSrc->eType == OPERAND_TYPE_IMMEDIATE32);
			if(psDest->eType == OPERAND_TYPE_SPECIAL_IMMCONSTINT)
			{
				bformata(glsl, "const ivec4 IntImmConst%d = ", psDest->ui32RegisterNumber);
			}
			else
			{
				bformata(glsl, "const vec4 ImmConst%d = ", psDest->ui32RegisterNumber);
				AddToDx9ImmConstIndexableArray(psContext, psDest);
			}
			TranslateOperand(psContext, psSrc, psDest->eType == OPERAND_TYPE_SPECIAL_IMMCONSTINT ? TO_FLAG_INTEGER : TO_AUTO_BITCAST_TO_FLOAT);
			bcatcstr(glsl, ";\n");

			break;
		}
	case OPCODE_DCL_CONSTANT_BUFFER:
		{
			const Operand* psOperand = &psDecl->asOperands[0];
			const uint32_t ui32BindingPoint = psOperand->aui32ArraySizes[0];
			ConstantBuffer* psCBuf = NULL;

			GetConstantBufferFromBindingPoint(RGROUP_CBUFFER, ui32BindingPoint, &psContext->psShader->sInfo, &psCBuf);

			if (psCBuf)
			{
				// Constant buffers declared as "dynamicIndexed" are declared as raw vec4 arrays, as there is no general way to retrieve the member corresponding to a dynamic index.
				// Simple cases can probably be handled easily, but for example when arrays (possibly nested with structs) are contained in the constant buffer and the shader reads
				// from a dynamic index we would need to "undo" the operations done in order to compute the variable offset, and such a feature is not available at the moment.
				psCBuf->blob = psDecl->value.eCBAccessPattern == CONSTANT_BUFFER_ACCESS_PATTERN_DYNAMICINDEXED;

				// TheoM: disabled for now as it causes name mangling of constant buffer variables,
				// which in turn makes them inaccessible from the renderer.
				psCBuf->blob = 0;

			}

			// We don't have a original resource name, maybe generate one???
			if(!psCBuf)
			{
				if (HaveUniformBindingsAndLocations(psContext->psShader->eTargetLanguage, psContext->psShader->extensions, psContext->flags) && (psContext->flags & HLSLCC_FLAG_AVOID_RESOURCE_BINDINGS_AND_LOCATIONS) == 0)
					// Constant buffer locations start at 0.
					bformata(glsl, "layout(std140, binding = %d) ", ui32BindingPoint);
				else
					bformata(glsl, "layout(std140) ");

				bformata(glsl, "uniform ConstantBuffer%d {\n\tvec4 data[%d];\n} cb%d;\n", ui32BindingPoint, psOperand->aui32ArraySizes[1], ui32BindingPoint);
				break;
			}
			else if (psCBuf->blob)
			{
				if (HaveUniformBindingsAndLocations(psContext->psShader->eTargetLanguage, psContext->psShader->extensions, psContext->flags) && (psContext->flags & HLSLCC_FLAG_AVOID_RESOURCE_BINDINGS_AND_LOCATIONS) == 0)
					// Constant buffer locations start at 0.
					bformata(glsl, "layout(std140, binding = %d) ", ui32BindingPoint);
				else
					bformata(glsl, "layout(std140) ");

				bcatcstr(glsl, "uniform ");
				ConvertToUniformBufferName(glsl, psShader, psCBuf->Name, ui32BindingPoint);
				bcatcstr(glsl, " {\n\tvec4 ");
				ConvertToUniformBufferName(glsl, psShader, psCBuf->Name, ui32BindingPoint);
				bformata(glsl, "_data[%d];\n};\n", psOperand->aui32ArraySizes[1]);
				break;
			}

			if (psContext->flags & HLSLCC_FLAG_UNIFORM_BUFFER_OBJECT)
			{
				if (psContext->flags & HLSLCC_FLAG_GLOBAL_CONSTS_NEVER_IN_UBO && psCBuf->Name[0] == '$')
				{
					DeclareStructConstants(psContext, ui32BindingPoint, psCBuf, psOperand);
				}
				else
				{
					DeclareUBOConstants(psContext, ui32BindingPoint, psCBuf);
				}
			}
			else
			{
				DeclareStructConstants(psContext, ui32BindingPoint, psCBuf, psOperand);
			}
			break;
		}
	case OPCODE_DCL_RESOURCE:
		{
			if (HaveUniformBindingsAndLocations(psContext->psShader->eTargetLanguage, psContext->psShader->extensions, psContext->flags) && (psContext->flags & HLSLCC_FLAG_AVOID_RESOURCE_BINDINGS_AND_LOCATIONS) == 0)
			{
				// Explicit layout bindings are not currently compatible with combined texture samplers. The layout below assumes there is exactly one GLSL sampler
				// for each HLSL texture declaration, but when combining textures+samplers, there can be multiple OGL samplers for each HLSL texture declaration.
				// if (!(psContext->flags & HLSLCC_FLAG_COMBINE_TEXTURE_SAMPLERS))
					// Constant buffer locations start at 0. Resource locations start at ui32NumConstantBuffers.
					bformata(glsl, "layout(binding = %d) ", psContext->psShader->sInfo.ui32NumConstantBuffers + psDecl->asOperands[0].ui32RegisterNumber);
			}

			switch(psDecl->value.eResourceDimension)
			{
			case RESOURCE_DIMENSION_BUFFER:
				{
					bformata(glsl, "uniform %s ", GetSamplerType(psContext, RESOURCE_DIMENSION_BUFFER, psDecl->asOperands[0].ui32RegisterNumber));
					TextureName(*psContext->currentGLSLString, psContext->psShader, psDecl->asOperands[0].ui32RegisterNumber, MAX_RESOURCE_BINDINGS, 0);
					bcatcstr(glsl, ";\n");
					break;
				}
			case RESOURCE_DIMENSION_TEXTURE1D:
				{
					TranslateResourceTexture(psContext, psDecl, 1);
					break;
				}
			case RESOURCE_DIMENSION_TEXTURE2D:
				{
					TranslateResourceTexture(psContext, psDecl, 1);
					break;
				}
			case RESOURCE_DIMENSION_TEXTURE2DMS:
				{
					TranslateResourceTexture(psContext, psDecl, 0);
					break;
				}
			case RESOURCE_DIMENSION_TEXTURE3D:
				{
					TranslateResourceTexture(psContext, psDecl, 0);
					break;
				}
			case RESOURCE_DIMENSION_TEXTURECUBE:
				{
					TranslateResourceTexture(psContext, psDecl, 1);
					break;
				}
			case RESOURCE_DIMENSION_TEXTURE1DARRAY:
				{
					TranslateResourceTexture(psContext, psDecl, 1);
					break;
				}
			case RESOURCE_DIMENSION_TEXTURE2DARRAY:
				{
					TranslateResourceTexture(psContext, psDecl, 1);
					break;
				}
			case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
				{
					TranslateResourceTexture(psContext, psDecl, 0);
					break;
				}
			case RESOURCE_DIMENSION_TEXTURECUBEARRAY:
				{
					TranslateResourceTexture(psContext, psDecl, 1);
					break;
				}
			}

			ASSERT(psDecl->asOperands[0].ui32RegisterNumber < MAX_TEXTURES);
			psShader->aeResourceDims[psDecl->asOperands[0].ui32RegisterNumber] = psDecl->value.eResourceDimension;
			break;
		}
	case OPCODE_DCL_GLOBAL_FLAGS:
		{
			uint32_t ui32Flags = psDecl->value.ui32GlobalFlags;

			if(ui32Flags & GLOBAL_FLAG_FORCE_EARLY_DEPTH_STENCIL)
			{
				bcatcstr(glsl, "layout(early_fragment_tests) in;\n");
			}
			if(!(ui32Flags & GLOBAL_FLAG_REFACTORING_ALLOWED))
			{
				//TODO add precise
				//HLSL precise - http://msdn.microsoft.com/en-us/library/windows/desktop/hh447204(v=vs.85).aspx
			}
			if(ui32Flags & GLOBAL_FLAG_ENABLE_DOUBLE_PRECISION_FLOAT_OPS)
			{
				bcatcstr(glsl, "#extension GL_ARB_gpu_shader_fp64 : enable\n");
				psShader->fp64 = 1;
			}
			break;
		}

	case OPCODE_DCL_THREAD_GROUP:
		{
			bformata(glsl, "layout(local_size_x = %d, local_size_y = %d, local_size_z = %d) in;\n",
				psDecl->value.aui32WorkGroupSize[0],
				psDecl->value.aui32WorkGroupSize[1],
				psDecl->value.aui32WorkGroupSize[2]);
			break;
		}
	case OPCODE_DCL_TESS_OUTPUT_PRIMITIVE:
		{
			if(psContext->psShader->eShaderType == HULL_SHADER)
			{
				psContext->psShader->sInfo.eTessOutPrim = psDecl->value.eTessOutPrim;
			}
			break;
		}
	case OPCODE_DCL_TESS_DOMAIN:
		{
			if(psContext->psShader->eShaderType == DOMAIN_SHADER)
			{
				switch(psDecl->value.eTessDomain)
				{
				case TESSELLATOR_DOMAIN_ISOLINE:
					{
						bcatcstr(glsl, "layout(isolines) in;\n");
						break;
					}
				case TESSELLATOR_DOMAIN_TRI:
					{
						bcatcstr(glsl, "layout(triangles) in;\n");
						break;
					}
				case TESSELLATOR_DOMAIN_QUAD:
					{
						bcatcstr(glsl, "layout(quads) in;\n");
						break;
					}
				default:
					{
						break;
					}
				}
			}
			break;
		}
	case OPCODE_DCL_TESS_PARTITIONING:
		{
			if(psContext->psShader->eShaderType == HULL_SHADER)
			{
				psContext->psShader->sInfo.eTessPartitioning = psDecl->value.eTessPartitioning;
			}
			break;
		}
	case OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY:
		{
			switch(psDecl->value.eOutputPrimitiveTopology)
			{
			case PRIMITIVE_TOPOLOGY_POINTLIST:
				{
					bcatcstr(glsl, "layout(points) out;\n");
					break;
				}
			case PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
			case PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
			case PRIMITIVE_TOPOLOGY_LINELIST:
			case PRIMITIVE_TOPOLOGY_LINESTRIP:
				{
					bcatcstr(glsl, "layout(line_strip) out;\n");
					break;
				}

			case PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
			case PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
			case PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
			case PRIMITIVE_TOPOLOGY_TRIANGLELIST:
				{
					bcatcstr(glsl, "layout(triangle_strip) out;\n");
					break;
				}
			default:
				{
					break;
				}
			}
			break;
		}
	case OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT:
		{
			bformata(glsl, "layout(max_vertices = %d) out;\n", psDecl->value.ui32MaxOutputVertexCount);
			break;
		}
	case OPCODE_DCL_GS_INPUT_PRIMITIVE:
		{
			switch(psDecl->value.eInputPrimitive)
			{
			case PRIMITIVE_POINT:
				{
					bcatcstr(glsl, "layout(points) in;\n");
					break;
				}
			case PRIMITIVE_LINE:
				{
					bcatcstr(glsl, "layout(lines) in;\n");
					break;
				}
			case PRIMITIVE_LINE_ADJ:
				{
					bcatcstr(glsl, "layout(lines_adjacency) in;\n");
					break;
				}
			case PRIMITIVE_TRIANGLE:
				{
					bcatcstr(glsl, "layout(triangles) in;\n");
					break;
				}
			case PRIMITIVE_TRIANGLE_ADJ:
				{
					bcatcstr(glsl, "layout(triangles_adjacency) in;\n");
					break;
				}
			default:
				{
					break;
				}
			}
			break;
		}
	case OPCODE_DCL_INTERFACE:
		{
			const uint32_t interfaceID = psDecl->value.interface.ui32InterfaceID;
			const uint32_t numUniforms = psDecl->value.interface.ui32ArraySize;
			const uint32_t ui32NumBodiesPerTable = psContext->psShader->funcPointer[interfaceID].ui32NumBodiesPerTable;
			ShaderVar* psVar;
			uint32_t varFound;

			const char* uniformName;

			varFound = GetInterfaceVarFromOffset(interfaceID, &psContext->psShader->sInfo, &psVar);
			ASSERT(varFound);
			uniformName = &psVar->Name[0];

			bformata(glsl, "subroutine uniform SubroutineType %s[%d*%d];\n", uniformName, numUniforms, ui32NumBodiesPerTable);
			break;
		}
	case OPCODE_DCL_FUNCTION_BODY:
		{
			//bformata(glsl, "void Func%d();//%d\n", psDecl->asOperands[0].ui32RegisterNumber, psDecl->asOperands[0].eType);
			break;
		}
	case OPCODE_DCL_FUNCTION_TABLE:
		{
			break;
		}
	case OPCODE_CUSTOMDATA:
		{
			const uint32_t ui32NumVec4 = psDecl->ui32NumOperands;
			const uint32_t ui32NumVec4Minus1 = (ui32NumVec4-1);
			uint32_t ui32ConstIndex = 0;
			float x, y, z, w;

			//If ShaderBitEncodingSupported then 1 integer buffer, use intBitsToFloat to get float values. - More instructions.
			//else 2 buffers - one integer and one float. - More data

			if(ShaderBitEncodingSupported(psShader->eTargetLanguage) == 0)
			{
				bcatcstr(glsl, "#define immediateConstBufferI(idx) immediateConstBufferInt[idx]\n");
				bcatcstr(glsl, "#define immediateConstBufferF(idx) immediateConstBuffer[idx]\n");

				bformata(glsl, "vec4 immediateConstBuffer[%d] = vec4[%d] (\n", ui32NumVec4, ui32NumVec4);
				for(;ui32ConstIndex < ui32NumVec4Minus1; ui32ConstIndex++)
				{
					float loopLocalX, loopLocalY, loopLocalZ, loopLocalW;
					loopLocalX = *(float*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].a;
					loopLocalY = *(float*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].b;
					loopLocalZ = *(float*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].c;
					loopLocalW = *(float*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].d;

					//A single vec4 can mix integer and float types.
					//Forced NAN and INF to zero inside the immediate constant buffer. This will allow the shader to compile.
					if(fpcheck(loopLocalX))
					{
						loopLocalX = 0;
					}
					if(fpcheck(loopLocalY))
					{
						loopLocalY = 0;
					}
					if(fpcheck(loopLocalZ))
					{
						loopLocalZ = 0;
					}
					if(fpcheck(loopLocalW))
					{
						loopLocalW = 0;
					}

					bformata(glsl, "\tvec4(%e, %e, %e, %e), \n", loopLocalX, loopLocalY, loopLocalZ, loopLocalW);
				}
				//No trailing comma on this one
				x = *(float*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].a;
				y = *(float*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].b;
				z = *(float*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].c;
				w = *(float*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].d;
				if(fpcheck(x))
				{
					x = 0;
				}
				if(fpcheck(y))
				{
					y = 0;
				}
				if(fpcheck(z))
				{
					z = 0;
				}
				if(fpcheck(w))
				{
					w = 0;
				}
				bformata(glsl, "\tvec4(%e, %e, %e, %e)\n", x, y, z, w);
				bcatcstr(glsl, ");\n");
			}
			else
			{
				bcatcstr(glsl, "#define immediateConstBufferI(idx) immediateConstBufferInt[idx]\n");
				bcatcstr(glsl, "#define immediateConstBufferF(idx) intBitsToFloat(immediateConstBufferInt[idx])\n");
			}

			{
				uint32_t ui32ConstIndex = 0;
				int x, y, z, w;

				bformata(glsl, "ivec4 immediateConstBufferInt[%d] = ivec4[%d] (\n", ui32NumVec4, ui32NumVec4);
				for(;ui32ConstIndex < ui32NumVec4Minus1; ui32ConstIndex++)
				{
					int loopLocalX, loopLocalY, loopLocalZ, loopLocalW;
					loopLocalX = *(int*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].a;
					loopLocalY = *(int*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].b;
					loopLocalZ = *(int*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].c;
					loopLocalW = *(int*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].d;

					bformata(glsl, "\tivec4(%d, %d, %d, %d), \n", loopLocalX, loopLocalY, loopLocalZ, loopLocalW);
				}
				//No trailing comma on this one
				x = *(int*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].a;
				y = *(int*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].b;
				z = *(int*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].c;
				w = *(int*)&psDecl->asImmediateConstBuffer[ui32ConstIndex].d;

				bformata(glsl, "\tivec4(%d, %d, %d, %d)\n", x, y, z, w);
				bcatcstr(glsl, ");\n");
			}

			break;
		}
	case OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT:
		{
			const uint32_t forkPhaseNum = psDecl->value.aui32HullPhaseInstanceInfo[0];
			const uint32_t instanceCount = psDecl->value.aui32HullPhaseInstanceInfo[1];
			bformata(glsl, "const int HullPhase%dInstanceCount = %d;\n", forkPhaseNum, instanceCount);
			break;
		}
	case OPCODE_DCL_INDEXABLE_TEMP:
		{
			const int usePrec = psContext->flags & HLSLCC_FLAG_FORCE_PRECISION_QUALIFIERS;

			const uint32_t ui32RegIndex = psDecl->sIdxTemp.ui32RegIndex;
			const uint32_t ui32RegCount = psDecl->sIdxTemp.ui32RegCount;
			const uint32_t ui32RegComponentSize = psDecl->sIdxTemp.ui32RegComponentSize;

			AddIndentation(psContext);
			bformata(psContext->glsl, "%s TempArray%d_%s[%d];\n", GetConstructorForType(SVT_FLOAT, ui32RegComponentSize, usePrec), ui32RegIndex, GetConstructorForType(SVT_FLOAT, 1, 0), ui32RegCount);

			if (psContext->flags & HLSLCC_FLAG_AVOID_TEMP_REGISTER_ALIASING)
			{
				AddIndentation(psContext);
				bformata(psContext->glsl, "%s TempArray%d_%s[%d];\n", GetConstructorForType(SVT_INT, ui32RegComponentSize, usePrec), ui32RegIndex, GetConstructorForType(SVT_INT, 1, 0), ui32RegCount);
				AddIndentation(psContext);
				bformata(psContext->glsl, "%s TempArray%d_%s[%d];\n", GetConstructorForType(SVT_BOOL, ui32RegComponentSize, usePrec), ui32RegIndex, GetConstructorForType(SVT_BOOL, 1, 0), ui32RegCount);

				if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
				{
					AddIndentation(psContext);
					bformata(psContext->glsl, "%s TempArray%d_m%s[%d];\n", GetConstructorForType(SVT_INT16, ui32RegComponentSize, usePrec), ui32RegIndex, GetConstructorForType(SVT_INT, 1, 0), ui32RegCount);
					AddIndentation(psContext);
					bformata(psContext->glsl, "%s TempArray%d_l%s[%d];\n", GetConstructorForType(SVT_INT12, ui32RegComponentSize, usePrec), ui32RegIndex, GetConstructorForType(SVT_INT, 1, 0), ui32RegCount);
					AddIndentation(psContext);
					bformata(psContext->glsl, "%s TempArray%d_m%s[%d];\n", GetConstructorForType(SVT_FLOAT16, ui32RegComponentSize, usePrec), ui32RegIndex, GetConstructorForType(SVT_FLOAT, 1, 0), ui32RegCount);
					AddIndentation(psContext);		
					bformata(psContext->glsl, "%s TempArray%d_l%s[%d];\n", GetConstructorForType(SVT_FLOAT10, ui32RegComponentSize, usePrec), ui32RegIndex, GetConstructorForType(SVT_FLOAT, 1, 0), ui32RegCount);
				}
		
				if (HaveUVec(psShader->eTargetLanguage))
				{
					AddIndentation(psContext);
					bformata(psContext->glsl, "%s TempArray%d_%s[%d];\n", GetConstructorForType(SVT_UINT, ui32RegComponentSize, usePrec), ui32RegIndex, GetConstructorForType(SVT_UINT, 1, 0), ui32RegCount);

					if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
					{
						AddIndentation(psContext);				
						bformata(psContext->glsl, "%s TempArray%d_m%s[%d];\n", GetConstructorForType(SVT_UINT16, ui32RegComponentSize, usePrec), ui32RegIndex, GetConstructorForType(SVT_UINT, 1, 0), ui32RegCount);
					}
				}
		
				if (psShader->fp64)
				{
					AddIndentation(psContext);
					bformata(psContext->glsl, "%s TempArray%d_%s[%d];\n", GetConstructorForType(SVT_DOUBLE, ui32RegComponentSize, usePrec), ui32RegIndex, GetConstructorForType(SVT_DOUBLE, 1, 0), ui32RegCount);
				}
			}
			break;
		}
	case OPCODE_DCL_INDEX_RANGE:
		{
			break;
		}
	case OPCODE_HS_DECLS:
		{
			break;
		}
	case OPCODE_DCL_INPUT_CONTROL_POINT_COUNT:
		{
			break;
		}
	case OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT:
		{
			if(psContext->psShader->eShaderType == HULL_SHADER)
			{
				bformata(glsl, "layout(vertices=%d) out;\n", psDecl->value.ui32MaxOutputVertexCount);
			}
			break;
		}
	case OPCODE_HS_FORK_PHASE:
		{
			break;
		}
	case OPCODE_HS_JOIN_PHASE:
		{
			break;
		}
	case OPCODE_DCL_SAMPLER:
		{
			break;
		}
	case OPCODE_DCL_HS_MAX_TESSFACTOR:
		{
			//For GLSL the max tessellation factor is fixed to the value of gl_MaxTessGenLevel. 
			break;
		}
	case OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED:
		{
		//	const uint32_t ui32BindingPoint = psDecl->asOperands[0].ui32RegisterNumber;
			int op;

			if (HaveUniformBindingsAndLocations(psContext->psShader->eTargetLanguage, psContext->psShader->extensions, psContext->flags) && (psContext->flags & HLSLCC_FLAG_AVOID_RESOURCE_BINDINGS_AND_LOCATIONS) == 0)
			{
				// Constant buffer locations start at 0. UAV locations start at ui32NumConstantBuffers. UAV locations start at ui32NumConstantBuffers + ui32NumSamplers.
				bformata(glsl, "layout(binding = %d) ", psContext->psShader->sInfo.ui32NumConstantBuffers + psContext->psShader->sInfo.ui32NumSamplers + psDecl->asOperands[0].ui32RegisterNumber);
			}

			if (psDecl->sUAV.ui32GloballyCoherentAccess & GLOBALLY_COHERENT_ACCESS)
			{
				bcatcstr(glsl, "coherent ");
			}

			// NOTE: this is really irrelevant, without checking for each declared resource _individually_ instead of for the whole shader
			for (op = OPCODE_ATOMIC_AND; (op <= OPCODE_IMM_ATOMIC_UMIN) && !psShader->aiOpcodeUsed[op]; ++op);

			if (psShader->aiOpcodeUsed[OPCODE_LD_UAV_TYPED] == 0)
			{
				if (op >= OPCODE_IMM_ATOMIC_UMIN)
					bcatcstr(glsl, "writeonly ");
				else
					bcatcstr(glsl, "restrict ");
			}
			else if (psShader->aiOpcodeUsed[OPCODE_STORE_UAV_TYPED] == 0)
			{
				if (op >= OPCODE_IMM_ATOMIC_UMIN)
					bcatcstr(glsl, "readonly ");
				else
					bcatcstr(glsl, "restrict ");
			}

			{
				char* prefix = "";
				uint32_t elements = 1;

				switch (psDecl->sUAV.Type[0])
				{
				case RETURN_TYPE_UINT:
					prefix = "u";
					break;
				case RETURN_TYPE_SINT:
					prefix = "i";
					break;
				default:
					break;
				}

				// NOTE: apparently DirectX does _not_ store the sizeof a typed UAV (RWBuffer<uint> has only one element), so this is not really working
				if (psDecl->sUAV.Type[1] == psDecl->sUAV.Type[0]) {
					++elements;
					if (psDecl->sUAV.Type[2] == psDecl->sUAV.Type[0]) {
						++elements;
						if (psDecl->sUAV.Type[3] == psDecl->sUAV.Type[0]) {
							++elements;
						}
					}
				}

				// NOTE: in this case it's always a uint, because in regular DirectX until 11.2/12.0 there is only R32 RWBuffer support (restricted typed loads)
				if (psDecl->value.eResourceDimension == RESOURCE_DIMENSION_BUFFER)
					bcatcstr(glsl, "layout(r32ui) ");
				// NOTE: because we don't know the number of elements, we can not create a correct layout specifier, but have to hack it together
				//       according to how we use it
				else switch (psDecl->sUAV.Type[0])
				{
				case RETURN_TYPE_FLOAT:
					bcatcstr(glsl, "layout(rgba32f) ");
					break;
				case RETURN_TYPE_UNORM:
					bcatcstr(glsl, "layout(rgba8) ");
					break;
				case RETURN_TYPE_SNORM:
					bcatcstr(glsl, "layout(rgba8_snorm) ");
					break;
				case RETURN_TYPE_UINT:
					bcatcstr(glsl, "layout(rgba32ui) ");
					break;
				case RETURN_TYPE_SINT:
					bcatcstr(glsl, "layout(rgba32i) ");
					break;
				default:
					ASSERT(0);
				}

				switch(psDecl->value.eResourceDimension)
				{
				case RESOURCE_DIMENSION_BUFFER:
					bformata(glsl, "uniform %simageBuffer ", prefix);
					break;
				case RESOURCE_DIMENSION_TEXTURE1D:
					bformata(glsl, "uniform %simage1D ", prefix);
					break;
				case RESOURCE_DIMENSION_TEXTURE2D:
					bformata(glsl, "uniform %simage2D ", prefix);
					break;
				case RESOURCE_DIMENSION_TEXTURE2DMS:
					bformata(glsl, "uniform %simage2DMS ", prefix);
					break;
				case RESOURCE_DIMENSION_TEXTURE3D:
					bformata(glsl, "uniform %simage3D ", prefix);
					break;
				case RESOURCE_DIMENSION_TEXTURECUBE:
					bformata(glsl, "uniform %simageCube ", prefix);
					break;
				case RESOURCE_DIMENSION_TEXTURE1DARRAY:
					bformata(glsl, "uniform %simage1DArray ", prefix);
					break;
				case RESOURCE_DIMENSION_TEXTURE2DARRAY:
					bformata(glsl, "uniform %simage2DArray ", prefix);
					break;
				case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
					bformata(glsl, "uniform %simage3DArray ", prefix);
					break;
				case RESOURCE_DIMENSION_TEXTURECUBEARRAY:
					bformata(glsl, "uniform %simageCubeArray ", prefix);
					break;
				}
			}
			TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NONE);
			bcatcstr(glsl, ";\n");
			break;
		}
	case OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED:
		{
			const uint32_t ui32BindingPoint = psDecl->asOperands[0].aui32ArraySizes[0];
			ConstantBuffer* psCBuf = NULL;

			if(psDecl->sUAV.bCounter)
			{
				bformata(glsl, "layout (binding = 1) uniform atomic_uint ");
				//ResourceName(glsl, psContext, RGROUP_UAV, psDecl->asOperands[0].ui32RegisterNumber, 0);
				bformata(glsl, "UAV%d;\n", psDecl->asOperands[0].ui32RegisterNumber);
				bformata(glsl, "_counter; \n");
			}

			GetConstantBufferFromBindingPoint(RGROUP_UAV, ui32BindingPoint, &psContext->psShader->sInfo, &psCBuf);
			psCBuf->iUnsized = 1;

			DeclareBufferVariable(psContext, ui32BindingPoint, psCBuf, &psDecl->asOperands[0], psDecl->sUAV.ui32GloballyCoherentAccess, RTYPE_UAV_RWSTRUCTURED);
			break;
		}
	case OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW:
		{
			bstring varName;
			if(psDecl->sUAV.bCounter)
			{
				bformata(glsl, "layout (binding = 1) uniform atomic_uint ");
				//ResourceName(glsl, psContext, RGROUP_UAV, psDecl->asOperands[0].ui32RegisterNumber, 0);
				bformata(glsl, "UAV%d;\n", psDecl->asOperands[0].ui32RegisterNumber);
				bformata(glsl, "_counter; \n");
			}

			varName = bfromcstralloc(16, "");
			//ResourceName(glsl, psContext, RGROUP_UAV, psDecl->asOperands[0].ui32RegisterNumber, 0);
			bformata(varName, "UAV%d", psDecl->asOperands[0].ui32RegisterNumber);

			bformata(glsl, "buffer Block%d {\n\tuint ", psDecl->asOperands[0].ui32RegisterNumber);
			ShaderVarName(glsl, psShader, bstr2cstr(varName, '\0'));
			bcatcstr(glsl, "[];\n};\n");

			bdestroy(varName);

			break;
		}
	case OPCODE_DCL_RESOURCE_STRUCTURED:
		{
			ConstantBuffer* psCBuf = NULL;

			GetConstantBufferFromBindingPoint(RGROUP_TEXTURE, psDecl->asOperands[0].ui32RegisterNumber, &psContext->psShader->sInfo, &psCBuf);
			psCBuf->iUnsized = 1;

			DeclareBufferVariable(psContext, psDecl->asOperands[0].ui32RegisterNumber, psCBuf, &psDecl->asOperands[0], 0, RTYPE_STRUCTURED);
			break;
		}
	case OPCODE_DCL_RESOURCE_RAW:
		{
			bstring varName = bfromcstralloc(16, "");
			bformata(varName, "RawRes%d", psDecl->asOperands[0].ui32RegisterNumber);

			bformata(glsl, "buffer Block%d {\n\tuint ", psDecl->asOperands[0].ui32RegisterNumber);
			ShaderVarName(glsl, psContext->psShader, bstr2cstr(varName, '\0'));
			bcatcstr(glsl, "[];\n};\n");

			bdestroy(varName);
			break;
		}
	case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW:
		{
			ShaderVarType* psVarType = &psShader->sGroupSharedVarType[psDecl->asOperands[0].ui32RegisterNumber];

			ASSERT(psDecl->asOperands[0].ui32RegisterNumber < MAX_GROUPSHARED);
			ASSERT(psDecl->sTGSM.ui32Count == 1);

			bcatcstr(glsl, "shared uint ");

			TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NONE);
			bformata(glsl, "[%d];\n", psDecl->sTGSM.ui32Count);

			memset(psVarType, 0, sizeof(ShaderVarType));
			strcpy(psVarType->Name, "$Element");

			psVarType->Columns = psDecl->sTGSM.ui32Stride/4;
			psVarType->Elements = psDecl->sTGSM.ui32Count;
			psVarType->Type = SVT_UINT;
			break;
		}
	case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED:
		{
			ShaderVarType* psVarType = &psShader->sGroupSharedVarType[psDecl->asOperands[0].ui32RegisterNumber];

			ASSERT(psDecl->asOperands[0].ui32RegisterNumber < MAX_GROUPSHARED);

			bcatcstr(glsl, "shared struct {\n");
			bformata(glsl, "uint value[%d];\n", psDecl->sTGSM.ui32Stride/4);
			bcatcstr(glsl, "} ");
			TranslateOperand(psContext, &psDecl->asOperands[0], TO_FLAG_NONE);
			bformata(glsl, "[%d];\n",
				psDecl->sTGSM.ui32Count);

			memset(psVarType, 0, sizeof(ShaderVarType));
			strcpy(psVarType->Name, "$Element");

			psVarType->Columns = psDecl->sTGSM.ui32Stride/4;
			psVarType->Elements = psDecl->sTGSM.ui32Count;
			psVarType->Type = SVT_UINT;
			break;
		}
	case OPCODE_DCL_STREAM:
		{
			ASSERT(psDecl->asOperands[0].eType == OPERAND_TYPE_STREAM);

			psShader->ui32CurrentVertexOutputStream = psDecl->asOperands[0].ui32RegisterNumber;

			bformata(glsl, "layout(stream = %d) out;\n", psShader->ui32CurrentVertexOutputStream);

			break;
		}
	case OPCODE_DCL_GS_INSTANCE_COUNT:
		{
			bformata(glsl, "layout(invocations = %d) in;\n", psDecl->value.ui32GSInstanceCount);
			break;
		}
	default:
		{
			ASSERT(0);
			break;
		}
	}
}

//Convert from per-phase temps to global temps for GLSL.
void ConsolidateHullTempVars(Shader* psShader)
{
	uint32_t i, k;
	uint32_t ui32Phase, ui32Instance;
	const uint32_t ui32NumDeclLists =
		psShader->asPhase[HS_FORK_PHASE].ui32InstanceCount +
		psShader->asPhase[HS_CTRL_POINT_PHASE].ui32InstanceCount +
		psShader->asPhase[HS_JOIN_PHASE].ui32InstanceCount +
		psShader->asPhase[HS_GLOBAL_DECL].ui32InstanceCount;

	Declaration** pasDeclArray = hlslcc_malloc(sizeof(Declaration*) * ui32NumDeclLists);

	uint32_t* pui32DeclCounts = hlslcc_malloc(sizeof(uint32_t) * ui32NumDeclLists);
	uint32_t ui32NumTemps = 0;

	i = 0;
	for(ui32Phase = HS_GLOBAL_DECL; ui32Phase < NUM_PHASES; ui32Phase++)
	{
		for(ui32Instance = 0; ui32Instance < psShader->asPhase[ui32Phase].ui32InstanceCount; ++ui32Instance)
	{
			pasDeclArray[i] = psShader->asPhase[ui32Phase].ppsDecl[ui32Instance];
			pui32DeclCounts[i++] = psShader->asPhase[ui32Phase].pui32DeclCount[ui32Instance];
		}
	}

	for(k = 0; k < ui32NumDeclLists; ++k)
	{
        for(i=0; i < pui32DeclCounts[k]; ++i)
		{
			Declaration* psDecl = pasDeclArray[k]+i;

			if(psDecl->eOpcode == OPCODE_DCL_TEMPS)
			{
				if(ui32NumTemps < psDecl->value.ui32NumTemps)
				{
					//Find the total max number of temps needed by the entire
					//shader.
					ui32NumTemps = psDecl->value.ui32NumTemps;
				}
				//Only want one global temp declaration.
				psDecl->value.ui32NumTemps = 0;
			}
		}
	}

	//Find the first temp declaration and make it
	//declare the max needed amount of temps.
	for(k = 0; k < ui32NumDeclLists; ++k)
	{
		for(i=0; i < pui32DeclCounts[k]; ++i)
		{
			Declaration* psDecl = pasDeclArray[k]+i;

			if(psDecl->eOpcode == OPCODE_DCL_TEMPS)
			{
				psDecl->value.ui32NumTemps = ui32NumTemps;
				return;
			}
		}
	}

	hlslcc_free(pasDeclArray);
	hlslcc_free(pui32DeclCounts);
}

const char* GetMangleSuffix(const SHADER_TYPE eShaderType)
{
	switch (eShaderType)
	{
	case VERTEX_SHADER  : return "_VS";
	case PIXEL_SHADER   : return "_PS";
	case GEOMETRY_SHADER: return "_GS";
	case HULL_SHADER    : return "_HS";
	case DOMAIN_SHADER  : return "_DS";
	case COMPUTE_SHADER : return "_CS";
	}
	ASSERT(0);
	return "";
}

