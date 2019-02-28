// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   cvars.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: console variables used in 3dengine
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "3dEngine.h"
#include <CryRenderer/IRenderer.h>
#include <Cry3DEngine/IStatObj.h>  // MAX_STATOBJ_LODS_NUM
#include <Cry3DEngine/ITimeOfDay.h>
#include <VisAreas.h>

//////////////////////////////////////////////////////////////////////////
void OnTimeOfDayVarChange(ICVar* pArgs)
{
	gEnv->p3DEngine->GetTimeOfDay()->SetTime(Cry3DEngineBase::GetCVars()->e_TimeOfDay);
}

void OnTimeOfDaySpeedVarChange(ICVar* pArgs)
{
	ITimeOfDay::SAdvancedInfo advInfo;
	gEnv->p3DEngine->GetTimeOfDay()->GetAdvancedInfo(advInfo);
	advInfo.fAnimSpeed = Cry3DEngineBase::GetCVars()->e_TimeOfDaySpeed;
	gEnv->p3DEngine->GetTimeOfDay()->SetAdvancedInfo(advInfo);
}

void OnCGFStreamingChange(ICVar* pArgs)
{
	if (gEnv->IsEditor())
	{
		Cry3DEngineBase::GetCVars()->e_StreamCgf = 0;
	}
}

void OnPerCharacterShadowsChange(ICVar* pArgs)
{
	if (Cry3DEngineBase::Get3DEngine()->GetObjectsTree())
	{
		Cry3DEngineBase::Get3DEngine()->GetObjectsTree()->MarkAsUncompiled();
	}

	if (Cry3DEngineBase::GetVisAreaManager())
	{
		Cry3DEngineBase::GetVisAreaManager()->MarkAllSectorsAsUncompiled();
	}
}

void OnGsmLodsNumChange(ICVar* pArgs)
{
	if (Cry3DEngineBase::GetRenderer())
	{
		Cry3DEngineBase::GetRenderer()->UpdateCachedShadowsLodCount(pArgs->GetIVal());
}
}

void OnDynamicDistanceShadowsVarChange(ICVar* pArgs)
{
	if (Cry3DEngineBase::Get3DEngine()->GetObjectsTree())
	{
		Cry3DEngineBase::Get3DEngine()->GetObjectsTree()->MarkAsUncompiled();
	}

	Cry3DEngineBase::Get3DEngine()->SetRecomputeCachedShadows(ShadowMapFrustum::ShadowCacheData::eFullUpdate);
}

void OnVegetationVisibleChange(ICVar* pArgs)
{
	COctreeNode* pObjectsTree = Cry3DEngineBase::Get3DEngine()->m_pObjectsTree;

	if (pObjectsTree && !pObjectsTree->IsEmpty())
	{
		if (pArgs->GetIVal() != 0)
		{
			pObjectsTree->PhysicalizeOfType(eRNListType_Vegetation, false);
		}
		else
		{
			pObjectsTree->DePhysicalizeOfType(eRNListType_Vegetation, false);
		}
	}
}

void CVars::Init()
{
	REGISTER_CVAR(e_PermanentRenderObjects, 1, VF_NULL, "Activates usage of permanent render objects\n2 = skip level objects where permanent render objects are used");

	DefineConstIntCVar(e_Fog, 1, VF_CHEAT | VF_CHEAT_ALWAYS_CHECK,
	                   "Activates global height/distance based fog");
	DefineConstIntCVar(e_FogVolumes, 1, VF_CHEAT | VF_CHEAT_ALWAYS_CHECK,
	                   "Activates local height/distance based fog volumes");
	REGISTER_CVAR(e_VolumetricFog, 0, VF_REQUIRE_APP_RESTART,
	              "Activates volumetric fog");
	REGISTER_CVAR(e_Entities, 1, VF_CHEAT | VF_CHEAT_ALWAYS_CHECK,
	              "Activates drawing of entities and brushes");
	DefineConstIntCVar(e_SkyBox, 1, VF_CHEAT,
	                   "Activates drawing of skybox and moving cloud layers");
	DefineConstIntCVar(e_WaterOcean, e_WaterOceanDefault, VF_CHEAT | VF_CHEAT_ALWAYS_CHECK,
	                   "Activates drawing of ocean. \n"
	                   "1: use usual rendering path\n"
	                   "2: use fast rendering path with merged fog");
	REGISTER_CVAR(e_WaterOceanBottom, 1, VF_NULL,
	                   "Activates drawing bottom of ocean");

	REGISTER_CVAR(e_WaterOceanFFT, 0, VF_NULL,
	              "Activates fft based ocean");

	DefineConstIntCVar(e_WaterRipplesDebug, 0, VF_CHEAT,
	                   "Draw water hits that affect water ripple simulation");

	REGISTER_CVAR(e_DebugDrawMaxDistance, 1000.0f, VF_NULL, 
				  "Draw distance for e_debugDraw text");

	DefineConstIntCVar(e_DebugDrawShowOnlyCompound, 0, VF_NULL,
	                   "e_DebugDraw shows only Compound (less efficient) static meshes");
	DefineConstIntCVar(e_DebugDrawShowOnlyLod, -1, VF_NULL,
	                   "e_DebugDraw shows only objects showing lod X");

	DefineConstIntCVar(e_DebugDraw, 0, VF_CHEAT | VF_CHEAT_ALWAYS_CHECK,
	                   "Draw helpers with information for each object (same number negative hides the text)\n"
	                   " 1: Name of the used cgf, polycount, used LOD\n"
	                   " 2: Color coded polygon count\n"
	                   " 3: Show color coded LODs count, flashing color indicates no Lod\n"
	                   " 4: Display object texture memory usage\n"
	                   " 5: Display color coded number of render materials\n"
	                   " 6: Display ambient color\n"
	                   " 7: Display tri count, number of render materials, texture memory\n"
	                   " 8: Display per object editor MaxViewDistance\n"
	                   " 9: Free slot\n"
	                   "10: Render geometry with simple lines and triangles\n"
	                   "11: Free slot\n"
	                   "12: Free slot\n"
	                   "13: Display occlusion amount (used during AO computations). Warning: can take a long time to calculate, depending on level size! \n"
	                   "15: Display helpers\n"
	                   "16: Display debug gun\n"
	                   "17: Streaming info (buffer sizes)\n"
	                   "18: Free slot\n"
	                   "19: Physics proxy triangle count\n"
	                   "20: Display Character attachments texture memory usage\n"
	                   "21: Display animated object distance to camera\n"
	                   "22: Display object's current LOD vertex count\n"
	                   "23: Display shadow casters in red\n"
	                   "24: Show LOD info for characters\n"
	                   "----------------debug draw list values. Any of them enable 2d on-screen listing type info debug. Specific values define the list sorting-----------\n"
	                   " 100: tri count\n"
	                   " 101: verts count\n"
	                   " 102: draw calls\n"
	                   " 103: texture memory\n"
	                   " 104: mesh memory");

#ifndef _RELEASE
	DefineConstIntCVar(e_DebugDrawListSize, 24, VF_DEV_ONLY, "num objects in the list for e_DebugDraw list infodebug");
	ICVar* e_DebugDrawListFilter = REGISTER_STRING_CB_DEV_ONLY("e_DebugDrawListFilter", "", VF_NULL,
	                                                           "filter for e_DebugDraw list. Combine object type letters to create the filter\n"
	                                                           "(example: e_DebugDrawListFilter BVC = shows Brushes+Vegetation+Characters). 'all' = no filter.\n"
	                                                           " B: Brushes\n"
	                                                           " V: Vegetation\n"
	                                                           " C: Character\n"
	                                                           " S: StatObj (non characters)\n", NULL);
	DefineConstIntCVar(e_DebugDrawListBBoxIndex, 0, VF_DEV_ONLY,
	                   "enables BBOX drawing for the 'n' element of the DebugDrawList (starting by 1.   0 = no bbox drawing).");
	REGISTER_COMMAND("e_DebugDrawListCMD", CDebugDrawListMgr::ConsoleCommand, VF_DEV_ONLY,
	                 "Issue commands to control e_DebugDraw list debuginfo behaviour\n"
	                 "'Freeze' (F) - stops refreshing stats\n"
	                 "'Continue' (C) - unfreezes\n"
	                 "'DumpLog' (D) - dumps the current on-screen info into the log");
#endif

#if !defined(_RELEASE)
	e_pStatObjRenderFilterStr = NULL;
	REGISTER_CVAR2("e_StatObjRenderFilter", &e_pStatObjRenderFilterStr, "", VF_NULL, "Debug: Controls which cgfs are rendered, based on input string");
	e_statObjRenderFilterMode = 0;
	REGISTER_CVAR2("e_StatObjRenderFilterMode", &e_statObjRenderFilterMode, 0, VF_NULL, "Debug: Controls how e_StatObjRenderFilter is use. 0=disabled 1=include 2=exclude");
#endif

	REGISTER_CVAR(e_TerrainDetailMaterials, 1, VF_CHEAT | VF_CHEAT_ALWAYS_CHECK,
	              "Activates drawing of detail materials on terrain ground");
	DefineConstIntCVar(e_TerrainDetailMaterialsDebug, 0, VF_CHEAT,
	                   "Shows number of materials in use per terrain sector");
	REGISTER_CVAR(e_TerrainDetailMaterialsViewDistZ, e_TerrainDetailMaterialsViewDistZDefault, VF_NULL,
	                     "Max view distance of terrain Z materials");
	REGISTER_CVAR(e_TerrainDetailMaterialsViewDistXY, e_TerrainDetailMaterialsViewDistXYDefault, VF_NULL,
	                     "Max view distance of terrain XY materials");
	DefineConstFloatCVar(e_SunAngleSnapSec, VF_NULL,
	                     "Sun dir snap control");
	DefineConstFloatCVar(e_SunAngleSnapDot, VF_NULL,
	                     "Sun dir snap control");

	REGISTER_CVAR(e_Particles, 1, VF_CHEAT | VF_CHEAT_ALWAYS_CHECK,
	              "Activates drawing of particles");
	DefineConstIntCVar(e_ParticlesDebug, 0, VF_CHEAT | VF_BITFIELD,
	                   "Particle debug flags: <flags> to set, <flags>+ to add, <flags>- to remove"
	                   " 1 = show basic stats"
	                   " m = show memory usage"
	                   " r = show reiteration, rejection, and collision stats"
	                   " b = draw bounding boxes and labels, show bounding box stats"
	                   " x = show bounding box stats"
	                   " d = force dynamic bounds and update for all emitters"
	                   " c = disable clipping against water and vis area bounds"
	                   " z = freeze particle system"
	                   " t = used by developers to debug test algorithms");
	REGISTER_CVAR(e_ParticlesThread, 4, VF_NULL,
	              "Enable particle threading: 1 = basic, 4 = optimal");
	REGISTER_CVAR(e_ParticlesCollisions, 3,  VF_BITFIELD,
	              "Enable collisions for non-physical particles:\n"
	              "  1 = terrain only, 2 = static objects also, 3 = dynamic objects also");
	REGISTER_CVAR(e_ParticlesObjectCollisions, -1, VF_CHEAT,
	              "Enable particle/object collisions for SimpleCollision:\n"
	              "  1 = against static objects only, 2 = dynamic also");
	REGISTER_CVAR(e_ParticlesMinPhysicsDynamicBounds, 2, VF_NULL,
	              "Min particle PhysicsType to force dynamic bounds computation:\n"
	              "  0 = never force, 1 = SimpleCollision, 2 = SimplePhysics, 3 = RigidBody");
	REGISTER_CVAR(e_ParticlesSortQuality, 1, VF_NULL,
	              "Minimum sort quality for new particle insertion:\n"
	              "  0 = basic, 1 = better, 2 = best");
	REGISTER_CVAR(e_ParticlesPreload, 0, VF_NULL,
	              "Enable preloading of all particle effects at the beginning");
	REGISTER_CVAR(e_ParticlesAllowRuntimeLoad, 1, VF_NULL,
	              "Allow loading of dynamic particle effects at runtime");
	REGISTER_CVAR(e_ParticlesConvertPfx1, 0, VF_NULL,
	              "Convert pfx1 to pfx2 when loaded. Combinable options:\n"
	              "  1 = Convert if pfx2 version doesn't exist.\n"
	              "  2 = Convert and overwrite pfx2 version.\n"
	              "  4 = Replace loaded pfx1 effects with pfx2 version.\n");
	DefineConstIntCVar(e_ParticlesSerializeNamedFields, 1, VF_NULL,
	                   "Save effects libraries with named fields for future compatibility (compatible with versions >= 24)");
	REGISTER_CVAR(e_ParticlesLod, 1, VF_NULL,
	              "Multiplier to particle count");
	REGISTER_CVAR(e_ParticlesMinDrawAlpha, 0.004f, VF_NULL,
	              "Alpha cutoff for rendering particles");
	REGISTER_CVAR(e_ParticlesMinDrawPixels, 0.5, VF_NULL,
	              "Pixel size min per particle -- fade out earlier");
	REGISTER_CVAR(e_ParticlesMaxDrawScreen, 2, VF_NULL,
	              "Screen size max per particle -- fade out earlier");
	REGISTER_CVAR(e_ParticlesMaxScreenFill, 64, VF_NULL,
	              "Screen size max of total particles to draw");
	DefineConstIntCVar(e_ParticlesCullAgainstViewFrustum, 1, VF_CHEAT,
	                   "Cull Particles against the view-frustum");
	DefineConstIntCVar(e_ParticlesCullAgainstOcclusionBuffer, 1, VF_CHEAT,
	                   "Cull Particles against the Occlusion Buffer");

	DefineConstIntCVar(e_ParticlesDumpMemoryAfterMapLoad, 0, VF_NULL,
	                   "Set to 1 to dump particle memory informations after map load");

	REGISTER_CVAR(e_ParticlesVertexPoolSize, 256, VF_NULL,
	              "Memory Size of Vertex Pool between Particle and Render Thread");

	REGISTER_CVAR(e_ParticlesIndexPoolSize, 16, VF_NULL,
	              "Memory Size of Index Pool between Particle and Render Thread");

	REGISTER_CVAR(e_ParticlesProfile, 0, VF_NULL,
	              "PFx1 only:\n"
	              "1 - always show statistics about particle pools usage\n"
	              "2 - disable the warning message when running out of pool memory");
	REGISTER_CVAR(e_ParticlesProfiler, 0, VF_BITFIELD,
	              "Wavicle only:\n"
	              "1 - Display performance profiler on screen\n"
	              "2 - Display memory profiler on screen\n"
	              "f - Output statistics to a csv file");
	e_ParticlesProfilerOutputFolder = REGISTER_STRING("e_ParticlesProfilerOutputFolder", "%USER%/ParticlesProfiler/", VF_NULL,
	                                                  "Folder to output particle profiler");
	e_ParticlesProfilerOutputName = REGISTER_STRING("e_ParticlesProfilerOutputName", "frame", VF_NULL,
	                                                "Name of the particle statistics file name");
	REGISTER_CVAR(e_ParticlesProfilerCountBudget, 80000, VF_NULL,
	              "Particle counts budget to be shown during profiling");
	REGISTER_CVAR(e_ParticlesProfilerTimingBudget, 10000, VF_NULL,
	              "Particle processing time budget (in nanoseconds) to be shown during profiling");

	REGISTER_CVAR(e_ParticlesForceSeed, 0, VF_NULL,
	              "0 - every emitter is random unless a seed is specified\n"
	              "n - uses this value as seed for all emitters without specified seed");

	DefineConstIntCVar(e_Roads, 1, VF_CHEAT | VF_CHEAT_ALWAYS_CHECK,
	                   "Activates drawing of road objects");

	REGISTER_CVAR(e_Decals, 1, VF_NULL | VF_CHEAT_ALWAYS_CHECK,
	              "Activates drawing of decals (game decals and hand-placed)");
	REGISTER_CVAR(e_DecalsForceDeferred, 0, VF_NULL,
	              "1 - force to convert all decals to use deferred ones");
	REGISTER_CVAR(e_DecalsDeferredStatic, 1, VF_NULL,
	              "1 - switch all non-planar decals placed by level designer to deferred");
	REGISTER_CVAR(e_DecalsDeferredDynamic, 1, VF_NULL,
	              "1 - make all game play decals deferred, 2 - make all game play decals non deferred");
	DefineConstFloatCVar(e_DecalsDeferredDynamicMinSize, VF_CHEAT,
	                     "Convert only dynamic decals bigger than X into deferred");
	DefineConstFloatCVar(e_DecalsDeferredDynamicDepthScale, VF_CHEAT,
	                     "Scale decal projection depth");
	DefineConstFloatCVar(e_DecalsPlacementTestAreaSize, VF_CHEAT,
	                     "Avoid spawning decals on the corners or edges of entity geometry");
	DefineConstFloatCVar(e_DecalsPlacementTestMinDepth, VF_CHEAT,
	                     "Avoid spawning decals on the corners or edges of entity geometry");
	REGISTER_CVAR(e_DecalsMaxTrisInObject, 8000, VF_NULL,
	              "Do not create decals on objects having more than X triangles");
	REGISTER_CVAR(e_DecalsAllowGameDecals, 1, VF_NULL,
	              "Allows creation of decals by game (like weapon bullets marks)");
	DefineConstIntCVar(e_DecalsHitCache, 1, VF_CHEAT,
	                   "Use smart hit caching for bullet hits (may cause no decals in some cases)");
	DefineConstIntCVar(e_DecalsMerge, 0, VF_NULL,
	                   "Combine pieces of decals into one render call");
	DefineConstIntCVar(e_DecalsPreCreate, 1, VF_NULL,
	                   "Pre-create decals at load time");
	DefineConstIntCVar(e_DecalsClip, 1, VF_NULL,
	                   "Clip decal geometry by decal bbox");
	REGISTER_CVAR(e_DecalsRange, e_DecalsRangeDefault, VF_NULL,
	                     "Less precision for decals outside this range");
	DefineConstFloatCVar(e_MinMassDistanceCheckRenderMeshCollision, VF_NULL,
	                     "Minimum mass to check for e_DecalsRange as distance in a RenderMesh Collision check");
	REGISTER_CVAR(e_DecalsLifeTimeScale, 1.f, VF_NULL,
	              "Allows to increase or reduce decals life time for different specs");
	REGISTER_CVAR(e_DecalsNeighborMaxLifeTime, 4.f, VF_NULL,
	              "If not zero - new decals will force old decals to fade in X seconds");
	REGISTER_CVAR(e_DecalsOverlapping, 0, VF_NULL,
	              "If zero - new decals will not be spawned if the distance to nearest decals less than X");
	DefineConstIntCVar(e_DecalsMaxValidFrames, 600, VF_NULL,
	                   "Number of frames after which not visible static decals are removed");
	REGISTER_CVAR(e_DecalsMaxUpdatesPerFrame, 4, VF_NULL,
	              "Maximum number of static decal render mesh updates per frame");
	REGISTER_CVAR(e_DecalsSpawnDistRatio, 4.f, VF_NULL,
	              "Max distance ratio for spawning dynamic decals.\n"
	              "This will be applied on top of e_ViewDistRatio");
	REGISTER_CVAR(e_VegetationBending, 2, VF_NULL,
	                   "Enable vegetation bending (does not affect merged grass)");
	REGISTER_CVAR(e_VegetationBillboards, 0, VF_NULL,
	              "Allow replacing distant vegetation with billboards\n"
	              "Billboard textures must be prepared by ed_GenerateBillboardTextures command in the editor");
	REGISTER_CVAR(e_VegetationUseTerrainColor, 1, VF_NULL,
	              "Allow blend with terrain color for vegetations");
	REGISTER_CVAR(e_VegetationUseTerrainColorDistance, 0, VF_NULL,
	              "Controls distance of fading into terrain color\n"
	              " 0 = Use 30% of maximum view distance of each vegetation instance (old default way)\n"
	              "<0 = Maximum view distance value is calculated using vegetation mesh size (non scaled CGF size) and then multiplied by this cvar\n"
	              ">0 = This value used directly as fading distance for all vegetations");
	REGISTER_CVAR(e_BrushUseTerrainColor, 0, VF_NULL,
	              "Allow blend with terrain color for brushes");
	REGISTER_CVAR_CB(e_Vegetation, 1, VF_CHEAT | VF_CHEAT_ALWAYS_CHECK,
	                 "Activates drawing of distributed objects like trees", OnVegetationVisibleChange); // it run Physicalize or UnPhysicalize, So when use this value on Game, you should check the Performance.
	REGISTER_CVAR(e_VegetationSprites, 0, VF_DEPRECATED,
	              "[DEPRECATED] Activates drawing of sprites instead of distributed objects at far distance");
	REGISTER_CVAR(e_LodTransitionSpriteMinDist, 4.f, VF_DEPRECATED,
	              "[DEPRECATED] The min dist over which vegetation sprites dissolve");
	REGISTER_CVAR(e_LodTransitionSpriteDistRatio, 0.1f, VF_DEPRECATED,
	              "[DEPRECATED] The fraction of vegetation sprite draw distance over which to dissolve");
	DefineConstIntCVar(e_VegetationSpritesBatching, 1, VF_DEPRECATED,
	                   "[DEPRECATED] Activates batch processing for sprites, optimizes CPU usage in case of dense vegetation");
	DefineConstIntCVar(e_VegetationBoneInfo, 0, VF_CHEAT,
	                   "Logs information about number of bones in each vegetation object loaded");
	REGISTER_CVAR(e_VegetationSpritesMinDistance, 8.0f, VF_DEPRECATED,
	              "[DEPRECATED] Sets minimal distance when distributed object can be replaced with sprite");

	REGISTER_CVAR(e_VegetationSpritesDistanceRatio, 1.0f, VF_DEPRECATED,
	              "[DEPRECATED] Allows changing distance on what vegetation switch into sprite");
	REGISTER_CVAR(e_VegetationSpritesDistanceCustomRatioMin, 1.0f, VF_DEPRECATED,
	              "[DEPRECATED] Clamps SpriteDistRatio setting in vegetation properties");
	DefineConstIntCVar(e_ForceDetailLevelForScreenRes, 0, VF_DEPRECATED,
	                   "[DEPRECATED] Force sprite distance and other values used for some specific screen resolution, 0 means current");
	REGISTER_CVAR(e_VegetationMinSize, 0.f, VF_REQUIRE_LEVEL_RELOAD | VF_CVARGRP_IGNOREINREALVAL,
	              "Minimal size of static object, smaller objects will be not rendered");
	REGISTER_CVAR(e_WindBendingStrength, 1.f, VF_CVARGRP_IGNOREINREALVAL,
	              "Minimal size of static object, smaller objects will be not rendered");
	REGISTER_CVAR(e_WindBendingAreaStrength, 1.f, VF_CVARGRP_IGNOREINREALVAL,
	              "Minimal size of static object, smaller objects will be not rendered");

	DefineConstFloatCVar(e_VegetationSpritesScaleFactor, VF_DEPRECATED,
	                     "[DEPRECATED] [0-1] amount to factor in scale of sprites in sprite switch distance.");

	REGISTER_CVAR(e_Wind, 1, VF_NULL,
	                   "Enable global wind calculations, affects vegetations bending animations");
	REGISTER_CVAR(e_WindAreas, 1, VF_NULL,
	                   "Debug");
	DefineConstFloatCVar(e_WindBendingDistRatio, VF_CHEAT,
	                     "Wind cutoff distance for bending (linearly attentuated to that distance)");
	REGISTER_CVAR(e_Shadows, 1, VF_NULL,
	              "Controls rendering of shadows\n"
	              "  0=off,\n"
	              "  1=on,\n"
	              "  2=sun shadows only,\n"
	              "  3=point light shadows only\n");
	REGISTER_CVAR(e_ShadowsBlendCascades, 1, VF_NULL,
	              "Blend between shadow cascades: 0=off, 1=on");
	REGISTER_CVAR(e_ShadowsBlendCascadesVal, 0.75f, VF_NULL,
	              "Size of cascade blend region");
#if CRY_PLATFORM_WINDOWS
	REGISTER_CVAR(e_ShadowsLodBiasFixed, 1, VF_NULL,
	              "Simplifies mesh for shadow map generation by X LOD levels");
#else
	DefineConstIntCVar(e_ShadowsLodBiasFixed, 0, VF_NULL,
	                   "Simplifies mesh for shadow map generation by X LOD levels");
#endif
	REGISTER_CVAR(e_ShadowsLodBiasInvis, 0, VF_NULL,
	                   "Simplifies mesh for shadow map generation by X LOD levels, if object is not visible in main frame");

	REGISTER_CVAR(e_Tessellation, 1, VF_NULL,
	                   "HW geometry tessellation  0 = not allowed, 1 = allowed");
	REGISTER_CVAR(e_TessellationMaxDistance, 30.f, VF_NULL,
	              "Maximum distance from camera in meters to allow tessellation, also affects distance-based displacement fadeout");
	REGISTER_CVAR(e_ShadowsTessellateCascades, 1, VF_NULL,
	                   "Maximum cascade number to render tessellated shadows (0 = no tessellation for sun shadows)");
	DefineConstIntCVar(e_ShadowsTessellateDLights, 0, VF_NULL,
	                   "Disable/enable tessellation for local lights shadows");
	REGISTER_CVAR(e_GsmCastFromTerrain, 1, VF_NULL,
	              "Allows sun shadows from terrain to be activated in editor level settings");
	DefineConstIntCVar(e_ShadowsFrustums, 0, VF_CHEAT,
	                   "Debug");
	DefineConstIntCVar(e_ShadowsDebug, 0, VF_CHEAT,
	                   "0=off,\n"
	                   "2=visualize shadow maps on the screen,\n"
	                   "4=visualize shadow frustums as spheres and cones\n");
	REGISTER_CVAR(e_ShadowsCacheUpdate, 0, VF_NULL,
	              "Trigger updates of the shadow cache: 0=no update, 1=one update, 2=continuous updates");
	REGISTER_CVAR(e_ShadowsCacheObjectLod, 0, VF_NULL,
	              "The lod used for rendering objects into the shadow cache. Set to -1 to disable");
	REGISTER_CVAR_CB(e_ShadowsCacheRenderCharacters, 0, VF_NULL,
	                 "Render characters into the shadow cache. 0=disabled, 1=enabled", OnDynamicDistanceShadowsVarChange);
	REGISTER_CVAR(e_ShadowsCacheExtendLastCascade, 0, VF_NULL,
	                 "Always render full extent of last cached shadow cascade. 0=disabled, 1=enabled");
	REGISTER_CVAR(e_ShadowsCacheMaxNodesPerFrame, 50, VF_NULL,
	                 "Maximum number of octree nodes to visit during incremental update. default: 50");
	REGISTER_CVAR_CB(e_DynamicDistanceShadows, 1, VF_NULL,
	                 "Enable dynamic distance shadows, 0 = disable, 1 = enable only for movable object types, 2 = enable for all object types, -1 = don't render dynamic distance shadows", OnDynamicDistanceShadowsVarChange);
	DefineConstIntCVar(e_ShadowsCascadesCentered, 0, VF_NULL,
	                   "Force shadow cascades to be centered 0=disable 1=enable ");
	DefineConstIntCVar(e_ShadowsCascadesDebug, 0, VF_CHEAT,
	                   "0=off, 1=visualize sun shadow cascades on screen");
	REGISTER_CVAR_CB(e_ShadowsPerObject, 0, VF_NULL,
	                 "Per object shadow maps 0=off, 1=on, -1=don't draw object shadows", OnPerCharacterShadowsChange);
	REGISTER_CVAR(e_ShadowsPerObjectResolutionScale, 1, VF_NULL,
	              "Global scale for per object shadow texture resolution\n"
	              "NOTE: individual texture resolution is rounded to next power of two ");
	REGISTER_CVAR(e_ShadowsClouds, 1, VF_NULL,
	              "Cloud shadows"); // no cheat var because this feature shouldn't be strong enough to affect gameplay a lot
	REGISTER_CVAR(e_ShadowsPoolSize, 2048, VF_NULL,
	              "Set size of shadow pool (e_ShadowsPoolSize*e_ShadowsPoolSize)");
	REGISTER_CVAR(e_ShadowsMaxTexRes, 1024, VF_NULL,
	              "Set maximum resolution of shadow map\n256(faster), 512(medium), 1024(better quality)");
	REGISTER_CVAR(e_ShadowsResScale, 2.8f, VF_NULL,
	              "Shadows slope bias for shadowgen");
	REGISTER_CVAR(e_ShadowsAdaptScale, 2.72f, VF_NULL,
	              "Shadows slope bias for shadowgen");
	REGISTER_CVAR(e_ShadowsSlopeBias, 1.0f, VF_NULL,
	              "Shadows slope bias for shadowgen");
	REGISTER_CVAR(e_ShadowsConstBias, 1.0f, VF_NULL,
	              "Shadows slope bias for shadowgen");
	REGISTER_CVAR(e_ShadowsAutoBias, 0.0f, VF_NULL,
	              "Attempts to compute an optimal shadow bias, ignoring all other bias settings (Experimental)\n"
	              "  0: Deactivated\n"
	              "  1.0: Good default value\n"
	              "  Other values scale bias relative to default\n");

	REGISTER_CVAR(e_ShadowsUpdateViewDistRatio, 128, VF_NULL,
	              "View dist ratio for shadow maps updating for shadowpool");
	REGISTER_CVAR(e_ShadowsCastViewDistRatioLights, e_ShadowsCastViewDistRatioLightsDefault, VF_NULL,
	                     "View dist ratio for shadow maps casting for light sources");
	REGISTER_CVAR(e_ShadowsCastViewDistRatio, 0.8f, VF_NULL,
	              "View dist ratio for shadow maps casting from objects");
	REGISTER_CVAR(e_GsmRange, 3.0f, VF_NULL,
	              "Size of LOD 0 GSM area (in meters)");
	REGISTER_CVAR(e_GsmRangeStep, 3.0f, VF_NULL,
	              "Range of next GSM lod is previous range multiplied by step");
	REGISTER_CVAR_CB(e_GsmLodsNum, 5, VF_NULL,
	                 "Number of GSM lods (0..5)", OnGsmLodsNumChange);
	DefineConstIntCVar(e_GsmDepthBoundsDebug, 0, VF_NULL,
	                   "Debug GSM bounds regions calculation");
	DefineConstIntCVar(e_GsmStats, 0, VF_CHEAT,
	                   "Show GSM statistics 0=off, 1=enable debug to the screens");
	REGISTER_CVAR(e_RNTmpDataPoolMaxFrames, 300, VF_NULL,
	              "Cache RNTmpData at least for X framres");

	REGISTER_CVAR(e_Terrain, 1, VF_CHEAT | VF_CHEAT_ALWAYS_CHECK,
	              "Activates drawing of terrain ground");
	REGISTER_CVAR(e_TerrainAutoGenerateBaseTexture, 0, VF_NULL,
	              "Instead of manually painting the base texture - just build it automatically based on terrain materials info painted");
	REGISTER_CVAR(e_TerrainAutoGenerateBaseTextureTiling, 1.f / 16.f, VF_NULL,
	              "Controls tiling of baked diffuse textures");
	REGISTER_CVAR(e_TerrainIntegrateObjectsMaxVertices, 30000, VF_NULL,
	              "Preallocate specified number of vertices to be used for objects integration into terrain (per terrain sector)\n"
	              "0 - disable the feature completelly");
	REGISTER_CVAR(e_TerrainIntegrateObjectsMaxHeight, 32.f, VF_NULL,
	              "Take only trianglses close to terrain for objects integration");
	DefineConstIntCVar(e_TerrainDeformations, 0, VF_CHEAT,
	                   "Allows in-game terrain surface deformations");
	REGISTER_CVAR(e_TerrainEditPostponeTexturesUpdate, 10, VF_NULL,
	              "Controls the postpone of terrain normal and elevation textures update during terrain sculpting in the editor");
	DefineConstIntCVar(e_AutoPrecacheCameraJumpDist, 16, VF_CHEAT,
	                   "When not 0 - Force full pre-cache of textures, procedural vegetation and shaders\n"
	                   "if camera moved for more than X meters in one frame or on new cut scene start");
	REGISTER_CVAR(e_AutoPrecacheTerrainAndProcVeget, 0, VF_CHEAT,
	              "Force auto pre-cache of terrain textures and procedural vegetation");
	DefineConstIntCVar(e_AutoPrecacheTexturesAndShaders, 0, VF_CHEAT,
	                   "Force auto pre-cache of general textures and shaders");
	DefineConstIntCVar(e_AutoPrecacheCgf, 1, VF_CHEAT,
	                   "Force auto pre-cache of CGF render meshes. 1=pre-cache all mehes around camera. 2=pre-cache only important ones (twice faster)");
	DefineConstIntCVar(e_TerrainBBoxes, 0, VF_CHEAT,
	                   "Show terrain nodes bboxes");
	DefineConstIntCVar(e_TerrainOcclusionCulling, 1, VF_CHEAT,
	                   "heightmap occlusion culling with time coherency 0=off, 1=on");
	DefineConstFloatCVar(e_TerrainOcclusionCullingStepSizeDelta, VF_CHEAT,
	                     "Step size scale on every next step (for version 1)");
	REGISTER_CVAR(e_TerrainOcclusionCullingMaxDist, 200.f, VF_NULL,
	              "Max length of ray (for version 1)");
	REGISTER_CVAR(e_TerrainMeshInstancingMinLod, 3, VF_NULL,
	              "Mesh instancing is used for distant terrain sectors and for shadow map generation");
	REGISTER_CVAR(e_TerrainMeshInstancingShadowLodRatio, 0.3f, VF_NULL,
	              "Smaller values produce less draw calls and less polygons for terrain shadow map generation");
	REGISTER_CVAR(e_TerrainMeshInstancingShadowBias, 0.5f, VF_NULL,
	              "During shadow map generation render distant terrain sectors little lower for less problems with terrain self-shadowing");
	REGISTER_CVAR(e_StreamPredictionUpdateTimeSlice, 0.3f, VF_NULL,
	              "Maximum amount of time to spend for scene streaming priority update in milliseconds");
	REGISTER_CVAR(e_StreamAutoMipFactorSpeedThreshold, 0.f, VF_NULL,
	              "Debug");
	REGISTER_CVAR(e_StreamAutoMipFactorMin, 0.5f, VF_NULL,
	              "Debug");
	REGISTER_CVAR(e_StreamAutoMipFactorMax, 1.0f, VF_NULL,
	              "Debug");
	REGISTER_CVAR(e_StreamAutoMipFactorMaxDVD, 0.5f, VF_NULL,
	              "Debug");
	DefineConstIntCVar(e_TerrainOcclusionCullingDebug, 0, VF_CHEAT,
	                   "Draw sphere on every terrain height sample");
	DefineConstIntCVar(e_TerrainOcclusionCullingMaxSteps, 50, VF_CHEAT,
	                   "Max number of tests per ray (for version 0)");
	DefineConstIntCVar(e_TerrainOcclusionCullingStepSize, 4, VF_CHEAT,
	                   "Initial size of single step (in heightmap units)");
	DefineConstIntCVar(e_TerrainTextureStreamingDebug, 0, VF_CHEAT,
	                   "Debug");
	REGISTER_CVAR(e_TerrainTextureStreamingPoolItemsNum, 256, VF_REQUIRE_LEVEL_RELOAD,
	              "Specifies number of textures in terrain base texture streaming pool");
	DefineConstIntCVar(e_TerrainDrawThisSectorOnly, 0, VF_CHEAT,
	                   "1 - render only sector where camera is and objects registered in sector 00\n"
	                   "2 - render only sector where camera is");
	DefineConstFloatCVar(e_TerrainOcclusionCullingPrecision, VF_CHEAT,
	                     "Density of rays");
	DefineConstFloatCVar(e_TerrainOcclusionCullingPrecisionDistRatio, VF_CHEAT,
	                     "Controls density of rays depending on distance to the object");

	REGISTER_CVAR(e_TerrainLodDistanceRatio, 0.5f, VF_NULL,
	              "Controls heightmap LOD by comparing sector distance with a sector size");
	REGISTER_CVAR(e_TerrainLodErrorRatio, 0.05f, VF_NULL,
	              "Controls heightmap LOD by comparing sector distance with the maximum elevation difference between the sector and its childs");

	REGISTER_CVAR(e_OcclusionCullingViewDistRatio, 0.5f, VF_NULL,
	              "Skip per object occlusion test for very far objects - culling on tree level will handle it");

	DefineConstFloatCVar(e_TerrainTextureLodRatio, VF_NULL,
	                     "Adjust terrain base texture resolution on distance");

	REGISTER_CVAR(e_Sun, 1, VF_CHEAT,
	              "Activates sun light source");
	REGISTER_CVAR(e_CoverageBuffer, 1, VF_NULL,
	              "Activates usage of software coverage buffer.\n"
	              "1 - camera culling only\n"
	              "2 - camera culling and light-to-object check");
	REGISTER_CVAR(e_CoverageBufferCullIndividualBrushesMaxNodeSize, 16, VF_CHEAT,
	              "128 - cull only nodes of scene tree and very big brushes\n"
	              "0 - cull all brushes individually");
	DefineConstIntCVar(e_CoverageBufferTerrain, 0, VF_NULL,
	                   "Activates usage of coverage buffer for terrain");
	DefineConstIntCVar(e_CoverageBufferDebug, 0, VF_CHEAT,
	                   "Display content of main camera coverage buffer");
	DefineConstIntCVar(e_CoverageBufferDebugFreeze, 0, VF_CHEAT,
	                   "Freezes view matrix/-frustum ");
	DefineConstIntCVar(e_CoverageBufferDrawOccluders, 0, VF_CHEAT,
	                   "Debug draw of occluders for coverage buffer");
	REGISTER_CVAR(e_CoverageBufferBias, 0.05f, VF_NULL,
	              "Coverage buffer z-biasing");
	REGISTER_CVAR(e_CoverageBufferAABBExpand, 0.020f, VF_NULL,
	              "expanding the AABB's of the objects to test to avoid z-fighting issues in the Coverage buffer");
	REGISTER_CVAR(e_CoverageBufferEarlyOut, 1, VF_NULL,
	              "preempting occluder rasterization to avoid stalling in the main thread if rendering is faster");
	REGISTER_CVAR(e_CoverageBufferEarlyOutDelay, 3.0f, VF_NULL,
	              "Time in ms that rasterizer is allowed to continue working after early out request");
	REGISTER_CVAR(e_CoverageBufferTerrainExpand, 0.025f, VF_NULL,
	              "expanding the AABB Z axis of terrain to avoid flat terrain flickering");

	REGISTER_CVAR(e_CoverageBufferReproj, 6, VF_NULL,
	              "Selects occlusion culling mode\n"
	              "  1 - Simple reprojection\n"
	              "  2 - Reprojection with hole filling\n"
	              "  4 - Rasterized occlusion meshes\n"
	              "  6 - Reprojection and occlusion meshes");
	REGISTER_CVAR(e_CoverageBufferRastPolyLimit, 60000, VF_NULL,
	              "maximum amount of polys to rasterize cap, 0 means no limit\ndefault is 500000");
	REGISTER_CVAR(e_CoverageBufferShowOccluder, 0, VF_NULL,
	              "1 show only meshes used as occluder, 2 show only meshes not used as occluder");
	REGISTER_CVAR(e_CoverageBufferOccludersViewDistRatio, 1.0f, VF_CHEAT,
	              "Debug");

	REGISTER_CVAR(e_DynamicLightsMaxCount, 512, VF_NULL,
	                   "Sets maximum amount of dynamic light sources");

	DefineConstIntCVar(e_DynamicLights, 1, VF_CHEAT,
	                   "Activates dynamic light sources");
	DefineConstIntCVar(e_DynamicLightsForceDeferred, 1, VF_CHEAT,
	                   "Convert all lights to deferred (except sun)");
	REGISTER_CVAR(e_DynamicLightsFrameIdVisTest, 1, VF_NULL,
	              "Use based on last draw frame visibility test");
	DefineConstIntCVar(e_DynamicLightsConsistentSortOrder, 1, VF_NULL,
	                   "Debug");

	DefineConstIntCVar(e_HwOcclusionCullingWater, 1, VF_NULL,
	                   "Activates usage of HW occlusion test for ocean");

	DefineConstIntCVar(e_Portals, 1, VF_CHEAT,
	                   "Activates drawing of visareas content (indoors), values 2,3,4 used for debugging");
	DefineConstIntCVar(e_PortalsBigEntitiesFix, 1, VF_CHEAT,
	                   "Enables special processing of big entities like vehicles intersecting portals");
	DefineConstIntCVar(e_PortalsBlend, 1, VF_CHEAT,
	                   "Blend lights and cubemaps of vis areas connected to portals 0=off, 1=on");
	REGISTER_CVAR(e_PortalsMaxRecursion, 8, VF_NULL,
	              "Maximum number of visareas and portals to traverse for indoor rendering");
	REGISTER_CVAR(e_DynamicLightsMaxEntityLights, 16, VF_NULL,
	              "Set maximum number of lights affecting object");
	DefineConstFloatCVar(e_MaxViewDistance, VF_CHEAT,
	                     "Far clipping plane distance");
	REGISTER_CVAR(e_MaxViewDistSpecLerp, 1, VF_NULL,
	              "1 - use max view distance set by designer for very high spec\n0 - for very low spec\nValues between 0 and 1 - will lerp between high and low spec max view distances");
	DefineConstFloatCVar(e_MaxViewDistFullDistCamHeight, VF_CHEAT,
	                     "Debug");
	DefineConstIntCVar(e_WaterVolumes, e_WaterVolumesDefault, VF_CHEAT,
	                   "Activates drawing of water volumes\n"
	                   "1: use usual rendering path\n"
	                   "2: use fast rendering path with merged fog");
	DefineConstIntCVar(e_WaterWaves, 0, VF_CHEAT,
	                   "Activates drawing of water waves");
	DefineConstIntCVar(e_WaterWavesTessellationAmount, 5, VF_NULL,
	                   "Sets water waves tessellation amount");
	REGISTER_CVAR(e_WaterTessellationAmount, 10, VF_NULL,
	              "Set tessellation amount");
	REGISTER_CVAR(e_WaterTessellationAmountX, 10, VF_NULL,
	              "Set tessellation on x axis - 0 means not used");
	REGISTER_CVAR(e_WaterTessellationAmountY, 10, VF_NULL,
	              "Set tessellation on y axis - 0 means not used");
	REGISTER_CVAR(e_WaterTessellationSwathWidth, 12, VF_NULL,
	              "Set the swath width for the boustrophedonic mesh stripping");
	DefineConstIntCVar(e_BBoxes, 0, VF_CHEAT,
	                   "Activates drawing of bounding boxes");

	DefineConstIntCVar(e_StreamSaveStartupResultsIntoXML, 0, VF_NULL,
	                   "Save basic information about streaming performance on level start into XML");
	REGISTER_CVAR(e_StreamCgfPoolSize, 128, VF_NULL,
	              "Render mesh cache size in MB");
	REGISTER_CVAR(e_SQTestBegin, 0, VF_NULL,
	              "If not zero - start streaming latency unit test");
	REGISTER_CVAR(e_SQTestCount, 0, VF_NULL,
	              "If not zero - restart test X times");
	REGISTER_CVAR(e_SQTestExitOnFinish, 0, VF_NULL,
	              "If not zero - shutdown when finished testing");
	REGISTER_CVAR(e_SQTestDistance, 80, VF_NULL,
	              "Distance to travel");
	REGISTER_CVAR(e_SQTestMip, 1, VF_NULL,
	              "Mip to wait during test");
	REGISTER_CVAR(e_SQTestMoveSpeed, 10, VF_NULL,
	              "Camera speed during test (meters/sec)");

	// Small temp pool size for consoles, editor and pc have much larger capabilities
	DefineConstIntCVar(e_3dEngineTempPoolSize, 1024, VF_NULL,
	                   "pool size for temporary allocations in kb, requires app restart");

	DefineConstIntCVar(e_3dEngineLogAlways, 0, VF_NULL,
	                   "Set maximum verbosity to 3dengine.dll log messages");

	DefineConstIntCVar(e_CoverCgfDebug, 0, VF_NULL, "Shows the cover setups on cfg files");

	REGISTER_CVAR(e_StreamCgfMaxTasksInProgress, 32, VF_CHEAT,
	              "Maximum number of files simultaneously requested from streaming system");
	REGISTER_CVAR(e_StreamCgfMaxNewTasksPerUpdate, 4, VF_CHEAT,
	              "Maximum number of files requested from streaming system per update");
	REGISTER_CVAR(e_StreamPredictionMaxVisAreaRecursion, 9, VF_CHEAT,
	              "Maximum number visareas and portals to traverse.");
	REGISTER_CVAR(e_StreamPredictionBoxRadius, 1, VF_CHEAT, "Radius of stream prediction box");
	REGISTER_CVAR(e_StreamPredictionTexelDensity, 1, VF_CHEAT,
	              "Use mesh texture mapping density info for textures streaming");
	REGISTER_CVAR(e_StreamPredictionAlwaysIncludeOutside, 0, VF_CHEAT,
	              "Always include outside octrees in streaming");
	DefineConstFloatCVar(e_StreamCgfFastUpdateMaxDistance, VF_CHEAT,
	                     "Update streaming priorities for near objects every second frame");
	DefineConstFloatCVar(e_StreamPredictionMinFarZoneDistance, VF_CHEAT,
	                     "Debug");
	DefineConstFloatCVar(e_StreamPredictionMinReportDistance, VF_CHEAT,
	                     "Debug");
	REGISTER_CVAR(e_StreamInstances, 0, VF_CHEAT,
	              "Enable streaming of static world instances (usually vegetation and decals, not used in editor)");
	REGISTER_CVAR(e_StreamInstancesMaxTasks, 32, VF_CHEAT,
	              "Maximum allowed number of file streaming requests for instances streaming (one request reads single octree node)");
	REGISTER_CVAR(e_StreamInstancesDistRatio, 1.f, VF_CHEAT,
	              "Controls streaming distance");
	REGISTER_CVAR_CB(e_StreamCgf, 1, VF_REQUIRE_APP_RESTART,
	                 "Enable streaming of static render meshes", OnCGFStreamingChange);
	DefineConstIntCVar(e_StreamCgfDebug, 0, VF_NULL,
	                   "Draw helpers and other debug information about CGF streaming\n"
	                   " 1: Draw color coded boxes for objects taking more than e_StreamCgfDebugMinObjSize,\n"
	                   "    also shows are the LOD's stored in single CGF or were split into several CGF's\n"
	                   " 2: Trace into console every loading and unloading operation\n"
	                   " 3: Print list of currently active objects taking more than e_StreamCgfDebugMinObjSize KB");
	DefineConstIntCVar(e_StreamCgfDebugMinObjSize, 100, VF_CHEAT,
	                   "Threshold for objects debugging in KB");
	DefineConstIntCVar(e_StreamCgfDebugHeatMap, 0, VF_CHEAT,
	                   "Generate and show mesh streaming heat map\n"
	                   " 1: Generate heat map for entire level\n"
	                   " 2: Show last heat map");
	DefineConstFloatCVar(e_StreamPredictionDistanceFar, VF_CHEAT,
	                     "Prediction distance for streaming, affects far objects");
	DefineConstFloatCVar(e_StreamPredictionDistanceNear, VF_CHEAT,
	                     "Prediction distance for streaming, affets LOD of objects");
	DefineConstFloatCVar(e_StreamCgfVisObjPriority, VF_CHEAT,
	                     "Priority boost for visible objects\n"
	                     "0 - visible objects has no priority over invisible objects, camera direction does not affect streaming\n"
	                     "1 - visible objects has highest priority, in case of trashing will produce even more trashing");

	DefineConstFloatCVar(e_StreamCgfGridUpdateDistance, VF_CHEAT,
	                     "Update streaming priorities when camera moves more than this value");

	DefineConstFloatCVar(e_StreamPredictionAhead, VF_CHEAT,
	                     "Use preducted camera position for streaming priority updates");

	DefineConstFloatCVar(e_StreamPredictionAheadDebug, VF_CHEAT,
	                     "Draw ball at predicted position");

	DefineConstIntCVar(e_StreamCgfUpdatePerNodeDistance, 1, VF_CHEAT,
	                   "Use node distance as entity distance for far nodex ");

	DefineConstIntCVar(e_ScissorDebug, 0, VF_CHEAT,
	                   "Debug");

	REGISTER_CVAR(e_Brushes, 1, VF_CHEAT | VF_CHEAT_ALWAYS_CHECK,
	              "Draw brushes");

	//  REGISTER_CVAR(e_SceneMerging, 0, VF_CHEAT,
	//	"Merge brushes on level loading");
	//  REGISTER_CVAR(e_SceneMergingViewDistRatio, 3.f, VF_CHEAT,
	//  "Controls size of rendered nodes depends from distance");
	//  REGISTER_CVAR(e_SceneMergingMinNodeSize, 16.f, VF_CHEAT,
	//  "Min size of merge area (smallest areas are used near camera)");
	//  REGISTER_CVAR(e_SceneMergingMaxTrisPerInputChunk, 100, VF_CHEAT,
	//  "Merge only chunks containing less than X triangles");
	/*	REGISTER_CVAR(e_scene_merging_compact_vertices, 1, VF_NULL,
	    "Minimize number of vertices in decals and merged brushes");
	   REGISTER_CVAR(e_scene_merging_show_onlymerged, 0, VF_CHEAT,
	    "Show only merged brushes");
	   REGISTER_CVAR(e_scene_merging_max_tris_in_chunk, 64, VF_NULL,
	    "Only objects having less than X triangles per chunk will me merged");
	   REGISTER_CVAR(e_scene_merging_max_time_ms, 5, VF_NULL,
	    "Spend only X ms per frame on merging");
	   REGISTER_CVAR(e_scene_merging_min_merge_distance, 0, VF_NULL,
	    "Don't merge nodes closer than X");*/

	REGISTER_CVAR(e_OnDemandPhysics, 1, VF_NULL,
	              "Turns on on-demand physicalization (0=off, 1=vegetation only[default], 2=brushes only, 3=brushes&vegetation");
	REGISTER_CVAR(e_OnDemandMaxSize, 20.0f, VF_NULL,
	              "Specifies the maximum size of vegetation objects that are physicalized on-demand");
	DefineConstIntCVar(e_Sleep, 0, VF_CHEAT,
	                   "Sleep X in C3DEngine::Draw");
	REGISTER_CVAR(e_ObjectLayersActivation, (m_bEditor ? 0 : 1), VF_CHEAT, "Allow game to activate/deactivate object layers");
	DefineConstIntCVar(e_ObjectLayersActivationPhysics, 1, VF_CHEAT,
	                   "Allow game to create/free physics of objects: 0: Disable; 1: All; 2: Water only.");
	DefineConstIntCVar(e_Objects, 1, VF_CHEAT,
	                   "Render or not all objects");
	DefineConstIntCVar(e_Render, e_RenderDefault, VF_CHEAT,
	                   "Enable engine rendering");
	DefineConstIntCVar(e_ObjectsTreeBBoxes, 0, VF_CHEAT,
	                   "Debug draw of object tree bboxes");
	REGISTER_CVAR(e_ObjectsTreeNodeMinSize, 8.f, VF_CHEAT,
	              "Controls objects tree balancing");
	REGISTER_CVAR(e_ObjectsTreeNodeSizeRatio, 1.f / 16.f, VF_CHEAT,
	              "Controls objects tree balancing");
	REGISTER_CVAR(e_ExecuteRenderAsJobMask, BIT(eERType_Brush) | BIT(eERType_Vegetation) | BIT(eERType_Road) | BIT(eERType_WaterVolume), VF_NULL,
	              "Each bit specifies object type to render it in jobs");
	REGISTER_CVAR(e_ObjectsTreeLevelsDebug, 0, VF_CHEAT,
	              "If non 0 - render only octree nodes of specified size");
	REGISTER_CVAR(e_StatObjBufferRenderTasks, 1, VF_NULL,
	              "1 - occlusion test on render node level, 2 - occlusion test on render mesh level");
	REGISTER_CVAR(e_CheckOcclusion, 1, VF_NULL, "Perform a visible check in check occlusion job");

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT // for big dense levels and for editor
	#define DEFAULT_CHECK_OCCLUSION_QUEUE_SIZE        4096
	#define DEFAULT_CHECK_OCCLUSION_OUTPUT_QUEUE_SIZE 32768
#else
	#define DEFAULT_CHECK_OCCLUSION_QUEUE_SIZE        1024
	#define DEFAULT_CHECK_OCCLUSION_OUTPUT_QUEUE_SIZE 4096
#endif

	REGISTER_CVAR(e_CheckOcclusionQueueSize, DEFAULT_CHECK_OCCLUSION_QUEUE_SIZE, VF_NULL,
	              "Size of queue for data send to check occlusion job");
	REGISTER_CVAR(e_CheckOcclusionOutputQueueSize, DEFAULT_CHECK_OCCLUSION_OUTPUT_QUEUE_SIZE, VF_NULL,
	              "Size of queue for data send from check occlusion job");
	REGISTER_CVAR(e_StatObjTessellationMode, 1, VF_CHEAT,
	              "Set they way pre-tessellated version of meshes is created: 0 = no pre-tessellation, 1 = load from disk, 2 = generate from normal mesh on loading");
	DefineConstIntCVar(e_ObjStats, 0, VF_CHEAT,
	                   "Show instances count");
	DefineConstIntCVar(e_ObjFastRegister, 1, VF_CHEAT,
	                   "Debug");

	DefineConstIntCVar(e_OcclusionLazyHideFrames, 0, VF_CHEAT,
	                   "Makes less occluson tests, but it takes more frames to detect invisible objects");
	DefineConstIntCVar(e_OcclusionVolumes, e_OcclusionVolumesDefault, VF_CHEAT,
	                   "Enable occlusion volumes(antiportals)");
	DefineConstFloatCVar(e_OcclusionVolumesViewDistRatio, VF_NULL,
	                     "Controls how far occlusion volumes starts to occlude objects");

	DefineConstIntCVar(e_PrecacheLevel, 0, VF_NULL,
	                   "Pre-render objects right after level loading");

	REGISTER_CVAR(e_Lods, 1, VF_NULL,
	                   "Load and use LOD models for static geometry");
	DefineConstIntCVar(e_LodFaceArea, 1, VF_NULL,
	                   "Use geometric mean of faces area to compute LOD");
	DefineConstIntCVar(e_LodsForceUse, 1, VF_NULL,
	                   "Force using LODs even if triangle count do not suit");

	REGISTER_CVAR(e_SQTestDelay, 5.f, VF_NULL,
	              "Time to stabilize the system before camera movements");

	REGISTER_CVAR(e_ProcVegetation, 1, VF_NULL,
	              "Show procedurally distributed vegetation");
	REGISTER_CVAR(e_ProcVegetationMaxViewDistance, 128, VF_NULL,
	              "Maximum distance where procedural objects may be spawn for heightmap quad-tree level 0, every next level multiply it by 2");
	REGISTER_CVAR(e_ProcVegetationMaxCacheLevels, 1, VF_REQUIRE_APP_RESTART,
	              "Number of heightmap quad-tree levels used for vegetation spawning");
	REGISTER_CVAR(e_ProcVegetationMaxSectorsInCache, 16, VF_REQUIRE_APP_RESTART,
	              "Maximum number of 64x64 meter sectors cached in memory");
	REGISTER_CVAR(e_ProcVegetationMaxChunksInCache, 128, VF_REQUIRE_APP_RESTART,
	              "Maximum number of object chunks cached in memory");
	REGISTER_CVAR(e_ProcVegetationMaxObjectsInChunk, 1024, VF_REQUIRE_APP_RESTART,
	              "Maximum number of instances per chunk");

	REGISTER_CVAR(e_Recursion, 1, VF_NULL,
	                   "If 0 - will skip recursive render calls like render into texture");
	REGISTER_CVAR(e_RecursionViewDistRatio, 0.1f, VF_NULL,
	              "Set all view distances shorter by factor of X");

	REGISTER_CVAR(e_Clouds, 0, VF_NULL,
	              "Enable clouds rendering");

	REGISTER_CVAR(e_SkyUpdateRate, 0.12f, VF_NULL,
	              "Percentage of a full dynamic sky update calculated per frame (0..100].");
	REGISTER_CVAR(e_SkyQuality, 1, VF_NULL,
	                   "Quality of dynamic sky: 1 (very high), 2 (high).");
	REGISTER_CVAR(e_SkyType, 1, VF_NULL,
	                   "Type of sky used: 0 (static), 1 (dynamic).");

	DefineConstIntCVar(e_DisplayMemoryUsageIcon, e_DisplayMemoryUsageIconDefault, VF_NULL,
	                   "Turns On/Off the memory usage icon rendering: 1 on, 0 off.");

	REGISTER_CVAR(e_LodRatio, 6.0f, VF_NULL,
	              "LOD distance ratio for objects");
	REGISTER_CVAR(e_LodTransitionTime, 0.5f, VF_NULL,
	              "If non 0 - use dissolve for smooth LOD transition");
	REGISTER_CVAR(e_LodFaceAreaTargetSize, 0.005f, VF_NULL,
	              "Threshold used for LOD computation.");
	DefineConstFloatCVar(e_LodCompMaxSize, VF_NULL,
	                     "Affects LOD selection for big objects, small number will switch more objects into lower LOD");
	REGISTER_CVAR(e_ViewDistRatio, 60.0f, VF_CVARGRP_IGNOREINREALVAL | VF_LIVE_CREATE_SYNCED,
	              "View distance ratio for objects");
	DefineConstFloatCVar(e_ViewDistCompMaxSize, VF_NULL | VF_LIVE_CREATE_SYNCED,
	                     "Affects max view distance for big objects, small number will render less objects");
	REGISTER_CVAR(e_ViewDistRatioPortals, e_ViewDistRatioPortalsDefault, VF_NULL | VF_LIVE_CREATE_SYNCED,
	                     "View distance ratio for portals");
	REGISTER_CVAR(e_ViewDistRatioDetail, 30.0f, VF_NULL | VF_LIVE_CREATE_SYNCED,
	              "View distance ratio for detail objects");
	REGISTER_CVAR(e_ViewDistRatioVegetation, 30.0f, VF_CVARGRP_IGNOREINREALVAL | VF_LIVE_CREATE_SYNCED,
	              "View distance ratio for vegetation");
	REGISTER_CVAR(e_ViewDistRatioModifierGameDecals, 4.0f, VF_NULL | VF_LIVE_CREATE_SYNCED,
	              "View distance ratio for dynamically generated decals"
	              "This will be applied on top of e_ViewDistRatio");
	REGISTER_CVAR(e_ViewDistRatioLights, 50.0f, VF_NULL | VF_LIVE_CREATE_SYNCED,
	              "View distance ratio for light sources");
	REGISTER_CVAR(e_LightIlluminanceThreshold, ILLUMINANCE_THRESHOLD, VF_NULL | VF_LIVE_CREATE_SYNCED,
	              "Min illuminance to determine light effect radius");
	REGISTER_CVAR(e_ViewDistRatioCustom, 60.0f, VF_NULL | VF_LIVE_CREATE_SYNCED,
	              "View distance ratio for special marked objects (Players,AI,Vehicles)");
	REGISTER_CVAR(e_ViewDistMin, 0.0f, VF_NULL | VF_LIVE_CREATE_SYNCED,
	              "Min distance on what far objects will be culled out");
	e_AutoViewDistRatio = REGISTER_STRING("e_AutoViewDistRatio", "0", VF_NULL,
	                                      CVARHELP("If " " - will be skip auto calculating the ViewDistRatio for a selected object"));
	REGISTER_CVAR(e_LodMin, 0, VF_NULL,
	              "Min LOD for objects");
	REGISTER_CVAR(e_RenderMeshCollisionLod, 1, VF_NULL,
	              "Lod for render mesh collision check test");
	REGISTER_CVAR(e_CharLodMin, 0, VF_NULL,
	              "Min LOD for character objects");
	REGISTER_CVAR(e_LodMax, MAX_STATOBJ_LODS_NUM - 1, VF_NULL,
	              "Max LOD for objects");
	DefineConstIntCVar(e_LodMinTtris, 300, VF_CHEAT,
	                   "LODs with less triangles will not be used");
	DefineConstIntCVar(e_DebugLights, 0, VF_CHEAT,
	                   "Use different colors for objects affected by different number of lights\n"
	                   "black:0, blue:1, green:2, red:3 or more, blinking yellow: more then the maximum enabled\n"
	                   "Obsolete for deferred lights. Use r_DeferredShadingDebug.");
	REGISTER_CVAR(e_PhysMinCellSize, 4, VF_NULL,
	              "Min size of cell in physical entity grid");
	REGISTER_CVAR(e_PhysProxyTriLimit, 5000, VF_NULL,
	              "Maximum allowed triangle count for phys proxies");
	REGISTER_CVAR(e_PhysFoliage, 2, VF_NULL,
	                   "Enables physicalized foliage\n"
	                   "1 - only for dynamic objects\n"
	                   "2 - for static and dynamic)");
	DefineConstIntCVar(e_RenderMeshUpdateAsync, 1, VF_NULL,
	                   "Enables async updating of dynamically updated rendermeshes\n"
	                   "0 - performs a synchronous update\n"
	                   "1 - performs the update in an async job (default))");
	DefineConstFloatCVar(e_FoliageStiffness, VF_NULL,
	                     "Stiffness of the spongy obstruct geometry");
	DefineConstFloatCVar(e_FoliageBranchesStiffness, VF_NULL,
	                     "Stiffness of branch ropes");
	DefineConstFloatCVar(e_FoliageBranchesDamping, VF_NULL,
	                     "Damping of branch ropes");
	DefineConstFloatCVar(e_FoliageBrokenBranchesDamping, VF_NULL,
	                     "Damping of branch ropes of broken vegetation");
	REGISTER_CVAR(e_FoliageBranchesTimeout, 4.f, VF_NULL,
	              "Maximum lifetime of branch ropes (if there are no collisions)");
	REGISTER_CVAR(e_FoliageWindActivationDist, 0, VF_NULL,
	              "If the wind is sufficiently strong, visible foliage in this view dist will be forcefully activated");

	REGISTER_CVAR(e_DeformableObjects, e_DeformableObjectsDefault, VF_NULL,
	                   "Enable / Disable morph based deformable objects");

	REGISTER_CVAR(e_CullVegActivation, 200, VF_NULL,
	              "Vegetation activation distance limit; 0 disables visibility-based culling (= unconditional activation)");

	REGISTER_CVAR(e_PhysOceanCell, e_PhysOceanCellDefault, VF_NULL,
	              "Cell size for ocean approximation in physics, 0 assumes flat plane");

	DefineConstFloatCVar(e_JointStrengthScale, VF_NULL,
	                     "Scales the strength of prebroken objects\' joints (for tweaking)");

	DefineConstFloatCVar(e_VolObjShadowStrength, VF_NULL,
	                     "Self shadow intensity of volume objects [0..1].");

	DefineConstIntCVar(e_ScreenShot, 0, VF_NULL,
	                   "Make screenshot combined up of multiple rendered frames\n"
	                   "(negative values for multiple frames, positive for a a single frame)\n"
	                   " 1 highres\n"
					   " 2 360 degree panorama (not supported)\n"
	                   " 3 Map top-down view\n"
	                   "\n"
	                   "see:\n"
	                   "  e_ScreenShotWidth, e_ScreenShotHeight, e_ScreenShotQuality, e_ScreenShotMapCenterX,\n"
	                   "  e_ScreenShotMapCenterY, e_ScreenShotMapSize, e_ScreenShotMinSlices, e_ScreenShotDebug");

	REGISTER_CVAR(e_ScreenShotWidth, 1920, VF_NULL,
	              "used for all type highres screenshots made by e_ScreenShot to define the\n"
	              "width of the destination image, 1920 default [1-4096]");
	REGISTER_CVAR(e_ScreenShotHeight, 1080, VF_NULL,
	              "used for all type highres screenshots made by e_ScreenShot to define the\n"
	              "height of the destination image, 1080 default [1-4096]");
	REGISTER_CVAR(e_ScreenShotQuality, 30, VF_NULL,
	              "used for all type highres screenshots made by e_ScreenShot to define the quality\n"
	              "0=fast, 10 .. 30 .. 100 = extra border in percent (soften seams), negative value to debug");
	REGISTER_CVAR(e_ScreenShotMinSlices, 1, VF_NULL,
	              "used for all type highres screenshots made by e_ScreenShot to define the amount\n"
	              "of sub-screenshots for the width and height to generate the image,\n the min count\n"
	              "will be automatically raised if not sufficient (per screenshot-based)");
	REGISTER_CVAR(e_ScreenShotMapCenterX, 0.0f, VF_NULL,
	              "param for the centerX position of the camera, see e_ScreenShotMap\n"
	              "defines the x position of the top left corner of the screenshot-area on the terrain,\n"
	              "0.0 - 1.0 (0.0 is default)");
	REGISTER_CVAR(e_ScreenShotMapCenterY, 0.0f, VF_NULL,
	              "param for the centerX position of the camera, see e_ScreenShotMap\n"
	              "defines the y position of the top left corner of the screenshot-area on the terrain,\n"
	              "0.0 - 1.0 (0.0 is default)");
	REGISTER_CVAR(e_ScreenShotMapSizeX, 1024.f, VF_NULL,
	              "param for the size in worldunits of area to make map screenshot, see e_ScreenShotMap\n"
	              "defines the x position of the bottom right corner of the screenshot-area on the terrain,\n"
	              "0.0 - 1.0 (1.0 is default)");
	REGISTER_CVAR(e_ScreenShotMapSizeY, 1024.f, VF_NULL,
	              "param for the size in worldunits of area to make map screenshot, see e_ScreenShotMap\n"
	              "defines the x position of the bottom right corner of the screenshot-area on the terrain,\n"
	              "0.0 - 1.0 (1.0 is default)");
	REGISTER_CVAR(e_ScreenShotMapResolution, 512, VF_NULL,
				  "used for mini map screenshots to define the\n"
				  "resolution of the destination image, 512 default [1-4096]");
	REGISTER_CVAR(e_ScreenShotMapCamHeight, 4000.f, VF_NULL,
	              "param for top-down-view screenshot creation, defining the camera height for screenshots,\n"
	              "see e_ScreenShotMap defines the y position of the bottom right corner of the\n"
	              "screenshot-area on the terrain,\n"
	              "0.0 - 1.0 (1.0 is default)");
	REGISTER_CVAR(e_ScreenShotMapOrientation, 0, VF_NULL,
	              "param for rotating the orientation through 90 degrees so the screen shot width is along the X axis\n"
	              "see e_ScreenShotMap\n"
	              "0 - 1 (0 is default)");
	REGISTER_CVAR(e_ScreenShotDebug, 0, VF_NULL,
	              "0 off\n1 show stitching borders\n2 show overlapping areas");

	DefineConstIntCVar(e_Ropes, 1, VF_CHEAT,
	                   "Turn Rendering of Ropes on/off");

	DefineConstIntCVar(e_StatObjValidate, e_StatObjValidateDefault, VF_NULL,
	                   "Enable CGF mesh validation during loading");

	DefineConstIntCVar(e_StatObjPreload, 1, VF_NULL,
	                   "Load level CGF's in efficient way");

	DefineConstIntCVar(e_PreloadMaterials, 1, VF_NULL,
	                   "Preload level materials from level cache pak and resources list");
	REGISTER_CVAR(e_PreloadDecals, 1, VF_NULL,
	                   "Preload all materials for decals");

	DefineConstIntCVar(e_StatObjMerge, 1, VF_NULL,
	                   "Enable CGF sub-objects meshes merging");
	DefineConstIntCVar(e_StatObjMergeMaxTrisPerDrawCall, 500, VF_NULL,
	                   "Skip merging of meshes already having acceptable number of triangles per draw call");
	DefineConstIntCVar(e_StatObjStoreMesh, 0, VF_NULL,
	                   "Store the mesh if enabled, used for cheat detection purposes (they will be stored by default on the dedi server)");

	DefineConstIntCVar(e_DefaultMaterial, 0, VF_CHEAT,
	                   "use gray illumination as default");

	REGISTER_CVAR(e_ObjQuality, 0, VF_NULL,
	              "Object detail quality");
	REGISTER_CVAR(e_StaticInstancing, 0, VF_NULL,
	              "Prepare and cache instancing info (mostly for vegetation)\n"
	              "2 = Show only instanced objects\n"
	              "3 = Show only instanced objects and bounding boxes\n"
	              "4 = Show only non instanced objects\n");
	REGISTER_CVAR(e_StaticInstancingMinInstNum, 32, VF_NULL,
	              "Minimum number of objects for grouping");
	REGISTER_CVAR(e_ParticlesQuality, 0, VF_NULL,
	              "Particles detail quality");
	REGISTER_CVAR(e_ObjShadowCastSpec, 0, VF_NULL,
	              "Object shadow casting spec. Only objects with Shadow Cast Spec <= e_ObjShadowCastSpec will cast shadows");

	REGISTER_CVAR(e_ParticlesPoolSize, 16 << 10, VF_CHEAT | VF_CHEAT_NOCHECK,
	              "Particle system pool memory size in KB");

	REGISTER_CVAR(e_ParticlesLights, 1, VF_NULL,
	                   "Allows to have light source attached to every particle\n"
	                   "0 = Off\n"
	                   "1 = Deferred lights\n");

	REGISTER_CVAR(e_ParticlesLightsViewDistRatio, e_ParticlesLightsViewDistRatioDefault, VF_NULL,
	                     "Set particles lights view distance ratio");

	REGISTER_CVAR(e_LightVolumes, e_LightVolumesDefault, VF_NULL,
	                   "Allows deferred lighting for registered alpha blended geometry\n"
	                   "0 = Off\n"
	                   "1 = Enabled\n"
	                   "2 = Enabled just for sun light\n");

	DefineConstIntCVar(e_LightVolumesDebug, 0, VF_NULL,
	                   "Display light volumes debug info\n"
	                   "0 = Off\n"
	                   "1 = Enabled\n");

	REGISTER_CVAR(e_ParticlesUseLevelSpecificLibs, 0, VF_NULL,
	              "Allows searching for level-specific version of effects files\n"
	              "0 = Off\n"
	              "1 = Enabled\n");

	REGISTER_CVAR(e_ParticlesAnimBlend, 1, VF_NULL,
	              "Blend between animated texture frames\n"
	              "Usage: e_ParticlesAnimBlend [0/1/2]\n"
	              "0 = Off\n"
	              "1 = On\n"
	              "2 = Force");

	REGISTER_CVAR(e_ParticlesGI, 1, VF_NULL,
	              "Apply global illumination to appropriate particle effects\n"
	              "Usage: e_ParticlesGI [0/1/2]\n"
	              "0 = Off\n"
	              "1 = On\n"
	              "2 = Force");

	REGISTER_CVAR(e_ParticlesSoftIntersect, 1, VF_NULL,
	              "Render appropriate particles with soft intersection\n"
	              "Usage: e_ParticlesSoftIntersect [0/1/2]\n"
	              "0 = Off\n"
	              "1 = On\n"
	              "2 = Force");

	REGISTER_CVAR(e_ParticlesMotionBlur, 0, VF_INVISIBLE,
	              "Motion blur for particles\n"
	              "Usage: e_ParticlesMotionBlur [0/1/2]\n"
	              "0 = Off\n"
	              "1 = On\n"
	              "2 = Force");

	REGISTER_CVAR(e_ParticlesShadows, 1, VF_NULL,
	              "Shadows on particles\n"
	              "Usage: e_ParticlesShadows [0/1/2]\n"
	              "0 = Off\n"
	              "1 = On\n"
	              "2 = Force");

	REGISTER_CVAR(e_ParticlesAudio, 1, VF_CHEAT,
	              "Toggles audio on particles on or off.\n"
	              "Usage: e_ParticlesAudio [0/1]\n"
	              "0 = Off\n"
	              "1 = On (Default)\n");

	REGISTER_CVAR(e_ParticleShadowsNumGSMs, 3, VF_NULL,
	              "Number of shadow GSMs used for particle shadows");

	e_ScreenShotFileFormat = REGISTER_STRING("e_ScreenShotFileFormat", "tga", VF_NULL,
	                                         "Set output image file format for hires screen shots. Can be JPG or TGA");
	e_SQTestTextureName = REGISTER_STRING("e_SQTestTextureName", "strfrn_advrt_boards_screen", VF_NULL,
	                                      "Reference texture name for streaming latency test");
	e_StreamCgfDebugFilter = REGISTER_STRING("e_StreamCgfDebugFilter", "", VF_NULL,
	                                         "Show only items containing specified text");

	e_CameraGoto = REGISTER_STRING("e_CameraGoto", "0", VF_CHEAT,
	                               "Move cameras to a certain pos/angle");
	e_DebugDrawFilter = REGISTER_STRING("e_DebugDrawFilter", "", VF_NULL,
	                                    "Show a specified text on DebugDraw");

	REGISTER_CVAR_CB(e_TimeOfDay, 0.0f, VF_CHEAT | VF_CHEAT_NOCHECK, "Current Time of Day", OnTimeOfDayVarChange);
	REGISTER_CVAR_CB(e_TimeOfDaySpeed, 0.0f, VF_CHEAT | VF_CHEAT_NOCHECK, "Time of Day change speed", OnTimeOfDaySpeedVarChange);
	DefineConstIntCVar(e_TimeOfDayDebug, 0, VF_NULL,
	                   "Display time of day current values on screen");

	DefineConstFloatCVar(e_CameraRotationSpeed, VF_CHEAT,
	                     "Rotate camera around Z axis for debugging");
	DefineConstIntCVar(e_CameraFreeze, 0, VF_CHEAT,
	                   "Freeze 3dengine camera (good to debug object culling and LOD).\n"
	                   "The view frustum is drawn in write frame.\n"
	                   " 0 = off\n"
	                   " 1 = activated");

	REGISTER_CVAR(e_GI, 0, VF_CVARGRP_IGNOREINREALVAL,
	              "Enable/disable global illumination. Default: 1 - enabled");

	REGISTER_CVAR(e_RenderMeshCollisionTolerance, 0.3f, VF_NULL,
	              "Min distance between physics-proxy and rendermesh before collision is considered a hole");

	DefineConstIntCVar(e_PrepareDeformableObjectsAtLoadTime, 0, VF_CHEAT,
	                   "Enable to Prepare deformable objects at load time instead on demand, prevents peaks but increases memory usage");

	DefineConstIntCVar(e_DeferredPhysicsEvents, 1, VF_CHEAT,
	                   "Enable to Perform some physics events deferred as a task/job");

	REGISTER_CVAR(e_levelStartupFrameNum, 0, VF_NULL,
	              "Set to number of frames to capture for avg fps computation");

	REGISTER_CVAR(e_levelStartupFrameDelay, 0, VF_NULL,
	              "Set to number of frames to wait after level load before beginning fps measuring");

	REGISTER_CVAR(e_CacheNearestCubePicking, 1, VF_NULL,
	              "Enable caching nearest cube maps probe picking for alpha blended geometry");

	REGISTER_CVAR(e_CGFMaxFileSize, -1, VF_CHEAT,
	              "will refuse to load any cgf larger than the given filesize (in kb)\n"
	              "-1 - 1024 (<0 off (default), >0 filesize limit)");

	REGISTER_CVAR(e_MaxDrawCalls, 0, VF_CHEAT,
	              "Will not render CGFs past the given amount of drawcalls\n"
	              "(<=0 off (default), >0 draw calls limit)");

	REGISTER_CVAR(e_ClipVolumes, 1, VF_CHEAT,
	              "Enable/Disable light clip volumes. Default: 1 - Enabled");
	
	int defaultMergedMeshesValue = 1;
#if defined(DEDICATED_SERVER)
	defaultMergedMeshesValue = 0;
#endif
	REGISTER_CVAR(e_MergedMeshes, defaultMergedMeshesValue, VF_NULL, "Show runtime merged meshes");
	REGISTER_CVAR(e_MergedMeshesDebug, 0, VF_NULL,
	              "enable debug drawing of runtime merged meshes\n"
	              "2: Show AABB + debug info (position, state, size, visibility)\n"
	              "64: Show the calculated wind\n"
	              "256: Draw colliders of objects influencing the merged meshes\n"
	              "544: Draw spines\n"
	              "1056: Draw simulated spines\n"
	              "2080: Draw spines with LOD info (red/blue)\n");
	REGISTER_CVAR(e_MergedMeshesPool, 2750, VF_NULL, "amount of mainmeory (in kb) that merged meshes are allowed to sustain");
	REGISTER_CVAR(e_MergedMeshesPoolSpines, 32, VF_NULL, "percentage of the pool for spines");
	REGISTER_CVAR(e_MergedMeshesTesselationSupport, 0, VF_NULL, "Enable or disable support for tesselation on mergedmeshes");
	REGISTER_CVAR(e_MergedMeshesViewDistRatio, 30.f, VF_NULL, "merged meshes view dist ratio");
	REGISTER_CVAR(e_MergedMeshesLodRatio, 3.f, VF_NULL, "merged meshes lod ratio");
	REGISTER_CVAR(e_MergedMeshesDeformViewDistMod, 0.45f, VF_NULL, "distance modifier applied to view dist ratios after which deformables stop updating");
	REGISTER_CVAR(e_MergedMeshesInstanceDist, 4.5f, VF_NULL, "Distance fudge factor at which merged meshes turn off animation");
	REGISTER_CVAR(e_MergedMeshesActiveDist, 250.f, VF_NULL, "Active distance up until merged mesh patches will be streamed in");
	REGISTER_CVAR(e_MergedMeshesUseSpines, 1, VF_NULL, "MergedMeshes use touchbending");
	REGISTER_CVAR(e_MergedMeshesBulletSpeedFactor, 0.05f, VF_NULL, "MergedMesh Bullet approximations speed factor");
	REGISTER_CVAR(e_MergedMeshesBulletScale, 35.f, VF_NULL, "MergedMesh Bullet approximations size scale");
	REGISTER_CVAR(e_MergedMeshesBulletLifetime, 0.15f, VF_NULL, "MergedMesh Bullet approximations lifetime");
	REGISTER_CVAR(e_MergedMeshesOutdoorOnly, 0, VF_NULL, "MergedMeshes will recieve ERF_OUTDOORONLY by default");
	REGISTER_CVAR(e_MergedMeshesMaxTriangles, 600, VF_NULL, "Do not merge meshes containing too many triangles. It's more efficient to render them without merging");
	REGISTER_CVAR(e_CheckOctreeObjectsBoxSize, 1, VF_NULL, "CryWarning for crazy sized COctreeNode m_objectsBoxes");
	REGISTER_CVAR(e_DebugGeomPrep, 0, VF_NULL, "enable logging of Geom preparation");
	DefineConstIntCVar(e_GeomCaches, 1, VF_NULL, "Activates drawing of geometry caches");
	REGISTER_CVAR(e_GeomCacheBufferSize, 128, VF_CHEAT, "Geometry cache stream buffer upper limit size in MB. Default: 128");
	REGISTER_CVAR(e_GeomCacheMaxPlaybackFromMemorySize, 16, VF_CHEAT,
	              "Maximum size of geometry cache animated data in MB before always streaming from disk ignoring the memory playback flag. Default: 16");
	REGISTER_CVAR(e_GeomCachePreferredDiskRequestSize, 1024, VF_CHEAT,
	              "Preferred disk request size for geometry cache streaming in KB. Default: 1024");
	REGISTER_CVAR(e_GeomCacheMinBufferAheadTime, 2.0f, VF_CHEAT,
	              "Time in seconds minimum that data will be buffered ahead for geom cache streaming. Default: 2.0");
	REGISTER_CVAR(e_GeomCacheMaxBufferAheadTime, 5.0f, VF_CHEAT,
	              "Time in seconds maximum that data will be buffered ahead for geom cache streaming. Default: 5.0");
	REGISTER_CVAR(e_GeomCacheDecodeAheadTime, 0.5f, VF_CHEAT,
	              "Time in seconds that data will be decoded ahead for geom cache streaming. Default: 0.5");
#ifndef _RELEASE
	DefineConstIntCVar(e_GeomCacheDebug, 0, VF_CHEAT, "Show geometry cache debug overlay. Default: 0");
	e_GeomCacheDebugFilter = REGISTER_STRING("e_GeomCacheDebugFilter", "", VF_CHEAT, "Set name filter for e_geomCacheDebug");
	DefineConstIntCVar(e_GeomCacheDebugDrawMode, 0, VF_CHEAT, "Geometry cache debug draw mode\n"
	                                                          " 0 = normal\n"
	                                                          " 1 = only animated meshes\n"
	                                                          " 2 = only static meshes\n"
	                                                          " 3 = debug instancing");
	REGISTER_CVAR_DEV_ONLY(e_MergedMeshesClusterVisualization, 0, VF_NULL,
	                       "Activates drawing of clusters calculated around merged mesh type vegetation.\n"
	                       "0 = off\n"
	                       "1 = runtime areas only (use Y key for temporary drawing of clusters)\n"
	                       "2 = runtime areas + clusters around vegetation patches");
	REGISTER_CVAR_DEV_ONLY(e_MergedMeshesClusterVisualizationDimension, 32, VF_NULL,
	                       "Area around the camera in which the clusters should be visible. (high number == slow!\n"
	                       "Note: max value is 255");
#endif
	DefineConstIntCVar(e_GeomCacheLerpBetweenFrames, 1, VF_CHEAT, "Interpolate between geometry cache frames. Default: 1");

#if defined(FEATURE_SVO_GI)
	if (!gEnv->IsDedicated())
	{
		RegisterTICVars();
	}
#endif
}
