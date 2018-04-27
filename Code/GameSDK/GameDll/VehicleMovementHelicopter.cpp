// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a standard helicopter movements

-------------------------------------------------------------------------
History:
- 04:04:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"

#include "IMovementController.h"
#include "IVehicleSystem.h"
#include "VehicleMovementHelicopter.h"
#include "VehicleActionLandingGears.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryGame/GameUtils.h>
#include "Vehicle/VehicleUtils.h"

#include <CryRenderer/IRenderAuxGeom.h>

float CVehicleMovementHelicopter::k_netErrorPosScale=2.f;

/*
=========================================================================================================
	Noise Generation for turbulence
=========================================================================================================
*/

CVTolNoise::CVTolNoise() : m_bEnabled(false), m_amplitude(ZERO), m_frequency(ZERO), m_rotAmplitude(ZERO), m_rotFrequency(ZERO), m_startDamageRatio(0.f)
{
}

void CVTolNoise::Init(const Vec3& amplitude, const Vec3& frequency, const Vec3& rotAmplitude, const Vec3& rotFrequency, const float& startDamageRatio)
{
	m_bEnabled = true;

	m_amplitude = amplitude;
	m_frequency = frequency;
	m_rotAmplitude = rotAmplitude;
	m_rotFrequency = rotFrequency;
	m_startDamageRatio = startDamageRatio;

	m_posGen.x.Setup(m_amplitude.x, m_frequency.x );
	m_posGen.y.Setup(m_amplitude.y, m_frequency.y, 0x234567 );
	m_posGen.z.Setup(m_amplitude.z, m_frequency.z, 0x566757 );
	m_rotGen.x.Setup(m_rotAmplitude.x, m_rotFrequency.x );
	m_rotGen.y.Setup(m_rotAmplitude.y, m_rotFrequency.y, 0x234567 );
	m_rotGen.z.Setup(m_rotAmplitude.z, m_rotFrequency.z, 0x566757 );

	// Seed initial offsets
	m_pos = m_posGen.Update(0.1f);
	m_rot = m_rotGen.Update(0.1f);
	m_posDifference.zero();
	m_angDifference.zero();
}

void CVTolNoise::Update(float dt)
{
	if (m_bEnabled && dt>0.f)
	{
		float invDt = 1.f/dt;
		Vec3 pos = m_posGen.Update(dt);
		Vec3 rot = m_rotGen.Update(dt);
		// Calculate the difference
		m_posDifference = (pos - m_pos)*invDt;
		m_angDifference = (rot - m_rot)*invDt;
		m_pos = pos;
		m_rot = rot;
	}
}


//------------------------------------------------------------------------
CVehicleMovementHelicopter::CVehicleMovementHelicopter() : 
  m_HoverAnim(0),
	m_pRotorPart(0),
	m_pRotorHelper(0),
	m_pLandingGears(0),
	m_engineWarmupDelay(0.0f),
	m_yawPerRoll(0.0f),
	m_enginePower(0.0f),
	m_enginePowerMax(0.0f),
	m_EnginePerformance(1.0f),
	m_damageActual(0.0f),
	m_actionPitch(0.0f),
	m_actionYaw(0.0f),
	m_actionRoll(0.0f),
	m_maxRollAngle(0.0f),
	m_maxPitchAngle(0.0f),
	m_rollDamping(0.98f),
	m_yawResponseScalar(1.0f),
	m_CurrentSpeed(0.0f),
	m_CurrentVel(ZERO),
  m_steeringDamage(ZERO),
	m_bExtendMoveTarget(true),
	m_bApplyNoiseAsVelocity(true),
	m_sendTime(0.f),
	m_sendTimer(0.f),
	m_updateAspects(CNetworkMovementHelicopterCrysis2::CONTROLLED_ASPECT),
	m_pNoise(&m_defaultNoise)
{
	m_CurrentVel.zero();
	m_netPosAdjust.zero();

	m_netActionSync.Write(this);
}
	
//------------------------------------------------------------------------
bool CVehicleMovementHelicopter::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	if (!CVehicleMovementBase::Init(pVehicle, table))
		return false;

	// Initialise the arcade physics, handling helper
	if (CVehicleParams handlingParams = table.findChild("HandlingArcade"))
		if (!m_arcade.Init(pVehicle, handlingParams))
			return false;

	MOVEMENT_VALUE("engineWarmupDelay", m_engineWarmupDelay);
	MOVEMENT_VALUE("enginePowerMax", m_enginePowerMax);
	MOVEMENT_VALUE("yawPerRoll", m_yawPerRoll);
	MOVEMENT_VALUE("maxSpeed", m_maxSpeed);
	MOVEMENT_VALUE("maxPitchAngle", m_maxPitchAngle);
	MOVEMENT_VALUE("maxRollAngle", m_maxRollAngle);
	MOVEMENT_VALUE("rollDamping", m_rollDamping);

	if(table.haveAttr("extendMoveTarget"))
	{
		table.getAttr("extendMoveTarget", m_bExtendMoveTarget);
	}
	table.getAttr("applyNoiseAsVelocity", m_bApplyNoiseAsVelocity);

	if(CVehicleParams noiseParams = table.findChild("MovementNoise"))
	{
		InitMovementNoise(noiseParams, m_defaultNoise);
	}

	// high-level controller
	Ang3 angles				= m_pEntity->GetWorldAngles();
	m_enginePower			= 0.0f;

	if (table.haveAttr("rotorPartName"))
		m_pRotorPart = m_pVehicle->GetPart(table.getAttr("rotorPartName"));

	m_isEngineGoingOff = true;
	m_isEnginePowered = false;

	ResetActions();

	m_pVehicle->GetGameObject()->EnableUpdateSlot(m_pVehicle, IVehicle::eVUS_EnginePowered);

	return true;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::PostInit()
{
	CVehicleMovementBase::PostInit();
  m_HoverAnim			= m_pVehicle->GetAnimation("hover");
	m_pRotorHelper  = m_pVehicle->GetHelper("rotorSmokeOut");

	for (int i = 0; i < m_pVehicle->GetActionCount(); i++)
	{
		IVehicleAction* pAction = m_pVehicle->GetAction(i);

		m_pLandingGears = CAST_VEHICLEOBJECT(CVehicleActionLandingGears, pAction);
		if (m_pLandingGears)
			break;
	}

  if (!gEnv->IsEditor())
  {
    Reset();
  }

	OccupyWeaponSeats();
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::Release()
{
	CVehicleMovementBase::Release();
	delete this;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::Physicalize()
{
	CVehicleMovementBase::Physicalize();

	IPhysicalEntity * pPhysicalEntity = GetPhysics();

	pe_params_flags pf;
	pf.flagsOR = pef_ignore_areas;
	pPhysicalEntity->SetParams(&pf);

	pe_simulation_params simParams;
	simParams.dampingFreefall = 0.01f;

	pPhysicalEntity->SetParams(&simParams);
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::Reset()
{
	CVehicleMovementBase::Reset();

	m_damageActual = 0.0f;

	ResetActions();

	m_enginePower		= 0.0f;
	m_EnginePerformance = 1.0f;

	m_rpmScale			= 100.0f;
	m_isEnginePowered	= false;

	m_actionPitch		= 0.0f;
	m_actionRoll		= 0.0f;
	m_actionYaw			= 0.0f;

	m_CurrentSpeed		= 0.0f;
	m_CurrentVel		  = ZERO;
  m_steeringDamage  = ZERO;

	m_pNoise = &m_defaultNoise;

  m_pVehicle->GetGameObject()->EnableUpdateSlot(m_pVehicle, IVehicle::eVUS_EnginePowered);

	if(m_HoverAnim)
		m_HoverAnim->Reset();

	SetAnimationSpeed(eVMA_Engine, (m_enginePower / m_enginePowerMax));
	
	m_arcade.Reset(m_pVehicle, m_pEntity);
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::ResetActions()
{
	m_inputAction.Reset();
}

//------------------------------------------------------------------------
bool CVehicleMovementHelicopter::StartDriving(EntityId driverId)
{
	if (!CVehicleMovementBase::StartDriving(driverId))
	{
		return false;
	}

  m_isEnginePowered = true;

	return true;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::StopDriving()
{
	CVehicleMovementBase::StopDriving();
	ResetActions();

	m_engineStartup += m_engineWarmupDelay;
  m_isEnginePowered = false;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params)
{
	switch (event)
	{
	case eVME_Repair:
		{
			if(params.fValue < 0.25)
			{
				m_damageActual = 0.0f;
				m_damage = 0.0f;
			}
		}
		break;

  case eVME_Collision:
    {
      if (0)
      {
        m_isEnginePowered = false;

        pe_simulation_params simParams;
        simParams.dampingFreefall = 0.01f;
        simParams.gravity = Vec3(0.0f, 0.0f, -9.8f);
        GetPhysics()->SetParams(&simParams);
      }
    }
    break;

	case eVME_GroundCollision:
		{
			const float stopOver = 1.0f;
		}
		break;

	case eVME_Damage:
		{
      if (!m_pVehicle->IsIndestructable())
      {
			  const float stopOver = 1.0f;

			  m_damage = params.fValue;

        if (m_damage > 0.95f)
        {
          m_isEngineDisabled	= true;
          m_isEnginePowered	= false;

          StopExhaust();
          StopAllTriggers();

					IPhysicalEntity * pPhysicalEntity = GetPhysics();

          pe_action_impulse impulse;
          impulse.angImpulse = Vec3(0.1f, 100.f, 0.0f);
          pPhysicalEntity->Action(&impulse);

          pe_simulation_params simParams;
          simParams.dampingFreefall = 0.01f;
          simParams.gravity = Vec3(0.0f, 0.0f, -9.8f);
          pPhysicalEntity->SetParams(&simParams);

          SVehicleEventParams eventParams;
          eventParams.entityId = 0;
          m_pVehicle->BroadcastVehicleEvent(eVE_Destroyed, eventParams);

          if (m_pVehicle)
          {
            if (IEntity *entity = m_pVehicle->GetEntity())
            {
              if (IAIObject *aiobject = entity->GetAI())
              {
                aiobject->Event(AIEVENT_DISABLE, NULL);
              }
            }
          }
        }
      }
		}
		break;

	case eVME_WarmUpEngine:
		m_enginePower = m_enginePowerMax;
		break;

	//case eVME_Turbulence:
	//	m_turbulence = max(m_turbulence, params.fValue);
	//	break;

	default:
		CVehicleMovementBase::OnEvent(event, params);
		break; 
	}
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	CryAutoCriticalSection lk(m_lock);

	CVehicleMovementBase::OnAction(actionId, activationMode, value);

	if (m_arcade.m_handling.maxSpeedForward>0.f) // Use the new handling code
	{
		m_inputAction.forwardBack = m_movementAction.power;

		if (actionId==eVAI_StrafeLeft)
		{
			m_inputAction.leftRight = -value; 
		}
		if(actionId==eVAI_XIMoveY)
		{
			m_inputAction.forwardBack += value; 
		}
		if(actionId==eVAI_TurnRight)
		{
			m_inputAction.leftRight = +value; 
		}
		if(actionId==eVAI_TurnLeft)
		{
			m_inputAction.leftRight = -value; 
		}
		if (actionId==eVAI_MoveForward)
		{
			m_inputAction.forwardBack = value; 
		}
		if (actionId==eVAI_MoveBack)
		{
			m_inputAction.forwardBack = -value; 
		}
		if (actionId==eVAI_RotatePitch)
		{
			m_inputAction.forwardBack = value; 
		}
		else if (actionId==eVAI_StrafeRight)
		{
			m_inputAction.leftRight = +value; 
		}
		else if (actionId==eVAI_MoveUp)
		{
			m_inputAction.rawInputUp = min(1.f, sqr(value));
			m_inputAction.upDown = m_inputAction.rawInputUp + m_inputAction.rawInputDown;
		}
		else if (actionId==eVAI_MoveDown)
		{
			m_inputAction.rawInputDown = max(-1.f, -sqr(value));
			m_inputAction.upDown = m_inputAction.rawInputUp + m_inputAction.rawInputDown;
		}
		else if (actionId==eVAI_RotateYaw)
		{
			m_inputAction.yaw = value;
		}

		const float eps = 0.05f;
		if(fabs(m_inputAction.upDown) > eps || fabs(m_inputAction.leftRight) > eps)
		{
			m_pVehicle->NeedsUpdate(IVehicle::eVUF_AwakePhysics);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::ProcessActions(const float deltaTime)
{

}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::ProcessActionsLift(float deltaTime)
{

}

float CVehicleMovementHelicopter::GetAdditionalSlopePitch(const Vec3 &desiredMoveDir) const
{
  return desiredMoveDir.z * gf_PI * 0.05f;
}

//===================================================================
// ProcessAI
// This treats the helicopter as able to move in any horizontal direction
// by tilting in any direction. Yaw control is thus secondary. Throttle
// control is also secondary since it is adjusted to maintain or change
// the height, and the amount needed depends on the tilt.
//===================================================================
//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.

void CVehicleMovementHelicopter::ProcessAI(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	CryAutoCriticalSection lk(m_lock);
	SVehiclePhysicsStatus* physStatus = &m_physStatus[k_physicsThread];
		
	if (m_arcade.m_handling.maxSpeedForward>0.f) // Use the new handling code
	{
		//ResetActions();
		m_movementAction.Clear();
		m_movementAction.isAI = true;

		SVehiclePhysicsHelicopterProcessAIParams params;
		params.pPhysStatus = physStatus;
		params.pInputAction = &m_inputAction;
		params.pAiRequest = &m_aiRequest;
		params.dt = deltaTime;
		params.aiRequiredVel = m_CurrentVel;
		params.aiCurrentSpeed = m_CurrentSpeed;
		params.aiYawResponseScalar = m_yawResponseScalar;
		m_yawResponseScalar = 1.f;
	
		// Use helper class to process the AI input
		// It will return a requested velocity, and change the input
		m_arcade.ProcessAI(params);

		// Get the output velocity
		m_CurrentVel = params.aiRequiredVel;
		m_CurrentSpeed = params.aiCurrentSpeed;
		return;
	}
	////////////////////// OLD DEPRECATED CODE :( //////////////////////////////////

	m_movementAction.Clear();
	ResetActions();

	// Our current state
	const Vec3 worldPos =  physStatus->pos;
	const Matrix33 worldMat( physStatus->q);
	const Matrix33 localMat( physStatus->q.GetInverted());
	const Ang3 worldAngles = Ang3::GetAnglesXYZ(worldMat);
	const Ang3 localAngles = Ang3::GetAnglesXYZ(localMat);

	const Vec3 currentVel = physStatus->v;
	const Vec3 currentVel2D(currentVel.x, currentVel.y, 0.0f);
	m_CurrentSpeed = m_CurrentVel.len(); //currentVel.len();
  float currentSpeed2d = currentVel2D.len();

	// +ve direction mean rotation anti-clocwise about the z axis - 0 means along y
	float currentDir = worldAngles.z;

	// to avoid singularity
	const Vec3 vWorldDir = worldMat.GetRow(1);
	const Vec3 vSideWays = worldMat.GetRow(0);
	const Vec3 vWorldDir2D =  Vec3( vWorldDir.x,  vWorldDir.y, 0.0f ).GetNormalizedSafe();

	// Our inputs
	float desiredSpeed = m_aiRequest.HasDesiredSpeed() ? m_aiRequest.GetDesiredSpeed() : 0.0f;
	Limit(desiredSpeed, -m_maxSpeed, m_maxSpeed);

	const Vec3 desiredMoveDir = m_aiRequest.HasMoveTarget() ? (m_aiRequest.GetMoveTarget() - worldPos).GetNormalizedSafe() : vWorldDir;
	Vec3 desiredMoveDir2D = Vec3(desiredMoveDir.x, desiredMoveDir.y, 0.0f);
	desiredMoveDir2D = desiredMoveDir2D.GetNormalizedSafe(desiredMoveDir2D);

	const Vec3 desiredVel = desiredMoveDir * desiredSpeed; 

  Vec3 desiredLookDir(desiredMoveDir);

  if (m_aiRequest.HasDesiredBodyDirectionAtTarget())
  {
	    desiredLookDir = m_aiRequest.GetDesiredBodyDirectionAtTarget().GetNormalizedSafe(desiredMoveDir);
  }
  else if (m_aiRequest.HasLookTarget())
  {
    desiredLookDir = (m_aiRequest.GetLookTarget() - worldPos).GetNormalizedSafe(desiredMoveDir);  
  }


	//const Vec3 desiredLookDir = m_aiRequest.HasLookTarget() ? (m_aiRequest.GetLookTarget() - worldPos).GetNormalizedSafe() : desiredMoveDir;
	const Vec3 desiredLookDir2D = Vec3(desiredLookDir.x, desiredLookDir.y, 0.0f).GetNormalizedSafe(vWorldDir2D);

  Vec3 prediction = m_aiRequest.HasBodyTarget() ? m_aiRequest.GetBodyTarget() : ZERO;

  prediction = (prediction.IsEquivalent(ZERO)) ? desiredMoveDir2D : prediction - worldPos;
  prediction.z = 0.0f;

  float speedLimit = prediction.GetLength2D();

	if(speedLimit > 0.0f)
	{
		prediction *= 1.0f / speedLimit;
	}

  Vec3 tempDir = currentVel2D.IsEquivalent(ZERO) ? localMat.GetRow(1) : currentVel2D;
  tempDir.z = 0.0f;
  tempDir.NormalizeFast();

	float dotProd = tempDir.dot(prediction);
	Limit(dotProd, FLT_EPSILON, 1.0f);

	float accel = m_enginePowerMax * min(2.0f,  1.0f / dotProd); // * dotProd;

	if (!m_aiRequest.HasDesiredBodyDirectionAtTarget())
	{
		dotProd *= dotProd;
		dotProd *= dotProd;
		float tempf = min(max(speedLimit * speedLimit, 2.0f), m_maxSpeed * dotProd);
		Limit(desiredSpeed, -tempf, tempf);
	}
	else if (dotProd < 0.0125f)
	{
		Limit(desiredSpeed, -m_maxSpeed * 0.25f, m_maxSpeed * 0.25f);
	}
  
  float posNeg = (float)__fsel(desiredSpeed - m_CurrentSpeed, 1.0f, -5.0f);

  m_CurrentSpeed = m_CurrentSpeed + posNeg * accel * deltaTime;

	if (posNeg > 0.0f && m_CurrentSpeed > desiredSpeed)
	{
		m_CurrentSpeed = desiredSpeed;
	}
	else if (posNeg < 0.0f && m_CurrentSpeed < desiredSpeed)
	{
		m_CurrentSpeed = desiredSpeed;
	}

	
	// ---------------------------- Rotation ----------------------------
	
	float desiredDir = (desiredLookDir2D.GetLengthSquared() > 0.0f) ? atan2f(-desiredLookDir2D.x, desiredLookDir2D.y) : atan2f(-vWorldDir2D.x, vWorldDir2D.y);
	while (currentDir < desiredDir - gf_PI)
		currentDir += 2.0f * gf_PI;
	while (currentDir > desiredDir + gf_PI)
		currentDir -= 2.0f * gf_PI;

	// ---------------------------- Yaw ----------------------------

	Ang3 dirDiff(0.0f, 0.0f, desiredDir - currentDir);
	dirDiff.RangePI();
	float absDiff   = fabsf(dirDiff.z);

	float rotSpeed		= (float)__fsel(dirDiff.z, m_yawPerRoll, -m_yawPerRoll);
	m_actionYaw		= m_actionYaw + deltaTime * (rotSpeed - m_actionYaw);

	float temp = fabsf(m_actionYaw);

	float multiplier = ((absDiff / (temp + 0.001f)) + 1.0f) * 0.5f;
	
	m_actionYaw	*= (float)__fsel(absDiff - temp, 1.0f, multiplier);

	// ---------------------------- Yaw ------------------------------

	m_CurrentVel = desiredMoveDir * m_CurrentSpeed;

  // ---------------------------- Pitch ----------------------------

  if (m_CurrentVel.GetLengthSquared2D() > 0.1f)
  {
    CalculatePitch(worldAngles, desiredMoveDir, currentSpeed2d, desiredSpeed, deltaTime);
  }
  else
  {
    Quat rot;
    rot.SetRotationVDir(desiredLookDir, 0.0f);
    float desiredXRot = Ang3::GetAnglesXYZ(rot).x + m_steeringDamage.x;

    m_actionPitch = worldAngles.x + (desiredXRot - worldAngles.x) * deltaTime/* * 10.0f*/;
    Limit(m_actionPitch, -m_maxPitchAngle * 2.0f, m_maxPitchAngle * 2.0f);
  }


	// ---------------------------- Roll ----------------------------
	float rollSpeed = GetRollSpeed();

	rollSpeed *= deltaTime;
	rollSpeed		= (float)__fsel(absDiff - rollSpeed, rollSpeed, absDiff);
	float roll		=(float) __fsel(dirDiff.z, -rollSpeed, rollSpeed);

	float speedPerUnit	   = 1.5f;
	float desiredRollSpeed = (-m_actionYaw * 2.5f) + m_steeringDamage.y;

	m_actionRoll = m_actionRoll + deltaTime * (desiredRollSpeed - m_actionRoll);
	Limit(m_actionRoll, -m_maxRollAngle + m_steeringDamage.y, m_maxRollAngle - m_steeringDamage.y);
	m_actionRoll *= m_rollDamping;
	// ---------------------------- Roll ----------------------------


	// ---------------------------- Convert and apply ----------------------------
	Ang3 angles(m_actionPitch, m_actionRoll, worldAngles.z + deltaTime * m_actionYaw);
	
	pe_params_pos paramPos;
	paramPos.q.SetRotationXYZ(angles);
	paramPos.q.Normalize();

	IPhysicalEntity * pPhysicalEntity = GetPhysics();
	pPhysicalEntity->SetParams(&paramPos, 1);

	pe_action_set_velocity vel;
	vel.v = m_CurrentVel + m_netPosAdjust;
	pPhysicalEntity->Action(&vel, 1);

	// ---------------------------- Convert and apply ----------------------------

  m_rpmScale = max(0.2f, fabs_tpl(m_CurrentSpeed / m_maxSpeed));
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::PreProcessMovement(const float deltaTime)
{
}

//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementHelicopter::ProcessMovement(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	IPhysicalEntity* pPhysics = GetPhysics();
	assert(pPhysics);
	
	if (m_arcade.m_handling.maxSpeedForward>0.f) // Use the new handling code
	{
		CryAutoCriticalSection lk(m_lock);

		if (!m_isEnginePowered || m_isEngineStarting)
			return;

		CVehicleMovementBase::ProcessMovement(deltaTime);
		SVehiclePhysicsStatus* physStatus = &m_physStatus[k_physicsThread];

		if(m_bApplyNoiseAsVelocity)
		{
			physStatus->v -= m_pNoise->m_posDifference;
			physStatus->w -= m_pNoise->m_angDifference;

			m_pNoise->Update(deltaTime);
		}

	
		///////////////////////////////////////////////////////////////
		// Pass on the movement request to the active physics handler
		// NB: m_physStatus is update by this call
		SVehiclePhysicsHelicopterProcessParams params;
		params.pPhysics = pPhysics;
		params.pPhysStatus = physStatus;
		params.pInputAction = &m_inputAction;
		params.dt = deltaTime;
		params.haveDriver = (m_actorId!=0)||m_remotePilot;
		params.isAI = m_movementAction.isAI;
		params.aiRequiredVel = m_CurrentVel;
		
		m_arcade.ProcessMovement(params);
		
		// Network error adjustment
		m_netPosAdjust *= max(0.f, 1.f-deltaTime*k_netErrorPosScale);
		physStatus->v += m_netPosAdjust * k_netErrorPosScale;

		if(m_bApplyNoiseAsVelocity)
		{
			physStatus->v += m_pNoise->m_posDifference;
			physStatus->w += m_pNoise->m_angDifference;
		}

		//===============================================
		// Commit the velocity back to the physics engine
		//===============================================
		pe_action_set_velocity setVelocity;
		setVelocity.v = physStatus->v;
		setVelocity.w = physStatus->w;
		pPhysics->Action(&setVelocity, 1);
		///////////////////////////////////////////////////////////////
	}
	else
	{
		if (m_isEnginePowered && pPhysics)
		{
			m_movementAction.isAI = true;
	
			pe_status_pos psp;
			pe_status_dynamics psd;
			if (!pPhysics->GetStatus(&psp) || !pPhysics->GetStatus(&psd))
				return;
			UpdatePhysicsStatus(&m_physStatus[k_physicsThread], &psp, &psd);

			ProcessAI(deltaTime);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::UpdateDamages(float deltaTime)
{}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::UpdateEngine(float deltaTime)
{
	// will update the engine power up to the maximum according to the ignition time

	if (m_isEnginePowered && !m_isEngineGoingOff)
	{
		if (m_enginePower < m_enginePowerMax)
		{
			m_enginePower += deltaTime * (m_enginePowerMax / m_engineWarmupDelay);
		}
	}
	else
	{
		if (m_enginePower >= 0.0f)
		{
			m_enginePower -= deltaTime * (m_enginePowerMax / m_engineWarmupDelay);
			
			if (m_damageActual > 0.0f)
				m_enginePower *= 1.0f - min(1.0f, m_damageActual);
		}
	}

	clamp_tpl(m_enginePower, 0.0f, m_enginePowerMax);
}


//------------------------------------------------------------------------
void CVehicleMovementHelicopter::Update(const float deltaTime)
{
  CRY_PROFILE_FUNCTION( PROFILE_GAME );

	CVehicleMovementBase::Update(deltaTime);

	UpdateDamages(deltaTime);
	UpdateEngine(deltaTime);

	{
		CryAutoCriticalSection lk(m_lock);
		m_netActionSync.Read(this);
		if (gEnv->bServer)
		{
			m_sendTimer -= deltaTime;
			if (m_sendTimer<=0.f)
			{
				m_netActionSync.Write(this);
				CHANGED_NETWORK_STATE(m_pVehicle, m_updateAspects);
				m_sendTimer = m_sendTime;
			}
		}
	}

	// update animation
	if(m_isEngineGoingOff)
	{
		if(m_enginePower > 0.0f)
		{
			UpdateEngine(deltaTime);
		}
		else
		{
			m_enginePower = 0.0f;
		}
	}

	SetAnimationSpeed(eVMA_Engine, (m_enginePower / m_enginePowerMax));
}

//------------------------------------------------------------------------
float CVehicleMovementHelicopter::GetEnginePedal()
{
	return 0.0f;
}

//------------------------------------------------------------------------
float CVehicleMovementHelicopter::GetEnginePower()
{
	float enginePower = (m_enginePower / m_enginePowerMax) * (1.0f - min(1.0f, m_damageActual));

	return enginePower;
}

//------------------------------------------------------------------------
bool CVehicleMovementHelicopter::RequestMovement(CMovementRequest& movementRequest)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );
 
	m_movementAction.isAI = true;

	if (!m_isEnginePowered)
		return false;

  if (m_HoverAnim && !(m_HoverAnim->GetAnimTime() < 1.0f - FLT_EPSILON))
  {
    m_HoverAnim->StartAnimation();
  }


	CryAutoCriticalSection lk(m_lock);


	if (movementRequest.HasLookTarget())
		m_aiRequest.SetLookTarget(movementRequest.GetLookTarget());
	else
		m_aiRequest.ClearLookTarget();

  if (movementRequest.HasBodyTarget())
    m_aiRequest.SetBodyTarget(movementRequest.GetBodyTarget());
  else
    m_aiRequest.ClearBodyTarget();

  if (movementRequest.HasDesiredBodyDirectionAtTarget())
    m_aiRequest.SetDesiredBodyDirectionAtTarget(movementRequest.GetDesiredBodyDirectionAtTarget());
  else
    m_aiRequest.ClearDesiredBodyDirectionAtTarget();

	if (movementRequest.HasMoveTarget())
	{
		Vec3 end( movementRequest.GetMoveTarget() );
		if(m_bExtendMoveTarget)
		{
			Vec3 entityPos = m_pEntity->GetWorldPos();
			Vec3 start(entityPos);
			Vec3 pos = ( end - start ) * 100.0f;
			pos +=start;
			m_aiRequest.SetMoveTarget( pos );
		}
		else
		{
			m_aiRequest.SetMoveTarget( end );
		}


    if (!m_isEnginePowered)
    {
      StartDriving(0);
    }
	}
	else
		m_aiRequest.ClearMoveTarget();

	float fDesiredSpeed = 0.0f;

	if (movementRequest.HasDesiredSpeed())
		fDesiredSpeed = movementRequest.GetDesiredSpeed();
	else
		m_aiRequest.ClearDesiredSpeed();

	if (movementRequest.HasForcedNavigation())
	{
		const Vec3 forcedNavigation = movementRequest.GetForcedNavigation();
		const Vec3 entityPos = m_pEntity->GetWorldPos();
		m_aiRequest.SetForcedNavigation(forcedNavigation);
		m_aiRequest.SetMoveTarget(entityPos+forcedNavigation.GetNormalizedSafe()*100.0f);
		
		if (fabsf(fDesiredSpeed) <= FLT_EPSILON)
			fDesiredSpeed = forcedNavigation.GetLength();
	}
	else
		m_aiRequest.ClearForcedNavigation();

	m_aiRequest.SetDesiredSpeed(fDesiredSpeed * m_EnginePerformance);

	m_pVehicle->NeedsUpdate(IVehicle::eVUF_AwakePhysics, false);

	return true;
		
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::Serialize(TSerialize ser, EEntityAspects aspects)
{
	CVehicleMovementBase::Serialize(ser, aspects);

	if (ser.GetSerializationTarget() == eST_Network)
	{
		m_netActionSync.Serialize(ser, aspects);
	}
	else
	{
		ser.Value("enginePower", m_enginePower);
		//ser.Value("turbulence", m_turbulence);

		ser.Value("steeringDamage",m_steeringDamage);
	}

	if (ser.IsReading() && !m_isEnginePowered)
	{
		m_arcade.Reset(m_pVehicle, m_pEntity);
	}
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::PostSerialize()
{
  if (m_pVehicle)
  {
    if (IEntity *entity = m_pVehicle->GetEntity())
    {
      if (entity->IsActivatedForUpdates() && !entity->IsHidden() && m_isEnginePowered)
      {
        StartExhaust();
      }
    }
  }
}

void CVehicleMovementHelicopter::OccupyWeaponSeats()
{
	uint32 seatCount = m_pVehicle->GetSeatCount();

	for (uint32 seatId = 1; !(seatId > seatCount); ++seatId)
	{
		IVehicleSeat *pSeat = m_pVehicle->GetSeatById(seatId);

		if (pSeat)
		{
			IVehicleSeatAction *pWeaponAction = pSeat->GetISeatActionWeapons();
			if (pWeaponAction)
			{
				pWeaponAction->ForceUsage();
			}

		}

	}

	m_pVehicle->GetGameObject()->EnableUpdateSlot(m_pVehicle, IVehicle::eVUS_AIControlled);
}

//------------------------------------------------------------------------
float CVehicleMovementHelicopter::GetDamageMult()
{
	float damageMult = (1.0f - min(1.0f, m_damage)) * 0.75f;
	damageMult += 0.25f;

	return damageMult;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::GetMemoryUsage( ICrySizer * pSizer ) const
{
	pSizer->AddObject(this, sizeof(*this)); 
	GetMemoryUsageInternal(pSizer);
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::GetMemoryUsageInternal( ICrySizer * pSizer ) const
{
	CVehicleMovementBase::GetMemoryUsageInternal(pSizer);
}

float CVehicleMovementHelicopter::GetRollSpeed() const
{
	return gf_PI * (m_CurrentSpeed / m_maxSpeed);
}

void CVehicleMovementHelicopter::CalculatePitch(const Ang3& worldAngles, const Vec3& desiredMoveDir, float currentSpeed2d, float desiredSpeed, float deltaTime)
{
	static const float kSpeedToPitchFactor = 100.0f;
	float desiredPitch = -static_cast<float>(__fsel(currentSpeed2d - desiredSpeed * 0.50f, currentSpeed2d, currentSpeed2d * kSpeedToPitchFactor)) / m_maxSpeed * gf_PI * 0.25f;
	desiredPitch += m_steeringDamage.x; 
	Limit(desiredPitch, -m_maxPitchAngle - m_steeringDamage.x, m_maxPitchAngle + m_steeringDamage.x);

	//additional pitch due to path slope is max 90 degrees 
	float upDown = GetAdditionalSlopePitch(desiredMoveDir); 
	float upDownAbs = fabsf(upDown);

	m_actionPitch = worldAngles.x + (desiredPitch - worldAngles.x + upDown) * deltaTime;
	Limit(m_actionPitch, -m_maxPitchAngle - upDownAbs * 0.1f , m_maxPitchAngle + upDownAbs);
}

void CVehicleMovementHelicopter::UpdateNetworkError(const Vec3& netPosition, const Quat& netRotation)
{
	SVehiclePhysicsStatus* physStatus = &m_physStatus[k_physicsThread];
	if(!physStatus->pos.IsZero(0.5f))
	{
		static const float maxDotError = cosf(0.5f*DEG2RAD(10.f));
		Vec3 posError = netPosition - physStatus->pos;
		float dotProduct = physStatus->q | netRotation;
		// CryWatch("posError=%.2f", posError.GetLength());
		if (posError.GetLengthSquared()>sqr(4.f) || dotProduct < maxDotError)
		{
			// Snap!
			m_pEntity->SetPos(netPosition);
			m_pEntity->SetRotation(netRotation);
			m_netPosAdjust.zero();
		}
		else
		{
			// Change the error smoothly
			m_netPosAdjust += (posError - m_netPosAdjust)*0.5f;
		}
	}
}

void CVehicleMovementHelicopter::InitMovementNoise( const CVehicleParams& noiseParams, CVTolNoise& noise )
{
	Vec3 amplitude(ZERO);
	Vec3 frequency(ZERO);
	Vec3 rotAmplitudeDeg(ZERO);
	Vec3 rotFrequency(ZERO);
	float startDamageRatio = 0.f;

	noiseParams.getAttr("amplitude", amplitude);
	noiseParams.getAttr("frequency", frequency);
	noiseParams.getAttr("rotAmplitude", rotAmplitudeDeg);
	noiseParams.getAttr("rotFrequency", rotFrequency);
	noiseParams.getAttr("startDamageRatio", startDamageRatio);

	noise.Init(amplitude, frequency, DEG2RAD(rotAmplitudeDeg), rotFrequency, startDamageRatio);
}

//------------------------------------------------------------------------
CNetworkMovementHelicopterCrysis2::CNetworkMovementHelicopterCrysis2()
: m_lookTarget(0.f, 0.f, 0.f)
, m_moveTarget(0.f, 0.f, 0.f)
, m_currentPos(0.f, 0.f, 0.f)
, m_desiredSpeed(0.0f)
{
	m_netSyncFlags = k_syncPos|k_syncLookMoveSpeed;
	m_currentRot.SetIdentity();
}

//------------------------------------------------------------------------
void CNetworkMovementHelicopterCrysis2::Write(CVehicleMovementHelicopter* pMovement)
{
	CMovementRequest& currentRequest = pMovement->m_aiRequest;
	m_lookTarget = currentRequest.HasLookTarget() ? currentRequest.GetLookTarget() : Vec3(0.f, 0.f, 0.f);
	m_moveTarget = currentRequest.HasMoveTarget() ? currentRequest.GetMoveTarget() : Vec3(0.f, 0.f, 0.f);
	m_desiredSpeed = currentRequest.HasDesiredSpeed() ? currentRequest.GetDesiredSpeed() : 0.f;
	const SVehiclePhysicsStatus& ps = pMovement->GetPhysicsStatus(CVehicleMovementBase::k_mainThread);
	m_currentPos = ps.pos;
	m_currentRot = ps.q;
}

//------------------------------------------------------------------------
void CNetworkMovementHelicopterCrysis2::Read(CVehicleMovementHelicopter* pMovement)
{
	if(!gEnv->bServer)
	{
		if (m_netSyncFlags & k_syncLookMoveSpeed)
		{
			CMovementRequest newRequest;
			if(!m_lookTarget.IsZero())
			{
				newRequest.SetLookTarget(m_lookTarget);
			}
			if(!m_moveTarget.IsZero())
			{
				newRequest.SetMoveTarget(m_moveTarget);
				newRequest.SetBodyTarget(m_moveTarget);
			}
			if(m_desiredSpeed != 0.f)
			{
				newRequest.SetDesiredSpeed(m_desiredSpeed);
			}
			pMovement->RequestMovement(newRequest);
		}

		if ((m_netSyncFlags&(k_controlPos|k_syncPos))==(k_controlPos|k_syncPos))
		{
			// Let the helicoptor fix the network error
			if (!m_currentPos.IsZero(0.5f))
				pMovement->UpdateNetworkError(m_currentPos, m_currentRot);
		}
	}	
}

//------------------------------------------------------------------------
void CNetworkMovementHelicopterCrysis2::Serialize(TSerialize ser, EEntityAspects aspects)
{
	CRY_ASSERT(ser.GetSerializationTarget() == eST_Network);

	NET_PROFILE_SCOPE("Movement", ser.IsReading());
	NET_PROFILE_SCOPE("NetMovementHelicopterCrysis2", ser.IsReading());

	if (m_netSyncFlags & k_syncLookMoveSpeed)
	{
		ser.Value("look", m_lookTarget, 'wrl3');
		ser.Value("move", m_moveTarget, 'wrl3');
		ser.Value("speed", m_desiredSpeed, 'iii');
	}
	if (m_netSyncFlags & k_syncPos)
	{
		ser.Value("pos", m_currentPos, 'wrld');
		ser.Value("rot", m_currentRot, 'ori1');
	}
}
