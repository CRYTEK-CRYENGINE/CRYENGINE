// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   EntityCVars.h
//  Version:     v1.00
//  Created:     18/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __EntityCVars_h__
#define __EntityCVars_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
// Console variables local to EntitySystem.
//////////////////////////////////////////////////////////////////////////
struct CVar
{
	static ICVar* pDebug;
	static ICVar* pCharacterIK;
	static ICVar* pProfileEntities;
	//	static ICVar *pUpdateInvisibleCharacter;
	//	static ICVar *pUpdateBonePositions;
	static ICVar*   pUpdateScript;
	static ICVar*   pUpdateTimer;
	static ICVar*   pUpdateCamera;
	static ICVar*   pUpdatePhysics;
	static ICVar*   pUpdateAI;
	static ICVar*   pUpdateEntities;
	static ICVar*   pUpdateCollision;
	static ICVar*   pUpdateCollisionScript;
	static ICVar*   pUpdateContainer;
	static ICVar*   pUpdateCoocooEgg;
	static ICVar*   pPiercingCamera;
	static ICVar*   pVisCheckForUpdate;
	static ICVar*   pEntityBBoxes;
	static ICVar*   pEntityHelpers;
	static ICVar*   pMinImpulseVel;
	static ICVar*   pImpulseScale;
	static ICVar*   pMaxImpulseAdjMass;
	static ICVar*   pDebrisLifetimeScale;
	static ICVar*   pSplashThreshold;
	static ICVar*   pSplashTimeout;
	static ICVar*   pHitCharacters;
	static ICVar*   pHitDeadBodies;
	static ICVar*   pCharZOffsetSpeed;
	static ICVar*   pLogCollisions;
	static ICVar*   pNotSeenTimeout;
	static ICVar*   pDebugNotSeenTimeout;
	static ICVar*   pDrawAreas;
	static ICVar*   pDrawAreaGrid;
	static ICVar*   pDrawAreaDebug;
	static ICVar*   pDrawAudioProxyZRay;
	static ICVar*   pMotionBlur;
	static ICVar*   pSysSpecLight;

	static Matrix34 audioListenerOffset;

	static int      es_UsePhysVisibilityChecks;
	static float    es_MaxPhysDist;
	static float    es_MaxPhysDistInvisible;
	static float    es_MaxPhysDistCloth;
	static float    es_FarPhysTimeout;
	static int      es_SortUpdatesByClass;

	// debug only
	static ICVar*      pEnableFullScriptSave;

	static int         es_DebugTimers;
	static int         es_DebugFindEntity;
	static int         es_DebugEvents;
	static int         es_debugEntityLifetime;
	static int         es_DisableTriggers;
	static int         es_DrawProximityTriggers;

	static int         es_DebugEntityUsage;
	static const char* es_DebugEntityUsageFilter;

	// Entity pool usage
	static int    es_LayerSaveLoadSerialization;
	static int    es_SaveLoadUseLUANoSaveFlag;
	static int    es_LayerDebugInfo;

	static ICVar* pUpdateType;

	static float  es_EntityUpdatePosDelta;

	static int    es_debugDrawEntityIDs;

	static int    es_MaxJointFx;

	// Initialize console variables.
	static void Init(struct IConsole* pConsole);

	// Dump Entities
	static void DumpEntities(IConsoleCmdArgs*);
	static void DumpEntityClassesInUse(IConsoleCmdArgs*);

	// Recompile area grid
	static void CompileAreaGrid(IConsoleCmdArgs*);

	static void EnableDebugAnimText(IConsoleCmdArgs* args);

	static void SetAudioListenerOffsets(IConsoleCmdArgs* pArgs);
};

#endif // __EntityCVars_h__
