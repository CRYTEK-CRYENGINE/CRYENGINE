// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/ITexture.h>

//===================================================================

#define MAX_ENVTEXTURES    16
#define MAX_ENVTEXSCANDIST 0.1f

class CTexture;
struct SEnvTexture;
class CCVarUpdateRecorder;

// Custom Textures IDs
enum
{
	TO_RT_2D = 1,

	TO_FOG,
	TO_FROMOBJ,
	TO_WINDGRID,
	TO_SVOTREE,
	TO_SVOTRIS,
	TO_SVOGLCM,
	TO_SVORGBS,
	TO_SVONORM,
	TO_SVOOPAC,
	TO_FROMOBJ_CM,

	TO_SHADOWID0,
	TO_SHADOWID1,
	TO_SHADOWID2,
	TO_SHADOWID3,
	TO_SHADOWID4,
	TO_SHADOWID5,
	TO_SHADOWID6,
	TO_SHADOWID7,

	TO_FROMRE0,
	TO_FROMRE1,

	TO_FROMRE0_FROM_CONTAINER,
	TO_FROMRE1_FROM_CONTAINER,

	TO_SCREENMAP,
	TO_SHADOWMASK,
	TO_TERRAIN_LM,
	TO_RT_CM,
	TO_CLOUDS_LM,
	TO_BACKBUFFERMAP,
	TO_MIPCOLORS_DIFFUSE,
	TO_MIPCOLORS_BUMP,

	TO_DOWNSCALED_ZTARGET_FOR_AO,
	TO_QUARTER_ZTARGET_FOR_AO,
	TO_WATEROCEANMAP,
	TO_WATERVOLUMEMAP,

	TO_WATERVOLUMEREFLMAP,
	TO_WATERVOLUMEREFLMAPPREV,
	TO_WATERVOLUMECAUSTICSMAP,
	TO_WATERVOLUMECAUSTICSMAPTEMP,

	TO_COLORCHART,

	TO_ZTARGET_MS,

	TO_SCENE_NORMALMAP,
	TO_SCENE_NORMALMAP_MS,

	TO_SCENE_DIFFUSE_ACC,
	TO_SCENE_SPECULAR_ACC,
	TO_SCENE_DIFFUSE_ACC_MS,
	TO_SCENE_SPECULAR_ACC_MS,
	TO_SCENE_TEXTURES,
	TO_SCENE_TARGET,

	TO_BACKBUFFERSCALED_D2,
	TO_BACKBUFFERSCALED_D4,
	TO_BACKBUFFERSCALED_D8,

	TO_SKYDOME_MIE,
	TO_SKYDOME_RAYLEIGH,
	TO_SKYDOME_MOON,

	TO_VOLFOGSHADOW_BUF,

	TO_HDR_MEASURED_LUMINANCE,
	TO_MODELHUD,
};

#define NUM_HDR_TONEMAP_TEXTURES 4
#define NUM_HDR_BLOOM_TEXTURES   3
#define MIN_DOF_COC_K            6

#define MAX_OCCLUSION_READBACK_TEXTURES (MAX_GPU_NUM * MAX_FRAMES_IN_FLIGHT)

//  render targets info - first gather list of hdr targets, sort by size and create after
struct SRenderTargetInfo
{
	SRenderTargetInfo() : nWidth(0), nHeight(0), cClearColor(Clr_Empty), Format(eTF_Unknown), nFlags(0), lplpStorage(0), nPitch(0), fPriority(0.0f), nCustomID(0)
	{}

	uint32      nWidth;
	uint32      nHeight;
	ColorF      cClearColor;
	ETEX_Format Format;
	uint32      nFlags;
	CTexture**  lplpStorage;
	char        szName[64];
	uint32      nPitch;
	float       fPriority;
	int32       nCustomID;
};

struct RenderTargetSizeSort
{
	bool operator()(const SRenderTargetInfo& drtStart, const SRenderTargetInfo& drtEnd) { return (drtStart.nPitch * drtStart.fPriority) > (drtEnd.nPitch * drtEnd.fPriority); }
};

class SRenderTargetPool
{
public:
	void AddRenderTarget(uint32 nWidth, uint32 nHeight, const ColorF& cClear, ETEX_Format Format, float fPriority, const char* szName, CTexture** pStorage, uint32 nFlags = 0, int32 nCustomID = -1, bool bDynamicTex = 0);
	bool CreateRenderTargetList();
	void ClearRenderTargetList()
	{
		m_pRenderTargets.clear();
	}

	static SRenderTargetPool* GetInstance()
	{
		static SRenderTargetPool pInstance;
		return &pInstance;
	}

private:
	std::vector<SRenderTargetInfo> m_pRenderTargets;
};

class CRendererResources
{
public:
	void InitResources() {}

public:
	static size_t m_RTallocs, m_DTallocs;

	static CTexture*   CreateDepthTarget(int nWidth, int nHeight, const ColorF& cClear, ETEX_Format eTF);
	static CTexture*   CreateRenderTarget(int nWidth, int nHeight, const ColorF& cClear, ETEX_Format eTF);

	static ETEX_Format GetHDRFormat(bool withAlpha, bool lowQuality);
	static ETEX_Format GetLDRFormat(bool precisionBits);
	static ETEX_Format GetDisplayFormat();
	static ETEX_Format GetDepthFormat();

public:
	static bool m_bLoadedSystem;

	// Global renderer resources
	static void CreateHDRMaps(int resourceWidth, int resourceHeight);
	static bool CreatePostFXMaps(int resourceWidth, int resourceHeight);
	static void CreateSystemTargets(int resourceWidth, int resourceHeight);

	static void ResizeSystemTargets(int renderWidth, int renderHeight);

	static void DestroyHDRMaps();
	static void DestroyPostFXMaps();
	static void DestroySystemTargets();

	static void LoadDefaultSystemTextures();
	static void UnloadDefaultSystemTextures(bool bFinalRelease = false);

	static void ShutDown();

	static void OnRenderResolutionChanged(int renderWidth, int renderHeight);
	static void OnOutputResolutionChanged(int outputWidth, int outputHeight);
	static void OnDisplayResolutionChanged(int displayWidth, int displayHeight);

	static int         s_resourceWidth, s_resourceHeight;
	static int         s_renderWidth, s_renderHeight, s_renderMinDim, s_renderArea;
	static int         s_outputWidth, s_outputHeight;
	static int         s_displayWidth, s_displayHeight;
	static ETEX_Format s_eTFZ;

	// ==============================================================================
	// Capabilities
	static SPixFormatSupport s_hwTexFormatSupport;

	// ==============================================================================
	// Memory/Texture pools
	static SEnvTexture* FindSuitableEnvTex(Vec3& Pos, Ang3& Angs, bool bMustExist, int RendFlags, bool bUseExistingREs, CShader* pSH, CShaderResources* pRes, CRenderObject* pObj, bool bReflect, CRenderElement* pRE, bool* bMustUpdate, const SRenderingPassInfo* pPassInfo);

	// ==============================================================================

	static CTexture* s_ptexDisplayTargetScaledPrev;           // CSonarVision, ?, 2x
	static CTexture* s_ptexMipColors_Diffuse;                 // ?
	static CTexture* s_ptexMipColors_Bump;                    // ?
	static CTexture* s_ptexFromRE[8];                         // ?
	static CTexture* s_ptexShadowID[8];                       // ?
	static CTexture* s_ptexRT_2D;                             // CREWaterOcean, ?
	static CTexture* s_ptexFromRE_FromContainer[2];           // ?
	static CTexture* s_ptexFromObj;                           // ?
	static CTexture* s_ptexHDRMeasuredLuminanceDummy;         // ?
	static CTexture* s_ptexCloudsLM;                          // ?
	static CTexture* s_ptexColorChart;                        // ?

	static CTexture* s_pTexNULL;

	// From file loaded textures ===============================================================================

	static CTexture* s_ptexNoTexture;
	static CTexture* s_ptexNoTextureCM;
	static CTexture* s_ptexWhite;
	static CTexture* s_ptexGray;
	static CTexture* s_ptexMedian;
	static CTexture* s_ptexBlack;
	static CTexture* s_ptexBlackAlpha;
	static CTexture* s_ptexBlackCM;
	static CTexture* s_ptexDefaultProbeCM;
	static CTexture* s_ptexDefaultMergedDetail;
	static CTexture* s_ptexFlatBump;
#if !defined(_RELEASE)
	static CTexture* s_ptexMipMapDebug;
	static CTexture* s_ptexColorBlue;
	static CTexture* s_ptexColorCyan;
	static CTexture* s_ptexColorGreen;
	static CTexture* s_ptexColorPurple;
	static CTexture* s_ptexColorRed;
	static CTexture* s_ptexColorWhite;
	static CTexture* s_ptexColorYellow;
	static CTexture* s_ptexColorMagenta;
	static CTexture* s_ptexColorOrange;
#endif
	static CTexture* s_ptexPaletteDebug;
	static CTexture* s_ptexPaletteTexelsPerMeter;
	static CTexture* s_ptexIconShaderCompiling;
	static CTexture* s_ptexIconStreaming;
	static CTexture* s_ptexIconStreamingTerrainTexture;
	static CTexture* s_ptexIconNavigationProcessing;
	static CTexture* s_ptexShadowJitterMap;
	static CTexture* s_ptexEnvironmentBRDF;
	static CTexture* s_ptexScreenNoiseMap;
	static CTexture* s_ptexDissolveNoiseMap;
	static CTexture* s_ptexNoise3D;
	static CTexture* s_ptexGrainFilterMap;
	static CTexture* s_ptexFilmGrainMap;
	static CTexture* s_ptexVignettingMap; // todo: create at runtime based on viggneting cvars
	static CTexture* s_ptexAOJitter;
	static CTexture* s_ptexAOVOJitter;
	static CTexture* s_ptexNormalsFitting;
	static CTexture* s_ptexPerlinNoiseMap;
	static CTexture* s_ptexLTC1;
	static CTexture* s_ptexLTC2;

	// Static resolution [independent] targets =================================================================

	static CTexture* s_ptexWindGrid;                                             // CSceneGBufferStage, CSceneForwardStage, CSceneCustomStage
	static CTexture* s_ptexHDRAdaptedLuminanceCur[8];                            // CAutoExposureStage
	static int       s_nCurLumTextureIndex;                                      // CAutoExposureStage
	static CTexture* s_ptexCurLumTexture;                                        // CAutoExposureStage, CToneMappingStage, CPostAAStage
	static CTexture* s_ptexHDRToneMaps[NUM_HDR_TONEMAP_TEXTURES];                // CAutoExposureStage, CBloomStage
	static CTexture* s_ptexFarPlane;                                             // shadow map representing the far plane (all tests pass)
	static CTexture* s_ptexWaterOcean;                                           // CWaterStage, CREWaterOcean, water ocean vertex texture
	static CTexture* s_ptexWaterVolumeTemp[2];                                   // CWaterStage, water volume heightmap
	static CTexture* s_ptexWaterVolumeDDN;                                       // CWaterStage, water volume heightmap
	static CTexture* s_ptexFlaresOcclusionRing[MAX_OCCLUSION_READBACK_TEXTURES]; // CFlareSoftOcclusionQuery
	static CTexture* s_ptexFlaresGather;                                         // CFlareSoftOcclusionQuery
	static CTexture* s_ptexVolumetricFog;                                        // CVolumetricFogStage

	// Pools ===================================================================================================
	static SEnvTexture         s_EnvTexts[MAX_ENVTEXTURES];                      // FX_DrawToRenderTarget, SHRenderTarget::GetEnv2D
	static TArray<SEnvTexture> s_CustomRT_2D;                                    // FX_DrawToRenderTarget, SHRenderTarget::GetEnv2D, CREWaterOcean
	static TArray<CTexture>    s_ShaderTemplates;
	static bool                s_ShaderTemplatesInitialized;
};
