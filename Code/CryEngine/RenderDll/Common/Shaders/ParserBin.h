// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   ParserBin.h : Script parser declarations.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#ifndef PARSERBIN_H
#define PARSERBIN_H

#include "ShaderCache.h"
#include "ShaderAllocator.h"

extern TArray<bool> sfxIFDef;
extern TArray<bool> sfxIFIgnore;

typedef TArray<uint32> ShaderTokensVec;

// key tokens
enum EToken
{
	eT_unknown      = 0,
	eT_include      = 1,
	eT_define       = 2,
	eT_define_2     = 3,
	eT_undefine     = 4,

	eT_fetchinst    = 5,
	eT_if           = 6,
	eT_ifdef        = 7,
	eT_ifndef       = 8,
	eT_if_2         = 9,
	eT_ifdef_2      = 10,
	eT_ifndef_2     = 11,
	eT_elif         = 12,
	eT_elif_2       = 13,

	eT_endif        = 14,
	eT_else         = 15,
	eT_or           = 16,
	eT_and          = 17,
	eT_warning      = 18,
	eT_register_env = 19,
	eT_ifcvar       = 20,
	eT_ifncvar      = 21,
	eT_elifcvar     = 22,
	eT_skip         = 23,
	eT_skip_1       = 24,
	eT_skip_2       = 25,

	eT_br_rnd_1     = 26,
	eT_br_rnd_2     = 27,
	eT_br_sq_1      = 28,
	eT_br_sq_2      = 29,
	eT_br_cv_1      = 30,
	eT_br_cv_2      = 31,
	eT_br_tr_1      = 32,
	eT_br_tr_2      = 33,
	eT_comma        = 34,
	eT_dot          = 35,
	eT_colon        = 36,
	eT_semicolumn   = 37,
	eT_excl         = 38, // !
	eT_quote        = 39,
	eT_sing_quote   = 40,

	eT_question     = 41,
	eT_eq           = 42,
	eT_plus         = 43,
	eT_minus        = 44,
	eT_div          = 45,
	eT_mul          = 46,
	eT_dot_math     = 47,
	eT_mul_math     = 48,
	eT_sqrt_math    = 49,
	eT_exp_math     = 50,
	eT_log_math     = 51,
	eT_log2_math    = 52,
	eT_sin_math     = 53,
	eT_cos_math     = 54,
	eT_sincos_math  = 55,
	eT_floor_math   = 56,
	eT_ceil_math    = 57,
	eT_frac_math    = 58,
	eT_lerp_math    = 59,
	eT_abs_math     = 60,
	eT_clamp_math   = 61,
	eT_min_math     = 62,
	eT_max_math     = 63,
	eT_length_math  = 64,

	eT_tex2D,
	eT_tex2Dproj,
	eT_tex3D,
	eT_texCUBE,
	eT_SamplerState,
	eT_SamplerComparisonState,
	eT_sampler_state,
	eT_Texture2D,
	eT_RWTexture2D,
	eT_RWTexture2DArray,
	eT_Texture2DArray,
	eT_Texture2DMS,
	eT_TextureCube,
	eT_TextureCubeArray,
	eT_Texture3D,
	eT_RWTexture3D,

	eT_snorm,
	eT_unorm,
	eT_float,
	eT_float2,
	eT_float3,
	eT_float4,
	eT_float2x4,
	eT_float3x4,
	eT_float4x4,
	eT_float3x3,
	eT_half,
	eT_half2,
	eT_half3,
	eT_half4,
	eT_half2x4,
	eT_half3x4,
	eT_half4x4,
	eT_half3x3,
	eT_bool,
	eT_int,
	eT_int2,
	eT_int3,
	eT_int4,
	eT_uint,
	eT_uint2,
	eT_uint3,
	eT_uint4,
	eT_min16float,
	eT_min16float2,
	eT_min16float3,
	eT_min16float4,
	eT_min16float4x4,
	eT_min16float3x4,
	eT_min16float2x4,
	eT_min16float3x3,
	eT_min10float,
	eT_min10float2,
	eT_min10float3,
	eT_min10float4,
	eT_min10float4x4,
	eT_min10float3x4,
	eT_min10float2x4,
	eT_min10float3x3,
	eT_min16int,
	eT_min16int2,
	eT_min16int3,
	eT_min16int4,
	eT_min12int,
	eT_min12int2,
	eT_min12int3,
	eT_min12int4,
	eT_min16uint,
	eT_min16uint2,
	eT_min16uint3,
	eT_min16uint4,

	eT_sampler1D,
	eT_sampler2D,
	eT_sampler3D,
	eT_samplerCUBE,
	eT_const,

	eT_inout,

	eT_struct,
	eT_sampler,
	eT_TEXCOORDN,
	eT_TEXCOORD0,
	eT_TEXCOORD1,
	eT_TEXCOORD2,
	eT_TEXCOORD3,
	eT_TEXCOORD4,
	eT_TEXCOORD5,
	eT_TEXCOORD6,
	eT_TEXCOORD7,
	eT_TEXCOORD8,
	eT_TEXCOORD9,
	eT_TEXCOORD10,
	eT_TEXCOORD11,
	eT_TEXCOORD12,
	eT_TEXCOORD13,
	eT_TEXCOORD14,
	eT_TEXCOORD15,
	eT_TEXCOORD16,
	eT_TEXCOORD17,
	eT_TEXCOORD18,
	eT_TEXCOORD19,
	eT_TEXCOORD20,
	eT_TEXCOORD21,
	eT_TEXCOORD22,
	eT_TEXCOORD23,
	eT_TEXCOORD24,
	eT_TEXCOORD25,
	eT_TEXCOORD26,
	eT_TEXCOORD27,
	eT_TEXCOORD28,
	eT_TEXCOORD29,
	eT_TEXCOORD30,
	eT_TEXCOORD31,
	eT_TEXCOORDN_centroid,
	eT_TEXCOORD0_centroid,
	eT_TEXCOORD1_centroid,
	eT_TEXCOORD2_centroid,
	eT_TEXCOORD3_centroid,
	eT_TEXCOORD4_centroid,
	eT_TEXCOORD5_centroid,
	eT_TEXCOORD6_centroid,
	eT_TEXCOORD7_centroid,
	eT_TEXCOORD8_centroid,
	eT_TEXCOORD9_centroid,
	eT_TEXCOORD10_centroid,
	eT_TEXCOORD11_centroid,
	eT_TEXCOORD12_centroid,
	eT_TEXCOORD13_centroid,
	eT_TEXCOORD14_centroid,
	eT_TEXCOORD15_centroid,
	eT_TEXCOORD16_centroid,
	eT_TEXCOORD17_centroid,
	eT_TEXCOORD18_centroid,
	eT_TEXCOORD19_centroid,
	eT_TEXCOORD20_centroid,
	eT_TEXCOORD21_centroid,
	eT_TEXCOORD22_centroid,
	eT_TEXCOORD23_centroid,
	eT_TEXCOORD24_centroid,
	eT_TEXCOORD25_centroid,
	eT_TEXCOORD26_centroid,
	eT_TEXCOORD27_centroid,
	eT_TEXCOORD28_centroid,
	eT_TEXCOORD29_centroid,
	eT_TEXCOORD30_centroid,
	eT_TEXCOORD31_centroid,
	eT_COLOR0,
	eT_static,
	eT_shared,
	eT_groupshared,
	eT_packoffset,
	eT_register,
	eT_return,
	eT_vsregister,
	eT_psregister,
	eT_gsregister,
	eT_dsregister,
	eT_hsregister,
	eT_csregister,

	eT_slot,
	eT_vsslot,
	eT_psslot,
	eT_gsslot,
	eT_dsslot,
	eT_hsslot,
	eT_csslot,

	eT_StructuredBuffer,
	eT_RWStructuredBuffer,
	eT_ByteAddressBuffer,
	eT_RWByteAddressBuffer,
	eT_Buffer,
	eT_RWBuffer,

	eT_color,
	eT_Position,
	eT_Allways,

	eT_STANDARDSGLOBAL,

	eT_technique,
	eT_string,
	eT_UIName,
	eT_UIDescription,
	eT_UIWidget,
	eT_UIWidget0,
	eT_UIWidget1,
	eT_UIWidget2,
	eT_UIWidget3,

	eT_Texture,
	eT_Filter,
	eT_MinFilter,
	eT_MagFilter,
	eT_MipFilter,
	eT_AddressU,
	eT_AddressV,
	eT_AddressW,
	eT_BorderColor,
	eT_sRGBLookup,

	eT_LINEAR,
	eT_POINT,
	eT_NONE,
	eT_ANISOTROPIC,
	eT_MIN_MAG_MIP_POINT,
	eT_MIN_MAG_MIP_LINEAR,
	eT_MIN_MAG_LINEAR_MIP_POINT,
	eT_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
	eT_MINIMUM_MIN_MAG_MIP_LINEAR,
	eT_MAXIMUM_MIN_MAG_MIP_LINEAR,

	eT_Clamp,
	eT_Border,
	eT_Wrap,
	eT_Mirror,

	eT_Script,
	eT_comment,
	eT_asm,

	eT_RenderOrder,
	eT_ProcessOrder,
	eT_RenderCamera,
	eT_RenderType,
	eT_RenderFilter,
	eT_RenderColorTarget1,
	eT_RenderDepthStencilTarget,
	eT_ClearSetColor,
	eT_ClearSetDepth,
	eT_ClearTarget,
	eT_RenderTarget_IDPool,
	eT_RenderTarget_UpdateType,
	eT_RenderTarget_Width,
	eT_RenderTarget_Height,
	eT_GenerateMips,

	eT_PreProcess,
	eT_PostProcess,
	eT_PreDraw,

	eT_WaterReflection,
	eT_Panorama,

	eT_WaterPlaneReflected,
	eT_PlaneReflected,
	eT_Current,

	eT_CurObject,
	eT_CurScene,
	eT_RecursiveScene,
	eT_CopyScene,

	eT_Refractive,
	eT_ForceRefractionUpdate,
	eT_Heat,

	eT_DepthBuffer,
	eT_DepthBufferTemp,
	eT_DepthBufferOrig,

	eT_$ScreenSize,
	eT_WaterReflect,
	eT_FogColor,

	eT_Color,
	eT_Depth,

	eT_$RT_2D,
	eT_$RT_CM,
	eT_$RT_Cube,

	eT_pass,
	eT_CustomRE,
	eT_Style,

	eT_VertexShader,
	eT_PixelShader,
	eT_GeometryShader,
	eT_HullShader,
	eT_DomainShader,
	eT_ComputeShader,
	eT_ZEnable,
	eT_ZWriteEnable,
	eT_CullMode,
	eT_SrcBlend,
	eT_DestBlend,
	eT_AlphaBlendEnable,
	eT_AlphaFunc,
	eT_AlphaRef,
	eT_ZFunc,
	eT_ColorWriteEnable,
	eT_IgnoreMaterialState,

	eT_None,
	eT_Disable,
	eT_CCW,
	eT_CW,
	eT_Back,
	eT_Front,

	eT_Never,
	eT_Less,
	eT_Equal,
	eT_LEqual,
	eT_LessEqual,
	eT_NotEqual,
	eT_GEqual,
	eT_GreaterEqual,
	eT_Greater,
	eT_Always,

	eT_RED,
	eT_GREEN,
	eT_BLUE,
	eT_ALPHA,

	eT_ONE,
	eT_ZERO,
	eT_SRC_COLOR,
	eT_SrcColor,
	eT_ONE_MINUS_SRC_COLOR,
	eT_InvSrcColor,
	eT_SRC_ALPHA,
	eT_SrcAlpha,
	eT_ONE_MINUS_SRC_ALPHA,
	eT_InvSrcAlpha,
	eT_DST_ALPHA,
	eT_DestAlpha,
	eT_ONE_MINUS_DST_ALPHA,
	eT_InvDestAlpha,
	eT_DST_COLOR,
	eT_DestColor,
	eT_ONE_MINUS_DST_COLOR,
	eT_InvDestColor,
	eT_SRC_ALPHA_SATURATE,

	eT_NULL,

	eT_cbuffer,
	eT_PER_BATCH,
	eT_PER_INSTANCE,
	eT_PER_MATERIAL,
	eT_SKIN_DATA,
	eT_INSTANCE_DATA,

	eT_ShaderType,
	eT_ShaderDrawType,
	eT_PreprType,
	eT_Public,
	eT_NoPreview,
	eT_LocalConstants,
	eT_Cull,
	eT_SupportsAttrInstancing,
	eT_SupportsConstInstancing,
	eT_SupportsDeferredShading,
	eT_SupportsFullDeferredShading,
	eT_Decal,
	eT_DecalNoDepthOffset,
	eT_NoChunkMerging,
	eT_ForceTransPass,
	eT_AfterHDRPostProcess,
	eT_AfterPostProcess,
	eT_ForceZpass,
	eT_ForceWaterPass,
	eT_ForceDrawLast,
	eT_ForceDrawFirst,
	eT_ForceDrawAfterWater,
	eT_DepthFixup,
	eT_SingleLightPass,
	eT_HWTessellation,
	eT_WaterParticle,
	eT_AlphaBlendShadows,
	eT_ZPrePass,
	eT_WrinkleBlending,

	eT_Light,
	eT_Shadow,
	eT_Fur,
	eT_General,
	eT_Terrain,
	eT_Overlay,
	eT_NoDraw,
	eT_Custom,
	eT_Sky,
	eT_OceanShore,
	eT_Hair,
	eT_Compute,
	eT_ForceGeneralPass,
	eT_SkinPass,
	eT_EyeOverlay,

	eT_Metal,
	eT_Ice,
	eT_Water,
	eT_FX,
	eT_HDR,
	eT_HUD3D,
	eT_Glass,
	eT_Vegetation,
	eT_Particle,
	eT_GenerateClouds,
	eT_ScanWater,

	eT_NoLights,
	eT_NoMaterialState,
	eT_PositionInvariant,
	eT_TechniqueZ,  // Has to be first Technique
	eT_TechniqueShadowGen,
	eT_TechniqueMotionBlur,
	eT_TechniqueCustomRender,
	eT_TechniqueEffectLayer,
	eT_TechniqueDebug,
	eT_TechniqueWaterRefl,
	eT_TechniqueWaterCaustic,
	eT_TechniqueZPrepass,
	eT_TechniqueThickness,

	eT_TechniqueMax,

	eT_KeyFrameParams,
	eT_KeyFrameRandColor,
	eT_KeyFrameRandIntensity,
	eT_KeyFrameRandSpecMult,
	eT_KeyFrameRandPosOffset,
	eT_Speed,

	eT_Beam,
	eT_LensOptics,
	eT_Cloud,
	eT_Ocean,

	eT_Model,
	eT_StartRadius,
	eT_EndRadius,
	eT_StartColor,
	eT_EndColor,
	eT_LightStyle,
	eT_Length,

	eT_RGBStyle,
	eT_Scale,
	eT_Blind,
	eT_SizeBlindScale,
	eT_SizeBlindBias,
	eT_IntensBlindScale,
	eT_IntensBlindBias,
	eT_MinLight,
	eT_DistFactor,
	eT_DistIntensityFactor,
	eT_FadeTime,
	eT_Layer,
	eT_Importance,
	eT_VisAreaScale,

	eT_Poly,
	eT_Identity,
	eT_FromObj,
	eT_FromLight,
	eT_Fixed,

	eT_ParticlesFile,

	eT_Gravity,
	eT_WindDirection,
	eT_WindSpeed,
	eT_WaveHeight,
	eT_DirectionalDependence,
	eT_ChoppyWaveFactor,
	eT_SuppressSmallWavesFactor,

	eT__LT_LIGHTS,
	eT__LT_NUM,
	eT__LT_HASPROJ,
	eT__LT_0_TYPE,
	eT__LT_1_TYPE,
	eT__LT_2_TYPE,
	eT__LT_3_TYPE,
	eT__TT_TEXCOORD_MATRIX,
	eT__TT_TEXCOORD_PROJ,
	eT__TT_TEXCOORD_GEN_OBJECT_LINEAR,
	eT__VT_TYPE,
	eT__VT_TYPE_MODIF,
	eT__VT_BEND,
	eT__VT_DET_BEND,
	eT__VT_GRASS,
	eT__VT_WIND,
	eT__VT_DEPTH_OFFSET,
	eT__FT_TEXTURE,
	eT__FT_TEXTURE1,
	eT__FT_NORMAL,
	eT__FT_PSIZE,
	eT__FT_DIFFUSE,
	eT__FT_SPECULAR,
	eT__FT_TANGENT_STREAM,
	eT__FT_QTANGENT_STREAM,
	eT__FT_SKIN_STREAM,
	eT__FT_VERTEX_VELOCITY_STREAM,
	eT__FT0_COP,
	eT__FT0_AOP,
	eT__FT0_CARG1,
	eT__FT0_CARG2,
	eT__FT0_AARG1,
	eT__FT0_AARG2,

	eT__VS,
	eT__PS,
	eT__GS,
	eT__HS,
	eT__DS,
	eT__CS,

	eT_x,
	eT_y,
	eT_z,
	eT_w,
	eT_r,
	eT_g,
	eT_b,
	eT_a,

	eT_true,
	eT_false,

	eT_0,
	eT_1,
	eT_2,
	eT_3,
	eT_4,
	eT_5,
	eT_6,
	eT_7,
	eT_8,
	eT_9,
	eT_10,
	eT_11,
	eT_12,
	eT_13,
	eT_14,
	eT_15,

	eT_AnisotropyLevel,

	eT_ORBIS,
	eT_DURANGO,
	eT_PCDX11,
	eT_OPENGL,
	eT_VULKAN,

	eT_VT_DetailBendingGrass,
	eT_VT_DetailBending,
	eT_VT_WindBending,
	eT_VertexColors,

	eT_s0,
	eT_s1,
	eT_s2,
	eT_s3,
	eT_s4,
	eT_s5,
	eT_s6,
	eT_s7,
	eT_s8,
	eT_s9,
	eT_s10,
	eT_s11,
	eT_s12,
	eT_s13,
	eT_s14,
	eT_s15,

	eT_t0,
	eT_t1,
	eT_t2,
	eT_t3,
	eT_t4,
	eT_t5,
	eT_t6,
	eT_t7,
	eT_t8,
	eT_t9,
	eT_t10,
	eT_t11,
	eT_t12,
	eT_t13,
	eT_t14,
	eT_t15,

	eT_Global,

	eT_Load,
	eT_Sample,
	eT_Gather,
	eT_GatherRed,
	eT_GatherGreen,
	eT_GatherBlue,
	eT_GatherAlpha,

	eT_$AutoGS_MultiRes,
	eT_Billboard,
	eT_DebugHelper,

	eT_max,
	eT_user_first = eT_max + 1
};

enum ETokenStorageClass
{
	eTS_invalid = 0,
	eTS_default,
	eTS_static,
	eTS_const,
	eTS_shared,
	eTS_groupshared
};

struct SFXTokenBin
{
	uint32 id;
};

#define FX_BEGIN_TOKENS \
  static SFXTokenBin sCommands[] = {

#define FX_END_TOKENS \
  { eT_unknown }      \
  };

#define FX_TOKEN(id) \
  { Parser.fxTokenKey( # id, eT_ ## id) },

#define FX_REGISTER_TOKEN(id) fxTokenKey( # id, eT_ ## id);

extern char* g_KeyTokens[];

struct SMacroBinFX
{
	std::vector<uint32> m_Macro;
	uint64 m_nMask;
};

class CParserBin;

typedef std::map<uint32, SMacroBinFX> FXMacroBin;
typedef FXMacroBin::iterator          FXMacroBinItor;

struct SParserFrame
{
	uint32 m_nFirstToken;
	uint32 m_nLastToken;
	uint32 m_nCurToken;
	SParserFrame(uint32 nFirstToken, uint32 nLastToken)
	{
		m_nFirstToken = nFirstToken;
		m_nLastToken = nLastToken;
		m_nCurToken = m_nFirstToken;
	}
	SParserFrame()
	{
		m_nFirstToken = 0;
		m_nLastToken = 0;
		m_nCurToken = m_nFirstToken;
	}
	inline void Reset()
	{
		m_nFirstToken = 0;
		m_nLastToken = 0;
		m_nCurToken = m_nFirstToken;
	}
	inline bool IsEmpty() const
	{
		if (!m_nFirstToken && !m_nLastToken)
			return true;
		return (m_nLastToken < m_nFirstToken);
	}
};

enum EFragmentType
{
	eFT_Unknown,
	eFT_Function,
	eFT_Structure,
	eFT_Sampler,
	eFT_ConstBuffer,
	eFT_StorageClass
};

struct SCodeFragment
{
	uint32 m_nFirstToken;
	uint32 m_nLastToken;
	uint32 m_dwName;
	EFragmentType m_eType;
#ifdef _DEBUG
	//string m_Name;
#endif
	SCodeFragment()
	{
		m_nFirstToken = 0;
		m_nLastToken = 0;
		m_dwName = 0;
		m_eType = eFT_Unknown;
	}
};

struct SortByToken
{
	bool operator()(const STokenD& left, const STokenD& right) const
	{
		return left.Token < right.Token;
	}
	bool operator()(const uint32 left, const STokenD& right) const
	{
		return left < right.Token;
	}
	bool operator()(const STokenD& left, uint32 right) const
	{
		return left.Token < right;
	}
};

#define SF_VULKAN   0x04000000
#define SF_GLES3    0x08000000
#define SF_D3D11    0x10000000
#define SF_ORBIS    0x20000000
#define SF_DURANGO  0x40000000
#define SF_GL4      0x80000000
#define SF_PLATFORM 0xfC000000

class CParserBin
{
	friend class CShaderManBin;
	friend class CHWShader;
	friend class CHWShader_D3D;
	friend struct SFXParam;
	friend struct SFXSampler;
	friend struct SFXTexture;

	//bool m_bEmbeddedSearchInfo;
	struct SShaderBin* m_pCurBinShader;
	CShader* m_pCurShader;
	TArray<uint32> m_Tokens;
	FXMacroBin m_Macros[3];
	FXShaderToken m_TokenTable;
	TArray<uint64> m_IfAffectMask;
	//std::vector<std::vector<int>> m_KeyOffsets;
	EToken m_eToken;
	uint32 m_nFirstToken;
	TArray<SCodeFragment> m_CodeFragments;

	SParserFrame m_CurFrame;

	SParserFrame m_Name;
	SParserFrame m_Assign;
	SParserFrame m_Annotations;
	SParserFrame m_Value;
	SParserFrame m_Data;

	static FXMacroBin m_StaticMacros;

public:
	CParserBin(SShaderBin* pBin);
	CParserBin(SShaderBin* pBin, CShader* pSH);

	static FXMacroBin&        GetStaticMacroses() { return m_StaticMacros; }
	static const char*        GetString(uint32 nToken, const FXShaderToken& Table, bool bOnlyKey = false);
	const char*               GetString(uint32 nToken, bool bOnlyKey = false);
	string                    GetString(SParserFrame& Frame);
	CCryNameR                 GetNameString(SParserFrame& Frame);
	void                      BuildSearchInfo();
	bool                      PreprocessTokens(ShaderTokensVec& Tokens, int nPass, PodArray<uint32>& tokensBuffer);
	bool                      Preprocess(int nPass, ShaderTokensVec& Tokens, const FXShaderToken& pSrcTable);
	static const SMacroBinFX* FindMacro(uint32 dwName, FXMacroBin& Macro);
	static bool               AddMacro(uint32 dwToken, const uint32* pMacro, int nMacroTokens, uint64 nMask, FXMacroBin& Macro);
	static bool               RemoveMacro(uint32 dwToken, FXMacroBin& Macro);
	static void               CleanPlatformMacros();
	uint32                    NewUserToken(uint32 nToken, const char* psToken, bool bUseFinalTable);
	//uint32 NewUserToken(uint32 nToken, const string& sToken, bool bUseFinalTable);
	void                      MergeTable(SShaderBin* pBin);
	bool                      CheckIfExpression(const uint32* pTokens, uint32& nT, int nPass, uint64* nMask = 0);
	bool                      IgnorePreprocessBlock(const uint32* pTokens, uint32& nT, int nMaxTokens, PodArray<uint32>& tokensBuffer, int nPass);
	static bool               CorrectScript(uint32* pTokens, uint32& i, uint32 nT, TArray<char>& Text);
	static bool               ConvertToAscii(uint32* pTokens, uint32 nT, const FXShaderToken& Table, TArray<char>& Text, bool bInclSkipTokens = false);
	bool                      GetBool(SParserFrame& Frame);
	inline uint32*            GetTokens(int nStart) { return &m_Tokens[nStart]; }
	inline int                GetNumTokens()        { return m_Tokens.size(); }
	inline EToken             GetToken()            { return m_eToken; }
	inline EToken             GetToken(SParserFrame& Frame)
	{
		assert(!Frame.IsEmpty());
		return (EToken)m_Tokens[Frame.m_nFirstToken];
	}
	inline uint32 FirstToken() { return m_nFirstToken; }
	inline int    GetInt(uint32 nToken)
	{
		const char* szStr = GetString(nToken);
		if (szStr[0] == '0' && szStr[1] == 'x')
		{
			int i = 0;
			int res = sscanf(&szStr[2], "%x", &i);
			assert(res != 0);
			return i;
		}
		return atoi(szStr);
	}
	inline float GetFloat(SParserFrame& Frame)
	{
		return (float)atof(GetString(Frame).c_str());
	}
	static inline uint32 NextToken(const uint32* pTokens, uint32& nCur, uint32 nLast)
	{
		while (nCur <= nLast)
		{
			uint32 nToken = pTokens[nCur++];
			if (nToken == eT_skip)
			{
				nCur++;
				continue;
			}
			if (nToken == eT_skip_1)
			{
				while (nCur <= nLast)
				{
					nToken = pTokens[nCur++];
					if (nToken == eT_skip_2)
						break;
				}
				continue;
			}
			return nToken;
		}
		return 0;
	}

	SParserFrame       BeginFrame(SParserFrame& Frame);
	void               EndFrame(SParserFrame& Frame);

	byte               GetCompareFunc(EToken eT);
	int                GetSrcBlend(EToken eT);
	int                GetDstBlend(EToken eT);

	void               InsertSkipTokens(const uint32* pTokens, uint32 nStart, uint32 nTokens, bool bSingle, PodArray<uint32>& tokensBuffer);
	ETokenStorageClass ParseObject(SFXTokenBin* pTokens, int& nIndex);
	ETokenStorageClass ParseObject(SFXTokenBin* pTokens);
	int                GetNextToken(uint32& nStart, ETokenStorageClass& nTokenStorageClass);
	bool               FXGetAssignmentData(SParserFrame& Frame);
	bool               FXGetAssignmentData2(SParserFrame& Frame);
	bool               GetAssignmentData(SParserFrame& Frame);
	bool               GetSubData(SParserFrame& Frame, EToken eT1, EToken eT2);
	static int32       FindToken(uint32 nStart, uint32 nLast, const uint32* pTokens, uint32 nToken);
	int32              FindToken(uint32 nStart, uint32 nLast, uint32 nToken);
	int32              FindToken(uint32 nStart, uint32 nLast, const uint32* pTokens);
	int                CopyTokens(SParserFrame& Fragment, std::vector<uint32>& NewTokens);
	int                CopyTokens(SCodeFragment* pCF, PodArray<uint32>& SHData, TArray<SCodeFragment>& Replaces, TArray<uint32>& NewTokens, uint32 nID);
	static inline void AddDefineToken(uint32 dwToken, ShaderTokensVec& Tokens)
	{
		if (dwToken == 611)
		{
			int nnn = 0;
		}
		Tokens.push_back(eT_define);
		Tokens.push_back(dwToken);
		Tokens.push_back(0);
	}
	static inline void AddDefineToken(uint32 dwToken, uint32 dwToken2, ShaderTokensVec& Tokens)
	{
		if (dwToken == 611)
		{
			int nnn = 0;
		}
		Tokens.push_back(eT_define);
		Tokens.push_back(dwToken);
		Tokens.push_back(dwToken2);
		Tokens.push_back(0);
	}
	bool                 JumpSemicolumn(uint32& nStart, uint32 nEnd);

	static uint32        fxToken(const char* szToken, bool* bKey = NULL);
	static uint32        fxTokenKey(char* szToken, EToken eT = eT_unknown);
	static uint32        GetCRC32(const char* szStr);
	static uint32        NextToken(const char*& buf, char* com, bool& bKey);
	static uint32        NextToken(char*& buf, char* com, bool& bKey);
	static void          Init();
	static void          RemovePlatformDefines();
	static void          SetupForPlatform(uint32 nPlatform);
	static void          SetupFeatureDefines();
	static CCryNameTSCRC GetPlatformSpecName(CCryNameTSCRC orgName);
	static const char*   GetPlatformShaderlistName();
	static bool          PlatformSupportsConstantBuffers() { return (CParserBin::m_nPlatform & (SF_D3D11 | SF_ORBIS | SF_DURANGO | SF_GL4 | SF_VULKAN | SF_GLES3)) != 0; };
	static bool          PlatformSupportsGeometryShaders() { return (CParserBin::m_nPlatform & (SF_D3D11 | SF_ORBIS | SF_DURANGO | SF_GL4 | SF_VULKAN)) != 0; }
	static bool          PlatformSupportsHullShaders()     { return (CParserBin::m_nPlatform & (SF_D3D11 | SF_ORBIS | SF_DURANGO | SF_GL4 | SF_VULKAN)) != 0; }
	static bool          PlatformSupportsDomainShaders()   { return (CParserBin::m_nPlatform & (SF_D3D11 | SF_ORBIS | SF_DURANGO | SF_GL4 | SF_VULKAN)) != 0; }
	static bool          PlatformSupportsComputeShaders()  { return (CParserBin::m_nPlatform & (SF_D3D11 | SF_ORBIS | SF_DURANGO | SF_GL4 | SF_VULKAN)) != 0; }
	static bool          PlatformIsConsole()               { return (CParserBin::m_nPlatform & (SF_ORBIS | SF_DURANGO)) != 0; };

	static bool m_bEditable;
	static uint32 m_nPlatform;
	static bool m_bEndians;
	static bool m_bParseFX;
	static bool m_bShaderCacheGen;
};

char* fxFillPr(char** buf, char* dst);

#endif
