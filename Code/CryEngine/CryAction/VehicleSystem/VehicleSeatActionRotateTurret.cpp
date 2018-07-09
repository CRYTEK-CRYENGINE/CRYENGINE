// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a seat action to rotate a turret

   -------------------------------------------------------------------------
   History:
   - 14:12:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"

#include "VehicleSeatActionRotateTurret.h"

#include "GameObjects/GameObject.h"
#include "IGameObject.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleSeat.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimated.h"

#include "VehicleSeatActionWeapons.h"

#include "PersistantDebug.h"
#include <CryRenderer/IRenderAuxGeom.h>

//------------------------------------------------------------------------
bool CVehicleSeatActionRotateTurret::Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table)
{
	m_pUserEntity = NULL;
	m_aimGoal.zero();
	m_aimGoalPriority = 0;

	m_rotTestHelpers[0] = NULL;
	m_rotTestHelpers[1] = NULL;
	m_rotTestRadius = 0.f;

	m_pVehicle = static_cast<CVehicle*>(pVehicle);
	m_pSeat = static_cast<CVehicleSeat*>(pSeat);
	assert(m_pSeat);

	CVehicleParams rotationTable = table.findChild("RotateTurret");
	if (!rotationTable)
		return false;

	// first the actual rotation setups
	if (CVehicleParams pitchTable = rotationTable.findChild("Pitch"))
	{
		InitRotation(pVehicle, pitchTable, eVTRT_Pitch);
		InitRotationSounds(pitchTable, eVTRT_Pitch);
	}
	if (CVehicleParams yawTable = rotationTable.findChild("Yaw"))
	{
		InitRotation(pVehicle, yawTable, eVTRT_Yaw);
		InitRotationSounds(yawTable, eVTRT_Yaw);
	}

	// then the (optional) rotation testing
	if (CVehicleParams rotationTestTable = rotationTable.findChild("RotationTest"))
	{
		if (rotationTestTable.haveAttr("helper1"))
		{
			const char* helpName = rotationTestTable.getAttr("helper1");
			if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(helpName))
				m_rotTestHelpers[0] = pHelper;
		}
		if (rotationTestTable.haveAttr("helper2"))
		{
			const char* helpName = rotationTestTable.getAttr("helper2");
			if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(helpName))
				m_rotTestHelpers[1] = pHelper;
		}

		rotationTestTable.getAttr("radius", m_rotTestRadius);
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::Reset()
{
	m_aimGoal.zero();
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::StartUsing(EntityId passengerId)
{
	m_pUserEntity = gEnv->pEntitySystem->GetEntity(passengerId);
	m_aimGoal.zero();

	IVehiclePart::SVehiclePartEvent partEvent;
	partEvent.type = IVehiclePart::eVPE_StartUsing;

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

	for (int i = 0; i < eVTRT_NumRotationTypes; ++i)
	{
		if (m_rotations[i].m_pPart)
		{
			m_rotations[i].m_relSpeed = 0.0f;
			m_rotations[i].m_prevWorldQuat = Quat(m_rotations[i].m_pPart->GetWorldTM());
			m_rotations[i].m_pPart->OnEvent(partEvent);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::StopUsing()
{
	m_pUserEntity = NULL;
	m_aimGoal.zero();

	IVehiclePart::SVehiclePartEvent partEvent;
	partEvent.type = IVehiclePart::eVPE_StopUsing;

	for (int i = 0; i < eVTRT_NumRotationTypes; ++i)
	{
		if (m_rotations[i].m_pPart)
		{
			m_rotations[i].m_pPart->OnEvent(partEvent);
			m_rotations[i].m_relSpeed = 0.f;
			m_pVehicle->StopSound(m_rotations[i].m_turnSoundId);
			m_pVehicle->StopSound(m_rotations[i].m_damageSoundId);
		}
	}

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	if (!m_aimGoal.IsZero())
	{
		// if seat action is used remotely, rotation is set through aim goal, thus return here
		if (m_pSeat && m_pSeat->GetCurrentTransition() == CVehicleSeat::eVT_RemoteUsage)
			return;
	}

	if (actionId == eVAI_RotatePitch && m_rotations[eVTRT_Pitch].m_pPart)
	{
		m_rotations[eVTRT_Pitch].m_action = -value;
	}
	else if (actionId == eVAI_RotateYaw && m_rotations[eVTRT_Yaw].m_pPart)
	{
		m_rotations[eVTRT_Yaw].m_action = -value;
	}

	if (actionId == eVAI_RotatePitchAimAssist && m_rotations[eVTRT_Pitch].m_pPart)
	{
		m_rotations[eVTRT_Pitch].m_aimAssist = value;
	}
	else if (actionId == eVAI_RotateYawAimAssist && m_rotations[eVTRT_Yaw].m_pPart)
	{
		m_rotations[eVTRT_Yaw].m_aimAssist = value;
	}

	if (m_pSeat)
		m_pSeat->ChangedNetworkState(CVehicle::ASPECT_PART_MATRIX);
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::Serialize(TSerialize ser, EEntityAspects aspects)
{
	// MR: for network, only turret parts are serialized
	// for savegame, all parts are serialized (by CVehicle)
	if (ser.GetSerializationTarget() == eST_Network)
	{
		for (int i = 0; i < eVTRT_NumRotationTypes; ++i)
		{
			if (m_rotations[i].m_pPart)
			{
				m_rotations[i].m_pPart->Serialize(ser, aspects);
			}
		}
	}
	else
	{
		// save rotation details
		CryFixedStringT<16> tag;
		for (int i = 0; i < eVTRT_NumRotationTypes; ++i)
		{
			if (m_rotations[i].m_pPart)
			{
				Quat q;
				Matrix34 currentTM = m_rotations[i].m_pPart->GetLocalBaseTM();
				if (ser.IsWriting())
					q = Quat(currentTM);

				tag = (i == eVTRT_Pitch) ? "rotation_pitch" : "rotation_yaw";
				ser.Value(tag.c_str(), q, 'ori1');

				if (ser.IsReading())
				{
					Matrix34 newTM(q);
					newTM.SetTranslation(currentTM.GetTranslation());
					m_rotations[i].m_pPart->SetLocalBaseTM(newTM);
					m_rotations[i].m_orientation.Set(q);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::Update(float frameTime)
{
	if (!m_pUserEntity)
	{
		DoUpdate(frameTime);
	}
}

void CVehicleSeatActionRotateTurret::UpdateFromPassenger(const float frameTime, EntityId playerId)
{
	if (m_pUserEntity && (m_pUserEntity->GetId() == playerId))
	{
		DoUpdate(frameTime);
	}
}

void CVehicleSeatActionRotateTurret::DoUpdate(const float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (gEnv->IsClient() && m_pVehicle->IsProbablyDistant() && !m_pVehicle->GetGameObject()->IsProbablyVisible())
		return;

	// AI use the aim goal to control their rotation (remote usage too)
	if (!m_aimGoal.IsZero())
	{
		UpdateAimGoal();
	}

	// Cache the helper position before applying any rotation
	IActor* pActor = m_pSeat->GetPassengerActor();
	bool checkRotation = (m_rotTestHelpers[0] && m_rotTestHelpers[1] && pActor);
	Vec3 oldHelperPos = checkRotation ? m_rotTestHelpers[1]->GetWorldSpaceTranslation() : Vec3(ZERO);

	Matrix34 oldMatrices[eVTRT_NumRotationTypes];

	for (int i = 0; i < eVTRT_NumRotationTypes; ++i)
	{
		if (m_rotations[i].m_pPart)
		{
			oldMatrices[i] = m_rotations[i].m_pPart->GetLocalBaseTM();
			UpdatePartRotation((EVehicleTurretRotationType)i, frameTime);
		}
	}

	// Check for turret collisions
	if (checkRotation)
	{
		// need to test the new rotations before applying them. Sweep a sphere between the two helpers and check for collisions...
		static IPhysicalEntity* pSkipEntities[10];
		int numSkip = m_pVehicle->GetSkipEntities(pSkipEntities, 10);
		primitives::sphere sphere;
		sphere.center = m_rotTestHelpers[0]->GetWorldSpaceTranslation();
		sphere.r = m_rotTestRadius;

		geom_contact* pContact = NULL;
		Vec3 dir = m_rotTestHelpers[1]->GetWorldSpaceTranslation() - sphere.center;
		float hit = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, dir, ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid, &pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEntities, numSkip);
		if (hit > 0.001f && pContact)
		{
			// there was a collision. check whether the barrel is moving towards the collision point or not... if not, ignore the collision.
#if ENABLE_VEHICLE_DEBUG
			if (VehicleCVars().v_debugdraw > 0)
			{
				CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
				pPD->Begin("VehicleCannon", false);

				ColorF col(1.0f, 0.0f, 0.0f, 1.0f);
				if (pContact && hit > 0.0f)
				{
					pPD->AddSphere(pContact->pt, 0.1f, col, 30.0f);
				}
			}
#endif
			Vec3 endPos = m_rotTestHelpers[1]->GetWorldSpaceTranslation();
			Vec3 moveDir = endPos - oldHelperPos;
			Vec3 hitDir = pContact->pt - oldHelperPos;

			if (moveDir.Dot(hitDir) > 0.0f)
			{
				// reset as though the rotation never happened.
				for (int i = 0; i < eVTRT_NumRotationTypes; ++i)
				{
					if (m_rotations[i].m_pPart)
					{
						CVehiclePartBase* pPart = m_rotations[i].m_pPart;
						pPart->SetLocalBaseTM(oldMatrices[i]);
						const Matrix34& worldTM = pPart->GetWorldTM();
						m_rotations[i].m_prevWorldQuat = Quat(worldTM);
						m_rotations[i].m_orientation.Set(Quat(Matrix33(oldMatrices[i])));
					}
				}
			}
		}
	}

	m_aimGoalPriority = 0;
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::UpdateAimGoal()
{
	Vec3 aimGoalLocal = m_pVehicle->GetEntity()->GetWorldTM().GetInverted() * m_aimGoal;

	IVehiclePart* pPitchPart = m_rotations[eVTRT_Pitch].m_pPart;
	if (pPitchPart)
	{
		Vec3 pitchPartToAimGoal = aimGoalLocal - pPitchPart->GetLocalTM(false).GetTranslation();

		Quat pitchAimDir = Quat::CreateRotationVDir(pitchPartToAimGoal.GetNormalizedSafe());
		Ang3 desiredPitchAngles(pitchAimDir);

		Ang3 currentPitchAngles(pPitchPart->GetLocalTM(false));
		m_rotations[eVTRT_Pitch].m_action = desiredPitchAngles.x - currentPitchAngles.x;
	}

	IVehiclePart* pYawPart = m_rotations[eVTRT_Yaw].m_pPart;
	if (pYawPart)
	{
		Vec3 yawPartToAimGoal = aimGoalLocal - pYawPart->GetLocalTM(false).GetTranslation();

		Quat yawAimDir = Quat::CreateRotationVDir(yawPartToAimGoal.GetNormalizedSafe());
		Ang3 desiredYawAngles(yawAimDir);

		Ang3 currentYawAngles(pYawPart->GetLocalTM(false));
		m_rotations[eVTRT_Yaw].m_action = fmod(desiredYawAngles.z - currentYawAngles.z + 3.0f * gf_PI, gf_PI2) - gf_PI;
	}
}

void CVehicleSeatActionRotateTurret::MaintainPartRotationWorldSpace(EVehicleTurretRotationType eType)
{
	CVehiclePartBase* pPart = m_rotations[eType].m_pPart;
	IVehiclePart* pParent = pPart->GetParent();
	IActor* pActor = m_pSeat->GetPassengerActor();

	bool remote = m_pSeat->GetCurrentTransition() == IVehicleSeat::eVT_RemoteUsage;
	bool worldSpace = m_rotations[eType].m_worldSpace && VehicleCVars().v_independentMountedGuns != 0;

	if (worldSpace && pParent && pActor && pActor->IsClient() && !remote)
	{
		// we want to keep the old worldspace rotation
		// therefore we're updating the local transform from it
		// NB: there is no need to clamp here, its done later

		Matrix34 localTM = pParent->GetWorldTM().GetInverted() * Matrix34(m_rotations[eType].m_prevWorldQuat);
		localTM.OrthonormalizeFast(); // precision issue

		const Matrix34& baseTM = pPart->GetLocalBaseTM();

		if (!Matrix34::IsEquivalent(baseTM, localTM))
		{
			Ang3 anglesCurr(baseTM);
			Ang3 angles(localTM);

			if (eType == eVTRT_Pitch)
			{
				angles.y = anglesCurr.y;
				angles.z = anglesCurr.z;
			}
			else if (eType == eVTRT_Yaw)
			{
				angles.x = anglesCurr.x;
				angles.y = anglesCurr.y;
			}

			localTM.SetRotationXYZ(angles);
			localTM.SetTranslation(baseTM.GetTranslation());
			pPart->SetLocalBaseTM(localTM);

			m_pSeat->ChangedNetworkState(CVehicle::ASPECT_PART_MATRIX);
		}

#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_debugdraw == eVDB_Parts)
		{
			float color[] = { 1, 1, 1, 1 };
			Ang3 a(localTM), aBase(baseTM);
			IRenderAuxText::Draw2dLabel(200, 200, 1.4f, color, false, "localAng: %.1f (real: %.1f)", RAD2DEG(a.z), RAD2DEG(aBase.z));
		}
#endif
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::UpdatePartRotation(EVehicleTurretRotationType eType, float frameTime)
{
	CRY_ASSERT(eType < eVTRT_NumRotationTypes);

	const float threshold = 0.01f;
	if (frameTime > 0.08f) frameTime = 0.08f;

	CVehiclePartBase* pPart = m_rotations[eType].m_pPart;
	IVehiclePart* pParent = pPart->GetParent();
	IActor* pActor = m_pSeat->GetPassengerActor();

	float rot_dir = fsgnf(m_rotations[eType].m_action);
	float max_rotation = fabsf(m_rotations[eType].m_action);
	float rot_speed = DEG2RAD(fabsf(m_rotations[eType].m_speed)) * GetDamageSpeedMul(pPart);

	float delta = rot_dir * rot_speed * frameTime;
	delta += m_rotations[eType].m_aimAssist;

	delta = fmod(delta, gf_PI2);
	if (delta > gf_PI)  delta -= gf_PI2;
	if (delta < -gf_PI) delta += gf_PI2;

	Limit(delta, -max_rotation, max_rotation);

	Ang3 deltaAngles(ZERO);
	if (eType == eVTRT_Pitch)
		deltaAngles.x = delta;
	else if (eType == eVTRT_Yaw)
		deltaAngles.z = delta;
	else
		CRY_ASSERT(false && "Unknown turret rotation");

	Matrix34 tm = pPart->GetLocalBaseTM();
	Ang3 angles = Ang3::GetAnglesXYZ(tm) + deltaAngles;

	float lerp = 0.f;
	if (eType == eVTRT_Pitch)
	{
		Vec3 yAxis = m_rotations[eVTRT_Yaw].m_pPart->GetLocalBaseTM().GetColumn1();
		yAxis.z = 0.f;
		yAxis.normalize();
		lerp = 0.5f - 0.5f * yAxis.y;
		Limit(lerp, 0.0f, 1.0f);
	}

	// clamp to limits
	if (m_rotations[eType].m_minLimitF != 0.0f || m_rotations[eType].m_maxLimit != 0.0f)
	{
		// Different clamp angles facing forwards/backwards
		float minLimit = m_rotations[eType].m_minLimitF + (m_rotations[eType].m_minLimitB - m_rotations[eType].m_minLimitF) * lerp;
		float angle = (eType == eVTRT_Pitch) ? angles.x : angles.z;
		if (angle > m_rotations[eType].m_maxLimit || angle < minLimit)
		{
			angle = clamp_tpl(angle, minLimit, m_rotations[eType].m_maxLimit);
			m_rotations[eType].m_currentValue = 0.f;

			if (eType == eVTRT_Pitch)
				angles.x = angle;
			else
				angles.z = angle;
		}
	}

	m_rotations[eType].m_orientation.Set(Quat::CreateRotationXYZ(angles));
	m_rotations[eType].m_orientation.Update(frameTime);

	m_rotations[eType].m_action = 0.0f;
	m_rotations[eType].m_aimAssist = 0.0f;

	Matrix34 newTM(m_rotations[eType].m_orientation.Get().GetNormalized());
	newTM.SetTranslation(tm.GetTranslation());
	pPart->SetLocalBaseTM(newTM);

	// store world-space rotation
	const Matrix34& worldTM = pPart->GetWorldTM();
	m_rotations[eType].m_prevWorldQuat = Quat(worldTM);
	CRY_ASSERT(m_rotations[eType].m_prevWorldQuat.IsValid());

	// now update the turret sound based on the calculated rotation speed
	UpdateRotationSound(eType, delta, frameTime);

}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::SetAimGoal(Vec3 aimPos, int priority)
{
	if (m_aimGoalPriority <= priority)
	{
		m_aimGoal = aimPos;
		m_aimGoalPriority = priority;
	}
}

//------------------------------------------------------------------------
bool CVehicleSeatActionRotateTurret::GetRemainingAnglesToAimGoalInDegrees(float& pitch, float& yaw)
{
	// no aim goal set (or it got cleared)?
	if (m_aimGoal.IsZero())
	{
		return false; // have no aim goal
	}

	IVehiclePart* pPitchPart = m_rotations[eVTRT_Pitch].m_pPart;
	IVehiclePart* pYawPart = m_rotations[eVTRT_Yaw].m_pPart;

	if (!pYawPart)
	{
		pitch = yaw = 0.0f;
		return true;  // have an aim goal
	}

	// aim goal is a world pos. Convert it to vehicle space:
	Vec3 aimGoalLocal = m_pVehicle->GetEntity()->GetWorldTM().GetInverted() * m_aimGoal;

	// direction from yaw part pivot to aim goal
	Vec3 yawPartToAimGoal = aimGoalLocal - pYawPart->GetLocalTM(false).GetTranslation();

	// angles from yaw part to aim goal
	Quat aimDir = Quat::CreateRotationVDir(yawPartToAimGoal.GetNormalizedSafe());
	Ang3 desiredAngles(aimDir);

	if (pPitchPart)
	{
		Ang3 pitchAngles(pPitchPart->GetLocalTM(false));
		pitch = RAD2DEG(desiredAngles.x - pitchAngles.x);
		pitch = fmod(pitch, 360.0f);
	}
	else
	{
		pitch = 0.0f;
	}

	Ang3 yawAngles(pYawPart->GetLocalTM(false));
	yaw = RAD2DEG(desiredAngles.z - yawAngles.z);
	yaw = fmod(yaw, 360.0f);

	return true;  // have an aim goal
}

//------------------------------------------------------------------------
float CVehicleSeatActionRotateTurret::GetDamageSpeedMul(CVehiclePartBase* pPart)
{
	// slowdown by max 50%, starting from 75% damage
	return 1.0f - 2.f * max(min(1.f, pPart->m_damageRatio) - 0.75f, 0.f);
}

//------------------------------------------------------------------------
bool CVehicleSeatActionRotateTurret::InitRotation(IVehicle* pVehicle, const CVehicleParams& rotationTable, EVehicleTurretRotationType eType)
{
	if (rotationTable)
	{
		if (rotationTable.haveAttr("part"))
			m_rotations[eType].m_pPart = static_cast<CVehiclePartBase*>(m_pVehicle->GetPart(rotationTable.getAttr("part")));

		if (rotationTable.getAttr("speed", m_rotations[eType].m_speed) && m_rotations[eType].m_pPart)
		{
			m_rotations[eType].m_pPart->SetMoveable();

			rotationTable.getAttr("accel", m_rotations[eType].m_acceleration);

			if (CVehicleParams limitsTable = rotationTable.findChild("Limits"))
			{
				// Forward facing limits
				if (limitsTable.getChildCount() >= 2)
				{
					if (CVehicleParams limitRef = limitsTable.getChild(0))
						m_rotations[eType].m_minLimitF = (float)DEG2RAD((float)atof(limitRef.getAttr("value")));
					else
						m_rotations[eType].m_minLimitF = 0.0f;

					if (CVehicleParams limitRef = limitsTable.getChild(1))
						m_rotations[eType].m_maxLimit = (float)DEG2RAD((float)atof(limitRef.getAttr("value")));
					else
						m_rotations[eType].m_maxLimit = 0.0f;
				}

				// Backwards facing limits
				m_rotations[eType].m_minLimitB = m_rotations[eType].m_minLimitF;
				if (limitsTable.getChildCount() >= 3)
				{
					if (CVehicleParams limitRef = limitsTable.getChild(2))
						m_rotations[eType].m_minLimitB = (float)DEG2RAD((float)atof(limitRef.getAttr("value")));
				}
			}
		}
		rotationTable.getAttr("worldSpace", m_rotations[eType].m_worldSpace);
	}

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeatActionRotateTurret::InitRotationSounds(const CVehicleParams& rotationParams, EVehicleTurretRotationType eType)
{
	CVehicleParams sound = rotationParams.findChild("Sound");
	if (!sound)
		return false;

	if (sound.haveAttr("event"))
	{
		if (string helperName = sound.getAttr("helper"))
		{
			if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(helperName))
			{
				SVehicleSoundInfo info;
				info.name = sound.getAttr("event");
				info.pHelper = pHelper;
				m_rotations[eType].m_turnSoundId = m_pVehicle->AddSoundEvent(info);

				if (sound.haveAttr("eventDamage"))
				{
					SVehicleSoundInfo dmgInfo;
					info.name = sound.getAttr("eventDamage");
					info.pHelper = pHelper;
					m_rotations[eType].m_damageSoundId = m_pVehicle->AddSoundEvent(info);
				}

				return true;
			}
		}

		return false;
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::UpdateRotationSound(EVehicleTurretRotationType type, float speed, float deltaTime)
{
	// update rotation sound, if available
	if (m_rotations[type].m_turnSoundId == InvalidSoundEventId)
		return;

	if (!m_pVehicle->IsPlayerDriving() && !m_pVehicle->IsPlayerPassenger())
		return;

	const static float minSpeed = 0.0002f;
	const static float soundDamageRatio = 0.2f;

	speed *= GetDamageSpeedMul(m_rotations[type].m_pPart);

	bool bDamage = m_rotations[type].m_pPart->m_damageRatio > soundDamageRatio && m_pVehicle->IsPlayerPassenger();

	if (speed != m_rotations[type].m_relSpeed)
	{
		if ((fabsf(speed) < 1e-4) && (fabsf(m_rotations[type].m_relSpeed) < 1e-4))
		{
			m_rotations[type].m_relSpeed = 0.0f;
		}
		else
		{
			Interpolate(m_rotations[type].m_relSpeed, speed, 8.0f, deltaTime);
		}

		float speedParam = (speed != 0.0f) ? min(1.0f, m_rotations[type].m_relSpeed / speed) : 0.0f;

		m_pVehicle->SetSoundParam(m_rotations[type].m_turnSoundId, "speed", speedParam, true);

		if (bDamage)
		{
			m_pVehicle->SetSoundParam(m_rotations[type].m_damageSoundId, "speed", speedParam, true);
		}

		if (fabsf(m_rotations[type].m_relSpeed) < minSpeed)
		{
			m_pVehicle->StopSound(m_rotations[type].m_turnSoundId);
			m_pVehicle->StopSound(m_rotations[type].m_damageSoundId);
		}
	}

	float inout = 1.f;
	if (m_pVehicle->IsPlayerPassenger())
	{
		if (IVehicleSeat* pSeat = m_pVehicle->GetSeatForPassenger(CCryAction::GetCryAction()->GetClientActor()->GetEntityId()))
		{
			if (IVehicleView* pView = pSeat->GetCurrentView())
			{
				if (!pView->IsThirdPerson())
					inout = pSeat->GetSoundParams().inout;
			}
		}
	}

	REINST("update params");
	/*if (ISound* pSound = m_pVehicle->GetSound(m_rotations[type].m_turnSoundId, false))
	   pSound->SetParam("in_out", inout, false);

	   if (bDamage)
	   {
	   if (ISound* pSound = m_pVehicle->GetSound(m_rotations[type].m_damageSoundId, false))
	   {
	    pSound->SetParam("in_out", inout, false);
	    pSound->SetParam("damage", min(1.f, m_rotations[type].m_pPart->m_damageRatio), false);
	   }
	   } */
}

//------------------------------------------------------------------------
bool CVehicleSeatActionRotateTurret::GetRotationLimits(int axis, float& min, float& max)
{
	EVehicleTurretRotationType type;
	if (axis == 0)
		type = eVTRT_Pitch;
	else if (axis == 2)
		type = eVTRT_Yaw;
	else
		return false;

	min = m_rotations[type].m_minLimitF;
	max = m_rotations[type].m_maxLimit;
	return true;
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionRotateTurret);
