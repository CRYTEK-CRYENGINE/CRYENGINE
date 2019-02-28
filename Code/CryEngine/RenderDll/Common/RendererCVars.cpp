// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RendererCVars.h"
#include "Renderer.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/ITimeOfDay.h>
#include "DriverD3D.h"
#include "GraphicsPipeline/DebugRenderTargets.h"
#include <cctype>

#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	#define RENDERER_DEFAULT_MESHPOOLSIZE         (64U << 10)
	#define RENDERER_DEFAULT_MESHINSTANCEPOOLSIZE (16U << 10)
#endif
#if !defined(RENDERER_DEFAULT_MESHPOOLSIZE)
	#define RENDERER_DEFAULT_MESHPOOLSIZE (0U)
#endif
#if !defined(RENDERER_DEFAULT_MESHINSTANCEPOOLSIZE)
	#define RENDERER_DEFAULT_MESHINSTANCEPOOLSIZE (0U)
#endif

//////////////////////////////////////////////////////////////////////////
int CRendererCVars::CV_r_GraphicsPipeline;
int CRendererCVars::CV_r_GraphicsPipelineMobile;
int CRendererCVars::CV_r_GraphicsPipelinePassScheduler;
int CRendererCVars::CV_r_PostProcess_CB;
int CRendererCVars::CV_r_PostProcess;
AllocateConstIntCVar(CRendererCVars, CV_r_NightVision);
float CRendererCVars::CV_r_NightVisionFinalMul;
float CRendererCVars::CV_r_NightVisionBrightLevel;
float CRendererCVars::CV_r_NightVisionSonarRadius;
float CRendererCVars::CV_r_NightVisionSonarLifetime;
float CRendererCVars::CV_r_NightVisionSonarMultiplier;
float CRendererCVars::CV_r_NightVisionCamMovNoiseAmount;
float CRendererCVars::CV_r_NightVisionCamMovNoiseBlendSpeed;
float CRendererCVars::CV_r_dofMinZ;
float CRendererCVars::CV_r_dofMinZScale;
float CRendererCVars::CV_r_dofMinZBlendMult;
float CRendererCVars::CV_r_dofDilation;
int CRendererCVars::CV_r_vsync;
#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
float CRendererCVars::CV_r_overrideRefreshRate = 0;
int CRendererCVars::CV_r_overrideScanlineOrder = 0;
#endif
#if CRY_PLATFORM_WINDOWS
int CRendererCVars::CV_r_FullscreenPreemption = 1;
#endif
AllocateConstIntCVar(CRendererCVars, CV_r_SyncToFrameFence);
AllocateConstIntCVar(CRendererCVars, CV_e_DebugTexelDensity);
AllocateConstIntCVar(CRendererCVars, CV_e_DebugDraw);
AllocateConstIntCVar(CRendererCVars, CV_r_statsMinDrawcalls);
AllocateConstIntCVar(CRendererCVars, CV_r_stats);
AllocateConstIntCVar(CRendererCVars, CV_r_profiler);

int CRendererCVars::CV_r_HDRDithering;

float CRendererCVars::CV_r_profilerTargetFPS;
float CRendererCVars::CV_r_profilerSmoothingWeight;
AllocateConstIntCVar(CRendererCVars, CV_r_log);
AllocateConstIntCVar(CRendererCVars, CV_r_logTexStreaming);
AllocateConstIntCVar(CRendererCVars, CV_r_logShaders);
int CRendererCVars::CV_r_logVBuffers;
AllocateConstIntCVar(CRendererCVars, CV_r_logVidMem);
AllocateConstIntCVar(CRendererCVars, CV_r_useESRAM);
int CRendererCVars::CV_r_DeferredShadingSortLights;

int CRendererCVars::CV_r_BreakOnError;
int CRendererCVars::CV_r_durango_async_dips;
int CRendererCVars::CV_r_durango_async_dips_sync;
int CRendererCVars::CV_r_D3D12SubmissionThread;
int CRendererCVars::CV_r_D3D12WaitableSwapChain;
int CRendererCVars::CV_r_D3D12BatchResourceBarriers;
int CRendererCVars::CV_r_D3D12EarlyResourceBarriers;
int CRendererCVars::CV_r_D3D12AsynchronousCompute;
int CRendererCVars::CV_r_D3D12HardwareComputeQueue;
int CRendererCVars::CV_r_D3D12HardwareCopyQueue;
int CRendererCVars::CV_r_VkSubmissionThread;
int CRendererCVars::CV_r_VkBatchResourceBarriers;
int CRendererCVars::CV_r_VkHardwareComputeQueue;
int CRendererCVars::CV_r_VkHardwareCopyQueue;
int CRendererCVars::CV_r_ReprojectOnlyStaticObjects;
int CRendererCVars::CV_r_ReadZBufferDirectlyFromVMEM;
int CRendererCVars::CV_r_FlushToGPU;

int CRendererCVars::CV_r_EnableDebugLayer;
int CRendererCVars::CV_r_NoDraw;
int CRendererCVars::CV_r_UpdateInstances;

AllocateConstIntCVar(CRendererCVars, CV_r_multithreaded);
AllocateConstIntCVar(CRendererCVars, CV_r_multithreadedDrawing);
AllocateConstIntCVar(CRendererCVars, CV_r_multithreadedDrawingMinJobSize);
int CRendererCVars::CV_r_multigpu;
AllocateConstIntCVar(CRendererCVars, CV_r_validateDraw);
AllocateConstIntCVar(CRendererCVars, CV_r_texturecompiling);
AllocateConstIntCVar(CRendererCVars, CV_r_texturecompilingIndicator);
AllocateConstIntCVar(CRendererCVars, CV_r_texturesstreaming);
AllocateConstIntCVar(CRendererCVars, CV_r_TexturesStreamingDebug);
AllocateConstIntCVar(CRendererCVars, CV_r_texturesstreamingnoupload);
AllocateConstIntCVar(CRendererCVars, CV_r_texturesstreamingonlyvideo);
AllocateConstIntCVar(CRendererCVars, CV_r_texturesstreamingUpdateType);
AllocateConstIntCVar(CRendererCVars, CV_r_texturesstreamingPrecacheRounds);
AllocateConstIntCVar(CRendererCVars, CV_r_texturesstreamingSuppress);
AllocateConstIntCVar(CRendererCVars, CV_r_texturesstreamingPostponeMips);
AllocateConstIntCVar(CRendererCVars, CV_r_texturesstreamingPostponeThresholdKB);
AllocateConstIntCVar(CRendererCVars, CV_r_texturesstreamingPostponeThresholdMip);
AllocateConstIntCVar(CRendererCVars, CV_r_texturesstreamingMinReadSizeKB);
AllocateConstIntCVar(CRendererCVars, CV_r_TexturesStreamingLowestPrefetchBias);
AllocateConstIntCVar(CRendererCVars, CV_r_TexturesStreamingMaxUpdateRate);
int CRendererCVars::CV_r_texturesstreamingSkipMips;
int CRendererCVars::CV_r_texturesstreamingMinUsableMips;
int CRendererCVars::CV_r_texturesstreamingJobUpdate;
#if defined(TEXSTRM_DEFERRED_UPLOAD)
int CRendererCVars::CV_r_texturesstreamingDeferred;
#endif
#if defined(SUPPORTS_INPLACE_TEXTURE_STREAMING)
int CRendererCVars::CV_r_texturesstreamingInPlace;
#endif
float CRendererCVars::CV_r_rain_maxviewdist;
float CRendererCVars::CV_r_rain_maxviewdist_deferred;
float CRendererCVars::CV_r_measureoverdrawscale;
AllocateConstIntCVar(CRendererCVars, CV_r_texturesstreamingmipfading);
int CRendererCVars::CV_r_TexturesStreamPoolSize;
int CRendererCVars::CV_r_TexturesStreamPoolSecondarySize;
int CRendererCVars::CV_r_texturesstreampooldefragmentation;
int CRendererCVars::CV_r_texturesstreampooldefragmentationmaxmoves;
int CRendererCVars::CV_r_texturesstreampooldefragmentationmaxamount;
int CRendererCVars::CV_r_rendertargetpoolsize;
float CRendererCVars::CV_r_TexturesStreamingMaxRequestedMB;
int CRendererCVars::CV_r_TexturesStreamingMaxRequestedJobs;
float CRendererCVars::CV_r_TexturesStreamingMipBias;
int CRendererCVars::CV_r_TexturesStreamingMipClampDVD;
int CRendererCVars::CV_r_TexturesStreamingDisableNoStreamDuringLoad;
float CRendererCVars::CV_r_TextureLodDistanceRatio;

int CRendererCVars::CV_r_buffer_banksize;
int CRendererCVars::CV_r_constantbuffer_banksize;
int CRendererCVars::CV_r_constantbuffer_watermark;
int CRendererCVars::CV_r_buffer_sli_workaround;
int CRendererCVars::CV_r_transient_pool_size;
AllocateConstIntCVar(CRendererCVars, CV_r_buffer_enable_lockless_updates);
AllocateConstIntCVar(CRendererCVars, CV_r_enable_full_gpu_sync);
int CRendererCVars::CV_r_buffer_pool_max_allocs;
int CRendererCVars::CV_r_buffer_pool_defrag_static;
int CRendererCVars::CV_r_buffer_pool_defrag_dynamic;
int CRendererCVars::CV_r_buffer_pool_defrag_max_moves;

int CRendererCVars::CV_r_dyntexmaxsize;
int CRendererCVars::CV_r_dyntexatlascloudsmaxsize;
int CRendererCVars::CV_r_dyntexatlasspritesmaxsize;
int CRendererCVars::CV_r_dyntexatlasvoxterrainsize;
int CRendererCVars::CV_r_dyntexatlasdyntexsrcsize;

AllocateConstIntCVar(CRendererCVars, CV_r_texpostponeloading);
AllocateConstIntCVar(CRendererCVars, CV_r_texpreallocateatlases);
int CRendererCVars::CV_r_texatlassize;
int CRendererCVars::CV_r_texminanisotropy;
int CRendererCVars::CV_r_texmaxanisotropy;
AllocateConstIntCVar(CRendererCVars, CV_r_texlog);
AllocateConstIntCVar(CRendererCVars, CV_r_texnoload);

AllocateConstIntCVar(CRendererCVars, CV_r_debugrendermode);
AllocateConstIntCVar(CRendererCVars, CV_r_debugrefraction);

int CRendererCVars::CV_r_DeferredShadingTiled;
int CRendererCVars::CV_r_DeferredShadingTiledDebug;
int CRendererCVars::CV_r_DeferredShadingTiledHairQuality;
int CRendererCVars::CV_r_DeferredShadingSSS;
int CRendererCVars::CV_r_DeferredShadingFilterGBuffer;

int CRendererCVars::CV_r_UsePersistentRTForModelHUD;

AllocateConstIntCVar(CRendererCVars, CV_r_deferredshadingLightVolumes);
AllocateConstIntCVar(CRendererCVars, CV_r_deferredDecals);
AllocateConstIntCVar(CRendererCVars, CV_r_deferredDecalsDebug);

AllocateConstIntCVar(CRendererCVars, CV_r_deferredshadingDBTstencil);
AllocateConstIntCVar(CRendererCVars, CV_r_DeferredShadingScissor);
AllocateConstIntCVar(CRendererCVars, CV_r_DeferredShadingLBuffersFmt);
AllocateConstIntCVar(CRendererCVars, CV_r_DeferredShadingDepthBoundsTest);
AllocateConstIntCVar(CRendererCVars, CV_r_DeferredShadingDebugGBuffer);
int CRendererCVars::CV_r_DeferredShadingAmbient;
AllocateConstIntCVar(CRendererCVars, CV_r_DeferredShadingEnvProbes);
AllocateConstIntCVar(CRendererCVars, CV_r_DeferredShadingAmbientLights);
AllocateConstIntCVar(CRendererCVars, CV_r_DeferredShadingLights);
AllocateConstIntCVar(CRendererCVars, CV_r_DeferredShadingAreaLights);
AllocateConstIntCVar(CRendererCVars, CV_r_DeferredShadingStencilPrepass);
AllocateConstIntCVar(CRendererCVars, CV_r_CBufferUseNativeDepth);

float CRendererCVars::CV_r_DeferredShadingLightLodRatio;
float CRendererCVars::CV_r_DeferredShadingLightStencilRatio;

int CRendererCVars::CV_r_HDRSwapChain;
AllocateConstIntCVar(CRendererCVars, CV_r_HDRDebug);
int CRendererCVars::CV_r_HDRBloom;
int CRendererCVars::CV_r_HDRBloomQuality;
int CRendererCVars::CV_r_HDRVignetting;
AllocateConstIntCVar(CRendererCVars, CV_r_HDRTexFormat);
AllocateConstIntCVar(CRendererCVars, CV_r_HDRRangeAdapt);

float CRendererCVars::CV_r_HDRRangeAdaptMax;
float CRendererCVars::CV_r_HDRRangeAdaptMaxRange;
float CRendererCVars::CV_r_HDRRangeAdaptLBufferMaxRange;
float CRendererCVars::CV_r_HDRRangeAdaptLBufferMax;

float CRendererCVars::CV_r_HDREyeAdaptationSpeed;
int CRendererCVars::CV_r_HDREyeAdaptationMode;

float CRendererCVars::CV_r_HDRRangeAdaptationSpeed;
float CRendererCVars::CV_r_HDRGrainAmount;
AllocateConstIntCVar(CRendererCVars, CV_r_GrainEnableExposureThreshold);

float CRendererCVars::CV_r_Sharpening;
float CRendererCVars::CV_r_ChromaticAberration;

AllocateConstIntCVar(CRendererCVars, CV_r_geominstancing);
AllocateConstIntCVar(CRendererCVars, CV_r_geominstancingdebug);
AllocateConstIntCVar(CRendererCVars, CV_r_materialsbatching);
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_RENDERER_GNM
int CRendererCVars::CV_r_SilhouettePOM;
#endif
#ifdef WATER_TESSELLATION_RENDERER
int CRendererCVars::CV_r_WaterTessellationHW;
#endif
int CRendererCVars::CV_r_tessellationdebug;
float CRendererCVars::CV_r_tessellationtrianglesize;
float CRendererCVars::CV_r_displacementfactor;

int CRendererCVars::CV_r_batchtype;
int CRendererCVars::CV_r_geominstancingthreshold;

AllocateConstIntCVar(CRendererCVars, CV_r_DebugLightVolumes);

float CRendererCVars::CV_r_ShadowsBias;
float CRendererCVars::CV_r_ShadowsAdaptionRangeClamp;
float CRendererCVars::CV_r_ShadowsAdaptionSize;
float CRendererCVars::CV_r_ShadowsAdaptionMin;
float CRendererCVars::CV_r_ShadowsParticleKernelSize;
float CRendererCVars::CV_r_ShadowsParticleJitterAmount;
float CRendererCVars::CV_r_ShadowsParticleAnimJitterAmount;
float CRendererCVars::CV_r_ShadowsParticleNormalEffect;

AllocateConstIntCVar(CRendererCVars, CV_r_shadowtexformat);
AllocateConstIntCVar(CRendererCVars, CV_r_ShadowsMask);
AllocateConstIntCVar(CRendererCVars, CV_r_ShadowsMaskResolution);
AllocateConstIntCVar(CRendererCVars, CV_r_ShadowMaskStencilPrepass);
int CRendererCVars::CV_r_ShadowsPCFiltering;
float CRendererCVars::CV_r_shadow_jittering;
int CRendererCVars::CV_r_ShadowPoolMaxTimeslicedUpdatesPerFrame;
int CRendererCVars::CV_r_ShadowCastingLightsMaxCount;
AllocateConstIntCVar(CRendererCVars, CV_r_ShadowsGridAligned);
AllocateConstIntCVar(CRendererCVars, CV_r_ShadowMapsUpdate);
AllocateConstIntCVar(CRendererCVars, CV_r_ShadowGenDepthClip);
AllocateConstIntCVar(CRendererCVars, CV_r_ShadowPoolMaxFrames);
int CRendererCVars::CV_r_ShadowsCache;
int CRendererCVars::CV_r_ShadowsCacheFormat;
int CRendererCVars::CV_r_ShadowsScreenSpace;
float CRendererCVars::CV_r_ShadowsScreenSpaceLength;
int CRendererCVars::CV_r_ShadowsNearestMapResolution;
int CRendererCVars::CV_r_HeightMapAO;
float CRendererCVars::CV_r_HeightMapAOAmount;
float CRendererCVars::CV_r_HeightMapAORange;
float CRendererCVars::CV_r_HeightMapAOResolution;
float CRendererCVars::CV_r_RenderMeshHashGridUnitSize;

AllocateConstIntCVar(CRendererCVars, CV_r_debuglights);
AllocateConstIntCVar(CRendererCVars, CV_r_lightssinglepass);

AllocateConstIntCVar(CRendererCVars, CV_r_shaderslazyunload);
AllocateConstIntCVar(CRendererCVars, CV_r_shadersdebug);
AllocateConstIntCVar(CRendererCVars, CV_r_shadersCompileStrict);
AllocateConstIntCVar(CRendererCVars, CV_r_shadersCompileCompatible);
#if CRY_PLATFORM_DESKTOP
ICVar*      CRendererCVars::CV_r_ShaderTarget;
int         CRendererCVars::ShaderTargetFlag;
#endif

ICVar*      CRendererCVars::CV_r_VkShaderCompiler = nullptr;

AllocateConstIntCVar(CRendererCVars, CV_r_shadersignoreincludeschanging);
int CRendererCVars::CV_r_shadersAllowCompilation;
AllocateConstIntCVar(CRendererCVars, CV_r_shadersediting);
AllocateConstIntCVar(CRendererCVars, CV_r_shaderscompileautoactivate);
int CRendererCVars::CV_r_shadersremotecompiler;
int CRendererCVars::CV_r_shadersasynccompiling;
int CRendererCVars::CV_r_shadersasyncactivation;
int CRendererCVars::CV_r_shadersasyncmaxthreads;
int CRendererCVars::CV_r_shaderscachedeterministic;
int CRendererCVars::CV_r_ShadersCachePrecacheAll;
AllocateConstIntCVar(CRendererCVars, CV_r_shadersprecachealllights);
AllocateConstIntCVar(CRendererCVars, CV_r_ReflectTextureSlots);
int CRendererCVars::CV_r_shaderssubmitrequestline;
int CRendererCVars::CV_r_shaderslogcachemisses;
int CRendererCVars::CV_r_shadersImport;
int CRendererCVars::CV_r_shadersExport;

AllocateConstIntCVar(CRendererCVars, CV_r_meshprecache);
int CRendererCVars::CV_r_meshpoolsize;
int CRendererCVars::CV_r_meshinstancepoolsize;

AllocateConstIntCVar(CRendererCVars, CV_r_ZPassDepthSorting);
float CRendererCVars::CV_r_ZPrepassMaxDist;
int CRendererCVars::CV_r_usezpass;

AllocateConstIntCVar(CRendererCVars, CV_r_TransparentPasses);
int CRendererCVars::CV_r_TranspDepthFixup;
AllocateConstIntCVar(CRendererCVars, CV_r_SkipAlphaTested);
AllocateConstIntCVar(CRendererCVars, CV_r_usehwskinning);
AllocateConstIntCVar(CRendererCVars, CV_r_usemateriallayers);
int CRendererCVars::CV_r_ParticlesSoftIsec;
int CRendererCVars::CV_r_ParticlesRefraction;
int CRendererCVars::CV_r_ParticlesHalfRes;
AllocateConstIntCVar(CRendererCVars, CV_r_ParticlesHalfResAmount);
AllocateConstIntCVar(CRendererCVars, CV_r_ParticlesHalfResBlendMode);
AllocateConstIntCVar(CRendererCVars, CV_r_ParticlesInstanceVertices);
float CRendererCVars::CV_r_ParticlesAmountGI;

int CRendererCVars::CV_r_AntialiasingMode_CB;
int CRendererCVars::CV_r_AntialiasingMode;
int CRendererCVars::CV_r_AntialiasingModeSCull;
int CRendererCVars::CV_r_AntialiasingTAAPattern;
float CRendererCVars::CV_r_AntialiasingTAASharpening;
float CRendererCVars::CV_r_AntialiasingTAAFalloffHiFreq;
float CRendererCVars::CV_r_AntialiasingTAAFalloffLowFreq;
float CRendererCVars::CV_r_AntialiasingTSAAMipBias;
float CRendererCVars::CV_r_AntialiasingTSAASubpixelDetection;
float CRendererCVars::CV_r_AntialiasingTSAASmoothness;
AllocateConstIntCVar(CRendererCVars, CV_r_AntialiasingModeDebug);
AllocateConstIntCVar(CRendererCVars, CV_r_AntialiasingModeEditor);

int CRendererCVars::CV_r_MotionVectors;
int CRendererCVars::CV_r_MotionBlur;
int CRendererCVars::CV_r_MotionBlurQuality;
int CRendererCVars::CV_r_MotionBlurGBufferVelocity;
float CRendererCVars::CV_r_MotionBlurThreshold;
int CRendererCVars::CV_r_UseMergedPosts;
int CRendererCVars::CV_r_MaxFrameLatency;
float CRendererCVars::CV_r_MotionBlurShutterSpeed;
float CRendererCVars::CV_r_MotionBlurCameraMotionScale;
float CRendererCVars::CV_r_MotionBlurMaxViewDist;

float CRendererCVars::CV_r_cloakLightScale;
float CRendererCVars::CV_r_cloakTransitionLightScale;
int CRendererCVars::CV_r_cloakFadeByDist;
float CRendererCVars::CV_r_cloakFadeLightScale;
float CRendererCVars::CV_r_cloakFadeStartDistSq;
float CRendererCVars::CV_r_cloakFadeEndDistSq;
float CRendererCVars::CV_r_cloakFadeMinValue;
int CRendererCVars::CV_r_cloakRefractionFadeByDist;
float CRendererCVars::CV_r_cloakRefractionFadeStartDistSq;
float CRendererCVars::CV_r_cloakRefractionFadeEndDistSq;
float CRendererCVars::CV_r_cloakRefractionFadeMinValue;
float CRendererCVars::CV_r_cloakMinLightValue;
float CRendererCVars::CV_r_cloakHeatScale;
int CRendererCVars::CV_r_cloakRenderInThermalVision;
float CRendererCVars::CV_r_cloakMinAmbientOutdoors;
float CRendererCVars::CV_r_cloakMinAmbientIndoors;
float CRendererCVars::CV_r_cloakSparksAlpha;
float CRendererCVars::CV_r_cloakInterferenceSparksAlpha;
float CRendererCVars::CV_r_cloakHighlightStrength;
float CRendererCVars::CV_r_armourPulseSpeedMultiplier;
float CRendererCVars::CV_r_maxSuitPulseSpeedMultiplier;

AllocateConstIntCVar(CRendererCVars, CV_r_customvisions);
AllocateConstIntCVar(CRendererCVars, CV_r_DebugLayerEffect);
AllocateConstIntCVar(CRendererCVars, CV_r_VrProjectionType);
AllocateConstIntCVar(CRendererCVars, CV_r_VrProjectionPreset);
AllocateConstIntCVar(CRendererCVars, CV_r_stereoMirrorProjection);

int CRendererCVars::CV_r_snow;
AllocateConstIntCVar(CRendererCVars, CV_r_snow_halfres);
AllocateConstIntCVar(CRendererCVars, CV_r_snow_displacement);
AllocateConstIntCVar(CRendererCVars, CV_r_snowFlakeClusters);

int CRendererCVars::CV_r_rain;
int CRendererCVars::CV_r_rain_ignore_nearest;
float CRendererCVars::CV_r_rainamount;
float CRendererCVars::CV_r_rainDistMultiplier;
float CRendererCVars::CV_r_rainOccluderSizeTreshold;

int CRendererCVars::CV_r_SSReflections;
int CRendererCVars::CV_r_SSReflHalfRes;
int CRendererCVars::CV_r_SSReflSamples;
float CRendererCVars::CV_r_SSReflDistance;
int CRendererCVars::CV_r_ssdo;
int CRendererCVars::CV_r_ssdoHalfRes;
int CRendererCVars::CV_r_ssdoColorBleeding;
float CRendererCVars::CV_r_ssdoRadius;
float CRendererCVars::CV_r_ssdoRadiusMin;
float CRendererCVars::CV_r_ssdoRadiusMax;
float CRendererCVars::CV_r_ssdoAmountDirect;
float CRendererCVars::CV_r_ssdoAmountAmbient;
float CRendererCVars::CV_r_ssdoAmountReflection;

int CRendererCVars::CV_r_dof;
int CRendererCVars::CV_r_DofMode;
int CRendererCVars::CV_r_DofBokehQuality;

//AllocateConstIntCVar(CRendererCVars, CV_r_measureoverdraw);
AllocateConstIntCVar(CRendererCVars, CV_r_printmemoryleaks);
AllocateConstIntCVar(CRendererCVars, CV_r_releaseallresourcesonexit);

int CRendererCVars::CV_r_rc_autoinvoke;

int CRendererCVars::CV_r_Refraction;
int CRendererCVars::CV_r_sunshafts;

AllocateConstIntCVar(CRendererCVars, CV_r_SonarVision);
AllocateConstIntCVar(CRendererCVars, CV_r_ThermalVision);
float CRendererCVars::CV_r_ThermalVisionViewDistance;
AllocateConstIntCVar(CRendererCVars, CV_r_ThermalVisionViewCloakFrequencyPrimary);
AllocateConstIntCVar(CRendererCVars, CV_r_ThermalVisionViewCloakFrequencySecondary);
float CRendererCVars::CV_r_ThermalVisionViewCloakFlickerMinIntensity;
float CRendererCVars::CV_r_ThermalVisionViewCloakFlickerMaxIntensity;

AllocateConstIntCVar(CRendererCVars, CV_r_MergeShadowDrawcalls);

AllocateConstIntCVar(CRendererCVars, CV_r_PostProcessParamsBlending);
int CRendererCVars::CV_r_PostProcessReset;
int CRendererCVars::CV_r_PostProcessFilters;
AllocateConstIntCVar(CRendererCVars, CV_r_PostProcessGameFx);
float CRendererCVars::CV_r_PostprocessParamsBlendingTimeScale;
AllocateConstIntCVar(CRendererCVars, CV_r_PostProcessHUD3D);
AllocateConstIntCVar(CRendererCVars, CV_r_PostProcessHUD3DDebugView);
int CRendererCVars::CV_r_PostProcessHUD3DCache;
float CRendererCVars::CV_r_PostProcessHUD3DShadowAmount;
float CRendererCVars::CV_r_PostProcessHUD3DGlowAmount;
AllocateConstIntCVar(CRendererCVars, CV_r_PostProcessHUD3DStencilClear);
AllocateConstIntCVar(CRendererCVars, CV_r_PostProcessNanoGlassDebugView);

int CRendererCVars::CV_r_colorRangeCompression;

int CRendererCVars::CV_r_colorgrading;
int CRendererCVars::CV_r_colorgrading_selectivecolor;
AllocateConstIntCVar(CRendererCVars, CV_r_colorgrading_levels);
AllocateConstIntCVar(CRendererCVars, CV_r_colorgrading_filters);
int CRendererCVars::CV_r_colorgrading_charts;
int CRendererCVars::CV_r_ColorgradingChartsCache;

AllocateConstIntCVar(CRendererCVars, CV_r_showdyntextures);
int CRendererCVars::CV_r_ShowDynTexturesMaxCount;

ICVar* CRendererCVars::CV_r_ShowDynTexturesFilter;
AllocateConstIntCVar(CRendererCVars, CV_r_shownormals);
AllocateConstIntCVar(CRendererCVars, CV_r_showlines);
float CRendererCVars::CV_r_normalslength;
AllocateConstIntCVar(CRendererCVars, CV_r_showtangents);
AllocateConstIntCVar(CRendererCVars, CV_r_showtimegraph);
AllocateConstIntCVar(CRendererCVars, CV_r_DebugFontRendering);
AllocateConstIntCVar(CRendererCVars, CV_profileStreaming);
AllocateConstIntCVar(CRendererCVars, CV_r_graphstyle);
AllocateConstIntCVar(CRendererCVars, CV_r_showbufferusage);

ICVar* CRendererCVars::CV_r_ShaderCompilerServer;
ICVar* CRendererCVars::CV_r_ShaderCompilerFolderName;
ICVar* CRendererCVars::CV_r_ShaderCompilerFolderSuffix;
ICVar* CRendererCVars::CV_r_ShaderEmailTags;
ICVar* CRendererCVars::CV_r_ShaderEmailCCs;
int CRendererCVars::CV_r_ShaderCompilerPort;
int CRendererCVars::CV_r_ShaderCompilerDontCache;
int CRendererCVars::CV_r_flares = FLARES_DEFAULT_VAL;
int CRendererCVars::CV_r_flareHqShafts;
float CRendererCVars::CV_r_FlaresChromaShift;
int CRendererCVars::CV_r_FlaresIrisShaftMaxPolyNum;
int CRendererCVars::CV_r_FlaresEnableColorGrading;
float CRendererCVars::CV_r_FlaresTessellationRatio;

int CRendererCVars::CV_r_envcmresolution;
int CRendererCVars::CV_r_envtexresolution;
float CRendererCVars::CV_r_waterupdateFactor;
float CRendererCVars::CV_r_waterupdateDistance;
float CRendererCVars::CV_r_envtexupdateinterval;
int CRendererCVars::CV_r_waterreflections;
AllocateConstIntCVar(CRendererCVars, CV_r_waterreflections_mgpu);
AllocateConstIntCVar(CRendererCVars, CV_r_waterreflections_quality);
AllocateConstIntCVar(CRendererCVars, CV_r_waterreflections_use_min_offset);
float CRendererCVars::CV_r_waterreflections_min_visible_pixels_update;
float CRendererCVars::CV_r_waterreflections_minvis_updatefactormul;
float CRendererCVars::CV_r_waterreflections_minvis_updatedistancemul;
int CRendererCVars::CV_r_watercaustics;
float CRendererCVars::CV_r_watercausticsdistance;
int CRendererCVars::CV_r_watercausticsdeferred;
int CRendererCVars::CV_r_watervolumecaustics;
int CRendererCVars::CV_r_watervolumecausticsdensity;
int CRendererCVars::CV_r_watervolumecausticsresolution;
float CRendererCVars::CV_r_watervolumecausticssnapfactor;
float CRendererCVars::CV_r_watervolumecausticsmaxdistance;
int CRendererCVars::CV_r_water_godrays;
float CRendererCVars::CV_r_water_godrays_distortion;
int CRendererCVars::CV_r_WaterUpdateThread;
int CRendererCVars::CV_r_texNoAnisoAlphaTest;
int CRendererCVars::CV_r_reflections;
int CRendererCVars::CV_r_reflections_quality;
float CRendererCVars::CV_r_waterreflections_offset;
AllocateConstIntCVar(CRendererCVars, CV_r_reloadshaders);
AllocateConstIntCVar(CRendererCVars, CV_r_detailtextures);
float CRendererCVars::CV_r_detaildistance;
AllocateConstIntCVar(CRendererCVars, CV_r_texbindmode);

int CRendererCVars::CV_r_nodrawnear;
float CRendererCVars::CV_r_DrawNearZRange;
float CRendererCVars::CV_r_DrawNearFarPlane;
float CRendererCVars::CV_r_drawnearfov;
int CRendererCVars::CV_r_DrawNearShadows;
ICVar* CRendererCVars::CV_r_excludeshader;
ICVar* CRendererCVars::CV_r_excludemesh;

float CRendererCVars::CV_r_gamma;
float CRendererCVars::CV_r_contrast;
float CRendererCVars::CV_r_brightness;

AllocateConstIntCVar(CRendererCVars, CV_r_nohwgamma);

int CRendererCVars::CV_r_scissor;

AllocateConstIntCVar(CRendererCVars, CV_r_wireframe);
int CRendererCVars::CV_r_GetScreenShot;

AllocateConstIntCVar(CRendererCVars, CV_r_character_nodeform);

AllocateConstIntCVar(CRendererCVars, CV_r_VegetationSpritesGenDebug);
AllocateConstIntCVar(CRendererCVars, CV_r_VegetationSpritesMaxLightingUpdates);
AllocateConstIntCVar(CRendererCVars, CV_r_ZPassOnly);
AllocateConstIntCVar(CRendererCVars, CV_r_VegetationSpritesNoGen);
int CRendererCVars::CV_r_VegetationSpritesTexRes;
AllocateConstIntCVar(CRendererCVars, CV_r_VegetationSpritesGenAlways);

int CRendererCVars::CV_r_ShowVideoMemoryStats;

ICVar* CRendererCVars::CV_r_ShowTexture = NULL;
ICVar* CRendererCVars::CV_r_TexturesStreamingDebugfilter = NULL;
int CRendererCVars::CV_r_TexturesStreamingDebugMinSize;
int CRendererCVars::CV_r_TexturesStreamingDebugMinMip;
AllocateConstIntCVar(CRendererCVars, CV_r_TexturesStreamingDebugDumpIntoLog);

AllocateConstIntCVar(CRendererCVars, CV_r_ShowLightBounds);

AllocateConstIntCVar(CRendererCVars, CV_r_TextureCompressor);

int CRendererCVars::CV_r_ParticlesTessellation;
int CRendererCVars::CV_r_ParticlesTessellationTriSize;

float CRendererCVars::CV_r_ZFightingDepthScale;
float CRendererCVars::CV_r_ZFightingExtrude;

float CRendererCVars::CV_r_TexelsPerMeter;

int CRendererCVars::CV_r_ConditionalRendering;
int CRendererCVars::CV_r_enableAltTab;
int CRendererCVars::CV_r_StereoFlipEyes;
int CRendererCVars::CV_r_StereoEnableMgpu;
float CRendererCVars::CV_r_stereoScaleCoefficient;
float CRendererCVars::CV_r_StereoStrength;
float CRendererCVars::CV_r_StereoEyeDist;
float CRendererCVars::CV_r_StereoScreenDist;
float CRendererCVars::CV_r_StereoNearGeoScale;
float CRendererCVars::CV_r_StereoHudScreenDist;
float CRendererCVars::CV_r_StereoGammaAdjustment;
int CRendererCVars::CV_r_ConsoleBackbufferWidth;
int CRendererCVars::CV_r_ConsoleBackbufferHeight;

int CRendererCVars::CV_r_CustomResMaxSize;
int CRendererCVars::CV_r_CustomResWidth;
int CRendererCVars::CV_r_CustomResHeight;
int CRendererCVars::CV_r_CustomResPreview;
int CRendererCVars::CV_r_Supersampling;
int CRendererCVars::CV_r_SupersamplingFilter;

float CRendererCVars::CV_r_FogDepthTest;
#if defined(VOLUMETRIC_FOG_SHADOWS)
int CRendererCVars::CV_r_FogShadows;
int CRendererCVars::CV_r_FogShadowsMode;
#endif
int CRendererCVars::CV_r_FogShadowsWater;

int CRendererCVars::CV_r_RainDropsEffect;

AllocateConstIntCVar(CRendererCVars, CV_r_RefractionPartialResolveMode);
AllocateConstIntCVar(CRendererCVars, CV_r_RefractionPartialResolveMinimalResolveArea);
AllocateConstIntCVar(CRendererCVars, CV_r_RefractionPartialResolveMaxResolveCount);
AllocateConstIntCVar(CRendererCVars, CV_r_RefractionPartialResolvesDebug);

AllocateConstIntCVar(CRendererCVars, CV_r_Batching);

AllocateConstIntCVar(CRendererCVars, CV_r_Unlit);
AllocateConstIntCVar(CRendererCVars, CV_r_HideSunInCubemaps);
AllocateConstIntCVar(CRendererCVars, CV_r_CubemapGenerationTimeout);

AllocateConstIntCVar(CRendererCVars, CV_r_ParticlesDebug);

float CRendererCVars::CV_r_FlashMatTexResQuality;

int CRendererCVars::CV_r_DynTexSourceSharedRTWidth;
int CRendererCVars::CV_r_DynTexSourceSharedRTHeight;
int CRendererCVars::CV_r_DynTexSourceUseSharedRT;

#if defined(ENABLE_RENDER_AUX_GEOM)
int CRendererCVars::CV_r_enableauxgeom;
#endif

int CRendererCVars::CV_r_ParticleVerticePoolSize;
int CRendererCVars::CV_r_ParticleMaxVerticePoolSize;
int CRendererCVars::CV_r_GeomCacheInstanceThreshold;
int CRendererCVars::CV_r_VisAreaClipLightsPerPixel;

int CRendererCVars::CV_r_VolumetricFogTexScale;
int CRendererCVars::CV_r_VolumetricFogTexDepth;
float CRendererCVars::CV_r_VolumetricFogReprojectionBlendFactor;
int CRendererCVars::CV_r_VolumetricFogSample;
int CRendererCVars::CV_r_VolumetricFogShadow;
int CRendererCVars::CV_r_VolumetricFogDownscaledSunShadow;
int CRendererCVars::CV_r_VolumetricFogDownscaledSunShadowRatio;
int CRendererCVars::CV_r_VolumetricFogReprojectionMode;
float CRendererCVars::CV_r_VolumetricFogMinimumLightBulbSize;
int CRendererCVars::CV_r_VolumetricFogSunLightCorrection;

int CRendererCVars::CV_r_GpuParticles;
int CRendererCVars::CV_r_GpuParticlesConstantRadiusBoundingBoxes;
int CRendererCVars::CV_r_GpuPhysicsFluidDynamicsDebug;

int CRendererCVars::CV_r_VolumetricClouds;
int CRendererCVars::CV_r_VolumetricCloudsRaymarchStepNum;
int CRendererCVars::CV_r_VolumetricCloudsPipeline;
int CRendererCVars::CV_r_VolumetricCloudsStereoReprojection;
int CRendererCVars::CV_r_VolumetricCloudsTemporalReprojection;
int CRendererCVars::CV_r_VolumetricCloudsShadowResolution;

Vec2 CRendererCVars::s_overscanBorders(0, 0);

int CRendererCVars::CV_d3d11_CBUpdateStats;
ICVar* CRendererCVars::CV_d3d11_forcedFeatureLevel;

#if defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)
ICVar* CRendererCVars::CV_d3d11_debugMuteSeverity;
ICVar* CRendererCVars::CV_d3d11_debugMuteMsgID;
ICVar* CRendererCVars::CV_d3d11_debugBreakOnMsgID;
int CRendererCVars::CV_d3d11_debugBreakOnce;
#endif

#if defined(CRY_PLATFORM_WINDOWS)
int CRendererCVars::CV_d3d11_preventDriverThreading;
#endif

//////////////////////////////////////////////////////////////////////////
// On CVar change callbacks
//////////////////////////////////////////////////////////////////////////
char* resourceName[] = {
	"UNKNOWN",
	"Surfaces",
	"Volumes",
	"Textures",
	"Volume Textures",
	"Cube Textures",
	"Vertex Buffers",
	"Index Buffers"
};

#if (CRY_RENDERER_DIRECT3D >= 120)
static void OnChange_CV_D3D12HardwareComputeQueue(ICVar* /*pCVar*/)
{
	gcpRendD3D->FlushRTCommands(true, true, true);

	reinterpret_cast<CCryDX12Device*>(gcpRendD3D->GetDevice().GetRealDevice())->FlushAndWaitForGPU();
	reinterpret_cast<CCryDX12DeviceContext*>(gcpRendD3D->GetDeviceContext().GetRealDeviceContext())->RecreateCommandListPool(CMDQUEUE_COMPUTE);
}

static void OnChange_CV_D3D12HardwareCopyQueue(ICVar* /*pCVar*/)
{
	gcpRendD3D->FlushRTCommands(true, true, true);

	reinterpret_cast<CCryDX12Device*>(gcpRendD3D->GetDevice().GetRealDevice())->FlushAndWaitForGPU();
	reinterpret_cast<CCryDX12DeviceContext*>(gcpRendD3D->GetDeviceContext().GetRealDeviceContext())->RecreateCommandListPool(CMDQUEUE_COPY);
}
#endif

#if defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)
static void OnChange_CV_d3d11_debugMuteMsgID(ICVar* /*pCVar*/)
{
	gcpRendD3D->m_bUpdateD3DDebug = true;
}
#endif

#if CRY_PLATFORM_DESKTOP
static void OnChange_CV_r_ShaderTarget(ICVar* pCVar)
{
	if (!pCVar)
		return;

	std::string r_driverStr = gEnv->pConsole->GetCVar("r_driver")->GetString();
	std::string shaderTargetStr(pCVar->GetString());
	auto toUpperLamda = [](unsigned char c) -> unsigned char { return std::toupper(c); };
	std::transform(shaderTargetStr.begin(), shaderTargetStr.end(), shaderTargetStr.begin(), toUpperLamda);
	std::transform(r_driverStr.begin(), r_driverStr.end(), r_driverStr.begin(), toUpperLamda);

#if CRY_PLATFORM_ANDROID || CRY_PLATFORM_LINUX || CRY_PLATFORM_APPLE
	if (strcmp(pCVar->GetString(), STR_GL4_SHADER_TARGET) && strcmp(pCVar->GetString(), STR_VULKAN_SHADER_TARGET))
	{
		pCVar->Set(STR_VULKAN_SHADER_TARGET);
		return;
	}
#elif CRY_PLATFORM_DURANGO
	if(strcmp(pCVar->GetString(), STR_DURANGO_SHADER_TARGET))
	{
		pCVar->Set(STR_DURANGO_SHADER_TARGET);
		return;
	}
#elif CRY_PLATFORM_ORBIS
	if (strcmp(pCVar->GetString(), STR_ORBIS_SHADER_TARGET))
	{
		pCVar->Set(STR_ORBIS_SHADER_TARGET);
		return;
	}
#elif CRY_PLATFORM_WINDOWS
	// Keep the shader target value
#else
	if (strcmp(pCVar->GetString(), STR_VULKAN_SHADER_TARGET))
	{
		pCVar->Set(STR_VULKAN_SHADER_TARGET);
		return;
	}
#endif

	if (shaderTargetStr == "")
	{
		CRenderer::ShaderTargetFlag = -1;
	}
	else if (shaderTargetStr == STR_ORBIS_SHADER_TARGET)
	{
		CRenderer::ShaderTargetFlag = SF_ORBIS;
		if(r_driverStr != STR_GNM_RENDERER && r_driverStr != STR_DX11_RENDERER)
			CryFatalError("r_driver MUST match shader target flag.");
	}
	else if (shaderTargetStr == STR_DURANGO_SHADER_TARGET)
	{
		CRenderer::ShaderTargetFlag = SF_DURANGO;
		if (r_driverStr != STR_DX11_RENDERER && r_driverStr != STR_DX12_RENDERER)
			CryFatalError("r_driver MUST match shader target flag.");
	}
	else if (shaderTargetStr == STR_D3D11_SHADER_TARGET)
	{
		CRenderer::ShaderTargetFlag = SF_D3D11;
		if (r_driverStr != STR_DX11_RENDERER && r_driverStr != STR_DX12_RENDERER)
			CryFatalError("r_driver MUST match shader target flag.");
	}
	else if (shaderTargetStr == STR_GL4_SHADER_TARGET)
	{
		CRenderer::ShaderTargetFlag = SF_GL4;
		if (r_driverStr != STR_GL_RENDERER)
			CryFatalError("r_driver MUST match shader target flag.");
	}
	else if (shaderTargetStr == STR_GLES3_SHADER_TARGET)
	{
		CRenderer::ShaderTargetFlag = SF_GLES3;
		if (r_driverStr != STR_GL_RENDERER)
			CryFatalError("r_driver MUST match shader target flag.");
	}
	else if (shaderTargetStr == STR_VULKAN_SHADER_TARGET)
	{
		CRenderer::ShaderTargetFlag = SF_VULKAN;
		if (r_driverStr != STR_VK_RENDERER)
			CryFatalError("r_driver MUST match shader target flag.");
	}
	else
	{
		CryFatalError("Using %s as a shader target string is not allowed. Available valid options are %s/%s/%s/%s/%s/%s", 
			shaderTargetStr, 
			STR_ORBIS_SHADER_TARGET, 
			STR_DURANGO_SHADER_TARGET,
			STR_D3D11_SHADER_TARGET,
			STR_GL4_SHADER_TARGET,
			STR_GLES3_SHADER_TARGET,
			STR_VULKAN_SHADER_TARGET);
	}
}
#endif

static void OnChange_CV_r_PostProcess(ICVar* pCVar)
{
	if (!pCVar)
		return;

	if (gRenDev->m_pRT && !gRenDev->m_pRT->IsRenderThread())
		gRenDev->m_pRT->FlushAndWait();

	gRenDev->CV_r_PostProcess = pCVar->GetIVal();
}

// Track all AA conditions/dependencies in one place. Set corresponding cvars.
static void OnChange_CV_r_AntialiasingMode(ICVar* pCVar)
{
	if (!pCVar)
		return;

	if (gRenDev->m_pRT && !gRenDev->m_pRT->IsRenderThread())
		gRenDev->m_pRT->FlushAndWait();

	gRenDev->CV_r_AntialiasingMode = pCVar->GetIVal();

	int32 nVal = gRenDev->CV_r_AntialiasingMode;
	nVal = min(eAT_AAMODES_COUNT - 1, nVal);
	pCVar->Set(nVal);
	gRenDev->CV_r_AntialiasingMode = nVal;
}

static void OnChange_CV_r_ShadersAllowCompiliation(ICVar* pCVar)
{
	// disable async activation. Can be a problem though if some shader cache files were opened async/streamed
	// before this.
	CRenderer::CV_r_shadersasyncactivation = 0;
	CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Changing r_ShadersAllowCompilation at runtime can cause problems. Please set it in your system.cfg or user.cfg instead.");
}

static void OnChange_CV_r_MaxFrameLatency(ICVar* pCVar)
{
	int clampedLatency = clamp_tpl(pCVar->GetIVal(), 1, MAX_FRAME_LATENCY);
	pCVar->Set(clampedLatency);
}

static void OnChange_CV_r_FlaresTessellationRatio(ICVar* pCVar)
{
	gEnv->pOpticsManager->Invalidate();
}

static void GetLogVBuffersStatic(ICVar* pCVar)
{
	gRenDev->GetLogVBuffers();
}

static void OnChangeShadowJitteringCVar(ICVar* pCVar)
{
	gRenDev->SetShadowJittering(pCVar->GetFVal());
}

void CRendererCVars::OnChange_CachedShadows(ICVar* pCVar)
{
	if (gEnv->p3DEngine)  // 3DEngine not initialized during ShaderCacheGen
	{
		CRendererResources::CreateCachedShadowMaps();

		gEnv->p3DEngine->SetShadowsGSMCache(true);
		gEnv->p3DEngine->SetRecomputeCachedShadows(ShadowMapFrustum::ShadowCacheData::eFullUpdate);
		gEnv->p3DEngine->InvalidateShadowCacheData();
	}
}

void CRendererCVars::OnChange_GeomInstancingThreshold(ICVar* pVar)
{
	// get user value
	CRenderer::m_iGeomInstancingThreshold = CRendererCVars::CV_r_geominstancingthreshold;

	// auto
	if (CRenderer::m_iGeomInstancingThreshold < 0)
	{
		int nGPU = gRenDev->GetFeatures() & RFT_HW_MASK;

		if (nGPU == RFT_HW_ATI)
			CRenderer::m_iGeomInstancingThreshold = 2;        // seems to help in performance on all cards
		else if (nGPU == RFT_HW_NVIDIA)
			CRenderer::m_iGeomInstancingThreshold = 8;          //
		else
			CRenderer::m_iGeomInstancingThreshold = 7;          // might be a good start - can be tweaked further
	}

	iLog->Log(" Used GeomInstancingThreshold is %d", CRenderer::m_iGeomInstancingThreshold);
}
void CRendererCVars::Cmd_ShowRenderTarget(IConsoleCmdArgs* pArgs)
{
	if (gcpRendD3D)
	{
		gcpRendD3D->GetGraphicsPipeline().GetDebugRenderTargetsStage()->OnShowRenderTargetsCmd(pArgs);
	}
}

static void cmd_OverscanBorders(IConsoleCmdArgs* pParams)
{
	int argCount = pParams->GetArgCount();

	if (argCount > 1)
	{
		CRenderer::s_overscanBorders.x = clamp_tpl((float)atof(pParams->GetArg(1)), 0.0f, 25.0f) * 0.01f;

		if (argCount > 2)
		{
			CRenderer::s_overscanBorders.y = clamp_tpl((float)atof(pParams->GetArg(2)), 0.0f, 25.0f) * 0.01f;
		}
		else
		{
			CRenderer::s_overscanBorders.y = CRenderer::s_overscanBorders.x;
		}
	}
	else
	{
		gEnv->pLog->LogWithType(ILog::eInputResponse, "Overscan Borders: Left/Right %G %% , Top/Bottom %G %%",
		                        CRenderer::s_overscanBorders.x * 100.0f, CRenderer::s_overscanBorders.y * 100.0f);
	}
}

static void OnChange_r_OverscanBorderScale(ICVar* pCVar)
{
	const float maxOverscanBorderScale = 0.5f;
	CRenderer::s_overscanBorders.x = clamp_tpl(CRenderer::s_overscanBorders.x, 0.0f, maxOverscanBorderScale);
	CRenderer::s_overscanBorders.y = clamp_tpl(CRenderer::s_overscanBorders.y, 0.0f, maxOverscanBorderScale);
}

/// Used by console auto completion.
struct STextureNameAutoComplete : public IConsoleArgumentAutoComplete
{
	virtual int GetCount() const
	{
		SResourceContainer* pRC = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		if (!pRC)
			return 0;
		return pRC->m_RMap.size();
	}
	virtual const char* GetValue(int nIndex) const
	{
		SResourceContainer* pRC = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		if (!pRC || pRC->m_RMap.empty())
			return "";
		nIndex %= pRC->m_RMap.size();
		ResourcesMap::const_iterator it = pRC->m_RMap.begin();
		std::advance(it, nIndex);
		CTexture* pTex = (CTexture*)it->second;
		if (pTex)
			return pTex->GetSourceName();
		return "";
	}
} g_TextureNameAutoComplete;

//////////////////////////////////////////////////////////////////////

#if CRY_PLATFORM_DESKTOP

static void ShadersPrecacheList(IConsoleCmdArgs* Cmd)
{
	gRenDev->m_cEF.mfPrecacheShaders(false);
}
static void ShadersStatsList(IConsoleCmdArgs* Cmd)
{
	gRenDev->m_cEF.mfPrecacheShaders(true);
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CRendererCVars::InitCVars()
{
	REGISTER_CVAR3("r_GraphicsPipeline", CV_r_GraphicsPipeline, 3, VF_NULL,
	               "Toggles new optimized graphics pipeline\n"
	               "  0: Use legacy graphics pipeline\n"
	               "  1: Use new graphics pipeline with objects compiled on the fly\n"
	               "  2: Use new graphics pipeline with permanent render objects");

	REGISTER_CVAR3("r_GraphicsPipelineMobile", CV_r_GraphicsPipelineMobile, 0, VF_NULL,
	               "Run limited pipeline specifically optimized for mobile devices.\n"
								 "  1: Using compressed micro GBuffer\n"
								 "  2: Using standard GBuffer");

	REGISTER_CVAR3("r_GraphicsPipelinePassScheduler", CV_r_GraphicsPipelinePassScheduler, 0, VF_NULL,
	               "Toggles render pass scheduler that submits passes in a deferred way, allowing improved multithreading and barrier scheduling.");
	
#if CRY_PLATFORM_DESKTOP
	CV_r_ShaderTarget = REGISTER_STRING_CB("r_ShaderTarget", "", VF_DUMPTODISK,
			"Shader cache generation only CVar."
			"Sets the shader generation target ( Orbis/Durango/D3D11/GL4/GLES3/Vulkan ).\n"
			"Specify in system.cfg like this: r_ShaderTarget = \"D3D11\"", OnChange_CV_r_ShaderTarget);
		OnChange_CV_r_ShaderTarget(CV_r_ShaderTarget);
#endif
#if CRY_RENDERER_VULKAN
		CV_r_VkShaderCompiler = REGISTER_STRING("r_VkShaderCompiler", "DXC", VF_DUMPTODISK,
			"Vulkan renderer only CVar."
			"Sets the HLSL to SPIRV compiler to use for local/remote shader comilation ( HLSLCC/DXC/GLSLANG ).\n"
			"Specify in system.cfg like this: r_VkShaderCompiler = \"DXC\"");
#endif

	REGISTER_CVAR3("r_DeferredShadingTiled", CV_r_DeferredShadingTiled, 3, VF_DUMPTODISK,
	               "Toggles tile based shading.\n"
	               "0 - Off"
	               "1 - Tiled forward shading for transparent objects\n"
	               "2 - Tiled deferred and forward shading\n"
	               "3 - Tiled deferred and forward shading using volume rasterization for light list generation\n"
	               "4 - Tiled forward shading for opaque and transparent objects (using volume rasterization)\n");

	REGISTER_CVAR3("r_DeferredShadingTiledDebug", CV_r_DeferredShadingTiledDebug, 0, VF_DUMPTODISK,
	               "1 - Display debug info\n"
	               "2 - Light coverage visualization\n");

	REGISTER_CVAR3("r_DeferredShadingTiledHairQuality", CV_r_DeferredShadingTiledHairQuality, 2, VF_DUMPTODISK,
	               "Tiled shading hair quality\n"
	               "0 - Regular forward shading\n"
	               "1 - Tiled shading on selected assets and more accurate probe blending\n"
	               "2 - Full tiled shading with high quality shadow filter\n");

	REGISTER_CVAR3("r_DeferredShadingSSS", CV_r_DeferredShadingSSS, DEF_SHAD_SSS_DEFAULT_VAL, VF_DUMPTODISK,
	               "Toggles deferred subsurface scattering (requires full deferred shading)");

	REGISTER_CVAR3("r_DeferredShadingFilterGBuffer", CV_r_DeferredShadingFilterGBuffer, 0, VF_DUMPTODISK,
	               "Enables filtering of GBuffer to reduce specular aliasing.\n");

	DefineConstIntCVar3("r_DeferredShadingLightVolumes", CV_r_deferredshadingLightVolumes, 1, VF_DUMPTODISK,
	                    "Toggles Light volumes for deferred shading.\n"
	                    "Usage: r_DeferredShadingLightVolumes [0/1]\n"
	                    "Default is 1 (enabled)");

	DefineConstIntCVar3("r_DeferredDecals", CV_r_deferredDecals, 1, VF_DUMPTODISK,
	                    "Toggles deferred decals.\n"
	                    "Usage: r_DeferredDecals [0/1]\n"
	                    "Default is 1 (enabled), 0 Disabled. ");

	DefineConstIntCVar3("r_deferredDecalsDebug", CV_r_deferredDecalsDebug, 0, VF_DUMPTODISK,
	                    "Display decals debug info.\n"
	                    "Usage: r_deferredDecalsDebug [0/1]");

	DefineConstIntCVar3("r_DeferredShadingEnvProbes", CV_r_DeferredShadingEnvProbes, 1, VF_DUMPTODISK,
	                    "Toggles deferred environment probes rendering.\n"
	                    "Usage: r_DeferredShadingEnvProbes [0/1]\n"
	                    "Default is 1 (enabled)");

	DefineConstIntCVar3("r_DeferredShadingStencilPrepass", CV_r_DeferredShadingStencilPrepass, 1, VF_DUMPTODISK,
	                    "Toggles deferred shading stencil pre pass.\n"
	                    "Usage: r_DeferredShadingStencilPrepass [0/1]\n"
	                    "Default is 1 (enabled)");

	DefineConstIntCVar3("r_DeferredShadingScissor", CV_r_DeferredShadingScissor, 1, VF_DUMPTODISK,
	                    "Toggles deferred shading scissor test.\n"
	                    "Usage: r_DeferredShadingScissor [0/1]\n"
	                    "Default is 1 (enabled)");

	DefineConstIntCVar3("r_DeferredShadingLBuffersFmt", CV_r_DeferredShadingLBuffersFmt, 1, VF_NULL,
	                    "Toggles light buffers format.\n"
	                    "Usage: r_DeferredShadingLBuffersFmt [0/1]\n"
	                    "Default is 1 (R11G11B10F), 0: R16G16B16A16F");

	DefineConstIntCVar3("r_DeferredShadingDepthBoundsTest", CV_r_DeferredShadingDepthBoundsTest, DEF_SHAD_DBT_DEFAULT_VAL,
	                    VF_DUMPTODISK,
	                    "Toggles deferred shading depth bounds test.\n"
	                    "Usage: r_DeferredShadingDepthBoundsTest [0/1]\n"
	                    "Default is 1 (enabled)");

	DefineConstIntCVar3("r_DeferredShadingDBTstencil", CV_r_deferredshadingDBTstencil, DEF_SHAD_DBT_STENCIL_DEFAULT_VAL,
	                    VF_DUMPTODISK,
	                    "Toggles deferred shading combined depth bounds test + stencil test.\n"
	                    "Usage: r_DeferredShadingDBTstencil [0/1]\n"
	                    "Default is 1 (enabled)");

	DefineConstIntCVar3("r_DebugGBuffer", CV_r_DeferredShadingDebugGBuffer, 0, VF_NULL,
	                    "Debug view for gbuffer attributes\n"
	                    "  0 - Disabled\n"
	                    "  1 - Normals\n"
	                    "  2 - Smoothness\n"
	                    "  3 - Reflectance\n"
	                    "  4 - Albedo\n"
	                    "  5 - Lighting model\n"
	                    "  6 - Translucency\n"
	                    "  7 - Sun self-shadowing\n"
	                    "  8 - Subsurface scattering\n"
	                    "  9 - Specular validation overlay\n"
	                    );

	DefineConstIntCVar3("r_DeferredShadingLights", CV_r_DeferredShadingLights, 1, VF_DUMPTODISK,
	                    "Enables/Disables lights processing.\n"
	                    "Usage: r_DeferredShadingLights [0/1]\n"
	                    "Default is 1 (enabled)");

	DefineConstIntCVar3("r_DeferredShadingAmbientLights", CV_r_DeferredShadingAmbientLights, 1, VF_DUMPTODISK,
	                    "Enables/Disables ambient lights.\n"
	                    "Usage: r_DeferredShadingAmbientLights [0/1]\n"
	                    "Default is 1 (enabled)");

	DefineConstIntCVar3("r_DeferredShadingAreaLights", CV_r_DeferredShadingAreaLights, 0, VF_DUMPTODISK,
	                    "Enables/Disables more complex area lights processing.\n"
	                    "Usage: r_DeferredShadingAreaLights [0/1]\n"
	                    "Default is 0 (disabled)");

	REGISTER_CVAR3("r_DeferredShadingAmbient", CV_r_DeferredShadingAmbient, 1, VF_DUMPTODISK,
	                    "Enables/Disables ambient processing.\n"
	                    "Usage: r_DeferredShadingAmbient [0/1/2]\n"
	                    "  0: no ambient passes (disabled)\n"
	                    "  1: vis areas and outdoor ambient  (default)\n"
	                    "  2: only outdoor (debug vis areas mode)\n");

	REGISTER_CVAR3("r_DeferredShadingLightLodRatio", CV_r_DeferredShadingLightLodRatio, 1.0f, VF_DUMPTODISK,
	               "Sets deferred shading light intensity threshold.\n"
	               "Usage: r_DeferredShadingLightLodRatio [value]\n"
	               "Default is 0.1");

	REGISTER_CVAR3("r_DeferredShadingLightStencilRatio", CV_r_DeferredShadingLightStencilRatio, 0.21f, VF_DUMPTODISK,
	               "Sets screen ratio for deferred lights to use stencil (eg: 0.2 - 20% of screen).\n"
	               "Usage: r_DeferredShadingLightStencilRatio [value]\n"
	               "Default is 0.2");

	REGISTER_CVAR3("r_DeferredShadingSortLights", CV_r_DeferredShadingSortLights, 0, VF_CHEAT,
	               "Sorts light by influence\n"
	               "Usage: r_DeferredShadingSortLights [0/1]\n"
	               "Default is 0 (off)");

	//	TODO: Activate once sRGB encoding is gone from the shader
#if 0
#if !defined(__dxgi1_5_h__) || true /* TODO */
#if !CRY_PLATFORM_DURANGO
	REGISTER_CVAR3("r_HDRSwapChain", CV_r_HDRSwapChain, 0, VF_DUMPTODISK,
	                  "Toggles HDR display.\n"
	                  "Usage:r_HDRSwapChain [0/1/2]\n"
	                  "  0 - RGBA8 BT.709 (sRGB)\n"
	                  "  1 - RGBA16 (Linear)\n"
	                  "  2 - RGBA16F (Linear)\n"
	                  "Default is 0 (8-bit sRGB), film curve tone mapping.\n");
#endif
#else
	REGISTER_CVAR3("r_HDRSwapChain", CV_r_HDRSwapChain, 0, VF_DUMPTODISK,
	                  "Toggles HDR display.\n"
	                  "Usage:r_HDRSwapChain [0/1/2/3/4/5/6/7/8/9]\n"
	                  "  0 - RGBA8 BT.709 (sRGB)\n"
	                  "  1 - RGBA16 (Linear)\n"
	                  "  2 - RGBA16F (Linear)\n"
	                  // Require meta-data and possible transcoding of input-data to the other color-space
	                  "  3 - RGBA10 BT.709 (sRGB)\n"
	                  "  4 - RGBA16 BT.709 (sRGB)\n"
	                  "  5 - RGBA16F BT.709 (sRGB)\n"
	                  "  6 - RGBA8 BT.2020\n"
	                  "  7 - RGBA10 BT.2020\n"
	                  "  8 - RGBA16 BT.2020\n"
	                  "  9 - RGBA16F BT.2020\n"
	                  "Default is 0 (8-bit sRGB), film curve tone mapping.\n");
#endif
#endif

	DefineConstIntCVar3("r_HDRDebug", CV_r_HDRDebug, 0, VF_NULL,
	                    "Toggles HDR debugging info (to debug HDR/eye adaptation)\n"
	                    "Usage: r_HDRDebug\n"
	                    "0 off (default)\n"
						"1 display avgerage luminance, estimated luminance, and exposure values\n"
	                    "2 show gamma-corrected scene target without tone-mapping processing\n"
	                    "3 identify illegal colors (grey=normal, red=NotANumber, green=negative)\n");

	REGISTER_CVAR3("r_HDRBloom", CV_r_HDRBloom, 1, VF_NULL,
	               "Enables bloom/glare.\n"
	               "Usage: r_HDRBloom [0/1]\n");

	REGISTER_CVAR3("r_HDRBloomQuality", CV_r_HDRBloomQuality, 2, VF_NULL,
	               "Set bloom quality (0: low, 1: medium, 2: high)\n");

	REGISTER_CVAR3("r_HDRVignetting", CV_r_HDRVignetting, 1, VF_DUMPTODISK,
	                    "HDR viggneting\n"
	                    "Usage: r_HDRVignetting [Value]\n"
	                    "Default is 1 (enabled)");

	DefineConstIntCVar3("r_HDRRangeAdapt", CV_r_HDRRangeAdapt, HDR_RANGE_ADAPT_DEFAULT_VAL, VF_DUMPTODISK,
	                    "Enable/Disable HDR range adaptation (improve precision - minimize banding) \n"
	                    "Usage: r_HDRRangeAdapt [Value]\n"
	                    "Default is 1");

	REGISTER_CVAR3("r_HDRRangeAdaptMax", CV_r_HDRRangeAdaptMax, 1.0f, VF_DUMPTODISK,
	               "Set HDR range adaptation max adaptation (improve precision - minimize banding) \n"
	               "Usage: r_HDRRangeAdaptMax [Value]\n"
	               "Default is 1.0f");

	REGISTER_CVAR3("r_HDRRangeAdaptMaxRange", CV_r_HDRRangeAdaptMaxRange, 4.0f, VF_DUMPTODISK,
	               "Set HDR range adaptation max adaptation (improve precision - minimize banding) \n"
	               "Usage: r_HDRRangeAdaptMaxRange [Value]\n"
	               "Default is 4.0f");

	REGISTER_CVAR3("r_HDRRangeAdaptLBufferMax", CV_r_HDRRangeAdaptLBufferMax, 0.125f, VF_DUMPTODISK,
	               "Set range adaptation max adaptation for light buffers (improve precision - minimize banding) \n"
	               "Usage: r_HDRRangeAdaptLBufferMax [Value]\n"
	               "Default is 0.25f");

	REGISTER_CVAR3("r_HDRRangeAdaptLBufferMaxRange", CV_r_HDRRangeAdaptLBufferMaxRange, 2.0f, VF_DUMPTODISK,
	               "Set range adaptation max range adaptation for light buffers (improve precision - minimize banding) \n"
	               "Usage: r_HDRRangeAdaptLBufferMaxRange [Value]\n"
	               "Default is 2.0f");

	DefineConstIntCVar3("r_HDRTexFormat", CV_r_HDRTexFormat, 1, VF_DUMPTODISK | VF_REQUIRE_APP_RESTART,
	                    "Sets HDR render target precision. Default is 1.\n"
	                    "Usage: r_HDRTexFormat [Value]\n"
	                    "  0: (lower precision R11G11B10F, except for Main Target)\n"
	                    "  1: (standard precision R16G16B16A16F)\n"
	                    "  2: (lower precision R11G11B10F, for all targets but DoF should be off)");

	// Eye Adaptation
	REGISTER_CVAR3("r_HDREyeAdaptationSpeed", CV_r_HDREyeAdaptationSpeed, 1.0f, VF_NULL,
	               "HDR rendering eye adaptation speed\n"
	               "Usage: r_EyeAdaptationSpeed [Value]");

	REGISTER_CVAR3("r_HDREyeAdaptationMode", CV_r_HDREyeAdaptationMode, 2, VF_DUMPTODISK,
	               "Set the eye adaptation (auto exposure) mode.\n"
	               "  1: Standard auto exposure (using EV values)\n"
	               "  2: Legacy auto exposure for backwards compatibility (CE 3.6 to 3.8.1)");

	REGISTER_CVAR3("r_HDRRangeAdaptationSpeed", CV_r_HDRRangeAdaptationSpeed, 4.0f, VF_NULL,
	               "HDR range adaption speed\n"
	               "Usage: r_HDRRangeAdaptationSpeed [Value]");

	REGISTER_CVAR3("r_HDRGrainAmount", CV_r_HDRGrainAmount, 0.0f, VF_NULL,
	               "HDR camera grain amount\n"
	               "Usage: r_HDRGrainAmount [Value]");


	DefineConstIntCVar3("r_GrainEnableExposureThreshold", CV_r_GrainEnableExposureThreshold, 0, VF_DUMPTODISK,
			"Enable/Disable Legacy Exposure-based grain threshold\n"
			"Usage: r_GrainEnableExposureThreshold [Value]\n"
			"Default is 0");


	REGISTER_CVAR3("r_ChromaticAberration", CV_r_ChromaticAberration, 0.0f, VF_NULL,
	               "Chromatic aberration amount\n"
	               "Usage: r_ChromaticAberration [Value]");

	REGISTER_CVAR3("r_Sharpening", CV_r_Sharpening, 0.0f, VF_NULL,
	               "Image sharpening amount\n"
	               "Usage: r_Sharpening [Value]");

	REGISTER_CVAR3_CB("r_GeomInstancingThreshold", CV_r_geominstancingthreshold, -1, VF_NULL,
	                  "If the instance count gets bigger than the specified value the instancing feature is used.\n"
	                  "Usage: r_GeomInstancingThreshold [Num]\n"
	                  "Default is -1 (automatic depending on hardware, used value can be found in the log)",
	                  OnChange_GeomInstancingThreshold);

	REGISTER_CVAR3("r_BatchType", CV_r_batchtype, 0, VF_NULL,
	               "0 - CPU friendly.\n"
	               "1 - GPU friendly.\n"
	               "2 - Automatic.\n");

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_RENDERER_GNM
	REGISTER_CVAR3("r_SilhouettePOM", CV_r_SilhouettePOM, 0, VF_NULL,
	               "Enables use of silhouette parallax occlusion mapping.\n"
	               "Usage: r_SilhouettePOM [0/1]");
#endif
#ifdef WATER_TESSELLATION_RENDERER
	REGISTER_CVAR3("r_WaterTessellationHW", CV_r_WaterTessellationHW, 0, VF_NULL,
	               "Enables hw water tessellation.\n"
	               "Usage: r_WaterTessellationHW [0/1]");
#endif

	REGISTER_CVAR3("r_TessellationDebug", CV_r_tessellationdebug, 0, VF_NULL,
	               "1 - Factor visualizing.\n"
	               "Default is 0");
	REGISTER_CVAR3("r_TessellationTriangleSize", CV_r_tessellationtrianglesize, 8.0f, VF_NULL,
	               "Desired triangle size for screen-space tessellation.\n"
	               "Default is 10.");
	REGISTER_CVAR3("r_UseDisplacementFactor", CV_r_displacementfactor, 0.2f, VF_NULL,
	               "Global displacement amount.\n"
	               "Default is 0.4f.");

	DefineConstIntCVar3("r_GeomInstancing", CV_r_geominstancing, GEOM_INSTANCING_DEFAULT_VAL, VF_NULL,
	                    "Toggles HW geometry instancing.\n"
	                    "Usage: r_GeomInstancing [0/1]\n"
	                    "Default is 1 (on). Set to 0 to disable geom. instancing.");

	DefineConstIntCVar3("r_GeomInstancingDebug", CV_r_geominstancingdebug, 0, VF_NULL,
	                    "Toggles HW geometry instancing debug display.\n"
	                    "Usage: r_GeomInstancingDebug [0/1/2]\n"
	                    "Default is 0 (off). Set to 1 to add GPU markers around instanced objects. 2 will visually highlight them as well.");

	DefineConstIntCVar3("r_MaterialsBatching", CV_r_materialsbatching, 1, VF_NULL,
	                    "Toggles materials batching.\n"
	                    "Usage: r_MaterialsBatching [0/1]\n"
	                    "Default is 1 (on). Set to 0 to disable.");

	DefineConstIntCVar3("r_ZPassDepthSorting", CV_r_ZPassDepthSorting, ZPASS_DEPTH_SORT_DEFAULT_VAL, VF_NULL,
	                    "Toggles Z pass depth sorting.\n"
	                    "Usage: r_ZPassDepthSorting [0/1/2]\n"
	                    "0: No depth sorting\n"
	                    "1: Sort by depth layers (default)\n"
	                    "2: Sort by distance\n");

	REGISTER_CVAR3("r_ZPrepassMaxDist", CV_r_ZPrepassMaxDist, 16.0f, VF_NULL,
	               "Set ZPrepass max dist.\n"
	               "Usage: r_ZPrepassMaxDist (16.0f default) [distance in meters]\n");

	REGISTER_CVAR3("r_UseZPass", CV_r_usezpass, 2, VF_RENDERER_CVAR,
	               "Toggles g-buffer pass.\n"
	               "Usage: r_UseZPass [0/1/2]\n"
	               "0: Disable Z-pass (not recommended, this disables any g-buffer rendering)\n"
	               "1: Enable Z-pass (g-buffer only)\n"
	               "2: Enable Z-pass (g-buffer and additional Z-prepass)");

	DefineConstIntCVar3("r_TransparentPasses", CV_r_TransparentPasses, 1, VF_NULL,
	                    "Toggles rendering of transparent/alpha blended objects.\n");

	REGISTER_CVAR3("r_TranspDepthFixup", CV_r_TranspDepthFixup, 1, VF_NULL,
	                    "Write approximate depth for certain transparent objects before post effects\n"
	                    "Usage: r_TranspDepthFixup [0/1]\n"
	                    "Default is 1 (enabled)\n");

	DefineConstIntCVar3("r_SkipAlphaTested", CV_r_SkipAlphaTested, 0, VF_CHEAT,
	                    "Disables rendering of alpha-tested objects.\n");

	DefineConstIntCVar3("r_UseHWSkinning", CV_r_usehwskinning, 1, VF_NULL,
	                    "Toggles HW skinning.\n"
	                    "Usage: r_UseHWSkinning [0/1]\n"
	                    "Default is 1 (on). Set to 0 to disable HW-skinning.");
	DefineConstIntCVar3("r_UseMaterialLayers", CV_r_usemateriallayers, 2, VF_NULL,
	                    "Enables material layers rendering.\n"
	                    "Usage: r_UseMaterialLayers [0/1/2]\n"
	                    "Default is 2 (optimized). Set to 1 for enabling but with optimization disabled (for debug).");

	REGISTER_CVAR3("r_ParticlesSoftIsec", CV_r_ParticlesSoftIsec, 1, VF_NULL,
	                    "Enables particles soft intersections.\n"
	                    "Usage: r_ParticlesSoftIsec [0/1]");

	REGISTER_CVAR3("r_ParticlesRefraction", CV_r_ParticlesRefraction, 1, VF_NULL,
	                    "Enables refractive particles.\n"
	                    "Usage: r_ParticlesRefraction [0/1]");

	REGISTER_CVAR3("r_ParticlesHalfRes", CV_r_ParticlesHalfRes, 0, VF_NULL,
	               "Enables (1) or forces (2) rendering of particles in a half-resolution buffer.\n"
	               "Usage: r_ParticlesHalfRes [0/1/2]");

	DefineConstIntCVar3("r_ParticlesHalfResBlendMode", CV_r_ParticlesHalfResBlendMode, 0, VF_NULL,
	                    "Specifies which particles can be rendered in half resolution.\n"
	                    "Usage: r_ParticlesHalfResBlendMode [0=alpha / 1=additive]");

	DefineConstIntCVar3("r_ParticlesHalfResAmount", CV_r_ParticlesHalfResAmount, 0, VF_NULL,
	                    "Sets particle half-res buffer to half (0) or quarter (1) screen size.\n"
	                    "Usage: r_ParticlesHalfResForce [0/1]");

	DefineConstIntCVar3("r_ParticlesInstanceVertices", CV_r_ParticlesInstanceVertices, 1, VF_NULL,
	                    "Enable instanced-vertex rendering.\n"
	                    "Usage: r_ParticlesInstanceVertices [0/1]");

	REGISTER_CVAR3("r_ParticlesAmountGI", CV_r_ParticlesAmountGI, 0.15f, VF_NULL,
	               "Global illumination amount for particles without material.\n"
	               "Usage: r_ParticlesAmountGI [n]");

	static string aaModesDesc = "Enables post process based anti-aliasing modes.\nUsage: r_AntialiasingMode [n]\n";

	for (int i = 0; i < eAT_AAMODES_COUNT; ++i)
	{
		string mode;
		mode.Format("%d: %s\n", i, s_pszAAModes[i]);
		aaModesDesc.append(mode);
	}

	REGISTER_CVAR3_CB("r_AntialiasingMode", CV_r_AntialiasingMode_CB, eAT_DEFAULT_AA, VF_NULL, aaModesDesc.c_str(), OnChange_CV_r_AntialiasingMode);
	CV_r_AntialiasingMode = CV_r_AntialiasingMode_CB;

	REGISTER_CVAR3("r_AntialiasingTAAPattern", CV_r_AntialiasingTAAPattern, 1, VF_NULL,
	               "Selects TAA sampling pattern.\n"
	               "  0: no subsamples\n"
	               "  1: optimal pattern for selected aa mode\n"
	               "  2: 2x\n  3: 3x\n  4: regular 4x\n  5: rotated 4x\n  6: 8x\n  7: sparse grid 8x8\n  8: random\n  9: Halton 8x\n  10: Halton 16x\n  11: Halton random");

	REGISTER_CVAR3("r_AntialiasingModeSCull", CV_r_AntialiasingModeSCull, 1, VF_NULL,
	               "Enables post processed based aa modes stencil culling optimization\n");

	DefineConstIntCVar3("r_AntialiasingModeDebug", CV_r_AntialiasingModeDebug, 0, VF_NULL,
	                    "Enables AA debugging views\n"
	                    "Usage: r_AntialiasingModeDebug [n]"
	                    "1: Display edge detection"
	                    "2: Zoom image 2x"
	                    "3: Zoom image 2x + display edge detection"
	                    "4: Zoom image 4x, etc");

	REGISTER_CVAR3("r_AntialiasingTAASharpening", CV_r_AntialiasingTAASharpening, 0.2f, VF_NULL,
	               "Enables TAA sharpening.\n");

	REGISTER_CVAR3("r_AntialiasingTAAFalloffHiFreq", CV_r_AntialiasingTAAFalloffHiFreq, 6.0f, VF_NULL,
	               "Set TAA falloff for high frequencies (high contrast regions). Default 6.0\n"
	               "Bigger value increases temporal stability but also overall image blurriness\n"
	               "Smaller value decreases temporal stability but gives overall sharper image");

	REGISTER_CVAR3("r_AntialiasingTAAFalloffLowFreq", CV_r_AntialiasingTAAFalloffLowFreq, 2.0f, VF_NULL,
	               "Set TAA falloff for low frequencies (low contrast regions). Default 2.0\n"
	               "Bigger value increases temporal stability but also overall image blurriness\n"
	               "Smaller value decreases temporal stability but gives overall sharper image");

	REGISTER_CVAR3("r_AntialiasingTSAAMipBias", CV_r_AntialiasingTSAAMipBias, -0.25f, VF_NULL,
	               "Sets mimap lod bias for TSAA (negative values for sharpening).\n");

	REGISTER_CVAR3("r_AntialiasingTSAASubpixelDetection", CV_r_AntialiasingTSAASubpixelDetection, 0.02f, VF_NULL,
	               "Adjusts metric for detecting subpixel changes (higher: less flickering but more ghosting)\n");

	REGISTER_CVAR3("r_AntialiasingTSAASmoothness", CV_r_AntialiasingTSAASmoothness, 40.0f, VF_NULL,
	               "Adjusts smoothness of image under motion.\n");

	DefineConstIntCVar3("r_AntialiasingModeEditor", CV_r_AntialiasingModeEditor, 1, VF_NULL,
	                    "Sets antialiasing modes to editing mode (disables jitter on modes using camera jitter which can cause flickering of helper objects)\n"
	                    "Usage: r_AntialiasingModeEditor [0/1]");

	REGISTER_CVAR3("r_MotionVectors", CV_r_MotionVectors, 1, VF_NULL,
	                    "Enables generation of motion vectors for dynamic objects\n");

	REGISTER_CVAR3("r_MotionBlur", CV_r_MotionBlur, 2, VF_NULL,
	               "Enables per object and camera motion blur.\n"
	               "Usage: r_MotionBlur [0/1/2/3]\n"
	               "Default is 1 (camera motion blur on).\n"
	               "1: camera motion blur\n"
	               "2: camera and object motion blur\n"
	               "3: debug mode\n");

	REGISTER_CVAR3("r_MotionBlurQuality", CV_r_MotionBlurQuality, 1, VF_NULL,
	               "Set motion blur sample count.\n"
	               "Usage: r_MotionBlurQuality [0/1]\n"
	               "0 - low quality, 1 - medium quality, 2 - high quality\n");

	REGISTER_CVAR3("r_MotionBlurGBufferVelocity", CV_r_MotionBlurGBufferVelocity, 1, VF_NULL,
	               "Pack velocity output in g-buffer.\n"
	               "Usage: r_MotionBlurGBufferVelocity [0/1]\n"
	               "Default is 1 (enabled). 0 - disabled\n");

	REGISTER_CVAR3("r_MotionBlurThreshold", CV_r_MotionBlurThreshold, 0.0001f, VF_NULL,
	               "Object motion blur velocity threshold.\n"
	               "Usage: r_MotionBlurThreshold (val)\n"
	               "Default is 0.0001.  0 - disabled\n");

	REGISTER_CVAR3("r_UseMergedPosts", CV_r_UseMergedPosts, 1, VF_NULL,
	               "Enables motion blur merged with dof.\n"
	               "Usage: r_UseMergedPosts [0/1/2]\n"
	               "Default is 1.\n"
	               "1: fastest mode - half res rendering\n"
	               "2: full res rendering mode (tbd)\n"
	               "3: quality mode, hdr + fullres (tbd)\n");

	REGISTER_CVAR3("r_MotionBlurShutterSpeed", CV_r_MotionBlurShutterSpeed, 250.0f, 0,
	               "Sets camera exposure time for motion blur as 1/x seconds.\n"
	               "Default: 250 (1/250 of a second)");

	REGISTER_CVAR3("r_MotionBlurCameraMotionScale", CV_r_MotionBlurCameraMotionScale, 0.2f, 0,
	               "Reduces motion blur for camera movements to account for a fixed focus point of the viewer.\n"
	               "Default: 0.2");

	REGISTER_CVAR3("r_MotionBlurMaxViewDist", CV_r_MotionBlurMaxViewDist, 16.0f, 0,
	               "Sets motion blur max view distance for objects.\n"
	               "Usage: r_MotionBlurMaxViewDist [0...1]\n"
	               "Default is 16 meters");

	DefineConstIntCVar3("r_CustomVisions", CV_r_customvisions, CUSTOMVISIONS_DEFAULT_VAL, VF_NULL,
	                    "Enables custom visions, like heatvision, binocular view, etc.\n"
	                    "Usage: r_CustomVisions [0/1/2/3]\n"
	                    "Default is 0 (disabled). 1 enables. 2 - cheaper version, no post processing. 3 - cheaper post version");

	DefineConstIntCVar3("r_DebugLayerEffect", CV_r_DebugLayerEffect, 0, VF_NULL,
	                    "Enables debug mode (independent from game code) for layer effects\n"
	                    "Usage: r_DebugLayerEffect [0/1/2/3/etc]\n"
	                    "Default is 0 (disabled). 1: 1st layer mode, etc");

	DefineConstIntCVar3("r_VrProjectionType", CV_r_VrProjectionType, 0, VF_REQUIRE_APP_RESTART,
	                    "Selects which modified projection to use for rendering\n"
	                    "Usage: r_VrProjectionType [0/1/2]\n"
	                    "0: none/planar (default)\n"
	                    "1: Nvidia Multi-Res Shading\n"
	                    "2: NVidia Lens-Matched Shading");
	
	DefineConstIntCVar3("r_VrProjectionPreset", CV_r_VrProjectionPreset, 0, VF_NULL,
	                    "Selects the quality preset for the modified projection\n"
	                    "Usage: r_VrProjectionPreset [0..8]\n"
	                    "0: full-resolution preset, can be used for quality testing\n"
	                    "1,2: desktop high and low quality presets\n"
	                    "3..5: Oculus Rift high..low quality presets\n"
	                    "6..8: HTC Vive high..low quality presets");

	DefineConstIntCVar3("r_stereoMirrorProjection", CV_r_stereoMirrorProjection, 1, VF_NULL,
	                    "Enables mirroring of MRS or LMS projection for the right eye\n"
	                    "Usage: r_LensMatchedRendering [0/1]\n"
	                    "Default is 1 (enabled).");

	REGISTER_CVAR3("r_Snow", CV_r_snow, 2, VF_NULL,
	                    "Enables snow rendering\n"
	                    "Usage: r_Snow [0/1/2]\n"
	                    "0 - disabled\n"
	                    "1 - enabled\n"
	                    "2 - enabled with snow occlusion");

	DefineConstIntCVar3("r_SnowHalfRes", CV_r_snow_halfres, 0, VF_NULL,
	                    "When enabled, snow renders at half resolution to conserve fill rate.\n"
	                    "Usage: r_SnowHalfRes [0/1]\n"
	                    "0 - disabled\n"
	                    "1 - enabled\n");

	DefineConstIntCVar3("r_SnowDisplacement", CV_r_snow_displacement, 0, VF_NULL,
	                    "Enables displacement for snow accumulation\n"
	                    "Usage: r_SnowDisplacement [0/1]\n"
	                    "0 - disabled\n"
	                    "1 - enabled");

	DefineConstIntCVar3("r_SnowFlakeClusters", CV_r_snowFlakeClusters, 100, VF_NULL,
	                    "Number of snow flake clusters.\n"
	                    "Usage: r_SnowFlakeClusters [n]");

	REGISTER_CVAR3("r_Rain", CV_r_rain, 2, VF_NULL,
	                    "Enables rain rendering\n"
	                    "Usage: r_Rain [0/1/2]\n"
	                    "0 - disabled"
	                    "1 - enabled"
	                    "2 - enabled with rain occlusion");

	REGISTER_CVAR3("r_RainAmount", CV_r_rainamount, 1.0f, VF_NULL,
	               "Sets rain amount\n"
	               "Usage: r_RainAmount");

	REGISTER_CVAR3("r_RainMaxViewDist", CV_r_rain_maxviewdist, 32.0f, VF_NULL,
	               "Sets rain max view distance\n"
	               "Usage: r_RainMaxViewDist");

	REGISTER_CVAR3("r_RainMaxViewDist_Deferred", CV_r_rain_maxviewdist_deferred, 40.f, VF_NULL,
	               "Sets maximum view distance (in meters) for deferred rain reflection layer\n"
	               "Usage: r_RainMaxViewDist_Deferred [n]");

	REGISTER_CVAR3("r_RainDistMultiplier", CV_r_rainDistMultiplier, 2.f, VF_NULL, "Rain layer distance from camera multiplier");

	REGISTER_CVAR3("r_RainOccluderSizeTreshold", CV_r_rainOccluderSizeTreshold, 25.f, VF_NULL, "Only objects bigger than this size will occlude rain");

	REGISTER_CVAR3("r_SSReflections", CV_r_SSReflections, 0, VF_NULL,
	               "Glossy screen space reflections [0/1]\n");
	REGISTER_CVAR3("r_SSReflHalfRes", CV_r_SSReflHalfRes, 1, VF_NULL,
	               "Toggles rendering reflections in half resolution\n");
	REGISTER_CVAR3("r_SSReflDistance", CV_r_SSReflDistance, 0.25, VF_NULL,
	               "Maximum distance of SS raytrace in relation to far-plane.\n"
	               "Usage: r_SSReflDistance [0...1]\n"
	               "Default is 0.25, which is about 500m (further away content is fetched from cube-maps)");
	REGISTER_CVAR3("r_SSReflSamples", CV_r_SSReflSamples, 28, VF_NULL,
	               "Maximum number of samples taken within allowed distance, in addition to 4 always taken samples.\n"
	               "Usage: r_SSReflSamples [0...inf]\n"
	               "Default is 4+28, which is about 1 sample every 15m (with default distance of 500m)");

	REGISTER_CVAR3("r_ssdo", CV_r_ssdo, 1, VF_NULL, "Screen Space Directional Occlusion [0/1]\n");
	REGISTER_CVAR3("r_ssdoHalfRes", CV_r_ssdoHalfRes, 2, VF_NULL,
	               "Apply SSDO bandwidth optimizations\n"
	               "0 - Full resolution (not recommended)\n"
	               "1 - Use lower resolution depth\n"
	               "2 - Low res depth except for small camera FOVs to avoid artifacts\n"
	               "3 - Half resolution output");
	REGISTER_CVAR3("r_ssdoColorBleeding", CV_r_ssdoColorBleeding, 1, VF_NULL,
	               "Enables AO color bleeding to avoid overly dark occlusion on bright surfaces (requires tiled deferred shading)\n"
	               "Usage: r_ssdoColorBleeding [0/1]\n");
	REGISTER_CVAR3("r_ssdoRadius", CV_r_ssdoRadius, 1.2f, VF_NULL, "SSDO radius");
	REGISTER_CVAR3("r_ssdoRadiusMin", CV_r_ssdoRadiusMin, 0.04f, VF_NULL, "Min clamped SSDO radius");
	REGISTER_CVAR3("r_ssdoRadiusMax", CV_r_ssdoRadiusMax, 0.20f, VF_NULL, "Max clamped SSDO radius");
	REGISTER_CVAR3("r_ssdoAmountDirect", CV_r_ssdoAmountDirect, 2.0f, VF_NULL, "Strength of occlusion applied to light sources");
	REGISTER_CVAR3("r_ssdoAmountAmbient", CV_r_ssdoAmountAmbient, 1.0f, VF_NULL, "Strength of occlusion applied to probe irradiance");
	REGISTER_CVAR3("r_ssdoAmountReflection", CV_r_ssdoAmountReflection, 1.5f, VF_NULL, "Strength of occlusion applied to probe specular");

	REGISTER_CVAR3("r_RainIgnoreNearest", CV_r_rain_ignore_nearest, 1, VF_NULL,
	                    "Disables rain wet/reflection layer for nearest objects\n"
	                    "Usage: r_RainIgnoreNearest [0/1]\n");

	REGISTER_CVAR3("r_DepthOfField", CV_r_dof, DOF_DEFAULT_VAL, VF_NULL,
	                    "Enables depth of field.\n"
	                    "Usage: r_DepthOfField [0/1/2]\n"
	                    "Default is 0 (disabled). 1 enables, 2 hdr time of day dof enabled");

	REGISTER_CVAR3("r_DepthOfFieldMode", CV_r_DofMode, 1, VF_NULL,
	               "Selects DOF implementation (0: sprite-based, 1: gather-based).\n");

	REGISTER_CVAR3("r_DepthOfFieldBokehQuality", CV_r_DofBokehQuality, 0, VF_NULL,
	               "Sets depth of field bokeh quality (samples multiplier).\n"
	               "Usage: r_DepthOfFieldBokeh [0/1/etc]\n"
	               "Default is 0: ingame quality, 1: high quality mode");

	DefineConstIntCVar3("r_DebugLightVolumes", CV_r_DebugLightVolumes, 0, VF_NULL,
	                    "0=Disable\n"
	                    "1=Enable\n"
	                    "Usage: r_DebugLightVolumes[0/1]");

	REGISTER_CVAR3("r_ShadowsBias", CV_r_ShadowsBias, 0.00008f, VF_DUMPTODISK, //-0.00002
	               "Select shadow map blurriness if r_ShadowsBias is activated.\n"
	               "Usage: r_ShadowsBias [0.1 - 16]");

	REGISTER_CVAR3("r_ShadowsAdaptionRangeClamp", CV_r_ShadowsAdaptionRangeClamp, 0.02f, VF_DUMPTODISK, //-0.00002
	               "maximum range between caster and reciever to take into account.\n"
	               "Usage: r_ShadowsAdaptionRangeClamp [0.0 - 1.0], default 0.01");

	REGISTER_CVAR3("r_ShadowsAdaptionSize", CV_r_ShadowsAdaptionSize, 0.3f, VF_DUMPTODISK, //-0.00002
	               "Select shadow map blurriness if r_ShadowsBias is activated.\n"
	               "Usage: r_ShadowsAdaptoinSize [0 for none - 10 for rapidly changing]");

	REGISTER_CVAR3("r_ShadowsAdaptionMin", CV_r_ShadowsAdaptionMin, 0.35f, VF_DUMPTODISK, //-0.00002
	               "starting kernel size, to avoid blocky shadows.\n"
	               "Usage: r_ShadowsAdaptionMin [0.0 for blocky - 1.0 for blury], 0.35 is default");

	REGISTER_CVAR3("r_ShadowsParticleKernelSize", CV_r_ShadowsParticleKernelSize, 1.f, VF_DUMPTODISK,
	               "Blur kernel size for particles shadows.\n"
	               "Usage: r_ShadowsParticleKernelSize [0.0 hard edge - x for blur], 1. is default");

	REGISTER_CVAR3("r_ShadowsParticleJitterAmount", CV_r_ShadowsParticleJitterAmount, 0.5f, VF_DUMPTODISK,
	               "Amount of jittering for particles shadows.\n"
	               "Usage: r_ShadowsParticleJitterAmount [x], 0.5 is default");

	REGISTER_CVAR3("r_ShadowsParticleAnimJitterAmount", CV_r_ShadowsParticleAnimJitterAmount, 1.f, VF_DUMPTODISK,
	               "Amount of animated jittering for particles shadows.\n"
	               "Usage: r_ShadowsParticleJitterAmount [x], 1. is default");

	REGISTER_CVAR3("r_ShadowsParticleNormalEffect", CV_r_ShadowsParticleNormalEffect, 1.f, VF_DUMPTODISK,
	               "Shadow taps on particles affected by normal and intensity (breaks lines and uniformity of shadows).\n"
	               "Usage: r_ShadowsParticleNormalEffect [x], 1. is default");

	REGISTER_CVAR3_CB("r_ShadowsCache", CV_r_ShadowsCache, 0, VF_NULL,
	                  "Replace all sun cascades above cvar value with cached (static) shadow map: 0=no cached shadows, 1=replace first cascade and up, 2=replace second cascade and up,...",
	                  OnChange_CachedShadows);

	REGISTER_CVAR3_CB("r_ShadowsCacheFormat", CV_r_ShadowsCacheFormat, 1, VF_NULL,
	                  "0=use D32 texture format for shadow cache\n"
	                  "1=use D16 texture format for shadow cache\n",
	                  OnChange_CachedShadows);

	REGISTER_STRING_CB("r_ShadowsCacheResolutions", "", VF_RENDERER_CVAR, "Shadow cache resolution per cascade. ", OnChange_CachedShadows);

	REGISTER_CVAR3("r_ShadowsNearestMapResolution", CV_r_ShadowsNearestMapResolution, 4096, VF_REQUIRE_APP_RESTART,
	               "Nearest shadow map resolution. Default: 4096");

	REGISTER_CVAR3("r_ShadowsScreenSpace", CV_r_ShadowsScreenSpace, 0, VF_NULL,
	               "Include screen space tracing into shadow computations\n"
	               "Helps reducing artifacts caused by limited shadow map resolution and biasing");

	DefineConstIntCVar3("r_ShadowTexFormat", CV_r_shadowtexformat, 0, VF_NULL,
	                    "0=use D32 texture format for depth map\n"
	                    "1=use D16 texture format for depth map\n"
	                    "2=use D24S8 texture format for depth map\n"
	                    "Usage: r_ShadowTexFormat [0-2]");

	DefineConstIntCVar3("r_ShadowsMask", CV_r_ShadowsMask, 1, VF_CHEAT,
	                    "Controls screen space calculation of shadow contribution"
	                    "0=off\n"
	                    "1=all shadow casting lights\n"
	                    "2=sun only\n"
	                    "3=point lights only\n"
	                    "Usage: r_ShadowsMask [0/1/2/3]");

	DefineConstIntCVar3("r_ShadowsMaskResolution", CV_r_ShadowsMaskResolution, 0, VF_INVISIBLE,
	                    "0=per pixel shadow mask\n"
	                    "1=horizontal half resolution shadow mask\n"
	                    "2=horizontal and vertical half resolution shadow mask\n"
	                    "Usage: r_ShadowsMaskResolution [0/1/2]");

	DefineConstIntCVar3("r_CBufferUseNativeDepth", CV_r_CBufferUseNativeDepth, CBUFFER_NATIVE_DEPTH_DEAFULT_VAL, VF_NULL,
	                    "1= enable, 0 = disable\n"
	                    "Usage: r_CBufferUseNativeDepth [0/1]");

	DefineConstIntCVar3("r_ShadowMaskStencilPrepass", CV_r_ShadowMaskStencilPrepass, 0, VF_NULL,
	                    "1=Run explicit stencil prepass for shadow mask generation (as opposed to merged stencil/sampling passes)");

	REGISTER_CVAR3("r_ShadowsPCFiltering", CV_r_ShadowsPCFiltering, 1, VF_NULL,
	               "1=use PCF for shadows\n"
	               "Usage: r_ShadowsPCFiltering [0/1]");
	REGISTER_CVAR3_CB("r_ShadowJittering", CV_r_shadow_jittering, 3.4f, VF_NULL,
	                  "Shadow map jittering radius.\n"
	                  "In PC the only use of this cvar is to instantly see the effects of diferent jittering values,\n"
	                  "because any value set here will be overwritten by ToD animation (only in PC) as soon as ToD changes.\n"
	                  "Usage: r_ShadowJittering [0=off]", OnChangeShadowJitteringCVar);
	DefineConstIntCVar3("r_DebugLights", CV_r_debuglights, 0, VF_CHEAT,
	                    "Display dynamic lights for debugging.\n"
	                    "Usage: r_DebugLights [0/1/2/3]\n"
	                    "Default is 0 (off). Set to 1 to display centers of light sources,\n"
	                    "or set to 2 to display light centers and attenuation spheres, 3 to get light properties to the screen");
	DefineConstIntCVar3("r_ShadowsGridAligned", CV_r_ShadowsGridAligned, 1, VF_DUMPTODISK,
	                    "Selects algorithm to use for shadow mask generation:\n"
	                    "0 - Disable shadows snapping\n"
	                    "1 - Enable shadows snapping");
	DefineConstIntCVar3("r_ShadowGenDepthClip", CV_r_ShadowGenDepthClip, 1, VF_NULL,
	                    "0=disable shadow gen depth clipping, 1=enable shadow gen depth clipping");
	DefineConstIntCVar3("r_ShadowMapsUpdate", CV_r_ShadowMapsUpdate, 1, VF_NULL,
	                    "0=disable shadow map updates, 1=enable shadow map updates");
	DefineConstIntCVar3("r_ShadowPoolMaxFrames", CV_r_ShadowPoolMaxFrames, 30, VF_NULL,
	                    "Maximum number of frames a shadow can exist in the pool");
	REGISTER_CVAR3("r_ShadowPoolMaxTimeslicedUpdatesPerFrame", CV_r_ShadowPoolMaxTimeslicedUpdatesPerFrame, 1, VF_NULL,
	               "Max number of time sliced shadow pool updates allowed per frame");
	REGISTER_CVAR3("r_ShadowCastingLightsMaxCount", CV_r_ShadowCastingLightsMaxCount, 12, VF_REQUIRE_APP_RESTART,
	               "Maximum number of simultaneously visible shadow casting lights");

#if !CRY_RENDERER_VULKAN
	REGISTER_CVAR3_CB("r_HeightMapAO", CV_r_HeightMapAO, 1, VF_NULL,
	                  "Large Scale Ambient Occlusion based on height map approximation of the scene\n"
	                  "0=off, 1=quarter resolution, 2=half resolution, 3=full resolution",
	                  OnChange_CachedShadows);
	REGISTER_CVAR3("r_HeightMapAOAmount", CV_r_HeightMapAOAmount, 1.0f, VF_NULL,
	               "Height Map Ambient Occlusion Amount");
	REGISTER_CVAR3_CB("r_HeightMapAOResolution", CV_r_HeightMapAOResolution, 2048, VF_NULL,
	                  "Texture resolution of the height map used for HeightMapAO", OnChange_CachedShadows);
	REGISTER_CVAR3_CB("r_HeightMapAORange", CV_r_HeightMapAORange, 1000.f, VF_NULL,
	                  "Range of height map AO around viewer in meters", OnChange_CachedShadows);
#endif

	REGISTER_CVAR3("r_RenderMeshHashGridUnitSize", CV_r_RenderMeshHashGridUnitSize, .5f, VF_NULL, "Controls density of render mesh triangle indexing structures");

	DefineConstIntCVar3("r_LightsSinglePass", CV_r_lightssinglepass, 1, VF_NULL, "");

	DefineConstIntCVar3("r_ShowDynTextures", CV_r_showdyntextures, 0, VF_NULL,
	                    "Display a dyn. textures, filtered by r_ShowDynTexturesFilter\n"
	                    "Usage: r_ShowDynTextures 0/1/2\n"
	                    "Default is 0. Set to 1 to show all dynamic textures or 2 to display only the ones used in this frame\n"
	                    "Textures are sorted by memory usage");

	REGISTER_CVAR3("r_ShowDynTexturesMaxCount", CV_r_ShowDynTexturesMaxCount, 36, VF_NULL,
	               "Allows to adjust number of textures shown on the screen\n"
	               "Usage: r_ShowDynTexturesMaxCount [1...36]\n"
	               "Default is 36");

	CV_r_ShowDynTexturesFilter = REGISTER_STRING("r_ShowDynTexturesFilter", "*", VF_NULL,
	                                             "Usage: r_ShowDynTexturesFilter *end\n"
	                                             "Usage: r_ShowDynTexturesFilter *mid*\n"
	                                             "Usage: r_ShowDynTexturesFilter start*\n"
	                                             "Default is *. Set to 'pattern' to show only specific textures (activate r_ShowDynTextures)");

	CV_r_ShaderCompilerServer = REGISTER_STRING("r_ShaderCompilerServer", "localhost", VF_NULL,
	                                            "Usage: r_ShaderCompilerServer <ip;ip;ip;...> \n"
	                                            "Default is localhost ");
																							
	CV_r_ShaderCompilerFolderName = REGISTER_STRING("r_ShaderCompilerFolderName", "", VF_NULL,
	                                             "Usage: r_ShaderCompilerFolderName foldername \n"
	                                             "Default is empty. This overwrites the default path of using the game directory name when storing shader caches.");

	CV_r_ShaderCompilerFolderSuffix = REGISTER_STRING("r_ShaderCompilerFolderSuffix", "", VF_NULL,
	                                                  "Usage: r_ShaderCompilerFolderSuffix suffix \n"
	                                                  "Default is empty. Set to some other value to append this suffix to the sys_game_folder when compiling shaders");

	{
		const SFileVersion& ver = gEnv->pSystem->GetFileVersion();

		char versionString[128];
		memset(versionString, 0, sizeof(versionString));
		sprintf(versionString, "Build Version: %d.%d.%d.%d", ver.v[3], ver.v[2], ver.v[1], ver.v[0]);

		CV_r_ShaderEmailTags = REGISTER_STRING("r_ShaderEmailTags", versionString, VF_NULL,
		                                       "Adds optional tags to shader error emails e.g. own name or build run\n"
		                                       "Usage: r_ShaderEmailTags \"some set of tags or text\" \n"
		                                       "Default is build version ");
	}

	{
		CV_r_ShaderEmailCCs = REGISTER_STRING("r_ShaderEmailCCs", "", VF_NULL,
		                                      "Adds optional CC addresses to shader error emails\n"
		                                      "Default is empty ");
	}

	REGISTER_CVAR3("r_ShaderCompilerPort", CV_r_ShaderCompilerPort, 61453, VF_NULL,
	               "set user defined port of the shader compile server.\n"
	               "Usage: r_ShaderCompilerPort 61453 #\n"
	               "Default is 61453");

	REGISTER_CVAR3("r_ShaderCompilerDontCache", CV_r_ShaderCompilerDontCache, 0, VF_NULL,
	               "Disables caching on server side.\n"
	               "Usage: r_ShaderCompilerDontCache 0 #\n"
	               "Default is 0");

	REGISTER_CVAR3("r_RC_AutoInvoke", CV_r_rc_autoinvoke, (gEnv->pSystem->IsDevMode() ? 1 : 0), VF_NULL,
	               "Enable calling the resource compiler (rc.exe) to compile assets at run-time if the date check\n"
	               "shows that the destination is older or does not exist.\n"
	               "Usage: r_RC_AutoInvoke 0 (default is 1)");

	DefineConstIntCVar3("r_NightVision", CV_r_NightVision, 2, VF_NULL,
	                    "Toggles nightvision enabling.\n"
	                    "Usage: r_NightVision [0/1]\n"
	                    "Default is 2 (HDR). "
	                    "Set to 1 (older version - kept for backward compatibility)"
	                    "Set to 3 to enable debug mode (force enabling)."
	                    "Set to 0 to completely disable nightvision. ");

	REGISTER_CVAR3("r_NightVisionFinalMul", CV_r_NightVisionFinalMul, 6.0f, VF_NULL,
	               "Set nightvision final color multiplier for fine tunning.\n");
	REGISTER_CVAR3("r_NightVisionBrightLevel", CV_r_NightVisionBrightLevel, 3.0f, VF_NULL,
	               "Set nightvision bloom brightlevel.\n");
	REGISTER_CVAR3("r_NightVisionSonarLifetime", CV_r_NightVisionSonarLifetime, 2.0f, VF_NULL,
	               "Set nightvision sonar hints lifetime.\n");
	REGISTER_CVAR3("r_NightVisionSonarMultiplier", CV_r_NightVisionSonarMultiplier, 0.2f, VF_NULL,
	               "Set nightvision sonar hints color multiplier.\n");
	REGISTER_CVAR3("r_NightVisionSonarRadius", CV_r_NightVisionSonarRadius, 32.0f, VF_NULL,
	               "Set nightvision sonar hints radius.\n");
	REGISTER_CVAR3("r_NightVisionCamMovNoiseAmount", CV_r_NightVisionCamMovNoiseAmount, 0.4f, VF_NULL,
	               "Set nightvision noise amount based on camera movement.\n");
	REGISTER_CVAR3("r_NightVisionCamMovNoiseBlendSpeed", CV_r_NightVisionCamMovNoiseBlendSpeed, 2.0f, VF_NULL,
	               "Set nightvision noise amount blend speed.\n");

	REGISTER_CVAR3("r_dofMinZ", CV_r_dofMinZ, 0.0f, VF_NULL,
	               "Set dof min z distance, anything behind this distance will get out focus. (good default value 0.4) \n");

	REGISTER_CVAR3("r_dofMinZScale", CV_r_dofMinZScale, 0.0f, VF_NULL,
	               "Set dof min z out of focus strenght (good default value - 1.0f)\n");

	REGISTER_CVAR3("r_dofMinZBlendMult", CV_r_dofMinZBlendMult, 1.0f, VF_NULL,
	               "Set dof min z blend multiplier (bigger value means faster blendind transition)\n");

	REGISTER_CVAR3("r_DepthOfFieldDilation", CV_r_dofDilation, 1.0f, VF_NULL,
	               "Sets dilation width to reduce ghosting artifacts on near objects (can introduce other artifacts)\n");

	DefineConstIntCVar3("r_SonarVision", CV_r_SonarVision, 1, VF_NULL,
	                    "Toggles sonar vision enabling.\n"
	                    "Usage: r_SonarVision [0/1]\n"
	                    "Default is 1 (on). Set to 2 to enable debug mode (force enabling). Set to 0 to completely disable sonar vision modes.");

	DefineConstIntCVar3("r_ThermalVision", CV_r_ThermalVision, 1, VF_NULL,
	                    "Toggles termal vision enabling.\n"
	                    "Usage: r_ThermalVision [0/1]\n"
	                    "Default is 1 (on). Set to 2 to enable debug mode (force enabling). Set to 0 to completely disable termal vision modes.");

	REGISTER_CVAR3_DEV_ONLY("r_ThermalVisionViewDistance", CV_r_ThermalVisionViewDistance, 150.0f, VF_NULL,
	                        "Toggles thermal vision distance attenuation.\n"
	                        "Default is 150 (meters)");

	DefineConstIntCVar3("r_ThermalVisionViewCloakFrequencyPrimary", CV_r_ThermalVisionViewCloakFrequencyPrimary, 1, VF_NULL,
	                    "Sets thermal vision cloaked-object flicker primary frequency.\n"
	                    "Usage: r_ThermalVisionViewCloakFrequencyPrimary [1+]\n"
	                    "When looking at a refracting (cloaked) object sets the inverse frequency of the primary sine wave for the objects heat. Higher = slower");

	DefineConstIntCVar3("r_ThermalVisionViewCloakFrequencySecondary", CV_r_ThermalVisionViewCloakFrequencySecondary, 1, VF_NULL,
	                    "Sets thermal vision cloaked-object flicker secondary frequency.\n"
	                    "Usage: r_ThermalVisionViewCloakFrequencySecondary [1+]\n"
	                    "When looking at a refracting (cloaked) object sets the inverse frequency of the secondary sine wave for the objects heat. Higher = slower");

	REGISTER_CVAR3("r_ThermalVisionViewCloakFlickerMinIntensity", CV_r_ThermalVisionViewCloakFlickerMinIntensity, 1.0f, VF_NULL,
	               "Sets thermal vision cloaked-object flicker random min intensity.\n"
	               "Usage: r_ThermalVisionViewCloakFlickerMinIntensity [0.0+]\n"
	               "When looking at a refracting (cloaked) object sets the min scaling factor at which the object displays hot in thermal vision");

	REGISTER_CVAR3("r_ThermalVisionViewCloakFlickerMaxIntensity", CV_r_ThermalVisionViewCloakFlickerMaxIntensity, 1.0f, VF_NULL,
	               "Sets thermal vision cloaked-object flicker random max intensity.\n"
	               "Usage: r_ThermalVisionViewCloakFlickerMaxIntensity [0.0+]\n"
	               "When looking at a refracting (cloaked) object sets the min scaling factor at which the object displays hot in thermal vision");

	REGISTER_CVAR3("r_Refraction", CV_r_Refraction, 1, VF_NULL,
	               "Enables refraction.\n"
	               "Usage: r_Refraction [0/1]\n"
	               "Default is 1 (on). Set to 0 to disable.");

	REGISTER_CVAR3("r_sunshafts", CV_r_sunshafts, SUNSHAFTS_DEFAULT_VAL, VF_NULL,
	               "Enables sun shafts.\n"
	               "Usage: r_sunshafts [0/1]\n"
	               "Default is 1 (on). Set to 0 to disable.");

	REGISTER_CVAR3_CB("r_PostProcessEffects", CV_r_PostProcess_CB, 1, VF_CHEAT,
	                  "Enables post processing special effects.\n"
	                  "Usage: r_PostProcessEffects [0/1/2]\n"
	                  "Default is 1 (enabled). 2 enables and displays active effects",
	                  OnChange_CV_r_PostProcess);
	CV_r_PostProcess = CV_r_PostProcess_CB;

	DefineConstIntCVar3("r_PostProcessParamsBlending", CV_r_PostProcessParamsBlending, 1, VF_NULL,
	                    "Enables post processing effects parameters smooth blending\n"
	                    "Usage: r_PostProcessEffectsParamsBlending [0/1]\n"
	                    "Default is 1 (enabled).");

	REGISTER_CVAR3("r_PostprocessParamsBlendingTimeScale", CV_r_PostprocessParamsBlendingTimeScale, 12.0f, VF_NULL,
	               "Sets post processing effects parameters smooth blending time scale\n"
	               "Usage: r_PostprocessParamsBlendingTimeScale [scale]\n"
	               "Default is 12.0f.");

	REGISTER_CVAR3("r_PostProcessFilters", CV_r_PostProcessFilters, 1, VF_NULL,
	                    "Enables post processing special effects filters.\n"
	                    "Usage: r_PostProcessEffectsFilters [0/1]\n"
	                    "Default is 1 (enabled). 0 disabled");

	DefineConstIntCVar3("r_PostProcessGameFx", CV_r_PostProcessGameFx, 1, VF_CHEAT,
	                    "Enables post processing special effects game fx.\n"
	                    "Usage: r_PostProcessEffectsGameFx [0/1]\n"
	                    "Default is 1 (enabled). 0 disabled");

	REGISTER_CVAR3("r_PostProcessReset", CV_r_PostProcessReset, 0, VF_CHEAT,
	               "Enables post processing special effects reset.\n"
	               "Usage: r_PostProcessEffectsReset [0/1]\n"
	               "Default is 0 (disabled). 1 enabled");

	DefineConstIntCVar3("r_MergeShadowDrawcalls", CV_r_MergeShadowDrawcalls, 1, VF_NULL,
	                    "Enabled Merging of RenderChunks for ShadowRendering\n"
	                    "Default is 1 (enabled). 0 disabled");

	DefineConstIntCVar3("r_PostProcessHUD3D", CV_r_PostProcessHUD3D, 1, VF_NULL,
	                    "Toggles 3d hud post processing.\n"
	                    "Usage: r_PostProcessHUD3D [0/1]\n"
	                    "Default is 1 (post process hud enabled). 0 Disabled");

	DefineConstIntCVar3("r_PostProcessHUD3DDebugView", CV_r_PostProcessHUD3DDebugView, 0, VF_NULL,
	                    "Debug views for 3d hud post processing.\n"
	                    "Usage: CV_r_PostProcessHUD3DDebugView [0/1/2/3]\n"
	                    "Default is 0 (disabled). 1 Solid fill. 2 Wire frame. 3 Unwrap mesh onto flash texture");

	REGISTER_CVAR3("r_PostProcessHUD3DCache", CV_r_PostProcessHUD3DCache, 2, VF_CVARGRP_IGNOREINREALVAL, // Needs to be ignored from cvar group state otherwise some adv graphic options will show as custom when using multi GPU due to mgpu.cfg being reloaded after changing syspec and graphic options have changed
	               "Enables 3d hud caching overframes.\n"
	               "Usage: r_PostProcessHUD3DCache [0/1/2/3]\n"
	               "Default is 0 (disabled). 1 Cache every 1 frame. 2 Every 2 frames. Etc");

	REGISTER_CVAR3("r_PostProcessHUD3DShadowAmount", CV_r_PostProcessHUD3DShadowAmount, 1.7f, VF_NULL,
	               "Controls 3D HUD 'Shadow' Amount.\n"
	               "Usage: r_PostProcessHUD3DShadowAmount [> 0.0]\n"
	               "Default is 1.7f, higher = darker");

	REGISTER_CVAR3("r_PostProcessHUD3DGlowAmount", CV_r_PostProcessHUD3DGlowAmount, 1.0f, VF_NULL,
	               "Controls 3D HUD 'Glow' Amount.\n"
	               "Usage: r_PostProcessHUD3DGlowAmount [> 0.0]\n"
	               "Default is 1.0f, higher = more glow");

	DefineConstIntCVar3("r_PostProcessHUD3DStencilClear", CV_r_PostProcessHUD3DStencilClear, 1, VF_NULL,
	                    "Enables stencil clears for flash masks when rendering HUD in 3D post process.\n"
	                    "Usage: r_PostProcessHUD3DNoStencilClear [0/1]\n"
	                    "Default is 1 (enabled), 0 disabled");

	DefineConstIntCVar3("r_PostProcessNanoGlassDebugView", CV_r_PostProcessNanoGlassDebugView, 0, VF_NULL,
	                    "Debug views for Nano Glass post processing.\n"
	                    "Usage: CV_r_PostProcessNanoGlassDebugView [0/1]\n"
	                    "Default is 0 (disabled). 1 Wire frame.");

	REGISTER_CVAR3("r_ColorRangeCompression", CV_r_colorRangeCompression, 0, VF_NULL,
	               "Enables color range compression to account for the limited RGB range of TVs.\n"
	               "  0: Disabled (full extended range)\n"
	               "  1: Range 16-235\n");

	REGISTER_CVAR3("r_ColorGrading", CV_r_colorgrading, COLOR_GRADING_DEFAULT_VAL, VF_NULL,
	               "Enables color grading.\n"
	               "Usage: r_ColorGrading [0/1]");

	REGISTER_CVAR3("r_ColorGradingSelectiveColor", CV_r_colorgrading_selectivecolor, 1, VF_NULL,
	               "Enables color grading.\n"
	               "Usage: r_ColorGradingSelectiveColor [0/1]");

	DefineConstIntCVar3("r_ColorGradingLevels", CV_r_colorgrading_levels, 1, VF_NULL,
	                    "Enables color grading.\n"
	                    "Usage: r_ColorGradingLevels [0/1]");

	DefineConstIntCVar3("r_ColorGradingFilters", CV_r_colorgrading_filters, 1, VF_NULL,
	                    "Enables color grading.\n"
	                    "Usage: r_ColorGradingFilters [0/1]");

	REGISTER_CVAR3("r_ColorGradingCharts", CV_r_colorgrading_charts, 1, VF_NULL,
	               "Enables color grading via color charts.\n"
	               "Usage: r_ColorGradingCharts [0/1]");

	REGISTER_CVAR3("r_ColorGradingChartsCache", CV_r_ColorgradingChartsCache, 4, VF_CVARGRP_IGNOREINREALVAL, // Needs to be ignored from cvar group state otherwise some adv graphic options will show as custom when using multi GPU due to mgpu.cfg being reloaded after changing syspec and graphic options have changed
	               "Enables color grading charts update caching.\n"
	               "Usage: r_ColorGradingCharts [0/1/2/etc]\n"
	               "Default is 4 (update every 4 frames), 0 - always update, 1- update every other frame");

	REGISTER_CVAR3("r_DynTexMaxSize", CV_r_dyntexmaxsize, 48, VF_NULL, ""); // 48 Mb
	DefineConstIntCVar3("r_TexPreallocateAtlases", CV_r_texpreallocateatlases, TEXPREALLOCATLAS_DEFAULT_VAL, VF_NULL, "");
	REGISTER_CVAR3("r_TexAtlasSize", CV_r_texatlassize, 1024, VF_NULL, ""); // 1024x1024

	DefineConstIntCVar3("r_TexPostponeLoading", CV_r_texpostponeloading, 1, VF_NULL, "");
	REGISTER_CVAR3("r_DynTexAtlasCloudsMaxSize", CV_r_dyntexatlascloudsmaxsize, 32, VF_NULL, "");       // 32 Mb
	REGISTER_CVAR3("r_DynTexAtlasSpritesMaxSize", CV_r_dyntexatlasspritesmaxsize, 32, VF_NULL, "");     // 32 Mb
	REGISTER_CVAR3("r_DynTexAtlasVoxTerrainMaxSize", CV_r_dyntexatlasvoxterrainsize, 250, VF_NULL, ""); // this pool is never preallocated
	REGISTER_CVAR3("r_DynTexAtlasDynTexSrcSize", CV_r_dyntexatlasdyntexsrcsize, 16, VF_NULL, "");       // this pool is never preallocated

	const int defaultbuffer_bank_size = 4;
	const int default_transient_bank_size = 4;
	const int default_cb_bank_size = 4;
	const int defaultt_cb_watermark = 64;
	const int defaultbuffer_pool_max_allocs = 0xfff0;
	const int defaultbuffer_pool_defrag = 0;

	REGISTER_CVAR3("r_buffer_banksize", CV_r_buffer_banksize, defaultbuffer_bank_size, VF_CHEAT, "the bank size in MB for buffer pooling");
	REGISTER_CVAR3("r_constantbuffer_banksize", CV_r_constantbuffer_banksize, default_cb_bank_size, VF_CHEAT, "the bank size in MB for constant buffers pooling");
	REGISTER_CVAR3("r_constantbuffer_watermarm", CV_r_constantbuffer_watermark, defaultt_cb_watermark, VF_CHEAT, "the threshold aftyer which constants buffers will reclaim memory");
	REGISTER_CVAR3("r_buffer_sli_workaround", CV_r_buffer_sli_workaround, 0, VF_NULL, "enable SLI workaround for buffer pooling");
	REGISTER_CVAR3("r_transient_pool_size", CV_r_transient_pool_size, default_transient_bank_size, VF_CHEAT, "the bank size in MB for the transient pool");
	DefineConstIntCVar3("r_buffer_enable_lockless_updates", CV_r_buffer_enable_lockless_updates, 1, VF_CHEAT, "enable/disable lockless buffer updates on platforms that support them");
	DefineConstIntCVar3("r_enable_full_gpu_sync", CV_r_enable_full_gpu_sync, 0, VF_CHEAT, "enable full gpu synchronization for debugging purposes on the every buffer I/O operation (debugging only)");
	REGISTER_CVAR3("r_buffer_pool_max_allocs", CV_r_buffer_pool_max_allocs, defaultbuffer_pool_max_allocs, VF_CHEAT, "the maximum number of allocations per buffer pool if defragmentation is enabled");
	REGISTER_CVAR3("r_buffer_pool_defrag_static", CV_r_buffer_pool_defrag_static, defaultbuffer_pool_defrag, VF_CHEAT, "enable/disable runtime defragmentation of static buffers");
	REGISTER_CVAR3("r_buffer_pool_defrag_dynamic", CV_r_buffer_pool_defrag_dynamic, defaultbuffer_pool_defrag, VF_CHEAT, "enable/disable runtime defragmentation of dynamic buffers");
	REGISTER_CVAR3("r_buffer_pool_defrag_max_moves", CV_r_buffer_pool_defrag_max_moves, 64, VF_CHEAT, "maximum number of moves the defragmentation system is allowed to perform per frame");

	REGISTER_CVAR3("r_TexMinAnisotropy", CV_r_texminanisotropy, 0, VF_REQUIRE_LEVEL_RELOAD,
	               "Specifies the minimum level allowed for anisotropic texture filtering.\n"
	               "0(default) means abiding by the filtering setting in each material, except possibly being capped by r_TexMaxAnisotropy.");
	REGISTER_CVAR3("r_TexMaxAnisotropy", CV_r_texmaxanisotropy, TEXMAXANISOTROPY_DEFAULT_VAL, VF_REQUIRE_LEVEL_RELOAD,
	               "Specifies the maximum level allowed for anisotropic texture filtering.");
	REGISTER_CVAR3("r_TexNoAnisoAlphaTest", CV_r_texNoAnisoAlphaTest, TEXNOANISOALPHATEST_DEFAULT_VAL, VF_DUMPTODISK,
	                    "Disables anisotropic filtering on alpha-tested geometry like vegetation.\n");
	DefineConstIntCVar3("r_TexLog", CV_r_texlog, 0, VF_NULL,
	                    "Configures texture information logging.\n"
	                    "Usage:	r_TexLog #\n"
	                    "where # represents:\n"
	                    "	0: Texture logging off\n"
	                    "	1: Texture information logged to screen\n"
	                    "	2: All loaded textures logged to 'UsedTextures.txt'\n"
	                    "	3: Missing textures logged to 'MissingTextures.txt");
	DefineConstIntCVar3("r_TexNoLoad", CV_r_texnoload, 0, VF_NULL,
	                    "Disables loading of textures.\n"
	                    "Usage:	r_TexNoLoad [0/1]\n"
	                    "When 1 texture loading is disabled.");

	REGISTER_CVAR3("r_RenderTargetPoolSize", CV_r_rendertargetpoolsize, 0, VF_NULL,
	               "Size of pool for render targets in MB.\n"
	               "Default is 50(MB).");

	int nDefaultDefragState = 0;
	int nDefaultTexPoolSize = 1024;

	REGISTER_CVAR3("r_TexturesStreamPoolSize", CV_r_TexturesStreamPoolSize, nDefaultTexPoolSize, VF_NULL,
	               "Size of texture streaming pool in MB.\n");

	REGISTER_CVAR3("r_TexturesStreamPoolSecondarySize", CV_r_TexturesStreamPoolSecondarySize, 0, VF_NULL,
	               "Size of secondary pool for textures in MB.");

	REGISTER_CVAR3("r_texturesstreampooldefragmentation", CV_r_texturesstreampooldefragmentation, nDefaultDefragState, VF_NULL,
	              "Enabled CPU (1), GPU(2) and disable (0) textures stream pool defragmentation.\n");
	REGISTER_CVAR3("r_texturesstreampooldefragmentationmaxmoves", CV_r_texturesstreampooldefragmentationmaxmoves, 8, VF_NULL,
	               "Specify the maximum number of blocks to move per defragmentation update");
	REGISTER_CVAR3("r_texturesstreampooldefragmentationmaxamount", CV_r_texturesstreampooldefragmentationmaxamount, 512 * 1024, VF_NULL,
	               "Specify the limit (in bytes) that defrag update will stop");

	REGISTER_CVAR3("r_TexturesStreamingMaxRequestedMB", CV_r_TexturesStreamingMaxRequestedMB, 2.f, VF_NULL,
	               "Maximum amount of texture data requested from streaming system per frame in MB.\n"
	               "Usage: r_TexturesStreamingMaxRequestedMB [size]\n"
	               "Default is 2.0(MB)");

	DefineConstIntCVar3("r_TexturesStreamingPostponeMips", CV_r_texturesstreamingPostponeMips, 0, VF_NULL,
	                    "Postpone loading of high res mipmaps to improve resolution ballance of texture streaming.\n"
	                    "Usage: r_TexturesStreamingPostponeMips [0/1]\n"
	                    "Default is 1 (on).");

	DefineConstIntCVar3("r_TexturesStreamingPostponeThresholdKB", CV_r_texturesstreamingPostponeThresholdKB, 1024, VF_NULL,
	                    "Threshold used to postpone high resolution mipmap loads in KB.\n"
	                    "Usage: r_TexturesStreamingPostponeThresholdKB [size]\n"
	                    "Default is 1024(KB)");
	DefineConstIntCVar3("r_texturesstreamingPostponeThresholdMip", CV_r_texturesstreamingPostponeThresholdMip, 1, VF_NULL,
	                    "Threshold used to postpone high resolution mipmaps.\n"
	                    "Usage: r_texturesstreamingPostponeThresholdMip [count]\n"
	                    "Default is 1");
	DefineConstIntCVar3("r_TexturesStreamingMinReadSizeKB", CV_r_texturesstreamingMinReadSizeKB, 64, VF_NULL,
	                    "Minimal streaming request size in KB.\n"
	                    "Usage: r_TexturesStreamingMinReadSizeKB [size]\n"
	                    "Default is 64KiBi");
	REGISTER_CVAR3("r_texturesstreamingSkipMips", CV_r_texturesstreamingSkipMips, 0, VF_NULL,
	               "Number of top mips to ignore when streaming.\n");
	REGISTER_CVAR3("r_texturesstreamingMinUsableMips", CV_r_texturesstreamingMinUsableMips, 7, VF_NULL,
	               "Minimum number of mips a texture should be able to use after applying r_texturesstreamingSkipMips.\n");
	REGISTER_CVAR3("r_texturesstreamingJobUpdate", CV_r_texturesstreamingJobUpdate, 1, VF_NULL,
	               "Enable texture streaming update job");
#if defined(TEXSTRM_DEFERRED_UPLOAD)
	REGISTER_CVAR3("r_texturesstreamingDeferred", CV_r_texturesstreamingDeferred, 1, VF_NULL,
	               "When enabled textures will be uploaded through a deferred context.\n");
#endif
#if defined(SUPPORTS_INPLACE_TEXTURE_STREAMING)
	REGISTER_CVAR3("r_texturesstreamingInPlace", CV_r_texturesstreamingInPlace, 1, VF_NULL,
	               "When enabled textures will stream directly into video memory.\n");
#endif
	REGISTER_CVAR3("r_TexturesStreamingMaxRequestedJobs", CV_r_TexturesStreamingMaxRequestedJobs, 32, VF_NULL,
	               "Maximum number of tasks submitted to streaming system.\n"
	               "Usage: r_TexturesStreamingMaxRequestedJobs [jobs number]\n"
	               "Default is 32 jobs");
#ifdef TEXSTRM_SUPPORT_REACTIVE
	DefineConstIntCVar3("r_TexturesStreamingUpdateType", CV_r_texturesstreamingUpdateType, TEXSTREAMING_UPDATETYPE_DEFAULT_VAL, VF_NULL,
	                    "Texture streaming update type.\n"
	                    "Default is 0\n"
	                    "0 - Use reactive texture streamer\n"
	                    "1 - Use planning texture streamer");
#else
	DefineConstIntCVar3("r_TexturesStreamingUpdateType", CV_r_texturesstreamingUpdateType, TEXSTREAMING_UPDATETYPE_DEFAULT_VAL, VF_NULL,
	                    "Texture streaming update type.\n"
	                    "Default is 0\n"
	                    "0 - Unavailable (maps to 1)\n"
	                    "1 - Use planning texture streamer");
#endif
	DefineConstIntCVar3("r_TexturesStreamingPrecacheRounds", CV_r_texturesstreamingPrecacheRounds, 1, VF_NULL,
	                    "Number of precache rounds to include in active streamed texture lists.\n"
	                    "Default is 1");
	DefineConstIntCVar3("r_TexturesStreamingSuppress", CV_r_texturesstreamingSuppress, 0, VF_NULL,
	                    "Force unloading of all textures and suppress new stream tasks.\n"
	                    "Default is 0");
	REGISTER_CVAR3("r_TexturesStreamingMipBias", CV_r_TexturesStreamingMipBias, 0, VF_NULL,
	               "Controls how texture LOD depends from distance to the objects.\n"
	               "Increasing this value will reduce amount of memory required for textures.\n"
	               "Usage: r_TexturesStreamingMipBias [-4..0..4]\n"
	               "Default is 0.");
	REGISTER_CVAR3("r_TexturesStreamingMipClampDVD", CV_r_TexturesStreamingMipClampDVD, 1, VF_NULL,
	               "Clamp the texture mip level to certain value when streaming from DVD. 1 will never allow highest mips to be loaded for example.\n"
	               "Usage: r_TexturesStreamingMipClampDVD [0..4]\n"
	               "Default is 1.");
	REGISTER_CVAR3("r_TexturesStreamingDisableNoStreamDuringLoad", CV_r_TexturesStreamingDisableNoStreamDuringLoad, 0, VF_NULL,
	               "Load time optimisation. When enabled, textures flagged as non-streaming will still be streamed during level load, but will have a "
	               "high priority stream request added in RT_Precache. Once streamed in, the texture will remain resident\n");
	DefineConstIntCVar3("r_TexturesStreamingMipFading", CV_r_texturesstreamingmipfading, 8, VF_NULL,
	                    "Controls how the new texture MIP appears after being streamed in.\n"
	                    "This variable influences only a visual quality of appearing texture details.\n"
	                    "Usage: r_TexturesStreamingMipFading [0/1...N]\n"
	                    "Default is 8 (fade-in in 8 frames time). N is the number of frames needed to fade-in all mips");
	DefineConstIntCVar3("r_TexturesStreamingNoUpload", CV_r_texturesstreamingnoupload, 0, VF_NULL,
	                    "Disable uploading data into texture from system memory. Useful for debug purposes.\n"
	                    "Usage: r_TexturesStreamingNoUpload [0/1]\n"
	                    "Default is 0 (off).");
	DefineConstIntCVar3("r_TexturesStreamingOnlyVideo", CV_r_texturesstreamingonlyvideo, 0, VF_NULL,
	                    "Don't store system memory copy of texture. Applicable only for PC.\n"
	                    "Usage: r_TexturesStreamingOnlyVideo [0/1]\n"
	                    "Default is 0 (off).");
	DefineConstIntCVar3("r_TexturesStreaming", CV_r_texturesstreaming, TEXSTREAMING_DEFAULT_VAL, VF_REQUIRE_APP_RESTART,
	                    "Enables direct streaming of textures from disk during game.\n"
	                    "Usage: r_TexturesStreaming [0/1/2]\n"
	                    "Default is 0 (off). All textures save in native format with mips in a\n"
	                    "cache file. Textures are then loaded into texture memory from the cache.\n"
	                    "1 - stream only mesh/material textures on-demand\n"
	                    "2 - stream also cube textures on-demand");
	DefineConstIntCVar3("r_TexturesStreamingLowestPrefetchBias", CV_r_TexturesStreamingLowestPrefetchBias, 0, VF_NULL,
	                    "Clamping texture prefetch to at most fetch this many more mips than requested if there's free pool memory.\n"
	                    "Usage: r_TexturesStreamingLowestPrefetchBias [-0...-inf]\n"
	                    "Default is 0 (don't prefetch).");
	DefineConstIntCVar3("r_TexturesStreamingMaxUpdateRate", CV_r_TexturesStreamingMaxUpdateRate, 1, VF_NULL,
	                    "Clamping texture stream in to at most fetch this many mips at the same time.\n"
	                    "Usage: r_TexturesStreamingMaxUpdateRate [1...inf]\n"
	                    "Default is 1 (stream in at most 1 mip per frame per texture).");

	DefineConstIntCVar3("r_TexturesStreamingDebug", CV_r_TexturesStreamingDebug, 0, VF_CHEAT,
	                    "Enables textures streaming debug mode. (Log uploads and remove unnecessary mip levels)\n"
	                    "Usage: r_TexturesStreamingDebug [0/1/2]\n"
	                    "Default is 0 (off).\n"
	                    "1 - texture streaming log.\n"
	                    "2 - Show textures hit-parade based on streaming priorities\n"
	                    "3 - Show textures hit-parade based on the memory consumed");
	CV_r_TexturesStreamingDebugfilter = REGISTER_STRING("r_TexturesStreamingDebugFilter", "", VF_CHEAT, "Filters displayed textures by name in texture streaming debug mode\n");
	REGISTER_CVAR3("r_TexturesStreamingDebugMinSize", CV_r_TexturesStreamingDebugMinSize, 100, VF_NULL,
	               "Filters displayed textures by size in texture streaming debug mode");
	REGISTER_CVAR3("r_TexturesStreamingDebugMinMip", CV_r_TexturesStreamingDebugMinMip, 100, VF_NULL,
	               "Filters displayed textures by loaded mip in texture streaming debug mode");
	DefineConstIntCVar3("r_TexturesStreamingDebugDumpIntoLog", CV_r_TexturesStreamingDebugDumpIntoLog, 0, VF_NULL,
	                    "Dump content of current texture streaming debug screen into log");
	REGISTER_CVAR3("r_TextureLodDistanceRatio", CV_r_TextureLodDistanceRatio, -1, VF_NULL,
	               "Controls dynamic LOD system for textures used in materials.\n"
	               "Usage: r_TextureLodDistanceRatio [-1, 0 and bigger]\n"
	               "Default is -1 (completely off). Value 0 will set full LOD to all textures used in frame.\n"
	               "Values bigger than 0 will activate texture LOD selection depending on distance to the objects.");

	DefineConstIntCVar3("r_TextureCompiling", CV_r_texturecompiling, 1, VF_NULL,
	                    "Enables Run-time compilation and subsequent injection of changed textures from disk during editing.\n"
	                    "Usage: r_TextureCompiling [0/1]\n"
	                    "Default is 1 (on). Changes are tracked and passed through to the rendering.\n"
	                    "Compilation can also be muted by the r_RC_AutoInvoke config.");
	DefineConstIntCVar3("r_TextureCompilingIndicator", CV_r_texturecompilingIndicator, 0, VF_NULL,
	                    "Replaces the textures which are currently compiled by a violet indicator-texture.\n"
	                    "Usage: r_TextureCompilingIndicator [-1/0/1]\n"
	                    "Default is 0 (off). Textures are silently replaced by their updated versions without any indication.\n"
	                    "Negative values will also stop show indicators for compilation errors.\n"
	                    "Positive values will show indicators whenever a texture is subject to imminent changes.\n");

#ifndef STRIP_RENDER_THREAD
	DefineConstIntCVar3("r_MultiThreaded", CV_r_multithreaded, MULTITHREADED_DEFAULT_VAL, VF_REQUIRE_APP_RESTART,
	                    "Enables dedicated render thread.\n"
	                    "  0: disabled\n"
	                    "  1: enabled\n"
	                    "  2: automatic detection");
#endif
	DefineConstIntCVar3("r_MultiThreadedDrawing", CV_r_multithreadedDrawing, -1, VF_NULL,
	                    "Enables submission of render items in dedicated jobs.\n"
	                    "  0: disabled\n"
	                    "  N: use specified number of concurrent draw recording jobs\n"
	                    " -1: automatically choose optimal number of recording jobs");
	DefineConstIntCVar3("r_MultiThreadedDrawingMinJobSize", CV_r_multithreadedDrawingMinJobSize, 100, VF_NULL,
	                    "Specifies threshold for creation of recording jobs.\n"
	                    "  0: no threshold\n"
	                    "  N: minimum number of render items per job\n");
	REGISTER_CVAR3("r_MultiGPU", CV_r_multigpu, 1, VF_NULL,
	               "Toggles MGPU support. Should usually be set before startup.\n"
	               "  0: force off\n"
	               "  1: automatic detection (reliable with SLI, does not respect driver app profiles with Crossfire)\n");
	DefineConstIntCVar3("r_ValidateDraw", CV_r_validateDraw, 0, VF_NULL,
	                    "0=disabled, 1=validate each DIP (meshes consistency, shaders, declarations, etc)");

	DefineConstIntCVar3("r_ShowNormals", CV_r_shownormals, 0, VF_CHEAT,
	                    "Toggles visibility of normal vectors.\n"
	                    "Usage: r_ShowNormals [0/1]"
	                    "Default is 0 (off).");
	DefineConstIntCVar3("r_ShowLines", CV_r_showlines, 0, VF_CHEAT,
	                    "Toggles visibility of wireframe overlay.\n"
	                    "Usage: r_ShowLines [0/1]\n"
	                    "Default is 0 (off).");
	REGISTER_CVAR3("r_NormalsLength", CV_r_normalslength, 0.2f, VF_CHEAT,
	               "Sets the length of displayed vectors.\n"
	               "r_NormalsLength 0.2\n"
	               "Default is 0.2 (meters). Used with r_ShowTangents and r_ShowNormals.");
	DefineConstIntCVar3("r_ShowTangents", CV_r_showtangents, 0, VF_CHEAT,
	                    "Toggles visibility of three tangent space vectors.\n"
	                    "Usage: r_ShowTangents [0/1]\n"
	                    "Default is 0 (off).");
	DefineConstIntCVar3("r_ShowTimeGraph", CV_r_showtimegraph, 0, VF_NULL,
	                    "Configures graphic display of frame-times.\n"
	                    "Usage: r_ShowTimeGraph [0/1/2]\n"
	                    "	1: Graph displayed as points."
	                    "	2: Graph displayed as lines."
	                    "Default is 0 (off).");
#ifndef EXCLUDE_DOCUMENTATION_PURPOSE
	DefineConstIntCVar3("r_DebugFontRendering", CV_r_DebugFontRendering, 0, VF_CHEAT,
	                    "0=off, 1=display various features of the font rendering to verify function and to document usage");
#endif // EXCLUDE_DOCUMENTATION_PURPOSE
	DefineConstIntCVar3("profileStreaming", CV_profileStreaming, 0, VF_NULL,
	                    "Profiles streaming of different assets.\n"
	                    "Usage: profileStreaming [0/1/2]\n"
	                    "	1: Graph displayed as points."
	                    "	2: Graph displayed as lines."
	                    "Default is 0 (off).");
	DefineConstIntCVar3("r_GraphStyle", CV_r_graphstyle, 0, VF_NULL, "");
	DefineConstIntCVar3("r_ShowBufferUsage", CV_r_showbufferusage, 0, VF_NULL,
	                    "Shows usage of statically allocated buffers.\n"
	                    "Usage: r_ShowBufferUSage [0/1]\n"
	                    "Default is 0 (off).");
	REGISTER_CVAR3_CB("r_LogVBuffers", CV_r_logVBuffers, 0, VF_CHEAT | VF_CONST_CVAR,
	                  "Logs vertex buffers in memory to 'LogVBuffers.txt'.\n"
	                  "Usage: r_LogVBuffers [0/1]\n"
	                  "Default is 0 (off).",
	                  GetLogVBuffersStatic);
	DefineConstIntCVar3("r_LogTexStreaming", CV_r_logTexStreaming, 0, VF_CHEAT,
	                    "Logs streaming info to Direct3DLogStreaming.txt\n"
	                    "0: off\n"
	                    "1: normal\n"
	                    "2: extended");
	DefineConstIntCVar3("r_LogShaders", CV_r_logShaders, 0, VF_CHEAT,
	                    "Logs shaders info to Direct3DLogShaders.txt\n"
	                    "0: off\n"
	                    "1: normal\n"
	                    "2: extended");

	DefineConstIntCVar3("r_SyncToFrameFence", CV_r_SyncToFrameFence, 1, VF_CHEAT,
	                    "Stall the render thread until GPU finished processing previous frame");

	REGISTER_CVAR3_CB("r_MaxFrameLatency", CV_r_MaxFrameLatency, 1, VF_NULL, 
	                  "Maximum number of frames that can be in-flight on the GPU",
	                   OnChange_CV_r_MaxFrameLatency);

	DefineConstIntCVar3("r_ShadersDebug", CV_r_shadersdebug, 0, VF_DUMPTODISK,
	                    "Enable special logging when shaders become compiled\n"
	                    "Usage: r_ShadersDebug [0/1/2/3/4]\n"
	                    " 1 = assembly into directory Main/{Game}/shaders/cache/d3d9\n"
	                    " 2 = compiler input into directory Main/{Game}/testcg\n"
	                    " 3 = compiler input with debug information (useful for PIX etc./{Game}/testcg_1pass\n"
	                    " 4 = compiler input with debug information, but optimized shaders\n"
	                    "Default is 0 (off)");

	DefineConstIntCVar3("r_ShadersIgnoreIncludesChanging", CV_r_shadersignoreincludeschanging, 0, VF_NULL, "");
	DefineConstIntCVar3("r_ShadersLazyUnload", CV_r_shaderslazyunload, 0, VF_NULL, "");
	DefineConstIntCVar3("r_ShadersCompileStrict", CV_r_shadersCompileStrict, 0, VF_NULL, "");
	DefineConstIntCVar3("r_ShadersCompileCompatible", CV_r_shadersCompileCompatible, 1, VF_NULL, "");

	REGISTER_CVAR3_CB("r_ShadersAllowCompilation", CV_r_shadersAllowCompilation, SHADERS_ALLOW_COMPILATION_DEFAULT_VAL, VF_NULL, "", OnChange_CV_r_ShadersAllowCompiliation);

	REGISTER_CVAR3("r_ShadersRemoteCompiler", CV_r_shadersremotecompiler, 0, VF_DUMPTODISK, "Enables remote shader compilation on dedicated machine");
	REGISTER_CVAR3("r_ShadersAsyncCompiling", CV_r_shadersasynccompiling, 3, VF_NULL,
	               "Enable asynchronous shader compiling\n"
	               "Usage: r_ShadersAsyncCompiling [0/1/2/3]\n"
	               " 0 = off, (stalling) shaders compiling\n"
	               " 1 = on, shaders are compiled in parallel, missing shaders are rendered in yellow\n"
	               " 2 = on, shaders are compiled in parallel, missing shaders are not rendered\n"
	               " 3 = on, shaders are compiled in parallel in precache mode");
	REGISTER_CVAR3("r_ShadersAsyncActivation", CV_r_shadersasyncactivation, 0, VF_NULL,
	               "Enable asynchronous shader activation\n"
	               "Usage: r_ShadersAsyncActivation [0/1]\n"
	               " 0 = off, (stalling) synchronous shaders activation\n"
	               " 1 = on, shaders are activated/streamed asynchronously\n");

	DefineConstIntCVar3("r_ShadersEditing", CV_r_shadersediting, 0, VF_NULL, "Force all cvars to settings, which allow shader editing");

	DefineConstIntCVar3("r_ShadersCompileAutoActivate", CV_r_shaderscompileautoactivate, 0, VF_NULL, "Automatically reenable shader compilation if outdated shader is detected");

	REGISTER_CVAR3_DEV_ONLY("r_CloakLightScale", CV_r_cloakLightScale, 0.25f, VF_NULL, "Cloak light scale - default = 0.25");
	REGISTER_CVAR3_DEV_ONLY("r_CloakTransitionLightScale", CV_r_cloakTransitionLightScale, 1.0f, VF_NULL, "Cloak transition light scale - default = 1.0");
	REGISTER_CVAR3_DEV_ONLY("r_CloakFadeByDist", CV_r_cloakFadeByDist, 0, VF_NULL, "Cloak fade by distance - default = false");
	REGISTER_CVAR3_DEV_ONLY("r_CloakFadeLightScale", CV_r_cloakFadeLightScale, 0.25f, VF_NULL, "Cloak fade light scale - default = 0.25");
	REGISTER_CVAR3_DEV_ONLY("r_CloakFadeMinDistSq", CV_r_cloakFadeStartDistSq, 1.0f, VF_NULL, "Cloak fade start distance squared - default = 1.0");
	REGISTER_CVAR3_DEV_ONLY("r_CloakFadeMaxDistSq", CV_r_cloakFadeEndDistSq, 25.0f, VF_NULL, "Cloak fade end distance squared - default = 25.0");
	REGISTER_CVAR3_DEV_ONLY("r_CloakFadeMinValue", CV_r_cloakFadeMinValue, 0.2f, VF_NULL, "Cloak fade minimum value (between 0 and 1) - default = 0.2");
	REGISTER_CVAR3_DEV_ONLY("r_CloakRefractionFadeByDist", CV_r_cloakRefractionFadeByDist, 0, VF_NULL, "Cloak refraction fade by distance - default = false");
	REGISTER_CVAR3_DEV_ONLY("r_CloakRefractionFadeMinDistSq", CV_r_cloakRefractionFadeStartDistSq, 1.0f, VF_NULL, "Cloak refraction fade start distance squared - default = 1.0");
	REGISTER_CVAR3_DEV_ONLY("r_CloakRefractionFadeMaxDistSq", CV_r_cloakRefractionFadeEndDistSq, 25.0f, VF_NULL, "Cloak refraction fade end distance squared - default = 25.0");
	REGISTER_CVAR3_DEV_ONLY("r_CloakRefractionFadeMinValue", CV_r_cloakRefractionFadeMinValue, 0.2f, VF_NULL, "Cloak refraction fade minimum value (between 0 and 1) - default = 0.2");
	REGISTER_CVAR3_DEV_ONLY("r_CloakMinLightValue", CV_r_cloakMinLightValue, 0.0f, VF_NULL, "Cloak min light value - default = 0.0");
	REGISTER_CVAR3_DEV_ONLY("r_CloakHeatScale", CV_r_cloakHeatScale, 1.0f, VF_NULL, "Cloak heat scale for thermal vision - default = 1.0");
	REGISTER_CVAR3_DEV_ONLY("r_CloakRenderInThermalVision", CV_r_cloakRenderInThermalVision, 0, VF_NULL, "Render cloak in thermal vision - default = 0");
	REGISTER_CVAR3_DEV_ONLY("r_CloakMinAmbientOutdoors", CV_r_cloakMinAmbientOutdoors, 0.3f, VF_NULL, "Cloak minimum ambient outdoors");
	REGISTER_CVAR3_DEV_ONLY("r_CloakMinAmbientIndoors", CV_r_cloakMinAmbientIndoors, 0.3f, VF_NULL, "Cloak minimum ambient indoors");
	REGISTER_CVAR3_DEV_ONLY("r_CloakSparksAlpha", CV_r_cloakSparksAlpha, 1.0f, VF_NULL, "Cloak sparks alpha");
	REGISTER_CVAR3_DEV_ONLY("r_CloakInterferenceSparksAlpha", CV_r_cloakInterferenceSparksAlpha, 1000.0f, VF_NULL, "Cloak interference sparks alpha");
	REGISTER_CVAR3_DEV_ONLY("r_cloakHighlightStrength", CV_r_cloakHighlightStrength, 100.0f, VF_NULL, "Cloak hightlight strength");
	REGISTER_CVAR3("r_ArmourPulseSpeedMultiplier", CV_r_armourPulseSpeedMultiplier, 1.0f, VF_NULL, "Armour pulse speed multiplier - default = 1.0");
	REGISTER_CVAR3("r_MaxSuitPulseSpeedMultiplier", CV_r_maxSuitPulseSpeedMultiplier, 1.0f, VF_NULL, "Max suit pulse speed multiplier - default = 1.0");

	DefineConstIntCVar3("r_ReflectTextureSlots", CV_r_ReflectTextureSlots, 1, VF_NULL, "Reflect texture slot information from shader");

	REGISTER_CVAR3("r_ShadersAsyncMaxThreads", CV_r_shadersasyncmaxthreads, 1, VF_DUMPTODISK, "");
	REGISTER_CVAR3("r_ShadersCacheDeterministic", CV_r_shaderscachedeterministic, 1, VF_NULL, "Ensures that 2 shaderCaches built from the same source are binary equal");
	REGISTER_CVAR3("r_ShadersCachePrecacheAll", CV_r_ShadersCachePrecacheAll, 1, VF_NULL, "Precaches all possible combinations for every shader instance");
	DefineConstIntCVar3("r_ShadersPrecacheAllLights", CV_r_shadersprecachealllights, 1, VF_NULL, "");
	REGISTER_CVAR3("r_ShadersSubmitRequestline", CV_r_shaderssubmitrequestline, 1, VF_NULL, "");

	bool bShaderLogCacheMisses = true;
#if defined(_RELEASE)
	bShaderLogCacheMisses = false;
#endif
	REGISTER_CVAR3("r_ShadersLogCacheMisses", CV_r_shaderslogcachemisses, bShaderLogCacheMisses ? 2 : 0, VF_NULL,
	               "Log all shader caches misses on HD (both level and global shader cache misses).\n"
	               "0 = No logging to disk or TTY\n"
	               "1 = Logging to disk only\n"
	               "2 = Logging to disk and TTY (default)");

	REGISTER_CVAR3("r_ShadersImport", CV_r_shadersImport, 0, VF_NULL,
	               "0 off, 1 import and allow fallback to getBinShader, 2 import, no fallback if import fails (optimal).");

	REGISTER_CVAR3("r_ShadersExport", CV_r_shadersExport, 1, VF_NULL,
	               "0 off, 1 allow shader export during shader cache generation - Currently unavailable.");

	DefineConstIntCVar3("r_DebugRenderMode", CV_r_debugrendermode, 0, VF_CHEAT, "");
	DefineConstIntCVar3("r_DebugRefraction", CV_r_debugrefraction, 0, VF_CHEAT,
	                    "Debug refraction usage. Displays red instead of refraction\n"
	                    "Usage: r_DebugRefraction\n"
	                    "Default is 0 (off)");

	DefineConstIntCVar3("r_MeshPrecache", CV_r_meshprecache, 1, VF_NULL, "");
	REGISTER_CVAR3("r_MeshPoolSize", CV_r_meshpoolsize, RENDERER_DEFAULT_MESHPOOLSIZE, VF_NULL,
	               "The size of the pool for render data in kilobytes. "
	               "Disabled by default on PC (mesh data allocated on heap)."
	               "Enabled by default on consoles. Requires app restart to change.");
	REGISTER_CVAR3("r_MeshInstancePoolSize", CV_r_meshinstancepoolsize, RENDERER_DEFAULT_MESHINSTANCEPOOLSIZE, VF_NULL,
	               "The size of the pool for volatile render data in kilobytes. "
	               "Disabled by default on PC (mesh data allocated on heap)."
	               "Enabled by default on consoles. Requires app restart to change.");

	CV_r_excludeshader = REGISTER_STRING("r_ExcludeShader", "0", VF_CHEAT,
	                                     "Exclude the named shader from the render list.\n"
	                                     "Usage: r_ExcludeShader ShaderName\n"
	                                     "Sometimes this is useful when debugging.");

	CV_r_excludemesh = REGISTER_STRING("r_ExcludeMesh", "", VF_CHEAT,
	                                   "Exclude or ShowOnly the named mesh from the render list.\n"
	                                   "Usage: r_ExcludeShader Name\n"
	                                   "Usage: r_ExcludeShader !Name\n"
	                                   "Sometimes this is useful when debugging.");

	/*  REGISTER_CVAR3("r_EnvCMWrite", CV_r_envcmwrite, 0, VF_NULL,
	   "Writes cube-map textures to disk.\n"
	   "Usage: r_EnvCMWrite [0/1]\n"
	   "Default is 0 (off). The textures are written to 'Cube_posx.jpg'\n"
	   "'Cube_negx.jpg',...,'Cube_negz.jpg'. At least one of the real-time\n"
	   "cube-map shaders should be present in the current scene.");
	 */
	REGISTER_CVAR3("r_EnvCMResolution", CV_r_envcmresolution, 1, VF_DUMPTODISK,
	               "Sets resolution for target environment cubemap, in pixels.\n"
	               "Usage: r_EnvCMResolution #\n"
	               "where # represents:\n"
	               "	0: 64\n"
	               "	1: 128\n"
	               "	2: 256\n"
	               "Default is 2 (256 by 256 pixels).");

	REGISTER_CVAR3("r_EnvTexResolution", CV_r_envtexresolution, ENVTEXRES_DEFAULT_VAL, VF_DUMPTODISK,
	               "Sets resolution for 2d target environment texture, in pixels.\n"
	               "Usage: r_EnvTexResolution #\n"
	               "where # represents:\n"
	               "	0: 64\n"
	               "	1: 128\n"
	               "	2: 256\n"
	               "	3: 512\n"
	               "Default is 3 (512 by 512 pixels).");

	REGISTER_CVAR3("r_WaterUpdateDistance", CV_r_waterupdateDistance, 2.0f, VF_NULL, "");

	REGISTER_CVAR3("r_WaterUpdateFactor", CV_r_waterupdateFactor, 0.01f, VF_DUMPTODISK | VF_CVARGRP_IGNOREINREALVAL, // Needs to be ignored from cvar group state otherwise some adv graphic options will show as custom when using multi GPU due to mgpu.cfg being reloaded after changing syspec and graphic options have changed,
	               "Distance factor for water reflected texture updating.\n"
	               "Usage: r_WaterUpdateFactor 0.01\n"
	               "Default is 0.01. 0 means update every frame");

	REGISTER_CVAR3("r_EnvTexUpdateInterval", CV_r_envtexupdateinterval, 0.001f, VF_DUMPTODISK,
	               "Sets the interval between environmental 2d texture updates.\n"
	               "Usage: r_EnvTexUpdateInterval 0.001\n"
	               "Default is 0.001.");

	REGISTER_CVAR3("r_WaterReflections", CV_r_waterreflections, 1, VF_DUMPTODISK,
	                    "Toggles water reflections.\n"
	                    "Usage: r_WaterReflections [0/1]\n"
	                    "Default is 1 (water reflects).");

	// deprecated - we update every frame now on mgpu mode - todo: remove all related code
	DefineConstIntCVar3("r_WaterReflectionsMGPU", CV_r_waterreflections_mgpu, 0, VF_DUMPTODISK,
	                    "Toggles water reflections.multi-gpu support\n"
	                    "Usage: r_WaterReflectionsMGPU [0/1/2]\n"
	                    "Default is 0 (single render update), 1 (multiple render updates)");

	DefineConstIntCVar3("r_WaterReflectionsQuality", CV_r_waterreflections_quality, WATERREFLQUAL_DEFAULT_VAL, VF_DUMPTODISK,
	                    "Activates water reflections quality setting.\n"
	                    "Usage: r_WaterReflectionsQuality [0/1/2/3]\n"
	                    "Default is 0 (terrain only), 1 (terrain + particles), 2 (terrain + particles + brushes), 3 (everything)");

	DefineConstIntCVar3("r_WaterReflectionsUseMinOffset", CV_r_waterreflections_use_min_offset, 1, VF_DUMPTODISK,
	                    "Activates water reflections use min distance offset.");

	REGISTER_CVAR3("r_WaterReflectionsMinVisiblePixelsUpdate", CV_r_waterreflections_min_visible_pixels_update, 0.05f, VF_DUMPTODISK,
	               "Activates water reflections if visible pixels above a certain threshold.");

	REGISTER_CVAR3("r_WaterReflectionsMinVisUpdateFactorMul", CV_r_waterreflections_minvis_updatefactormul, 20.0f, VF_DUMPTODISK,
	               "Activates update factor multiplier when water mostly occluded.");
	REGISTER_CVAR3("r_WaterReflectionsMinVisUpdateDistanceMul", CV_r_waterreflections_minvis_updatedistancemul, 10.0f, VF_DUMPTODISK,
	               "Activates update distance multiplier when water mostly occluded.");

	REGISTER_CVAR3("r_WaterCaustics", CV_r_watercaustics, 1, VF_RENDERER_CVAR,
	               "Toggles under water caustics.\n"
	               "Usage: r_WaterCaustics [0/1]\n"
	               "Default is 1 (enabled).");

	REGISTER_CVAR3("r_WaterCausticsDistance", CV_r_watercausticsdistance, 100.0f, VF_NULL,
	               "Toggles under water caustics max distance.\n"
	               "Usage: r_WaterCausticsDistance\n"
	               "Default is 100.0 meters");

	REGISTER_CVAR3("r_WaterCausticsDeferred", CV_r_watercausticsdeferred, 2, VF_NULL,
	               "Toggles under water caustics deferred pass.\n"
	               "Usage: r_WaterCausticsDeferred [0/1/2]\n"
	               "Default is 0 (disabled). 1 - enables. 2 - enables with stencil pre-pass");

	REGISTER_CVAR3("r_WaterVolumeCaustics", CV_r_watervolumecaustics, WATERVOLCAUSTICS_DEFAULT_VAL, VF_NULL,
	               "Toggles advanced water caustics for watervolumes.\n"
	               "Usage: r_WaterVolumeCaustics [0/1]\n"
	               "Default is 0 (disabled). 1 - enables.");

	REGISTER_CVAR3("r_WaterVolumeCausticsDensity", CV_r_watervolumecausticsdensity, 128, VF_NULL,
	               "Density/resolution of watervolume caustic grid.\n"
	               "Usage: r_WaterVolumeCausticsDensity [16/256]\n"
	               "Default is 256");

	REGISTER_CVAR3("r_WaterVolumeCausticsRes", CV_r_watervolumecausticsresolution, 512, VF_NULL,
	               "Resolution of watervoluem caustics texture.\n"
	               "Usage: r_WaterVolumeCausticsRes [n]\n"
	               "Default is 1024");

	REGISTER_CVAR3("r_WaterVolumeCausticsSnapFactor", CV_r_watervolumecausticssnapfactor, 1.0f, VF_NULL,
	               "Distance in which to snap the vertex grid/projection (to avoid aliasing).\n"
	               "Usage: r_WaterVolumeCausticsSnapFactor [n]\n"
	               "Default is 1.0");

	REGISTER_CVAR3("r_WaterVolumeCausticsMaxDist", CV_r_watervolumecausticsmaxdistance, 35.0f, VF_NULL,
	               "Maximum distance in which caustics are visible.\n"
	               "Usage: r_WaterVolumeCausticsMaxDist [n]\n"
	               "Default is 35");

	REGISTER_CVAR3("r_WaterGodRays", CV_r_water_godrays, 1, VF_NULL,
	                    "Enables under water god rays.\n"
	                    "Usage: r_WaterGodRays [0/1]\n"
	                    "Default is 1 (enabled).");

	REGISTER_CVAR3("r_WaterGodRaysDistortion", CV_r_water_godrays_distortion, 1, VF_NULL,
	               "Set the amount of distortion when underwater.\n"
	               "Usage: r_WaterGodRaysDistortion [n]\n"
	               "Default is 1.");

	REGISTER_CVAR3("r_WaterUpdateThread", CV_r_WaterUpdateThread, 5, VF_NULL,
	               "Enables water updating on separate thread (when MT supported).\n"
	               "Usage: r_WaterUpdateThread [0/1/2/3/4/n]\n"
	               "Default is 5 (enabled and on 5 hw thread).");

	REGISTER_CVAR3("r_Reflections", CV_r_reflections, 1, VF_DUMPTODISK,
	                    "Toggles reflections.\n"
	                    "Usage: r_Reflections [0/1]\n"
	                    "Default is 1 (reflects).");

	REGISTER_CVAR3("r_ReflectionsOffset", CV_r_waterreflections_offset, 0.0f, VF_NULL, "");

	REGISTER_CVAR3("r_ReflectionsQuality", CV_r_reflections_quality, 3, VF_DUMPTODISK,
	                    "Toggles reflections quality.\n"
	                    "Usage: r_ReflectionsQuality [0/1/2/3]\n"
	                    "Default is 0 (terrain only), 1 (terrain + particles), 2 (terrain + particles + brushes), 3 (everything)");

	DefineConstIntCVar3("r_DetailTextures", CV_r_detailtextures, 1, VF_DUMPTODISK,
	                    "Toggles detail texture overlays.\n"
	                    "Usage: r_DetailTextures [0/1]\n"
	                    "Default is 1 (detail textures on).");

	DefineConstIntCVar3("r_ReloadShaders", CV_r_reloadshaders, 0, VF_CHEAT,
	                    "Reloads shaders.\n"
	                    "Usage: r_ReloadShaders [0/1]\n"
	                    "Default is 0. Set to 1 to reload shaders.");

	REGISTER_CVAR3("r_DetailDistance", CV_r_detaildistance, 6.0f, VF_DUMPTODISK,
	               "Distance used for per-pixel detail layers blending.\n"
	               "Usage: r_DetailDistance (1-20)\n"
	               "Default is 6.");

	DefineConstIntCVar3("r_TexBindMode", CV_r_texbindmode, 0, VF_CHEAT,
	                    "Enable texture overrides.\n"
	                    "Usage: r_TexBindMode [0/1/2/4/5/6/7/8/9/10/11]\n"
	                    "\t1 - Force gray non-font maps\n"
	                    "\t5 - Force flat normal maps\n"
	                    "\t6 - Force white diffuse maps\n"
	                    "\t7 - Force diffuse maps to use mipmapdebug texture\n"
	                    "\t8 - Colour code diffuse maps to show minimum uploaded mip [0:green,1:cyan,2:blue,3:purple,4:magenta,5:yellow,6:orange,7:red,higher:white]\n"
	                    "\t9 - Colour code diffuse maps to show textures streaming in in green and out in red\n"
	                    "\t10 - Colour code diffuse maps that have requested a lower mip than the lowest available [-3: red, -2: yellow, -1: green]\n"
	                    "\t11 - Force white diffuse map and flat normal map\n"
	                    "\t12 - Visualise textures that have more or less mips in memory than needed\n"
	                    "Default is 0 (disabled).");
	
	REGISTER_CVAR3("r_DrawNearShadows", CV_r_DrawNearShadows, 0, VF_NULL,
	               "Enable shadows for near objects.\n"
	               "Usage: r_DrawNearShadows [0/1]\n");
	REGISTER_CVAR3("r_NoDrawNear", CV_r_nodrawnear, 0, VF_RENDERER_CVAR,
	               "Disable drawing of near objects.\n"
	               "Usage: r_NoDrawNear [0/1]\n"
	               "Default is 0 (near objects are drawn).");
	REGISTER_CVAR3("r_DrawNearZRange", CV_r_DrawNearZRange, 0.12f, VF_NULL,
	               "Default is 0.1.");
	REGISTER_CVAR3("r_DrawNearFarPlane", CV_r_DrawNearFarPlane, 40.0f, VF_NULL,
	               "Default is 40.");
	REGISTER_CVAR3("r_DrawNearFoV", CV_r_drawnearfov, 60.0f, VF_NULL,
	               "Sets the FoV for drawing of near objects.\n"
	               "Usage: r_DrawNearFoV [n]\n"
	               "Default is 60.");

	REGISTER_CVAR3("r_Flares", CV_r_flares, FLARES_DEFAULT_VAL, VF_DUMPTODISK,
	               "Toggles lens flare effect.\n"
	               "Usage: r_Flares [0/1]\n"
	               "Default is 1 (on).");

	REGISTER_CVAR3("r_FlareHqShafts", CV_r_flareHqShafts, FLARES_HQSHAFTS_DEFAULT_VAL, VF_DUMPTODISK,
	                    "Toggles high quality mode for point light shafts.\n"
	                    "Usage: r_FlareHqShafts [0/1]\n"
	                    "Default is 1 (on).");

	REGISTER_CVAR3("r_FlaresChromaShift", CV_r_FlaresChromaShift, 6.0f, VF_NULL,
	               "Set flares chroma shift amount.\n"
	               "Usage: r_FlaresChromaShift [n]\n"
	               "Default is 6\n"
	               "0 Disables");

	REGISTER_CVAR3("r_FlaresIrisShaftMaxPolyNum", CV_r_FlaresIrisShaftMaxPolyNum, 200, VF_NULL,
	               "Set the maximum number of polygon of IrisShaft.\n"
	               "Usage : r_FlaresIrisShaftMaxPolyNum [n]\n"
	               "Default is 200\n"
	               "0 Infinite");

	REGISTER_CVAR3("r_FlaresEnableColorGrading", CV_r_FlaresEnableColorGrading, 1, VF_NULL,
				   "Toggles color grading on lens flares.\n"
				   "Usage : r_FlaresEnableColorGrading [n]\n"
				   "Default is 1 (on).");

	REGISTER_CVAR3_CB("r_FlaresTessellationRatio", CV_r_FlaresTessellationRatio, 1, VF_NULL,
	                  "Set the tessellation rate of flares. 1 is the original mesh.\n"
	                  "Usage : r_FlaresTessellationRatio 0.5\n"
	                  "Default is 1.0\n"
	                  "Range is from 0 to 1", OnChange_CV_r_FlaresTessellationRatio);

	REGISTER_CVAR3("r_Gamma", CV_r_gamma, 1.0f, VF_DUMPTODISK,
	               "Adjusts the graphics card gamma correction (fast, needs hardware support, affects also HUD and desktop)\n"
	               "Usage: r_Gamma 1.0\n"
	               "1 off (default)");
	REGISTER_CVAR3("r_Brightness", CV_r_brightness, 0.5f, VF_DUMPTODISK,
	               "Sets the display brightness.\n"
	               "Usage: r_Brightness 0.5\n"
	               "Default is 0.5.");
	REGISTER_CVAR3("r_Contrast", CV_r_contrast, 0.5f, VF_DUMPTODISK,
	               "Sets the display contrast.\n"
	               "Usage: r_Contrast 0.5\n"
	               "Default is 0.5.");

	DefineConstIntCVar3("r_NoHWGamma", CV_r_nohwgamma, 2, VF_DUMPTODISK,
	                    "Sets renderer to ignore hardware gamma correction.\n"
	                    "Usage: r_NoHWGamma [0/1/2]\n"
	                    "0 - allow hardware gamma correction\n"
	                    "1 - disable hardware gamma correction\n"
	                    "2 - disable hardware gamma correction in Editor\n");

	REGISTER_CVAR3("r_Scissor", CV_r_scissor, 1, VF_RENDERER_CVAR, "Enables scissor test");

	DefineConstIntCVar3("r_wireframe", CV_r_wireframe, R_SOLID_MODE, VF_CHEAT, "Toggles wireframe rendering mode");

	REGISTER_CVAR3("r_GetScreenShot", CV_r_GetScreenShot, 0, VF_NULL,
	               "To capture one screenshot (variable is set to 0 after capturing)\n"
	               "0=do not take a screenshot (default), 1=save a screenshot (together with .HDR if enabled), 2=save a screenshot");

	DefineConstIntCVar3("r_Character_NoDeform", CV_r_character_nodeform, 0, VF_NULL, "");

	DefineConstIntCVar3("r_Log", CV_r_log, 0, VF_CHEAT,
	                    "Logs rendering information to Direct3DLog.txt.\n"
	                    "Use negative values to log a single frame.\n"
	                    "Usage: r_Log +/-[0/1/2/3/4]\n"
	                    "	1: Logs a list of all shaders without profile info.\n"
	                    "	2: Log contains a list of all shaders with profile info.\n"
	                    "	3: Logs all API function calls.\n"
	                    "	4: Highly detailed pipeline log, including all passes,\n"
	                    "			states, lights and pixel/vertex shaders.\n"
	                    "Default is 0 (off). Use this function carefully, because\n"
	                    "log files grow very quickly.");

	DefineConstIntCVar3("r_LogVidMem", CV_r_logVidMem, 0, VF_CHEAT,
	                    "Logs vid mem information to VidMemLog.txt.");

	DefineConstIntCVar3("r_Stats", CV_r_stats, 0, VF_CHEAT,
	                    "Toggles render statistics.\n"
	                    "0=disabled,\n"
	                    "1=global render stats,\n"
	                    "2=print shaders for selected object,\n"
	                    "3=CPU times of render passes and video memory usage,\n"
	                    "4=CPU times of render passes,\n"
	                    "5=Occlusion query calls (calls to mfDraw/mfReadResult_Now/mfReadResult_Try),\n"
	                    "6=display per-instance drawcall count,\n"
	                    "8=Info about instanced DIPs,\n"
	                    "13=print info about cleared RT's,\n"
	                    "Usage: r_Stats [0/1/n]");

	DefineConstIntCVar3("r_statsMinDrawCalls", CV_r_statsMinDrawcalls, 0, VF_CHEAT,
	                    "Minimum drawcall amount to display for use with r_Stats 6");

	DefineConstIntCVar3("r_profiler", CV_r_profiler, 0, VF_NULL,
	                    "Display render pipeline profiler.\n"
	                    "  0: Disabled\n"
	                    "  1: Basic overview\n"
	                    "  2: Detailed pass stats\n");

	REGISTER_CVAR3("r_HDRDithering", CV_r_HDRDithering, 1, VF_NULL,
					"Toggles hdr dithering.\n"
					"  0: disabled\n"
					"  1: enabled\n");

	REGISTER_CVAR3("r_profilerTargetFPS", CV_r_profilerTargetFPS, 30.0f, VF_NULL,
	               "Target framerate for application.");

	REGISTER_CVAR3("r_profilerSmoothingWeight", CV_r_profilerSmoothingWeight, 0.1f, VF_NULL,
		"Set how much the current time measurement weights into the previous one.\n"
		"  Single Exponential Smoothing -> (1-a)*oldVal + a*newVal\n"
		"  Range: [0.0, 1.0]");

	REGISTER_CVAR3("r_VSync", CV_r_vsync, 1, VF_RESTRICTEDMODE | VF_DUMPTODISK,
	               "Toggles vertical sync.\n"
	               "0: Disabled\n"
	               "1: Enabled\n");

#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
	REGISTER_CVAR3("r_overrideRefreshRate", CV_r_overrideRefreshRate, 0, VF_RESTRICTEDMODE | VF_DUMPTODISK,
	               "Enforces specified refresh rate when running in fullscreen (0=off).");
	REGISTER_CVAR3("r_overrideScanlineOrder", CV_r_overrideScanlineOrder, 0, VF_RESTRICTEDMODE | VF_DUMPTODISK,
	               "Enforces specified scanline order when running in fullscreen.\n"
	               "0=off,\n"
	               "1=progressive,\n"
	               "2=interlaced (upper field first),\n"
	               "3=interlaced (lower field first)\n"
	               "Usage: r_overrideScanlineOrder [0/1/2/3]");
#endif
#if CRY_PLATFORM_WINDOWS
	REGISTER_CVAR3("r_FullscreenPreemption", CV_r_FullscreenPreemption, 1, VF_NULL,
	               "While in fullscreen activities like notification pop ups of other applications won't cause a mode switch back into windowed mode.");
#endif
	DefineConstIntCVar3("r_UseESRAM", CV_r_useESRAM, 1, VF_REQUIRE_APP_RESTART,
	                    "Toggles using ESRAM for render targets (Durango only)\n"
	                    "Usage: r_UseESRAM [0/1]");
	//DefineConstIntCVar3("r_MeasureOverdraw", CV_r_measureoverdraw, 0, VF_CHEAT,
	//                    "Activate a special rendering mode that visualize the rendering cost of each pixel by color.\n"
	//                    "0=off,\n"
	//                    "1=pixel shader instructions,\n"
	//                    "2=pass count,\n"
	//                    "3=vertex shader instructions,\n"
	//                    "4=overdraw estimation with 360 Hi-Z,\n"
	//                    "Usage: r_MeasureOverdraw [0/1/2/3/4]");
	REGISTER_CVAR3("r_MeasureOverdrawScale", CV_r_measureoverdrawscale, 1.5f, VF_CHEAT, "");

	DefineConstIntCVar3("r_PrintMemoryLeaks", CV_r_printmemoryleaks, 0, VF_NULL, "");
	DefineConstIntCVar3("r_ReleaseAllResourcesOnExit", CV_r_releaseallresourcesonexit, 1, VF_NULL, "");

	DefineConstIntCVar3("r_VegetationSpritesDebug", CV_r_VegetationSpritesGenDebug, 0, VF_DEPRECATED, "[DEPRECATED]");
	DefineConstIntCVar3("r_VegetationSpritesMaxLightingUpdates", CV_r_VegetationSpritesMaxLightingUpdates, 8, VF_DEPRECATED, "[DEPRECATED]");
	DefineConstIntCVar3("r_ZPassOnly", CV_r_ZPassOnly, 0, VF_CHEAT, "");
	DefineConstIntCVar3("r_VegetationSpritesNoGen", CV_r_VegetationSpritesNoGen, 0, VF_DEPRECATED, "[DEPRECATED]");
	DefineConstIntCVar3("r_VegetationSpritesGenAlways", CV_r_VegetationSpritesGenAlways, 0, VF_DEPRECATED, "[DEPRECATED]");
	REGISTER_CVAR3("r_VegetationSpritesTexRes", CV_r_VegetationSpritesTexRes, 128, VF_DEPRECATED, "[DEPRECATED]");

	REGISTER_CVAR3("r_ShowVideoMemoryStats", CV_r_ShowVideoMemoryStats, 0, VF_NULL, "");
	REGISTER_COMMAND("r_ShowRenderTarget", &Cmd_ShowRenderTarget, VF_CHEAT, CDebugRenderTargetsStage::showRenderTargetHelp);

	REGISTER_CVAR3("r_BreakOnError", CV_r_BreakOnError, 0, VF_NULL, "calls debugbreak on illegal behaviour");
	REGISTER_CVAR3("r_durango_async_dips", CV_r_durango_async_dips, 0, VF_NULL, "enables async dip submission on durango");
	REGISTER_CVAR3("r_durango_async_dips_sync", CV_r_durango_async_dips_sync, 9999, VF_CHEAT, "enables async dip submission sync on durango");
	REGISTER_CVAR3("r_D3D12SubmissionThread", CV_r_D3D12SubmissionThread, 15, VF_NULL,
	               "Run DX12 command queue submission tasks from dedicated thread(s).\n"
	               "0=Off,\n"
	               "+1=Direct (default off),\n"
	               "+2=Bundle (default off),\n"
	               "+4=Compute (default off),\n"
	               "+8=Copy (default on)\n"
	               "Usage: r_D3D12SubmissionThread [0-15]");
	REGISTER_CVAR3("r_D3D12WaitableSwapChain", CV_r_D3D12WaitableSwapChain, 0, VF_REQUIRE_APP_RESTART | VF_CHEAT,
	               "Enables highest performance in windowed mode (does not allow switching to fullscreen).");
	REGISTER_CVAR3("r_D3D12BatchResourceBarriers", CV_r_D3D12BatchResourceBarriers, 1, VF_NULL,
	               "Enables batching of resource barriers.\n"
	               "0=Off,\n"
	               "1=On,\n"
	               "2=On + actively rewrite redundant barriers\n"
	               "Usage: r_D3D12BatchResourceBarriers [0-2]");
	REGISTER_CVAR3("r_D3D12EarlyResourceBarriers", CV_r_D3D12EarlyResourceBarriers, 0, VF_NULL,
	               "Enables explicitly scheduled split barriers.");
	REGISTER_CVAR3("r_D3D12AsynchronousCompute", CV_r_D3D12AsynchronousCompute, 0, VF_NULL,
	               "Enables asynchronous compute for different sub-systems.\n"
	               "Setting this to a non-zero value also enables concurrent UAV access.\n"
	               "0=Off,\n"
	               "+1=GPU-Skinning (default off),\n"
	               "+2=GPU-Particles (default off),\n"
	               "+4=Tiled-Shading (default off),\n"
	               "+8=Volumetric-Clouds (default off)\n"
	               "Usage: r_D3D12SubmissionThread [0-15]");
	REGISTER_CVAR3("r_D3D12HardwareComputeQueue", CV_r_D3D12HardwareComputeQueue, 1, VF_NULL,
	               "Selects the hardware queue on which compute tasks run.\n"
	               "0=Direct Queue,\n"
	               "1=Compute Queue\n"
	               "Usage: r_D3D12HardwareComputeQueue [0-1]");
	REGISTER_CVAR3("r_D3D12HardwareCopyQueue", CV_r_D3D12HardwareCopyQueue, 2, VF_NULL,
	               "Selects the hardware queue on which copy tasks run.\n"
	               "0=Direct Queue,\n"
	               "1=Compute Queue,\n"
	               "2=Copy Queue\n"
	               "Usage: r_D3D12HardwareCopyQueue [0-2]");

#if (CRY_RENDERER_DIRECT3D >= 120)
	gEnv->pConsole->GetCVar("r_D3D12HardwareComputeQueue")->SetOnChangeCallback(OnChange_CV_D3D12HardwareComputeQueue);
	gEnv->pConsole->GetCVar("r_D3D12HardwareCopyQueue")->SetOnChangeCallback(OnChange_CV_D3D12HardwareCopyQueue);
#endif

	REGISTER_CVAR3("r_ReprojectOnlyStaticObjects", CV_r_ReprojectOnlyStaticObjects, 1, VF_NULL, "Forces a split in the zpass, to prevent moving object from beeing reprojected");
	REGISTER_CVAR3("r_ReadZBufferDirectlyFromVMEM", CV_r_ReadZBufferDirectlyFromVMEM, 0, VF_NULL, "Uses direct VMEM reads instead of a staging buffer on durango for the reprojection ZBuffer");
	REGISTER_CVAR3("r_FlushToGPU", CV_r_FlushToGPU, 1, VF_NULL,
		"Configure gpu-work flushing behaviour"
		"0: Flush at end-frame only"
		"1: Flush at positions where the character of the work changes drastically (Flash vs. Scene vs. Post vs. Uploads etc.)");

	REGISTER_CVAR3("r_EnableDebugLayer", CV_r_EnableDebugLayer, 0, VF_NULL, 
		"Enable Graphics API specific debug layer"
		"0: Debug layers disabled"
		"1: Debug layers enabled"
		"2: (DX12 specific) Enable GBV (GPU-Based Validation) in addition to debug layers");
	REGISTER_CVAR3("r_NoDraw", CV_r_NoDraw, 0, VF_NULL, "Disable submitting of certain draw operations: 1-(Do not process render objects at all), 2-(Do not submit individual render objects), 3-(No DrawIndexed) 4-Disable entire GraphicsPipeline execution.");
	REGISTER_CVAR3("r_UpdateInstances", CV_r_UpdateInstances, 0, VF_NULL, "Enabling runtime instancing CB updatings each frame");

	// show texture debug routine + auto completion
	CV_r_ShowTexture = REGISTER_STRING("r_ShowTexture", "", VF_CHEAT, "Displays loaded texture - for debug purpose\n");
	gEnv->pConsole->RegisterAutoComplete("r_ShowTexture", &g_TextureNameAutoComplete);

	DefineConstIntCVar3("r_ShowLightBounds", CV_r_ShowLightBounds, 0, VF_CHEAT,
	                    "Display light bounds - for debug purpose\n"
	                    "Usage: r_ShowLightBounds [0=off/1=on]");

	REGISTER_CVAR3("r_ParticlesTessellation", CV_r_ParticlesTessellation, 1, VF_NULL, "Enables particle tessellation for higher quality lighting. (DX11 only)");
	REGISTER_CVAR3("r_ParticlesTessellationTriSize", CV_r_ParticlesTessellationTriSize, 16, VF_NULL, "Sets particles tessellation triangle screen space size in pixels (DX11 only)");

	REGISTER_CVAR3("r_ZFightingDepthScale", CV_r_ZFightingDepthScale, 0.995f, VF_CHEAT, "Controls anti z-fighting measures in shaders (scaling homogeneous z).");
	REGISTER_CVAR3("r_ZFightingExtrude", CV_r_ZFightingExtrude, 0.001f, VF_CHEAT, "Controls anti z-fighting measures in shaders (extrusion along normal in world units).");

	REGISTER_CVAR3("r_TexelsPerMeter", CV_r_TexelsPerMeter, 0, 0,
	                  "Enables visualization of the color coded \"texels per meter\" ratio for objects in view.\n"
	                  "The checkerboard pattern displayed represents the mapping of the assigned diffuse\n"
	                  "texture onto the object's uv space. One block in the pattern represents 8x8 texels.\n"
	                  "Usage: r_TexelsPerMeter [n] (where n is the desired number of texels per meter; 0 = off)");

	REGISTER_CVAR3("r_enableAltTab", CV_r_enableAltTab, 1, VF_NULL,
	               "Toggles alt tabbing in and out of fullscreen when the game is not in devmode.\n"
	               "Usage: r_enableAltTab [toggle]\n"
	               "Notes: Should only be added to system.cfg and requires a restart");

	REGISTER_CVAR3("r_StereoEnableMgpu", CV_r_StereoEnableMgpu, 1, VF_DUMPTODISK,
	               "Sets support for multi GPU stereo rendering.\n"
	               "Usage: r_StereoEnableMgpu [0=disabled/else=enabled]\n"
	               "0: Disable multi-GPU for dual rendering\n"
	               " 1: Enable multi-GPU for dual rendering\n"
	               "-1: Enable multi-GPU for dual rendering, but run on only one GPU (simulation)\n");

#undef VRDEVICE_STEREO_OUTPUT_INFO

	REGISTER_CVAR3("r_StereoFlipEyes", CV_r_StereoFlipEyes, 0, VF_DUMPTODISK,
	               "Flip eyes in stereo mode.\n"
	               "Usage: r_StereoFlipEyes [0=off/1=on]\n"
	               "0: don't flip\n"
	               "1: flip\n");

	REGISTER_CVAR3("r_stereoScaleCoefficient", CV_r_stereoScaleCoefficient, 1.0f, VF_NULL,
	               "Multiplier which influences the perceived scale of the world in VR (1.f = no effect, 0.25 = you are about 1/4th the size).");

	REGISTER_CVAR3("r_StereoStrength", CV_r_StereoStrength, 1.0f, VF_DUMPTODISK,
	               "Multiplier which influences the strength of the stereo effect.");

	REGISTER_CVAR3("r_StereoEyeDist", CV_r_StereoEyeDist, 0.02f, VF_DUMPTODISK,
	               "[For 3D TV] Maximum separation between stereo images in percentage of the screen.");

	REGISTER_CVAR3("r_StereoScreenDist", CV_r_StereoScreenDist, 0.25f, VF_DUMPTODISK,
	               "Distance to plane where stereo parallax converges to zero.");

	REGISTER_CVAR3("r_StereoNearGeoScale", CV_r_StereoNearGeoScale, 0.65f, VF_DUMPTODISK,
	               "Scale for near geometry (weapon) that gets pushed into the screen");

	REGISTER_CVAR3("r_StereoHudScreenDist", CV_r_StereoHudScreenDist, 0.5f, VF_DUMPTODISK,
	               "Distance to plane where hud stereo parallax converges to zero.\n"
	               "If not zero, HUD needs to be rendered two times.");

	REGISTER_CVAR3("r_StereoGammaAdjustment", CV_r_StereoGammaAdjustment, 0.12f, VF_DUMPTODISK,
	               "Additional adjustment to the graphics card gamma correction when Stereo is enabled.\n"
	               "Usage: r_StereoGammaAdjustment [offset]"
	               "0: off");

	REGISTER_CVAR3("r_ConditionalRendering", CV_r_ConditionalRendering, 0, VF_NULL, "Enables conditional rendering .");

	REGISTER_CVAR3("r_CustomResMaxSize", CV_r_CustomResMaxSize, 4096, VF_NULL, "Maximum resolution of custom resolution rendering");
	REGISTER_CVAR3("r_CustomResWidth", CV_r_CustomResWidth, 0, VF_NULL, "Width of custom resolution rendering");
	REGISTER_CVAR3("r_CustomResHeight", CV_r_CustomResHeight, 0, VF_NULL, "Height of custom resolution rendering");
	REGISTER_CVAR3("r_CustomResPreview", CV_r_CustomResPreview, 1, VF_NULL, "Enable/disable preview of custom resolution rendering in viewport"
	                                                                        "(0 - no preview, 1 - scaled to match viewport, 2 - custom resolution clipped to viewport");
	REGISTER_CVAR3("r_Supersampling", CV_r_Supersampling, 1, VF_NULL, "Use supersampled antialiasing"
	                                                                  "(1 - 1x1 no SSAA, 2 - 2x2, 3 - 3x3 ...)");
	REGISTER_CVAR3("r_SupersamplingFilter", CV_r_SupersamplingFilter, 0, VF_NULL, "Filter method to use when resolving supersampled output"
	                                                                              "\n0 - Box filter"
	                                                                              "\n1 - Tent filter"
	                                                                              "\n2 - Gaussian filter"
	                                                                              "\n3 - Lanczos filter");

#if CRY_PLATFORM_DESKTOP
	REGISTER_COMMAND("r_PrecacheShaderList", &ShadersPrecacheList, VF_NULL, "");
	REGISTER_COMMAND("r_StatsShaderList", &ShadersStatsList, VF_NULL, "");
#endif

	DefineConstIntCVar3("r_TextureCompressor", CV_r_TextureCompressor, 1, VF_DUMPTODISK,
	                    "Defines which texture compressor is used (fallback is DirectX)\n"
	                    "Usage: r_TextureCompressor [0/1]\n"
	                    "0 uses DirectX, 1 uses squish if possible");

	REGISTER_CVAR3("r_FogDepthTest", CV_r_FogDepthTest, -0.0005f, VF_NULL,
	               "Enables per-pixel culling for deferred volumetric fog pass.\n"
	               "Fog computations for all pixels closer than a given depth value will be skipped.\n"
	               "Usage: r_FogDepthTest z with...\n"
	               "  z = 0, culling disabled\n"
	               "  z > 0, fixed linear world space culling depth\n"
	               "  z < 0, optimal culling depth will be computed automatically based on camera direction and fog settings");

#if defined(VOLUMETRIC_FOG_SHADOWS)
	REGISTER_CVAR3("r_FogShadows", CV_r_FogShadows, 0, VF_NULL,
	               "Enables deferred volumetric fog shadows\n"
	               "Usage: r_FogShadows [0/1/2]\n"
	               "  0: off\n"
	               "  1: standard resolution\n"
	               "  2: reduced resolution\n");
	REGISTER_CVAR3("r_FogShadowsMode", CV_r_FogShadowsMode, 0, VF_NULL,
	               "Ray-casting mode for shadowed fog\n"
	               "Usage: r_FogShadowsMode [0/1]\n"
	               "  0: brute force shadowmap sampling\n"
	               "  1: optimized shadowmap sampling\n");
#endif
	REGISTER_CVAR3("r_FogShadowsWater", CV_r_FogShadowsWater, 1, VF_NULL, "Enables volumetric fog shadows for watervolumes");

	REGISTER_CVAR3("r_RainDropsEffect", CV_r_RainDropsEffect, 1, VF_NULL,
	                    "Enable RainDrops effect.\n"
	                    "Usage: r_RainDropEffect [0/1/2]\n"
	                    "0: force off\n"
	                    "1: on (default)\n"
	                    "2: on (forced)");
	
	DefineConstIntCVar3("r_RefractionPartialResolveMode", CV_r_RefractionPartialResolveMode, 2, VF_NULL,
	                    "Specifies mode of operation of partial screen resolves before refraction\n"
	                    "Usage: r_RefractionPartialResolveMode [0/1/2]\n"
		                "0: Static approach: Single resolve pass before transparent forward pass.\n"
	                    "1: Simple iterative approach: Resolve pass before every refractive render items that requires resolve.\n"
	                    "2: Topological sorting of overlaping resolve regions (default)");
	DefineConstIntCVar3("r_RefractionPartialResolveMinimalResolveArea", CV_r_RefractionPartialResolveMinimalResolveArea, 0, VF_NULL,
	                    "Minimal resolve area, in pixels, required to inject a partial resolve (default: 0).");
	DefineConstIntCVar3("r_RefractionPartialResolveMaxResolveCount", CV_r_RefractionPartialResolveMaxResolveCount, 0, VF_NULL,
	                    "Provides an upper limit on partial screen resolves per render-items list.\n"
		                "(Unlimited if a non-positive integer is provided)");
	DefineConstIntCVar3("r_RefractionPartialResolvesDebug", CV_r_RefractionPartialResolvesDebug, 0, VF_NULL,
	                    "Toggle refraction partial resolves debug display\n"
	                    "Usage: r_RefractionPartialResolvesDebug\n"
	                    "0: disable \n"
	                    "1: Statistics \n"
	                    "2: Bounding boxes \n");

	DefineConstIntCVar3("r_Batching", CV_r_Batching, 1, VF_NULL,
	                    "Enable/disable render items batching\n"
	                    "Usage: r_Batching [0/1]\n");

	DefineConstIntCVar3("r_Unlit", CV_r_Unlit, 0, VF_CHEAT,
	                    "Render just diffuse texture with no lighting (for most materials).");

	DefineConstIntCVar3("r_HideSunInCubemaps", CV_r_HideSunInCubemaps, 1, VF_NULL,
	                    "Stops the sun being drawn during cubemap generation.\n");

	DefineConstIntCVar3("r_CubemapGenerationTimeout", CV_r_CubemapGenerationTimeout, 100, VF_NULL,
	                    "Maximum number of frames cubemap generation waits for streaming operations to complete.\n");

	REGISTER_CVAR3("r_FlashMatTexResQuality", CV_r_FlashMatTexResQuality, 1.0f, VF_NULL,
	               "Texture resolution quality of flash materials.\n"
	               "Only used if flash asset is tagged CE_HQR!");

	REGISTER_CVAR3("r_DynTexSourceSharedRTWidth", CV_r_DynTexSourceSharedRTWidth, 0, VF_NULL,
	               "Width override for shared RT for dynamic texture sources. Takes effect when bigger than 0.");

	REGISTER_CVAR3("r_DynTexSourceSharedRTHeight", CV_r_DynTexSourceSharedRTHeight, 0, VF_NULL,
	               "Height override of shared RT for dynamic texture sources. Takes effect when bigger than 0.");

	REGISTER_CVAR3("r_DynTexSourceUseSharedRT", CV_r_DynTexSourceUseSharedRT, 0, VF_REQUIRE_LEVEL_RELOAD,
	               "Defines if dynamic flash textures are rendered into shared RT\n"
	               "Usage: r_DynTexSourceUseSharedRT [0/1].\n"
	               "0: Use Unique RT for each dynamic flash texture (with alpha support)\n"
	               "1: Use Shared RT for all dynamic flash textures (no alpha support!)\n"
	               "Disabled by default. Requires level reload to change.");

	// more details: http://en.wikipedia.org/wiki/Overscan
	// Microsoft's Xbox game developer guidelines recommend using 85 percent of the screen width and height,[7] or a title safe area of 7.5% per side.
	REGISTER_COMMAND("r_OverscanBorders", &cmd_OverscanBorders, VF_NULL,
	                 "Changes the size of the overscan borders for the left/right and top/bottom\n"
	                 "of the screen for adjusting the title safe area. This is for logo placements\n"
	                 "and text printout to account for the TV overscan and is mostly needed for consoles.\n"
	                 "If only one value is specified, the overscan borders for left/right and top/bottom\n"
	                 "are set simultaneously, but you may also specify different percentages for left/right\n"
	                 "and top/bottom.\n"
	                 "Usage: r_OverscanBorders [0..25]\n"
	                 "       r_OverscanBorders [0..25] [0..25]\n"
	                 "Default is 0=off, >0 defines the size of the overscan borders for left/right\n"
	                 "or top/bottom as percentages of the whole screen size (e.g. 7.5).");

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	float overscanBorderScale = 0.0f;
#else
	float overscanBorderScale = 0.03f; // Default consoles to sensible overscan border
#endif
	REGISTER_CVAR3_CB("r_OverscanBorderScaleX", s_overscanBorders.x, overscanBorderScale, VF_NULL,
	                  "Sets the overscan border width scale\n"
	                  "Usage: r_OverscanBorderScaleX [0.0->0.25]",
	                  OnChange_r_OverscanBorderScale);

	REGISTER_CVAR3_CB("r_OverscanBorderScaleY", s_overscanBorders.y, overscanBorderScale, VF_NULL,
	                  "Sets the overscan border height scale\n"
	                  "Usage: r_OverscanBorderScaleY [0.0->0.25]",
	                  OnChange_r_OverscanBorderScale);

	REGISTER_CVAR2("r_UsePersistentRTForModelHUD", &CV_r_UsePersistentRTForModelHUD, 0, VF_NULL,
	               "Uses a seperate RT to render models for the ModelHud Renderer");

#if defined(ENABLE_RENDER_AUX_GEOM)
	const int defValAuxGeomEnable = 1;
	REGISTER_CVAR2("r_enableAuxGeom", &CV_r_enableauxgeom, defValAuxGeomEnable, VF_REQUIRE_APP_RESTART, "Enables aux geometry rendering.");
#endif

	REGISTER_CVAR2("r_ParticleVerticePoolSize", &CV_r_ParticleVerticePoolSize, 131072, VF_REQUIRE_APP_RESTART, "Initial size Particles' buffers");
	REGISTER_CVAR2("r_ParticleMaxVerticePoolSize", &CV_r_ParticleMaxVerticePoolSize, 131072*8, VF_REQUIRE_APP_RESTART, "Max size of Particles' buffers");

	DefineConstIntCVar3("r_ParticlesDebug", CV_r_ParticlesDebug, 0, VF_NULL,
	                    "Particles debugging\n"
	                    "Usage: \n"
	                    "0 disabled\n"
	                    "1 particles screen coverage (red = bad, blue = good)\n"
	                    "2 particles overdraw (white = really bad, red = bad, blue = good)");

	REGISTER_CVAR3("r_GeomCacheInstanceThreshold", CV_r_GeomCacheInstanceThreshold, 10, VF_NULL, "Threshold after which instancing is used to draw geometry cache pieces");

	REGISTER_CVAR3("r_VisAreaClipLightsPerPixel", CV_r_VisAreaClipLightsPerPixel, 1, VF_NULL, "Per pixel light/cubemap culling for vis areas: 0=off, 1=on");

	REGISTER_CVAR3("r_VolumetricFogTexScale", CV_r_VolumetricFogTexScale, 10, VF_NULL,
	               "Width and height scale factor (divided by screen resolution) for volume texture.\n"
	               "Acceptable value is more than equal 2.\n"
	               );
	REGISTER_CVAR3("r_VolumetricFogTexDepth", CV_r_VolumetricFogTexDepth, 32, VF_NULL,
	               "Depth resolution of volume texture.\n"
	               "Huge value runs out of performance and video memory.\n"
	               );
	REGISTER_CVAR3("r_VolumetricFogReprojectionBlendFactor", CV_r_VolumetricFogReprojectionBlendFactor, 0.9f, VF_NULL,
	               "Adjust the blend factor of temporal reprojection.\n"
	               "Acceptable value is between 0 and 1.\n"
	               "0 means temporal reprojecton is off.\n"
	               );
	REGISTER_CVAR3("r_VolumetricFogSample", CV_r_VolumetricFogSample, 0, VF_NULL,
	               "Adjust number of sample points.\n"
	               "0: 1 sample point in a voxel\n"
	               "1: 2 sample points in a voxel\n"
	               "2: 4 sample points in a voxel\n"
	               );
	REGISTER_CVAR3("r_VolumetricFogShadow", CV_r_VolumetricFogShadow, 1, VF_NULL,
	               "Adjust shadow sample count per sample point.\n"
	               "0: 1 shadow sample per sample point\n"
	               "1: 2 shadow samples per sample point \n"
	               "2: 3 shadow samples per sample point\n"
	               "3: 4 shadow samples per sample point\n"
	               );
	REGISTER_CVAR3("r_VolumetricFogDownscaledSunShadow", CV_r_VolumetricFogDownscaledSunShadow, 1, VF_NULL,
	               "Enable replacing sun shadow maps with downscaled shadow maps or static shadow map if possible.\n"
	               "0: disabled\n"
	               "1: replace first and second cascades with downscaled shadow maps. the others are replaced with static shadow map if possible.\n"
	               "2: replace first, second, and third cascades with downscaled shadow maps. the others are replaced with static shadow map if possible.\n"
	               );
	REGISTER_CVAR3("r_VolumetricFogDownscaledSunShadowRatio", CV_r_VolumetricFogDownscaledSunShadowRatio, 1, VF_NULL,
	               "Set downscale ratio for sun shadow maps\n"
	               "0: 1/4 downscaled sun shadow maps\n"
	               "1: 1/8 downscaled sun shadow maps\n"
	               "2: 1/16 downscaled sun shadow maps\n"
	               );
	REGISTER_CVAR3("r_VolumetricFogReprojectionMode", CV_r_VolumetricFogReprojectionMode, 1, VF_NULL,
	               "Set the mode of ghost reduction for temporal reprojection.\n"
	               "0: conservative\n"
	               "1: advanced"
	               );
	REGISTER_CVAR3("r_VolumetricFogMinimumLightBulbSize", CV_r_VolumetricFogMinimumLightBulbSize, 0.4f, VF_NULL,
	               "Adjust the minimum size threshold for light attenuation bulb size. Small bulb size causes the light flicker."
	               );
	REGISTER_CVAR3("r_VolumetricFogSunLightCorrection", CV_r_VolumetricFogSunLightCorrection, 1, VF_NULL,
	               "Enables the correction of sun light luminance. Volumetric fog gets brighter but it's consistent with other lighting.\n"
	               "0: Disabled.\n"
	               "1: Enabled.\n"
	               );

	REGISTER_CVAR3("r_VolumetricClouds", CV_r_VolumetricClouds, 0, VF_NULL,
	               "Enables procedural volumetric clouds (experimental feature, data compatibility may break in a future release.).\n"
	               "0 - Disabled.\n"
	               "1 - Enabled (1/2x Res).\n"
	               "2 - Enabled (1/4x Res)."
	               );
	REGISTER_CVAR3("r_VolumetricCloudsRaymarchStepNum", CV_r_VolumetricCloudsRaymarchStepNum, 64, VF_NULL,
	               "Set the step number of ray-marching for procedural volumetric clouds.\n"
	               "Acceptable number is from 16 to 256, and it should be multiple of 16."
	               );
	REGISTER_CVAR3("r_VolumetricCloudsPipeline", CV_r_VolumetricCloudsPipeline, 0, VF_NULL,
	               "Set the pipeline mode of procedural volumetric clouds.\n"
	               "0 - Monolithic shader pipeline, using less memory.\n"
	               "1 - Multiple shaders pipeline, using more memory, faster when using Cloud Blocker."
	               );
	REGISTER_CVAR3("r_VolumetricCloudsStereoReprojection", CV_r_VolumetricCloudsStereoReprojection, 1, VF_NULL,
	               "Enables stereoscopic reprojection for procedural volumetric clouds to accelerate the rendering.\n"
	               "0 - Disabled.\n"
	               "1 - Enabled.\n"
	               );
	REGISTER_CVAR3("r_VolumetricCloudsTemporalReprojection", CV_r_VolumetricCloudsTemporalReprojection, 1, VF_NULL,
	               "Set temporal reprojection mode for procedural volumetric clouds.\n"
	               "0 - faster but prone to flickering artifacts.\n"
	               "1 - a bit slower but less flickering artifacts.\n"
	               );
	REGISTER_CVAR3("r_VolumetricCloudsShadowResolution", CV_r_VolumetricCloudsShadowResolution, 64, VF_NULL,
	               "Set the resolution of volumetric clouds shadow map for casting shadow on the terrain and low shading-LOD clouds."
	               );

	REGISTER_CVAR3("r_GpuParticles", CV_r_GpuParticles, 1, VF_NULL,
	               "Enables processing of GPU particles (Default: 1).\n"
	               "0 - Disabled.\n"
	               "1 - Enabled.\n"
	               );

	REGISTER_CVAR3("r_GpuParticlesConstantRadiusBoundingBoxes", CV_r_GpuParticlesConstantRadiusBoundingBoxes, 100, VF_NULL,
	               "Constant radius bounding boxes for GPU particles (Default: 100).\n"
	               "0 - Dynamic Calculation.\n"
	               ">0 - Bounding Box radius.\n"
	               );

	REGISTER_CVAR3("r_GpuPhysicsFluidDynamicsDebug", CV_r_GpuPhysicsFluidDynamicsDebug, 1, VF_NULL,
	               "Draw GPU Fluid Particles for debugging purposes. This drastically decreases simulation performance. (Default: 1).\n"
	               "0 - Don't draw any debug information.\n"
	               "1 - Draw acceleration structure bounds and fluid particle positions.\n"
	               );

	REGISTER_CVAR2("d3d11_CBUpdateStats", &CV_d3d11_CBUpdateStats, 0, 0, "Logs constant buffer updates statistics.");
	CV_d3d11_forcedFeatureLevel = REGISTER_STRING("d3d11_forcedFeatureLevel", NULL, VF_DEV_ONLY | VF_REQUIRE_APP_RESTART,
	                                              "Forces the Direct3D device to target a specific feature level - supported values are:\n"
	                                              " 10_0\n"
	                                              " 10_1\n"
	                                              " 11_0\n"
	                                              " 11_1");

#if defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)
	CV_d3d11_debugMuteSeverity = REGISTER_INT("d3d11_debugMuteSeverity", 2, VF_NULL,
	                                          "Mute whole group of messages of certain severity when D3D debug runtime enabled:\n"
	                                          " 0 - no severity based mute\n"
	                                          " 1 - mute INFO only\n"
	                                          " 2 - mute INFO and WARNING (default)\n"
	                                          " 3 - mute INFO, WARNING and ERROR\n"
	                                          " 4 - mute all (INFO, WARNING, ERROR, CORRUPTION)");
	CV_d3d11_debugMuteMsgID = REGISTER_STRING("d3d11_debugMuteMsgID", "388", VF_NULL,
	                                          "List of D3D debug runtime messages to mute (see DirectX Control Panel for full message ID list)\n"
	                                          "Use space separated list of IDs, eg. '388 10 544'");
	CV_d3d11_debugBreakOnMsgID = REGISTER_STRING("d3d11_debugBreakOnMsgID", "0", VF_NULL,
	                                             "List of D3D debug runtime messages to break on.\nUsage:\n"
	                                             " 0                    - no break (default)\n"
	                                             " msgID1 msgID2 msgID3 - break whenever a message with one of given IDs occurs\n"
	                                             " -1                   - break on any error or corruption message");
	REGISTER_CVAR2("d3d11_debugBreakOnce", &CV_d3d11_debugBreakOnce, 1, VF_NULL,
	               "If enabled, D3D debug runtime break on message/error will be enabled only for 1 frame since last change.\n");

	CV_d3d11_debugMuteSeverity->SetOnChangeCallback(OnChange_CV_d3d11_debugMuteMsgID);
	CV_d3d11_debugMuteMsgID->SetOnChangeCallback(OnChange_CV_d3d11_debugMuteMsgID);
	CV_d3d11_debugBreakOnMsgID->SetOnChangeCallback(OnChange_CV_d3d11_debugMuteMsgID);
#endif

#if defined(CRY_PLATFORM_WINDOWS)
	REGISTER_CVAR2("d3d11_preventDriverThreading", &CV_d3d11_preventDriverThreading, 0, VF_REQUIRE_APP_RESTART,
	               "Prevent internal threading optimizations for D3D Device.\n");
#endif

	CV_capture_frames = 0;
	CV_capture_folder = 0;
	CV_capture_file_format = 0;

#ifndef CONSOLE_CONST_CVAR_MODE
	CV_r_reloadshaders = 0;
#endif

	// compute skinning cvars
	DefineConstIntCVar(r_ComputeSkinning, 1, VF_NULL, "Activate skinning via compute shaders (0=disabled, 1=enabled, 2=forced)");
	DefineConstIntCVar(r_ComputeSkinningMorphs, 1, VF_NULL, "Apply morphs before skinning (0=disabled, 1=enabled, 2=forced)");
	DefineConstIntCVar(r_ComputeSkinningTangents, 1, VF_NULL, "Calculate new tangents after skinning is computed (0=disabled, 1=enabled, 2=forced)");
	DefineConstIntCVar(r_ComputeSkinningDebugDraw, 0, VF_NULL, "Enable debug draw mode for geometry deformation");

	//////////////////////////////////////////////////////////////////////////
	InitExternalCVars();
}

void CRendererCVars::InitExternalCVars()
{
	m_CVWidth = iConsole->GetCVar("r_Width");
	m_CVHeight = iConsole->GetCVar("r_Height");
	m_CVWindowType = iConsole->GetCVar("r_WindowType");
	m_CVDisplayInfo = iConsole->GetCVar("r_DisplayInfo");
	m_CVColorBits = iConsole->GetCVar("r_ColorBits");
}

void CRendererCVars::CacheCaptureCVars()
{
	// cache console vars
	if (!CV_capture_frames || !CV_capture_folder || !CV_capture_file_format || !CV_capture_frame_once ||
	    !CV_capture_file_name || !CV_capture_file_prefix)
	{
		ISystem* pSystem(GetISystem());
		if (!pSystem)
			return;

		IConsole* pConsole(gEnv->pConsole);
		if (!pConsole)
			return;

		CV_capture_frames = !CV_capture_frames ? pConsole->GetCVar("capture_frames") : CV_capture_frames;
		CV_capture_folder = !CV_capture_folder ? pConsole->GetCVar("capture_folder") : CV_capture_folder;
		CV_capture_file_format = !CV_capture_file_format ? pConsole->GetCVar("capture_file_format") : CV_capture_file_format;
		CV_capture_frame_once = !CV_capture_frame_once ? pConsole->GetCVar("capture_frame_once") : CV_capture_frame_once;
		CV_capture_file_name = !CV_capture_file_name ? pConsole->GetCVar("capture_file_name") : CV_capture_file_name;
		CV_capture_file_prefix = !CV_capture_file_prefix ? pConsole->GetCVar("capture_file_prefix") : CV_capture_file_prefix;
	}
}

CCVarUpdateRecorder::SUpdateRecord::SUpdateRecord(ICVar* pCVar)
{
	type = pCVar->GetType();
	name = pCVar->GetName();

	switch (type)
	{
		case CVAR_INT:    intValue = pCVar->GetIVal();                 break;
		case CVAR_FLOAT:  floatValue = pCVar->GetFVal();               break;
		case CVAR_STRING: cry_strcpy(stringValue, pCVar->GetString()); break;
		default: assert(false);
	};
}

CCVarUpdateRecorder::CCVarUpdateRecorder(IConsole* pConsole)
{
	m_pConsole = pConsole;
	m_pConsole->AddConsoleVarSink(this);
}

CCVarUpdateRecorder::~CCVarUpdateRecorder()
{
	m_pConsole->RemoveConsoleVarSink(this);
}

void CCVarUpdateRecorder::OnAfterVarChange(ICVar* pVar) 
{ 
	m_updatedCVars[gRenDev->GetMainThreadID()].emplace_back(pVar);
}

void CCVarUpdateRecorder::OnVarUnregister(ICVar* pVar)
{
	SUpdateRecord tempRecord(pVar);
	for (CVarList& cvarList : m_updatedCVars)
	{
		const auto it = std::remove(std::begin(cvarList), std::end(cvarList), tempRecord);
		cvarList.erase(it, std::end(cvarList));
	}
}

void CCVarUpdateRecorder::Reset() 
{ 
	m_updatedCVars[gRenDev->GetRenderThreadID()].clear(); 
}

const CCVarUpdateRecorder::CVarList& CCVarUpdateRecorder::GetCVars() const
{ 
	return m_updatedCVars[gRenDev->GetRenderThreadID()];
}

const CCVarUpdateRecorder::SUpdateRecord* CCVarUpdateRecorder::GetCVar(const char* cvarName) const
{
	for (auto& cvar : m_updatedCVars[gRenDev->GetRenderThreadID()])
	{
		if (cry_strcmp(cvar.name, cvarName) == 0)
			return &cvar;
	}

	return nullptr;
}
