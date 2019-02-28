// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   ParserBin.cpp : Script parser implementations.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include <CryMemory/BucketAllocatorImpl.h>
#include <CryCore/CryCrc32.h>

char* g_KeyTokens[eT_max];
TArray<bool> sfxIFDef;
TArray<bool> sfxIFIgnore;
bool CParserBin::m_bEditable;
uint32 CParserBin::m_nPlatform;
bool CParserBin::m_bEndians;
bool CParserBin::m_bParseFX = true;
bool CParserBin::m_bShaderCacheGen = false;

ShaderBucketAllocator g_shaderBucketAllocator;

IGeneralMemoryHeap* g_shaderGeneralHeap;

CParserBin::CParserBin(SShaderBin* pBin)
{
	m_pCurBinShader = pBin;
	m_pCurShader = NULL;
}
CParserBin::CParserBin(SShaderBin* pBin, CShader* pSH)
{
	m_pCurBinShader = pBin;
	m_pCurShader = pSH;
}

uint32 CParserBin::GetCRC32(const char* szStr)
{
	uint32 nGen = CCrc32::Compute(szStr);
	assert(nGen >= eT_user_first);

	return nGen;
}

FXMacroBin CParserBin::m_StaticMacros;

void CParserBin::Init()
{
	// Register key tokens
	fxTokenKey("#include", eT_include);
	fxTokenKey("#define", eT_define);
	fxTokenKey("#undefine", eT_undefine);
	fxTokenKey("#define", eT_define_2);
	fxTokenKey("#fetchinst", eT_fetchinst);
	fxTokenKey("#if", eT_if);
	fxTokenKey("#ifdef", eT_ifdef);
	fxTokenKey("#ifndef", eT_ifndef);
	fxTokenKey("#if", eT_if_2);
	fxTokenKey("#ifdef", eT_ifdef_2);
	fxTokenKey("#ifndef", eT_ifndef_2);
	fxTokenKey("#endif", eT_endif);
	fxTokenKey("#else", eT_else);
	fxTokenKey("#elif", eT_elif);
	fxTokenKey("#elif", eT_elif_2);
	fxTokenKey("#warning", eT_warning);
	fxTokenKey("#register_env", eT_register_env);
	fxTokenKey("#ifcvar", eT_ifcvar);
	fxTokenKey("#ifncvar", eT_ifncvar);
	fxTokenKey("#elifcvar", eT_elifcvar);
	fxTokenKey("#skip", eT_skip);
	fxTokenKey("#skip_(", eT_skip_1);
	fxTokenKey("#skip_)", eT_skip_2);

	fxTokenKey("|", eT_or);
	fxTokenKey("&", eT_and);

	fxTokenKey("(", eT_br_rnd_1);
	fxTokenKey(")", eT_br_rnd_2);
	fxTokenKey("[", eT_br_sq_1);
	fxTokenKey("]", eT_br_sq_2);
	fxTokenKey("{", eT_br_cv_1);
	fxTokenKey("}", eT_br_cv_2);
	fxTokenKey("<", eT_br_tr_1);
	fxTokenKey(">", eT_br_tr_2);
	fxTokenKey(",", eT_comma);
	fxTokenKey(".", eT_dot);
	fxTokenKey(":", eT_colon);
	fxTokenKey(";", eT_semicolumn);
	fxTokenKey("!", eT_excl);
	fxTokenKey("\"", eT_quote);
	fxTokenKey("'", eT_sing_quote);

	fxTokenKey("s0", eT_s0);
	fxTokenKey("s1", eT_s1);
	fxTokenKey("s2", eT_s2);
	fxTokenKey("s3", eT_s3);
	fxTokenKey("s4", eT_s4);
	fxTokenKey("s5", eT_s5);
	fxTokenKey("s6", eT_s6);
	fxTokenKey("s7", eT_s7);
	fxTokenKey("s8", eT_s8);
	fxTokenKey("s9", eT_s9);
	fxTokenKey("s10", eT_s10);
	fxTokenKey("s11", eT_s11);
	fxTokenKey("s12", eT_s12);
	fxTokenKey("s13", eT_s13);
	fxTokenKey("s14", eT_s14);
	fxTokenKey("s15", eT_s15);

	fxTokenKey("t0", eT_t0);
	fxTokenKey("t1", eT_t1);
	fxTokenKey("t2", eT_t2);
	fxTokenKey("t3", eT_t3);
	fxTokenKey("t4", eT_t4);
	fxTokenKey("t5", eT_t5);
	fxTokenKey("t6", eT_t6);
	fxTokenKey("t7", eT_t7);
	fxTokenKey("t8", eT_t8);
	fxTokenKey("t9", eT_t9);
	fxTokenKey("t10", eT_t10);
	fxTokenKey("t11", eT_t11);
	fxTokenKey("t12", eT_t12);
	fxTokenKey("t13", eT_t13);
	fxTokenKey("t14", eT_t14);
	fxTokenKey("t15", eT_t15);

	fxTokenKey("//", eT_comment);

	fxTokenKey("?", eT_question);
	fxTokenKey("=", eT_eq);
	fxTokenKey("+", eT_plus);
	fxTokenKey("-", eT_minus);
	fxTokenKey("/", eT_div);
	fxTokenKey("*", eT_mul);
	fxTokenKey("dot", eT_dot_math);
	fxTokenKey("mul", eT_mul_math);
	fxTokenKey("sqrt", eT_sqrt_math);
	fxTokenKey("exp", eT_exp_math);
	fxTokenKey("log", eT_log_math);
	fxTokenKey("log2", eT_log2_math);
	fxTokenKey("sin", eT_sin_math);
	fxTokenKey("cos", eT_cos_math);
	fxTokenKey("sincos", eT_sincos_math);
	fxTokenKey("floor", eT_floor_math);
	fxTokenKey("floor", eT_ceil_math);
	fxTokenKey("frac", eT_frac_math);
	fxTokenKey("lerp", eT_lerp_math);
	fxTokenKey("abs", eT_abs_math);
	fxTokenKey("clamp", eT_clamp_math);
	fxTokenKey("min", eT_min_math);
	fxTokenKey("max", eT_max_math);
	fxTokenKey("length", eT_length_math);

	fxTokenKey("%_LT_LIGHTS", eT__LT_LIGHTS);
	fxTokenKey("%_LT_NUM", eT__LT_NUM);
	fxTokenKey("%_LT_HASPROJ", eT__LT_HASPROJ);
	fxTokenKey("%_LT_0_TYPE", eT__LT_0_TYPE);
	fxTokenKey("%_LT_1_TYPE", eT__LT_1_TYPE);
	fxTokenKey("%_LT_2_TYPE", eT__LT_2_TYPE);
	fxTokenKey("%_LT_3_TYPE", eT__LT_3_TYPE);
	fxTokenKey("%_TT_TEXCOORD_MATRIX", eT__TT_TEXCOORD_MATRIX);
	fxTokenKey("%_TT_TEXCOORD_GEN_OBJECT_LINEAR", eT__TT_TEXCOORD_GEN_OBJECT_LINEAR);
	fxTokenKey("%_TT_TEXCOORD_PROJ", eT__TT_TEXCOORD_PROJ);
	fxTokenKey("%_VT_TYPE", eT__VT_TYPE);
	fxTokenKey("%_VT_TYPE_MODIF", eT__VT_TYPE_MODIF);
	fxTokenKey("%_VT_BEND", eT__VT_BEND);
	fxTokenKey("%_VT_DET_BEND", eT__VT_DET_BEND);
	fxTokenKey("%_VT_GRASS", eT__VT_GRASS);
	fxTokenKey("%_VT_WIND", eT__VT_WIND);
	fxTokenKey("%_VT_DEPTH_OFFSET", eT__VT_DEPTH_OFFSET);
	fxTokenKey("%_FT_TEXTURE", eT__FT_TEXTURE);
	fxTokenKey("%_FT_TEXTURE1", eT__FT_TEXTURE1);
	fxTokenKey("%_FT_NORMAL", eT__FT_NORMAL);
	fxTokenKey("%_FT_PSIZE", eT__FT_PSIZE);
	fxTokenKey("%_FT_DIFFUSE", eT__FT_DIFFUSE);
	fxTokenKey("%_FT_SPECULAR", eT__FT_SPECULAR);
	fxTokenKey("%_FT_TANGENT_STREAM", eT__FT_TANGENT_STREAM);
	fxTokenKey("%_FT_QTANGENT_STREAM", eT__FT_QTANGENT_STREAM);
	fxTokenKey("%_FT_SKIN_STREAM", eT__FT_SKIN_STREAM);
	fxTokenKey("%_FT_VERTEX_VELOCITY_STREAM", eT__FT_VERTEX_VELOCITY_STREAM);
	fxTokenKey("%_FT0_COP", eT__FT0_COP);
	fxTokenKey("%_FT0_AOP", eT__FT0_AOP);
	fxTokenKey("%_FT0_CARG1", eT__FT0_CARG1);
	fxTokenKey("%_FT0_CARG2", eT__FT0_CARG2);
	fxTokenKey("%_FT0_AARG1", eT__FT0_AARG1);
	fxTokenKey("%_FT0_AARG2", eT__FT0_AARG2);

	fxTokenKey("%_VS", eT__VS);
	fxTokenKey("%_PS", eT__PS);
	fxTokenKey("%_GS", eT__GS);
	fxTokenKey("%_HS", eT__HS);
	fxTokenKey("%_DS", eT__DS);
	fxTokenKey("%_CS", eT__CS);

	FX_REGISTER_TOKEN(tex2D);
	FX_REGISTER_TOKEN(tex2Dproj);
	FX_REGISTER_TOKEN(tex3D);
	FX_REGISTER_TOKEN(texCUBE);
	FX_REGISTER_TOKEN(sampler1D);
	FX_REGISTER_TOKEN(sampler2D);
	FX_REGISTER_TOKEN(sampler3D);
	FX_REGISTER_TOKEN(samplerCUBE);
	FX_REGISTER_TOKEN(SamplerState);
	FX_REGISTER_TOKEN(SamplerComparisonState);
	FX_REGISTER_TOKEN(sampler_state);
	FX_REGISTER_TOKEN(Texture2D);
	FX_REGISTER_TOKEN(Texture2DArray);
	FX_REGISTER_TOKEN(Texture2DMS);
	FX_REGISTER_TOKEN(RWTexture2D);
	FX_REGISTER_TOKEN(RWTexture2DArray);
	FX_REGISTER_TOKEN(TextureCube);
	FX_REGISTER_TOKEN(TextureCubeArray);
	FX_REGISTER_TOKEN(Texture3D);
	FX_REGISTER_TOKEN(RWTexture3D);

	FX_REGISTER_TOKEN(unorm);
	FX_REGISTER_TOKEN(snorm);
	FX_REGISTER_TOKEN(float);
	FX_REGISTER_TOKEN(float2);
	FX_REGISTER_TOKEN(float3);
	FX_REGISTER_TOKEN(float4);
	FX_REGISTER_TOKEN(float2x4);
	FX_REGISTER_TOKEN(float3x4);
	FX_REGISTER_TOKEN(float4x4);
	FX_REGISTER_TOKEN(float3x3);
	FX_REGISTER_TOKEN(half);
	FX_REGISTER_TOKEN(half2);
	FX_REGISTER_TOKEN(half3);
	FX_REGISTER_TOKEN(half4);
	FX_REGISTER_TOKEN(half2x4);
	FX_REGISTER_TOKEN(half3x4);
	FX_REGISTER_TOKEN(half4x4);
	FX_REGISTER_TOKEN(half3x3);
	FX_REGISTER_TOKEN(bool);
	FX_REGISTER_TOKEN(int);
	FX_REGISTER_TOKEN(int2);
	FX_REGISTER_TOKEN(int3);
	FX_REGISTER_TOKEN(int4);
	FX_REGISTER_TOKEN(uint);
	FX_REGISTER_TOKEN(uint2);
	FX_REGISTER_TOKEN(uint3);
	FX_REGISTER_TOKEN(uint4);
	FX_REGISTER_TOKEN(min16float);
	FX_REGISTER_TOKEN(min16float2);
	FX_REGISTER_TOKEN(min16float3);
	FX_REGISTER_TOKEN(min16float4);
	FX_REGISTER_TOKEN(min16float4x4);
	FX_REGISTER_TOKEN(min16float3x4);
	FX_REGISTER_TOKEN(min16float2x4);
	FX_REGISTER_TOKEN(min16float3x3);
	FX_REGISTER_TOKEN(min10float);
	FX_REGISTER_TOKEN(min10float2);
	FX_REGISTER_TOKEN(min10float3);
	FX_REGISTER_TOKEN(min10float4);
	FX_REGISTER_TOKEN(min10float4x4);
	FX_REGISTER_TOKEN(min10float3x4);
	FX_REGISTER_TOKEN(min10float2x4);
	FX_REGISTER_TOKEN(min10float3x3);
	FX_REGISTER_TOKEN(min16int);
	FX_REGISTER_TOKEN(min16int2);
	FX_REGISTER_TOKEN(min16int3);
	FX_REGISTER_TOKEN(min16int4);
	FX_REGISTER_TOKEN(min12int);
	FX_REGISTER_TOKEN(min12int2);
	FX_REGISTER_TOKEN(min12int3);
	FX_REGISTER_TOKEN(min12int4);
	FX_REGISTER_TOKEN(min16uint);
	FX_REGISTER_TOKEN(min16uint2);
	FX_REGISTER_TOKEN(min16uint3);
	FX_REGISTER_TOKEN(min16uint4);

	FX_REGISTER_TOKEN(inout);
	FX_REGISTER_TOKEN(asm);

	FX_REGISTER_TOKEN(struct);
	FX_REGISTER_TOKEN(sampler);
	FX_REGISTER_TOKEN(const);
	FX_REGISTER_TOKEN(static);
	FX_REGISTER_TOKEN(groupshared);
	FX_REGISTER_TOKEN(TEXCOORDN);
	FX_REGISTER_TOKEN(TEXCOORD0);
	FX_REGISTER_TOKEN(TEXCOORD1);
	FX_REGISTER_TOKEN(TEXCOORD2);
	FX_REGISTER_TOKEN(TEXCOORD3);
	FX_REGISTER_TOKEN(TEXCOORD4);
	FX_REGISTER_TOKEN(TEXCOORD5);
	FX_REGISTER_TOKEN(TEXCOORD6);
	FX_REGISTER_TOKEN(TEXCOORD7);
	FX_REGISTER_TOKEN(TEXCOORD8);
	FX_REGISTER_TOKEN(TEXCOORD9);
	FX_REGISTER_TOKEN(TEXCOORD10);
	FX_REGISTER_TOKEN(TEXCOORD11);
	FX_REGISTER_TOKEN(TEXCOORD12);
	FX_REGISTER_TOKEN(TEXCOORD13);
	FX_REGISTER_TOKEN(TEXCOORD14);
	FX_REGISTER_TOKEN(TEXCOORD15);
	FX_REGISTER_TOKEN(TEXCOORD16);
	FX_REGISTER_TOKEN(TEXCOORD17);
	FX_REGISTER_TOKEN(TEXCOORD18);
	FX_REGISTER_TOKEN(TEXCOORD19);
	FX_REGISTER_TOKEN(TEXCOORD20);
	FX_REGISTER_TOKEN(TEXCOORD21);
	FX_REGISTER_TOKEN(TEXCOORD22);
	FX_REGISTER_TOKEN(TEXCOORD23);
	FX_REGISTER_TOKEN(TEXCOORD24);
	FX_REGISTER_TOKEN(TEXCOORD25);
	FX_REGISTER_TOKEN(TEXCOORD26);
	FX_REGISTER_TOKEN(TEXCOORD27);
	FX_REGISTER_TOKEN(TEXCOORD28);
	FX_REGISTER_TOKEN(TEXCOORD29);
	FX_REGISTER_TOKEN(TEXCOORD30);
	FX_REGISTER_TOKEN(TEXCOORD31);
	FX_REGISTER_TOKEN(TEXCOORDN_centroid);
	FX_REGISTER_TOKEN(TEXCOORD0_centroid);
	FX_REGISTER_TOKEN(TEXCOORD1_centroid);
	FX_REGISTER_TOKEN(TEXCOORD2_centroid);
	FX_REGISTER_TOKEN(TEXCOORD3_centroid);
	FX_REGISTER_TOKEN(TEXCOORD4_centroid);
	FX_REGISTER_TOKEN(TEXCOORD5_centroid);
	FX_REGISTER_TOKEN(TEXCOORD6_centroid);
	FX_REGISTER_TOKEN(TEXCOORD7_centroid);
	FX_REGISTER_TOKEN(TEXCOORD8_centroid);
	FX_REGISTER_TOKEN(TEXCOORD9_centroid);
	FX_REGISTER_TOKEN(TEXCOORD10_centroid);
	FX_REGISTER_TOKEN(TEXCOORD11_centroid);
	FX_REGISTER_TOKEN(TEXCOORD12_centroid);
	FX_REGISTER_TOKEN(TEXCOORD13_centroid);
	FX_REGISTER_TOKEN(TEXCOORD14_centroid);
	FX_REGISTER_TOKEN(TEXCOORD15_centroid);
	FX_REGISTER_TOKEN(TEXCOORD16_centroid);
	FX_REGISTER_TOKEN(TEXCOORD17_centroid);
	FX_REGISTER_TOKEN(TEXCOORD18_centroid);
	FX_REGISTER_TOKEN(TEXCOORD19_centroid);
	FX_REGISTER_TOKEN(TEXCOORD20_centroid);
	FX_REGISTER_TOKEN(TEXCOORD21_centroid);
	FX_REGISTER_TOKEN(TEXCOORD22_centroid);
	FX_REGISTER_TOKEN(TEXCOORD23_centroid);
	FX_REGISTER_TOKEN(TEXCOORD24_centroid);
	FX_REGISTER_TOKEN(TEXCOORD25_centroid);
	FX_REGISTER_TOKEN(TEXCOORD26_centroid);
	FX_REGISTER_TOKEN(TEXCOORD27_centroid);
	FX_REGISTER_TOKEN(TEXCOORD28_centroid);
	FX_REGISTER_TOKEN(TEXCOORD29_centroid);
	FX_REGISTER_TOKEN(TEXCOORD30_centroid);
	FX_REGISTER_TOKEN(TEXCOORD31_centroid);
	FX_REGISTER_TOKEN(COLOR0);

	FX_REGISTER_TOKEN(packoffset);
	FX_REGISTER_TOKEN(register);
	FX_REGISTER_TOKEN(return );
	FX_REGISTER_TOKEN(vsregister);
	FX_REGISTER_TOKEN(psregister);
	FX_REGISTER_TOKEN(gsregister);
	FX_REGISTER_TOKEN(dsregister);
	FX_REGISTER_TOKEN(hsregister);
	FX_REGISTER_TOKEN(csregister);
	FX_REGISTER_TOKEN(slot);
	FX_REGISTER_TOKEN(vsslot);
	FX_REGISTER_TOKEN(psslot);
	FX_REGISTER_TOKEN(gsslot);
	FX_REGISTER_TOKEN(dsslot);
	FX_REGISTER_TOKEN(hsslot);
	FX_REGISTER_TOKEN(csslot);
	FX_REGISTER_TOKEN(color);

	FX_REGISTER_TOKEN(Buffer);
	FX_REGISTER_TOKEN(RWBuffer);
	FX_REGISTER_TOKEN(StructuredBuffer);
	FX_REGISTER_TOKEN(RWStructuredBuffer);
	FX_REGISTER_TOKEN(ByteAddressBuffer);
	FX_REGISTER_TOKEN(RWByteAddressBuffer);

	FX_REGISTER_TOKEN(Position);
	FX_REGISTER_TOKEN(Allways);

	FX_REGISTER_TOKEN(STANDARDSGLOBAL);

	FX_REGISTER_TOKEN(technique);
	FX_REGISTER_TOKEN(string);
	FX_REGISTER_TOKEN(UIName);
	FX_REGISTER_TOKEN(UIDescription);
	FX_REGISTER_TOKEN(UIWidget);
	FX_REGISTER_TOKEN(UIWidget0);
	FX_REGISTER_TOKEN(UIWidget1);
	FX_REGISTER_TOKEN(UIWidget2);
	FX_REGISTER_TOKEN(UIWidget3);

	FX_REGISTER_TOKEN(Texture);
	FX_REGISTER_TOKEN(Filter);
	FX_REGISTER_TOKEN(MinFilter);
	FX_REGISTER_TOKEN(MagFilter);
	FX_REGISTER_TOKEN(MipFilter);
	FX_REGISTER_TOKEN(AddressU);
	FX_REGISTER_TOKEN(AddressV);
	FX_REGISTER_TOKEN(AddressW);
	FX_REGISTER_TOKEN(BorderColor);
	FX_REGISTER_TOKEN(AnisotropyLevel);
	FX_REGISTER_TOKEN(sRGBLookup);
	FX_REGISTER_TOKEN(Global);

	FX_REGISTER_TOKEN(LINEAR);
	FX_REGISTER_TOKEN(POINT);
	FX_REGISTER_TOKEN(NONE);
	FX_REGISTER_TOKEN(ANISOTROPIC);
	FX_REGISTER_TOKEN(MIN_MAG_MIP_POINT);
	FX_REGISTER_TOKEN(MIN_MAG_MIP_LINEAR);
	FX_REGISTER_TOKEN(MIN_MAG_LINEAR_MIP_POINT);
	FX_REGISTER_TOKEN(COMPARISON_MIN_MAG_LINEAR_MIP_POINT);
	FX_REGISTER_TOKEN(MINIMUM_MIN_MAG_MIP_LINEAR);
	FX_REGISTER_TOKEN(MAXIMUM_MIN_MAG_MIP_LINEAR);

	FX_REGISTER_TOKEN(Clamp);
	FX_REGISTER_TOKEN(Border);
	FX_REGISTER_TOKEN(Wrap);
	FX_REGISTER_TOKEN(Mirror);

	FX_REGISTER_TOKEN(Script);

	FX_REGISTER_TOKEN(RenderOrder);
	FX_REGISTER_TOKEN(ProcessOrder);
	FX_REGISTER_TOKEN(RenderCamera);
	FX_REGISTER_TOKEN(RenderType);
	FX_REGISTER_TOKEN(RenderFilter);
	FX_REGISTER_TOKEN(RenderColorTarget1);
	FX_REGISTER_TOKEN(RenderDepthStencilTarget);
	FX_REGISTER_TOKEN(ClearSetColor);
	FX_REGISTER_TOKEN(ClearSetDepth);
	FX_REGISTER_TOKEN(ClearTarget);
	FX_REGISTER_TOKEN(RenderTarget_IDPool);
	FX_REGISTER_TOKEN(RenderTarget_UpdateType);
	FX_REGISTER_TOKEN(RenderTarget_Width);
	FX_REGISTER_TOKEN(RenderTarget_Height);
	FX_REGISTER_TOKEN(GenerateMips);

	FX_REGISTER_TOKEN(PreProcess);
	FX_REGISTER_TOKEN(PostProcess);
	FX_REGISTER_TOKEN(PreDraw);

	FX_REGISTER_TOKEN(WaterReflection);
	FX_REGISTER_TOKEN(Panorama);

	FX_REGISTER_TOKEN(WaterPlaneReflected);
	FX_REGISTER_TOKEN(PlaneReflected);
	FX_REGISTER_TOKEN(Current);

	FX_REGISTER_TOKEN(CurObject);
	FX_REGISTER_TOKEN(CurScene);
	FX_REGISTER_TOKEN(RecursiveScene);
	FX_REGISTER_TOKEN(CopyScene);

	FX_REGISTER_TOKEN(Refractive);
	FX_REGISTER_TOKEN(ForceRefractionUpdate);
	FX_REGISTER_TOKEN(Heat);

	FX_REGISTER_TOKEN(DepthBuffer);
	FX_REGISTER_TOKEN(DepthBufferTemp);
	FX_REGISTER_TOKEN(DepthBufferOrig);

	FX_REGISTER_TOKEN($ScreenSize);
	FX_REGISTER_TOKEN(WaterReflect);
	FX_REGISTER_TOKEN(FogColor);

	FX_REGISTER_TOKEN(Color);
	FX_REGISTER_TOKEN(Depth);

	FX_REGISTER_TOKEN($RT_2D);
	FX_REGISTER_TOKEN($RT_CM);
	FX_REGISTER_TOKEN($RT_Cube);

	FX_REGISTER_TOKEN(pass);
	FX_REGISTER_TOKEN(CustomRE);
	FX_REGISTER_TOKEN(Style);

	FX_REGISTER_TOKEN(VertexShader);
	FX_REGISTER_TOKEN(PixelShader);
	FX_REGISTER_TOKEN(GeometryShader);
	FX_REGISTER_TOKEN(DomainShader);
	FX_REGISTER_TOKEN(HullShader);
	FX_REGISTER_TOKEN(ComputeShader);
	FX_REGISTER_TOKEN(ZEnable);
	FX_REGISTER_TOKEN(ZWriteEnable);
	FX_REGISTER_TOKEN(CullMode);
	FX_REGISTER_TOKEN(SrcBlend);
	FX_REGISTER_TOKEN(DestBlend);
	FX_REGISTER_TOKEN(AlphaBlendEnable);
	FX_REGISTER_TOKEN(AlphaFunc);
	FX_REGISTER_TOKEN(AlphaRef);
	FX_REGISTER_TOKEN(ZFunc);
	FX_REGISTER_TOKEN(ColorWriteEnable);
	FX_REGISTER_TOKEN(IgnoreMaterialState);

	FX_REGISTER_TOKEN(None);
	FX_REGISTER_TOKEN(Disable);
	FX_REGISTER_TOKEN(CCW);
	FX_REGISTER_TOKEN(CW);
	FX_REGISTER_TOKEN(Back);
	FX_REGISTER_TOKEN(Front);

	FX_REGISTER_TOKEN(Never);
	FX_REGISTER_TOKEN(Less);
	FX_REGISTER_TOKEN(Equal);
	FX_REGISTER_TOKEN(LEqual);
	FX_REGISTER_TOKEN(LessEqual);
	FX_REGISTER_TOKEN(NotEqual);
	FX_REGISTER_TOKEN(GEqual);
	FX_REGISTER_TOKEN(GreaterEqual);
	FX_REGISTER_TOKEN(Greater)
	FX_REGISTER_TOKEN(Always);

	FX_REGISTER_TOKEN(RED);
	FX_REGISTER_TOKEN(GREEN);
	FX_REGISTER_TOKEN(BLUE);
	FX_REGISTER_TOKEN(ALPHA);

	FX_REGISTER_TOKEN(ONE);
	FX_REGISTER_TOKEN(ZERO);
	FX_REGISTER_TOKEN(SRC_COLOR);
	FX_REGISTER_TOKEN(SrcColor);
	FX_REGISTER_TOKEN(ONE_MINUS_SRC_COLOR);
	FX_REGISTER_TOKEN(InvSrcColor);
	FX_REGISTER_TOKEN(SRC_ALPHA);
	FX_REGISTER_TOKEN(SrcAlpha);
	FX_REGISTER_TOKEN(ONE_MINUS_SRC_ALPHA);
	FX_REGISTER_TOKEN(InvSrcAlpha);
	FX_REGISTER_TOKEN(DST_ALPHA);
	FX_REGISTER_TOKEN(DestAlpha);
	FX_REGISTER_TOKEN(ONE_MINUS_DST_ALPHA);
	FX_REGISTER_TOKEN(InvDestAlpha);
	FX_REGISTER_TOKEN(DST_COLOR);
	FX_REGISTER_TOKEN(DestColor);
	FX_REGISTER_TOKEN(ONE_MINUS_DST_COLOR);
	FX_REGISTER_TOKEN(InvDestColor);
	FX_REGISTER_TOKEN(SRC_ALPHA_SATURATE);

	FX_REGISTER_TOKEN(NULL);

	FX_REGISTER_TOKEN(cbuffer);
	FX_REGISTER_TOKEN(PER_BATCH);
	FX_REGISTER_TOKEN(PER_INSTANCE);
	FX_REGISTER_TOKEN(PER_MATERIAL);
	FX_REGISTER_TOKEN(SKIN_DATA);
	FX_REGISTER_TOKEN(INSTANCE_DATA);

	FX_REGISTER_TOKEN(ShaderType);
	FX_REGISTER_TOKEN(ShaderDrawType);
	FX_REGISTER_TOKEN(PreprType);
	FX_REGISTER_TOKEN(Public);
	FX_REGISTER_TOKEN(NoPreview);
	FX_REGISTER_TOKEN(LocalConstants);
	FX_REGISTER_TOKEN(Cull);
	FX_REGISTER_TOKEN(SupportsAttrInstancing);
	FX_REGISTER_TOKEN(SupportsConstInstancing);
	FX_REGISTER_TOKEN(SupportsDeferredShading);
	FX_REGISTER_TOKEN(SupportsFullDeferredShading);
	FX_REGISTER_TOKEN(Decal);
	FX_REGISTER_TOKEN(DecalNoDepthOffset);
	FX_REGISTER_TOKEN(NoChunkMerging);
	FX_REGISTER_TOKEN(ForceTransPass);
	FX_REGISTER_TOKEN(AfterHDRPostProcess);
	FX_REGISTER_TOKEN(AfterPostProcess);
	FX_REGISTER_TOKEN(ForceZpass);
	FX_REGISTER_TOKEN(ForceWaterPass);
	FX_REGISTER_TOKEN(ForceDrawLast);
	FX_REGISTER_TOKEN(ForceDrawFirst);
	FX_REGISTER_TOKEN(ForceDrawAfterWater);
	FX_REGISTER_TOKEN(DepthFixup);
	FX_REGISTER_TOKEN(SingleLightPass);
	FX_REGISTER_TOKEN(HWTessellation);
	FX_REGISTER_TOKEN(VertexColors);
	FX_REGISTER_TOKEN(WaterParticle);
	FX_REGISTER_TOKEN(AlphaBlendShadows);
	FX_REGISTER_TOKEN(ZPrePass);
	FX_REGISTER_TOKEN(WrinkleBlending);

	FX_REGISTER_TOKEN(VT_DetailBendingGrass);
	FX_REGISTER_TOKEN(VT_DetailBending);
	FX_REGISTER_TOKEN(VT_WindBending);

	FX_REGISTER_TOKEN(Light);
	FX_REGISTER_TOKEN(Shadow);
	FX_REGISTER_TOKEN(Fur);
	FX_REGISTER_TOKEN(General);
	FX_REGISTER_TOKEN(Terrain);
	FX_REGISTER_TOKEN(Overlay);
	FX_REGISTER_TOKEN(NoDraw);
	FX_REGISTER_TOKEN(Custom);
	FX_REGISTER_TOKEN(Sky);
	FX_REGISTER_TOKEN(OceanShore);
	FX_REGISTER_TOKEN(Hair);
	FX_REGISTER_TOKEN(Compute);
	FX_REGISTER_TOKEN(SkinPass);
	FX_REGISTER_TOKEN(ForceGeneralPass);
	FX_REGISTER_TOKEN(EyeOverlay);

	FX_REGISTER_TOKEN(Metal);
	FX_REGISTER_TOKEN(Ice);
	FX_REGISTER_TOKEN(Water);
	FX_REGISTER_TOKEN(FX);
	FX_REGISTER_TOKEN(HDR);
	FX_REGISTER_TOKEN(HUD3D);
	FX_REGISTER_TOKEN(Glass);
	FX_REGISTER_TOKEN(Vegetation);
	FX_REGISTER_TOKEN(Particle);
	FX_REGISTER_TOKEN(GenerateClouds);
	FX_REGISTER_TOKEN(ScanWater);

	FX_REGISTER_TOKEN(NoLights);
	FX_REGISTER_TOKEN(NoMaterialState);
	FX_REGISTER_TOKEN(PositionInvariant);
	FX_REGISTER_TOKEN(TechniqueZ);
	FX_REGISTER_TOKEN(TechniqueZPrepass);
	FX_REGISTER_TOKEN(TechniqueShadowGen);
	FX_REGISTER_TOKEN(TechniqueMotionBlur);
	FX_REGISTER_TOKEN(TechniqueCustomRender);
	FX_REGISTER_TOKEN(TechniqueEffectLayer);
	FX_REGISTER_TOKEN(TechniqueDebug);
	FX_REGISTER_TOKEN(TechniqueWaterRefl);
	FX_REGISTER_TOKEN(TechniqueWaterCaustic);
	FX_REGISTER_TOKEN(TechniqueThickness);

	FX_REGISTER_TOKEN(KeyFrameParams);
	FX_REGISTER_TOKEN(KeyFrameRandColor);
	FX_REGISTER_TOKEN(KeyFrameRandIntensity);
	FX_REGISTER_TOKEN(KeyFrameRandSpecMult);
	FX_REGISTER_TOKEN(KeyFrameRandPosOffset);
	FX_REGISTER_TOKEN(Speed);

	FX_REGISTER_TOKEN(Beam);
	FX_REGISTER_TOKEN(LensOptics);
	FX_REGISTER_TOKEN(Cloud);
	FX_REGISTER_TOKEN(Ocean);

	FX_REGISTER_TOKEN(Model);
	FX_REGISTER_TOKEN(StartRadius);
	FX_REGISTER_TOKEN(EndRadius);
	FX_REGISTER_TOKEN(StartColor);
	FX_REGISTER_TOKEN(EndColor);
	FX_REGISTER_TOKEN(LightStyle);
	FX_REGISTER_TOKEN(Length);

	FX_REGISTER_TOKEN(RGBStyle);
	FX_REGISTER_TOKEN(Scale);
	FX_REGISTER_TOKEN(Blind);
	FX_REGISTER_TOKEN(SizeBlindScale);
	FX_REGISTER_TOKEN(SizeBlindBias);
	FX_REGISTER_TOKEN(IntensBlindScale);
	FX_REGISTER_TOKEN(IntensBlindBias);
	FX_REGISTER_TOKEN(MinLight);
	FX_REGISTER_TOKEN(DistFactor);
	FX_REGISTER_TOKEN(DistIntensityFactor);
	FX_REGISTER_TOKEN(FadeTime);
	FX_REGISTER_TOKEN(Layer);
	FX_REGISTER_TOKEN(Importance);
	FX_REGISTER_TOKEN(VisAreaScale);

	FX_REGISTER_TOKEN(Poly);
	FX_REGISTER_TOKEN(Identity);
	FX_REGISTER_TOKEN(FromObj);
	FX_REGISTER_TOKEN(FromLight);
	FX_REGISTER_TOKEN(Fixed);

	FX_REGISTER_TOKEN(ParticlesFile);

	FX_REGISTER_TOKEN(Gravity);
	FX_REGISTER_TOKEN(WindDirection);
	FX_REGISTER_TOKEN(WindSpeed);
	FX_REGISTER_TOKEN(WaveHeight);
	FX_REGISTER_TOKEN(DirectionalDependence);
	FX_REGISTER_TOKEN(ChoppyWaveFactor);
	FX_REGISTER_TOKEN(SuppressSmallWavesFactor);

	FX_REGISTER_TOKEN(x);
	FX_REGISTER_TOKEN(y);
	FX_REGISTER_TOKEN(z);
	FX_REGISTER_TOKEN(w);
	FX_REGISTER_TOKEN(r);
	FX_REGISTER_TOKEN(g);
	FX_REGISTER_TOKEN(b);
	FX_REGISTER_TOKEN(a);

	FX_REGISTER_TOKEN(true);
	FX_REGISTER_TOKEN(false);

	FX_REGISTER_TOKEN(0);
	FX_REGISTER_TOKEN(1);
	FX_REGISTER_TOKEN(2);
	FX_REGISTER_TOKEN(3);
	FX_REGISTER_TOKEN(4);
	FX_REGISTER_TOKEN(5);
	FX_REGISTER_TOKEN(6);
	FX_REGISTER_TOKEN(7);
	FX_REGISTER_TOKEN(8);
	FX_REGISTER_TOKEN(9);
	FX_REGISTER_TOKEN(10);
	FX_REGISTER_TOKEN(11);
	FX_REGISTER_TOKEN(12);
	FX_REGISTER_TOKEN(13);
	FX_REGISTER_TOKEN(14);
	FX_REGISTER_TOKEN(15);

	FX_REGISTER_TOKEN(ORBIS);
	FX_REGISTER_TOKEN(DURANGO);
	FX_REGISTER_TOKEN(PCDX11);
	FX_REGISTER_TOKEN(OPENGL);
	FX_REGISTER_TOKEN(VULKAN);

	FX_REGISTER_TOKEN(STANDARDSGLOBAL);

	FX_REGISTER_TOKEN(Load);
	FX_REGISTER_TOKEN(Sample);
	FX_REGISTER_TOKEN(Gather);
	FX_REGISTER_TOKEN(GatherRed);
	FX_REGISTER_TOKEN(GatherGreen);
	FX_REGISTER_TOKEN(GatherBlue);
	FX_REGISTER_TOKEN(GatherAlpha);

	FX_REGISTER_TOKEN($AutoGS_MultiRes);
	FX_REGISTER_TOKEN(Billboard);
	FX_REGISTER_TOKEN(DebugHelper);

	FXMacroItor it;
	for (it = sStaticMacros.begin(); it != sStaticMacros.end(); it++)
	{
		bool bKey = false;
		uint32 nName = CParserBin::fxToken(it->first.c_str(), &bKey);
		if (!bKey)
			nName = GetCRC32(it->first.c_str());
		SMacroFX* pr = &it->second;
		uint32 nMacros = 0;
		uint32 Macro[64];
		if (pr->m_szMacro[0])
		{
			const char* szBuf = (char*)pr->m_szMacro.c_str();
			SkipCharacters(&szBuf, " ");
			if (!szBuf[0])
				break;
			char com[1024];
			bKey = false;
			uint32 dwToken = CParserBin::NextToken(szBuf, com, bKey);
			if (!bKey)
				dwToken = GetCRC32(com);
			Macro[nMacros++] = dwToken;
		}
		AddMacro(nName, Macro, nMacros, pr->m_nMask, m_StaticMacros);
	}
	sStaticMacros.clear();

	if (!CParserBin::m_bShaderCacheGen)
	{
#if CRY_PLATFORM_ORBIS
		SetupForPlatform(SF_ORBIS);
#elif CRY_PLATFORM_DURANGO
		SetupForPlatform(SF_DURANGO);
#elif CRY_RENDERER_OPENGLES && DXGL_INPUT_GLSL
		SetupForPlatform(SF_GLES3);
#elif CRY_RENDERER_OPENGL && DXGL_INPUT_GLSL
		SetupForPlatform(SF_GL4);
#elif CRY_RENDERER_VULKAN
		SetupForPlatform(SF_VULKAN);
#else
		SetupForPlatform(SF_D3D11);
#endif
	}
}

void CParserBin::SetupForPlatform(uint32 nPlatform)
{
	CleanPlatformMacros();
	uint32 nMacro[1] = { eT_1 };

	switch (nPlatform)
	{
	case SF_D3D11:
#if CRY_PLATFORM_WINDOWS || CRY_RENDERER_OPENGL
		AddMacro(CParserBin::fxToken("PCDX11"), nMacro, 1, 0, m_StaticMacros);
#endif
		gRenDev->m_cEF.m_ShadersCache = "Shaders/Cache/D3D11/";
		gRenDev->m_cEF.m_ShadersFilter = "D3D11";
		break;

	case SF_ORBIS:
		AddMacro(CParserBin::fxToken("ORBIS"), nMacro, 1, 0, m_StaticMacros);
		gRenDev->m_cEF.m_ShadersCache = "Shaders/Cache/Orbis/";
		gRenDev->m_cEF.m_ShadersFilter = "Orbis";
		break;

	case SF_GL4:
#if CRY_PLATFORM_WINDOWS || CRY_RENDERER_OPENGL
		AddMacro(CParserBin::fxToken("PCDX11"), nMacro, 1, 0, m_StaticMacros);
		AddMacro(CParserBin::fxToken("OPENGL"), nMacro, 1, 0, m_StaticMacros);
#endif
		gRenDev->m_cEF.m_ShadersCache = "Shaders/Cache/GL4/";
		gRenDev->m_cEF.m_ShadersFilter = "GL4";
		break;

	case SF_DURANGO:
		AddMacro(CParserBin::fxToken("DURANGO"), nMacro, 1, 0, m_StaticMacros);
		gRenDev->m_cEF.m_ShadersCache = "Shaders/Cache/Durango/";
		gRenDev->m_cEF.m_ShadersFilter = "Durango";
		break;

	case SF_GLES3:
#if CRY_PLATFORM_WINDOWS || CRY_RENDERER_OPENGL
		AddMacro(CParserBin::fxToken("PCDX11"), nMacro, 1, 0, m_StaticMacros);
#endif
		gRenDev->m_cEF.m_ShadersCache = "Shaders/Cache/GLES3/";
		gRenDev->m_cEF.m_ShadersFilter = "GLES3";
		break;

	case SF_VULKAN:
		AddMacro(CParserBin::fxToken("VULKAN"), nMacro, 1, 0, m_StaticMacros);
		gRenDev->m_cEF.m_ShadersCache = "Shaders/Cache/Vulkan/";
		gRenDev->m_cEF.m_ShadersFilter = "Vulkan";
		break;

	default:
		CRY_ASSERT_MESSAGE(false, "Unknown platform.");
		gRenDev->m_cEF.m_ShadersCache = "Shaders/Cache/INVALIDPATH/";
		gRenDev->m_cEF.m_ShadersFilter = "INVALIDFILTER";
	}

	m_nPlatform = nPlatform;

	SetupFeatureDefines();
	gRenDev->m_cEF.m_Bin.InvalidateCache();
	gRenDev->m_cEF.mfInitLookups();

	SAFE_DELETE(gRenDev->m_cEF.m_pGlobalExt);
	gRenDev->m_cEF.m_pGlobalExt = gRenDev->m_cEF.mfCreateShaderGenInfo("RunTime", true);
}

const char* CParserBin::GetPlatformShaderlistName()
{
	return GetShaderlistName(CParserBin::m_nPlatform);
}

CCryNameTSCRC CParserBin::GetPlatformSpecName(CCryNameTSCRC orgName)
{
	CCryNameTSCRC nmTemp = orgName;
	if (CParserBin::m_nPlatform == SF_D3D11)
		nmTemp.add(0x200);
	else if (CParserBin::m_nPlatform == SF_GL4)
		nmTemp.add(0x300);
	else if (CParserBin::m_nPlatform == SF_VULKAN)
		nmTemp.add(0x400);
	else if (CParserBin::m_nPlatform == SF_GLES3)
		nmTemp.add(0x800);
	else if (CParserBin::m_nPlatform == SF_ORBIS)
		nmTemp.add(0x600);
	else if (CParserBin::m_nPlatform == SF_DURANGO)
		nmTemp.add(0x700);
	else if (CParserBin::m_bEndians)
		nmTemp.add(0x500);

	return nmTemp;
}

uint32 CParserBin::fxTokenKey(char* szToken, EToken eTC)
{
	g_KeyTokens[eTC] = szToken;
	return eTC;
}

uint32 CParserBin::fxToken(const char* szToken, bool* bKey)
{
	for (int i = 0; i < eT_max; i++)
	{
		if (!g_KeyTokens[i])
			continue;
		if (!strcmp(szToken, g_KeyTokens[i]))
		{
			if (bKey)
				*bKey = true;
			return i;
		}
	}
	if (bKey)
		*bKey = false;
	return eT_unknown;
}
/*uint32 CParserBin::NewUserToken(uint32 nToken, const char* psToken, bool bUseFinalTable)
   {
   const string tmpstr = psToken;
   return NewUserToken(nToken,tmpstr,bUseFinalTable);
   }*/
uint32 CParserBin::NewUserToken(uint32 nToken, const char* psToken, bool bUseFinalTable)
{
	if (nToken != eT_unknown)
		return nToken;
	nToken = GetCRC32(psToken);

	if (bUseFinalTable)
	{
		FXShaderTokenItor itor = std::lower_bound(m_TokenTable.begin(), m_TokenTable.end(), nToken, SortByToken());
		if (itor != m_TokenTable.end() && (*itor).Token == nToken)
		{
			assert(!strcmp((*itor).SToken.c_str(), psToken));
			return nToken;
		}
		STokenD TD;
		TD.SToken = psToken;
		TD.Token = nToken;
		m_TokenTable.insert(itor, TD);
	}
	else
	{
		SShaderBin* pBin = m_pCurBinShader;
		assert(pBin);
		FXShaderTokenItor itor = std::lower_bound(pBin->m_TokenTable.begin(), pBin->m_TokenTable.end(), nToken, SortByToken());
		if (itor != pBin->m_TokenTable.end() && (*itor).Token == nToken)
		{
			assert(!strcmp((*itor).SToken.c_str(), psToken));
			return nToken;
		}
		STokenD TD;
		TD.SToken = psToken;
		TD.Token = nToken;
		pBin->m_TokenTable.insert(itor, TD);
	}

	return nToken;
}

uint32 CParserBin::NextToken(const char*& buf, char* com, bool& bKey)
{
	char ch;
	int n = 0;
	while ((ch = *buf) != 0)
	{
		if (SkipChar(ch))
			break;
		com[n++] = ch;
		++buf;
		if (ch == '/')
			break;
	}
	if (!n)
	{
		if (ch != ' ')
		{
			com[n++] = ch;
			++buf;
		}
	}
	com[n] = 0;
	uint32 dwToken = fxToken(com, &bKey);
	return dwToken;
}
uint32 CParserBin::NextToken(char*& buf, char* com, bool& bKey)
{
	char ch;
	int n = 0;
	while ((ch = *buf) != 0)
	{
		if (SkipChar(ch))
			break;
		com[n++] = ch;
		++buf;
		if (ch == '/')
			break;
	}
	if (!n)
	{
		if (ch != ' ')
		{
			com[n++] = ch;
			++buf;
		}
	}
	com[n] = 0;
	uint32 dwToken = fxToken(com, &bKey);
	return dwToken;
}

bool CParserBin::AddMacro(uint32 dwName, const uint32* pMacro, int nMacroTokens, uint64 nMask, FXMacroBin& Macro)
{
	// Use 'lower_bound' instead of 'find' to avoid double traversal of the
	// underlying tree.  Iterator reused as insertion hint to 'insert()'.
	FXMacroBinItor it = Macro.lower_bound(dwName);

	SMacroBinFX* macro;
	if (it != Macro.end() && it->first == dwName)
	{
		macro = &it->second;
	}
	else
	{
		// Creates empty vector - no allocation.
		macro = &Macro.insert(it, FXMacroBinItor::value_type(dwName, SMacroBinFX()))->second;
	}
	macro->m_nMask = nMask;
	if (nMacroTokens)
	{
		macro->m_Macro.resize(nMacroTokens);
		memcpy(&macro->m_Macro[0], pMacro, nMacroTokens * sizeof(uint32));
	}
	else
		macro->m_Macro.clear();
	return true;
}

bool CParserBin::RemoveMacro(uint32 dwName, FXMacroBin& Macro)
{
	FXMacroBinItor it = Macro.find(dwName);
	if (it == Macro.end())
		return false;
	else
		Macro.erase(dwName);
	return true;
}

void CParserBin::CleanPlatformMacros()
{
	RemoveMacro(CParserBin::fxToken("DURANGO"), m_StaticMacros);
	RemoveMacro(CParserBin::fxToken("ORBIS"),   m_StaticMacros);
	RemoveMacro(CParserBin::fxToken("PCDX11"),  m_StaticMacros);
	RemoveMacro(CParserBin::fxToken("OPENGL"),  m_StaticMacros);
	RemoveMacro(CParserBin::fxToken("VULKAN"),  m_StaticMacros);
}

const SMacroBinFX* CParserBin::FindMacro(uint32 dwName, FXMacroBin& Macro)
{
	FXMacroBinItor it = Macro.find(dwName);
	if (it != Macro.end())
		return &it->second;
	return NULL;
}

bool CParserBin::GetBool(SParserFrame& Frame)
{
	if (Frame.IsEmpty())
		return true;
	EToken eT = GetToken(Frame);
	if (eT == eT_true || eT == eT_1)
		return true;
	if (eT == eT_false || eT == eT_0)
		return false;
	assert(0);
	return false;
}

CCryNameR CParserBin::GetNameString(SParserFrame& Frame)
{
	if (Frame.IsEmpty())
		return CCryNameR("");

	stack_string Str;
	int nCur = Frame.m_nFirstToken;
	int nLast = Frame.m_nLastToken;
	while (nCur <= nLast)
	{
		int nTok = m_Tokens[nCur++];
		const char* szStr = GetString(nTok);
		if (Str.size() && !SkipChar(Str[Str.size() - 1]) && !SkipChar(szStr[0]))
			Str += " ";
		Str += szStr;
	}
	return CCryNameR(Str.c_str());
}

string CParserBin::GetString(SParserFrame& Frame)
{
	if (Frame.IsEmpty())
		return "";

	stack_string Str;
	int nCur = Frame.m_nFirstToken;
	int nLast = Frame.m_nLastToken;
	while (nCur <= nLast)
	{
		int nTok = m_Tokens[nCur++];
		const char* szStr = GetString(nTok);
		if (Str.size() && !SkipChar(Str[Str.size() - 1]) && !SkipChar(szStr[0]))
			Str += " ";
		Str += szStr;
	}
	return string(Str.c_str());
}

const char* CParserBin::GetString(uint32 nToken, const FXShaderToken& Table, bool bOnlyKey)
{
	if (nToken < eT_max)
	{
		assert(g_KeyTokens[nToken]);
		return g_KeyTokens[nToken];
	}
	if (!bOnlyKey)
	{
		auto it = std::lower_bound(Table.begin(), Table.end(), nToken, SortByToken());
		if (it != Table.end() && it->Token == nToken)
			return it->SToken.c_str();
	}

	assert(0);
	return "";
}

const char* CParserBin::GetString(uint32 nToken, bool bOnlyKey)
{
	return GetString(nToken, m_TokenTable, bOnlyKey);
}

static void sCR(TArray<char>& Text, int nLevel)
{
	Text.AddElem('\n');
	for (int i = 0; i < nLevel; i++)
	{
		Text.AddElem(' ');
		Text.AddElem(' ');
	}
}

bool CParserBin::CorrectScript(uint32* pTokens, uint32& i, uint32 nT, TArray<char>& Text)
{
	bool bRes = true;

	int nTex = Text.Num() - 1;
	int nTT = nTex;
	while (nTex > 0)
	{
		char c = Text[nTex];
		if (c <= 32)
		{
			nTex++;
			break;
		}
		nTex--;
	}
	if (strncmp(&Text[nTex], "float", 5))
	{
		assert(0);
		iLog->Log("Wrong script tokens...");
		return false;
	}
	memset(&Text[nTex], 0x20, nTT - nTex + 1);
	i++;
	while (i < nT)
	{
		uint32 nTok = pTokens[i];
		if (nTok == eT_semicolumn)
			return true;
		i++;
	}
	return false;
}

bool CParserBin::ConvertToAscii(uint32* pTokens, uint32 nT, const FXShaderToken& Table, TArray<char>& Text, bool bInclSkipTokens)
{
	uint32 i;
	bool bRes = true;

	const char* szPrev = " ";
	int nLevel = 0;
	for (i = 0; i < nT; i++)
	{
		uint32 nToken = pTokens[i];
		if (nToken == 0)
		{
			Text.Copy("\n", 1);
			continue;
		}
		if (!bInclSkipTokens)
		{
			if (nToken == eT_skip)
			{
				i++;
				continue;
			}
			if (nToken == eT_skip_1)
			{
				while (i < nT)
				{
					nToken = pTokens[i];
					if (nToken == eT_skip_2)
						break;
					i++;
				}
				assert(i < nT);
				continue;
			}
		}
		const char* szStr = GetString(nToken, Table, false);
		assert(szStr);
		if (!szStr || !szStr[0])
		{
			bRes = CParserBin::CorrectScript(pTokens, i, nT, Text);
		}
		else
		{
			if (nToken == eT_semicolumn || nToken == eT_br_cv_1)
			{
				if (nToken == eT_br_cv_1)
				{
					sCR(Text, nLevel);
					nLevel++;
				}
				Text.Copy(szStr, strlen(szStr));
				if (nToken == eT_semicolumn)
				{
					if (i + 1 < nT && pTokens[i + 1] == eT_br_cv_2)
						sCR(Text, nLevel - 1);
					else
						sCR(Text, nLevel);
				}
				else if (i + 1 < nT)
				{
					if (pTokens[i + 1] < eT_br_rnd_1 || pTokens[i + 1] >= eT_float)
						sCR(Text, nLevel);
				}
			}
			else
			{
				if (i + 1 < nT)
				{
					if (Text.Num())
					{
						char cPrev = Text[Text.Num() - 1];
						if (!SkipChar((uint8)cPrev) && !SkipChar((uint8)szStr[0]))
							Text.AddElem(' ');
					}
				}
				Text.Copy(szStr, strlen(szStr));
				if (nToken == eT_br_cv_2)
				{
					nLevel--;
					if (i + 1 < nT && pTokens[i + 1] != eT_semicolumn)
						sCR(Text, nLevel);
				}
			}
		}
	}
	Text.AddElem(0);

	return bRes;
}

void CParserBin::MergeTable(SShaderBin* pBin)
{
	FXShaderTokenItor it = m_TokenTable.begin();
	FXShaderTokenItor end = m_TokenTable.end();
	FXShaderTokenItor bit = pBin->m_TokenTable.begin();
	FXShaderTokenItor bend = pBin->m_TokenTable.end();

	FXShaderToken newTable;
	newTable.reserve(m_TokenTable.size() + pBin->m_TokenTable.size());
	while (true)
	{
		STokenD* last = newTable.size() ? &(*newTable.rbegin()) : NULL;

		unsigned mask = 0;
		mask |= (bit != bend) << 0;
		mask |= (it != end) << 1;

		switch (mask)
		{
		// No iterators valid anymore, nothing left to do
		case 0x0:
			goto done;

		// Other iterator valid, internal iterator invalid
		case 0x1:
			if (!last || bit->Token != last->Token)
				newTable.push_back(*bit);
			++bit;
			break;

		// Other iterator invalid, internal iterator valid
		case 0x2:
			if (!last || it->Token != last->Token)
				newTable.push_back(*it);
			++it;
			break;

		// Noth iterators valid
		case 0x3:
			{
				STokenD& iTD = (*it);
				STokenD& oTD = (*bit);
				if (iTD.Token < oTD.Token)
				{
					if (!last || it->Token != last->Token)
						newTable.push_back(*it);
					++it;
				}
				else
				{
					if (!last || bit->Token != last->Token)
						newTable.push_back(*bit);
					++bit;
				}
			}
			break;
		}
		;
	}

	// Verify that the merging results in a sorted table
#if defined _DEBUG
	for (std::size_t i = 1; i < newTable.size(); ++i)
	{
		assert(newTable[i - 1].Token <= newTable[i].Token);
	}
#endif

done:
	swap(newTable, m_TokenTable);
}

bool CParserBin::IgnorePreprocessBlock(const uint32* pTokens, uint32& nT, int nMaxTokens, PodArray<uint32>& tokensBuffer, int nPass)
{
	int nLevel = 0;
	bool bEnded = false;
	int nTFirst = nT;
	while (*pTokens != 0)
	{
		if ((int)nT >= nMaxTokens)
			break;
		uint32 nToken = NextToken(pTokens, nT, nMaxTokens - 1);
		if (nToken >= eT_if && nToken <= eT_ifndef_2)
		{
			if (!nPass)
				InsertSkipTokens(pTokens, nT - 1, nMaxTokens, false, tokensBuffer);
			nLevel++;
			continue;
		}
		if (nToken == eT_endif)
		{
			if (!nLevel)
			{
				bEnded = true;
				nT--;
				break;
			}
			if (!nPass)
				InsertSkipTokens(pTokens, nT - 1, nMaxTokens, true, tokensBuffer);
			nLevel--;
		}
		else if (nToken == eT_else || nToken == eT_elif || nToken == eT_elif_2)
		{
			if (!nLevel)
			{
				nT--;
				break;
			}
			if (!nPass)
				InsertSkipTokens(pTokens, nT - 1, nMaxTokens, nToken == eT_else, tokensBuffer);
		}
	}
	if ((int)nT >= nMaxTokens)
	{
		assert(0);
		Warning("Couldn't find #endif directive for associated #ifdef");
		return false;
	}

	return bEnded;
}

void CParserBin::InsertSkipTokens(const uint32* pTokens, uint32 nStart, uint32 nTokens, bool bSingle, PodArray<uint32>& tokensBuffer)
{
	SParserFrame Fr;
	Fr.m_nFirstToken = nStart;
	Fr.m_nLastToken = nStart;

	if (!bSingle)
	{
		uint32 nS = nStart + 1;
		CheckIfExpression(pTokens, nS, 0);
		Fr.m_nLastToken = nS - 1;
	}

	if (Fr.m_nLastToken - Fr.m_nFirstToken == 0)
	{
		tokensBuffer.push_back(eT_skip);
		tokensBuffer.push_back(pTokens[Fr.m_nFirstToken]);
	}
	else
	{
		tokensBuffer.push_back(eT_skip_1);
		while (Fr.m_nFirstToken <= Fr.m_nLastToken)
		{
			tokensBuffer.push_back(pTokens[Fr.m_nFirstToken]);
			Fr.m_nFirstToken++;
		}
		tokensBuffer.push_back(eT_skip_2);
	}
}

bool CParserBin::CheckIfExpression(const uint32* pTokens, uint32& nT, int nPass, uint64* nMask)
{
	uint32 tmpBuf[64] = { 0 };
	byte bRes[64] = { 0 };
	byte bOr[64] = { 0 };
	int nLevel = 0;
	int i;
	while (true)
	{
		uint32 nToken = pTokens[nT];
		if (nToken == eT_br_rnd_1) // check for '('
		{
			nT++;
			int n = 0;
			int nD = 0;
			while (true)
			{
				int nTok = pTokens[nT];
				if (nTok == eT_br_rnd_1) // check for '('
					n++;
				else if (nTok == eT_br_rnd_2) // check for ')'
				{
					if (!n)
					{
						tmpBuf[nD] = 0;
						nT++;
						break;
					}
					n--;
				}
				else if (nTok == 0)
					return false;
				tmpBuf[nD++] = nTok;
				nT++;
			}
			uint32 nT2 = 0;
			bRes[nLevel] = CheckIfExpression(&tmpBuf[0], nT2, nPass, nMask);
			nLevel++;
			bOr[nLevel] = 255;
		}
		else
		{
			uint32 nTok = pTokens[nT++];
			bool bNeg = false;
			if (nTok == eT_excl)
			{
				bNeg = true;
				nTok = pTokens[nT++];
			}
			const SMacroBinFX* pFound = FindMacro(nTok, m_Macros[nPass]);
			if (!pFound)
				pFound = FindMacro(nTok, m_StaticMacros);
			if (!pFound && !nPass)
				pFound = FindMacro(nTok, m_Macros[1]);
			bRes[nLevel] = (pFound) ? true : false;

			if (pFound && nMask)
				*nMask |= pFound->m_nMask;

			if (nTok == eT_1)
				bRes[nLevel] = true;
			if (bNeg)
				bRes[nLevel] = !bRes[nLevel];
			nLevel++;
			bOr[nLevel] = 255;
		}
		uint32 nTok = pTokens[nT];
		if (nTok == eT_or)
		{
			bOr[nLevel] = true;
			nT++;
			assert(pTokens[nT] == eT_or);
			if (pTokens[nT] == eT_or)
				nT++;
		}
		else if (nTok == eT_and)
		{
			bOr[nLevel] = false;
			nT++;
			assert(pTokens[nT] == eT_and);
			if (pTokens[nT] == eT_and)
				nT++;
		}
		else
			break;
	}
	byte Res = false;
	for (i = 0; i < nLevel; i++)
	{
		if (!i)
			Res = bRes[i];
		else
		{
			assert(bOr[i] != 255);
			if (bOr[i])
				Res = Res | bRes[i];
			else
				Res = Res & bRes[i];
		}
	}
	return Res != 0;
}

void CParserBin::BuildSearchInfo()
{
	/*  int i;
	   for (i=0; i<m_Tokens.size(); i++)
	    {
	    uint32 nToken = m_Tokens[i];
	    if (nToken < eT_max)
	      {
	      if (m_KeyOffsets.size() <= nToken)
	        m_KeyOffsets.resize(nToken+1);
	      std::vector<int>& Dst = m_KeyOffsets[nToken];
	      Dst.push_back(i);
	      }
	      else
	      {
	      FXShaderTokenItor itor = m_Table.find(nToken);
	      if (itor != m_Table.end())
	        itor->second.Offsets.push_back(i);
	      else
	      {
	        assert(0);
	      }
	    }
	   }
	   m_bEmbeddedSearchInfo = true;*/
}

bool CParserBin::PreprocessTokens(ShaderTokensVec& Tokens, int nPass, PodArray<uint32>& tokensBuffer)
{
	bool bRet = true;
	uint32 nTokenParam;

	uint32 nT = 0;
	const uint32* pTokens = &Tokens[0];
	const uint32 nTSize = Tokens.size();
	while (nT < nTSize)
	{
		uint32 nToken = NextToken(pTokens, nT, nTSize - 1);
		if (!nToken)
			break;
		bool bFirst = false;
		switch (nToken)
		{
		case eT_include:
			{
				nTokenParam = pTokens[nT++];
				const char* szName = GetString(nTokenParam, false);
				assert(szName);
				SShaderBin* pBin = gRenDev->m_cEF.m_Bin.GetBinShader(szName, true, 0);
				if (!pBin)
				{
					iLog->Log("Warning: Couldn't find include file '%s'", szName);
					assert(0);
				}
				else
				{
					assert(pBin);
					MergeTable(pBin);
					pBin->Lock();
					PreprocessTokens(pBin->m_Tokens, nPass, tokensBuffer);
					pBin->Unlock();
				}
			}
			break;
		case eT_define:
		case eT_define_2:
			{
				uint32 nMacro = 0;
				int n = nPass;
				uint64 nMask = 0;
				nTokenParam = pTokens[nT++];
				/*const char *szname = GetString(nTokenParam);
				   if (!stricmp(szname, "%_SHADOW_GEN"))
				   {
				   int nnn = 0;
				   }*/
				const uint32* pMacro = &pTokens[nT];
				while (pMacro[nMacro])
					nMacro++;
				if (nToken == eT_define_2)
				{
					n = 1;
					if (nMacro)
						nMask = GetInt(pMacro[0]);
				}
				else
				{
					for (uint32 i = 0; i < m_IfAffectMask.Num(); i++)
					{
						nMask |= m_IfAffectMask[i];
					}
				}
				AddMacro(nTokenParam, pMacro, nMacro, nMask, m_Macros[n]);

				if (nPass == 0)
				{
					if (sfxIFDef.Num())
					{
						AddMacro(nTokenParam, pMacro, nMacro, nMask, m_Macros[1]);
					}

					tokensBuffer.push_back(eT_define_2);
					tokensBuffer.push_back(nTokenParam);
					tokensBuffer.AddList((uint32*)pMacro, nMacro + 1);
				}
				nT += nMacro + 1;
			}
			break;
		case eT_undefine:
			{
				uint32 nMacro = 0;
				nTokenParam = pTokens[nT++];
				int n = nPass;
				FXMacroBinItor it = m_Macros[nPass].find(nTokenParam);
				if (it == m_Macros[n].end() && !nPass)
				{
					it = m_Macros[1].find(nTokenParam);
					n = 1;
				}
				if (it == m_Macros[n].end())
				{
					Warning("Couldn't find macro '%s'", GetString(nTokenParam));
				}
				else
				{
					m_Macros[n].erase(nTokenParam);
				}
			}
			break;
		case eT_if:
		case eT_ifdef:
		case eT_ifndef:
			bFirst = true;
		case eT_if_2:
		case eT_ifdef_2:
		case eT_ifndef_2:
			if (nPass != 2 && ((nPass == 0 && !bFirst) || (nPass == 1 && bFirst)))
			{
				if (nPass == 1)
				{
					assert(0);
				}

				sfxIFIgnore.AddElem(true);
				sfxIFDef.AddElem(false);
				tokensBuffer.push_back(nToken);

				uint64 nIfMask = 0;
				uint32 nS = nT;
				CheckIfExpression(pTokens, nT, 0, &nIfMask);
				tokensBuffer.AddList((uint32*)pTokens + nS, nT - nS);

				m_IfAffectMask.AddElem(nIfMask);
			}
			else
			{
				uint64 nIfMask = 0;
				sfxIFIgnore.AddElem(false);
				if (!nPass)
					InsertSkipTokens(pTokens, nT - 1, nTSize, false, tokensBuffer);
				bool bRes = CheckIfExpression(pTokens, nT, nPass, &nIfMask);
				m_IfAffectMask.AddElem(nIfMask);
				if (nToken == eT_ifndef || nToken == eT_ifndef_2)
					bRes = !bRes;
				if (!bRes)
				{
					IgnorePreprocessBlock(pTokens, nT, nTSize, tokensBuffer, nPass);
					sfxIFDef.AddElem(false);
				}
				else
				{
					sfxIFDef.AddElem(true);
				}
			}
			break;
		case eT_elif:
			bFirst = true;
		case eT_elif_2:
			{
				int nLevel = sfxIFDef.Num() - 1;
				if (nLevel < 0)
				{
					assert(0);
					Warning("#elif without #ifdef");
					return false;
				}
				if (sfxIFIgnore[nLevel] == true)
					tokensBuffer.push_back(nToken);
				else
				{
					if (!nPass)
						InsertSkipTokens(pTokens, nT - 1, Tokens.size(), false, tokensBuffer);
					if (sfxIFDef[nLevel] == true)
					{
						IgnorePreprocessBlock(pTokens, nT, Tokens.size(), tokensBuffer, nPass);
					}
					else
					{
						if (!bFirst && !nPass)
						{
							sfxIFIgnore[nLevel] = true;
							sfxIFDef[nLevel] = false;
							tokensBuffer.push_back(eT_if_2);

							uint64 nIfMask = 0;
							uint32 nS = nT;
							CheckIfExpression(pTokens, nT, 0, &nIfMask);
							tokensBuffer.AddList((uint32*)pTokens + nS, nT - nS);

							m_IfAffectMask[nLevel] = nIfMask;
						}
						else
						{
							uint64 nIfMask = 0;
							bool bRes = CheckIfExpression(pTokens, nT, nPass, &nIfMask);
							if (!bRes)
								IgnorePreprocessBlock(pTokens, nT, Tokens.size(), tokensBuffer, nPass);
							else
								sfxIFDef[nLevel] = true;

							m_IfAffectMask[nLevel] = nIfMask;
						}
					}
				}
			}
			break;
		case eT_else:
			{
				int nLevel = sfxIFDef.Num() - 1;
				if (nLevel < 0)
				{
					assert(0);
					Warning("#else without #ifdef");
					return false;
				}
				if (sfxIFIgnore[nLevel] == true)
					tokensBuffer.push_back(nToken);
				else
				{
					if (!nPass)
						InsertSkipTokens(pTokens, nT - 1, Tokens.size(), true, tokensBuffer);
					if (sfxIFDef[nLevel] == true)
					{
						bool bEnded = IgnorePreprocessBlock(pTokens, nT, Tokens.size(), tokensBuffer, nPass);
						if (!bEnded)
						{
							assert(0);
							Warning("#else or #elif after #else");
							return false;
						}
					}
				}
			}
			break;
		case eT_endif:
			{
				int nLevel = sfxIFDef.Num() - 1;
				if (nLevel < 0)
				{
					assert(0);
					Warning("#endif without #ifdef");
					return false;
				}
				if (sfxIFIgnore[nLevel] == true)
					tokensBuffer.push_back(nToken);
				else if (!nPass)
					InsertSkipTokens(pTokens, nT - 1, Tokens.size(), true, tokensBuffer);
				sfxIFDef.Remove(nLevel);
				sfxIFIgnore.Remove(nLevel);
				m_IfAffectMask.Remove(nLevel);
			}
			break;
		case eT_warning:
			{
				const char* szStr = GetString(pTokens[nT++]);
				Warning("%s", szStr);
			}
			break;
		case eT_register_env:
			{
				const char* szStr = GetString(pTokens[nT++]);
				fxRegisterEnv(szStr);
			}
			break;
		case eT_ifcvar:
		case eT_ifncvar:
			{
				const char* szStr = GetString(pTokens[nT++]);
				sfxIFIgnore.AddElem(false);
				ICVar* pVar = iConsole->GetCVar(szStr);
				int nVal = 0;
				if (!pVar)
					iLog->Log("Warning: couldn't find variable '%s'", szStr);
				else
					nVal = pVar->GetIVal();
				if (nToken == eT_ifncvar)
					nVal = !nVal;
				if (!nVal)
				{
					IgnorePreprocessBlock(pTokens, nT, Tokens.size(), tokensBuffer, nPass);
					sfxIFDef.AddElem(false);
				}
				else
					sfxIFDef.AddElem(true);
			}
			break;
		case eT_elifcvar:
			{
				int nLevel = sfxIFDef.Num() - 1;
				if (nLevel < 0)
				{
					assert(0);
					Warning("#elifcvar without #ifcvar or #ifdef");
					return false;
				}
				sfxIFIgnore.AddElem(false);
				if (sfxIFDef[nLevel] == true)
				{
					IgnorePreprocessBlock(pTokens, nT, Tokens.size(), tokensBuffer, nPass);
				}
				else
				{
					const char* szStr = GetString(pTokens[nT++]);
					ICVar* pVar = iConsole->GetCVar(szStr);
					int nVal = 0;
					if (!pVar)
						iLog->Log("Warning: couldn't find variable '%s'", szStr);
					else
						nVal = pVar->GetIVal();
					if (!nVal)
						IgnorePreprocessBlock(pTokens, nT, Tokens.size(), tokensBuffer, nPass);
					else
						sfxIFDef[nLevel] = true;
				}
			}
			break;
		default:
			{
				FXMacroBinItor it = m_Macros[nPass].find(nToken);
				if (it != m_Macros[nPass].end())
				{
					// Found macro
					SMacroBinFX* pM = &it->second;
					for (uint32 i = 0; i < pM->m_Macro.size(); i++)
					{
						tokensBuffer.push_back(pM->m_Macro[i]);
					}
				}
				else
				{
					tokensBuffer.push_back(nToken);
				}
			}
			break;
		}
	}

	return bRet;
}

bool CParserBin::Preprocess(int nPass, ShaderTokensVec& Tokens, const FXShaderToken& srcTable)
{
	m_IfAffectMask.Reserve(5);
	m_IfAffectMask.SetUse(0);
	sfxIFDef.SetUse(0);
	sfxIFIgnore.SetUse(0);

	m_Macros[nPass].clear();

	// Use a buffer to process tokens, transfer to the
	// real storage in Tokens once we know the full quantity
	enum { TOKENS_BUFFER_SIZE = 100000 };

	PodArray<uint32> tokensBuffer(TOKENS_BUFFER_SIZE);

	m_TokenTable = srcTable;
	bool bRes = PreprocessTokens(Tokens, nPass, tokensBuffer);
#ifndef _RELEASE
	if (tokensBuffer.Size() > TOKENS_BUFFER_SIZE)
	{
		CryLogAlways("CParserBin::Preprocess: tokenBuffer has been exceeded (buffer=%d, count=%u). Adjust buffer size to remove unnecessary allocs.", TOKENS_BUFFER_SIZE, tokensBuffer.Size());
	}
#endif
	m_Tokens.reserve(tokensBuffer.Size());
	m_Tokens.SetUse(0);
	m_Tokens.Copy(tokensBuffer.GetElements(), tokensBuffer.Count());

	/*{
	   TArray<char> Text;
	   ConvertToAscii(&tokensBuffer[0], tokensBuffer.size(), m_TokenTable, Text, true);
	   FILE *fp;
	   if (!nPass)
	    fp = fopen("SrcBin1.cg", "wb");
	   else
	    fp = fopen("SrcBin2.cg", "wb");
	   if (fp)
	   {
	    fwrite(&Text[0], 1, Text.Num()-1, fp);
	    fclose(fp);
	   }
	   }*/

	if (!nPass)
		BuildSearchInfo();

	assert(!sfxIFDef.Num());
	assert(!sfxIFIgnore.Num());

	/*{
	   TArray<char> Text;
	   ConvertToAscii(&m_Tokens[0], m_Tokens.size(), m_Table, Text);
	   FILE *fp;
	   if (!nPass)
	    fp = fopen("PreprBin1.cg", "wb");
	   else
	    fp = fopen("PreprBin2.cg", "wb");
	   if (fp)
	   {
	    fwrite(&Text[0], 1, Text.Num()-1, fp);
	    fclose(fp);
	   }
	   }*/

	return bRes;
}

int CParserBin::CopyTokens(SParserFrame& Fragment, std::vector<uint32>& NewTokens)
{
	if (Fragment.IsEmpty())
		return 0;
	int nCopy = Fragment.m_nLastToken - Fragment.m_nFirstToken + 1;
	int nStart = NewTokens.size();
	NewTokens.resize(nStart + nCopy);
	memcpy(&NewTokens[nStart], &m_Tokens[Fragment.m_nFirstToken], nCopy * sizeof(uint32));

	return nCopy;
}

int CParserBin::CopyTokens(SCodeFragment* pCF, PodArray<uint32>& SHData, TArray<SCodeFragment>& Replaces, TArray<uint32>& NewTokens, uint32 nID)
{
	int nRepl = -1;
	uint32 i;

	for (i = 0; i < Replaces.size(); i++)
	{
		SCodeFragment* pFR = &Replaces[i];

		if (pFR->m_dwName == nID)
			break;
	}
	if (i != Replaces.size())
	{
		assert(!(i & 1));
		nRepl = i;
	}
	int nDst = SHData.size();
	int nSrc = pCF->m_nFirstToken;
	uint32* pSrc = &m_Tokens[0];
	int nSize = pCF->m_nLastToken - nSrc + 1;
	if (nRepl >= 0)
	{
		int nDstStart = nDst;
		uint32* pSrc2 = &NewTokens[0];
		while (nSrc <= (int)pCF->m_nLastToken)
		{
			if (nRepl >= (int)Replaces.size() || Replaces[nRepl].m_dwName != nID)
			{
				int nCopy = pCF->m_nLastToken - nSrc + 1;
				SHData.resize(nDst + nCopy);
				memcpy(&SHData[nDst], &pSrc[nSrc], nCopy * sizeof(uint32));
				nSrc += nCopy;
				nDst += nCopy;
			}
			else
			{
				int nCopy = Replaces[nRepl].m_nFirstToken - nSrc;
				if (nCopy)
				{
					assert(nCopy > 0);
					SHData.resize(nDst + nCopy);
					memcpy(&SHData[nDst], &pSrc[nSrc], nCopy * sizeof(uint32));
					nSrc += nCopy + (Replaces[nRepl].m_nLastToken - Replaces[nRepl].m_nFirstToken + 1);
					nDst += nCopy;
				}
				nRepl++;
				nCopy = Replaces[nRepl].m_nLastToken - Replaces[nRepl].m_nFirstToken + 1;
				SHData.resize(nDst + nCopy);
				memcpy(&SHData[nDst], &pSrc2[Replaces[nRepl].m_nFirstToken], nCopy * sizeof(uint32));
				nDst += nCopy;
				nRepl++;
			}
		}
		return SHData.size() - nDstStart;
	}
	else
	{
		SHData.resize(nDst + nSize);
		memcpy(&SHData[nDst], &pSrc[nSrc], nSize * sizeof(uint32));
		return nSize;
	}
}

int32 CParserBin::FindToken(uint32 nStart, uint32 nLast, const uint32* pTokens, uint32 nToken)
{
	while (nStart <= nLast)
	{
		if (pTokens[nStart] == nToken)
			return nStart;
		nStart++;
	}
	return -1;
}

int32 CParserBin::FindToken(uint32 nStart, uint32 nLast, uint32 nToken)
{
	if (nStart >= m_Tokens.size() || nLast >= m_Tokens.size())
		return -1;

	//if (!m_bEmbeddedSearchInfo)
	return FindToken(nStart, nLast, &m_Tokens[0], nToken);
	/*std::vector<int> *pSI;
	   if (nToken < eT_max)
	   {
	   if (nToken >= m_KeyOffsets.size())
	    return -1;
	   pSI = &m_KeyOffsets[nToken];
	   }
	   else
	   {
	   FXShaderTokenItor itor = m_Table.find(nToken);
	   if (itor == m_Table.end())
	    return -1;
	   pSI = &itor->second.Offsets;
	   }
	   for (int i=0; i<pSI->size(); i++)
	   {
	   int nOffs = (*pSI)[i];
	   if (nOffs>=nStart && nOffs<=nLast)
	    return nOffs;
	   }
	   return -1;*/
}

int32 CParserBin::FindToken(uint32 nStart, uint32 nLast, const uint32* pToks)
{
	uint32* pTokens = &m_Tokens[0];
	while (nStart <= nLast)
	{
		int n = 0;
		uint32 nTok;
		while (nTok = pToks[n])
		{
			if (pTokens[nStart] == nTok)
				return nStart;
			n++;
		}
		nStart++;
	}
	return -1;
}

static ETokenStorageClass sCheckForModificator(uint32 nToken)
{
	switch (nToken)
	{
	case eT_const:
		return eTS_const;
	case eT_static:
		return eTS_static;
	case eT_shared:
		return eTS_shared;
	case eT_groupshared:
		return eTS_groupshared;
	default:
		return eTS_default;
	}
}
int CParserBin::GetNextToken(uint32& nStart, ETokenStorageClass& nTokenStorageClass)
{
	uint32* pTokens = &m_Tokens[0];
	uint32 nTokensSize = m_Tokens.size();
	ETokenStorageClass nNewTokenStorageClass = eTS_default;
	while (true)
	{
		bool bFunction = false;

		if (m_CurFrame.m_nCurToken >= m_CurFrame.m_nLastToken)
			return -1;

		uint32 nToken;
		if ((nToken = pTokens[m_CurFrame.m_nCurToken]) == eT_unknown)
			return -2;

		nStart = m_CurFrame.m_nCurToken;
		bool bFound = false;

		if (nToken == eT_quote)
		{
			m_CurFrame.m_nCurToken++;
			continue;
		}
		if (nToken == eT_skip)
		{
			m_CurFrame.m_nCurToken += 2;
			continue;
		}
		if (nToken == eT_skip_1)
		{
			while (m_CurFrame.m_nCurToken <= m_CurFrame.m_nLastToken)
			{
				nToken = pTokens[m_CurFrame.m_nCurToken++];
				if (nToken == eT_skip_2)
					break;
			}
			continue;
		}

		// Check for storage class- if existing, add all tokens in line (its global constant - should not go to any constant buffer)
		nNewTokenStorageClass = sCheckForModificator(nToken);
		if (nNewTokenStorageClass != eTS_default)
		{
			nTokenStorageClass = nNewTokenStorageClass;

			SCodeFragment Fr;
			Fr.m_eType = eFT_StorageClass;                              // Set code fragment type
			Fr.m_nFirstToken = m_CurFrame.m_nCurToken;                  // Set first token offset for code fragment
			while (pTokens[m_CurFrame.m_nCurToken] != eT_semicolumn)
			{
				if (m_CurFrame.m_nCurToken + 1 == nTokensSize)
					break;
				//const char *pszToken0 = GetString( pTokens[m_CurFrame.m_nCurToken]  );  // handy for debugging - do not remove for now plz
				m_CurFrame.m_nCurToken++;
			}

			Fr.m_nLastToken = m_CurFrame.m_nCurToken++;                 // Set last token offset to code fragment
			Fr.m_dwName = pTokens[Fr.m_nLastToken - 1];                 // Set fragment name (this is used for parser to search and discard fragments of code if not used in functions or structures)
			m_CodeFragments.push_back(Fr);                              // Finally add to list

			//const char *pszToken0 = GetString( Fr.m_dwName  );  // handy for debugging - do not remove for now plz

			continue;
		}

		// if preprocessor
		if (nToken >= eT_include && nToken <= eT_elifcvar)
		{
			SCodeFragment Fr;
			Fr.m_nFirstToken = m_CurFrame.m_nCurToken;
			if (nToken >= eT_if && nToken <= eT_elif || nToken == eT_define || nToken == eT_define_2)
			{
				while (pTokens[m_CurFrame.m_nCurToken])
				{
					if (m_CurFrame.m_nCurToken + 1 == nTokensSize)
						break;
					m_CurFrame.m_nCurToken++;
				}
			}
			Fr.m_nLastToken = m_CurFrame.m_nCurToken++;
			m_CodeFragments.push_back(Fr);
		}
		else
		{
			m_CurFrame.m_nCurToken = nStart;
			// Check for function
			uint32 nLastTok = nStart;
			uint32 nFnName;
			int nBrIndex = -1;
			if (m_CurFrame.m_nCurToken + 4 < m_CurFrame.m_nLastToken)
			{
				uint32 nFnRet = pTokens[m_CurFrame.m_nCurToken];
				// DX11 stuff
				uint32 nCur = m_CurFrame.m_nCurToken + 1;
				uint32 nCount = 0;

				while (true)
				{
					switch (nFnRet)
					{
					case eT_br_sq_1: 
					nCount++;
					nLastTok = FindToken(nCur, m_CurFrame.m_nLastToken, eT_br_sq_2);
						if (nLastTok < 0)
					{
							Warning("Unmatched [");
							assert(0);
							nLastTok = nCur;
						}
						nLastTok++;
						nFnRet = pTokens[nLastTok];
						nCur = nLastTok + 1;
						continue;

					case eT_skip_1:
						nCount++;
						nLastTok = FindToken(nCur, m_CurFrame.m_nLastToken, eT_skip_2);
						if (nLastTok < 0)
						{
							Warning("Unmatched #skip_(");
							assert(0);
							nLastTok = nCur;
						}
						nLastTok++;
						nFnRet = pTokens[nLastTok];
						nCur = nLastTok + 1;
						continue;

					case eT_skip:
							pTokens[nLastTok + 1] = eT_skip;
							nLastTok += 2;
							nFnRet = pTokens[nLastTok];
						nCur = nLastTok + 1;
						continue;
					}

					break;
				}
				nFnName = pTokens[nLastTok + 1];
				//const char *szFn = GetString(nFnName);
				if (pTokens[nLastTok + 2] == eT_br_rnd_1)
				{
					nBrIndex = nLastTok + 3;
					nLastTok = FindToken(nLastTok + 3, m_CurFrame.m_nLastToken, eT_br_cv_1);
					int nRecurse = 0;
					while (nLastTok <= m_CurFrame.m_nLastToken)
					{
						uint32 nT = pTokens[nLastTok];
						if (nT == eT_br_cv_1)
							nRecurse++;
						else if (nT == eT_br_cv_2)
						{
							nRecurse--;
							if (nRecurse == 0)
							{
								bFunction = true;
								break;
							}
						}
						nLastTok++;
					}
				}
			}
			if (bFunction)
			{
				// check for function if expressions
				assert(nBrIndex > 0);
				uint32 nIdTok = FindToken(nBrIndex, m_CurFrame.m_nLastToken, eT_br_rnd_2);
				assert(nIdTok > 0);
				if (nIdTok > 0 && pTokens[nIdTok + 1] != eT_br_cv_1)
				{
					nIdTok++;
					while (pTokens[nIdTok] != eT_br_cv_1)
					{
						if (pTokens[nIdTok] == eT_skip)
						{
							pTokens[nIdTok + 1] = eT_skip;
							nIdTok += 2;
						}
						else if (pTokens[nIdTok] == eT_skip_1)
						{
							while (pTokens[nIdTok] != eT_skip_2)
							{
								pTokens[nIdTok++] = eT_skip;
							}
							pTokens[nIdTok++] = eT_skip;
						}
						else
							nIdTok++;
					}
				}

				SCodeFragment Fr;
				Fr.m_nFirstToken = m_CurFrame.m_nCurToken;
				Fr.m_nLastToken = nLastTok;
				Fr.m_dwName = nFnName;
#ifdef _DEBUG
				//Fr.m_Name = GetString(nFnName);
#endif
				Fr.m_eType = eFT_Function;
				m_CodeFragments.push_back(Fr);
				m_CurFrame.m_nCurToken = nLastTok + 1;
			}
			else
			{
				m_eToken = (EToken)nToken;
#ifdef _DEBUG
				const char* szToken = GetString(m_eToken);
#endif
				assert(m_eToken < eT_user_first);
				m_CurFrame.m_nCurToken++;
				break;
			}
		}
	}
	return 1;
}

bool CParserBin::FXGetAssignmentData(SParserFrame& Frame)
{
	bool bRes = true;

	Frame.m_nFirstToken = m_CurFrame.m_nCurToken;
	uint32 nLastToken = m_CurFrame.m_nCurToken;
	while (nLastToken <= m_CurFrame.m_nLastToken)
	{
		uint32 nTok = m_Tokens[nLastToken++];
		if (nTok == eT_quote)
		{
			while (nLastToken <= m_CurFrame.m_nLastToken)
			{
				nTok = m_Tokens[nLastToken++];
				if (nTok == eT_quote)
					break;
			}
		}
		else if (nTok == eT_semicolumn)
			break;
	}
	Frame.m_nLastToken = nLastToken - 2;
	if (m_Tokens[nLastToken] == eT_semicolumn)
		m_CurFrame.m_nCurToken = nLastToken + 1;
	else
		m_CurFrame.m_nCurToken = nLastToken;

	return bRes;
}

bool CParserBin::FXGetAssignmentData2(SParserFrame& Frame)
{
	bool bRes = true;

	Frame.m_nFirstToken = m_CurFrame.m_nCurToken;
	uint32 nLastToken = m_CurFrame.m_nCurToken;
	uint32 nTok = m_Tokens[nLastToken];
	if (nTok == eT_br_cv_1)
	{
		nLastToken++;
		while (nLastToken + 1 <= m_CurFrame.m_nLastToken)
		{
			nTok = m_Tokens[nLastToken];
			if (nTok == eT_semicolumn)
				break;
			nLastToken++;
		}
	}
	else if (nTok == eT_br_rnd_1)
	{
		nLastToken++;
		int n = 1;
		while (nLastToken + 1 <= m_CurFrame.m_nLastToken)
		{
			nTok = m_Tokens[nLastToken];
			if (nTok == eT_semicolumn || nTok == eT_br_tr_1 || nTok == eT_eq)
			{
				assert(!n);
				break;
			}
			if (nTok == eT_br_rnd_1)
				n++;
			else if (nTok == eT_br_rnd_2)
				n--;
			nLastToken++;
		}
	}
	else
	{
		while (nLastToken <= m_CurFrame.m_nLastToken)
		{
			nTok = m_Tokens[nLastToken];
			if (nTok == eT_semicolumn || nTok == eT_br_rnd_1 || nTok == eT_br_cv_1 || nTok == eT_br_tr_1)
				break;
			nLastToken++;
		}
	}

	Frame.m_nLastToken = nLastToken - 1;
	if (m_Tokens[nLastToken] == eT_semicolumn)
		m_CurFrame.m_nCurToken = nLastToken + 1;
	else
		m_CurFrame.m_nCurToken = nLastToken;

	return bRes;
}

bool CParserBin::GetAssignmentData(SParserFrame& Frame)
{
	bool bRes = true;

	Frame.m_nFirstToken = m_CurFrame.m_nCurToken;
	uint32 nLastToken = m_CurFrame.m_nCurToken;
	uint32* pTokens = &m_Tokens[0];
	uint32 nTok = pTokens[nLastToken + 1];
	if (nTok == eT_br_sq_1 || nTok == eT_br_rnd_1)
	{
		EToken eTClose = nTok == eT_br_sq_1 ? eT_br_sq_2 : eT_br_rnd_2;
		nLastToken += 2;
		while (nLastToken <= m_CurFrame.m_nLastToken)
		{
			nTok = pTokens[nLastToken];
			if (nTok == eTClose || nTok == eT_semicolumn)
			{
				if (nTok == eT_semicolumn)
					nLastToken--;
				break;
			}
			nLastToken++;
		}
	}
	Frame.m_nLastToken = nLastToken;

	nLastToken++;

	if (pTokens[nLastToken] == eT_semicolumn)
		m_CurFrame.m_nCurToken = nLastToken + 1;
	else
		m_CurFrame.m_nCurToken = nLastToken;

	return bRes;
}

bool CParserBin::GetSubData(SParserFrame& Frame, EToken eT1, EToken eT2)
{
	bool bRes = true;

	Frame.m_nFirstToken = 0;
	Frame.m_nLastToken = 0;
	uint32 nTok = m_Tokens[m_CurFrame.m_nCurToken];
	if (nTok != eT1)
		return false;
	m_CurFrame.m_nCurToken++;
	Frame.m_nFirstToken = m_CurFrame.m_nCurToken;
	uint32 nCurToken = m_CurFrame.m_nCurToken;
	int skip = 1;
	while (nCurToken <= m_CurFrame.m_nLastToken)
	{
		nTok = m_Tokens[nCurToken];
		if (nTok == eT1)
			skip++;
		else if (nTok == eT2)
		{
			if (--skip == 0)
			{
				Frame.m_nLastToken = nCurToken - 1;
				nCurToken++;
				break;
			}
		}
		nCurToken++;
	}
	if (Frame.IsEmpty())
		Frame.Reset();
	if (nCurToken <= m_CurFrame.m_nLastToken && m_Tokens[nCurToken] == eT_semicolumn)
		m_CurFrame.m_nCurToken = nCurToken + 1;
	else
		m_CurFrame.m_nCurToken = nCurToken;

	return (Frame.m_nFirstToken <= Frame.m_nLastToken);
}

ETokenStorageClass CParserBin::ParseObject(SFXTokenBin* pTokens)
{
	assert(m_CurFrame.m_nFirstToken <= m_CurFrame.m_nLastToken);

	if (m_CurFrame.m_nCurToken + 1 >= m_CurFrame.m_nLastToken)
		return eTS_invalid;
	if (m_Tokens.size() <= m_CurFrame.m_nCurToken)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Attempted out-of-bounds access in CParserBin::ParseObject");
		return eTS_invalid;
	}
	if (m_Tokens[m_CurFrame.m_nCurToken] == eT_unknown)
		return eTS_invalid;

	ETokenStorageClass nTokenStorageClass = eTS_default;
	int nRes = GetNextToken(m_nFirstToken, nTokenStorageClass);
	if (nRes < 0)
		return eTS_invalid;

	m_Name.Reset();
	m_Assign.Reset();
	m_Value.Reset();
	m_Data.Reset();
	m_Annotations.Reset();

	SFXTokenBin* pT = pTokens;
	while (pTokens->id != 0)
	{
		if (pTokens->id == m_eToken)
			break;
		pTokens++;
	}
	if (pTokens->id == 0)
	{
		pTokens = pT;
		Warning("FXBin parser found token '%s' which was not one of the list (Skipping).\n", GetString(m_eToken));
		while (pTokens->id != 0)
		{
			Warning("    %s\n", GetString(pTokens->id));
			pTokens++;
		}
		assert(0);
#ifdef _DEBUG
		TArray<char> Text;
		SParserFrame Fr;
		Fr.m_nFirstToken = max(m_CurFrame.m_nFirstToken, m_CurFrame.m_nCurToken - 5);
		Fr.m_nLastToken = m_CurFrame.m_nLastToken;
		ConvertToAscii(&m_Tokens[Fr.m_nFirstToken], Fr.m_nLastToken - Fr.m_nFirstToken + 1, m_TokenTable, Text);
#endif
		return eTS_invalid;
	}

	bool bAnnot = false;
	if (m_Tokens[m_CurFrame.m_nCurToken] == eT_br_tr_1)
	{
		GetSubData(m_Annotations, eT_br_tr_1, eT_br_tr_2);
		bAnnot = true;
	}

	GetAssignmentData(m_Name);
	if (m_Tokens[m_CurFrame.m_nCurToken] == eT_colon)
	{
		m_CurFrame.m_nCurToken++;                       // skip the ':'
		GetAssignmentData(m_Assign);
	}

	if (!bAnnot)
		GetSubData(m_Annotations, eT_br_tr_1, eT_br_tr_2);
	if (m_CurFrame.m_nCurToken <= m_CurFrame.m_nLastToken)
	{
		if (m_Tokens[m_CurFrame.m_nCurToken] == eT_eq)
		{
			m_CurFrame.m_nCurToken++;                       // skip the '='
			FXGetAssignmentData2(m_Value);
		}

		GetSubData(m_Data, eT_br_cv_1, eT_br_cv_2);
	}

	if (m_CurFrame.m_nCurToken <= m_CurFrame.m_nLastToken && m_Tokens[m_CurFrame.m_nCurToken] == eT_semicolumn)
		m_CurFrame.m_nCurToken++;

	return nTokenStorageClass;
}

ETokenStorageClass CParserBin::ParseObject(SFXTokenBin* pTokens, int& nIndex)
{
	assert(m_CurFrame.m_nFirstToken <= m_CurFrame.m_nLastToken);

	if (m_Tokens[m_CurFrame.m_nCurToken] == eT_unknown)
		return eTS_invalid;
	if (m_CurFrame.m_nCurToken + 1 >= m_CurFrame.m_nLastToken)
		return eTS_invalid;

	ETokenStorageClass nTokenStorageClass = eTS_default;
	int nRes = GetNextToken(m_nFirstToken, nTokenStorageClass);
	if (nRes < 0)
		return eTS_invalid;

	m_Name.Reset();
	m_Assign.Reset();
	m_Value.Reset();
	m_Data.Reset();
	m_Annotations.Reset();

	SFXTokenBin* pT = pTokens;
	while (pTokens->id != 0)
	{
		if (pTokens->id == m_eToken)
			break;
		pTokens++;
	}
	if (pTokens->id == 0)
	{
		pTokens = pT;
		Warning("Warning: FXBin parser found token '%s' which was not one of the list (Skipping).\n", GetString(m_eToken));
		while (pTokens->id != 0)
		{
			Warning("    %s\n", GetString(pTokens->id));
			pTokens++;
		}
		assert(0);
#ifdef _DEBUG
		TArray<char> Text;
		SParserFrame Fr;
		Fr.m_nFirstToken = max(m_CurFrame.m_nFirstToken, m_CurFrame.m_nCurToken - 5);
		Fr.m_nLastToken = m_CurFrame.m_nLastToken;
		ConvertToAscii(&m_Tokens[Fr.m_nFirstToken], Fr.m_nLastToken - Fr.m_nFirstToken + 1, m_TokenTable, Text);
#endif
		return eTS_invalid;
	}
	if (m_Tokens[m_CurFrame.m_nCurToken] == eT_br_sq_1)
	{
		m_CurFrame.m_nCurToken++;
		nIndex = GetInt(m_Tokens[m_CurFrame.m_nCurToken++]);
	}
	if (m_Tokens[m_CurFrame.m_nCurToken] == eT_sing_quote)
		GetSubData(m_Name, eT_sing_quote, eT_sing_quote);
	else if (m_Tokens[m_CurFrame.m_nCurToken] != eT_eq)
		m_Name.m_nFirstToken = m_Name.m_nLastToken = m_CurFrame.m_nCurToken++;

	if (m_Tokens[m_CurFrame.m_nCurToken] == eT_eq)
	{
		m_CurFrame.m_nCurToken++;
		FXGetAssignmentData(m_Data);
	}
	else
		GetSubData(m_Data, eT_br_cv_1, eT_br_cv_2);

	if (m_Tokens[m_CurFrame.m_nCurToken] == eT_semicolumn || m_Tokens[m_CurFrame.m_nCurToken] == eT_quote)
		m_CurFrame.m_nCurToken++;

	return nTokenStorageClass;
}

bool CParserBin::JumpSemicolumn(uint32& nStart, uint32 nEnd)
{
	while (nStart <= nEnd)
	{
		uint32 nTok = m_Tokens[nStart++];
		if (nTok == eT_semicolumn)
			return true;
	}
	return false;
}

SParserFrame CParserBin::BeginFrame(SParserFrame& Frame)
{
	SParserFrame RetFrame = m_CurFrame;
	m_CurFrame = Frame;
	m_eToken = eT_unknown;
	m_CurFrame.m_nCurToken = Frame.m_nFirstToken;

	return RetFrame;
}

void CParserBin::EndFrame(SParserFrame& Frame)
{
	m_CurFrame = Frame;
}

void SFXParam::PostLoad(CParserBin& Parser, SParserFrame& Name, SParserFrame& Annotations, SParserFrame& Values, SParserFrame& Assign)
{
	m_Annotations = Parser.GetNameString(Annotations);
	if (!Values.IsEmpty())
	{
		if (Parser.GetToken(Values) == eT_br_cv_1)
		{
			Values.m_nFirstToken++;
			int32 nFind = Parser.FindToken(Values.m_nFirstToken, Values.m_nLastToken, eT_br_cv_2);
			assert(nFind > 0 && Values.m_nLastToken == nFind);
			if (nFind > 0)
				Values.m_nLastToken--;
		}
		m_Values = Parser.GetString(Values);
	}
	m_Semantic = Parser.GetNameString(Assign);
	m_Name = Parser.GetString(Name);

	m_nFlags = 0;
	if (m_nComps == 1 && m_nParameters <= 1)
		m_nFlags |= PF_SCALAR;
	if (m_eType == eType_INT)
		m_nFlags |= PF_INTEGER;
	else if (m_eType == eType_BOOL)
		m_nFlags |= PF_BOOL;
	else if (m_eType != eType_FLOAT && m_eType != eType_HALF)
	{
		assert(0);
	}

	if (Annotations.IsEmpty())
		return;

	uint32 nCur = Annotations.m_nFirstToken;
	uint32 nLast = Annotations.m_nLastToken;
	uint32* pTokens = &Parser.m_Tokens[0];
	while (nCur <= nLast)
	{
		uint32 nTok = pTokens[nCur++];
		if (nTok == eT_register || nTok == eT_psregister || nTok == eT_vsregister || nTok == eT_gsregister || nTok == eT_dsregister || nTok == eT_hsregister || nTok == eT_csregister)
		{
			m_nFlags |= PF_CUSTOM_BINDED;
			{
				uint32 nTok2 = pTokens[nCur++];
				if (nTok2 != eT_eq)
				{
					assert(0);
				}
				else
				{
					nTok2 = pTokens[nCur++];
					const char* szReg = Parser.GetString(nTok2);
					assert(szReg[0] == 'c');
					if (nTok == eT_register)
					{
						m_nRegister[eHWSC_Vertex] = atoi(&szReg[1]);
						m_nRegister[eHWSC_Pixel] = m_nRegister[eHWSC_Vertex];
						if (CParserBin::PlatformSupportsGeometryShaders())
							m_nRegister[eHWSC_Geometry] = m_nRegister[eHWSC_Vertex];
						if (CParserBin::PlatformSupportsDomainShaders())
							m_nRegister[eHWSC_Domain] = m_nRegister[eHWSC_Vertex];
						if (CParserBin::PlatformSupportsHullShaders())
							m_nRegister[eHWSC_Hull] = m_nRegister[eHWSC_Vertex];
						if (CParserBin::PlatformSupportsComputeShaders())
							m_nRegister[eHWSC_Compute] = m_nRegister[eHWSC_Vertex];
					}
					else if (nTok == eT_vsregister)
						m_nRegister[eHWSC_Vertex] = atoi(&szReg[1]);
					else if (nTok == eT_psregister)
						m_nRegister[eHWSC_Pixel] = atoi(&szReg[1]);
					else if (CParserBin::PlatformSupportsGeometryShaders() && nTok == eT_gsregister)
					{
						m_nRegister[eHWSC_Geometry] = atoi(&szReg[1]);
					}
					else if (CParserBin::PlatformSupportsDomainShaders() && nTok == eT_dsregister)
					{
						m_nRegister[eHWSC_Domain] = atoi(&szReg[1]);
						m_nRegister[eHWSC_Vertex] = m_nRegister[eHWSC_Domain];
					}
					else if (CParserBin::PlatformSupportsHullShaders() && nTok == eT_hsregister)
					{
						m_nRegister[eHWSC_Hull] = atoi(&szReg[1]);
						m_nRegister[eHWSC_Vertex] = m_nRegister[eHWSC_Hull];
					}
					else if (CParserBin::PlatformSupportsComputeShaders() && nTok == eT_csregister)
					{
						m_nRegister[eHWSC_Compute] = atoi(&szReg[1]);
						m_nRegister[eHWSC_Vertex] = m_nRegister[eHWSC_Compute];
					}
				}
			}
		}
		else if (nTok == eT_Position)
			m_nFlags |= PF_POSITION;
		else if (nTok == eT_Allways)
			m_nFlags |= PF_ALWAYS;
		else
		{
			if (nTok == eT_string)
			{
				uint32 nTokName = pTokens[nCur++];
				if (nTokName == eT_UIWidget || nTokName == eT_UIWidget0)
				{
					uint32 nTok0 = pTokens[nCur++];
					uint32 nTok1 = pTokens[nCur++];
					if (nTok0 == eT_eq && nTok1 == eT_quote)
					{
						nTok = pTokens[nCur++];
						if (nTok == eT_color)
							m_nFlags |= PF_TWEAKABLE_MASK;
						else
							m_nFlags |= PF_TWEAKABLE_0;
					}
				}
				else if (nTokName == eT_UIWidget1)
					m_nFlags |= PF_TWEAKABLE_1;
				else if (nTokName == eT_UIWidget2)
					m_nFlags |= PF_TWEAKABLE_2;
				else if (nTokName == eT_UIWidget3)
					m_nFlags |= PF_TWEAKABLE_3;
			}
		}
		if (!Parser.JumpSemicolumn(nCur, nLast))
			return;
	}
}

void SFXSampler::PostLoad(CParserBin& Parser, SParserFrame& Name, SParserFrame& Annotations, SParserFrame& Values, SParserFrame& Assign)
{
	m_Annotations = Parser.GetNameString(Annotations);
	if (!Values.IsEmpty())
	{
		if (Parser.GetToken(Values) == eT_br_cv_1)
		{
			Values.m_nFirstToken++;
			int32 nFind = Parser.FindToken(Values.m_nFirstToken, Values.m_nLastToken, eT_br_cv_2);
			assert(nFind > 0 && Values.m_nLastToken == nFind);
			if (nFind > 0)
				Values.m_nLastToken--;
		}
		if (Parser.GetToken(Values) == eT_quote)
		{
			Values.m_nFirstToken++;
			int32 nFind = Parser.FindToken(Values.m_nFirstToken, Values.m_nLastToken, eT_quote);
			assert(nFind > 0 && Values.m_nLastToken == nFind);
			if (nFind > 0)
				Values.m_nLastToken--;
		}
		m_Values = Parser.GetString(Values);
	}
	m_Semantic = Parser.GetNameString(Assign);
	m_Name = Parser.GetString(Name);
	m_nFlags = 0;

	if (!Assign.IsEmpty())
	{
		uint32 nCur = Assign.m_nFirstToken;
		uint32 nLast = Assign.m_nLastToken;
		uint32* pTokens = &Parser.m_Tokens[0];
		while (nCur <= nLast)
		{
			uint32 nTok = pTokens[nCur++];
			if (nTok == eT_register)
			{
				m_nFlags |= PF_CUSTOM_BINDED;
				{
					uint32 nTok2 = pTokens[nCur++];
					if (nTok2 != eT_br_rnd_1)
					{
						assert(0);
					}
					else
					{
						nTok2 = pTokens[nCur++];
						const char* szReg = Parser.GetString(nTok2);
						while (szReg[0] == 's' || szReg[0] == 't')
							++szReg;
						assert(isdigit(szReg[0]));
						m_nRegister[eHWSC_Vertex] =
						  m_nRegister[eHWSC_Pixel] =
						    m_nRegister[eHWSC_Geometry] =
						      m_nRegister[eHWSC_Domain] =
						        m_nRegister[eHWSC_Hull] =
						          m_nRegister[eHWSC_Compute] = atoi(&szReg[0]);

						uint32 nTok2 = pTokens[nCur++];
						if (nTok2 != eT_br_rnd_2)
						{
							assert(0);
						}
					}
				}
			}
		}
	}

	if (!Annotations.IsEmpty())
	{
		uint32 nCur = Annotations.m_nFirstToken;
		uint32 nLast = Annotations.m_nLastToken;
		uint32* pTokens = &Parser.m_Tokens[0];
		while (nCur <= nLast)
		{
			uint32 nTok = pTokens[nCur++];
			if (nTok == eT_psslot || nTok == eT_vsslot || nTok == eT_gsslot || nTok == eT_dsslot || nTok == eT_hsslot || nTok == eT_csslot || nTok == eT_slot)
			{
				m_nFlags |= PF_CUSTOM_BINDED;
				{
					uint32 nTok2 = pTokens[nCur++];
					if (nTok2 != eT_eq)
					{
						assert(0);
					}
					else
					{
						nTok2 = pTokens[nCur++];
						const char* szReg = Parser.GetString(nTok2);
						assert(isdigit(szReg[0]));
						if (nTok == eT_vsslot || nTok == eT_slot)
							m_nRegister[eHWSC_Vertex] = atoi(&szReg[0]);
						if (nTok == eT_psslot || nTok == eT_slot)
							m_nRegister[eHWSC_Pixel] = atoi(&szReg[0]);
						if (CParserBin::PlatformSupportsGeometryShaders() && (nTok == eT_gsslot || nTok == eT_slot))
							m_nRegister[eHWSC_Geometry] = atoi(&szReg[0]);
						if (CParserBin::PlatformSupportsDomainShaders() && (nTok == eT_dsslot || nTok == eT_slot))
							m_nRegister[eHWSC_Domain] = atoi(&szReg[0]);
						if (CParserBin::PlatformSupportsHullShaders() && (nTok == eT_hsslot || nTok == eT_slot))
							m_nRegister[eHWSC_Hull] = atoi(&szReg[0]);
						if (CParserBin::PlatformSupportsComputeShaders() && (nTok == eT_csslot || nTok == eT_slot))
							m_nRegister[eHWSC_Compute] = atoi(&szReg[0]);
					}
				}
			}
			if (!Parser.JumpSemicolumn(nCur, nLast))
				break;
		}
	}
}

void SFXTexture::PostLoad(CParserBin& Parser, SParserFrame& Name, SParserFrame& Annotations, SParserFrame& Values, SParserFrame& Assign)
{
	m_Annotations = Parser.GetNameString(Annotations);
	if (!Values.IsEmpty())
	{
		if (Parser.GetToken(Values) == eT_br_cv_1)
		{
			Values.m_nFirstToken++;
			int32 nFind = Parser.FindToken(Values.m_nFirstToken, Values.m_nLastToken, eT_br_cv_2);
			assert(nFind > 0 && Values.m_nLastToken == nFind);
			if (nFind > 0)
				Values.m_nLastToken--;
		}
		if (Parser.GetToken(Values) == eT_quote)
		{
			Values.m_nFirstToken++;
			int32 nFind = Parser.FindToken(Values.m_nFirstToken, Values.m_nLastToken, eT_quote);
			assert(nFind > 0 && Values.m_nLastToken == nFind);
			if (nFind > 0)
				Values.m_nLastToken--;
		}
		m_Values = Parser.GetString(Values);
	}
	m_Semantic = Parser.GetNameString(Assign);
	m_Name = Parser.GetString(Name);
	m_nFlags = 0;

	if (!Assign.IsEmpty())
	{
		uint32 nCur = Assign.m_nFirstToken;
		uint32 nLast = Assign.m_nLastToken;
		uint32* pTokens = &Parser.m_Tokens[0];
		while (nCur <= nLast)
		{
			uint32 nTok = pTokens[nCur++];
			if (nTok == eT_register)
			{
				m_nFlags |= PF_CUSTOM_BINDED;
				{
					uint32 nTok2 = pTokens[nCur++];
					if (nTok2 != eT_br_rnd_1)
					{
						assert(0);
					}
					else
					{
						nTok2 = pTokens[nCur++];
						const char* szReg = Parser.GetString(nTok2);
						while (szReg[0] == 's' || szReg[0] == 't')
							++szReg;
						assert(isdigit(szReg[0]));
						m_nRegister[eHWSC_Vertex] =
						  m_nRegister[eHWSC_Pixel] =
						    m_nRegister[eHWSC_Geometry] =
						      m_nRegister[eHWSC_Domain] =
						        m_nRegister[eHWSC_Hull] =
						          m_nRegister[eHWSC_Compute] = atoi(&szReg[0]);

						uint32 nTok2 = pTokens[nCur++];
						if (nTok2 != eT_br_rnd_2)
						{
							assert(0);
						}
					}
				}
			}
		}
	}

	if (!Annotations.IsEmpty())
	{
		uint32 nCur = Annotations.m_nFirstToken;
		uint32 nLast = Annotations.m_nLastToken;
		uint32* pTokens = &Parser.m_Tokens[0];
		while (nCur <= nLast)
		{
			uint32 nTok = pTokens[nCur++];
			if (nTok == eT_psslot || nTok == eT_vsslot || nTok == eT_gsslot || nTok == eT_dsslot || nTok == eT_hsslot || nTok == eT_csslot || nTok == eT_slot)
			{
				m_nFlags |= PF_CUSTOM_BINDED;
				{
					uint32 nTok2 = pTokens[nCur++];
					if (nTok2 != eT_eq)
					{
						assert(0);
					}
					else
					{
						nTok2 = pTokens[nCur++];
						const char* szReg = Parser.GetString(nTok2);
						assert(isdigit(szReg[0]));
						if (nTok == eT_vsslot || nTok == eT_slot)
							m_nRegister[eHWSC_Vertex] = atoi(&szReg[0]);
						if (nTok == eT_psslot || nTok == eT_slot)
							m_nRegister[eHWSC_Pixel] = atoi(&szReg[0]);
						if (CParserBin::PlatformSupportsGeometryShaders() && (nTok == eT_gsslot || nTok == eT_slot))
							m_nRegister[eHWSC_Geometry] = atoi(&szReg[0]);
						if (CParserBin::PlatformSupportsDomainShaders() && (nTok == eT_dsslot || nTok == eT_slot))
							m_nRegister[eHWSC_Domain] = atoi(&szReg[0]);
						if (CParserBin::PlatformSupportsHullShaders() && (nTok == eT_hsslot || nTok == eT_slot))
							m_nRegister[eHWSC_Hull] = atoi(&szReg[0]);
						if (CParserBin::PlatformSupportsComputeShaders() && (nTok == eT_csslot || nTok == eT_slot))
							m_nRegister[eHWSC_Compute] = atoi(&szReg[0]);
					}
				}
			}
			else
				switch (nTok)
				{
				case eT_float:
				case eT_float2:
				case eT_float3:
				case eT_float4:
				case eT_uint:
				case eT_uint2:
				case eT_uint4:
				case eT_int:
				case eT_int2:
				case eT_int4:
					m_Type = nTok;
					break;
				default:
					break;
				}

			if (!Parser.JumpSemicolumn(nCur, nLast))
				break;
		}
	}
}

byte CParserBin::GetCompareFunc(EToken eT)
{
	switch (eT)
	{
	case eT_None:
	case eT_Disable:
		return eCF_Disable;
	case eT_Never:
		return eCF_Never;
	case eT_Less:
		return eCF_Less;
	case eT_Equal:
		return eCF_Equal;
	case eT_LEqual:
	case eT_LessEqual:
		return eCF_LEqual;
	case eT_Greater:
		return eCF_Greater;
	case eT_NotEqual:
		return eCF_NotEqual;
	case eT_GEqual:
	case eT_GreaterEqual:
		return eCF_NotEqual;
	case eT_Always:
		return eCF_Always;
	default:
		Warning("unknown CompareFunc parameter '%s' (Skipping)\n", GetString(eT));
	}
	return eCF_Less;
}

int CParserBin::GetSrcBlend(EToken eT)
{
	switch (eT)
	{
	case eT_ONE:
		return GS_BLSRC_ONE;
	case eT_ZERO:
		return GS_BLSRC_ZERO;
	case eT_DST_COLOR:
	case eT_DestColor:
		return GS_BLSRC_DSTCOL;
	case eT_ONE_MINUS_DST_COLOR:
	case eT_InvDestColor:
		return GS_BLSRC_ONEMINUSDSTCOL;
	case eT_SRC_ALPHA:
	case eT_SrcAlpha:
		return GS_BLSRC_SRCALPHA;
	case eT_ONE_MINUS_SRC_ALPHA:
	case eT_InvSrcAlpha:
		return GS_BLSRC_ONEMINUSSRCALPHA;
	case eT_DST_ALPHA:
	case eT_DestAlpha:
		return GS_BLSRC_DSTALPHA;
	case eT_ONE_MINUS_DST_ALPHA:
	case eT_InvDestAlpha:
		return GS_BLSRC_ONEMINUSDSTALPHA;
	case eT_SRC_ALPHA_SATURATE:
		return GS_BLSRC_ALPHASATURATE;

	default:
		{
			Warning("unknown SrcBlend parameter '%s' (Skipping)\n", GetString(eT));
			assert(0);
		}
	}
	return GS_BLSRC_ONE;
}

int CParserBin::GetDstBlend(EToken eT)
{
	switch (eT)
	{
	case eT_ONE:
		return GS_BLDST_ONE;
	case eT_ZERO:
		return GS_BLDST_ZERO;
	case eT_SRC_COLOR:
	case eT_SrcColor:
		return GS_BLDST_SRCCOL;
	case eT_ONE_MINUS_SRC_COLOR:
	case eT_InvSrcColor:
		return GS_BLDST_ONEMINUSSRCCOL;
	case eT_SRC_ALPHA:
	case eT_SrcAlpha:
		return GS_BLDST_SRCALPHA;
	case eT_ONE_MINUS_SRC_ALPHA:
	case eT_InvSrcAlpha:
		return GS_BLDST_ONEMINUSSRCALPHA;
	case eT_DST_ALPHA:
	case eT_DestAlpha:
		return GS_BLDST_DSTALPHA;
	case eT_ONE_MINUS_DST_ALPHA:
	case eT_InvDestAlpha:
		return GS_BLDST_ONEMINUSDSTALPHA;

	default:
		{
			Warning("unknown DstBlend parameter '%s' (Skipping)\n", GetString(eT));
			assert(0);
		}
	}
	return GS_BLDST_ONE;

}

void CParserBin::SetupFeatureDefines()
{
	// globally remove all features here and selectively re-enable them based on project defines and platform validation
	RemoveMacro(CParserBin::GetCRC32("FEATURE_MESH_TESSELLATION"), m_StaticMacros);
	RemoveMacro(CParserBin::GetCRC32("FEATURE_SELF_SHADOWS"), m_StaticMacros);
	RemoveMacro(CParserBin::GetCRC32("FEATURE_PARTICLES_TESSELLATION"), m_StaticMacros);
	RemoveMacro(CParserBin::GetCRC32("FEATURE_GEOMETRY_SHADERS"), m_StaticMacros);
	RemoveMacro(CParserBin::GetCRC32("FEATURE_SVO_GI"), m_StaticMacros);

	uint32 nEnable[1] = { eT_1 };
#if defined(MESH_TESSELLATION)
	if (m_nPlatform & (SF_D3D11 | SF_DURANGO | SF_ORBIS | SF_GL4 | SF_VULKAN))
	{
		AddMacro(CParserBin::GetCRC32("FEATURE_MESH_TESSELLATION"), nEnable, 1, 0, m_StaticMacros);
	}
#endif

#if defined(FEATURE_DEFERRED_SHADING_SELF_SHADOWS)
	if (m_nPlatform & (SF_D3D11 | SF_GL4 | SF_GLES3 | SF_VULKAN))
	{
		AddMacro(CParserBin::GetCRC32("FEATURE_SELF_SHADOWS"), nEnable, 1, 0, m_StaticMacros);
	}
#endif

#if defined(PARTICLES_TESSELLATION)
	if (m_nPlatform & (SF_D3D11 | SF_DURANGO | SF_ORBIS |  SF_GL4 | SF_VULKAN))
	{
		AddMacro(CParserBin::GetCRC32("FEATURE_PARTICLES_TESSELLATION"), nEnable, 1, 0, m_StaticMacros);
	}
#endif

	if (m_nPlatform & (SF_D3D11 | SF_ORBIS | SF_DURANGO | SF_GL4 | SF_VULKAN))
	{
		AddMacro(CParserBin::GetCRC32("FEATURE_GEOMETRY_SHADERS"), nEnable, 1, 0, m_StaticMacros);
	}

#if defined(FEATURE_SVO_GI)
	if (m_nPlatform & (SF_D3D11 | SF_ORBIS | SF_DURANGO | SF_VULKAN))
	{
		AddMacro(CParserBin::GetCRC32("FEATURE_SVO_GI"), nEnable, 1, 0, m_StaticMacros);
	}
#endif
}
