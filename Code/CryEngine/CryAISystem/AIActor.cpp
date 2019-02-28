// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIActor.h"
#include "PipeUser.h"

#include "TargetSelection/TargetTrackManager.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "Group/GroupManager.h"
#include "Factions/FactionMap.h"
#include "Formation/FormationManager.h"
#include <CryCore/CryCrc32.h>
#include <CryEntitySystem/IEntity.h>

#include <CryAISystem/BehaviorTree/IBehaviorTree.h>
#include <CryAISystem/VisionMapTypes.h>

#include <limits>

//#pragma optimize("", off)
//#pragma inline_depth(0)

static const float UNINITIALIZED_COS_CACHE = 2.0f;

//////////////////////////////////////////////////////////////////////////
// AI Actor Collision Avoidance
//////////////////////////////////////////////////////////////////////////

CActorCollisionAvoidance::CActorCollisionAvoidance(CAIActor* pActor)
	: m_pActor(pActor)
	, m_radiusIncrement(0.0f)
{
	gAIEnv.pCollisionAvoidanceSystem->RegisterAgent(this);
}

CActorCollisionAvoidance::~CActorCollisionAvoidance()
{
	gAIEnv.pCollisionAvoidanceSystem->UnregisterAgent(this);
}

void CActorCollisionAvoidance::Reset()
{
	m_radiusIncrement = 0.0f;
}

void CActorCollisionAvoidance::Serialize(TSerialize ser)
{
	ser.BeginGroup("CollisionAvoidance");
	{
		ser.Value("m_radiusIncrement", m_radiusIncrement);
	}
	ser.EndGroup();
}

NavigationAgentTypeID CActorCollisionAvoidance::GetNavigationTypeId() const
{
	return m_pActor->GetNavigationTypeID();
}

const INavMeshQueryFilter* CActorCollisionAvoidance::GetNavigationQueryFilter() const
{
	return nullptr;
}

const char* CActorCollisionAvoidance::GetName() const
{
	return m_pActor->GetName();
}

ICollisionAvoidanceAgent::TreatType CActorCollisionAvoidance::GetTreatmentType() const
{
	if (!m_pActor->IsEnabled() || !m_pActor->GetMovementAbility().collisionAvoidanceParticipation)
		return ICollisionAvoidanceAgent::TreatType::None;

	uint16 aiType = m_pActor->GetAIType();
	if (aiType == AIOBJECT_PLAYER)
	{
		// player is always treated only as obstacle
		return ICollisionAvoidanceAgent::TreatType::Obstacle;
	}

	if ((aiType == AIOBJECT_ALIENTICK) || (aiType == AIOBJECT_ACTOR) || (aiType == AIOBJECT_INFECTED))
	{
		const float targetCutoff = gAIEnv.CVars.CollisionAvoidanceTargetCutoffRange;
		const float pathEndCutoff = gAIEnv.CVars.CollisionAvoidancePathEndCutoffRange;
		const float smartObjectCutoff = gAIEnv.CVars.CollisionAvoidanceSmartObjectCutoffRange;

		CPipeUser* pPipeUser = m_pActor->CastToCPipeUser();

		bool bIsMoving = (fabs_tpl(m_pActor->m_State.fDesiredSpeed) > 0.0001f);
		bool bCuttoff = (m_pActor->m_State.fDistanceFromTarget < targetCutoff)
			|| (m_pActor->m_State.fDistanceToPathEnd < pathEndCutoff)
			|| (pPipeUser && pPipeUser->GetPendingSmartObjectID() && (m_pActor->m_State.fDistanceToPathEnd < smartObjectCutoff));

		return (bIsMoving && !bCuttoff) ? ICollisionAvoidanceAgent::TreatType::Agent : ICollisionAvoidanceAgent::TreatType::Obstacle;
	}
	return ICollisionAvoidanceAgent::TreatType::None;
}

void CActorCollisionAvoidance::InitializeCollisionAgent(CCollisionAvoidanceSystem::SAgentParams& agent) const
{
	const float forcedSpeed = gAIEnv.CVars.DebugCollisionAvoidanceForceSpeed;
	const bool bUseForcedSpeed = fabs_tpl(forcedSpeed) > 0.0001f;

	float minSpeed;
	float maxSpeed;
	float normalSpeed;

	m_pActor->GetMovementSpeedRange(m_pActor->m_State.fMovementUrgency, false, normalSpeed, minSpeed, maxSpeed);

	agent.radius = m_pActor->m_Parameters.m_fPassRadius + gAIEnv.CVars.CollisionAvoidanceAgentExtraFat;
	if (gAIEnv.CVars.CollisionAvoidanceEnableRadiusIncrement)
		agent.radius += m_radiusIncrement;
	agent.maxSpeed = min(m_pActor->m_State.fDesiredSpeed, maxSpeed);
	agent.maxAcceleration = min(agent.maxAcceleration, m_pActor->m_movementAbility.maxAccel);
	agent.currentLocation = m_pActor->GetPhysicsPos();
	agent.currentVelocity = Vec2(m_pActor->GetVelocity());

	agent.desiredVelocity = bUseForcedSpeed ? Vec2(m_pActor->GetMoveDir() * forcedSpeed) : Vec2(m_pActor->m_State.vMoveDir * m_pActor->m_State.fDesiredSpeed);
	agent.currentLookDirection = Vec2(agent.desiredVelocity);
}

void CActorCollisionAvoidance::InitializeCollisionObstacle(CCollisionAvoidanceSystem::SObstacleParams& obstacle) const
{
	obstacle.currentLocation = m_pActor->GetPhysicsPos();
	obstacle.currentVelocity = Vec2(m_pActor->GetVelocity());
	obstacle.radius = m_pActor->m_Parameters.m_fPassRadius + gAIEnv.CVars.CollisionAvoidanceAgentExtraFat;
}
	

void CActorCollisionAvoidance::ApplyComputedVelocity(const Vec2& avoidanceVelocity, float updateTime)
{
	m_pActor->m_State.allowStrafing = false;
	m_pActor->ResetBodyTargetDir();

	const Vec3 currentVelocity(m_pActor->m_State.vMoveDir * m_pActor->m_State.fDesiredSpeed);
	const Vec3 avoidanceVelocity3D(avoidanceVelocity.x, avoidanceVelocity.y, currentVelocity.z);

	if ((avoidanceVelocity - Vec2(currentVelocity)).GetLength2() >= 0.000001f)
	{
		float speedSq = avoidanceVelocity3D.len2();

		if (m_pActor->m_State.bodyOrientationMode != FullyTowardsAimOrLook)
		{
			m_pActor->m_State.allowStrafing = true;
			m_pActor->SetBodyTargetDir(m_pActor->m_State.vMoveDir);
		}

		if (speedSq > 0.000001f)
		{
			float speed = sqrt_tpl(speedSq);

			m_pActor->m_State.vMoveDir = avoidanceVelocity3D / speed;
			m_pActor->m_State.fDesiredSpeed = speed;
		}
		else
		{
			m_pActor->m_State.vMoveDir.zero();
			m_pActor->m_State.fDesiredSpeed = 0.0f;
		}

		m_pActor->m_State.vMoveTarget.zero();
	}

	if (gAIEnv.CVars.CollisionAvoidanceEnableRadiusIncrement)
	{
		if (m_pActor->m_State.fDesiredSpeed > 0.5f)
		{
			m_radiusIncrement = min(
				m_radiusIncrement + (m_pActor->m_movementAbility.collisionAvoidanceRadiusIncrement * gAIEnv.CVars.CollisionAvoidanceRadiusIncrementIncreaseRate * updateTime),
				m_pActor->m_movementAbility.collisionAvoidanceRadiusIncrement
			);
		}
		else
		{
			m_radiusIncrement = max(
				m_radiusIncrement - (m_pActor->m_movementAbility.collisionAvoidanceRadiusIncrement * gAIEnv.CVars.CollisionAvoidanceRadiusIncrementDecreaseRate * updateTime),
				0.0f
			);
		}
	}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define _ser_value_(val) ser.Value( # val, val)

CAIActor::CAIActor()
	: m_bCheckedBody(true)
#ifdef CRYAISYSTEM_DEBUG
	,m_healthHistory(0)
#endif
	, m_lightLevel(AILL_LIGHT)
	, m_usingCombatLight(false)
	, m_cachedWaterOcclusionValue(0.0f)
	, m_vLastFullUpdatePos(ZERO)
	, m_lastFullUpdateStance(STANCE_NULL)
	, m_observer(false)
	, m_FOVPrimaryCos(UNINITIALIZED_COS_CACHE)
	, m_FOVSecondaryCos(UNINITIALIZED_COS_CACHE)
	, m_territoryShape(0)
	, m_lastBodyDir(ZERO)
	, m_bodyTurningSpeed(0)
	, m_stimulusStartTime(-100.f)
	, m_activeCoordinationCount(0)
	, m_navigationTypeID(0)
	, m_runningBehaviorTree(false)
	, m_collisionAvoidanceAgent(this)
{
	_fastcast_CAIActor = true;

	AILogComment("CAIActor (%p)", this);
}

CAIActor::~CAIActor()
{
	StopBehaviorTree();

	AILogComment("~CAIActor  %s (%p)", GetName(), this);

	m_proxy.reset();

	gAIEnv.pGroupManager->RemoveGroupMember(GetGroupId(), GetAIObjectID());

	CAISystem* pAISystem = GetAISystem();

	CAIGroup* pGroup = pAISystem->GetAIGroup(GetGroupId());
	if (pGroup)
		pGroup->RemoveMember(this);

	SetObserver(false);

#ifdef CRYAISYSTEM_DEBUG
	delete m_healthHistory;
#endif

	m_State.ClearSignals();

	pAISystem->NotifyEnableState(this, false);
	pAISystem->UnregisterAIActor(StaticCast<CAIActor>(GetSelfReference()));
}

void CAIActor::SetBehaviorVariable(const char* variableName, bool value)
{
	{
		Variables::Collection* variableCollection = GetAISystem()->GetIBehaviorTreeManager()->GetBehaviorVariableCollection_Deprecated(GetEntityID());
		const Variables::Declarations* variableDeclarations = GetAISystem()->GetIBehaviorTreeManager()->GetBehaviorVariableDeclarations_Deprecated(GetEntityID());
		if (!variableCollection || !variableDeclarations)
			return;

		Variables::VariableID variableID = Variables::GetVariableID(variableName);

		if (variableDeclarations->IsDeclared(variableID))
			variableCollection->SetVariable(variableID, value);
		else
			AIWarning("Variable '%s' missing from %s's Behavior Tree.", variableName, GetName());
	}
}

bool CAIActor::GetBehaviorVariable(const char* variableName) const
{
	Variables::Collection* variableCollection = GetAISystem()->GetIBehaviorTreeManager()->GetBehaviorVariableCollection_Deprecated(GetEntityID());
	const Variables::Declarations* variableDeclarations = GetAISystem()->GetIBehaviorTreeManager()->GetBehaviorVariableDeclarations_Deprecated(GetEntityID());
	if (!variableCollection || !variableDeclarations)
		return false;

	Variables::VariableID variableID = Variables::GetVariableID(variableName);

	bool value = false;
	if (variableDeclarations->IsDeclared(variableID))
		variableCollection->GetVariable(variableID, &value);
	else
		AIWarning("Variable '%s' missing from %s's Behavior Tree.", variableName, GetName());

	return value;
}

void CAIActor::ResetModularBehaviorTree(EObjectResetType type)
{
	if (type == AIOBJRESET_SHUTDOWN)
	{
		StopBehaviorTree();
	}
	else
	{
		if (!m_modularBehaviorTreeName.empty())
		{
			StartBehaviorTree(m_modularBehaviorTreeName.c_str());
		}
		else
		{
			// Try to load a Modular Behavior Tree
			if (IScriptTable* table = GetEntity()->GetScriptTable())
			{
				SmartScriptTable properties;
				if (table->GetValue("Properties", properties) && properties)
				{
					const char* behaviorTreeName = NULL;

					if (properties->GetValue("esModularBehaviorTree", behaviorTreeName) && behaviorTreeName && behaviorTreeName[0])
					{
						MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Modular Behavior Tree Runtime");

						StartBehaviorTree(behaviorTreeName);
					}
				}
			}
		}
	}
}

const SAIBodyInfo& CAIActor::QueryBodyInfo()
{
	m_proxy->QueryBodyInfo(m_bodyInfo);
	return m_bodyInfo;
}

const SAIBodyInfo& CAIActor::GetBodyInfo() const
{
	return m_bodyInfo;
}

void CAIActor::SetPos(const Vec3& pos, const Vec3& dirFwrd)
{
	gAIEnv.pActorLookUp->Prepare(0);

	Vec3 position(pos);
	Vec3 vEyeDir(dirFwrd);

	if (IAIActorProxy* pProxy = GetProxy())
	{
		//Overwrite parameters by values from body info if possible
		SAIBodyInfo bodyInfo;
		if (pProxy->QueryBodyInfo(bodyInfo))
		{
			assert(bodyInfo.vEyeDir.IsValid());
			assert(bodyInfo.vEyePos.IsValid());
			assert(bodyInfo.vFireDir.IsValid());
			assert(bodyInfo.vFirePos.IsValid());

			position = bodyInfo.vEyePos;

			vEyeDir = bodyInfo.GetEyeDir();

			assert(vEyeDir.IsUnit());

			SetViewDir(vEyeDir);
			SetBodyDir(bodyInfo.GetBodyDir());

			SetFirePos(bodyInfo.vFirePos);
			SetFireDir(bodyInfo.vFireDir);
			SetMoveDir(bodyInfo.vMoveDir);
			SetEntityDir(bodyInfo.GetBodyDir());

			assert(bodyInfo.vFireDir.IsUnit());
			assert(bodyInfo.vMoveDir.IsUnit() || bodyInfo.vMoveDir.IsZero());
			assert(bodyInfo.vEntityDir.IsUnit() || bodyInfo.vEntityDir.IsZero());
		}
	}

	CAIObject::SetPos(position, vEyeDir); // can set something else than passed position

	if (m_pFormation)
		m_pFormation->Update();

	if (m_observer)
	{
		ObserverParams observerParams;
		observerParams.eyePosition = position;
		observerParams.eyeDirection = vEyeDir;

		gAIEnv.pVisionMap->ObserverChanged(GetVisionID(), observerParams, eChangedPosition | eChangedOrientation);
	}

	gAIEnv.pActorLookUp->UpdatePosition(this, position);
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::Reset(EObjectResetType type)
{
	CAIObject::Reset(type);

	if (type == AIOBJRESET_INIT)
	{
		m_initialPosition.pos = GetEntity()->GetPos();
		m_initialPosition.isValid = true;
	}

	m_bCheckedBody = true;

	CAISystem* pAISystem = GetAISystem();

	m_State.FullReset();

	// TODO : (MATT) Hack - move into the method {2007/10/30:21:01:16}
	m_State.eLookStyle = LOOKSTYLE_DEFAULT;

	m_State.bAllowLowerBodyToTurn = true;

	ReleaseFormation();

	if (!m_proxy)
	{
		AILogComment("CAIActor(%p) Creating AIActorProxy", this);
		m_proxy.reset(pAISystem->GetActorProxyFactory()->CreateActorProxy(GetEntityID()));
		gAIEnv.pActorLookUp->UpdateProxy(this);
	}

	m_proxy->Reset(type);

	m_bEnabled = true;
	m_bUpdatedOnce = false;

#ifdef AI_COMPILE_WITH_PERSONAL_LOG
	m_personalLog.Clear();
#endif

#ifdef CRYAISYSTEM_DEBUG
	if (m_healthHistory)
		m_healthHistory->Reset();
#endif

	// synch self with owner entity if there is one
	IEntity* pEntity(GetEntity());
	if (pEntity)
	{
		m_bEnabled = pEntity->IsActivatedForUpdates();
		SetPos(pEntity->GetPos());
	}

	m_lightLevel = AILL_LIGHT;
	m_usingCombatLight = false;

	m_cachedWaterOcclusionValue = 0.0f;

	m_vLastFullUpdatePos.zero();
	m_lastFullUpdateStance = STANCE_NULL;

	m_blackBoard.Clear();
	m_blackBoard.GetForScript()->SetValue("Owner", this->GetName());

	m_perceptionHandlerModifiers.clear();

	ResetPersonallyHostiles();

	ResetModularBehaviorTree(type);

	const char* navigationTypeName = m_proxy->GetNavigationTypeName();
	if (navigationTypeName && *navigationTypeName)
	{
		if (NavigationAgentTypeID id = gAIEnv.pNavigationSystem->GetAgentTypeID(navigationTypeName))
		{
			m_navigationTypeID = id;
		}
	}

	pAISystem->NotifyEnableState(this, m_bEnabled && (type == AIOBJRESET_INIT));

	// Clear the W/T names we use for lookup
	m_territoryShape = 0;
	m_territoryShapeName.clear();

	m_lastBodyDir.zero();
	m_bodyTurningSpeed = 0;

	if (GetType() != AIOBJECT_PLAYER)
	{
		SetObserver(type == AIOBJRESET_INIT);
		SetObservable(type == AIOBJRESET_INIT);
	}

	m_stimulusStartTime = -100.f;

	m_bodyInfo = SAIBodyInfo();

	m_activeCoordinationCount = 0;

	m_collisionAvoidanceAgent.Reset();
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::ParseParameters(const AIObjectParams& params, bool bParseMovementParams)
{
	SetParameters(params.m_sParamStruct);

	if (bParseMovementParams)
		m_movementAbility = params.m_moveAbility;

	GetAISystem()->NotifyEnableState(this, m_bEnabled);
}

//====================================================================
// CAIObject::OnObjectRemoved
//====================================================================
void CAIActor::OnObjectRemoved(CAIObject* pObject)
{
	CAIObject::OnObjectRemoved(pObject);

	// make sure no pending signal left from removed AIObjects
	if (!m_State.vSignals.empty())
	{
		if (EntityId removedEntityID = pObject->GetEntityID())
		{
			DynArray<AISIGNAL>::iterator it = m_State.vSignals.begin();
			DynArray<AISIGNAL>::iterator end = m_State.vSignals.end();

			for (; it != end; )
			{
				AISIGNAL& curSignal = *it;

				if (curSignal.senderID == removedEntityID)
				{
					delete static_cast<AISignalExtraData*>(curSignal.pEData);
					it = m_State.vSignals.erase(it);
					end = m_State.vSignals.end();
				}
				else
					++it;
			}
		}
	}

	RemovePersonallyHostile(pObject->GetAIObjectID());
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::Update(IAIObject::EUpdateType type)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	IAIActorProxy* pAIActorProxy = GetProxy();

	if (!CastToCPipeUser())
	{
		if (!IsEnabled())
		{
			AIWarning("CAIActor::Update: Trying to update disabled AI Actor: %s", GetName());
			AIAssert(0);
			return;
		}

		// There should never be AI Actors without proxies.
		if (!pAIActorProxy)
		{
			AIWarning("CAIActor::Update: AI Actor does not have proxy: %s", GetName());
			AIAssert(0);
			return;
		}
		// There should never be AI Actors without physics.
		if (!GetPhysics())
		{
			AIWarning("CAIActor::Update: AI Actor does not have physics: %s", GetName());
			AIAssert(0);
			return;
		}
		// dead AI Actors should never be updated
		if (pAIActorProxy->IsDead())
		{
			AIWarning("CAIActor::Update: Trying to update dead AI Actor: %s ", GetName());
			AIAssert(0);
			return;
		}
	}

	QueryBodyInfo();

	UpdateCloakScale();

	CAISystem* pAISystem = GetAISystem();

	// Determine if position has changed
	const Vec3& vPos = GetPos();
	if (type == EUpdateType::Full)
	{
		if (!IsEquivalent(m_vLastFullUpdatePos, vPos, 1.f))
		{
			// Recalculate the water occlusion at the new point
			m_cachedWaterOcclusionValue = pAISystem->GetWaterOcclusionValue(vPos);

			m_vLastFullUpdatePos = vPos;
			m_lastFullUpdateStance = m_bodyInfo.stance;
		}
	}

	const float dt = pAISystem->GetFrameDeltaTime();
	if (dt > 0.f)
	{
		// Update body angle and body turn speed
		float turnAngle = Ang3::CreateRadZ(m_lastBodyDir, GetEntityDir());
		m_bodyTurningSpeed = turnAngle / dt;
	}
	else
	{
		m_bodyTurningSpeed = 0;
	}

	m_lastBodyDir = GetEntityDir();

	if (!CastToCPipeUser())
	{
		if (type == EUpdateType::Full)
		{
			m_lightLevel = pAISystem->GetLightManager()->GetLightLevelAt(GetPos(), this, &m_usingCombatLight);
		}

		// make sure to update direction when entity is not moved
		const SAIBodyInfo& bodyInfo = GetBodyInfo();
		SetPos(bodyInfo.vEyePos);
		SetEntityDir(bodyInfo.vEntityDir);
		SetBodyDir(bodyInfo.GetBodyDir());

		// AI Actor goto stuff
		if (!m_State.vMoveTarget.IsZero())
		{
			Vec3 vToMoveTarget = m_State.vMoveTarget - GetPos();
			if (!m_movementAbility.b3DMove)
			{
				vToMoveTarget.z = 0.f;
			}
			if (vToMoveTarget.GetLengthSquared() < sqr(m_movementAbility.pathRadius))
			{
				ResetLookAt();
				SetBodyTargetDir(bodyInfo.vEntityDir);
			}
			else
			{
				SetBodyTargetDir(vToMoveTarget.GetNormalized());
			}
		}
		// End of AI Actor goto stuff

		SetMoveDir(bodyInfo.vMoveDir);
		m_State.vMoveDir = bodyInfo.vMoveDir;

		SetViewDir(bodyInfo.GetEyeDir());

		CAIObject* pAttTarget = m_refAttentionTarget.GetAIObject();
		if (pAttTarget && pAttTarget->IsEnabled())
		{
			if (CanSee(pAttTarget->GetVisionID()))
			{
				m_State.eTargetType = AITARGET_VISUAL;
				m_State.nTargetType = pAttTarget->GetType();
				m_State.bTargetEnabled = true;
			}
			else
			{
				switch (m_State.eTargetType)
				{
				case AITARGET_VISUAL:
					m_State.eTargetThreat = AITHREAT_AGGRESSIVE;
					m_State.eTargetType = AITARGET_MEMORY;
					m_State.nTargetType = pAttTarget->GetType();
					m_State.bTargetEnabled = true;
					m_stimulusStartTime = GetAISystem()->GetFrameStartTimeSeconds();
					break;

				case AITARGET_MEMORY:
				case AITARGET_SOUND:
					if (GetAISystem()->GetFrameStartTimeSeconds() - m_stimulusStartTime >= 5.f)
					{
						m_State.nTargetType = -1;
						m_State.bTargetEnabled = false;
						m_State.eTargetThreat = AITHREAT_NONE;
						m_State.eTargetType = AITARGET_NONE;

						SetAttentionTarget(NILREF);
					}
					break;
				}
			}
		}
		else
		{
			m_State.nTargetType = -1;
			m_State.bTargetEnabled = false;
			m_State.eTargetThreat = AITHREAT_NONE;
			m_State.eTargetType = AITARGET_NONE;

			SetAttentionTarget(NILREF);
		}
	}

	m_bUpdatedOnce = true;
}

void CAIActor::UpdateProxy(EUpdateType type)
{
	IAIActorProxy* pAIActorProxy = GetProxy();

	SetMoveDir(m_State.vMoveDir);

	// Always update the AI proxy, also during dry updates. The Animation system
	// needs correct and constantly updated predictions to correctly set animation
	// parameters.
	// (MATT) Try avoiding UpdateMind, which triggers script, signal and behaviour code, if only a dry update {2009/12/06}
	assert(pAIActorProxy);
	if (pAIActorProxy)
		pAIActorProxy->Update(m_State, (type == EUpdateType::Full));
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::UpdateCloakScale()
{
	m_Parameters.m_fCloakScale = m_Parameters.m_fCloakScaleTarget;
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::UpdateDisabled(EUpdateType type)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// (MATT) I'm assuming that AIActor should always have a proxy, or this could be bad for performance {2009/04/03}
	IAIActorProxy* pProxy = GetProxy();
	if (pProxy)
		pProxy->CheckUpdateStatus();
}

//===================================================================
// SetNavNodes
//===================================================================
void CAIActor::UpdateDamageParts(DamagePartVector& parts)
{
	IAIActorProxy* pProxy = GetProxy();
	if (!pProxy)
		return;

	IPhysicalEntity* phys = pProxy->GetPhysics(true);
	if (!phys)
		return;

	bool queryDamageValues = true;

	static ISurfaceTypeManager* pSurfaceMan = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();

	pe_status_nparts statusNParts;
	int nParts = phys->GetStatus(&statusNParts);

	if ((int)parts.size() != nParts)
	{
		parts.resize(nParts);
		queryDamageValues = true;
	}

	// The global damage table
	SmartScriptTable pSinglePlayerTable;
	SmartScriptTable pDamageTable;
	if (queryDamageValues)
	{
		gEnv->pScriptSystem->GetGlobalValue("SinglePlayer", pSinglePlayerTable);
		if (pSinglePlayerTable.GetPtr())
		{
			if (GetType() == AIOBJECT_PLAYER)
				pSinglePlayerTable->GetValue("DamageAIToPlayer", pDamageTable);
			else
				pSinglePlayerTable->GetValue("DamageAIToAI", pDamageTable);
		}
		if (!pDamageTable.GetPtr())
			queryDamageValues = false;
	}

	pe_status_pos statusPos;
	pe_params_part paramsPart;
	for (statusPos.ipart = 0, paramsPart.ipart = 0; statusPos.ipart < nParts; ++statusPos.ipart, ++paramsPart.ipart)
	{
		if (!phys->GetParams(&paramsPart) || !phys->GetStatus(&statusPos))
			continue;

		primitives::box box;
		statusPos.pGeomProxy->GetBBox(&box);

		box.center *= statusPos.scale;
		box.size *= statusPos.scale;

		parts[statusPos.ipart].pos = statusPos.pos + statusPos.q * box.center;
		parts[statusPos.ipart].volume = (box.size.x * 2) * (box.size.y * 2) * (box.size.z * 2);

		if (queryDamageValues)
		{
			const char* matName = 0;
			phys_geometry* pGeom = paramsPart.pPhysGeomProxy ? paramsPart.pPhysGeomProxy : paramsPart.pPhysGeom;
			if (pGeom->surface_idx >= 0 && pGeom->surface_idx < paramsPart.nMats)
			{
				matName = pSurfaceMan->GetSurfaceType(pGeom->pMatMapping[pGeom->surface_idx])->GetName();
				parts[statusPos.ipart].surfaceIdx = pGeom->surface_idx;
			}
			else
			{
				parts[statusPos.ipart].surfaceIdx = -1;
			}
			float damage = 0.0f;
			// The this is a bit dodgy, but the keys in the damage table exclude the 'mat_' part of the material name.
			if (pDamageTable.GetPtr() && matName && strlen(matName) > 4)
				pDamageTable->GetValue(matName + 4, damage);
			parts[statusPos.ipart].damageMult = damage;
		}
	}
}

void CAIActor::OnAIHandlerSentSignal(const char* signalText, uint32 crc)
{
	IF_UNLIKELY (crc == 0)
	{
		crc = CCrc32::Compute(signalText);
	}
	else
	{
		assert(crc == CCrc32::Compute(signalText));
	}

	if (gAIEnv.CVars.LogSignals)
		gEnv->pLog->Log("OnAIHandlerSentSignal: '%s' [%s].", signalText, GetName());

	if (IsRunningBehaviorTree())
	{
		BehaviorTree::Event event(
		  crc
#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
		  , signalText
#endif
		  );

		GetAISystem()->GetIBehaviorTreeManager()->HandleEvent(GetEntityID(), event);
	}
}

// nSignalID = 10 allow duplicating signals
// nSignalID = 9 use the signal only to notify wait goal operations
//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::SetSignal(int nSignalID, const char* szText, IEntity* pSender, IAISignalExtraData* pData, uint32 crcCode)
{
	CCCPOINT(SetSignal);

	// Ensure we delete the pData object if we early out
	struct DeleteBeforeReturning
	{
		IAISignalExtraData** _p;
		DeleteBeforeReturning(IAISignalExtraData** p) : _p(p) {}
		~DeleteBeforeReturning()
		{
			if (*_p)
				GetAISystem()->FreeSignalExtraData((AISignalExtraData*)*_p);
		}
	} autoDelete(&pData);

#ifdef _DEBUG
	if (strlen(szText) + 1 > sizeof(((AISIGNAL*)0)->strText))
	{
		AIWarning("####>CAIObject::SetSignal SIGNAL STRING IS TOO LONG for <%s> :: %s  sz-> %" PRISIZE_T, GetName(), szText, strlen(szText));
		//		AILogEvent("####>CAIObject::SetSignal <%s> :: %s  sz-> %d",m_sName.c_str(),szText,strlen(szText));
	}
#endif // _DEBUG

	// always process signals sent only to notify wait goal operation
	if (crcCode == 0)
		crcCode = CCrc32::Compute(szText);

	// (MATT) This is the only place that the CRCs are used and their implementation is very clumsy {2008/08/09}
	if (nSignalID != AISIGNAL_NOTIFY_ONLY)
	{
#ifdef _DEBUG
		if (nSignalID != AISIGNAL_ALLOW_DUPLICATES)
		{
			DynArray<AISIGNAL>::iterator ai;
			for (ai = m_State.vSignals.begin(); ai != m_State.vSignals.end(); ++ai)
			{
				//	if ((*ai).strText == szText)
				//if (!stricmp((*ai).strText,szText))

				if (!stricmp((*ai).strText, szText) && !(*ai).Compare(crcCode))
				{
					AIWarning("Hash values are different, but strings are identical! %s - %s ", (*ai).strText, szText);
				}

				if (stricmp((*ai).strText, szText) && (*ai).Compare(crcCode))
				{
					AIWarning("Please report to alexey@crytek.de! Hash values are identical, but strings are different! %s - %s ", (*ai).strText, szText);
				}
			}
		}
#endif // _DEBUG

		if (!m_bEnabled && nSignalID != AISIGNAL_INCLUDE_DISABLED)
		{
			// (Kevin) This seems like an odd assumption to be making. INCLUDE_DISABLED needs to be a bit or a separate passed-in value.
			//	WarFace compatibility cannot have duplicate signals sent to disabled AI. (08/14/2009)
			if (gAIEnv.configuration.eCompatibilityMode == ECCM_WARFACE || nSignalID != AISIGNAL_ALLOW_DUPLICATES)
			{
				AILogComment("AIActor %p %s dropped signal \'%s\' due to being disabled", this, GetName(), szText);
				return;
			}
		}
	}

#ifdef CRYAISYSTEM_DEBUG
	if (nSignalID != AISIGNAL_RECEIVED_PREV_UPDATE)// received last update
	{
		IAIRecordable::RecorderEventData recorderEventData(szText);
		RecordEvent(IAIRecordable::E_SIGNALRECIEVED, &recorderEventData);
		GetAISystem()->Record(this, IAIRecordable::E_SIGNALRECIEVED, szText);
	}
#endif

	// don't let notify signals enter the queue
	if (nSignalID == AISIGNAL_NOTIFY_ONLY)
	{
		OnAIHandlerSentSignal(szText, crcCode); // still a polymorphic call that ends up in the most derived class (which is intended)
		return;
	}

	AISIGNAL signal;
	signal.nSignal = nSignalID;
	cry_strcpy(signal.strText, szText);
	signal.m_nCrcText = crcCode;
	signal.senderID = pSender ? pSender->GetId() : 0;
	signal.pEData = pData;

	// If all our early-outs passed and it wasn't just a "notify" then actually enter the signal into the stack!

	// cppcheck-suppress uselessAssignmentPtrArg
	pData = NULL; // set to NULL to prevent autodeletion of pData on return

	// need to make sure constructor signal is always at the back - to be processed first
	if (!m_State.vSignals.empty())
	{
		AISIGNAL backSignal(m_State.vSignals.back());

		if (!stricmp("Constructor", backSignal.strText))
		{
			m_State.vSignals.pop_back();
			m_State.vSignals.push_back(signal);
			m_State.vSignals.push_back(backSignal);
		}
		else
			m_State.vSignals.push_back(signal);
	}
	else
		m_State.vSignals.push_back(signal);
}

//====================================================================
// IsHostile
//====================================================================
bool CAIActor::IsHostile(const IAIObject* pOtherAI, bool bUsingAIIgnorePlayer) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	bool hostile = false;

	if (pOtherAI)
	{
		const CAIObject* other = static_cast<const CAIObject*>(pOtherAI);

		if (other->GetType() == AIOBJECT_ATTRIBUTE)
		{
			if (CAIObject* association = other->GetAssociation().GetAIObject())
				other = association;
		}

		if (bUsingAIIgnorePlayer && (other->GetType() == AIOBJECT_PLAYER) && (gAIEnv.CVars.IgnorePlayer != 0))
			return false;

		uint8 myFaction = GetFactionID();
		uint8 otherFaction = other->GetFactionID();

		if (!other->IsThreateningForHostileFactions())
			return false;

		hostile = (gAIEnv.pFactionMap->GetReaction(myFaction, otherFaction) == IFactionMap::Hostile);

		if (!hostile && !m_forcefullyHostiles.empty())
		{
			if (m_forcefullyHostiles.find(pOtherAI->GetAIObjectID()) != m_forcefullyHostiles.end())
				hostile = true;
		}

		if (hostile)
		{
			if (const CAIActor* actor = other->CastToCAIActor())
				if (bUsingAIIgnorePlayer && (m_Parameters.m_bAiIgnoreFgNode || actor->GetParameters().m_bAiIgnoreFgNode))
					hostile = false;
		}
	}

	return hostile;
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::Event(unsigned short eType, SAIEVENT* pEvent)
{
	CAISystem* pAISystem = GetAISystem();
	IAIActorProxy* pAIActorProxy = GetProxy();

	bool bWasEnabled = m_bEnabled;

	CAIObject::Event(eType, pEvent);

	// Inspired by CPuppet::Event's switch [4/25/2010 evgeny]
	switch (eType)
	{
	case AIEVENT_DISABLE:
		{
			// Reset and disable the agent's target track
			const tAIObjectID aiObjectId = GetAIObjectID();
			gAIEnv.pTargetTrackManager->ResetAgent(aiObjectId);
			gAIEnv.pTargetTrackManager->SetAgentEnabled(aiObjectId, false);

			pAISystem->UpdateGroupStatus(GetGroupId());
			pAISystem->NotifyEnableState(this, false);

			SetObserver(false);
		}
		break;
	case AIEVENT_ENABLE:
		if (pAIActorProxy->IsDead())
		{
			// can happen when rendering dead bodies? AI should not be enabled
			//				AIAssert(!"Trying to enable dead character!");
			return;
		}
		m_bEnabled = true;
		gAIEnv.pTargetTrackManager->SetAgentEnabled(GetAIObjectID(), true);
		pAISystem->UpdateGroupStatus(GetGroupId());
		pAISystem->NotifyEnableState(this, true);

		SetObserver(GetType() != AIOBJECT_PLAYER);
		SetObservable(true);
		break;
	case AIEVENT_SLEEP:
		m_bCheckedBody = false;
		if (pAIActorProxy->GetLinkedVehicleEntityId() == 0)
		{
			m_bEnabled = false;
			pAISystem->NotifyEnableState(this, m_bEnabled);
		}
		break;
	case AIEVENT_WAKEUP:
		m_bEnabled = true;
		pAISystem->NotifyEnableState(this, m_bEnabled);
		m_bCheckedBody = true;
		pAISystem->UpdateGroupStatus(GetGroupId());
		break;
	case AIEVENT_ONVISUALSTIMULUS:
		HandleVisualStimulus(pEvent);
		break;
	case AIEVENT_ONSOUNDEVENT:
		HandleSoundEvent(pEvent);
		break;
	case AIEVENT_ONBULLETRAIN:
		HandleBulletRain(pEvent);
		break;
	case AIEVENT_AGENTDIED:
		{
			ResetModularBehaviorTree(AIOBJRESET_SHUTDOWN);

			pAISystem->NotifyTargetDead(this);

			m_bCheckedBody = false;
			m_bEnabled = false;
			pAISystem->NotifyEnableState(this, m_bEnabled);

			pAISystem->RemoveFromGroup(GetGroupId(), this);

			gAIEnv.pFormationManager->ReleaseFormationPoint(this);
			CancelRequestedPath(false);
			ReleaseFormation();

			m_State.ClearSignals();

			const EntityId killerID = pEvent->targetId;
			pAISystem->OnAgentDeath(GetEntityID(), killerID);

			if (pAIActorProxy)
			{
				pAIActorProxy->Reset(AIOBJRESET_SHUTDOWN);
			}

			SetObservable(false);
			SetObserver(false);
		}
		break;
	}

	// Update the group status
	if (bWasEnabled != m_bEnabled)
	{
		GetAISystem()->UpdateGroupStatus(GetGroupId());
	}
}

void CAIActor::EntityEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_ATTACH_THIS:
	case ENTITY_EVENT_DETACH_THIS:
		QueryBodyInfo();
		break;

	case ENTITY_EVENT_ENABLE_PHYSICS:
		UpdateObserverSkipList();
		break;

	case ENTITY_EVENT_DONE:
		StopBehaviorTree();
		break;

	default:
		break;
	}

	CAIObject::EntityEvent(event);
}

//====================================================================
//
//====================================================================
bool CAIActor::CanAcquireTarget(IAIObject* pOther) const
{
	if (!pOther || !pOther->IsEnabled() || pOther->GetEntity()->IsHidden())
		return false;

	CCCPOINT(CAIActor_CanAcquireTarget);

	CAIObject* pOtherAI = (CAIObject*)pOther;
	if (pOtherAI->GetType() == AIOBJECT_ATTRIBUTE && pOtherAI->GetAssociation().IsValid())
		pOtherAI = (CAIObject*)pOtherAI->GetAssociation().GetAIObject();

	if (!pOtherAI || !pOtherAI->IsTargetable())
		return false;

	CAIActor* pOtherActor = pOtherAI->CastToCAIActor();
	if (!pOtherActor)
		return (pOtherAI->GetType() == AIOBJECT_TARGET);

	if (GetAISystem()->GetCombatClassScale(m_Parameters.m_CombatClass, pOtherActor->GetParameters().m_CombatClass) > 0)
		return true;
	return false;
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::SetGroupId(int id)
{
	if (id != GetGroupId())
	{
		gAIEnv.pGroupManager->RemoveGroupMember(GetGroupId(), GetAIObjectID());
		if (id > 0)
			gAIEnv.pGroupManager->AddGroupMember(id, GetAIObjectID());

		CAIObject::SetGroupId(id);
		GetAISystem()->AddToGroup(this);

		CAIObject* pBeacon = (CAIObject*)GetAISystem()->GetBeacon(id);
		if (pBeacon)
			GetAISystem()->UpdateBeacon(id, pBeacon->GetPos(), this);

		m_Parameters.m_nGroup = id;
	}
}

void CAIActor::SetFactionID(uint8 factionID)
{
	CAIObject::SetFactionID(factionID);

	if (IsObserver())
	{
		ObserverParams params;
		uint8 faction = GetFactionID();
		params.factionsToObserveMask = GetFactionVisionMask(faction);
		params.faction = faction;

		gAIEnv.pVisionMap->ObserverChanged(GetVisionID(), params, eChangedFaction | eChangedFactionsToObserveMask);
	}
}

void CAIActor::RegisterBehaviorListener(IActorBehaviorListener* listener)
{
	m_behaviorListeners.insert(listener);
}

void CAIActor::UnregisterBehaviorListener(IActorBehaviorListener* listener)
{
	m_behaviorListeners.erase(listener);
}

void CAIActor::BehaviorEvent(EBehaviorEvent event)
{
	BehaviorListeners::iterator it = m_behaviorListeners.begin();
	BehaviorListeners::iterator end = m_behaviorListeners.end();

	for (; it != end; )
	{
		BehaviorListeners::iterator next = it;
		++next;

		IActorBehaviorListener* listener = *it;
		listener->BehaviorEvent(this, event);

		it = next;
	}
}

void CAIActor::BehaviorChanged(const char* current, const char* previous)
{
	BehaviorListeners::iterator it = m_behaviorListeners.begin();
	BehaviorListeners::iterator end = m_behaviorListeners.end();

	for (; it != end; )
	{
		BehaviorListeners::iterator next = it;
		++next;

		IActorBehaviorListener* listener = *it;
		listener->BehaviorChanged(this, current, previous);

		it = next;
	}
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::SetParameters(const AgentParameters& sParams)
{
	SetGroupId(sParams.m_nGroup);
	SetFactionID(sParams.factionID);

	m_Parameters = sParams;
	m_Parameters.m_fAccuracy = clamp_tpl(m_Parameters.m_fAccuracy, 0.0f, 1.0f);

	GetAISystem()->AddToFaction(this, sParams.factionID);

	CacheFOVCos(sParams.m_PerceptionParams.FOVPrimary, sParams.m_PerceptionParams.FOVSecondary);
	float range = std::max<float>(sParams.m_PerceptionParams.sightRange,
	                              sParams.m_PerceptionParams.sightRangeVehicle);

	VisionChanged(range, m_FOVPrimaryCos, m_FOVSecondaryCos);
}

#ifdef CRYAISYSTEM_DEBUG
void CAIActor::UpdateHealthHistory()
{
	if (!GetProxy()) return;
	if (!m_healthHistory)
		m_healthHistory = new CValueHistory<float>(100, 0.1f);
	//better add float functions here
	float health = (GetProxy()->GetActorHealth() + GetProxy()->GetActorArmor());
	float maxHealth = (float)GetProxy()->GetActorMaxHealth();

	m_healthHistory->Sample(health / maxHealth, GetAISystem()->GetFrameDeltaTime());
}
#endif

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::Serialize(TSerialize ser)
{
	ser.Value("m_bCheckedBody", m_bCheckedBody);

	m_State.Serialize(ser);
	m_Parameters.Serialize(ser);
	SerializeMovementAbility(ser);

	CAIObject::Serialize(ser);

	if (ser.IsReading())
	{
		SetParameters(m_Parameters);
	}

	if (ser.IsReading())
	{
		const EntityId entityId = GetEntityID();
		assert(entityId > 0);

		// The proxy may already exist, if this actor is being created due to an entity
		//	being prepared from the pool.
		if (!m_proxy.get())
		{
			AILogComment("CAIActor(%p) Creating AIActorProxy for serialization.", this);
			m_proxy.reset(GetAISystem()->GetActorProxyFactory()->CreateActorProxy(entityId));
		}

		gAIEnv.pActorLookUp->UpdateProxy(this);
	}

	assert(m_proxy != 0);
	if (m_proxy)
		m_proxy->Serialize(ser);
	else
		AIWarning("CAIActor::Serialize Missing proxy for \'%s\' after loading", GetName());

	if (ser.IsReading())
	{
		ResetModularBehaviorTree(AIOBJRESET_INIT);
	}

	bool observer = m_observer;
	ser.Value("m_observer", observer);

	if (ser.IsReading())
	{
		SetObserver(observer);

		// only alive puppets or leaders should be added to groups
		bool addToGroup = GetProxy() != NULL ? !GetProxy()->IsDead() : CastToCLeader() != NULL;
		if (addToGroup)
			GetAISystem()->AddToGroup(this);

		ReactionChanged(0, IFactionMap::Hostile);

		m_usingCombatLight = false;
		m_lightLevel = AILL_LIGHT;
	}

	ser.Value("m_cachedWaterOcclusionValue", m_cachedWaterOcclusionValue);

	ser.Value("m_vLastFullUpdatePos", m_vLastFullUpdatePos);
	uint32 lastFullUpdateStance = m_lastFullUpdateStance;
	ser.Value("m_lastFullUpdateStance", lastFullUpdateStance);
	if (ser.IsReading())
	{
		m_lastFullUpdateStance = static_cast<EStance>(lastFullUpdateStance);
	}

	if (ser.IsReading())
	{
		SetAttentionTarget(NILREF);
	}

	// Territory
	ser.Value("m_territoryShapeName", m_territoryShapeName);
	if (ser.IsReading())
		m_territoryShape = GetAISystem()->GetGenericShapeOfName(m_territoryShapeName.c_str());

	ser.Value("m_forcefullyHostiles", m_forcefullyHostiles);

	ser.Value("m_activeCoordinationCount", m_activeCoordinationCount);

	uint32 navigationTypeId = m_navigationTypeID;
	ser.Value("m_navigationTypeID", navigationTypeId);
	if (ser.IsReading())
	{
		m_navigationTypeID = NavigationAgentTypeID(navigationTypeId);
	}

	ser.Value("m_initialPosition.isValid", m_initialPosition.isValid);
	ser.Value("m_initialPosition.pos", m_initialPosition.pos);

	m_collisionAvoidanceAgent.Serialize(ser);
}

void CAIActor::SetAttentionTarget(CWeakRef<CAIObject> refTarget)
{
	CCCPOINT(CAIActor_SetAttentionTarget);
	CAIObject* pAttTarget = refTarget.GetAIObject();

	m_refAttentionTarget = refTarget;

#ifdef CRYAISYSTEM_DEBUG
	RecorderEventData recorderEventData(pAttTarget ? pAttTarget->GetName() : "<none>");
	RecordEvent(IAIRecordable::E_ATTENTIONTARGET, &recorderEventData);
#endif
}

Vec3 CAIActor::GetFloorPosition(const Vec3& pos)
{
	Vec3 floorPos = pos;
	return (GetFloorPos(floorPos, pos, WalkabilityFloorUpDist, WalkabilityFloorDownDist, WalkabilityDownRadius, AICE_STATIC))
	       ? floorPos : pos;
}

CWeakRef<CAIActor> CAIActor::GetLiveTarget(const CWeakRef<CAIObject>& refTarget)
{
	CCCPOINT(CPuppet_GetLiveTarget);

	CWeakRef<CAIActor> refResult;
	CAIObject* pTarget = refTarget.GetAIObject();
	if (pTarget)
	{
		CAIActor* pAIActor = pTarget->CastToCAIActor();
		if (pAIActor && pAIActor->IsActive() && pAIActor->IsAgent())
		{
			refResult = StaticCast<CAIActor>(refTarget);
		}
		else
		{
			CAIActor* pAssociatedAIActor = CastToCAIActorSafe(pTarget->GetAssociation().GetAIObject());
			if (pAssociatedAIActor && pAssociatedAIActor->IsEnabled() && pAssociatedAIActor->IsAgent())
			{
				refResult = GetWeakRef(pAssociatedAIActor);
			}
		}
	}
	return refResult;
}

const CAIObject* CAIActor::GetLiveTarget(const CAIObject* pTarget)
{
	if (!pTarget)
		return 0;
	if (const CAIActor* pTargetAIActor = pTarget->CastToCAIActor())
		if (!pTargetAIActor->IsActive())
			return 0;
	if (pTarget->IsAgent())
		return pTarget;
	const CAIObject* pAssociation = pTarget->GetAssociation().GetAIObject();
	return (pAssociation && pAssociation->IsEnabled() && pAssociation->IsAgent()) ? pAssociation : 0;
}

uint32 CAIActor::GetFactionVisionMask(uint8 factionID) const
{
	uint32 mask = 0;
	uint32 factionCount = gAIEnv.pFactionMap->GetFactionCount();

	for (uint32 i = 0; i < factionCount; ++i)
	{
		if (i != factionID)
		{
			if (gAIEnv.pFactionMap->GetReaction(factionID, i) < IFactionMap::Neutral)
				mask |= 1 << i;
		}
	}

	PersonallyHostiles::const_iterator it = m_forcefullyHostiles.begin();
	PersonallyHostiles::const_iterator end = m_forcefullyHostiles.end();

	for (; it != end; ++it)
	{
		if (const CAIObject* aiObject = static_cast<const CAIObject*>(GetAISystem()->GetAIObjectManager()->GetAIObject(*it)))
			if (aiObject->GetFactionID() != IFactionMap::InvalidFactionID)
				mask |= 1 << aiObject->GetFactionID();
	}

	return mask;
}

void CAIActor::SerializeMovementAbility(TSerialize ser)
{
	ser.BeginGroup("AgentMovementAbility");
	AgentMovementAbility& moveAbil = m_movementAbility;

	ser.Value("b3DMove", moveAbil.b3DMove);
	ser.Value("bUsePathfinder", moveAbil.bUsePathfinder);
	ser.Value("usePredictiveFollowing", moveAbil.usePredictiveFollowing);
	ser.Value("allowEntityClampingByAnimation", moveAbil.allowEntityClampingByAnimation);
	ser.Value("maxAccel", moveAbil.maxAccel);
	ser.Value("maxDecel", moveAbil.maxDecel);
	ser.Value("minTurnRadius", moveAbil.minTurnRadius);
	ser.Value("maxTurnRadius", moveAbil.maxTurnRadius);
	ser.Value("avoidanceRadius", moveAbil.avoidanceRadius);
	ser.Value("pathLookAhead", moveAbil.pathLookAhead);
	ser.Value("pathRadius", moveAbil.pathRadius);
	ser.Value("pathSpeedLookAheadPerSpeed", moveAbil.pathSpeedLookAheadPerSpeed);
	ser.Value("cornerSlowDown", moveAbil.cornerSlowDown);
	ser.Value("slopeSlowDown", moveAbil.slopeSlowDown);
	ser.Value("optimalFlightHeight", moveAbil.optimalFlightHeight);
	ser.Value("minFlightHeight", moveAbil.minFlightHeight);
	ser.Value("maxFlightHeight", moveAbil.maxFlightHeight);
	ser.Value("maneuverTrh", moveAbil.maneuverTrh);
	ser.Value("velDecay", moveAbil.velDecay);
	ser.Value("pathFindPrediction", moveAbil.pathFindPrediction);
	ser.Value("pathRegenIntervalDuringTrace", moveAbil.pathRegenIntervalDuringTrace);
	ser.Value("teleportEnabled", moveAbil.teleportEnabled);
	ser.Value("lightAffectsSpeed", moveAbil.lightAffectsSpeed);
	ser.Value("resolveStickingInTrace", moveAbil.resolveStickingInTrace);
	ser.Value("directionalScaleRefSpeedMin", moveAbil.directionalScaleRefSpeedMin);
	ser.Value("directionalScaleRefSpeedMax", moveAbil.directionalScaleRefSpeedMax);
	ser.Value("avoidanceAbilities", moveAbil.avoidanceAbilities);
	ser.Value("pushableObstacleWeakAvoidance", moveAbil.pushableObstacleWeakAvoidance);
	ser.Value("pushableObstacleAvoidanceRadius", moveAbil.pushableObstacleAvoidanceRadius);
	ser.Value("pushableObstacleMassMin", moveAbil.pushableObstacleMassMin);
	ser.Value("pushableObstacleMassMax", moveAbil.pushableObstacleMassMax);

	ser.BeginGroup("AgentMovementSpeeds");

	AgentMovementSpeeds& moveSpeeds = m_movementAbility.movementSpeeds;
	for (int i = 0; i < AgentMovementSpeeds::AMU_NUM_VALUES; i++)
		for (int j = 0; j < AgentMovementSpeeds::AMS_NUM_VALUES; j++)
		{
			ser.BeginGroup("range");
			AgentMovementSpeeds::SSpeedRange& range = moveSpeeds.GetRange(j, i);
			ser.Value("def", range.def);
			ser.Value("min", range.min);
			ser.Value("max", range.max);
			ser.EndGroup();
		}

	ser.EndGroup();

	ser.BeginGroup("AgentPathfindingProperties");
	AgentPathfindingProperties& pfProp = m_movementAbility.pathfindingProperties;

	pfProp.navCapMask.Serialize(ser);
	ser.Value("triangularResistanceFactor", pfProp.triangularResistanceFactor);
	ser.Value("waypointResistanceFactor", pfProp.waypointResistanceFactor);
	ser.Value("flightResistanceFactor", pfProp.flightResistanceFactor);
	ser.Value("volumeResistanceFactor", pfProp.volumeResistanceFactor);
	ser.Value("roadResistanceFactor", pfProp.roadResistanceFactor);

	ser.Value("waterResistanceFactor", pfProp.waterResistanceFactor);
	ser.Value("maxWaterDepth", pfProp.maxWaterDepth);
	ser.Value("minWaterDepth", pfProp.minWaterDepth);
	ser.Value("exposureFactor", pfProp.exposureFactor);
	ser.Value("dangerCost", pfProp.dangerCost);
	ser.Value("zScale", pfProp.zScale);

	ser.EndGroup();

	ser.EndGroup();
}

//
//------------------------------------------------------------------------------------------------------------------------
float CAIActor::AdjustTargetVisibleRange(const CAIActor& observer, float fVisibleRange) const
{
	return fVisibleRange;
}

//
//------------------------------------------------------------------------------------------------------------------------
float CAIActor::GetMaxTargetVisibleRange(const IAIObject* pTarget, bool bCheckCloak) const
{
	float fRange = 0.0f;

	const AgentParameters& parameters = GetParameters();
	const AgentPerceptionParameters& perception = parameters.m_PerceptionParams;

	IAIActor::CloakObservability cloakObservability;
	cloakObservability.cloakMaxDistCrouchedAndMoving = perception.cloakMaxDistCrouchedAndMoving;
	cloakObservability.cloakMaxDistCrouchedAndStill = perception.cloakMaxDistCrouchedAndStill;
	cloakObservability.cloakMaxDistMoving = perception.cloakMaxDistMoving;
	cloakObservability.cloakMaxDistStill = perception.cloakMaxDistStill;

	// Check if I'm invisible from the observer's position
	const CAIActor* pTargetActor = CastToCAIActorSafe(pTarget);
	if (!pTargetActor || !pTargetActor->IsInvisibleFrom(GetPos(), bCheckCloak, true, cloakObservability))
	{
		fRange = perception.sightRange;

		if (pTarget)
		{
			// Use correct range for vehicles
			if (pTarget->GetAIType() == AIOBJECT_VEHICLE && parameters.m_PerceptionParams.sightRangeVehicle > FLT_EPSILON)
			{
				fRange = parameters.m_PerceptionParams.sightRangeVehicle;
			}

			// Allow target to adjust the visible range as needed. This allows effects like
			//	underwater occlusion or light levels to take effect.
			if (pTargetActor && fRange > FLT_EPSILON)
			{
				fRange = pTargetActor->AdjustTargetVisibleRange(*this, fRange);
			}
		}
	}

	return fRange;
}

//
//------------------------------------------------------------------------------------------------------------------------
bool CAIActor::IsCloakEffective(const Vec3& pos) const
{
	return (m_Parameters.m_fCloakScaleTarget > 0.f &&
	        !IsUsingCombatLight() &&
	        (!GetGrabbedEntity() || !IsGrabbedEntityInView(pos)));
}

//
//------------------------------------------------------------------------------------------------------------------------
bool CAIActor::IsInvisibleFrom(const Vec3& pos, bool bCheckCloak, bool bCheckCloakDistance, const IAIActor::CloakObservability& cloakObservability) const
{
	bool bInvisible = (m_Parameters.m_bInvisible);

	if (!bInvisible && bCheckCloak)
	{
		bInvisible = (m_Parameters.m_bCloaked && IsCloakEffective(pos));

		// Cloaked targets can still be seen if they are close enough to the point
		if (bInvisible && bCheckCloakDistance)
		{
			float cloakMaxDist = 0.0f;

			if (m_bodyInfo.stance == STANCE_CROUCH)
			{
				cloakMaxDist = m_bodyInfo.isMoving ? cloakObservability.cloakMaxDistCrouchedAndMoving : cloakObservability.cloakMaxDistCrouchedAndStill;
			}
			else
			{
				cloakMaxDist = m_bodyInfo.isMoving ? cloakObservability.cloakMaxDistMoving : cloakObservability.cloakMaxDistStill;
			}

			bInvisible = Distance::Point_PointSq(GetPos(), pos) > sqr(cloakMaxDist);
		}
	}

	return bInvisible;
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::NotifyDeath()
{
}

//
//------------------------------------------------------------------------------------------------------------------------
static void CheckAndAddPhysEntity(PhysSkipList& skipList, IPhysicalEntity* physics)
{
	if (physics)
	{
		pe_status_pos stat;

		if ((physics->GetStatus(&stat) != 0) && (((1 << stat.iSimClass) & COVER_OBJECT_TYPES) != 0))
		{
			stl::push_back_unique(skipList, physics);
		}
	}
}

//
//------------------------------------------------------------------------------------------------------------------------
void CAIActor::GetPhysicalSkipEntities(PhysSkipList& skipList) const
{
	CAIObject::GetPhysicalSkipEntities(skipList);

	const SAIBodyInfo& bi = GetBodyInfo();
	if (IEntity* pLinkedVehicleEntity = bi.GetLinkedVehicleEntity())
	{
		CheckAndAddPhysEntity(skipList, pLinkedVehicleEntity->GetPhysics());
	}

	// if holding something in hands - skip it for the vis check
	IEntity* pGrabbedEntity(GetGrabbedEntity());
	if (pGrabbedEntity)
		CheckAndAddPhysEntity(skipList, pGrabbedEntity->GetPhysics());

	// (Kevin) Adding children in most cases causes us to add too many entities to the skip list (i.e., vehicles)
	// If this is needed, investigate how we can keep this under 5 skippable entities.
	/*if (IEntity* entity = GetEntity())
	   {
	   for (int i = 0, ni = entity->GetChildCount(); i < ni; ++i)
	   {
	    if (IEntity* child = entity->GetChild(i))
	      CheckAndAddPhysEntity(skipList, child->GetPhysics());
	   }
	   }*/

	CRY_ASSERT_MESSAGE(skipList.size() <= 5, "Too many physical skipped entities determined. See SRwiRequest definition.");
}

void CAIActor::UpdateObserverSkipList()
{
	if (m_observer)
	{
		PhysSkipList skipList;
		GetPhysicalSkipEntities(skipList);

		assert(skipList.size() <= ObserverParams::MaxSkipListSize);

		ObserverParams observerParams;
		observerParams.skipListSize = std::min<size_t>(skipList.size(), ObserverParams::MaxSkipListSize);
		for (size_t i = 0; i < static_cast<size_t>(observerParams.skipListSize); ++i)
			observerParams.skipList[i] = skipList[i];

		gAIEnv.pVisionMap->ObserverChanged(GetVisionID(), observerParams, eChangedSkipList);
	}
}

void CAIActor::GetLocalBounds(AABB& bbox) const
{
	bbox.min.zero();
	bbox.max.zero();

	IEntity* pEntity = GetEntity();
	IPhysicalEntity* pPhysicalEntity = pEntity->GetPhysics();
	if (pPhysicalEntity)
	{
		pe_status_pos pstate;
		if (pPhysicalEntity && pPhysicalEntity->GetStatus(&pstate))
		{
			bbox.min = pstate.BBox[0] / pstate.scale;
			bbox.max = pstate.BBox[1] / pstate.scale;
		}
	}
	else
	{
		return pEntity->GetLocalBounds(bbox);
	}
}

IEntity* CAIActor::GetPathAgentEntity() const
{
	return GetEntity();
}

const char* CAIActor::GetPathAgentName() const
{
	return GetName();
}

unsigned short CAIActor::GetPathAgentType() const
{
	return GetType();
}

float CAIActor::GetPathAgentPassRadius() const
{
	return GetParameters().m_fPassRadius;
}

Vec3 CAIActor::GetPathAgentPos() const
{
	return GetPhysicsPos();
}

Vec3 CAIActor::GetPathAgentVelocity() const
{
	return GetVelocity();
}

const AgentMovementAbility& CAIActor::GetPathAgentMovementAbility() const
{
	return m_movementAbility;
}

void CAIActor::SetPathToFollow(const char* pathName)
{

}

void CAIActor::SetPathAttributeToFollow(bool bSpline)
{

}

IPathFollower* CAIActor::GetPathFollower() const
{
	return NULL;
}

bool CAIActor::GetValidPositionNearby(const Vec3& proposedPosition, Vec3& adjustedPosition) const
{
	return false;
}

bool CAIActor::GetTeleportPosition(Vec3& teleportPos) const
{
	return false;
}

//===================================================================
// GetSightFOVCos
//===================================================================
void CAIActor::GetSightFOVCos(float& fPrimaryFOVCos, float& fSecondaryFOVCos) const
{
	fPrimaryFOVCos = m_FOVPrimaryCos;
	fSecondaryFOVCos = m_FOVSecondaryCos;
}

//===================================================================
// TransformFOV
//===================================================================
void CAIActor::CacheFOVCos(float FOVPrimary, float FOVSecondary)
{
	if (FOVPrimary < 0.0f || FOVPrimary > 360.0f)   // see all around
	{
		m_FOVPrimaryCos = -1.0f;
		m_FOVSecondaryCos = -1.0f;
	}
	else
	{
		if (FOVSecondary >= 0.0f && FOVPrimary > FOVSecondary)
			FOVSecondary = FOVPrimary;

		m_FOVPrimaryCos = cosf(DEG2RAD(FOVPrimary * 0.5f));

		if (FOVSecondary < 0.0f || FOVSecondary > 360.0f)   // see all around
			m_FOVSecondaryCos = -1.0f;
		else
			m_FOVSecondaryCos = cosf(DEG2RAD(FOVSecondary * 0.5f));
	}
}

void CAIActor::ReactionChanged(uint8 factionID, IFactionMap::ReactionType reaction)
{
	if (m_observer)
	{
		ObserverParams params;
		uint8 faction = GetFactionID();
		params.factionsToObserveMask = GetFactionVisionMask(faction);
		params.faction = faction;

		gAIEnv.pVisionMap->ObserverChanged(GetVisionID(), params, eChangedFaction | eChangedFactionsToObserveMask);
	}
}

void CAIActor::VisionChanged(float sightRange, float primaryFOVCos, float secondaryFOVCos)
{
	if (m_observer)
	{
		ObserverParams observerParams;
		observerParams.sightRange = sightRange;
		observerParams.fovCos = primaryFOVCos;

		gAIEnv.pVisionMap->ObserverChanged(GetVisionID(), observerParams, eChangedSightRange | eChangedFOV);
	}
}

void CAIActor::SetObserver(bool observer)
{
	if (m_observer != observer)
	{
		if (observer)
		{
			uint8 faction = GetFactionID();
			ObserverParams observerParams;
			observerParams.entityId = GetEntityID();
			observerParams.factionsToObserveMask = GetFactionVisionMask(faction);
			observerParams.faction = faction;
			observerParams.typesToObserveMask = GetObserverTypeMask();
			observerParams.typeMask = GetObservableTypeMask();
			observerParams.eyePosition = GetPos();
			observerParams.eyeDirection = GetViewDir();
			observerParams.fovCos = m_FOVPrimaryCos;
			observerParams.sightRange = m_Parameters.m_PerceptionParams.sightRange;

			PhysSkipList skipList;
			GetPhysicalSkipEntities(skipList);

			observerParams.skipListSize = std::min<size_t>(skipList.size(), ObserverParams::MaxSkipListSize);
			for (size_t i = 0; i < static_cast<size_t>(observerParams.skipListSize); ++i)
				observerParams.skipList[i] = skipList[i];

			VisionID visionID = GetVisionID();
			if (!visionID)
			{
				visionID = gAIEnv.pVisionMap->CreateVisionID(GetName());

				SetVisionID(visionID);
			}

			gAIEnv.pVisionMap->RegisterObserver(visionID, observerParams);
		}
		else
		{
			if (VisionID visionID = GetVisionID())
				gAIEnv.pVisionMap->UnregisterObserver(visionID);
		}

		m_observer = observer;
	}
}

uint32 CAIActor::GetObserverTypeMask() const
{
	return General | AliveAgent | DeadAgent | Player;
}

uint32 CAIActor::GetObservableTypeMask() const
{
	return General | AliveAgent;
}

bool CAIActor::IsObserver() const
{
	return m_observer;
}

bool CAIActor::CanSee(const VisionID& otherID) const
{
	return gEnv->pAISystem->GetVisionMap()->IsVisible(GetVisionID(), otherID);
}

void CAIActor::AddPersonallyHostile(tAIObjectID hostileID)
{
	m_forcefullyHostiles.insert(hostileID);

	ReactionChanged(0, IFactionMap::Hostile);
}

void CAIActor::RemovePersonallyHostile(tAIObjectID hostileID)
{
	m_forcefullyHostiles.erase(hostileID);

	ReactionChanged(0, IFactionMap::Hostile);
}

void CAIActor::ResetPersonallyHostiles()
{
	m_forcefullyHostiles.clear();

	ReactionChanged(0, IFactionMap::Hostile);
}

bool CAIActor::IsPersonallyHostile(tAIObjectID hostileID) const
{
	return m_forcefullyHostiles.find(hostileID) != m_forcefullyHostiles.end();
}

void CAIActor::SetProxy(IAIActorProxy* proxy)
{
	m_proxy.reset(proxy);
}

IAIActorProxy* CAIActor::GetProxy() const
{
	return m_proxy;
}

IAIObject::EFieldOfViewResult CAIActor::CheckPointInFOV(const Vec3& point, float sightRange) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const Vec3& eyePosition = GetPos();
	const Vec3 eyeToPointDisplacement = point - eyePosition;
	const float squaredEyeToPointDistance = eyeToPointDisplacement.GetLengthSquared();

	const bool pointIsAtEyePosition = (squaredEyeToPointDistance <= std::numeric_limits<float>::epsilon());
	IF_UNLIKELY (pointIsAtEyePosition)
		return eFOV_Outside;

	if (squaredEyeToPointDistance <= sqr(sightRange))
	{
		float primaryFovCos = 1.0f;
		float secondaryFovCos = 1.0f;
		GetSightFOVCos(primaryFovCos, secondaryFovCos);
		assert(primaryFovCos >= secondaryFovCos);

		const Vec3 eyeToPointDirection = eyeToPointDisplacement.GetNormalized();
		const Vec3& eyeDirection = GetViewDir();

		const float dotProduct = eyeDirection.Dot(eyeToPointDirection);
		if (dotProduct >= secondaryFovCos)
		{
			if (dotProduct >= primaryFovCos)
				return eFOV_Primary;
			else
				return eFOV_Secondary;
		}
	}

	return eFOV_Outside;
}

void CAIActor::HandlePathDecision(MNMPathRequestResult& result)
{
}

void CAIActor::HandleVisualStimulus(SAIEVENT* pAIEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const float fGlobalVisualPerceptionScale = gEnv->pAISystem->GetGlobalVisualScale(this);
	const float fVisualPerceptionScale = m_Parameters.m_PerceptionParams.perceptionScale.visual * fGlobalVisualPerceptionScale;
	if (gAIEnv.CVars.IgnoreVisualStimulus != 0 || m_Parameters.m_bAiIgnoreFgNode || fVisualPerceptionScale <= 0.0f)
		return;

	if (gAIEnv.pTargetTrackManager->IsEnabled())
	{
		// Check if in range (using perception scale)
		if (eFOV_Outside != IsPointInFOV(pAIEvent->vPosition, fVisualPerceptionScale))
		{
			gAIEnv.pTargetTrackManager->HandleStimulusFromAIEvent(GetAIObjectID(), pAIEvent, TargetTrackHelpers::eEST_Visual);

			IEntity* pEventOwnerEntity = gEnv->pEntitySystem->GetEntity(pAIEvent->sourceId);
			if (!pEventOwnerEntity)
				return;

			IAIObject* pEventOwnerAI = pEventOwnerEntity->GetAI();
			if (!pEventOwnerAI)
				return;

			if (IsHostile(pEventOwnerAI))
			{
				m_State.nTargetType = static_cast<CAIObject*>(pEventOwnerAI)->GetType();
				m_stimulusStartTime = GetAISystem()->GetFrameStartTimeSeconds();

				m_State.eTargetThreat = AITHREAT_AGGRESSIVE;
				m_State.eTargetType = AITARGET_VISUAL;

				CWeakRef<CAIObject> refAttentionTarget = GetWeakRef(static_cast<CAIObject*>(pEventOwnerAI));
				if (refAttentionTarget != m_refAttentionTarget)
				{
					SetAttentionTarget(refAttentionTarget);
				}
			}
		}
	}
}

void CAIActor::HandleSoundEvent(SAIEVENT* pAIEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const float fGlobalAudioPerceptionScale = gEnv->pAISystem->GetGlobalAudioScale(this);
	const float fAudioPerceptionScale = m_Parameters.m_PerceptionParams.perceptionScale.audio * fGlobalAudioPerceptionScale;
	if (gAIEnv.CVars.IgnoreSoundStimulus != 0 || m_Parameters.m_bAiIgnoreFgNode || fAudioPerceptionScale <= 0.0f)
		return;

	if (gAIEnv.pTargetTrackManager->IsEnabled())
	{
		// Check if in range (using perception scale)
		const Vec3& vMyPos = GetPos();
		const float fSoundDistance = vMyPos.GetDistance(pAIEvent->vPosition) * (1.0f / fAudioPerceptionScale);
		if (fSoundDistance <= pAIEvent->fThreat)
		{
			gAIEnv.pTargetTrackManager->HandleStimulusFromAIEvent(GetAIObjectID(), pAIEvent, TargetTrackHelpers::eEST_Sound);

			IEntity* pEventOwnerEntity = gEnv->pEntitySystem->GetEntity(pAIEvent->sourceId);
			if (!pEventOwnerEntity)
				return;

			IAIObject* pEventOwnerAI = pEventOwnerEntity->GetAI();
			if (!pEventOwnerAI)
				return;

			if (IsHostile(pEventOwnerAI))
			{
				if ((m_State.eTargetType != AITARGET_MEMORY) && (m_State.eTargetType != AITARGET_VISUAL))
				{
					m_State.nTargetType = static_cast<CAIObject*>(pEventOwnerAI)->GetType();
					m_stimulusStartTime = GetAISystem()->GetFrameStartTimeSeconds();

					m_State.nTargetType = static_cast<CAIObject*>(pEventOwnerAI)->GetType();
					m_State.eTargetThreat = AITHREAT_AGGRESSIVE;
					m_State.eTargetType = AITARGET_SOUND;

					SetAttentionTarget(GetWeakRef(static_cast<CAIObject*>(pEventOwnerAI)));
				}
			}
		}
	}
}

void CAIActor::HandleBulletRain(SAIEVENT* pAIEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (gAIEnv.CVars.IgnoreBulletRainStimulus || m_Parameters.m_bAiIgnoreFgNode)
		return;

	IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
	pData->point = pAIEvent->vPosition;
	pData->point2 = pAIEvent->vStimPos;
	pData->nID = pAIEvent->sourceId;
	pData->fValue = pAIEvent->fThreat; // pressureMultiplier

	SetSignal(0, "OnBulletRain", GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnBulletRain);

	if (gAIEnv.pTargetTrackManager->IsEnabled())
		gAIEnv.pTargetTrackManager->HandleStimulusFromAIEvent(GetAIObjectID(), pAIEvent, TargetTrackHelpers::eEST_BulletRain);
}

void CAIActor::CancelRequestedPath(bool actorRemoved)
{

}

IAIObject::EFieldOfViewResult CAIActor::IsPointInFOV(const Vec3& vPos, float fDistanceScale) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const float fSightRange = m_Parameters.m_PerceptionParams.sightRange * fDistanceScale;
	return CheckPointInFOV(vPos, fSightRange);
}

IAIObject::EFieldOfViewResult CAIActor::IsObjectInFOV(const IAIObject* pTarget, float fDistanceScale) const
{
	CCCPOINT(CAIActor_IsObjectInFOVCone);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const Vec3& vTargetPos = pTarget->GetPos();
	const float fSightRange = GetMaxTargetVisibleRange(pTarget) * fDistanceScale;
	return (fSightRange > 0.0f ? CheckPointInFOV(vTargetPos, fSightRange) : eFOV_Outside);
}

void CAIActor::GetMovementSpeedRange(float fUrgency, bool bSlowForStrafe, float& normalSpeed, float& minSpeed, float& maxSpeed) const
{
	AgentMovementSpeeds::EAgentMovementUrgency urgency;
	AgentMovementSpeeds::EAgentMovementStance stance;

	bool vehicle = GetType() == AIOBJECT_VEHICLE;

	if (fUrgency < 0.5f * (AISPEED_SLOW + AISPEED_WALK))
		urgency = AgentMovementSpeeds::AMU_SLOW;
	else if (fUrgency < 0.5f * (AISPEED_WALK + AISPEED_RUN))
		urgency = AgentMovementSpeeds::AMU_WALK;
	else if (fUrgency < 0.5f * (AISPEED_RUN + AISPEED_SPRINT))
		urgency = AgentMovementSpeeds::AMU_RUN;
	else
		urgency = AgentMovementSpeeds::AMU_SPRINT;

	if (IsAffectedByLight() && m_movementAbility.lightAffectsSpeed)
	{
		// Disable sprinting in dark light conditions.
		if (urgency == AgentMovementSpeeds::AMU_SPRINT)
		{
			EAILightLevel eAILightLevel = GetLightLevel();
			if ((eAILightLevel == AILL_DARK) || (eAILightLevel == AILL_SUPERDARK))
				urgency = AgentMovementSpeeds::AMU_RUN;
		}
	}

	const SAIBodyInfo& bodyInfo = GetBodyInfo();

	switch (bodyInfo.stance)
	{
	case STANCE_STEALTH:
		stance = AgentMovementSpeeds::AMS_STEALTH;
		break;
	case STANCE_CROUCH:
		stance = AgentMovementSpeeds::AMS_CROUCH;
		break;
	case STANCE_PRONE:
		stance = AgentMovementSpeeds::AMS_PRONE;
		break;
	case STANCE_SWIM:
		stance = AgentMovementSpeeds::AMS_SWIM;
		break;
	case STANCE_RELAXED:
		stance = AgentMovementSpeeds::AMS_RELAXED;
		break;
	case STANCE_ALERTED:
		stance = AgentMovementSpeeds::AMS_ALERTED;
		break;
	case STANCE_LOW_COVER:
		stance = AgentMovementSpeeds::AMS_LOW_COVER;
		break;
	case STANCE_HIGH_COVER:
		stance = AgentMovementSpeeds::AMS_HIGH_COVER;
		break;
	default:
		stance = AgentMovementSpeeds::AMS_COMBAT;
		break;
	}

	const float artificialMinSpeedMult = 1.0f;

	AgentMovementSpeeds::SSpeedRange fwdRange = m_movementAbility.movementSpeeds.GetRange(stance, urgency);
	fwdRange.min *= artificialMinSpeedMult;
	normalSpeed = fwdRange.def;
	minSpeed = fwdRange.min;
	maxSpeed = fwdRange.max;

	if (m_movementAbility.directionalScaleRefSpeedMin > 0.0f)
	{
		float desiredSpeed = normalSpeed;
		float desiredTurnSpeed = m_bodyTurningSpeed;
		float travelAngle = Ang3::CreateRadZ(GetEntityDir(), GetMoveDir()); //m_State.vMoveDir);

		const float refSpeedMin = m_movementAbility.directionalScaleRefSpeedMin;
		const float refSpeedMax = m_movementAbility.directionalScaleRefSpeedMax;

		// When a locomotion is slow (0.5-2.0f), then we can do this motion in all direction more or less at the same speed
		float t = sqr(clamp_tpl((desiredSpeed - refSpeedMin) / (refSpeedMax - refSpeedMin), 0.0f, 1.0f));
		float scaleLimit = clamp_tpl(0.8f * (1 - t) + 0.1f * t, 0.3f, 1.0f); //never scale more then 0.4 down

		float turnSlowDownFactor = (gAIEnv.configuration.eCompatibilityMode == ECCM_CRYSIS2) ? 0.2f : 0.4f;
		//adjust desired speed for turns
		float speedScale = 1.0f - fabsf(desiredTurnSpeed * turnSlowDownFactor) / gf_PI;
		speedScale = clamp_tpl(speedScale, scaleLimit, 1.0f);

		//adjust desired speed when strafing and running backward
		float strafeSlowDown = (gf_PI - fabsf(travelAngle * 0.60f)) / gf_PI;
		strafeSlowDown = clamp_tpl(strafeSlowDown, scaleLimit, 1.0f);

		//adjust desired speed when running uphill & downhill
		float slopeSlowDown = (gf_PI - fabsf(DEG2RAD(bodyInfo.slopeAngle) / 12.0f)) / gf_PI;
		slopeSlowDown = clamp_tpl(slopeSlowDown, scaleLimit, 1.0f);

		float scale = min(speedScale, min(strafeSlowDown, slopeSlowDown));

		normalSpeed *= scale;
		Limit(normalSpeed, minSpeed, maxSpeed);
	}
}

void CAIActor::ResetLookAt()
{
	m_State.vLookTargetPos.zero();
}

bool CAIActor::SetLookAtPointPos(const Vec3& vPoint, bool bPriority)
{
	m_State.vLookTargetPos = vPoint;

	// Inspired by CPipeUser::SetLookAtPointPos [5/3/2010 evgeny]
	Vec3 vDesired = vPoint - GetPos();
	if (!m_movementAbility.b3DMove)
	{
		vDesired.z = 0;
	}
	vDesired.NormalizeSafe();

	const SAIBodyInfo& bodyInfo = GetBodyInfo();

	Vec3 vCurrent = bodyInfo.vEyeDirAnim;
	if (!m_movementAbility.b3DMove)
	{
		vCurrent.z = 0;
	}
	vCurrent.NormalizeSafe();

	// cos( 11.5deg ) ~ 0.98
	return 0.98f <= vCurrent.Dot(vDesired);
}

bool CAIActor::SetLookAtDir(const Vec3& vDir, bool bPriority)
{
	Vec3 vDirCopy = vDir;
	return vDirCopy.NormalizeSafe() ? SetLookAtPointPos(GetPos() + vDirCopy * 100.0f) : true;
}

void CAIActor::ResetBodyTargetDir()
{
	m_State.vBodyTargetDir.zero();
}

void CAIActor::SetBodyTargetDir(const Vec3& vDir)
{
	m_State.vBodyTargetDir = vDir;
}

void CAIActor::SetMoveTarget(const Vec3& vMoveTarget)
{
	m_State.vMoveTarget = vMoveTarget;
}

const Vec3& CAIActor::GetBodyTargetDir() const
{
	return m_State.vBodyTargetDir;
}

void CAIActor::GoTo(const Vec3& vTargetPos)
{
	m_State.vLookTargetPos = vTargetPos;
	m_State.vMoveTarget = vTargetPos;
}

void CAIActor::SetSpeed(float fSpeed)
{
	m_State.fMovementUrgency = fSpeed;
}

IPhysicalEntity* CAIActor::GetPhysics(bool bWantCharacterPhysics) const
{
	IAIActorProxy* pAIActorProxy = GetProxy();
	return pAIActorProxy ? pAIActorProxy->GetPhysics(bWantCharacterPhysics)
	       : CAIObject::GetPhysics(bWantCharacterPhysics);
}

bool CAIActor::CanDamageTarget(IAIObject* target) const
{
	return true;
}

bool CAIActor::CanDamageTargetWithMelee() const
{
	return true;
}

void CAIActor::SetTerritoryShapeName(const char* szName)
{
	assert(szName);

	if (m_territoryShapeName.compare(szName))
	{
		m_territoryShapeName = szName;

		if (m_territoryShapeName.compare("<None>"))
		{
			m_territoryShape = GetAISystem()->GetGenericShapeOfName(szName);  // i.e. m_territoryShapeName

			if (m_territoryShape)
			{
				// Territory shapes should be really simple
				size_t size = m_territoryShape->shape.size();
				if (size > 8)
				{
					AIWarning("Territory shape %s for %s has %" PRISIZE_T " points.  Territories should not have more than 8 points",
					          szName, GetName(), size);
				}
			}
			else
			{
				m_territoryShapeName += " (not found)";
			}
		}
		else
		{
			m_territoryShape = 0;
		}
	}
}

const char* CAIActor::GetTerritoryShapeName() const
{
	return (gEnv->IsEditor() && !gEnv->IsEditing())
	       ? m_Parameters.m_sTerritoryName.c_str()
	       : m_territoryShapeName.c_str();
}

bool CAIActor::IsPointInsideTerritoryShape(const Vec3& vPos, bool bCheckHeight) const
{
	bool bResult = true;

	const SShape* pTerritory = GetTerritoryShape();
	if (pTerritory)
	{
		bResult = pTerritory->IsPointInsideShape(vPos, bCheckHeight);
	}

	return bResult;
}

bool CAIActor::ConstrainInsideTerritoryShape(Vec3& vPos, bool bCheckHeight) const
{
	bool bResult = true;

	const SShape* pTerritory = GetTerritoryShape();
	if (pTerritory)
	{
		bResult = pTerritory->ConstrainPointInsideShape(vPos, bCheckHeight);
	}

	return bResult;
}

//===================================================================
// GetObjectType
//===================================================================
CAIActor::EAIObjectType CAIActor::GetObjectType(const CAIObject* ai, unsigned short type)
{
	if (type == AIOBJECT_PLAYER)
		return AIOT_PLAYER;
	else if (type == AIOBJECT_ACTOR)
		return AIOT_AGENTSMALL;
	else if (type == AIOBJECT_VEHICLE)
	{
		// differentiate between medium and big vehicles (e.g. tank can drive over jeep)
		return AIOT_AGENTMED;
	}
	else
		return AIOT_UNKNOWN;
}

//===================================================================
// GetNavInteraction
//===================================================================
CAIActor::ENavInteraction CAIActor::GetNavInteraction(const CAIObject* navigator, const CAIObject* obstacle)
{
	if (const CAIActor* actor = navigator->CastToCAIActor())
	{
		const SAIBodyInfo& info = actor->GetBodyInfo();

		if (info.GetLinkedVehicleEntity())
			return NI_IGNORE;
	}

	unsigned short navigatorType = navigator->GetType();
	unsigned short obstacleType = obstacle->GetType();

	bool enemy = navigator->IsHostile(obstacle);

	EAIObjectType navigatorOT = GetObjectType(navigator, navigatorType);
	EAIObjectType obstacleOT = GetObjectType(obstacle, obstacleType);

	switch (navigatorOT)
	{
	case AIOT_UNKNOWN:
	case AIOT_PLAYER:
		return NI_IGNORE;
	case AIOT_AGENTSMALL:
		// don't navigate around their enemies, unless the enemy is bigger
		/*    if (enemy)
		      return obstacleOT > navigatorOT ? NI_STEER : NI_IGNORE;
		    else*/
		return NI_STEER;
	case AIOT_AGENTMED:
	case AIOT_AGENTBIG:
		// don't navigate around their enemies, unless the enemy is same size or bigger
		if (enemy)
			return obstacleOT >= navigatorOT ? NI_STEER : NI_IGNORE;
		else
			return NI_STEER;
	default:
		AIError("GetNavInteraction: Unhandled switch case %d", navigatorOT);
		return NI_IGNORE;
	}
}

void CAIActor::CoordinationEntered(const char* signalName)
{
	++m_activeCoordinationCount;
	assert(m_activeCoordinationCount < 10);

	SetSignal(AISIGNAL_ALLOW_DUPLICATES, signalName);
}

void CAIActor::CoordinationExited(const char* signalName)
{
	//assert(m_activeCoordinationCount < 10 && m_activeCoordinationCount > 0);
	// Morgan - 10-28-10 This should not require a check against zero.
	// If its already zero, then coordinations were stopped, that had already actors who had finished their roles.
	// Need addition to coordination system to prevent stop methods being called for already complete sequences.
	if (m_activeCoordinationCount > 0)
	{
		--m_activeCoordinationCount;
	}

	if (m_activeCoordinationCount == 0)
		SetSignal(AISIGNAL_ALLOW_DUPLICATES, signalName);
}

bool CAIActor::GetInitialPosition(Vec3& initialPosition) const
{
	if (m_initialPosition.isValid)
	{
		initialPosition = m_initialPosition.pos;
		return true;
	}
	else
	{
		return false;
	}
}

void CAIActor::StartBehaviorTree(const char* behaviorName)
{
	if (GetAISystem()->GetIBehaviorTreeManager()->StartModularBehaviorTree(GetEntityID(), behaviorName))
		m_runningBehaviorTree = true;
}

void CAIActor::StopBehaviorTree()
{
	if (m_runningBehaviorTree)
	{
		GetAISystem()->GetIBehaviorTreeManager()->StopModularBehaviorTree(GetEntityID());
		m_runningBehaviorTree = false;
	}
}

bool CAIActor::IsRunningBehaviorTree() const
{
	return m_runningBehaviorTree;
}
