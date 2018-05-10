// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a standard boat movement

-------------------------------------------------------------------------
History:
- 30:05:2005: Created by Michael Rauh

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"

#include "IVehicleSystem.h"
#include "VehicleMovementStdBoat.h"
#include <CryAISystem/IAgent.h>
#include "Network/NetActionSync.h"


//------------------------------------------------------------------------
CVehicleMovementStdBoat::CVehicleMovementStdBoat()
: m_pSplashPos(NULL)
, m_velMax( 15 )
, m_velMaxReverse( 10 )
, m_accel( 5 )
, m_turnRateMax( 1 )
, m_turnAccel( 1 )
, m_cornerForceCoeff( 1 )
, m_turnAccelCoeff( 2 )
, m_accelCoeff( 2 )
, m_pushTilt( 0 )
, m_pushOffset(ZERO)
, m_cornerTilt( 0 )
, m_cornerOffset(ZERO)
, m_turnDamping( 0 )
, m_massOffset(ZERO)
, m_pedalLimitReverse(1.f)
, m_waveIdleStrength(ZERO)
, m_waveSpeedMult(0.f)
, m_turnVelocityMult(1.f)
, m_inWater(false)
, m_waveRandomMult(1.f)
, m_velLift(0.f)
, m_waterDensity(100.f)
, m_lifted(false)
, m_bNetSync(true)
, m_prevAngle(0.0f)
, m_pWaveEffect(NULL)
, m_factorMaxSpeed(1.f)
, m_factorAccel(1.f)
{ 
  m_lateralDamping = 0.f;
	m_rollAccel = 0.f;
  m_netActionSync.PublishActions( CNetworkMovementStdBoat(this) );
}

//------------------------------------------------------------------------
CVehicleMovementStdBoat::~CVehicleMovementStdBoat()
{
}

//------------------------------------------------------------------------
bool CVehicleMovementStdBoat::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
  if (!CVehicleMovementBase::Init(pVehicle, table))
    return false;
  
  MOVEMENT_VALUE("velMax", m_velMax);      
  MOVEMENT_VALUE("velMaxReverse", m_velMaxReverse);
  MOVEMENT_VALUE("acceleration", m_accel);      
  MOVEMENT_VALUE("accelerationVelMax", m_accelVelMax);      
  MOVEMENT_VALUE("accelerationMultiplier", m_accelCoeff);         
  MOVEMENT_VALUE("pushTilt", m_pushTilt);     
  MOVEMENT_VALUE("turnRateMax", m_turnRateMax);
  MOVEMENT_VALUE("turnAccel", m_turnAccel);      
  MOVEMENT_VALUE("cornerForce", m_cornerForceCoeff);      
  MOVEMENT_VALUE("cornerTilt", m_cornerTilt);    
  MOVEMENT_VALUE("turnDamping", m_turnDamping); 
  MOVEMENT_VALUE("turnAccelMultiplier", m_turnAccelCoeff);
  MOVEMENT_VALUE_OPT("rollAccel", m_rollAccel, table);
  MOVEMENT_VALUE_OPT("pedalLimitReverse", m_pedalLimitReverse, table);
  MOVEMENT_VALUE_OPT("turnVelocityMult", m_turnVelocityMult, table);
  MOVEMENT_VALUE_OPT("velLift", m_velLift, table);
  MOVEMENT_VALUE_OPT("lateralDamping", m_lateralDamping, table);
  
  table.getAttr("waveIdleStrength", m_waveIdleStrength);
  table.getAttr("waveSpeedMult", m_waveSpeedMult);
  
  if (table.haveAttr("cornerHelper"))
	{
		if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(table.getAttr("cornerHelper")))
			m_cornerOffset = pHelper->GetVehicleSpaceTranslation();
	}

  if (table.haveAttr("pushHelper"))
	{
		if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(table.getAttr("pushHelper")))
			m_pushOffset = pHelper->GetVehicleSpaceTranslation();
	}

  // compute inertia [assumes box]
  AABB bbox;	
  
  IVehiclePart* pMassPart = pVehicle->GetPart("mass");
  if (!pMassPart)
    pMassPart = pVehicle->GetPart("massBox");
	
  if (pMassPart)
	{
		bbox = pMassPart->GetLocalBounds();
	}
	else
	{
		GameWarning("[CVehicleMovementStdBoat]: initialization: No \"mass\" geometry found!");
		m_pEntity->GetLocalBounds(bbox);
	}

  m_maxSpeed = m_velMax;
	float mass = pVehicle->GetMass();
  
  float width = bbox.max.x - bbox.min.x;
  float length = bbox.max.y - bbox.min.y;
  float height = bbox.max.z - bbox.min.z;
  m_Inertia.x = mass * (sqr(length)+ sqr(height)) / 12;
  m_Inertia.y = mass * (sqr(width) + sqr(height)) / 12;
  m_Inertia.z = mass * (sqr(width) + sqr(length)) / 12;
  
  m_massOffset = bbox.GetCenter();

  //CryLog("[StdBoat movement]: got mass offset (%f, %f, %f)", m_massOffset.x, m_massOffset.y, m_massOffset.z);

	m_pSplashPos = m_pVehicle->GetHelper("splashPos");

	if (m_pSplashPos)
		m_lastWakePos = m_pSplashPos->GetWorldSpaceTranslation();
	else
		m_lastWakePos = m_pVehicle->GetEntity()->GetWorldTM().GetTranslation();

	const char* waveEffect = "";
	MOVEMENT_VALUE_OPT("waveEffect", &waveEffect, table);
	m_pWaveEffect = gEnv->pParticleManager->FindEffect(waveEffect, "MovementStdBoat");

  m_waveTimer = cry_random(0.0f, gf_PI);
  m_diving = false;
  m_wakeSlot = -1;   
  m_waveSoundPitch = 0.f;
  m_rpmPitchDir = 0;
  m_waveSoundAmount = 0.1f;

  // AI related
  m_prevAngle = 0.0f;
  
	m_factorMaxSpeed = 1.f;
	m_factorAccel = 1.f;

  return true;
}

//------------------------------------------------------------------------
void CVehicleMovementStdBoat::Reset()
{
  CVehicleMovementBase::Reset();

  Lift(false);    
  m_waveTimer = cry_random(0.0f, gf_PI);

  m_prevAngle = 0.0f;
  m_diving = false;  
  m_wakeSlot = -1;
  m_rpmPitchDir = 0;
  m_waveSoundPitch = 0.f;
  m_inWater = false;  
	m_factorMaxSpeed = 1.f;
	m_factorAccel = 1.f;
}

//------------------------------------------------------------------------
void CVehicleMovementStdBoat::Release()
{
  CVehicleMovementBase::Release();
  delete this;
}


//------------------------------------------------------------------------
void CVehicleMovementStdBoat::PostPhysicalize()
{
  CVehicleMovementBase::PostPhysicalize();

	// removed fetching the centre of mass here; with MT physics it is not guaranteed to be correct
	//	at this point. Since boats only have one part with mass (the mass box) using the centre of that bbox is fine.

  if (GetMovementType() == eVMT_Sea)
  {
    pe_params_foreign_data pfd; 
    pfd.iForeignFlagsOR = PFF_UNIMPORTANT;    
    GetPhysics()->SetParams(&pfd);
  }  
}


//------------------------------------------------------------------------
void CVehicleMovementStdBoat::OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params)
{
  CVehicleMovementBase::OnEvent(event, params);

	switch (event)
	{
	case eVME_BecomeVisible:
		{     
			if (g_pGameCVars->v_rockBoats || m_actorId) 
				m_pVehicle->NeedsUpdate(IVehicle::eVUF_AwakePhysics);
			break;
		}
	case eVME_SetFactorMaxSpeed:
		{     
			m_factorMaxSpeed = max(params.fValue,0.001f);
			break;
		}
	case eVME_SetFactorAccel:
		{     
			m_factorAccel = params.fValue;
			break;
		}
	}
}
//------------------------------------------------------------------------
void CVehicleMovementStdBoat::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{

	CryAutoCriticalSection netlk(m_networkLock);
	CryAutoCriticalSection lk(m_lock);

	CVehicleMovementBase::OnAction(actionId, activationMode, value);

}

//------------------------------------------------------------------------
void CVehicleMovementStdBoat::Update(const float deltaTime)
{
	CVehicleMovementBase::Update(deltaTime);

	SetAnimationSpeed(eVMA_Engine, abs(m_rpmScaleSgn));
	if (m_inWater)
	{ 
		SetSoundParam(eSID_Run, "slip", 0.2f*abs(m_localSpeed.x)); 
	}

#if ENABLE_VEHICLE_DEBUG
	if (IsProfilingMovement() && g_pGameCVars->v_profileMovement != 2)
	{
		IEntity* pEntity = m_pVehicle->GetEntity();
		const Matrix34& wTM = pEntity->GetWorldTM();  
		Matrix34 wTMInv = wTM.GetInvertedFast();
		
		const SVehiclePhysicsStatus* physStatus = &m_physStatus[k_mainThread];
		Vec3 localW = physStatus->q * physStatus->w;

		float speed = physStatus->v.len2() > 0.001f ? physStatus->v.len() : 0.f;    
		float speedRatio = min(1.f, speed/(m_maxSpeed*m_factorMaxSpeed));  
		float absPedal = abs(m_movementAction.power);
		float absSteer = abs(m_movementAction.rotateYaw);
		
		static const float fSubmergedMin = 0.01f;
		static const float fWaterLevelMaxDiff = 0.15f; // max allowed height difference between propeller center and water level

		Vec3 worldPropPos = wTM * m_pushOffset;  
		float waterLevelWorld = gEnv->p3DEngine->GetWaterLevel( &worldPropPos );
		float fWaterLevelDiff = worldPropPos.z - waterLevelWorld;  

		// wave stuff 
		float waveFreq = 1.f;
		waveFreq += 3.f*speedRatio;

		float kx = m_waveIdleStrength.x*(m_waveRandomMult+0.3f) * (1.f-speedRatio + m_waveSpeedMult*speedRatio);
		float ky = m_waveIdleStrength.y * (1.f - 0.5f*absPedal - 0.5f*absSteer);
		Vec3 waveLoc = m_massOffset;
		waveLoc.y += speedRatio*min(0.f, m_pushOffset.y-m_massOffset.y);
		waveLoc = wTM * waveLoc;

		static float color[4] = {1,1,1,1};    
		float colorRed[4] = {1,0,0,1};
		float colorGreen[4] = {0,1,0,1};
		float y=50.f, step1=15.f, step2=20.f, size1=1.3f, size2=1.5f;

		IRenderAuxText::Draw2dLabel(5.0f, y, size2, color, false, "Boat movement");
		IRenderAuxText::Draw2dLabel(5.0f, y += step2, size1, color, false, "Speed: %.1f (%.1f km/h)", speed, speed*3.6f);
		IRenderAuxText::Draw2dLabel(5.0f, y += step1, size1, color, false, "LocalW.z norm: %.2f", abs(localW.z) / m_turnRateMax);
		if (m_velLift > 0.f)
		{
			IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size1, m_lifted ? colorGreen : color, false, m_lifted ? "Lifted" : "not lifted");
			//pRenderer->Draw2dLabel(5.0f,  y+=step2, size1, color, false, "Impulse lift: %.0f", liftImp.impulse.len());               
		}    
		IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size1, physStatus->submergedFraction > fSubmergedMin ? color : colorRed, false, "Submerged: %.2f", physStatus->submergedFraction);
		IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size1, fWaterLevelDiff < fWaterLevelMaxDiff ? color : colorRed, false, "WaterLevel: %.2f (max: %.2f)", fWaterLevelDiff, fWaterLevelMaxDiff);

		IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size2, color, false, "Driver input");
		IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size1, color, false, "power: %.2f", m_movementAction.power);
		IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size1, color, false, "steer: %.2f", m_movementAction.rotateYaw); 

		IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size2, color, false, "Propelling");
		//IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size1, color, false, "turnAccel (norm/real): %.2f / %.2f", turnAccelNorm, turnAccel);         
		//IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size1, color, false, "Impulse acc: %.0f", linearImp.impulse.len());         
		//IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size1, color, false, "Impulse steer/damp: %.0f", angularImp.angImpulse.len()); 
		//IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size1, color, false, "Impulse corner: %.0f", dampImp.impulse.len());

		IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size2, color, false, "Waves");
		IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size1, color, false, "timer: %.1f", m_waveTimer); 
		IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size1, color, false, "frequency: %.2f", waveFreq); 
		IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size1, color, false, "random: %.2f", m_waveRandomMult); 
		IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size1, color, false, "kX: %.2f", kx);     
		IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size1, color, false, "kY: %.2f", ky); 

		if (Boosting())
			IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size1, color, false, "Boost: %.2f", m_boostCounter);

		IRenderAuxGeom* pGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		ColorB colorB(0,255,0,255);

		IRenderAuxText::DrawLabelF(worldPropPos, 1.3f, "WL: %.2f", waterLevelWorld);

		pGeom->DrawSphere(worldPropPos, 0.15f, colorB);
		pGeom->DrawSphere(waveLoc, 0.25f, colorB);
		pGeom->DrawLine(waveLoc, colorB, waveLoc+Vec3(0,0,2), colorB);

		// impulses
		//DrawImpulse(linearImp, Vec3(0,0,1), 3.f/deltaTime, ColorB(255,0,0,255));
		//DrawImpulse(angularImp, Vec3(0,0,1), 2.f/deltaTime, ColorB(128,0,0,255));          
		//DrawImpulse(liftImp, Vec3(0,0,6), 2.f/deltaTime, ColorB(0,0,255,255));
	}
#endif
}

//------------------------------------------------------------------------
void CVehicleMovementStdBoat::UpdateRunSound(const float deltaTime)
{
	Vec3 localAccel(ZERO);
	m_measureSpeedTimer+=deltaTime;

	if (m_measureSpeedTimer > 0.25f)
	{
		const Vec3& v = m_physStatus[k_mainThread].v;
		Vec3 accel = (v - m_lastMeasuredVel) * (1.f/m_measureSpeedTimer);
		localAccel = accel * m_physStatus[k_mainThread].q;

		m_lastMeasuredVel = v;
		m_measureSpeedTimer = 0.f;
	}

	if (m_pVehicle->IsProbablyDistant())
		return;

  // rpm dropdown for waves
  if (m_rpmPitchDir != 0)    
  { 
    float speed = (m_rpmPitchDir > 0) ? 0.1f : -0.8f; // quick down, slow up
    m_waveSoundPitch += deltaTime * speed;

    if (m_waveSoundPitch < -m_waveSoundAmount) // dropdown amount
    {
      m_waveSoundPitch = -m_waveSoundAmount;
      m_rpmPitchDir = 1;
    }      
    else if (m_waveSoundPitch > 0.f)
    {
      m_waveSoundPitch = 0.f;
      m_rpmPitchDir = 0;
    }
  }

  if (m_rpmPitchSpeed>0.f)
  {    
    const float maxPedal = (!m_inWater) ? 1.f : Boosting() ? 0.8f : 0.7f;

    // pitch rpm with pedal          
    float pedal = GetEnginePedal();
    pedal = sgnnz(pedal)*max(ms_engineSoundIdleRatio, min(maxPedal, abs(pedal))); // clamp "pedal" to [0.2..0.7] range

    float delta = pedal - m_rpmScaleSgn;
    m_rpmScaleSgn = max(-1.f, min(1.f, m_rpmScaleSgn + sgn(delta)*min(abs(delta), m_rpmPitchSpeed*deltaTime)));

    // skip transition around 0 when on pedal (sounds more realistic)
    if (abs(GetEnginePedal()) > 0.001f && abs(delta) > 0.001f && sgn(m_rpmScaleSgn) != sgn(delta) && abs(m_rpmScaleSgn) <= 0.3f)
      m_rpmScaleSgn = sgn(delta)*0.3f;

    // for normal driving, rpm is clamped at max defined by sound dept
    m_rpmScale = abs(m_rpmScaleSgn);
    m_rpmScale = min(1.f, max(ms_engineSoundIdleRatio, m_rpmScale + m_waveSoundPitch));

		auto pIEntityAudioComponent = GetAudioProxy();
		if (pIEntityAudioComponent)
			pIEntityAudioComponent->SetParameter(m_audioControlIDs[eSID_VehicleRPM], m_rpmScale);
  }
}


//------------------------------------------------------------------------
void CVehicleMovementStdBoat::UpdateSurfaceEffects(const float deltaTime)
{
  CRY_PROFILE_FUNCTION( PROFILE_GAME );

  if (0 == g_pGameCVars->v_pa_surface)
  {
    ResetParticles();
    return;
  }
  
  IEntity* pEntity = m_pVehicle->GetEntity();
  const Matrix34& worldTM = pEntity->GetWorldTM();
  
  float distSq = worldTM.GetTranslation().GetSquaredDistance(GetISystem()->GetViewCamera().GetPosition());
  if (distSq > sqr(300.f) || (distSq > sqr(50.f) && !m_pVehicle->GetGameObject()->IsProbablyVisible()))
    return;

  Matrix34 worldTMInv = worldTM.GetInverted();
  const SVehicleStatus& status = m_pVehicle->GetStatus();    
  float velDot = status.vel * worldTM.GetColumn1();  
  float powerNorm = min(abs(m_movementAction.power), 1.f);

  SEnvironmentParticles* envParams = m_pPaParams->GetEnvironmentParticles();

  SEnvParticleStatus::TEnvEmitters::iterator end = m_paStats.envStats.emitters.end();
  for (SEnvParticleStatus::TEnvEmitters::iterator emitterIt = m_paStats.envStats.emitters.begin(); emitterIt!=end; ++emitterIt)  
  { 
    if (emitterIt->layer < 0)
    {
      assert(0);
      continue;
    }

    const SEnvironmentLayer& layer = envParams->GetLayer(emitterIt->layer);
    
    SEntitySlotInfo info;        
    info.pParticleEmitter = 0;
    pEntity->GetSlotInfo(emitterIt->slot, info);        

    float countScale = 1.f;
    float sizeScale = 1.f;
		float speedScale = 1.f;
    float speed = 0.f;

    // check if helper position is beneath water level      
                
    Vec3 emitterWorldPos = worldTM * emitterIt->quatT.t;
    float waterLevel = gEnv->p3DEngine->GetWaterLevel(&emitterWorldPos);
    int matId = 0;
    
    if (emitterWorldPos.z <= waterLevel+0.1f && m_physStatus[k_mainThread].submergedFraction<0.999f)
    {
      matId = gEnv->pPhysicalWorld->GetWaterMat();
      speed = status.speed;

      bool spray = !strcmp(layer.GetName(), "spray");        
      
      if (spray)
      {
        // slip based          
        speed -= abs(velDot);
      }

      GetParticleScale(layer, speed, powerNorm, countScale, sizeScale, speedScale);
    }
    else
    {
      countScale = 0.f;
    }
    
    if (matId && matId != emitterIt->matId)
    {
      // change effect       
      IParticleEffect* pEff = 0;                
      const char* effect = GetEffectByIndex( matId, layer.GetName() );

      if (effect && (pEff = gEnv->pParticleManager->FindEffect(effect)))
      {  
#if ENABLE_VEHICLE_DEBUG
        if (DebugParticles())              
					CryLog("%s changes water sfx to %s (slot %i)", pEntity->GetName(), effect, emitterIt->slot);
#endif

        if (info.pParticleEmitter)
        {
          info.pParticleEmitter->Activate(false);
          pEntity->FreeSlot(emitterIt->slot);                  
        }

        emitterIt->slot = pEntity->LoadParticleEmitter(emitterIt->slot, pEff);

        if (emitterIt->slot != -1)
          pEntity->SetSlotLocalTM(emitterIt->slot, Matrix34(emitterIt->quatT));

        info.pParticleEmitter = 0;
        pEntity->GetSlotInfo(emitterIt->slot, info);
      }
      else
        countScale = 0.f;
    }

    if (matId)
      emitterIt->matId = matId;

    if (info.pParticleEmitter)
    {
      SpawnParams sp;
      sp.fSizeScale = sizeScale;
      sp.fCountScale = countScale;    
			sp.fSpeedScale = speedScale;
      info.pParticleEmitter->SetSpawnParams(sp);

      if (layer.alignToWater && countScale > 0.f)
      {          
        Vec3 worldPos(emitterWorldPos.x, emitterWorldPos.y, waterLevel+0.05f);

        Matrix34 localTM(emitterIt->quatT);
        localTM.SetTranslation(worldTMInv * worldPos);
        pEntity->SetSlotLocalTM(emitterIt->slot, localTM);           
      }
    }

#if ENABLE_VEHICLE_DEBUG
    if (DebugParticles() && m_pVehicle->IsPlayerDriving())
    {          
      float color[] = {1,1,1,1};
      ColorB red(255,0,0,255);
      IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
      
      const char* effect = info.pParticleEmitter ? info.pParticleEmitter->GetName() : "";
      const Matrix34& slotTM = m_pEntity->GetSlotWorldTM(emitterIt->slot);
      Vec3 ppos = slotTM.GetTranslation();
      
      pAuxGeom->DrawSphere(ppos, 0.2f, red);
      pAuxGeom->DrawCone(ppos, slotTM.GetColumn1(), 0.1f, 0.5f, red);
      IRenderAuxText::Draw2dLabel(50.f, (float)(400+10*emitterIt->slot), 1.2f, color, false, "<%s> water fx: slot %i [%s], speed %.1f, sizeScale %.2f, countScale %.2f (pos %.0f,%0.f,%0.f)", pEntity->GetName(), emitterIt->slot, effect, speed, sizeScale, countScale, ppos.x, ppos.y, ppos.z);        
    }  
#endif
  }

  // generate water splashes
	Vec3 wakePos;
	if(m_pSplashPos)
	{
		wakePos = m_pSplashPos->GetWorldSpaceTranslation();
	}
	else
	{
		wakePos = worldTM.GetTranslation();
	}
  float wakeWaterLevel = gEnv->p3DEngine->GetWaterLevel(&wakePos);

  const Vec3& localW = m_localSpeed;
  if (localW.x >= 0.f)
    m_diving = false;
      
  if (!m_diving && localW.x < -0.03f && status.speed > 10.f && wakePos.z < m_lastWakePos.z && wakeWaterLevel+0.1f >= wakePos.z)
  {
    float speedRatio = min(1.f, status.speed/(m_maxSpeed*m_factorMaxSpeed)); 
    m_diving = true;              
    
    if (m_pWaveEffect)
    {
      if (IParticleEmitter* pEmitter = pEntity->GetParticleEmitter(m_wakeSlot))
      {
        pEmitter->Activate(false);
        pEntity->FreeSlot(m_wakeSlot);
        m_wakeSlot = -1;
      }

      SpawnParams spawnParams;
      spawnParams.fSizeScale = spawnParams.fCountScale = 0.5f + 0.25f*speedRatio;
      spawnParams.fSizeScale  += 0.4f*m_waveRandomMult;
      spawnParams.fCountScale += cry_random(0.0f, 0.4f);

      m_wakeSlot = pEntity->LoadParticleEmitter(m_wakeSlot, m_pWaveEffect, &spawnParams);        
    }

    // handle splash sound  
    ExecuteTrigger(eSID_Splash);
    SetSoundParam(eSID_Splash, "intensity", 0.2f*speedRatio + 0.5f*m_waveRandomMult);     

    if (m_rpmPitchDir == 0)
    {
      m_rpmPitchDir = -1;
      m_waveSoundPitch = 0.f;
      m_waveSoundAmount = 0.02f + m_waveRandomMult*0.08f;
    }      
  }  

  if (m_wakeSlot != -1)
  { 
    // update emitter local pos to short above waterlevel
    Matrix34 tm;
		if(m_pSplashPos)
			m_pSplashPos->GetVehicleTM(tm);
		else
			tm.SetIdentity();

    Vec3 pos = tm.GetTranslation();
    pos.z = worldTMInv.TransformPoint(Vec3(wakePos.x,wakePos.y,wakeWaterLevel)).z + 0.2f;
    tm.SetTranslation(pos);
    pEntity->SetSlotLocalTM(m_wakeSlot, tm);

#if ENABLE_VEHICLE_DEBUG
    if (IsProfilingMovement())
    {
      Vec3 wPos = worldTM * tm.GetTranslation();
      ColorB col(128, 128, 0, 200);
      gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(wPos, 0.4f, col);
      gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(wPos, col, wPos+Vec3(0,0,1.5f), col);
    }          
#endif
  } 

  m_lastWakePos = wakePos;
}


//------------------------------------------------------------------------
void CVehicleMovementStdBoat::Lift(bool lift)
{ 
  if (lift == m_lifted || m_velLift == 0.f)
    return;

  m_lifted = lift;
}

//------------------------------------------------------------------------
bool CVehicleMovementStdBoat::IsLifted()
{
  return m_lifted;
}

//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementStdBoat::ProcessAI(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	float dt = max( deltaTime, 0.005f);
	SVehiclePhysicsStatus* physStatus = &m_physStatus[k_physicsThread];

	// It is copyed from CVehicleMoventTank::ProcessAI
	m_movementAction.brake = false;
	m_movementAction.rotateYaw = 0.0f;
	m_movementAction.power = 0.0f;

	float inputSpeed = 0.0f;
	{
		if (m_aiRequest.HasDesiredSpeed())
			inputSpeed = m_aiRequest.GetDesiredSpeed();
		Limit(inputSpeed, -(m_maxSpeed*m_factorMaxSpeed), m_maxSpeed*m_factorMaxSpeed);
	}

	Vec3 vMove(ZERO);
	{
		if (m_aiRequest.HasMoveTarget())
			vMove = ( m_aiRequest.GetMoveTarget() - m_pEntity->GetWorldPos() ).GetNormalizedSafe();
	}

	//start calculation
	if ( fabsf( inputSpeed ) < 0.0001f )
	{
		m_movementAction.brake = true;
		if ( physStatus->v.GetLength() > 1.0f )
		{
			m_movementAction.power =-1.0f;
		}

	}
	else
	{
		Matrix33 entRotMatInvert( physStatus->q );
		entRotMatInvert.Invert();
		float currentAngleSpeed = RAD2DEG(-physStatus->w.z);

		const static float maxSteer = RAD2DEG(gf_PI/4.f); // fix maxsteer, shouldn't change  
		Vec3 vVel = physStatus->v;
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

		if ( inputSpeed < 0.0f )	// when going back
		{
			vFwd *= -1.0f;
			vMoveR *= -1.0f;
			currentAngleSpeed *=-1.0f;
		}

		float cosAngle = vFwd.Dot(vMoveR);
		float angle = RAD2DEG(acos_tpl(cosAngle));
		if ( vMoveR.Dot( Vec3( 1.0f,0.0f,0.0f ) )< 0 )
			 angle = -angle;

		//int step =0;
		m_movementAction.rotateYaw = angle * 1.75f/ maxSteer; 

		// implementation 1. if there is enough angle speed, we don't need to steer more
		if ( fabsf(currentAngleSpeed) > fabsf(angle) && angle*currentAngleSpeed > 0.0f )
		{
			m_movementAction.rotateYaw = m_movementAction.rotateYaw*0.995f; //step =1;
		}

		// implementation 2. if we can guess we reach the distination angle soon, start counter steer.
		float predictDelta = inputSpeed < 0.0f ? 0.1f : 0.1f;
		float dict = angle + predictDelta * ( angle - m_prevAngle) / dt ;
		if ( dict*currentAngleSpeed<0.0f )
		{
			if ( fabsf( angle ) < 2.0f )
			{
				m_movementAction.rotateYaw = angle* 1.75f/ maxSteer;// ; step =3;
			}
			else
			{
				m_movementAction.rotateYaw = currentAngleSpeed < 0.0f ? 1.0f : -1.0f;// step =2;
			}
		}

		if ( fabsf( angle ) > 20.0f && currentSpeed > 3.0f ) 
		{
			m_movementAction.power = 0.1f ;
			//step =4;
		}

		m_prevAngle =  angle;
		//CryLog("steering	%4.2f	%4.2f %4.2f	%4.2f	%4.2f	%4.2f	%d", deltaTime,inputSpeed - currentSpeed,angle,currentAngleSpeed, m_movementAction.rotateYaw,currentAngleSpeed-m_prevAngle,step);

	}

}

//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementStdBoat::ProcessMovement(const float deltaTime)
{  
  CRY_PROFILE_FUNCTION( PROFILE_GAME );

  static const float fWaterLevelMaxDiff = 0.15f; // max allowed height difference between propeller center and water level
  static const float fSubmergedMin = 0.01f;
  static const float fMinSpeedForTurn = 0.5f; // min speed so that turning becomes possible
  
  if (m_bNetSync)
    m_netActionSync.UpdateObject(this);

  CryAutoCriticalSection lk(m_lock);

  CVehicleMovementBase::ProcessMovement(deltaTime);

  IEntity* pEntity = m_pVehicle->GetEntity();
  IPhysicalEntity* pPhysics = pEntity->GetPhysics(); 
  SVehiclePhysicsStatus* physStatus = &m_physStatus[k_physicsThread];
  assert(pPhysics);

  float frameTime = min(deltaTime, 0.1f); 

  if (abs(m_movementAction.power) < 0.001f)
    m_movementAction.power = 0.f;
  if (abs(m_movementAction.rotateYaw) < 0.001f)
    m_movementAction.rotateYaw = 0.f;

  Matrix34 wTM( physStatus->q );
  wTM.AddTranslation( physStatus->pos );

  Matrix34 wTMInv = wTM.GetInvertedFast();
    
  Vec3 localVel = wTMInv.TransformVector( physStatus->v );
  Vec3 localW = wTMInv.TransformVector( physStatus->w );   

  const Vec3 xAxis = wTM.GetColumn0();
  const Vec3 yAxis = wTM.GetColumn1();
  const Vec3 zAxis = wTM.GetColumn2();
  
  // check if propeller is in water
  Vec3 worldPropPos = wTM * m_pushOffset;  
  float waterLevelWorld = gEnv->p3DEngine->GetWaterLevel( &worldPropPos );
  float fWaterLevelDiff = worldPropPos.z - waterLevelWorld;  
  
  bool submerged = physStatus->submergedFraction > fSubmergedMin;
  m_inWater = submerged && fWaterLevelDiff < fWaterLevelMaxDiff;
    
  float speed = physStatus->v.len2() > 0.001f ? physStatus->v.len() : 0.f;    
  float speedRatio = min(1.f, speed/(m_maxSpeed*m_factorMaxSpeed));  
  float absPedal = abs(m_movementAction.power);
  float absSteer = abs(m_movementAction.rotateYaw);
 
  // wave stuff 
  float waveFreq = 1.f;
  waveFreq += 3.f*speedRatio;

  float waveTimerPrev = m_waveTimer;
  m_waveTimer += frameTime*waveFreq;

  // new randomized amount for this oscillation
  if (m_waveTimer >= gf_PI && waveTimerPrev < gf_PI) 
    m_waveRandomMult = cry_random(0.0f, 1.0f);  
  
  if (m_waveTimer >= 2*gf_PI)  
    m_waveTimer -= 2*gf_PI;    

  float kx = m_waveIdleStrength.x*(m_waveRandomMult+0.3f) * (1.f-speedRatio + m_waveSpeedMult*speedRatio);
  float ky = m_waveIdleStrength.y * (1.f - 0.5f*absPedal - 0.5f*absSteer);

  Vec3 waveLoc = m_massOffset;
  waveLoc.y += speedRatio*min(0.f, m_pushOffset.y-m_massOffset.y);
  waveLoc = wTM * waveLoc;

  bool visible = m_pVehicle->GetGameObject()->IsProbablyVisible();
  bool doWave = visible && submerged && physStatus->submergedFraction < 0.99f;
    
  if (doWave && !m_isEnginePowered)
    m_pVehicle->NeedsUpdate(IVehicle::eVUF_AwakePhysics);
  
  if (m_isEnginePowered || (visible && !m_pVehicle->IsProbablyDistant()))
  {
    if (doWave && (m_isEnginePowered || g_pGameCVars->v_rockBoats))
    { 
      pe_action_impulse waveImp;
      waveImp.angImpulse.x = Boosting() ? 0.f : sinf(m_waveTimer) * frameTime * m_Inertia.x * kx;
      
      if (isneg(waveImp.angImpulse.x))
        waveImp.angImpulse.x *= (1.f - min(1.f, 2.f*speedRatio)); // less amplitude for negative impulse      

      waveImp.angImpulse.y = sinf(m_waveTimer-0.5f*gf_PI) * frameTime * m_Inertia.y * ky;  
      waveImp.angImpulse.z = 0.f;
      waveImp.angImpulse = wTM.TransformVector(waveImp.angImpulse);
      waveImp.point = waveLoc;
      if (!m_movementAction.isAI)
	      pPhysics->Action(&waveImp, 1);      
    }
  }
  // ~wave stuff 

	if (!m_isEnginePowered)
		return;

  pe_action_impulse linearImp, angularImp, dampImp, liftImp; 
  float turnAccel = 0, turnAccelNorm = 0;

	if (m_inWater)
	{     
    // Lateral damping
    if (m_lateralDamping>0.f)
    {
    		pe_action_impulse impulse;
    		impulse.impulse = - physStatus->mass * xAxis * (localVel.x * (frameTime * m_lateralDamping)/(1.f + frameTime*m_lateralDamping));
    		pPhysics->Action(&impulse, 1);
    }
    // optional lifting (catamarans)
    if (m_velLift > 0.f)
    {
      if (localVel.y > m_velLift && !IsLifted())
        Lift(true);
      else if (localVel.y < m_velLift && IsLifted())
        Lift(false);
    }

    if (Boosting() && IsLifted())
    {
      // additional lift force      
      liftImp.impulse = Vec3(0,0,physStatus->mass*frameTime*(localVel.y/(m_velMax*m_factorMaxSpeed))*3.f);
      liftImp.point = wTM * m_massOffset;
      pPhysics->Action(&liftImp, 1);
    }
    
    // apply driving force         
    float a = m_movementAction.power;

    if (sgn(a)*sgn(localVel.y) > 0)
    {
      // reduce acceleration with increasing speed
      float ratio = (localVel.y > 0.f) ? localVel.y/(m_velMax*m_factorMaxSpeed) : -localVel.y/(m_velMaxReverse*m_factorMaxSpeed);      
      a = (ratio>1.f) ? 0.f : sgn(a)*min(abs(a), 1.f-((1.f-m_accelVelMax)*sqr(ratio))); 
    }
    
    if (a != 0)
    {
      if (sgn(a) * sgn(localVel.y) < 0) // "braking"
        a *= m_accelCoeff;    
      else
        a = max(a, -m_pedalLimitReverse);

      Vec3 pushDir(FORWARD_DIRECTION);                
      
      // apply force downwards a bit for more realistic response  
      if (a > 0)
        pushDir = Quat_tpl<float>::CreateRotationAA( DEG2RAD(m_pushTilt), Vec3(-1,0,0) ) * pushDir;

      pushDir = wTM.TransformVector( pushDir );  
      linearImp.impulse = pushDir * physStatus->mass * a * m_accel * m_factorAccel * frameTime;

      linearImp.point = m_pushOffset;
      linearImp.point.x = m_massOffset.x;
      linearImp.point = wTM * linearImp.point;
			pPhysics->Action(&linearImp, 1);
    } 
    
    float roll = (float)__fsel(zAxis.z - 0.2f, xAxis.z / (frameTime + frameTime*frameTime), 0.f);		// Roll damping (with a exp. time constant of 1 sec)

    // apply steering           
    if (m_movementAction.rotateYaw != 0)
    { 
      if (abs(localVel.y) < fMinSpeedForTurn){ // if forward speed too small, no turning possible
        turnAccel = 0; 
      }
      else 
      {
        int iDir = m_movementAction.power != 0.f ? sgn(m_movementAction.power) : sgn(localVel.y);
        turnAccelNorm = m_movementAction.rotateYaw * iDir * max(1.f, m_turnVelocityMult * speedRatio);    

        // steering and current w in same direction?
        int sgnSteerW = sgn(m_movementAction.rotateYaw) * iDir * sgn(-localW.z);

        if (sgnSteerW < 0)
        { 
          // "braking"
          turnAccelNorm *= m_turnAccelCoeff; 
        }
        else 
        {    
          // reduce turn vel towards max
          float maxRatio = 1.f - 0.15f*min(1.f, abs(localW.z)/m_turnRateMax);
          turnAccelNorm = sgn(turnAccelNorm) * min(abs(turnAccelNorm), maxRatio);
        }

        turnAccel = turnAccelNorm * m_turnAccel;
        //roll = 0.2f * turnAccel; // slight roll        
      }        
    }   

		// Use the centripetal acceleration to determine the amount of roll
		float centripetalAccel = clamp_tpl(speed * localW.z, -10.f, +10.f);
		roll -= (1.f - 2.f*fabsf(xAxis.z)) * m_rollAccel * centripetalAccel;

		// Always damp rotation!
		turnAccel += localW.z * m_turnDamping;
    
    if (turnAccel != 0)
    {
      Vec3& angImp = angularImp.angImpulse; 
      
      angImp.x = 0.f;
      angImp.y = roll * frameTime * m_Inertia.y;
      angImp.z = -turnAccel * frameTime * m_Inertia.z;      
      
      angImp = wTM.TransformVector( angImp );
      pPhysics->Action(&angularImp, 1);
    }   
    
    if (abs(localVel.x) > 0.01f)  
    { 
      // lateral force         
      Vec3& cornerForce = dampImp.impulse;
      
      cornerForce.x = -localVel.x * m_cornerForceCoeff * physStatus->mass * frameTime;
      cornerForce.y = 0.f;
      cornerForce.z = 0.f;
      
      if (m_cornerTilt != 0)
        cornerForce = Quat_tpl<float>::CreateRotationAA( sgn(localVel.x)*DEG2RAD(m_cornerTilt), Vec3(0,1,0) ) * cornerForce;

      dampImp.impulse = wTM.TransformVector(cornerForce);

      dampImp.point = m_cornerOffset;
      dampImp.point.x = m_massOffset.x;
      dampImp.point = wTM.TransformPoint( dampImp.point );
      pPhysics->Action(&dampImp, 1);         
    }  
  }

  EjectionTest(deltaTime);
  
	if (!m_pVehicle->GetStatus().doingNetPrediction)
	{
		if (m_bNetSync && m_netActionSync.PublishActions( CNetworkMovementStdBoat(this) ))
			CHANGED_NETWORK_STATE(m_pVehicle, CNetworkMovementStdBoat::CONTROLLED_ASPECT );
	}
}

#if ENABLE_VEHICLE_DEBUG
//------------------------------------------------------------------------
void CVehicleMovementStdBoat::DrawImpulse(const pe_action_impulse& action, const Vec3& offset, float scale, const ColorB& col)
{
  if (!is_unused(action.impulse) && action.impulse.len2()>0)
  {
    IRenderAuxGeom* pGeom = gEnv->pRenderer->GetIRenderAuxGeom();
    Vec3 start = action.point + offset;
    Vec3 end = start - (action.impulse*scale/m_pVehicle->GetMass());
    Vec3 dir = (start-end).GetNormalizedSafe();
    pGeom->DrawCone(start-1.f*dir, dir, 0.5f, 1.f, col);
    pGeom->DrawLine(start, col, end, col);
    pGeom->DrawSphere(end, 0.25f, col);
  }  
}
#endif

//------------------------------------------------------------------------
bool CVehicleMovementStdBoat::RequestMovement(CMovementRequest& movementRequest)
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
void CVehicleMovementStdBoat::Serialize(TSerialize ser, EEntityAspects aspects) 
{
	CVehicleMovementBase::Serialize(ser, aspects);

  if (ser.GetSerializationTarget() == eST_Network) 
  {
    if (m_bNetSync && aspects & CNetworkMovementStdBoat::CONTROLLED_ASPECT)
      m_netActionSync.Serialize(ser, aspects);
  }
  else
  {
    ser.Value("m_prevAngle", m_prevAngle);
    ser.Value("lifted", m_lifted);
		ser.Value("factorMaxSpeed", m_factorMaxSpeed);
		ser.Value("factorAccel", m_factorAccel);
  }
};

void CVehicleMovementStdBoat::GetMemoryUsage(ICrySizer * pSizer) const
{
	pSizer->Add(*this);
	CVehicleMovementBase::GetMemoryUsageInternal(pSizer);
}


//------------------------------------------------------------------------
CNetworkMovementStdBoat::CNetworkMovementStdBoat()
: m_steer(0.0f)
, m_pedal(0.0f)
, m_boost(false)
{
}

//------------------------------------------------------------------------
CNetworkMovementStdBoat::CNetworkMovementStdBoat(CVehicleMovementStdBoat *pMovement)
{
  m_steer = pMovement->m_movementAction.rotateYaw;
  m_pedal = pMovement->m_movementAction.power;  
  m_boost = pMovement->m_boost;
}

//------------------------------------------------------------------------
void CNetworkMovementStdBoat::UpdateObject(CVehicleMovementStdBoat *pMovement)
{
  pMovement->m_movementAction.rotateYaw = m_steer;
  pMovement->m_movementAction.power = m_pedal;  
  pMovement->m_boost = m_boost;
}

//------------------------------------------------------------------------
void CNetworkMovementStdBoat::Serialize(TSerialize ser, EEntityAspects aspects)
{
  if (ser.GetSerializationTarget()==eST_Network && aspects&CONTROLLED_ASPECT)
  {
		NET_PROFILE_SCOPE("NetMovementStdBoat", ser.IsReading());

    ser.Value("pedal", m_pedal, 'vPed');
    ser.Value("steer", m_steer, 'vStr');   
    ser.Value("boost", m_boost, 'bool');
  }
}

