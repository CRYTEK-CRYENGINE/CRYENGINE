
#include "internal_includes/tokens.h"
#include "internal_includes/structs.h"
#include "internal_includes/decode.h"
#include "stdlib.h"
#include "stdio.h"
#include "internal_includes/reflect.h"
#include "internal_includes/debug.h"
#include "internal_includes/hlslcc_malloc.h"

#define FOURCC(a, b, c, d) ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24 ))
enum {FOURCC_CTAB = FOURCC('C', 'T', 'A', 'B')}; //Constant table

#ifdef _DEBUG
static uint64_t operandID = 0;
static uint64_t instructionID = 0;
#endif

static uint32_t aui32ImmediateConst[256];
static uint32_t ui32MaxTemp = 0;

uint32_t DX9_DECODE_OPERAND_IS_SRC = 0x1;
uint32_t DX9_DECODE_OPERAND_IS_DEST = 0x2;
uint32_t DX9_DECODE_OPERAND_IS_DECL = 0x4;

uint32_t DX9_DECODE_OPERAND_IS_CONST = 0x8;
uint32_t DX9_DECODE_OPERAND_IS_ICONST = 0x10;
uint32_t DX9_DECODE_OPERAND_IS_BCONST = 0x20;

#define MAX_INPUTS 64

static DECLUSAGE_DX9 aeInputUsage[MAX_INPUTS];
static uint32_t aui32InputUsageIndex[MAX_INPUTS];

static void DecodeOperandDX9(const Shader* psShader, const uint32_t ui32Token, const uint32_t ui32Token1, uint32_t ui32Flags, Operand *psOperand)
{
	const uint32_t ui32RegNum = DecodeOperandRegisterNumberDX9(ui32Token);
	const uint32_t ui32RegType = DecodeOperandTypeDX9(ui32Token);
	const uint32_t bRelativeAddr = DecodeOperandIsRelativeAddressModeDX9(ui32Token);

	const uint32_t ui32WriteMask = DecodeDestWriteMaskDX9(ui32Token);
	const uint32_t ui32Swizzle = DecodeOperandSwizzleDX9(ui32Token);

	SHADER_VARIABLE_TYPE ConstType;

	psOperand->ui32RegisterNumber = ui32RegNum;

	psOperand->iNumComponents = 4;

#ifdef _DEBUG
	psOperand->id = operandID++;
#endif

	psOperand->iWriteMaskEnabled = 0;
	psOperand->iGSInput = 0;
	psOperand->iExtended = 0;
	psOperand->psSubOperand[0] = 0;
	psOperand->psSubOperand[1] = 0;
	psOperand->psSubOperand[2] = 0;

	psOperand->iIndexDims = INDEX_0D;

	psOperand->iIntegerImmediate = 0;

	psOperand->pszSpecialName[0] ='\0';


	psOperand->eModifier = OPERAND_MODIFIER_NONE;
	if(ui32Flags & DX9_DECODE_OPERAND_IS_SRC)
	{
		uint32_t ui32Modifier = DecodeSrcModifierDX9(ui32Token);

		switch(ui32Modifier)
		{
		case SRCMOD_DX9_NONE:
			{
				break;
			}
		case SRCMOD_DX9_NEG:
			{
				psOperand->eModifier = OPERAND_MODIFIER_NEG;
				break;
			}
		case SRCMOD_DX9_ABS:
			{
				psOperand->eModifier = OPERAND_MODIFIER_ABS;
				break;
			}
		case SRCMOD_DX9_ABSNEG:
			{
				psOperand->eModifier = OPERAND_MODIFIER_ABSNEG;
				break;
			}
		default:
			{
				ASSERT(0);
				break;
			}
		}
	}

	if((ui32Flags & DX9_DECODE_OPERAND_IS_DECL)==0)
	{
		if(ui32Flags & DX9_DECODE_OPERAND_IS_DEST)
		{
			if(ui32WriteMask != DX9_WRITEMASK_ALL)
			{
				psOperand->iWriteMaskEnabled = 1;
				psOperand->eSelMode = OPERAND_4_COMPONENT_MASK_MODE;

				if(ui32WriteMask & DX9_WRITEMASK_0)
				{
					psOperand->ui32CompMask |= OPERAND_4_COMPONENT_MASK_X;
				}
				if(ui32WriteMask & DX9_WRITEMASK_1)
				{
					psOperand->ui32CompMask |= OPERAND_4_COMPONENT_MASK_Y;
				}
				if(ui32WriteMask & DX9_WRITEMASK_2)
				{
					psOperand->ui32CompMask |= OPERAND_4_COMPONENT_MASK_Z;
				}
				if(ui32WriteMask & DX9_WRITEMASK_3)
				{
					psOperand->ui32CompMask |= OPERAND_4_COMPONENT_MASK_W;
				}
			}
		}
		else
			if(ui32Swizzle != NO_SWIZZLE_DX9)
			{
				uint32_t component;

				psOperand->iWriteMaskEnabled = 1;
				psOperand->eSelMode = OPERAND_4_COMPONENT_SWIZZLE_MODE;

				psOperand->ui32Swizzle = 1;

				/* Add the swizzle */
				if(ui32Swizzle == REPLICATE_SWIZZLE_DX9(0))
				{
					psOperand->eSelMode = OPERAND_4_COMPONENT_SELECT_1_MODE;
					psOperand->aui32Swizzle[0] = OPERAND_4_COMPONENT_X;
				}
				else
					if(ui32Swizzle == REPLICATE_SWIZZLE_DX9(1))
					{
						psOperand->eSelMode = OPERAND_4_COMPONENT_SELECT_1_MODE;
						psOperand->aui32Swizzle[0] = OPERAND_4_COMPONENT_Y;
					}
					else if (ui32Swizzle == REPLICATE_SWIZZLE_DX9(2))
					{
						psOperand->eSelMode = OPERAND_4_COMPONENT_SELECT_1_MODE;
						psOperand->aui32Swizzle[0] = OPERAND_4_COMPONENT_Z;
					}
					else if (ui32Swizzle == REPLICATE_SWIZZLE_DX9(3))
					{
						psOperand->eSelMode = OPERAND_4_COMPONENT_SELECT_1_MODE;
						psOperand->aui32Swizzle[0] = OPERAND_4_COMPONENT_W;
					}
					else
					{
						for (component = 0; component < 4; component++)
						{
							uint32_t ui32CompSwiz =
								ui32Swizzle & (3 << (DX9_SWIZZLE_SHIFT + (component * 2)));
							ui32CompSwiz >>= (DX9_SWIZZLE_SHIFT + (component * 2));

							if (ui32CompSwiz == 0)
							{
								psOperand->aui32Swizzle[component] = OPERAND_4_COMPONENT_X;
							}
							else if (ui32CompSwiz == 1)
							{
								psOperand->aui32Swizzle[component] = OPERAND_4_COMPONENT_Y;
							}
							else if (ui32CompSwiz == 2)
							{
								psOperand->aui32Swizzle[component] = OPERAND_4_COMPONENT_Z;
							}
							else
							{
								psOperand->aui32Swizzle[component] = OPERAND_4_COMPONENT_W;
							}
						}
					}
			}

			if(bRelativeAddr)
			{
				psOperand->psSubOperand[0] = hlslcc_malloc(sizeof(Operand));
				DecodeOperandDX9(psShader, ui32Token1, 0, ui32Flags, psOperand->psSubOperand[0]);

				psOperand->iIndexDims = INDEX_1D;

				psOperand->eIndexRep[0] = OPERAND_INDEX_RELATIVE;

				psOperand->aui32ArraySizes[0] = 0;
			}
	}

	if(ui32RegType == OPERAND_TYPE_DX9_CONSTBOOL)
	{
		ui32Flags |= DX9_DECODE_OPERAND_IS_BCONST;
		ConstType = SVT_BOOL;
	}
	else if(ui32RegType == OPERAND_TYPE_DX9_CONSTINT)
	{
		ui32Flags |= DX9_DECODE_OPERAND_IS_ICONST;
		ConstType = SVT_INT;
	}
	else if(ui32RegType == OPERAND_TYPE_DX9_CONST)
	{
		ui32Flags |= DX9_DECODE_OPERAND_IS_CONST;
		ConstType = SVT_FLOAT;
	}

	switch(ui32RegType)
	{
	case OPERAND_TYPE_DX9_TEMP:
		{
			psOperand->eType = OPERAND_TYPE_TEMP;

			if(ui32MaxTemp < ui32RegNum+1)
			{
				ui32MaxTemp = ui32RegNum+1;
			}
			break;
		}
	case OPERAND_TYPE_DX9_INPUT:
		{
			psOperand->eType = OPERAND_TYPE_INPUT;

			ASSERT(ui32RegNum < MAX_INPUTS);

			if(psShader->eShaderType == PIXEL_SHADER)
			{
				if(aeInputUsage[ui32RegNum] == DECLUSAGE_TEXCOORD)
				{
					psOperand->eType = OPERAND_TYPE_SPECIAL_TEXCOORD_INPUT;
					psOperand->ui32RegisterNumber = aui32InputUsageIndex[ui32RegNum];
				}
				else
					//0 = base colour, 1 = offset colour.
					if(ui32RegNum == 0)
					{
						psOperand->eType = OPERAND_TYPE_SPECIAL_OUTBASECOLOUR;
					}
					else
					{
						ASSERT(ui32RegNum == 1);
						psOperand->eType = OPERAND_TYPE_SPECIAL_OUTOFFSETCOLOUR;
					}
			}
			break;
		}
		//Same value as OPERAND_TYPE_DX9_TEXCRDOUT
		//OPERAND_TYPE_DX9_TEXCRDOUT is the pre-SM3 equivalent
	case OPERAND_TYPE_DX9_OUTPUT:
		{
			psOperand->eType = OPERAND_TYPE_OUTPUT;

			if(psShader->eShaderType == VERTEX_SHADER)
			{
				psOperand->eType = OPERAND_TYPE_SPECIAL_TEXCOORD_OUTPUT;
			}
			break;
		}
	case OPERAND_TYPE_DX9_RASTOUT:
		{
			//RegNum:
			//0=POSIION
			//1=FOG
			//2=POINTSIZE
			psOperand->eType = OPERAND_TYPE_OUTPUT;
			switch(ui32RegNum)
			{
			case 0:
				{
					psOperand->eType = OPERAND_TYPE_SPECIAL_POSITION;
					break;
				}
			case 1:
				{
					psOperand->eType = OPERAND_TYPE_SPECIAL_FOG;
					break;
				}
			case 2:
				{
					psOperand->eType = OPERAND_TYPE_SPECIAL_POINTSIZE;
					psOperand->iNumComponents = 1;
					break;
				}
			}
			break;
		}
	case OPERAND_TYPE_DX9_ATTROUT:
		{
			ASSERT(psShader->eShaderType == VERTEX_SHADER);

			psOperand->eType = OPERAND_TYPE_OUTPUT;

			//0 = base colour, 1 = offset colour.
			if(ui32RegNum == 0)
			{
				psOperand->eType = OPERAND_TYPE_SPECIAL_OUTBASECOLOUR;
			}
			else
			{
				ASSERT(ui32RegNum == 1);
				psOperand->eType = OPERAND_TYPE_SPECIAL_OUTOFFSETCOLOUR;
			}

			break;
		}
	case OPERAND_TYPE_DX9_COLOROUT:
		{
			ASSERT(psShader->eShaderType == PIXEL_SHADER);
			psOperand->eType = OPERAND_TYPE_OUTPUT;
			break;
		}
	case OPERAND_TYPE_DX9_CONSTBOOL:
	case OPERAND_TYPE_DX9_CONSTINT:
	case OPERAND_TYPE_DX9_CONST:
		{
			//c# = constant float
			//i# = constant int
			//b# = constant bool

			//c0 might be an immediate while i0 is in the constant buffer
			if(aui32ImmediateConst[ui32RegNum] & ui32Flags)
			{
				if(ConstType != SVT_FLOAT)
				{
					psOperand->eType = OPERAND_TYPE_SPECIAL_IMMCONSTINT;
				}
				else
				{
					psOperand->eType = OPERAND_TYPE_SPECIAL_IMMCONST;
				}
			}
			else
			{
				psOperand->eType = OPERAND_TYPE_CONSTANT_BUFFER;
				psOperand->aui32ArraySizes[1] = psOperand->ui32RegisterNumber;
			}
			break;
		}
	case OPERAND_TYPE_DX9_ADDR:
		{
			//Vertex shader: address register (only have one of these)
			//Pixel shader: texture coordinate register (a few of these)
			if(psShader->eShaderType == PIXEL_SHADER)
			{
				psOperand->eType = OPERAND_TYPE_SPECIAL_TEXCOORD_INPUT;
			}
			else
			{
				psOperand->eType = OPERAND_TYPE_SPECIAL_ADDRESS;
			}
			break;
		}
	case OPERAND_TYPE_DX9_SAMPLER:
		{
			psOperand->eType = OPERAND_TYPE_RESOURCE;
			break;
		}
	case OPERAND_TYPE_DX9_LOOP:
		{
			psOperand->eType = OPERAND_TYPE_SPECIAL_LOOPCOUNTER;
			break;
		}
	default:
		{
			ASSERT(0);
			break;
		}
	}
}

static void DeclareNumTemps(Shader* psShader, const uint32_t ui32NumTemps, Declaration* psDecl)
{
	psDecl->eOpcode = OPCODE_DCL_TEMPS;
	psDecl->value.ui32NumTemps = ui32NumTemps;
}

static void SetupRegisterUsage(const Shader* psShader, const uint32_t ui32Token0, const uint32_t ui32Token1)
{
	DECLUSAGE_DX9 eUsage = DecodeUsageDX9(ui32Token0);
	uint32_t ui32UsageIndex = DecodeUsageIndexDX9(ui32Token0);
	uint32_t ui32RegNum = DecodeOperandRegisterNumberDX9(ui32Token1);
	uint32_t ui32RegType = DecodeOperandTypeDX9(ui32Token1);

	if(ui32RegType == OPERAND_TYPE_DX9_INPUT)
	{
		ASSERT(ui32RegNum < MAX_INPUTS);
		aeInputUsage[ui32RegNum] = eUsage;
		aui32InputUsageIndex[ui32RegNum] = ui32UsageIndex;
	}
}

//Declaring one constant from a constant buffer will cause all constants in the buffer decalared.
//In dx9 there is only one constant buffer per shader.
static void DeclareConstantBuffer(const Shader* psShader, Declaration* psDecl)
{
	DECLUSAGE_DX9 eUsage = (DECLUSAGE_DX9)0;
	uint32_t ui32UsageIndex = 0;
	//Pick any constant register in the table. Might not start at c0 (e.g. when register(cX) is used).
	uint32_t ui32RegNum = psShader->sInfo.psConstantBuffers->asVars[0].ui32StartOffset / 16;
	OPERAND_TYPE_DX9 ui32RegType = OPERAND_TYPE_DX9_CONST;

	if(psShader->sInfo.psConstantBuffers->asVars[0].sType.Type == SVT_INT)
	{
		ui32RegType = OPERAND_TYPE_DX9_CONSTINT;
	}
	else if(psShader->sInfo.psConstantBuffers->asVars[0].sType.Type == SVT_BOOL)
	{
		ui32RegType = OPERAND_TYPE_DX9_CONSTBOOL;
	}

	if(psShader->eShaderType == VERTEX_SHADER)
	{
		psDecl->eOpcode = OPCODE_DCL_INPUT;
	}
	else
	{
		psDecl->eOpcode = OPCODE_DCL_INPUT_PS;
	}
	psDecl->ui32NumOperands = 1;

	DecodeOperandDX9(psShader, CreateOperandTokenDX9(ui32RegNum, ui32RegType), 0, DX9_DECODE_OPERAND_IS_DECL, &psDecl->asOperands[0]);

	ASSERT(psDecl->asOperands[0].eType == OPERAND_TYPE_CONSTANT_BUFFER);

	psDecl->eOpcode = OPCODE_DCL_CONSTANT_BUFFER;

	ASSERT(psShader->sInfo.ui32NumConstantBuffers);

	psDecl->asOperands[0].aui32ArraySizes[0] = 0;//Const buffer index
	psDecl->asOperands[0].aui32ArraySizes[1] = psShader->sInfo.psConstantBuffers[0].ui32TotalSizeInBytes / 16;//Number of vec4 constants.
}

static void DecodeDeclarationDX9(const Shader* psShader, const uint32_t ui32Token0, const uint32_t ui32Token1, Declaration* psDecl)
{
	DECLUSAGE_DX9 eUsage = DecodeUsageDX9(ui32Token0);
	uint32_t ui32UsageIndex = DecodeUsageIndexDX9(ui32Token0);
	uint32_t ui32RegNum = DecodeOperandRegisterNumberDX9(ui32Token1);
	uint32_t ui32RegType = DecodeOperandTypeDX9(ui32Token1);

	if(psShader->eShaderType == VERTEX_SHADER)
	{
		psDecl->eOpcode = OPCODE_DCL_INPUT;
	}
	else
	{
		psDecl->eOpcode = OPCODE_DCL_INPUT_PS;
	}
	psDecl->ui32NumOperands = 1;
	DecodeOperandDX9(psShader, ui32Token1, 0, DX9_DECODE_OPERAND_IS_DECL, &psDecl->asOperands[0]);

	if(ui32RegType == OPERAND_TYPE_DX9_SAMPLER)
	{
		const RESOURCE_DIMENSION eResDim = DecodeTextureTypeMaskDX9(ui32Token0);
		psDecl->value.eResourceDimension = eResDim;
		psDecl->ui32IsShadowTex = 0;
		psDecl->eOpcode = OPCODE_DCL_RESOURCE;
	}

	if(psDecl->asOperands[0].eType == OPERAND_TYPE_OUTPUT)
	{
		psDecl->eOpcode = OPCODE_DCL_OUTPUT;

		if(psDecl->asOperands[0].ui32RegisterNumber==0 && psShader->eShaderType == VERTEX_SHADER)
		{
			psDecl->eOpcode = OPCODE_DCL_OUTPUT_SIV;
			//gl_Position
			psDecl->asOperands[0].eSpecialName = NAME_POSITION;
		}
	}
	else if (psDecl->asOperands[0].eType == OPERAND_TYPE_CONSTANT_BUFFER)
	{
		psDecl->eOpcode = OPCODE_DCL_CONSTANT_BUFFER;

		ASSERT(psShader->sInfo.ui32NumConstantBuffers);

		psDecl->asOperands[0].aui32ArraySizes[0] = 0;//Const buffer index
		psDecl->asOperands[0].aui32ArraySizes[1] = psShader->sInfo.psConstantBuffers[0].ui32TotalSizeInBytes / 16;//Number of vec4 constants.
	}
}

static void DefineDX9(Shader* psShader,
											const uint32_t ui32RegNum,
											const uint32_t ui32Flags,
											const uint32_t c0,
											const uint32_t c1,
											const uint32_t c2,
											const uint32_t c3,
											Declaration* psDecl)
{
	psDecl->eOpcode = OPCODE_SPECIAL_DCL_IMMCONST;
	psDecl->ui32NumOperands = 2;

	memset(&psDecl->asOperands[0], 0, sizeof(Operand));
	psDecl->asOperands[0].eType = OPERAND_TYPE_SPECIAL_IMMCONST;

	psDecl->asOperands[0].ui32RegisterNumber = ui32RegNum;

	if(ui32Flags & (DX9_DECODE_OPERAND_IS_ICONST|DX9_DECODE_OPERAND_IS_BCONST))
	{
		psDecl->asOperands[0].eType = OPERAND_TYPE_SPECIAL_IMMCONSTINT;
	}

	aui32ImmediateConst[ui32RegNum] |= ui32Flags;

	memset(&psDecl->asOperands[1], 0, sizeof(Operand));
	psDecl->asOperands[1].eType = OPERAND_TYPE_IMMEDIATE32;
	psDecl->asOperands[1].iNumComponents = 4;
	psDecl->asOperands[1].iIntegerImmediate = (ui32Flags & (DX9_DECODE_OPERAND_IS_ICONST|DX9_DECODE_OPERAND_IS_BCONST)) ? 1 : 0;
	psDecl->asOperands[1].afImmediates[0] = *((float*)&c0);
	psDecl->asOperands[1].afImmediates[1] = *((float*)&c1);
	psDecl->asOperands[1].afImmediates[2] = *((float*)&c2);
	psDecl->asOperands[1].afImmediates[3] = *((float*)&c3);
}

static void CreateD3D10Instruction(
	Shader* psShader,
	Instruction* psInst,
	const OPCODE_TYPE eType,
	const uint32_t bHasDest,
	const uint32_t ui32SrcCount,
	const uint32_t* pui32Tokens)
{
	uint32_t ui32Src;
	uint32_t ui32Offset = 1;

	memset(psInst, 0, sizeof(Instruction));

#ifdef _DEBUG
	psInst->id = instructionID++;
#endif

	psInst->eOpcode = eType;
	psInst->ui32NumOperands = ui32SrcCount;

	if(bHasDest)
	{
		++psInst->ui32NumOperands;

		DecodeOperandDX9(psShader,
			pui32Tokens[ui32Offset],
			pui32Tokens[ui32Offset+1],
			DX9_DECODE_OPERAND_IS_DEST,
			&psInst->asOperands[0]);

		if(DecodeDestModifierDX9(pui32Tokens[ui32Offset]) & DESTMOD_DX9_SATURATE)
		{
			psInst->bSaturate = 1;
		}

		ui32Offset++;
		psInst->ui32FirstSrc = 1;
	}

	for(ui32Src=0; ui32Src < ui32SrcCount; ++ui32Src)
	{
		DecodeOperandDX9(psShader,
			pui32Tokens[ui32Offset],
			pui32Tokens[ui32Offset+1],
			DX9_DECODE_OPERAND_IS_SRC,
			&psInst->asOperands[bHasDest+ui32Src]);

		ui32Offset++;
	}
}

Shader* DecodeDX9BC(const uint32_t* pui32Tokens)
{
	const uint32_t* pui32CurrentToken = pui32Tokens;
	uint32_t ui32NumInstructions = 0;
	uint32_t ui32NumDeclarations = 0;
	Instruction* psInst;
	Declaration* psDecl;
	uint32_t decl, inst;
	uint32_t bDeclareConstantTable = 0;
	Shader* psShader = hlslcc_calloc(1, sizeof(Shader));

	memset(aui32ImmediateConst, 0, 256);

	psShader->ui32MajorVersion = DecodeProgramMajorVersionDX9(*pui32CurrentToken);
	psShader->ui32MinorVersion = DecodeProgramMinorVersionDX9(*pui32CurrentToken);
	psShader->eShaderType = DecodeShaderTypeDX9(*pui32CurrentToken);

	pui32CurrentToken++;

	//Work out how many instructions and declarations we need to allocate memory for.
	while (1)
	{
		OPCODE_TYPE_DX9 eOpcode = DecodeOpcodeTypeDX9(pui32CurrentToken[0]);
		uint32_t ui32InstLen = DecodeInstructionLengthDX9(pui32CurrentToken[0]);

		if(eOpcode == OPCODE_DX9_END)
		{
			//SM4+ always end with RET.
			//Insert a RET instruction on END to
			//replicate this behaviour.
			++ui32NumInstructions;
			break;
		}
		else if(eOpcode == OPCODE_DX9_COMMENT)
		{
			ui32InstLen = DecodeCommentLengthDX9(pui32CurrentToken[0]);
			if(pui32CurrentToken[1] == FOURCC_CTAB)
			{
				LoadD3D9ConstantTable((char*)(&pui32CurrentToken[2]), &psShader->sInfo);

				ASSERT(psShader->sInfo.ui32NumConstantBuffers);

				if(psShader->sInfo.psConstantBuffers[0].ui32NumVars)
				{
					++ui32NumDeclarations;
					bDeclareConstantTable = 1;
				}
			}
		}
		else if((eOpcode == OPCODE_DX9_DEF)||(eOpcode == OPCODE_DX9_DEFI)||(eOpcode == OPCODE_DX9_DEFB))
		{
			++ui32NumDeclarations;
		}
		else if(eOpcode == OPCODE_DX9_DCL)
		{
			const OPERAND_TYPE_DX9 eType = DecodeOperandTypeDX9(pui32CurrentToken[2]);
			uint32_t ignoreDCL = 0;

			//Inputs and outputs are declared in AddVersionDependentCode
			if(psShader->eShaderType == PIXEL_SHADER && (OPERAND_TYPE_DX9_CONST != eType && OPERAND_TYPE_DX9_SAMPLER != eType))
			{
				ignoreDCL = 1;
			}
			if(!ignoreDCL)
			{
				++ui32NumDeclarations;
			}
		}
		else
		{
			switch(eOpcode)
			{
			case OPCODE_DX9_NRM:
				{
					//Emulate with dp4 and rsq
					ui32NumInstructions += 2;
					break;
				}
			default:
				{
					++ui32NumInstructions;
					break;
				}
			}
		}

		pui32CurrentToken += ui32InstLen + 1;
	}

	psInst = hlslcc_malloc(sizeof(Instruction) * ui32NumInstructions);
	psShader->asPhase[MAIN_PHASE].ui32InstanceCount = 1;
	psShader->asPhase[MAIN_PHASE].ppsInst = hlslcc_malloc(sizeof(Instruction*));
	psShader->asPhase[MAIN_PHASE].ppsInst[0] = psInst;
	psShader->asPhase[MAIN_PHASE].pui32InstCount = hlslcc_malloc(sizeof(uint32_t));
	psShader->asPhase[MAIN_PHASE].pui32InstCount[0] = ui32NumInstructions;

	if(psShader->eShaderType == VERTEX_SHADER)
	{
		//Declare gl_Position. vs_3_0 does declare it, SM1/2 do not
		ui32NumDeclarations++;
	}

	//For declaring temps.
	ui32NumDeclarations++;

	psDecl = hlslcc_malloc(sizeof(Declaration) * ui32NumDeclarations);
	psShader->asPhase[MAIN_PHASE].ppsDecl = hlslcc_malloc(sizeof(Declaration*));
	psShader->asPhase[MAIN_PHASE].ppsDecl[0] = psDecl;
	psShader->asPhase[MAIN_PHASE].pui32DeclCount = hlslcc_malloc(sizeof(uint32_t));
	psShader->asPhase[MAIN_PHASE].pui32DeclCount[0] = ui32NumDeclarations;

	pui32CurrentToken = pui32Tokens + 1;

	inst=0;
	decl=0;
	while (1)
	{
		OPCODE_TYPE_DX9 eOpcode = DecodeOpcodeTypeDX9(pui32CurrentToken[0]);
		uint32_t ui32InstLen = DecodeInstructionLengthDX9(pui32CurrentToken[0]);

		if(eOpcode == OPCODE_DX9_END)
		{
			CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_RET, 0, 0, pui32CurrentToken);
			inst++;
			break;
		}
		else if(eOpcode == OPCODE_DX9_COMMENT)
		{
			ui32InstLen = DecodeCommentLengthDX9(pui32CurrentToken[0]);
		}
		else if(eOpcode == OPCODE_DX9_DCL)
		{
			const OPERAND_TYPE_DX9 eType = DecodeOperandTypeDX9(pui32CurrentToken[2]);
			uint32_t ignoreDCL = 0;
			//Inputs and outputs are declared in AddVersionDependentCode
			if(psShader->eShaderType == PIXEL_SHADER && (OPERAND_TYPE_DX9_CONST != eType && OPERAND_TYPE_DX9_SAMPLER != eType))
			{
				ignoreDCL = 1;
			}

			SetupRegisterUsage(psShader, pui32CurrentToken[1], pui32CurrentToken[2]);

			if(!ignoreDCL)
			{
				DecodeDeclarationDX9(psShader, pui32CurrentToken[1], pui32CurrentToken[2], &psDecl[decl]);
				decl++;
			}
		}
		else if((eOpcode == OPCODE_DX9_DEF)||(eOpcode == OPCODE_DX9_DEFI)||(eOpcode == OPCODE_DX9_DEFB))
		{
			const uint32_t ui32Const0 = *(pui32CurrentToken+2);
			const uint32_t ui32Const1 = *(pui32CurrentToken+3);
			const uint32_t ui32Const2 = *(pui32CurrentToken+4);
			const uint32_t ui32Const3 = *(pui32CurrentToken+5);
			uint32_t ui32Flags = 0;

			if(eOpcode == OPCODE_DX9_DEF)
			{
				ui32Flags |= DX9_DECODE_OPERAND_IS_CONST;
			}
			else if(eOpcode == OPCODE_DX9_DEFI)
			{
				ui32Flags |= DX9_DECODE_OPERAND_IS_ICONST;
			}
			else
			{
				ui32Flags |= DX9_DECODE_OPERAND_IS_BCONST;
			}


			DefineDX9(psShader,
				DecodeOperandRegisterNumberDX9(pui32CurrentToken[1]),
				ui32Flags,
				ui32Const0,
				ui32Const1,
				ui32Const2,
				ui32Const3,
				&psDecl[decl]);
			decl++;
		}
		else
		{
			switch(eOpcode)
			{
			case OPCODE_DX9_MOV:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_MOV, 1, 1, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_LIT:
				{
					/*Dest.x = 1
					Dest.y = (Src0.x > 0) ? Src0.x : 0
					Dest.z = (Src0.x > 0 && Src0.y > 0) ? pow(Src0.y, Src0.w) : 0
					Dest.w = 1
					*/
					ASSERT(0);
					break;
				}
			case OPCODE_DX9_ADD:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_ADD, 1, 2, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_SUB:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_ADD, 1, 2, pui32CurrentToken);
					ASSERT(psInst[inst].asOperands[2].eModifier == OPERAND_MODIFIER_NONE);
					psInst[inst].asOperands[2].eModifier = OPERAND_MODIFIER_NEG;
					break;
				}
			case OPCODE_DX9_MAD:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_MAD, 1, 3, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_MUL:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_MUL, 1, 2, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_RCP:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_RCP, 1, 1, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_RSQ:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_RSQ, 1, 1, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_DP3:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_DP3, 1, 2, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_DP4:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_DP4, 1, 2, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_MIN:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_MIN, 1, 2, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_MAX:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_MAX, 1, 2, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_SLT:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_LT, 1, 2, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_SGE:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_GE, 1, 2, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_EXP:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_EXP, 1, 1, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_LOG:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_LOG, 1, 1, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_NRM:
				{
					//Convert NRM RESULT, SRCA into:
					//dp4 RESULT, SRCA, SRCA
					//rsq RESULT, RESULT

					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_DP4, 1, 1, pui32CurrentToken);
					memcpy(&psInst[inst].asOperands[2],&psInst[inst].asOperands[1], sizeof(Operand));
					psInst[inst].ui32NumOperands++;
					++inst;

					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_RSQ, 0, 0, pui32CurrentToken);
					memcpy(&psInst[inst].asOperands[0], &psInst[inst - 1].asOperands[0], sizeof(Operand));
					memcpy(&psInst[inst].asOperands[1], &psInst[inst - 1].asOperands[0], sizeof(Operand));
					psInst[inst].ui32NumOperands++;
					psInst[inst].ui32NumOperands++;
					break;
				}
			case OPCODE_DX9_SINCOS:
				{
					//Before SM3, SINCOS has 2 extra constant sources -D3DSINCOSCONST1 and D3DSINCOSCONST2.
					//Ignore them.

					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_SINCOS, 1, 1, pui32CurrentToken);
					//Pre-SM4:
					//If the write mask is .x: dest.x = cos( V )
					//If the write mask is .y: dest.y = sin( V )
					//If the write mask is .xy:
					//dest.x = cos( V )
					//dest.y = sin( V )

					//SM4+
					//destSin destCos Angle

					psInst[inst].ui32NumOperands = 3;

					//Set the angle
					memcpy(&psInst[inst].asOperands[2],&psInst[inst].asOperands[1], sizeof(Operand));

					//Set the cosine dest
					memcpy(&psInst[inst].asOperands[1],&psInst[inst].asOperands[0], sizeof(Operand));

					//Set write masks
					psInst[inst].asOperands[0].ui32CompMask &= ~OPERAND_4_COMPONENT_MASK_Y;
					if(psInst[inst].asOperands[0].ui32CompMask & OPERAND_4_COMPONENT_MASK_X)
					{
						//Need cosine
					}
					else
					{
						psInst[inst].asOperands[0].eType = OPERAND_TYPE_NULL;
					}
					psInst[inst].asOperands[1].ui32CompMask &= ~OPERAND_4_COMPONENT_MASK_X;
					if(psInst[inst].asOperands[1].ui32CompMask & OPERAND_4_COMPONENT_MASK_Y)
					{
						//Need sine
					}
					else
					{
						psInst[inst].asOperands[1].eType = OPERAND_TYPE_NULL;
					}

					break;
				}
			case OPCODE_DX9_FRC:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_FRC, 1, 1, pui32CurrentToken);
					break;
				}

			case OPCODE_DX9_MOVA:
				{
					//MOVA preforms RoundToNearest on the src data.
					//The only rounding functions available in all GLSL version are ceil and floor.
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_ROUND_NI, 1, 1, pui32CurrentToken);
					break;
				}

			case OPCODE_DX9_TEX:
				{
					//texld r0, t0, s0
					// srcAddress[.swizzle], srcResource[.swizzle], srcSampler
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_SAMPLE, 1, 2, pui32CurrentToken);
					psInst[inst].asOperands[2].ui32RegisterNumber = 0;


					break;
				}
			case OPCODE_DX9_TEXLDL:
				{
					//texld r0, t0, s0
					// srcAddress[.swizzle], srcResource[.swizzle], srcSampler
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_SAMPLE_L, 1, 2, pui32CurrentToken);
					psInst[inst].asOperands[2].ui32RegisterNumber = 0;

					//Lod comes from fourth coordinate of address.
					memcpy(&psInst[inst].asOperands[4], &psInst[inst].asOperands[1], sizeof(Operand));

					psInst[inst].ui32NumOperands = 5;

					break;
				}

			case OPCODE_DX9_IF:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_IF, 0, 1, pui32CurrentToken);
					psInst[inst].eDX9TestType = D3DSPC_BOOLEAN;
					break;
				}

			case OPCODE_DX9_IFC:
				{
					const COMPARISON_DX9 eCmpOp = DecodeComparisonDX9(pui32CurrentToken[0]);
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_IF, 0, 2, pui32CurrentToken);
					psInst[inst].eDX9TestType = eCmpOp;
					break;
				}
			case OPCODE_DX9_ELSE:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_ELSE, 0, 0, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_CMP:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_MOVC, 1, 3, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_REP:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_REP, 0, 1, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_ENDREP:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_ENDREP, 0, 0, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_BREAKC:
				{
					const COMPARISON_DX9 eCmpOp = DecodeComparisonDX9(pui32CurrentToken[0]);
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_BREAKC, 0, 2, pui32CurrentToken);
					psInst[inst].eDX9TestType = eCmpOp;
					break;
				}

			case OPCODE_DX9_DSX:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_DERIV_RTX, 1, 1, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_DSY:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_DERIV_RTY, 1, 1, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_TEXKILL:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_DISCARD, 1, 0, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_TEXLDD:
				{
					// texldd, dst, src0, src1, src2, src3
					// srcAddress[.swizzle], srcResource[.swizzle], srcSampler, XGradient, YGradient
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_SAMPLE_D, 1, 4, pui32CurrentToken);

					// Move the gradients one slot up
					memcpy(&psInst[inst].asOperands[5], &psInst[inst].asOperands[4], sizeof(Operand));
					memcpy(&psInst[inst].asOperands[4], &psInst[inst].asOperands[3], sizeof(Operand));

					// Sampler register
					psInst[inst].asOperands[3].ui32RegisterNumber = 0;
					psInst[inst].ui32NumOperands = 6;
					break;
				}
			case OPCODE_DX9_LRP:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_LRP, 1, 3, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_DP2ADD:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_DP2ADD, 1, 3, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_POW:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_POW, 1, 2, pui32CurrentToken);
					break;
				}

			case OPCODE_DX9_DST:
			case OPCODE_DX9_M4x4:
			case OPCODE_DX9_M4x3:
			case OPCODE_DX9_M3x4:
			case OPCODE_DX9_M3x3:
			case OPCODE_DX9_M3x2:
			case OPCODE_DX9_CALL:
			case OPCODE_DX9_CALLNZ:
			case OPCODE_DX9_LABEL:

			case OPCODE_DX9_CRS:
			case OPCODE_DX9_SGN:
			case OPCODE_DX9_ABS:

			case OPCODE_DX9_TEXCOORD:
			case OPCODE_DX9_TEXBEM:
			case OPCODE_DX9_TEXBEML:
			case OPCODE_DX9_TEXREG2AR:
			case OPCODE_DX9_TEXREG2GB:
			case OPCODE_DX9_TEXM3x2PAD:
			case OPCODE_DX9_TEXM3x2TEX:
			case OPCODE_DX9_TEXM3x3PAD:
			case OPCODE_DX9_TEXM3x3TEX:
			case OPCODE_DX9_TEXM3x3SPEC:
			case OPCODE_DX9_TEXM3x3VSPEC:
			case OPCODE_DX9_EXPP:
			case OPCODE_DX9_LOGP:
			case OPCODE_DX9_CND:
			case OPCODE_DX9_TEXREG2RGB:
			case OPCODE_DX9_TEXDP3TEX:
			case OPCODE_DX9_TEXM3x2DEPTH:
			case OPCODE_DX9_TEXDP3:
			case OPCODE_DX9_TEXM3x3:
			case OPCODE_DX9_TEXDEPTH:
			case OPCODE_DX9_BEM:
			case OPCODE_DX9_SETP:
			case OPCODE_DX9_BREAKP:
				{
					ASSERT(0);
					break;
				}
			case OPCODE_DX9_NOP:
			case OPCODE_DX9_PHASE:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_NOP, 0, 0, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_LOOP:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_LOOP, 0, 2, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_RET:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_RET, 0, 0, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_ENDLOOP:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_ENDLOOP, 0, 0, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_ENDIF:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_ENDIF, 0, 0, pui32CurrentToken);
					break;
				}
			case OPCODE_DX9_BREAK:
				{
					CreateD3D10Instruction(psShader, &psInst[inst], OPCODE_BREAK, 0, 0, pui32CurrentToken);
					break;
				}
			default:
				{
					ASSERT(0);
					break;
				}
			}

			UpdateInstructionReferences(psShader, &psInst[inst]);

			inst++;
		}

		pui32CurrentToken += ui32InstLen + 1;
	}

	DeclareNumTemps(psShader, ui32MaxTemp, &psDecl[decl]);
	++decl;

	if(psShader->eShaderType == VERTEX_SHADER)
	{
		//Declare gl_Position. vs_3_0 does declare it, SM1/2 do not
		if(bDeclareConstantTable)
		{
			DecodeDeclarationDX9(psShader, 0, CreateOperandTokenDX9(0, OPERAND_TYPE_DX9_RASTOUT), &psDecl[decl+1]);
		}
		else
		{
			DecodeDeclarationDX9(psShader, 0, CreateOperandTokenDX9(0, OPERAND_TYPE_DX9_RASTOUT), &psDecl[decl]);
		}
	}

	if(bDeclareConstantTable)
	{
		DeclareConstantBuffer(psShader, &psDecl[decl]);
	}

	return psShader;
}
