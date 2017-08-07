#include "internal_includes/tokens.h"
#include "internal_includes/structs.h"
#include "internal_includes/decode.h"
#include "stdlib.h"
#include "stdio.h"
#include "bstrlib.h"
#include "internal_includes/toGLSLInstruction.h"
#include "internal_includes/toGLSLOperand.h"
#include "internal_includes/toGLSLDeclaration.h"
#include "internal_includes/languages.h"
#include "internal_includes/debug.h"
#include "internal_includes/hlslcc_malloc.h"
#include "../offline/hash.h"

#if defined(_WIN32) && !defined(PORTABLE)
#include <d3dcompiler.h>
#pragma comment(lib,"d3dcompiler.lib")
#endif //defined(_WIN32) && !defined(PORTABLE)

#ifndef GL_VERTEX_SHADER_ARB
#define GL_VERTEX_SHADER_ARB              0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER_ARB
#define GL_FRAGMENT_SHADER_ARB            0x8B30
#endif
#ifndef GL_GEOMETRY_SHADER
#define GL_GEOMETRY_SHADER                0x8DD9
#endif
#ifndef GL_TESS_EVALUATION_SHADER
#define GL_TESS_EVALUATION_SHADER         0x8E87
#endif
#ifndef GL_TESS_CONTROL_SHADER
#define GL_TESS_CONTROL_SHADER            0x8E88
#endif
#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER                 0x91B9
#endif


HLSLCC_API void HLSLCC_APIENTRY HLSLcc_SetMemoryFunctions(void* (*malloc_override)(size_t),void* (*calloc_override)(size_t,size_t),void (*free_override)(void *),void* (*realloc_override)(void*,size_t))
{
	hlslcc_malloc = malloc_override;
	hlslcc_calloc = calloc_override;
	hlslcc_free = free_override;	
	hlslcc_realloc = realloc_override;
}

static void ClearDependencyData(SHADER_TYPE eType, GLSLCrossDependencyData* depends)
{
    if(depends == NULL)
    {
        return;
    }

    switch(eType)
    {
        case PIXEL_SHADER:
        {
            uint32_t i;
            for(i=0;i<MAX_SHADER_VEC4_INPUT; ++i)
            {
                depends->aePixelInputInterpolation[i] = INTERPOLATION_UNDEFINED;
            }
            break;
        }
        case HULL_SHADER:
        {
            depends->eTessPartitioning = TESSELLATOR_PARTITIONING_UNDEFINED;
            depends->eTessOutPrim = TESSELLATOR_OUTPUT_UNDEFINED;
            break;
        }
    }
}

extern void AddAssignToDest(HLSLCrossCompilerContext* psContext, const Operand* psDest, SHADER_VARIABLE_TYPE eSrcType, uint32_t ui32SrcElementCount, int* pNeedsParenthesis);
extern void AddAssignPrologue(HLSLCrossCompilerContext *psContext, int numParenthesis);

void AddIndentation(HLSLCrossCompilerContext* psContext)
{
	int i;
	int indent = psContext->indent;
	bstring glsl = *psContext->currentGLSLString;
	for(i=0; i < indent; ++i)
	{
		bcatcstr(glsl, "    ");
	}
}

uint32_t AddImport(HLSLCrossCompilerContext* psContext, SYMBOL_TYPE eType, uint32_t ui32ID, uint32_t ui32Default)
{
	bstring glsl = *psContext->currentGLSLString;
	uint32_t ui32Symbol = psContext->psShader->sInfo.ui32NumImports;

	psContext->psShader->sInfo.psImports = (Symbol*)hlslcc_realloc(psContext->psShader->sInfo.psImports, (ui32Symbol + 1) * sizeof(Symbol));
	++psContext->psShader->sInfo.ui32NumImports;

	bformata(glsl, "#ifndef IMPORT_%d\n", ui32Symbol);
	bformata(glsl, "#define IMPORT_%d %d\n", ui32Symbol, ui32Default);
	bformata(glsl, "#endif\n", ui32Symbol);

	psContext->psShader->sInfo.psImports[ui32Symbol].eType = eType;
	psContext->psShader->sInfo.psImports[ui32Symbol].ui32ID = ui32ID;
	psContext->psShader->sInfo.psImports[ui32Symbol].ui32Value = ui32Default;

	return ui32Symbol;
}

uint32_t AddExport(HLSLCrossCompilerContext* psContext, SYMBOL_TYPE eType, uint32_t ui32ID, uint32_t ui32Value)
{
	uint32_t ui32Param = psContext->psShader->sInfo.ui32NumExports;

	psContext->psShader->sInfo.psExports = (Symbol*)hlslcc_realloc(psContext->psShader->sInfo.psExports, (ui32Param + 1) * sizeof(Symbol));
	++psContext->psShader->sInfo.ui32NumExports;

	psContext->psShader->sInfo.psExports[ui32Param].eType = eType;
	psContext->psShader->sInfo.psExports[ui32Param].ui32ID = ui32ID;
	psContext->psShader->sInfo.psExports[ui32Param].ui32Value = ui32Value;

	return ui32Param;
}

void AddVersionDependentCode(HLSLCrossCompilerContext* psContext)
{
	bstring glsl = *psContext->currentGLSLString;
	uint32_t ui32DepthClampImp;

	if (psContext->psShader->ui32MajorVersion > 3 && psContext->psShader->eTargetLanguage != LANG_ES_300 && psContext->psShader->eTargetLanguage != LANG_ES_310 && !(psContext->psShader->eTargetLanguage >= LANG_330))
	{
		//DX10+ bycode format requires the ability to treat registers
		//as raw bits. ES3.0+ has that built-in, also 330 onwards
		bcatcstr(glsl,"#extension GL_ARB_shader_bit_encoding : require\n");
	
		bcatcstr(glsl, "int RepCounter;\n");
		bcatcstr(glsl, "int LoopCounter;\n");
		bcatcstr(glsl, "int ZeroBasedCounter;\n");
		if(psContext->psShader->eShaderType == VERTEX_SHADER)
		{
			uint32_t texCoord;
			bcatcstr(glsl, "ivec4 Address;\n");

			if(InOutSupported(psContext->psShader->eTargetLanguage))
			{
				bcatcstr(glsl, "out vec4 OffsetColour;\n");
				bcatcstr(glsl, "out vec4 BaseColour;\n");

				bcatcstr(glsl, "out vec4 Fog;\n");

				for(texCoord=0; texCoord<8; ++texCoord)
				{
					bformata(glsl, "out vec4 TexCoord%d;\n", texCoord);
				}
			}
			else
			{
				bcatcstr(glsl, "varying vec4 OffsetColour;\n");
				bcatcstr(glsl, "varying vec4 BaseColour;\n");

				bcatcstr(glsl, "varying vec4 Fog;\n");

				for(texCoord=0; texCoord<8; ++texCoord)
				{
					bformata(glsl, "varying vec4 TexCoord%d;\n", texCoord);
				}
			}
		}
		else
		{
			uint32_t renderTargets, texCoord;

			bcatcstr(glsl, "varying vec4 OffsetColour;\n");
			bcatcstr(glsl, "varying vec4 BaseColour;\n");

			bcatcstr(glsl, "varying vec4 Fog;\n");

			for(texCoord=0; texCoord<8; ++texCoord)
			{
				bformata(glsl, "varying vec4 TexCoord%d;\n", texCoord);
			}

			for(renderTargets=0; renderTargets<8; ++renderTargets)
			{
				bformata(glsl, "#define Output%d gl_FragData[%d]\n", renderTargets, renderTargets);
			}
		}
	}

	if (psContext->psShader->sInfo.ui32NumUniformBuffers > 0)
	{
		bcatcstr(glsl, "#extension GL_ARB_enhanced_layouts : enable\n");
	}

	//	bcatcstr(glsl, "#extension GL_EXT_shader_implicit_conversions : enable\n"); // ES3.1

	// if ((psContext->psShader->eTargetLanguage >= LANG_320) && (psContext->psShader->eTargetLanguage < LANG_400))
	// {
	// 	bcatcstr(glsl, "#extension GL_ARB_gpu_shader5 : require\n"); // 3.2+
	// }
	// else if ((psContext->psShader->eTargetLanguage >= LANG_200) && (psContext->psShader->eTargetLanguage < LANG_320))
	// {
	// 	bcatcstr(glsl, "#extension GL_EXT_gpu_shader4 : require\n"); // 2.0+
	// }

	if (!HaveCompute(psContext->psShader->eTargetLanguage))
	{
		if (psContext->psShader->eShaderType == COMPUTE_SHADER)
		{
			bcatcstr(glsl,"#extension GL_ARB_compute_shader : enable\n");
			bcatcstr(glsl,"#extension GL_ARB_shader_storage_buffer_object : enable\n");
		}
	}

	if (!HaveAtomicMem(psContext->psShader->eTargetLanguage) ||
		!HaveAtomicCounter(psContext->psShader->eTargetLanguage))
	{
		if (psContext->psShader->aiOpcodeUsed[OPCODE_IMM_ATOMIC_ALLOC] ||
			psContext->psShader->aiOpcodeUsed[OPCODE_IMM_ATOMIC_CONSUME] ||
			psContext->psShader->aiOpcodeUsed[OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED])
		{
			bcatcstr(glsl,"#extension GL_ARB_shader_atomic_counters : enable\n");
			bcatcstr(glsl,"#extension GL_ARB_shader_storage_buffer_object : enable\n");
		}
	}

	if (!HaveGather(psContext->psShader->eTargetLanguage) ||
		!HaveGatherCompareComponent(psContext->psShader->eTargetLanguage))
	{
		if (psContext->psShader->aiOpcodeUsed[OPCODE_GATHER4] ||
			psContext->psShader->aiOpcodeUsed[OPCODE_GATHER4_PO_C] ||
			psContext->psShader->aiOpcodeUsed[OPCODE_GATHER4_PO] ||
			psContext->psShader->aiOpcodeUsed[OPCODE_GATHER4_C])
		{
			bcatcstr(glsl,"#extension GL_ARB_texture_gather : enable\n");
		}
	}

	if (!HaveGatherNonConstOffset(psContext->psShader->eTargetLanguage))
	{
		if (psContext->psShader->aiOpcodeUsed[OPCODE_GATHER4_PO_C] ||
			psContext->psShader->aiOpcodeUsed[OPCODE_GATHER4_PO])
		{
			bcatcstr(glsl,"#extension GL_ARB_gpu_shader5 : enable\n");
		}
	}

	if (!HaveQueryLod(psContext->psShader->eTargetLanguage))
	{
		if (psContext->psShader->aiOpcodeUsed[OPCODE_LOD])
		{
			bcatcstr(glsl,"#extension GL_ARB_texture_query_lod : enable\n");
		}
	}

	if (!HaveQueryLevels(psContext->psShader->eTargetLanguage))
	{
		if (psContext->psShader->aiOpcodeUsed[OPCODE_RESINFO])
		{
			bcatcstr(glsl,"#extension GL_ARB_texture_query_levels : enable\n");
		}
	}

	if(!HaveImageLoadStore(psContext->psShader->eTargetLanguage))
	{
		if(psContext->psShader->aiOpcodeUsed[OPCODE_STORE_UAV_TYPED] ||
			psContext->psShader->aiOpcodeUsed[OPCODE_STORE_RAW] ||
			psContext->psShader->aiOpcodeUsed[OPCODE_STORE_STRUCTURED])
		{
			bcatcstr(glsl,"#extension GL_ARB_shader_image_load_store : enable\n");
			bcatcstr(glsl,"#extension GL_ARB_shader_bit_encoding : enable\n");
		}
		else
			if(psContext->psShader->aiOpcodeUsed[OPCODE_LD_UAV_TYPED] ||
				psContext->psShader->aiOpcodeUsed[OPCODE_LD_RAW] ||
				psContext->psShader->aiOpcodeUsed[OPCODE_LD_STRUCTURED])
			{
				bcatcstr(glsl,"#extension GL_ARB_shader_image_load_store : enable\n");
			}
	}


	// #extension directive must occur before any non-preprocessor token
	if (EmulateDepthClamp(psContext->psShader->eTargetLanguage) && (psContext->psShader->eShaderType == VERTEX_SHADER || psContext->psShader->eShaderType == PIXEL_SHADER))
	{
		char* szInOut = psContext->psShader->eShaderType == VERTEX_SHADER ? "out" : "in";
		ui32DepthClampImp = AddImport(psContext, SYMBOL_EMULATE_DEPTH_CLAMP, 0, 0);

		bformata(glsl, "#if IMPORT_%d > 0\n", ui32DepthClampImp);
		if (!HaveNoperspectiveInterpolation(psContext->psShader->eTargetLanguage))
		{
			bcatcstr(glsl, "#ifdef GL_NV_shader_noperspective_interpolation\n");
			bcatcstr(glsl, "#extension GL_NV_shader_noperspective_interpolation : enable\n");
			bformata(glsl, "#endif\n");
		}
		bformata(glsl, "#endif\n");
	}

	if((psContext->flags & HLSLCC_FLAG_ORIGIN_UPPER_LEFT)
		&& (psContext->psShader->eTargetLanguage >= LANG_150)
		&& (psContext->psShader->eShaderType == PIXEL_SHADER))
	{
		bcatcstr(glsl,"layout(origin_upper_left) in vec4 gl_FragCoord;\n");
	}

	if((psContext->flags & HLSLCC_FLAG_PIXEL_CENTER_INTEGER)
		&& (psContext->psShader->eTargetLanguage >= LANG_150))
	{
		bcatcstr(glsl,"layout(pixel_center_integer) in vec4 gl_FragCoord;\n");
	}

	/* For versions which do not support a vec1 (currently all versions) */
	bcatcstr(glsl,"struct vec1 {\n");
	if (psContext->psShader->eTargetLanguage == LANG_ES_300 || psContext->psShader->eTargetLanguage == LANG_ES_310 || psContext->psShader->eTargetLanguage == LANG_ES_100)
		bcatcstr(glsl,"\thighp float x;\n");
	else
		bcatcstr(glsl,"\tfloat x;\n");
	bcatcstr(glsl,"};\n");

	if(HaveUVec(psContext->psShader->eTargetLanguage))
	{
		bcatcstr(glsl,"struct uvec1 {\n");
		bcatcstr(glsl,"\tuint x;\n");
		bcatcstr(glsl,"};\n");
	}

	bcatcstr(glsl,"struct ivec1 {\n");
	bcatcstr(glsl,"\tint x;\n");
	bcatcstr(glsl,"};\n");

	/*
	OpenGL 4.1 API spec:
	To use any built-in input or output in the gl_PerVertex block in separable
	program objects, shader code must redeclare that block prior to use.
	*/
	if(psContext->psShader->eShaderType == VERTEX_SHADER && psContext->psShader->eTargetLanguage >= LANG_410)
	{
		bcatcstr(glsl, "out gl_PerVertex {\n");
		bcatcstr(glsl, "\tvec4 gl_Position;\n");
	//	bcatcstr(glsl, "\tfloat gl_PointSize;\n");
	//	bcatcstr(glsl, "\tfloat gl_ClipDistance[];\n");
		bcatcstr(glsl, "};\n");
	}

	//The fragment language has no default precision qualifier for floating point types.
	if(psContext->psShader->eShaderType == PIXEL_SHADER &&
		psContext->psShader->eTargetLanguage == LANG_ES_100 || psContext->psShader->eTargetLanguage == LANG_ES_300  || psContext->psShader->eTargetLanguage == LANG_ES_310)
	{
		bcatcstr(glsl,"precision highp float;\n");
	}

	/* There is no default precision qualifier for the following sampler types in either the vertex or fragment language: */
	if(psContext->psShader->eTargetLanguage == LANG_ES_300 || psContext->psShader->eTargetLanguage == LANG_ES_310)
	{
		bcatcstr(glsl,"precision lowp sampler3D;\n");
		bcatcstr(glsl,"precision lowp samplerCubeShadow;\n");
		bcatcstr(glsl,"precision lowp sampler2DShadow;\n");
		bcatcstr(glsl,"precision lowp sampler2DArray;\n");
		bcatcstr(glsl,"precision lowp sampler2DArrayShadow;\n");
		bcatcstr(glsl,"precision lowp isampler2D;\n");
		bcatcstr(glsl,"precision lowp isampler3D;\n");
		bcatcstr(glsl,"precision lowp isamplerCube;\n");
		bcatcstr(glsl,"precision lowp isampler2DArray;\n");
		bcatcstr(glsl,"precision lowp usampler2D;\n");
		bcatcstr(glsl,"precision lowp usampler3D;\n");
		bcatcstr(glsl,"precision lowp usamplerCube;\n");
		bcatcstr(glsl,"precision lowp usampler2DArray;\n");

		if(psContext->psShader->eTargetLanguage == LANG_ES_310)
		{
			bcatcstr(glsl,"precision lowp isampler2DMS;\n");
			bcatcstr(glsl,"precision lowp usampler2D;\n");
			bcatcstr(glsl,"precision lowp usampler3D;\n");
			bcatcstr(glsl,"precision lowp usamplerCube;\n");
			bcatcstr(glsl,"precision lowp usampler2DArray;\n");
			bcatcstr(glsl,"precision lowp usampler2DMS;\n");
			bcatcstr(glsl,"precision lowp image2D;\n");
			bcatcstr(glsl,"precision lowp image3D;\n");
			bcatcstr(glsl,"precision lowp imageCube;\n");
			bcatcstr(glsl,"precision lowp image2DArray;\n");
			bcatcstr(glsl,"precision lowp iimage2D;\n");
			bcatcstr(glsl,"precision lowp iimage3D;\n");
			bcatcstr(glsl,"precision lowp iimageCube;\n");
			bcatcstr(glsl,"precision lowp uimage2DArray;\n");
			//Only highp is valid for atomic_uint
			bcatcstr(glsl,"precision highp atomic_uint;\n");
		}
		bcatcstr(glsl, "\n");
	}

	if(SubroutinesSupported(psContext->psShader->eTargetLanguage))
	{
		bcatcstr(glsl, "subroutine void SubroutineType();\n");
	}

	if(EmulateDepthClamp(psContext->psShader->eTargetLanguage) && (psContext->psShader->eShaderType == VERTEX_SHADER || psContext->psShader->eShaderType == PIXEL_SHADER))
	{
		uint32_t ui32DepthClampImp = AddImport(psContext, SYMBOL_EMULATE_DEPTH_CLAMP, 0, 0);
		char* szInOut = psContext->psShader->eShaderType == VERTEX_SHADER ? "out" : "in";

		bformata(glsl, "#if IMPORT_%d > 0\n", ui32DepthClampImp);
		if (!HaveNoperspectiveInterpolation(psContext->psShader->eTargetLanguage))
		{
			bcatcstr(glsl, "#ifdef GL_NV_shader_noperspective_interpolation\n");
			bcatcstr(glsl, "#extension GL_NV_shader_noperspective_interpolation : enable\n");
		}
		bcatcstr(glsl, "#define EMULATE_DEPTH_CLAMP 1\n");
		bformata(glsl, "noperspective %s float unclampedDepth;\n", szInOut);
		if (!HaveNoperspectiveInterpolation(psContext->psShader->eTargetLanguage))
		{
			bcatcstr(glsl, "#else\n");
			bcatcstr(glsl, "#define EMULATE_DEPTH_CLAMP 2\n");
			bformata(glsl, "%s float unclampedZ;\n", szInOut);
			bformata(glsl, "#endif\n");
		}
		bformata(glsl, "#endif\n");

		if (psContext->psShader->eShaderType == PIXEL_SHADER)
		{
			bcatcstr(psContext->earlyMain, "#ifdef EMULATE_DEPTH_CLAMP\n");
			bcatcstr(psContext->earlyMain, "#if EMULATE_DEPTH_CLAMP == 2\n");
			bcatcstr(psContext->earlyMain, "\tfloat unclampedDepth = gl_DepthRange.near + unclampedZ *  gl_FragCoord.w;\n");
			bcatcstr(psContext->earlyMain, "#endif\n");
			bcatcstr(psContext->earlyMain, "\tgl_FragDepth = clamp(unclampedDepth, 0.0, 1.0);\n");
			bcatcstr(psContext->earlyMain, "#endif\n");
		}
	}

	if (psContext->psShader->ui32MajorVersion <= 3)
	{
		bcatcstr(glsl, "int RepCounter;\n");
		bcatcstr(glsl, "int LoopCounter;\n");
		bcatcstr(glsl, "int ZeroBasedCounter;\n");
		if (psContext->psShader->eShaderType == VERTEX_SHADER)
		{
			uint32_t texCoord;
			bcatcstr(glsl, "ivec4 Address;\n");

			if (InOutSupported(psContext->psShader->eTargetLanguage))
			{
				bcatcstr(glsl, "out vec4 OffsetColour;\n");
				bcatcstr(glsl, "out vec4 BaseColour;\n");

				bcatcstr(glsl, "out vec4 Fog;\n");

				for (texCoord = 0; texCoord < 8; ++texCoord)
				{
					bformata(glsl, "out vec4 TexCoord%d;\n", texCoord);
				}
			}
			else
			{
				bcatcstr(glsl, "varying vec4 OffsetColour;\n");
				bcatcstr(glsl, "varying vec4 BaseColour;\n");

				bcatcstr(glsl, "varying vec4 Fog;\n");

				for (texCoord = 0; texCoord < 8; ++texCoord)
				{
					bformata(glsl, "varying vec4 TexCoord%d;\n", texCoord);
				}
			}
		}
		else
		{
			uint32_t renderTargets, texCoord;

			if (InOutSupported(psContext->psShader->eTargetLanguage))
			{
				bcatcstr(glsl, "in vec4 OffsetColour;\n");
				bcatcstr(glsl, "in vec4 BaseColour;\n");

				bcatcstr(glsl, "in vec4 Fog;\n");

				for (texCoord = 0; texCoord < 8; ++texCoord)
				{
					bformata(glsl, "in vec4 TexCoord%d;\n", texCoord);
				}
			}
			else
			{
				bcatcstr(glsl, "varying vec4 OffsetColour;\n");
				bcatcstr(glsl, "varying vec4 BaseColour;\n");

				bcatcstr(glsl, "varying vec4 Fog;\n");

				for (texCoord = 0; texCoord < 8; ++texCoord)
				{
					bformata(glsl, "varying vec4 TexCoord%d;\n", texCoord);
				}
			}

			if (psContext->psShader->eTargetLanguage > LANG_120)
			{
				bcatcstr(glsl, "out vec4 outFragData[8];\n");
				for (renderTargets = 0; renderTargets < 8; ++renderTargets)
				{
					bformata(glsl, "#define Output%d outFragData[%d]\n", renderTargets, renderTargets);
				}
			}
			else if (psContext->psShader->eTargetLanguage >= LANG_ES_300 && psContext->psShader->eTargetLanguage < LANG_120)
			{
				// ES 3 supports min 4 rendertargets, I guess this is reasonable lower limit for DX9 shaders
				bcatcstr(glsl, "out vec4 outFragData[4];\n");
				for (renderTargets = 0; renderTargets < 4; ++renderTargets)
				{
					bformata(glsl, "#define Output%d outFragData[%d]\n", renderTargets, renderTargets);
				}
			}
			else if (psContext->psShader->eTargetLanguage == LANG_ES_100)
			{
				bcatcstr(glsl, "#define Output0 gl_FragColor;\n");
			}
			else
			{
				for (renderTargets = 0; renderTargets < 8; ++renderTargets)
				{
					bformata(glsl, "#define Output%d gl_FragData[%d]\n", renderTargets, renderTargets);
				}
			}
		}
	}

    if((psContext->flags & HLSLCC_FLAG_ORIGIN_UPPER_LEFT)
        && (psContext->psShader->eTargetLanguage >= LANG_150))
    {
        bcatcstr(glsl,"layout(origin_upper_left) in vec4 gl_FragCoord;\n");
    }

    if((psContext->flags & HLSLCC_FLAG_PIXEL_CENTER_INTEGER)
        && (psContext->psShader->eTargetLanguage >= LANG_150))
    {
        bcatcstr(glsl,"layout(pixel_center_integer) in vec4 gl_FragCoord;\n");
    }
}

uint16_t GetOpcodeWriteMask(OPCODE_TYPE eOpcode)
{
	switch (eOpcode)
	{
	default:
		ASSERT(0);

		// No writes
	case OPCODE_ENDREP:
	case OPCODE_REP:
	case OPCODE_BREAK:
	case OPCODE_BREAKC:
	case OPCODE_CALL:
	case OPCODE_CALLC:
	case OPCODE_CASE:
	case OPCODE_CONTINUE:
	case OPCODE_CONTINUEC:
	case OPCODE_CUT:
	case OPCODE_DISCARD:
	case OPCODE_ELSE:
	case OPCODE_EMIT:
	case OPCODE_EMITTHENCUT:
	case OPCODE_ENDIF:
	case OPCODE_ENDLOOP:
	case OPCODE_ENDSWITCH:
	case OPCODE_IF:
	case OPCODE_LABEL:
	case OPCODE_LOOP:
	case OPCODE_NOP:
	case OPCODE_RET:
	case OPCODE_RETC:
	case OPCODE_SWITCH:
	case OPCODE_HS_DECLS:
	case OPCODE_HS_CONTROL_POINT_PHASE:
	case OPCODE_HS_FORK_PHASE:
	case OPCODE_HS_JOIN_PHASE:
	case OPCODE_EMIT_STREAM:
	case OPCODE_CUT_STREAM:
	case OPCODE_EMITTHENCUT_STREAM:
	case OPCODE_INTERFACE_CALL:
	case OPCODE_STORE_UAV_TYPED:
	case OPCODE_STORE_RAW:
	case OPCODE_STORE_STRUCTURED:
	case OPCODE_ATOMIC_AND:
	case OPCODE_ATOMIC_OR:
	case OPCODE_ATOMIC_XOR:
	case OPCODE_ATOMIC_CMP_STORE:
	case OPCODE_ATOMIC_IADD:
	case OPCODE_ATOMIC_IMAX:
	case OPCODE_ATOMIC_IMIN:
	case OPCODE_ATOMIC_UMAX:
	case OPCODE_ATOMIC_UMIN:
	case OPCODE_SYNC:
	case OPCODE_ABORT:
	case OPCODE_DEBUG_BREAK:
		return 0;

		// Write to 0
	case OPCODE_POW:
	case OPCODE_DP2ADD:
	case OPCODE_LRP:
	case OPCODE_ADD:
	case OPCODE_AND:
	case OPCODE_DERIV_RTX:
	case OPCODE_DERIV_RTY:
	case OPCODE_DEFAULT:
	case OPCODE_DIV:
	case OPCODE_DP2:
	case OPCODE_DP3:
	case OPCODE_DP4:
	case OPCODE_EXP:
	case OPCODE_FRC:
	case OPCODE_ITOF:
	case OPCODE_LOG:
	case OPCODE_LT:
	case OPCODE_MAD:
	case OPCODE_MIN:
	case OPCODE_MAX:
	case OPCODE_MUL:
	case OPCODE_ROUND_NE:
	case OPCODE_ROUND_NI:
	case OPCODE_ROUND_PI:
	case OPCODE_ROUND_Z:
	case OPCODE_RSQ:
	case OPCODE_SQRT:
	case OPCODE_UTOF:
	case OPCODE_SAMPLE_POS:
	case OPCODE_SAMPLE_INFO:
	case OPCODE_DERIV_RTX_COARSE:
	case OPCODE_DERIV_RTX_FINE:
	case OPCODE_DERIV_RTY_COARSE:
	case OPCODE_DERIV_RTY_FINE:
	case OPCODE_RCP:
	case OPCODE_F32TOF16:
	case OPCODE_F16TOF32:
	case OPCODE_DTOF:
	case OPCODE_EQ:
	case OPCODE_FTOU:
	case OPCODE_GE:
	case OPCODE_IEQ:
	case OPCODE_IGE:
	case OPCODE_ILT:
	case OPCODE_NE:
	case OPCODE_NOT:
	case OPCODE_OR:
	case OPCODE_ULT:
	case OPCODE_UGE:
	case OPCODE_UMAD:
	case OPCODE_XOR:
	case OPCODE_UMAX:
	case OPCODE_UMIN:
	case OPCODE_USHR:
	case OPCODE_COUNTBITS:
	case OPCODE_FIRSTBIT_HI:
	case OPCODE_FIRSTBIT_LO:
	case OPCODE_FIRSTBIT_SHI:
	case OPCODE_UBFE:
	case OPCODE_BFI:
	case OPCODE_BFREV:
	case OPCODE_IMM_ATOMIC_AND:
	case OPCODE_IMM_ATOMIC_OR:
	case OPCODE_IMM_ATOMIC_XOR:
	case OPCODE_IMM_ATOMIC_EXCH:
	case OPCODE_IMM_ATOMIC_CMP_EXCH:
	case OPCODE_IMM_ATOMIC_UMAX:
	case OPCODE_IMM_ATOMIC_UMIN:
	case OPCODE_DEQ:
	case OPCODE_DGE:
	case OPCODE_DLT:
	case OPCODE_DNE:
	case OPCODE_MSAD:
	case OPCODE_DTOU:
	case OPCODE_FTOI:
	case OPCODE_IADD:
	case OPCODE_IMAD:
	case OPCODE_IMAX:
	case OPCODE_IMIN:
	case OPCODE_IMUL:
	case OPCODE_INE:
	case OPCODE_INEG:
	case OPCODE_ISHL:
	case OPCODE_ISHR:
	case OPCODE_BUFINFO:
	case OPCODE_IBFE:
	case OPCODE_IMM_ATOMIC_ALLOC:
	case OPCODE_IMM_ATOMIC_CONSUME:
	case OPCODE_IMM_ATOMIC_IADD:
	case OPCODE_IMM_ATOMIC_IMAX:
	case OPCODE_IMM_ATOMIC_IMIN:
	case OPCODE_DTOI:
	case OPCODE_DADD:
	case OPCODE_DMAX:
	case OPCODE_DMIN:
	case OPCODE_DMUL:
	case OPCODE_DMOV:
	case OPCODE_DMOVC:
	case OPCODE_FTOD:
	case OPCODE_DDIV:
	case OPCODE_DFMA:
	case OPCODE_DRCP:
	case OPCODE_ITOD:
	case OPCODE_UTOD:
	case OPCODE_LD:
	case OPCODE_LD_MS:
	case OPCODE_RESINFO:
	case OPCODE_SAMPLE:
	case OPCODE_SAMPLE_C:
	case OPCODE_SAMPLE_C_LZ:
	case OPCODE_SAMPLE_L:
	case OPCODE_SAMPLE_D:
	case OPCODE_SAMPLE_B:
	case OPCODE_LOD:
	case OPCODE_GATHER4:
	case OPCODE_GATHER4_C:
	case OPCODE_GATHER4_PO:
	case OPCODE_GATHER4_PO_C:
	case OPCODE_LD_UAV_TYPED:
	case OPCODE_LD_RAW:
	case OPCODE_LD_STRUCTURED:
	case OPCODE_EVAL_SNAPPED:
	case OPCODE_EVAL_SAMPLE_INDEX:
	case OPCODE_EVAL_CENTROID:
	case OPCODE_MOV:
	case OPCODE_MOVC:
		return 1u << 0;

		// Write to 0, 1
	case OPCODE_SINCOS:
	case OPCODE_UDIV:
	case OPCODE_UMUL:
	case OPCODE_UADDC:
	case OPCODE_USUBB:
	case OPCODE_SWAPC:
		return (1u << 0) | (1u << 1);
	}
}

void CreateTracingInfo(Shader* psShader)
{
	VariableTraceInfo asInputVarsInfo[MAX_SHADER_VEC4_INPUT * 4];
	uint32_t ui32NumInputVars = 0;
	uint32_t uInputVec, uInstruction;

	psShader->sInfo.ui32NumTraceSteps = psShader->ui32InstCount + 1;
	psShader->sInfo.psTraceSteps = hlslcc_malloc(sizeof(StepTraceInfo) * psShader->sInfo.ui32NumTraceSteps);

	for (uInputVec = 0; uInputVec < psShader->sInfo.ui32NumInputSignatures; ++uInputVec)
	{
		uint32_t ui32RWMask = psShader->sInfo.psInputSignatures[uInputVec].ui32ReadWriteMask;
		uint8_t ui8Component = 0;

		while (ui32RWMask != 0)
		{
			if (ui32RWMask & 1)
			{
				TRACE_VARIABLE_TYPE eType;
				switch (psShader->sInfo.psInputSignatures[uInputVec].eComponentType)
				{
				default:
					ASSERT(0);
				case INOUT_COMPONENT_UNKNOWN:
				case INOUT_COMPONENT_UINT32:
					eType = TRACE_VARIABLE_UINT;
					break;
				case INOUT_COMPONENT_SINT32:
					eType = TRACE_VARIABLE_SINT;
					break;
				case INOUT_COMPONENT_FLOAT32:
					eType = TRACE_VARIABLE_FLOAT;
					break;
				}

				asInputVarsInfo[ui32NumInputVars].eGroup = TRACE_VARIABLE_INPUT;
				asInputVarsInfo[ui32NumInputVars].eType = eType;
				asInputVarsInfo[ui32NumInputVars].ui8Index = (uint8_t)(psShader->sInfo.psInputSignatures[uInputVec].ui32Register);
				asInputVarsInfo[ui32NumInputVars].ui8Component = ui8Component;
				++ui32NumInputVars;
			}
			ui32RWMask >>= 1;
			++ui8Component;
		}
	}

	psShader->sInfo.psTraceSteps[0].ui32NumVariables = ui32NumInputVars;
	psShader->sInfo.psTraceSteps[0].psVariables = hlslcc_malloc(sizeof(VariableTraceInfo) * ui32NumInputVars);
	memcpy(psShader->sInfo.psTraceSteps[0].psVariables, asInputVarsInfo, sizeof(VariableTraceInfo) * ui32NumInputVars);

	for (uInstruction = 0; uInstruction < psShader->ui32InstCount; ++uInstruction)
	{
		VariableTraceInfo* psStepVars = NULL;
		uint32_t ui32StepVarsCapacity = 0;
		uint32_t ui32StepVarsSize = 0;
		uint32_t auStepDirtyVecMask[MAX_TEMP_VEC4 + MAX_SHADER_VEC4_OUTPUT] = {0};
		uint8_t auStepCompTypeMask[4 * (MAX_TEMP_VEC4 + MAX_SHADER_VEC4_OUTPUT)]= {0};
		uint32_t uOpcodeWriteMask = GetOpcodeWriteMask(psShader->psInst[uInstruction].eOpcode);
		uint32_t uOperand, uStepVec;

		for (uOperand = 0; uOperand < psShader->psInst[uInstruction].ui32NumOperands; ++uOperand)
		{
			if (uOpcodeWriteMask & (1 << uOperand))
			{
				uint32_t ui32OperandCompMask = ConvertOperandSwizzleToComponentMask(&psShader->psInst[uInstruction].asOperands[uOperand]);
				uint32_t ui32Register = psShader->psInst[uInstruction].asOperands[uOperand].ui32RegisterNumber;
				uint32_t ui32VecOffset = 0;
				uint8_t ui8Component = 0;
				switch (psShader->psInst[uInstruction].asOperands[uOperand].eType)
				{
				case OPERAND_TYPE_TEMP:   ui32VecOffset = 0; break;
				case OPERAND_TYPE_OUTPUT: ui32VecOffset = MAX_TEMP_VEC4; break;
				default: continue;
				}

				auStepDirtyVecMask[ui32VecOffset + ui32Register] |= ui32OperandCompMask;
				while (ui32OperandCompMask)
				{
					ASSERT(ui8Component < 4);
					if (ui32OperandCompMask & 1)
					{
						TRACE_VARIABLE_TYPE eOperandCompType = TRACE_VARIABLE_UNKNOWN;
						switch (psShader->psInst[uInstruction].asOperands[uOperand].aeDataType[ui8Component])
						{
						case SVT_INT:
							eOperandCompType = TRACE_VARIABLE_SINT;
							break;
						case SVT_FLOAT:
							eOperandCompType = TRACE_VARIABLE_FLOAT;
							break;
						case SVT_UINT:
							eOperandCompType = TRACE_VARIABLE_UINT;
							break;
						case SVT_BOOL:
							eOperandCompType = TRACE_VARIABLE_BOOL;
							break;
						case SVT_DOUBLE:
							eOperandCompType = TRACE_VARIABLE_DOUBLE;
							break;
						}
						if (auStepCompTypeMask[4 * (ui32VecOffset + ui32Register) + ui8Component] == 0)
							auStepCompTypeMask[4 * (ui32VecOffset + ui32Register) + ui8Component] = 1u + (uint8_t)eOperandCompType;
						else if (auStepCompTypeMask[4 * (ui32VecOffset + ui32Register) + ui8Component] != eOperandCompType)
							auStepCompTypeMask[4 * (ui32VecOffset + ui32Register) + ui8Component] = 1u + (uint8_t)TRACE_VARIABLE_UNKNOWN;
					}
					ui32OperandCompMask >>= 1;
					++ui8Component;
				}
			}
		}

		for (uStepVec = 0; uStepVec < MAX_TEMP_VEC4 + MAX_SHADER_VEC4_OUTPUT; ++uStepVec)
		{
			TRACE_VARIABLE_GROUP eGroup;
			uint32_t uBase;
			uint8_t ui8Component = 0;
			if (uStepVec < MAX_TEMP_VEC4)
			{
				eGroup = TRACE_VARIABLE_TEMP;
				uBase = 0;
			}
			else
			{
				eGroup = TRACE_VARIABLE_OUTPUT;
				uBase = MAX_TEMP_VEC4;
			}

			while (auStepDirtyVecMask[uStepVec] != 0)
			{
				if (auStepDirtyVecMask[uStepVec] & 1)
				{
					if (ui32StepVarsCapacity == ui32StepVarsSize)
					{
						ui32StepVarsCapacity = (1 > ui32StepVarsCapacity ? 1 : ui32StepVarsCapacity) * 16;
						if (psStepVars == NULL)
							psStepVars = hlslcc_malloc(ui32StepVarsCapacity * sizeof(VariableTraceInfo));
						else
							psStepVars = hlslcc_realloc(psStepVars, ui32StepVarsCapacity * sizeof(VariableTraceInfo));
					}
					ASSERT(ui32StepVarsSize < ui32StepVarsCapacity);

					psStepVars[ui32StepVarsSize].eGroup = eGroup;
					psStepVars[ui32StepVarsSize].eType = auStepCompTypeMask[4 * uStepVec + ui8Component] == 0 ? TRACE_VARIABLE_UNKNOWN : (TRACE_VARIABLE_TYPE)(auStepCompTypeMask[4 * uStepVec + ui8Component] - 1);
					psStepVars[ui32StepVarsSize].ui8Component = ui8Component;
					psStepVars[ui32StepVarsSize].ui8Index = (uint8_t)(uStepVec - uBase);
					++ui32StepVarsSize;
				}

				++ui8Component;
				auStepDirtyVecMask[uStepVec] >>= 1;
			}
		}

		psShader->sInfo.psTraceSteps[1 + uInstruction].ui32NumVariables = ui32StepVarsSize;
		psShader->sInfo.psTraceSteps[1 + uInstruction].psVariables = psStepVars;
	}
}

void WriteTraceDeclarations(HLSLCrossCompilerContext* psContext)
{
	bstring glsl = *psContext->currentGLSLString;

	AddIndentation(psContext); bcatcstr(glsl, "layout (std430) buffer Trace\n");
	AddIndentation(psContext); bcatcstr(glsl, "{\n");
	++psContext->indent;
	AddIndentation(psContext); bcatcstr(glsl, "uint uTraceSize;\n");
	AddIndentation(psContext); bcatcstr(glsl, "uint uTraceStride;\n");
	AddIndentation(psContext); bcatcstr(glsl, "uint uTraceCapacity;\n");
	switch (psContext->psShader->eShaderType)
	{
	case PIXEL_SHADER:
		AddIndentation(psContext); bcatcstr(glsl, "float fTracePixelCoordX;\n");
		AddIndentation(psContext); bcatcstr(glsl, "float fTracePixelCoordY;\n");
		break;
	case VERTEX_SHADER:
		AddIndentation(psContext); bcatcstr(glsl, "uint uTraceVertexID;\n");
		break;
	case COMPUTE_SHADER:
		AddIndentation(psContext); bcatcstr(glsl, "uint uTraceGloballInvocationIDX;\n");
		AddIndentation(psContext); bcatcstr(glsl, "uint uTraceGloballInvocationIDY;\n");
		AddIndentation(psContext); bcatcstr(glsl, "uint uTraceGloballInvocationIDZ;\n");
		break;
	default:
		AddIndentation(psContext); bcatcstr(glsl, "// Trace ID not implelemented for this shader type\n");
		break;
	}
	AddIndentation(psContext); bcatcstr(glsl, "uint auTraceValues[];\n");
	--psContext->indent;
	AddIndentation(psContext); bcatcstr(glsl, "};\n");
}

void WriteTraceDefinitions(HLSLCrossCompilerContext* psContext)
{
	bstring glsl = *psContext->currentGLSLString;

	bcatcstr(glsl, "#define SETUP_TRACING() \\\n");
	AddIndentation(psContext); bcatcstr(glsl, "bool bRecord = ");
	switch (psContext->psShader->eShaderType)
	{
	case VERTEX_SHADER:
		bcatcstr(glsl, "uint(gl_VertexID) == uTraceVertexID");
		break;
	case PIXEL_SHADER:
		bcatcstr(glsl, "max(abs(gl_FragCoord.x - fTracePixelCoordX), abs(gl_FragCoord.y - fTracePixelCoordY)) <= 0.5");
		break;
	case COMPUTE_SHADER:
		bcatcstr(glsl, "gl_GlobalInvocationID == uvec3(uTraceGloballInvocationIDX, uTraceGloballInvocationIDY, uTraceGloballInvocationIDZ)");
		break;
	default:
		bcatcstr(glsl, "/* Trace condition not implemented for this shader type */");
		bcatcstr(glsl, "false");
		break;
	}
	bcatcstr(glsl, "; \\\n");
	AddIndentation(psContext); bcatcstr(glsl, "uint uTraceIndex = atomicAdd(uTraceSize, uTraceStride * (bRecord ? 1 : 0)); \\\n");
	AddIndentation(psContext); bcatcstr(glsl, "uint uTraceEnd = uTraceIndex + uTraceStride; \\\n");
	AddIndentation(psContext); bcatcstr(glsl, "bRecord = bRecord && uTraceEnd <= uTraceCapacity; \\\n");
	AddIndentation(psContext); bcatcstr(glsl, "uTraceEnd *= (bRecord ? 1 : 0); \n");
	bcatcstr(glsl, "#define TRACE(X) auTraceValues[min(++uTraceIndex, uTraceEnd)] = X\n");
}

void WritePreStepsTrace(HLSLCrossCompilerContext* psContext, StepTraceInfo* psStep)
{
	uint32_t uVar;
	bstring glsl = *psContext->currentGLSLString;

	AddIndentation(psContext); bcatcstr(glsl, "SETUP_TRACING();\n");

	if (psStep->ui32NumVariables > 0)
	{
		AddIndentation(psContext); bformata(glsl, "/* BEGIN */ TRACE(0u);");

		for (uVar = 0; uVar < psStep->ui32NumVariables; ++uVar)
		{
			VariableTraceInfo* psVar = &psStep->psVariables[uVar];
			ASSERT(psVar->eGroup == TRACE_VARIABLE_INPUT);
			if (psVar->eGroup == TRACE_VARIABLE_INPUT)
			{
				bcatcstr(glsl, " TRACE(");

				switch (psVar->eType)
				{
				case TRACE_VARIABLE_FLOAT:
					bcatcstr(glsl, "floatBitsToUint(");
					break;
				case TRACE_VARIABLE_SINT:
					bcatcstr(glsl, "uint(");
					break;
				case TRACE_VARIABLE_DOUBLE:
					ASSERT(0);
					// Not implemented yet;
					break;
				}

				bformata(glsl, "Input%d.%c", psVar->ui8Index, "xyzw"[psVar->ui8Component]);

				switch (psVar->eType)
				{
				case TRACE_VARIABLE_FLOAT:
				case TRACE_VARIABLE_SINT:
					bcatcstr(glsl, ")");
					break;
				}

				bcatcstr(glsl, ");");
			}
		}

		bcatcstr(glsl, "\n");
	}
}

void WritePostStepTrace(HLSLCrossCompilerContext* psContext, uint32_t uStep)
{
	Instruction* psInstruction = psContext->psShader->psInst + uStep;
	StepTraceInfo* psStep = psContext->psShader->sInfo.psTraceSteps + (1 + uStep);

	if (psStep->ui32NumVariables > 0)
	{
		uint32_t uVar;

		AddIndentation(psContext); bformata(psContext->glsl, "/* STEP %d */ TRACE(%du);", uStep + 1, uStep + 1);

		for (uVar = 0; uVar < psStep->ui32NumVariables; ++uVar)
		{
			VariableTraceInfo* psVar = &psStep->psVariables[uVar];
			uint16_t uOpcodeWriteMask = GetOpcodeWriteMask(psInstruction->eOpcode);
			uint8_t uOperand = 0;
			OPERAND_TYPE eOperandType = OPERAND_TYPE_NULL;
			SHADER_VARIABLE_TYPE eVarToFlags = TO_FLAG_NONE;
			Operand* psOperand = NULL;
			uint32_t uiIgnoreSwizzle = 0;

			switch (psVar->eGroup)
			{
			case TRACE_VARIABLE_TEMP:
				eOperandType = OPERAND_TYPE_TEMP;
				break;
			case TRACE_VARIABLE_OUTPUT:
				eOperandType = OPERAND_TYPE_OUTPUT;
				break;
			}

			if (psVar->eType == TRACE_VARIABLE_DOUBLE)
			{
				ASSERT(0);
				// Not implemented yet
				continue;
			}
			while (uOpcodeWriteMask)
			{
				if (uOpcodeWriteMask & 1)
				{
					if (eOperandType == psInstruction->asOperands[uOperand].eType &&
						psVar->ui8Index == psInstruction->asOperands[uOperand].ui32RegisterNumber)
					{
						psOperand = &psInstruction->asOperands[uOperand];
						break;
					}
				}
				uOpcodeWriteMask >>= 1;
				++uOperand;
			}

			if (psOperand == NULL)
			{
				ASSERT(0);
				continue;
			}

			bcatcstr(psContext->glsl, " TRACE(");

			TranslateVariableName(psContext, psOperand, TO_FLAG_UNSIGNED_INTEGER, &uiIgnoreSwizzle);
			ASSERT(uiIgnoreSwizzle == 0);

			bformata(psContext->glsl, ".%c);", "xyzw"[psVar->ui8Component]);
		}

		bcatcstr(psContext->glsl, "\n");
	}
}

void WriteEndTrace(HLSLCrossCompilerContext* psContext)
{
	AddIndentation(psContext); bcatcstr(psContext->glsl, "/* END */ TRACE(0xFFFFFFFFu);\n");
}

int FindEmbeddedResourceName(EmbeddedResourceName* psEmbeddedName, HLSLCrossCompilerContext* psContext, bstring name)
{
	int limit = psContext->glsl->slen;
	int offset = BSTR_ERR;
	int size = name->slen;
	int cursor = 0;
	char post = '\0';
	do 
	{
		offset = binstr(psContext->glsl, cursor, name);
		if (offset == BSTR_ERR)
			return 0;

		post = psContext->glsl->data[offset + size];
		if ((!isalpha(post) && (post != '_')))
			break;

		cursor += offset + 1;
		offset = BSTR_ERR;
	} while (cursor < limit);

	if (offset == BSTR_ERR || size > 0x3FF || offset > 0x7FFFF)
		return 0;

	psEmbeddedName->ui20Offset = offset;
	psEmbeddedName->ui12Size = size;
	return 1;
}

void IgnoreSampler(ShaderInfo* psInfo, uint32_t index)
{
	if (index + 1 < psInfo->ui32NumSamplers)
		psInfo->asSamplers[index] = psInfo->asSamplers[psInfo->ui32NumSamplers - 1];
	--psInfo->ui32NumSamplers;
}

void IgnoreResource(Resource* psResources, uint32_t* puSize, uint32_t index)
{
	if (index + 1 < *puSize)
		psResources[index] = psResources[*puSize - 1];
	--*puSize;
}

void FillInResourceDescriptions(HLSLCrossCompilerContext* psContext)
{
	const int useCombinedTextureSamplers = (psContext->flags & HLSLCC_FLAG_COMBINE_TEXTURE_SAMPLERS) ? 1 : 0;
	bstring resourceName = bfromcstralloc(MAX_REFLECT_STRING_LENGTH, "");
	Shader* psShader = psContext->psShader;
	uint32_t i;

	for (i = 0; i < psShader->sInfo.ui32NumSamplers; ++i)
	{
		Sampler* psSampler = psShader->sInfo.asSamplers + i;
		SamplerMask* psMask = &psSampler->sMask;
		if (psMask->bNormalSample || psMask->bCompareSample)
		{
			if (psMask->bNormalSample)
			{
				btrunc(resourceName, 0);
				TextureName(resourceName, psShader, psMask->ui10TextureBindPoint, !useCombinedTextureSamplers ? MAX_RESOURCE_BINDINGS : psMask->ui10SamplerBindPoint, 0);
				if (!FindEmbeddedResourceName(&psSampler->sNormalName, psContext, resourceName))
					psMask->bNormalSample = 0;
			}
			if (psMask->bCompareSample)
			{
				btrunc(resourceName, 0);
				TextureName(resourceName, psShader, psMask->ui10TextureBindPoint, !useCombinedTextureSamplers ? MAX_RESOURCE_BINDINGS : psMask->ui10SamplerBindPoint, 1);
				if (!FindEmbeddedResourceName(&psSampler->sCompareName, psContext, resourceName))
					psMask->bCompareSample = 0;
			}
			if (!psMask->bNormalSample && !psMask->bCompareSample)
				IgnoreSampler(&psShader->sInfo, i--); // Not used in the shader - ignore
		}
		else
		{
			btrunc(resourceName, 0);
			TextureName(resourceName, psShader, psMask->ui10TextureBindPoint, !useCombinedTextureSamplers ? MAX_RESOURCE_BINDINGS : psMask->ui10SamplerBindPoint, 0);
			if (!FindEmbeddedResourceName(&psSampler->sNormalName, psContext, resourceName))
				IgnoreSampler(&psShader->sInfo, i--); // Not used in the shader - ignore
		}
	}

	for (i = 0; i < psShader->sInfo.ui32NumImages; ++i)
	{
		Resource* psResources = psShader->sInfo.asImages;
		uint32_t* puSize = &psShader->sInfo.ui32NumImages;

		Resource* psResource = psResources + i;
		ResourceBinding* psBinding = NULL;
		if (!GetResourceFromBindingPoint(psResource->eGroup, psResource->ui32BindPoint, &psShader->sInfo, &psBinding))
		{
			ASSERT(0);
			IgnoreResource(psResources, puSize, i--);
		}

		btrunc(resourceName, 0);
		ConvertToUAVName(resourceName, psShader, psBinding->Name, psResource->ui32BindPoint);
		if (!FindEmbeddedResourceName(&psResource->sName, psContext, resourceName))
			IgnoreResource(psResources, puSize, i--);
	}

	for (i = 0; i < psShader->sInfo.ui32NumUniformBuffers; ++i)
	{
		Resource* psResources = psShader->sInfo.asUniformBuffers;
		uint32_t* puSize = &psShader->sInfo.ui32NumUniformBuffers;

		Resource* psResource = psResources + i;
		ConstantBuffer* psCB = NULL;
		GetConstantBufferFromBindingPoint(psResource->eGroup, psResource->ui32BindPoint, &psShader->sInfo, &psCB);

		btrunc(resourceName, 0);
		ConvertToUniformBufferName(resourceName, psShader, psCB->Name, psResource->ui32BindPoint);
		if (!FindEmbeddedResourceName(&psResource->sName, psContext, resourceName))
			IgnoreResource(psResources, puSize, i--);
	}

	for (i = 0; i < psShader->sInfo.ui32NumStorageBuffers; ++i)
	{
		Resource* psResources = psShader->sInfo.asStorageBuffers;
		uint32_t* puSize = &psShader->sInfo.ui32NumStorageBuffers;

		Resource* psResource = psResources + i;
		ConstantBuffer* psCB = NULL;
		GetConstantBufferFromBindingPoint(psResource->eGroup, psResource->ui32BindPoint, &psShader->sInfo, &psCB);

		btrunc(resourceName, 0);
		if (psResource->eGroup == RGROUP_UAV)
			ConvertToUAVName(resourceName, psShader, psCB->Name, psResource->ui32BindPoint);
		else
			ConvertToTextureName(resourceName, psShader, psCB->Name, NULL, psResource->ui32BindPoint, 0);
		if (!FindEmbeddedResourceName(&psResource->sName, psContext, resourceName))
			IgnoreResource(psResources, puSize, i--);
	}

	bdestroy(resourceName);
}

GLLang ChooseLanguage(Shader* psShader)
{
	// Depends on the HLSL shader model extracted from bytecode.
	switch(psShader->ui32MajorVersion)
	{
	case 5:
		{
			return LANG_430;
		}
	case 4:
		{
			return LANG_330;
		}
	default:
		{
			return LANG_120;
		}
	}
}

const char* GetVersionString(GLLang language)
{
	switch(language)
	{
	case LANG_ES_100:
		{
			return "#version 100\n";
			break;
		}
	case LANG_ES_300:
		{
			return "#version 300 es\n";
			break;
		}
	case LANG_ES_310:
		{
			return "#version 310 es\n";
			break;
		}
	case LANG_120:
		{
			return "#version 120\n";
			break;
		}
	case LANG_130:
		{
			return "#version 130\n";
			break;
		}
	case LANG_140:
		{
			return "#version 140\n";
			break;
		}
	case LANG_150:
		{
			return "#version 150\n";
			break;
		}
	case LANG_330:
		{
			return "#version 330\n";
			break;
		}
	case LANG_400:
		{
			return "#version 400\n";
			break;
		}
	case LANG_410:
		{
			return "#version 410\n";
			break;
		}
	case LANG_420:
		{
			return "#version 420\n";
			break;
		}
	case LANG_430:
		{
			return "#version 430\n";
			break;
		}
	case LANG_440:
		{
			return "#version 440\n";
			break;
	}
	case LANG_450:
		{
			return "#version 450\n";
			break;
		}
	default:
		{
			return "";
			break;
		}
	}
}

void TranslateToGLSL(HLSLCrossCompilerContext* psContext, GLLang* planguage, GLSLCrossDependencyData* dependencies, const GlExtensions *extensions)
{
	bstring glsl;
	uint32_t i;
	Shader* psShader = psContext->psShader;
	GLLang language = *planguage;
	uint32_t ui32InstCount = psShader->ui32InstCount;
	uint32_t ui32DeclCount = psShader->ui32DeclCount;

	psContext->indent = 0;

	/*psShader->sPhase[MAIN_PHASE].ui32InstanceCount = 1;
	psShader->sPhase[MAIN_PHASE].ppsDecl = hlslcc_malloc(sizeof(Declaration*));
	psShader->sPhase[MAIN_PHASE].ppsInst = hlslcc_malloc(sizeof(Instruction*));
	psShader->sPhase[MAIN_PHASE].pui32DeclCount = hlslcc_malloc(sizeof(uint32_t));
	psShader->sPhase[MAIN_PHASE].pui32InstCount = hlslcc_malloc(sizeof(uint32_t));*/

	if(language == LANG_DEFAULT)
	{
		language = ChooseLanguage(psShader);
		*planguage = language;
	}

	glsl = bfromcstralloc (1024, "");
	if (!(psContext->flags & HLSLCC_FLAG_NO_VERSION_STRING))
	{
		bcatcstr(glsl, GetVersionString(language));
	}

	if (psContext->flags & HLSLCC_FLAG_ADD_DEBUG_HEADER)
	{
		bstring version = glsl;
		glsl = psContext->debugHeader;
		bconcat(glsl, version);
		bdestroy(version);
	}

	psContext->glsl = glsl;
	psContext->earlyMain = bfromcstralloc (1024, "");
	for(i=0; i<NUM_PHASES;++i)
	{
		psContext->postShaderCode[i] = bfromcstralloc (1024, "");
	}
	psContext->currentGLSLString = &glsl;
	psShader->eTargetLanguage = language;
	psShader->extensions = (const struct GlExtensions*)extensions;
	psContext->currentPhase = MAIN_PHASE;
	psContext->currentInst = 0;

	if(extensions)
	{
		if(extensions->ARB_explicit_attrib_location)
			bcatcstr(glsl,"#extension GL_ARB_explicit_attrib_location : require\n");
		if(extensions->ARB_explicit_uniform_location)
			bcatcstr(glsl,"#extension GL_ARB_explicit_uniform_location : require\n");
		if(extensions->ARB_shading_language_420pack)
			bcatcstr(glsl,"#extension GL_ARB_shading_language_420pack : require\n");
	}

	ClearDependencyData(psShader->eShaderType, psContext->psDependencies);
	psContext->psShader->sInfo.ui32SymbolsOffset = blength(glsl);

	AddVersionDependentCode(psContext);

	if(psContext->flags & HLSLCC_FLAG_UNIFORM_BUFFER_OBJECT)
	{
		bcatcstr(glsl, "layout(std140) uniform;\n");
	}

	bcatcstr(glsl, "layout(std430) buffer;\n");

	//Special case. Can have multiple phases.
	if (psShader->eShaderType == HULL_SHADER)
	{
		int haveInstancedForkPhase = 0;			// Do we have an instanced fork phase?
		int isCurrentForkPhasedInstanced = 0;	// Is the current fork phase instanced?
		const char* asPhaseFuncNames[NUM_PHASES];
		uint32_t ui32PhaseFuncCallOrder[3];
		uint32_t ui32PhaseCallIndex;

		uint32_t ui32Phase;
		uint32_t ui32Instance;

		asPhaseFuncNames[MAIN_PHASE] = "";
		asPhaseFuncNames[HS_GLOBAL_DECL] = "";
		asPhaseFuncNames[HS_FORK_PHASE] = "fork_phase";
		asPhaseFuncNames[HS_CTRL_POINT_PHASE] = "control_point_phase";
		asPhaseFuncNames[HS_JOIN_PHASE] = "join_phase";

		ConsolidateHullTempVars(psShader);

		for(i=0; i < psShader->asPhase[HS_GLOBAL_DECL].pui32DeclCount[0]; ++i)
		{
			TranslateDeclaration(psContext, psShader->asPhase[HS_GLOBAL_DECL].ppsDecl[0]+i);
		}

		for(ui32Phase=HS_CTRL_POINT_PHASE; ui32Phase<NUM_PHASES; ui32Phase++)
		{
			psContext->currentPhase = ui32Phase;
			for(ui32Instance = 0; ui32Instance < psShader->asPhase[ui32Phase].ui32InstanceCount; ++ui32Instance)
			{
				psContext->currentInst = ui32Instance;
				isCurrentForkPhasedInstanced = 0; //reset for each fork phase for cases we don't have a fork phase instance count opcode.
				bformata(glsl, "//%s declarations\n", asPhaseFuncNames[ui32Phase]);
				for(i=0; i < psShader->asPhase[ui32Phase].pui32DeclCount[ui32Instance]; ++i)
				{
					TranslateDeclaration(psContext, psShader->asPhase[ui32Phase].ppsDecl[ui32Instance]+i);
					if(psShader->asPhase[ui32Phase].ppsDecl[ui32Instance][i].eOpcode == OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT)
					{
						haveInstancedForkPhase = 1;
						isCurrentForkPhasedInstanced = 1;
					}
				}

				bformata(glsl, "void %s%d()\n{\n", asPhaseFuncNames[ui32Phase], ui32Instance);
				psContext->indent++;

				MarkIntegerImmediates(psContext, ui32Phase);

				SetDataTypes(psContext, psShader->asPhase[ui32Phase].ppsInst[ui32Instance], psShader->asPhase[ui32Phase].pui32InstCount[ui32Instance]-1, psContext->psShader->aeCommonTempVecType);

				if (psContext->flags & HLSLCC_FLAG_AVOID_TEMP_REGISTER_ALIASING)
				{
					const int usePrec = psContext->flags & HLSLCC_FLAG_FORCE_PRECISION_QUALIFIERS;

					for (i = 0; i < MAX_TEMP_VEC4; ++i)
					{
						if (psShader->aeCommonTempVecType[i] == SVT_FORCE_DWORD)
							continue;
						if (psShader->aeCommonTempVecType[i] == SVT_VOID)
							psShader->aeCommonTempVecType[i] = SVT_FLOAT;

						AddIndentation(psContext);
						bformata(psContext->glsl, "%s Temp%d;\n", GetConstructorForType(psShader->aeCommonTempVecType[i], 4, usePrec), i);
					}

					if (psContext->psShader->bUseTempCopy)
					{
						AddIndentation(psContext);
						bformata(psContext->glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_FLOAT, 4, usePrec), GetConstructorForType(SVT_FLOAT, 1, 0));
						AddIndentation(psContext);
						bformata(psContext->glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_INT, 4, usePrec), GetConstructorForType(SVT_INT, 1, 0));
						AddIndentation(psContext);
						bformata(psContext->glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_BOOL, 4, usePrec), GetConstructorForType(SVT_BOOL, 1, 0));

						if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
						{
							AddIndentation(psContext);
							bformata(psContext->glsl, "%s TempCopy_m%s;\n", GetConstructorForType(SVT_INT16, 4, usePrec), GetConstructorForType(SVT_INT, 1, 0));
							AddIndentation(psContext);
							bformata(psContext->glsl, "%s TempCopy_l%s;\n", GetConstructorForType(SVT_INT12, 4, usePrec), GetConstructorForType(SVT_INT, 1, 0));
							AddIndentation(psContext);
							bformata(psContext->glsl, "%s TempCopy_m%s;\n", GetConstructorForType(SVT_FLOAT16, 4, usePrec), GetConstructorForType(SVT_FLOAT, 1, 0));
							AddIndentation(psContext);
							bformata(psContext->glsl, "%s TempCopy_l%s;\n", GetConstructorForType(SVT_FLOAT10, 4, usePrec), GetConstructorForType(SVT_FLOAT, 1, 0));
						}

						if (HaveUVec(psShader->eTargetLanguage))
						{
							AddIndentation(psContext);
							bformata(psContext->glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_UINT, 4, usePrec), GetConstructorForType(SVT_UINT, 1, 0));

							if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
							{
								AddIndentation(psContext);
								bformata(psContext->glsl, "%s TempCopy_m%s;\n", GetConstructorForType(SVT_UINT16, 4, usePrec), GetConstructorForType(SVT_UINT, 1, 0));
							}
						}

						if (psShader->fp64)
						{
							AddIndentation(psContext);
							bformata(psContext->glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_DOUBLE, 4, usePrec), GetConstructorForType(SVT_DOUBLE, 1, 0));
						}
					}
				}

				if(isCurrentForkPhasedInstanced)
				{
					AddIndentation(psContext);
					bformata(glsl, "for(int forkInstanceID = 0; forkInstanceID < HullPhase%dInstanceCount; ++forkInstanceID) {\n", ui32Instance);
					psContext->indent++;
				}

				//The minus one here is remove the return statement at end of phases.
				//This is needed otherwise the for loop will only run once.
				ASSERT(psShader->asPhase[ui32Phase].ppsInst[ui32Instance]  [psShader->asPhase[ui32Phase].pui32InstCount[ui32Instance]-1].eOpcode == OPCODE_RET);
				for(i=0; i < psShader->asPhase[ui32Phase].pui32InstCount[ui32Instance]-1; ++i)
				{
					TranslateInstruction(psContext, psShader->asPhase[ui32Phase].ppsInst[ui32Instance]+i, NULL);
				}

				if(haveInstancedForkPhase)
				{
					psContext->indent--;
					AddIndentation(psContext);

					if(isCurrentForkPhasedInstanced)
					{
						bcatcstr(glsl, "}\n");
					}
				}

				psContext->indent--;
				bcatcstr(glsl, "}\n");
			}
		}

		bcatcstr(glsl, "void main()\n{\n");

		psContext->indent++;

#ifdef _DEBUG
		AddIndentation(psContext);
		bcatcstr(glsl, "//--- Start Early Main ---\n");
#endif
		bconcat(glsl, psContext->earlyMain);
#ifdef _DEBUG
		AddIndentation(psContext);
		bcatcstr(glsl, "//--- End Early Main ---\n");
#endif

		ui32PhaseFuncCallOrder[0] = HS_CTRL_POINT_PHASE;
		ui32PhaseFuncCallOrder[1] = HS_FORK_PHASE;
		ui32PhaseFuncCallOrder[2] = HS_JOIN_PHASE;

		for(ui32PhaseCallIndex=0; ui32PhaseCallIndex<3; ui32PhaseCallIndex++)
		{
			ui32Phase = ui32PhaseFuncCallOrder[ui32PhaseCallIndex];
			for(ui32Instance = 0; ui32Instance < psShader->asPhase[ui32Phase].ui32InstanceCount; ++ui32Instance)
			{
				AddIndentation(psContext);
				bformata(glsl, "%s%d();\n", asPhaseFuncNames[ui32Phase], ui32Instance);

				if(ui32Phase == HS_FORK_PHASE)
				{
					if(psShader->asPhase[HS_JOIN_PHASE].ui32InstanceCount ||
						(ui32Instance+1 < psShader->asPhase[HS_FORK_PHASE].ui32InstanceCount))
					{
						AddIndentation(psContext);
						bcatcstr(glsl, "barrier();\n");
					}
				}
			}

			if (psContext->havePostShaderCode[ui32Phase])
			{
#ifdef _DEBUG
				AddIndentation(psContext);
				bcatcstr(glsl, "//--- Post shader code ---\n");
#endif
				bconcat(glsl, psContext->postShaderCode[ui32Phase]);
#ifdef _DEBUG
				AddIndentation(psContext);
				bcatcstr(glsl, "//--- End post shader code ---\n");
#endif
			}
		}

		psContext->indent--;

		bcatcstr(glsl, "}\n");

		if(psContext->psDependencies)
		{
			//Save partitioning and primitive type for use by domain shader.
			psContext->psDependencies->eTessOutPrim = psShader->sInfo.eTessOutPrim;

			psContext->psDependencies->eTessPartitioning = psShader->sInfo.eTessPartitioning;
		}

		// Add exports
		AddExport(psContext, SYMBOL_TESSELLATOR_PARTITIONING, 0, psShader->sInfo.eTessPartitioning);
		AddExport(psContext, SYMBOL_TESSELLATOR_OUTPUT_PRIMITIVE, 0, psShader->sInfo.eTessOutPrim);

		FillInResourceDescriptions(psContext);

		return;
	}

	if(psShader->eShaderType == DOMAIN_SHADER && psContext->psDependencies)
	{
		//Load partitioning and primitive type from hull shader.
		switch(psContext->psDependencies->eTessOutPrim)
		{
		case TESSELLATOR_OUTPUT_TRIANGLE_CCW:
			{
				bcatcstr(glsl, "layout(ccw) in;\n");
				break;
			}
		case TESSELLATOR_OUTPUT_TRIANGLE_CW:
			{
				bcatcstr(glsl, "layout(cw) in;\n");
				break;
			}
		case TESSELLATOR_OUTPUT_POINT:
			{
				bcatcstr(glsl, "layout(point_mode) in;\n");
				break;
			}
		default:
			{
				break;
			}
		}

		switch(psContext->psDependencies->eTessPartitioning)
		{
		case TESSELLATOR_PARTITIONING_FRACTIONAL_ODD:
			{
				bcatcstr(glsl, "layout(fractional_odd_spacing) in;\n");
				break;
			}
		case TESSELLATOR_PARTITIONING_FRACTIONAL_EVEN:
			{
				bcatcstr(glsl, "layout(fractional_even_spacing) in;\n");
				break;
			}
		default:
			{
				break;
			}
		}
	}

	ui32InstCount = psShader->asPhase[MAIN_PHASE].pui32InstCount[0];
	ui32DeclCount = psShader->asPhase[MAIN_PHASE].pui32DeclCount[0];

	for (i=0; i < ui32DeclCount; ++i)
	{
		TranslateDeclaration(psContext, psShader->asPhase[MAIN_PHASE].ppsDecl[0]+i);
	}

	if (psContext->psShader->ui32NumDx9ImmConst)
	{
		bformata(psContext->glsl, "vec4 ImmConstArray [%d];\n", psContext->psShader->ui32NumDx9ImmConst);
	}

#ifndef MOVEBACK_INTO_MAIN
	MarkIntegerImmediates(psContext, MAIN_PHASE);

	SetDataTypes(psContext, psShader->asPhase[MAIN_PHASE].ppsInst[0], ui32InstCount, psContext->psShader->aeCommonTempVecType);

	if (psContext->flags & HLSLCC_FLAG_AVOID_TEMP_REGISTER_ALIASING)
	{
		const int usePrec = psContext->flags & HLSLCC_FLAG_FORCE_PRECISION_QUALIFIERS;

		for (i = 0; i < MAX_TEMP_VEC4; ++i)
		{
			if (psShader->aeCommonTempVecType[i] == SVT_FORCE_DWORD)
				continue;
			if (psShader->aeCommonTempVecType[i] == SVT_VOID)
				psShader->aeCommonTempVecType[i] = SVT_FLOAT;

		//	AddIndentation(psContext);
			bformata(psContext->glsl, "%s Temp%d;\n", GetConstructorForType(psShader->aeCommonTempVecType[i], 4, usePrec), i);
		}

		if (psContext->psShader->bUseTempCopy)
		{
		//	AddIndentation(psContext);
			bformata(psContext->glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_FLOAT, 4, usePrec), GetConstructorForType(SVT_FLOAT, 1, 0));
		//	AddIndentation(psContext);
			bformata(psContext->glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_INT, 4, usePrec), GetConstructorForType(SVT_INT, 1, 0));
		//	AddIndentation(psContext);
			bformata(psContext->glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_BOOL, 4, usePrec), GetConstructorForType(SVT_BOOL, 1, 0));

			if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
			{
		//		AddIndentation(psContext);
				bformata(psContext->glsl, "%s TempCopy_m%s;\n", GetConstructorForType(SVT_INT16, 4, usePrec), GetConstructorForType(SVT_INT, 1, 0));
		//		AddIndentation(psContext);
				bformata(psContext->glsl, "%s TempCopy_l%s;\n", GetConstructorForType(SVT_INT12, 4, usePrec), GetConstructorForType(SVT_INT, 1, 0));
		//		AddIndentation(psContext);
				bformata(psContext->glsl, "%s TempCopy_m%s;\n", GetConstructorForType(SVT_FLOAT16, 4, usePrec), GetConstructorForType(SVT_FLOAT, 1, 0));
		//		AddIndentation(psContext);
				bformata(psContext->glsl, "%s TempCopy_l%s;\n", GetConstructorForType(SVT_FLOAT10, 4, usePrec), GetConstructorForType(SVT_FLOAT, 1, 0));
			}

			if (HaveUVec(psShader->eTargetLanguage))
			{
		//		AddIndentation(psContext);
				bformata(psContext->glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_UINT, 4, usePrec), GetConstructorForType(SVT_UINT, 1, 0));

				if (HavePrecisionQualifers(psShader->eTargetLanguage, psContext->flags))
				{
		//			AddIndentation(psContext);
					bformata(psContext->glsl, "%s TempCopy_m%s;\n", GetConstructorForType(SVT_UINT16, 4, usePrec), GetConstructorForType(SVT_UINT, 1, 0));
				}
			}

			if (psShader->fp64)
			{
		//		AddIndentation(psContext);
				bformata(psContext->glsl, "%s TempCopy_%s;\n", GetConstructorForType(SVT_DOUBLE, 4, usePrec), GetConstructorForType(SVT_DOUBLE, 1, 0));
			}
		}
	}
#endif // !MOVEBACK_INTO_MAIN

	if (psContext->flags & HLSLCC_FLAG_TRACING_INSTRUMENTATION)
	{
		uint32_t ui32TracingImp = AddImport(psContext, SYMBOL_TRACE_SHADER, 0, 0);

		CreateTracingInfo(psShader);

		bformata(glsl, "#if IMPORT_%d\n", ui32TracingImp);
		WriteTraceDeclarations(psContext);
		WriteTraceDefinitions(psContext);
		bcatcstr(glsl, "#else\n");
		bcatcstr(glsl, "#define SETUP_TRACING()\n");
		bcatcstr(glsl, "#define TRACE(X)\n");
		bcatcstr(glsl, "#endif\n");
	}

	bcatcstr(glsl, "void main()\n{\n");

	psContext->indent++;

#ifdef _DEBUG
	AddIndentation(psContext);
	bcatcstr(glsl, "//--- Start Early Main ---\n");
#endif
	bconcat(glsl, psContext->earlyMain);
	if (psContext->flags & HLSLCC_FLAG_TRACING_INSTRUMENTATION)
		WritePreStepsTrace(psContext, psShader->sInfo.psTraceSteps);
#ifdef _DEBUG
	AddIndentation(psContext);
	bcatcstr(glsl, "//--- End Early Main ---\n");
#endif

	for(i=0; i < ui32InstCount; ++i)
	{
		TranslateInstruction(psContext, psShader->asPhase[MAIN_PHASE].ppsInst[0]+i, i+1 < ui32InstCount ? psShader->asPhase[MAIN_PHASE].ppsInst[0]+i+1 : 0);

		if (psContext->flags & HLSLCC_FLAG_TRACING_INSTRUMENTATION)
			WritePostStepTrace(psContext, i);
	}

	psContext->indent--;

	bcatcstr(glsl, "}\n");

	// Add exports
	if (psShader->eShaderType == PIXEL_SHADER)
	{
		uint32_t ui32Input;
		for (ui32Input = 0; ui32Input < MAX_SHADER_VEC4_INPUT; ++ui32Input)
		{
			INTERPOLATION_MODE eMode = dependencies ? dependencies->aePixelInputInterpolation[ui32Input] : INTERPOLATION_LINEAR;
			if (eMode != INTERPOLATION_LINEAR)
			{
				AddExport(psContext, SYMBOL_INPUT_INTERPOLATION_MODE, ui32Input, (uint32_t)eMode);
			}
		}
	}

	FillInResourceDescriptions(psContext);
}

static void FreeSubOperands(Instruction* psInst, const uint32_t ui32NumInsts)
{
	uint32_t ui32Inst;
	for(ui32Inst = 0; ui32Inst < ui32NumInsts; ++ui32Inst)
	{
		Instruction* psCurrentInst = &psInst[ui32Inst];
		const uint32_t ui32NumOperands = psCurrentInst->ui32NumOperands;
		uint32_t ui32Operand;

		for(ui32Operand = 0; ui32Operand < ui32NumOperands; ++ui32Operand)
		{
			uint32_t ui32SubOperand;
			for(ui32SubOperand = 0; ui32SubOperand < MAX_SUB_OPERANDS; ++ui32SubOperand)
			{
				if(psCurrentInst->asOperands[ui32Operand].psSubOperand[ui32SubOperand])
				{
					hlslcc_free(psCurrentInst->asOperands[ui32Operand].psSubOperand[ui32SubOperand]);
					psCurrentInst->asOperands[ui32Operand].psSubOperand[ui32SubOperand] = NULL;
				}
			}
		}
	}
}

const char* GetMangleSuffix(const SHADER_TYPE eShaderType);
#define original_strcat(a,b) strcat(a,b) 
#undef strcat

void UpdateFullName(ShaderVarType* psParentVarType)
{
	uint32_t uMember;
	for (uMember = 0; uMember < psParentVarType->MemberCount; ++uMember)
	{
		ShaderVarType* psMemberVarType = psParentVarType->Members + uMember;
		ASSERT( (strlen(psParentVarType->FullName) + 1 + strlen(psMemberVarType->Name) + 1 + 2) < MAX_REFLECT_STRING_LENGTH);

		strcpy(psMemberVarType->FullName, psParentVarType->FullName);
		strcat(psMemberVarType->FullName, ".");
		strcat(psMemberVarType->FullName, psMemberVarType->Name);

		UpdateFullName(psMemberVarType);
	}
}

void MangleIdentifiersPerStage(Shader* psShader)
{
	uint32_t i, j;
	const char* suffix = GetMangleSuffix(psShader->eShaderType);

	for (i = 0; i < psShader->sInfo.ui32NumConstantBuffers; ++i)
	{
		for (j = 0; j < psShader->sInfo.psConstantBuffers[i].ui32NumVars; ++j)
		{
			strcat(psShader->sInfo.psConstantBuffers[i].asVars[j].sType.Name, suffix);
			strcat(psShader->sInfo.psConstantBuffers[i].asVars[j].sType.FullName, suffix);

			UpdateFullName(&psShader->sInfo.psConstantBuffers[i].asVars[j].sType);
		}
	}
}

#define strcat(a,b) original_strcat(a,b) 

void RemoveDoubleUnderscores(char* szName)
{
	char* position;
	size_t length;
	length = strlen(szName);
	position = szName;
	while (position = strstr(position, "__"))
	{
		position[1] = '0';
		position += 2;
	}
}

void RemoveDoubleUnderscoresFromIdentifiers(Shader* psShader)
{
	uint32_t i, j;
	for (i = 0; i < psShader->sInfo.ui32NumConstantBuffers; ++i)
	{
		for (j = 0; j < psShader->sInfo.psConstantBuffers[i].ui32NumVars; ++j)
		{
			RemoveDoubleUnderscores(psShader->sInfo.psConstantBuffers[i].asVars[j].sType.Name);
			RemoveDoubleUnderscores(psShader->sInfo.psConstantBuffers[i].asVars[j].sType.FullName);
		}
	}
}

HLSLCC_API int HLSLCC_APIENTRY TranslateHLSLFromMem(const char* shader, size_t size, unsigned int flags, GLLang language, const GlExtensions *extensions, GLSLCrossDependencyData* dependencies, GLSLShader* result)
{
	uint32_t* tokens;
	Shader* psShader;
	char* glslcstr = NULL;
	int GLSLShaderType = GL_FRAGMENT_SHADER_ARB;
	int success = 0;
	uint32_t i;

	tokens = (uint32_t*)shader;

	psShader = DecodeDXBC(tokens, flags, language);

	if (flags & (HLSLCC_FLAG_HASH_INPUT | HLSLCC_FLAG_ADD_DEBUG_HEADER))
	{
		uint64_t ui64InputHash = hash64((const uint8_t*)tokens, tokens[6], 0);
		psShader->sInfo.ui32InputHash = (uint32_t)ui64InputHash ^ (uint32_t)(ui64InputHash >> 32);
	}

	RemoveDoubleUnderscoresFromIdentifiers(psShader);
	if (flags & HLSLCC_FLAG_MANGLE_IDENTIFIERS_PER_STAGE)
	{
		MangleIdentifiersPerStage(psShader);
	}

	if(psShader)
	{
		HLSLCrossCompilerContext sContext;

		if(psShader->ui32MajorVersion <= 3)
		{
			flags &= ~HLSLCC_FLAG_COMBINE_TEXTURE_SAMPLERS;
		}

		sContext.psShader = psShader;
		sContext.flags = flags;
		sContext.psDependencies = dependencies;
		sContext.bTempAssignment = 0;
		sContext.bTempConsumption = 0;

		for(i=0; i<NUM_PHASES;++i)
		{
			sContext.havePostShaderCode[i] = 0;
		}

		if (flags & HLSLCC_FLAG_ADD_DEBUG_HEADER)
		{
#if defined(_WIN32) && !defined(PORTABLE)
			ID3DBlob* pDisassembly = NULL;
#endif //defined(_WIN32) && !defined(PORTABLE)

			sContext.debugHeader = bformat("// HASH = 0x%08X\n", psShader->sInfo.ui32InputHash);

#if defined(_WIN32) && !defined(PORTABLE)
			D3DDisassemble(shader, size, 0, "", &pDisassembly);
			bcatcstr(sContext.debugHeader, "/*\n");
			bcatcstr(sContext.debugHeader, (const char*)pDisassembly->lpVtbl->GetBufferPointer(pDisassembly));
			bcatcstr(sContext.debugHeader, "\n*/\n");
			pDisassembly->lpVtbl->Release(pDisassembly);
#endif //defined(_WIN32) && !defined(PORTABLE)
		}

		TranslateToGLSL(&sContext, &language, dependencies, extensions);

		switch(psShader->eShaderType)
		{
		case VERTEX_SHADER:
			{
				GLSLShaderType = GL_VERTEX_SHADER_ARB;
				break;
			}
		case GEOMETRY_SHADER:
			{
				GLSLShaderType = GL_GEOMETRY_SHADER;
				break;
			}
		case DOMAIN_SHADER:
			{
				GLSLShaderType = GL_TESS_EVALUATION_SHADER;
				break;
			}
		case HULL_SHADER:
			{
				GLSLShaderType = GL_TESS_CONTROL_SHADER;
				break;
			}
		case COMPUTE_SHADER:
			{
				GLSLShaderType = GL_COMPUTE_SHADER;
				break;
			}
		default:
			{
				break;
			}
		}

		glslcstr = bstr2cstr(sContext.glsl, '\0');

		bdestroy(sContext.glsl);
		bdestroy(sContext.earlyMain);
		for(i=0; i<NUM_PHASES; ++i)
		{
			bdestroy(sContext.postShaderCode[i]);
		}

		for(i=0; i<NUM_PHASES;++i)
		{
			if(psShader->asPhase[i].ppsDecl != 0)
			{
				uint32_t k;
				for(k=0; k < psShader->asPhase[i].ui32InstanceCount; ++k)
				{
					hlslcc_free(psShader->asPhase[i].ppsDecl[k]);
				}
				hlslcc_free(psShader->asPhase[i].ppsDecl);
			}
			if(psShader->asPhase[i].ppsInst != 0)
			{
				uint32_t k;
				for(k=0; k < psShader->asPhase[i].ui32InstanceCount; ++k)
		{
					FreeSubOperands(psShader->asPhase[i].ppsInst[k], psShader->asPhase[i].pui32InstCount[k]);
					hlslcc_free(psShader->asPhase[i].ppsInst[k]);
				}
				hlslcc_free(psShader->asPhase[i].ppsInst);
			}
		}

		memcpy(&result->reflection,&psShader->sInfo,sizeof(psShader->sInfo));

		result->textureSamplerInfo.ui32NumTextureSamplerPairs = psShader->textureSamplerInfo.ui32NumTextureSamplerPairs;
		for (i=0; i<result->textureSamplerInfo.ui32NumTextureSamplerPairs; i++)
			strcpy(result->textureSamplerInfo.aTextureSamplerPair[i].Name, psShader->textureSamplerInfo.aTextureSamplerPair[i].Name);

		hlslcc_free(psShader);

		success = 1;
	}

	shader = 0;
	tokens = 0;

	/* Fill in the result struct */

	result->shaderType = GLSLShaderType;
	result->sourceCode = glslcstr;
	result->GLSLLanguage = language;

	return success;
}

HLSLCC_API int HLSLCC_APIENTRY TranslateHLSLFromFile(const char* filename, unsigned int flags, GLLang language, const GlExtensions *extensions, GLSLCrossDependencyData* dependencies, GLSLShader* result)
{
	FILE* shaderFile;
	int length;
	size_t readLength;
	char* shader;
	int success = 0;

	shaderFile = fopen(filename, "rb");

	if(!shaderFile)
	{
		return 0;
	}

	fseek(shaderFile, 0, SEEK_END);
	length = ftell(shaderFile);
	fseek(shaderFile, 0, SEEK_SET);

	shader = (char*)hlslcc_malloc(length+1);

	readLength = fread(shader, 1, length, shaderFile);

	fclose(shaderFile);
	shaderFile = 0;

	shader[readLength] = '\0';

	success = TranslateHLSLFromMem(shader, readLength, flags, language, extensions, dependencies, result);

	hlslcc_free(shader);

	return success;
}

HLSLCC_API void HLSLCC_APIENTRY FreeGLSLShader(GLSLShader* s)
{
	bcstrfree(s->sourceCode);
	s->sourceCode = NULL;
	FreeShaderInfo(&s->reflection);
}

