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
#ifndef __VEHICLESEATACTIONROTATETURRET_H__
#define __VEHICLESEATACTIONROTATETURRET_H__

#include <CryAction/InterpolationHelpers.h>

class CVehiclePartBase;
class CVehicle;
class CVehicleSeat;

// holds information for each of the rotation types
struct SVehiclePartRotationParameters
{
	SVehiclePartRotationParameters() : m_prevWorldQuat(IDENTITY)
	{
		m_pPart = NULL;
		m_action = 0.0f;
		m_aimAssist = 0.f;
		m_currentValue = 0.0f;
		m_speed = 0.0f;
		m_acceleration = 0.0f;
		m_minLimitF = 0.0f;
		m_minLimitB = 0.0f;
		m_maxLimit = 0.0f;
		m_relSpeed = 0.0f;
		m_worldSpace = false;
		m_orientation = InterpolatedQuat(Quat::CreateIdentity(), 0.75f, 3.0f);
		m_turnSoundId = InvalidSoundEventId;
		m_damageSoundId = InvalidSoundEventId;
	}

	InterpolatedQuat     m_orientation; //	Interpolated orientation for this part

	CVehiclePartBase*    m_pPart; // TODO: IVehiclePart*?

	float                m_action;       // what the user input is requesting
	float                m_aimAssist;    // what the aim assist is requesting
	float                m_currentValue; // current rotation
	float                m_speed;        // speed of rotation (from vehicle xml)
	float                m_acceleration; // acceleration of rotation (from vehicle xml)

	float                m_minLimitF; // smallest permissable value (from xml), when facing forwards
	float                m_minLimitB; // smallest permissable value (from xml), when facing backwards
	float                m_maxLimit;  // largest permissable value (from xml)

	float                m_relSpeed; // used to interpolate the rotation speed (for sound)

	Quat                 m_prevWorldQuat; // previous world-space rotation

	TVehicleSoundEventId m_turnSoundId;
	TVehicleSoundEventId m_damageSoundId;

	bool                 m_worldSpace; // rotation should be calculated in world space
};

class CVehicleSeatActionRotateTurret
	: public IVehicleSeatAction
{
	IMPLEMENT_VEHICLEOBJECT
public:

	// IVehicleSeatAction
	virtual bool Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override { delete this; }

	virtual void StartUsing(EntityId passengerId) override;
	virtual void ForceUsage() override {}
	virtual void StopUsing() override;
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value) override;

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override;
	virtual void PostSerialize() override {}
	virtual void Update(const float deltaTime) override;

	virtual void UpdateFromPassenger(const float deltaTime, EntityId playerId) override;

	virtual void GetMemoryUsage(ICrySizer* s) const override { s->Add(*this); }
	// ~IVehicleSeatAction

	void         SetAimGoal(Vec3 aimPos, int priority = 0);
	bool         GetRemainingAnglesToAimGoalInDegrees(float& pitch, float& yaw); // FIXME: should be const, but IVehiclePart::GetLocalTM() is not const

	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override {}
	virtual bool GetRotationLimits(int axis, float& min, float& max);

protected:

	enum EVehicleTurretRotationType
	{
		eVTRT_Pitch = 0,
		eVTRT_Yaw,
		eVTRT_NumRotationTypes,
	};

	void  DoUpdate(const float deltaTime);

	void  UpdateAimGoal();
	void  MaintainPartRotationWorldSpace(EVehicleTurretRotationType eType);
	void  UpdatePartRotation(EVehicleTurretRotationType eType, float frameTime);
	float GetDamageSpeedMul(CVehiclePartBase* pPart);

	bool  InitRotation(IVehicle* pVehicle, const CVehicleParams& rotationTable, EVehicleTurretRotationType eType);
	bool  InitRotationSounds(const CVehicleParams& rotationParams, EVehicleTurretRotationType eType);
	void  UpdateRotationSound(EVehicleTurretRotationType eType, float speed, float deltaTime);

	CVehicle*                      m_pVehicle;
	IEntity*                       m_pUserEntity;
	CVehicleSeat*                  m_pSeat;

	Vec3                           m_aimGoal;
	int                            m_aimGoalPriority;

	SVehiclePartRotationParameters m_rotations[eVTRT_NumRotationTypes];

	IVehicleHelper*                m_rotTestHelpers[2];
	float                          m_rotTestRadius;

	friend class CVehiclePartBase;
};

#endif
