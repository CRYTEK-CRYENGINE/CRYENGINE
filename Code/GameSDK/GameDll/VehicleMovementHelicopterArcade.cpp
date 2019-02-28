// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a standard helicopter movements

-------------------------------------------------------------------------
History:
	- Created by Stan Fichele

*************************************************************************/
#include "StdAfx.h"
#if 0
#include "Game.h"
#include "GameCVars.h"

#include "IMovementController.h"
#include "IVehicleSystem.h"
#include "VehicleMovementHelicopterArcade.h"
#include "VehicleActionLandingGears.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryGame/GameUtils.h>
#include "Vehicle/VehicleUtils.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include "Utility/CryWatch.h"

//------------------------------------------------------------------------
//	CVehicleMovementHelicopterArcade
//------------------------------------------------------------------------
CVehicleMovementHelicopterArcade::CVehicleMovementHelicopterArcade()
{
	m_pRotorHelper=NULL;
	m_damageActual=0.0f;
	m_steeringDamage=0.0f;
	m_turbulence=0.0f;
	m_pLandingGears=NULL;
	m_isTouchingGround=false;
	m_timeOnTheGround=50.0f;
	m_netActionSync.PublishActions( CNetworkMovementHelicopterArcade(this) );
}

//------------------------------------------------------------------------
bool CVehicleMovementHelicopterArcade::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	if (!CVehicleMovementBase::Init(pVehicle, table))
		return false;

	MOVEMENT_VALUE("engineWarmupDelay", m_engineWarmupDelay);
	MOVEMENT_VALUE("altitudeMax", m_altitudeMax);

	m_pRotorAnim = NULL;

	m_enginePower = 0.0f;

	m_isTouchingGround = false;
	m_timeOnTheGround = 50.0f;

	if (table.haveAttr("rotorPartName"))
		m_pRotorPart = m_pVehicle->GetPart(table.getAttr("rotorPartName"));
	else
		m_pRotorPart = NULL;

	ResetActions();

	//==============
	// Handling
	//==============
	CVehicleParams handlingParams = table.findChild("HandlingArcade");
	if (!handlingParams)
		return false;

	if (!m_arcade.Init(pVehicle, handlingParams))
		return false;

	m_maxSpeed = m_arcade.m_handling.maxSpeedForward;

	return true;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::PostInit()
{
	CVehicleMovementBase::PostInit();
	m_pRotorAnim = m_pVehicle->GetAnimation("rotor");
	m_pRotorHelper = m_pVehicle->GetHelper("rotorSmokeOut");

	for (int i = 0; i < m_pVehicle->GetActionCount(); i++)
	{
		IVehicleAction* pAction = m_pVehicle->GetAction(i);

		m_pLandingGears = CAST_VEHICLEOBJECT(CVehicleActionLandingGears, pAction);
		if (m_pLandingGears)
			break;
	}
	// This needs to be called from two places due to the init order on server and client
	Reset();
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::Release()
{
	CVehicleMovementBase::Release();
	delete this;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::Physicalize()
{
	CVehicleMovementBase::Physicalize();

	pe_simulation_params simParams;
	simParams.dampingFreefall = 0.01f;
	GetPhysics()->SetParams(&simParams);
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::PostPhysicalize()
{
	CVehicleMovementBase::PostPhysicalize();
	
	// This needs to be called from two places due to the init order on server and client
	if(!gEnv->pSystem->IsSerializingFile()) //don't do this while loading a savegame, it will overwrite the engine
		Reset();
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::Reset()
{
	CVehicleMovementBase::Reset();

	m_isEnginePowered = false;

	m_steeringDamage = 0.0f;
	m_damageActual = 0.0f;
	m_turbulence = 0.0f;

	ResetActions();

	m_enginePower = 0.0f;

	m_isTouchingGround = false;
	m_timeOnTheGround = 50.0f;

	m_arcade.Reset(m_pVehicle, m_pEntity);
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::ResetActions()
{
	m_inputAction.Reset();
}

//------------------------------------------------------------------------
bool CVehicleMovementHelicopterArcade::StartEngine(EntityId driverId)
{
	if (!CVehicleMovementBase::StartEngine(driverId))
	{
		return false;
	}

	if (m_pRotorAnim)
		m_pRotorAnim->StartAnimation();

	return true;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::StopEngine()
{
	if(m_enginePower > 0.0f)
	{
		m_isEngineGoingOff = true;
	}

	CVehicleMovementBase::StopEngine();
	ResetActions();

}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params)
{
	CVehicleMovementBase::OnEvent(event, params);

	if (event == eVME_DamageSteering)
	{
		float newSteeringDamage = (((float(cry_rand()) / float(CRY_RAND_MAX)) * 2.5f ) + 0.5f);
		m_steeringDamage = max(m_steeringDamage, newSteeringDamage);
	}
	else if (event == eVME_Repair)
	{
		m_steeringDamage = min(m_steeringDamage, params.fValue);

		if(params.fValue < 0.25)
		{
			m_damageActual = 0.0f;
			m_damage = 0.0f;
		}
	}
	else if (event == eVME_GroundCollision)
	{
		const float stopOver = 1.0f;

		m_isTouchingGround = true;
	}
	else if (event == eVME_Damage)
	{
		const float stopOver = 1.0f;

		m_damage = params.fValue;
	}
	else if (event == eVME_WarmUpEngine)
	{
		m_enginePower = 1.0f;
	}
	else if (event == eVME_PlayerSwitchView)
	{
	}
	else if (event == eVME_Turbulence)
	{
		m_turbulence = max(m_turbulence, params.fValue);
	}
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	CryAutoCriticalSection netlk(m_networkLock);
	CryAutoCriticalSection lk(m_lock);

	CVehicleMovementBase::OnAction(actionId, activationMode, value);
	
	m_inputAction.forwardBack = m_movementAction.power;

	if (actionId==eVAI_StrafeLeft)
	{
		m_inputAction.leftRight = -value; 
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

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::ProcessActions(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	UpdateDamages(deltaTime);
	UpdateEngine(deltaTime);
	
	if (!m_pVehicle->GetStatus().doingNetPrediction)
	{
		if (m_netActionSync.PublishActions( CNetworkMovementHelicopterArcade(this) ))
			m_pVehicle->GetGameObject()->ChangedNetworkState(CNetworkMovementHelicopterArcade::CONTROLLED_ASPECT);
	}
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
void CVehicleMovementHelicopterArcade::ProcessAI(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );
	
	// it's useless to progress further if the engine has yet to be turned on
	if (!m_isEnginePowered)
		return;

	//========================
	// TODO !
	//========================
}

//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementHelicopterArcade::ProcessMovement(const float dt)
{
  CRY_PROFILE_FUNCTION( PROFILE_GAME );

	IPhysicalEntity* pPhysics = GetPhysics();
	if (!pPhysics)
		return;
	
	CryAutoCriticalSection lk(m_lock);
	m_netActionSync.UpdateObject(this);

	if (!m_isEnginePowered)
		return;

	CVehicleMovementBase::ProcessMovement(dt);

	const bool haveDriver=(m_actorId!=0);

	/////////////////////////////////////////////////////////////
	// Pass on the movement request to the active physics handler
	// NB: m_physStatus is update by this call
	SVehiclePhysicsHelicopterProcessParams params;
	params.pPhysics = pPhysics;
	params.pPhysStatus = &m_physStatus[k_physicsThread];
	params.pInputAction = &m_inputAction;
	params.dt = dt;
	params.haveDriver = haveDriver;
	params.isAI = m_movementAction.isAI;
	params.aiRequiredVel = Vec3(0.f);
	m_arcade.ProcessMovement(params);
		
	//===============================================
	// Commit the velocity back to the physics engine
	//===============================================
	pe_action_set_velocity setVelocity;
	setVelocity.v = m_physStatus[k_physicsThread].v;
	setVelocity.w = m_physStatus[k_physicsThread].w;
	pPhysics->Action(&setVelocity, 1);
	/////////////////////////////////////////////////////////////
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::UpdateDamages(float deltaTime)
{
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::UpdateEngine(float deltaTime)
{
	// will update the engine power up to the maximum according to the ignition time

	if (m_isEnginePowered && !m_isEngineGoingOff)
	{
		if (m_enginePower < 1.f)
		{
			m_enginePower += deltaTime * (1.f / m_engineWarmupDelay);
			m_enginePower = min(m_enginePower, 1.f);
		}
	}
	else
	{
		if (m_enginePower >= 0.0f)
		{
			m_enginePower -= deltaTime * (1.f / m_engineWarmupDelay);
			if (m_damageActual > 0.0f)
				m_enginePower *= 1.0f - min(1.0f, m_damageActual);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::Update(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	CVehicleMovementBase::Update(deltaTime);

	CryAutoCriticalSection lk(m_lock);

	if (m_isTouchingGround)
	{
		m_timeOnTheGround += deltaTime;
		m_isTouchingGround = false;
	}
	else
	{
		m_timeOnTheGround = 0.0f;
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
			if(m_pRotorAnim)
			{
				m_pRotorAnim->StopAnimation();
			}

			m_isEngineGoingOff = false;
		}	
	}

	if(m_pRotorAnim)
	{
		m_pRotorAnim->SetSpeed(m_enginePower);
	}

	IActor* pActor = m_pActorSystem->GetActor(m_actorId);

#if ENABLE_VEHICLE_DEBUG
	int profile = g_pGameCVars->v_profileMovement;
	if ((profile == 1 && pActor && pActor->IsClient()) || profile == 2)
	{
		IRenderer* pRenderer = gEnv->pRenderer;
		float color[4] = {1,1,1,1};

		Ang3 localAngles = m_pEntity->GetWorldAngles();

		const Vec3& velocity = m_physStatus[k_mainThread].v;
		const Vec3& angVelocity = m_physStatus[k_mainThread].w;
	
		SVehiclePhysicsStatus* physStatus = &m_physStatus[k_mainThread];

		const Vec3 xAxis = physStatus->q.GetColumn0();
		const Vec3 yAxis = physStatus->q.GetColumn1();
		const Vec3 zAxis = physStatus->q.GetColumn2();
		const Vec3 pos = physStatus->centerOfMass;
	
		float x=30.f, y=30.f, dy=30.f;
		pRenderer->Draw2dLabel(x, y, 2.0f, color, false, "Helicopter movement"); y+=dy;
		pRenderer->Draw2dLabel(x, y+=dy, 2.0f, color, false, "Action: forwardBack: %.2f", m_inputAction.forwardBack);
		pRenderer->Draw2dLabel(x, y+=dy, 2.0f, color, false, "Action: upDown: %.2f", m_inputAction.upDown);
		pRenderer->Draw2dLabel(x, y+=dy, 2.0f, color, false, "Action: yaw: %.2f", m_inputAction.yaw);
		pRenderer->Draw2dLabel(x, y+=dy, 2.0f, color, false, "Action: leftRight: %.2f", m_inputAction.leftRight);

		// Forward, and left-right vectors in the xy plane
		const Vec3 up(0.f, 0.f, 1.f);
		Vec3 forward(yAxis.x, yAxis.y, 0.f); 
		forward.normalize();
		Vec3 right(forward.y, -forward.x, 0.f);

		Vec3 f = forward;
		pRenderer->Draw2dLabel(x, y+=dy, 2.0f, color, false, "Forward: %.2f, %.2f, %.2f ", f.x, f.y, f.z);
		f = right;
		pRenderer->Draw2dLabel(x, y+=dy, 2.0f, color, false, "Right: %.2f, %.2f, %.2f ", f.x, f.y, f.z);

		// Aux Renderer
		IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		SAuxGeomRenderFlags flags = pAuxGeom->GetRenderFlags();
		SAuxGeomRenderFlags oldFlags = pAuxGeom->GetRenderFlags();
		flags.SetDepthWriteFlag(e_DepthWriteOff);
		flags.SetDepthTestFlag(e_DepthTestOff);
		pAuxGeom->SetRenderFlags(flags);

		ColorB colRed(255,0,0,255);
		ColorB colGreen(0,255,0,255);
		ColorB colBlue(0,0,255,255);
		ColorB col1(255,255,0,255);

		pAuxGeom->DrawSphere(pos, 0.1f, colGreen);
		pAuxGeom->DrawLine(pos, colGreen, pos + forward, colGreen);
		pAuxGeom->DrawLine(pos, colGreen, pos + right, colBlue);
	}
#endif
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::OnEngineCompletelyStopped()
{
	CVehicleMovementBase::OnEngineCompletelyStopped();

	RemoveSurfaceEffects();
}

//------------------------------------------------------------------------
float CVehicleMovementHelicopterArcade::GetEnginePedal()
{
	return fabsf(m_inputAction.forwardBack);
}

//------------------------------------------------------------------------
float CVehicleMovementHelicopterArcade::GetEnginePower()
{
	float enginePower = (m_enginePower) * (1.0f - min(1.0f, m_damageActual));

	return enginePower;
}

//------------------------------------------------------------------------
SVehicleMovementAction CVehicleMovementHelicopterArcade::GetMovementActions()
{
	// This is used for vehicle client prediction, the mapping of SHelicopterAction variables to SVehicleMovementAction isn't important
	// but it must be consistent with the RequestActions function
	SVehicleMovementAction actions;
	actions.power = m_inputAction.upDown;
	actions.rotatePitch = m_inputAction.forwardBack;
	actions.rotateRoll = m_inputAction.leftRight;
	actions.rotateYaw = m_inputAction.yaw;
	return actions;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::RequestActions(const SVehicleMovementAction& movementAction)
{
	m_inputAction.upDown = movementAction.power;
	m_inputAction.forwardBack = movementAction.rotatePitch;
	m_inputAction.leftRight = movementAction.rotateRoll;
	m_inputAction.yaw = movementAction.rotateYaw;
}

//------------------------------------------------------------------------
bool CVehicleMovementHelicopterArcade::RequestMovement(CMovementRequest& movementRequest)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );
 
	m_movementAction.isAI = true;
	if (!m_isEnginePowered)
		return false;

	CryAutoCriticalSection lk(m_lock);

	if (movementRequest.HasLookTarget())
		m_aiRequest.SetLookTarget(movementRequest.GetLookTarget());
	else
		m_aiRequest.ClearLookTarget();

	if (movementRequest.HasMoveTarget())
	{
		Vec3 entityPos = m_pEntity->GetWorldPos();
		Vec3 start(entityPos);
		Vec3 end( movementRequest.GetMoveTarget() );
		Vec3 pos = ( end - start ) * 100.0f;
		pos +=start;
		m_aiRequest.SetMoveTarget( pos );
	}
	else
		m_aiRequest.ClearMoveTarget();

	if (movementRequest.HasDesiredSpeed())
		m_aiRequest.SetDesiredSpeed(movementRequest.GetDesiredSpeed());
	else
		m_aiRequest.ClearDesiredSpeed();

	if (movementRequest.HasForcedNavigation())
	{
		Vec3 entityPos = m_pEntity->GetWorldPos();
		m_aiRequest.SetForcedNavigation(movementRequest.GetForcedNavigation());
		m_aiRequest.SetDesiredSpeed(movementRequest.GetForcedNavigation().GetLength());
		m_aiRequest.SetMoveTarget(entityPos+movementRequest.GetForcedNavigation().GetNormalizedSafe()*100.0f);
	}
	else
		m_aiRequest.ClearForcedNavigation();

	return true;
		
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::Serialize(TSerialize ser, EEntityAspects aspects)
{
	CVehicleMovementBase::Serialize(ser, aspects);

	if ((ser.GetSerializationTarget() == eST_Network) &&(aspects & CNetworkMovementHelicopterArcade::CONTROLLED_ASPECT))
	{
		m_netActionSync.Serialize(ser, aspects);
		if (ser.IsReading())
		{
			IPhysicalEntity *pPhysics = GetPhysics();
			if (pPhysics)
			{
				pe_action_awake awake;
				pPhysics->Action(&awake);
			}
		}
	}
	else if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.Value("enginePower", m_enginePower);
		ser.Value("timeOnTheGround", m_timeOnTheGround);
		ser.Value("turbulence", m_turbulence);

		ser.Value("steeringDamage",m_steeringDamage);
	
		ser.Value("upDown", m_inputAction.upDown);
		ser.Value("forwardBack", m_inputAction.forwardBack);
		ser.Value("leftRight", m_inputAction.leftRight);
		ser.Value("yaw", m_inputAction.yaw);
		ser.Value("timer", m_arcade.m_chassis.timer);

		if (ser.IsReading())
			m_isTouchingGround = m_timeOnTheGround > 0.0f;
	}
};

//------------------------------------------------------------------------
SVehicleNetState CVehicleMovementHelicopterArcade::GetVehicleNetState()
{
	SVehicleNetState state;
	static_assert(sizeof(m_pArcade->m_chassis) <= sizeof(SVehicleNetState), "Invalid type size!");
	memcpy(&state, &m_pArcade->m_chassis, sizeof(m_pArcade->m_chassis));
	return state;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::SetVehicleNetState(const SVehicleNetState& state)
{
	memcpy(&m_pArcade->m_chassis, &state, sizeof(m_pArcade->m_chassis));
}

//------------------------------------------------------------------------
float CVehicleMovementHelicopterArcade::GetDamageMult()
{
	float damageMult = (1.0f - min(1.0f, m_damage)) * 0.75f;
	damageMult += 0.25f;

	return damageMult;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::GetMemoryUsage( ICrySizer * pSizer ) const
{
	pSizer->AddObject(this, sizeof(*this)); 
	GetMemoryUsageInternal(pSizer);
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopterArcade::GetMemoryUsageInternal( ICrySizer * pSizer ) const
{
	CVehicleMovementBase::GetMemoryUsageInternal(pSizer);
}



//------------------------------------------------------------------------
//	CNetworkMovementHelicopterArcade
//------------------------------------------------------------------------
CNetworkMovementHelicopterArcade::CNetworkMovementHelicopterArcade()
{
	action.Reset();
}

//------------------------------------------------------------------------
CNetworkMovementHelicopterArcade::CNetworkMovementHelicopterArcade(CVehicleMovementHelicopterArcade *pMovement)
{
	action = pMovement->m_inputAction;
}

//------------------------------------------------------------------------
void CNetworkMovementHelicopterArcade::UpdateObject(CVehicleMovementHelicopterArcade *pMovement)
{
	pMovement->m_inputAction = action;
}

//------------------------------------------------------------------------
void CNetworkMovementHelicopterArcade::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget()==eST_Network && aspects&CONTROLLED_ASPECT)
	{
		NET_PROFILE_SCOPE("NetMovementHelicopterArcade", ser.IsReading());

		ser.Value("upDown", action.upDown, 'vPow');
		ser.Value("forwardBack", action.forwardBack, 'vPow');
		ser.Value("leftRight", action.leftRight, 'vPow');
		ser.Value("yaw", action.yaw, 'vPow');
	}
}
#endif //0
