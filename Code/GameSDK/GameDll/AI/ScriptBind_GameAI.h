// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef ScriptBind_GameAI_h
#define ScriptBind_GameAI_h

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>
#include "RangeModule.h" // For RangeID

struct IGameFramework;
struct ISystem;

class CScriptBind_GameAI : public CScriptableBase
{
public:
	CScriptBind_GameAI(ISystem* system, IGameFramework* gameFramework);

	virtual void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

private:
	void RegisterMethods();
	void RegisterGlobals();

	int RegisterWithModule(IFunctionHandler* pH, const char* moduleName, ScriptHandle entityID);
	int UnregisterWithModule(IFunctionHandler* pH, const char* moduleName, ScriptHandle entityID);
	int UnregisterWithAllModules(IFunctionHandler* pH, ScriptHandle entityID);
	int PauseModule(IFunctionHandler* pH, const char* moduleName, ScriptHandle entityID);
	int PauseAllModules(IFunctionHandler* pH, ScriptHandle entityID);
	int ResumeModule(IFunctionHandler* pH, const char* moduleName, ScriptHandle entityID);
	int ResumeAllModules(IFunctionHandler* pH, ScriptHandle entityID);
	bool GetEntitiesInRange(const Vec3& center, float radius, const char* pClassName, SEntityProximityQuery* pQuery) const;
	int GetClosestEntityToTarget(IFunctionHandler* funcHandler, Vec3 attackerPos, Vec3 targetPos, const char* pClassName, float radius, float maxAngle);
	int GetBattleFrontPosition(IFunctionHandler* pH, int groupID);

	int ResetAdvantagePointOccupancyControl(IFunctionHandler* pH);
	int OccupyAdvantagePoint(IFunctionHandler* pH, ScriptHandle entityID, Vec3 point);
	int ReleaseAdvantagePointFor(IFunctionHandler* pH, ScriptHandle entityID);
	int IsAdvantagePointOccupied(IFunctionHandler* pH, Vec3 point);

	int StartSearchModuleFor(IFunctionHandler* pH, int groupID, Vec3 targetPos);
	int StopSearchModuleFor(IFunctionHandler* pH, int groupID);
	int GetNextSearchSpot(IFunctionHandler* pH, ScriptHandle entityID, float closenessToAgentWeight, float closenessToTargetWeight, float minDistanceFromAgent);
	int MarkAssignedSearchSpotAsUnreachable(IFunctionHandler* pH, ScriptHandle entityID);

	int ResetRanges(IFunctionHandler* pH, ScriptHandle entityID);
	int AddRange(IFunctionHandler* pH, ScriptHandle entityID, float range, const char* enterSignal, const char* leaveSignal);
	int GetRangeState(IFunctionHandler* pH, ScriptHandle entityID, int rangeID);
	int ChangeRange(IFunctionHandler* pH, ScriptHandle entityID, int rangeID, float distance);

	int ResetAloneDetector(IFunctionHandler* pH, ScriptHandle entityID);
	int SetupAloneDetector(IFunctionHandler* pH, ScriptHandle entityID, float range, const char* aloneSignal, const char* notAloneSignal);
	int AddActorClassToAloneDetector(IFunctionHandler* pH, ScriptHandle entityID, const char* entityClassName);
	int RemoveActorClassFromAloneDetector(IFunctionHandler* pH, ScriptHandle entityID, const char* entityClassName);
	int IsAloneForAloneDetector(IFunctionHandler* pH, ScriptHandle entityID);

	int RegisterObjectVisible(IFunctionHandler *pH, ScriptHandle entityID);
	int UnregisterObjectVisible(IFunctionHandler *pH, ScriptHandle entityID);

	int IsAISystemEnabled(IFunctionHandler *pH);

	int RegisterEntityForAISquadManager(IFunctionHandler* pH, ScriptHandle entityId);
	int RemoveEntityForAISquadManager(IFunctionHandler* pH, ScriptHandle entityId);
	int GetSquadId(IFunctionHandler* pH, ScriptHandle entityId);
	int GetSquadMembers(IFunctionHandler* pH, int squadId);
	int GetAveragePositionOfSquadScopeUsers(IFunctionHandler* pH, ScriptHandle entityId, const char* squadScopeName);
	int GetSquadScopeUserCount(IFunctionHandler* pH, ScriptHandle entityId, const char* squadScopeName);

	int IsSwimmingUnderwater(IFunctionHandler* pH, ScriptHandle entityID);

	ISystem* m_system;
	IGameFramework* m_gameFramework;
	IScriptSystem* m_scriptSystem;
};

#endif // ScriptBind_GameAI_h
