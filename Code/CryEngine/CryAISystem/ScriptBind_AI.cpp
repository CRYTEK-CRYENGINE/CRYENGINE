// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   ScriptBind_AI.h
   Version:     v1.00
   Description: Core scriptbinds for the AI system
   Notes:       Many methods in this file use a series of early-out tests
             Hence CCCPOINT often placed at the end, where work is done
   -------------------------------------------------------------------------
   History:
   - 3:11:2004   - Created by Kirill Bulatsev
   -13:02:2008   - Moved into AI system by Matthew

 *********************************************************************/

#include "StdAfx.h"
#include "ScriptBind_AI.h"
#include <CrySystem/ISystem.h>
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAIGroup.h>
#include <CryGame/IGame.h>
#include "GoalPipe.h"
#include "GoalOp.h"
#include "GoalOpFactory.h"
#include "Puppet.h"
#include "AIVehicle.h"
#include <CryString/StringUtils.h>

#include "GameSpecific/GoalOp_Crysis2.h"

#include "AIActions.h"

#include <CryEntitySystem/IEntityComponent.h>
#include <CryFlowGraph/IFlowSystem.h>
#include <CryAISystem/IInterestSystem.h>
#include "TacticalPointSystem/TacticalPointSystem.h"
#include "Communication/CommunicationManager.h"
#include "Group/GroupManager.h"
#include "TargetSelection/TargetTrackManager.h"
#include "AIBubblesSystem/AIBubblesSystem.h"
#include "Sequence/SequenceManager.h"
#include <CryAISystem/IBlackBoard.h>
#include <CryAISystem/IFactionMap.h>
#include <ctype.h>
#include "PipeUser.h"

#include <list>
#include <functional>

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#include <float.h> // for FLT_MAX
#endif
#include <CryMath/Cry_GeoDistance.h>

#include "GoalPipeXMLReader.h"
#include <CryAISystem/IMovementSystem.h>
#include <CryAISystem/MovementRequest.h>

#include "BehaviorTree/BehaviorTreeManager.h"
#include "Formation/FormationManager.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

// AI Object filters: define them as one-bit binary masks (1,2,4,8 etc..)
#define AIOBJECTFILTER_SAMEFACTION     1 // AI objects of the same species of the querying object.
#define AIOBJECTFILTER_SAMEGROUP       2 // AI objects of the same group of the querying object or with no group.
#define AIOBJECTFILTER_NOGROUP         4 // AI objects with Group ID ==AI_NOGROUP.
#define AIOBJECTFILTER_INCLUDEINACTIVE 8 // AI objects and Inactive AI Objects.

#define AI_NOGROUP                     -1 // group id == AI_NOGROUP -> the entity has no group

// AI Object search methods.
//-------------------- for anchors -----------------------------
#define AIANCHOR_NEAREST                   0
#define AIANCHOR_NEAREST_IN_FRONT          1
#define AIANCHOR_RANDOM_IN_RANGE           2
#define AIANCHOR_RANDOM_IN_RANGE_FACING_AT 3
#define AIANCHOR_NEAREST_FACING_AT         4
#define AIANCHOR_NEAREST_TO_REFPOINT       5
#define AIANCHOR_FARTHEST                  6
#define AIANCHOR_BEHIND_IN_RANGE           7
#define AIANCHOR_LEFT_TO_REFPOINT          8
#define AIANCHOR_RIGHT_TO_REFPOINT         9
#define AIANCHOR_HIDE_FROM_REFPOINT        10
//-------------------- bit flags -------------------------------
#define AIANCHOR_SEES_TARGET               (1 << 6)
#define AIANCHOR_BEHIND                    (1 << 7)

enum EAIForcedEventTypes
{
	SOUND_INTERESTING = 0,
	SOUND_THREATENING = 1,
};

enum EAIUseCoverAction
{
	COVER_HIDE   = 0,
	COVER_UNHIDE = 1,
};

enum EAIAdjustAimAction
{
	ADJUSTAIM_AIM  = 0,
	ADJUSTAIM_HIDE = 1,
};

enum EAIProximityFlags
{
	AIPROX_SIGNAL_ON_OBJ_DISABLE = 0x1,
	AIPROX_VISIBLE_TARGET_ONLY   = 0x2,
};

enum EAIParamNames
{
	AIPARAM_SIGHTRANGE = 1,
	AIPARAM_ATTACKRANGE,
	AIPARAM_ACCURACY,
	AIPARAM_GROUPID,
	AIPARAM_FOVPRIMARY,
	AIPARAM_FOVSECONDARY,
	AIPARAM_COMMRANGE,
	AIPARAM_FWDSPEED,
	AIPARAM_FACTION,
	AIPARAM_FACTIONHOSTILITY,
	AIPARAM_STRAFINGPITCH,
	AIPARAM_COMBATCLASS,
	AIPARAM_INVISIBLE,
	AIPARAM_PERCEPTIONSCALE_VISUAL,
	AIPARAM_PERCEPTIONSCALE_AUDIO,
	AIPARAM_CLOAK_SCALE,
	AIPARAM_FORGETTIME_TARGET,
	AIPARAM_FORGETTIME_SEEK,
	AIPARAM_FORGETTIME_MEMORY,
	AIPARAM_LOOKIDLE_TURNSPEED,
	AIPARAM_LOOKCOMBAT_TURNSPEED,
	AIPARAM_AIM_TURNSPEED,
	AIPARAM_FIRE_TURNSPEED,
	AIPARAM_MELEE_DISTANCE,
	AIPARAM_MELEE_SHORT_DISTANCE,
	AIPARAM_MIN_ALARM_LEVEL,
	AIPARAM_PROJECTILE_LAUNCHDIST,
	AIPARAM_SIGHTDELAY,
	AIPARAM_SIGHTNEARRANGE,
	AIPARAM_CLOAKED,

	// Warface perception extra data
	AIPARAM_PERCEPTIONAUDIORANGE_AGGRESSIVE,
	AIPARAM_PERCEPTIONAUDIORANGE_THREATENING,
	AIPARAM_PERCEPTIONAUDIORANGE_INTERESTING,
	AIPARAM_PERCEPTIONVISUALTIME_NEARREACT,
	AIPARAM_PERCEPTIONVISUALTIME_AGGRESSIVE,
	AIPARAM_PERCEPTIONVISUALTIME_THREATENING,
	AIPARAM_PERCEPTIONVISUALTIME_INTERESTING,
	AIPARAM_PERCEPTIONVISUAL_TARGETSTATIONARYTIME,
	AIPARAM_PERCEPTIONVISUAL_TARGETLONGMEMORYTIME,
	AIPARAM_PERCEPTIONVISUAL_CANFORGET,
};

enum EAIMoveAbilityNames
{
	AIMOVEABILITY_OPTIMALFLIGHTHEIGHT = 1,
	AIMOVEABILITY_MINFLIGHTHEIGHT,
	AIMOVEABILITY_MAXFLIGHTHEIGHT,
	AIMOVEABILITY_TELEPORTENABLE,
	AIMOVEABILITY_USEPREDICTIVEFOLLOWING
};

enum EStickFlags
{
	STICK_BREAK       = 0x01,
	STICK_SHORTCUTNAV = 0x02,
};

enum EDirection
{
	DIR_NORTH = 0,
	DIR_SOUTH,
	DIR_EAST,
	DIR_WEST
};

enum FindObjectOfTypeFlags
{
	AIFO_FACING_TARGET              = 0x0001,
	AIFO_NONOCCUPIED                = 0x0002,
	AIFO_CHOOSE_RANDOM              = 0x0004,
	AIFO_NONOCCUPIED_REFPOINT       = 0x0008,
	AIFO_USE_BEACON_AS_FALLBACK_TGT = 0x0010,
	AIFO_NO_DEVALUE                 = 0x0020,
};

#undef GET_ENTITY
#define GET_ENTITY(i)                              \
  ScriptHandle hdl;                                \
  pH->GetParam(i, hdl);                            \
  EntityId nID     = static_cast<EntityId>(hdl.n); \
  IEntity* pEntity = gEnv->pEntitySystem->GetEntity(nID);

bool GetSoundPerceptionDescriptorHelper(IPuppet* pPuppet, int iSoundType, SSoundPerceptionDescriptor& sDescriptor)
{
	if (pPuppet)
	{
		CPuppet* pCPuppet = static_cast<CPuppet*>(pPuppet);
		return pCPuppet->GetSoundPerceptionDescriptor(static_cast<EAISoundStimType>(iSoundType), sDescriptor);
	}

	return false;
}

int ParsePostureInfo(CPuppet* pPuppet, const SmartScriptTable& posture, const PostureManager::PostureInfo& base, int parentID, IScriptSystem* pSS)
{
	PostureManager::PostureInfo postureInfo = base;
	postureInfo.parentID = parentID;
	postureInfo.enabled = true;

	int type = postureInfo.type;
	if (!posture->GetValue("type", type) && (parentID == -1))
	{
		pSS->RaiseError("<CScriptBind_AI> "
		                "SetPostures: Missing or invalid posture type!");

		return -1;
	}

	postureInfo.type = (PostureManager::PostureType)type;

	const char* name = "UnnamedPosture";
	if (posture->GetValue("name", name))
		postureInfo.name = name;

	posture->GetValue("lean", postureInfo.lean);
	posture->GetValue("peekOver", postureInfo.peekOver);

	const char* agInput = "";
	if (posture->GetValue("agInput", agInput))
		postureInfo.agInput = agInput;

	int stance = postureInfo.stance;
	posture->GetValue("stance", stance);
	postureInfo.stance = (EStance)stance;

	posture->GetValue("priority", postureInfo.priority);

	bool templateOnly = false;  // Intentionally not inherited
	posture->GetValue("templateOnly", templateOnly);
	postureInfo.enabled = !templateOnly;  // Intentionally not inherited

	int id = pPuppet->GetPostureManager().AddPosture(postureInfo);

	// Parse children
	int i = 0;
	SmartScriptTable child;
	while (posture->GetAt(++i, child))
		ParsePostureInfo(pPuppet, child, postureInfo, id, pSS);

	return id;
}

//====================================================================
// OverlapSphere
//====================================================================
/*static bool OverlapSphere(const Vec3& pos, float radius, IPhysicalEntity **entities, unsigned nEntities, Vec3& hitDir)
   {
   primitives::sphere spherePrim;
   spherePrim.center = pos;
   spherePrim.r = radius;

   unsigned hitCount = 0;
   ray_hit hit;
   IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
   for (unsigned iEntity = 0 ; iEntity < nEntities ; ++iEntity)
   {
    IPhysicalEntity *pEntity = entities[iEntity];
    if (pPhysics->CollideEntityWithPrimitive(pEntity, spherePrim.type, &spherePrim, Vec3(ZERO), &hit))
    {
      hitDir += hit.n;
      hitCount++;
    }
   }
   hitDir.NormalizeSafe();
   return hitCount != 0;
   }
 */

//====================================================================
// OverlapSweptSphere
//====================================================================
static bool OverlapSweptSphere(const Vec3& pos, const Vec3& dir, float radius, IPhysicalEntity** entities, unsigned nEntities, Vec3& hitDir)
{
	primitives::sphere spherePrim;
	spherePrim.center = pos;
	spherePrim.r = radius;

	unsigned hitCount = 0;
	ray_hit hit;
	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	for (unsigned iEntity = 0; iEntity < nEntities; ++iEntity)
	{
		IPhysicalEntity* pEntity = entities[iEntity];
		if (pPhysics->CollideEntityWithPrimitive(pEntity, spherePrim.type, &spherePrim, dir, &hit))
		{
			hitDir += hit.n;
			hitCount++;
		}
	}
	hitDir.NormalizeSafe();

	return hitCount != 0;
}

//====================================================================
// HasPointInRange
//====================================================================
/*
   static bool	HasPointInRange(const Vec3& pos, float range, const std::list<Vec3>& points)
   {
   const float rangeSqr = sqr(range);
   for(std::list<Vec3>::const_iterator ptIt = points.begin(); ptIt != points.end(); ++ ptIt)
   {
    if(Distance::Point_PointSq((*ptIt), pos) < rangeSqr)
      return true;
   }
   return false;
   }
 */

//====================================================================
// RunStartupScript
//load the perception look-up table
//====================================================================
void InitLookUp()
{
	std::vector<float> theTable;
	string sName = PathUtil::Make("Scripts/AI", "lookup.xml");
	XmlNodeRef root = GetISystem()->LoadXmlFromFile(sName);
	if (!root)
		return;

	XmlNodeRef nodeWorksheet = root->findChild("Worksheet");
	if (!nodeWorksheet)
		return;

	XmlNodeRef nodeTable = nodeWorksheet->findChild("Table");
	if (!nodeTable)
		return;

	for (int childN = 0; childN < nodeTable->getChildCount(); ++childN)
	{
		XmlNodeRef nodeRow = nodeTable->getChild(childN);
		if (!nodeRow->isTag("Row"))
			continue;
		for (int childrenCntr(0), cellIndex(1); childrenCntr < nodeRow->getChildCount(); ++childrenCntr, ++cellIndex)
		{
			XmlNodeRef nodeCell = nodeRow->getChild(childrenCntr);
			if (!nodeCell->isTag("Cell"))
				continue;

			if (nodeCell->haveAttr("ss:Index"))
			{
				const char* pStrIdx = nodeCell->getAttr("ss:Index");
				PREFAST_SUPPRESS_WARNING(6031) sscanf(pStrIdx, "%d", &cellIndex);
			}
			XmlNodeRef nodeCellData = nodeCell->findChild("Data");
			if (!nodeCellData)
				continue;

			if (cellIndex == 1)
			{
				const char* item(nodeCellData->getContent());
				if (item)
				{
					float theValue(.0f);
					PREFAST_SUPPRESS_WARNING(6031) sscanf(item, "%f", &theValue);

					theTable.push_back(theValue);
				}
				break;
			}
		}
	}
	int tableSize(theTable.size());
	float* pTemBuffer(new float[tableSize]);
	for (int i(0); i < tableSize; ++i)
		pTemBuffer[i] = theTable[i];
	gEnv->pAISystem->SetPerceptionDistLookUp(pTemBuffer, tableSize);
	delete[] pTemBuffer;
}

//====================================================================
// CScriptBind_AI
//====================================================================
CScriptBind_AI::CScriptBind_AI() :
	m_pCurrentGoalPipe(0),
	m_IsGroupOpen(false)
{
	Init(gEnv->pSystem->GetIScriptSystem(), gEnv->pSystem);

	SetGlobalName("AI");

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_AI::

	SCRIPT_REG_FUNC(Warning);
	SCRIPT_REG_FUNC(Error);
	SCRIPT_REG_FUNC(LogProgress);
	SCRIPT_REG_FUNC(LogEvent);
	SCRIPT_REG_FUNC(LogComment);
	SCRIPT_REG_FUNC(RecComment);

	SCRIPT_REG_FUNC(ResetParameters);
	SCRIPT_REG_FUNC(ChangeParameter);
	SCRIPT_REG_FUNC(GetParameter);
	SCRIPT_REG_FUNC(IsEnabled);
	//	SCRIPT_REG_FUNC(AddSmartObjectCondition);
	SCRIPT_REG_FUNC(ChangeMovementAbility);
	SCRIPT_REG_FUNC(ExecuteAction);
	SCRIPT_REG_FUNC(AbortAction);
	SCRIPT_REG_FUNC(SetSmartObjectState);
	SCRIPT_REG_FUNC(ModifySmartObjectStates);
	SCRIPT_REG_FUNC(SmartObjectEvent);
	SCRIPT_REG_FUNC(GetLastUsedSmartObject);
	SCRIPT_REG_FUNC(CreateGoalPipe);
	SCRIPT_REG_FUNC(BeginGoalPipe);
	SCRIPT_REG_FUNC(EndGoalPipe);
	SCRIPT_REG_FUNC(BeginGroup);
	SCRIPT_REG_FUNC(EndGroup);
	SCRIPT_REG_FUNC(PushGoal);
	SCRIPT_REG_FUNC(PushLabel);
	SCRIPT_REG_FUNC(IsGoalPipe);
	SCRIPT_REG_FUNC(Signal);
	SCRIPT_REG_TEMPLFUNC(NotifyGroup, "groupID, senderID, notification");
	SCRIPT_REG_FUNC(FreeSignal);
	SCRIPT_REG_FUNC(SetIgnorant);
	SCRIPT_REG_TEMPLFUNC(BreakEvent, "entityID, pos, radius");
	SCRIPT_REG_TEMPLFUNC(AddCoverEntity, "entityID");
	SCRIPT_REG_TEMPLFUNC(RemoveCoverEntity, "entityID");
	SCRIPT_REG_FUNC(SetAssesmentMultiplier);
	SCRIPT_REG_FUNC(SetFactionThreatMultiplier);
	SCRIPT_REG_FUNC(GetGroupCount);
	SCRIPT_REG_FUNC(GetGroupMember);
	SCRIPT_REG_FUNC(GetGroupOf);
	SCRIPT_REG_FUNC(GetGroupAveragePosition);
	SCRIPT_REG_FUNC(Hostile);
	SCRIPT_REG_FUNC(FindObjectOfType);
	SCRIPT_REG_FUNC(SoundEvent);
	SCRIPT_REG_FUNC(VisualEvent);
	SCRIPT_REG_FUNC(GetSoundPerceptionDescriptor);
	SCRIPT_REG_FUNC(SetSoundPerceptionDescriptor);
	SCRIPT_REG_FUNC(GetAnchor);

	SCRIPT_REG_TEMPLFUNC(GetFactionOf, "entityID");
	SCRIPT_REG_TEMPLFUNC(SetFactionOf, "entityID, factionName");

	SCRIPT_REG_FUNC(GetReactionOf);
	SCRIPT_REG_TEMPLFUNC(SetReactionOf, "factionOne, factionTwo, reaction");

	SCRIPT_REG_FUNC(GetTypeOf);
	SCRIPT_REG_FUNC(GetSubTypeOf);
	SCRIPT_REG_FUNC(GetAttentionTargetAIType);
	SCRIPT_REG_FUNC(GetAttentionTargetOf);
	SCRIPT_REG_FUNC(GetAttentionTargetPosition);
	SCRIPT_REG_FUNC(GetAttentionTargetDirection);
	SCRIPT_REG_FUNC(GetAttentionTargetViewDirection);
	SCRIPT_REG_FUNC(GetAttentionTargetDistance);
	SCRIPT_REG_TEMPLFUNC(GetAttentionTargetEntity, "entityID");
	SCRIPT_REG_FUNC(GetAttentionTargetType);
	SCRIPT_REG_TEMPLFUNC(GetAttentionTargetThreat, "entityID");
	SCRIPT_REG_FUNC(GetTargetType);
	SCRIPT_REG_FUNC(GetTargetSubType);
	SCRIPT_REG_FUNC(GetAIObjectPosition);
	SCRIPT_REG_FUNC(GetBeaconPosition);
	SCRIPT_REG_FUNC(SetBeaconPosition);
	SCRIPT_REG_FUNC(GetTotalLengthOfPath);
	SCRIPT_REG_FUNC(GetNearestEntitiesOfType);
	SCRIPT_REG_FUNC(SetRefPointPosition);
	SCRIPT_REG_FUNC(SetRefPointDirection);
	SCRIPT_REG_FUNC(GetRefPointPosition);
	SCRIPT_REG_FUNC(GetRefPointDirection);
	SCRIPT_REG_FUNC(SetRefPointRadius);
	SCRIPT_REG_FUNC(SetRefShapeName);
	SCRIPT_REG_FUNC(GetRefShapeName);
	SCRIPT_REG_FUNC(SetVehicleStickTarget);
	SCRIPT_REG_FUNC(SetTerritoryShapeName);
	SCRIPT_REG_FUNC(CreateTempGenericShapeBox);
	SCRIPT_REG_FUNC(GetForwardDir);
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	SCRIPT_REG_FUNC(SetCharacter);
#endif
	SCRIPT_REG_FUNC(GetDirectAnchorPos);
	SCRIPT_REG_FUNC(GetEnclosingGenericShapeOfType);
	SCRIPT_REG_FUNC(IsPointInsideGenericShape);
	SCRIPT_REG_FUNC(DistanceToGenericShape);
	SCRIPT_REG_FUNC(ConstrainPointInsideGenericShape);
	SCRIPT_REG_FUNC(InvalidateHidespot);
	SCRIPT_REG_FUNC(EvalHidespot);
	SCRIPT_REG_FUNC(EvalPeek);
	SCRIPT_REG_TEMPLFUNC(AddPersonallyHostile, "entityID, hostileID");
	SCRIPT_REG_TEMPLFUNC(RemovePersonallyHostile, "entityID, hostileID");
	SCRIPT_REG_TEMPLFUNC(ResetPersonallyHostiles, "entityID, hostileID");
	SCRIPT_REG_TEMPLFUNC(IsPersonallyHostile, "entityID, hostileID");
	SCRIPT_REG_FUNC(NotifyReinfDone);
	SCRIPT_REG_FUNC(BehaviorEvent);
	SCRIPT_REG_FUNC(IntersectsForbidden);
	SCRIPT_REG_FUNC(IsPointInFlightRegion);
	SCRIPT_REG_FUNC(IsPointInWaterRegion);
	SCRIPT_REG_FUNC(GetEnclosingSpace);
	SCRIPT_REG_FUNC(Event);

	// Formation
	SCRIPT_REG_FUNC(CreateFormation);
	SCRIPT_REG_FUNC(AddFormationPoint);
	SCRIPT_REG_FUNC(AddFormationPointFixed);
	SCRIPT_REG_FUNC(GetFormationPointClass);
	SCRIPT_REG_FUNC(GetFormationPointPosition);
	SCRIPT_REG_FUNC(ChangeFormation);
	SCRIPT_REG_FUNC(ScaleFormation);
	SCRIPT_REG_FUNC(SetFormationUpdate);
	SCRIPT_REG_FUNC(SetFormationUpdateSight);
	// !Formation

	SCRIPT_REG_FUNC(GetLeader);
	SCRIPT_REG_FUNC(SetLeader);
	SCRIPT_REG_FUNC(UpTargetPriority);
	SCRIPT_REG_FUNC(DropTarget);
	SCRIPT_REG_FUNC(ClearPotentialTargets);
	SCRIPT_REG_FUNC(SetTempTargetPriority);
	SCRIPT_REG_FUNC(AddAggressiveTarget);
	SCRIPT_REG_FUNC(UpdateTempTarget);
	SCRIPT_REG_FUNC(ClearTempTarget);
	SCRIPT_REG_FUNC(SetExtraPriority);
	SCRIPT_REG_FUNC(GetExtraPriority);
	SCRIPT_REG_FUNC(RegisterTargetTrack);
	SCRIPT_REG_FUNC(UnregisterTargetTrack);
	SCRIPT_REG_FUNC(SetTargetTrackClassThreat);
	SCRIPT_REG_TEMPLFUNC(TriggerCurrentTargetTrackPulse, "entityID, stimulusName, pulseName");
	SCRIPT_REG_TEMPLFUNC(CreateStimulusEvent, "ownerId, targetID, stimulusName, pData");
	SCRIPT_REG_TEMPLFUNC(CreateStimulusEventInRange, "targetID, stimulusName, dataScriptTable");
	SCRIPT_REG_FUNC(GetStance);
	SCRIPT_REG_FUNC(SetStance);
	SCRIPT_REG_FUNC(SetUnitProperties);
	SCRIPT_REG_FUNC(GetUnitCount);
	SCRIPT_REG_FUNC(SetForcedNavigation);
	SCRIPT_REG_FUNC(SetAdjustPath);
	SCRIPT_REG_FUNC(GetHeliAdvancePoint);
	SCRIPT_REG_FUNC(CheckVehicleColision);
	SCRIPT_REG_FUNC(GetFlyingVehicleFlockingPos);
	SCRIPT_REG_FUNC(AssignPFPropertiesToPathType);
	SCRIPT_REG_FUNC(AssignPathTypeToSOUser);
	SCRIPT_REG_FUNC(SetPFProperties);
	SCRIPT_REG_FUNC(GetGroupTarget);
	SCRIPT_REG_TEMPLFUNC(GetGroupTargetType, "groupID");
	SCRIPT_REG_TEMPLFUNC(GetGroupTargetThreat, "groupID");
	SCRIPT_REG_TEMPLFUNC(GetGroupTargetEntity, "groupID");
	SCRIPT_REG_TEMPLFUNC(GetGroupScriptTable, "groupID");
	SCRIPT_REG_FUNC(GetGroupTargetCount);
	SCRIPT_REG_FUNC(GetNavigationType);
	SCRIPT_REG_FUNC(SetPathToFollow);
	SCRIPT_REG_FUNC(SetPathAttributeToFollow);
	SCRIPT_REG_FUNC(GetPredictedPosAlongPath);
	SCRIPT_REG_FUNC(SetPointListToFollow);
	SCRIPT_REG_FUNC(GetNearestPointOnPath);
	SCRIPT_REG_FUNC(GetPathSegNoOnPath);
	SCRIPT_REG_FUNC(GetPointOnPathBySegNo);
	SCRIPT_REG_FUNC(GetPathLoop);
	SCRIPT_REG_FUNC(GetNearestPathOfTypeInRange);
	SCRIPT_REG_FUNC(GetAlertness);
	SCRIPT_REG_FUNC(GetGroupAlertness);
	SCRIPT_REG_FUNC(GetDistanceAlongPath);
	SCRIPT_REG_FUNC(SetFireMode);
	SCRIPT_REG_FUNC(SetMemoryFireType);
	SCRIPT_REG_FUNC(GetMemoryFireType);
	SCRIPT_REG_FUNC(ThrowGrenade);
	SCRIPT_REG_FUNC(EnableCoverFire);
	SCRIPT_REG_FUNC(EnableFire);
	SCRIPT_REG_FUNC(IsFireEnabled);
	SCRIPT_REG_FUNC(CanFireInStance);
	SCRIPT_REG_FUNC(SetUseSecondaryVehicleWeapon);
	SCRIPT_REG_FUNC(SetRefPointToGrenadeAvoidTarget);
	SCRIPT_REG_FUNC(IsAgentInTargetFOV);
	SCRIPT_REG_FUNC(AgentLookAtPos);
	SCRIPT_REG_FUNC(ResetAgentLookAtPos);
	SCRIPT_REG_FUNC(IsAgentInAgentFOV);
	SCRIPT_REG_FUNC(CreateGroupFormation);
	SCRIPT_REG_FUNC(SetFormationPosition);
	SCRIPT_REG_FUNC(SetFormationLookingPoint);
	SCRIPT_REG_FUNC(GetFormationPosition);
	SCRIPT_REG_FUNC(GetFormationLookingPoint);
	SCRIPT_REG_FUNC(SetFormationAngleThreshold);

	SCRIPT_REG_TEMPLFUNC(SetMovementContext, "entityId, context");
	SCRIPT_REG_TEMPLFUNC(ClearMovementContext, "entityId, context");

	SCRIPT_REG_TEMPLFUNC(SetPostures, "entityId, postures");
	SCRIPT_REG_TEMPLFUNC(SetPosturePriority, "entityId, postureName, priority");
	SCRIPT_REG_TEMPLFUNC(GetPosturePriority, "entityId, postureName");

	SCRIPT_REG_FUNC(AddCombatClass);
	SCRIPT_REG_FUNC(SetRefPointAtDefensePos);
	SCRIPT_REG_FUNC(RegisterDamageRegion);
	SCRIPT_REG_FUNC(IgnoreCurrentHideObject);
	SCRIPT_REG_FUNC(GetCurrentHideAnchor);
	SCRIPT_REG_FUNC(GetBiasedDirection);
	SCRIPT_REG_FUNC(FindStandbySpotInShape);
	SCRIPT_REG_FUNC(FindStandbySpotInSphere);
	SCRIPT_REG_FUNC(GetObjectRadius);
	SCRIPT_REG_FUNC(GetProbableTargetPosition);
	SCRIPT_REG_FUNC(NotifySurpriseEntityAction);
	SCRIPT_REG_FUNC(Animation);
	SCRIPT_REG_TEMPLFUNC(SetAnimationTag, "entityID, tagName");
	SCRIPT_REG_TEMPLFUNC(ClearAnimationTag, "entityID, tagName");

	SCRIPT_REG_FUNC(SetRefpointToAnchor);
	SCRIPT_REG_FUNC(SetRefpointToPunchableObject);
	SCRIPT_REG_FUNC(MeleePunchableObject);
	SCRIPT_REG_FUNC(IsPunchableObjectValid);

	SCRIPT_REG_FUNC(ProcessBalancedDamage);

	SCRIPT_REG_FUNC(CanMelee);
	SCRIPT_REG_FUNC(CheckMeleeDamage);
	SCRIPT_REG_FUNC(IsMoving);

	SCRIPT_REG_FUNC(GetDirLabelToPoint);

	SCRIPT_REG_FUNC(SetAttentiontarget);
	SCRIPT_REG_FUNC(DebugReportHitDamage);

	SCRIPT_REG_TEMPLFUNC(RegisterInterestingEntity, "entityId, radius, baseInterest, category, aiAction");
	SCRIPT_REG_TEMPLFUNC(UnregisterInterestingEntity, "entityId");
	SCRIPT_REG_TEMPLFUNC(RegisterInterestedActor, "entityId, fInterestFilter, fAngleInDegrees");
	SCRIPT_REG_TEMPLFUNC(UnregisterInterestedActor, "entityId");

	SCRIPT_REG_FUNC(RegisterTacticalPointQuery);
	SCRIPT_REG_FUNC(GetTacticalPoints);
	SCRIPT_REG_FUNC(DestroyAllTPSQueries);

	SCRIPT_REG_FUNC(GetObjectBlackBoard);
	SCRIPT_REG_FUNC(GetBehaviorBlackBoard);

	SCRIPT_REG_FUNC(IsCoverCompromised);
	SCRIPT_REG_FUNC(SetCoverCompromised);
	SCRIPT_REG_FUNC(IsTakingCover);
	SCRIPT_REG_FUNC(IsMovingToCover);
	SCRIPT_REG_FUNC(IsMovingInCover);
	SCRIPT_REG_FUNC(IsInCover);
	SCRIPT_REG_FUNC(SetInCover);

	SCRIPT_REG_FUNC(IsOutOfAmmo);
	SCRIPT_REG_FUNC(IsLowOnAmmo);
	SCRIPT_REG_TEMPLFUNC(ResetAgentState, "entityId, stateLabel");
	SCRIPT_REG_TEMPLFUNC(PlayCommunication, "entityId, commName, channelName, contextExpiry, [skipSound], [skipAnimation], [targetId], [target]");
	SCRIPT_REG_TEMPLFUNC(StopCommunication, "playID");

	SCRIPT_REG_TEMPLFUNC(SetBehaviorVariable, "entityId, variableName, value");
	SCRIPT_REG_TEMPLFUNC(GetBehaviorVariable, "entityId, variableName");

	SCRIPT_REG_FUNC(SetAlarmed);

	SCRIPT_REG_TEMPLFUNC(LoadBehaviors, "folderName, extensions");
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	SCRIPT_REG_TEMPLFUNC(LoadCharacters, "folderName, table");
#endif
	SCRIPT_REG_TEMPLFUNC(IsLowHealthPauseActive, "entityID");

	SCRIPT_REG_TEMPLFUNC(GetPreviousBehaviorName, "entityID");
	SCRIPT_REG_TEMPLFUNC(SetContinuousMotion, "entityID, continuousMotion");

	SCRIPT_REG_TEMPLFUNC(GetPeakThreatLevel, "entityID");
	SCRIPT_REG_TEMPLFUNC(GetPeakThreatType, "entityID");

	SCRIPT_REG_TEMPLFUNC(GetPreviousPeakThreatLevel, "entityID");
	SCRIPT_REG_TEMPLFUNC(GetPreviousPeakThreatType, "entityID");

	SCRIPT_REG_TEMPLFUNC(CheckForFriendlyAgentsAroundPoint, "entityID, point, radius");

	SCRIPT_REG_TEMPLFUNC(EnableUpdateLookTarget, "entityID, bEnable");

	SCRIPT_REG_TEMPLFUNC(UpdateGlobalPerceptionScale, "visualScale, audioScale, [filterType], [faction]");
	SCRIPT_REG_TEMPLFUNC(QueueBubbleMessage, "entityID, message");

	SCRIPT_REG_TEMPLFUNC(SequenceBehaviorReady, "entityID");
	SCRIPT_REG_TEMPLFUNC(SequenceInterruptibleBehaviorLeft, "entityID");
	SCRIPT_REG_TEMPLFUNC(SequenceNonInterruptibleBehaviorLeft, "entityID");

	SCRIPT_REG_TEMPLFUNC(SetCollisionAvoidanceRadiusIncrement, "entityID, radius");
	SCRIPT_REG_TEMPLFUNC(RequestToStopMovement, "entityID");
	SCRIPT_REG_TEMPLFUNC(GetDistanceToClosestGroupMember, "entityID");
	SCRIPT_REG_TEMPLFUNC(IsAimReady, "entityID");

	SCRIPT_REG_TEMPLFUNC(AllowLowerBodyToTurn, "entityID, allowLowerBodyToTurn");

	SCRIPT_REG_TEMPLFUNC(GetGroupScopeUserCount, "entityIdHandle, groupScopeName");
	SCRIPT_REG_TEMPLFUNC(StartModularBehaviorTree, "entityIdHandle, treeName");
	SCRIPT_REG_TEMPLFUNC(StopModularBehaviorTree, "entityIdHandle");

	SCRIPT_REG_FUNC(LoadGoalPipes);

	SCRIPT_REG_FUNC(AutoDisable);

	SCRIPT_REG_TEMPLFUNC(GetPotentialTargetCountFromFaction, "entityID, factionName");
	SCRIPT_REG_TEMPLFUNC(GetPotentialTargetCount, "entityID");

	gEnv->pScriptSystem->SetGlobalValue("BEHAVIOR_STARTED", BehaviorStarted);
	gEnv->pScriptSystem->SetGlobalValue("BEHAVIOR_FINISHED", BehaviorFinished);
	gEnv->pScriptSystem->SetGlobalValue("BEHAVIOR_INTERRUPTED", BehaviorInterrupted);
	gEnv->pScriptSystem->SetGlobalValue("BEHAVIOR_FAILED", BehaviorFailed);

	gEnv->pScriptSystem->SetGlobalValue("Hostile", IFactionMap::Hostile);
	gEnv->pScriptSystem->SetGlobalValue("Neutral", IFactionMap::Neutral);
	gEnv->pScriptSystem->SetGlobalValue("Friendly", IFactionMap::Friendly);

	gEnv->pScriptSystem->SetGlobalValue("POSTURE_NULL", PostureManager::InvalidPosture);
	gEnv->pScriptSystem->SetGlobalValue("POSTURE_PEEK", PostureManager::PeekPosture);
	gEnv->pScriptSystem->SetGlobalValue("POSTURE_AIM", PostureManager::AimPosture);
	gEnv->pScriptSystem->SetGlobalValue("POSTURE_HIDE", PostureManager::HidePosture);

	SCRIPT_REG_GLOBAL(AIWEPA_LASER);
	SCRIPT_REG_GLOBAL(AIWEPA_COMBAT_LIGHT);
	SCRIPT_REG_GLOBAL(AIWEPA_PATROL_LIGHT);
	SCRIPT_REG_FUNC(EnableWeaponAccessory);

	SCRIPT_REG_GLOBAL(AIFO_FACING_TARGET);
	SCRIPT_REG_GLOBAL(AIFO_NONOCCUPIED);
	SCRIPT_REG_GLOBAL(AIFO_CHOOSE_RANDOM);
	SCRIPT_REG_GLOBAL(AIFO_NONOCCUPIED_REFPOINT);
	SCRIPT_REG_GLOBAL(AIFO_USE_BEACON_AS_FALLBACK_TGT);
	SCRIPT_REG_GLOBAL(AIFO_NO_DEVALUE);

	SCRIPT_REG_GLOBAL(AIPARAM_SIGHTRANGE);
	SCRIPT_REG_GLOBAL(AIPARAM_ATTACKRANGE);
	SCRIPT_REG_GLOBAL(AIPARAM_ACCURACY);
	SCRIPT_REG_GLOBAL(AIPARAM_GROUPID);
	SCRIPT_REG_GLOBAL(AIPARAM_FOVPRIMARY);
	SCRIPT_REG_GLOBAL(AIPARAM_FOVSECONDARY);
	SCRIPT_REG_GLOBAL(AIPARAM_COMMRANGE);
	SCRIPT_REG_GLOBAL(AIPARAM_FWDSPEED);
	SCRIPT_REG_GLOBAL(AIPARAM_FACTION);
	SCRIPT_REG_GLOBAL(AIPARAM_FACTIONHOSTILITY);
	SCRIPT_REG_GLOBAL(AIPARAM_STRAFINGPITCH);
	SCRIPT_REG_GLOBAL(AIPARAM_COMBATCLASS);
	SCRIPT_REG_GLOBAL(AIPARAM_INVISIBLE);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONSCALE_VISUAL);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONSCALE_AUDIO);
	SCRIPT_REG_GLOBAL(AIPARAM_CLOAK_SCALE);
	SCRIPT_REG_GLOBAL(AIPARAM_FORGETTIME_TARGET);
	SCRIPT_REG_GLOBAL(AIPARAM_FORGETTIME_MEMORY);
	SCRIPT_REG_GLOBAL(AIPARAM_FORGETTIME_SEEK);
	SCRIPT_REG_GLOBAL(AIPARAM_LOOKIDLE_TURNSPEED);
	SCRIPT_REG_GLOBAL(AIPARAM_LOOKCOMBAT_TURNSPEED);
	SCRIPT_REG_GLOBAL(AIPARAM_AIM_TURNSPEED);
	SCRIPT_REG_GLOBAL(AIPARAM_FIRE_TURNSPEED);
	SCRIPT_REG_GLOBAL(AIPARAM_MELEE_DISTANCE);
	SCRIPT_REG_GLOBAL(AIPARAM_MIN_ALARM_LEVEL);
	SCRIPT_REG_GLOBAL(AIPARAM_PROJECTILE_LAUNCHDIST);
	SCRIPT_REG_GLOBAL(AIPARAM_SIGHTDELAY);
	SCRIPT_REG_GLOBAL(AIPARAM_SIGHTNEARRANGE);
	SCRIPT_REG_GLOBAL(AIPARAM_CLOAKED);

	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONAUDIORANGE_AGGRESSIVE);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONAUDIORANGE_THREATENING);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONAUDIORANGE_INTERESTING);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONVISUALTIME_NEARREACT);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONVISUALTIME_AGGRESSIVE);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONVISUALTIME_THREATENING);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONVISUALTIME_INTERESTING);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONVISUAL_TARGETSTATIONARYTIME);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONVISUAL_TARGETLONGMEMORYTIME);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONVISUAL_CANFORGET);

	SCRIPT_REG_GLOBAL(AIMOVEABILITY_OPTIMALFLIGHTHEIGHT);
	SCRIPT_REG_GLOBAL(AIMOVEABILITY_MINFLIGHTHEIGHT);
	SCRIPT_REG_GLOBAL(AIMOVEABILITY_MAXFLIGHTHEIGHT);
	SCRIPT_REG_GLOBAL(AIMOVEABILITY_TELEPORTENABLE);
	SCRIPT_REG_GLOBAL(AIMOVEABILITY_USEPREDICTIVEFOLLOWING);

	SCRIPT_REG_GLOBAL(AILOOKMOTIVATION_LOOK);
	SCRIPT_REG_GLOBAL(AILOOKMOTIVATION_GLANCE);
	SCRIPT_REG_GLOBAL(AILOOKMOTIVATION_STARTLE);
	SCRIPT_REG_GLOBAL(AILOOKMOTIVATION_DOUBLETAKE);

	SCRIPT_REG_GLOBAL(AIOBJECT_ACTOR);
	SCRIPT_REG_GLOBAL(AIOBJECT_VEHICLE);
	SCRIPT_REG_GLOBAL(AIOBJECT_CAR);
	SCRIPT_REG_GLOBAL(AIOBJECT_BOAT);
	SCRIPT_REG_GLOBAL(AIOBJECT_HELICOPTER);
	SCRIPT_REG_GLOBAL(AIOBJECT_HELICOPTERCRYSIS2);
	SCRIPT_REG_GLOBAL(AIOBJECT_TARGET);
	SCRIPT_REG_GLOBAL(AIOBJECT_GUNFIRE);

	SCRIPT_REG_GLOBAL(AISIGNAL_INCLUDE_DISABLED);
	SCRIPT_REG_GLOBAL(AISIGNAL_DEFAULT);
	SCRIPT_REG_GLOBAL(AISIGNAL_PROCESS_NEXT_UPDATE);
	SCRIPT_REG_GLOBAL(AISIGNAL_NOTIFY_ONLY);
	SCRIPT_REG_GLOBAL(AISIGNAL_ALLOW_DUPLICATES);

	SCRIPT_REG_GLOBAL(AIOBJECT_2D_FLY);

	SCRIPT_REG_GLOBAL(AIOBJECT_ATTRIBUTE);
	SCRIPT_REG_GLOBAL(AIOBJECT_WAYPOINT);
	SCRIPT_REG_GLOBAL(AIOBJECT_SNDSUPRESSOR);
	SCRIPT_REG_GLOBAL(AIOBJECT_NAV_SEED);
	SCRIPT_REG_GLOBAL(AIOBJECT_MOUNTEDWEAPON);
	SCRIPT_REG_GLOBAL(AIOBJECT_GLOBALALERTNESS);
	SCRIPT_REG_GLOBAL(AIOBJECT_PLAYER);
	SCRIPT_REG_GLOBAL(AIOBJECT_DUMMY);
	SCRIPT_REG_GLOBAL(AIOBJECT_NONE);
	SCRIPT_REG_GLOBAL(AIOBJECT_ORDER);
	SCRIPT_REG_GLOBAL(AIOBJECT_GRENADE);
	SCRIPT_REG_GLOBAL(AIOBJECT_RPG);
	SCRIPT_REG_GLOBAL(AIOBJECT_INFECTED);
	SCRIPT_REG_GLOBAL(AIOBJECT_ALIENTICK);

	SCRIPT_REG_GLOBAL(AI_USE_HIDESPOTS);

	SCRIPT_REG_GLOBAL(STICK_BREAK);
	SCRIPT_REG_GLOBAL(STICK_SHORTCUTNAV);

	SCRIPT_REG_GLOBAL(GE_GROUP_STATE);
	SCRIPT_REG_GLOBAL(GE_UNIT_STATE);
	SCRIPT_REG_GLOBAL(GE_ADVANCE_POS);
	SCRIPT_REG_GLOBAL(GE_SEEK_POS);
	SCRIPT_REG_GLOBAL(GE_DEFEND_POS);
	SCRIPT_REG_GLOBAL(GE_LEADER_COUNT);
	SCRIPT_REG_GLOBAL(GE_MOST_LOST_UNIT);
	SCRIPT_REG_GLOBAL(GE_MOVEMENT_SIGNAL);
	SCRIPT_REG_GLOBAL(GE_NEAREST_SEEK);

	SCRIPT_REG_GLOBAL(GN_INIT);
	SCRIPT_REG_GLOBAL(GN_MARK_DEFEND_POS);
	SCRIPT_REG_GLOBAL(GN_CLEAR_DEFEND_POS);
	SCRIPT_REG_GLOBAL(GN_AVOID_CURRENT_POS);
	SCRIPT_REG_GLOBAL(GN_PREFER_ATTACK);
	SCRIPT_REG_GLOBAL(GN_PREFER_FLEE);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_ADVANCING);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_COVERING);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_WEAK_COVERING);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_HIDING);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_SEEKING);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_ALERTED);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_UNAVAIL);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_IDLE);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_SEARCHING);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_REINFORCE);

	SCRIPT_REG_GLOBAL(GS_IDLE);
	SCRIPT_REG_GLOBAL(GS_COVER);
	SCRIPT_REG_GLOBAL(GS_ADVANCE);
	SCRIPT_REG_GLOBAL(GS_SEEK);
	SCRIPT_REG_GLOBAL(GS_SEARCH);

	SCRIPT_REG_GLOBAL(GU_HUMAN_CAMPER);
	SCRIPT_REG_GLOBAL(GU_HUMAN_COVER);
	SCRIPT_REG_GLOBAL(GU_HUMAN_SNEAKER);
	SCRIPT_REG_GLOBAL(GU_HUMAN_LEADER);
	SCRIPT_REG_GLOBAL(GU_HUMAN_SNEAKER_SPECOP);
	SCRIPT_REG_GLOBAL(GU_ALIEN_MELEE);
	SCRIPT_REG_GLOBAL(GU_ALIEN_ASSAULT);
	SCRIPT_REG_GLOBAL(GU_ALIEN_MELEE_DEFEND);
	SCRIPT_REG_GLOBAL(GU_ALIEN_ASSAULT_DEFEND);
	SCRIPT_REG_GLOBAL(GU_ALIEN_EVADE);

	SCRIPT_REG_GLOBAL(AIEVENT_ONVISUALSTIMULUS);
	SCRIPT_REG_GLOBAL(AIEVENT_AGENTDIED);
	SCRIPT_REG_GLOBAL(AIEVENT_SLEEP);
	SCRIPT_REG_GLOBAL(AIEVENT_WAKEUP);
	SCRIPT_REG_GLOBAL(AIEVENT_ENABLE);
	SCRIPT_REG_GLOBAL(AIEVENT_DISABLE);
	SCRIPT_REG_GLOBAL(AIEVENT_PATHFINDON);
	SCRIPT_REG_GLOBAL(AIEVENT_PATHFINDOFF);
	SCRIPT_REG_GLOBAL(AIEVENT_CLEAR);
	SCRIPT_REG_GLOBAL(AIEVENT_DROPBEACON);
	SCRIPT_REG_GLOBAL(AIEVENT_USE);
	SCRIPT_REG_GLOBAL(AIEVENT_CLEARACTIVEGOALS);
	SCRIPT_REG_GLOBAL(AIEVENT_DRIVER_IN);
	SCRIPT_REG_GLOBAL(AIEVENT_DRIVER_OUT);
	SCRIPT_REG_GLOBAL(AIEVENT_FORCEDNAVIGATION);

	SCRIPT_REG_GLOBAL(AISOUND_GENERIC);
	SCRIPT_REG_GLOBAL(AISOUND_COLLISION);
	SCRIPT_REG_GLOBAL(AISOUND_COLLISION_LOUD);
	SCRIPT_REG_GLOBAL(AISOUND_MOVEMENT);
	SCRIPT_REG_GLOBAL(AISOUND_MOVEMENT_LOUD);
	SCRIPT_REG_GLOBAL(AISOUND_WEAPON);
	SCRIPT_REG_GLOBAL(AISOUND_EXPLOSION);

	SCRIPT_REG_GLOBAL(AI_LOOKAT_CONTINUOUS);
	SCRIPT_REG_GLOBAL(AI_LOOKAT_USE_BODYDIR);

	SCRIPT_REG_GLOBAL(AILASTOPRES_LOOKAT);
	SCRIPT_REG_GLOBAL(AILASTOPRES_USE);
	SCRIPT_REG_GLOBAL(AI_LOOK_FORWARD);
	SCRIPT_REG_GLOBAL(AI_MOVE_RIGHT);
	SCRIPT_REG_GLOBAL(AI_MOVE_LEFT);
	SCRIPT_REG_GLOBAL(AI_MOVE_FORWARD);
	SCRIPT_REG_GLOBAL(AI_MOVE_BACKWARD);
	SCRIPT_REG_GLOBAL(AI_MOVE_BACKLEFT);
	SCRIPT_REG_GLOBAL(AI_MOVE_BACKRIGHT);
	SCRIPT_REG_GLOBAL(AI_MOVE_TOWARDS_GROUP);
	SCRIPT_REG_GLOBAL(AI_USE_TARGET_MOVEMENT);
	SCRIPT_REG_GLOBAL(AI_CHECK_SLOPE_DISTANCE);

	SCRIPT_REG_GLOBAL(AI_REQUEST_PARTIAL_PATH);
	SCRIPT_REG_GLOBAL(AI_BACKOFF_FROM_TARGET);
	SCRIPT_REG_GLOBAL(AI_BREAK_ON_LIVE_TARGET);
	SCRIPT_REG_GLOBAL(AI_RANDOM_ORDER);
	SCRIPT_REG_GLOBAL(AI_USE_TIME);
	SCRIPT_REG_GLOBAL(AI_STOP_ON_ANIMATION_START);
	SCRIPT_REG_GLOBAL(AI_CONSTANT_SPEED);
	SCRIPT_REG_GLOBAL(AI_ADJUST_SPEED);

	SCRIPT_REG_GLOBAL(FIREMODE_OFF);
	SCRIPT_REG_GLOBAL(FIREMODE_BURST);
	SCRIPT_REG_GLOBAL(FIREMODE_CONTINUOUS);
	SCRIPT_REG_GLOBAL(FIREMODE_FORCED);
	SCRIPT_REG_GLOBAL(FIREMODE_AIM);
	SCRIPT_REG_GLOBAL(FIREMODE_SECONDARY);
	SCRIPT_REG_GLOBAL(FIREMODE_SECONDARY_SMOKE);
	SCRIPT_REG_GLOBAL(FIREMODE_MELEE);
	SCRIPT_REG_GLOBAL(FIREMODE_MELEE_FORCED);
	SCRIPT_REG_GLOBAL(FIREMODE_KILL);
	SCRIPT_REG_GLOBAL(FIREMODE_BURST_WHILE_MOVING);
	SCRIPT_REG_GLOBAL(FIREMODE_PANIC_SPREAD);
	SCRIPT_REG_GLOBAL(FIREMODE_BURST_DRAWFIRE);
	SCRIPT_REG_GLOBAL(FIREMODE_BURST_ONCE);
	SCRIPT_REG_GLOBAL(FIREMODE_BURST_SNIPE);
	SCRIPT_REG_GLOBAL(FIREMODE_AIM_SWEEP);

	SCRIPT_REG_GLOBAL(AITHREAT_NONE);
	SCRIPT_REG_GLOBAL(AITHREAT_SUSPECT);
	SCRIPT_REG_GLOBAL(AITHREAT_INTERESTING);
	SCRIPT_REG_GLOBAL(AITHREAT_THREATENING);
	SCRIPT_REG_GLOBAL(AITHREAT_AGGRESSIVE);

	SCRIPT_REG_GLOBAL(CHECKTYPE_MIN_DISTANCE);
	SCRIPT_REG_GLOBAL(CHECKTYPE_MIN_ROOMSIZE);

	SCRIPT_REG_GLOBAL(AITSR_NONE);
	SCRIPT_REG_GLOBAL(AITSR_SEE_STUNT_ACTION);
	SCRIPT_REG_GLOBAL(AITSR_SEE_CLOAKED);

	SCRIPT_REG_GLOBAL(AIGOALPIPE_NOTDUPLICATE);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_LOOP);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_RUN_ONCE);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_HIGHPRIORITY);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_SAMEPRIORITY);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_DONT_RESET_AG);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_KEEP_LAST_SUBPIPE);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_KEEP_ON_TOP);

	gEnv->pScriptSystem->SetGlobalValue("ORDERED", SCommunicationRequest::Ordered);
	gEnv->pScriptSystem->SetGlobalValue("UNORDERED", SCommunicationRequest::Unordered);

	SCRIPT_REG_GLOBAL(AI_REG_NONE);
	SCRIPT_REG_GLOBAL(AI_REG_REFPOINT);
	SCRIPT_REG_GLOBAL(AI_REG_LASTOP);
	SCRIPT_REG_GLOBAL(AI_REG_ATTENTIONTARGET);
	SCRIPT_REG_GLOBAL(AI_REG_COVER);
	SCRIPT_REG_GLOBAL(AI_REG_PATH);

	gEnv->pScriptSystem->SetGlobalValue("AI_RGT_ANY", eRGT_ANY);
	gEnv->pScriptSystem->SetGlobalValue("AI_RGT_SMOKE_GRENADE", eRGT_SMOKE_GRENADE);
	gEnv->pScriptSystem->SetGlobalValue("AI_RGT_FLASHBANG_GRENADE", eRGT_FLASHBANG_GRENADE);
	gEnv->pScriptSystem->SetGlobalValue("AI_RGT_FRAG_GRENADE", eRGT_FRAG_GRENADE);
	gEnv->pScriptSystem->SetGlobalValue("AI_RGT_EMP_GRENADE", eRGT_EMP_GRENADE);
	gEnv->pScriptSystem->SetGlobalValue("AI_RGT_GRUNT_GRENADE", eRGT_GRUNT_GRENADE);

	gEnv->pScriptSystem->SetGlobalValue("TTP_Last", eTTP_Last);
	gEnv->pScriptSystem->SetGlobalValue("TTP_OverSounds", eTTP_OverSounds);
	gEnv->pScriptSystem->SetGlobalValue("TTP_Always", eTTP_Always);

	gEnv->pScriptSystem->SetGlobalValue("NAV_TRIANGULAR", IAISystem::NAV_TRIANGULAR);
	gEnv->pScriptSystem->SetGlobalValue("NAV_ROAD", IAISystem::NAV_ROAD);
	gEnv->pScriptSystem->SetGlobalValue("NAV_WAYPOINT_HUMAN", IAISystem::NAV_WAYPOINT_HUMAN);
	gEnv->pScriptSystem->SetGlobalValue("NAV_VOLUME", IAISystem::NAV_VOLUME);
	gEnv->pScriptSystem->SetGlobalValue("NAV_UNSET", IAISystem::NAV_UNSET);
	gEnv->pScriptSystem->SetGlobalValue("NAV_FLIGHT", IAISystem::NAV_FLIGHT);
	gEnv->pScriptSystem->SetGlobalValue("NAV_WAYPOINT_3DSURFACE", IAISystem::NAV_WAYPOINT_3DSURFACE);
	gEnv->pScriptSystem->SetGlobalValue("NAV_SMARTOBJECT", IAISystem::NAV_SMARTOBJECT);
	gEnv->pScriptSystem->SetGlobalValue("NAV_FREE_2D", IAISystem::NAV_FREE_2D);
	gEnv->pScriptSystem->SetGlobalValue("NAVMASK_SURFACE", IAISystem::NAVMASK_SURFACE);

	gEnv->pScriptSystem->SetGlobalValue("GROUP_ALL", IAISystem::GROUP_ALL);
	gEnv->pScriptSystem->SetGlobalValue("GROUP_ENABLED", IAISystem::GROUP_ENABLED);
	gEnv->pScriptSystem->SetGlobalValue("GROUP_MAX", IAISystem::GROUP_MAX);

	SCRIPT_REG_GLOBAL(LA_NONE);
	SCRIPT_REG_GLOBAL(LA_HIDE);
	SCRIPT_REG_GLOBAL(LA_HOLD);
	SCRIPT_REG_GLOBAL(LA_ATTACK);
	SCRIPT_REG_GLOBAL(LA_SEARCH);
	SCRIPT_REG_GLOBAL(LA_FOLLOW);
	SCRIPT_REG_GLOBAL(LA_USE);
	SCRIPT_REG_GLOBAL(LA_USE_VEHICLE);

	SCRIPT_REG_GLOBAL(LAS_DEFAULT);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_FUNNEL);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_FLANK);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_FLANK_HIDE);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_FOLLOW_LEADER);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_ROW);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_CIRCLE);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_LEAPFROG);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_FRONT);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_CHAIN);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_COORDINATED_FIRE1);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_COORDINATED_FIRE2);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_USE_SPOTS);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_HIDE);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_HIDE_COVER);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_SWITCH_POSITIONS);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_CHASE);
	SCRIPT_REG_GLOBAL(LAS_SEARCH_DEFAULT);
	SCRIPT_REG_GLOBAL(LAS_SEARCH_COVER);
	SCRIPT_REG_GLOBAL(UPR_COMBAT_FLIGHT);
	SCRIPT_REG_GLOBAL(UPR_COMBAT_GROUND);
	SCRIPT_REG_GLOBAL(UPR_COMBAT_MARINE);
	SCRIPT_REG_GLOBAL(UPR_COMBAT_RECON);

	SCRIPT_REG_GLOBAL(AITARGET_NONE);
	SCRIPT_REG_GLOBAL(AITARGET_MEMORY);
	SCRIPT_REG_GLOBAL(AITARGET_VISUAL);
	SCRIPT_REG_GLOBAL(AITARGET_BEACON);
	SCRIPT_REG_GLOBAL(AITARGET_ENEMY);
	SCRIPT_REG_GLOBAL(AITARGET_SOUND);
	SCRIPT_REG_GLOBAL(AITARGET_GRENADE);
	SCRIPT_REG_GLOBAL(AITARGET_RPG);
	SCRIPT_REG_GLOBAL(AITARGET_FRIENDLY);

	SCRIPT_REG_GLOBAL(AIALERTSTATUS_SAFE);
	SCRIPT_REG_GLOBAL(AIALERTSTATUS_UNSAFE);
	SCRIPT_REG_GLOBAL(AIALERTSTATUS_READY);
	SCRIPT_REG_GLOBAL(AIALERTSTATUS_ACTION);

	SCRIPT_REG_GLOBAL(AIUSEOP_PLANTBOMB);
	SCRIPT_REG_GLOBAL(AIUSEOP_VEHICLE);
	SCRIPT_REG_GLOBAL(AIUSEOP_RPG);

	SCRIPT_REG_GLOBAL(AIREADIBILITY_NORMAL);
	SCRIPT_REG_GLOBAL(AIREADIBILITY_NOPRIORITY);
	SCRIPT_REG_GLOBAL(AIREADIBILITY_SEEN);
	SCRIPT_REG_GLOBAL(AIREADIBILITY_LOST);
	SCRIPT_REG_GLOBAL(AIREADIBILITY_INTERESTING);

	//SCRIPT_REG_GLOBAL("SIGNALID_THROWGRENADE", -10);
	gEnv->pScriptSystem->SetGlobalValue("SIGNALID_READIBILITY", SIGNALFILTER_READABILITY);
	gEnv->pScriptSystem->SetGlobalValue("SIGNALID_READIBILITYAT", SIGNALFILTER_READABILITYAT);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_LASTOP);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_GROUPONLY);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_GROUPONLY_EXCEPT);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_FACTIONONLY);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_ANYONEINCOMM);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_ANYONEINCOMM_EXCEPT);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_TARGET);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_SUPERGROUP);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_SUPERFACTION);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_SUPERTARGET);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_NEARESTGROUP);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_NEARESTINCOMM);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_NEARESTINCOMM_FACTION);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_NEARESTINCOMM_LOOKING);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_HALFOFGROUP);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_SENDER);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_LEADER);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_LEADERENTITY);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_FORMATION);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_FORMATION_EXCEPT);

	SCRIPT_REG_GLOBAL(AIOBJECTFILTER_SAMEFACTION);
	SCRIPT_REG_GLOBAL(AIOBJECTFILTER_SAMEGROUP);
	SCRIPT_REG_GLOBAL(AIOBJECTFILTER_NOGROUP);
	SCRIPT_REG_GLOBAL(AIOBJECTFILTER_INCLUDEINACTIVE);

	SCRIPT_REG_GLOBAL(AI_NOGROUP);

	SCRIPT_REG_GLOBAL(UNIT_CLASS_UNDEFINED);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_INFANTRY);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_SCOUT);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_ENGINEER);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_MEDIC);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_LEADER);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_CIVILIAN);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_COMPANION);
	SCRIPT_REG_GLOBAL(SPECIAL_FORMATION_POINT);
	SCRIPT_REG_GLOBAL(SHOOTING_SPOT_POINT);

	SCRIPT_REG_GLOBAL(AIANCHOR_NEAREST);
	SCRIPT_REG_GLOBAL(AIANCHOR_NEAREST_IN_FRONT);
	SCRIPT_REG_GLOBAL(AIANCHOR_RANDOM_IN_RANGE);
	SCRIPT_REG_GLOBAL(AIANCHOR_RANDOM_IN_RANGE_FACING_AT);
	SCRIPT_REG_GLOBAL(AIANCHOR_NEAREST_FACING_AT);
	SCRIPT_REG_GLOBAL(AIANCHOR_NEAREST_TO_REFPOINT);
	SCRIPT_REG_GLOBAL(AIANCHOR_FARTHEST);
	SCRIPT_REG_GLOBAL(AIANCHOR_BEHIND_IN_RANGE);
	SCRIPT_REG_GLOBAL(AIANCHOR_LEFT_TO_REFPOINT);
	SCRIPT_REG_GLOBAL(AIANCHOR_RIGHT_TO_REFPOINT);
	SCRIPT_REG_GLOBAL(AIANCHOR_HIDE_FROM_REFPOINT);
	SCRIPT_REG_GLOBAL(AIANCHOR_SEES_TARGET);
	SCRIPT_REG_GLOBAL(AIANCHOR_BEHIND);

	SCRIPT_REG_GLOBAL(BRANCH_ALWAYS);
	SCRIPT_REG_GLOBAL(IF_ACTIVE_GOALS);
	SCRIPT_REG_GLOBAL(IF_ACTIVE_GOALS_HIDE);
	SCRIPT_REG_GLOBAL(IF_NO_PATH);
	SCRIPT_REG_GLOBAL(IF_PATH_STILL_FINDING);
	SCRIPT_REG_GLOBAL(IF_IS_HIDDEN);
	SCRIPT_REG_GLOBAL(IF_CAN_HIDE);
	SCRIPT_REG_GLOBAL(IF_CANNOT_HIDE);
	SCRIPT_REG_GLOBAL(IF_STANCE_IS);
	SCRIPT_REG_GLOBAL(IF_FIRE_IS);
	SCRIPT_REG_GLOBAL(IF_HAS_FIRED);
	SCRIPT_REG_GLOBAL(IF_NO_LASTOP);
	SCRIPT_REG_GLOBAL(IF_SEES_LASTOP);
	SCRIPT_REG_GLOBAL(IF_SEES_TARGET);
	SCRIPT_REG_GLOBAL(IF_EXPOSED_TO_TARGET);
	SCRIPT_REG_GLOBAL(IF_CAN_SHOOT_TARGET);
	SCRIPT_REG_GLOBAL(IF_CAN_MELEE);
	SCRIPT_REG_GLOBAL(IF_NO_ENEMY_TARGET);
	SCRIPT_REG_GLOBAL(IF_PATH_LONGER);
	SCRIPT_REG_GLOBAL(IF_PATH_SHORTER);
	SCRIPT_REG_GLOBAL(IF_PATH_LONGER_RELATIVE);
	SCRIPT_REG_GLOBAL(IF_NAV_TRIANGULAR);
	SCRIPT_REG_GLOBAL(IF_NAV_WAYPOINT_HUMAN);
	SCRIPT_REG_GLOBAL(IF_TARGET_DIST_LESS);
	SCRIPT_REG_GLOBAL(IF_TARGET_DIST_GREATER);
	SCRIPT_REG_GLOBAL(IF_TARGET_IN_RANGE);
	SCRIPT_REG_GLOBAL(IF_TARGET_OUT_OF_RANGE);
	SCRIPT_REG_GLOBAL(IF_TARGET_MOVED_SINCE_START);
	SCRIPT_REG_GLOBAL(IF_TARGET_MOVED);
	SCRIPT_REG_GLOBAL(IF_TARGET_LOST_TIME_MORE);
	SCRIPT_REG_GLOBAL(IF_TARGET_LOST_TIME_LESS);
	SCRIPT_REG_GLOBAL(IF_COVER_COMPROMISED);
	SCRIPT_REG_GLOBAL(IF_COVER_NOT_COMPROMISED);
	SCRIPT_REG_GLOBAL(IF_COVER_SOFT);
	SCRIPT_REG_GLOBAL(IF_COVER_NOT_SOFT);
	SCRIPT_REG_GLOBAL(IF_CAN_SHOOT_TARGET_CROUCHED);
	SCRIPT_REG_GLOBAL(IF_COVER_FIRE_ENABLED);
	SCRIPT_REG_GLOBAL(IF_RANDOM);
	SCRIPT_REG_GLOBAL(IF_LASTOP_FAILED);
	SCRIPT_REG_GLOBAL(IF_LASTOP_SUCCEED);
	SCRIPT_REG_GLOBAL(NOT);

	SCRIPT_REG_GLOBAL(AIFAF_VISIBLE_FROM_REQUESTER);
	SCRIPT_REG_GLOBAL(AIFAF_INCLUDE_DEVALUED);
	SCRIPT_REG_GLOBAL(AIFAF_INCLUDE_DISABLED);

	// These numerical values are deprecated; use the strings instead
	SCRIPT_REG_GLOBAL(AIPATH_DEFAULT);
	SCRIPT_REG_GLOBAL(AIPATH_HUMAN);
	SCRIPT_REG_GLOBAL(AIPATH_HUMAN_COVER);
	SCRIPT_REG_GLOBAL(AIPATH_CAR);
	SCRIPT_REG_GLOBAL(AIPATH_TANK);
	SCRIPT_REG_GLOBAL(AIPATH_BOAT);
	SCRIPT_REG_GLOBAL(AIPATH_HELI);
	SCRIPT_REG_GLOBAL(AIPATH_3D);
	SCRIPT_REG_GLOBAL(AIPATH_SCOUT);
	SCRIPT_REG_GLOBAL(AIPATH_TROOPER);
	SCRIPT_REG_GLOBAL(AIPATH_HUNTER);

	SCRIPT_REG_GLOBAL(COVER_UNHIDE);
	SCRIPT_REG_GLOBAL(COVER_HIDE);

	SCRIPT_REG_GLOBAL(ADJUSTAIM_AIM);
	SCRIPT_REG_GLOBAL(ADJUSTAIM_HIDE);

	SCRIPT_REG_GLOBAL(AIPROX_SIGNAL_ON_OBJ_DISABLE);
	SCRIPT_REG_GLOBAL(AIPROX_VISIBLE_TARGET_ONLY);

	SCRIPT_REG_GLOBAL(AIANIM_SIGNAL);
	SCRIPT_REG_GLOBAL(AIANIM_ACTION);

	SCRIPT_REG_GLOBAL(DIR_NORTH);
	SCRIPT_REG_GLOBAL(DIR_SOUTH);
	SCRIPT_REG_GLOBAL(DIR_EAST);
	SCRIPT_REG_GLOBAL(DIR_WEST);

	SCRIPT_REG_GLOBAL(WAIT_ALL);
	SCRIPT_REG_GLOBAL(WAIT_ANY);
	SCRIPT_REG_GLOBAL(WAIT_ANY_2);

	SCRIPT_REG_GLOBAL(SOUND_INTERESTING);
	SCRIPT_REG_GLOBAL(SOUND_THREATENING);

	SCRIPT_REG_GLOBAL(AI_JUMP_CHECK_COLLISION);
	SCRIPT_REG_GLOBAL(AI_JUMP_ON_GROUND);
	SCRIPT_REG_GLOBAL(AI_JUMP_RELATIVE);
	SCRIPT_REG_GLOBAL(AI_JUMP_MOVING_TARGET);

	SCRIPT_REG_GLOBAL(JUMP_ANIM_FLY);
	SCRIPT_REG_GLOBAL(JUMP_ANIM_LAND);

	SCRIPT_REG_GLOBAL(STANCE_PRONE);
	SCRIPT_REG_GLOBAL(STANCE_CROUCH);
	SCRIPT_REG_GLOBAL(STANCE_STAND);
	SCRIPT_REG_GLOBAL(STANCE_RELAXED);
	SCRIPT_REG_GLOBAL(STANCE_LOW_COVER);
	SCRIPT_REG_GLOBAL(STANCE_HIGH_COVER);
	SCRIPT_REG_GLOBAL(STANCE_ALERTED);
	SCRIPT_REG_GLOBAL(STANCE_STEALTH);
	SCRIPT_REG_GLOBAL(STANCE_SWIM);

	gEnv->pScriptSystem->SetGlobalValue("SPEED_ZERO", AISPEED_ZERO);
	gEnv->pScriptSystem->SetGlobalValue("SPEED_SLOW", AISPEED_SLOW);
	gEnv->pScriptSystem->SetGlobalValue("SPEED_WALK", AISPEED_WALK);
	gEnv->pScriptSystem->SetGlobalValue("SPEED_RUN", AISPEED_RUN);
	gEnv->pScriptSystem->SetGlobalValue("SPEED_SPRINT", AISPEED_SPRINT);

	// Marcio: Kept here for retro-compatibility
	gEnv->pScriptSystem->SetGlobalValue("BODYPOS_PRONE", STANCE_PRONE);
	gEnv->pScriptSystem->SetGlobalValue("BODYPOS_CROUCH", STANCE_CROUCH);
	gEnv->pScriptSystem->SetGlobalValue("BODYPOS_STAND", STANCE_STAND);
	gEnv->pScriptSystem->SetGlobalValue("BODYPOS_RELAX", STANCE_RELAXED);
	gEnv->pScriptSystem->SetGlobalValue("BODYPOS_STEALTH", STANCE_STEALTH);

	gEnv->pScriptSystem->SetGlobalValue("MFT_Disabled", eMFT_Disabled);
	gEnv->pScriptSystem->SetGlobalValue("MFT_UseCoverFireTime", eMFT_UseCoverFireTime);
	gEnv->pScriptSystem->SetGlobalValue("MFT_Always", eMFT_Always);

	SCRIPT_REG_FUNC(GoTo);
	SCRIPT_REG_FUNC(SetSpeed);
	SCRIPT_REG_FUNC(SetEntitySpeedRange);

	gEnv->pScriptSystem->SetGlobalValue("AVOIDANCE_NONE", eAvoidance_NONE);
	gEnv->pScriptSystem->SetGlobalValue("AVOIDANCE_VEHICLES", eAvoidance_Vehicles);
	gEnv->pScriptSystem->SetGlobalValue("AVOIDANCE_PUPPETS", eAvoidance_Actors);
	gEnv->pScriptSystem->SetGlobalValue("AVOIDANCE_PLAYERS", eAvoidance_Players);
	gEnv->pScriptSystem->SetGlobalValue("AVOIDANCE_STATIC_OBSTACLES", eAvoidance_StaticObstacle);
	gEnv->pScriptSystem->SetGlobalValue("AVOIDANCE_PUSHABLE_OBSTACLES", eAvoidance_PushableObstacle);
	gEnv->pScriptSystem->SetGlobalValue("AVOIDANCE_DAMAGE_REGION", eAvoidance_DamageRegion);
	gEnv->pScriptSystem->SetGlobalValue("AVOIDANCE_ALL", eAvoidance_ALL);
	gEnv->pScriptSystem->SetGlobalValue("AVOIDANCE_DEFAULT", eAvoidance_DEFAULT);

	gEnv->pScriptSystem->SetGlobalValue("EST_Generic", TargetTrackHelpers::eEST_Generic);
	gEnv->pScriptSystem->SetGlobalValue("EST_Visual", TargetTrackHelpers::eEST_Visual);
	gEnv->pScriptSystem->SetGlobalValue("EST_Sound", TargetTrackHelpers::eEST_Sound);
	gEnv->pScriptSystem->SetGlobalValue("EST_BulletRain", TargetTrackHelpers::eEST_BulletRain);

	SCRIPT_REG_TEMPLFUNC(SetLastOpResult, "entityID, targetEntityId");

	InitLookUp();
}

//====================================================================
// ~CScriptBind_AI
//====================================================================
CScriptBind_AI::~CScriptBind_AI(void)
{
}

//====================================================================
// RunStartupScript
//====================================================================
bool CScriptBind_AI::RunStartupScript(bool reload)
{
	return m_pSS->ExecuteFile("scripts/ai/aiconfig.lua", true, reload);
}

//====================================================================
// Fetch entity ID from script parameter
//====================================================================
EntityId CScriptBind_AI::GetEntityIdFromParam(IFunctionHandler* pH, int i)
{
	assert(pH);
	assert(i > 0);
	ScriptHandle hdl(0);
	if (pH->GetParam(i, hdl))
	{
		return static_cast<EntityId>(hdl.n);
	}

	return 0;
}

//====================================================================
// Fetch entity pointer from script parameter
//====================================================================
IEntity* CScriptBind_AI::GetEntityFromParam(IFunctionHandler* pH, int i)
{
	EntityId nID = GetEntityIdFromParam(pH, i);
	return gEnv->pEntitySystem->GetEntity(nID);
}

//====================================================================
// Warning
//====================================================================
int CScriptBind_AI::Warning(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	const char* sParam = NULL;
	pH->GetParam(1, sParam);
	if (sParam && GetAISystem())
		AIWarningID("<Lua> ", "%s", sParam);
	return (pH->EndFunction());
}

//====================================================================
// Error
//====================================================================
int CScriptBind_AI::Error(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	const char* sParam = NULL;
	pH->GetParam(1, sParam);
	if (sParam && GetAISystem())
		AIErrorID("<Lua> ", "%s", sParam);
	return (pH->EndFunction());
}

//====================================================================
// LogProgress
//====================================================================
int CScriptBind_AI::LogProgress(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	const char* sParam = NULL;
	pH->GetParam(1, sParam);
	if (sParam && GetAISystem())
		GetAISystem()->LogProgress("<Lua> ", "%s", sParam);
	return (pH->EndFunction());
}

//====================================================================
// LogEvent
//====================================================================
int CScriptBind_AI::LogEvent(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	const char* sParam = NULL;
	pH->GetParam(1, sParam);
	if (sParam && GetAISystem())
		GetAISystem()->LogEvent("<Lua> ", "%s", sParam);
	return (pH->EndFunction());
}

//====================================================================
// LogComment
//====================================================================
int CScriptBind_AI::LogComment(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	const char* sParam = NULL;
	pH->GetParam(1, sParam);
	if (sParam && GetAISystem())
		GetAISystem()->LogComment("<Lua> ", "%s", sParam);
	return (pH->EndFunction());
}

//====================================================================
// RecComment
//====================================================================
int CScriptBind_AI::RecComment(IFunctionHandler* pH)
{
	//	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	const char* sComment = NULL;
	pH->GetParam(2, sComment);
	if (sComment && pEntity && pEntity->GetAI())
	{
		IAIRecordable::RecorderEventData recorderEventData(sComment);
		pEntity->GetAI()->RecordEvent(IAIRecordable::E_LUACOMMENT, &recorderEventData);

#ifdef CRYAISYSTEM_DEBUG
		AILogAlways("Lua Comment: Entity: %s: %s", pEntity->GetName(), sComment);
#endif
	}
	return (pH->EndFunction());
}

/*
   //
   //-----------------------------------------------------------------------------------------------------------
   int CScriptBind_AI::AddSmartObjectCondition(IFunctionHandler *pH)
   {
   SCRIPT_CHECK_PARAMETERS( 1 );
   SmartScriptTable pTable, pObject, pUser, pLimits, pDelay, pMultipliers, pAction;
   pH->GetParam( 1, pTable );

   const char* temp;
   SmartObjectCondition condition;
   pTable->GetValue( "sName", temp ); condition.sName = temp;
   pTable->GetValue( "sDescription", temp ); condition.sDescription = temp;
   pTable->GetValue( "sFolder", temp ); condition.sFolder = temp;
   pTable->GetValue( "iMaxAlertness", condition.iMaxAlertness );
   pTable->GetValue( "bEnabled", condition.bEnabled );
   pTable->GetValue( "iRuleType", condition.iRuleType );
   pTable->GetValue( "sohelper_EntranceHelper", temp ); condition.sEntranceHelper = temp;
   pTable->GetValue( "sohelper_ExitHelper", temp ); condition.sExitHelper = temp;
   pTable->GetValue( "Object", pObject );
   pObject->GetValue( "soclass_Class", temp ); condition.sObjectClass = temp;
   pObject->GetValue( "sopattern_State", temp ); condition.sObjectState = temp;
   pObject->GetValue( "sohelper_Helper", temp ); condition.sObjectHelper = temp;
   pTable->GetValue( "User", pUser );
   pUser->GetValue( "soclass_Class", temp ); condition.sUserClass = temp;
   pUser->GetValue( "soPattern_State", temp ); condition.sUserState = temp;
   pUser->GetValue( "sohelper_Helper", temp ); condition.sUserHelper = temp;
   pTable->GetValue( "Limits", pLimits );
   pLimits->GetValue( "fDistanceFrom", condition.fDistanceFrom );
   pLimits->GetValue( "fDistanceTo", condition.fDistanceTo );
   pLimits->GetValue( "fOrientation", condition.fOrientationLimit );
   pLimits->GetValue( "fOrientToTargetLimit", condition.fOrientationToTargetLimit );
   pTable->GetValue( "Delay", pDelay );
   pDelay->GetValue( "fMinimum", condition.fMinDelay );
   pDelay->GetValue( "fMaximum", condition.fMaxDelay );
   pDelay->GetValue( "fMemory", condition.fMemory );
   pTable->GetValue( "Multipliers", pMultipliers );
   pMultipliers->GetValue( "fProximity", condition.fProximityFactor );
   pMultipliers->GetValue( "fOrientation", condition.fOrientationFactor );
   pMultipliers->GetValue( "fVisibility", condition.fVisibilityFactor );
   pMultipliers->GetValue( "fRandomness", condition.fRandomnessFactor );
   pTable->GetValue( "Action", pAction );
   pAction->GetValue( "fLookAtOnPerc", condition.fLookAtOnPerc );
   pAction->GetValue( "sostates_ObjectPostActionState", temp ); condition.sObjectPostActionState = temp;
   pAction->GetValue( "sostates_UserPostActionState", temp ); condition.sUserPostActionState = temp;
   pAction->GetValue( "soaction_Name", temp ); condition.sAction = temp;
   pAction->GetValue( "sostates_ObjectPreActionState", temp ); condition.sObjectPreActionState = temp;
   pAction->GetValue( "sostates_UserPreActionState", temp ); condition.sUserPreActionState = temp;

   GetAISystem()->AddSmartObjectCondition( condition );
   return pH->EndFunction();
   }
 */

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::ExecuteAction(IFunctionHandler* pH)
{
	const char* sActionName = NULL;
	if (!pH->GetParam(1, sActionName))
	{
		gEnv->pAISystem->LogComment("<CScriptBind_AI::ExecuteAction> ", "ERROR: AI Action (param 1) must reference the Action name!");
		return pH->EndFunction();
	}

	IEntity* pUser = NULL;
	IEntity* pObject = NULL;

	EntityId nUserID = GetEntityIdFromParam(pH, 2);
	if (!nUserID)
	{
		gEnv->pAISystem->LogComment("<CScriptBind_AI::ExecuteAction> ", "ERROR: Not an entity id (param 2)!");
		return pH->EndFunction();
	}
	else
	{
		pUser = gEnv->pEntitySystem->GetEntity(nUserID);
		if (!pUser)
		{
			gEnv->pAISystem->LogComment("<CScriptBind_AI::ExecuteAction> ", "ERROR: Specified entity id (param 2) not found!");
			return pH->EndFunction();
		}
	}

	EntityId nObjectID = GetEntityIdFromParam(pH, 3);
	if (!nObjectID)
	{
		gEnv->pAISystem->LogComment("<CScriptBind_AI::ExecuteAction> ", "ERROR: Not an entity id (param 3)!");
		return pH->EndFunction();
	}
	else
	{
		pObject = gEnv->pEntitySystem->GetEntity(nObjectID);
		if (!pObject)
		{
			gEnv->pAISystem->LogComment("<CScriptBind_AI::ExecuteAction> ", "ERROR: Specified entity id (param 3) not found!");
			return pH->EndFunction();
		}
	}

	int maxAlertness = 0;
	pH->GetParam(4, maxAlertness);

	int goalPipeId = 0;
	if (pH->GetParamCount() > 4)
		pH->GetParam(5, goalPipeId);

	gAIEnv.pAIActionManager->ExecuteAIAction(sActionName, pUser, pObject, maxAlertness, goalPipeId);

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AbortAction(IFunctionHandler* pH)
{
	IEntity* pUser = NULL;

	EntityId nUserID = GetEntityIdFromParam(pH, 1);
	if (!nUserID)
	{
		gEnv->pAISystem->LogComment("<CScriptBind_AI::AbortAction> ", "ERROR: Not an entity id (param 1)!");
		return pH->EndFunction();
	}
	else
	{
		pUser = gEnv->pEntitySystem->GetEntity(nUserID);
		if (!pUser)
		{
			gEnv->pAISystem->LogComment("<CScriptBind_AI::AbortAction> ", "ERROR: Specified entity id (param 1) not found!");
			return pH->EndFunction();
		}
	}

	int goalPipeId = 0;
	if (pH->GetParamCount() > 1)
		pH->GetParam(2, goalPipeId);

	gAIEnv.pAIActionManager->AbortAIAction(pUser, goalPipeId);

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetSmartObjectState(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	const char* stateName;
	if (pEntity && gAIEnv.pSmartObjectManager)
	{
		pH->GetParam(2, stateName);
		gAIEnv.pSmartObjectManager->SetSmartObjectState(pEntity, stateName);
	}
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::ModifySmartObjectStates(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (pEntity && gAIEnv.pSmartObjectManager)
	{
		const char* listStates;
		pH->GetParam(2, listStates);
		gAIEnv.pSmartObjectManager->ModifySmartObjectStates(pEntity, listStates);
	}
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SmartObjectEvent(IFunctionHandler* pH)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	int id = 0;

	if (gAIEnv.pSmartObjectManager)
	{
		const char* sActionName = NULL;
		if (!pH->GetParam(1, sActionName))
		{
			AIErrorID("<CScriptBind_AI::SmartObjectEvent> ", "ERROR: The first parameter must be the name of the smart action!");
			return pH->EndFunction();
		}

		IEntity* pUser = NULL;
		IEntity* pObject = NULL;

		if (pH->GetParamCount() > 1 && pH->GetParamType(2) != svtNull)
		{
			pUser = GetEntityFromParam(pH, 2);
			if (!pUser)
				AIWarningID("<CScriptBind_AI::SmartObjectEvent> ", "WARNING: Specified entity id (param 2) not found!");
		}

		if (pH->GetParamCount() > 2 && pH->GetParamType(3) != svtNull)
		{
			pObject = GetEntityFromParam(pH, 3);
			if (!pObject)
				AIWarningID("<CScriptBind_AI::SmartObjectEvent> ", "WARNING: Specified entity id (param 3) not found!");
		}

		bool bUseRefPoint = false;
		Vec3 vRefPoint;
		if (pH->GetParamCount() > 3 && pH->GetParamType(4) != svtNull && pH->GetParam(4, vRefPoint))
			bUseRefPoint = true;

		id = gAIEnv.pSmartObjectManager->SmartObjectEvent(sActionName, pUser, pObject, bUseRefPoint ? &vRefPoint : NULL);
	}

	return pH->EndFunction(id);   // returns the id of the inserted goal pipe or 0 if no rule was found
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetLastUsedSmartObject(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	if (!pEntity)
	{
		AIWarningID("<CScriptBind_AI> ", "GetLastUsedSmartObject: Entity with id [%d] doesn't exist.", nID);
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "GetLastUsedSmartObject: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
	{
		AIWarningID("<CScriptBind_AI> ", "GetLastUsedSmartObject: Entity [%s] is registered in AI but not as a PipeUser.", pEntity->GetName());
		return pH->EndFunction();
	}

	IEntity* pObject = gEnv->pEntitySystem->GetEntity(pPipeUser->GetLastUsedSmartObjectId());
	if (pObject)
		return pH->EndFunction(pObject->GetScriptTable());   // returns the script table of the last used smart object entity
	else
		return pH->EndFunction(); // there's no last used smart object - return nil
}

//
//-----------------------------------------------------------------------------------------------------------
bool CScriptBind_AI::ParseTables(int firstTable, bool parseMovementAbility, IFunctionHandler* pH, AIObjectParams& params, bool& autoDisable)
{
	AgentParameters& agentParams = params.m_sParamStruct;

	SmartScriptTable pTable;
	SmartScriptTable pTableInstance;
	SmartScriptTable pMovementTable;
	SmartScriptTable pMeleeTable;
	// Properties table
	if (pH->GetParamCount() > firstTable - 1)
		pH->GetParam(firstTable, pTable);
	else
		return false;

	if (!pTable)
	{
		AIWarning("Failed to find table '%s'...", pH->GetFuncName());
		return false;
	}

	// Instance Properties table
	if (pH->GetParamCount() > firstTable)
		pH->GetParam(firstTable + 1, pTableInstance);
	else
		return false;

	if (!pTableInstance)
	{
		AIWarning("Failed to find table instance '%s'...", pH->GetFuncName());
		return false;
	}

	if (parseMovementAbility)
	{
		// Movement Abilities table
		if (pH->GetParamCount() > firstTable + 1)
			pH->GetParam(firstTable + 2, pMovementTable);
		else
			return false;
	}

	pTable->GetValue("bFactionHostility", params.m_sParamStruct.factionHostility);
	pTable->GetValue("bAffectSOM", params.m_sParamStruct.m_bPerceivePlayer);
	pTable->GetValue("awarenessOfPlayer", params.m_sParamStruct.m_fAwarenessOfPlayer);

	if (!pTable->GetValue("commrange", params.m_sParamStruct.m_fCommRange))
		pTableInstance->GetValue("commrange", params.m_sParamStruct.m_fCommRange);
	if (!pTable->GetValue("strafingPitch", params.m_sParamStruct.m_fStrafingPitch))
		params.m_sParamStruct.m_fStrafingPitch = 0.0f;

	if (!pTable->GetValue("groupid", params.m_sParamStruct.m_nGroup))
		pTableInstance->GetValue("groupid", params.m_sParamStruct.m_nGroup);

	const char* faction = 0;
	if (pTable->GetValue("esFaction", faction))
	{
		params.m_sParamStruct.factionID = gAIEnv.pFactionMap->GetFactionID(faction);
		if (faction && *faction && (params.m_sParamStruct.factionID == IFactionMap::InvalidFactionID))
		{
			AIWarning("Unknown faction '%s' being set...", faction);
		}
	}
	else
	{
		// Mrcio: backwards compatibility
		int species = -1;
		if (!pTable->GetValue("eiSpecies", species))
			pTable->GetValue("species", species);

		if (species > -1)
			params.m_sParamStruct.factionID = species;
	}

	pTableInstance->GetValue("bAutoDisable", autoDisable);
	// (MATT) Make really sure we never use autodisable for G04 - but actually I think autodisable param will be respected now {2008/11/26}
	if (gAIEnv.configuration.eCompatibilityMode == ECCM_GAME04 ||
	    gAIEnv.configuration.eCompatibilityMode == ECCM_WARFACE ||
	    gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS2)
		autoDisable = false;

	SmartScriptTable TnW;
	if (pTableInstance->GetValue("AITerritoryAndWave", TnW))
	{
		// Look up territories
		char* sTemp = NULL;
		TnW->GetValue("aiterritory_Territory", sTemp);
		if (sTemp) params.m_sParamStruct.m_sTerritoryName = sTemp;

		// Look up waves
		TnW->GetValue("aiwave_Wave", sTemp);
		if (sTemp) params.m_sParamStruct.m_sWaveName = sTemp;
	}

	params.m_sParamStruct.m_bAiIgnoreFgNode = false;

	float maneuverSpeed, walkSpeed, runSpeed, sprintSpeed;

	if (pMovementTable)
	{
		pMovementTable->GetValue("b3DMove", params.m_moveAbility.b3DMove);
		pMovementTable->GetValue("usePathfinder", params.m_moveAbility.bUsePathfinder);
		pMovementTable->GetValue("usePredictiveFollowing", params.m_moveAbility.usePredictiveFollowing);
		pMovementTable->GetValue("allowEntityClampingByAnimation", params.m_moveAbility.allowEntityClampingByAnimation);
		pMovementTable->GetValue("walkSpeed", walkSpeed);
		pMovementTable->GetValue("runSpeed", runSpeed);
		pMovementTable->GetValue("sprintSpeed", sprintSpeed);
		pMovementTable->GetValue("maxAccel", params.m_moveAbility.maxAccel);
		pMovementTable->GetValue("maxDecel", params.m_moveAbility.maxDecel);
		pMovementTable->GetValue("maneuverSpeed", maneuverSpeed);
		pMovementTable->GetValue("minTurnRadius", params.m_moveAbility.minTurnRadius);
		pMovementTable->GetValue("maxTurnRadius", params.m_moveAbility.maxTurnRadius);
		pMovementTable->GetValue("pathLookAhead", params.m_moveAbility.pathLookAhead);
		pMovementTable->GetValue("pathRadius", params.m_moveAbility.pathRadius);
		pMovementTable->GetValue("pathSpeedLookAheadPerSpeed", params.m_moveAbility.pathSpeedLookAheadPerSpeed);
		pMovementTable->GetValue("cornerSlowDown", params.m_moveAbility.cornerSlowDown);
		pMovementTable->GetValue("slopeSlowDown", params.m_moveAbility.slopeSlowDown);
		pMovementTable->GetValue("maneuverTrh", params.m_moveAbility.maneuverTrh);
		pMovementTable->GetValue("velDecay", params.m_moveAbility.velDecay);
		// for flying.
		pMovementTable->GetValue("optimalFlightHeight", params.m_moveAbility.optimalFlightHeight);
		pMovementTable->GetValue("minFlightHeight", params.m_moveAbility.minFlightHeight);
		pMovementTable->GetValue("maxFlightHeight", params.m_moveAbility.maxFlightHeight);

		pMovementTable->GetValue("passRadius", params.m_sParamStruct.m_fPassRadius);
		pMovementTable->GetValue("attackZoneHeight", params.m_sParamStruct.m_fAttackZoneHeight);

		pMovementTable->GetValue("pathFindPrediction", params.m_moveAbility.pathFindPrediction);

		pMovementTable->GetValue("lookIdleTurnSpeed", params.m_sParamStruct.m_lookIdleTurnSpeed);
		pMovementTable->GetValue("lookCombatTurnSpeed", params.m_sParamStruct.m_lookCombatTurnSpeed);
		pMovementTable->GetValue("aimTurnSpeed", params.m_sParamStruct.m_aimTurnSpeed);
		pMovementTable->GetValue("fireTurnSpeed", params.m_sParamStruct.m_fireTurnSpeed);
		params.m_sParamStruct.m_lookIdleTurnSpeed = DEG2RAD(params.m_sParamStruct.m_lookIdleTurnSpeed);
		params.m_sParamStruct.m_lookCombatTurnSpeed = DEG2RAD(params.m_sParamStruct.m_lookCombatTurnSpeed);
		params.m_sParamStruct.m_aimTurnSpeed = DEG2RAD(params.m_sParamStruct.m_aimTurnSpeed);
		params.m_sParamStruct.m_fireTurnSpeed = DEG2RAD(params.m_sParamStruct.m_fireTurnSpeed);

		pMovementTable->GetValue("resolveStickingInTrace", params.m_moveAbility.resolveStickingInTrace);
		pMovementTable->GetValue("pathRegenIntervalDuringTrace", params.m_moveAbility.pathRegenIntervalDuringTrace);
		pMovementTable->GetValue("avoidanceRadius", params.m_moveAbility.avoidanceRadius);
		pMovementTable->GetValue("lightAffectsSpeed", params.m_moveAbility.lightAffectsSpeed);

		pMovementTable->GetValue("directionalScaleRefSpeedMin", params.m_moveAbility.directionalScaleRefSpeedMin);
		pMovementTable->GetValue("directionalScaleRefSpeedMax", params.m_moveAbility.directionalScaleRefSpeedMax);

		pMovementTable->GetValue("avoidanceAbilities", params.m_moveAbility.avoidanceAbilities);
		pMovementTable->GetValue("pushableObstacleWeakAvoidance", params.m_moveAbility.pushableObstacleWeakAvoidance);
		pMovementTable->GetValue("pushableObstacleAvoidanceRadius", params.m_moveAbility.pushableObstacleAvoidanceRadius);

		pMovementTable->GetValue("pushableObstacleMassMin", params.m_moveAbility.pushableObstacleMassMin);
		pMovementTable->GetValue("pushableObstacleMassMax", params.m_moveAbility.pushableObstacleMassMax);

		pMovementTable->GetValue("collisionAvoidanceParticipation", params.m_moveAbility.collisionAvoidanceParticipation);
		pMovementTable->GetValue("collisionAvoidanceRadiusIncrement", params.m_moveAbility.collisionAvoidanceRadiusIncrement);

		pMovementTable->GetValue("distanceToCover", params.m_sParamStruct.distanceToCover);
		pMovementTable->GetValue("inCoverRadius", params.m_sParamStruct.inCoverRadius);
		pMovementTable->GetValue("effectiveCoverHeight", params.m_sParamStruct.effectiveCoverHeight);
		pMovementTable->GetValue("effectiveHighCoverHeight", params.m_sParamStruct.effectiveHighCoverHeight);

		const char* pathTypeStr;
		if (pMovementTable->GetValue("pathType", pathTypeStr))
		{
			SetPFProperties(params.m_moveAbility, pathTypeStr);
		}
		else
		{
			int pathType(AIPATH_DEFAULT);
			pMovementTable->GetValue("pathType", pathType);
			SetPFProperties(params.m_moveAbility, static_cast<EAIPathType>(pathType));
		}

		params.m_moveAbility.movementSpeeds.SetBasicSpeeds(0.5f * walkSpeed, walkSpeed, runSpeed, sprintSpeed, maneuverSpeed);

		const char* tableName = 0;
		SmartScriptTable pSpeedTable;
		if (pMovementTable->GetValue("AIMovementSpeeds", pSpeedTable))
		{
			SmartScriptTable pStanceSpeedTable;
			for (int s = 0; s < AgentMovementSpeeds::AMS_NUM_VALUES; ++s)
			{
				switch (s)
				{
				case AgentMovementSpeeds::AMS_RELAXED:
					tableName = "Relaxed";
					break;
				case AgentMovementSpeeds::AMS_COMBAT:
					tableName = "Combat";
					break;
				case AgentMovementSpeeds::AMS_STEALTH:
					tableName = "Stealth";
					break;
				case AgentMovementSpeeds::AMS_ALERTED:
					tableName = "Alerted";
					break;
				case AgentMovementSpeeds::AMS_CROUCH:
					tableName = "Crouch";
					break;
				case AgentMovementSpeeds::AMS_LOW_COVER:
					tableName = "LowCover";
					break;
				case AgentMovementSpeeds::AMS_HIGH_COVER:
					tableName = "HighCover";
					break;
				case AgentMovementSpeeds::AMS_PRONE:
					tableName = "Prone";
					break;
				case AgentMovementSpeeds::AMS_SWIM:
					tableName = "Swim";
					break;
				default:
					tableName = "invalid";
					break;
				}
				if (pSpeedTable->GetValue(tableName, pStanceSpeedTable))
				{
					for (int u = 0; u < AgentMovementSpeeds::AMU_NUM_VALUES /*-1*/; ++u)
					{
						switch (u)
						{
						case AgentMovementSpeeds::AMU_SLOW:
							tableName = "Slow";
							break;
						case AgentMovementSpeeds::AMU_WALK:
							tableName = "Walk";
							break;
						case AgentMovementSpeeds::AMU_RUN:
							tableName = "Run";
							break;
						case AgentMovementSpeeds::AMU_SPRINT:
							tableName = "Sprint";
							break;
						default:
							tableName = "invalid";
							break;
						}
						SmartScriptTable pUrgencySpeedTable;
						if (pStanceSpeedTable->GetValue(tableName, pUrgencySpeedTable))
						{
							float sdef = 0, smin = 0, smax = 0;
							pUrgencySpeedTable->GetAt(1, sdef);
							pUrgencySpeedTable->GetAt(2, smin);
							pUrgencySpeedTable->GetAt(3, smax);

							params.m_moveAbility.movementSpeeds.SetRanges(s, u, sdef, smin, smax);
						}
						else
						{
							if (u > 0)
								params.m_moveAbility.movementSpeeds.CopyRanges(s, u, u - 1);
							//							params.m_moveAbility.movementSpeeds.SetRanges(s, u, -1, -1,-1, -1,-1, -1,-1);
						}
					}
				}
				/*				else
				        {
				          AIWarningID("<CScriptBind_AI> ", "ParseTables: Unable to get element %s from AIMovementSpeeds table", tableName);
				        }
				 */}

		}
	}

	// RateOfDeath table
	SmartScriptTable pRateOfDeath;
	if (pTable->GetValue("RateOfDeath", pRateOfDeath))
	{
		if (!pRateOfDeath->GetValue("attackrange", params.m_sParamStruct.m_fAttackRange))
			pTableInstance->GetValue("attackrange", params.m_sParamStruct.m_fAttackRange);
		if (!pRateOfDeath->GetValue("accuracy", params.m_sParamStruct.m_fAccuracy))
			pTableInstance->GetValue("accuracy", params.m_sParamStruct.m_fAccuracy);
		pRateOfDeath->GetValue("reactionTime", params.m_sParamStruct.m_PerceptionParams.reactionTime);
	}

	// perception table
	SmartScriptTable pPerceptionTable;
	if (pTable->GetValue("Perception", pPerceptionTable))
	{
		pPerceptionTable->GetValue("FOVSecondary", params.m_sParamStruct.m_PerceptionParams.FOVSecondary);
		pPerceptionTable->GetValue("FOVPrimary", params.m_sParamStruct.m_PerceptionParams.FOVPrimary);
		pPerceptionTable->GetValue("stanceScale", params.m_sParamStruct.m_PerceptionParams.stanceScale);
		pPerceptionTable->GetValue("sightrange", params.m_sParamStruct.m_PerceptionParams.sightRange);
		pPerceptionTable->GetValue("sightnearrange", params.m_sParamStruct.m_PerceptionParams.sightNearRange);
		pPerceptionTable->GetValue("sightrangeVehicle", params.m_sParamStruct.m_PerceptionParams.sightRangeVehicle);
		pPerceptionTable->GetValue("sightdelay", params.m_sParamStruct.m_PerceptionParams.sightDelay);
		pPerceptionTable->GetValue("audioScale", params.m_sParamStruct.m_PerceptionParams.audioScale);
		pPerceptionTable->GetValue("persistence", params.m_sParamStruct.m_PerceptionParams.targetPersistence);
		pPerceptionTable->GetValue("bulletHitRadius", params.m_sParamStruct.m_PerceptionParams.bulletHitRadius);
		pPerceptionTable->GetValue("bIsAffectedByLight", params.m_sParamStruct.m_PerceptionParams.isAffectedByLight);
		pPerceptionTable->GetValue("minAlarmLevel", params.m_sParamStruct.m_PerceptionParams.minAlarmLevel);
		pPerceptionTable->GetValue("minDistanceToSpotDeadBodies", params.m_sParamStruct.m_PerceptionParams.minDistanceToSpotDeadBodies);
		pPerceptionTable->GetValue("cloakMaxDistCrouchedAndMoving", params.m_sParamStruct.m_PerceptionParams.cloakMaxDistCrouchedAndMoving);
		pPerceptionTable->GetValue("cloakMaxDistCrouchedAndStill", params.m_sParamStruct.m_PerceptionParams.cloakMaxDistCrouchedAndStill);
		pPerceptionTable->GetValue("cloakMaxDistMoving", params.m_sParamStruct.m_PerceptionParams.cloakMaxDistMoving);
		pPerceptionTable->GetValue("cloakMaxDistStill", params.m_sParamStruct.m_PerceptionParams.cloakMaxDistStill);
		Limit(params.m_sParamStruct.m_PerceptionParams.minAlarmLevel, 0.0f, 1.0f);
		pTable->GetValue("stuntReactionTimeOut", params.m_sParamStruct.m_PerceptionParams.stuntReactionTimeOut);
		pTable->GetValue("collisionReactionScale", params.m_sParamStruct.m_PerceptionParams.collisionReactionScale);
	}

	if ((pH->GetParamCount() > firstTable + 2) && (pH->GetParamType(firstTable + 3) == svtObject))
	{
		if (pH->GetParam(firstTable + 3, pMeleeTable))
		{
			float meleeAngleThreshold = 20.0f;

			pMeleeTable->GetValue("damageRadius", agentParams.m_fMeleeRange);
			pMeleeTable->GetValue("damageRadiusShort", agentParams.m_fMeleeRangeShort);
			pMeleeTable->GetValue("hitRange", agentParams.m_fMeleeHitRange);
			pMeleeTable->GetValue("angleThreshold", meleeAngleThreshold);
			pMeleeTable->GetValue("damage", agentParams.m_fMeleeDamage);
			pMeleeTable->GetValue("knockdownChance", agentParams.m_fMeleeKnowckdownChance);
			pMeleeTable->GetValue("impulse", agentParams.m_fMeleeImpulse);

			agentParams.m_fMeleeAngleCosineThreshold = cosf(DEG2RAD(meleeAngleThreshold));
		}
	}

	return true;
}

//
//-----------------------------------------------------------------------------------------------------------
void CScriptBind_AI::AssignPFPropertiesToPathType(const string& sPathType, AgentPathfindingProperties& properties)
{
	GetAISystem()->AssignPFPropertiesToPathType(sPathType, properties);
}

//
//-----------------------------------------------------------------------------------------------------------
void CScriptBind_AI::SetPFProperties(AgentMovementAbility& moveAbility, EAIPathType nPathType) const
{
	SetPFProperties(moveAbility, GetPathTypeName(nPathType));
}

//
//-----------------------------------------------------------------------------------------------------------
void CScriptBind_AI::SetPFProperties(AgentMovementAbility& moveAbility, const string& sPathType) const
{
	const AgentPathfindingProperties* pAgentPathfindingProperties = GetAISystem()->GetPFPropertiesOfPathType(sPathType);

	if (pAgentPathfindingProperties)
	{
		moveAbility.pathfindingProperties = *pAgentPathfindingProperties;
	}
	else
	{
		AIWarningID("<CScriptBind_AI> ", "SetPFProperties: Path type %s not handled - using default", sPathType.c_str());

		pAgentPathfindingProperties = GetAISystem()->GetPFPropertiesOfPathType("AIPATH_DEFAULT");
		if (pAgentPathfindingProperties)
		{
			moveAbility.pathfindingProperties = *pAgentPathfindingProperties;
		}
		else
		{
			/// character that travels on the surface but has no preferences - except it prefers to walk around
			/// hills rather than over them
			AgentPathfindingProperties hardcodedDefaultCharacterProperties(
			  IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_SMARTOBJECT,
			  0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			  5.0f, 0.5f, -10000.0f, 0.0f, 20.0f, 7.0f);

			moveAbility.pathfindingProperties = hardcodedDefaultCharacterProperties;
		}
	}
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::ResetParameters(IFunctionHandler* pH)
{
	// Mrcio: Enabling AI in Multiplayer!
	if (gEnv->bMultiplayer && !gEnv->bServer)
		return pH->EndFunction();

	GET_ENTITY(1);

	if (!pEntity)
	{
		AIWarningID("<CScriptBind_AI> ", "Tried to reset parameters with nonExisting entity with id [%d]. ", nID);
		return pH->EndFunction();
	}

	CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
	if (!pActor)
	{
		AIWarningID("<CScriptBind_AI> ", "Tried to set parameters with entity without AI actor [%s]. ", pEntity->GetName());
		return pH->EndFunction();
	}

	AIObjectParams params(AIOBJECT_NONE, pActor->GetParameters(), pActor->GetMovementAbility());
	bool autoDisable(true);

	bool bProcessMovement = false;
	if (pH->GetParamType(2) == svtBool)
		pH->GetParam(2, bProcessMovement);

	if (!ParseTables(3, bProcessMovement, pH, params, autoDisable))
	{
		AIWarningID("<CScriptBind_AI> ", "Failed resetting parameters for AI actor [%s]. (ProcessMovement = %d)", pEntity->GetName(), bProcessMovement ? 1 : 0);
		return pH->EndFunction();
	}

	pActor->ParseParameters(params, bProcessMovement);

	// (MATT) Respect autodisable settings on Reset {2008/11/26}
	IAIObject* pAI = pEntity->GetAI();
	IAIActorProxy* pPuppetProxy = pAI->GetProxy();
	if (pPuppetProxy)
	{
		pPuppetProxy->UpdateMeAlways(!autoDisable);
	}

	CCCPOINT(CScriptbind_AI_ResetParameters);
	return pH->EndFunction();
}

/*!Create a sequence of AI atomic commands (a goal pipe)
   @param desired name of the goal pipe
 */
//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::CreateGoalPipe(IFunctionHandler* pH)
{
	//	CHECK_PARAMETERS(1);

	const char* name;

	if (!pH->GetParams(name))
		return pH->EndFunction();

	/*IGoalPipe *pPipe =*/ gAIEnv.pPipeManager->CreateGoalPipe(name, CPipeManager::SilentlyReplaceDuplicate);

	//	m_mapGoals.insert(goalmap::iterator::value_type(name,pPipe));
	m_IsGroupOpen = false;

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::BeginGoalPipe(IFunctionHandler* pH)
{
	if (m_pCurrentGoalPipe)
	{
		AIWarningID("<CScriptBind_AI> ", "Calling AI.BeginGoalPipe() with active goalpipe '%s', must call AI.EndGoalPipe() to end goalpipe.", m_pCurrentGoalPipe->GetName());
		m_pCurrentGoalPipe = 0;
	}

	const char* name;
	if (!pH->GetParams(name))
		return pH->EndFunction();

	m_IsGroupOpen = false;
	m_pCurrentGoalPipe = gAIEnv.pPipeManager->CreateGoalPipe(name, CPipeManager::SilentlyReplaceDuplicate);
	if (!m_pCurrentGoalPipe)
		AIWarningID("<CScriptBind_AI> ", "AI.BeginGoalPipe: Goalpipe %s was not created.", name);

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::EndGoalPipe(IFunctionHandler* pH)
{
	if (!m_pCurrentGoalPipe)
		AIWarningID("<CScriptBind_AI> ", "Calling AI.EndGoalPipe() without AI.BeginGoalPipe() (current pipe is null).");
	m_pCurrentGoalPipe = 0;
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::BeginGroup(IFunctionHandler* pH)
{
	if (m_IsGroupOpen)
		AIWarningID("<CScriptBind_AI> ", "Calling AI.BeginGroup() twice (no nesed grouping supported).");
	m_IsGroupOpen = true;
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::EndGroup(IFunctionHandler* pH)
{
	if (!m_IsGroupOpen)
		AIWarningID("<CScriptBind_AI> ", "Calling AI.EndGroup() without AI.BeginGroup().");
	m_IsGroupOpen = false;
	return pH->EndFunction();
}

/*!Push a label into a goal pipe. The label is appended at the end of the goal pipe. Pipe of given name has to be previously created.
   @param name of the goal pipe in which the goal will be pushed.
   @param name of the label that needs to be pushed into the pipe
   @see CScriptBind_AI::CreateGoalPipe
 */
//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::PushLabel(IFunctionHandler* pH)
{
	const char* pipename;
	const char* labelname;
	stack_string goalname;

	if (m_pCurrentGoalPipe)
	{
		// Begin/EndGoalpipe
		if (!pH->GetParams(labelname))
			return pH->EndFunction();
		m_pCurrentGoalPipe->PushLabel(labelname);
	}
	else
	{
		// CreateGoalPipe
		GoalParameters params;
		if (!pH->GetParams(pipename, labelname))
			return pH->EndFunction();

		IGoalPipe* pPipe = 0;

		if (!(pPipe = gAIEnv.pPipeManager->OpenGoalPipe(pipename)))
			return pH->EndFunction();

		pPipe->PushLabel(labelname);
	}

	return pH->EndFunction();
}

/*!Push a goal into a goal pipe. The goal is appended at the end of the goal pipe. Pipe of given name has to be previously created.
   @param name of the goal pipe in which the goal will be pushed.
   @param name of atomic goal that needs to be pushed into the pipe
   @param 1 if the goal should block the pipe execution, 0 if the goal should not block the goal execution
   @see CScriptBind_AI::CreateGoalPipe
 */
//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::PushGoal(IFunctionHandler* pH)
{
	// TODO : (MATT) For the love of whatever you hold sacred lets remove the dozens of redundant PushGoal lines after this integration {2007/06/08:16:33:14}

	const char* pipename;
	const char* temp;
	string goalname;
	//	int id;
	GoalParameters params;
	bool blocking(false);
	IGoalPipe::EGroupType grouped(IGoalPipe::eGT_NOGROUP);
	IGoalPipe* pPipe(0);
	int cnt(0);

	// (MATT) Below is required to support old syntax - which could be removed in an afternoon {2008/02/21}
	if (m_pCurrentGoalPipe)
	{
		// Begin/EndGoalPipe
		if (!pH->GetParam(1, temp))
			temp = 0;
		if (pH->GetParamCount() > 1)
			pH->GetParam(2, blocking);
		pPipe = m_pCurrentGoalPipe;
		cnt = 2;
	}
	else
	{
		// CreateGoalPipe
		if (!pH->GetParam(1, pipename))
			pipename = 0;
		if (!pH->GetParam(2, temp))
			temp = 0;
		if (pH->GetParamCount() > 2)
			pH->GetParam(3, blocking);
		pPipe = gAIEnv.pPipeManager->OpenGoalPipe(pipename);
		cnt = 3;
	}

	if (!pPipe)
		return pH->EndFunction();

	if (m_IsGroupOpen)
		grouped = IGoalPipe::eGT_GROUPED;
	if (temp && *temp == '+')
	{
		++temp;
		grouped = IGoalPipe::eGT_GROUPWITHPREV;
	}
	goalname = temp;

	// Ask each registered goalop factory to create a goalop from this
	EGoalOperations op = CGoalPipe::GetGoalOpEnum(goalname.c_str());
	IGoalOp* pGoalOp = gAIEnv.pGoalOpFactory->GetGoalOp(goalname.c_str(), pH, cnt + 1, params);
	if (pGoalOp)
	{
		pPipe->PushGoal(pGoalOp, op, blocking, grouped, params);

		return pH->EndFunction();
	}

	switch (op)
	{
	case eGO_ACQUIRETARGET:
		{
			pH->GetParam(cnt + 1, temp);
			params.str = temp;
			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_ADJUSTAIM:
		{
			params.nValue = 0;
			int aimOrHide = 0;
			int useLastOpResultAsBackup = 0;
			int allowProne = 0;
			params.fValue = 0.0f; // timeout

			if (pH->GetParamCount() > cnt + 0)
				pH->GetParam(cnt + 1, aimOrHide); // aim = 0, hide = 1
			if (pH->GetParamCount() > cnt + 1)
				pH->GetParam(cnt + 2, useLastOpResultAsBackup); // useLastOpResultAsBackup = 1
			if (pH->GetParamCount() > cnt + 2)
				pH->GetParam(cnt + 3, allowProne);
			if (pH->GetParamCount() > cnt + 3)
				pH->GetParam(cnt + 4, params.fValue); // timeout

			if (aimOrHide) params.nValue |= 0x1;
			if (useLastOpResultAsBackup) params.nValue |= 0x2;
			if (allowProne) params.nValue |= 0x4;

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_ANIMATION:
		{
			// signal/action flag
			pH->GetParam(cnt + 1, params.nValue);
			// Anim action to set.
			pH->GetParam(cnt + 2, temp);
			params.str = temp;
			// Timeout
			params.fValue = -1;
			if (pH->GetParamCount() > cnt + 2)
				pH->GetParam(cnt + 3, params.fValue);

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_ANIMTARGET:
		{
			// signal/action flag
			pH->GetParam(cnt + 1, params.nValue);
			// Anim action to set.
			pH->GetParam(cnt + 2, temp);
			params.str = temp;
			// start width
			pH->GetParam(cnt + 3, params.vPos.x);
			// direction tolerance
			pH->GetParam(cnt + 4, params.vPos.y);
			// arc radius
			pH->GetParam(cnt + 5, params.vPos.z);
			// auto alignment
			params.bValue = false;

			if (pH->GetParamCount() >= cnt + 6)
				pH->GetParam(cnt + 6, params.bValue);

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_PATHFIND:
		{
			if (pH->GetParamCount() > cnt)
			{
				pH->GetParam(cnt + 1, temp);
				params.str = temp;
				params.pTarget = params.str.empty() ? 0 : gAIEnv.pAIObjectManager->GetAIObjectByName(0, temp);
			}
			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_LOCATE:
		{
			if (pH->GetParamType(cnt + 1) == svtString)
			{
				const char* temp2;
				pH->GetParam(cnt + 1, temp2);
				params.str = temp2;
				params.nValue = 0;
			}
			else if (pH->GetParamType(cnt + 1) == svtNumber)
			{
				params.str.clear();
				pH->GetParam(cnt + 1, params.nValue);
			}

			if (pH->GetParamCount() > cnt + 1)
				pH->GetParam(cnt + 2, params.fValue);
			else
				params.fValue = 0;

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_HIDE:
		{
			// Distance
			pH->GetParam(cnt + 1, params.fValue);
			// Method
			pH->GetParam(cnt + 2, params.nValue);
			// Exact
			if (pH->GetParamCount() > cnt + 2)
				pH->GetParam(cnt + 3, params.bValue);
			else
				params.bValue = true;
			// Min Distance
			if (pH->GetParamCount() > cnt + 3)
				pH->GetParam(cnt + 4, params.fValueAux);
			// Lastop result
			if (pH->GetParamCount() > cnt + 4)
				pH->GetParam(cnt + 5, params.nValueAux);

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_TACTICALPOS:
		{
			// Try to fetch the tactical query by name
			params.nValue = 0;
			const char* sQueryName = "null";
			ITacticalPointSystem* pTPS = gAIEnv.pTacticalPointSystem;
			if (pH->GetParamType(cnt + 1) == svtString)
			{
				if (pH->GetParam(cnt + 1, sQueryName)) // Raises error on fail
				{
					params.nValue = pTPS->GetQueryID(sQueryName);
					if (!params.nValue)
					{
						// Raise this as a script error so we get a stack trace
						m_pSS->RaiseError("<CScriptBind_AI> "
						                  "PushGoal: tacticalpos query does not exist (yet) : %s", sQueryName);
					}
				}
			}
			else
			{
				// Try to fetch by ID
				if (pH->GetParam(cnt + 1, params.nValue))
				{
					sQueryName = pTPS->GetQueryName(params.nValue);  // Raises error on fail
					if (!sQueryName)
					{
						// Raise this as a script error so we get a stack trace
						m_pSS->RaiseError("<CScriptBind_AI> "
						                  "PushGoal: tacticalpos query with id %d%s could not be found", params.nValue, (params.nValue ? "" : " (nil?)"));
					}
				}
			}

			if (params.nValue)
			{
				// Which register to put it in instead? For now, just refpoint or lastop
				if (pH->GetParamCount() > cnt + 1)
					pH->GetParam(cnt + 2, params.nValueAux);

				pPipe->PushGoal(eGO_TACTICALPOS, blocking, grouped, params);
			}
		}
		break;
	case eGO_LOOK:
		{
			// Look mode/style
			pH->GetParam(cnt + 1, params.nValue);
			// Allow body turn
			pH->GetParam(cnt + 2, params.bValue);
			// Which register to find the look target - for now just lastop and refpoint
			if (pH->GetParamCount() > cnt + 1)
				pH->GetParam(cnt + 3, params.nValueAux);

			pPipe->PushGoal(eGO_LOOK, blocking, grouped, params);
		}
		break;
	case eGO_TRACE:
		{
			params.fValue = 0;
			params.nValue = 0;
			params.fValueAux = 0;
			params.nValueAux = 0;
			if (pH->GetParamCount() > cnt)
			{
				// Exact
				pH->GetParam(cnt + 1, params.nValue);
				// Single step
				if (pH->GetParamCount() > cnt + 1)
					pH->GetParam(cnt + 2, params.nValueAux);
				// Distance
				if (pH->GetParamCount() > cnt + 2)
					pH->GetParam(cnt + 3, params.fValue);
			}
			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_LOOKAT:
		{
			pH->GetParam(cnt + 1, params.fValue);
			if (pH->GetParamCount() > cnt + 1)
				pH->GetParam(cnt + 2, params.fValueAux);
			if (pH->GetParamCount() > cnt + 2)
				pH->GetParam(cnt + 3, params.bValue);    //use LastOp
			if (pH->GetParamCount() > cnt + 3)
				pH->GetParam(cnt + 4, params.nValue);
			// Allow body turn
			if (pH->GetParamCount() > cnt + 4)
				pH->GetParam(cnt + 5, params.nValueAux);
			else
				params.nValueAux = 1;

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_LOOKAROUND:
		{
			// float lookAtRange, float scanIntervalRange, float intervalMin, float intervalMax, flags
			// Look at range
			pH->GetParam(cnt + 1, params.fValue);
			// Scan Interval
			if (pH->GetParamCount() > cnt + 1)
				pH->GetParam(cnt + 2, params.fValueAux);
			else
				params.fValueAux = -1;
			// Timeout min
			if (pH->GetParamCount() > cnt + 2)
				pH->GetParam(cnt + 3, params.vPos.x);
			else
				params.vPos.x = -1;
			// Timeout max
			if (pH->GetParamCount() > cnt + 3)
				pH->GetParam(cnt + 4, params.vPos.y);
			else
				params.vPos.y = -1;
			// flags: break look when live target , use last op as reference direction
			int flags = 0;
			if (pH->GetParamCount() > cnt + 4)
				pH->GetParam(cnt + 5, flags);
			params.nValue = flags;
			// Allow body turn
			if (pH->GetParamCount() > cnt + 5)
				pH->GetParam(cnt + 6, params.nValueAux);
			else
				params.nValueAux = 1;
			// Avoid looking at obstacles around the agent (uses ray casts...)
			if (pH->GetParamCount() > cnt + 6)
				pH->GetParam(cnt + 7, params.bValue);
			else
				params.bValue = false;

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_DEVALUE:
		{
			if (pH->GetParamCount() > cnt)
			{
				pH->GetParam(cnt + 1, params.fValue);
				if (pH->GetParamCount() > cnt + 1)
					pH->GetParam(cnt + 2, params.bValue);
				else
					params.bValue = false;
			}
			else
				params.fValue = false;

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_SIGNAL:
		{
			params.nValueAux = 0;
			pH->GetParam(cnt + 1, params.nValueAux); // get the signal id
			params.nValue = 0;

			if (pH->GetParamCount() > cnt + 1)
			{
				const char* sTemp;
				if (pH->GetParam(cnt + 2, sTemp)) // get the signal text
					params.str = sTemp;
			}

			if (pH->GetParamCount() > cnt + 2)
			{
				pH->GetParam(cnt + 3, params.nValue);  // get the desired filter
			}

			if (pH->GetParamCount() > cnt + 3)
			{
				pH->GetParam(cnt + 4, params.fValueAux);  // extra signal data passed as data.iValue.
			}

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_SCRIPT:
		{
			string scriptCode;
			ScriptVarType functionType = pH->GetParamType(cnt + 1);

			if (functionType == svtString)
			{
				const char* funcBody = 0;

				pH->GetParam(cnt + 1, funcBody);
				if (funcBody)
					scriptCode = funcBody;
			}
			else
			{
				assert(!"Invalid GoalOp Parameter!");
			}

			params.scriptCode = scriptCode;

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_APPROACH:
		{
			// Approach distance
			pH->GetParam(cnt + 1, params.fValue);

			// Lastop result usage flags, see EAILastOpResFlags.
			params.nValue = 0;
			if (pH->GetParamCount() > cnt + 1)
				pH->GetParam(cnt + 2, params.nValue);
			// get the accuracy.
			params.fValueAux = 1.f;
			if (pH->GetParamCount() > cnt + 2)
				pH->GetParam(cnt + 3, params.fValueAux);
			// Special signal to send when pathfinding fails.
			if (pH->GetParamCount() > cnt + 3)
			{
				const char* sTemp = "";
				if (pH->GetParam(cnt + 4, sTemp))
				{
					if (strlen(sTemp) > 1)
						params.str = sTemp;
				}
			}
			// Approach distance Variance
			params.vPos.x = 0.0f;
			if (pH->GetParamCount() > cnt + 4)
				pH->GetParam(cnt + 5, params.vPos.x);

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_BACKOFF:
		{
			pH->GetParam(cnt + 1, params.fValue); // distance
			if (pH->GetParamCount() > cnt + 1)
				pH->GetParam(cnt + 2, params.fValueAux);//max duration
			if (pH->GetParamCount() > cnt + 2)
				pH->GetParam(cnt + 3, params.nValue);//filter (use/look last op, direction)
			if (pH->GetParamCount() > cnt + 3)
				pH->GetParam(cnt + 4, params.nValueAux); //min distance

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_STANCE:
	case eGO_BODYPOS:
		{
			pH->GetParam(cnt + 1, params.nValue);
			if (pH->GetParamCount() > cnt + 1)
				pH->GetParam(cnt + 2, params.bValue);
			else
				params.bValue = false;
			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_TIMEOUT:
		{
			pH->GetParam(cnt + 1, params.fValue);
			if (pH->GetParamCount() > cnt + 1)
				pH->GetParam(cnt + 2, params.fValueAux);
			else
				params.fValueAux = 0;
			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_BRANCH:
		{
			// Label or Branch offset.
			if (pH->GetParamType(cnt + 1) == svtString)
			{
				// Label
				pH->GetParam(cnt + 1, temp);
				params.str = temp;
			}
			else
			{
				// Jump offset
				pH->GetParam(cnt + 1, params.nValueAux);
			}

			if (pH->GetParamCount() > cnt + 1)
			{
				// Branch type (see EOPBranchType).
				pH->GetParam(cnt + 2, params.nValue);

				// Parameters.
				if (pH->GetParamCount() > cnt + 2)
					pH->GetParam(cnt + 3, params.fValue);
				if (pH->GetParamCount() > cnt + 3)
					pH->GetParam(cnt + 4, params.fValueAux);
				else
					params.fValueAux = -1.0f;
			}
			else
				params.nValue = 0;

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_RANDOM:
		{
			pH->GetParam(cnt + 1, params.nValue);
			pH->GetParam(cnt + 2, params.fValue);
			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_CLEAR:
		{
			if (pH->GetParamCount() > cnt)
				pH->GetParam(cnt + 1, params.fValue);
			else
				params.fValue = 1.0f;
			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_FIRECMD:
		{
			// Fire mode, see EFireMode (IAgent.h ) for complete list of modes.
			if (pH->GetParamCount() > cnt)
				pH->GetParam(cnt + 1, params.nValue);
			// Use last op result.
			int useLastOpResult = 0;
			params.bValue = false;
			if (pH->GetParamCount() > cnt + 1)
			{
				pH->GetParam(cnt + 2, useLastOpResult);
				if (useLastOpResult & 1)
					params.bValue = true;
			}
			// Min timeout
			params.fValue = -1.f;
			if (pH->GetParamCount() > cnt + 2)
				pH->GetParam(cnt + 3, params.fValue);
			// Max timeout
			params.fValueAux = -1.f;
			if (pH->GetParamCount() > cnt + 3)
				pH->GetParam(cnt + 4, params.fValueAux);

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_STICKMINIMUMDISTANCE:
	case eGO_STICK:
		{
			// Stick distance
			pH->GetParam(cnt + 1, params.fValue);

			// Lastop result usage flags, see EAILastOpResFlags.
			params.nValue = 0;
			if (pH->GetParamCount() > cnt + 1)
				pH->GetParam(cnt + 2, params.nValue);

			// Stick flags, see EStickFlags.
			// When creating the goal pipe this flag actually indicates "break when approached the target".
			params.nValueAux = 0;
			if (pH->GetParamCount() > cnt + 2)
				pH->GetParam(cnt + 3, params.nValueAux);

			// Pathfinding accuracy
			params.fValueAux = params.fValue;
			if (pH->GetParamCount() > cnt + 3)
				pH->GetParam(cnt + 4, params.fValueAux);

			// End distance variance, the variance distance is substraceted from the end distance.
			params.vPos.x = 0.0f;
			if (pH->GetParamCount() > cnt + 4)
				pH->GetParam(cnt + 5, params.vPos.x);

			params.bValue = (op == eGO_STICK);

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_CHARGE:
		{
			assert(false);
		}
		break;
	case eGO_STRAFE:
		{
			int n = pH->GetParamCount();
			// Distance Start
			params.fValue = 0;
			pH->GetParam(cnt + 1, params.fValue);
			// Distance End
			params.fValueAux = 0;
			if (n > cnt + 1)
				pH->GetParam(cnt + 2, params.fValueAux);
			// Strafe while moving
			int whileMoving = 0;
			if (n > cnt + 2)
				pH->GetParam(cnt + 3, whileMoving);
			params.bValue = whileMoving != 0;

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_WAITSIGNAL:
		{
			// signal to wait for
			pH->GetParam(cnt + 1, temp);
			params.str = temp;

			// extra signal data to wait for
			if (pH->GetParamType(cnt + 2) == svtNull)
				params.fValue = 0;
			else
			{
				// (MATT) The other forms of waitsignal weren't used, but scripts are inconsistent - cleanup required {2008/08/09}
				params.fValue = 0;
				AIErrorID("<CScriptBind_AI::PushGoal> ", "This form of \"waitsignal\" has been removed");
			}

			// maximum waiting time
			if (pH->GetParamCount() == cnt + 3)
				pH->GetParam(cnt + 3, params.fValueAux);

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_FOLLOWPATH:
		{
			bool pathFindToStart = false, reverse = false, startNearest = false, bUsePointList = false, bControlSpeed = true;
			float fValueAux = 0.1f, fDesiredSpeed = 0.0f;

			pH->GetParam(cnt + 1, pathFindToStart);
			pH->GetParam(cnt + 2, reverse);
			pH->GetParam(cnt + 3, startNearest);
			pH->GetParam(cnt + 4, params.nValueAux);  // Loops
			if (pH->GetParamCount() > cnt + 4)
				pH->GetParam(cnt + 5, fValueAux);
			if (pH->GetParamCount() > cnt + 5)
				pH->GetParam(cnt + 6, bUsePointList);
			if (pH->GetParamCount() > cnt + 6)
				pH->GetParam(cnt + 7, bControlSpeed);
			if (pH->GetParamCount() > cnt + 7)
				pH->GetParam(cnt + 8, fDesiredSpeed);
			params.nValue = (pathFindToStart ? 1 : 0) | (reverse ? 2 : 0) | (startNearest ? 4 : 0) | (bUsePointList ? 8 : 0) | (bControlSpeed ? 16 : 0);
			params.fValue = fDesiredSpeed;
			params.fValueAux = fValueAux;
			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_USECOVER:
		{
			params.nValue = 0;
			params.nValueAux = 0;
			params.bValue = 0;

			pH->GetParam(cnt + 1, params.nValue);     // ECoverUsage

			if (pH->GetParamCount() > cnt + 1)
				pH->GetParam(cnt + 2, params.nValueAux);// ECoverUsageLocation

			if (pH->GetParamCount() > cnt + 2)
				pH->GetParam(cnt + 3, params.bValue);   // UseLastOpAsBackup

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_WAIT:
		{
			if (m_IsGroupOpen)
				AIWarningID("<CScriptBind_AI> ", "No WAIT goalOp allowed within a group. Pipe: %s ", pPipe->GetName());
			else
			{
				params.nValue = WAIT_ALL;
				pH->GetParam(cnt + 1, params.nValue);
				pPipe->PushGoal(op, blocking, grouped, params);
			}
		}
		break;
	case eGO_IGNOREALL:
		{
			pH->GetParam(cnt + 1, params.bValue);
			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;
	case eGO_SEEKCOVER:
		{
			assert(false);
		}
		break;
	case eGO_SPEED:
	case eGO_RUN:
		{
			// Maximum Movement urgency
			params.fValue = 0;
			pH->GetParam(cnt + 1, params.fValue);

			if (gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS || gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS2)
			{
				// Minimum Movement urgency
				params.fValueAux = 0;
				if (pH->GetParamCount() > cnt + 1)
					pH->GetParam(cnt + 2, params.fValueAux);
				// Scale down path length, if the path length is less than this parameter
				// the urgency is scaled down towards the minimum.
				params.vPos.x = 0;
				if (pH->GetParamCount() > cnt + 2)
					pH->GetParam(cnt + 3, params.vPos.x);
			}

			pPipe->PushGoal(op, blocking, grouped, params);
		}
		break;

	default:
		{
			if (pH->GetParamCount() > cnt)
			{
				// with float parameter one
				pH->GetParam(cnt + 1, params.fValue);
				if (!gAIEnv.pPipeManager->OpenGoalPipe(goalname))
					AIWarningID("<CScriptBind_AI> ", "PushGoal: Tried to push a goalpipe to %s that is not yet defined: %s", m_pCurrentGoalPipe ? m_pCurrentGoalPipe->GetName() : "(null current goalpipe)", goalname.c_str());
				if (op != eGO_LAST)
					pPipe->PushGoal(op, blocking, grouped, params);
				else
					pPipe->PushPipe(goalname, blocking, grouped, params); // Assume another goalpipe
			}
			else
			{
				// without parameters
				if (!gAIEnv.pPipeManager->OpenGoalPipe(goalname))
					AIWarningID("<CScriptBind_AI> ", "PushGoal: Tried to push a goalpipe to %s that is not yet defined: %s", m_pCurrentGoalPipe ? m_pCurrentGoalPipe->GetName() : "(null current goalpipe)", goalname.c_str());
				if (op != eGO_LAST)
					pPipe->PushGoal(op, blocking, grouped, params);
				else
					pPipe->PushPipe(goalname, blocking, grouped, params); // Assume another goalpipe
			}
			// NOTE: (MATT) Last two cases are used partly to allow the insertion of whole goalpipes into other goalpipes but also were used to save a little code for a few simple ops. That's not worth the possible confusion. {2007/06/08:17:35:53}
		}
		break;
	}

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IsGoalPipe(IFunctionHandler* pH)
{
	const char* pipename;

	if (!pH->GetParams(pipename))
		return pH->EndFunction();

	if (gAIEnv.pPipeManager->OpenGoalPipe(pipename))
		return pH->EndFunction(true);
	return pH->EndFunction(false);
}

bool CScriptBind_AI::GetSignalExtraData(IFunctionHandler* pH, int iParam, IAISignalExtraData*& pEData)
{
	bool bDataFound = false;
	SmartScriptTable theObj;
	int iNumberValues = 0;

	int iValue = 0;
	float fValue = 0.0f;
	ScriptHandle nID(0);
	const char* sObjectName = 0;
	Vec3 point(ZERO);

	for (int i = iParam; i <= iParam + 3 && i <= pH->GetParamCount(); i++)
	{
		//int iType = pH->GetParamType(i);
		if (pH->GetParamType(i) == svtNumber)
		{
			//entity ID or whatever

			if (++iNumberValues == 1)
				pH->GetParam(i, iValue);//convention: first parameter is always an integer. if you want to send
			// a float, always send an integer first
			else
				pH->GetParam(i, fValue);

			bDataFound = true;
		}
		else if (pH->GetParamType(i) == svtPointer)
		{
			pH->GetParam(i, nID);
			bDataFound = true;
		}
		else if (pH->GetParamType(i) == svtString)
		{
			pH->GetParam(i, sObjectName);
			bDataFound = true;
		}
		else if (pH->GetParamType(i) == svtObject)//supposed table
		{
			pH->GetParam(i, theObj);

			if (theObj.GetPtr())
			{
				// a vector is passed
				if (theObj->GetValue("x", point.x))
				{
					theObj->GetValue("y", point.y);
					theObj->GetValue("z", point.z);
					bDataFound = true;
				}
				else if (theObj->HaveValue("point"))
				{
					// the entire Extra data structure (nID, fValue,point, pObject) is passed
					pEData = GetAISystem()->CreateSignalExtraData();
					pEData->FromScriptTable(theObj);
					return true;
				}
				else
				{

					// Wrong type of data passed from LUA
				}
			}
		}
	}

	if (bDataFound)
	{
		pEData = GetAISystem()->CreateSignalExtraData();
		pEData->fValue = fValue;
		pEData->iValue = iValue;
		pEData->nID = nID;
		pEData->point = point;

		if (sObjectName)
			pEData->SetObjectName(sObjectName);

		return true;

	}

	return false;
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::Signal(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 4)
	{
		AIWarningID("<CScriptBind_AI::Signal> ", "Requires at least 4 arguments");
		return pH->EndFunction();
	}

	if (IEntity* pEntity = GetEntityFromParam(pH, 4))
	{
		int cFilter;
		int nSignalID;
		const char* szSignalText = 0;
		Vec3 point;

		pH->GetParam(1, cFilter);
		pH->GetParam(2, nSignalID);
		pH->GetParam(3, szSignalText);

		// sanity check
		if (szSignalText == NULL)
		{
			AIWarningID("<CScriptBind_AI::Signal> ", "ERROR: SignalName (param 3) cannot be nil!");
			return pH->EndFunction();
		}

		IAISignalExtraData* pEData = 0;
		GetSignalExtraData(pH, 5, pEData);

		if (IAIObject* aiObject = pEntity->GetAI())
		{
			GetAISystem()->SendSignal(cFilter, nSignalID, szSignalText, aiObject, pEData);

			return pH->EndFunction();
		}
	}

#if !defined(_RELEASE)
	{
		const char* szSignalText = NULL;
		pH->GetParam(3, szSignalText);
		EntityId nID = GetEntityIdFromParam(pH, 4);
		GetAISystem()->Warning("<CScriptBind_AI::Signal> ", "Could not deliver signal '%s' to invalid entity ID %u!",
		                       (szSignalText != NULL) ? szSignalText : "",
		                       nID);
	}
#endif

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::NotifyGroup(IFunctionHandler* pH, int groupID, ScriptHandle sender, const char* notification)
{
	return pH->EndFunction(ScriptHandle(gAIEnv.pGroupManager->NotifyGroup(groupID, (EntityId)sender.n, notification)));
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::FreeSignal(IFunctionHandler* pH)
{
	//CHECK_PARAMETERS(4);
	float fRadius;
	int nSignalID;
	const char* szSignalText;
	Vec3 pos;
	ScriptHandle hdl;
	IEntity* pEntity = NULL;
	IAIObject* pObject = NULL;

	if (!pH->GetParam(1, nSignalID) ||
	    !pH->GetParam(2, szSignalText) ||
	    !pH->GetParam(3, pos) ||
	    !pH->GetParam(4, fRadius))
	{
		return pH->EndFunction();
	}

	if (pH->GetParamCount() > 4)
		pEntity = GetEntityFromParam(pH, 5);

	IAISignalExtraData* pEData = 0;
	GetSignalExtraData(pH, 6, pEData);

	if (pEntity)
	{
		pObject = pEntity->GetAI();
	}

	GetAISystem()->SendAnonymousSignal(nSignalID, szSignalText, pos, fRadius, pObject, pEData);

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////
int CScriptBind_AI::SetIgnorant(IFunctionHandler* pH)
{
	//	AIWarningID("<CScriptBind_AI> ", "AI.MakePuppetIgnorant(...) is obsolete!" );
	GET_ENTITY(1);

	if (!pEntity)
		return pH->EndFunction();

	int iIgnorant;
	pH->GetParam(2, iIgnorant);

	if (IAIObject* pObject = pEntity->GetAI())
	{
		IPipeUser* pPiper = pObject->CastToIPipeUser();
		if (pPiper)
			pPiper->MakeIgnorant(iIgnorant > 0);
	}

	return pH->EndFunction();
}

int CScriptBind_AI::BreakEvent(IFunctionHandler* pH, ScriptHandle entityID, Vec3 pos, float radius)
{
	if (gAIEnv.pCoverSystem)
		gAIEnv.pCoverSystem->BreakEvent(pos, radius);

	return pH->EndFunction();
}

int CScriptBind_AI::AddCoverEntity(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (gAIEnv.pCoverSystem)
		gAIEnv.pCoverSystem->AddCoverEntity((EntityId)entityID.n);

	return pH->EndFunction();
}

int CScriptBind_AI::RemoveCoverEntity(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (gAIEnv.pCoverSystem)
		gAIEnv.pCoverSystem->RemoveCoverEntity((EntityId)entityID.n);

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetAssesmentMultiplier(IFunctionHandler* pH)
{
	pH->GetIScriptSystem()->RaiseError("AI.SetAssesmentMultiplier is deprecated.");
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetGroupCount(IFunctionHandler* pH)
{
	int groupId = -1;
	if (pH->GetParamType(1) == svtNumber)
		pH->GetParam(1, groupId);
	else if (pH->GetParamType(1) == svtPointer)
	{
		GET_ENTITY(1);
		if (pEntity)
		{
			if (IAIObject* aiObject = pEntity->GetAI())
				groupId = aiObject->GetGroupId();
		}
	}

	int flags = IAISystem::GROUP_ALL;
	if (pH->GetParamCount() > 1)
		pH->GetParam(2, flags);
	int type = 0;
	if (pH->GetParamCount() > 2)
		pH->GetParam(3, type);

	if (groupId > 0)
	{
		return pH->EndFunction(GetAISystem()->GetGroupCount(groupId, flags, type));
	}

	return pH->EndFunction(0);

}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetGroupMember(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 2)
		return pH->EndFunction();

	int groupId = -1;
	if (pH->GetParamType(1) == svtNumber)
		pH->GetParam(1, groupId);
	else if (pH->GetParamType(1) == svtPointer)
	{
		GET_ENTITY(1);
		if (pEntity)
		{
			if (IAIObject* aiObject = pEntity->GetAI())
				groupId = aiObject->GetGroupId();
		}
	}
	if (groupId < 0)
		return pH->EndFunction();
	int index = 0;
	pH->GetParam(2, index);

	int flags = IAISystem::GROUP_ALL;
	if (pH->GetParamCount() > 2)
		pH->GetParam(3, flags);

	int type = 0;
	if (pH->GetParamCount() > 3)
		pH->GetParam(4, type);

	IAIObject* pObject = GetAISystem()->GetGroupMember(groupId, index - 1, flags, type);
	if (pObject)
	{
		IEntity* pEntityMember = pObject->GetEntity();
		if (pEntityMember)
			return pH->EndFunction(pEntityMember->GetScriptTable());
	}
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetGroupTarget(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 1)
	{
		AIWarningID("<CScriptBind_AI> ", "GetGroupTarget: Too few parameters.");
		return pH->EndFunction();
	}
	GET_ENTITY(1);
	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		IAIActor* pAIActor = CastToIAIActorSafe(pAI);
		if (pAIActor)
		{
			int groupId = pAI->GetGroupId();
			IAIGroup* pGroup = GetAISystem()->GetIAIGroup(groupId);
			if (pGroup)
			{
				bool bHostileOnly = true;
				bool bLiveOnly = true;
				if (pH->GetParamCount() > 1)
					pH->GetParam(2, bHostileOnly);
				if (pH->GetParamCount() > 2)
					pH->GetParam(3, bLiveOnly);
				IAIObject* pTarget = pGroup->GetAttentionTarget(bHostileOnly, bLiveOnly);
				if (pTarget)
				{
					if (pTarget->CastToIPuppet()
					    || pTarget->CastToCAIVehicle()
					    || static_cast<CAIObject*>(pTarget)->GetType() == AIOBJECT_PLAYER)
					{
						IEntity* pTargetEntity = pTarget->GetEntity();
						if (pTargetEntity)
						{
							IScriptTable* pScript = pTargetEntity->GetScriptTable();
							return pH->EndFunction(pScript);
							//return pH->EndFunction(pTargetEntity->GetId());
						}
					}
					return pH->EndFunction(pTarget->GetName());
				}
			}
		}
	}
	return pH->EndFunction();
}

int CScriptBind_AI::GetGroupTargetType(IFunctionHandler* pH, int groupID)
{
	const Group& group = gAIEnv.pGroupManager->GetGroup(groupID);
	return pH->EndFunction(group.GetTargetType());
}

int CScriptBind_AI::GetGroupTargetThreat(IFunctionHandler* pH, int groupID)
{
	const Group& group = gAIEnv.pGroupManager->GetGroup(groupID);
	return pH->EndFunction(group.GetTargetThreat());
}

int CScriptBind_AI::GetGroupTargetEntity(IFunctionHandler* pH, int groupID)
{
	const Group& group = gAIEnv.pGroupManager->GetGroup(groupID);

	CCountedRef<CAIObject> targetRef = group.GetTarget();
	if (CAIObject* targetAIObj = targetRef.GetAIObject())
	{
		IEntity* entity = targetAIObj->GetEntity();
		if (entity)
			return pH->EndFunction(entity->GetScriptTable());
		else if (CAIObject* association = targetAIObj->GetAssociation().GetAIObject())
			if (entity = association->GetEntity())
				return pH->EndFunction(entity->GetScriptTable());
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetGroupScriptTable(IFunctionHandler* pH, int groupID)
{
	const Group& group = gAIEnv.pGroupManager->GetGroup(groupID);
	if (IAIGroupProxy* proxy = group.GetProxy())
		return pH->EndFunction(proxy->GetScriptTable());

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetGroupTargetCount(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 1)
	{
		AIWarningID("<CScriptBind_AI> ", "GetGroupTarget: Too few parameters.");
		return pH->EndFunction();
	}
	GET_ENTITY(1);
	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
		if (pAIActor)
		{
			int groupId = pAI->GetGroupId();
			IAIGroup* pGroup = GetAISystem()->GetIAIGroup(groupId);
			if (pGroup)
			{
				bool bHostileOnly = true;
				bool bLiveOnly = true;
				if (pH->GetParamCount() > 1)
					pH->GetParam(2, bHostileOnly);
				if (pH->GetParamCount() > 2)
					pH->GetParam(3, bLiveOnly);
				int n = pGroup->GetTargetCount(bHostileOnly, bLiveOnly);
				return pH->EndFunction(n);
			}
		}
	}
	return pH->EndFunction(0);
}

/*
   static void	DrawDebugBox(const AABB& aabb, uint8 r, uint8 g, uint8 b, float t)
   {
   Vec3	verts[8];

   verts[0].Set(aabb.min.x, aabb.min.y, aabb.min.z);
   verts[1].Set(aabb.max.x, aabb.min.y, aabb.min.z);
   verts[2].Set(aabb.max.x, aabb.max.y, aabb.min.z);
   verts[3].Set(aabb.min.x, aabb.max.y, aabb.min.z);

   verts[4].Set(aabb.min.x, aabb.min.y, aabb.max.z);
   verts[5].Set(aabb.max.x, aabb.min.y, aabb.max.z);
   verts[6].Set(aabb.max.x, aabb.max.y, aabb.max.z);
   verts[7].Set(aabb.min.x, aabb.max.y, aabb.max.z);

   gEnv->pAISystem->AddDebugLine(verts[0], verts[1], r, g, b, t);
   gEnv->pAISystem->AddDebugLine(verts[1], verts[2], r, g, b, t);
   gEnv->pAISystem->AddDebugLine(verts[2], verts[3], r, g, b, t);
   gEnv->pAISystem->AddDebugLine(verts[3], verts[0], r, g, b, t);

   gEnv->pAISystem->AddDebugLine(verts[4], verts[5], r, g, b, t);
   gEnv->pAISystem->AddDebugLine(verts[5], verts[6], r, g, b, t);
   gEnv->pAISystem->AddDebugLine(verts[6], verts[7], r, g, b, t);
   gEnv->pAISystem->AddDebugLine(verts[7], verts[4], r, g, b, t);

   gEnv->pAISystem->AddDebugLine(verts[0], verts[4], r, g, b, t);
   gEnv->pAISystem->AddDebugLine(verts[1], verts[5], r, g, b, t);
   gEnv->pAISystem->AddDebugLine(verts[2], verts[6], r, g, b, t);
   gEnv->pAISystem->AddDebugLine(verts[3], verts[7], r, g, b, t);
   }
 */
//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::FindObjectOfType(IFunctionHandler* pH)
{
	int type;
	Vec3 searchPos;
	Vec3 returnPos;
	float fRadius;
	int nFlags = 0;
	CScriptVector vReturnPos;
	CScriptVector vReturnDir;
	CScriptVector vInputPos;
	IAIObject* pAI = 0;

	if (pH->GetParamType(1) == svtPointer)
	{
		pH->GetParam(2, fRadius);
		pH->GetParam(3, type);
		if (pH->GetParamCount() > 3)
			pH->GetParam(4, nFlags);
		if (pH->GetParamCount() > 4)
		{
			if (pH->GetParamType(5) == svtObject)
				pH->GetParam(5, vReturnPos);
			else
			{
				AIWarningID("<CScriptBind_AI> ", "FindObjectOfType: wrong position return value passed (not a table)");
				return pH->EndFunction();
			}
		}
		if (pH->GetParamCount() > 5)
			pH->GetParam(6, vReturnDir);

		GET_ENTITY(1);
		if (!pEntity || !pEntity->HasAI())
		{
			AIWarningID("<CScriptBind_AI> ", "FindObjectOfType: The specified entity does not exists or does not have AI.");
			return pH->EndFunction();
		}
		pAI = pEntity->GetAI();
		searchPos = pAI->GetPos();
	}
	else
	{
		if (pH->GetParamType(1) != svtObject)
		{
			AIWarningID("<CScriptBind_AI> ", "FindObjectOfType: wrong search position value passed (not a table)");
			return pH->EndFunction();
		}
		pH->GetParam(1, vInputPos);
		searchPos = vInputPos.Get();
		pH->GetParam(2, fRadius);
		pH->GetParam(3, type);
		if (pH->GetParamCount() > 3)
			pH->GetParam(4, nFlags);
		if (pH->GetParamCount() > 4)
		{
			if (pH->GetParamType(5) == svtObject)
				pH->GetParam(5, vReturnPos);
			else
			{
				AIWarningID("<CScriptBind_AI> ", "FindObjectOfType: wrong position return value passed (not a table)");
				return pH->EndFunction();
			}
		}
		if (pH->GetParamCount() > 5)
			pH->GetParam(6, vReturnDir);
	}

	PREFAST_ASSUME(pAI);

	IAIObject* pFoundObject = 0;

	if (nFlags != 0)
	{
		// [mikko] This is inconsistent with the AISystem version of the FindObjectOfType.
		// The only excuse to do it this way is that there already is a lot of different
		// versions of the find objects of different ways and it is just a big mess anyway.

		bool facingTarget = (nFlags & AIFO_FACING_TARGET) != 0;
		bool nonOccupied = (nFlags & AIFO_NONOCCUPIED) != 0;
		bool chooseRandom = (nFlags & AIFO_CHOOSE_RANDOM) != 0;
		bool nonOccupiedRefPoint = (nFlags & AIFO_NONOCCUPIED_REFPOINT) != 0;
		bool useBeaconAsFallback = (nFlags & AIFO_USE_BEACON_AS_FALLBACK_TGT) != 0;
		bool noDevalue = (nFlags & AIFO_NO_DEVALUE) != 0;

		float nearestDist = FLT_MAX;

		Vec3 targetPos = searchPos;
		bool hasTarget = false;
		IAIActor* pAIActor = CastToIAIActorSafe(pAI);
		if (pAIActor)
		{
			if (IAIObject* pAttTarget = pAIActor->GetAttentionTarget())
			{
				targetPos = pAttTarget->GetPos();
				hasTarget = true;
			}
			else if (useBeaconAsFallback)
			{
				if (IAIObject* pBeacon = GetAISystem()->GetBeacon(pAI->GetGroupId()))
				{
					targetPos = pBeacon->GetPos();
					hasTarget = true;
				}
			}
		}

		// Find puppets that are within the search range.
		std::vector<AABB> avoidBoxes;
		std::vector<IAIObject*> randomObjs;

		if (nonOccupied)
		{
			// Send a signal to all entities that see the specified entity, that that entity have just done something miraculous.
			AutoAIObjectIter it(gAIEnv.pAIObjectManager->GetFirstAIObjectInRange(OBJFILTER_TYPE, AIOBJECT_ACTOR, searchPos, fRadius + 1.0f, false));
			for (; it->GetObject(); it->Next())
			{
				IAIObject* obj = it->GetObject();
				if (obj == pAI) continue;
				if (!obj->IsEnabled()) continue;
				if (!obj->GetEntity()) continue;
				IPipeUser* pCurPipeUser = obj->CastToIPipeUser();
				if (!pCurPipeUser)
					continue;

				AABB bounds;
				obj->GetEntity()->GetWorldBounds(bounds);
				bounds.min -= Vec3(0.1f, 0.1f, 0.1f);
				bounds.max += Vec3(0.1f, 0.1f, 0.1f);

				avoidBoxes.push_back(bounds);
				//				DrawDebugBox(bounds, 255,255,255, 10);

				if (nonOccupiedRefPoint && pCurPipeUser->GetRefPoint() && !pCurPipeUser->GetRefPoint()->GetPos().IsZero(0.001f))
				{
					// This is not very accurate.
					Vec3 size = bounds.GetSize() * 0.5f;
					const Vec3& refPos = pCurPipeUser->GetRefPoint()->GetPos();
					AABB refBounds(refPos - size, refPos + size);
					avoidBoxes.push_back(refBounds);
					//					DrawDebugBox(bounds, 255,255,255, 10);
				}
			}
		}

		AutoAIObjectIter it(gAIEnv.pAIObjectManager->GetFirstAIObjectInRange(OBJFILTER_TYPE, type, searchPos, fRadius, false));
		for (; it->GetObject(); it->Next())
		{
			IAIObject* obj = it->GetObject();
			if (!obj->IsEnabled()) continue;

			const Vec3& objPos = obj->GetPos();
			const Vec3& objDir = obj->GetMoveDir();

			if (nonOccupied)
			{
				// Skip occupied
				bool occupied = false;
				for (std::vector<AABB>::iterator bit = avoidBoxes.begin(); bit != avoidBoxes.end(); ++bit)
				{
					if (bit->IsContainPoint(objPos))
					{
						occupied = true;
						break;
					}
				}
				if (occupied)
				{
					//					GetAISystem()->AddDebugLine(objPos - Vec3(0.5f,0,0), objPos + Vec3(0.5f,0,0), 255, 0, 0, 10 );
					//					GetAISystem()->AddDebugLine(objPos - Vec3(0,0.5f,0), objPos + Vec3(0,0.5f,0), 255, 0, 0, 10 );
					continue;
				}
			}

			float dotToTarget = 1.0f;
			if (facingTarget)
			{
				Vec3 dirToTarget;
				if (hasTarget)
					dirToTarget = targetPos - objPos;
				else
					dirToTarget = objPos - searchPos;

				dirToTarget.NormalizeSafe();
				const float dotTreshold = cosf(DEG2RAD(160.0f));
				dotToTarget = dirToTarget.Dot(objDir);
				if (dotToTarget < dotTreshold)
				{
					Vec3 norm(dirToTarget.y, -dirToTarget.x, 0);
					norm.NormalizeSafe();
					//					GetAISystem()->AddDebugLine(objPos, objPos + dirToTarget, 255, 0, 0, 4 );
					//					GetAISystem()->AddDebugLine(objPos - norm * 0.25f, objPos + norm * 0.25f, 255, 0, 0, 10 );

					continue;
				}
			}

			if (chooseRandom)
			{
				randomObjs.push_back(obj);
			}
			else
			{
				if (facingTarget)
				{
					float d = -dotToTarget;
					if (d < nearestDist)
					{
						pFoundObject = obj;
						nearestDist = d;
					}
				}
				else
				{
					float d = Distance::Point_PointSq(searchPos, objPos);
					if (d < nearestDist)
					{
						pFoundObject = obj;
						nearestDist = d;
					}
				}
			}
		}

		if (chooseRandom && !randomObjs.empty())
		{
			std::random_shuffle(randomObjs.begin(), randomObjs.end());
			pFoundObject = randomObjs[0];
		}

		if (pAIActor && pFoundObject && !noDevalue)
			GetAISystem()->Devalue(pAI, pFoundObject, false);
	}
	else
	{
		if (pAI)
			pFoundObject = GetAISystem()->GetNearestObjectOfTypeInRange(pAI, type, 0, fRadius);
		else
			pFoundObject = GetAISystem()->GetNearestObjectOfTypeInRange(searchPos, type, 0, fRadius);
	}

	if (pFoundObject)
	{
		if (type == 6)
		{
			gEnv->pAISystem->LogComment("<CScriptBind_AI> ", "Entity requested a waypoint anchor! Here is a dump of its state:");
			GetAISystem()->DumpStateOf(pFoundObject);
			return pH->EndFunction();
		}

		if (pH->GetParamCount() > 4)
		{
			//requested AIObject position as return value
			vReturnPos.Set(pFoundObject->GetPos());
		}
		if (pH->GetParamCount() > 5)
		{
			//requested AIObject direction as return value
			vReturnDir.Set(pFoundObject->GetMoveDir());
		}

		return pH->EndFunction(pFoundObject->GetName());
	}

	return pH->EndFunction();
}

int CScriptBind_AI::SoundEvent(IFunctionHandler* pH)
{
	int type = 0;
	float radius = 0;
	Vec3 pos(0, 0, 0);
	ScriptHandle hdl(0);
	
	IEntity* pEntity = GetEntityFromParam(pH, 1);
	IAIObject* pAI = pEntity ? pEntity->GetAI() : nullptr;
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SoundEvent(): wrong type of parameter 1");
		return pH->EndFunction(0);
	}

	if (!pH->GetParam(2, pos) || !pH->GetParam(3, radius) || !pH->GetParam(4, type))
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SoundEvent(): wrong type of parameter 2, 3 or 4");
		return pH->EndFunction(0);
	}

	// can be called from scripts without owner
	if (pH->GetParamCount() > 4)
		pH->GetParam(5, hdl);

	// Send event.
	SAIEVENT event;
	event.vPosition = pos;
	event.fThreat = radius;
	event.nType = type;
	event.nFlags = 0;
	event.sourceId = (EntityId)hdl.n;
	pAI->Event(AIEVENT_ONSOUNDEVENT, &event);

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::VisualEvent(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	IEntity* pEntity = GetEntityFromParam(pH, 1);
	IAIObject* pAI = pEntity ? pEntity->GetAI() : NULL;
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.VisualEvent(): wrong type of parameter 1");
		return pH->EndFunction(0);
	}

	IEntity* pTarget = GetEntityFromParam(pH, 2);
	IAIObject* pTargetAI = pTarget ? pTarget->GetAI() : NULL;
	if (!pTargetAI)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.VisualEvent(): wrong type of parameter 2");
		return pH->EndFunction(0);
	}

	SAIEVENT visualEvent;
	visualEvent.sourceId = pTarget->GetId();
	pAI->Event(AIEVENT_ONVISUALSTIMULUS, &visualEvent);
	return pH->EndFunction(1);
}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetSoundPerceptionDescriptor(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	IEntity* pEntity = GetEntityFromParam(pH, 1);
	IAIObject* pAI = pEntity ? pEntity->GetAI() : NULL;
	IPuppet* pPuppet = CastToIPuppetSafe(pAI);
	if (!pPuppet)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetSoundPerceptionDescriptor(): wrong type of parameter 1");
		return pH->EndFunction(false);
	}

	int iSoundType = AISOUND_LAST;
	if (!pH->GetParam(2, iSoundType) || iSoundType < 0 || iSoundType >= AISOUND_LAST)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetSoundPerceptionDescriptor(): wrong type of parameter 2");
		return pH->EndFunction(false);
	}

	SmartScriptTable pRetTable;
	if (!pH->GetParam(3, pRetTable))
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetSoundPerceptionDescriptor(): wrong type of parameter 3");
		return pH->EndFunction(false);
	}

	SSoundPerceptionDescriptor sDescriptor;
	if (!GetSoundPerceptionDescriptorHelper(pPuppet, iSoundType, sDescriptor))
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetSoundPerceptionDescriptor(): Failed to get sound descriptor for specified type");
		return pH->EndFunction(false);
	}

	pRetTable->SetValue("minDist", sDescriptor.fMinDist);
	pRetTable->SetValue("radiusScale", sDescriptor.fRadiusScale);
	pRetTable->SetValue("soundTime", sDescriptor.fSoundTime);
	pRetTable->SetValue("baseThreat", sDescriptor.fBaseThreat);
	pRetTable->SetValue("linStepMin", sDescriptor.fLinStepMin);
	pRetTable->SetValue("linStepMax", sDescriptor.fLinStepMax);
	return pH->EndFunction(true);
}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::SetSoundPerceptionDescriptor(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	IEntity* pEntity = GetEntityFromParam(pH, 1);
	IAIObject* pAI = pEntity ? pEntity->GetAI() : NULL;
	IPuppet* pPuppet = CastToIPuppetSafe(pAI);
	if (!pPuppet)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetSoundPerceptionDescriptor(): wrong type of parameter 1");
		return pH->EndFunction(false);
	}

	int iSoundType = AISOUND_LAST;
	if (!pH->GetParam(2, iSoundType) || iSoundType < 0 || iSoundType >= AISOUND_LAST)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetSoundPerceptionDescriptor(): wrong type of parameter 2");
		return pH->EndFunction(false);
	}

	SmartScriptTable pInfoTable;
	if (!pH->GetParam(3, pInfoTable))
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetSoundPerceptionDescriptor(): wrong type of parameter 3");
		return pH->EndFunction(false);
	}

	SSoundPerceptionDescriptor sDescriptor;
	if (!GetSoundPerceptionDescriptorHelper(pPuppet, iSoundType, sDescriptor))
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetSoundPerceptionDescriptor(): Failed to get sound descriptor for specified type");
		return pH->EndFunction(false);
	}

	// Set info if it was passed in
	float fTemp = 0.0f;
	if (pInfoTable->GetValue("minDist", fTemp))
		sDescriptor.fMinDist = fTemp;
	if (pInfoTable->GetValue("radiusScale", fTemp))
		sDescriptor.fRadiusScale = fTemp;
	if (pInfoTable->GetValue("soundTime", fTemp))
		sDescriptor.fSoundTime = fTemp;
	if (pInfoTable->GetValue("baseThreat", fTemp))
		sDescriptor.fBaseThreat = fTemp;
	if (pInfoTable->GetValue("linStepMin", fTemp))
		sDescriptor.fLinStepMin = fTemp;
	if (pInfoTable->GetValue("linStepMax", fTemp))
		sDescriptor.fLinStepMax = fTemp;

	CPuppet* pCPuppet = static_cast<CPuppet*>(pPuppet);
	const bool bResult = pCPuppet->SetSoundPerceptionDescriptor((EAISoundStimType)iSoundType, sDescriptor);
	return pH->EndFunction(bResult);
}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetAnchor(IFunctionHandler* pH)
{
	CCCPOINT(CScriptBind_AI_GetAnchor);

	int nAnchor; // Anchor type
	float fRadiusMin = 0;
	float fRadiusMax = 0;
	float fCone = -1.0f;
	bool bFaceAttTarget = false;
	int findType = 0;

	IEntity* pEntity = GetEntityFromParam(pH, 1);
	pH->GetParam(2, nAnchor);

	if (pH->GetParamType(3) == svtNumber)
		pH->GetParam(3, fRadiusMax);
	else if (pH->GetParamType(3) == svtObject)
	{
		SmartScriptTable radiusTable;
		pH->GetParam(3, radiusTable);
		if (radiusTable.GetPtr())
		{
			radiusTable->GetValue("min", fRadiusMin);
			radiusTable->GetValue("max", fRadiusMax);
		}
	}
	if (pH->GetParamCount() > 3)
		pH->GetParam(4, findType);

	if (findType == AIANCHOR_NEAREST_IN_FRONT)
		fCone = 0.8f;
	else if (findType == AIANCHOR_NEAREST_FACING_AT)
		bFaceAttTarget = true;

	//retrieve the entity
	IAIObject* pObject = 0;
	if (pEntity)
	{
		pObject = pEntity->GetAI();

		IAIObject* pRefPoint = 0;
		if (IPipeUser* pPipeUser = CastToIPipeUserSafe(pObject))
			pRefPoint = pPipeUser->GetRefPoint();

		if (pObject)
		{
			IAIObject* pAnchor = 0;
			if (findType == AIANCHOR_RANDOM_IN_RANGE)
				pAnchor = GetAISystem()->GetRandomObjectInRange(pObject, nAnchor, fRadiusMin, fRadiusMax);
			else if (findType == AIANCHOR_RANDOM_IN_RANGE_FACING_AT)
				pAnchor = GetAISystem()->GetRandomObjectInRange(pObject, nAnchor, fRadiusMin, fRadiusMax, true);
			else if (findType == AIANCHOR_BEHIND_IN_RANGE)
				pAnchor = GetAISystem()->GetBehindObjectInRange(pObject, nAnchor, fRadiusMin, fRadiusMax);
			else if (findType == AIANCHOR_NEAREST_TO_REFPOINT && pRefPoint)
				pAnchor = GetAISystem()->GetNearestObjectOfTypeInRange(pObject, nAnchor, fRadiusMin, fRadiusMax, AIFAF_USE_REFPOINT_POS);
			else if (findType == AIANCHOR_LEFT_TO_REFPOINT && pRefPoint)
				pAnchor = GetAISystem()->GetNearestObjectOfTypeInRange(pObject, nAnchor, fRadiusMin, fRadiusMax, AIFAF_LEFT_FROM_REFPOINT);
			else if (findType == AIANCHOR_RIGHT_TO_REFPOINT && pRefPoint)
				pAnchor = GetAISystem()->GetNearestObjectOfTypeInRange(pObject, nAnchor, fRadiusMin, fRadiusMax, AIFAF_RIGHT_FROM_REFPOINT);
			else // AIANCHOR_NEAREST_IN_FRONT or AIANCHOR_NEAREST_IN_HIDE
				pAnchor = GetAISystem()->GetNearestToObjectInRange(pObject, nAnchor, fRadiusMin, fRadiusMax, fCone, bFaceAttTarget);
			if (pAnchor)
				return pH->EndFunction(pAnchor->GetName());
		}
	}
	return pH->EndFunction();
}

int CScriptBind_AI::GetFactionOf(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)entityID.n))
	{
		if (IAIObject* entityObject = entity->GetAI())
		{
			if (const char* factionName = gAIEnv.pFactionMap->GetFactionName(entityObject->GetFactionID()))
				return pH->EndFunction(factionName);
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::SetFactionOf(IFunctionHandler* pH, ScriptHandle entityID, const char* factionName)
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)entityID.n))
	{
		if (IAIObject* entityObject = entity->GetAI())
		{
			uint8 factionID = gAIEnv.pFactionMap->GetFactionID(factionName);

			if ((factionID != IFactionMap::InvalidFactionID) || !*factionName)
				entityObject->SetFactionID(factionID);
			else
				AIWarning("Unknown faction '%s' being set...", factionName);
		}
	}

	return pH->EndFunction();
}

static uint8 GetFactionIDFrom(IFunctionHandler* pH, uint32 paramID)
{
	if (pH->GetParamType(paramID) == svtPointer)
	{
		ScriptHandle entityID;
		pH->GetParam(paramID, entityID);

		if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)entityID.n))
			if (IAIObject* entityObject = entity->GetAI())
				return entityObject->GetFactionID();
			else
				return IFactionMap::InvalidFactionID;
	}
	else if (pH->GetParamType(paramID) == svtString)
	{
		const char* factionName;
		pH->GetParam(2, factionName);

		return gAIEnv.pFactionMap->GetFactionID(factionName);
	}

	AIWarning("Invalid param %d passed to AI.GetReactionOf() - expected entityID or factionName", paramID);

	return IFactionMap::InvalidFactionID;
}

int CScriptBind_AI::GetReactionOf(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	uint8 factionOne = GetFactionIDFrom(pH, 1);
	uint8 factionTwo = GetFactionIDFrom(pH, 2);

	return pH->EndFunction(gAIEnv.pFactionMap->GetReaction(factionOne, factionTwo));
}

int CScriptBind_AI::SetReactionOf(IFunctionHandler* pH, const char* factionOne, const char* factionTwo, int reaction)
{
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetTypeOf(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	IAIObject* pObject = 0;
	if (pEntity)
	{
		pObject = pEntity->GetAI();
		if (pObject)
		{
			return pH->EndFunction(static_cast<CAIObject*>(pObject)->GetType());
		}
	}
	return pH->EndFunction(0);

}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetSubTypeOf(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	IAIObject* pObject = 0;
	if (pEntity)
	{
		pObject = pEntity->GetAI();
		if (pObject)
		{
			int subType = pObject->GetSubType();
			int result = 0;
			if (subType == IAIObject::STP_CAR)
				result = AIOBJECT_CAR;
			if (subType == IAIObject::STP_BOAT)
				result = AIOBJECT_BOAT;
			if (subType == IAIObject::STP_HELI)
				result = AIOBJECT_HELICOPTER;
			if (subType == IAIObject::STP_HELICRYSIS2)
				result = AIOBJECT_HELICOPTERCRYSIS2;
			if (subType == IAIObject::STP_GUNFIRE)
				result = AIOBJECT_GUNFIRE;
			return pH->EndFunction(result);
		}
	}
	return pH->EndFunction(0);

}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetGroupOf(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		if (pAI)
			return pH->EndFunction(pAI->GetGroupId());
	}
	return pH->EndFunction();

}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::Hostile(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 1)
		return pH->EndFunction(false);
	bool bHostile = false;
	GET_ENTITY(1);
	IAIObject* pObject = NULL;
	IAIObject* pAI2 = NULL;
	if (pEntity)
	{
		pObject = pEntity->GetAI();
		if (pObject && pH->GetParamCount() > 1)
		{
			if (pH->GetParamType(2) == svtPointer)
			{
				IEntity* pEntity2 = GetEntityFromParam(pH, 2);
				if (pEntity2)
					pAI2 = pEntity2->GetAI();
			}
			else if (pH->GetParamType(2) == svtObject)
			{
				SmartScriptTable pEntityScript;
				ScriptHandle hdl2;
				if (pH->GetParam(2, pEntityScript) && pEntityScript->GetValue("id", hdl2))
				{
					IEntity* pEntity2 = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(hdl2.n));
					if (pEntity2)
						pAI2 = pEntity2->GetAI();
				}
			}
			else if (pH->GetParamType(2) == svtString)
			{
				const char* sName;
				pH->GetParam(2, sName);
				pAI2 = gAIEnv.pAIObjectManager->GetAIObjectByName(0, sName);
			}

		}
	}

	if (pObject && pAI2)
	{
		bool bUsingAIIgnorePlayer = true;
		if (pH->GetParamCount() > 2)
		{
			if (pH->GetParamType(3) == svtBool)
				pH->GetParam(3, bUsingAIIgnorePlayer);
			else if (pH->GetParamType(3) == svtNumber)
			{
				int i = 1;
				pH->GetParam(3, i);
				bUsingAIIgnorePlayer = i != 0;
			}
		}
		bHostile = pObject->IsHostile(pAI2, bUsingAIIgnorePlayer);
	}

	return pH->EndFunction(bHostile);

}

int CScriptBind_AI::SetRefPointPosition(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();
	CPipeUser* pPipeUser = pAIObject->CastToCPipeUser();
	if (!pPipeUser)
		return pH->EndFunction();

	IAIObject* pAttTarget = pPipeUser->GetAttentionTarget();

	if (pH->GetParamCount() < 2)
	{
		//missing parameter 2, assume the refpoint owner's attention target as a target position
		if (pAttTarget)
		{
			pPipeUser->SetRefPointPos(pAttTarget->GetPos());
			return pH->EndFunction(1);
		}
		return pH->EndFunction();
	}

	ScriptVarType paramType2 = pH->GetParamType(2);

	if (paramType2 == svtNumber)
	{
		int nIDTarget;
		pH->GetParam(2, nIDTarget);
		if (IEntity* pEntity2 = gEnv->pEntitySystem->GetEntity(nIDTarget))
		{
			if (IAIObject* pAIObject2 = pEntity2->GetAI())
			{
				pPipeUser->SetRefPointPos(pAIObject2->GetPos());
				return pH->EndFunction(1);
			}
		}
		return pH->EndFunction();
	}

	if (paramType2 == svtString)
	{
		const char* szTargetName;
		pH->GetParam(2, szTargetName);
		if (!strcmp(szTargetName, "formation"))
		{
			if (IAIObject* pFormationPoint = gAIEnv.pFormationManager->GetFormationPoint(pAIObject))
			{
				pPipeUser->SetRefPointPos(pFormationPoint->GetPos());
				return pH->EndFunction(1);
			}
			return pH->EndFunction();
		}

		if (IAIObject* pAIObject2 = gAIEnv.pAIObjectManager->GetAIObjectByName(0, szTargetName))
		{
			pPipeUser->SetRefPointPos(pAIObject2->GetPos());
			return pH->EndFunction(1);
		}
		return pH->EndFunction();
	}

	if (paramType2 == svtNull)
	{
		if (pAttTarget)
		{
			pPipeUser->SetRefPointPos(pAttTarget->GetPos());
			return pH->EndFunction(1);
		}
		return pH->EndFunction();
	}

	Vec3 vPos;
	if (pH->GetParam(2, vPos))
	{
		pPipeUser->SetRefPointPos(vPos);
		return pH->EndFunction(1);
	}

	return pH->EndFunction();
}

int CScriptBind_AI::SetRefPointDirection(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	//retrieve the entity
	Vec3 vPos(0, 0, 0);
	if (pEntity)
	{
		IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
		if (pPipeUser)
		{
			if (pH->GetParam(2, vPos))
			{
				if (!vPos.IsZero())
				{
					vPos.Normalize();
					pPipeUser->SetRefPointPos(pPipeUser->GetRefPoint()->GetPos(), vPos);
				}
				return pH->EndFunction(1);
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::SetRefPointRadius(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	//retrieve the entity
	float radius(0);
	if (pEntity)
	{
		IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
		if (pPipeUser)
		{
			if (pH->GetParam(2, radius))
			{
				pPipeUser->GetRefPoint()->SetRadius(radius);
				return pH->EndFunction(1);
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetRefPointPosition(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if (pEntity)
	{
		IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
		if (pPipeUser)
		{
			Vec3 vPos = pPipeUser->GetRefPoint()->GetPos();
			return pH->EndFunction(Script::SetCachedVector(vPos, pH, 1));
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetRefPointDirection(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if (pEntity)
	{
		IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
		if (pPipeUser)
		{
			Vec3 vdir = pPipeUser->GetRefPoint()->GetMoveDir();
			return pH->EndFunction(Script::SetCachedVector(vdir, pH, 1));
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetFormationPointPosition(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	SmartScriptTable pTgtPos;
	if (!pH->GetParam(2, pTgtPos))
		return pH->EndFunction();

	if (pEntity && pEntity->GetAI())
	{
		IAIObject* pObject = pEntity->GetAI();
		IAIObject* pFormationPoint = gAIEnv.pFormationManager->GetFormationPoint(pObject);
		if (pFormationPoint)
		{
			Vec3 pos = pFormationPoint->GetPos();
			pTgtPos->SetValue("x", pos.x);
			pTgtPos->SetValue("y", pos.y);
			pTgtPos->SetValue("z", pos.z);
			return pH->EndFunction(true);
		}
	}

	return pH->EndFunction();
}

//====================================================================
// SetRefShapeName
//====================================================================
int CScriptBind_AI::SetRefShapeName(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	const char* shapeName = 0;
	if (!pH->GetParam(2, shapeName))
		return pH->EndFunction();
	if (!pEntity)
		return pH->EndFunction();

	IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (pPipeUser)
		pPipeUser->SetRefShapeName(shapeName);

	return pH->EndFunction();
}

//====================================================================
// GetRefShapeName
//====================================================================
int CScriptBind_AI::GetRefShapeName(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();

	IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (pPipeUser)
		return pH->EndFunction(pPipeUser->GetRefShapeName());

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetVehicleStickTarget(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	ScriptHandle targetId;
	pH->GetParam(2, targetId);

	if (pEntity)
	{
		CPuppet* pPuppet = CastToCPuppetSafe(pEntity->GetAI());
		if (pPuppet)
			pPuppet->SetVehicleStickTarget((EntityId)targetId.n);
	}

	return pH->EndFunction();
}

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetCharacter(IFunctionHandler* pH)
{
	//SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	const char* sCharacterName = 0;
	const char* sBehaviourName = 0;
	pH->GetParam(2, sCharacterName);

	if (pEntity && sCharacterName)
	{
		IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
		if (pPipeUser)
		{
			if (pH->GetParamCount() > 2)
				pH->GetParam(3, sBehaviourName);
			pPipeUser->SetCharacter(sCharacterName, sBehaviourName);
		}
	}

	return pH->EndFunction();
}
#endif

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetForcedNavigation(IFunctionHandler* pH)
{

	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject(pEntity->GetAI());
	if (!pAIObject)
		return pH->EndFunction();

	Vec3 vPos(ZERO);
	if (!pH->GetParam(2, vPos))
		return pH->EndFunction();

	SAIEVENT event;
	event.vForcedNavigation = vPos;

	pAIObject->Event(AIEVENT_FORCEDNAVIGATION, &event);

	return pH->EndFunction();

}

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetAdjustPath(IFunctionHandler* pH)
{

	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject(pEntity->GetAI());
	if (!pAIObject)
		return pH->EndFunction();

	int nType = 0;
	if (!pH->GetParam(2, nType))
		return pH->EndFunction();

	SAIEVENT event;
	event.nType = nType;

	pAIObject->Event(AIEVENT_ADJUSTPATH, &event);

	return pH->EndFunction();

}
//
//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetHeliAdvancePoint(IFunctionHandler* pH)
{

	// First param is the entity that is attacking, and the second param is the target location (script vector3).

	if (pH->GetParamCount() < 5)
	{
		AIWarningID("<CScriptBind_AI> ", "GetAlienApproachParams: Too few parameters.");
		return pH->EndFunction(0);
	}

	GET_ENTITY(1);

	if (!pEntity || !pEntity->GetAI())
	{
		AIWarningID("<CScriptBind_AI> ", "GetAlienApproachParams: No Entity or AI.");
		return pH->EndFunction(0);
	}

	//retrieve the entity
	Vec3 targetPos(0, 0, 0);
	Vec3 targetDir(0, 0, 0);
	SmartScriptTable pNewTgtPos;
	SmartScriptTable pNewTgtDir;
	int type = 0;

	if (!pH->GetParam(2, type))
	{
		AIWarningID("<CScriptBind_AI> ", "GetAlienApproachParams: No type param.");
		return pH->EndFunction(0);
	}
	if (!pH->GetParam(3, targetPos))
	{
		AIWarningID("<CScriptBind_AI> ", "GetAlienApproachParams: No target pos.");
		return pH->EndFunction(0);
	}
	if (!pH->GetParam(4, targetDir))
	{
		AIWarningID("<CScriptBind_AI> ", "GetAlienApproachParams: No target dir.");
		return pH->EndFunction(0);
	}
	if (!pH->GetParam(5, pNewTgtPos))
	{
		AIWarningID("<CScriptBind_AI> ", "GetAlienApproachParams: No out target pos.");
		return pH->EndFunction(0);
	}
	if (!pH->GetParam(6, pNewTgtDir))
	{
		AIWarningID("<CScriptBind_AI> ", "GetAlienApproachParams: No out target dir.");
		return pH->EndFunction(0);
	}

	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (!pAIActor)
		return pH->EndFunction(0);

	Vec3 entPos = pEntity->GetPos();
	Vec3 diff = entPos - targetPos;

	//--------------------------------------------------------------------------------

	Vec3 diff2DN(diff);
	diff2DN.z = 0;
	diff2DN.normalize();

	AgentParameters agentParams = pAIActor->GetParameters();
	float attackHeight = agentParams.m_fAttackZoneHeight;
	float attackRad = agentParams.m_fAttackRange * 0.1f;

	Vec3 desiredPos = targetPos - diff2DN * attackRad + Vec3(0, 0, attackHeight);
	// Returns the calculated target position and direction.
	pNewTgtPos->SetValue("x", desiredPos.x);
	pNewTgtPos->SetValue("y", desiredPos.y);
	pNewTgtPos->SetValue("z", desiredPos.z);

	pNewTgtDir->SetValue("x", diff2DN.x);
	pNewTgtDir->SetValue("y", diff2DN.y);
	pNewTgtDir->SetValue("z", diff2DN.z);

	return pH->EndFunction(1);
}

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetFlyingVehicleFlockingPos(IFunctionHandler* pH)
{

	Vec3 vSumOfPotential(ZERO);

	GET_ENTITY(1);
	if (!pEntity || !pEntity->GetAI())
		return pH->EndFunction(vSumOfPotential);

	if (pH->GetParamCount() != 5)
		return pH->EndFunction(vSumOfPotential);

	float radius = 40.0f;
	float massfilter = 200.0f;
	float sec = 3.0f;
	float checkAbove = 10.0f;
	Vec3 vTarget(ZERO);

	if (!pH->GetParam(2, radius))
		return pH->EndFunction(vSumOfPotential);

	if (!pH->GetParam(3, massfilter))
		return pH->EndFunction(vSumOfPotential);

	if (!pH->GetParam(4, sec))
		return pH->EndFunction(vSumOfPotential);

	if (!pH->GetParam(5, checkAbove))
		return pH->EndFunction(vSumOfPotential);

	//---------------------------------------------------------------------------

	const Vec3 vUp(0.0f, 0.0f, 1.0f);
	const Vec3 vDown(0.0f, 0.0f, -1.0f);

	// get my information
	pe_status_dynamics pMyDynamics;
	IPhysicalEntity* pMyPhycs = pEntity->GetPhysics();
	if (!pMyPhycs)
		return pH->EndFunction(vSumOfPotential);

	pMyPhycs->GetStatus(&pMyDynamics);
	Vec3 vMyVelocity(pMyDynamics.v);
	{
		// at least make a 5m circle in any case
		if (vMyVelocity.GetLength() < 0.01f)
		{
			Matrix33 worldMat(pEntity->GetWorldTM());
			vMyVelocity = worldMat * FORWARD_DIRECTION * 5.0f;
		}
		else if (vMyVelocity.GetLength() < 5.0f)
		{
			vMyVelocity.NormalizeSafe();
			vMyVelocity *= 5.0f;
		}
	}

	Vec3 vMyPos(pEntity->GetWorldPos());
	Vec3 vMyPos2D(vMyPos.x, vMyPos.y, 0.0f);
	Vec3 vMyPosInFuture(vMyPos + vMyVelocity * sec);
	Vec3 vMyPosInFuture2D(vMyPosInFuture.x, vMyPosInFuture.y, 0.0f);
	Vec3 vMyCircleCetner2D(vMyPos2D);
	Vec3 vR2(vMyPosInFuture2D - vMyCircleCetner2D);
	float r2 = vR2.GetLength();

	Vec3 vMyVelocity2D(vMyVelocity.x, vMyVelocity.y, 0.0f);
	Vec3 vMyVelocity2DUnit(vMyVelocity2D.GetNormalizedSafe());
	Vec3 vMyVelocity2DUnitWng((vMyVelocity2DUnit.Cross(vUp)).GetNormalizedSafe());

	for (float deg = DEG2RAD(0.0f); deg < 3.1416f * 2.0f; deg += DEG2RAD(5.0f))
	{
		float degSrc = deg;
		float degDst = deg + DEG2RAD(5.0f);

		Vec3 debugVecSrc;
		Vec3 debugVecDst;

		debugVecSrc.x = cosf(degSrc) * r2;
		debugVecSrc.y = sinf(degSrc) * r2;
		debugVecSrc.z = 0.0f;

		debugVecDst.x = cosf(degDst) * r2;
		debugVecDst.y = sinf(degDst) * r2;
		debugVecDst.z = 0.0f;

		debugVecSrc += vMyPos;
		debugVecDst += vMyPos;

		gEnv->pAISystem->AddDebugLine(debugVecSrc, debugVecDst, 255, 255, 255, 1.0f);

	}

	//---------------------------------------------------------------------------
	// get enemy information
	float entityRadius = radius + 10.0f;
	const Vec3 bboxsize(entityRadius, entityRadius, entityRadius);

	IPhysicalEntity** pEntityList;
	int nCount = gAIEnv.pWorld->GetEntitiesInBox(vMyPos - bboxsize, vMyPos + bboxsize, pEntityList, ent_living | ent_rigid | ent_sleeping_rigid);

	float minLen = radius * 20.0f;

	for (int i = 0; i < nCount; ++i)
	{

		IEntity* pEntityAround = gEnv->pEntitySystem->GetEntityFromPhysics(pEntityList[i]);

		if (!pEntityAround)                 // just in case
			continue;

		if (pEntityAround->GetId() == pEntity->GetId())   // skip myself
			continue;

		pe_status_dynamics pdynamics;           // if it has a small mass. guess can cllide.
		pEntityList[i]->GetStatus(&pdynamics);  // falling humans/ small aliens etc
		if (pdynamics.mass < massfilter)
			continue;

		Vec3 vHisVelocity(pdynamics.v);
		{
			if (vHisVelocity.GetLength() < 0.01f)
			{
				Matrix33 worldMat(pEntityAround->GetWorldTM());
				vHisVelocity = worldMat * FORWARD_DIRECTION * 5.0f;
			}
			else if (vHisVelocity.GetLength() < 5.0f)
			{
				vHisVelocity.NormalizeSafe();
				vHisVelocity *= 5.0f;
			}
		}

		Vec3 vHisVelocity2D(vHisVelocity.x, vHisVelocity.y, 0.0f);
		Vec3 vHisVelocity2DUnit(vHisVelocity2D.GetNormalizedSafe());
		Vec3 vHisVelocity2DUnitWng((vHisVelocity2DUnit.Cross(vUp)).GetNormalizedSafe());

		Vec3 vHisPos(pEntityAround->GetWorldPos());
		Vec3 vHisPos2D(vHisPos.x, vHisPos.y, 0.0f);
		Vec3 vHisPosInFuture(vHisPos + vHisVelocity * sec);
		Vec3 vHisPosInFuture2D(vHisPosInFuture.x, vHisPosInFuture.y, 0.0f);
		Vec3 vHisCircleCetner2D(vHisPos2D);
		Vec3 vR1(vHisPosInFuture2D - vHisCircleCetner2D);

		float r1 = vR1.GetLength();

		float distance2d = (vHisPos2D - vMyPos2D).GetLength();
		if (distance2d > r1 + r2)
			continue;

		if (distance2d > minLen)
			continue;

		float height = vHisPos.z - gEnv->p3DEngine->GetTerrainElevation(vHisPos.x, vHisPos.y);
		if (height < checkAbove)                // only think flying object
			continue;

		float heightAboveWater = vHisPos.z - gEnv->p3DEngine->GetWaterLevel(&vHisPos);
		if (heightAboveWater < checkAbove)          // only think flying object
			continue;

		if (vHisVelocity.GetLength() < 0.1f)          // will write other implement for this later
			continue;

		Vec3 vUnitDir2D(vHisPos2D - vMyPos2D);
		{
			vUnitDir2D.z = 0.0f;
			if (vUnitDir2D.GetLength() < 0.1f)
				continue;                 // too close to calculate
		}

		// condition : only check the object in FOV
		Vec3 vUnitDir2DUnit(vUnitDir2D.GetNormalizedSafe());

		if (vUnitDir2DUnit.Dot(vMyVelocity2DUnit) < 0.0f)
			continue;                   // he is not in FOV of me

		// calculate intersections between 2 circle

		float a = vHisCircleCetner2D.x - vMyCircleCetner2D.x;
		float b = vHisCircleCetner2D.y - vMyCircleCetner2D.y;

		if (fabsf(a) < 0.0001f)
			continue;                   // ideally need swap(a,b), but just continue for easiness

		if (fabsf(b) < 0.0001f)
			continue;                   // ideally need swap(a,b), but just continue for easiness

		float t = -1.0f * b / a;
		float ss = (a * a + b * b - r1 * r1 + r2 * r2);
		float s = ss / (2.0f * a);
		float k = t * t + 1.0f;
		float l = 2.0f * s * t;
		float m = s * s - r2 * r2;
		float det = l * l - 4.0f * k * m;

		if (det < 0.0f)
			continue;

		float y1 = (-1.0f * l + sqrtf(det)) / (2.0f * k);
		float y2 = (-1.0f * l - sqrtf(det)) / (2.0f * k);
		float x1 = t * y1 + s;
		float x2 = t * y2 + s;

		Vec3 evadeVector2D_1(x1, y1, 0.0f);
		Vec3 evadeVector2D_2(x2, y2, 0.0f);
		Vec3 evadeVectorUnit2D_1(evadeVector2D_1.GetNormalizedSafe());
		Vec3 evadeVectorUnit2D_2(evadeVector2D_2.GetNormalizedSafe());

		// condition : current speed vector sould be in the corn which is made by these 2 evadeVector2D

		if (sgn(vMyVelocity2DUnitWng.Dot(evadeVectorUnit2D_1))
		    * sgn(vMyVelocity2DUnitWng.Dot(evadeVectorUnit2D_2)) > 0.0f)
			continue;

		// condition : choose a vector which is more pararrel each other

		float dot1 = vHisVelocity2DUnitWng.Dot(evadeVectorUnit2D_1);
		float dot2 = vHisVelocity2DUnitWng.Dot(evadeVectorUnit2D_2);

		if (dot1 > dot2)
		{
			vSumOfPotential = evadeVector2D_1;
		}
		else
		{
			vSumOfPotential = evadeVector2D_2;
		}

		{

			Vec3 debugVec;
			Vec3 debugVec2;

			debugVec = evadeVector2D_1 + vMyPos2D;
			debugVec.z = vMyPos.z;
			debugVec2 = evadeVector2D_2 + vMyPos2D;
			debugVec2.z = vMyPos.z;
			if (dot1 > dot2)
			{
				gEnv->pAISystem->AddDebugLine(vMyPos, debugVec, 0, 255, 0, 1.0f);
				gEnv->pAISystem->AddDebugLine(vMyPos, debugVec2, 255, 255, 255, 1.0f);
			}
			else
			{
				gEnv->pAISystem->AddDebugLine(vMyPos, debugVec, 255, 255, 255, 1.0f);
				gEnv->pAISystem->AddDebugLine(vMyPos, debugVec2, 0, 255, 0, 1.0f);
			}

		}
		minLen = distance2d;

	}

	return pH->EndFunction(vSumOfPotential);

}

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::CheckVehicleColision(IFunctionHandler* pH)
{
	Vec3 vHitPos(ZERO);

	if (pH->GetParamCount() != 4)
	{
		AIWarningID("<CScriptBind_AI> ", "CCheckVehicleColision: suppose 4 arguments");
		return pH->EndFunction(vHitPos);
	}

	GET_ENTITY(1);

	IPhysicalEntity* pPhysicalEntity = pEntity->GetPhysics();
	if (!pEntity || !pPhysicalEntity || !pEntity->GetAI())
		return pH->EndFunction(vHitPos);

	Vec3 vPos, vFwd;
	float radius;

	if (!pH->GetParam(2, vPos) || !pH->GetParam(3, vFwd) || !pH->GetParam(4, radius))
	{
		AIWarningID("<CScriptBind_AI> ", "CheckVehicleColision: No type param.");
		return pH->EndFunction(vHitPos);
	}

	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;

	primitives::sphere spherePrim;
	spherePrim.center = vPos;
	spherePrim.r = radius;

	geom_contact* pContact = 0;
	geom_contact** ppContact = &pContact;

	int geomFlagsAny = geom_colltype0;

	float d = pPhysics->PrimitiveWorldIntersection(primitives::sphere::type, &spherePrim, vFwd,
	                                               ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid | ent_ignore_noncolliding, ppContact,
	                                               0, geomFlagsAny, 0, 0, 0, &pPhysicalEntity, 1);

	if (d > 0.0f)
	{
		PREFAST_ASSUME(pContact);
		vHitPos = pContact->pt;
		gEnv->pAISystem->AddDebugLine(pEntity->GetPos(), vHitPos, 0, 255, 255, 1.0f);
	}
	else
	{
		gEnv->pAISystem->AddDebugLine(pEntity->GetPos(), pEntity->GetPos() + vFwd, 0, 0, 255, 1.0f);
	}

	return pH->EndFunction(vHitPos);

}
//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IntersectsForbidden(IFunctionHandler* pH)
{

	Vec3 vStart, vEnd, vResult;

	if (!pH->GetParam(1, vStart))
		return pH->EndFunction(0);
	if (!pH->GetParam(2, vEnd))
		return pH->EndFunction(0);

	// "forbidden areas" are no longer used in MNM, so, given path will never intersect
	return pH->EndFunction(vEnd);

}

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IsPointInFlightRegion(IFunctionHandler* pH)
{

	Vec3 vStart;
	int nBuildingID;

	if (!pH->GetParam(1, vStart))
		return pH->EndFunction(0);

	return pH->EndFunction(IAISystem::NAV_UNSET != gAIEnv.pNavigation->CheckNavigationType(vStart, nBuildingID, IAISystem::NAV_FLIGHT));

}

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IsPointInWaterRegion(IFunctionHandler* pH)
{

	Vec3 vStart;

	if (!pH->GetParam(1, vStart))
		return pH->EndFunction(0);

	float level = gEnv->p3DEngine->GetWaterLevel(&vStart) - gEnv->p3DEngine->GetTerrainElevation(vStart.x, vStart.y);

	return pH->EndFunction(level);

}

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetEnclosingSpace(IFunctionHandler* pH)
{
	// This function tests the approximate space around certain point in space.
	// The test is done by shootin 4 or 6 rays depending on the current navigation type of the specified entity.
	//

	// Get parameters. The expected parameters are entity.id, check position, and check radius.
	if (pH->GetParamCount() < 3)
	{
		AIWarningID("<CScriptBind_AI> ", "GetNavigableSpaceSize: Too few parameters.");
		return pH->EndFunction(0);
	}

	Vec3 pos(0, 0, 0);
	float rad = 0;

	int checkType = CHECKTYPE_MIN_DISTANCE;

	GET_ENTITY(1);
	if (!pEntity)
	{
		AIWarningID("<CScriptBind_AI> ", "GetNavigableSpaceSize: No Entity or AI.");
		return pH->EndFunction(0);
	}
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (!pAIActor)
	{
		AIWarningID("<CScriptBind_AI> ", "GetNavigableSpaceSize: No Entity or AI.");
		return pH->EndFunction(0);
	}

	if (!pH->GetParam(2, pos))
	{
		AIWarningID("<CScriptBind_AI> ", "GetNavigableSpaceSize: No pos.");
		return pH->EndFunction(0);
	}
	if (!pH->GetParam(3, rad))
	{
		AIWarningID("<CScriptBind_AI> ", "GetNavigableSpaceSize: No radius.");
		return pH->EndFunction(0);
	}
	if (pH->GetParamCount() > 3)
		pH->GetParam(4, checkType);

	// Calculate approximate distance to close by geometry.
	const Vec3 tests[6] =
	{
		Vec3(1,  0,  0),
		Vec3(-1, 0,  0),
		Vec3(0,  1,  0),
		Vec3(0,  -1, 0),
		Vec3(0,  0,  1),
		Vec3(0,  0,  -1),
	};

	/*	float	avgDist = 0;
	   float	weight = 0;*/
	float minDist = rad;

	unsigned numTests = 6;

	// The down test is not done when in 2D navigation area.
	int nBuilding;
	IAISystem::ENavigationType type = gAIEnv.pNavigation->CheckNavigationType(pos, nBuilding, pAIActor->GetMovementAbility().pathfindingProperties.navCapMask);
	if ((type & (IAISystem::NAV_VOLUME | IAISystem::NAV_WAYPOINT_3DSURFACE)) == 0)
		numTests = 4;

	int numIntermediateTests = (checkType == CHECKTYPE_MIN_ROOMSIZE ? 2 : 1);
	// Do the ray tests. The purpose of the weighting is to bias the clamped values.
	for (unsigned i = 0; i < numTests; )
	{
		ray_hit hit;
		float totalDist = 0;
		for (int k = 0; k < numIntermediateTests; k++)
		{
			float d = rad;
			if (RayWorldIntersectionWrapper(pos, tests[i] * rad, ent_static, rwi_stop_at_pierceable, &hit, 1))
				d = hit.dist;
			totalDist += d;
			i++;
		}
		/*		float	w = 1.0f - (d / (rad * 2.0f));
		   avgDist += d * w;
		   weight += w;*/
		if (totalDist < minDist)
			minDist = totalDist;
	}
	//	if( weight > 0.0001f )
	//		avgDist /= weight;
	//	return pH->EndFunction( avgDist );
	return pH->EndFunction(minDist);
}

int CScriptBind_AI::GetAIObjectPosition(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	const char* objName = "";
	if (pH->GetParamType(1) == svtPointer)
	{
		GET_ENTITY(1);
		if (pEntity)
		{
			Vec3 vPos = pEntity->GetPos();
			return pH->EndFunction(vPos);
		}

	}
	else if (pH->GetParamType(1) == svtString)
	{
		pH->GetParam(1, objName);
		IAIObject* pAIObjectTarget = (IAIObject*) gAIEnv.pAIObjectManager->GetAIObjectByName(0, objName);
		if (pAIObjectTarget)
		{
			return pH->EndFunction(pAIObjectTarget->GetPos());
		}
	}
	return pH->EndFunction();
}

int CScriptBind_AI::GetBeaconPosition(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	SmartScriptTable pBeaconPos;
	if (!pH->GetParam(2, pBeaconPos))
		return pH->EndFunction(false);

	IAIObject* pAI = NULL;
	if (pH->GetParamType(1) == svtPointer)
	{
		//retrieve the entity
		IEntity* pEntity = GetEntityFromParam(pH, 1);
		if (pEntity)
			pAI = pEntity->GetAI();
	}
	else if (pH->GetParamType(1) == svtString)
	{
		const char* objName = "";
		pH->GetParam(1, objName);
		pAI = gAIEnv.pAIObjectManager->GetAIObjectByName(0, objName);
	}

	assert(pAI);
	PREFAST_ASSUME(pAI);

	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (pAIActor)
	{
		IAIObject* pBeacon = GetAISystem()->GetBeacon(pAI->GetGroupId());
		if (pBeacon)
		{
			Vec3 pos = pBeacon->GetPos();
			pBeaconPos->SetValue("x", pos.x);
			pBeaconPos->SetValue("y", pos.y);
			pBeaconPos->SetValue("z", pos.z);
			return pH->EndFunction(true);
		}
	}
	return pH->EndFunction(false);
}

int CScriptBind_AI::SetBeaconPosition(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	Vec3 vPos;
	if (!pH->GetParam(2, vPos))
		return pH->EndFunction();
	IAIObject* pAI = NULL;
	if (pH->GetParamType(1) == svtPointer)
	{
		//retrieve the entity
		IEntity* pEntity = GetEntityFromParam(pH, 1);
		if (pEntity)
			pAI = pEntity->GetAI();
	}
	else if (pH->GetParamType(1) == svtString)
	{
		const char* objName = "";
		pH->GetParam(1, objName);
		pAI = gAIEnv.pAIObjectManager->GetAIObjectByName(0, objName);
	}

	assert(pAI);
	PREFAST_ASSUME(pAI);

	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (pAIActor)
		GetAISystem()->UpdateBeacon(pAI->GetGroupId(), vPos, pAI);

	return pH->EndFunction();
}

struct PairDistanceObject
{
	float      fDistance;
	IAIObject* pObject;
};
bool operator<(const PairDistanceObject& left, const PairDistanceObject& right)
{
	return (left.fDistance < right.fDistance);
}

int CScriptBind_AI::GetNearestEntitiesOfType(IFunctionHandler* pH)
{
	SmartScriptTable pAIObjectScriptList;
	int iObjectType;
	int iNumObjects;
	int iFilter = 0;
	float fRadius = 30;
	IAIObject* pAIObject = NULL;
	Vec3 vPos;

	if (pH->GetParamCount() < 4)
		return pH->EndFunction();

	pH->GetParam(2, iObjectType);
	pH->GetParam(3, iNumObjects);
	pH->GetParam(4, pAIObjectScriptList);

	if (pH->GetParamCount() > 4)
		pH->GetParam(5, iFilter);

	if (pH->GetParamCount() > 5)
		pH->GetParam(6, fRadius);

	if (pH->GetParamType(1) == svtPointer)
	{
		GET_ENTITY(1);
		if (!pEntity)
			return pH->EndFunction(0);
		pAIObject = pEntity->GetAI();
		vPos = pAIObject->GetPos();
	}
	else if (pH->GetParamType(1) == svtString)
	{
		const char* sObjName;
		pH->GetParam(1, sObjName);
		pAIObject = (IAIObject*) gAIEnv.pAIObjectManager->GetAIObjectByName(0, sObjName);
		if (!pAIObject)
			return pH->EndFunction(0);
		vPos = pAIObject->GetPos();
	}
	else if (pH->GetParamType(1) == svtObject)
	{
		pH->GetParam(1, vPos);
		//		iFilter = 0; // can't have a species or a group if it's not relative to an AIObject
	}
	else // unrecognized 1st parameter
		return pH->EndFunction(0);

	if (!pAIObject)
		return pH->EndFunction(0);

	float fRadiusSQR = fRadius * fRadius;
	int iObjectsfound = 0;

	typedef std::multimap<float, IAIObject*> AIObjectList_t;
	AIObjectList_t AIObjectList;

	for (AutoAIObjectIter it(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_TYPE, iObjectType)); it->GetObject(); it->Next())
	{
		IAIObject* pObjectIt = it->GetObject();
		PREFAST_ASSUME(pObjectIt);
		if (pObjectIt == pAIObject || !pObjectIt->GetEntity())  // skip AIObjects without an entity associated
			continue;

		if (!(iFilter & AIOBJECTFILTER_INCLUDEINACTIVE))
		{
			if (!pObjectIt->IsEnabled())
				continue;
		}

		if (iFilter)
		{
			if ((iFilter & AIOBJECTFILTER_SAMEFACTION) && pAIObject->GetFactionID() != pObjectIt->GetFactionID())
				continue;
		}

		int objectGroupId = pObjectIt->GetGroupId();
		if ((iFilter & AIOBJECTFILTER_SAMEGROUP) && objectGroupId != AI_NOGROUP &&
		    pAIObject->GetGroupId() != objectGroupId)
			continue;

		if ((iFilter & AIOBJECTFILTER_NOGROUP) && objectGroupId != AI_NOGROUP)
			continue;

		Vec3 vObjectPos = pObjectIt->GetPos();
		float ds = (vObjectPos - vPos).len2();
		//float fActivationRadius = (ai->second)->GetRadius();
		//fActivationRadius*=fActivationRadius;
		if (ds < fRadiusSQR)
		{
			/*			if (pPuppet->m_mapDevaluedPoints.find(pObjectIt) == pPuppet->m_mapDevaluedPoints.end())
			   {
			   if ((fActivationRadius>0) && (ds>fActivationRadius))
			   continue;
			   }
			 */
			iObjectsfound++;
			AIObjectList.insert(std::make_pair(ds, pObjectIt));
			//for(int i=1; i< iObjectsfound; i++)

		}
	}

	AIObjectList_t::iterator iOL = AIObjectList.begin();

	int i;
	for (i = 1; i <= iObjectsfound && i <= iNumObjects && iOL != AIObjectList.end(); i++)
	{
		IEntity* pEntity = (*iOL).second->GetEntity();
		if (pEntity)
			pAIObjectScriptList->SetAt(i, pEntity->GetScriptTable());
		++iOL;
	}
	iObjectsfound = i - 1; //used as a return value

	for (; i <= iNumObjects; i++)
		pAIObjectScriptList->SetNullAt(i);

	return pH->EndFunction(iObjectsfound);

}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::Event(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	const char* szEventText;
	int event;
	pH->GetParams(event, szEventText);

	GetAISystem()->Event(event, szEventText);
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetAttentionTargetOf(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();

	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (pAIActor)
	{
		IAIObject* pTarget = pAIActor->GetAttentionTarget();
		if (pTarget)
			return pH->EndFunction(pTarget->GetName());
	}
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetAttentionTargetAIType(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction(AIOBJECT_NONE);

	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (pAIActor)
	{
		IAIObject* pTarget = pAIActor->GetAttentionTarget();
		if (pTarget)
			return pH->EndFunction(static_cast<CAIObject*>(pTarget)->GetType());
	}
	return pH->EndFunction(AIOBJECT_NONE);
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetAttentionTargetType(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction(AITARGET_NONE);

	CAIActor* pAIActor = CastToCAIActorSafe(pEntity->GetAI());
	return pH->EndFunction(pAIActor ? pAIActor->GetAttentionTargetType() : AITARGET_NONE);
}

//
//-----------------------------------------------------------------------------------------------------------

CAIActor* GetAIActor(EntityId entityID)
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity(entityID))
	{
		if (IAIObject* aiObject = entity->GetAI())
		{
			return aiObject->CastToCAIActor();
		}
	}

	return NULL;
}

int CScriptBind_AI::GetAttentionTargetEntity(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (CAIActor* aiActor = GetAIActor(static_cast<EntityId>(entityID.n)))
	{
		if (CAIObject* target = static_cast<CAIObject*>(aiActor->GetAttentionTarget()))
		{
			// When the attention target is a memory or a sound the ai object
			// itself is not the real target, but rather a unique ai object
			// holding the position of the memory or sound.
			if (target->GetAIType() == AIOBJECT_DUMMY)
			{
				// Extract the real target behind the memory or sound.
				target = target->GetAssociation().GetAIObject();
			}

			if (IEntity* targetEntity = target ? target->GetEntity() : NULL)
			{
				return pH->EndFunction(targetEntity->GetScriptTable());
			}
		}
	}

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetAttentionTargetPosition(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	SmartScriptTable pTgtPos;
	if (!pH->GetParam(2, pTgtPos))
		return pH->EndFunction();

	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (pAIActor)
	{
		IAIObject* pTarget = pAIActor->GetAttentionTarget();
		if (pTarget)
		{
			Vec3 pos = pTarget->GetPos();
			pTgtPos->SetValue("x", pos.x);
			pTgtPos->SetValue("y", pos.y);
			pTgtPos->SetValue("z", pos.z);
			return pH->EndFunction(true);
		}
	}
	return pH->EndFunction(false);
}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetAttentionTargetDistance(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();

	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (pAIActor)
	{
		IAIObject* pTarget = pAIActor->GetAttentionTarget();
		if (pTarget)
		{
			return pH->EndFunction((pTarget->GetPos() - pEntity->GetPos()).GetLength());
		}
	}
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetAttentionTargetDirection(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	SmartScriptTable pTgtPos;
	if (!pH->GetParam(2, pTgtPos))
		return pH->EndFunction();

	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (pAIActor)
	{
		IAIObject* pTarget = pAIActor->GetAttentionTarget();
		if (pTarget)
		{
			Vec3 dir = pTarget->GetMoveDir();
			dir.NormalizeSafe();
			pTgtPos->SetValue("x", dir.x);
			pTgtPos->SetValue("y", dir.y);
			pTgtPos->SetValue("z", dir.z);
		}
	}
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetAttentionTargetViewDirection(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	SmartScriptTable pTgtPos;
	if (!pH->GetParam(2, pTgtPos))
		return pH->EndFunction();

	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (pAIActor)
	{
		IAIObject* pTarget = pAIActor->GetAttentionTarget();
		if (pTarget)
		{
			Vec3 dir = pTarget->GetViewDir();
			dir.NormalizeSafe();
			pTgtPos->SetValue("x", dir.x);
			pTgtPos->SetValue("y", dir.y);
			pTgtPos->SetValue("z", dir.z);
		}
	}
	return pH->EndFunction();
}

int CScriptBind_AI::GetAttentionTargetThreat(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityID.n)))
	{
		if (IAIObject* pAI = pEntity->GetAI())
		{
			if (IAIActor* pAIActor = CastToIAIActorSafe(pAI))
			{
				return pH->EndFunction(pAIActor->GetAttentionTargetThreat());
			}
		}
	}

	return pH->EndFunction(AITHREAT_NONE);
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetPotentialTargetCountFromFaction(IFunctionHandler* pH, ScriptHandle entityID, const char* factionName)
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)entityID.n))
	{
		if (IAIObject* entityObject = entity->GetAI())
		{
			if (gAIEnv.pFactionMap->GetFactionID(factionName) != IFactionMap::InvalidFactionID)
			{
				int count = gAIEnv.pTargetTrackManager->GetPotentialTargetCountFromFaction(entityObject->GetAIObjectID(), factionName);
				return pH->EndFunction(count);
			}
		}
	}

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetPotentialTargetCount(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)entityID.n))
	{
		if (IAIObject* entityObject = entity->GetAI())
		{
			int count = gAIEnv.pTargetTrackManager->GetPotentialTargetCount(entityObject->GetAIObjectID());
			return pH->EndFunction(count);
		}
	}

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::CreateFormation(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 1)
		return pH->EndFunction();

	const char* name;

	if (pH->GetParam(1, name))
	{
		FormationNode nodeDescr;
		gAIEnv.pFormationManager->CreateFormationDescriptor(name);
		if (pH->GetParamCount() > 1)
			pH->GetParam(2, nodeDescr.eClass);
		// create the owner's node at 0,0,0
		gAIEnv.pFormationManager->AddFormationPoint(name, nodeDescr);
	}
	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AddFormationPointFixed(IFunctionHandler* pH)
{
	AIWarningID("<CScriptBind_AI> ", "AddFormationPointFixed: this function is deprecated as it doesn't allow to specify a follow-distance (a follow distance is be required to smoothly follow the formation) - use AddFormationPoint() instead");

	FormationNode nodeDescr;
	float fSight = 0;
	const char* name;
	if (pH->GetParamCount() < 5)
		return pH->EndFunction();
	if (!pH->GetParams(name, fSight, nodeDescr.vOffset.x, nodeDescr.vOffset.y, nodeDescr.vOffset.z))
		return pH->EndFunction();
	if (pH->GetParamCount() > 5)
		pH->GetParam(6, nodeDescr.eClass);

	Matrix33 m = Matrix33::CreateRotationXYZ(DEG2RAD(Ang3(0, 0, fSight)));
	nodeDescr.vSightDirection = m * Vec3(0, 60, 0); //put the sight target far away in order to not make change
	// head orientation too much while approaching

	gAIEnv.pFormationManager->AddFormationPoint(name, nodeDescr);

	return pH->EndFunction();

}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AddFormationPoint(IFunctionHandler* pH)
{
	FormationNode nodeDescr;
	float fSight;
	const char* name;

	if (pH->GetParamCount() < 4)
		return pH->EndFunction();
	if (!pH->GetParams(name, fSight, nodeDescr.fFollowDistance, nodeDescr.fFollowOffset))
		return pH->EndFunction();
	if (pH->GetParamCount() > 4)
		pH->GetParam(5, nodeDescr.eClass);
	if (pH->GetParamCount() > 5)
	{
		pH->GetParam(6, nodeDescr.fFollowDistanceAlternate);
		pH->GetParam(7, nodeDescr.fFollowOffsetAlternate);
	}
	if (nodeDescr.fFollowDistanceAlternate == 0)
	{
		nodeDescr.fFollowDistanceAlternate = nodeDescr.fFollowDistance;
		nodeDescr.fFollowOffsetAlternate = nodeDescr.fFollowOffset;
	}

	if (nodeDescr.fFollowDistance < FLT_EPSILON)
	{
		AIWarningID("<CScriptBind_AI> ", "AddFormationPoint: formation '%s' has a point with a follow distance of %f (should be > 0.0 or else smooth formation following will not work)", name, nodeDescr.fFollowDistance);
	}

	Matrix33 m = Matrix33::CreateRotationXYZ(DEG2RAD(Ang3(0, 0, fSight)));
	nodeDescr.vSightDirection = m * Vec3(0, 60, 0); //put the sight target far away in order to not make change
	// head orientation too much while approaching

	gAIEnv.pFormationManager->AddFormationPoint(name, nodeDescr);

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetFormationPointClass(IFunctionHandler* pH)
{
	FormationNode nodeDescr;
	int pos;
	const char* name;
	SCRIPT_CHECK_PARAMETERS(2);
	if (!pH->GetParams(name, pos))
		return pH->EndFunction(-1);

	return pH->EndFunction(gAIEnv.pFormationManager->GetFormationPointClass(name, pos));

}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetLeader(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	IAIObject* pAILeader = NULL;
	if (pH->GetParamType(1) == svtNumber)
	{
		int iGroupId;
		pH->GetParam(1, iGroupId);
		pAILeader = GetAISystem()->GetLeaderAIObject(iGroupId);
	}
	else if (pH->GetParamType(1) == svtPointer)
	{
		GET_ENTITY(1);
		if (pEntity)
		{
			IAIObject* pObject = pEntity->GetAI();
			if (pObject)
				pAILeader = GetAISystem()->GetLeaderAIObject(pObject);
		}
	}
	if (pAILeader)
	{
		IEntity* pLeaderEntity = pAILeader->GetEntity();
		if (pLeaderEntity)
			return pH->EndFunction(pLeaderEntity->GetScriptTable());
	}
	return pH->EndFunction();
}

int CScriptBind_AI::UpTargetPriority(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	if (!pEntity || !pEntity->GetAI())
		return pH->EndFunction();

	IEntity* pOtherEntity = GetEntityFromParam(pH, 2);
	if (!pOtherEntity || !pOtherEntity->GetAI())
		return pH->EndFunction();
	float increment = 0;
	pH->GetParam(3, increment);
	IPuppet* pPuppet = CastToIPuppetSafe(pEntity->GetAI());
	if (pPuppet)
	{
		pPuppet->UpTargetPriority(pOtherEntity->GetAI(), increment);
	}
	return pH->EndFunction();
}

int CScriptBind_AI::DropTarget(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity || !pEntity->GetAI())
		return pH->EndFunction();

	IEntity* pOtherEntity = GetEntityFromParam(pH, 2);
	if (!pOtherEntity || !pOtherEntity->GetAI())
		return pH->EndFunction();

	CPuppet* pPuppet = CastToCPuppetSafe(pEntity->GetAI());
	if (pPuppet)
	{
		pPuppet->DropTarget(pOtherEntity->GetAI());
	}
	return pH->EndFunction();
}

int CScriptBind_AI::ClearPotentialTargets(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	if (pEntity && pEntity->GetAI())
	{
		CPuppet* pPuppet = CastToCPuppetSafe(pEntity->GetAI());
		if (pPuppet)
		{
			pPuppet->ClearPotentialTargets();
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::SetTempTargetPriority(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	bool bResult = false;
	if (pEntity && pEntity->GetAI())
	{
		CPuppet* pPuppet = CastToCPuppetSafe(pEntity->GetAI());
		if (pPuppet)
		{
			int iPriority = eTTP_OverSounds;
			pH->GetParam(2, iPriority);
			bResult = pPuppet->SetTempTargetPriority((ETempTargetPriority)iPriority);
		}
	}

	return pH->EndFunction(bResult);
}

int CScriptBind_AI::AddAggressiveTarget(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	bool bResult = false;
	if (pEntity && pEntity->GetAI())
	{
		CPuppet* pPuppet = CastToCPuppetSafe(pEntity->GetAI());
		if (pPuppet)
		{
			IEntity* pOtherEntity = GetEntityFromParam(pH, 2);
			IAIObject* pOtherAI = pOtherEntity ? pOtherEntity->GetAI() : NULL;
			if (pOtherAI)
			{
				bResult = pPuppet->AddAggressiveTarget(pOtherAI);
			}
		}
	}

	return pH->EndFunction(bResult);
}

int CScriptBind_AI::UpdateTempTarget(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	bool bResult = false;
	if (pEntity && pEntity->GetAI())
	{
		CPuppet* pPuppet = CastToCPuppetSafe(pEntity->GetAI());
		if (pPuppet)
		{
			Vec3 vPos;
			pH->GetParam(2, vPos);
			bResult = pPuppet->UpdateTempTarget(vPos);
		}
	}

	return pH->EndFunction(bResult);
}

int CScriptBind_AI::ClearTempTarget(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	bool bResult = false;
	if (pEntity && pEntity->GetAI())
	{
		CPuppet* pPuppet = CastToCPuppetSafe(pEntity->GetAI());
		if (pPuppet)
		{
			bResult = pPuppet->ClearTempTarget();
		}
	}

	return pH->EndFunction(bResult);
}

int CScriptBind_AI::SetExtraPriority(IFunctionHandler* pH)
{

	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction(false);

	IPipeUser* pPiper = CastToIPipeUserSafe(pEntity->GetAI());
	if (!pPiper)
		return pH->EndFunction(false);

	float priority = 0.0f;
	pH->GetParam(2, priority);

	pPiper->SetExtraPriority(priority);

	return pH->EndFunction(true);

}

int CScriptBind_AI::GetExtraPriority(IFunctionHandler* pH)
{

	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction(false);

	IPipeUser* pPiper = CastToIPipeUserSafe(pEntity->GetAI());
	if (!pPiper)
		return pH->EndFunction(false);

	return pH->EndFunction(pPiper->GetExtraPriority());

}

int CScriptBind_AI::RegisterTargetTrack(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS_MIN(3);

	bool bResult = false;

	GET_ENTITY(1);
	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		if (pAI)
		{
			tAIObjectID aiObjectId = pAI->GetAIObjectID();

			const char* szConfig;
			int nTargetLimit = 0;
			pH->GetParam(2, szConfig);
			pH->GetParam(3, nTargetLimit);

			bResult = gAIEnv.pTargetTrackManager->RegisterAgent(aiObjectId, szConfig, nTargetLimit);
			if (bResult && pH->GetParamCount() > 3)
			{
				float fClassThreat = 0.0f;
				pH->GetParam(4, fClassThreat);

				gAIEnv.pTargetTrackManager->SetTargetClassThreat(aiObjectId, fClassThreat);
			}

			if (!bResult)
			{
				gEnv->pAISystem->LogComment("<CScriptBind_AI::RegisterTargetTrack> ", "Warning: Failed to register agent \'%s\' with target tracks using configuration \'%s\'",
				                            pEntity->GetName(), szConfig);
			}
		}
	}

	return pH->EndFunction(bResult);
}

int CScriptBind_AI::UnregisterTargetTrack(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	bool bResult = false;

	GET_ENTITY(1);
	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		if (pAI)
		{
			bResult = gAIEnv.pTargetTrackManager->UnregisterAgent(pAI->GetAIObjectID());
		}
	}

	return pH->EndFunction(bResult);
}

int CScriptBind_AI::SetTargetTrackClassThreat(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	bool bResult = false;

	GET_ENTITY(1);
	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		if (pAI)
		{
			float fClassThreat = 0.0f;
			pH->GetParam(2, fClassThreat);

			bResult = gAIEnv.pTargetTrackManager->SetTargetClassThreat(pAI->GetAIObjectID(), fClassThreat);
		}
	}

	return pH->EndFunction(bResult);
}

int CScriptBind_AI::TriggerCurrentTargetTrackPulse(IFunctionHandler* pH, ScriptHandle entityID, const char* stimulusName,
                                                   const char* pulseName)
{
	bool bResult = false;

	GET_ENTITY(1);

	if (pEntity)
	{
		if (IAIObject* pAI = pEntity->GetAI())
		{
			if (CPuppet* pPuppet = pAI->CastToCPuppet())
			{
				IAIObject* pTargetObject = pPuppet->GetAttentionTarget();
				if (!pTargetObject || !pTargetObject->IsAgent())
					pTargetObject = pPuppet->GetAttentionTargetAssociation();

				if (pTargetObject && pTargetObject->IsAgent())
					bResult = gAIEnv.pTargetTrackManager->TriggerPulse(pAI->GetAIObjectID(), pTargetObject->GetAIObjectID(),
					                                                   stimulusName, pulseName);
			}
		}
	}

	return pH->EndFunction(bResult);
}

int CScriptBind_AI::CreateStimulusEvent(IFunctionHandler* pH, ScriptHandle ownerId, ScriptHandle targetId,
                                        const char* stimulusName, SmartScriptTable pData)
{
	IEntity* pOwnerEntity = gEnv->pEntitySystem->GetEntity((EntityId)ownerId.n);
	IAIObject* pOwnerAI = pOwnerEntity ? pOwnerEntity->GetAI() : NULL;
	if (pOwnerEntity)
	{
		IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity((EntityId)targetId.n);
		IAIObject* pTargetAI = pTargetEntity ? pTargetEntity->GetAI() : NULL;

		tAIObjectID aiOwnerId = pOwnerAI->GetAIObjectID();
		tAIObjectID aiTargetId = (pTargetAI ? pTargetAI->GetAIObjectID() : 0);
		TargetTrackHelpers::SStimulusEvent eventInfo;

		CScriptVector vPos;
		pData->GetValue("pos", vPos);
		if (!vPos)
		{
			gEnv->pLog->LogError("CScriptBind_AI::CreateStimulusEvent: Invalid 'pos' parameter for stimulus '%s'.", stimulusName);
			return pH->EndFunction(false);
		}

		eventInfo.vPos = vPos.Get();

		int stimulusType = TargetTrackHelpers::eEST_Generic;
		pData->GetValue("type", stimulusType);
		eventInfo.eStimulusType = (TargetTrackHelpers::EAIEventStimulusType)stimulusType;

		int threat = AITHREAT_NONE;
		pData->GetValue("threat", threat);
		eventInfo.eTargetThreat = (EAITargetThreat)threat;

		float radius = 0.0f;
		pData->GetValue("radius", radius);

		pH->EndFunction(gAIEnv.pTargetTrackManager->HandleStimulusEventForAgent(aiOwnerId, aiTargetId, stimulusName, eventInfo));
	}

	return pH->EndFunction(false);
}

int CScriptBind_AI::CreateStimulusEventInRange(IFunctionHandler* pH, ScriptHandle targetId, const char* stimulusName, SmartScriptTable dataScriptTable)
{
	IEntity* targetEntity = gEnv->pEntitySystem->GetEntity((EntityId)targetId.n);
	IAIObject* targetAIObject = targetEntity ? targetEntity->GetAI() : NULL;
	tAIObjectID targetAIObjectId = (targetAIObject ? targetAIObject->GetAIObjectID() : 0);
	TargetTrackHelpers::SStimulusEvent eventInfo;

	CScriptVector positionScriptVector;
	dataScriptTable->GetValue("pos", positionScriptVector);
	if (!positionScriptVector)
	{
		gEnv->pLog->LogError("CScriptBind_AI::CreateStimulusEventInRange: Invalid 'pos' parameter for stimulus '%s'.", stimulusName);
		return pH->EndFunction(false);
	}

	eventInfo.vPos = positionScriptVector.Get();

	int stimulusType = TargetTrackHelpers::eEST_Generic;
	dataScriptTable->GetValue("type", stimulusType);
	eventInfo.eStimulusType = (TargetTrackHelpers::EAIEventStimulusType)stimulusType;

	int threat = AITHREAT_NONE;
	dataScriptTable->GetValue("threat", threat);
	eventInfo.eTargetThreat = (EAITargetThreat)threat;

	float radius = 0.0f;
	dataScriptTable->GetValue("radius", radius);

	pH->EndFunction(gAIEnv.pTargetTrackManager->HandleStimulusEventInRange(targetAIObjectId, stimulusName, eventInfo, radius));

	return pH->EndFunction();
}

int CScriptBind_AI::SetUnitProperties(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	int properties = 0;
	pH->GetParam(2, properties);
	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (pAIActor)
	{
		int groupId = pAI->GetGroupId();
		IAIGroup* pGroup = GetAISystem()->GetIAIGroup(groupId);
		if (pGroup)
			pGroup->SetUnitProperties(pAI, properties);
	}
	return pH->EndFunction();
}

int CScriptBind_AI::GetUnitCount(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	int properties = 0;
	pH->GetParam(2, properties);
	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (pAIActor)
	{
		int groupId = pAI->GetGroupId();
		IAIGroup* pGroup = GetAISystem()->GetIAIGroup(groupId);
		if (pGroup)
			return pH->EndFunction(pGroup->GetUnitCount(properties));
	}
	return pH->EndFunction();
}

int CScriptBind_AI::SetFactionThreatMultiplier(IFunctionHandler* pH)
{
	pH->GetIScriptSystem()->RaiseError("AI.SetFactionThreatMultiplier function is deprecated");
	return pH->EndFunction();
}

/*! Makes an entity a leader of the group
   @return 1 if succeeded
 */
int CScriptBind_AI::SetLeader(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	IAIObject* pLeader = NULL;
	if (pEntity)
	{
		IAIObject* pAIObject = pEntity->GetAI();
		if (pAIObject)
		{
			IAIObject* pOldLeader = GetAISystem()->GetLeaderAIObject(pAIObject);
			if (pOldLeader != pAIObject)
			{
				if (pOldLeader)
					GetAISystem()->SendSignal(0, 0, "OnLeaderDeassigned", pOldLeader);
				GetAISystem()->SetLeader(pAIObject);
				GetAISystem()->SendSignal(0, 0, "OnLeaderAssigned", pAIObject);
			}
		}
	}
	return pH->EndFunction(pLeader != NULL);
}

int CScriptBind_AI::GetStance(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	IAIActor* pAIActor;
	if (!pEntity || !(pAIActor = CastToIAIActorSafe(pEntity->GetAI())))
		return pH->EndFunction();
	return pH->EndFunction(pAIActor->GetState().bodystate);
}

int CScriptBind_AI::SetStance(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	IAIActor* pAIActor;
	if (!pEntity || !(pAIActor = CastToIAIActorSafe(pEntity->GetAI())))
		return pH->EndFunction();

	int stance;
	if (pH->GetParam(2, stance))
		pAIActor->GetState().bodystate = stance;

	return pH->EndFunction();
}

int CScriptBind_AI::SetMovementContext(IFunctionHandler* pH, ScriptHandle entityId, int context)
{
	GET_ENTITY(1);
	CPipeUser* pPipeUser;
	if (!pEntity || !(pPipeUser = CastToCPipeUserSafe(pEntity->GetAI())))
		return pH->EndFunction();

	pPipeUser->SetMovementContext(context);

	return pH->EndFunction();
}

int CScriptBind_AI::ClearMovementContext(IFunctionHandler* pH, ScriptHandle entityId, int context)
{
	GET_ENTITY(1);
	CPipeUser* pPipeUser;
	if (!pEntity || !(pPipeUser = CastToCPipeUserSafe(pEntity->GetAI())))
		return pH->EndFunction();

	pPipeUser->ClearMovementContext(context);

	return pH->EndFunction();
}

int CScriptBind_AI::AssignPFPropertiesToPathType(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(19);

	const char* sPathType = "AIPATH_DEFAULT";

	if (pH->GetParamType(1) == svtNumber)
	{
		int nPathType = AIPATH_DEFAULT;
		pH->GetParam(1, nPathType);
		sPathType = GetPathTypeName(static_cast<EAIPathType>(nPathType));
	}
	else
	{
		pH->GetParam(1, sPathType);
	}

#define GET_PF_PARAM(n, s)                                            \
  if ((pH->GetParamCount() >= n) && (pH->GetParamType(n) != svtNull)) \
    pH->GetParam(n, s)

	AgentPathfindingProperties properties;

	int navCapMask = 0;
	GET_PF_PARAM(2, navCapMask);
	properties.navCapMask = navCapMask;
	GET_PF_PARAM(3, properties.triangularResistanceFactor);
	GET_PF_PARAM(4, properties.waypointResistanceFactor);
	GET_PF_PARAM(5, properties.flightResistanceFactor);
	GET_PF_PARAM(6, properties.volumeResistanceFactor);
	GET_PF_PARAM(7, properties.roadResistanceFactor);
	GET_PF_PARAM(8, properties.waterResistanceFactor);
	GET_PF_PARAM(9, properties.maxWaterDepth);
	GET_PF_PARAM(10, properties.minWaterDepth);
	GET_PF_PARAM(11, properties.exposureFactor);
	GET_PF_PARAM(12, properties.dangerCost);
	GET_PF_PARAM(13, properties.zScale);
	GET_PF_PARAM(14, properties.customNavCapsMask);
	GET_PF_PARAM(15, properties.radius);
	GET_PF_PARAM(16, properties.height);
	GET_PF_PARAM(17, properties.maxSlope);
	GET_PF_PARAM(18, properties.id);
	GET_PF_PARAM(19, properties.avoidObstacles);
	properties.navCapMask |= properties.id * (1 << 24);
#undef GET_PF_PARAM

	// IMPORTANT: If changes are made here be sure to update CScriptBind_AI::AssignPFPropertiesToPathType() in Editor!

	AssignPFPropertiesToPathType(sPathType, properties);

	return pH->EndFunction();
}

int CScriptBind_AI::AssignPathTypeToSOUser(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	const char* soUser;
	const char* pathType = "AIPATH_DEFAULT";

	pH->GetParam(1, soUser);
	pH->GetParam(2, pathType);

	if (gAIEnv.pSmartObjectManager)
	{
		gAIEnv.pSmartObjectManager->MapPathTypeToSoUser(soUser, pathType);
	}

	return pH->EndFunction();
};

int CScriptBind_AI::SetPFProperties(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (!pAIActor)
		return pH->EndFunction();

	int nPathType = AIPATH_DEFAULT;
	const char* sPathType = "AIPATH_DEFAULT";

	if (pH->GetParamType(2) == svtNumber)
		pH->GetParam(2, nPathType);
	else
		pH->GetParam(2, sPathType);

	AgentMovementAbility mov = pAIActor->GetMovementAbility();
	if (pH->GetParamType(2) == svtNumber)
		SetPFProperties(mov, static_cast<EAIPathType>(nPathType));
	else
		SetPFProperties(mov, sPathType);

	pAIActor->SetMovementAbility(mov);
	return pH->EndFunction();
}

int CScriptBind_AI::GetTargetType(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction(AITARGET_NONE);
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction(AITARGET_NONE);
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (pAIActor)
	{
		IAIObject* pTarget = pAIActor->GetAttentionTarget();
		if (pTarget)
		{
			switch (static_cast<CAIObject*>(pTarget)->GetType())
			{
			case AIOBJECT_ACTOR:
			case AIOBJECT_PLAYER:
			case AIOBJECT_VEHICLE:
				return pH->EndFunction(pAIObject->IsHostile(pTarget) ? AITARGET_ENEMY : AITARGET_FRIENDLY);

			case AIOBJECT_DUMMY:
				switch (pTarget->GetSubType())
				{
				case IAIObject::STP_MEMORY:
					return pH->EndFunction(AITARGET_MEMORY);
				case IAIObject::STP_BEACON:
					return pH->EndFunction(AITARGET_BEACON);
				case IAIObject::STP_SOUND:
				case IAIObject::STP_GUNFIRE:
					return pH->EndFunction(AITARGET_SOUND);
				}
				break;
			case AIOBJECT_GRENADE:
				return pH->EndFunction(AITARGET_GRENADE);

			case AIOBJECT_RPG:
				return pH->EndFunction(AITARGET_RPG);

			default:
				return pH->EndFunction(AITARGET_NONE);
			}
		}
	}
	return pH->EndFunction(AITARGET_NONE);
}

int CScriptBind_AI::GetTargetSubType(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	if (!pEntity)
		return pH->EndFunction(0);
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction(0);
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (pAIActor)
	{
		IAIObject* pTarget = pAIActor->GetAttentionTarget();
		if (pTarget)
		{
			int subType = pTarget->GetSubType();
			int result = 0;
			if (subType == IAIObject::STP_CAR)
				result = AIOBJECT_CAR;
			if (subType == IAIObject::STP_BOAT)
				result = AIOBJECT_BOAT;
			if (subType == IAIObject::STP_HELI)
				result = AIOBJECT_HELICOPTER;
			if (subType == IAIObject::STP_HELICRYSIS2)
				result = AIOBJECT_HELICOPTERCRYSIS2;
			if (subType == IAIObject::STP_GUNFIRE)
				result = AIOBJECT_GUNFIRE;
			return pH->EndFunction(result);
		}
	}

	return pH->EndFunction(0);
}

int CScriptBind_AI::ChangeFormation(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 2)
		return pH->EndFunction();
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();
	const char* szDescriptor;
	pH->GetParam(2, szDescriptor);

	float fScale = 1;
	if (pH->GetParamCount() > 2)
		pH->GetParam(3, fScale);

	bool bSuccessful = gAIEnv.pFormationManager->ChangeFormation(pAIObject /*->GetParameters().m_nGroup*/, szDescriptor, fScale);

	return pH->EndFunction(bSuccessful);

}

int CScriptBind_AI::ScaleFormation(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 1)
		return pH->EndFunction();
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();

	float fScale = 1;
	if (pH->GetParamCount() > 1)
		pH->GetParam(2, fScale);

	bool bSuccessful = gAIEnv.pFormationManager->ScaleFormation(pAIObject, fScale);

	return pH->EndFunction(bSuccessful);

}

int CScriptBind_AI::SetFormationUpdate(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();

	bool bUpdate;
	pH->GetParam(2, bUpdate);

	bool bSuccessful = gAIEnv.pFormationManager->SetFormationUpdate(pAIObject, bUpdate);

	return pH->EndFunction(bSuccessful);

}

int CScriptBind_AI::SetPostures(IFunctionHandler* pH, ScriptHandle entityId, SmartScriptTable postures)
{
	GET_ENTITY(1);

	if (!pEntity)
		return pH->EndFunction();

	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();

	CPuppet* pPuppet = pAIObject->CastToCPuppet();
	if (!pPuppet)
		return pH->EndFunction();

	pPuppet->GetPostureManager().ResetPostures();

	int i = 0;
	SmartScriptTable posture;

	while (postures->GetAt(++i, posture))
		ParsePostureInfo(pPuppet, posture, PostureManager::PostureInfo(), -1, m_pSS);

	return pH->EndFunction();
}

int CScriptBind_AI::SetPosturePriority(IFunctionHandler* pH, ScriptHandle entityId, const char* postureName, float priority)
{
	GET_ENTITY(1);

	if (!pEntity)
		return pH->EndFunction();

	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();

	CPuppet* pPuppet = pAIObject->CastToCPuppet();
	if (!pPuppet)
		return pH->EndFunction();

	int postureID = pPuppet->GetPostureManager().GetPostureID(postureName);
	if (postureID > -1)
		pPuppet->GetPostureManager().SetPosturePriority(postureID, priority);

	return pH->EndFunction();
}

int CScriptBind_AI::GetPosturePriority(IFunctionHandler* pH, ScriptHandle entityId, const char* postureName)
{
	GET_ENTITY(1);

	if (!pEntity)
		return pH->EndFunction();

	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();

	CPuppet* pPuppet = pAIObject->CastToCPuppet();
	if (!pPuppet)
		return pH->EndFunction();

	PostureManager::PostureInfo posture;
	if (pPuppet->GetPostureManager().GetPostureByName(postureName, &posture))
		return pH->EndFunction(posture.priority);

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_AI::ChangeParameter(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();

	int nParameter;
	float fValue;
	pH->GetParam(2, nParameter);

	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (pAIActor)
	{
		AgentParameters ap = pAIActor->GetParameters();

		if (pH->GetParamType(3) == svtNumber)
		{
			pH->GetParam(3, fValue);

			switch (nParameter)
			{
			case AIPARAM_SIGHTRANGE:
				ap.m_PerceptionParams.sightRange = fValue;
				break;
			case AIPARAM_ATTACKRANGE:
				ap.m_fAttackRange = fValue;
				break;
			case AIPARAM_ACCURACY:
				ap.m_fAccuracy = fValue;
				break;
			case AIPARAM_GROUPID:
				ap.m_nGroup = (int) fValue;
				break;
			case AIPARAM_FOVPRIMARY:
				ap.m_PerceptionParams.FOVPrimary = fValue;
				break;
			case AIPARAM_FOVSECONDARY:
				ap.m_PerceptionParams.FOVSecondary = fValue;
				break;
			case AIPARAM_COMMRANGE:
				ap.m_fCommRange = fValue;
				break;
			case AIPARAM_FACTION:
				ap.factionID = (int)(fValue + 0.5f);
				break;
			case AIPARAM_FACTIONHOSTILITY:
				ap.factionHostility = (fValue != 0.0f);
				break;
			case AIPARAM_STRAFINGPITCH:
				ap.m_fStrafingPitch = fValue;
				break;
			case AIPARAM_COMBATCLASS:
				ap.m_CombatClass = (int) fValue;
				break;
			case AIPARAM_INVISIBLE:
				ap.m_bInvisible = fValue > 0.01f;
				break;
			case AIPARAM_PERCEPTIONSCALE_VISUAL:
				ap.m_PerceptionParams.perceptionScale.visual = fValue;
				break;
			case AIPARAM_PERCEPTIONSCALE_AUDIO:
				ap.m_PerceptionParams.perceptionScale.audio = fValue;
				break;
			case AIPARAM_CLOAK_SCALE:
				ap.m_fCloakScaleTarget = fValue;
				break;
			case AIPARAM_FORGETTIME_TARGET:
				ap.m_PerceptionParams.forgetfulnessTarget = fValue;
				break;
			case AIPARAM_FORGETTIME_SEEK:
				ap.m_PerceptionParams.forgetfulnessSeek = fValue;
				break;
			case AIPARAM_FORGETTIME_MEMORY:
				ap.m_PerceptionParams.forgetfulnessMemory = fValue;
				break;
			case AIPARAM_LOOKIDLE_TURNSPEED:
				ap.m_lookIdleTurnSpeed = DEG2RAD(fValue);
				break;
			case AIPARAM_LOOKCOMBAT_TURNSPEED:
				ap.m_lookCombatTurnSpeed = DEG2RAD(fValue);
				break;
			case AIPARAM_AIM_TURNSPEED:
				ap.m_aimTurnSpeed = DEG2RAD(fValue);
				break;
			case AIPARAM_FIRE_TURNSPEED:
				ap.m_fireTurnSpeed = DEG2RAD(fValue);
				break;
			case AIPARAM_MELEE_DISTANCE:
				ap.m_fMeleeRange = fValue;
				break;
			case AIPARAM_MELEE_SHORT_DISTANCE:
				ap.m_fMeleeRangeShort = fValue;
				break;
			case AIPARAM_MIN_ALARM_LEVEL:
				ap.m_PerceptionParams.minAlarmLevel = fValue;
				break;
			case AIPARAM_PROJECTILE_LAUNCHDIST:
				ap.m_fProjectileLaunchDistScale = fValue;
				break;
			case AIPARAM_SIGHTDELAY:
				ap.m_PerceptionParams.sightDelay = fValue;
				break;
			case AIPARAM_SIGHTNEARRANGE:
				ap.m_PerceptionParams.sightNearRange = fValue;
				break;
			}
		}
		else if (pH->GetParamType(3) == svtString)
		{
			const char* value = 0;
			pH->GetParam(3, value);

			switch (nParameter)
			{
			case AIPARAM_FACTION:
				ap.factionID = gAIEnv.pFactionMap->GetFactionID(value);
				break;
			}
		}

		pAIActor->SetParameters(ap);
	}
	else if (pAI)
	{
		if (pH->GetParamType(3) == svtNumber)
		{
			pH->GetParam(3, fValue);

			switch (nParameter)
			{
			case AIPARAM_GROUPID:
				pAI->SetGroupId((int)fValue);
				break;
			case AIPARAM_FACTION:
				pAI->SetFactionID((int)(fValue + 0.5f));
				break;
			default:
				break;
			}
		}
		else if (pH->GetParamType(3) == svtString)
		{
			const char* value = 0;
			pH->GetParam(3, value);

			switch (nParameter)
			{
			case AIPARAM_FACTION:
				pAI->SetFactionID(gAIEnv.pFactionMap->GetFactionID(value));
				break;
			}
		}
	}

	return pH->EndFunction();
}

const char* CScriptBind_AI::GetPathTypeName(EAIPathType pathType)
{
	switch (pathType)
	{
	default:
	case AIPATH_DEFAULT:
		return "AIPATH_DEFAULT";

	case AIPATH_HUMAN:
		return "AIPATH_HUMAN";
	case AIPATH_HUMAN_COVER:
		return "AIPATH_HUMAN_COVER";
	case AIPATH_CAR:
		return "AIPATH_CAR";
	case AIPATH_TANK:
		return "AIPATH_TANK";
	case AIPATH_BOAT:
		return "AIPATH_BOAT";
	case AIPATH_HELI:
		return "AIPATH_HELI";
	case AIPATH_3D:
		return "AIPATH_3D";
	case AIPATH_SCOUT:
		return "AIPATH_SCOUT";
	case AIPATH_TROOPER:
		return "AIPATH_TROOPER";
	case AIPATH_HUNTER:
		return "AIPATH_HUNTER";
	}
	assert(!!!"Should not be here");
	return 0;
}

// INTEGRATION : (MATT) GetAIParameter will appear from main branch and is duplicate {2007/08/03:11:00:39}
// Heavily changed their code which was very limited - check their changees for new parameters
int CScriptBind_AI::GetParameter(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	if (!pEntity)
		return pH->EndFunction();

	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();

	int nParameter;
	pH->GetParam(2, nParameter);

	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (pAIActor)
	{
		AgentParameters ap = pAIActor->GetParameters();

		switch (nParameter)
		{
		case AIPARAM_SIGHTRANGE:
			return pH->EndFunction(ap.m_PerceptionParams.sightRange);
			break;
		case AIPARAM_ATTACKRANGE:
			return pH->EndFunction(ap.m_fAttackRange);
			break;
		case AIPARAM_ACCURACY:
			return pH->EndFunction(ap.m_fAccuracy);
			break;
		case AIPARAM_GROUPID:
			return pH->EndFunction(ap.m_nGroup);
			break;
		case AIPARAM_FOVPRIMARY:
			return pH->EndFunction(ap.m_PerceptionParams.FOVPrimary);
			break;
		case AIPARAM_FOVSECONDARY:
			return pH->EndFunction(ap.m_PerceptionParams.FOVSecondary);
			break;
		case AIPARAM_COMMRANGE:
			return pH->EndFunction(ap.m_fCommRange);
			break;
		case AIPARAM_FACTION:
			return pH->EndFunction(ap.factionID);
			break;
		case AIPARAM_FACTIONHOSTILITY:
			return pH->EndFunction(ap.factionHostility);
			break;
		case AIPARAM_STRAFINGPITCH:
			return pH->EndFunction(ap.m_fStrafingPitch);
			break;
		case AIPARAM_COMBATCLASS:
			return pH->EndFunction(ap.m_CombatClass);
			break;
		case AIPARAM_INVISIBLE:
			return pH->EndFunction(ap.m_bInvisible);
			break;
		case AIPARAM_PERCEPTIONSCALE_VISUAL:
			return pH->EndFunction(ap.m_PerceptionParams.perceptionScale.visual);
			break;
		case AIPARAM_PERCEPTIONSCALE_AUDIO:
			return pH->EndFunction(ap.m_PerceptionParams.perceptionScale.audio);
			break;
		case AIPARAM_CLOAK_SCALE:
			return pH->EndFunction(ap.m_fCloakScale);
			break;
		case AIPARAM_FORGETTIME_TARGET:
			return pH->EndFunction(10.0f / ap.m_PerceptionParams.forgetfulnessTarget);
			break;
		case AIPARAM_FORGETTIME_SEEK:
			return pH->EndFunction(10.0f / ap.m_PerceptionParams.forgetfulnessSeek);
			break;
		case AIPARAM_FORGETTIME_MEMORY:
			return pH->EndFunction(10.0f / ap.m_PerceptionParams.forgetfulnessMemory);
			break;
		case AIPARAM_LOOKIDLE_TURNSPEED:
			return pH->EndFunction(RAD2DEG(ap.m_lookIdleTurnSpeed));
			break;
		case AIPARAM_LOOKCOMBAT_TURNSPEED:
			return pH->EndFunction(RAD2DEG(ap.m_lookCombatTurnSpeed));
			break;
		case AIPARAM_AIM_TURNSPEED:
			return pH->EndFunction(RAD2DEG(ap.m_aimTurnSpeed));
			break;
		case AIPARAM_FIRE_TURNSPEED:
			return pH->EndFunction(RAD2DEG(ap.m_fireTurnSpeed));
			break;
		case AIPARAM_MELEE_DISTANCE:
			return pH->EndFunction(ap.m_fMeleeRange);
			break;
		case AIPARAM_MELEE_SHORT_DISTANCE:
			return pH->EndFunction(ap.m_fMeleeRangeShort);
			break;
		case AIPARAM_MIN_ALARM_LEVEL:
			return pH->EndFunction(ap.m_PerceptionParams.minAlarmLevel);
			break;
		case AIPARAM_PROJECTILE_LAUNCHDIST:
			return pH->EndFunction(ap.m_fProjectileLaunchDistScale);
			break;
		case AIPARAM_SIGHTDELAY:
			return pH->EndFunction(ap.m_PerceptionParams.sightDelay);
			break;
		case AIPARAM_SIGHTNEARRANGE:
			return pH->EndFunction(ap.m_PerceptionParams.sightNearRange);
			break;
		case AIPARAM_CLOAKED:
			return pH->EndFunction(ap.m_bCloaked);
			break;

		default:
			return pH->EndFunction();
			break;
		}
	}

	return pH->EndFunction();

}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_AI::ChangeMovementAbility(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();

	int nParameter;
	float fValue;
	pH->GetParam(2, nParameter);

	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (pAIActor)
	{
		AgentMovementAbility mov = pAIActor->GetMovementAbility();

		if (pH->GetParamType(3) == svtNumber)
		{
			pH->GetParam(3, fValue);

			switch (nParameter)
			{
			case AIMOVEABILITY_OPTIMALFLIGHTHEIGHT:
				mov.optimalFlightHeight = fValue;
				break;
			case AIMOVEABILITY_MINFLIGHTHEIGHT:
				mov.minFlightHeight = fValue;
				break;
			case AIMOVEABILITY_MAXFLIGHTHEIGHT:
				mov.maxFlightHeight = fValue;
				break;
			case AIMOVEABILITY_TELEPORTENABLE:
				mov.teleportEnabled = fValue != 0.0f;
				break;
			case AIMOVEABILITY_USEPREDICTIVEFOLLOWING:
				mov.usePredictiveFollowing = fValue != 0;
				break;
			default:
				AIErrorID("<Lua> ", "ChangeMovementAbility: unhandled param %d", nParameter);
				break;
			}
		}
		pAIActor->SetMovementAbility(mov);
	}

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetForwardDir(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	SmartScriptTable pTgtPos;
	if (!pH->GetParam(2, pTgtPos))
		return pH->EndFunction();

	IAIObject* pObject = pEntity->GetAI();
	if (pObject)
	{
		Vec3 dir = pObject->GetMoveDir();
		dir.NormalizeSafe();
		pTgtPos->SetValue("x", dir.x);
		pTgtPos->SetValue("y", dir.y);
		pTgtPos->SetValue("z", dir.z);
	}
	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetNavigationType(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 1)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetNavigationType(): wrong number of parameters passed");
		return pH->EndFunction();
	}

	int nBuilding;
	if (pH->GetParamType(1) == svtPointer)
	{
		GET_ENTITY(1);
		if (!pEntity)
		{
			AIWarningID("<CScriptBind_AI> ", "AI.GetNavigationType(): wrong type of parameter 1");
			return pH->EndFunction();
		}
		IAIObject* pAIObject = pEntity->GetAI();
		IAIActor* pAIActor = CastToIAIActorSafe(pAIObject);
		if (!pAIActor)
			return pH->EndFunction();
		return pH->EndFunction(gAIEnv.pNavigation->CheckNavigationType(pAIObject->GetPos(), nBuilding, pAIActor->GetMovementAbility().pathfindingProperties.navCapMask));
	}
	else if (pH->GetParamType(1) == svtNumber)
	{
		int groupId = -1;
		pH->GetParam(1, groupId);
		uint32 unitProp = UPR_ALL;
		if (pH->GetParamCount() > 1 && pH->GetParamType(2) == svtNumber)
			pH->GetParam(2, unitProp);

		typedef std::map<int, int> TmapIntInt;
		TmapIntInt NavTypeCount;

		for (int i = 1; i <= IAISystem::NAV_MAX_VALUE; i <<= 1)
			NavTypeCount.insert(std::make_pair(i, 0));

		int numAgents = GetAISystem()->GetGroupCount(groupId, IAISystem::GROUP_ENABLED);
		for (int i = 0; i < numAgents; i++)
		{
			IAIObject* pAIObject = GetAISystem()->GetGroupMember(groupId, i, IAISystem::GROUP_ENABLED);
			IAIActor* pAIActor = CastToIAIActorSafe(pAIObject);
			if (pAIActor)
			{
				int navType = gAIEnv.pNavigation->CheckNavigationType(pAIObject->GetPos(), nBuilding, pAIActor->GetMovementAbility().pathfindingProperties.navCapMask);
				if (NavTypeCount.find(navType) != NavTypeCount.end())
					NavTypeCount[navType] = NavTypeCount[navType] + 1;
			}
		}
		int maxNavigationTypeCount = 0;
		int navTypeSelected = IAISystem::NAV_UNSET;
		for (TmapIntInt::iterator it = NavTypeCount.begin(); it != NavTypeCount.end(); ++it)
		{
			int navCount = it->second;
			if (navCount > maxNavigationTypeCount)
			{
				navCount = maxNavigationTypeCount;
				navTypeSelected = it->first;
			}
		}
		return pH->EndFunction(navTypeSelected);
	}
	else
		AIWarningID("<CScriptBind_AI> ", "AI.GetNavigationType(): wrong type of parameter 1");
	return pH->EndFunction();

}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetDirectAnchorPos(IFunctionHandler* pH)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	//	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	float fRadiusMax = 15;
	float fRadiusMin = 1;
	int nAnchor(0);

	if (!pH->GetParam(2, nAnchor))
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetDirectAttackPos(): wrong type of parameter 2");
		return pH->EndFunction();
	}
	if (!pH->GetParam(3, fRadiusMin))
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetDirectAttackPos(): wrong type of parameter 3");
		return pH->EndFunction();
	}
	if (!pH->GetParam(4, fRadiusMax))
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetDirectAttackPos(): wrong type of parameter 3");
		return pH->EndFunction();
	}
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return pH->EndFunction();

	IAIObject* pTarget = pAIActor->GetAttentionTarget();
	if (!pTarget)
		return pH->EndFunction();

	IAIObject* pAnchor = GetAISystem()->GetNearestToObjectInRange(pAI, nAnchor, fRadiusMin, fRadiusMax, -1, true, true, false);

	if (pAnchor)
		return pH->EndFunction(pAnchor->GetPos());

	return pH->EndFunction();

}

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::InvalidateHidespot(IFunctionHandler* pH)
{
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	CPipeUser* pPipeUser = CastToCPipeUserSafe(pAI);

	if (pPipeUser)
		pPipeUser->m_CurrentHideObject.Invalidate();

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::EvalHidespot(IFunctionHandler* pH)
{
	// Evaluation:
	// -1 - bad target or shooter
	//  0 - no cover at all
	//  1 - low cover
	//  2 - blind cover
	//  3 - high cover

	CRY_PROFILE_FUNCTION(PROFILE_AI);

	//	SCRIPT_CHECK_PARAMETERS(4);
	GET_ENTITY(1);

	IAIObject* pShooter = pEntity->GetAI();
	CAIActor* pShooterAIActor = CastToCAIActorSafe(pShooter);
	if (!pShooterAIActor)
		return pH->EndFunction(-1);

	IAIObject* pTarget = pShooterAIActor->GetAttentionTarget();
	if (!pTarget)
		return pH->EndFunction(-1);

	IPhysicalEntity* pPhysSkip[4];
	int nSkip = 0;
	if (pShooter->GetProxy())
	{
		// Get both the collision proxy and the animation proxy
		pPhysSkip[nSkip] = pShooter->GetProxy()->GetPhysics(false);
		if (pPhysSkip[nSkip]) nSkip++;
		pPhysSkip[nSkip] = pShooter->GetProxy()->GetPhysics(true);
		if (pPhysSkip[nSkip]) nSkip++;
	}
	if (pTarget->GetProxy())
	{
		// Get both the collision proxy and the animation proxy
		pPhysSkip[nSkip] = pTarget->GetProxy()->GetPhysics(false);
		if (pPhysSkip[nSkip]) nSkip++;
		pPhysSkip[nSkip] = pTarget->GetProxy()->GetPhysics(true);
		if (pPhysSkip[nSkip]) nSkip++;
	}

	Vec3 floorPos = pShooterAIActor->GetFloorPosition(pShooter->GetPos());
	const SAIBodyInfo& bodyInfo = pShooterAIActor->GetBodyInfo();

	// Hard-coded for human size
	Vec3 waistPos = floorPos + Vec3(0, 0, 0.75f);
	Vec3 headPos = floorPos + Vec3(0, 0, 1.3f);
	Vec3 targetPos = pTarget->GetPos();

	int evaluation = 0;
	ray_hit hit;
	int rayresult;

	rayresult = RayWorldIntersectionWrapper(waistPos, (targetPos - waistPos), ent_static, rwi_stop_at_pierceable, &hit, 1, pPhysSkip, nSkip);
	if (rayresult && hit.dist < 2.5f)
		evaluation |= 1;

	rayresult = RayWorldIntersectionWrapper(headPos, (targetPos - headPos), ent_static, rwi_stop_at_pierceable, &hit, 1, pPhysSkip, nSkip);
	if (rayresult && hit.dist < 2.5f)
		evaluation |= 2;

	return pH->EndFunction(evaluation);
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::EvalPeek(IFunctionHandler* pH)
{
	// Evaluation:
	//  -1 - don't need to peek
	//  0 - cannot peek
	//  1 - can peek from left
	//  2 - can peek from right
	//  3 - can peek from left & right

	CRY_PROFILE_FUNCTION(PROFILE_AI);
	GET_ENTITY(1);

	// Optimal Side will cause the function to return either left or right (not both, if both sides are
	//	determined to be valid) depending on which side the attention target is to the AI object.
	//	This is a primitive way of getting him to peek out the right side so the AI object could see
	//	the attention target (and vise versa) but is not fool-proof
	bool bGetOptimalSide = false;
	if (pH->GetParamCount() > 1)
	{
		pH->GetParam(2, bGetOptimalSide);
	}

	int retValue = 0;

	IAIObject* pShooter = pEntity->GetAI();
	CAIActor* pShooterAIActor = CastToCAIActorSafe(pShooter);

	if (pShooterAIActor)
	{
		IPhysicalEntity* pPhysSkip[4] = { 0 }; // Initialize all elements to 0
		int nSkip = 0;

		// Use either attention target or beacon if available
		IAIObject* pTarget = pShooterAIActor->GetAttentionTarget();
		if (!pTarget)
		{
			pTarget = GetAISystem()->GetBeacon(pShooter->GetGroupId());
		}
		if (pTarget)
		{
			IAIActorProxy* pTargetProxy = pTarget->GetProxy();
			if (pTargetProxy)
			{
				// Get both the collision proxy and the animation proxy
				pPhysSkip[nSkip] = pTargetProxy->GetPhysics(false);
				if (pPhysSkip[nSkip]) nSkip++;
				pPhysSkip[nSkip] = pTargetProxy->GetPhysics(true);
				if (pPhysSkip[nSkip]) nSkip++;
			}
		}

		IAIActorProxy* pShooterProxy = pShooter->GetProxy();
		if (pShooterProxy)
		{
			// Get both the collision proxy and the animation proxy
			pPhysSkip[nSkip] = pShooterProxy->GetPhysics(false);
			if (pPhysSkip[nSkip]) nSkip++;
			pPhysSkip[nSkip] = pShooterProxy->GetPhysics(true);
			if (pPhysSkip[nSkip]) nSkip++;
		}

		// Height is based on collider
		pe_player_dimensions dimensions;
		pEntity->GetPhysics()->GetParams(&dimensions);
		const float height = dimensions.heightCollider;

		const Vec3& floorPos = pShooterAIActor->GetFloorPosition(pShooter->GetPos());
		const Vec3 shooterPos(floorPos.x, floorPos.y, floorPos.z + height);
		const Vec3& right(pEntity->GetWorldTM().GetColumn0());

		Vec3 dir;
		if (pTarget)
		{
			dir = pTarget->GetPos() - shooterPos;
		}
		else
		{
			// Base it on last goalpipe's starting attention target pos
			Vec3 targetPos = Vec3Constants<float>::fVec3_Zero;

			if (CPipeUser* pShooterPipeUser = pShooterAIActor->CastToCPipeUser())
				if (const CGoalPipe* pLastGoalPipe = pShooterPipeUser->GetActiveGoalPipe())
					targetPos = pLastGoalPipe->GetAttTargetPosAtStart();

			if (!targetPos.IsZero())
			{
				dir = targetPos - shooterPos;
			}
			else
			{
				// Use shooter's forward dir
				dir = pEntity->GetForwardDir();
			}
		}
		dir.NormalizeSafe();

		retValue = -1;
		ray_hit hit;

		float dist = 3.0f;     // how far to check for the obstruction
		float rad = 0.5f;      // the radius of the obstruction checked in the middle.
		float peekDist = 0.7f; // the lateral distance to check for visibility

		AABB rayBox;
		rayBox.Reset();
		rayBox.Add(shooterPos, rad);
		rayBox.Add(shooterPos + dir * dist, rad);

		IPhysicalEntity** entities;
		unsigned nEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(rayBox.min, rayBox.max, entities, ent_static | ent_ignore_noncolliding);
		Vec3 hitDir(ZERO);

		if (OverlapSweptSphere(shooterPos + Vec3(0, 0, rad * 0.5f), dir * dist, rad, entities, nEntities, hitDir))
		{
			// The center must be partially obscured for the peek to make sense.
			retValue = 0;

			GetAISystem()->AddDebugLine(shooterPos, shooterPos + right * peekDist, 255, 255, 255, 4);

			if (!RayWorldIntersectionWrapper(shooterPos, right * peekDist, ent_static, rwi_stop_at_pierceable, &hit, 1))
			{
				Vec3 p = shooterPos + right * peekDist;
				GetAISystem()->AddDebugLine(p, p + dir * dist, 255, 255, 255, 4);
				if (!RayWorldIntersectionWrapper(p, dir * dist, ent_static, rwi_stop_at_pierceable, &hit, 1))
					retValue |= 1;
			}

			GetAISystem()->AddDebugLine(shooterPos, shooterPos - right * peekDist, 255, 255, 255, 4);

			if (!RayWorldIntersectionWrapper(shooterPos, -right * peekDist, ent_static, rwi_stop_at_pierceable, &hit, 1))
			{
				Vec3 p = shooterPos - right * peekDist;
				GetAISystem()->AddDebugLine(p, p + dir * dist, 255, 255, 255, 4);
				if (!RayWorldIntersectionWrapper(p, dir * dist, ent_static, rwi_stop_at_pierceable, &hit, 1))
					retValue |= 2;
			}
		}

		// Check for optimal peek side
		if (retValue > 2 && bGetOptimalSide && pTarget)
		{
			if (dir.Dot(right) >= 0)
				retValue = 2;
			else
				retValue = 1;
		}
	}

	CCCPOINT(CScriptbind_AI_EvalPeek);
	return pH->EndFunction(retValue);
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AddPersonallyHostile(IFunctionHandler* pH, ScriptHandle entityID, ScriptHandle hostileID)
{
	CAIActor* actor = 0;

	if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)entityID.n))
	{
		if (IAIObject* entityObject = entity->GetAI())
			actor = entityObject->CastToCAIActor();
	}

	if (actor)
	{
		if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)hostileID.n))
		{
			if (tAIObjectID objectID = entity->GetAIObjectID())
				actor->AddPersonallyHostile(objectID);
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::RemovePersonallyHostile(IFunctionHandler* pH, ScriptHandle entityID, ScriptHandle hostileID)
{
	CAIActor* actor = 0;

	if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)entityID.n))
	{
		if (IAIObject* entityObject = entity->GetAI())
			actor = entityObject->CastToCAIActor();
	}

	if (actor)
	{
		if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)hostileID.n))
		{
			if (tAIObjectID objectID = entity->GetAIObjectID())
				actor->RemovePersonallyHostile(objectID);
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::ResetPersonallyHostiles(IFunctionHandler* pH, ScriptHandle entityID, ScriptHandle hostileID)
{
	CAIActor* actor = 0;

	if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)entityID.n))
	{
		if (IAIObject* entityObject = entity->GetAI())
		{
			actor = entityObject->CastToCAIActor();

			actor->ResetPersonallyHostiles();
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::IsPersonallyHostile(IFunctionHandler* pH, ScriptHandle entityID, ScriptHandle hostileID)
{
	CAIActor* actor = 0;

	if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)entityID.n))
	{
		if (IAIObject* entityObject = entity->GetAI())
			actor = entityObject->CastToCAIActor();
	}

	if (actor)
	{
		if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)hostileID.n))
		{
			if (tAIObjectID objectID = entity->GetAIObjectID())
				return pH->EndFunction(actor->IsPersonallyHostile(objectID));
		}
	}

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::NotifyReinfDone(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	int done = 0;
	if (!pH->GetParam(2, done))
	{
		AIWarningID("<CScriptBind_AI> ", "AI.NotifyReinfDone(): wrong type of parameter 2");
		return pH->EndFunction();
	}

	if (IAIObject* aiObject = pEntity ? pEntity->GetAI() : 0)
	{
		if (IAIGroup* pGroup = GetAISystem()->GetIAIGroup(aiObject->GetGroupId()))
			pGroup->NotifyReinfDone(aiObject, done != 0);
	}
	return pH->EndFunction(0);
}

int CScriptBind_AI::BehaviorEvent(IFunctionHandler* pH)
{
	GET_ENTITY(1);

	int event = 0;

	if (pEntity && pH->GetParam(2, event))
	{

		if (IAIObject* aiObject = pEntity->GetAI())
		{
			if (IAIActor* actor = aiObject->CastToIAIActor())
				actor->BehaviorEvent((EBehaviorEvent)event);
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetGroupAveragePosition(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	uint32 properties;
	pH->GetParam(2, properties);

	CScriptVector vReturnPos;
	pH->GetParam(3, vReturnPos);

	if (!pEntity)
	{
		vReturnPos.Set(ZERO);
		return pH->EndFunction();
	}
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
	{
		vReturnPos.Set(ZERO);
		return pH->EndFunction();
	}
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		vReturnPos.Set(ZERO);
		return pH->EndFunction();
	}

	IAIGroup* pGroup = GetAISystem()->GetIAIGroup(pAI->GetGroupId());
	if (!pGroup)
	{
		vReturnPos.Set(ZERO);
		return pH->EndFunction();
	}

	Vec3 pos = pGroup->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES, properties);

	vReturnPos.Set(pos);

	return pH->EndFunction();

}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetFireMode(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (pEntity)
	{
		IAIObject* pObject = pEntity->GetAI();
		IPipeUser* pPipeUser = NULL;
		if (pObject) pPipeUser = CastToIPipeUserSafe(pObject);

		// TODO: Should report cases where this doesn't apply
		if (pPipeUser)
		{
			int mode;
			pH->GetParam(2, mode);
			pPipeUser->SetFireMode((EFireMode)mode);
		}
	}

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetMemoryFireType(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (pEntity)
	{
		IPuppet* pPuppet = CastToIPuppetSafe(pEntity->GetAI());
		if (pPuppet)
		{
			int type;
			pH->GetParam(2, type);
			pPuppet->SetMemoryFireType((EMemoryFireType)type);
		}
	}

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetMemoryFireType(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (pEntity)
	{
		IPuppet* pPuppet = CastToIPuppetSafe(pEntity->GetAI());
		if (pPuppet)
		{
			return pH->EndFunction(pPuppet->GetMemoryFireType());
		}
	}

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::ThrowGrenade(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	bool bResult = false;
	if (pEntity)
	{
		CPuppet* pPuppet = CastToCPuppetSafe(pEntity->GetAI());
		if (!pPuppet)
		{
			AIWarningID("<CScriptBind_AI::ThrowGrenade> ", "WARNING: no AI for entity %s", pEntity->GetName());
		}

		int iGrenadeType = eRGT_INVALID;
		if (!pH->GetParam(2, iGrenadeType) || iGrenadeType <= eRGT_ANY || iGrenadeType >= eRGT_COUNT)
		{
			AIWarningID("<CScriptBind_AI::ThrowGrenade> ", "WARNING: Bad grenade type (%d)", iGrenadeType);
		}

		int iReqType = -1;
		pH->GetParam(3, iReqType);
		switch (iReqType)
		{
		case AI_REG_LASTOP:
		case AI_REG_REFPOINT:
		case AI_REG_ATTENTIONTARGET:
			// Valid types
			break;

		default:
			AIWarningID("<CScriptBind_AI::ThrowGrenade> ", "WARNING: Bad requested target type (%d)", iReqType);
		}

		if (pPuppet)
		{
			pPuppet->RequestThrowGrenade((ERequestedGrenadeType)iGrenadeType, iReqType);
			bResult = true;
		}
	}

	return pH->EndFunction(bResult);
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetRefPointToGrenadeAvoidTarget(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 3)
	{
		AIWarningID("<CScriptBind_AI::SetRefPointToGrenadeAvoidTarget> ", "WARNING: too few parameters (expected at least 3)");
		return pH->EndFunction(-1.0f);
	}
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction(-1.0f);

	Vec3 grenadePos(0, 0, 0);
	if (!pH->GetParam(2, grenadePos))
		return pH->EndFunction(-1.0f);

	float reactionRadius = 0.0f;
	if (!pH->GetParam(3, reactionRadius))
		return pH->EndFunction(-1.0f);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction(-1.0f);
	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return pH->EndFunction(-1.0f);
	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return pH->EndFunction(-1.0f);

	const Vec3& posAI = pAI->GetPos();
	float distToGrenadeSq = Distance::Point_PointSq(posAI, grenadePos);
	if (distToGrenadeSq > sqr(reactionRadius))
		return pH->EndFunction(-1.0f);

	IAIObject* pTarget = pAIActor->GetAttentionTarget();

	Vec3 dir(0, 0, 0);

	if (pTarget && pAI->IsHostile(pTarget))
	{
		// Avoid both the target and the grenade.
		Lineseg line(grenadePos, pTarget->GetPos());
		float t = 0;
		dir = posAI - line.GetPoint(t);
		//		GetAISystem()->AddDebugLine(line.start, line.end, 255,0,0, 10.0f);
	}
	else
	{
		// Away from the grenade.
		dir = posAI - grenadePos;
	}

	dir.z = 0;
	dir.Normalize();
	dir *= reactionRadius * 2;
	pPipeUser->SetRefPointPos(posAI + dir);

	//	GetAISystem()->AddDebugLine(posAI, posAI + dir, 255,255,255, 10.0f);
	//	GetAISystem()->AddDebugSphere(posAI + dir, 0.3f, 255,255,255, 10.0f);

	return pH->EndFunction(sqrtf(distToGrenadeSq));
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IsAgentInTargetFOV(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	float FOV;
	if (!pH->GetParam(2, FOV))
		return pH->EndFunction(0);

	if (!pEntity)
		return pH->EndFunction(0);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction(0);

	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return pH->EndFunction(0);

	IAIObject* pTarget = pAIActor->GetAttentionTarget();
	if (!pTarget)
		return pH->EndFunction(0);

	// Check if the entity is within the target FOV.
	Vec3 dirToTarget = pAI->GetPos() - pTarget->GetPos();
	dirToTarget.z = 0.0f;
	dirToTarget.NormalizeSafe();

	Vec3 viewDir(pTarget->GetViewDir());
	viewDir.z = 0.0f;
	viewDir.NormalizeSafe();

	Vec3 p(pTarget->GetPos() + Vec3(0, 0, 0.5f));
	float dot = dirToTarget.Dot(viewDir);

	if (dot > cosf(DEG2RAD(FOV / 2)))
		return pH->EndFunction(1);

	return pH->EndFunction(0);
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AgentLookAtPos(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	Vec3 pos;

	if (!pH->GetParam(2, pos))
		return pH->EndFunction(0);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction(0);

	IPipeUser* pPipeUser = CastToIPipeUserSafe(pAI);

	if (pPipeUser != NULL)
		return pH->EndFunction(0);

	//i am almost sure this would crash :-)
	//pPipeUser->SetLookAtDir(pos);

	return pH->EndFunction(0);
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::ResetAgentLookAtPos(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction(0);

	IPipeUser* pPipeUser = CastToIPipeUserSafe(pAI);

	if (pPipeUser != NULL)
		return pH->EndFunction(0);

	//i am almost sure this would crash :-)
	//pPipeUser->ResetLookAt();

	return pH->EndFunction(0);
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IsAgentInAgentFOV(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	if (!pEntity)
		return pH->EndFunction(0);

	IEntity* pEntity2 = GetEntityFromParam(pH, 2);
	if (!pEntity2)
		return pH->EndFunction(0);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction(0);

	const IAIObject::EFieldOfViewResult viewResult = pAI->IsPointInFOV(pEntity2->GetPos());
	return (pH->EndFunction(viewResult != IAIObject::eFOV_Outside, viewResult == IAIObject::eFOV_Primary));
}

// AI.CreateGroupFormation(entityId, leaderId)
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::CreateGroupFormation(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.CreateGroupFormation(): wrong type of parameter 1");
		return pH->EndFunction(0);
	}

	IEntity* pEntity2 = GetEntityFromParam(pH, 2);
	if (!pEntity2)
		return pH->EndFunction(0);

	if (pEntity2 == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.CreateGroupFormation(): wrong type of parameter 2");
		return pH->EndFunction(0);
	}

	IAIObject* pAI = pEntity->GetAI();
	if (pAI == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.CreateGroupFormation(): wrong parameter 1");
		return pH->EndFunction(0);
	}

	IAIObject* pAI2 = pEntity2->GetAI();
	if (pAI == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.CreateGroupFormation(): wrong parameter 2");
		return pH->EndFunction(0);
	}

	pAI->CreateGroupFormation(pAI2);

	return pH->EndFunction(1);
}

// AI.SetFormationPosition(entityId, v2RelativePosition )
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetFormationPosition(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetFormationPosition(): wrong type of parameter 1");
		return pH->EndFunction(0);
	}

	IAIObject* pAI = pEntity->GetAI();
	if (pAI == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetFormationPosition(): wrong parameter 1");
		return pH->EndFunction(0);
	}

	Vec3 pos;

	if (!pH->GetParam(2, pos))
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetFormationPosition(): wrong type of parameter 2");
		return pH->EndFunction(0);
	}

	Vec2 pos2(pos.x, pos.y);
	pAI->SetFormationPos(pos2);

	return pH->EndFunction(1);
}

// AI.SetFormationLookingPoint(entityId, v3RelativePosition )
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetFormationLookingPoint(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetFormationLookingPoint(): wrong type of parameter 1");
		return pH->EndFunction(0);
	}

	IAIObject* pAI = pEntity->GetAI();
	if (pAI == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetFormationLookingPoint(): wrong parameter 1");
		return pH->EndFunction(0);
	}

	Vec3 pos;

	if (!pH->GetParam(2, pos))
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetFormationLookingPoint(): wrong type of parameter 2");
		return pH->EndFunction(0);
	}

	pAI->SetFormationLookingPoint(pos);

	return pH->EndFunction(1);
}

// AI.SetFormationAngleThreshold(entityId, fAngleThreshold )
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetFormationAngleThreshold(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetFormationAngleThreshold(): wrong type of parameter 1");
		return pH->EndFunction(0);
	}

	IAIObject* pAI = pEntity->GetAI();
	if (pAI == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetFormationAngleThreshold(): wrong parameter 1");
		return pH->EndFunction(0);
	}

	float fDegrees = 0.0f;

	if (!pH->GetParam(2, fDegrees))
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetFormationAngleThreshold(): wrong type of parameter 2");
		return pH->EndFunction();
	}

	pAI->SetFormationAngleThreshold(fDegrees);

	return pH->EndFunction(1);
}

// vec3 AI.GetFormationPosition(entityId)
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetFormationPosition(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetFormationPosition(): wrong type of parameter 1");
		return pH->EndFunction(0);
	}

	IAIObject* pAI = pEntity->GetAI();
	if (pAI == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetFormationPosition(): wrong parameter 1");
		return pH->EndFunction(0);
	}

	Vec3 v3 = pAI->GetFormationPos();

	return pH->EndFunction(v3);
}

// vec3 AI.GetFormationPosition(entityId)
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetFormationLookingPoint(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetFormationLookingPoint(): wrong type of parameter 1");
		return pH->EndFunction(0);
	}

	IAIObject* pAI = pEntity->GetAI();
	if (pAI == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetFormationLookingPoint(): wrong parameter 1");
		return pH->EndFunction(0);
	}

	Vec3 v3 = pAI->GetFormationLookingPoint();

	return pH->EndFunction(v3);
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AddCombatClass(IFunctionHandler* pH)
{
	if (pH->GetParamCount() == 0)
	{
		GetAISystem()->AddCombatClass(0, NULL, 0, 0);
		return pH->EndFunction();
	}

	int combatClass = 0;
	if (!pH->GetParam(1, combatClass))
		return pH->EndFunction();

	int n = 0;
	std::vector<float> scalesVector;
	SmartScriptTable pTable;
	if (pH->GetParam(2, pTable))
	{
		float scaleValue = 1.f;
		while (pTable->GetAt(n + 1, scaleValue))
		{
			scalesVector.push_back(scaleValue);
			++n;
		}
	}

	// Check is optional custom OnSeen signal specified
	const char* szCustomSignal = 0;
	if (pH->GetParamCount() > 2)
		pH->GetParam(3, szCustomSignal);

	GetAISystem()->AddCombatClass(combatClass, &scalesVector[0], n, szCustomSignal);

	return pH->EndFunction();
}

//
// wrapper, to make it use tick-counter
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::RayWorldIntersectionWrapper(Vec3 org, Vec3 dir, int objtypes, unsigned int flags, ray_hit* hits, int nMaxHits,
                                                IPhysicalEntity** pSkipEnts, int nSkipEnts, void* pForeignData, int iForeignData)
{
	int rwiResult(0);
	rwiResult = gAIEnv.pWorld->RayWorldIntersection(org, dir, objtypes, flags, hits, nMaxHits,
	                                                pSkipEnts, nSkipEnts, pForeignData, iForeignData, "RayWorldIntersectionWrapper(AI)", 0);
	return rwiResult;
}
//
//-----------------------------------------------------------------------------------------------------------
//

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetRefPointAtDefensePos(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	Vec3 vDefendSpot;
	float distance;
	pH->GetParam(2, vDefendSpot);
	pH->GetParam(3, distance);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return pH->EndFunction(0);
	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return pH->EndFunction();
	IAIObject* pTarget = pAIActor->GetAttentionTarget();
	if (!pTarget)
		return pH->EndFunction();
	Vec3 dir = (pTarget->GetPos() - vDefendSpot);
	float maxdist = dir.GetLength();
	distance = min(maxdist / 2, distance);
	Vec3 defendPos = dir.GetNormalizedSafe() * distance + vDefendSpot;
	pPipeUser->SetRefPointPos(defendPos);

	return pH->EndFunction(1);
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetFormationUpdateSight(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 2)
	{
		AIWarningID("<CScriptBind_AI::SetFormationUpdateSight> ", "WARNING: too few parameters");
		return pH->EndFunction();
	}

	GET_ENTITY(1);
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	float range = 0, minTime = 2, maxTime = 2;

	pH->GetParam(2, range);
	if (range > 0)
	{
		if (pH->GetParamCount() > 2)
		{
			pH->GetParam(3, minTime);
			if (pH->GetParamCount() > 3)
				pH->GetParam(4, maxTime);
			else
				maxTime = minTime;
		}
	}

	pAI->SetFormationUpdateSight(DEG2RAD(range), minTime, maxTime);

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetPathToFollow(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	const char* pathName = 0;
	pH->GetParam(2, pathName);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();
	if (pAI->CastToIAIActor())
		pAI->CastToIAIActor()->SetPathToFollow(pathName);

	return pH->EndFunction();
}
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetPathAttributeToFollow(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	bool bSpline = false;
	pH->GetParam(2, bSpline);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	if (pAI->CastToIAIActor())
		pAI->CastToIAIActor()->SetPathAttributeToFollow(bSpline);

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetTotalLengthOfPath(IFunctionHandler* pH)
{

	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	const char* pathName = 0;

	if (!pH->GetParam(2, pathName))
		return pH->EndFunction();

	Vec3 vTargetPos = pAI->GetPos();
	Vec3 vResult(ZERO);
	bool bLoop(false);
	float totalLength = 0.0f;

	gAIEnv.pNavigation->GetNearestPointOnPath(pathName, vTargetPos, vResult, bLoop, totalLength);

	return pH->EndFunction(totalLength);

}
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetNearestPointOnPath(IFunctionHandler* pH)
{

	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	const char* pathName = 0;

	if (!pH->GetParam(2, pathName))
		return pH->EndFunction();

	Vec3 vTargetPos;

	if (!pH->GetParam(3, vTargetPos))
		return pH->EndFunction();

	Vec3 vResult(ZERO);
	bool bLoop(false);
	float totalLength = 0.0f;

	gAIEnv.pNavigation->GetNearestPointOnPath(pathName, vTargetPos, vResult, bLoop, totalLength);

	return pH->EndFunction(Script::SetCachedVector(vResult, pH, 1));

}
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetPathSegNoOnPath(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	const char* pathName = 0;

	if (!pH->GetParam(2, pathName))
		return pH->EndFunction();

	Vec3 vTargetPos;
	Vec3 vResult(ZERO);
	bool bLoop(false);
	float totalLength = 0.0f;

	if (!pH->GetParam(3, vTargetPos))
		return pH->EndFunction();

	return pH->EndFunction(gAIEnv.pNavigation->GetNearestPointOnPath(pathName, vTargetPos, vResult, bLoop, totalLength));

}
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetPointOnPathBySegNo(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	const char* pathName = 0;

	if (!pH->GetParam(2, pathName))
		return pH->EndFunction();

	Vec3 vResult(ZERO);
	float segNo = 0.0f;

	if (!pH->GetParam(3, segNo))
		return pH->EndFunction();

	gAIEnv.pNavigation->GetPointOnPathBySegNo(pathName, vResult, segNo);
	gEnv->pAISystem->AddDebugLine(pAI->GetPos(), vResult, 0, 255, 0, 0.4f);

	return pH->EndFunction(vResult);

}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetPathLoop(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	const char* pathName = 0;

	if (!pH->GetParam(2, pathName))
		return pH->EndFunction();

	Vec3 vTargetPos(ZERO);
	Vec3 vResult(ZERO);
	bool bLoop(false);
	float totalLength = 0.0f;

	gAIEnv.pNavigation->GetNearestPointOnPath(pathName, vTargetPos, vResult, bLoop, totalLength);

	return pH->EndFunction(bLoop);

}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetPointListToFollow(IFunctionHandler* pH)
{

	GET_ENTITY(1);

	if (pH->GetParamCount() < 4)
	{
		AIWarningID("<SetPointListToFollow> ", "Too few parameters.");
		return pH->EndFunction();
	}
	if (pH->GetParamCount() > 5)
	{
		AIWarningID("<SetPointListToFollow> ", "Too many parameters.");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction(false);

	if (pH->GetParamType(2) != svtObject)
		return pH->EndFunction(false);

	SmartScriptTable theObj;

	if (!pH->GetParam(2, theObj))
		return pH->EndFunction(false);

	int nPoints = 0;
	if (!pH->GetParam(3, nPoints))
		return pH->EndFunction(false);

	bool bSpline = false;
	if (!pH->GetParam(4, bSpline))
		return pH->EndFunction(false);

	IAISystem::ENavigationType navType = IAISystem::NAV_FLIGHT;
	int navTypeScript = static_cast<int>(IAISystem::NAV_FLIGHT);

	if (pH->GetParamCount() > 4)
	{
		if (!pH->GetParam(5, navTypeScript))
			return pH->EndFunction(false);
		navType = static_cast<IAISystem::ENavigationType>(navTypeScript);
	}

	Vec3 vec;
	std::vector<Vec3> pointList;
	int i;
	for (i = 1; i <= nPoints; i++)
	{
		if (!theObj->GetAt(i, vec))
			break;
		pointList.push_back(vec);
	}

	if (i >= 2)
	{
		if (CPipeUser* pPipeUser = pAI->CastToCPipeUser())
			pPipeUser->SetPointListToFollow(pointList, navType, bSpline);

		return pH->EndFunction(true);
	}

	return pH->EndFunction(false);

}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetNearestPathOfTypeInRange(IFunctionHandler* pH)
{
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	Vec3 pos(pAI->GetPos());
	float range(0);
	int type(0);
	float devalue(0.0f);
	bool useStartNode(false);

	if (!pH->GetParam(2, pos))
		return pH->EndFunction();
	if (!pH->GetParam(3, range))
		return pH->EndFunction();
	if (!pH->GetParam(4, type))
		return pH->EndFunction();
	if (pH->GetParamCount() > 4)
		if (!pH->GetParam(5, devalue))
			return pH->EndFunction();
	if (pH->GetParamCount() > 5)
		if (!pH->GetParam(6, useStartNode))
			return pH->EndFunction();

	const char* pathName = gAIEnv.pNavigation->GetNearestPathOfTypeInRange(pAI, pos, range, type, devalue, useStartNode);

	return pH->EndFunction(pathName);
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetAlertness(IFunctionHandler* pH)
{
	GET_ENTITY(1);

	int alertness = 0;
	IAIObject* pAI = pEntity->GetAI();
	if (pAI)
	{
		alertness = pAI->GetProxy()->GetAlertnessState();
	}

	return pH->EndFunction(alertness);
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetGroupAlertness(IFunctionHandler* pH)
{
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (!pAIActor)
		return pH->EndFunction();

	int maxAlertness = 0;
	int groupID = pAI->GetGroupId();
	int n = GetAISystem()->GetGroupCount(groupID, IAISystem::GROUP_ENABLED);
	for (int i = 0; i < n; i++)
	{
		IAIObject* pMember = GetAISystem()->GetGroupMember(groupID, i, IAISystem::GROUP_ENABLED);
		if (pMember && pMember->GetProxy())
			maxAlertness = max(maxAlertness, pMember->GetProxy()->GetAlertnessState());
	}

	return pH->EndFunction(maxAlertness);
}

//====================================================================
// RegisterDamageRegion
//====================================================================
int CScriptBind_AI::RegisterDamageRegion(IFunctionHandler* pH)
{
	//	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	Sphere damageSphere;
	if (!pH->GetParam(2, damageSphere.radius))
		damageSphere.radius = -1.f;
	damageSphere.center = pEntity->GetWorldPos();
	GetAISystem()->RegisterDamageRegion(pEntity, damageSphere);
	return (pH->EndFunction());
}

//====================================================================
// GetDistanceAlongPath
//====================================================================
int CScriptBind_AI::GetDistanceAlongPath(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 2)
	{
		AIWarningID("<GetDistanceAlongPath> ", "Too few parameters.");
		return pH->EndFunction();
	}

	IEntity* pEntity1 = GetEntityFromParam(pH, 1);
	IEntity* pEntity2 = GetEntityFromParam(pH, 2);

	if (pEntity1 && pEntity2)
	{
		IAIObject* pAI = pEntity1->GetAI();
		bool bInit = false;
		if (pH->GetParamCount() > 2)
			pH->GetParam(3, bInit);
		IPuppet* pPuppet = CastToIPuppetSafe(pAI);
		if (pPuppet)
		{
			Vec3 pos(pEntity2->GetPos());
			return pH->EndFunction(pPuppet->GetDistanceAlongPath(pos, bInit));
		}
	}
	return pH->EndFunction(0);
}

//====================================================================
// IgnoreCurrentHideObject
//====================================================================
int CScriptBind_AI::IgnoreCurrentHideObject(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 1)
	{
		AIWarningID("<IgnoreCurrentHideObject> ", "Too few parameters.");
		return pH->EndFunction();
	}
	GET_ENTITY(1);

	float timeOut = 10.0f;
	if (pH->GetParamCount() > 1)
		pH->GetParam(2, timeOut);

	IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (pPipeUser)
		pPipeUser->IgnoreCurrentHideObject(timeOut);

	return pH->EndFunction(0);
}

//====================================================================
// GetCurrentHideAnchor
//====================================================================
int CScriptBind_AI::GetCurrentHideAnchor(IFunctionHandler* pH)
{
	if (pH->GetParamCount() < 1)
	{
		AIWarningID("<GetCurrentHideAnchor> ", "Too few parameters.");
		return pH->EndFunction();
	}

	GET_ENTITY(1);
	CPipeUser* pPipeUser = CastToCPipeUserSafe(pEntity->GetAI());
	if (pPipeUser && pPipeUser->m_CurrentHideObject.GetHideSpotType() == SHideSpotInfo::eHST_ANCHOR)
	{
		const char* szName = pPipeUser->m_CurrentHideObject.GetAnchorName();
		if (szName && szName[0])
		{
			return pH->EndFunction(szName);
		}
	}

	return pH->EndFunction();
}

//====================================================================
// FindStandbySpotInShape
//====================================================================
int CScriptBind_AI::FindStandbySpotInShape(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();
	const Vec3& reqPos = pAI->GetPos();

	const char* shapeName = 0;
	int type = 0;
	Vec3 targetPos(0, 0, 0);

	if (!pH->GetParam(2, shapeName))
		return pH->EndFunction();
	if (!shapeName)
	{
		AIWarningID("<FindStandbySpotInShape> ", "Invalid parameter 1.");
		return pH->EndFunction();
	}
	if (!pH->GetParam(3, targetPos))
		return pH->EndFunction();
	if (!pH->GetParam(4, type))
		return pH->EndFunction();

	const float TRESHOLD = cosf(DEG2RAD(120.0f / 2));

	IAIObject* bestObj = 0;
	float bestDist = FLT_MAX;
	AutoAIObjectIter it(GetAISystem()->GetFirstAIObjectInShape(OBJFILTER_TYPE, type, shapeName, false));
	for (; it->GetObject(); it->Next())
	{
		IAIObject* obj = it->GetObject();

		const Vec3& pos = obj->GetPos();
		Vec3 dirObjToTarget = targetPos - pos;
		Vec3 forward = obj->GetMoveDir();

		dirObjToTarget.z = 0.0f;
		dirObjToTarget.NormalizeSafe();
		forward.z = 0.0f;
		forward.NormalizeSafe();

		float dot = dirObjToTarget.Dot(forward);
		if (dot < TRESHOLD)
			continue;

		float d = Distance::Point_Point(pos, reqPos);

		if (d < bestDist)
		{
			bestObj = obj;
			bestDist = d;
		}
	}

	if (bestObj)
		return pH->EndFunction(bestObj->GetName());

	return pH->EndFunction();
}

//====================================================================
// FindStandbySpotInSphere
//====================================================================
int CScriptBind_AI::FindStandbySpotInSphere(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();
	const Vec3& reqPos = pAI->GetPos();

	int type = 0;
	Vec3 spherePos;
	float sphereRad;
	Vec3 targetPos(0, 0, 0);

	if (!pH->GetParam(2, spherePos))
		return pH->EndFunction();
	if (!pH->GetParam(3, sphereRad))
		return pH->EndFunction();
	if (!pH->GetParam(4, targetPos))
		return pH->EndFunction();
	if (!pH->GetParam(5, type))
		return pH->EndFunction();

	const float TRESHOLD = cosf(DEG2RAD(120.0f / 2));

	IAIObject* bestObj = 0;
	float bestDist = FLT_MAX;
	AutoAIObjectIter it(gAIEnv.pAIObjectManager->GetFirstAIObjectInRange(OBJFILTER_TYPE, type, spherePos, sphereRad, false));
	for (; it->GetObject(); it->Next())
	{
		IAIObject* obj = it->GetObject();

		const Vec3& pos = obj->GetPos();
		Vec3 dirObjToTarget = targetPos - pos;
		Vec3 forward = obj->GetMoveDir();

		dirObjToTarget.z = 0.0f;
		dirObjToTarget.NormalizeSafe();
		forward.z = 0.0f;
		forward.NormalizeSafe();

		float dot = dirObjToTarget.Dot(forward);
		if (dot < TRESHOLD)
			continue;

		float d = Distance::Point_Point(pos, reqPos);

		if (d < bestDist)
		{
			bestObj = obj;
			bestDist = d;
		}
	}

	if (bestObj)
		return pH->EndFunction(bestObj->GetName());

	return pH->EndFunction();
}

//====================================================================
// GetProbableTargetPosition
//====================================================================
int CScriptBind_AI::GetProbableTargetPosition(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (!pPipeUser)
		return pH->EndFunction();
	return pH->EndFunction(pPipeUser->GetProbableTargetPosition());
}

//====================================================================
// GetObjectRadius
//====================================================================
int CScriptBind_AI::GetObjectRadius(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	if (IAIObject* aiObject = pEntity ? pEntity->GetAI() : 0)
		return pH->EndFunction(aiObject->GetRadius());
	return pH->EndFunction(0);
}

//====================================================================
// GetEnclosingGenericShapeOfType
//====================================================================
int CScriptBind_AI::GetEnclosingGenericShapeOfType(IFunctionHandler* pH)
{
	Vec3 pos(0, 0, 0);
	int type = 0;
	int checkHeight = 0;

	if (!pH->GetParam(1, pos))
		return pH->EndFunction();
	if (!pH->GetParam(2, type))
		return pH->EndFunction();
	if (!pH->GetParam(3, checkHeight))
		return pH->EndFunction();

	const char* name = GetAISystem()->GetEnclosingGenericShapeOfType(pos, type, checkHeight != 0);
	if (name)
		return pH->EndFunction(name);

	return pH->EndFunction();
}

//====================================================================
// IsPointInsideGenericShape
//====================================================================
int CScriptBind_AI::IsPointInsideGenericShape(IFunctionHandler* pH)
{
	Vec3 pos(0, 0, 0);
	const char* shapeName = 0;
	int checkHeight = 0;

	if (!pH->GetParam(1, pos))
		return pH->EndFunction(0);
	if (!pH->GetParam(2, shapeName))
		return pH->EndFunction(0);
	if (!pH->GetParam(3, checkHeight))
		return pH->EndFunction(0);

	if (!shapeName)
		return pH->EndFunction(0);

	return pH->EndFunction(GetAISystem()->IsPointInsideGenericShape(pos, shapeName, checkHeight != 0) ? 1 : 0);
}

//====================================================================
// DistanceToGenericShape
//====================================================================
int CScriptBind_AI::DistanceToGenericShape(IFunctionHandler* pH)
{
	Vec3 pos(0, 0, 0);
	const char* shapeName = 0;
	int checkHeight = 0;

	if (!pH->GetParam(1, pos))
		return pH->EndFunction(0);
	if (!pH->GetParam(2, shapeName))
		return pH->EndFunction(0);
	if (!pH->GetParam(3, checkHeight))
		return pH->EndFunction(0);

	if (!shapeName)
		return pH->EndFunction(0);

	return pH->EndFunction(GetAISystem()->DistanceToGenericShape(pos, shapeName, checkHeight != 0));
}

//====================================================================
// ConstrainPointInsideGenericShape
//====================================================================
int CScriptBind_AI::ConstrainPointInsideGenericShape(IFunctionHandler* pH)
{
	Vec3 pos(0, 0, 0);
	const char* shapeName = 0;
	int checkHeight = 0;

	if (!pH->GetParam(1, pos))
		return pH->EndFunction(0);
	if (!pH->GetParam(2, shapeName))
		return pH->EndFunction(0);
	if (!pH->GetParam(3, checkHeight))
		return pH->EndFunction(0);

	if (!shapeName)
		return pH->EndFunction(0);

	GetAISystem()->ConstrainInsideGenericShape(pos, shapeName, checkHeight != 0);

	return pH->EndFunction(pos);
}

//====================================================================
// CreateTempGenericShapeBox
//====================================================================
int CScriptBind_AI::CreateTempGenericShapeBox(IFunctionHandler* pH)
{
	Vec3 c(0, 0, 0);
	float r = 0;
	float height = 0;
	int type = 0;

	if (!pH->GetParams(c, r, height, type))
		return pH->EndFunction();

	Vec3 points[4];

	points[0].Set(c.x - r, c.y - r, c.z);
	points[1].Set(c.x + r, c.y - r, c.z);
	points[2].Set(c.x + r, c.y + r, c.z);
	points[3].Set(c.x - r, c.y + r, c.z);

	const char* name = GetAISystem()->CreateTemporaryGenericShape(points, 4, height, type);
	if (name)
		return pH->EndFunction(name);
	return pH->EndFunction();
}

//====================================================================
// SetTerritoryShapeName
//====================================================================
int CScriptBind_AI::SetTerritoryShapeName(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	//retrieve the entity
	if (pEntity)
	{
		if (IAIObject* pAIObject = pEntity->GetAI())
		{
			const char* szShapeName = 0;
			if (pH->GetParam(2, szShapeName) && szShapeName)
			{
				if (CAIActor* pAIActor = pAIObject->CastToCAIActor())
				{
					pAIActor->SetTerritoryShapeName(szShapeName);
					return pH->EndFunction(1);
				}
			}
		}
	}

	return pH->EndFunction();
}

//====================================================================
// NotifySurpriseEntityAction
//====================================================================
int CScriptBind_AI::NotifySurpriseEntityAction(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	float radius = 0;
	int note = 0;
	if (!pH->GetParam(2, radius))
		return pH->EndFunction();
	if (!pH->GetParam(3, note))
		return pH->EndFunction();

	EntityId miracleId = pEntity->GetId();

	// Send a signal to all entities that see the specified entity, that that entity have just done something miraculous.
	AutoAIObjectIter it(gAIEnv.pAIObjectManager->GetFirstAIObjectInRange(OBJFILTER_TYPE, AIOBJECT_ACTOR, pEntity->GetWorldPos(), radius, false));
	for (; it->GetObject(); it->Next())
	{
		IAIActor* pAIActor = it->GetObject()->CastToIAIActor();
		if (pAIActor)
		{
			IAIObject* pAttTarget = pAIActor->GetAttentionTarget();
			if (pAttTarget && (pAttTarget->GetEntityID() == miracleId))
			{
				IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
				pData->iValue = note;
				pAIActor->SetSignal(1, "SURPRISE_ACTION", pEntity, pData);
			}
		}
	}

	return pH->EndFunction();
}

//====================================================================
// DebugReportHitDamage
//====================================================================
int CScriptBind_AI::DebugReportHitDamage(IFunctionHandler* pH)
{
	IEntity* pVictimEntity = GetEntityFromParam(pH, 1);
	IEntity* pShooterEntity = GetEntityFromParam(pH, 2);

	float damage = 0;
	const char* material = 0;

	if (!pH->GetParam(3, damage))
		return pH->EndFunction();
	if (!pH->GetParam(4, material))
		return pH->EndFunction();

	gEnv->pAISystem->DebugReportHitDamage(pVictimEntity, pShooterEntity, damage, material);

	return pH->EndFunction();
}

//====================================================================
// CanMelee
//====================================================================
int CScriptBind_AI::CanMelee(IFunctionHandler* pH)
{
	GET_ENTITY(1);

	if (!pEntity)
		return pH->EndFunction(0);
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction(0);

	SAIWeaponInfo weaponInfo;
	pAIObject->GetProxy()->QueryWeaponInfo(weaponInfo);
	if (weaponInfo.canMelee)
		return pH->EndFunction(0);

	return pH->EndFunction(0);
}

//====================================================================
// IsMoving
//====================================================================
int CScriptBind_AI::IsMoving(IFunctionHandler* pH)
{
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction(0);
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction(0);
	IAIActor* pActor = pAIObject->CastToIAIActor();
	if (!pActor)
		return pH->EndFunction(0);

	int run = 0;
	if (!pH->GetParam(2, run))
		return pH->EndFunction(0);

	switch (run)
	{
	case 0:
		run = (int)AISPEED_WALK;
		break;
	case 1:
		run = (int)AISPEED_RUN;
		break;
	case 2:
		run = (int)AISPEED_WALK;
		break;
	default:
		run = (int)AISPEED_SPRINT;
		break;
	}

	int idx = MovementUrgencyToIndex(pActor->GetState().fMovementUrgency);

	if (idx < run)
		return 0;

	if (pActor->GetState().fDesiredSpeed > 0.0f && pActor->GetState().vMoveDir.GetLength() > 0.01f)
		return pH->EndFunction(1);
	return pH->EndFunction(0);
}

//====================================================================
// IsEnabled
//====================================================================
int CScriptBind_AI::IsEnabled(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	IAIObject* pAIObject = NULL;

	if (pH->GetParamType(1) == svtPointer)
	{
		GET_ENTITY(1);
		if (pEntity)
		{
			pAIObject = pEntity->GetAI();
		}
	}
	else if (pH->GetParamType(1) == svtString)
	{
		const char* objName = "";
		pH->GetParam(1, objName);
		pAIObject = (IAIObject*)gAIEnv.pAIObjectManager->GetAIObjectByName(0, objName);
	}

	if (!pAIObject)
	{
		return pH->EndFunction();
	}

	return pH->EndFunction(pAIObject->IsEnabled());
}

//====================================================================
// EnableCoverFire
//====================================================================
int CScriptBind_AI::EnableCoverFire(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();

	IPuppet* pPuppet = pAIObject->CastToIPuppet();
	if (pPuppet)
	{
		bool bEnable = false;
		pH->GetParam(2, bEnable);
		pPuppet->EnableCoverFire(bEnable);
	}

	return pH->EndFunction();
}

//====================================================================
// EnableFire
//====================================================================
int CScriptBind_AI::EnableFire(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();

	if (!pAIObject)
		return pH->EndFunction();

	IPuppet* pPuppet = pAIObject->CastToIPuppet();
	if (pPuppet)
	{
		bool bEnable = false;
		pH->GetParam(2, bEnable);
		pPuppet->EnableFire(bEnable);
	}

	return pH->EndFunction();
}

//====================================================================
// EnableFire
//====================================================================
int CScriptBind_AI::IsFireEnabled(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	if (!pEntity)
		return pH->EndFunction();

	IAIObject* pAIObject = pEntity->GetAI();

	if (!pAIObject)
		return pH->EndFunction();

	IPuppet* pPuppet = pAIObject->CastToIPuppet();
	if (pPuppet && pPuppet->IsFireEnabled())
		return pH->EndFunction(true);

	return pH->EndFunction();
}

//====================================================================
// CanFireInStance
//====================================================================
int CScriptBind_AI::CanFireInStance(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS_MIN(2);

	bool bResult = false;

	GET_ENTITY(1);
	if (pEntity)
	{
		IAIObject* pAIObject = pEntity->GetAI();
		if (pAIObject)
		{
			CPuppet* pPuppet = pAIObject->CastToCPuppet();
			if (pPuppet)
			{
				int iStance = STANCE_NULL;
				if (!pH->GetParam(2, iStance) || iStance <= STANCE_NULL || iStance >= STANCE_LAST)
				{
					AIWarningID("<CScriptBind_AI> ", "CanFireInStance: Invalid stance (%d)", iStance);
				}
				else
				{
					float fDistanceRatio = 0.9f;
					pH->GetParam(3, fDistanceRatio);
					bResult = pPuppet->CanFireInStance((EStance)iStance, fDistanceRatio);
				}
			}
		}
	}

	return pH->EndFunction(bResult);
}

//====================================================================
// SetUseSecondaryVehicleWeapon
//====================================================================
int CScriptBind_AI::SetUseSecondaryVehicleWeapon(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS_MIN(2);

	GET_ENTITY(1);
	if (pEntity)
	{
		IAIObject* pAIObject = pEntity->GetAI();
		IAIActorProxy* pAIProxy = pAIObject ? pAIObject->GetProxy() : NULL;
		if (pAIProxy)
		{
			bool bUseSecondary = false;
			pH->GetParam(2, bUseSecondary);

			pAIProxy->SetUseSecondaryVehicleWeapon(bUseSecondary);
		}
	}

	return pH->EndFunction();
}

//
//====================================================================
int CScriptBind_AI::Animation(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();

	const char* animName;
	int iType;
	pH->GetParam(2, iType);
	EAnimationMode type = (EAnimationMode)iType;
	pH->GetParam(3, animName);

	if (pAIObject->GetProxy())
		pAIObject->GetProxy()->SetAGInput(type == AIANIM_ACTION ? AIAG_ACTION : AIAG_SIGNAL, animName);

	return pH->EndFunction();
}

// ============================================================================
//	Set a Mannequin animation tag.
//
//	In:		The function handler (NULL is invalid!)
//	In:		The script handle of the entity on which to set the animation tag.
//	In:		The name of the animation tag that should be set (case insensitive).
//
//	Returns:	A default result code (in Lua: void).
//
int CScriptBind_AI::SetAnimationTag(
  IFunctionHandler* pH, ScriptHandle entityID, const char* tagName)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityID.n);
	if (pEntity == NULL)
	{
		AIWarningID("CScriptBind_Actor::SetAnimationTag()", "Tried to set animation tag on non-existing entity with ID [%d]. ", (EntityId)entityID.n);
		return pH->EndFunction();
	}
	if ((tagName == NULL) || (tagName[0] == '\0'))
	{
		AIWarningID("CScriptBind_Actor::SetAnimationTag()", "Animation tag name was empty (entity name ='%s')!", pEntity->GetName());
		return pH->EndFunction();
	}
	IAIObject* pAIObject = pEntity->GetAI();
	if (pAIObject == NULL)
	{
		AIWarningID("CScriptBind_Actor::SetAnimationTag()", "Entity '%s' is not registered as AI!", pEntity->GetName());
		return pH->EndFunction();
	}
	CPipeUser* pPipeUser = pAIObject->CastToCPipeUser();
	if (pPipeUser == NULL)
	{
		AIWarningID("CScriptBind_Actor::SetAnimationTag()", "Entity '%s' is not registered as pipe-user!", pEntity->GetName());
		return pH->EndFunction();
	}

	SOBJECTSTATE& state = pPipeUser->GetState();
	uint32 tagCrc = CCrc32::ComputeLowercase(tagName);
	const aiMannequin::SCommand* pCommand = state.mannequinRequest.CreateSetTagCommand(tagCrc);
	if (pCommand == NULL)
	{
		AIWarningID("CScriptBind_Actor::SetAnimationTag()", "Could not add a set tag command to the mannequin request this frame on entity '%s' (tag name = '%s')!", pEntity->GetName(), tagName);
		return pH->EndFunction();
	}

	return pH->EndFunction();
}

// ============================================================================
//	Clear a Mannequin animation tag group.
//
//	In:		The function handler (NULL is invalid!)
//	In:		The script handle of the entity on which to clear the animation tag.
//	In:		The name of any animation tag in the group that we want to clear
//          (case insensitive).
//
//	Returns:	A default result code (in Lua: void).
//
int CScriptBind_AI::ClearAnimationTag(
  IFunctionHandler* pH, ScriptHandle entityID, const char* tagName)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityID.n);
	if (pEntity == NULL)
	{
		AIWarningID("CScriptBind_Actor::ClearAnimationTag()", "Tried to clear animation tag on non-existing entity with ID [%d]. ", (EntityId)entityID.n);
		return pH->EndFunction();
	}
	if ((tagName == NULL) || (tagName[0] == '\0'))
	{
		AIWarningID("CScriptBind_Actor::ClearAnimationTag()", "Animation tag name was empty (entity name ='%s')!", pEntity->GetName());
		return pH->EndFunction();
	}
	IAIObject* pAIObject = pEntity->GetAI();
	if (pAIObject == NULL)
	{
		AIWarningID("CScriptBind_Actor::ClearAnimationTag()", "Entity '%s' is not registered as AI!", pEntity->GetName());
		return pH->EndFunction();
	}
	CPipeUser* pPipeUser = pAIObject->CastToCPipeUser();
	if (pPipeUser == NULL)
	{
		AIWarningID("CScriptBind_Actor::ClearAnimationTag()", "Entity '%s' is not registered as pipe-user!", pEntity->GetName());
		return pH->EndFunction();
	}

	SOBJECTSTATE& state = pPipeUser->GetState();
	uint32 tagCrc = CCrc32::ComputeLowercase(tagName);
	const aiMannequin::SCommand* pCommand = state.mannequinRequest.CreateClearTagCommand(tagCrc);
	if (pCommand == NULL)
	{
		AIWarningID("CScriptBind_Actor::ClearAnimationTag()", "Could not add a clear tag command to the mannequin request this frame on entity '%s' (tag name = '%s')!", pEntity->GetName(), tagName);
		return pH->EndFunction();
	}

	return pH->EndFunction();
}

int CScriptBind_AI::ProcessBalancedDamage(IFunctionHandler* pH)
{
	IEntity* pShooterEntity = GetEntityFromParam(pH, 1);
	IEntity* pTargetEntity = GetEntityFromParam(pH, 2);
	if (!pShooterEntity || !pTargetEntity)
		return pH->EndFunction(0);

	float damage;
	const char* damageType;

	pH->GetParam(3, damage);
	pH->GetParam(4, damageType);

	return pH->EndFunction(GetAISystem()->ProcessBalancedDamage(pShooterEntity, pTargetEntity, damage, damageType));
}

//
//====================================================================
int CScriptBind_AI::SetRefpointToAnchor(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(5);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction();
	IAIActor* pAIActor = pAIObject->CastToIAIActor();
	if (!pAIActor)
		return pH->EndFunction();
	IPipeUser* pPipeUser = pAIObject->CastToIPipeUser();
	if (!pPipeUser)
		return pH->EndFunction();

	float rangeMin = 0.0f;
	float rangeMax = 1.0f;
	pH->GetParam(2, rangeMin);
	pH->GetParam(3, rangeMax);

	int findType = 0;
	pH->GetParam(4, findType);

	int findMethod = AIANCHOR_NEAREST;
	pH->GetParam(5, findMethod);

	bool bSeesTarget = (findMethod & AIANCHOR_SEES_TARGET) != 0;
	bool bBehind = (findMethod & AIANCHOR_BEHIND) != 0;
	findMethod &= ~(AIANCHOR_SEES_TARGET | AIANCHOR_BEHIND);

	Vec3 nearestPos(0, 0, 0);
	//	float nearestDist = FLT_MAX;
	float bestCandidatreDist(findMethod == AIANCHOR_FARTHEST ? FLT_MIN : FLT_MAX);

	const Vec3& searchPos = pAIObject->GetPos();
	Vec3 targetPos(searchPos);
	if (IAIObject* pAttTarget = pAIActor->GetAttentionTarget())
		targetPos = pAttTarget->GetPos();

	AutoAIObjectIter it(gAIEnv.pAIObjectManager->GetFirstAIObjectInRange(OBJFILTER_TYPE, findType, searchPos, rangeMax, false));
	for (; it->GetObject(); it->Next())
	{
		IAIObject* obj = it->GetObject();
		if (!obj->IsEnabled()) continue;

		const Vec3& pos = obj->GetPos() + Vec3(0.f, 0.f, .8f); // put it a bit up - normally anchor would be on the ground. BAD for aliens/3D?

		float d = Distance::Point_PointSq(searchPos, pos);
		if (d < sqr(rangeMin))
			continue;

		if (bBehind)
		{
			Vec3 toTarget(targetPos - searchPos);
			Vec3 toAnchor(pos - searchPos);
			toTarget.Normalize();
			toAnchor.Normalize();
			float dotProd(toTarget * toAnchor);
			if (dotProd > 0.f)//-.5f)
				continue;
		}

		if (findMethod == AIANCHOR_FARTHEST && d > bestCandidatreDist ||
		    findMethod != AIANCHOR_FARTHEST && d < bestCandidatreDist)
		{
			ray_hit hit;
			int hitsCount = RayWorldIntersectionWrapper(pos, targetPos - pos, ent_static | ent_terrain | ent_sleeping_rigid | ent_rigid,
			                                            rwi_stop_at_pierceable, &hit, 1);
			if (bSeesTarget && hitsCount == 0 || !bSeesTarget && hitsCount)
			{
				gEnv->pAISystem->AddDebugLine(searchPos, hit.pt, 255, 0, 0, 10.0f);
				bestCandidatreDist = d;
				nearestPos = pos;
			}
		}
	}

	if (!nearestPos.IsZero())
	{
		gEnv->pAISystem->AddDebugLine(searchPos, nearestPos, 255, 255, 255, 10.0f);
		pPipeUser->SetRefPointPos(nearestPos);
		return pH->EndFunction(true);
	}

	return pH->EndFunction(false);
}

//
//====================================================================
int CScriptBind_AI::SetRefpointToPunchableObject(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();
	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return pH->EndFunction();
	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return pH->EndFunction();
	IAIObject* pAttTarget = pAIActor->GetAttentionTarget();
	if (!pAttTarget)
		return pH->EndFunction();

	float range = 10.0f;
	pH->GetParam(2, range);

	Vec3 posOut;
	Vec3 dirOut;
	IEntity* objEntOut = 0;

	if (!GetAISystem()->GetNearestPunchableObjectPosition(pAI, pAI->GetPos(), range, pAttTarget->GetPos(),
	                                                      0.5f, 4.0f, 10.0f, 1550.0f, posOut, dirOut, &objEntOut))
		return pH->EndFunction();

	if (!objEntOut)
		return pH->EndFunction();

	pPipeUser->SetRefPointPos(posOut, dirOut);

	GetAISystem()->AddDebugLine(posOut, posOut + Vec3(0, 0, 4), 255, 255, 255, 10.0f);
	GetAISystem()->AddDebugLine(posOut + Vec3(0, 0, 1), posOut + Vec3(0, 0, 1) + dirOut, 255, 255, 255, 10.0f);

	return pH->EndFunction(objEntOut->GetScriptTable());
}

//
//====================================================================
int CScriptBind_AI::MeleePunchableObject(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	IEntity* pUser = GetEntityFromParam(pH, 1);
	if (!pUser)
	{
		gEnv->pAISystem->LogComment("<CScriptBind_AI::MeleePunchableObject> ", "ERROR: Specified entity id (param 1) not found!");
		return pH->EndFunction(0);
	}

	IEntity* pEntity = GetEntityFromParam(pH, 2);
	if (!pEntity)
	{
		gEnv->pAISystem->LogComment("<CScriptBind_AI::MeleePunchableObject> ", "ERROR: Specified entity id (param 2) not found!");
		return pH->EndFunction(0);
	}

	IAIObject* pAI = pUser->GetAI();
	if (!pAI)
		return pH->EndFunction(0);
	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return pH->EndFunction();
	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return pH->EndFunction();
	IAIObject* pAttTarget = pAIActor->GetAttentionTarget();
	if (!pAttTarget)
		return pH->EndFunction();

	// Make sure the AI is on spot
	const float dirThr = cosf(DEG2RAD(45.0f));
	const Vec3& standPos = pPipeUser->GetRefPoint()->GetPos();
	const Vec3& standDir = pPipeUser->GetRefPoint()->GetEntityDir();
	if (Distance::Point_Point2D(standPos, pAI->GetPos()) > 0.4f || standDir.Dot(pAI->GetEntityDir()) < dirThr)
		return pH->EndFunction(0);

	Vec3 origPos;
	if (!pH->GetParam(3, origPos))
		return pH->EndFunction(0);

	if (Distance::Point_PointSq(pEntity->GetWorldPos(), origPos) < 0.5f)
	{
		if (pEntity && pEntity->GetPhysics())
		{
			pe_status_dynamics dyn;

			pEntity->GetPhysics()->GetStatus(&dyn);
			float mass = dyn.mass;

			Vec3 vel = pAttTarget->GetPos() - pAI->GetPos(); // standDir;
			vel.Normalize();
			vel += pAI->GetEntityDir();
			vel.z += 0.2f;
			vel.Normalize();
			vel *= 18.0f;

			Vec3 pt = standPos;
			pt.x += cry_random(-0.3f, 0.3f);
			pt.y += cry_random(-0.3f, 0.3f);
			pt.z += 0.6f;
			pEntity->AddImpulse(-1, pt, vel * mass, true, 1);

			return pH->EndFunction(1);
		}
	}

	return pH->EndFunction(0);
}

//
//====================================================================
int CScriptBind_AI::IsPunchableObjectValid(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	IEntity* pUser = GetEntityFromParam(pH, 1);
	if (!pUser)
	{
		gEnv->pAISystem->LogComment("<CScriptBind_AI::MeleePunchableObject> ", "ERROR: Specified entity id (param 1) not found!");
		return pH->EndFunction(0);
	}

	IEntity* pObject = GetEntityFromParam(pH, 2);
	if (!pObject)
	{
		gEnv->pAISystem->LogComment("<CScriptBind_AI::MeleePunchableObject> ", "ERROR: Specified entity id (param 2) not found!");
		return pH->EndFunction(0);
	}

	IAIObject* pAI = pUser->GetAI();
	if (!pAI)
		return pH->EndFunction(0);
	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return pH->EndFunction(0);

	// Make sure the AI is on spot
	const float dirThr = cosf(DEG2RAD(45.0f));
	const Vec3& standPos = pPipeUser->GetRefPoint()->GetPos();
	const Vec3& standDir = pPipeUser->GetRefPoint()->GetEntityDir();
	if (Distance::Point_Point2D(standPos, pUser->GetWorldPos()) > 0.4f || standDir.Dot(pAI->GetEntityDir()) < dirThr)
		return pH->EndFunction(0);

	Vec3 origPos;
	if (!pH->GetParam(3, origPos))
		return pH->EndFunction(0);

	if (Distance::Point_PointSq(pObject->GetWorldPos(), origPos) < 0.5f)
	{
		return pH->EndFunction(1);
	}

	return pH->EndFunction(0);
}

struct SSortedAIObject
{
	SSortedAIObject(IAIObject* pObj) : pObj(pObj), w(0) {}
	bool operator<(const SSortedAIObject& rhs) const { return w < rhs.w; }
	IAIObject* pObj;
	float      w;
};

//
//====================================================================
bool CScriptBind_AI::GetGroupSpatialProperties(IAIObject* pAI, float& offset, Vec3& avgGroupPos, Vec3& targetPos, Vec3& dirToTarget, Vec3& normToTarget)
{
	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return false;

	int groupId = pAI->GetGroupId();

	const Vec3& requesterPos = pAI->GetPos();

	targetPos.Set(0, 0, 0);
	if (IAIObject* pAttTarget = pAIActor->GetAttentionTarget())
		targetPos = pAttTarget->GetPos();
	else if (IAIObject* pBeacon = GetAISystem()->GetBeacon(groupId))
		targetPos = pBeacon->GetPos();

	if (targetPos.IsZero())
		targetPos = requesterPos + pAI->GetEntityDir() * 30.0f;

	avgGroupPos.Set(0, 0, 0);
	float avgGroupPosWeight = 0;

	std::vector<SSortedAIObject> group;

	AutoAIObjectIter it(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_GROUP, groupId));
	for (; it->GetObject(); it->Next())
	{
		IAIObject* pObj = it->GetObject();
		if (!pObj->IsEnabled()) continue;

		IAIActorProxy* pAIActorProxy = pObj->GetProxy();
		if (pAIActorProxy && pAIActorProxy->IsCurrentBehaviorExclusive())
			continue;

		const Vec3& pos = pObj->GetPos();

		avgGroupPos += pos;
		avgGroupPosWeight += 1.0f;

		group.push_back(SSortedAIObject(pObj));
	}

	if (avgGroupPosWeight > 0.0f)
		avgGroupPos /= avgGroupPosWeight;
	else
		return false;

	dirToTarget = targetPos - requesterPos;
	dirToTarget.z = 0;
	dirToTarget.Normalize();
	normToTarget.Set(dirToTarget.y, -dirToTarget.x, 0);

	for (unsigned i = 0, ni = group.size(); i < ni; ++i)
		group[i].w = normToTarget.Dot(group[i].pObj->GetPos() - avgGroupPos);

	std::sort(group.begin(), group.end());

	unsigned selfIdx = 0;
	for (unsigned i = 0, ni = group.size(); i < ni; ++i)
	{
		if (group[i].pObj == pAI)
		{
			selfIdx = i;
			break;
		}
	}

	offset = (float)selfIdx - group.size() * 0.5f;

	CCCPOINT(CScriptBind_AI_GetGroupSpatialProperties);

	return true;
}

//
//====================================================================
int CScriptBind_AI::GetDirLabelToPoint(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction(-1);
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction(-1);

	Vec3 pos;
	if (!pH->GetParam(2, pos))
		return pH->EndFunction(-1);

	Vec3 forw = pAI->GetEntityDir();
	forw.z = 0;
	forw.NormalizeSafe();
	Vec3 right(forw.y, -forw.x, 0);

	Vec3 dir = pos - pAI->GetPos();
	dir.NormalizeSafe();

	const float upThr = sinf(DEG2RAD(20.0f));
	if (dir.z > upThr)
		return pH->EndFunction(4); // up

	float x = forw.x * dir.x + forw.y * dir.y;
	float y = right.x * dir.x + right.y * dir.y;

	float angle = atan2f(y, x);

	// Prefer sides slightly
	if (fabsf(angle) <= gf_PI * 0.2f)
		return pH->EndFunction(0); // front
	else if (fabsf(angle) >= gf_PI * 0.8f)
		return pH->EndFunction(1); // back
	else if (angle < 0)
		return pH->EndFunction(2); // left
	else
		return pH->EndFunction(3); // right
}

//
//====================================================================
int CScriptBind_AI::GetPredictedPosAlongPath(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	IPuppet* pPuppet = pAI->CastToIPuppet();
	if (!pPuppet)
		return pH->EndFunction();

	float time;
	if (!pH->GetParam(2, time))
		return pH->EndFunction();

	SmartScriptTable retPosTable;
	Vec3 retPos;

	if (!pH->GetParam(3, retPosTable))
		return pH->EndFunction();

	pe_status_dynamics dyn;
	IPhysicalEntity* phys = pEntity->GetPhysics();
	if (!phys)
		return pH->EndFunction();
	phys->GetStatus(&dyn);

	if (pPuppet->GetPosAlongPath(time * dyn.v.GetLength(), false, retPos))
	{
		retPosTable->SetValue("x", retPos.x);
		retPosTable->SetValue("y", retPos.y);
		retPosTable->SetValue("z", retPos.z);
		return pH->EndFunction(true);
	}

	return pH->EndFunction();

}

//====================================================================
// GetBiasedDirection
//====================================================================
int CScriptBind_AI::GetBiasedDirection(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetBiasedDirection(): wrong parameter 1");
		return pH->EndFunction(0);
	}

	Vec3 vPoint;

	if (pH->GetParam(2, vPoint) == false)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.GetBiasedDirection(): wrong type of parameter 2");
		return pH->EndFunction(0);
	}

	float fBias;

	if (pH->GetParam(3, fBias) == false)
	{
		fBias = 1.0f;
	}

	Vec3 vEntityPos = pEntity->GetPos();
	Vec3 vEntityDir = pEntity->GetAI()->GetViewDir();

	Vec3 vDiff = vPoint - vEntityPos;

	float fDotFront = (vDiff.x * vEntityDir.x) + (vDiff.y * vEntityDir.y);
	float fDotLeft = (vDiff.x * -vEntityDir.y) + (vDiff.y * vEntityDir.x);

	int iDir;

	if (fDotFront > 0 && fDotFront > fBias)
	{
		iDir = DIR_NORTH;
	}
	else if (fDotFront < 0 && fDotFront < (-fBias))
	{
		iDir = DIR_SOUTH;
	}
	else if (fDotLeft < 0.0f)
	{
		iDir = DIR_EAST;
	}
	else
	{
		iDir = DIR_WEST;
	}

	return pH->EndFunction(iDir);
}

//====================================================================
// SetAttentiontarget
//====================================================================
int CScriptBind_AI::SetAttentiontarget(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetAttentiontarget(): wrong parameter 1");
		return pH->EndFunction(0);
	}

	IEntity* pEntity2 = GetEntityFromParam(pH, 2);

	if (pEntity2 == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetAttentiontarget(): wrong parameter 2");
		return pH->EndFunction(0);
	}

	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetAttentiontarget(): entity of parameter 1 doesn't have AI");
		return pH->EndFunction(0);
	}

	CAIActor* pAIActor = pAIObject->CastToCAIActor();
	if (!pAIActor)
	{
		AIWarningID("<CScriptBind_AI> ", "AI.SetAttentiontarget(): entity of parameter 1 is not an AI Actor");
		return pH->EndFunction(0);
	}

	if (IAIObject* pObject2 = pEntity2->GetAI())
	{
		if (CAIActor* pAIActor2 = pObject2->CastToCAIActor())
		{
			pAIActor->SetAttentionTarget(GetWeakRef(pAIActor2));
		}
	}

	return pH->EndFunction(1);
}

int CScriptBind_AI::RegisterInterestingEntity(IFunctionHandler* pH, ScriptHandle entityId, float radius, float baseInterest, const char* aiAction, Vec3 offset, float pause, int shared)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n);
	if (!pEntity)
		return pH->EndFunction();

	ICentralInterestManager* pCIM = gEnv->pAISystem->GetCentralInterestManager();
	pCIM->ChangeInterestingEntityProperties(pEntity, radius, baseInterest, aiAction, offset, pause, shared);

	return pH->EndFunction();
}

int CScriptBind_AI::UnregisterInterestingEntity(IFunctionHandler* pH, ScriptHandle entityId)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n);
	if (!pEntity)
		return pH->EndFunction();

	ICentralInterestManager* pCIM = gEnv->pAISystem->GetCentralInterestManager();

	pCIM->DeregisterInterestingEntity(pEntity);

	return pH->EndFunction();
}

int CScriptBind_AI::RegisterInterestedActor(IFunctionHandler* pH, ScriptHandle entityId, float fInterestFilter, float fAngleInDegrees)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n);
	if (!pEntity)
		return pH->EndFunction();

	ICentralInterestManager* pCIM = gEnv->pAISystem->GetCentralInterestManager();

	clamp_tpl(fAngleInDegrees, 0.f, 360.f);
	float fAngleCos = cos(DEG2RAD(.5f * fAngleInDegrees));

	pCIM->ChangeInterestedAIActorProperties(pEntity, fInterestFilter, fAngleCos);
	return pH->EndFunction();
}

int CScriptBind_AI::UnregisterInterestedActor(IFunctionHandler* pH, ScriptHandle entityId)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n);
	if (!pEntity)
		return pH->EndFunction();

	ICentralInterestManager* pCIM = gEnv->pAISystem->GetCentralInterestManager();
	pCIM->DeregisterInterestedAIActor(pEntity);

	return pH->EndFunction();
}

//====================================================================
// IsTakingCover
//====================================================================
int CScriptBind_AI::IsTakingCover(IFunctionHandler* pH)
{
	GET_ENTITY(1);

	// Check entity
	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "IsTakingCover: no entity given parameter 1");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "IsTakingCover: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
	{
		AIWarningID("<CScriptBind_AI> ", "IsTakingCover: Entity [%s] is registered in AI but not as a PipeUser.", pEntity->GetName());
		return pH->EndFunction();
	}

	float distanceThreshold = 0.0f;
	if (pH->GetParamCount() > 1)
		pH->GetParam(2, distanceThreshold);

	return pH->EndFunction(pPipeUser->IsTakingCover(distanceThreshold));
}

//====================================================================
// IsMovingToCover
//====================================================================
int CScriptBind_AI::IsMovingToCover(IFunctionHandler* pH)
{
	GET_ENTITY(1);

	// Check entity
	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "IsMovingToCover: no entity given parameter 1");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "IsMovingToCover: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
	{
		AIWarningID("<CScriptBind_AI> ", "IsMovingToCover: Entity [%s] is registered in AI but not as a PipeUser.", pEntity->GetName());
		return pH->EndFunction();
	}

	return pH->EndFunction(pPipeUser->IsMovingToCover());
}

//====================================================================
// IsMovingInCover
//====================================================================
int CScriptBind_AI::IsMovingInCover(IFunctionHandler* pH)
{
	GET_ENTITY(1);

	// Check entity
	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "IsMovingInCover: no entity given parameter 1");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "IsMovingInCover: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	CPipeUser* pPipeUser = pAI->CastToCPipeUser();
	if (!pPipeUser)
	{
		AIWarningID("<CScriptBind_AI> ", "IsMovingInCover: Entity [%s] is registered in AI but not as a PipeUser.", pEntity->GetName());
		return pH->EndFunction();
	}

	return pH->EndFunction(pPipeUser->IsMovingInCover());
}

//====================================================================
// IsCoverCompromised
//====================================================================
int CScriptBind_AI::IsCoverCompromised(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	// Check entity
	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "IsCoverCompromised: no entity given parameter 1");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "IsCoverCompromised: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
	{
		AIWarningID("<CScriptBind_AI> ", "IsCoverCompromised: Entity [%s] is registered in AI but not as a PipeUser.", pEntity->GetName());
		return pH->EndFunction();
	}

	return pH->EndFunction(pPipeUser->IsCoverCompromised());
}

int CScriptBind_AI::SetCoverCompromised(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	// Check entity
	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "SetCoverCompromised: no entity given parameter 1");

		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "SetCoverCompromised: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	CPipeUser* pPipeUser = pAI->CastToCPipeUser();
	if (!pPipeUser)
	{
		AIWarningID("<CScriptBind_AI> ", "SetCoverCompromised: Entity [%s] is registered in AI but not as a PipeUser.", pEntity->GetName());
		return pH->EndFunction();
	}

	pPipeUser->SetCoverCompromised();

	return pH->EndFunction();
}

int CScriptBind_AI::IsInCover(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	// Check entity
	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "IsInCover: no entity given parameter 1");

		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "IsInCover: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	CPipeUser* pPipeUser = pAI->CastToCPipeUser();
	if (!pPipeUser)
	{
		AIWarningID("<CScriptBind_AI> ", "IsInCover: Entity [%s] is registered in AI but not as a PipeUser.", pEntity->GetName());
		return pH->EndFunction();
	}

	return pH->EndFunction(pPipeUser->IsInCover());
}

int CScriptBind_AI::SetInCover(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	// Check entity
	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "SetInCover: no entity given parameter 1");

		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "SetInCover: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	CPipeUser* pPipeUser = pAI->CastToCPipeUser();
	if (!pPipeUser)
	{
		AIWarningID("<CScriptBind_AI> ", "SetInCover: Entity [%s] is registered in AI but not as a PipeUser.", pEntity->GetName());
		return pH->EndFunction();
	}

	bool inCover = false;
	if (pH->GetParam(2, inCover))
		pPipeUser->SetInCover(inCover);

	return pH->EndFunction();
}

// <title PlayCommunication>
// Syntax: AI.PlayCommunication(entityId, commName, channelName[, targetId] [, targetPos])
// Description: Plays communication on the AI agent.
// Arguments:
//	entityId - AI's entity id
//	commName - The name of the communication to play
//	channelName - The name of the channel where the communication will play
int CScriptBind_AI::PlayCommunication(IFunctionHandler* pH, ScriptHandle entityId, const char* commName,
                                      const char* channelName, float contextExpiry)
{
	IEntity* entity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityId.n));
	if (!entity)
		return pH->EndFunction();

	IAIObject* aiObject = entity->GetAI();
	if (!aiObject)
		return pH->EndFunction();

	IAIActorProxy* proxy = aiObject->GetProxy();
	if (!proxy)
		return pH->EndFunction();

	CCommunicationManager& commManager = *gAIEnv.pCommunicationManager;

	SCommunicationRequest request;

	request.actorID = static_cast<EntityId>(entityId.n);
	request.channelID = commManager.GetChannelID(channelName);
	request.commID = commManager.GetCommunicationID(commName);
	request.configID = commManager.GetConfigID(proxy->GetCommunicationConfigName());
	request.contextExpiry = contextExpiry;
	request.ordering = SCommunicationRequest::Ordered;

	if (pH->GetParamType(5) == svtBool)
		pH->GetParam(5, request.skipCommSound);

	if (pH->GetParamType(6) == svtBool)
		pH->GetParam(6, request.skipCommAnimation);

	ScriptHandle target(0);
	if (pH->GetParamType(7) == svtPointer)
		pH->GetParam(7, target);

	request.targetID = static_cast<EntityId>(target.n);

	if (pH->GetParamType(8) == svtObject)
		pH->GetParam(8, request.target);

	ScriptHandle playID(commManager.PlayCommunication(request));

	return pH->EndFunction(playID);
}

// <title StopCommunication>
// Syntax: AI.StopCommunication(playID)
// Description: Stops given communication.
// Arguments:
//	playID - The id of the communication to stop.
int CScriptBind_AI::StopCommunication(IFunctionHandler* pH, ScriptHandle playID)
{
	gAIEnv.pCommunicationManager->StopCommunication(CommPlayID(static_cast<uint32>(playID.n)));
	return pH->EndFunction();
}

//====================================================================
// IsOutOfAmmo
//====================================================================
int CScriptBind_AI::IsOutOfAmmo(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	// Check entity
	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "IsOutOfAmmo: no entity given parameter 1");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "IsOutOfAmmo: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	IAIActorProxy* pAIActorProxy = pAI->GetProxy();
	if (!pAIActorProxy)
	{
		AIWarningID("<CScriptBind_AI> ", "IsOutOfAmmo: Entity '%s' does not have puppet AI proxy.", pEntity->GetName());
		return pH->EndFunction();
	}

	SAIWeaponInfo weaponInfo;
	pAIActorProxy->QueryWeaponInfo(weaponInfo);
	CCCPOINT(CScriptBind_AI_IsOutOfAmmo);
	return pH->EndFunction(weaponInfo.outOfAmmo);
}

//====================================================================
// IsOutOfAmmo
//====================================================================
int CScriptBind_AI::IsLowOnAmmo(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	// Check entity
	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "IsLowOnAmmo: no entity given parameter 1");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "IsLowOnAmmo: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	IAIActorProxy* pAIActorProxy = pAI->GetProxy();
	if (!pAIActorProxy)
	{
		AIWarningID("<CScriptBind_AI> ", "IsLowOnAmmo: Entity '%s' does not have puppet AI proxy.", pEntity->GetName());
		return pH->EndFunction();
	}

	float threshold = 0.15f;
	pH->GetParam(2, threshold);

	CCCPOINT(CScriptBind_AI_IsLowOnAmmo);

	SAIWeaponInfo weaponInfo;
	pAIActorProxy->QueryWeaponInfo(weaponInfo);

	if (weaponInfo.clipSize > 0)
	{
		float current = weaponInfo.ammoCount / (float)weaponInfo.clipSize;
		return pH->EndFunction(current <= threshold);
	}

	return pH->EndFunction(weaponInfo.lowAmmo);
}

//====================================================================
// EnableWeaponAccessory
//====================================================================
int CScriptBind_AI::EnableWeaponAccessory(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	// Check entity
	if (!pEntity)
	{
		AIWarningID("<CScriptBind_AI> ", "EnableWeaponAccessory(): no entity given parameter 1");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (!pAI || !pAIActor)
	{
		AIWarningID("<CScriptBind_AI> ", "EnableWeaponAccessory(): Entity '%s' does not have AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	int accessory = 0;
	if (!pH->GetParam(2, accessory))
		return pH->EndFunction();

	bool state = 0;
	if (!pH->GetParam(3, state))
		return pH->EndFunction();

	AgentParameters params = pAIActor->GetParameters();

	if (state)
		params.m_weaponAccessories |= accessory;
	else
		params.m_weaponAccessories &= ~accessory;

	pAIActor->SetParameters(params);

	return pH->EndFunction();

}

//====================================================================
// ResetAgentState
//====================================================================
int CScriptBind_AI::ResetAgentState(IFunctionHandler* pH, ScriptHandle entityId, const char* stateLabel)
{
	if (!stateLabel) return pH->EndFunction();

	EntityId id = (EntityId)entityId.n;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (!pEntity) return pH->EndFunction();

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI) return pH->EndFunction();

	return pH->EndFunction();
}

//====================================================================
// RegisterTacticalPointQuery
//====================================================================
int CScriptBind_AI::RegisterTacticalPointQuery(IFunctionHandler* pH)
{
	// Check and grab specification table
	SmartScriptTable specTable;
	if (!pH->GetParam(1, specTable))
	{
		AIWarningID("<CScriptBind_AI> ", "RegisterTacticalPointQuery(): no table given parameter 1");
		return pH->EndFunction();
	}

	int result = CreateQueryFromTacticalSpec(specTable);
	return pH->EndFunction(result);
}

//====================================================================
// AutoDisable
//====================================================================
int CScriptBind_AI::AutoDisable(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	// Check entity
	if (!pEntity)
	{
		AIWarningID("<CScriptBind_AI> ", "EnableWeaponAccessory(): no entity given parameter 1");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "AutoDisable(): Entity '%s' does not have AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	IAIActorProxy* pAIActorProxy = pAI->GetProxy();
	if (!pAIActorProxy)
	{
		AIWarningID("<CScriptBind_AI> ", "AutoDisable(): Entity '%s' does not have AI proxy.", pEntity->GetName());
		return pH->EndFunction();
	}

	int enable = 0;
	if (!pH->GetParam(2, enable))
		return pH->EndFunction();

	// Note that autodisable is the logical opposite of updateMeAlways
	if (enable == 0)
		pAIActorProxy->UpdateMeAlways(true);
	else
		pAIActorProxy->UpdateMeAlways(false);

	CCCPOINT(CScriptBind_AI_AutoDisable);

	return pH->EndFunction();
}

//====================================================================
// CheckMeleeDamage
//====================================================================
int CScriptBind_AI::CheckMeleeDamage(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(6);

	ScriptHandle hdl;
	ScriptHandle hdl2;
	float radius, minheight, maxheight, angle;
	if (pH->GetParams(hdl, hdl2, radius, minheight, maxheight, angle))
	{
		EntityId nID = static_cast<EntityId>(hdl.n);
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(nID);

		// Check entity
		if (!pEntity)
		{
			AIWarningID("<CScriptBind_AI> ", "CheckMeleeDamage(): no entity given parameter 1");
			return pH->EndFunction();
		}

		IAIObject* pAI = pEntity->GetAI();
		if (!pAI)
		{
			AIWarningID("<CScriptBind_AI> ", "CheckMeleeDamage(): Entity '%s' does not have AI.", pEntity->GetName());
			return pH->EndFunction();
		}
		IAIActor* pAIActor = pAI->CastToIAIActor();
		bool b3d = pAIActor && pAIActor->GetMovementAbility().b3DMove;

		EntityId nID2 = static_cast<EntityId>(hdl2.n);
		IEntity* pEntityTarget = gEnv->pEntitySystem->GetEntity(nID2);
		if (!pEntityTarget)
		{
			AIWarningID("<CScriptBind_AI> ", "CheckMeleeDamage(): no target entity given parameter 2");
			return pH->EndFunction();
		}

		angle = DEG2RAD(angle);
		Vec3 mydir(pEntity->GetWorldTM().TransformVector(Vec3(0, 1, 0)));
		Vec3 targetdir(pEntityTarget->GetWorldPos() - pEntity->GetWorldPos());

		Vec3 mydirN(mydir.GetNormalizedSafe());
		Vec3 targetdirN(targetdir.GetNormalizedSafe());

		float dot = mydirN.Dot(targetdirN);
		float myangle = acos_tpl(dot);
		if (myangle > angle / 2)
			return pH->EndFunction();

		float dist = b3d ? targetdir.GetLength() : targetdir.GetLength2D();
		if (dist > radius)
			return pH->EndFunction();

		if (targetdir.z < minheight || targetdir.z > maxheight)
			return pH->EndFunction();

		return pH->EndFunction(dist, RAD2DEG(myangle));
	}

	return pH->EndFunction();
}

//====================================================================
// GetTacticalPoints
//====================================================================
int CScriptBind_AI::GetTacticalPoints(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS_MIN(3);
	GET_ENTITY(1);

	// Check entity
	if (pEntity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "GetTacticalPoints: no entity given parameter 1");
		return pH->EndFunction();
	}
	// Check it's got an AI object
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "IGetTacticalPoints: Entity [%s] is not an AI.", pEntity->GetName());
		return pH->EndFunction();
	}
	// Is it me or are these tests getting kinda boring?
	CAIActor* pAIActor = pAI->CastToCAIActor();
	if (!pAIActor)
	{
		AIWarningID("<CScriptBind_AI> ", "GetTacticalPoints: Entity [%s] is registered in AI but not as an AI Actor.", pEntity->GetName());
		return pH->EndFunction();
	}

	SmartScriptTable specTable;
	char* sQueryName = "null";
	int queryID = 0;
	bool bRemoveQuery = true;

	// if given by name, try to find it
	if (pH->GetParamType(2) == svtString)
	{
		if (pH->GetParam(2, sQueryName))
			queryID = gAIEnv.pTacticalPointSystem->GetQueryID(sQueryName);

		if (!queryID)
		{
			AIErrorID("<CScriptBind_AI> ", "GetTacticalPoints(): tacticalpos query does not exist: %s", sQueryName);
			return pH->EndFunction();
		}

		bRemoveQuery = false;
	}
	else
	{
		// Check and grab specification table
		if (!pH->GetParam(2, specTable))
		{
			AIWarningID("<CScriptBind_AI> ", "GetTacticalPoints(): no table given parameter 2");
			return pH->EndFunction();
		}

		queryID = CreateQueryFromTacticalSpec(specTable);
	}

	// Check and grab results table
	SmartScriptTable resultPoint;
	if (!pH->GetParam(3, resultPoint))
	{
		AIWarningID("<CScriptBind_AI> ", "GetTacticalPoints(): no table given parameter 3");
		return pH->EndFunction();
	}

	// Translate the spec table to create a query
	bool bFoundValid = false;
	if (queryID)
	{
		// Do query
		CTacticalPoint point;
		CTacticalPointSystem* pTPS = gAIEnv.pTacticalPointSystem;

		QueryContext context;
		InitQueryContextFromActor(pAIActor, context);

		int nOptionUsed = pTPS->SyncQuery(queryID, context, point);
		bFoundValid = (nOptionUsed >= 0);

		// and if any point found
		if (bFoundValid)
		{
			// put point coordinates in the table supplied
			const Vec3& vPos(point.GetPos());
			resultPoint.GetPtr()->SetValue("x", vPos.x);
			resultPoint.GetPtr()->SetValue("y", vPos.y);
			resultPoint.GetPtr()->SetValue("z", vPos.z);

			// and an optional 'optionLable' if it was supplied
			resultPoint.GetPtr()->SetValue("optionLabel", pTPS->GetOptionLabel(queryID, nOptionUsed));

			// optional argument to mark as our hide spot
			bool bMarkAsHidespot = false;
			if (pH->GetParamCount() > 3 && pH->GetParam(4, bMarkAsHidespot) && bMarkAsHidespot)
			{
				const SHideSpot* pHS = point.GetHidespot();
				const SHideSpotInfo* pHSInfo = point.GetHidespotInfo();
				if (pHS && pHSInfo)
				{
					CPipeUser* pPipeUser = CastToCPipeUserSafe(pAI);
					if (pPipeUser)
						pPipeUser->m_CurrentHideObject.Set(pHS, pHSInfo->pos, pHSInfo->dir);
				}
			}
		}

		// Remove query
		if (bRemoveQuery == true)
		{
			pTPS->DestroyQueryID(queryID);
		}
	}

	return pH->EndFunction(bFoundValid);
}

// Description:
//
// Arguments:
//
// Return:
//
int CScriptBind_AI::DestroyAllTPSQueries(IFunctionHandler* pH)
{
	gAIEnv.pTacticalPointSystem->DestroyAllQueries();

	return(pH->EndFunction());
}

int CScriptBind_AI::CreateQueryFromTacticalSpec(SmartScriptTable specTable)
{
	ITacticalPointSystem* pTPS = gAIEnv.pTacticalPointSystem;
	int option = 0;
	bool bOk = true, bAllTables = true, bAtLeastOneGenerationProvided = false, bAtLeastOneWeightProvided = false;

	// Myriad checking to add here :/

	char* queryName;
	if (!specTable->GetValue("Name", queryName))
	{
		AIErrorID("<CScriptBind_AI> ", "CreateQueryFromTacticalSpec() Query specification missing name");
		return 0;
	}

	int queryID = pTPS->CreateQueryID(queryName);

	IScriptTable::Iterator itO;
	for (itO = specTable->BeginIteration(); specTable->MoveNext(itO); option++)
	{
		if (itO.value.GetType() == EScriptAnyType::String) continue;  // Skip the name field

		SmartScriptTable optionTable;
		if (!itO.value.CopyTo(optionTable)) return false; // Scream and shout!
		SmartScriptTable subTable;
		IScriptTable::Iterator itS;

		// parameter table is optional
		if (optionTable->GetValue("Parameters", subTable))
		{
			for (itS = subTable->BeginIteration(); subTable->MoveNext(itS); )
			{
				const char* querySpec = itS.sKey;
				if (itS.value.GetType() == EScriptAnyType::Number)
				{
					float fVal = itS.value.GetNumber();
					bOk &= pTPS->AddToParameters(queryID, querySpec, fVal, option);
				}
				else if (itS.value.GetType() == EScriptAnyType::String)
				{
					const char* sVal = itS.value.GetString();
					bOk &= pTPS->AddToParameters(queryID, querySpec, sVal, option);
				}
				else
				{
					bOk = false;
					AIErrorID("<ScriptBind_AI> ", "CreateQueryFromTacticalSpec(): \"%s\": Parameters criteria only take numbers (at this point)", queryName);
				}
			}

			specTable->EndIteration(itS);
		}

		if (!optionTable->GetValue("Generation", subTable))
			bAllTables = false;
		else
			for (itS = subTable->BeginIteration(); subTable->MoveNext(itS); )
			{
				bAtLeastOneGenerationProvided = true;
				const char* querySpec = itS.sKey;
				if (itS.value.GetType() == EScriptAnyType::Number)
				{
					float fVal = itS.value.GetNumber();
					bOk &= pTPS->AddToGeneration(queryID, querySpec, fVal, option);
				}
				else if (itS.value.GetType() == EScriptAnyType::String)
				{
					const char* sVal = itS.value.GetString();
					bOk &= pTPS->AddToGeneration(queryID, querySpec, sVal, option);
				}
				else
				{
					bOk = false;
					AIErrorID("<ScriptBind_AI> ", "CreateQueryFromTacticalSpec(): \"%s\": Generation criteria only take numbers", queryName);
				}
			}
		specTable->EndIteration(itS);

		if (!optionTable->GetValue("Conditions", subTable)) bAllTables = false;
		else
			for (itS = subTable->BeginIteration(); subTable->MoveNext(itS); )
			{
				const char* querySpec = itS.sKey;
				if (itS.value.GetType() == EScriptAnyType::Number)
				{
					float fVal = itS.value.GetNumber();
					bOk &= pTPS->AddToConditions(queryID, querySpec, fVal, option);
				}
				else if (itS.value.GetType() == EScriptAnyType::Boolean)
				{
					bool bVal = itS.value.GetBool();
					bOk &= pTPS->AddToConditions(queryID, querySpec, bVal, option);
				}
				else
				{
					bOk = false;
					AIErrorID("<ScriptBind_AI> ", "CreateQueryFromTacticalSpec(): \"%s\": Condition criteria only take numbers and bools", queryName);
				}

			}
		specTable->EndIteration(itS);

		if (!optionTable->GetValue("Weights", subTable)) bAllTables = false;
		else
			for (itS = subTable->BeginIteration(); subTable->MoveNext(itS); )
			{
				bAtLeastOneWeightProvided = true;
				const char* querySpec = itS.sKey;
				if (itS.value.GetType() == EScriptAnyType::Number)
				{
					float fVal = itS.value.GetNumber();
					bOk &= pTPS->AddToWeights(queryID, querySpec, fVal, option);
				}
				else
				{
					bOk = false;
					AIErrorID("<ScriptBind_AI> ", "CreateQueryFromTacticalSpec(): \"%s\": Weight criteria only take numbers", queryName);
				}
			}
		specTable->EndIteration(itS);
	}
	specTable->EndIteration(itO);

	bOk &= bAllTables;
	bOk &= bAtLeastOneGenerationProvided;
	bOk &= bAtLeastOneWeightProvided;

	if (bAllTables)
	{
		if (!bAtLeastOneGenerationProvided)
			AIErrorID("<CScriptBind_AI> ", "CreateQueryFromTacticalSpec(): \"%s\": Queries specifications must provide at least one entry in the Generation table", queryName);

		if (!bAtLeastOneWeightProvided)
			AIErrorID("<CScriptBind_AI> ", "CreateQueryFromTacticalSpec(): \"%s\": Queries specifications must provide at least one entry in the Weights table", queryName);
	}
	else
	{
		AIErrorID("<CScriptBind_AI> ", "CreateQueryFromTacticalSpec(): \"%s\": Queries specifications must have tables for Generation, Conditions and Weights", queryName);
	}

	// If fails, destroy query ID and return 0
	if (!bOk)
	{
		pTPS->DestroyQueryID(queryID);
		queryID = 0;
	}

	return queryID;
}

//====================================================================
// GetObjectBlackBoard
//====================================================================
int CScriptBind_AI::GetObjectBlackBoard(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	SmartScriptTable* pBlackBoard = NULL;
	IAIObject* pAI = NULL;

	if (pH->GetParamType(1) == svtPointer)
	{
		//retrieve the entity
		IEntity* pEntity = GetEntityFromParam(pH, 1);
		if (pEntity)
			pAI = pEntity->GetAI();
	}
	else if (pH->GetParamType(1) == svtString)
	{
		const char* objName = "";
		pH->GetParam(1, objName);
		pAI = gAIEnv.pAIObjectManager->GetAIObjectByName(0, objName);
	}

	if (pAI != NULL)
	{
		if (IBlackBoard* pAIBlackboard = pAI->GetBlackBoard())
		{
			pBlackBoard = &(pAIBlackboard->GetForScript());
		}
	}

	assert(pBlackBoard);
	PREFAST_ASSUME(pBlackBoard);

	return pH->EndFunction(*pBlackBoard);
}

//====================================================================
// GetBehaviorBlackBoard
//====================================================================
int CScriptBind_AI::GetBehaviorBlackBoard(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	SmartScriptTable* pBlackBoard = NULL;
	IAIObject* pAI = NULL;

	if (pH->GetParamType(1) == svtPointer)
	{
		//retrieve the entity
		IEntity* pEntity = GetEntityFromParam(pH, 1);
		if (pEntity)
			pAI = pEntity->GetAI();
	}
	else if (pH->GetParamType(1) == svtString)
	{
		const char* objName = "";
		pH->GetParam(1, objName);
		pAI = gAIEnv.pAIObjectManager->GetAIObjectByName(0, objName);
	}

	if (pAI != NULL)
	{
		IAIActor* pAIActor = CastToIAIActorSafe(pAI);
		if (pAIActor)
		{
			pBlackBoard = &(pAIActor->GetBehaviorBlackBoard()->GetForScript());
		}
	}

	return pBlackBoard != NULL ? pH->EndFunction(*pBlackBoard) : pH->EndFunction();
}

int CScriptBind_AI::SetBehaviorVariable(IFunctionHandler* pH, ScriptHandle entityId, const char* variableName, bool value)
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n))
	{
		if (CAIActor* actor = CastToCAIActorSafe(entity->GetAI()))
			actor->SetBehaviorVariable(variableName, value);
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetBehaviorVariable(IFunctionHandler* pH, ScriptHandle entityId, const char* variableName)
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n))
	{
		if (CAIActor* actor = CastToCAIActorSafe(entity->GetAI()))
			return pH->EndFunction(actor->GetBehaviorVariable(variableName));
	}

	return pH->EndFunction();
}

//====================================================================
// GoTo
//====================================================================
int CScriptBind_AI::GoTo(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	IAIObject* pAIObject = pEntity ? pEntity->GetAI() : 0;
	if (!pAIObject)
	{
		AIWarningID("<CScriptBind_AI> ", "GoTo(): Entity is not an AI Object.");
		return pH->EndFunction();
	}

	IAIActor* pAIActor = pAIObject->CastToIAIActor();
	if (!pAIActor)
	{
		AIWarningID("<CScriptBind_AI> ", "GoTo(): Entity is not an AI Actor.");
		return pH->EndFunction();
	}

	Vec3 vDestination(ZERO);
	if (!pH->GetParam(2, vDestination))
	{
		AIWarningID("<CScriptBind_AI> ", "GoTo(): Invalid destination specified.");
		return pH->EndFunction();
	}

	pAIActor->GoTo(vDestination);

	return pH->EndFunction();
}

//====================================================================
// SetSpeed
//====================================================================
int CScriptBind_AI::SetSpeed(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	IAIObject* pAIObject = pEntity ? pEntity->GetAI() : 0;
	if (!pAIObject)
	{
		AIWarningID("<CScriptBind_AI> ", "SetSpeed(): Entity is not an AI Object.");
		return pH->EndFunction();
	}

	IAIActor* pAIActor = pAIObject->CastToIAIActor();
	if (!pAIActor)
	{
		AIWarningID("<CScriptBind_AI> ", "SetSpeed(): Entity is not an AI Actor.");
		return pH->EndFunction();
	}

	float fSpeed = 0.f;
	if (!pH->GetParam(2, fSpeed))
	{
		AIWarningID("<CScriptBind_AI> ", "SetSpeed(): Invalid speed specified.");
		return pH->EndFunction();
	}

	pAIActor->SetSpeed(fSpeed);

	return pH->EndFunction();
}

//====================================================================
// SetEntitySpeedRange
//====================================================================
int CScriptBind_AI::SetEntitySpeedRange(IFunctionHandler* pH)
{
	bool ret = false;

	//first, get the selected entity
	IEntity* pEntity = GetEntityFromParam(pH, 1);

	if (pEntity)
	{
		IAIObject* aiObject = pEntity->GetAI();

		if (aiObject)
		{
			uint32 stancestart = 0;
			uint32 stanceend = AgentMovementSpeeds::AMS_NUM_VALUES;

			int32 count = pH->GetParamCount();

			//now get the desired urgency
			uint32 urgency = AgentMovementSpeeds::AMU_NUM_VALUES;
			pH->GetParam(2, urgency);

			//now get the values for default, min and max speed
			float sdef = 0.0f, smin = 0.0f, smax = 0.0f;
			pH->GetParam(3, sdef);
			pH->GetParam(4, smin);
			pH->GetParam(5, smax);

			//if there is at least one more param, use it as filter for
			//the stance
			if (count > 5)
			{
				pH->GetParam(6, stancestart);
				stanceend = stancestart + 1;
			}

			if (AgentMovementSpeeds::AMS_NUM_VALUES > stancestart)
			{
				if (CAIActor* pAIActor = aiObject->CastToCAIActor())
				{
					//now loop over the selected stances (default is all) and set the ranges
					for (uint32 stance = stancestart; stanceend > stance; ++stance)
					{
						pAIActor->m_movementAbility.movementSpeeds.SetRanges(stance, urgency, sdef, smin, smax);
					}

					ret = true;
				}
			}
			else
			{
				AIErrorID("<CScriptBind_AI> ", "SetEntitySpeedRange: stance value (%d) out of range.", stancestart);
			}
		}
		else
		{
			AIWarningID("<CScriptBind_AI> ", "SetEntitySpeedRange: Entity (%d) does not have an AIObject", pEntity->GetId());
		}
	}
	else
	{
		AIWarningID("<CScriptBind_AI> ", "SetEntitySpeedRange: Entity not found");
	}

	return pH->EndFunction(ret);
}

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
//====================================================================
// RecursiveScriptLoader
//====================================================================
class CRecursiveScriptLoader
{
public:
	CRecursiveScriptLoader(const char* ext, const char* tag, const char* folderName, SmartScriptTable tbl)
	{
		string folder = folderName;
		string search = folder;
		search += string("/*.*");

		if (folder[folder.length() - 1] != '/')
			folder += "/";

		ICryPak* pPak = gEnv->pCryPak;

		_finddata_t fd;
		intptr_t handle = pPak->FindFirst(search.c_str(), &fd);

		if (handle > -1)
		{
			const char* theTag = tag;
			size_t theTagLen = strlen(theTag);
			string commentTag("--");
			commentTag += theTag;

			const char* internalTag = "Internal";
			size_t internalTagLen = strlen(internalTag);

			do
			{
				if (!strcmp(fd.name, ".") || !strcmp(fd.name, ".."))
					continue;

				if (fd.attrib & _A_SUBDIR)
				{
					string subName = folder + fd.name;

					CRecursiveScriptLoader recurser(ext, tag, subName.c_str(), tbl);

					continue;
				}

				if (stricmp(PathUtil::GetExt(fd.name), ext))
					continue;

				bool isInternal = false;
				string luaFile = folder + string(fd.name);
				string name;

				{
					CCryFile file;

					if (!file.Open(luaFile.c_str(), "r"))
						continue;

					static unsigned char buf[256] = { 0 };
					size_t n = file.ReadRaw(buf, 255);
					if (n < 2 + theTagLen)
						continue;

					assert(n < 256);
					buf[n] = 0;
					const unsigned char* p = buf;

					while (*p && isspace(*p)) ++p;
					const unsigned char* nl = p;
					while (*nl && *nl != '\n' && *nl != '\r') ++nl;
					if ((size_t)(nl - p) < internalTagLen)
						continue;

					string line(reinterpret_cast<const char*>(p), reinterpret_cast<const char*>(nl));

					int tok = 0;
					string token = line.Tokenize(" ", tok);

					if (token != "--" && token != commentTag)
						continue;

					if (token == "--")
					{
						token = line.Tokenize(" ", tok);
						if (token != theTag)
							continue;
					}

					name = line.Tokenize(" ", tok);
					if (name.empty())
						continue;

					string inttag = line.Tokenize(" ", tok);
					if (!inttag.empty() && !stricmp(inttag.c_str(), internalTag))
						isInternal = true;

					assert(!name.empty());
				}

				SmartScriptTable place;

				if (isInternal)
					tbl->GetValue("INTERNAL", place);
				else
					tbl->GetValue("AVAILABLE", place);

				place->SetValue(name.c_str(), luaFile.c_str());

				gEnv->pScriptSystem->ReloadScript(luaFile.c_str());

			}
			while (pPak->FindNext(handle, &fd) >= 0);
		}
	}

	~CRecursiveScriptLoader() {};
};
#endif

int CScriptBind_AI::SetAlarmed(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	IAIObject* pAIObject = pEntity->GetAI();
	if (pAIObject)
	{
		if (CPuppet* pPuppet = pAIObject->CastToCPuppet())
			pPuppet->SetAlarmed();
	}

	return pH->EndFunction();
}

//====================================================================
// DirectoryExplorer
//====================================================================
struct DirectoryExplorer
{
	template<typename Action>
	uint32 operator()(const char* rootFolderName, const char* extension, Action& action)
	{
		stack_string folder = rootFolderName;
		stack_string search = folder;
		search.append("/*.*");

		uint32 fileCount = 0;

		if (folder[folder.length() - 1] != '/')
			folder += "/";

		ICryPak* pak = gEnv->pCryPak;

		_finddata_t fd;
		intptr_t handle = pak->FindFirst(search.c_str(), &fd);

		if (handle > -1)
		{
			do
			{
				if (!strcmp(fd.name, ".") || !strcmp(fd.name, ".."))
					continue;

				if (fd.attrib & _A_SUBDIR)
				{
					stack_string subName = folder + fd.name;

					fileCount += DirectoryExplorer()(subName.c_str(), extension, action);

					continue;
				}

				if (stricmp(PathUtil::GetExt(fd.name), PathUtil::GetExt(extension)))
					continue;

				stack_string luaFile = folder + stack_string(fd.name);
				action(luaFile);

				++fileCount;

			}
			while (pak->FindNext(handle, &fd) >= 0);
		}

		return fileCount;
	}
};

struct BehaviorExplorer
{
	BehaviorExplorer(const char* _createFuncName)
		: createFuncName(_createFuncName)
	{
		globalBehaviors.Create(gEnv->pScriptSystem, false);

		string fakeCreateCode =
		  "local name, baseName = select(1, ...)\n"
		  "if (type(baseName) ~= \"string\") then baseName = true end\n"
		  "__DiscoveredBehavior[name] = baseName\n"
		  "return {}\n";

		fakeCreateBehavior = SmartScriptFunction(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(fakeCreateCode,
		                                                                                                 strlen(fakeCreateCode), "Fake CreateBehaviorCode"));

		assert(fakeCreateBehavior != 0);
	}

	struct BehaviorInfo
	{
		BehaviorInfo(const char* base, const char* file)
			: baseName(base)
			, fileName(file)
			, loaded(false)
		{
		}

		string baseName;
		string fileName;

		bool   loaded;
	};

	typedef std::map<string, BehaviorInfo> Behaviors;
	Behaviors behaviors;

	void operator()(const char* fileName)
	{
		HSCRIPTFUNCTION createBehavior = gEnv->pScriptSystem->GetFunctionPtr(createFuncName.c_str());
		if (!createBehavior)
		{
			// error
			return;
		}

		gEnv->pScriptSystem->SetGlobalValue(createFuncName.c_str(), fakeCreateBehavior);

		const char* discoveredBehaviorName = "__DiscoveredBehavior";
		gEnv->pScriptSystem->SetGlobalValue(discoveredBehaviorName, globalBehaviors);

		globalBehaviors->Clear();

		if (gEnv->pScriptSystem->ExecuteFile(fileName, true, gEnv->IsEditor())) // Force reload if in editor mode
		{
			IScriptTable::Iterator it = globalBehaviors->BeginIteration();

			while (globalBehaviors->MoveNext(it))
			{
				if (it.key.GetType() != EScriptAnyType::String)
					continue;

				const char* behaviorName = 0;
				it.key.CopyTo(behaviorName);

				const char* baseName = 0;
				if (it.value.GetType() == EScriptAnyType::String)
					it.value.CopyTo(baseName);

				std::pair<Behaviors::iterator, bool> iresult = behaviors.insert(
				  Behaviors::value_type(behaviorName, BehaviorInfo(baseName, fileName)));

				if (!iresult.second)
				{
					AIWarning("Duplicate behavior definition '%s' in file '%s'. First seen in file '%s'.",
					          behaviorName, fileName, iresult.first->second.fileName.c_str());

					continue;
				}
			}

			globalBehaviors->EndIteration(it);
		}

		gEnv->pScriptSystem->SetGlobalToNull(discoveredBehaviorName);
		gEnv->pScriptSystem->SetGlobalValue(createFuncName.c_str(), createBehavior);
		gEnv->pScriptSystem->ReleaseFunc(createBehavior);
	}

private:
	string              createFuncName;
	SmartScriptTable    globalBehaviors;
	SmartScriptFunction fakeCreateBehavior;
};

struct BehaviorLoader
{
	bool CheckCyclicDeps(BehaviorExplorer::Behaviors& behaviors, const char* behaviorName,
	                     BehaviorExplorer::BehaviorInfo& behaviorInfo)
	{
		const char* baseName = behaviorInfo.baseName.c_str();
		const char* baseFileName = behaviorInfo.fileName.c_str();

		while (baseName && *baseName)
		{
			if (!stricmp(baseName, behaviorName))
			{
				AIWarning("Cyclic dependency found loading behavior '%s' in file '%s'. Cycle starts in file '%s'.",
				          behaviorName, behaviorInfo.fileName.c_str(), baseFileName);

				return false;
			}

			BehaviorExplorer::Behaviors::iterator it = behaviors.find(CONST_TEMP_STRING(baseName));
			if (it != behaviors.end())
			{
				BehaviorExplorer::BehaviorInfo& baseInfo = it->second;
				if (!CheckCyclicDeps(behaviors, baseName, baseInfo))
					return false;

				baseName = baseInfo.baseName.c_str();
				baseFileName = baseInfo.fileName.c_str();
			}
			else
				baseName = 0;
		}

		return true;
	}

	bool operator()(BehaviorExplorer::Behaviors& behaviors, const char* behaviorName,
	                BehaviorExplorer::BehaviorInfo& behaviorInfo)
	{
		if (behaviorInfo.loaded)
			return true;

		if (!behaviorInfo.baseName.empty())
		{
			if (!CheckCyclicDeps(behaviors, behaviorName, behaviorInfo))
				return false;

			BehaviorExplorer::Behaviors::iterator it = behaviors.find(CONST_TEMP_STRING(behaviorInfo.baseName.c_str()));
			if (it == behaviors.end())
			{
				AIWarning("Failed to load behavior '%s' in file '%s' - Parent behavior '%s' not found.",
				          behaviorName, behaviorInfo.fileName.c_str(), behaviorInfo.baseName.c_str());

				return false;
			}

			BehaviorExplorer::BehaviorInfo& parentInfo = it->second;

			if (!parentInfo.loaded)
			{
				if (!BehaviorLoader()(behaviors, it->first.c_str(), it->second))
					return false;
			}
		}

		if (gEnv->pScriptSystem->ExecuteFile(behaviorInfo.fileName.c_str(), true, true))
		{
			behaviorInfo.loaded = true;

			return true;
		}

		return false;
	}
};

int CScriptBind_AI::LoadBehaviors(IFunctionHandler* pH, const char* folderName, const char* extension)
{
	CTimeValue startTime = gEnv->pTimer->GetAsyncTime();

	BehaviorExplorer behaviorExplorer("CreateAIBehavior");
	DirectoryExplorer()(folderName, extension, behaviorExplorer);

	BehaviorExplorer::Behaviors::iterator it = behaviorExplorer.behaviors.begin();
	BehaviorExplorer::Behaviors::iterator end = behaviorExplorer.behaviors.end();

	int numFailed = 0;

	for (; it != end; ++it)
	{
		if (!BehaviorLoader()(behaviorExplorer.behaviors, it->first.c_str(), it->second))
		{
			CRY_ASSERT_TRACE(false, ("Couldn't load behavior %s", it->first.c_str()));
			++numFailed;
		}
	}

	CTimeValue endTime = gEnv->pTimer->GetAsyncTime();
	AILogAlways("Loaded %" PRISIZE_T " AI Behaviors in %.2fs...", behaviorExplorer.behaviors.size(), (endTime - startTime).GetSeconds());
	if (numFailed != 0)
	{
		AIWarning("%d behaviors couldn't be loaded!", numFailed);
	}

	return pH->EndFunction();
}

#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
int CScriptBind_AI::LoadCharacters(IFunctionHandler* pH, const char* folderName, SmartScriptTable tbl)
{
	CRecursiveScriptLoader loader("lua", "AICharacter:", folderName, tbl);

	return pH->EndFunction();
}
#endif

int CScriptBind_AI::IsLowHealthPauseActive(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity((EntityId)entityID.n))
	{
		if (IAIObject* aiObject = entity->GetAI())
		{
			if (CAIActor* actor = aiObject->CastToCAIActor())
				return pH->EndFunction(actor->IsLowHealthPauseActive());
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetPreviousBehaviorName(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityID.n)))
	{
		if (IAIObject* pAI = pEntity->GetAI())
		{
			if (IAIActorProxy* pAIProxy = pAI->GetProxy())
			{
				return pH->EndFunction(pAIProxy->GetPreviousBehaviorName());
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetPeakThreatLevel(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityID.n)))
	{
		if (IAIObject* pAI = pEntity->GetAI())
		{
			if (IAIActor* pAIActor = CastToIAIActorSafe(pAI))
			{
				//If the current attention target is equal to the peak attention target, then return the previous peak threat instead
				IAIObject* attentionTarget = pAIActor->GetAttentionTarget();
				if (attentionTarget && pAIActor->GetPeakTargetID() == attentionTarget->GetAIObjectID())
				{
					return pH->EndFunction(pAIActor->GetPreviousPeakThreatLevel());
				}

				return pH->EndFunction(pAIActor->GetPeakThreatLevel());
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetPeakThreatType(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityID.n)))
	{
		if (IAIObject* pAI = pEntity->GetAI())
		{
			if (IAIActor* pAIActor = CastToIAIActorSafe(pAI))
			{
				return pH->EndFunction(pAIActor->GetPeakThreatType());
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetPreviousPeakThreatLevel(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityID.n)))
	{
		if (IAIObject* pAI = pEntity->GetAI())
		{
			if (IAIActor* pAIActor = CastToIAIActorSafe(pAI))
			{
				return pH->EndFunction(pAIActor->GetPreviousPeakThreatLevel());
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetPreviousPeakThreatType(IFunctionHandler* pH, ScriptHandle entityID)
{
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityID.n)))
	{
		if (IAIObject* pAI = pEntity->GetAI())
		{
			if (IAIActor* pAIActor = CastToIAIActorSafe(pAI))
			{
				return pH->EndFunction(pAIActor->GetPreviousPeakThreatType());
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::SetContinuousMotion(IFunctionHandler* pH, ScriptHandle entityID, bool continuousMotion)
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityID.n)))
	{
		if (IAIObject* ai = entity->GetAI())
		{
			if (IAIActor* aiActor = ai->CastToIAIActor())
			{
				SOBJECTSTATE& state = aiActor->GetState();

				state.continuousMotion = continuousMotion;

				if (!continuousMotion)
					state.fDesiredSpeed = 0.0f;
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::LoadGoalPipes(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	if (pH->GetParamType(1) == svtString)
	{
		const char* szFileName;
		if (pH->GetParam(1, szFileName))
		{
			CGoalPipeXMLReader reader;

			if (reader.LoadGoalPipesFromXmlFile(szFileName))
				return pH->EndFunction(true);
			else
				gEnv->pLog->LogError("CScriptBind_AI::LoadGoalPipes: Failed to load %s", szFileName);
		}
	}
	else
	{
		gEnv->pLog->LogError("CScriptBind_AI::LoadGoalPipes: Parameter 1 must be of type string");
	}

	return pH->EndFunction(false);
}

int CScriptBind_AI::CheckForFriendlyAgentsAroundPoint(IFunctionHandler* pH, ScriptHandle entityID, Vec3 point, float radius)
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityID.n)))
	{
		if (IAIObject* ai = entity->GetAI())
		{
			const float radiusSq = radius * radius;
			AutoAIObjectIter itFriendly(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_FACTION, ai->GetFactionID()));
			IAIObject* friendly = itFriendly->GetObject();
			while (friendly)
			{
				if (friendly->IsEnabled())
				{
					const float distance = Distance::Point_PointSq(point, friendly->GetPos());
					if (distance <= radiusSq)
						return pH->EndFunction(true);
				}
				itFriendly->Next();
				friendly = itFriendly->GetObject();
			}
		}
	}

	return pH->EndFunction(false);
}

int CScriptBind_AI::EnableUpdateLookTarget(IFunctionHandler* pH, ScriptHandle entityID, bool bEnable)
{
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityID.n)))
	{
		if (IAIObject* pAIObject = pEntity->GetAI())
		{
			if (CPipeUser* pPipeUser = pAIObject->CastToCPipeUser())
			{
				pPipeUser->EnableUpdateLookTarget(bEnable);
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::UpdateGlobalPerceptionScale(IFunctionHandler* pH, float visualScale, float audioScale)
{
	static std::map<string, EAIFilterType> filterTypeMap;
	filterTypeMap["All"] = eAIFT_All;
	filterTypeMap["Enemies"] = eAIFT_Enemies;
	filterTypeMap["Friends"] = eAIFT_Friends;
	filterTypeMap["Faction"] = eAIFT_Faction;
	filterTypeMap["None"] = eAIFT_None;

	char* filterTypeName = NULL;
	if (pH->GetParamType(3) == svtString)
	{
		pH->GetParam(3, filterTypeName);
	}

	EAIFilterType filterAI = (filterTypeName != NULL && filterTypeMap.find(filterTypeName) != filterTypeMap.end()) ? filterTypeMap[filterTypeName] : eAIFT_All;

	char* factionName = NULL;
	if (pH->GetParamType(4) == svtString)
	{
		pH->GetParam(4, factionName);
	}

	gEnv->pAISystem->UpdateGlobalPerceptionScale(visualScale, audioScale, filterAI, factionName);
	return pH->EndFunction();
}

int CScriptBind_AI::QueueBubbleMessage(IFunctionHandler* pH, ScriptHandle entityID, const char* message)
{
	EntityId id = static_cast<EntityId>(entityID.n);
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id))
	{
		AIQueueBubbleMessage("Message from LUA", id, message, eBNS_Balloon | eBNS_LogWarning);
	}

	return pH->EndFunction();
}

int CScriptBind_AI::SequenceBehaviorReady(IFunctionHandler* pH, ScriptHandle entityId)
{
	GetAISystem()->GetSequenceManager()->SequenceBehaviorReady((EntityId)entityId.n);
	return pH->EndFunction();
}

int CScriptBind_AI::SequenceInterruptibleBehaviorLeft(IFunctionHandler* pH, ScriptHandle entityId)
{
	GetAISystem()->GetSequenceManager()->SequenceInterruptibleBehaviorLeft((EntityId)entityId.n);
	return pH->EndFunction();
}

int CScriptBind_AI::SequenceNonInterruptibleBehaviorLeft(IFunctionHandler* pH, ScriptHandle entityId)
{
	GetAISystem()->GetSequenceManager()->SequenceNonInterruptibleBehaviorLeft((EntityId)entityId.n);
	return pH->EndFunction();
}

int CScriptBind_AI::SetCollisionAvoidanceRadiusIncrement(IFunctionHandler* pH, ScriptHandle entityId, float radius)
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityId.n)))
	{
		if (IAIObject* aiObject = entity->GetAI())
		{
			if (CAIActor* actor = aiObject->CastToCAIActor())
			{
				actor->m_movementAbility.collisionAvoidanceRadiusIncrement = radius;
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::RequestToStopMovement(IFunctionHandler* pH, ScriptHandle entityID)
{
	MovementRequest request;
	request.entityID = static_cast<EntityId>(entityID.n);
	request.type = MovementRequest::Stop;

	gEnv->pAISystem->GetMovementSystem()->QueueRequest(request);

	return pH->EndFunction();
}

int CScriptBind_AI::GetDistanceToClosestGroupMember(IFunctionHandler* pH, ScriptHandle entityIdHandle)
{
	float closestDistance = std::numeric_limits<float>::max();

	if (IEntity* myEntity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityIdHandle.n)))
	{
		if (IAIObject* me = myEntity->GetAI())
		{
			const Vec3 myPosition = me->GetPos();

			if (CAISystem* aiSystem = GetAISystem())
			{
				const int groupId = me->GetGroupId();
				const int groupCount = aiSystem->GetGroupCount(groupId, IAISystem::GROUP_ENABLED);

				for (int i = 0; i < groupCount; ++i)
				{
					IAIObject* member = aiSystem->GetGroupMember(groupId, i, IAISystem::GROUP_ENABLED);
					if (member && (member != me))
					{
						const float distance = member->GetPos().GetDistance(myPosition);
						if (distance < closestDistance)
						{
							closestDistance = distance;
						}
					}
				}
			}
		}
	}

	return pH->EndFunction(closestDistance);
}

int CScriptBind_AI::IsAimReady(IFunctionHandler* pH, ScriptHandle entityIdHandle)
{
	if (IEntity* myEntity = gEnv->pEntitySystem->GetEntity(static_cast<EntityId>(entityIdHandle.n)))
	{
		if (IAIObject* me = myEntity->GetAI())
		{
			if (CPipeUser* pipeUser = me->CastToCPipeUser())
			{
				return pH->EndFunction((pipeUser->GetAimState() == AI_AIM_READY) && (pipeUser->GetState().aimObstructed == false));
			}
		}
	}

	return pH->EndFunction(false);
}

int CScriptBind_AI::AllowLowerBodyToTurn(IFunctionHandler* pH, ScriptHandle entityID, bool bAllowLowerBodyToTurn)
{
	EntityId nID = static_cast<EntityId>(entityID.n);
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(nID);
	if (!pEntity)
	{
		AIWarningID("<CScriptBind_AI> ", "LockBodyTurn: Entity with id [%d] doesn't exist.", nID);
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		AIWarningID("<CScriptBind_AI> ", "LockBodyTurn: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	CPipeUser* pPipeUser = CastToCPipeUserSafe(pAI);
	if (!pPipeUser)
	{
		AIWarningID("<CScriptBind_AI> ", "LockBodyTurn: Entity [%s] is registered in AI but not as a PipeUser.", pEntity->GetName());
		return pH->EndFunction();
	}

	pPipeUser->AllowLowerBodyToTurn(bAllowLowerBodyToTurn);

	return pH->EndFunction();
}

int CScriptBind_AI::GetGroupScopeUserCount(IFunctionHandler* pH, ScriptHandle entityIdHandle, const char* groupScopeName)
{
	EntityId entityID = static_cast<EntityId>(entityIdHandle.n);
	IEntity* entity = gEnv->pEntitySystem->GetEntity(entityID);
	IF_UNLIKELY (entity == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "GetGroupScopeUserCount(): Invalid entity ID!");
		return pH->EndFunction();
	}

	CAIActor* actor = CastToCAIActorSafe(entity->GetAI());
	IF_UNLIKELY (actor == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "GetGroupScopeUserCount(): Entity '%s' has no actor representation!", entity->GetName());
		return pH->EndFunction();
	}

	CAIGroup* aiGroup = GetAISystem()->GetAIGroup(actor->GetGroupId());
	IF_UNLIKELY (aiGroup == NULL)
	{
		AIWarningID("<CScriptBind_AI> ", "GetGroupScopeUserCount(): Entity '%s' is not part of a group!", entity->GetName());
		return pH->EndFunction();
	}

	GroupScopeID groupScopeID = CAIGroup::GetGroupScopeId(groupScopeName);

	// Note: it is okay if a group scope does not exist, it just means that no-one has entered it.
	//assert(aiGroup->IsGroupScopeIDValid(groupScopeID);

	return pH->EndFunction(aiGroup->GetAmountOfActorsInScope(groupScopeID));
}

int CScriptBind_AI::StartModularBehaviorTree(IFunctionHandler* pH, ScriptHandle entityIdHandle, const char* treeName)
{
	gAIEnv.pBehaviorTreeManager->StartModularBehaviorTree(static_cast<EntityId>(entityIdHandle.n), treeName);
	return pH->EndFunction();
}

int CScriptBind_AI::StopModularBehaviorTree(IFunctionHandler* pH, ScriptHandle entityIdHandle)
{
	gAIEnv.pBehaviorTreeManager->StopModularBehaviorTree(static_cast<EntityId>(entityIdHandle.n));
	return pH->EndFunction();
}

int CScriptBind_AI::SetLastOpResult(IFunctionHandler* pH, ScriptHandle entityIdHandle, ScriptHandle targetEntityIdHandle)
{
	EntityId entityId = static_cast<EntityId>(entityIdHandle.n);
	IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
	IF_UNLIKELY (!entity)
	{
		return pH->EndFunction();
	}

	IAIObject* aiObject = entity->GetAI();
	IF_UNLIKELY (!aiObject)
	{
		return pH->EndFunction();
	}

	CPipeUser* pipeUser = CastToCPipeUserSafe(aiObject);
	IF_UNLIKELY (!pipeUser)
	{
		return pH->EndFunction();
	}

	EntityId targetEntityId = static_cast<EntityId>(targetEntityIdHandle.n);
	IEntity* targetEntity = gEnv->pEntitySystem->GetEntity(targetEntityId);
	IF_UNLIKELY (!targetEntity)
	{
		return pH->EndFunction();
	}

	CAIObject* targetAIObject = gAIEnv.pObjectContainer->GetAIObject(targetEntity->GetAIObjectID());
	IF_UNLIKELY (!targetAIObject)
	{
		return pH->EndFunction();
	}

	pipeUser->SetLastOpResult(GetWeakRef(targetAIObject));

	return pH->EndFunction();
}
