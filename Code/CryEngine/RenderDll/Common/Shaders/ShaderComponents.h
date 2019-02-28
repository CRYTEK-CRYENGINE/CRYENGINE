// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   ShaderComponents.h : FX Shaders semantic components declarations.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#ifndef __SHADERCOMPONENTS_H__
#define __SHADERCOMPONENTS_H__

#include "../Defs.h"

#if CRY_PLATFORM_ORBIS
	#undef PF_LOCAL // Defined in socket library
#endif

#if defined(PF_LOCAL) && !(PF_LOCAL == 1)
// e.g.: In Mac OS X it is defined in a system header
	#error "PF_LOCAL defined somewhere else with a different value"
#elif !defined(PF_LOCAL)
	#define PF_LOCAL            1
#endif
#define PF_SINGLE_COMP        2
#define PF_DONTALLOW_DYNMERGE 4
#define PF_INTEGER            8
#define PF_BOOL               0x10
#define PF_POSITION           0x20
#define PF_MATRIX             0x40
#define PF_SCALAR             0x80
#define PF_TWEAKABLE_0        0x100
#define PF_TWEAKABLE_1        0x200
#define PF_TWEAKABLE_2        0x400
#define PF_TWEAKABLE_3        0x800
#define PF_TWEAKABLE_MASK     0xf00
#define PF_MERGE_MASK         0xff000
#define PF_MERGE_SHIFT        12
#define PF_INSTANCE           0x100000
#define PF_MATERIAL           0x200000
// unused                     0x400000
// unused                     0x800000
#define PF_CUSTOM_BINDED      0x1000000
#define PF_CANMERGED          0x2000000
#define PF_AUTOMERGED         0x4000000
#define PF_ALWAYS             0x8000000
#define PF_GLOBAL             0x10000000

enum ECGParam
{
	ECGP_Unknown,

	ECGP_Matr_PI_ObjOrigComposite,
	ECGP_PB_VisionMtlParams,
	ECGP_PI_EffectLayerParams,
	ECGP_PI_NumInstructions,

	ECGP_PI_WrinklesMask0,
	ECGP_PI_WrinklesMask1,
	ECGP_PI_WrinklesMask2,

	ECGP_PB_Scalar,
	ECGP_PM_Tweakable,

	ECGP_PM_MatChannelSB,
	ECGP_PM_MatDiffuseColor,
	ECGP_PM_MatSpecularColor,
	ECGP_PM_MatEmissiveColor,
	ECGP_PM_MatMatrixTCM,
	ECGP_PM_MatDeformWave,
	ECGP_PM_MatDetailTilingAndAlphaRef,
	ECGP_PM_MatSilPomDetailParams,

	ECGP_PB_HDRParams,
	ECGP_PB_StereoParams,

	ECGP_PB_IrregKernel,
	ECGP_PB_RegularKernel,

	ECGP_PB_VolumetricFogParams,
	ECGP_PB_VolumetricFogRampParams,
	ECGP_PB_VolumetricFogSunDir,
	ECGP_PB_FogColGradColBase,
	ECGP_PB_FogColGradColDelta,
	ECGP_PB_FogColGradParams,
	ECGP_PB_FogColGradRadial,
	ECGP_PB_VolumetricFogSamplingParams,
	ECGP_PB_VolumetricFogDistributionParams,
	ECGP_PB_VolumetricFogScatteringParams,
	ECGP_PB_VolumetricFogScatteringBlendParams,
	ECGP_PB_VolumetricFogScatteringColor,
	ECGP_PB_VolumetricFogScatteringSecondaryColor,
	ECGP_PB_VolumetricFogHeightDensityParams,
	ECGP_PB_VolumetricFogHeightDensityRampParams,
	ECGP_PB_VolumetricFogDistanceParams,
	ECGP_PB_VolumetricFogGlobalEnvProbe0,
	ECGP_PB_VolumetricFogGlobalEnvProbe1,

	ECGP_PB_FromRE,

	ECGP_PB_WaterLevel,
	ECGP_PB_CausticsParams,
	ECGP_PB_CausticsSmoothSunDirection,

	ECGP_PB_Time,
	ECGP_PB_FrameTime,
	ECGP_PB_CameraPos,
	ECGP_PB_ScreenSize,
	ECGP_PB_NearFarDist,

	ECGP_PB_CloudShadingColorSun,
	ECGP_PB_CloudShadingColorSky,

	ECGP_PB_CloudShadowParams,
	ECGP_PB_CloudShadowAnimParams,

	ECGP_PB_ClipVolumeParams,

	ECGP_PB_WaterRipplesLookupParams,

#if defined(FEATURE_SVO_GI)
	ECGP_PB_SvoViewProj0,
	ECGP_PB_SvoViewProj1,
	ECGP_PB_SvoViewProj2,
	ECGP_PB_SvoNodeBoxWS,
	ECGP_PB_SvoNodeBoxTS,
	ECGP_PB_SvoNodesForUpdate0,
	ECGP_PB_SvoNodesForUpdate1,
	ECGP_PB_SvoNodesForUpdate2,
	ECGP_PB_SvoNodesForUpdate3,
	ECGP_PB_SvoNodesForUpdate4,
	ECGP_PB_SvoNodesForUpdate5,
	ECGP_PB_SvoNodesForUpdate6,
	ECGP_PB_SvoNodesForUpdate7,

	ECGP_PB_SvoTreeSettings0,
	ECGP_PB_SvoTreeSettings1,
	ECGP_PB_SvoTreeSettings2,
	ECGP_PB_SvoTreeSettings3,
	ECGP_PB_SvoTreeSettings4,
	ECGP_PB_SvoTreeSettings5,
	ECGP_PB_SvoParams0,
	ECGP_PB_SvoParams1,
	ECGP_PB_SvoParams2,
	ECGP_PB_SvoParams3,
	ECGP_PB_SvoParams4,
	ECGP_PB_SvoParams5,
	ECGP_PB_SvoParams6,
	ECGP_PB_SvoParams7,
	ECGP_PB_SvoParams8,
	ECGP_PB_SvoParams9,
#endif

	ECGP_COUNT,
};

// Constants for RenderView to be set in shaders
struct SRenderViewShaderConstants
{
public:
	int      nFrameID;

	Vec3     vWaterLevel;           // ECGP_PB_WaterLevel *
	float    fHDRDynamicMultiplier; // ECGP_PB_HDRDynamicMultiplier *

	Vec4     pFogColGradColBase;  // ECGP_PB_FogColGradColBase *
	Vec4     pFogColGradColDelta; // ECGP_PB_FogColGradColDelta *
	Vec4     pFogColGradParams;   // ECGP_PB_FogColGradParams *
	Vec4     pFogColGradRadial;   // ECGP_PB_FogColGradRadial *

	Vec4     pVolumetricFogParams;     // ECGP_PB_VolumetricFogParams *
	Vec4     pVolumetricFogRampParams; // ECGP_PB_VolumetricFogRampParams *
	Vec4     pVolumetricFogSunDir;     // ECGP_PB_VolumetricFogSunDir *

	Vec3     pCameraPos;   //ECGP_PF_CameraPos

	Vec3     pCausticsParams; //ECGP_PB_CausticsParams *
	Vec3     pSunColor;       //ECGP_PF_SunColor *
	float    sunSpecularMultiplier;
	Vec3     pSkyColor;       //ECGP_PF_SkyColor *
	Vec3     pSunDirection;   //ECGP_PF_SunDirection *

	Vec3     pCloudShadingColorSun; //ECGP_PB_CloudShadingColorSun *
	Vec3     pCloudShadingColorSky; //ECGP_PB_CloudShadingColorSky *

	Vec4     pCloudShadowParams;     //ECGP_PB_CloudShadowParams *
	Vec4     pCloudShadowAnimParams; //ECGP_PB_CloudShadowAnimParams *
	Vec4     pScreenspaceShadowsParams;

	Vec4     post3DRendererAmbient;

	Vec3     vCausticsCurrSunDir;
	int      nCausticsFrameID;

	Vec3     pVolCloudTilingSize;
	Vec3     pVolCloudTilingOffset;

	Vec4     pVolumetricFogSamplingParams;           // ECGP_PB_VolumetricFogSamplingParams
	Vec4     pVolumetricFogDistributionParams;       // ECGP_PB_VolumetricFogDistributionParams
	Vec4     pVolumetricFogScatteringParams;         // ECGP_PB_VolumetricFogScatteringParams
	Vec4     pVolumetricFogScatteringBlendParams;    // ECGP_PB_VolumetricFogScatteringParams
	Vec4     pVolumetricFogScatteringColor;          // ECGP_PB_VolumetricFogScatteringColor
	Vec4     pVolumetricFogScatteringSecondaryColor; // ECGP_PB_VolumetricFogScatteringSecondaryColor
	Vec4     pVolumetricFogHeightDensityParams;      // ECGP_PB_VolumetricFogHeightDensityParams
	Vec4     pVolumetricFogHeightDensityRampParams;  // ECGP_PB_VolumetricFogHeightDensityRampParams
	Vec4     pVolumetricFogDistanceParams;           // ECGP_PB_VolumetricFogDistanceParams

	float    irregularFilterKernel[8][4];
};

enum EOperation
{
	eOp_Unknown,
	eOp_Add,
	eOp_Sub,
	eOp_Div,
	eOp_Mul,
	eOp_Log,
};

struct SCGBind
{
	CCryNameR m_Name;
	uint32    m_Flags;
	short     m_dwBind;
	short     m_dwCBufSlot;
	int       m_nParameters;
	SCGBind()
	{
		m_nParameters = 1;
		m_dwBind = -2;
		m_dwCBufSlot = 0;
		m_Flags = 0;
	}
	SCGBind(const SCGBind& sb)
		: m_Name(sb.m_Name)
		, m_Flags(sb.m_Flags)
		, m_dwBind(sb.m_dwBind)
		, m_dwCBufSlot(sb.m_dwCBufSlot)
		, m_nParameters(sb.m_nParameters)
	{
	}
	SCGBind& operator=(const SCGBind& sb)
	{
		this->~SCGBind();
		new(this)SCGBind(sb);
		return *this;
	}
	int Size()
	{
		return sizeof(SCGBind);
	}
	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct SVertexInputStream
{
	char  semanticName[14];
	uint8 semanticIndex;
	uint8 attributeLocation;

	SVertexInputStream(const char* streamSemanticName, uint8 streamSemanticIndex, uint8 streamAttributeLocation)
	{
		CRY_ASSERT(strlen(streamSemanticName) < CRY_ARRAY_COUNT(semanticName));

		strncpy(semanticName, streamSemanticName, CRY_ARRAY_COUNT(semanticName));
		semanticIndex = streamSemanticIndex;
		attributeLocation = streamAttributeLocation;
	}
};

struct SParamData
{
	CCryNameR m_CompNames[4];
	union UData
	{
		uint64 nData64[4];
		uint32 nData32[4];
		float  fData[4];
	} d;
	SParamData()
	{
		memset(&d, 0, sizeof(UData));
	}
	~SParamData();
	SParamData(const SParamData& sp);
	SParamData& operator=(const SParamData& sp)
	{
		this->~SParamData();
		new(this)SParamData(sp);
		return *this;
	}
	unsigned Size() { return sizeof(SParamData); }
	void     GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
};

struct SCGLiteral
{
	int      m_nIndex;
	//Vec4 m_vVec;
	unsigned Size()                                  { return sizeof(SCGLiteral); }
	void     GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct SCGParam : SCGBind
{
	ECGParam    m_eCGParamType;
	SParamData* m_pData;
	UINT_PTR    m_nID;
	SCGParam()
	{
		m_eCGParamType = ECGP_Unknown;
		m_pData = NULL;
		m_nID = 0;
	}
	~SCGParam()
	{
		SAFE_DELETE(m_pData);
	}
	SCGParam(const SCGParam& sp) : SCGBind(sp)
	{
		m_eCGParamType = sp.m_eCGParamType;
		m_nID = sp.m_nID;
		if (sp.m_pData)
		{
			m_pData = new SParamData;
			*m_pData = *sp.m_pData;
		}
		else
			m_pData = NULL;
	}
	SCGParam& operator=(const SCGParam& sp)
	{
		this->~SCGParam();
		new(this)SCGParam(sp);
		return *this;
	}
	bool operator!=(const SCGParam& sp) const
	{
		if (sp.m_dwBind == m_dwBind &&
		    sp.m_Name == m_Name &&
		    sp.m_nID == m_nID &&
		    sp.m_nParameters == m_nParameters &&
		    sp.m_eCGParamType == m_eCGParamType &&
		    sp.m_dwCBufSlot == m_dwCBufSlot &&
		    sp.m_Flags == m_Flags &&
		    !sp.m_pData && !m_pData)
		{
			return false;
		}
		return true;
	}

	const CCryNameR GetParamCompName(int nComp) const
	{
		if (!m_pData)
			return CCryNameR("None");
		return m_pData->m_CompNames[nComp];
	}

	int Size()
	{
		int nSize = sizeof(SCGParam);
		if (m_pData)
			nSize += m_pData->Size();
		return nSize;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_pData);
	}
};

enum ECGSampler
{
	ECGS_Unknown,
	ECGS_MatSlot_Diffuse,
	ECGS_MatSlot_Normalmap,
	ECGS_MatSlot_Gloss,
	ECGS_MatSlot_Env,
	ECGS_Shadow0,
	ECGS_Shadow1,
	ECGS_Shadow2,
	ECGS_Shadow3,
	ECGS_Shadow4,
	ECGS_Shadow5,
	ECGS_Shadow6,
	ECGS_Shadow7,
	ECGS_TrilinearClamp,
	ECGS_TrilinearWrap,
	ECGS_MatAnisoHighWrap,
	ECGS_MatAnisoLowWrap,
	ECGS_MatTrilinearWrap,
	ECGS_MatBilinearWrap,
	ECGS_MatTrilinearClamp,
	ECGS_MatBilinearClamp,
	ECGS_MatAnisoHighBorder,
	ECGS_MatTrilinearBorder,
	ECGS_PointWrap,
	ECGS_PointClamp,
	ECGS_COUNT
};

struct SCGSampler : SCGBind
{
	SamplerStateHandle m_nStateHandle;
	ECGSampler m_eCGSamplerType;
	SCGSampler()
	{
		m_nStateHandle = EDefaultSamplerStates::Unspecified;
		m_eCGSamplerType = ECGS_Unknown;
	}
	~SCGSampler()
	{
	}
	SCGSampler(const SCGSampler& sp) : SCGBind(sp)
	{
		m_eCGSamplerType = sp.m_eCGSamplerType;
		m_nStateHandle = sp.m_nStateHandle;
	}
	SCGSampler& operator=(const SCGSampler& sp)
	{
		this->~SCGSampler();
		new(this)SCGSampler(sp);
		return *this;
	}
	bool operator!=(const SCGSampler& sp) const
	{
		if (sp.m_dwBind == m_dwBind &&
		    sp.m_Name == m_Name &&
		    sp.m_nStateHandle == m_nStateHandle &&
		    sp.m_nParameters == m_nParameters &&
		    sp.m_eCGSamplerType == m_eCGSamplerType &&
		    sp.m_dwCBufSlot == m_dwCBufSlot &&
		    sp.m_Flags == m_Flags)
		{
			return false;
		}
		return true;
	}

	int Size()
	{
		int nSize = sizeof(SCGSampler);
		return nSize;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
	}
};

enum ECGTexture : uint8
{
	ECGT_Unknown,

	ECGT_MatSlot_Diffuse,
	ECGT_MatSlot_Normals,
	ECGT_MatSlot_Specular,
	ECGT_MatSlot_Env,
	ECGT_MatSlot_Detail,
	ECGT_MatSlot_Smoothness,
	ECGT_MatSlot_Height,
	ECGT_MatSlot_DecalOverlay,
	ECGT_MatSlot_SubSurface,
	ECGT_MatSlot_Custom,
	ECGT_MatSlot_CustomSecondary,
	ECGT_MatSlot_Opacity,
	ECGT_MatSlot_Translucency,
	ECGT_MatSlot_Emittance,

	ECGT_ScaleformInput0,
	ECGT_ScaleformInput1,
	ECGT_ScaleformInput2,
	ECGT_ScaleformInputY,
	ECGT_ScaleformInputU,
	ECGT_ScaleformInputV,
	ECGT_ScaleformInputA,

	ECGT_Shadow0,
	ECGT_Shadow1,
	ECGT_Shadow2,
	ECGT_Shadow3,
	ECGT_Shadow4,
	ECGT_Shadow5,
	ECGT_Shadow6,
	ECGT_Shadow7,
	ECGT_ShadowMask,

	ECGT_HDR_Target,
	ECGT_HDR_TargetPrev,
	ECGT_HDR_AverageLuminance,
	ECGT_HDR_FinalBloom,

	ECGT_BackBuffer,
	ECGT_BackBufferScaled_d2,
	ECGT_BackBufferScaled_d4,
	ECGT_BackBufferScaled_d8,

	ECGT_ZTarget,
	ECGT_ZTargetMS,
	ECGT_ZTargetScaled_d2,
	ECGT_ZTargetScaled_d4,

	ECGT_SceneTarget,
	ECGT_SceneNormalsBent,
	ECGT_SceneNormals,
	ECGT_SceneDiffuse,
	ECGT_SceneSpecular,
	ECGT_SceneNormalsMS,

	ECGT_VolumetricClipVolumeStencil,
	ECGT_VolumetricFog,
	ECGT_VolumetricFogGlobalEnvProbe0,
	ECGT_VolumetricFogGlobalEnvProbe1,
	ECGT_VolumetricFogShadow0,
	ECGT_VolumetricFogShadow1,

	ECGT_WaterOceanMap,
	ECGT_WaterVolumeDDN,
	ECGT_WaterVolumeCaustics,
	ECGT_WaterVolumeRefl,
	ECGT_WaterVolumeReflPrev,
	ECGT_RainOcclusion,

	ECGT_TerrainNormMap,
	ECGT_TerrainBaseMap,
	ECGT_TerrainElevMap,

	ECGT_WindGrid,

	ECGT_CloudShadow,
	ECGT_VolCloudShadow,

	ECGT_COUNT
};

struct SCGTexture : SCGBind
{
	CTexture*  m_pTexture;
	STexAnim*  m_pAnimInfo;
	ECGTexture m_eCGTextureType;
	bool       m_bSRGBLookup;
	bool       m_bGlobal;

	SCGTexture()
	{
		m_pTexture = nullptr;
		m_pAnimInfo = nullptr;
		m_eCGTextureType = ECGT_Unknown;
		m_bSRGBLookup = false;
		m_bGlobal = false;
	}
	~SCGTexture();
	SCGTexture(const SCGTexture& sp);
	SCGTexture& operator=(const SCGTexture& sp)
	{
		this->~SCGTexture();
		new(this)SCGTexture(sp);
		return *this;
	}
	bool operator!=(const SCGTexture& sp) const
	{
		if (sp.m_dwBind == m_dwBind &&
		    sp.m_Name == m_Name &&
		    sp.m_nParameters == m_nParameters &&
		    sp.m_dwCBufSlot == m_dwCBufSlot &&
		    sp.m_Flags == m_Flags &&
		    sp.m_pAnimInfo == m_pAnimInfo &&
		    sp.m_pTexture == m_pTexture &&
		    sp.m_eCGTextureType == m_eCGTextureType &&
		    sp.m_bSRGBLookup == m_bSRGBLookup &&
		    sp.m_bGlobal == m_bGlobal)
		{
			return false;
		}
		return true;
	}

	int       Size()
	{
		int nSize = sizeof(SCGTexture);
		return nSize;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
	}
};

#endif
