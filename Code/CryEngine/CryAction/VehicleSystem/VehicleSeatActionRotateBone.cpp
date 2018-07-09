// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleSeatActionRotateBone.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleSeat.h"
#include <CryExtension/CryCreateClassInstance.h>

CVehicleSeatActionRotateBone::CVehicleSeatActionRotateBone() : m_pSeat(NULL)
	, m_pVehicle(NULL)
	, m_MoveBoneId(-1)
	, m_boneRotation(0.f, 0.f, 0.f)
	, m_prevBoneRotation(0.f, 0.f, 0.f)
	, m_currentMovementRotation(IDENTITY)
	, m_desiredBoneRotation(0.f, 0.f, 0.f)
	, m_rotationSmoothingRate(0.f, 0.f, 0.f)
	, m_soundRotSpeed(0.f)
	, m_soundRotLastAppliedSpeed(-1.f)
	, m_soundRotSpeedSmoothRate(0.f)
	, m_soundRotSpeedSmoothTime(0.f)
	, m_soundRotSpeedScalar(1.0f)
	, m_pitchLimitMin(-1.f)
	, m_pitchLimitMax(1.f)
	, m_settlePitch(0.f)
	, m_settleDelay(0.f)
	, m_settleTime(1.f)
	, m_noDriverTime(0.f)
	, m_pAnimatedCharacter(NULL)
	, m_autoAimStrengthMultiplier(1.0f)
	, m_networkSluggishness(0.f)
	, m_speedMultiplier(0.1f)
	, m_rotationMultiplier(1.f)
	//,m_rotatingSoundID(INVALID_SOUNDID)
	, m_hasDriver(0)
	, m_driverIsLocal(0)
	, m_settleBoneOnExit(0)
	, m_useRotationSounds(0)
	, m_stopCurrentSound(0)
	, m_rotationLockCount(0)
{
	CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), m_poseModifier);
}

CVehicleSeatActionRotateBone::~CVehicleSeatActionRotateBone()
{
	m_pVehicle->UnregisterVehicleEventListener(this);
}

bool CVehicleSeatActionRotateBone::Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;
	m_pSeat = pSeat;

	IDefaultSkeleton* pIDefaultSkeleton = GetCharacterModelSkeleton();
	if (pIDefaultSkeleton)
	{
		if (table.haveAttr("MoveBone"))
		{
			const char* boneName = table.getAttr("MoveBone");
			m_MoveBoneId = pIDefaultSkeleton->GetJointIDByName(boneName);
		}
	}

	if (CVehicleParams baseOrientationTable = table.findChild("MoveBoneBaseOrientation"))
	{
		float x, y, z;
		baseOrientationTable.getAttr("x", x);
		baseOrientationTable.getAttr("y", y);
		baseOrientationTable.getAttr("z", z);
		m_boneBaseOrientation.SetRotationXYZ(Ang3(x, y, z));
	}
	else
	{
		m_boneBaseOrientation.SetIdentity();
	}

	// Set the PitchLimits.
	float degMin = -40.0f, degMax = 40.0f;
	if (CVehicleParams pitchLimitParams = table.findChild("PitchLimit"))
	{
		pitchLimitParams.getAttr("min", degMin);
		pitchLimitParams.getAttr("max", degMax);
	}
	m_pitchLimitMin = DEG2RAD(degMin);
	m_pitchLimitMax = DEG2RAD(degMax);

	if (table.getAttr("AutoAimStrengthMultiplier", m_autoAimStrengthMultiplier)) { m_autoAimStrengthMultiplier = -fabs_tpl(m_autoAimStrengthMultiplier); }
	table.getAttr("NetworkSluggishness", m_networkSluggishness);
	table.getAttr("SpeedMultiplier", m_speedMultiplier);

	// Rotation Audio
	m_useRotationSounds = 1;
	const char* soundFP = NULL;
	if (m_useRotationSounds &= table.getAttr("soundFP", &soundFP))
	{
		m_soundNameFP = soundFP;
	}
	const char* soundTP = NULL;
	if (m_useRotationSounds &= table.getAttr("soundTP", &soundTP))
	{
		m_soundNameTP = soundTP;
	}
	const char* soundParam = NULL;
	if (m_useRotationSounds &= table.getAttr("soundParam", &soundParam))
	{
		m_soundParamRotSpeed = soundParam;
	}

	float idealSpeed = 180.f;
	table.getAttr("idealRotationSpeedForSound", idealSpeed);
	m_soundRotSpeedScalar = (180.0f / gf_PI) * __fres((float)__fsel(-idealSpeed, 180.f, idealSpeed));
	table.getAttr("soundRotSpeedSmoothTime", m_soundRotSpeedSmoothTime);

	if (table.getAttr("settlePitch", m_settlePitch))
	{
		m_settleBoneOnExit = 1;
		m_settlePitch = DEG2RAD(m_settlePitch);
	}
	m_settleBoneOnExit |= table.getAttr("settleDelay", m_settleDelay);
	m_settleBoneOnExit |= table.getAttr("settleTime", m_settleTime);

	//Rotate the bone rotation to the initial direction of the pinger, as the rotation for the bone is in world space
	m_boneRotation = m_boneRotation + Ang3(pVehicle->GetEntity()->GetWorldRotation());
	m_prevBoneRotation = m_boneRotation;

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

	return true;
}

void CVehicleSeatActionRotateBone::Reset()
{
	// Enable PrePhysUpdate.
	m_pVehicle->GetGameObject()->EnablePrePhysicsUpdate(ePPU_Always);
}

void CVehicleSeatActionRotateBone::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	bool aspectChanged = false;
	if (actionId == eVAI_RotateYaw || actionId == eVAI_RotateYawAimAssist)
	{
		value *= actionId == eVAI_RotateYawAimAssist ? m_autoAimStrengthMultiplier * m_rotationMultiplier * m_speedMultiplier : m_rotationMultiplier * m_speedMultiplier;

		m_boneRotation.z -= value;

		const static float kThreshold = DEG2RAD(360.f);
		const float fWrappedRotation = (float)__fsel(m_boneRotation.z, m_boneRotation.z - kThreshold, m_boneRotation.z + kThreshold);
		const float fNewRot = (float)__fsel(fabsf(m_boneRotation.z) - kThreshold, fWrappedRotation, m_boneRotation.z);
		aspectChanged = fabs_tpl(m_boneRotation.z - fNewRot) > 0.f;
		m_boneRotation.z = fNewRot;
	}
	else if (actionId == eVAI_RotatePitch || actionId == eVAI_RotatePitchAimAssist)
	{
		value *= actionId == eVAI_RotatePitchAimAssist ? m_autoAimStrengthMultiplier * m_rotationMultiplier * m_speedMultiplier : m_rotationMultiplier * m_speedMultiplier;

		float fNewRot = clamp_tpl(m_boneRotation.x - value, m_pitchLimitMin, m_pitchLimitMax);
		aspectChanged = fabs_tpl(m_boneRotation.x - fNewRot) > 0.f;
		m_boneRotation.x = fNewRot;
	}

	if (aspectChanged)
	{
		static_cast<CVehicleSeat*>(m_pSeat)->ChangedNetworkState(CVehicle::ASPECT_SEAT_ACTION);
	}
}

void CVehicleSeatActionRotateBone::StartUsing(EntityId passengerId)
{
	m_pVehicle->RegisterVehicleEventListener(this, "CVehicleSeatActionRotateBone");
	m_hasDriver = 1;
	m_driverIsLocal = (gEnv->pGameFramework->GetClientActorId() == passengerId);
	m_rotationMultiplier = 1.f;
	m_rotationLockCount = 0;
	m_stopCurrentSound = 1;
}

void CVehicleSeatActionRotateBone::StopUsing()
{
	m_pVehicle->UnregisterVehicleEventListener(this);
	m_hasDriver = 0;
	m_driverIsLocal = 0;
	m_noDriverTime = 0.f;
	m_stopCurrentSound = 1;
}

void CVehicleSeatActionRotateBone::PrePhysUpdate(const float dt)
{
	IEntity* pEntity = m_pVehicle->GetEntity();
	if (!pEntity)
	{
		return;
	}

	if (pEntity->IsHidden() || pEntity->IsInvisible())
	{
		m_boneRotation.x = m_settlePitch;
		m_desiredBoneRotation = m_boneRotation;
		return;
	}

	if (m_hasDriver)
	{
		if (!m_driverIsLocal)
		{
			// Smooth the networked rotation.
			m_boneRotation.z += (float)__fsel((m_desiredBoneRotation.z - m_boneRotation.z) - gf_PI, gf_PI2, 0.f);
			m_boneRotation.z -= (float)__fsel((m_boneRotation.z - m_desiredBoneRotation.z) - gf_PI, gf_PI2, 0.f);
			SmoothCD<Ang3>(m_boneRotation, m_rotationSmoothingRate, dt, m_desiredBoneRotation, m_networkSluggishness);
		}
		else
		{
			m_desiredBoneRotation = m_boneRotation;
		}
	}
	else
	{
		if (!m_rotationLockCount)
		{
			m_noDriverTime += dt;
			if (m_settleBoneOnExit && m_noDriverTime > m_settleDelay)
			{
				// When there is no Driver, locally smooth to specified pitch.
				Ang3 target(m_settlePitch, m_boneRotation.y, m_boneRotation.z);
				SmoothCD<Ang3>(m_boneRotation, m_rotationSmoothingRate, dt, target, m_settleTime);
			}
		}
		m_desiredBoneRotation = m_boneRotation;
	}

	IAnimationPoseModifierPtr modPtr = m_poseModifier;
	pEntity->GetCharacter(0)->GetISkeletonAnim()->PushPoseModifier(2, modPtr, "VehicleBoneRotate");

	const Quat vehicleRotation(pEntity->GetWorldRotation());
	const Quat headOrientation(Quat::CreateRotationXYZ(m_boneRotation));

	Quat finalheadOrientation = vehicleRotation.GetInverted() * m_currentMovementRotation.GetInverted() * headOrientation * m_boneBaseOrientation;
	m_poseModifier->PushOrientation(m_MoveBoneId, IAnimationOperatorQueue::eOp_Override, finalheadOrientation);
	m_currentMovementRotation.SetIdentity();

	// Update Sounds.
	UpdateSound(dt);

	// Store Prev Rotation.
	m_prevBoneRotation = m_boneRotation;
}

void CVehicleSeatActionRotateBone::UpdateSound(const float dt)
{
	if (!m_useRotationSounds)
		return;

	IEntity* pEntity = m_pVehicle->GetEntity();
	if (IEntityAudioComponent* pIEntityAudioComponent = pEntity->GetComponent<IEntityAudioComponent>())
	{
		if (m_stopCurrentSound)
		{
			m_stopCurrentSound = 0;
			//StopSound(*pIEntityAudioComponent, m_rotatingSoundID);
		}

		// Calculate Rotation Speed for the Audio.
		if (dt > FLT_EPSILON)
		{
			const float deltaPitch = (m_boneRotation.x - m_prevBoneRotation.x);
			float deltaYaw = fabs_tpl(m_boneRotation.z - m_prevBoneRotation.z);
			deltaYaw = (float)__fsel(deltaYaw - gf_PI, gf_PI2 - deltaYaw, deltaYaw);
			const float rotDist = sqrt_tpl((deltaYaw * deltaYaw) + (deltaPitch * deltaPitch));
			const float invTime = __fres(dt);
			const float rotSpeed = invTime * rotDist;
			const float prevSpeed = m_soundRotLastAppliedSpeed;
			m_soundRotSpeed = (float)__fsel(rotSpeed - prevSpeed, rotSpeed, m_soundRotSpeed);
			SmoothCD(m_soundRotSpeed, m_soundRotSpeedSmoothRate, dt, rotSpeed, m_soundRotSpeedSmoothTime);
			if (fabs_tpl(prevSpeed - m_soundRotSpeed) > 0.001f)
			{
				const float rotationRate = clamp_tpl(m_soundRotSpeed * m_soundRotSpeedScalar, 0.f, 10.f);
				//SetSoundParam(*pIEntityAudioComponent, m_rotatingSoundID, m_soundParamRotSpeed.c_str(), rotationRate);
				m_soundRotLastAppliedSpeed = m_soundRotSpeed;
			}
		}

		REINST("vehicle sounds");
		//if(m_soundRotSpeed>0.001f)
		//{
		//	bool isNotPlaying = m_rotatingSoundID==INVALID_SOUNDID;
		//	if(!isNotPlaying)
		//	{
		//		if(ISound* pSound = pIEntityAudioComponent->GetSound(m_rotatingSoundID))
		//		{
		//			isNotPlaying = !pSound->IsPlaying();
		//		}
		//		else
		//		{
		//			isNotPlaying = true;
		//		}
		//	}

		//	// Play the sound...
		//	if(isNotPlaying)
		//	{
		//		IActor* pDriver = m_pVehicle->GetDriver();
		//		const bool firstPerson = pDriver && pDriver->IsClient();
		//		m_rotatingSoundID=PlaySound(*pIEntityAudioComponent, firstPerson?m_soundNameFP.c_str():m_soundNameTP.c_str());
		//	}
		//}
	}
}

void CVehicleSeatActionRotateBone::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
}

IDefaultSkeleton* CVehicleSeatActionRotateBone::GetCharacterModelSkeleton() const
{
	if (ICharacterInstance* pCharacterInstance = m_pVehicle->GetEntity()->GetCharacter(0))
	{
		return &pCharacterInstance->GetIDefaultSkeleton();
	}

	return NULL;
}

void CVehicleSeatActionRotateBone::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget() == eST_Network && (aspects & CVehicle::ASPECT_SEAT_ACTION))
	{
		NET_PROFILE_SCOPE("SeatAction_RotateBone", ser.IsReading());

		Vec3 rotation(m_boneRotation.x, m_boneRotation.y, m_boneRotation.z);

		ser.Value("m_boneRotation", rotation, 'jmpv');

		if (ser.IsReading())
		{
			m_desiredBoneRotation.Set(rotation.x, rotation.y, rotation.z);
		}
	}
}

void CVehicleSeatActionRotateBone::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	if (event == eVE_LockBoneRotation)
	{
		const bool isLocking = params.bParam;
		const float pitch = params.fParam;
		if (isLocking)
		{
			if (pitch >= m_pitchLimitMin && pitch <= m_pitchLimitMax)
			{
				m_boneRotation.x = params.fParam;
			}
			if (m_rotationLockCount < 3)
				m_rotationLockCount++;
			m_rotationMultiplier = 0.f;
		}
		else
		{
			if (m_rotationLockCount > 0)
				m_rotationLockCount--;
			m_rotationMultiplier = m_rotationLockCount ? 0.f : 1.f;
		}
	}
	else if (event == eVE_VehicleMovementRotation)
	{
		const Quat currentRotation(params.fParam, params.vParam);
		m_currentMovementRotation = currentRotation;
	}
	else if (event == eVE_Destroyed || (event == eVE_Hide && params.iParam))
	{
		IEntity* pEntity = m_pVehicle->GetEntity();
		if (IEntityAudioComponent* pIEntityAudioComponent = pEntity->GetComponent<IEntityAudioComponent>())
		{
			m_stopCurrentSound = 0;
			//StopSound(*pIEntityAudioComponent, m_rotatingSoundID);
		}
	}

}

///*static*/ tSoundID CVehicleSeatActionRotateBone::PlaySound( IEntityAudioComponent& rIEntityAudioComponent, const char * soundName )
//{
//	return rIEntityAudioComponent.PlaySoundEx(soundName, ZERO, FORWARD_DIRECTION, FLAG_SOUND_DEFAULT_3D, 0, 1.0f, 0, 0, eSoundSemantic_Weapon);
//}
//
///*static*/ void CVehicleSeatActionRotateBone::SetSoundParam(IEntityAudioComponent& rIEntityAudioComponent, tSoundID soundID, const char* param, float value)
//{
//	if(ISound* pSound = rIEntityAudioComponent.GetSound(soundID))
//	{
//		pSound->SetParam(param, value, false);
//	}
//}
//
///*static*/ void CVehicleSeatActionRotateBone::StopSound( IEntityAudioComponent& rIEntityAudioComponent, tSoundID soundID )
//{
//	rIEntityAudioComponent.StopSound(soundID);
//}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionRotateBone);
