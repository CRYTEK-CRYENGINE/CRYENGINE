// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScriptBind_GameAI.h"
#include "GameAIEnv.h"
#include "GameRules.h"
#include "SearchModule.h"
#include "AIBattleFront.h"
#include "Assignment.h"
#include "AloneDetectorModule.h"
#include <ICryMannequin.h>
#include "IAnimatedCharacter.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "Actor.h"


CScriptBind_GameAI::CScriptBind_GameAI(ISystem* system, IGameFramework* gameFramework)
: m_system(system)
, m_gameFramework(gameFramework)
, m_scriptSystem(system->GetIScriptSystem())
{
	Init(m_scriptSystem, m_system);
	SetGlobalName("GameAI");

	RegisterMethods();
	RegisterGlobals();
}

void CScriptBind_GameAI::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_GameAI::

	SCRIPT_REG_TEMPLFUNC(RegisterWithModule, "moduleName, entityId");
	SCRIPT_REG_TEMPLFUNC(UnregisterWithModule, "moduleName, entityId");
	SCRIPT_REG_TEMPLFUNC(UnregisterWithAllModules, "entityId");
	SCRIPT_REG_TEMPLFUNC(PauseModule, "moduleName, entityId");
	SCRIPT_REG_TEMPLFUNC(PauseAllModules, "entityId");
	SCRIPT_REG_TEMPLFUNC(ResumeModule, "moduleName, entityId");
	SCRIPT_REG_TEMPLFUNC(ResumeAllModules, "entityId");
	SCRIPT_REG_TEMPLFUNC(GetClosestEntityToTarget, "attackerPos, targetPos, radius, maxAngle");
	SCRIPT_REG_TEMPLFUNC(GetBattleFrontPosition, "groupID");

 	SCRIPT_REG_FUNC(ResetAdvantagePointOccupancyControl);
 	SCRIPT_REG_TEMPLFUNC(OccupyAdvantagePoint, "entityId, point");
 	SCRIPT_REG_TEMPLFUNC(ReleaseAdvantagePointFor, "entityId");
	SCRIPT_REG_TEMPLFUNC(IsAdvantagePointOccupied, "point");

	SCRIPT_REG_TEMPLFUNC(StartSearchModuleFor, "groupID, point, [targetId], [searchSpotTimeout]");
	SCRIPT_REG_TEMPLFUNC(StopSearchModuleFor, "groupID");
	SCRIPT_REG_TEMPLFUNC(GetNextSearchSpot, "entityId, closenessToAgentWeight, closenessToTargetWeight, minDistanceFromAgent, [closenessToTargetCurrentPosWeight]");
	SCRIPT_REG_TEMPLFUNC(MarkAssignedSearchSpotAsUnreachable, "entityId");

	SCRIPT_REG_TEMPLFUNC(ResetRanges, "entityID");
	SCRIPT_REG_TEMPLFUNC(AddRange, "entityID, range, enterSignal, leaveSignal");
	SCRIPT_REG_TEMPLFUNC(GetRangeState, "entityID, rangeID");
	SCRIPT_REG_TEMPLFUNC(ChangeRange, "entityID, rangeID, distance");

	SCRIPT_REG_TEMPLFUNC(ResetAloneDetector, "entityID");
	SCRIPT_REG_TEMPLFUNC(SetupAloneDetector, "entityID, range, aloneSignal, notAloneSignal");
	SCRIPT_REG_TEMPLFUNC(AddActorClassToAloneDetector, "entityID, entityClassName");
	SCRIPT_REG_TEMPLFUNC(RemoveActorClassFromAloneDetector, "entityID, entityClassName");
	SCRIPT_REG_TEMPLFUNC(IsAloneForAloneDetector, "entityID");

	SCRIPT_REG_TEMPLFUNC(RegisterObjectVisible, "entityID");
	SCRIPT_REG_TEMPLFUNC(UnregisterObjectVisible, "entityID");

	SCRIPT_REG_FUNC(IsAISystemEnabled);

	SCRIPT_REG_TEMPLFUNC(RegisterEntityForAISquadManager, "entityID");
	SCRIPT_REG_TEMPLFUNC(RemoveEntityForAISquadManager, "entityID");

	SCRIPT_REG_TEMPLFUNC(GetSquadId, "entityId");
	SCRIPT_REG_TEMPLFUNC(GetSquadMembers, "squadId");
	SCRIPT_REG_TEMPLFUNC(GetAveragePositionOfSquadScopeUsers, "entityId, squadScopeName");
	SCRIPT_REG_TEMPLFUNC(GetSquadScopeUserCount, "entityId, squadScopeName");

	SCRIPT_REG_TEMPLFUNC(IsSwimmingUnderwater, "entityID");

#undef SCRIPT_REG_CLASSNAME
}

void CScriptBind_GameAI::RegisterGlobals()
{
	m_scriptSystem->SetGlobalValue("InsideRange", RangeContainer::Range::Inside);
	m_scriptSystem->SetGlobalValue("OutsideRange", RangeContainer::Range::Outside);
	m_scriptSystem->SetGlobalValue("UseAttentionTargetDistance", RangeContainer::Range::UseAttentionTargetDistance);
	m_scriptSystem->SetGlobalValue("UseLiveTargetDistance", RangeContainer::Range::UseLiveTargetDistance);

	m_scriptSystem->SetGlobalValue("VOR_Always", eVOR_Always);
	m_scriptSystem->SetGlobalValue("VOR_UseMaxViewDist", eVOR_UseMaxViewDist);
	m_scriptSystem->SetGlobalValue("VOR_OnlyWhenMoving", eVOR_OnlyWhenMoving);
	m_scriptSystem->SetGlobalValue("VOR_FlagNotifyOnSeen", eVOR_FlagNotifyOnSeen);
	m_scriptSystem->SetGlobalValue("VOR_FlagDropOnceInvisible", eVOR_FlagDropOnceInvisible);

	m_scriptSystem->SetGlobalValue("Assignment_NoAssignment", Assignment::NoAssignment);
	m_scriptSystem->SetGlobalValue("Assignment_DefendArea", Assignment::DefendArea);
	m_scriptSystem->SetGlobalValue("Assignment_HoldPosition", Assignment::HoldPosition);
	m_scriptSystem->SetGlobalValue("Assignment_CombatMove", Assignment::CombatMove);
	m_scriptSystem->SetGlobalValue("Assignment_ScanSpot", Assignment::ScanSpot);
	m_scriptSystem->SetGlobalValue("Assignment_ScorchSpot", Assignment::ScorchSpot);
	m_scriptSystem->SetGlobalValue("Assignment_PsychoCombatAllowed", Assignment::PsychoCombatAllowed);
}

int CScriptBind_GameAI::RegisterWithModule(IFunctionHandler* pH, const char* moduleName, ScriptHandle entityID)
{
	g_pGame->GetGameAISystem()->EnterModule((EntityId)entityID.n, moduleName);
	return pH->EndFunction();
}

int CScriptBind_GameAI::UnregisterWithModule(IFunctionHandler* pH, const char* moduleName, ScriptHandle entityID)
{
	g_pGame->GetGameAISystem()->LeaveModule((EntityId)entityID.n, moduleName);
	return pH->EndFunction();
}

int CScriptBind_GameAI::UnregisterWithAllModules(IFunctionHandler* pH, ScriptHandle entityID)
{
	g_pGame->GetGameAISystem()->LeaveAllModules((EntityId)entityID.n);
	return pH->EndFunction();
}

int CScriptBind_GameAI::PauseModule(IFunctionHandler* pH, const char* moduleName, ScriptHandle entityID)
{
	g_pGame->GetGameAISystem()->PauseModule((EntityId)entityID.n, moduleName);
	return pH->EndFunction();
}

int CScriptBind_GameAI::PauseAllModules(IFunctionHandler* pH, ScriptHandle entityID)
{
	g_pGame->GetGameAISystem()->PauseAllModules((EntityId)entityID.n);
	return pH->EndFunction();
}

int CScriptBind_GameAI::ResumeModule(IFunctionHandler* pH, const char* moduleName, ScriptHandle entityID)
{
	g_pGame->GetGameAISystem()->ResumeModule((EntityId)entityID.n, moduleName);
	return pH->EndFunction();
}


int CScriptBind_GameAI::ResumeAllModules(IFunctionHandler* pH, ScriptHandle entityID)
{
	g_pGame->GetGameAISystem()->ResumeAllModules(static_cast<EntityId>(entityID.n));
	return pH->EndFunction();
}

bool CScriptBind_GameAI::GetEntitiesInRange(const Vec3& center, float radius, const char* pClassName, SEntityProximityQuery* pQuery) const
{
	CRY_ASSERT(pQuery);

	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	IEntityClass* pClass = pEntitySystem->GetClassRegistry()->FindClass(pClassName);
	if (!pClass)
		return false;

	pQuery->box.min = center - Vec3(radius, radius, radius);
	pQuery->box.max = center + Vec3(radius, radius, radius);
	pQuery->pEntityClass = pClass;
	pEntitySystem->QueryProximity(*pQuery);

	return true;
}

int CScriptBind_GameAI::GetClosestEntityToTarget(IFunctionHandler* funcHandler, Vec3 attackerPos, Vec3 targetPos, const char* pClassName, float radius, float maxAngle)
{
	SEntityProximityQuery query;

	if (GetEntitiesInRange(targetPos, radius, pClassName, &query))
	{
		int closestIndex = -1;
		float closestDistSq = FLT_MAX;
		float cosMaxAngle = cosf(maxAngle);
		Vec3 attackerToTargetDir = (targetPos - attackerPos).GetNormalized();

		for (int i = 0; i < query.nCount; ++i)
		{
			IEntity* pEntity = query.pEntities[i];
			if (pEntity)
			{
				float targetToEntityDistSq = (pEntity->GetWorldPos() - targetPos).GetLengthSquared();
				if (targetToEntityDistSq < closestDistSq)
				{
					// Closer than the current, but is the angle valid?
					Vec3 attackerToEntityDir = (pEntity->GetWorldPos() - attackerPos).GetNormalized();
					if (attackerToTargetDir.Dot(attackerToEntityDir) > cosMaxAngle)
					{
						closestIndex = i;
						closestDistSq = targetToEntityDistSq;
					}
				}
			}
		}

		if (closestIndex != -1)
		{
			return funcHandler->EndFunction(query.pEntities[closestIndex]->GetScriptTable());
		}
	}

	return funcHandler->EndFunction();
}

int CScriptBind_GameAI::GetBattleFrontPosition(IFunctionHandler* pH, int groupID)
{
	if(CAIBattleFrontModule* battleFrontModule = static_cast<CAIBattleFrontModule*>(g_pGame->GetGameAISystem()->FindModule("BattleFront")))
		if(CAIBattleFrontGroup* battleFrontGroup = battleFrontModule->GetGroupByID(groupID))
			return pH->EndFunction(battleFrontGroup->GetBattleFrontPosition());

	return pH->EndFunction();
}

int CScriptBind_GameAI::ResetAdvantagePointOccupancyControl(IFunctionHandler* pH)
{
	g_pGame->GetGameAISystem()->GetAdvantagePointOccupancyControl().Reset();
	return pH->EndFunction();
}

int CScriptBind_GameAI::OccupyAdvantagePoint(IFunctionHandler* pH, ScriptHandle entityID, Vec3 point)
{
	g_pGame->GetGameAISystem()->GetAdvantagePointOccupancyControl().OccupyAdvantagePoint(static_cast<EntityId>(entityID.n), point);
	return pH->EndFunction();
}

int CScriptBind_GameAI::ReleaseAdvantagePointFor(IFunctionHandler* pH, ScriptHandle entityID)
{
	g_pGame->GetGameAISystem()->GetAdvantagePointOccupancyControl().ReleaseAdvantagePoint(static_cast<EntityId>(entityID.n));
	return pH->EndFunction();
}

int CScriptBind_GameAI::IsAdvantagePointOccupied(IFunctionHandler* pH, Vec3 point)
{
	g_pGame->GetGameAISystem()->GetAdvantagePointOccupancyControl().IsAdvantagePointOccupied(point);
	return pH->EndFunction();
}

int CScriptBind_GameAI::StartSearchModuleFor(IFunctionHandler* pH, int groupID, Vec3 targetPos)
{
	ScriptHandle target(0);
	if (pH->GetParamType(3) == svtPointer)
		pH->GetParam(3, target);
	EntityId targetID = static_cast<EntityId>(target.n);

	float searchSpotTimeout(0);
	if (pH->GetParamType(4) == svtNumber)
		pH->GetParam(4, searchSpotTimeout);

	if(!gGameAIEnv.searchModule->GroupExist(groupID))
		gGameAIEnv.searchModule->GroupEnter(groupID, targetPos, targetID, searchSpotTimeout);

	return pH->EndFunction();
}

int CScriptBind_GameAI::StopSearchModuleFor(IFunctionHandler* pH, int groupID)
{
	if(gGameAIEnv.searchModule->GroupExist(groupID))
		gGameAIEnv.searchModule->GroupLeave(groupID);

	return pH->EndFunction();
}

int CScriptBind_GameAI::GetNextSearchSpot(IFunctionHandler* pH,
	ScriptHandle entityID,
	float closenessToAgentWeight,
	float closenessToTargetWeight,
	float minDistanceFromAgent
	)
{
	SearchSpotQuery query;
	query.closenessToAgentWeight = closenessToAgentWeight;
	query.closenessToTargetWeight = closenessToTargetWeight;
	query.minDistanceFromAgent = minDistanceFromAgent;

	if (pH->GetParamType(5) == svtNumber)
		pH->GetParam(5, query.closenessToTargetCurrentPosWeight);
	else
		query.closenessToTargetCurrentPosWeight = 0.0f;

	if (gGameAIEnv.searchModule->GetNextSearchPoint((EntityId)entityID.n, &query))
	{
		return pH->EndFunction(Script::SetCachedVector(query.result, pH, 1));
	}

	return pH->EndFunction();
}

int CScriptBind_GameAI::MarkAssignedSearchSpotAsUnreachable(IFunctionHandler* pH, ScriptHandle entityID)
{
	gGameAIEnv.searchModule->MarkAssignedSearchSpotAsUnreachable((EntityId)entityID.n);
	return pH->EndFunction();
}

int CScriptBind_GameAI::ResetRanges(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (RangeContainer* rangeContainer = gGameAIEnv.rangeModule->GetRunningInstance((EntityId)entityID.n))
		rangeContainer->ResetRanges();

	return pH->EndFunction();
}

int CScriptBind_GameAI::AddRange(IFunctionHandler* pH, ScriptHandle entityID, float range, const char* enterSignal, const char* leaveSignal)
{
	if (RangeContainer* rangeContainer = gGameAIEnv.rangeModule->GetRunningInstance((EntityId)entityID.n))
	{
		RangeContainer::Range r;
		r.enterSignal = enterSignal;
		r.leaveSignal = leaveSignal;
		r.rangeSq = square(range);

		if (pH->GetParamCount() >= 5)
		{
			int targetMode = 0;
			if (pH->GetParam(5, targetMode))
			{
				r.targetMode = static_cast<RangeContainer::Range::TargetMode>(targetMode);
			}
		}

		RangeContainer::RangeID rangeID = rangeContainer->AddRange(r);
		return pH->EndFunction(rangeID);
	}

	return pH->EndFunction();
}

int CScriptBind_GameAI::ResetAloneDetector(IFunctionHandler* pH, ScriptHandle entityID)
{
	if(AloneDetectorContainer* aloneDetectorContainer = gGameAIEnv.aloneDetectorModule->GetRunningInstance(static_cast<EntityId>(entityID.n)))
		aloneDetectorContainer->ResetDetector();

	return pH->EndFunction();
}

int CScriptBind_GameAI::SetupAloneDetector(IFunctionHandler* pH, ScriptHandle entityID, float range, const char* aloneSignal, const char* notAloneSignal)
{
	if(AloneDetectorContainer* aloneDetectorContainer = gGameAIEnv.aloneDetectorModule->GetRunningInstance(static_cast<EntityId>(entityID.n)))
		aloneDetectorContainer->SetupDetector(AloneDetectorContainer::AloneDetectorSetup(range,aloneSignal,notAloneSignal));

	return pH->EndFunction();
}

int CScriptBind_GameAI::AddActorClassToAloneDetector(IFunctionHandler* pH, ScriptHandle entityID, const char* entityClassName)
{
	if(AloneDetectorContainer* aloneDetectorContainer = gGameAIEnv.aloneDetectorModule->GetRunningInstance(static_cast<EntityId>(entityID.n)))
		aloneDetectorContainer->AddEntityClass(entityClassName);

	return pH->EndFunction();
}

int CScriptBind_GameAI::RemoveActorClassFromAloneDetector(IFunctionHandler* pH, ScriptHandle entityID, const char* entityClassName)
{
	if(AloneDetectorContainer* aloneDetectorContainer = gGameAIEnv.aloneDetectorModule->GetRunningInstance(static_cast<EntityId>(entityID.n)))
		aloneDetectorContainer->RemoveEntityClass(entityClassName);

	return pH->EndFunction();
}

int CScriptBind_GameAI::IsAloneForAloneDetector(IFunctionHandler* pH, ScriptHandle entityID)
{
	if(AloneDetectorContainer* aloneDetectorContainer = gGameAIEnv.aloneDetectorModule->GetRunningInstance(static_cast<EntityId>(entityID.n)))
		return pH->EndFunction(aloneDetectorContainer->IsAlone());

	return pH->EndFunction();
}

int CScriptBind_GameAI::GetRangeState(IFunctionHandler* pH, ScriptHandle entityID, int rangeID)
{
	if (RangeContainer* rangeContainer = gGameAIEnv.rangeModule->GetRunningInstance((EntityId)entityID.n))
	{
		if (const RangeContainer::Range* range = rangeContainer->GetRange((RangeContainer::RangeID)rangeID))
			return pH->EndFunction(range->state);
	}

	return pH->EndFunction();	
}

int CScriptBind_GameAI::ChangeRange(IFunctionHandler* pH, ScriptHandle entityID, int rangeID, float distance)
{
	if (RangeContainer* rangeContainer = gGameAIEnv.rangeModule->GetRunningInstance((EntityId)entityID.n))
	{
		rangeContainer->ChangeRange((RangeContainer::RangeID)rangeID, distance);
	}

	return pH->EndFunction();
}

int CScriptBind_GameAI::RegisterObjectVisible(IFunctionHandler *pH, ScriptHandle entityID)
{
	const int paramCount = pH->GetParamCount();

	uint32 visibleObjectRule = eVOR_Default;
	int factionId = 0;
	if (paramCount > 1)
	{
		pH->GetParam(2, visibleObjectRule);

		if (paramCount > 2)
		{
			pH->GetParam(3, factionId);
		}
	}

	const bool bRegistered = g_pGame->GetGameAISystem()->GetVisibleObjectsHelper().RegisterObject((EntityId)entityID.n, factionId, visibleObjectRule);
	return pH->EndFunction(bRegistered);
}

int CScriptBind_GameAI::UnregisterObjectVisible(IFunctionHandler *pH, ScriptHandle entityID)
{
	const bool bUnregistered = g_pGame->GetGameAISystem()->GetVisibleObjectsHelper().UnregisterObject((EntityId)entityID.n);
	return pH->EndFunction(bUnregistered);
}

int CScriptBind_GameAI::IsAISystemEnabled(IFunctionHandler *pH)
{
	bool enabled = gEnv->pAISystem && gEnv->pAISystem->IsEnabled();
	return pH->EndFunction(enabled);
}

/* 
	Squad Manager	
*/
int CScriptBind_GameAI::RegisterEntityForAISquadManager(IFunctionHandler* pH, ScriptHandle entityID)
{
	EntityId entID = static_cast<EntityId>(entityID.n);
	if(entID)
	{
		g_pGame->GetGameAISystem()->GetAISquadManager().RegisterAgent(entID);
	}

	return pH->EndFunction();
}

int CScriptBind_GameAI::RemoveEntityForAISquadManager(IFunctionHandler* pH, ScriptHandle entityID)
{
	EntityId entID = static_cast<EntityId>(entityID.n);
	if(entID)
	{
		g_pGame->GetGameAISystem()->GetAISquadManager().UnregisterAgent(entID);
	}

	return pH->EndFunction();
}

int CScriptBind_GameAI::GetSquadId(IFunctionHandler* pH, ScriptHandle entityId)
{
	AISquadManager& squadManager = g_pGame->GetGameAISystem()->GetAISquadManager();

	const SquadId squadId = squadManager.GetSquadIdForAgent(static_cast<SquadId>(entityId.n));

	if (squadId!=UnknownSquadId)
		return pH->EndFunction(squadId);
	else
		return pH->EndFunction(); // lua expects a nil for invalid squad value
}

int CScriptBind_GameAI::GetSquadMembers(IFunctionHandler* pH, int squadId)
{
	// Query the squad manager for the entity id's of all members of the squad

	AISquadManager& squadManager = g_pGame->GetGameAISystem()->GetAISquadManager();
	AISquadManager::MemberIDArray memberIDs;
	squadManager.GetSquadMembers(squadId, memberIDs);

	// Go through all members and construct a table where
	//   key = entity ID
	//   value = entity script table

	SmartScriptTable memberTable(m_pSS);

	AISquadManager::MemberIDArray::const_iterator it = memberIDs.begin();
	AISquadManager::MemberIDArray::const_iterator end = memberIDs.end();

	for (; it != end; ++it)
	{
		const EntityId id = *it;

		if (IEntity* entity = gEnv->pEntitySystem->GetEntity(id))
		{
			memberTable->SetAt(id, entity->GetScriptTable());
		}
	}

	return pH->EndFunction(memberTable);
}

int CScriptBind_GameAI::GetAveragePositionOfSquadScopeUsers(IFunctionHandler* pH, ScriptHandle entityIdHandle, const char* squadScopeName)
{
	AISquadManager& squadManager = g_pGame->GetGameAISystem()->GetAISquadManager();

	const EntityId entityId = static_cast<EntityId>(entityIdHandle.n);
	const SquadId squadId = squadManager.GetSquadIdForAgent(entityId);
	const SquadScopeID squadScopeID(squadScopeName);

	AISquadManager::MemberIDArray memberIDs;
	squadManager.GetSquadMembersInScope(squadId, squadScopeID, memberIDs);

	// Calculate the average position of all the members
	AISquadManager::MemberIDArray::const_iterator it = memberIDs.begin();
	AISquadManager::MemberIDArray::const_iterator end = memberIDs.end();

	Vec3 averagePosition(ZERO);
	float count = 0.0f;

	for (; it != end; ++it)
	{
		const EntityId id = *it;

		if (IEntity* entity = gEnv->pEntitySystem->GetEntity(id))
		{
			averagePosition += entity->GetWorldPos();
			count += 1.0f;
		}
	}

	if (count > 0.0f)
		averagePosition /= count;
	else
		averagePosition.zero();

	return pH->EndFunction(averagePosition);
}

int CScriptBind_GameAI::GetSquadScopeUserCount(IFunctionHandler* pH, ScriptHandle entityIdHandle, const char* squadScopeName)
{
	AISquadManager& squadManager = g_pGame->GetGameAISystem()->GetAISquadManager();

	const EntityId entityId = static_cast<EntityId>(entityIdHandle.n);
	const SquadId squadId = squadManager.GetSquadIdForAgent(entityId);
	const SquadScopeID squadScopeID(squadScopeName);

	AISquadManager::MemberIDArray memberIDs;
	squadManager.GetSquadMembersInScope(squadId, squadScopeID, memberIDs);

	return pH->EndFunction(static_cast<uint32>(memberIDs.size()));
}
 // ============================================================================
 //	Query if an actor entity is swimming underwater.
 //
 //	In:		The function handler (NULL is invalid!)
 //	In:		The script handle of the target entity.
 //
 //	Returns:	A default result code (in Lua: true if swimming underwater; 
 //	            otherwise false).
 //
 int CScriptBind_GameAI::IsSwimmingUnderwater(IFunctionHandler* pH, ScriptHandle entityID)
 {	
	 bool swimmingUnderwaterFlag = false;

	 CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor((EntityId)entityID.n));
	 if (pActor != NULL)
	 {
		 swimmingUnderwaterFlag = (pActor->IsSwimming() && pActor->IsHeadUnderWater());
	 }

	 return pH->EndFunction(swimmingUnderwaterFlag);
 }
