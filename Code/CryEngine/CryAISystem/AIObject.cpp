// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIObject.h"
#include "CAISystem.h"
#include "AILog.h"
#include "Leader.h"
#include "GoalOp.h"
#include "AIEntityComponent.h"

#include <float.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ILog.h>
#include <CryNetwork/ISerialize.h>
#include <CryAISystem/VisionMapTypes.h>
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "Formation/FormationManager.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define _ser_value_(val) ser.Value( # val, val)

CAIObject::CAIObject() :
	m_vPosition(ZERO),
	m_entityID(0),
	m_bEnabled(true),
	m_fRadius(.0f),
	m_pFormation(0),
	m_nObjectType(0),
	m_objectSubType(CAIObject::STP_NONE),
	m_bUpdatedOnce(false),
	m_vFirePosition(ZERO),
	m_vFireDir(ZERO),
	m_expectedPhysicsPosFrameId(-1),
	m_vExpectedPhysicsPos(ZERO),
	m_vBodyDir(ZERO),
	m_vEntityDir(ZERO),
	m_vMoveDir(ZERO),
	m_vView(ZERO),
	m_vLastPosition(ZERO),
	m_groupId(-1),
	m_factionID(IFactionMap::InvalidFactionID),
	m_isThreateningForHostileFactions(true),
	m_bTouched(false),
	m_observable(false),
	m_serialize(true)
{
	AILogComment("CAIObject (%p)", this);
}

CAIObject::~CAIObject()
{
	AILogComment("~CAIObject  %s (%p)", GetName(), this);

	// Remove the AI entity component associated with the entity
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityID))
	{
		DynArray<CAIEntityComponent*> components;
		pEntity->GetAllComponents<CAIEntityComponent>(components);

		for(CAIEntityComponent* pExistingAIComponent : components)
		{
			if (pExistingAIComponent->GetAIObjectID() == GetAIObjectID())
			{
				pEntity->RemoveComponent(pExistingAIComponent);
				break;
			}
		}
	}

	SetObservable(false);

	ReleaseFormation();
}

void CAIObject::SetPos(const Vec3& pos, const Vec3& dirForw)
{
	CCCPOINT(CAIObject_SetPos);

#ifdef CRYAISYSTEM_DEBUG
	if (_isnan(pos.x) || _isnan(pos.y) || _isnan(pos.z))
	{
		AIWarning("NotANumber tried to be set for position of AI entity %s", GetName());
		return;
	}
#endif

	assert(pos.IsValid());
	assert(dirForw.IsValid());

	if (!IsEquivalent(m_vLastPosition, pos, VEC_EPSILON))
	{
		m_vLastPosition = m_vPosition;

		if (IEntity* pEntity = GetEntity())
			GetAISystem()->NotifyAIObjectMoved(pEntity, SEntityEvent(ENTITY_EVENT_XFORM));
	}

	if (!GetProxy())
	{
		Vec3 forward = dirForw.GetNormalizedSafe(Vec3Constants<float>::fVec3_Zero);
		m_vMoveDir = forward;
		m_vBodyDir = forward;
		m_vEntityDir = forward;
		m_vView = forward;
	}

	m_vPosition = pos;

	if (m_observable)
	{
		ObservableParams observableParams;
		GetObservablePositions(observableParams);
		gAIEnv.pVisionMap->ObservableChanged(m_visionID, observableParams, eChangedPosition);
	}
}

Vec3 CAIObject::GetPhysicsPos() const
{
	IEntity* pEntity = GetEntity();

	if (pEntity)
	{
		if ((m_expectedPhysicsPosFrameId == gEnv->nMainFrameID))
		{
			return m_vExpectedPhysicsPos;
		}
		else
		{
			return pEntity->GetWorldPos();
		}
	}
	else
	{
		return GetPos();
	}
}

void CAIObject::SetExpectedPhysicsPos(const Vec3& pos)
{
	CCCPOINT(CAIObject_SetExpectedPhysicsPos);

#ifdef CRYAISYSTEM_DEBUG
	if (_isnan(pos.x) || _isnan(pos.y) || _isnan(pos.z))
	{
		AIWarning("NotANumber tried to be set for expected position of AI entity %s", GetName());
		return;
	}
#endif

	assert(pos.IsValid());

	m_expectedPhysicsPosFrameId = gEnv->nMainFrameID;
	m_vExpectedPhysicsPos = pos;
}

void CAIObject::SetType(unsigned short type)
{
	m_nObjectType = type;
}

void CAIObject::SetSubType(CAIObject::ESubType subType)
{
	m_objectSubType = subType;
}

void CAIObject::SetAssociation(CWeakRef<CAIObject> refAssociation)
{
	m_refAssociation = refAssociation;
}

const char* CAIObject::GetName() const
{
	return m_name.c_str();
}

void CAIObject::SetName(const char* pName)
{
	m_name = pName;

#ifdef CRYAISYSTEM_DEBUG
	if (m_pMyRecord)
	{
		m_pMyRecord->SetName(pName);
	}
#endif //CRYAISYSTEM_DEBUG
}

void CAIObject::SetBodyDir(const Vec3& dir)
{
	m_vBodyDir = dir;
}

void CAIObject::SetEntityDir(const Vec3& dir)
{
	m_vEntityDir = dir;
}

void CAIObject::SetMoveDir(const Vec3& dir)
{
	m_vMoveDir = dir;
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIObject::SetViewDir(const Vec3& dir)
{
	m_vView = dir;
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIObject::Reset(EObjectResetType type)
{
	ReleaseFormation();

	m_bEnabled = true;
	m_bUpdatedOnce = false;
	m_bTouched = false;

	switch (type)
	{
	case AIOBJRESET_INIT:
		// grenades and rockets are always observable
		if ((m_nObjectType == AIOBJECT_RPG) || (m_nObjectType == AIOBJECT_GRENADE))
			SetObservable(true);
		break;

	case AIOBJRESET_SHUTDOWN:
		SetObservable(false);
		break;
	}
}

//
//
//------------------------------------------------------------------------------------------------------------------------
void CAIObject::SetRadius(float fRadius)
{
	m_fRadius = fRadius;
}

//
//------------------------------------------------------------------------------------------------------------------------
bool CAIObject::ShouldSerialize() const
{
	if (!m_serialize)
		return false;

	if (gAIEnv.CVars.ForceSerializeAllObjects == 0)
	{
		IEntity* pEntity = GetEntity();
		return pEntity ? ((pEntity->GetFlags() & ENTITY_FLAG_NO_SAVE) == 0) : true;
	}

	return true;
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIObject::Serialize(TSerialize ser)
{
	ser.Value("m_refThis", m_refThis);

	ser.Value("m_bEnabled", m_bEnabled);
	ser.Value("m_bTouched", m_bTouched);
	ser.Value("m_vLastPosition", m_vLastPosition);

	int formationIndex = CFormation::INVALID_FORMATION_ID;
	if (m_pFormation && ser.IsWriting())
	{
		formationIndex = m_pFormation->GetId();
	}
	ser.Value("formationIndex", formationIndex);
	if (formationIndex != CFormation::INVALID_FORMATION_ID && ser.IsReading())
	{
		m_pFormation = gAIEnv.pFormationManager->GetFormation(formationIndex);
	}

	// m_movementAbility.Serialize(ser); // not needed as ability is constant
	ser.Value("m_bUpdatedOnce", m_bUpdatedOnce);
	// todo m_listWaitGoalOps
	ser.Value("m_nObjectType", m_nObjectType);
	ser.EnumValue("m_objectSubType", m_objectSubType, STP_NONE, STP_MAXVALUE);
	ser.Value("m_vPosition", m_vPosition);
	ser.Value("m_vFirePosition", m_vFirePosition);
	ser.Value("m_vFireDir", m_vFireDir);
	ser.Value("m_vBodyDir", m_vBodyDir);
	ser.Value("m_vEntityDir", m_vEntityDir);
	ser.Value("m_vMoveDir", m_vMoveDir);
	ser.Value("m_vView", m_vView);
	// todo m_pAssociation
	ser.Value("m_fRadius", m_fRadius);
	ser.Value("m_groupId", m_groupId);
	m_refAssociation.Serialize(ser, "m_refAssociation");
	// m_listWaitGoalOps is not serialized but recreated after serializing goal pipe, when reading, in CPipeUser::Serialize()
	ser.Value("m_name", m_name);

	ser.Value("m_entityID", m_entityID);

	ser.Value("m_factionID", m_factionID);
	ser.Value("m_isThreateningForHostileFactions", m_isThreateningForHostileFactions);

	bool observable = m_observable;
	ser.Value("m_observable", observable);
	if (ser.IsReading())
	{
		SetObservable(observable);

#ifdef CRYAISYSTEM_DEBUG
		ResetRecorderUnit();
#endif //CRYAISYSTEM_DEBUG
	}
}

//
//------------------------------------------------------------------------------------------------------------------------
bool CAIObject::CreateFormation(const unsigned int nCrc32ForFormationName, Vec3 vTargetPos /* = ZERO */)
{
	string sFormationName(gAIEnv.pFormationManager->GetFormationNameFromCRC32(nCrc32ForFormationName));
	return CreateFormation(sFormationName.c_str(), vTargetPos);
}

bool CAIObject::CreateFormation(const char* szName, Vec3 vTargetPos)
{
	if (m_pFormation)
	{
		gAIEnv.pFormationManager->ReleaseFormation(GetWeakRef(this), true);
	}

	m_pFormation = 0;

	if (!szName)
		return false;

	CCCPOINT(CAIObject_CreateFormation);

	m_pFormation = gAIEnv.pFormationManager->CreateFormation(GetWeakRef(this), szName, vTargetPos);
	return (m_pFormation != NULL);
}

//
//------------------------------------------------------------------------------------------------------------------------
bool CAIObject::ReleaseFormation(void)
{
	if (m_pFormation)
	{
		CCCPOINT(CAIObject_ReleaseFormation);
		gAIEnv.pFormationManager->ReleaseFormation(GetWeakRef(this), true);
		m_pFormation = 0;
		return true;
	}
	return false;
}

//
//------------------------------------------------------------------------------------------------------------------------
Vec3 CAIObject::GetVelocity() const
{
	IAIActorProxy* pProxy = GetProxy();
	if (!pProxy)
		return ZERO;

	IPhysicalEntity* pPhysicalEntity = pProxy->GetPhysics();
	if (!pPhysicalEntity)
		return ZERO;

	CCCPOINT(CAIObject_GetVelocity);

	// if living entity return that vel since that is actualy the rate of
	// change of position
	pe_status_living status;
	if (pPhysicalEntity->GetStatus(&status))
		return status.vel;

	pe_status_dynamics dSt;
	pPhysicalEntity->GetStatus(&dSt);

	return dSt.v;

}

//
//------------------------------------------------------------------------------------------------------------------------
const char* CAIObject::GetEventName(unsigned short eType) const
{
	switch (eType)
	{
	case AIEVENT_ONVISUALSTIMULUS:
		return "OnVisualStimulus";
	case AIEVENT_ONSOUNDEVENT:
		return "OnSoundEvent";
	case AIEVENT_AGENTDIED:
		return "AgentDied";
	case AIEVENT_SLEEP:
		return "Sleep";
	case AIEVENT_WAKEUP:
		return "Wakeup";
	case AIEVENT_ENABLE:
		return "Enable";
	case AIEVENT_DISABLE:
		return "Disable";
	case AIEVENT_PATHFINDON:
		return "PathfindOn";
	case AIEVENT_PATHFINDOFF:
		return "PathfindOff";
	case AIEVENT_CLEAR:
		return "Clear";
	case AIEVENT_DROPBEACON:
		return "DropBeacon";
	case AIEVENT_USE:
		return "Use";
	default:
		return "undefined";
	}
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIObject::Event(unsigned short eType, SAIEVENT* pEvent)
{
	switch (eType)
	{
	case AIEVENT_DISABLE:
		{
			SetObservable(false);
			m_bEnabled = false;
		}
		break;
	case AIEVENT_ENABLE:
		m_bEnabled = true;
		break;
	case AIEVENT_SLEEP:
		m_bEnabled = false;
		break;
	case AIEVENT_WAKEUP:
		m_bEnabled = true;
		break;
	default:
		break;
	}
}

void CAIObject::EntityEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_ATTACH_THIS:
	case ENTITY_EVENT_DETACH_THIS:
	case ENTITY_EVENT_ENABLE_PHYSICS:
		UpdateObservableSkipList();
		break;
	default:
		break;
	}
}

IPhysicalEntity* CAIObject::GetPhysics(bool bWantCharacterPhysics) const
{
	IEntity* pEntity = GetEntity();
	return pEntity ? pEntity->GetPhysics() : 0;
}

//====================================================================
// GetEntityID
//====================================================================
unsigned CAIObject::GetEntityID() const
{
	return m_entityID;
}

//====================================================================
// SetEntityID
//====================================================================
void CAIObject::SetEntityID(unsigned ID)
{
	AIAssert((ID == 0) || (gEnv->pEntitySystem->GetEntity(ID) != NULL));

	m_entityID = ID;
}

//====================================================================
// GetEntity
//
// just a little helper
//====================================================================

IEntity* CAIObject::GetEntity() const
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	return pEntitySystem ? pEntitySystem->GetEntity(m_entityID) : 0;
}

//====================================================================
// SetFormationUpdateSight
//====================================================================
void CAIObject::SetFormationUpdateSight(float range, float minTime, float maxTime)
{
	if (m_pFormation)
		m_pFormation->SetUpdateSight(range, minTime, maxTime);
}

//====================================================================
// IsHostile
//====================================================================
bool CAIObject::IsHostile(const IAIObject* pOther, bool /*bUsingAIIgnorePlayer*/) const
{
	if (!pOther)
		return false;

	if (!IsThreateningForHostileFactions())
		return false;

	if (gAIEnv.pFactionMap->GetReaction(this->GetFactionID(), pOther->GetFactionID()) == IFactionMap::Hostile)
		return true;

	unsigned short nType = ((CAIObject*)pOther)->GetType();
	return (nType == AIOBJECT_GRENADE || nType == AIOBJECT_RPG);
}

//====================================================================
// IsThreateningForHostileFactions
//====================================================================
bool CAIObject::IsThreateningForHostileFactions() const
{
	return m_isThreateningForHostileFactions;
}

//====================================================================
// SetThreateningForHostileFactions
//====================================================================
void CAIObject::SetThreateningForHostileFactions(const bool threatening)
{
	m_isThreateningForHostileFactions = threatening;
}

//====================================================================
// SetGroupId
//====================================================================
void CAIObject::SetGroupId(int id)
{
	m_groupId = id;
}

//====================================================================
// GetGroupId
//====================================================================
int CAIObject::GetGroupId() const
{
	return m_groupId;
}

uint8 CAIObject::GetFactionID() const
{
	return m_factionID;
}

void CAIObject::SetFactionID(uint8 factionID)
{
	if (m_factionID != factionID)
	{
		m_factionID = factionID;
		if (IsObservable())
		{
			ObservableParams params;
			params.faction = GetFactionID();

			gAIEnv.pVisionMap->ObservableChanged(GetVisionID(), params, eChangedFaction);
		}
	}
}

//===================================================================
// GetAIDebugRecord
//===================================================================
IAIDebugRecord* CAIObject::GetAIDebugRecord()
{
#ifdef CRYAISYSTEM_DEBUG
	return GetOrCreateRecorderUnit(this);
#else
	return NULL;
#endif //CRYAISYSTEM_DEBUG
}

#ifdef CRYAISYSTEM_DEBUG
CRecorderUnit* CAIObject::CreateAIDebugRecord()
{
	return GetOrCreateRecorderUnit(this, true);
}
#endif //CRYAISYSTEM_DEBUG

tAIObjectID CAIObject::GetAIObjectID() const
{
	tAIObjectID nID = m_refThis.GetObjectID();
	assert(nID);
	return nID;
}

bool CAIObject::IsEnabled() const
{
	return m_bEnabled;
}

bool CAIObject::IsTargetable() const
{
	return IsEnabled();
}

EntityId CAIObject::GetPerceivedEntityID() const
{
	return GetEntityID();
}

bool CAIObject::HasFormation()
{
	return m_pFormation != NULL;
}

const Vec3 CAIObject::GetFormationVelocity()
{
	CAISystem* pAISystem = GetAISystem();

	if (CLeader* pLeader = pAISystem->GetLeader(GetGroupId()))
	{
		CAIObject* pAIObject = pLeader->GetFormationOwner().GetAIObject();
		if (pAIObject)
		{
			return pAIObject->GetVelocity();
		}
	}

	IAIObject* pBoss = pAISystem->GetNearestObjectOfTypeInRange(this, AIOBJECT_ACTOR, 0, 20.0f, AIFAF_HAS_FORMATION | AIFAF_SAME_GROUP_ID);
	if (pBoss)
	{
		const Vec3 bossVelocity = pBoss->GetVelocity();
		return bossVelocity;
	}

	return Vec3Constants<float>::fVec3_Zero;
}

void CAIObject::SetProxy(IAIActorProxy* proxy)
{
	assert((m_nObjectType != AIOBJECT_WAYPOINT) &&
	       (m_nObjectType != AIANCHOR_COMBAT_HIDESPOT));
}

IAIActorProxy* CAIObject::GetProxy() const
{
	return NULL;
}

bool CAIObject::IsUpdatedOnce() const
{
	return m_bUpdatedOnce;
}

void CAIObject::Release()
{
	delete this;
}

const Vec3& CAIObject::GetViewDir() const
{
	return m_vView;
}

const Vec3& CAIObject::GetMoveDir() const
{
	return m_vMoveDir;
}

void CAIObject::SetFireDir(const Vec3& dir)
{
	m_vFireDir = dir;
}

const Vec3& CAIObject::GetFireDir() const
{
	return m_vFireDir;
}

void CAIObject::SetFirePos(const Vec3& pos)
{
	m_vFirePosition = pos;
}

const Vec3& CAIObject::GetFirePos() const
{
	return m_vFirePosition;
}

const Vec3& CAIObject::GetBodyDir() const
{
	return m_vBodyDir;
}

const Vec3& CAIObject::GetEntityDir() const
{
	return m_vEntityDir;
}

float CAIObject::GetRadius() const
{
	return m_fRadius;
}

unsigned short CAIObject::GetAIType() const
{
	return m_nObjectType;
}

IAIObject::ESubType CAIObject::GetSubType() const
{
	return m_objectSubType;
}

const Vec3& CAIObject::GetPos() const
{
	m_bTouched = true;
	return m_vPosition;
}

const Vec3 CAIObject::GetPosInNavigationMesh(const uint32 agentTypeID) const
{
	Vec3 outputLocation = GetPos();
	NavigationMeshID meshID = gAIEnv.pNavigationSystem->GetEnclosingMeshID(static_cast<NavigationAgentTypeID>(agentTypeID), outputLocation);
	if (meshID)
	{
		const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshID);
		const MNM::real_t strictVerticalRange = MNM::real_t(1.0f);
		const float fPushUp = 0.2f;
		const MNM::real_t pushUp = MNM::real_t(fPushUp);
		MNM::TriangleID triangleID(0);
		const MNM::vector3_t location_t(MNM::real_t(outputLocation.x), MNM::real_t(outputLocation.y), MNM::real_t(outputLocation.z) + pushUp);
		if (!(triangleID = mesh.navMesh.GetTriangleAt(location_t, strictVerticalRange, strictVerticalRange, nullptr)))
		{
			const MNM::real_t largeVerticalRange = MNM::real_t(6.0f);
			const MNM::real_t largeHorizontalRange = MNM::real_t(3.0f);
			MNM::vector3_t closestLocation;
			if (triangleID = mesh.navMesh.GetClosestTriangle(location_t, largeVerticalRange, largeHorizontalRange, nullptr, nullptr, &closestLocation))
			{
				outputLocation = closestLocation.GetVec3();
				outputLocation.z += fPushUp;
			}
		}
	}
	return outputLocation;
}

const VisionID& CAIObject::GetVisionID() const
{
	return m_visionID;
}

void CAIObject::SetVisionID(const VisionID& visionID)
{
	m_visionID = visionID;
}

IAIObject::EFieldOfViewResult CAIObject::IsPointInFOV(const Vec3& pos, float distanceScale /*= 1.0f*/) const
{
	return eFOV_Outside;
}

IAIObject::EFieldOfViewResult CAIObject::IsObjectInFOV(const IAIObject* pTarget, float distanceScale /*= 1.0f*/) const
{
	return eFOV_Outside;
}

void CAIObject::SetObservable(bool observable)
{
	if (m_observable != observable)
	{
		if (observable)
		{
			ObservableParams observableParams;
			observableParams.entityId = GetEntityID();
			observableParams.faction = GetFactionID();
			observableParams.typeMask = GetObservableTypeMask();

			GetObservablePositions(observableParams);

			PhysSkipList skipList;
			GetPhysicalSkipEntities(skipList);

			observableParams.skipListSize = std::min<size_t>(skipList.size(), ObservableParams::MaxSkipListSize);
			for (size_t i = 0; i < static_cast<size_t>(observableParams.skipListSize); ++i)
				observableParams.skipList[i] = skipList[i];

			// Márcio: Should check for associated objects and add them here too?
			if (!m_visionID)
				m_visionID = gAIEnv.pVisionMap->CreateVisionID(GetName());

			gAIEnv.pVisionMap->RegisterObservable(m_visionID, observableParams);
		}
		else
		{
			if (m_visionID)
				gAIEnv.pVisionMap->UnregisterObservable(m_visionID);
		}

		m_observable = observable;
	}
}

bool CAIObject::IsObservable() const
{
	return m_observable;
}

void CAIObject::GetObservablePositions(ObservableParams& observableParams) const
{
	observableParams.observablePositionsCount = 1;
	observableParams.observablePositions[0] = GetPos();
}

uint32 CAIObject::GetObservableTypeMask() const
{
	return General;
}

void CAIObject::GetPhysicalSkipEntities(PhysSkipList& skipList) const
{
	if (IPhysicalEntity* physics = GetPhysics())
		stl::push_back_unique(skipList, physics);

	if (IPhysicalEntity* charPhysics = GetPhysics(true))
		stl::push_back_unique(skipList, charPhysics);
}

void CAIObject::UpdateObservableSkipList() const
{
	if (m_observable)
	{
		PhysSkipList skipList;
		GetPhysicalSkipEntities(skipList);

		ObservableParams observableParams;
		observableParams.skipListSize = std::min<uint32>(skipList.size(), ObservableParams::MaxSkipListSize);
		for (size_t i = 0; i < static_cast<size_t>(observableParams.skipListSize); ++i)
			observableParams.skipList[i] = skipList[i];

		gAIEnv.pVisionMap->ObservableChanged(GetVisionID(), observableParams, eChangedSkipList);
	}
}

//====================================================================
// SAIActorTargetRequest Serialize
//====================================================================
void SAIActorTargetRequest::Serialize(TSerialize ser)
{
	ser.BeginGroup("SAIActorTargetRequest");
	{
		_ser_value_(id);
		if (id != 0)
		{
			_ser_value_(approachLocation);
			_ser_value_(approachDirection);
			_ser_value_(animLocation);
			_ser_value_(animDirection);
			_ser_value_(vehicleName);
			_ser_value_(vehicleSeat);
			_ser_value_(speed);
			_ser_value_(directionTolerance);
			_ser_value_(startArcAngle);
			_ser_value_(startWidth);
			_ser_value_(signalAnimation);
			_ser_value_(projectEndPoint);
			_ser_value_(lowerPrecision);
			_ser_value_(useAssetAlignment);
			_ser_value_(animation);
			ser.EnumValue("stance", stance, STANCE_NULL, STANCE_LAST);
			// TODO: Pointers!
			//		TAnimationGraphQueryID * pQueryStart;
			//		TAnimationGraphQueryID * pQueryEnd;
		}
	}
	ser.EndGroup();
}

//====================================================================
// SAIPredictedCharacterState Serialize
//====================================================================
void SAIPredictedCharacterState::Serialize(TSerialize ser)
{
	ser.Value("position", position);
	ser.Value("velocity", velocity);
	ser.Value("predictionTime", predictionTime);
}

//====================================================================
// SAIPredictedCharacterStates Serialize
//====================================================================
void SAIPredictedCharacterStates::Serialize(TSerialize ser)
{
	ser.BeginGroup("SAIPredictedCharacterStates");
	{
		ser.Value("nStates", nStates);
		int counter(0);
		char stateGroupName[32];
		for (int i(0); i < maxStates; ++i, ++counter)
		{
			cry_sprintf(stateGroupName, "State_%d", counter);
			ser.BeginGroup(stateGroupName);
			{
				states[i].Serialize(ser);
			} ser.EndGroup();
		}
	} ser.EndGroup();
}
//====================================================================
// SOBJECTSTATE Serialize
//====================================================================
void SOBJECTSTATE::Serialize(TSerialize ser)
{
	ser.BeginGroup("SOBJECTSTATE");
	{
		ser.Value("jump", jump);
		ser.Value("bCloseContact", bCloseContact);
		ser.ValueWithDefault("fDesiredSpeed", fDesiredSpeed, 1.f);
		ser.Value("fMovementUrgency", fMovementUrgency);
		ser.Value("bodystate", bodystate);
		ser.Value("vForcedNavigation", vForcedNavigation);
		ser.Value("vBodyTargetDir", vBodyTargetDir);
		ser.Value("vLookTargetPos", vLookTargetPos);
		ser.Value("vDesiredBodyDirectionAtTarget", vDesiredBodyDirectionAtTarget);
		ser.Value("movementContext", movementContext);
		ser.EnumValue("eTargetType", eTargetType, AITARGET_NONE, AITARGET_LAST);
		ser.EnumValue("eTargetThreat", eTargetThreat, AITHREAT_NONE, AITHREAT_LAST);
		ser.EnumValue("ePeakTargetThreat", ePeakTargetThreat, AITHREAT_NONE, AITHREAT_LAST);
		ser.EnumValue("ePeakTargetType", ePeakTargetType, AITARGET_NONE, AITARGET_LAST);
		ser.Value("ePeakTargetID", ePeakTargetID);
		ser.Value("eTargetID", eTargetID);
		ser.Value("vTargetPos", vTargetPos);
		ser.EnumValue("ePreviousPeakTargetThreat", ePreviousPeakTargetThreat, AITHREAT_NONE, AITHREAT_LAST);
		ser.EnumValue("ePreviousPeakTargetType", ePreviousPeakTargetType, AITARGET_NONE, AITARGET_LAST);
		ser.Value("ePreviousPeakTargetID", ePreviousPeakTargetID);
		ser.Value("bTargetEnabled", bTargetEnabled);
		ser.Value("nTargetType", nTargetType);
		ser.Value("lean", lean);
		ser.Value("peekOver", peekOver);
		ser.Value("aimTargetIsValid", aimTargetIsValid);
		ser.Value("weaponAccessories", weaponAccessories);

		if (ser.IsReading())
		{
			fire = eAIFS_Off;
			fireSecondary = eAIFS_Off;
			fireMelee = eAIFS_Off;
		}
		ser.Value("aimObstructed", aimObstructed);
		ser.Value("forceWeaponAlertness", forceWeaponAlertness);
		ser.Value("fProjectileSpeedScale", projectileInfo.fSpeedScale);
		ser.Value("projectileTrackingId", projectileInfo.trackingId);
		ser.Value("vShootTargetPos", vShootTargetPos);
		ser.Value("vAimTargetPos", vAimTargetPos);
		ser.Value("allowStrafing", allowStrafing);
		ser.Value("allowEntityClampingByAnimation", allowEntityClampingByAnimation);
		ser.Value("fDistanceToPathEnd", fDistanceToPathEnd);
		ser.Value("curActorTargetFinishPos", curActorTargetFinishPos);
		//		ser.Value("remainingPath", remainingPath);
		ser.Value("fDistanceFromTarget", fDistanceFromTarget);
		ser.EnumValue("eTargetStuntReaction", eTargetStuntReaction, AITSR_NONE, AITSR_LAST);
		ser.EnumValue("curActorTargetPhase", curActorTargetPhase, eATP_None, eATP_Error);
		actorTargetReq.Serialize(ser);
		// serialize signals
		ser.BeginGroup("SIGNALS");
		{
			int signalsCount = vSignals.size();
			ser.Value("signalsCount", signalsCount);
			if (ser.IsReading())
			{
				vSignals.resize(signalsCount);
			}
			for (DynArray<AISIGNAL>::iterator ai(vSignals.begin()); ai != vSignals.end(); ++ai)
			{
				ser.BeginGroup("Signal");
				{
					AISIGNAL& signal = *ai;
					signal.Serialize(ser);
				}
				ser.EndGroup();
			}
		}
		ser.EndGroup();

		ser.Value("secondaryIndex", secondaryIndex);
		//this is not used in C2, don't waste space..
		//ser.Value("predictedCharacterStates", predictedCharacterStates);
	}
	ser.EndGroup();
}

//====================================================================
// AgentPerceptionParameters Serialize
//====================================================================
void AgentPerceptionParameters::Serialize(TSerialize ser)
{
	ser.BeginGroup("AgentPerceptionParameters");
	{
		_ser_value_(sightRange);
		_ser_value_(sightRangeVehicle);
		_ser_value_(sightNearRange);
		_ser_value_(sightDelay);
		_ser_value_(FOVPrimary);
		_ser_value_(FOVSecondary);
		_ser_value_(stanceScale);
		_ser_value_(audioScale);
		_ser_value_(targetPersistence);
		_ser_value_(reactionTime);
		_ser_value_(collisionReactionScale);
		_ser_value_(stuntReactionTimeOut);
		_ser_value_(forgetfulness);
		_ser_value_(forgetfulnessTarget);
		_ser_value_(forgetfulnessSeek);
		_ser_value_(forgetfulnessMemory);
		_ser_value_(isAffectedByLight);
		_ser_value_(minAlarmLevel);
		_ser_value_(bulletHitRadius);
		_ser_value_(cloakMaxDistStill);
		_ser_value_(cloakMaxDistMoving);
		_ser_value_(cloakMaxDistCrouchedAndStill);
		_ser_value_(cloakMaxDistCrouchedAndMoving);
		_ser_value_(perceptionScale.audio);
		_ser_value_(perceptionScale.visual);
	}
	ser.EndGroup();
}

//====================================================================
// AgentPerceptionParameters Serialize
//====================================================================
void AgentParameters::Serialize(TSerialize ser)
{
	ser.BeginGroup("AgentParameters");
	{
		m_PerceptionParams.Serialize(ser);

		_ser_value_(m_nGroup);
		_ser_value_(m_CombatClass);
		_ser_value_(m_fAccuracy);
		_ser_value_(m_fPassRadius);
		_ser_value_(m_fStrafingPitch);    //if this > 0, will do a strafing draw line firing. 04/12/05 Tetsuji
		_ser_value_(m_fAttackRange);
		_ser_value_(m_fCommRange);
		_ser_value_(m_fAttackZoneHeight);
		_ser_value_(m_weaponAccessories);
		_ser_value_(m_fMeleeRange);
		_ser_value_(m_fMeleeRangeShort);
		_ser_value_(m_fMeleeHitRange);
		_ser_value_(m_fMeleeAngleCosineThreshold);
		_ser_value_(factionID);
		_ser_value_(factionHostility);
		_ser_value_(m_bPerceivePlayer);
		_ser_value_(m_fAwarenessOfPlayer);
		_ser_value_(m_bInvisible);
		_ser_value_(m_fCloakScale);
		_ser_value_(m_fCloakScaleTarget);
		_ser_value_(m_lookIdleTurnSpeed);
		_ser_value_(m_lookCombatTurnSpeed);
		_ser_value_(m_aimTurnSpeed);
		_ser_value_(m_fireTurnSpeed);
		_ser_value_(m_bAiIgnoreFgNode);
		_ser_value_(m_fProjectileLaunchDistScale);
	}
	ser.EndGroup();
}

//--------------------------------------------------------------------
