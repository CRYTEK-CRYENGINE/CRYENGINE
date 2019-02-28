// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIVehicle.h"

#include "GoalOp.h"
#include <CryPhysics/IPhysics.h>
#include <CrySystem/ISystem.h>

#include <CrySystem/IConsole.h>
#include <vector>
#include <algorithm>
#include <CryMath/Cry_Vector2.h>

#include <CryNetwork/ISerialize.h>

CAIVehicle::CAIVehicle() :
	m_bDriverInside(false),
	m_driverInsideCheck(-1),
	m_playerInsideCheck(-1),
	m_fNextFiringTime(GetAISystem()->GetFrameStartTime()),
	m_fFiringStartTime(GetAISystem()->GetFrameStartTime()),
	m_fFiringPauseTime(GetAISystem()->GetFrameStartTime()),
	m_bPoweredUp(false),
	m_ShootPhase(0),
	m_vDeltaTarget(ZERO)
{
	_fastcast_CAIVehicle = true;
	// can't reset now - no parameters are initialized yet
	//	Reset();

}

//
//---------------------------------------------------------------------------------------------------------
CAIVehicle::~CAIVehicle(void)
{

}

//
//---------------------------------------------------------------------------------------------------------
void CAIVehicle::UpdateDisabled(EUpdateType type)
{
	CAIActor::UpdateDisabled(type);
	m_bDryUpdate = type == EUpdateType::Dry;
	AlertPuppets();
	m_driverInsideCheck = -1;
	m_playerInsideCheck = -1;
}

//
//---------------------------------------------------------------------------------------------------------
void CAIVehicle::Update(EUpdateType type)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	CCCPOINT(CAIVehicle_Update);

	m_driverInsideCheck = -1;
	m_playerInsideCheck = -1;

	const SAIBodyInfo& bodyInfo = QueryBodyInfo();

	m_bDryUpdate = type == EUpdateType::Dry;

	// make sure to update direction when entity is not moved
	SetPos(bodyInfo.vEyePos);
	SetBodyDir(bodyInfo.GetBodyDir());
	SetEntityDir(bodyInfo.vEntityDir);
	SetMoveDir(bodyInfo.vMoveDir);

	static bool doDryUpdateCall = true;
	AlertPuppets();

	UpdateHealthTracking();
	m_damagePartsUpdated = false;

	if (!m_bDryUpdate)
	{
		CRY_PROFILE_REGION(PROFILE_AI, "AI system vehicle full update");

		CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
		if (m_fLastUpdateTime.GetSeconds() > 0.0f)
			m_fTimePassed = min(0.5f, (fCurrentTime - m_fLastUpdateTime).GetSeconds());
		else
			m_fTimePassed = 0;

		m_fLastUpdateTime = fCurrentTime;

		m_State.Reset();

		GetStateFromActiveGoals(m_State);

		UpdatePuppetInternalState();
	}
	else if (doDryUpdateCall)
	{
		// if approaching then always update
		for (size_t i = 0; i < m_vActiveGoals.size(); i++)
		{
			QGoal& Goal = m_vActiveGoals[i];
			Goal.pGoalOp->ExecuteDry(this);
		}
	}

	//--------------------------------------------------------
	// Orient towards the attention target always
	CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();

	Navigate(pAttentionTarget);

	// ---------- update proxy object
	if (pAttentionTarget)
	{
		m_State.nTargetType = pAttentionTarget->GetType();
		m_State.bTargetEnabled = pAttentionTarget->IsEnabled();
	}
	else
	{
		m_State.nTargetType = -1;
		m_State.bTargetEnabled = false;
	}

	FireCommand();

	m_vLastMoveDir = m_State.vMoveDir;
	if (gAIEnv.CVars.UpdateProxy)
	{
		GetProxy()->Update(m_State, !m_bDryUpdate);
		UpdateAlertness();
	}
}

float CAIVehicle::RecalculateAccuracy(void)
{

	float fAccuracy = m_Parameters.m_fAccuracy;

	if (fAccuracy > 1.f)
		fAccuracy = 1.f;
	else if (fAccuracy < 0)
		fAccuracy = 0;

	return fAccuracy;

}
Vec3 CAIVehicle::PredictMovingTarget(const CAIObject* pTarget, const Vec3& vTargetPos, const Vec3& vFirePos, const float duration, const float distpred)
{
	Vec3 vError(ZERO);

	// if we need a prediction of the target
	if (pTarget)
	{
		pe_status_dynamics dSt;
		GetProxy()->GetPhysics()->GetStatus(&dSt);
		IPhysicalEntity* pPhys = pTarget->GetPhysics();
		if (pPhys)
		{
			pPhys->GetStatus(&dSt);
			if (GetSubType() == CAIObject::STP_HELI)
			{
				vError = dSt.v * duration;
			}
			else
			{
				Vec3 vPrediction = vTargetPos + dSt.v * duration;
				Vec3 vTmp = vPrediction - vFirePos;
				if (distpred > 0.0f)
				{
					float len = vTmp.GetLength();
					vError.z = len / distpred;
				}
			}

		}
	}
	return vError;
}

Vec3 CAIVehicle::GetError(const Vec3& vTargetPos, const Vec3& vFirePos, const float fAccuracy)
{
	//	add an error to the target position depending on accuracy
	Vec3 vError(ZERO);

	if (fAccuracy < 1.0f)
	{
		float dist = (vTargetPos - vFirePos).GetLength();
		float fAccuracyScale = (1.0f - fAccuracy);
		float rangeRotation = fAccuracyScale * cry_random(-50.0f, 50.0f);
		float zofs = dist * sinf(DEG2RAD(0.0f));

		float rangeLength;
		if (cry_random(0.0f, 0.99f) < fAccuracy)
			rangeLength = fAccuracyScale * cry_random(0.0f, 27.0f);
		else
			rangeLength = fAccuracyScale * cry_random(15.0f, 25.0f);

		vError = vFirePos - vTargetPos;
		vError.z = 0.0f;
		vError.NormalizeSafe();
		vError *= rangeLength;
		vError *= min(1.5f, dist / 100.0f);

		Matrix33 rotmatZ = Matrix33::CreateRotationZ(DEG2RAD(rangeRotation));
		vError = rotmatZ * vError;

		vError = vError + vTargetPos;
		vError.z = gEnv->p3DEngine->GetTerrainElevation(vError.x, vError.y);
		vError = vError - vTargetPos;
		vError.z -= zofs;
	}

	return vError;

}

bool CAIVehicle::CheckExplosion(const Vec3& vTargetPos, const Vec3& vFirePos, const Vec3& vActuallFireDir, const float fDamageRadius)
{
	// get the hit point of a bullet, then check if there is a friend around the hit point.
	// not to give a damage to same species.

	const Vec3 vDirVector = vTargetPos - vFirePos;
	float fDamageRadius2 = fDamageRadius > 0.f ? fDamageRadius * fDamageRadius : 1.0f;

	Vec3 vHitPoint;

	PhysSkipList skipList;
	GetPhysicalSkipEntities(skipList);

	ray_hit hit;

	if (gAIEnv.pWorld->RayWorldIntersection(
	      vFirePos, vDirVector, COVER_OBJECT_TYPES,
	      AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER,
	      &hit, 1, &skipList[0], skipList.size()))
	{
		//When the bullet will hit a object which is on the way.
		vHitPoint = hit.pt;
	}
	else
	{
		//When the bullet will hit the player.
		vHitPoint = vTargetPos;
	}

	CAIObject* pTarget = 0;
	bool bNoFriendsInRadius = GetEnemyTarget(AIOBJECT_ACTOR, vHitPoint, fDamageRadius2, &pTarget);
	if (bNoFriendsInRadius && !pTarget)
		bNoFriendsInRadius = GetEnemyTarget(AIOBJECT_PLAYER, vHitPoint, fDamageRadius2, &pTarget);
	if (bNoFriendsInRadius && !pTarget)
		bNoFriendsInRadius = GetEnemyTarget(AIOBJECT_VEHICLE, vHitPoint, fDamageRadius2, &pTarget);

	Vec3 vActualDir = vActuallFireDir;
	vActualDir.SetLength(vDirVector.GetLength());

	if (GetSubType() != CAIObject::STP_HELI)
	{
		// if will hit the terrain/static and there is no friend
		if (bNoFriendsInRadius == true)
		{
			if (gAIEnv.pWorld->RayWorldIntersection(
			      vFirePos, vActualDir, COVER_OBJECT_TYPES,
			      AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER,
			      &hit, 1, &skipList[0], skipList.size()))
			{
				// and if it is far away from the target.
				if ((vTargetPos - hit.pt).GetLength() > fDamageRadius * 3.0f)
					// shouldn't fire
					bNoFriendsInRadius = false;

			}
		}
	}
	else
	{
		if (bNoFriendsInRadius == true)
		{
			if (gAIEnv.pWorld->RayWorldIntersection(
			      vFirePos, vActualDir, COVER_OBJECT_TYPES,
			      AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER,
			      &hit, 1, &skipList[0], skipList.size()))
			{
				if ((vFirePos - hit.pt).GetLength() < fDamageRadius * 10.0f)
					bNoFriendsInRadius = false;
			}
		}
	}

	return bNoFriendsInRadius;

}

// decides whether fire or not
void CAIVehicle::FireCommand(void)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// basic filters

	m_State.fire = eAIFS_Off;
	m_State.aimTargetIsValid = false; //vAimTargetPos.Set(0,0,0);

	if (!AllowedToFire())
	{
		m_ShootPhase = 0;
		m_fFiringStartTime = GetAISystem()->GetFrameStartTime();
		return;
	}

	CAIObject* pFireTarget = GetFireTargetObject();

	if (!pFireTarget)
		return;

	if (!GetProxy())
		return;

	// basic paramaters
	// m_CurrentWeaponDescriptor is not correct now these paramters below should be given by Descriptor
	// 4/8/2006 Tetsuji

	bool bCoaxial = (m_fireMode == FIREMODE_SECONDARY);
	bool bAAA = (m_fireMode == FIREMODE_CONTINUOUS && GetSubType() == CAIObject::STP_CAR);
	float fDamageRadius = bCoaxial ? 0.5f : 15.0f;
	float fAccuracy = RecalculateAccuracy();

	const SAIBodyInfo& bodyInfo = GetBodyInfo();

	Vec3 vActualFireDir = bodyInfo.vFireDir;
	Vec3 vFirePos = bodyInfo.vFirePos;
	Vec3 vFwdDir = bodyInfo.vMoveDir;
	Vec3 vMyPos = bodyInfo.vEyePos;

	if (GetEntity())
	{
		Matrix33 worldRotMat(GetEntity()->GetWorldTM());
		Vec3 vFwdOrg(0.0f, 1.0f, 0.0f);
		vFwdDir = worldRotMat * vFwdOrg;
		//		gEnv->pAISystem->AddDebugLine( vFirePos, vFirePos + vFwdDir * 20.0f , 255, 255, 255, 1.0f);
	}

	IPhysicalEntity* pPhys = GetPhysics();
	if (pPhys)
	{
		pe_status_pos my_status;
		pPhys->GetStatus(&my_status);
		vMyPos = my_status.pos;
	}

	Vec3 vTargetPos;
	{
		IPhysicalEntity* pAttPhys = pFireTarget->GetPhysics();
		if (pAttPhys)
		{
			pe_status_pos target_status;
			pAttPhys->GetStatus(&target_status);
			vTargetPos = target_status.pos;
			Vec3 ofs = target_status.BBox[0] + target_status.BBox[1];
			ofs.x *= 0.5f;
			ofs.y *= 0.5f;
			ofs.z *= 0.25f;
			vTargetPos += ofs;
		}
		else
		{
			vTargetPos = pFireTarget->GetPos();
		}
	}

	float targetHeight = vTargetPos.z - gEnv->p3DEngine->GetTerrainElevation(vTargetPos.x, vTargetPos.y);
	float fireHeight = vFirePos.z - gEnv->p3DEngine->GetTerrainElevation(vFirePos.x, vFirePos.y);

	// For the helicopter/vtol missiles
	if (GetSubType() == CAIObject::STP_HELI)
	{

		if (m_ShootPhase == 0)
		{
			fDamageRadius = 2.0f;
			m_vDeltaTarget = GetError(vTargetPos, vFirePos, fAccuracy);
			m_vDeltaTarget += PredictMovingTarget(pFireTarget, vTargetPos, vFirePos, 1.0f, 0.0f);
			m_vDeltaTarget += vTargetPos;

			Vec3 vTargetDir = m_vDeltaTarget - vFirePos;
			Vec3 vUnitTargetDir = vTargetDir;
			Vec3 vNormalizedTargetDir = vTargetDir;

			vNormalizedTargetDir.NormalizeSafe();
			float distanceToTheTarget = vTargetDir.GetLength();
			// If the target is not in front of him. can't fire.

			vUnitTargetDir.NormalizeSafe();
			//			gEnv->pAISystem->AddDebugLine( vFirePos, vFirePos + vUnitTargetDir * 20.0f , 255, 0, 0, 1.0f);

			float fDifference = vUnitTargetDir.Dot(vFwdDir);
			if (fDifference < cos_tpl(DEG2RAD(30.0f)))
			{
				m_fFiringStartTime = GetAISystem()->GetFrameStartTime();
				return;
			}
			// if a missile pass by near a target
			Vec3 vTmp = vFwdDir.Cross(vTargetDir);

			float d = vTmp.GetLength();
			float dot = vNormalizedTargetDir.Dot(vFwdDir);

			if (d > 12.0f || (dot > DEG2RAD(10.0f) && d > 10.0f))
			{
				m_fFiringStartTime = GetAISystem()->GetFrameStartTime();
				return;
			}

			// it also need to be vertical to a wing vector.
			// this check is needed because firing position is not accurate.26/06/2006 tetsuji
			// check if there is the same specie around a target position

			if (m_fireMode != FIREMODE_FORCED)
			{
				if (CheckExplosion(m_vDeltaTarget, vFirePos, vUnitTargetDir, fDamageRadius) == false)
				{
					m_fFiringStartTime = GetAISystem()->GetFrameStartTime();
					return;
				}
				else
				{
					m_ShootPhase = 1;
				}
			}
			else
				m_ShootPhase = 1;

		}
		if (m_ShootPhase)
		{
			// finalize a result and check the time.
			CTimeValue firingDuration;

			firingDuration.SetSeconds(0.0f);
			firingDuration += m_fFiringStartTime;
			if (GetAISystem()->GetFrameStartTime() < firingDuration)
			{
				return;
			}

			m_State.vLookTargetPos = m_vDeltaTarget;
			m_State.vShootTargetPos = m_vDeltaTarget;
			m_State.vAimTargetPos = m_vDeltaTarget;
			m_State.aimTargetIsValid = true;
			m_State.fire = eAIFS_On;
			m_ShootPhase = 0;

		}
		return;
	}

	// For the warrior,
	if (fireHeight > 10.0f)
	{

		if (GetAISystem()->GetFrameStartTime() < m_fNextFiringTime)
		{
			m_State.fire = eAIFS_On;
		}
		else
		{
			m_State.vLookTargetPos = vTargetPos;
			m_State.vAimTargetPos = vTargetPos;
			m_State.aimTargetIsValid = true;
			m_State.vShootTargetPos = vTargetPos;
			m_State.fire = eAIFS_On;
			CTimeValue firingDuration;
			firingDuration.SetSeconds(3.0f);
			firingDuration += m_fFiringStartTime;
			m_fNextFiringTime = firingDuration;
		}

		return;
	}

	// For tank Coaxial Gun
	if (bCoaxial == true || bAAA == true)
	{

		Vec3 vTargetDir = vTargetPos - vFirePos;
		//After the rotation of the tunnet has been completed,
		float fDuration = 2.0f;
		float fBoxRange = 3.0f;
		float fMaxDot = 30.0f;
		if (bAAA == true)
		{
			fDuration = 2.0f;
			fBoxRange = 30.0f;
			fMaxDot = 30.0f;
			PredictMovingTarget(pFireTarget, vTargetPos, vFirePos, 1.0f, 30.0f);
			if (vTargetDir.GetLength() < 20.0f)
			{
				m_State.vLookTargetPos = vTargetPos;
				m_State.vAimTargetPos = vTargetPos;
				m_State.aimTargetIsValid = true;
				m_State.vShootTargetPos = vTargetPos;
				return;
			}
		}
		else
		{

			Vec3 vTargetDirFromCenter = vTargetPos - vMyPos;
			Vec3 vTargetDirFromFirePos = vTargetPos - vFirePos;
			Vec3 vCenterToFirePos = vFirePos - vMyPos;
			vTargetDirFromCenter.z = vTargetDirFromFirePos.z = vCenterToFirePos.z = 0.0f;
			float distanceFromCenter = vTargetDirFromCenter.GetLength();
			float distanceFromFirePos = vTargetDirFromFirePos.GetLength();
			float distanceFromCenterToFire = vCenterToFirePos.GetLength();
			fDuration = 4.0f;
			fBoxRange = 5.0f;
			fMaxDot = (distanceFromCenter < 15.0f) ? 120.0f : 15.0f;
			if (distanceFromCenter < distanceFromCenterToFire + 1.5f)
			{
				return;
			}
		}

		Vec3 vTmp = vActualFireDir.Cross(vTargetDir);
		float d = vTmp.GetLength();

		vTargetDir.NormalizeSafe();
		float inner = vActualFireDir.Dot(vTargetDir);

		bool allowFiring = true;

		//if there is still a big difference between acutal fire direction and ideal fire direction,
		if (d > fBoxRange || inner < cos_tpl(DEG2RAD(fMaxDot)) || GetAISystem()->GetFrameStartTime() >= m_fNextFiringTime)
		{
			m_fNextFiringTime.SetSeconds(fDuration);
			m_fNextFiringTime += GetAISystem()->GetFrameStartTime();

			m_State.vLookTargetPos = vTargetPos;
			m_State.vAimTargetPos = vTargetPos;
			m_State.aimTargetIsValid = true;
			m_State.vShootTargetPos = vTargetPos;
			allowFiring = false;
		}
		else
		{
			if (bAAA != true)
			{
				vTargetPos = pFireTarget->GetPos();
				vTargetDir = vTargetPos - vFirePos;
				vTargetPos.z -= 0.3f;
				m_State.vLookTargetPos = vTargetPos;
				m_State.vAimTargetPos = vTargetPos;
				m_State.aimTargetIsValid = true;
				m_State.vShootTargetPos = vTargetPos;
			}
		}

		UpdateTargetTracking(GetWeakRef(pFireTarget), m_State.vAimTargetPos);

		if (allowFiring && GetAISystem()->GetFrameStartTime() < m_fNextFiringTime)
		{
			if (AdjustFireTarget(pFireTarget, m_State.vShootTargetPos, CanDamageTarget(), 0.5f,
			                     DEG2RAD(30.0f), &m_State.vShootTargetPos))
				m_State.fire = eAIFS_On;
		}

		return;
	}

	// For the tank cannon
	// in this part, we wait until the turret is moving then shoot
	{

		if (m_ShootPhase == 0)      //aiming set up
		{
			if (targetHeight < 10.0f)
			{
				IPhysicalEntity* pAttPhys = pFireTarget->GetPhysics();
				if (pAttPhys)
				{
					pe_status_dynamics dSt;
					pAttPhys->GetStatus(&dSt);
				}

				m_vDeltaTarget = GetError(vTargetPos, vFirePos, fAccuracy);
				m_vDeltaTarget += PredictMovingTarget(pFireTarget, vTargetPos, vFirePos, 1.0f, 0.0f);
				m_vDeltaTarget += vTargetPos;

			}
			m_ShootPhase++;

			CTimeValue tValue;
			tValue.SetSeconds(5.0f);
			m_fFiringPauseTime = GetAISystem()->GetFrameStartTime() + tValue;

		}
		else if (m_ShootPhase == 1)     //aiming ( the turret is rotating )
		{
			m_State.vLookTargetPos = m_State.vAimTargetPos = m_State.vShootTargetPos = m_vDeltaTarget;

			//After the rotation of the tunnet has been completed,
			Vec3 vTargetDir = m_vDeltaTarget - vFirePos;

			vTargetDir.NormalizeSafe();
			vActualFireDir.NormalizeSafe();

			float inner = vActualFireDir.Dot(vTargetDir);
			float cosval = cos_tpl(DEG2RAD(3.0f));
			if (inner > cosval || m_fFiringPauseTime < GetAISystem()->GetFrameStartTime())
			{
				CTimeValue tValue;
				tValue.SetSeconds(0.5f);
				m_fFiringPauseTime = GetAISystem()->GetFrameStartTime() + tValue;
				m_ShootPhase++;
			}
		}
		else if (m_ShootPhase == 2)      //fire ( the turret is fixed )
		{
			m_State.vLookTargetPos = m_State.vAimTargetPos = m_State.vShootTargetPos = m_vDeltaTarget;
			if (m_fFiringPauseTime < GetAISystem()->GetFrameStartTime())
			{
				CTimeValue tValue;
				tValue.SetSeconds(0.5f);
				m_fFiringPauseTime = GetAISystem()->GetFrameStartTime() + tValue;
				m_ShootPhase++;
			}

		}
		else if (m_ShootPhase == 3)      //fire ( the turret is fixed )
		{
			m_State.vLookTargetPos = m_State.vAimTargetPos = m_State.vShootTargetPos = vActualFireDir * 10.0f + vFirePos; //fix the turret

			CTimeValue tValue;
			tValue.SetSeconds(3.0f);

			if (m_fFiringPauseTime < GetAISystem()->GetFrameStartTime())
			{
				if (CheckTargetInRange(m_vDeltaTarget) && m_fFiringPauseTime + tValue > GetAISystem()->GetFrameStartTime())
				{
					Vec3 vTargetDir = m_vDeltaTarget - vFirePos;
					Vec3 vTmp = vActualFireDir.Cross(vTargetDir);
					float d = vTmp.GetLength();
					float distanceToTheTarget = vTargetDir.GetLength();
					vTargetDir.NormalizeSafe();
					float inner = vActualFireDir.Dot(vTargetDir);

					if (d < 30.0f && inner > cos_tpl(DEG2RAD(30.0f)))
					{
						if (m_fireMode == FIREMODE_FORCED || CheckExplosion(vActualFireDir * distanceToTheTarget + vFirePos, vFirePos, vActualFireDir, fDamageRadius) == true)
						{
							m_State.fire = eAIFS_On;
							return;
						}

					}
					else
					{
						tValue.SetSeconds(3.0f);
					}
				}
				else
				{
					m_ShootPhase = 0;
				}

			}

		}
	}
}

//
//---------------------------------------------------------------------------------------------------------
void CAIVehicle::ParseParameters(const AIObjectParams& params, bool bParseMovementParams)
{
	CPuppet::ParseParameters(params, bParseMovementParams);   // calls also CAIActor.ParseParams

	m_Parameters = params.m_sParamStruct;
}

//
//---------------------------------------------------------------------------------------------------------
EntityId CAIVehicle::GetPerceivedEntityID() const
{
	// Use the driver if one is present
	EntityId driverId = GetDriverEntity();
	if (driverId != 0)
		return driverId;

	return CPuppet::GetPerceivedEntityID();
}

//
//---------------------------------------------------------------------------------------------------------
//
void CAIVehicle::Reset(EObjectResetType type)
{
	CPuppet::Reset(type);

	if (type == AIOBJRESET_INIT)
		SetObserver(false); // TODO(Márcio): clean this up

	CAISystem* pAISystem = GetAISystem();

	m_fNextFiringTime = pAISystem->GetFrameStartTime();
	m_fFiringStartTime = pAISystem->GetFrameStartTime();
	m_fFiringPauseTime = pAISystem->GetFrameStartTime();
	m_bPoweredUp = false;
	m_ShootPhase = 0;

	m_bDriverInside = false;
	m_vDeltaTarget.zero();
}

//
//---------------------------------------------------------------------------------------------------------
//
void CAIVehicle::Event(unsigned short eType, SAIEVENT* pEvent)
{
	CAISystem* pAISystem = GetAISystem();
	switch (eType)
	{
	case AIEVENT_AGENTDIED:
		m_bEnabled = false;
		pAISystem->NotifyEnableState(this, m_bEnabled);
		pAISystem->UpdateGroupStatus(GetGroupId());

		//SetObserver(false);
		OnDriverChanged(false);

		m_State.ClearSignals();

		break;
	case AIEVENT_DRIVER_IN:
	case AIEVENT_DRIVER_OUT:
		{
			const bool bEntered = (eType == AIEVENT_DRIVER_IN);
			OnDriverChanged(bEntered);
			if (pEvent && pEvent->bSetObserver)
			{
				SetObserver(bEntered);
			}
		}
		return;     // those are vehicle specific, don't pass it to puppet
	case AIEVENT_DISABLE:
		m_bEnabled = false;
		pAISystem->NotifyEnableState(this, m_bEnabled);
		pAISystem->UpdateGroupStatus(GetGroupId());

		//SetObserver(false);

		OnDriverChanged(false);
		return;
		break;
	// can not just enable vehicles - it has to have driver in
	case AIEVENT_ENABLE:
		if (m_bDriverInside)
			Event(AIEVENT_DRIVER_IN, pEvent);

		//SetObserver(true);
		OnDriverChanged(m_bDriverInside);
		return;
		break;
	default:
		break;
	}

	CPuppet::Event(eType, pEvent);
}

//
//---------------------------------------------------------------------------------------------------------
void CAIVehicle::OnDriverChanged(bool bEntered)
{
	CAISystem* pAISystem = GetAISystem();
	assert(pAISystem);

	m_bDriverInside = bEntered;
	m_bEnabled = bEntered;

	pAISystem->NotifyEnableState(this, m_bEnabled);
	pAISystem->UpdateGroupStatus(GetGroupId());

	// TODO This was called before when receiving the AIEVENT_DRIVER_OUT event. Check if this
	// needs to be executed still. I suspect not. (Kevin)
	/*if (!bEntered)
	   {
	   gAIEnv.pAIObjectManager->RemoveObjectFromAllOfType(AIOBJECT_ACTOR,this);
	   gAIEnv.pAIObjectManager->RemoveObjectFromAllOfType(AIOBJECT_VEHICLE,this);
	   gAIEnv.pAIObjectManager->RemoveObjectFromAllOfType(AIOBJECT_ATTRIBUTE,this);
	   }*/
}

//
//---------------------------------------------------------------------------------------------------------
void CAIVehicle::Navigate(CAIObject* pTarget)
{
	CCCPOINT(CAIVehicle_Navigate);

	m_State.vForcedNavigation = m_vForcedNavigation;
	m_State.fForcedNavigationSpeed = m_fForcedNavigationSpeed;

	if (m_bLooseAttention)
	{
		CAIObject* pLooseAttentionTarget = m_refLooseAttentionTarget.GetAIObject();
		if (pLooseAttentionTarget)
			pTarget = pLooseAttentionTarget;
		else
			pTarget = NULL;
	}

	Vec3 vDir, vTargetPos;
	float fTime = GetAISystem()->GetFrameDeltaTime();
	//	int TargetType = AIOBJECT_NONE;

	if (pTarget)
	{
		const SAIBodyInfo& bodyInfo = GetBodyInfo();

		vTargetPos = pTarget->GetPos();
		vDir = vTargetPos - bodyInfo.vFirePos;

		if (GetSubType() == CAIObject::STP_HELI)
		{
			// (MATT) I'm very dubious about this code {2009/02/04}
			if (m_refLooseAttentionTarget.IsValid())
				m_State.vLookTargetPos = vTargetPos;
			else
				m_State.vLookTargetPos.zero();

		}
		else if (vDir.GetLength() > 5.0 && !AllowedToFire())
		{
			Vec3 check1(m_State.vLookTargetPos - bodyInfo.vFirePos);
			Vec3 check2(vTargetPos - bodyInfo.vEyePos);
			check1.NormalizeSafe();
			check2.NormalizeSafe();
			if (check1.Dot(check2) < cosf(DEG2RAD(5.0f)))
			{
				m_State.vLookTargetPos = m_State.vShootTargetPos = m_State.vAimTargetPos = bodyInfo.vEyePos + vDir * 10.f;
			}
		}
	}
	else
	{
		if (GetSubType() == CAIObject::STP_HELI)
		{
			m_State.vLookTargetPos.zero();
		}
		else if (!AllowedToFire())
		{
			const SAIBodyInfo& bodyInfo = GetBodyInfo();
			m_State.vLookTargetPos = m_State.vShootTargetPos = m_State.vAimTargetPos = bodyInfo.vEyePos + bodyInfo.vFireDir * 10.f;
			;
		}
	}
}

void CAIVehicle::AlertPuppets(void)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (GetSubType() != CAIObject::STP_CAR)
		return;

	if (!GetPhysics())
		return;

	ActorLookUp& lookUp = *gAIEnv.pActorLookUp;
	lookUp.Prepare(ActorLookUp::Position | ActorLookUp::Proxy);

	pe_status_dynamics dSt;
	GetPhysics()->GetStatus(&dSt);
	Vec3 v = dSt.v;
	v.z = 0;
	const float coeff = 0.4f;
	float fv = coeff * v.GetLength();

	if (fv < 0.5f)
		return;

	Vec3 vn(v);
	vn.Normalize();

	IEntity* pVehicleEntity = GetEntity();

	// Find the vehicle rectangle to avoid
	AABB localBounds;
	GetEntity()->GetLocalBounds(localBounds);
	const Matrix34& tm = GetEntity()->GetWorldTM();
	SAIRect3 r;
	GetFloorRectangleFromOrientedBox(tm, localBounds, r);
	// Extend the box based on velocity.
	Vec3 vel = dSt.v;
	const float lookAheadTime = 3.0f;
	float speedu = r.axisu.Dot(vel) * lookAheadTime;
	float speedv = r.axisv.Dot(vel) * lookAheadTime;
	if (speedu > 0)
		r.max.x += speedu;
	else
		r.min.x += speedu;
	if (speedv > 0)
		r.max.y += speedv;
	else
		r.min.y += speedv;

	if (IsPlayerInside())
	{
		r.min.y -= 1.0f;
		r.max.y += 1.0f;
	}
	float avoidOffset = (r.max.x - r.min.x) / 2 + 0.5f;

	static bool debugdraw = false;

	if (debugdraw)
	{
		const unsigned maxpts = 4;
		Vec3 rect[maxpts];
		rect[0] = r.center + r.axisu * r.min.x + r.axisv * r.min.y;
		rect[1] = r.center + r.axisu * r.max.x + r.axisv * r.min.y;
		rect[2] = r.center + r.axisu * r.max.x + r.axisv * r.max.y;
		rect[3] = r.center + r.axisu * r.min.x + r.axisv * r.max.y;
		unsigned last = maxpts - 1;
		for (unsigned i = 0; i < maxpts; ++i)
		{
			GetAISystem()->AddDebugLine(rect[last], rect[i], 255, 0, 0, 0.2f);
			last = i;
		}
	}

	const float distToPathThr = (r.max.x - r.min.x) / 2 + 0.5f;
	const float dangerRange = max(r.min.GetLength(), r.max.GetLength()) * 1.5f;
	const float dangerRangeSq = sqr(dangerRange);

	size_t activeCount = lookUp.GetActiveCount();

	for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
	{
		CAIActor* pAIActor = lookUp.GetActor<CAIActor>(actorIndex);
		CPuppet* pPuppet = 0;

		// Skip distant vehicles
		if (Distance::Point_Point2DSq(lookUp.GetPosition(actorIndex), r.center) > dangerRangeSq)
		{
			pPuppet = pAIActor->CastToCPuppet();
			if (!pPuppet || pPuppet->GetType() == AIOBJECT_VEHICLE)
				continue;

			if (pPuppet->GetAvoidedVehicle() == this)
			{
				if (pPuppet->GetVehicleAvoidingTime() > 3000)
				{
					CCCPOINT(CAIVehicle_AlertPuppets_SignalVehicle);
					pPuppet->SetAvoidedVehicle(NILREF);
					pPuppet->SetSignal(1, "OnEndVehicleDanger", GetEntity());
				}
			}

			continue;
		}
		else
		{
			pPuppet = pAIActor->CastToCPuppet();
			if (!pPuppet || pPuppet->GetType() == AIOBJECT_VEHICLE)
				continue;
		}

		assert(pPuppet);

		// Skip puppets in vehicles.
		if (lookUp.GetProxy(actorIndex)->GetLinkedVehicleEntityId())
			continue;

		bool bNearPath = false;
		Vec3 puppetDir = pPuppet->GetPos() - r.center;

		float x = r.axisu.Dot(puppetDir);
		float y = r.axisv.Dot(puppetDir);

		Vec3 pathPosOut(ZERO);
		float distToPath = 0;

		if (!m_Path.Empty())
		{
			float distAlongPath = 0;
			distToPath = m_Path.GetDistToPath(pathPosOut, distAlongPath, pPuppet->GetPos(), dangerRange, false);//false ->2D only
			if (distToPath <= distToPathThr + pPuppet->GetParameters().m_fPassRadius)
				bNearPath = true;
		}

		const float pr = pPuppet->GetParameters().m_fPassRadius;

		if (bNearPath || m_Path.Empty() && x >= (r.min.x - pr) && x <= (r.max.x + pr) && y >= (r.min.y - pr) && y <= (r.max.y + pr))
		{
			Vec3 move(ZERO);
			if (bNearPath && distToPath > 0.1f && Distance::Point_Point2DSq(pPuppet->GetPos(), pathPosOut) > sqr(0.1f))
			{
				Vec3 norm = pPuppet->GetPos() - pathPosOut;
				norm.z = 0;
				norm.Normalize();
				move = norm * (avoidOffset + 2.0f - distToPath);
			}
			else
			{
				move = r.axisv * (avoidOffset + 2.0f - fabsf(y));
				if (y < 0.0f)
					move = -move;
			}

			// Note: There used to be a CAISystem.IsPathWorth() check here, but it was removed because of the huge performance peaks.
			// Now instead the stick has larger end accuracy (that is, does not need to find exact target location).
			Vec3 avoidPos = pPuppet->GetPos() + move;
			if (debugdraw)
			{
				GetAISystem()->AddDebugSphere(pPuppet->GetPos() + Vec3(0, 0, 2), 0.25f, 255, 255, 255, 0.2f);
				GetAISystem()->AddDebugLine(pPuppet->GetPos(), avoidPos, 255, 255, 255, 0.2f);
				GetAISystem()->AddDebugLine(avoidPos - Vec3(0, 0, 10), avoidPos + Vec3(0, 0, 10), 255, 255, 255, 0.2f);
			}

			if (pPuppet->GetAvoidedVehicle() != this)
			{
				AISignalExtraData* pData = new AISignalExtraData;
				pData->point2 = avoidPos;
				pData->point = vn;
				pPuppet->SetSignal(1, "OnVehicleDanger", GetEntity(), pData);
			}
			pPuppet->SetAvoidedVehicle(GetWeakRef(this));

			// Update pos.
			CAIObject* pAvoidTarget = pPuppet->GetOrCreateSpecialAIObject(AISPECIAL_VEHICLE_AVOID_POS);
			pAvoidTarget->SetPos(avoidPos, vn);

		}
		else if (pPuppet->GetAvoidedVehicle() == this)
		{
			//check rotation around Z axis (2D)
			if (!(fabs(dSt.w.z) > 0.2f && fv > 0.7f)) // if vehicle is not spinning like a tank
			{
				if (pPuppet->GetVehicleAvoidingTime() > 3000)
				{
					pPuppet->SetAvoidedVehicle(NILREF);
					pPuppet->SetSignal(1, "OnEndVehicleDanger", GetEntity());
				}
			}
		}
	}
}

void CAIVehicle::Serialize(TSerialize ser)
{
	CPuppet::Serialize(ser);
	ser.BeginGroup("AIVehicle");
	{
		ser.Value("m_bDriverInside", m_bDriverInside);
		ser.Value("m_bPoweredUp", m_bPoweredUp);
		ser.Value("m_ShootPhase", m_ShootPhase);
		ser.Value("m_vDeltaTarget", m_vDeltaTarget);

		if (ser.IsReading())
		{
			CAISystem* pAISystem = GetAISystem();

			m_fNextFiringTime = pAISystem->GetFrameStartTime();
			m_fFiringStartTime = pAISystem->GetFrameStartTime();
			m_fFiringPauseTime = pAISystem->GetFrameStartTime();
		}
	}
	ser.EndGroup();
}

bool CAIVehicle::GetEnemyTarget(int objectType, Vec3& hitPosition, float fDamageRadius2, CAIObject** pTarget)
{
	*pTarget = NULL;

	AIObjectOwners::iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(objectType);
	AIObjectOwners::iterator aiend = gAIEnv.pAIObjectManager->m_Objects.end();

	while (ai != aiend)
	{
		// (MATT)	Strong, so always valid {2009/03/25}
		CAIObject* pObject = ai->second.GetAIObject();
		if (pObject->GetType() != objectType)
			break;
		if (pObject->IsEnabled())
		{
			Vec3 dir = pObject->GetPos() - hitPosition;
			float dist2 = dir.GetLengthSquared();
			if (dist2 < fDamageRadius2)
			{
				CAIActor* pActor = pObject->CastToCAIActor();
				if (pActor && IsHostile(pObject, false) == false)
				{
					if (GetEntityID() != pObject->GetEntityID())
					{
						*pTarget = NULL;
						return false;
					}
				}
				else
					*pTarget = pObject;
			}
		}
		++ai;
	}
	return true;
}

//
//------------------------------------------------------------------------------------------------------------------
bool CAIVehicle::HandleVerticalMovement(const Vec3& targetPos)
{

	if (m_bPoweredUp)
		return false;

	Vec3 myPos(GetPos());
	Vec3 diff(targetPos - GetPos());

	Vec3 diff2d(diff);
	diff2d.z = 0;
	float ratio = diff2d.len2() > 0 ? fabs(diff.z) / diff2d.len() : fabs(diff.z);
	if (0)
		if (ratio < 2.0f)
		{
			m_bPoweredUp = true;
			return false;
		}

	//	m_State.vMoveDir = diff;
	//	m_State.vMoveDir.normalize();
	m_State.vMoveDir = Vec3(0, 0, 1);
	float zScale = fabs(diff.z);
	if (zScale < 5 || diff.z < 0.0f)
	{
		m_bPoweredUp = true;
		return false;
	}
	m_State.fDesiredSpeed = 5.0f;
	return true;
}

//
//------------------------------------------------------------------------------------------------------------------
bool CAIVehicle::IsDriverInside() const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (m_bEnabled)
		return true;
	if (m_driverInsideCheck == -1)
		m_driverInsideCheck = GetDriverEntity() != 0 ? 1 : 0;
	return m_driverInsideCheck == 1;
}

//
//------------------------------------------------------------------------------------------------------------------
bool CAIVehicle::IsPlayerInside()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (m_bEnabled)
		return true;
	if (m_playerInsideCheck == -1)
	{
		CAIActor* pDriver = GetDriver();
		m_playerInsideCheck = (pDriver && pDriver->GetAIType() == AIOBJECT_PLAYER ? 1 : 0);
	}
	return m_playerInsideCheck == 1;
}

//
//------------------------------------------------------------------------------------------------------------------
EntityId CAIVehicle::GetDriverEntity() const
{
	IAIActorProxy* pProxy = GetProxy();

	return (pProxy ? pProxy->GetLinkedDriverEntityId() : 0);
}

//
//------------------------------------------------------------------------------------------------------------------
CAIActor* CAIVehicle::GetDriver() const
{
	CAIActor* pDriverActor = NULL;

	const EntityId driverId = GetDriverEntity();
	if (driverId > 0)
	{
		IEntity* pDriverEntity = gEnv->pEntitySystem->GetEntity(driverId);
		IAIObject* pDriverAI = pDriverEntity ? pDriverEntity->GetAI() : NULL;
		pDriverActor = CastToCAIActorSafe(pDriverAI);
	}

	return pDriverActor;
}

//===================================================================
// GetPathFollowerParams
//===================================================================
void CAIVehicle::GetPathFollowerParams(PathFollowerParams& outParams) const
{
	CPuppet::GetPathFollowerParams(outParams);

	outParams.endAccuracy = 1.5f;
	outParams.isVehicle = true;
}

//
//------------------------------------------------------------------------------------------------------------------
