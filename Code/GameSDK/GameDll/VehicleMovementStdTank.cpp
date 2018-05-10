// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements movement type for tracked vehicles

-------------------------------------------------------------------------
History:
- 13:06:2005: Created by MichaelR

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include <CryGame/GameUtils.h>

#include "IVehicleSystem.h"
#include "VehicleMovementStdTank.h"

#define THREAD_SAFE 1
#define CVehicleMovementTank CVehicleMovementStdTank

//------------------------------------------------------------------------
CVehicleMovementTank::CVehicleMovementTank()
	: m_pedalSpeed(1.5f)
	, m_pedalThreshold(0.2f)
	, m_steerSpeedRelax(0.f)
	, m_steerLimit(1.f)
	, m_minFriction(0.0f)
	, m_maxFriction(0.0f)
	, m_latFricMin(1.0f)
	, m_latFricMinSteer(1.0f)
	, m_latFricMax(1.0f)
	, m_currentFricMin(1.0f)
	, m_latSlipMin(0.0f)
	, m_latSlipMax(5.0f)
	, m_currentSlipMin(0.0f)
	, m_currPedal(0)
	, m_currSteer(0)
	, m_steeringImpulseMin(0.f)
	, m_steeringImpulseMax(0.f)
	, m_steeringImpulseRelaxMin(0.f)
	, m_steeringImpulseRelaxMax(0.f)
{
	static_assert(CRY_ARRAY_COUNT(m_drivingWheels) == 2, "Invalid array size!");
	m_drivingWheels[0] = m_drivingWheels[1] = 0;

	m_steerSpeed = 0.f;

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
  if (!CVehicleMovementStdWheeled::Init(pVehicle, table))
    return false;
  
  MOVEMENT_VALUE("pedalSpeed", m_pedalSpeed);
  MOVEMENT_VALUE_OPT("pedalThreshold", m_pedalThreshold, table);
  MOVEMENT_VALUE("steerSpeed", m_steerSpeed);
  MOVEMENT_VALUE_OPT("steerSpeedRelax", m_steerSpeedRelax, table);
  MOVEMENT_VALUE_OPT("steerLimit", m_steerLimit, table);  
  MOVEMENT_VALUE("latFricMin", m_latFricMin);
  MOVEMENT_VALUE("latFricMinSteer", m_latFricMinSteer);
  MOVEMENT_VALUE("latFricMax", m_latFricMax);  
  MOVEMENT_VALUE("latSlipMin", m_latSlipMin);
  MOVEMENT_VALUE("latSlipMax", m_latSlipMax);

  MOVEMENT_VALUE_OPT("steeringImpulseMin", m_steeringImpulseMin, table);
  MOVEMENT_VALUE_OPT("steeringImpulseMax", m_steeringImpulseMax, table);
  MOVEMENT_VALUE_OPT("steeringImpulseRelaxMin", m_steeringImpulseRelaxMin, table);
  MOVEMENT_VALUE_OPT("steeringImpulseRelaxMax", m_steeringImpulseRelaxMax, table);

  m_maxSoundSlipSpeed = 10.f;  

	return true;
}

//------------------------------------------------------------------------
void CVehicleMovementTank::PostInit()
{
  CVehicleMovementStdWheeled::PostInit();
  
  for (size_t i=0; i<m_wheelParts.size(); ++i)
  { 
    if (m_wheelParts[i]->GetIWheel()->GetCarGeomParams()->bDriving)
      m_drivingWheels[m_wheelParts[i]->GetLocalTM(false).GetTranslation().x > 0.f] = m_wheelParts[i];
  }

  assert(m_drivingWheels[0] && m_drivingWheels[1]);

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
  CVehicleMovementStdWheeled::Reset();
}

//------------------------------------------------------------------------
void CVehicleMovementTank::SetLatFriction(float latFric)
{
  // todo: do calculation per-wheel?
  IPhysicalEntity* pPhysics = GetPhysics();  
  int numWheels = m_pVehicle->GetWheelCount();    
  
  pe_params_wheel params;
  params.kLatFriction = latFric;
  
  for (int i=0; i<numWheels; ++i)
  {    
    params.iWheel = i;    
    pPhysics->SetParams(&params, THREAD_SAFE);
  }

  m_latFriction = latFric;
}


//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementTank::ProcessMovement(const float deltaTime)
{ 
  CRY_PROFILE_FUNCTION( PROFILE_GAME );
  
  m_netActionSync.UpdateObject(this);

  CryAutoCriticalSection lk(m_lock);
  
  CVehicleMovementBase::ProcessMovement(deltaTime);

	if (!(m_actorId && m_isEnginePowered))
	{
		IPhysicalEntity* pPhysics = GetPhysics();

		if (m_latFriction != 1.3f)
		  SetLatFriction(1.3f);

		if (m_axleFriction != m_axleFrictionMax)
		  UpdateAxleFriction(0.f, false, deltaTime);
		
		m_action.bHandBrake = 1;
		m_action.pedal = 0;
		m_action.steer = 0;
		pPhysics->Action(&m_action, 1);
		return;
	}

  IPhysicalEntity* pPhysics = GetPhysics();
  MARK_UNUSED m_action.clutch;

  Matrix34 worldTM( m_PhysPos.q );
  worldTM.AddTranslation( m_PhysPos.pos );

	const Matrix34 invWTM = worldTM.GetInvertedFast();

  Vec3 localVel = invWTM.TransformVector(m_PhysDyn.v);
  Vec3 localW = invWTM.TransformVector(m_PhysDyn.w);
  float speed = m_PhysDyn.v.len();
  float speedRatio = min(1.f, speed/m_maxSpeed);

  float actionPedal = abs(m_movementAction.power) > 0.001f ? m_movementAction.power : 0.f;        

  // tank specific:
  // avoid steering input around 0.5 (ask Anton)
  float actionSteer = m_movementAction.rotateYaw;
  float absSteer = abs(actionSteer);
  float steerSpeed = (absSteer < 0.01f && abs(m_currSteer) > 0.01f) ? m_steerSpeedRelax : m_steerSpeed;
  
  if (steerSpeed == 0.f)
  {
    m_currSteer =	(float)sgn(actionSteer);
  }
  else
  { 
    if (m_movementAction.isAI)
    {
      m_currSteer = actionSteer;
    }
    else
    {
      m_currSteer += min(abs(actionSteer-m_currSteer), deltaTime*steerSpeed) * sgn(actionSteer-m_currSteer);        
    }
  }
  Limit(m_currSteer, -m_steerLimit, m_steerLimit);  
  
  if (abs(m_currSteer) > 0.0001f) 
  {
    // if steering, apply full throttle to have enough turn power    
    actionPedal = (float)sgn(actionPedal);
    
    if (actionPedal == 0.f) 
    {
      // allow steering-on-teh-spot only above maxReverseSpeed (to avoid sudden reverse of controls)
      const float maxReverseSpeed = -1.5f;
      actionPedal = max(0.f, min(1.f, 1.f-(localVel.y/maxReverseSpeed)));
      
      // todo
      float steerLim = 0.8f;
      Limit(m_currSteer, -steerLim*m_steerLimit, steerLim*m_steerLimit);
    }
  }

  if (!pPhysics->GetStatus(&m_vehicleStatus))
    return;
  
  int currGear = m_vehicleStatus.iCurGear - 1; // indexing for convenience: -1,0,1,2,..

  UpdateAxleFriction(m_movementAction.power, true, deltaTime);
  UpdateSuspension(deltaTime);   	
  
  float absPedal = abs(actionPedal);  
  
  // pedal ramping   
  if (m_pedalSpeed == 0.f)
    m_currPedal = actionPedal;
  else
  {
    m_currPedal += deltaTime * m_pedalSpeed * sgn(actionPedal - m_currPedal);  
    m_currPedal = clamp_tpl(m_currPedal, -absPedal, absPedal);
  }

  // only apply pedal after threshold is exceeded
  if (currGear == 0 && fabs_tpl(m_currPedal) < m_pedalThreshold) 
    m_action.pedal = 0;
  else
    m_action.pedal = m_currPedal;

  // change pedal amount based on damages
  float damageMul = 0.0f;
  {
		if (m_movementAction.isAI)
		{
			damageMul = 1.0f - 0.30f * m_damage; 
			m_action.pedal *= damageMul;
		}
		else
		{
			// request from Sten: damage shouldn't affect reversing so much.
			float effectiveDamage = m_damage;
			if(m_action.pedal < -0.1f)
				effectiveDamage = 0.4f * m_damage;

			m_action.pedal *= GetWheelCondition();
			damageMul = 1.0f - 0.7f*effectiveDamage; 
			m_action.pedal *= damageMul;
		}
  }

  // reverse steering value for backward driving
  float effSteer = m_currSteer * sgn(actionPedal);   
 
  // update lateral friction  
  float latSlipMinGoal = 0.f;
  float latFricMinGoal = m_latFricMin;
  
  if (abs(effSteer) > 0.01f && !m_movementAction.brake)
  {
    latSlipMinGoal = m_latSlipMin;
    
    // use steering friction, but not when countersteering
    if (sgn(effSteer) != sgn(localW.z))
      latFricMinGoal = m_latFricMinSteer;
  }

  Interpolate(m_currentSlipMin, latSlipMinGoal, 3.f, deltaTime);   
  
  if (latFricMinGoal < m_currentFricMin)
    m_currentFricMin = latFricMinGoal;
  else
    Interpolate(m_currentFricMin, latFricMinGoal, 3.f, deltaTime);

  float fractionSpeed = min(1.f, max(0.f, m_avgLateralSlip-m_currentSlipMin) / (m_latSlipMax-m_currentSlipMin));
  float latFric = fractionSpeed * (m_latFricMax-m_currentFricMin) + m_currentFricMin;

  if ( m_movementAction.brake && m_movementAction.isAI )
  {
    // it is natural for ai, apply differnt friction value while handbreaking
	 latFric = m_latFricMax;
  }

  if (latFric != m_latFriction)
  { 
    SetLatFriction(latFric);    
  }      
 
  const static float maxSteer = gf_PI/4.f; // fix maxsteer, shouldn't change  
  m_action.steer = m_currSteer * maxSteer;  
   
  if (m_steeringImpulseMin > 0.f && m_wheelContactsLeft != 0 && m_wheelContactsRight != 0)
  {  
    const float maxW = 0.3f*gf_PI;
    float steer = abs(m_currSteer)>0.001f ? m_currSteer : 0.f;    
    float desired = steer * maxW; 
    float curr = -localW.z;
    float err = desired - curr; // err>0 means correction to right 
    Limit(err, -maxW, maxW);

    if (abs(err) > 0.01f)
    { 
      float amount = m_steeringImpulseMin + speedRatio*(m_steeringImpulseMax-m_steeringImpulseMin);
      
      // bigger correction for relaxing
      if (desired == 0.f || (desired*curr>0 && abs(desired)<abs(curr))) 
        amount = m_steeringImpulseRelaxMin + speedRatio*(m_steeringImpulseRelaxMax-m_steeringImpulseRelaxMin);

      float corr = -err * amount * m_PhysDyn.mass * deltaTime;

      pe_action_impulse imp;
      imp.iApplyTime = 0;      
      imp.angImpulse = worldTM.GetColumn2() * corr;
      pPhysics->Action(&imp, THREAD_SAFE);
    }    
  }
  
	m_action.bHandBrake = (m_movementAction.brake) ? 1 : 0;	

  if (currGear > 0 && m_vehicleStatus.iCurGear < m_currentGear) 
  {
    // when shifted down, disengage clutch immediately to avoid power/speed dropdown
    m_action.clutch = 1.f;
  }
  
  pPhysics->Action(&m_action, 1);

  if (Boosting())  
    ApplyBoost(speed, 1.2f*m_maxSpeed*GetWheelCondition()*damageMul, m_boostStrength, deltaTime);  

  if (m_wheelContacts <= 1 && speed > 5.f)
  {
    ApplyAirDamp(DEG2RAD(20.f), DEG2RAD(10.f), deltaTime, THREAD_SAFE);
    UpdateGravity(-9.81f * 1.4f);
  }

	if (m_netActionSync.PublishActions( CNetworkMovementStdWheeled(this) ))
		CHANGED_NETWORK_STATE(m_pVehicle,  eEA_GameClientDynamic );
}

//////////////////////////////////////////////////////////////////////////
void CVehicleMovementTank::OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params)
{
	CVehicleMovementStdWheeled::OnEvent(event,params);

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

	CryAutoCriticalSection lk(m_lock);

	CVehicleMovementBase::OnAction(actionId, activationMode, value);

}

//------------------------------------------------------------------------
float CVehicleMovementTank::GetWheelCondition() const
{
  if (0 == m_blownTires)
    return 1.0f;

  int numTreads = m_treadParts.size();
  float treadCondition = (numTreads > 0) ? 1.0f - ((float)m_blownTires / numTreads) : 1.0f;

  // for a 2-tread vehicle, want to reduce speed by 40% for each tread shot out.
  return 1.0f - (0.8f * (1.0f - treadCondition));
}

#if ENABLE_VEHICLE_DEBUG
//----------------------------------------------------------------------------------
void CVehicleMovementTank::DebugDrawMovement(const float deltaTime)
{
  if (!IsProfilingMovement())
    return;

  CVehicleMovementStdWheeled::DebugDrawMovement(deltaTime);

  IPhysicalEntity* pPhysics = GetPhysics();
  IRenderer* pRenderer = gEnv->pRenderer;
//  float color[4] = {1,1,1,1};
//  float green[4] = {0,1,0,1};
  ColorB colRed(255,0,0,255);
//  float y = 50.f, step1 = 15.f, step2 = 20.f, size=1.3f, sizeL=1.5f;
}
#endif

//------------------------------------------------------------------------
bool CVehicleMovementTank::RequestMovement(CMovementRequest& movementRequest)
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

//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementTank::ProcessAI(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	float dt = max( deltaTime, 0.005f);

	m_movementAction.brake = false;
	m_movementAction.rotateYaw = 0.0f;
	m_movementAction.power = 0.0f;

	float inputSpeed = 0.0f;
	{
		if (m_aiRequest.HasDesiredSpeed())
			inputSpeed = m_aiRequest.GetDesiredSpeed();
		Limit(inputSpeed, -m_maxSpeed, m_maxSpeed);
	}

	Vec3 vMove(ZERO);
	{
		if (m_aiRequest.HasMoveTarget())
			vMove = ( m_aiRequest.GetMoveTarget() - m_PhysPos.pos ).GetNormalizedSafe();
	}

	//start calculation
	if ( fabsf( inputSpeed ) < 0.0001f || m_tireBlownTimer > 1.5f )
	{
		// only the case to use a hand break
		m_movementAction.brake = true;
	}
	else
	{

		Matrix33 entRotMatInvert( m_PhysPos.q );
		entRotMatInvert.Invert();
		float currentAngleSpeed = RAD2DEG(-m_PhysDyn.w.z);

		const static float maxSteer = RAD2DEG(gf_PI/4.f); // fix maxsteer, shouldn't change  
		Vec3 vVel = m_PhysDyn.v;
		Vec3 vVelR = entRotMatInvert * vVel;
		float currentSpeed =vVel.GetLength();
		vVelR.NormalizeSafe();
		if ( vVelR.Dot( FORWARD_DIRECTION ) < 0 )
			currentSpeed *= -1.0f;

		// calculate pedal
		static float accScale = 0.5f;
		m_movementAction.power = (inputSpeed - currentSpeed) * accScale;
		Limit( m_movementAction.power, -1.0f, 1.0f);

		// calculate angles
		Vec3 vMoveR = entRotMatInvert * vMove;
		Vec3 vFwd	= FORWARD_DIRECTION;

		vMoveR.z =0.0f;
		vMoveR.NormalizeSafe();

		if ( inputSpeed < 0.0 )	// when going back
		{
			vFwd *= -1.0f;
			vMoveR *= -1.0f;
			currentAngleSpeed *=-1.0f;
		}

		float cosAngle = vFwd.Dot(vMoveR);
		float angle = RAD2DEG( acos_tpl(cosAngle));
		if ( vMoveR.Dot( Vec3( 1.0f,0.0f,0.0f ) )< 0 )
			 angle = -angle;

//		int step =0;
		m_movementAction.rotateYaw = angle * 1.75f/ maxSteer; 

		// implementation 1. if there is enough angle speed, we don't need to steer more
		if ( fabsf(currentAngleSpeed) > fabsf(angle) && angle*currentAngleSpeed > 0.0f )
		{
			m_movementAction.rotateYaw = m_currSteer*0.995f; 
//			step =1;
		}

		// implementation 2. if we can guess we reach the distination angle soon, start counter steer.
		float predictDelta = inputSpeed < 0.0f ? 0.1f : 0.07f;
		float dict = angle + predictDelta * ( angle - m_prevAngle) / dt ;
		if ( dict*currentAngleSpeed<0.0f )
		{
			if ( fabsf( angle ) < 2.0f )
			{
				m_movementAction.rotateYaw = angle* 1.75f/ maxSteer; 
//				step =3;
			}
			else
			{
				m_movementAction.rotateYaw = currentAngleSpeed < 0.0f ? 1.0f : -1.0f; 
//				step =2;
			}
		}

		// implementation 3. tank can rotate at a point
		if ( fabs( angle )> 20.0f ) 
		{
			m_movementAction.power *=0.1f;
//			step =4;
		}

//		char buf[1024];
//		cry_sprintf(buf, "steering	%4.2f	%4.2f	%4.2f	%d\n", deltaTime,currentAngleSpeed, angle - m_prevAngle, step);
//		OutputDebugString( buf );
		m_prevAngle =  angle;

	}

}

//------------------------------------------------------------------------
void CVehicleMovementTank::Update(const float deltaTime)
{
	CVehicleMovementStdWheeled::Update(deltaTime); 

#if ENABLE_VEHICLE_DEBUG
	if (IsProfilingMovement())
	{
		if (m_steeringImpulseMin > 0.f && m_wheelContactsLeft != 0 && m_wheelContactsRight != 0)
		{  
			const Matrix34& worldTM = m_pVehicle->GetEntity()->GetWorldTM();   
			Vec3 localVel = worldTM.GetInvertedFast().TransformVector(m_statusDyn.v);
			Vec3 localW = worldTM.GetInvertedFast().TransformVector(m_statusDyn.w);
			float speed = m_statusDyn.v.len();
			float speedRatio = min(1.f, speed/m_maxSpeed);

			const float maxW = 0.3f*gf_PI;
			float steer = abs(m_currSteer)>0.001f ? m_currSteer : 0.f;    
			float desired = steer * maxW; 
			float curr = -localW.z;
			float err = desired - curr; // err>0 means correction to right 
			Limit(err, -maxW, maxW);

			if (abs(err) > 0.01f)
			{ 
				float amount = m_steeringImpulseMin + speedRatio*(m_steeringImpulseMax-m_steeringImpulseMin);

				float corr = -err * amount * m_statusDyn.mass * deltaTime;

				pe_action_impulse imp;
				imp.iApplyTime = 1;      
				imp.angImpulse = worldTM.GetColumn2() * corr;

				float color[] = {1,1,1,1};
				IRenderAuxText::Draw2dLabel(300,300,1.5f,color,false,"err: %.2f ", err);
				IRenderAuxText::Draw2dLabel(300,320,1.5f,color,false,"corr: %.3f", corr/m_statusDyn.mass);

				IRenderAuxGeom* pGeom = gEnv->pRenderer->GetIRenderAuxGeom();
				float len = 4.f * imp.angImpulse.len() / deltaTime / m_statusDyn.mass;
				Vec3 dir = (float)-sgn(corr) * worldTM.GetColumn0(); //imp.angImpulse.GetNormalized();
				pGeom->DrawCone(worldTM.GetTranslation()+Vec3(0,0,5)-(dir*len), dir, 0.5f, len, ColorB(128,0,0,255));        
			}
		}
	}    

	DebugDrawMovement(deltaTime);
#endif
}

//------------------------------------------------------------------------
void CVehicleMovementTank::UpdateSounds(const float deltaTime)
{ 
  CVehicleMovementStdWheeled::UpdateSounds(deltaTime);
}

//------------------------------------------------------------------------
void CVehicleMovementTank::UpdateSpeedRatio(const float deltaTime)
{  
  float speedSqr = max(sqr(m_avgWheelRot), m_statusDyn.v.len2());
  
  Interpolate(m_speedRatio, min(1.f, speedSqr/sqr(m_maxSpeed)), 5.f, deltaTime);
}

//------------------------------------------------------------------------
void CVehicleMovementTank::StopEngine()
{
  CVehicleMovementStdWheeled::StopEngine();
}

void CVehicleMovementTank::GetMemoryUsage(ICrySizer * pSizer) const
{
	pSizer->Add(*this);
	pSizer->AddObject(m_treadParts);
	CVehicleMovementStdWheeled::GetMemoryUsageInternal(pSizer);
}

#undef CVehicleMovementTank