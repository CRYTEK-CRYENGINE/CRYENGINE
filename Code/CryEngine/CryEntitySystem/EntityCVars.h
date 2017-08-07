// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// Console variables local to EntitySystem.
//////////////////////////////////////////////////////////////////////////
struct SEntityWithCharacterInstanceAutoComplete : public IConsoleArgumentAutoComplete
{
	SEntityWithCharacterInstanceAutoComplete();

	virtual int         GetCount() const override;
	virtual const char* GetValue(int index) const override;
};

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
	static ICVar*   pDrawAreaGridCells;
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
	static int         es_DebugEntityUsageSortMode;

	// Entity pool usage
	static int    es_LayerSaveLoadSerialization;
	static int    es_SaveLoadUseLUANoSaveFlag;
	static int    es_LayerDebugInfo;

	static float  es_EntityUpdatePosDelta;

	static int    es_debugDrawEntityIDs;

	static int    es_MaxJointFx;

	// Initialize console variables.
	static void Init();

	// Dump Entities
	static void DumpEntities(IConsoleCmdArgs*);
	static void DumpEntityClassesInUse(IConsoleCmdArgs*);

	// Recompile area grid
	static void CompileAreaGrid(IConsoleCmdArgs*);

	static void EnableDebugAnimText(IConsoleCmdArgs* args);
	static void SetDebugAnimText(IEntity* entity, const bool enable);

	static void SetAudioListenerOffsets(IConsoleCmdArgs* pArgs);

	// Console commands to enable/disable layers
	static void ConsoleCommandToggleLayer(IConsoleCmdArgs* pArgs);
};
