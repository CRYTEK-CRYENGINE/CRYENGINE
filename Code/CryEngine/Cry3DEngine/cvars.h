// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   cvars.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _3DENGINE_CVARS_H_
#define _3DENGINE_CVARS_H_

#if defined(CONSOLE_CONST_CVAR_MODE)
	#define GetFloatCVar(name) name ## Default
#else
	#define GetFloatCVar(name) (Cry3DEngineBase::GetCVars())->name
#endif

// console variables
struct CVars : public Cry3DEngineBase
{
	CVars()
	{ Init(); }

	void Init();

#if defined(FEATURE_SVO_GI)
	void RegisterTICVars();
#endif

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
	//default values used for const cvars
#ifdef _RELEASE
	enum { e_StatObjValidateDefault = 0 };  // Validate meshes in all but release builds.
#else
	enum { e_StatObjValidateDefault = 1 };  // Validate meshes in all but release builds.
#endif
#ifdef CONSOLE_CONST_CVAR_MODE
	enum { e_DisplayMemoryUsageIconDefault = 0 };
#else
	enum { e_DisplayMemoryUsageIconDefault = 1 };
#endif
#define e_PhysOceanCellDefault (0.f)
#if CRY_PLATFORM_DURANGO
	enum { e_ParticlesSimpleWaterCollisionDefault = 1 };
	enum { e_DeformableObjectsDefault = 0 }; // temporarily disabled until RC issue found
	enum { e_OcclusionVolumesDefault = 1 };
	enum { e_WaterOceanFastpathDefault = 0 };
	enum { e_WaterVolumesDefault = 1 };
	enum { e_WaterOceanDefault = 1 };
	enum { e_LightVolumesDefault = 1 };
	enum { e_ShadowsBlendDefault = 1 };
#else
	enum { e_DeformableObjectsDefault = 1 };
	enum { e_OcclusionVolumesDefault = 1 };
	enum { e_WaterOceanDefault = 1 };
	enum { e_WaterVolumesDefault = 1 };
	enum { e_LightVolumesDefault = 1 };
#endif

#define e_DecalsDeferredDynamicMinSizeDefault              (0.35f)
#define e_DecalsPlacementTestAreaSizeDefault               (0.08f)
#define e_DecalsPlacementTestMinDepthDefault               (0.05f)
#define e_StreamPredictionDistanceFarDefault               (16.f)
#define e_StreamPredictionDistanceNearDefault              (0.f)
#define e_StreamCgfVisObjPriorityDefault                   (0.5f)
#define e_WindBendingDistRatioDefault                      (0.5f)
#define e_MaxViewDistFullDistCamHeightDefault              (1000.f)
#define e_CoverageBufferOccludersLodRatioDefault           (0.25f)
#define e_LodCompMaxSizeDefault                            (6.f)
#define e_MaxViewDistanceDefault                           (-1.f)
#define e_ViewDistCompMaxSizeDefault                       (64.f)
#define e_ViewDistRatioPortalsDefault                      (60.f)
#define e_ParticlesLightsViewDistRatioDefault              (256.f)
#define e_TerrainOcclusionCullingPrecisionDefault          (0.25f)
#define e_TerrainOcclusionCullingPrecisionDistRatioDefault (3.f)
#define e_TerrainOcclusionCullingStepSizeDeltaDefault      (1.05f)
#define e_WindDefault                                      (0.1f)
#define e_ShadowsCastViewDistRatioLightsDefault            (1.f)
#define e_DecalsRangeDefault                               (20.f)
#define e_MinMassDistanceCheckRenderMeshCollisionDefault   (0.5f)
#define e_GsmRangeStepExtendedDefault                      (8.f)
#define e_TerrainDetailMaterialsViewDistXYDefault          (2048.f)
#define e_SunAngleSnapSecDefault                           (0.1f)
#define e_SunAngleSnapDotDefault                           (0.999999f)
#define e_OcclusionVolumesViewDistRatioDefault             (0.05f)
#define e_FoliageStiffnessDefault                          (3.2f)
#define e_FoliageBranchesStiffnessDefault                  (100.f)
#define e_FoliageBranchesDampingDefault                    (10.f)
#define e_FoliageBrokenBranchesDampingDefault              (15.f)
#define e_TerrainTextureLodRatioDefault                    (1.f)
#define e_JointStrengthScaleDefault                        (1.f)
#define e_VolObjShadowStrengthDefault                      (.4f)
#define e_CameraRotationSpeedDefault                       (0.f)
#define e_DecalsDeferredDynamicDepthScaleDefault           (4.0f)
#define e_TerrainDetailMaterialsViewDistZDefault           (128.f)
#define e_StreamCgfFastUpdateMaxDistanceDefault            (16.f)
#define e_StreamPredictionMinFarZoneDistanceDefault        (16.f)
#define e_StreamPredictionMinReportDistanceDefault         (0.75f)
#define e_StreamCgfGridUpdateDistanceDefault               (0.f)
#define e_StreamPredictionAheadDefault                     (0.5f)
#define e_StreamPredictionAheadDebugDefault                (0.f)
#define e_RenderMeshCollisionToleranceDefault              (0.3f)
#define e_VegetationSpritesScaleFactorDefault              (1.0f)
#ifdef DEDICATED_SERVER
	#define e_RenderDefault                                  (0)
#else
	#ifdef _RELEASE
		#define e_RenderDefault (1)
	#else
		#define e_RenderDefault (gEnv->IsDedicated() ? 0 : 1)
	#endif
#endif

	int    e_PermanentRenderObjects;
	int    e_TerrainTextureStreamingPoolItemsNum;
	int    e_ParticlesPoolSize;
	int    e_ParticlesVertexPoolSize;
	int    e_ParticlesIndexPoolSize;
	int    e_ParticlesProfile;
	int    e_ParticlesProfiler;
	ICVar* e_ParticlesProfilerOutputFolder;
	ICVar* e_ParticlesProfilerOutputName;
	int    e_ParticlesProfilerCountBudget;
	int    e_ParticlesProfilerTimingBudget;
	int    e_ParticlesForceSeed;
	float  e_VegetationSpritesDistanceRatio;
	int    e_Decals;
	int    e_DecalsAllowGameDecals;
	DeclareConstFloatCVar(e_FoliageBrokenBranchesDamping);
	float  e_ShadowsCastViewDistRatio;
	int    e_WaterTessellationAmountY;
	float  e_OnDemandMaxSize;
	float  e_MaxViewDistSpecLerp;
	float  e_StreamAutoMipFactorSpeedThreshold;
	DeclareConstFloatCVar(e_DecalsDeferredDynamicMinSize);
	DeclareConstIntCVar(e_Objects, 1);
	float e_ViewDistRatioCustom;
	float e_StreamPredictionUpdateTimeSlice;
	DeclareConstIntCVar(e_DisplayMemoryUsageIcon, e_DisplayMemoryUsageIconDefault);
	int   e_ScreenShotWidth;
	int   e_ScreenShotDebug;
#if CRY_PLATFORM_WINDOWS
	int   e_ShadowsLodBiasFixed;
#else
	DeclareConstIntCVar(e_ShadowsLodBiasFixed, 0);
#endif
	DeclareConstIntCVar(e_FogVolumes, 1);
	int    e_VolumetricFog;
	DeclareConstIntCVar(e_Render, e_RenderDefault);
	int    e_Tessellation;
	float  e_TessellationMaxDistance;
	int    e_ShadowsTessellateCascades;
	DeclareConstIntCVar(e_ShadowsTessellateDLights, 0);
	int    e_CoverageBufferReproj;
	int    e_CoverageBufferRastPolyLimit;
	int    e_CoverageBufferShowOccluder;
	float  e_ViewDistRatioPortals;
	int    e_ParticlesLights;
	DeclareConstIntCVar(e_ObjFastRegister, 1);
	float  e_ViewDistRatioLights;
	float  e_LightIlluminanceThreshold;
	DeclareConstIntCVar(e_DebugDraw, 0);
	ICVar* e_DebugDrawFilter;
	DeclareConstIntCVar(e_DebugDrawListSize, 16);
	DeclareConstIntCVar(e_DebugDrawListBBoxIndex, 0);
	float e_DebugDrawMaxDistance;
#if !defined(_RELEASE)
	const char* e_pStatObjRenderFilterStr;
	int         e_statObjRenderFilterMode;
#endif
	DeclareConstIntCVar(e_AutoPrecacheTexturesAndShaders, 0);
	int   e_StreamPredictionMaxVisAreaRecursion;
	float e_StreamPredictionBoxRadius;
	int   e_Clouds;
	int   e_VegetationBillboards;
	int   e_VegetationUseTerrainColor;
	float e_VegetationUseTerrainColorDistance;
	int   e_BrushUseTerrainColor;
	int   e_DecalsMaxTrisInObject;
	DeclareConstFloatCVar(e_OcclusionVolumesViewDistRatio);
	DeclareConstFloatCVar(e_SunAngleSnapDot);
	int   e_PreloadDecals;
	float e_DecalsLifeTimeScale;
	int   e_DecalsForceDeferred;
	DeclareConstIntCVar(e_CoverageBufferDebugFreeze, 0);
	DeclareConstIntCVar(e_TerrainOcclusionCulling, 1);
	int    e_PhysProxyTriLimit;
	float  e_FoliageWindActivationDist;
	ICVar* e_SQTestTextureName;
	int    e_ShadowsClouds;
	int    e_levelStartupFrameDelay;
	float  e_SkyUpdateRate;
	float  e_RecursionViewDistRatio;
	DeclareConstIntCVar(e_StreamCgfDebugMinObjSize, 100);
	int    e_CullVegActivation;
	int    e_StreamPredictionTexelDensity;
	int    e_StreamPredictionAlwaysIncludeOutside;
	DeclareConstIntCVar(e_DynamicLights, 1);
	int    e_DynamicLightsFrameIdVisTest;
	DeclareConstIntCVar(e_WaterWavesTessellationAmount, 5);
	int    e_ShadowsLodBiasInvis;
	float  e_CoverageBufferBias;
	int    e_DynamicLightsMaxEntityLights;
	int    e_SQTestMoveSpeed;
	float  e_StreamAutoMipFactorMax;
	int    e_ObjQuality;
	int    e_StaticInstancing;
	int    e_StaticInstancingMinInstNum;
	int    e_RNTmpDataPoolMaxFrames;
	int    e_DynamicLightsMaxCount;
	DeclareConstIntCVar(e_DebugLights, 0);
	int    e_StreamCgfPoolSize;
	DeclareConstIntCVar(e_StatObjPreload, 1);
	DeclareConstIntCVar(e_ShadowsDebug, 0);
	DeclareConstIntCVar(e_ShadowsCascadesCentered, 0);
	DeclareConstIntCVar(e_ShadowsCascadesDebug, 0);
	DeclareConstFloatCVar(e_StreamPredictionDistanceNear);
	DeclareConstIntCVar(e_TerrainDrawThisSectorOnly, 0);
	DeclareConstFloatCVar(e_VegetationSpritesScaleFactor);
	float e_ParticlesMaxDrawScreen;
	int   e_ParticlesAnimBlend;
	DeclareConstIntCVar(e_GsmStats, 0);
	DeclareConstIntCVar(e_DynamicLightsForceDeferred, 1);
	DeclareConstIntCVar(e_Fog, 1);
	float e_TimeOfDay;
	int   e_Terrain;
	int   e_TerrainAutoGenerateBaseTexture;
	float e_TerrainAutoGenerateBaseTextureTiling;
	int   e_TerrainIntegrateObjectsMaxVertices;
	int   e_TerrainIntegrateObjectsMaxHeight;
	DeclareConstIntCVar(e_SkyBox, 1);
	float e_CoverageBufferAABBExpand;
	int   e_CoverageBufferEarlyOut;
	float e_CoverageBufferEarlyOutDelay;
	float e_CoverageBufferTerrainExpand;
	DeclareConstIntCVar(e_WaterWaves, 0);
	int   e_GsmCastFromTerrain;
	float e_TerrainLodDistanceRatio;
	float e_TerrainLodErrorRatio;
	int   e_StatObjBufferRenderTasks;
	DeclareConstIntCVar(e_StreamCgfUpdatePerNodeDistance, 1);
	DeclareConstFloatCVar(e_DecalsDeferredDynamicDepthScale);
	DeclareConstIntCVar(e_TerrainBBoxes, 0);
	int e_LightVolumes;
	DeclareConstIntCVar(e_LightVolumesDebug, 0);
	DeclareConstIntCVar(e_Portals, 1);
	DeclareConstIntCVar(e_PortalsBlend, 1);
	int   e_PortalsMaxRecursion;
	float e_StreamAutoMipFactorMaxDVD;
	DeclareConstIntCVar(e_CameraFreeze, 0);
	float e_ParticlesMinDrawPixels;
	DeclareConstFloatCVar(e_StreamPredictionAhead);
	DeclareConstFloatCVar(e_FoliageBranchesStiffness);
	DeclareConstFloatCVar(e_StreamPredictionMinFarZoneDistance);
	int   e_StreamCgf;
	int   e_StreamInstances;
	int   e_StreamInstancesMaxTasks;
	float e_StreamInstancesDistRatio;
	int   e_CheckOcclusion;
	int   e_CheckOcclusionQueueSize;
	int   e_CheckOcclusionOutputQueueSize;
	DeclareConstIntCVar(e_WaterVolumes, e_WaterVolumesDefault);
	DeclareConstFloatCVar(e_TerrainOcclusionCullingPrecisionDistRatio);
	float e_ScreenShotMapCamHeight;
	int   e_DeformableObjects;
	DeclareConstFloatCVar(e_StreamCgfFastUpdateMaxDistance);
	DeclareConstIntCVar(e_DecalsClip, 1);
	ICVar* e_ScreenShotFileFormat;
	int    e_CharLodMin;
	float  e_PhysOceanCell;
	int    e_WindAreas;
	DeclareConstFloatCVar(e_WindBendingDistRatio);
	float  e_WindBendingStrength;
	float  e_WindBendingAreaStrength;
	float  e_SQTestDelay;
	int    e_PhysMinCellSize;
	int    e_StreamCgfMaxTasksInProgress;
	int    e_StreamCgfMaxNewTasksPerUpdate;
	DeclareConstFloatCVar(e_DecalsPlacementTestAreaSize);
	DeclareConstFloatCVar(e_DecalsPlacementTestMinDepth);
	DeclareConstFloatCVar(e_CameraRotationSpeed);
	float  e_ScreenShotMapSizeY;
	int    e_GI;
	DeclareConstIntCVar(e_PortalsBigEntitiesFix, 1);
	int    e_SQTestBegin;
	int    e_VegetationSprites;
	ICVar* e_CameraGoto;
	DeclareConstIntCVar(e_ParticlesCullAgainstViewFrustum, 1);
	DeclareConstIntCVar(e_ParticlesCullAgainstOcclusionBuffer, 1);
	DeclareConstFloatCVar(e_StreamPredictionMinReportDistance);
	int   e_WaterTessellationSwathWidth;
	DeclareConstIntCVar(e_StreamSaveStartupResultsIntoXML, 0);
	int   e_PhysFoliage;
	DeclareConstIntCVar(e_RenderMeshUpdateAsync, 1);
	int   e_ParticlesPreload;
	int   e_ParticlesAllowRuntimeLoad;
	int   e_ParticlesConvertPfx1;
	DeclareConstIntCVar(e_ParticlesSerializeNamedFields, 1);
	float e_CoverageBufferOccludersViewDistRatio; // TODO: make use of this cvar
	int   e_DecalsDeferredDynamic;
	DeclareConstIntCVar(e_DefaultMaterial, 0);
	int   e_LodMin;
	int   e_RenderMeshCollisionLod;
	DeclareConstIntCVar(e_PreloadMaterials, 1);
	DeclareConstIntCVar(e_ObjStats, 0);
	DeclareConstIntCVar(e_TerrainDeformations, 0);
	int e_TerrainDetailMaterials;
	DeclareConstIntCVar(e_ShadowsFrustums, 0);
	DeclareConstIntCVar(e_OcclusionVolumes, e_OcclusionVolumesDefault);
	int   e_TerrainEditPostponeTexturesUpdate;
	int   e_DecalsDeferredStatic;
	DeclareConstIntCVar(e_Roads, 1);
	float e_TerrainDetailMaterialsViewDistXY;
	int   e_ParticlesQuality;
	DeclareConstIntCVar(e_DebugDrawShowOnlyCompound, 0);
	DeclareConstFloatCVar(e_SunAngleSnapSec);
	float e_GsmRangeStep;
	int   e_ParticlesGI;
	int   e_ParticlesSoftIntersect;
	int   e_ParticlesMotionBlur;
	int   e_ParticlesShadows;
	int   e_ParticlesAudio;
	int   e_ParticleShadowsNumGSMs;
	float e_FoliageBranchesTimeout;
	DeclareConstFloatCVar(e_TerrainOcclusionCullingStepSizeDelta);
	float e_LodRatio;
	float e_LodTransitionTime;
	float e_LodFaceAreaTargetSize;
	float e_ObjectsTreeNodeMinSize;
	float e_ObjectsTreeNodeSizeRatio;
	int   e_ExecuteRenderAsJobMask;
	float e_ObjectsTreeLevelsDebug;
	DeclareConstIntCVar(e_CoverageBufferDrawOccluders, 0);
	DeclareConstIntCVar(e_ObjectsTreeBBoxes, 0);
	DeclareConstIntCVar(e_PrepareDeformableObjectsAtLoadTime, 0);
	DeclareConstIntCVar(e_3dEngineTempPoolSize, 1024);
	DeclareConstFloatCVar(e_MaxViewDistFullDistCamHeight);
	int   e_VegetationBending;
	DeclareConstFloatCVar(e_StreamPredictionAheadDebug);
	float e_ShadowsSlopeBias;
	float e_ShadowsAutoBias;
	DeclareConstIntCVar(e_GsmDepthBoundsDebug, 0);
	DeclareConstIntCVar(e_TimeOfDayDebug, 0);
	int   e_WaterTessellationAmount;
	int   e_Entities;
	int   e_CoverageBuffer;
	int   e_ScreenShotQuality;
	DeclareConstFloatCVar(e_FoliageBranchesDamping);
	int   e_levelStartupFrameNum;
	DeclareConstIntCVar(e_DecalsPreCreate, 1);
	float e_ParticlesLightsViewDistRatio;
	int   e_Brushes;
	int   e_SQTestCount;
	float e_GsmRange;
	int   e_ScreenShotMapOrientation;
	int   e_ScreenShotHeight;
	DeclareConstIntCVar(e_VegetationBoneInfo, 0);
	int   e_WaterOceanFFT;
	DeclareConstFloatCVar(e_MaxViewDistance);
	DeclareConstIntCVar(e_AutoPrecacheCameraJumpDist, 16);
	DeclareConstIntCVar(e_LodsForceUse, 1);
	int e_Particles;
	DeclareConstIntCVar(e_ParticlesDumpMemoryAfterMapLoad, 0);
	DeclareConstIntCVar(e_ForceDetailLevelForScreenRes, 0);
	DeclareConstIntCVar(e_TerrainTextureStreamingDebug, 0);
	DeclareConstIntCVar(e_3dEngineLogAlways, 0);
	float e_VegetationMinSize;
	DeclareConstIntCVar(e_DecalsHitCache, 1);
	DeclareConstIntCVar(e_TerrainOcclusionCullingDebug, 0);
	float e_ParticlesLod;
	DeclareConstIntCVar(e_BBoxes, 0);
	int   e_Vegetation;
	float e_TimeOfDaySpeed;
	int   e_LodMax;
	DeclareConstFloatCVar(e_ViewDistCompMaxSize);
	DeclareConstFloatCVar(e_TerrainTextureLodRatio);
	float e_ShadowsAdaptScale;
	float e_ScreenShotMapSizeX;
	int	e_ScreenShotMapResolution;
	float e_OcclusionCullingViewDistRatio;
	int   e_WaterOceanBottom;
	DeclareConstIntCVar(e_WaterRipplesDebug, 0);
	int   e_OnDemandPhysics;
	float e_ShadowsResScale;
	int   e_Recursion;
	DeclareConstIntCVar(e_StatObjValidate, e_StatObjValidateDefault);
	DeclareConstIntCVar(e_DecalsMaxValidFrames, 600);
	DeclareConstIntCVar(e_DecalsMerge, 0);
	DeclareConstFloatCVar(e_FoliageStiffness);
	int   e_SQTestDistance;
	float e_ViewDistMin;
	float e_StreamAutoMipFactorMin;
	DeclareConstIntCVar(e_LodMinTtris, 300);
	int   e_SkyQuality;
	DeclareConstIntCVar(e_ScissorDebug, 0);
	DeclareConstIntCVar(e_StatObjMergeMaxTrisPerDrawCall, 500);
	DeclareConstIntCVar(e_DynamicLightsConsistentSortOrder, 1);
	DeclareConstIntCVar(e_StreamCgfDebug, 0);
	float e_TerrainOcclusionCullingMaxDist;
	int   e_TerrainMeshInstancingMinLod;
	float e_TerrainMeshInstancingShadowLodRatio;
	float e_TerrainMeshInstancingShadowBias;
	int   e_StatObjTessellationMode;
	DeclareConstIntCVar(e_OcclusionLazyHideFrames, 0);
	int   e_CoverageBufferCullIndividualBrushesMaxNodeSize;
	DeclareConstFloatCVar(e_TerrainOcclusionCullingPrecision);
	float e_RenderMeshCollisionTolerance;
	int   e_ShadowsCacheUpdate;
	int   e_ShadowsCacheExtendLastCascade;
	int   e_ShadowsCacheMaxNodesPerFrame;
	int   e_ShadowsCacheObjectLod;
	int   e_ShadowsCacheRenderCharacters;
	int   e_ShadowsPerObject;
	int   e_DynamicDistanceShadows;
	float e_ShadowsPerObjectResolutionScale;
	int   e_ObjShadowCastSpec;
	DeclareConstFloatCVar(e_JointStrengthScale);
	float e_DecalsNeighborMaxLifeTime;
	DeclareConstFloatCVar(e_StreamCgfVisObjPriority);
	int   e_ObjectLayersActivation;
	float e_VegetationSpritesDistanceCustomRatioMin;
	float e_LodTransitionSpriteDistRatio;
	float e_LodTransitionSpriteMinDist;
	int   e_WaterTessellationAmountX;
	int   e_ScreenShotMinSlices;
	int   e_DecalsMaxUpdatesPerFrame;
	int   e_SkyType;
	int   e_GsmLodsNum;
	DeclareConstIntCVar(e_AutoPrecacheCgf, 1);
	DeclareConstIntCVar(e_HwOcclusionCullingWater, 1);
	DeclareConstIntCVar(e_DeferredPhysicsEvents, 1);
	float e_ParticlesMinDrawAlpha;
	float e_ShadowsCastViewDistRatioLights;
	int   e_ShadowsUpdateViewDistRatio;
	int   e_Lods;
	DeclareConstIntCVar(e_LodFaceArea, 1);
	float e_ShadowsConstBias;
	int   e_ParticlesCollisions;
	int   e_ParticlesObjectCollisions;
	int   e_ParticlesMinPhysicsDynamicBounds;
	int   e_ParticlesSortQuality;
	DeclareConstIntCVar(e_Ropes, 1);
	int   e_ShadowsPoolSize;
	int   e_ShadowsMaxTexRes;
	int   e_Sun;
	DeclareConstFloatCVar(e_MinMassDistanceCheckRenderMeshCollision);
	float e_DecalsRange;
	float e_ScreenShotMapCenterY;
	int   e_CacheNearestCubePicking;
	DeclareConstIntCVar(e_CoverCgfDebug, 0);
	DeclareConstFloatCVar(e_StreamCgfGridUpdateDistance);
	DeclareConstFloatCVar(e_LodCompMaxSize);
	float e_ViewDistRatioDetail;
	DeclareConstIntCVar(e_TerrainDetailMaterialsDebug, 0);
	DeclareConstIntCVar(e_Sleep, 0);
	DeclareConstIntCVar(e_TerrainOcclusionCullingStepSize, 4);
	int   e_Wind;
	int   e_SQTestMip;
	int   e_Shadows;
	int   e_ShadowsBlendCascades;
	float e_ShadowsBlendCascadesVal;
	float e_ParticlesMaxScreenFill;
	DeclareConstIntCVar(e_DebugDrawShowOnlyLod, -1);
	DeclareConstIntCVar(e_ScreenShot, 0);
	DeclareConstIntCVar(e_PrecacheLevel, 0);
	float e_ScreenShotMapCenterX;
	DeclareConstIntCVar(e_CoverageBufferDebug, 0);
	DeclareConstIntCVar(e_StatObjMerge, 1);
	DeclareConstIntCVar(e_StatObjStoreMesh, 0);
	ICVar* e_StreamCgfDebugFilter;
	float  e_VegetationSpritesMinDistance;
	float  e_TerrainDetailMaterialsViewDistZ;
	DeclareConstFloatCVar(e_VolObjShadowStrength);
	DeclareConstIntCVar(e_ParticlesDebug, 0);
	DeclareConstIntCVar(e_WaterOcean, e_WaterOceanDefault);
	float e_ViewDistRatio;
	float e_ViewDistRatioVegetation;
	ICVar* e_AutoViewDistRatio;
	float e_ViewDistRatioModifierGameDecals;
	DeclareConstIntCVar(e_ObjectLayersActivationPhysics, 1);
	DeclareConstIntCVar(e_StreamCgfDebugHeatMap, 0);
	DeclareConstFloatCVar(e_StreamPredictionDistanceFar);
	DeclareConstIntCVar(e_VegetationSpritesBatching, 1);
	DeclareConstIntCVar(e_CoverageBufferTerrain, 0);
	int   e_ParticlesThread;
	int   e_SQTestExitOnFinish;
	DeclareConstIntCVar(e_TerrainOcclusionCullingMaxSteps, 50);
	int   e_ParticlesUseLevelSpecificLibs;
	int   e_DecalsOverlapping;
	float e_DecalsSpawnDistRatio;
	int   e_CGFMaxFileSize;
	int   e_MaxDrawCalls;
	int   e_ClipVolumes;
#if !defined(_RELEASE)
	int   e_MergedMeshesClusterVisualization;
	int   e_MergedMeshesClusterVisualizationDimension;
#endif

	// ProcVegetation cvars
	int    e_ProcVegetation;
	int    e_ProcVegetationMaxSectorsInCache;
	int    e_ProcVegetationMaxChunksInCache;
	int    e_ProcVegetationMaxCacheLevels;
	int    e_ProcVegetationMaxViewDistance;
	int    e_ProcVegetationMaxObjectsInChunk;
	int    e_AutoPrecacheTerrainAndProcVeget;

	int    e_DebugGeomPrep;
	int    e_MergedMeshes;
	int    e_MergedMeshesDebug;
	int    e_MergedMeshesPool;
	int    e_MergedMeshesPoolSpines;
	int    e_MergedMeshesTesselationSupport;
	float  e_MergedMeshesViewDistRatio;
	float  e_MergedMeshesLodRatio;
	float  e_MergedMeshesInstanceDist;
	float  e_MergedMeshesActiveDist;
	float  e_MergedMeshesDeformViewDistMod;
	int    e_MergedMeshesUseSpines;
	float  e_MergedMeshesBulletSpeedFactor;
	float  e_MergedMeshesBulletScale;
	float  e_MergedMeshesBulletLifetime;
	int    e_MergedMeshesOutdoorOnly;
	int    e_MergedMeshesMaxTriangles;
	int    e_CheckOctreeObjectsBoxSize;
	DeclareConstIntCVar(e_GeomCaches, 1);
	int    e_GeomCacheBufferSize;
	int    e_GeomCacheMaxPlaybackFromMemorySize;
	int    e_GeomCachePreferredDiskRequestSize;
	float  e_GeomCacheMinBufferAheadTime;
	float  e_GeomCacheMaxBufferAheadTime;
	float  e_GeomCacheDecodeAheadTime;
	DeclareConstIntCVar(e_GeomCacheDebug, 0);
	ICVar* e_GeomCacheDebugFilter;
	DeclareConstIntCVar(e_GeomCacheDebugDrawMode, 0);
	DeclareConstIntCVar(e_GeomCacheLerpBetweenFrames, 1);

#if defined(FEATURE_SVO_GI)
	#include "SVO/SceneTreeCVars.inl" // include SVO related variables
#endif
};

#endif // _3DENGINE_CVARS_H_
