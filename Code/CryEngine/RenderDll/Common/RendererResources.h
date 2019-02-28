// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/ITexture.h>

//===================================================================

#define MAX_ENVTEXTURES    16
#define MAX_ENVTEXSCANDIST 0.1f

class CTexture;
struct SEnvTexture;

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

class CRendererResources
{

public:
	void InitResources() {}

public:
	static size_t m_RTallocs, m_DTallocs;

	static CTexture* CreateDepthTarget(int nWidth, int nHeight, const ColorF& cClear, ETEX_Format eTF);
	static CTexture* CreateRenderTarget(int nWidth, int nHeight, const ColorF& cClear, ETEX_Format eTF);

	static ETEX_Format GetHDRFormat(bool withAlpha, bool lowQuality);
	static ETEX_Format GetLDRFormat();
	static ETEX_Format GetDisplayFormat();
	static ETEX_Format GetDepthFormat();

public:
	static bool m_bLoadedSystem;

	// Global renderer resources
	static void CreateDepthMaps(int resourceWidth, int resourceHeight);
	static void CreateSceneMaps(ETEX_Format eTF, int resourceWidth, int resourceHeight);
	static void CreateHDRMaps(int resourceWidth, int resourceHeight);
	static bool CreatePostFXMaps(int resourceWidth, int resourceHeight);
	static void CreateCachedShadowMaps();
	static void CreateNearestShadowMap();
	static void CreateDeferredMaps(int resourceWidth, int resourceHeight);
	static void CreateSystemTargets(int resourceWidth, int resourceHeight);

	static void ResizeSystemTargets(int renderWidth, int renderHeight);

	static void DestroyDepthMaps();
	static void DestroySceneMaps();
	static void DestroyHDRMaps();
	static void DestroyPostFXMaps();
	static void DestroyCachedShadowMaps();
	static void DestroyNearestShadowMap();
	static void DestroyDeferredMaps();
	static void DestroySystemTargets();

	static void LoadDefaultSystemTextures();
	static void UnloadDefaultSystemTextures(bool bFinalRelease = false);

	static void Clear();
	static void ShutDown();

	static void OnRenderResolutionChanged(int renderWidth, int renderHeight);
	static void OnOutputResolutionChanged(int outputWidth, int outputHeight);
	static void OnDisplayResolutionChanged(int displayWidth, int displayHeight);
	
	static int s_resourceWidth, s_resourceHeight;
	static int s_renderWidth, s_renderHeight, s_renderMinDim, s_renderArea;
	static int s_outputWidth, s_outputHeight;
	static int s_displayWidth, s_displayHeight;
	static ETEX_Format s_eTFZ;

	// ==============================================================================
	// Capabilities
	static SPixFormatSupport s_hwTexFormatSupport;

	// ==============================================================================
	// Memory/Texture pools
	static SEnvTexture* FindSuitableEnvTex(Vec3& Pos, Ang3& Angs, bool bMustExist, int RendFlags, bool bUseExistingREs, CShader* pSH, CShaderResources* pRes, CRenderObject* pObj, bool bReflect, CRenderElement* pRE, bool* bMustUpdate, const SRenderingPassInfo* pPassInfo);

	// ==============================================================================

	static CTexture* s_ptexPrevFrameScaled;                   // CSonarVision, ?, 2x
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

	// Static resolution [independent] targets =================================================================

	static CTexture* s_ptexWindGrid;                                             // CSceneGBufferStage, CSceneForwardStage, CSceneCustomStage
	static CTexture* s_ptexHDRAdaptedLuminanceCur[8];                            // CAutoExposureStage
	static int       s_nCurLumTextureIndex;                                      // CAutoExposureStage
	static CTexture* s_ptexCurLumTexture;                                        // CAutoExposureStage, CToneMappingStage, CPostAAStage
	static CTexture* s_ptexHDRToneMaps[NUM_HDR_TONEMAP_TEXTURES];                // CAutoExposureStage, CBloomStage
	static CTexture* s_ptexHDRMeasuredLuminance[MAX_GPU_NUM];                    // CAutoExposureStage, CScreenSpaceReflectionsStage
	static CTexture* s_ptexFarPlane;                                             // shadow map representing the far plane (all tests pass)
	static CTexture* s_ptexRainOcclusion;                                        // CRainStage, CSnowStage, top-down rain occlusion
	static CTexture* s_ptexWaterOcean;                                           // CWaterStage, CREWaterOcean, water ocean vertex texture
	static CTexture* s_ptexWaterVolumeTemp[2];                                   // CWaterStage, water volume heightmap
	static CTexture* s_ptexWaterVolumeDDN;                                       // CWaterStage, water volume heightmap
	static CTexture* s_ptexFlaresOcclusionRing[MAX_OCCLUSION_READBACK_TEXTURES]; // CFlareSoftOcclusionQuery
	static CTexture* s_ptexFlaresGather;                                         // CFlareSoftOcclusionQuery
	static CTexture* s_ptexVolumetricFog;                                        // CVolumetricFogStage

	// CVar resolution [dependent] targets =====================================================================

	static CTexture* s_ptexNearestShadowMap;                                     // CShadowMaskStage, CTiledShadingStage, CSceneForwardStage
	static CTexture* s_ptexCachedShadowMap[MAX_GSM_LODS_NUM];                    // CShadowMapStage
	static CTexture* s_ptexHeightMapAODepth[2];                                  // CShadowMapStage, CHeightMapAOStage
	static CTexture* s_ptexRT_ShadowPool;                                        // CShadowMapStage
	static CTexture* s_ptexWaterCaustics[2];                                     // ?, caustics buffers
	static CTexture* s_ptexVolCloudShadow;                                       // CVolumetricCloudsStage

	// Render resolution [dependent] targets ===================================================================

	static CTexture* s_ptexSceneSelectionIDs;                                    // Selection ID buffer used for selection and highlight passes

	static CTexture* s_ptexSceneDepthScaled[3];                                  // Half/Quarter/Eighth resolution depth-stencil, used for sub-resolution rendering
	static CTexture* s_ptexLinearDepth;
	static CTexture* s_ptexLinearDepthScaled[3];                                 // Min, Max, Avg, med
	static CTexture* s_ptexLinearDepthDownSample[4];                             // CDepthReadbackStage
	static CTexture* s_ptexLinearDepthReadBack[4];                               // CDepthReadbackStage

	static CTexture* s_ptexSceneTarget;                                          // Shared rt for generic usage (refraction/srgb/diffuse accumulation/hdr motionblur/etc)
	static CTexture* s_ptexSceneTargetR11G11B10F[2];
	static CTexture* s_ptexSceneDiffuse;
	static CTexture* s_ptexSceneDiffuseTmp;
	static CTexture* s_ptexSceneSpecular;
#if defined(DURANGO_USE_ESRAM)
	static CTexture* s_ptexSceneSpecularESRAM;                                   // Temporary scene specular in ESRAM, aliased with other ESRAM RTs
#endif
	static CTexture* s_ptexSceneSpecularTmp;
	static CTexture* s_ptexSceneNormalsMap;                                      // RT with normals for deferred shading
	static CTexture* s_ptexSceneNormalsBent;

	static CTexture* s_ptexVelocity;                                             // CMotionBlurStage
	static CTexture* s_ptexVelocityTiles[3];                                     // CMotionBlurStage
	static CTexture* s_ptexVelocityObjects[CCamera::eEye_eCount];                // CSceneGBufferStage, Dynamic object velocity (for left and right eye)

	static CTexture* s_ptexHDRTarget;
	static CTexture* s_ptexHDRTargetPrev;
	static CTexture* s_ptexHDRTargetScaled[4];
	static CTexture* s_ptexHDRTargetScaledTmp[4];
	static CTexture* s_ptexHDRTargetScaledTempRT[4];                             // CScreenSpaceReflectionsStage, CDepthOfFieldStage

	static CTexture* s_ptexHDRDofLayers[2];                                      // CDepthOfFieldStage
	static CTexture* s_ptexHDRTempBloom[2];                                      // CBloomStage
	static CTexture* s_ptexHDRFinalBloom;                                        // CRainStage, CToneMappingStage, CBloomStage

	static CTexture* s_ptexBackBuffer;                                           // back buffer copy
	static CTexture* s_ptexBackBufferScaled[3];                                  // backbuffer low-resolution/blurred version. 2x/4x/8x/16x smaller than screen
	static CTexture* s_ptexBackBufferScaledTemp[2];                              // backbuffer low-resolution/blurred version. 2x/4x/8x/16x smaller than screen, temp textures (used for blurring/ping-pong)

	static CTexture* s_ptexAOColorBleed;                                         // CScreenSpaceObscuranceStage, CTiledShadingStage
	static CTexture* s_ptexShadowMask;                                           // CShadowMapStage
	static CTexture* s_ptexModelHudBuffer;                                       // CV_r_UsePersistentRTForModelHUD, used by Menu3DModelRenderer to postprocess render models
	static CTexture* s_ptexSceneCoC[MIN_DOF_COC_K];                              // CDepthOfFieldStage
	static CTexture* s_ptexSceneCoCTemp;                                         // CDepthOfFieldStage
	static CTexture* s_ptexHeightMapAO[2];                                       // CHeightMapAOStage
	static CTexture* s_ptexWaterVolumeRefl[2];                                   // CWaterStage, water volume reflections buffer
	static CTexture* s_ptexRainSSOcclusion[2];                                   // CRainStage, screen-space rain occlusion accumulation
#if defined(VOLUMETRIC_FOG_SHADOWS)
	static CTexture* s_ptexVolFogShadowBuf[2];                                   // CVolumetricFogStage, CFogStage
#endif

	// Display resolution [dependent] targets ==================================================================
	static CTexture* s_ptexCached3DHud;                                          // CHud3DPass, 3d hud cached overframes
	static CTexture* s_ptexCached3DHudScaled;                                    // CHud3DPass, downsampled 3d hud cached overframes
	
	// Pools ===================================================================================================
	static SEnvTexture         s_EnvTexts[MAX_ENVTEXTURES];                      // FX_DrawToRenderTarget, SHRenderTarget::GetEnv2D
	static TArray<SEnvTexture> s_CustomRT_2D;                                    // FX_DrawToRenderTarget, SHRenderTarget::GetEnv2D, CREWaterOcean
	static TArray<CTexture>    s_ShaderTemplates;
	static bool                s_ShaderTemplatesInitialized;
};
