// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include <CryGame/GameUtils.h>

#include "IVehicleSystem.h"
#include "VehicleMovementTank.h"

#define THREAD_SAFE 1

//------------------------------------------------------------------------
CVehicleMovementTank::CVehicleMovementTank()
	: m_currPedal(0)
	, m_currSteer(0)
	, m_bOnTurnTurret(false)
	, m_bTurretTurning(false)
	, m_turretTurnSpeed(0.0f)
	, m_lastTurretTurnSpeed(0.0f)
{	
#ifdef DEBUG_TANK_AI
	ZeroStruct (m_debug);
#endif

	m_boostRegen = m_boostEndurance = 5.f;
	m_boostStrength = 4.f;
}

//------------------------------------------------------------------------
CVehicleMovementTank::~CVehicleMovementTank()
{
}

//------------------------------------------------------------------------
bool CVehicleMovementTank::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	if (inherited::Init(pVehicle, table))
	{
		m_audioControlIDs[eSID_TankTurnTurret] = CryAudio::StringToId("Play_abrams_cannon_turn");
		m_turretTurnRtpcId = CryAudio::StringToId("vehicle_rotation_speed");

		m_audioControlIDs[eSID_VehiclePrimaryWeapon] = CryAudio::StringToId("Play_w_tank_cannon_fire");
		m_audioControlIDs[eSID_VehicleStopPrimaryWeapon] = CryAudio::DoNothingTriggerId;
		m_audioControlIDs[eSID_VehicleSecondaryWeapon] = CryAudio::StringToId("Play_w_tank_machinegun_fire");
		m_audioControlIDs[eSID_VehicleStopSecondaryWeapon] = CryAudio::StringToId("Stop_w_tank_machinegun_fire");

		return true;
	}

	return false;
}

//------------------------------------------------------------------------
void CVehicleMovementTank::PostInit()
{
	inherited::PostInit();

	m_treadParts.clear();
	int nParts = m_pVehicle->GetPartCount();
	for (int i=0; i<nParts; ++i)
	{      
		IVehiclePart* pPart = m_pVehicle->GetPart(i);
		if (pPart->GetType() == IVehiclePart::eVPT_Tread)
		{ 
			m_treadParts.push_back(pPart);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleMovementTank::Reset()
{
	inherited::Reset();

#ifdef DEBUG_TANK_AI
	m_debug.targetPos.zero();
	m_debug.inputSpeed = 0.f;
#endif
}


//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementTank::ProcessMovement(const float deltaTime)
{ 
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	m_currPedal = fabsf(m_movementAction.power) > 0.05f ? m_movementAction.power : 0.f;

	if (m_movementAction.isAI)
	{
		m_damageRPMScale = 1.0f - 0.30f * m_damage;
	}
	else
	{
		float effectiveDamage = m_damage;
		m_damageRPMScale = GetWheelCondition();
		m_damageRPMScale *= 1.0f - 0.4f*effectiveDamage; 
	}

	inherited::ProcessMovement(deltaTime);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleMovementTank::OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params)
{
	inherited::OnEvent(event,params);

	if (event == eVME_TireBlown || event == eVME_TireRestored)
	{ 
		// tracked vehicles don't have blowable tires, these events are sent on track destruction
		int numTreads = m_treadParts.size();
		int treadsPrev = m_blownTires;
		m_blownTires = max(0, min(numTreads, event==eVME_TireBlown ? m_blownTires+1 : m_blownTires-1));

		// reduce speed based on number of destroyed treads
		if (m_blownTires != treadsPrev)
		{	
			SetEngineRPMMult(GetWheelCondition());
		}
	}
}

//------------------------------------------------------------------------
void CVehicleMovementTank::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	CryAutoCriticalSection netlk(m_networkLock);
	CryAutoCriticalSection lk(m_lock);
	CVehicleMovementBase::OnAction(actionId, activationMode, value);

	if (actionId == eVAI_RotateYaw || actionId == eVAI_RotatePitch)
	{
		m_bOnTurnTurret = true;
		m_turretTurnSpeed += abs(value);
	}
}

//------------------------------------------------------------------------
float CVehicleMovementTank::GetWheelCondition() const
{
	if (0 == m_blownTires)
		return 1.0f;

	int numTreads = m_treadParts.size();
	float treadCondition = (numTreads > 0) ? 1.0f - ((float)m_blownTires / numTreads) : 1.0f;

	return 0.6f + 0.4f*treadCondition;
}

#if ENABLE_VEHICLE_DEBUG
//----------------------------------------------------------------------------------
void CVehicleMovementTank::DebugDrawMovement(const float deltaTime)
{
	if (!IsProfilingMovement())
		return;

	if (g_pGameCVars->v_profileMovement!=3)
		inherited::DebugDrawMovement(deltaTime);

#ifdef DEBUG_TANK_AI
	if (m_movementAction.isAI)
	{
		IPhysicalEntity* pPhysics = GetPhysics();
		IRenderer* pRenderer = gEnv->pRenderer;
		static float color[4] = {1,1,1,1};
		float green[4] = {0,1,0,1};
		float red[4] = {1,0,0,1};
		static ColorB colRed(255,0,0,255);
		static ColorB colBlue(0,0,255,255);
		static ColorB colWhite(255,255,255,255);
		ColorB colGreen(0,255,0,255);
		ColorB col1(255,255,0,255);
		float y = 50.f, step1 = 15.f, step2 = 20.f, size=1.3f, sizeL=1.5f;

		IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		SAuxGeomRenderFlags flags = pAuxGeom->GetRenderFlags();
		SAuxGeomRenderFlags oldFlags = pAuxGeom->GetRenderFlags();
		flags.SetDepthWriteFlag(e_DepthWriteOff);
		flags.SetDepthTestFlag(e_DepthTestOff);
		pAuxGeom->SetRenderFlags(flags);

		SVehiclePhysicsStatus* physStatus = &m_physStatus[k_mainThread];
		Matrix33 bodyRot( physStatus->q );
		Matrix34 bodyPose( bodyRot, physStatus->centerOfMass );
		const Vec3 xAxis = bodyPose.GetColumn0();
		const Vec3 yAxis = bodyPose.GetColumn1();
		const Vec3 zAxis = bodyPose.GetColumn2();
		const Vec3 chassisPos = bodyPose.GetColumn3();

		if (!m_debug.targetPos.IsZero())
		{
			Vec3 pos = physStatus->centerOfMass;
			Vec3 vMove = m_debug.targetPos - physStatus->centerOfMass;
			vMove = vMove - (vMove.dot(zAxis)) * zAxis;
			vMove.normalize();
			vMove *= 3.f;

			// Draw move dir
			pAuxGeom->DrawSphere(pos, 0.1f, colGreen);
			pAuxGeom->DrawLine(pos, colGreen, pos + vMove, colGreen,4.0);
			pAuxGeom->DrawSphere(pos + vMove, 0.1f, colGreen);

			// Draw forward y-axis
			pAuxGeom->DrawLine(pos, colWhite, pos + 3.f*yAxis, colWhite, 4.0);

			// Draw Yaw input
			pAuxGeom->DrawLine(pos, colRed, pos + m_movementAction.rotateYaw*xAxis, colRed, 4.0);

			// Draw input speed
			pos = pos + 0.05f*xAxis;
			pAuxGeom->DrawLine(pos, colBlue, pos + (m_debug.inputSpeed*(3.f/5.f))*yAxis, colBlue, 4.0);
		}
	}
#endif

}
#endif

//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementTank::ProcessAI(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	float dt = max( deltaTime, 0.005f);
	SVehiclePhysicsStatus* physStatus = &m_physStatus[k_physicsThread];

	// Save the last steer state
	m_currSteer = m_movementAction.rotateYaw;

	m_movementAction.brake = false;
	m_movementAction.rotateYaw = 0.0f;
	m_movementAction.power = 0.0f;
	m_action.bHandBrake = 0;

	float inputSpeed = 0.0f;
	{
		if (m_aiRequest.HasDesiredSpeed())
			inputSpeed = m_aiRequest.GetDesiredSpeed();
		Limit(inputSpeed, -m_maxSpeed, m_maxSpeed);

		float distanceToPathEnd = m_aiRequest.GetDistanceToPathEnd();
		if (distanceToPathEnd>=0.f)
		{
			inputSpeed *= 0.2f + 0.8f*min(1.f, distanceToPathEnd / 5.f);
		}
	}

	Vec3 vMove(ZERO);
	{
		if (m_aiRequest.HasMoveTarget())
			vMove = m_aiRequest.GetMoveTarget() - physStatus->pos;
	}

	//start calculation
	if ( fabsf( inputSpeed ) < 0.0001f || m_tireBlownTimer > 1.5f )
	{
		// only the case to use a hand break
		m_movementAction.brake = true;
		m_action.bHandBrake = 1;
	}
	else
	{

		float currentAngleSpeed = RAD2DEG(-physStatus->w.z);
		const static float maxSteer = RAD2DEG(gf_PI/4.f); // fix maxsteer, shouldn't change  
		float currentSpeed = m_localSpeed.y;

		// calculate pedal
		static float accScale = 0.3f;
		m_movementAction.power = (inputSpeed - currentSpeed) * accScale;
		Limit( m_movementAction.power, -1.0f, 1.0f);

		// calculate angles
		Vec3 vMoveR = vMove * physStatus->q;
		Vec3 vFwd	= FORWARD_DIRECTION;

		vMoveR.z =0.0f;
		vMoveR.NormalizeSafe();

		if ( inputSpeed < 0.0 )	// when going back
		{
			vFwd = -vFwd;
			vMoveR = -vMoveR;
			currentAngleSpeed = -currentAngleSpeed;
		}

		float cosAngle = vFwd.Dot(vMoveR);
		float angle = RAD2DEG( acos_tpl(cosAngle));
		float absAngle = fabsf(angle);
		if ( vMoveR.x < 0.f )
			angle = -angle;

		if ( inputSpeed < 0.0 )	// when going back
			angle = -angle;

		static float steerAcc = 1.3f;
		m_movementAction.rotateYaw = fsgnf(angle) * sqrtf(clamp_tpl(fabsf(angle) * steerAcc / maxSteer, 0.f, 1.f)); 

		// if there is enough angle speed, we don't need to steer more
		if ( fabsf(currentAngleSpeed) > absAngle && inputSpeed*angle*currentAngleSpeed > 0.0f )
		{
			m_movementAction.rotateYaw = m_currSteer*0.95f; 
		}

		// tank can rotate at a point
		m_movementAction.power *= 1.f - 0.05f * min(20.f, absAngle);

		m_prevAngle =  angle;

	}
#ifdef DEBUG_TANK_AI
	m_debug.targetPos = m_aiRequest.HasMoveTarget() ? m_aiRequest.GetMoveTarget() : Vec3(ZERO);
	m_debug.inputSpeed = inputSpeed;
#endif
}

//------------------------------------------------------------------------
void CVehicleMovementTank::Update(const float deltaTime)
{
	inherited::Update(deltaTime); 

	CryAudio::ControlId id = m_audioControlIDs[eSID_TankTurnTurret];
	if (m_bOnTurnTurret && !m_bTurretTurning)
	{
		auto pIEntityAudioComponent = GetAudioProxy();
		if (pIEntityAudioComponent)
			pIEntityAudioComponent->ExecuteTrigger(id);
		m_bTurretTurning = true;
	}
	else if (!m_bOnTurnTurret && m_bTurretTurning)
	{
		auto pIEntityAudioComponent = GetAudioProxy();
		if (pIEntityAudioComponent)
			pIEntityAudioComponent->StopTrigger(id);
		m_bTurretTurning = false;
	}

	if (m_turretTurnSpeed > 0.0f)
	{
		Interpolate(m_lastTurretTurnSpeed, m_turretTurnSpeed, 0.5f, gEnv->pTimer->GetFrameTime());
		m_turretTurnSpeed = m_lastTurretTurnSpeed;
		const float maxSpeedForSound = 2.0f;

		auto pIEntityAudioComponent = GetAudioProxy();
		if (pIEntityAudioComponent)
			pIEntityAudioComponent->SetParameter(m_turretTurnRtpcId, min(m_turretTurnSpeed / maxSpeedForSound, 1.0f));
	}

	m_bOnTurnTurret = false;
	m_lastTurretTurnSpeed = m_turretTurnSpeed;
	m_turretTurnSpeed = 0.0f;

#if ENABLE_VEHICLE_DEBUG
	DebugDrawMovement(deltaTime);
#endif
}

//------------------------------------------------------------------------

void CVehicleMovementTank::GetMemoryUsage(ICrySizer * s) const
{
	s->Add(*this);
	s->AddContainer(m_treadParts);
	inherited::GetMemoryUsage(s);
}
