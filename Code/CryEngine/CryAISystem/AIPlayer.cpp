// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIPlayer.cpp
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - ?

 *********************************************************************/

#include "StdAfx.h"
#include "AIPlayer.h"
#include "CAISystem.h"
#include "DebugDrawContext.h"
#include "MissLocationSensor.h"
#include "Puppet.h"
#include "Formation/FormationManager.h"

#include <CryAISystem/VisionMapTypes.h>

// The variables needs to be carefully tuned to possible the player action speeds.
static const float PLAYER_ACTION_SPRINT_RESET_TIME = 0.5f;
static const float PLAYER_ACTION_JUMP_RESET_TIME = 1.3f;
static const float PLAYER_ACTION_CLOAK_RESET_TIME = 1.5f;

static const float PLAYER_IGNORE_COVER_TIME = 6.0f;

//
//---------------------------------------------------------------------------------
CAIPlayer::CAIPlayer()
	: m_lastGrabbedEntityID(0)
	, m_FOV(0)
	, m_damagePartsUpdated(false)
	, m_deathCount(0)
	, m_stuntDir(ZERO)
	, m_playerStuntSprinting(-1.0f)
	, m_playerStuntJumping(-1.0f)
	, m_playerStuntCloaking(-1.0f)
	, m_playerStuntUncloaking(-1.0f)
	, m_mercyTimer(-1.0f)
	, m_coverExposedTime(-1.0f)
	, m_coolMissCooldown(0.0f)
#pragma warning(push)
#pragma warning(disable: 4355) // 'this': used in base member initializer list
#if ENABLE_MISSLOCATION_SENSOR
	, m_pMissLocationSensor(new CMissLocationSensor(this))
#endif
#pragma warning(pop)
{
	_fastcast_CAIPlayer = true;
}

//
//---------------------------------------------------------------------------------
CAIPlayer::~CAIPlayer()
{
	if (m_exposedCoverState.rayID)
		gAIEnv.pRayCaster->Cancel(m_exposedCoverState.rayID);

	ReleaseExposedCoverObjects();
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::Reset(EObjectResetType type)
{
	MyBase::Reset(type);

	SetObservable(type == AIOBJRESET_INIT);

	m_fLastUpdateTargetTime = 0.f;
	m_lastGrabbedEntityID = 0;

	if (m_exposedCoverState.rayID)
	{
		gAIEnv.pRayCaster->Cancel(m_exposedCoverState.rayID);
		m_exposedCoverState.rayID = 0;
	}

	m_deathCount = 0;
	m_lastThrownItems.clear();
	m_stuntTargets.clear();
	m_stuntDir.Set(0, 0, 0);
	m_mercyTimer = -1.0f;
	m_coverExposedTime = -1.0f;
	m_coolMissCooldown = 0.0f;
	m_damagePartsUpdated = false;

#if ENABLE_MISSLOCATION_SENSOR
	if (m_pMissLocationSensor.get())
		m_pMissLocationSensor->Reset();
#endif

	ReleaseExposedCoverObjects();
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::ReleaseExposedCoverObjects()
{
	for (unsigned i = 0, ni = m_exposedCoverObjects.size(); i < ni; ++i)
		m_exposedCoverObjects[i].pPhysEnt->Release();
	m_exposedCoverObjects.clear();
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::AddExposedCoverObject(IPhysicalEntity* pPhysEnt)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	unsigned oldest = 0;
	float oldestTime = FLT_MAX; // Count down timers, find smallest value.
	for (unsigned i = 0, ni = m_exposedCoverObjects.size(); i < ni; ++i)
	{
		SExposedCoverObject& co = m_exposedCoverObjects[i];
		if (co.pPhysEnt == pPhysEnt)
		{
			co.t = PLAYER_IGNORE_COVER_TIME;
			return;
		}
		if (co.t < oldestTime)
		{
			oldest = i;
			oldestTime = co.t;
		}
	}

	// Limit the number of covers, override oldest one.
	if (m_exposedCoverObjects.size() >= 3)
	{
		// Release the previous entity
		m_exposedCoverObjects[oldest].pPhysEnt->Release();
		// Fill in new.
		pPhysEnt->AddRef();
		m_exposedCoverObjects[oldest].pPhysEnt = pPhysEnt;
		m_exposedCoverObjects[oldest].t = PLAYER_IGNORE_COVER_TIME;
	}
	else
	{
		// Add new
		pPhysEnt->AddRef();
		m_exposedCoverObjects.push_back(SExposedCoverObject(pPhysEnt, PLAYER_IGNORE_COVER_TIME));
	}
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::CollectExposedCover()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_coverExposedTime > 0.0f)
	{
		if (m_exposedCoverState.asyncState == AsyncReady)
		{
			m_exposedCoverState.asyncState = AsyncInProgress;

			// Find the object directly in front of the player
			const Vec3& pos = GetPos();
			const Vec3 dir = GetViewDir() * 3.0f;
			const int flags = rwi_colltype_any | (geom_colltype_obstruct << rwi_colltype_bit) | (AI_VISION_RAY_CAST_PIERCEABILITY & rwi_pierceability_mask);

			m_exposedCoverState.rayID = gAIEnv.pRayCaster->Queue(
			  RayCastRequest::MediumPriority,
			  RayCastRequest(pos, dir, ent_static, flags),
			  functor(*this, &CAIPlayer::CollectExposedCoverRayComplete));
		}
	}
}

void CAIPlayer::CollectExposedCoverRayComplete(const QueuedRayID& rayID, const RayCastResult& result)
{
	if (m_exposedCoverState.rayID == rayID)
	{
		m_exposedCoverState.rayID = 0;
		m_exposedCoverState.asyncState = AsyncReady;

		if (result && result[0].pCollider)
			AddExposedCoverObject(result[0].pCollider);
	}
}

void CAIPlayer::GetObservablePositions(ObservableParams& observableParams) const
{
	IEntity* entity = GetEntity();
	IF_UNLIKELY (!entity)
	{
		return;
	}

	const Vec3& pos = GetPos();
	const Quat& rotation = entity->GetRotation();

	observableParams.observablePositionsCount = 3;
	observableParams.observablePositions[0] = pos;

	// shoulder positions
	observableParams.observablePositions[1] = pos + (rotation * Vec3(0.15f, 0.0f, -0.2f));
	observableParams.observablePositions[2] = pos + (rotation * Vec3(-0.15f, 0.0f, -0.2f));
}

uint32 CAIPlayer::GetObservableTypeMask() const
{
	return MyBase::GetObservableTypeMask() | Player;
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::GetPhysicalSkipEntities(PhysSkipList& skipList) const
{
	MyBase::GetPhysicalSkipEntities(skipList);

	// Skip exposed covers
	for (uint32 i = 0, ni = m_exposedCoverObjects.size(); i < ni; ++i)
	{
		stl::push_back_unique(skipList, m_exposedCoverObjects[i].pPhysEnt);
	}
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::ParseParameters(const AIObjectParams& params, bool bParseMovementParams)
{
	MyBase::ParseParameters(params, bParseMovementParams);
	m_Parameters = params.m_sParamStruct;
}

//
//---------------------------------------------------------------------------------
IAIObject::EFieldOfViewResult CAIPlayer::IsPointInFOV(const Vec3& pos, float distanceScale) const
{
	EFieldOfViewResult eResult = eFOV_Outside;

	Vec3 vDirection = pos - GetPos();
	const float fDirectionLengthSq = vDirection.GetLengthSquared();
	vDirection.NormalizeSafe();

	// lets see if it is outside of its vision range
	if (fDirectionLengthSq > 0.1f && fDirectionLengthSq <= sqr(m_Parameters.m_PerceptionParams.sightRange * distanceScale))
	{
		const Vec3 vViewDir = GetViewDir().GetNormalizedSafe();
		const float fDot = vDirection.Dot(vViewDir);

		eResult = (fDot >= m_FOV ? eFOV_Primary : eFOV_Outside);
	}

	return eResult;
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::UpdateAttentionTarget(CWeakRef<CAIObject> refTarget)
{
	bool bSameTarget = (m_refAttentionTarget == refTarget);
	if (bSameTarget)
	{
		m_fLastUpdateTargetTime = GetAISystem()->GetFrameStartTime();
	}
	else // compare the new target with the current one
	{
		CCCPOINT(CAIPlayer_UpdateAttentionTarget);

		CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();
		CAIObject* pTarget = refTarget.GetAIObject();

		Vec3 direction = pAttentionTarget->GetPos() - GetPos();

		// lets see if it is outside of its vision range

		Vec3 myorievector = GetViewDir();
		float dist = direction.GetLength();

		if (dist > 0)
			direction /= dist;
		Vec3 directionNew(pTarget->GetPos() - GetPos());

		float distNew = directionNew.GetLength();
		if (distNew > 0)
			directionNew /= dist;

		float fdot = ((Vec3)direction).Dot((Vec3)myorievector);
		// check if new target is more interesting, by checking if old target is still visible
		// and comparing distances if it is
		if (fdot < 0 || fdot < m_FOV || distNew < dist)
		{
			m_refAttentionTarget = refTarget;
			m_fLastUpdateTargetTime = GetAISystem()->GetFrameStartTime();
		}
	}
}

//
//---------------------------------------------------------------------------------
float CAIPlayer::AdjustTargetVisibleRange(const CAIActor& observer, float fVisibleRange) const
{
	float fRangeScale = 1.0f;

	// Adjust using my light level if the observer is affected by light
	if (IsAffectedByLight())
	{
		const EAILightLevel targetLightLevel = GetLightLevel();
		switch (targetLightLevel)
		{
		//	case AILL_LIGHT: SOMSpeed
		case AILL_MEDIUM:
			fRangeScale *= gAIEnv.CVars.SightRangeMediumIllumMod;
			break;
		case AILL_DARK:
			fRangeScale *= gAIEnv.CVars.SightRangeDarkIllumMod;
			break;
		case AILL_SUPERDARK:
			fRangeScale *= gAIEnv.CVars.SightRangeSuperDarkIllumMod;
			break;
		}
	}

	// Scale down sight range when target is underwater based on distance
	const float fCachedWaterOcclusionValue = GetCachedWaterOcclusionValue();
	if (fCachedWaterOcclusionValue > FLT_EPSILON)
	{
		const Vec3& observerPos = observer.GetPos();
		const float fDistance = Distance::Point_Point(GetPos(), observerPos);
		const float fDistanceFactor = (fVisibleRange > FLT_EPSILON ? GetAISystem()->GetVisPerceptionDistScale(fDistance / fVisibleRange) : 0.0f);

		const float fWaterOcclusionEffect = 2.0f * fCachedWaterOcclusionValue + (1 - fDistanceFactor) * 0.5f;

		fRangeScale *= (fWaterOcclusionEffect > 1.0f ? 0.0f : 1.0f - fWaterOcclusionEffect);
	}

	// Return new range
	return fVisibleRange * fRangeScale;
}

//
//---------------------------------------------------------------------------------
bool CAIPlayer::IsAffectedByLight() const
{
	return (gAIEnv.CVars.PlayerAffectedByLight != 0 ||
	        MyBase::IsAffectedByLight());
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::Update(EUpdateType type)
{
	if (m_refAttentionTarget.IsValid())
	{
		if (!m_refAttentionTarget.GetAIObject()->IsEnabled() ||
		    (GetAISystem()->GetFrameStartTime() - m_fLastUpdateTargetTime).GetMilliSecondsAsInt64() > 5000)
			m_refAttentionTarget.Reset();
	}

	// There should never be player without physics (except in multiplayer, this case is valid).
	if (!GetPhysics())
	{
		if (!gEnv->bMultiplayer)
		{
			AIWarning("AIPlayer::Update Player %s does not have physics!", GetName());
		}
		return;
	}

#if ENABLE_MISSLOCATION_SENSOR
	if (m_pMissLocationSensor.get())
		m_pMissLocationSensor->DebugDraw();
#endif

	CCCPOINT(CAIPlayer_Update);

	IEntity* grabbedEntity = GetGrabbedEntity();
	EntityId grabbedEntityID = grabbedEntity ? grabbedEntity->GetId() : 0;

	if (m_lastGrabbedEntityID != grabbedEntityID)
	{
		UpdateObservableSkipList();

		m_lastGrabbedEntityID = grabbedEntityID;
	}

	// make sure to update direction when entity is not moved
	const SAIBodyInfo& bodyInfo = QueryBodyInfo();
	SetPos(bodyInfo.vEyePos);
	SetEntityDir(bodyInfo.vEntityDir);
	SetMoveDir(bodyInfo.vMoveDir);
	SetViewDir(bodyInfo.GetEyeDir());
	SetBodyDir(bodyInfo.GetBodyDir());

	// Determine if position has changed. When crouching/standing we also need to recalculate the water
	// occlusion value even if we didn't move at least one meter.
	const float zMinDifferenceBetweenStandAndCrouchToRecalculateWaterOcclusion = 0.6f;
	const bool playerFullyChangedStance = (m_lastFullUpdateStance != bodyInfo.stance)
	                                      && abs(m_vLastFullUpdatePos.z - bodyInfo.vEyePos.z) > zMinDifferenceBetweenStandAndCrouchToRecalculateWaterOcclusion;
	if (type == EUpdateType::Full && (!IsEquivalent(m_vLastFullUpdatePos, bodyInfo.vEyePos, 1.0f) || playerFullyChangedStance))
	{
		// Recalculate the water occlusion at the new point
		m_cachedWaterOcclusionValue = GetAISystem()->GetWaterOcclusionValue(bodyInfo.vEyePos);

		m_vLastFullUpdatePos = bodyInfo.vEyePos;
		m_lastFullUpdateStance = bodyInfo.stance;
	}

	m_bUpdatedOnce = true;

	//@TODO: It is wrong here to use Global Camera!
	m_FOV = cosf(GetISystem()->GetViewCamera().GetFov());

	if (IsObserver())
		VisionChanged(75.0f, m_FOV, m_FOV);

	// (MATT) I'm assuming that AIActor should always have a proxy, or this could be bad for performance {2009/04/03}
	IAIActorProxy* pProxy = GetProxy();
	if (pProxy)
		pProxy->UpdateMind(m_State);

	if (type == EUpdateType::Full)
	{
		m_lightLevel = GetAISystem()->GetLightManager()->GetLightLevelAt(GetPos(), this, &m_usingCombatLight);

#if ENABLE_MISSLOCATION_SENSOR
		m_pMissLocationSensor->Update(0.005f);
#endif

#ifdef CRYAISYSTEM_DEBUG
		// Health
		{
			RecorderEventData recorderEventData(GetProxy()->GetActorHealth());
			RecordEvent(IAIRecordable::E_HEALTH, &recorderEventData);
		}

		// Pos
		{
			RecorderEventData recorderEventData(GetPos());
			RecordEvent(IAIRecordable::E_AGENTPOS, &recorderEventData);
		}

		// Dir
		{
			RecorderEventData recorderEventData(GetViewDir());
			RecordEvent(IAIRecordable::E_AGENTDIR, &recorderEventData);
		}
#endif  // #ifdef CRYAISYSTEM_DEBUG
	}

	const float dt = GetAISystem()->GetFrameDeltaTime();

	if (m_coolMissCooldown > 0.0f)
		m_coolMissCooldown -= dt;

	// Exposed (soft) covers. When the player fires the weapon,
	// disable the cover where the player is hiding at or the
	// soft cover the player is shooting at.

	// Timeout the cover exposure timer.
	if (m_coverExposedTime > 0.0f)
		m_coverExposedTime -= dt;
	else
		m_coverExposedTime = -1.0f;

	if (GetProxy())
	{
		SAIWeaponInfo wi;
		GetProxy()->QueryWeaponInfo(wi);
		if (wi.isFiring)
			m_coverExposedTime = 1.0f;
	}

	// Timeout the exposed covers
	for (unsigned i = 0; i < m_exposedCoverObjects.size(); )
	{
		m_exposedCoverObjects[i].t -= dt;
		if (m_exposedCoverObjects[i].t < 0.0f)
		{
			m_exposedCoverObjects[i].pPhysEnt->Release();
			m_exposedCoverObjects[i] = m_exposedCoverObjects.back();
			m_exposedCoverObjects.pop_back();
		}
		else
			++i;
	}

	// Collect new covers.
	if (type == EUpdateType::Full)
		CollectExposedCover();

#ifdef CRYAISYSTEM_DEBUG
	if (gAIEnv.CVars.DebugDrawDamageControl > 0)
		UpdateHealthHistory();
#endif

	UpdatePlayerStuntActions();
	UpdateCloakScale();

	// Update low health mercy pause
	if (m_mercyTimer > 0.0f)
		m_mercyTimer -= GetAISystem()->GetFrameDeltaTime();
	else
		m_mercyTimer = -1.0f;

	m_damagePartsUpdated = false;
}

void CAIPlayer::OnObjectRemoved(CAIObject* pObject)
{
	MyBase::OnObjectRemoved(pObject);

	// (MATT) Moved here from CAISystems call {2009/02/05}
	CAIActor* pRemovedAIActor = pObject->CastToCPuppet();
	if (!pRemovedAIActor)
		return;

	for (unsigned i = 0; i < m_stuntTargets.size(); )
	{
		if (m_stuntTargets[i].pAIActor == pRemovedAIActor)
		{
			m_stuntTargets[i] = m_stuntTargets.back();
			m_stuntTargets.pop_back();
		}
		else
			++i;
	}
}

void CAIPlayer::UpdatePlayerStuntActions()
{
	ActorLookUp& lookUp = *gAIEnv.pActorLookUp;
	lookUp.Prepare(ActorLookUp::Position);

	const float dt = GetAISystem()->GetFrameDeltaTime();

	// Update thrown entities
	for (unsigned i = 0; i < m_lastThrownItems.size(); )
	{
		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_lastThrownItems[i].id);
		if (pEnt)
		{
			if (IPhysicalEntity* pPhysEnt = pEnt->GetPhysics())
			{
				pe_status_dynamics statDyn;
				pPhysEnt->GetStatus(&statDyn);
				m_lastThrownItems[i].pos = pEnt->GetWorldPos();
				m_lastThrownItems[i].vel = statDyn.v;
				if (statDyn.v.GetLengthSquared() > sqr(3.0f))
					m_lastThrownItems[i].time = 0;
			}
		}

		m_lastThrownItems[i].time += dt;
		if (!pEnt || m_lastThrownItems[i].time > 1.0f)
		{
			m_lastThrownItems[i] = m_lastThrownItems.back();
			m_lastThrownItems.pop_back();
		}
		else
			++i;
	}

	Vec3 vel = GetVelocity();
	float speed = vel.GetLength();

	m_stuntDir = vel;
	if (m_stuntDir.GetLength() < 0.001f)
		m_stuntDir = GetEntityDir();
	m_stuntDir.z = 0;
	m_stuntDir.NormalizeSafe();

	if (m_playerStuntSprinting > 0.0f)
	{
		if (speed > 10.0f)
			m_playerStuntSprinting = PLAYER_ACTION_SPRINT_RESET_TIME;
		m_playerStuntSprinting -= dt;
	}

	if (m_playerStuntJumping > 0.0f)
	{
		if (speed > 7.0f)
			m_playerStuntJumping = PLAYER_ACTION_JUMP_RESET_TIME;
		m_playerStuntJumping -= dt;
	}

	if (m_playerStuntCloaking > 0.0f)
		m_playerStuntCloaking -= dt;

	if (m_playerStuntUncloaking > 0.0f)
		m_playerStuntUncloaking -= dt;

	bool checkMovement = m_playerStuntSprinting > 0.0f || m_playerStuntJumping > 0.0f;
	bool checkItems = !m_lastThrownItems.empty();

	if (checkMovement || checkItems)
	{
		float movementScale = 0.7f;
		//		if (m_playerStuntJumping > 0.0f)
		//			movementScale *= 2.0f;

		Lineseg playerMovement(GetPos(), GetPos() + vel * movementScale);
		const SAIBodyInfo& bi = GetBodyInfo();
		float playerRad = bi.stanceSize.GetRadius();

		// Update stunt effect on AI actors
		size_t activeCount = lookUp.GetActiveCount();

		for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
		{
			CAIActor* pAIActor = lookUp.GetActor<CAIActor>(actorIndex);

			if (!IsHostile(pAIActor))
				continue;

			const float scale = pAIActor->GetParameters().m_PerceptionParams.collisionReactionScale;

			bool hit = false;

			float t, distSq;
			Vec3 threatPos;

			if (checkMovement)
			{
				// Player movement
				distSq = Distance::Point_Lineseg2DSq(lookUp.GetPosition(actorIndex), playerMovement, t);
				if (distSq < sqr(playerRad * scale))
				{
					threatPos = GetPos();
					hit = true;
				}
			}

			if (checkItems)
			{
				// Thrown items
				for (unsigned i = 0, ni = m_lastThrownItems.size(); i < ni; ++i)
				{
					Lineseg itemMovement(m_lastThrownItems[i].pos, m_lastThrownItems[i].pos + m_lastThrownItems[i].vel * 2);
					distSq = Distance::Point_LinesegSq(lookUp.GetPosition(actorIndex), itemMovement, t);
					if (distSq < sqr(m_lastThrownItems[i].r * 2.0f * scale))
					{
						threatPos = m_lastThrownItems[i].pos;
						hit = true;
					}
				}
			}

			if (hit)
			{
				bool found = false;
				for (unsigned i = 0, ni = m_stuntTargets.size(); i < ni; ++i)
				{
					if (m_stuntTargets[i].pAIActor == pAIActor)
					{
						m_stuntTargets[i].threatPos = threatPos;
						m_stuntTargets[i].exposed += dt;
						m_stuntTargets[i].t = 0;
						found = true;
						break;
					}
				}
				if (!found)
				{
					m_stuntTargets.push_back(SStuntTargetAIActor(pAIActor, threatPos));
					m_stuntTargets.back().exposed += dt;
				}
			}
		}
	}

	for (unsigned i = 0; i < m_stuntTargets.size(); )
	{
		m_stuntTargets[i].t += dt;
		CAIActor* pAIActor = m_stuntTargets[i].pAIActor;
		if (!m_stuntTargets[i].signalled
		    && m_stuntTargets[i].exposed > 0.15f
		    && pAIActor->GetAttentionTarget()
		    && pAIActor->GetAttentionTarget()->GetEntityID() == GetEntityID())
		{
			IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
			pData->iValue = 1;
			pData->fValue = Distance::Point_Point(m_stuntTargets[i].pAIActor->GetPos(), m_stuntTargets[i].threatPos);
			pData->point = m_stuntTargets[i].threatPos;
			pAIActor->SetSignal(1, "OnCloseCollision", 0, pData);
			if (CPuppet* pPuppet = pAIActor->CastToCPuppet())
				pPuppet->SetAlarmed();
			m_stuntTargets[i].signalled = true;
		}
		if (m_stuntTargets[i].t > 2.0f)
		{
			m_stuntTargets[i] = m_stuntTargets.back();
			m_stuntTargets.pop_back();
		}
		else
			++i;
	}
}

bool CAIPlayer::IsDoingStuntActionRelatedTo(const Vec3& pos, float nearDistance)
{
	if (m_playerStuntCloaking > 0.0f)
		return true;

	if (m_playerStuntSprinting <= 0.0f && m_playerStuntJumping <= 0.0f)
		return false;

	// If the stunt is not done at really close range,
	// do not consider the stunt unless it is towards the specified position.
	Vec3 diff = pos - GetPos();
	diff.z = 0;
	float dist = diff.NormalizeSafe();
	const float thr = cosf(DEG2RAD(75.0f));
	if (dist > nearDistance && m_stuntDir.Dot(diff) < thr)
		return false;

	return true;
}

bool CAIPlayer::IsThrownByPlayer(EntityId id) const
{
	if (m_lastThrownItems.empty()) return false;
	for (unsigned i = 0, ni = m_lastThrownItems.size(); i < ni; ++i)
		if (m_lastThrownItems[i].id == id)
			return true;
	return false;
}

bool CAIPlayer::IsPlayerStuntAffectingTheDeathOf(CAIActor* pDeadActor) const
{
	if (!pDeadActor || !pDeadActor->GetEntity())
		return false;

	// If the actor is thrown/punched by the player.
	if (IsThrownByPlayer(pDeadActor->GetEntityID()))
		return true;

	// If any of the objects the player has thrown is close to the dead body.
	AABB deadBounds;
	pDeadActor->GetEntity()->GetWorldBounds(deadBounds);
	Vec3 deadPos = deadBounds.GetCenter();
	float deadRadius = deadBounds.GetRadius();

	for (unsigned i = 0, ni = m_lastThrownItems.size(); i < ni; ++i)
	{
		if (Distance::Point_PointSq(deadPos, m_lastThrownItems[i].pos) < sqr(deadRadius + m_lastThrownItems[i].r))
			return true;
	}

	return false;
}

EntityId CAIPlayer::GetNearestThrownEntity(const Vec3& pos)
{
	EntityId nearest = 0;
	float nearestDist = FLT_MAX;
	for (unsigned i = 0, ni = m_lastThrownItems.size(); i < ni; ++i)
	{
		float d = Distance::Point_Point(pos, m_lastThrownItems[i].pos) - m_lastThrownItems[i].r;
		if (d < nearestDist)
		{
			nearestDist = d;
			nearest = m_lastThrownItems[i].id;
		}
	}
	return nearest;
}

void CAIPlayer::AddThrownEntity(EntityId id)
{
	float oldestTime = 0.0f;
	unsigned oldestId = 0;
	for (unsigned i = 0, ni = m_lastThrownItems.size(); i < ni; ++i)
	{
		if (m_lastThrownItems[i].id == id)
		{
			m_lastThrownItems[i].time = 0;
			return;
		}
		if (m_lastThrownItems[i].time > oldestTime)
		{
			oldestTime = m_lastThrownItems[i].time;
			oldestId = i;
		}
	}

	IEntity* pEnt = gEnv->pEntitySystem->GetEntity(id);
	if (!pEnt)
		return;

	// The entity does not exists yet, add it to the list of entities to watch.
	m_lastThrownItems.push_back(SThrownItem(id));

	// Skip the nearest thrown entity, since it is potentially blocking the view to the corpse.
	IEntity* pThrownEnt = id ? gEnv->pEntitySystem->GetEntity(id) : 0;
	CAIActor* pThrownActor = CastToCAIActorSafe(pThrownEnt->GetAI());
	if (pThrownActor)
	{
		short gid = (short)pThrownActor->GetGroupId();
		AIObjects::iterator ai = GetAISystem()->m_mapGroups.find(gid);
		AIObjects::iterator end = GetAISystem()->m_mapGroups.end();
		for (; ai != end && ai->first == gid; ++ai)
		{
			CPuppet* pPuppet = CastToCPuppetSafe(ai->second.GetAIObject());
			if (!pPuppet) continue;
			if (pPuppet->GetEntityID() == pThrownActor->GetEntityID()) continue;
			float dist = FLT_MAX;
			if (!GetAISystem()->CheckVisibilityToBody(pPuppet, pThrownActor, dist))
				continue;
			pPuppet->SetSignal(1, "OnGroupMemberMutilated", pThrownActor->GetEntity(), 0);
			pPuppet->SetAlarmed();
		}
	}

	// Set initial position, radius and velocity.
	m_lastThrownItems.back().pos = pEnt->GetWorldPos();
	AABB bounds;
	pEnt->GetWorldBounds(bounds);
	m_lastThrownItems.back().r = bounds.GetRadius();

	if (IPhysicalEntity* pPhysEnt = pEnt->GetPhysics())
	{
		pe_status_dynamics statDyn;
		pPhysEnt->GetStatus(&statDyn);
		m_lastThrownItems.back().vel = statDyn.v;
	}

	// Limit the number of hot entities.
	if (m_lastThrownItems.size() > 4)
	{
		m_lastThrownItems[oldestId] = m_lastThrownItems.back();
		m_lastThrownItems.pop_back();
	}
}

void CAIPlayer::HandleArmoredHit()
{
	NotifyPlayerActionToTheLookingAgents("OnTargetArmoredHit");
}

void CAIPlayer::HandleCloaking(bool cloak)
{
	CAISystem* pAISystem = GetAISystem();

	m_Parameters.m_bCloaked = cloak;
	m_Parameters.m_fLastCloakEventTime = pAISystem->GetFrameStartTime().GetSeconds();
	NotifyPlayerActionToTheLookingAgents(cloak ? "OnTargetCloaked" : "OnTargetUncloaked");
}

void CAIPlayer::HandleStampMelee()
{
	if (!m_Parameters.m_bCloaked)
		NotifyPlayerActionToTheLookingAgents("OnTargetStampMelee");
}

//-----------------------------------------------------------

void CAIPlayer::NotifyPlayerActionToTheLookingAgents(const char* eventName)
{
	CAISystem* pAISystem = GetAISystem();
	ActorLookUp& lookUp = *gAIEnv.pActorLookUp;
	size_t activeCount = lookUp.GetActiveCount();

	// Notify AI who have the player as their target
	EntityId playerId = GetEntityID();
	for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
	{
		CAIActor* pAIActor = lookUp.GetActor<CAIActor>(actorIndex);

		CAIObject* pTarget = static_cast<CAIObject*>(pAIActor ? pAIActor->GetAttentionTarget() : NULL);
		if (pTarget)
		{
			bool targetIsPlayer = (pTarget->GetEntityID() == playerId);
			VisionID targetVisionId = GetVisionID(); // This is the player vision id
			if (!targetIsPlayer)
			{
				// Test association
				CWeakRef<CAIObject> refAssociation = pTarget->GetAssociation();
				CAIObject* pAssociation = (refAssociation.IsValid() ? refAssociation.GetAIObject() : NULL);
				targetIsPlayer = (pAssociation && pAssociation->GetEntityID() == playerId);
				if (targetIsPlayer)
				{
					targetVisionId = pAssociation->GetVisionID();
				}
			}

			if (targetIsPlayer)
			{
				const bool bCanSeeTarget = pAIActor->CanSee(targetVisionId);
				if (bCanSeeTarget)
				{
					AISignalExtraData* pData = new AISignalExtraData;
					pData->nID = playerId;
					pData->iValue = pAIActor->GetAttentionTargetType();
					pData->iValue2 = pAIActor->GetAttentionTargetThreat();
					pData->fValue = pAIActor->GetPos().GetDistance(GetPos());
					pAISystem->SendSignal(SIGNALFILTER_SENDER, 1, eventName, pAIActor, pData);
				}
			}
		}
	}
}

//-----------------------------------------------------------

void CAIPlayer::Event(unsigned short eType, SAIEVENT* pEvent)
{
	switch (eType)
	{
	case AIEVENT_AGENTDIED:
		// make sure everybody knows I have died
		GetAISystem()->NotifyTargetDead(this);
		m_bEnabled = false;
		GetAISystem()->RemoveFromGroup(GetGroupId(), this);

		gAIEnv.pFormationManager->ReleaseFormationPoint(this);
		ReleaseFormation();

		m_State.ClearSignals();

		if (m_proxy)
			m_proxy->Reset(AIOBJRESET_SHUTDOWN);

		SetObservable(false);
		break;
	case AIEVENT_PLAYER_STUNT_SPRINT:
		m_playerStuntSprinting = PLAYER_ACTION_SPRINT_RESET_TIME;
		m_playerStuntJumping = -1.0f;
		break;
	case AIEVENT_PLAYER_STUNT_JUMP:
		m_playerStuntJumping = PLAYER_ACTION_JUMP_RESET_TIME;
		m_playerStuntSprinting = -1.0f;
		break;
	case AIEVENT_PLAYER_STUNT_PUNCH:
	case AIEVENT_PLAYER_STUNT_THROW:
	case AIEVENT_PLAYER_STUNT_THROW_NPC:
	case AIEVENT_PLAYER_THROW:
		if (pEvent)
			AddThrownEntity(pEvent->targetId);
		break;
	case AIEVENT_PLAYER_STUNT_CLOAK:
		m_playerStuntCloaking = PLAYER_ACTION_CLOAK_RESET_TIME;
		HandleCloaking(true);
		break;
	case AIEVENT_PLAYER_STUNT_UNCLOAK:
		m_playerStuntUncloaking = PLAYER_ACTION_CLOAK_RESET_TIME;
		HandleCloaking(false);
		break;
	case AIEVENT_PLAYER_STUNT_ARMORED:
		HandleArmoredHit();
		break;
	case AIEVENT_PLAYER_STAMP_MELEE:
		HandleStampMelee();
		break;
	case AIEVENT_LOWHEALTH:
		{
			const float scale = (pEvent != NULL) ? pEvent->fThreat : 1.0f;
			m_mercyTimer = gAIEnv.CVars.RODLowHealthMercyTime * scale;
		}
		break;
	case AIEVENT_ENABLE:
		SetObservable(true);
		MyBase::Event(eType, pEvent);
		break;
	default:
		MyBase::Event(eType, pEvent);
		break;
	}
}

//
//---------------------------------------------------------------------------------
DamagePartVector* CAIPlayer::GetDamageParts()
{
	if (!m_damagePartsUpdated)
	{
		UpdateDamageParts(m_damageParts);
		m_damagePartsUpdated = true;
	}

	return &m_damageParts;
}

//
//----------------------------------------------------------------------------------------------
void CAIPlayer::RecordSnapshot()
{
	// Currently not used
}

//
//----------------------------------------------------------------------------------------------
void CAIPlayer::RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData)
{
#ifdef CRYAISYSTEM_DEBUG
	CRecorderUnit* pRecord = (CRecorderUnit*)GetAIDebugRecord();
	if (pRecord != NULL)
	{
		pRecord->RecordEvent(event, pEventData);
	}
#endif //CRYAISYSTEM_DEBUG
}

bool CAIPlayer::GetMissLocation(const Vec3& shootPos, const Vec3& shootDir, float maxAngle, Vec3& pos)
{
#if ENABLE_MISSLOCATION_SENSOR
	if (m_coolMissCooldown <= 0.000001f)
		return m_pMissLocationSensor->GetLocation(this, shootPos, shootDir, maxAngle, pos);
#endif

	return false;
}

void CAIPlayer::NotifyMissLocationConsumed()
{
	m_coolMissCooldown += gAIEnv.CVars.CoolMissesCooldown;
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::DebugDraw()
{
	CDebugDrawContext dc;

	// Draw items associated with player actions.
	for (unsigned i = 0, ni = m_lastThrownItems.size(); i < ni; ++i)
	{
		//		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_lastThrownItems[i].id);
		//		if (pEnt)
		{
			/*			AABB bounds;
			      pEnt->GetWorldBounds(bounds);
			      dc->DrawAABB(bounds, false, ColorB(255, 0, 0), eBBD_Faceted);
			      bounds.Move(m_lastThrownItems[i].vel);
			      dc->DrawAABB(bounds, false, ColorB(255, 0, 0, 128), eBBD_Faceted);*/

			AABB bounds(AABB::RESET);
			bounds.Add(m_lastThrownItems[i].pos, m_lastThrownItems[i].r);
			dc->DrawAABB(bounds, false, ColorB(255, 0, 0), eBBD_Faceted);
			bounds.Move(m_lastThrownItems[i].vel);
			dc->DrawLine(m_lastThrownItems[i].pos, ColorB(255, 0, 0), m_lastThrownItems[i].pos + m_lastThrownItems[i].vel, ColorB(255, 0, 0, 128));
			dc->DrawAABB(bounds, false, ColorB(255, 0, 0, 128), eBBD_Faceted);

			//			Vec3 dir = m_lastThrownItems[i].vel;
			//			float speed = dir.NormalizeSafe(Vec3(1,0,0));

			/*			float speed = m_lastThrownItems[i].vel.GetLength();
			      if (speed > 0.0f)
			      {
			      }
			      else
			      {
			      }*/
		}
	}

	for (unsigned i = 0, ni = m_stuntTargets.size(); i < ni; ++i)
	{
		const SAIBodyInfo& bodyInfo = m_stuntTargets[i].pAIActor->GetBodyInfo();
		Vec3 pos = m_stuntTargets[i].pAIActor->GetPhysicsPos();
		AABB aabb(bodyInfo.stanceSize);
		aabb.Move(pos);
		dc->DrawAABB(aabb, true, ColorB(255, 255, 255, m_stuntTargets[i].signalled ? 128 : 48), eBBD_Faceted);
	}

	ColorB color(255, 255, 255);

	// Draw special player actions
	if (m_playerStuntSprinting > 0.0f)
	{
		Vec3 pos = GetPos();
		Vec3 vel = GetVelocity();
		const SAIBodyInfo& bi = GetBodyInfo();
		float r = bi.stanceSize.GetRadius();
		AABB bounds(AABB::RESET);
		bounds.Add(pos, r);
		dc->DrawAABB(bounds, false, ColorB(255, 0, 0), eBBD_Faceted);
		bounds.Move(vel);
		dc->DrawLine(pos, ColorB(255, 0, 0), pos + vel, ColorB(255, 0, 0, 128));
		dc->DrawAABB(bounds, false, ColorB(255, 0, 0, 128), eBBD_Faceted);

		// [2/27/2009 evgeny] Here and below in this method,
		// first argument for Draw2dLabel was 10, not 100, and the text was hardly visible
		dc->Draw2dLabel(100, 10, 2.5f, color, true, "SPRINTING");
	}
	if (m_playerStuntJumping > 0.0f)
	{
		Vec3 pos = GetPos();
		Vec3 vel = GetVelocity();
		const SAIBodyInfo& bi = GetBodyInfo();
		float r = bi.stanceSize.GetRadius();
		AABB bounds(AABB::RESET);
		bounds.Add(pos, r);
		dc->DrawAABB(bounds, false, ColorB(255, 0, 0), eBBD_Faceted);
		bounds.Move(vel);
		dc->DrawLine(pos, ColorB(255, 0, 0), pos + vel, ColorB(255, 0, 0, 128));
		dc->DrawAABB(bounds, false, ColorB(255, 0, 0, 128), eBBD_Faceted);

		dc->Draw2dLabel(100, 40, 2.5f, color, true, "JUMPING");
	}
	if (m_playerStuntCloaking > 0.0f)
	{
		dc->Draw2dLabel(100, 70, 2.5f, color, true, "CLOAKING");
	}
	if (m_playerStuntUncloaking > 0.0f)
	{
		dc->Draw2dLabel(100, 110, 2.5f, color, true, "UNCLOAKING");
	}
	if (!m_lastThrownItems.empty())
	{
		dc->Draw2dLabel(100, 150, 2.5f, color, true, "THROWING");
	}

	if (IsLowHealthPauseActive())
	{
		ICVar* pLowHealth = gEnv->pConsole->GetCVar("g_playerLowHealthThreshold");
		dc->Draw2dLabel(150, 190, 2.0f, color, true, "Mercy %.2f/%.2f (when below %0.2f)", m_mercyTimer, gAIEnv.CVars.RODLowHealthMercyTime, pLowHealth->GetFVal());
	}

	for (unsigned i = 0, ni = m_exposedCoverObjects.size(); i < ni; ++i)
	{
		pe_status_pos statusPos;
		m_exposedCoverObjects[i].pPhysEnt->GetStatus(&statusPos);
		AABB bounds(AABB::RESET);
		bounds.Add(statusPos.BBox[0] + statusPos.pos);
		bounds.Add(statusPos.BBox[1] + statusPos.pos);
		dc->DrawAABB(bounds, false, ColorB(255, 0, 0), eBBD_Faceted);
		dc->Draw3dLabel(bounds.GetCenter(), 1.1f, "IGNORED %.1fs", m_exposedCoverObjects[i].t);
	}
}

//
//---------------------------------------------------------------------------------
bool CAIPlayer::IsLowHealthPauseActive() const
{
	if (m_mercyTimer > 0.0f)
		return true;
	return false;
}

//
//------------------------------------------------------------------------------------------------------------------------
IEntity* CAIPlayer::GetGrabbedEntity() const
{
	IAIActorProxy* pProxy = GetProxy();

	return pProxy ? pProxy->GetGrabbedEntity() : NULL;
}

//
//------------------------------------------------------------------------------------------------------------------------
bool CAIPlayer::IsGrabbedEntityInView(const Vec3& pos) const
{
	bool bInViewDist = true;

	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	assert(pEntitySystem);

	IEntity* pObjectEntity = GetGrabbedEntity();

	IEntityRender* pObjectRenderProxy = (pObjectEntity ? (pObjectEntity->GetRenderInterface()) : NULL);
	if (pObjectRenderProxy)
	{
		IRenderNode* pObjectRenderNode = pObjectRenderProxy->GetRenderNode();
		if (pObjectRenderNode)
		{
			const float fDistanceSq = pos.GetSquaredDistance(pObjectEntity->GetWorldPos());
			const float fMinDist = 4.0f;
			const float fMaxViewDistSq = sqr(max(pObjectRenderNode->GetMaxViewDist(), fMinDist));

			bInViewDist = (fDistanceSq <= fMaxViewDistSq);
		}
	}

	return bInViewDist;
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::Serialize(TSerialize ser)
{
	ser.BeginGroup("AIPlayer");

	MyBase::Serialize(ser);

	ser.Value("m_fLastUpdateTargetTime", m_fLastUpdateTargetTime);
	ser.Value("m_FOV", m_FOV);

	ser.Value("m_playerStuntSprinting", m_playerStuntSprinting);
	ser.Value("m_playerStuntJumping", m_playerStuntJumping);
	ser.Value("m_playerStuntCloaking", m_playerStuntCloaking);
	ser.Value("m_playerStuntUncloaking", m_playerStuntUncloaking);
	ser.Value("m_stuntDir", m_stuntDir);
	ser.ValueWithDefault("m_mercyTimer", m_mercyTimer, -1.0f);

	ser.EndGroup();
}
