// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a standard wheel based vehicle movements

-------------------------------------------------------------------------
History:
- 04:04:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"

#include "VehicleMovementStdWheeled.h"

#include "IVehicleSystem.h"
#include "Network/NetActionSync.h"
#include <CryAISystem/IAgent.h>
#include <CryGame/GameUtils.h>
#include <CryGame/IGameTokens.h>
#include "Player.h"

#include "NetInputChainDebug.h"

#define THREAD_SAFE 1
#define LOAD_RAMP_TIME 0.2f
#define LOAD_RELAX_TIME 0.25f

//------------------------------------------------------------------------
CVehicleMovementStdWheeled::CVehicleMovementStdWheeled()
{
  m_passengerCount = 0;
	m_steerSpeed = 40.0f;
	m_steerMax = 20.0f;
	m_steerSpeedMin = 90.0f;
	m_steerSpeedScale = 0.8f;
	m_kvSteerMax = 10.0f;
	m_steerSpeedScaleMin = 1.0f;
	m_v0SteerMax = 30.0f;
	m_steerRelaxation = 90.0f;
  m_vMaxSteerMax = 20.f;
  m_pedalLimitMax = 0.3f;  
  m_suspDampingMin = 0.f;
  m_suspDampingMax = 0.f;  
  m_speedSuspUpdated = -1.f;
  m_suspDamping = 0.f;
  m_suspDampingMaxSpeed = 0.f;
  m_stabiMin = m_stabiMax = m_stabi = 0.f;
  m_rpmTarget = 0.f;
  m_engineMaxRPM = m_engineIdleRPM = m_engineShiftDownRPM = 0.f;
  m_rpmRelaxSpeed = m_rpmInterpSpeed = m_rpmGearShiftSpeed = 4.f;  
  m_maxBrakingFriction = 0.f;
  m_lastBump = 0.f;
  m_axleFrictionMax = m_axleFrictionMin = m_axleFriction = 0.f;
  m_load = 0.f;
  m_bumpMinSusp = m_bumpMinSpeed = 0.f;
  m_bumpIntensityMult = 1.f;
  m_rpmScalePrev = 0.f;
  m_gearSoundPending = false;
  m_latFriction = 1.f;    
  m_prevAngle = 0.0f;  
  m_lostContactTimer = 0.f;
  m_airbrakeTime = 0.f;
  m_brakeTimer = 0.f;
	m_reverseTimer = 0.0f;
  m_tireBlownTimer = 0.f;
  m_boostEndurance = 7.5f;
  m_boostRegen = m_boostEndurance;
  m_brakeImpulse = 0.f;
  m_boostStrength = 6.f;
  m_lastDebugFrame = 0;
  m_wheelContactsLeft = 0;  
  m_wheelContactsRight = 0;  
	m_netActionSync.PublishActions( CNetworkMovementStdWheeled(this) );  	  
  m_blownTires = 0;
  m_bForceSleep = false;
  m_forceSleepTimer = 0.0f;
	m_submergedRatioMax = 0.0f;
	m_initialHandbreak = true;
	m_maxSoundSlipSpeed = 0.0f;
	m_massCW = 0;
	m_partidCW = 799;
	m_pos0CW.Set(0,0,1); 
	m_velxCW=m_posxCW = 0;
	m_poszCW = m_pos0CW.z;
	m_kzCW = 0;
	m_ktiltCW[0] = m_ktiltCW[1] = m_ktiltCW[2] = 1; 
	m_stiltCW = 40;
	m_maxxCW = 100;
	m_maxxCWcam = 0.5f;
	m_offsCWcam.zero();
	memset(m_posxCWhist, 0, sizeof(m_posxCWhist));
	m_minVelSteerLean = 0;
	m_dampingy = 0;
	m_velWheelDisable = 0;
	m_maskWheelDisable = 0;
	m_uprightOnMount = 0;
	m_qPrev.SetIdentity();
}

//------------------------------------------------------------------------
CVehicleMovementStdWheeled::~CVehicleMovementStdWheeled()
{ 
}

//------------------------------------------------------------------------
bool CVehicleMovementStdWheeled::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	if (!CVehicleMovementBase::Init(pVehicle, table))
	{
		assert(0);
    return false;
	}

  m_carParams.enginePower = 0.f;
  m_carParams.kStabilizer = 0.f;  
  m_carParams.engineIdleRPM = 0.f;
  m_carParams.engineMinRPM = m_carParams.engineMaxRPM = 0.f;     
  m_carParams.engineShiftDownRPM = m_carParams.engineShiftUpRPM = 0.f;
  m_carParams.steerTrackNeutralTurn = 0.f;  

  MOVEMENT_VALUE_OPT("steerSpeed", m_steerSpeed, table);
  MOVEMENT_VALUE_OPT("steerSpeedMin", m_steerSpeedMin, table);
  MOVEMENT_VALUE_OPT("steerSpeedScale", m_steerSpeedScale, table);
  MOVEMENT_VALUE_OPT("steerSpeedScaleMin", m_steerSpeedScaleMin, table);
  MOVEMENT_VALUE_OPT("kvSteerMax", m_kvSteerMax, table);
  MOVEMENT_VALUE_OPT("v0SteerMax", m_v0SteerMax, table);
  MOVEMENT_VALUE_OPT("steerRelaxation", m_steerRelaxation, table);
  MOVEMENT_VALUE_OPT("vMaxSteerMax", m_vMaxSteerMax, table);
  MOVEMENT_VALUE_OPT("pedalLimitMax", m_pedalLimitMax, table);

	MOVEMENT_VALUE_OPT("driverMass", m_massCW, table);
	MOVEMENT_VALUE_OPT("driverPosY", m_pos0CW.y, table);
	MOVEMENT_VALUE_OPT("driverPosZ", m_pos0CW.z, table);
	MOVEMENT_VALUE_OPT("driverMoveZ", m_kzCW, table);
	MOVEMENT_VALUE_OPT("driverMoveX", m_ktiltCW[0], table);
	MOVEMENT_VALUE_OPT("driverMoveXSteer", m_ktiltCW[1], table);
	MOVEMENT_VALUE_OPT("driverMoveXSteerHarder", m_ktiltCW[2], table);
	MOVEMENT_VALUE_OPT("driverMaxMoveX", m_maxxCW, table);
	MOVEMENT_VALUE_OPT("driverCamMaxMoveX", m_maxxCWcam, table);
	MOVEMENT_VALUE_OPT("driverCamOffs", m_offsCWcam, table);
	MOVEMENT_VALUE_OPT("driverSpeed", m_stiltCW, table);
	MOVEMENT_VALUE_OPT("minVelSteerLean", m_minVelSteerLean, table);
	MOVEMENT_VALUE_OPT("rollDamping", m_dampingy, table);
	MOVEMENT_VALUE_OPT("wheelDisableSpeed", m_velWheelDisable, table);
	MOVEMENT_VALUE_OPT("wheelDisableMask", m_maskWheelDisable, table);
	MOVEMENT_VALUE_OPT("uprightOnMount", m_uprightOnMount, table);
  
  table.getAttr("rpmRelaxSpeed", m_rpmRelaxSpeed);
  table.getAttr("rpmInterpSpeed", m_rpmInterpSpeed);
  table.getAttr("rpmGearShiftSpeed", m_rpmGearShiftSpeed);

  if (CVehicleParams soundParams = table.findChild("SoundParams"))
  {
    soundParams.getAttr("roadBumpMinSusp", m_bumpMinSusp);
    soundParams.getAttr("roadBumpMinSpeed", m_bumpMinSpeed);
    soundParams.getAttr("roadBumpIntensity", m_bumpIntensityMult);
    soundParams.getAttr("airbrake", m_airbrakeTime);
  }
  
	m_action.steer = 0.0f;
	m_action.pedal = 0.0f;
	m_action.dsteer = 0.0f;

	// Initialise the steering history.
	m_prevAngle = 0.0f;

	m_rpmScale = 0.0f;
	m_currentGear = 0;
  m_avgLateralSlip = 0.f;
  m_compressionMax = 0.f;
  m_avgWheelRot = 0.f;
  m_wheelContacts = 0;
	  
	if (!InitPhysics(table))
		return false;

	return true;
}

//------------------------------------------------------------------------
bool CVehicleMovementStdWheeled::InitPhysics(const CVehicleParams& table)
{
	CVehicleParams wheeledTable = table.findChild("Wheeled");
	if (!wheeledTable)
    return false;

  m_carParams.maxTimeStep = 0.02f;
    
	MOVEMENT_VALUE_REQ("axleFriction", m_axleFrictionMin, wheeledTable);
	MOVEMENT_VALUE_OPT("axleFrictionMax", m_axleFrictionMax, wheeledTable);
	MOVEMENT_VALUE_REQ("brakeTorque", m_carParams.brakeTorque, wheeledTable);
	MOVEMENT_VALUE_REQ("clutchSpeed", m_carParams.clutchSpeed, wheeledTable);
	MOVEMENT_VALUE_REQ("damping", m_carParams.damping, wheeledTable);
	MOVEMENT_VALUE_REQ("engineIdleRPM", m_carParams.engineIdleRPM, wheeledTable);
	MOVEMENT_VALUE_REQ("engineMaxRPM", m_carParams.engineMaxRPM, wheeledTable);
	MOVEMENT_VALUE_REQ("engineMinRPM", m_carParams.engineMinRPM, wheeledTable);
	MOVEMENT_VALUE_REQ("engineShiftDownRPM", m_carParams.engineShiftDownRPM, wheeledTable);
	MOVEMENT_VALUE_REQ("engineShiftUpRPM", m_carParams.engineShiftUpRPM, wheeledTable);
	MOVEMENT_VALUE_REQ("engineStartRPM", m_carParams.engineStartRPM, wheeledTable);
	wheeledTable.getAttr("integrationType", m_carParams.iIntegrationType);
	MOVEMENT_VALUE_REQ("stabilizer", m_carParams.kStabilizer, wheeledTable);
	MOVEMENT_VALUE_OPT("minBrakingFriction", m_carParams.minBrakingFriction, wheeledTable);		
	MOVEMENT_VALUE_REQ("maxSteer", m_carParams.maxSteer, wheeledTable);    
	wheeledTable.getAttr("maxTimeStep", m_carParams.maxTimeStep);
	wheeledTable.getAttr("minEnergy", m_carParams.minEnergy);
	MOVEMENT_VALUE_REQ("slipThreshold", m_carParams.slipThreshold, wheeledTable);
	MOVEMENT_VALUE_REQ("gearDirSwitchRPM", m_carParams.gearDirSwitchRPM, wheeledTable);
	MOVEMENT_VALUE_REQ("dynFriction", m_carParams.kDynFriction, wheeledTable);
	MOVEMENT_VALUE_OPT("latFriction", m_latFriction, wheeledTable);
	MOVEMENT_VALUE_OPT("steerTrackNeutralTurn", m_carParams.steerTrackNeutralTurn, wheeledTable);  
	MOVEMENT_VALUE_OPT("suspDampingMin", m_suspDampingMin, wheeledTable);
	MOVEMENT_VALUE_OPT("suspDampingMax", m_suspDampingMax, wheeledTable);
  MOVEMENT_VALUE_OPT("suspDampingMaxSpeed", m_suspDampingMaxSpeed, wheeledTable);
  MOVEMENT_VALUE_OPT("stabiMin", m_stabiMin, wheeledTable);
  MOVEMENT_VALUE_OPT("stabiMax", m_stabiMax, wheeledTable);
	MOVEMENT_VALUE_OPT("maxSpeed", m_maxSpeed, wheeledTable);
  MOVEMENT_VALUE_OPT("brakeImpulse", m_brakeImpulse, wheeledTable);
	MOVEMENT_VALUE_OPT("wheelMassScale", m_carParams.wheelMassScale, wheeledTable);
  
  int maxGear = 0;
  if (wheeledTable.getAttr("maxGear", maxGear) && maxGear>0)
    m_carParams.maxGear = maxGear+1; // to zero-based indexing
    
  m_carParams.axleFriction = m_axleFrictionMin;
  m_axleFriction = m_axleFrictionMin;
  m_stabi = m_carParams.kStabilizer;
  	
	if (wheeledTable.getAttr("enginePower", m_carParams.enginePower))
	{
		m_carParams.enginePower *= 1000.0f;
	}
	else
	{
		CryLog("Movement Init (%s) - failed to init due to missing <%s> parameter", m_pVehicle->GetEntity()->GetClass()->GetName(), "enginePower");
		return false;
	}

	if (g_pGameCVars->pVehicleQuality->GetIVal()==1)
    m_carParams.maxTimeStep = max(m_carParams.maxTimeStep, 0.04f);

	// don't submit maxBrakingFriction to physics, it's controlled by gamecode
	MOVEMENT_VALUE_OPT("maxBrakingFriction", m_maxBrakingFriction, wheeledTable);
	  
  m_carParams.pullTilt = 0.f;
  wheeledTable.getAttr("pullTilt", m_carParams.pullTilt);
  m_carParams.pullTilt = DEG2RAD(m_carParams.pullTilt);

	if (CVehicleParams gearRatiosTable = wheeledTable.findChild("gearRatios"))
	{
		int count = min(gearRatiosTable.getChildCount(), 16);

		for (int i=0; i<count; ++i)
		{	
			m_gearRatios[i] = 0.f;
			if (CVehicleParams gearRef = gearRatiosTable.getChild(i))
				gearRef.getAttr("value", m_gearRatios[i]);
		}

		m_carParams.nGears = count;
		m_carParams.gearRatios = m_gearRatios;
	}

	if (!table.getAttr("isBreakingOnIdle", m_isBreakingOnIdle))
		m_isBreakingOnIdle = false;

	{
		// Better Collisions!
		m_carParams.maxTilt = DEG2RAD(85.f);
		m_carParams.bKeepTractionWhenTilted = 1;
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::PostInit()
{
  CVehicleMovementBase::PostInit();

  m_wheelStats.clear();
  m_wheelParts.clear();
  
  int numWheels = m_pVehicle->GetWheelCount();    
  m_wheelStats.resize(numWheels);

  int nParts = m_pVehicle->GetPartCount();

  for (int i=0; i<nParts; ++i)
  {      
    IVehiclePart* pPart = m_pVehicle->GetPart(i);
    if (pPart->GetIWheel())
    { 
      m_wheelParts.push_back(pPart);
      m_wheelStats[m_wheelParts.size()-1].friction = pPart->GetIWheel()->GetCarGeomParams()->kLatFriction;
    }
  }
  assert(m_wheelParts.size() == numWheels);
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::Physicalize()
{
	CVehicleMovementBase::Physicalize();

	SEntityPhysicalizeParams physicsParams(m_pVehicle->GetPhysicsParams());	
  
  physicsParams.type = PE_WHEELEDVEHICLE;	  
  m_carParams.nWheels = m_pVehicle->GetWheelCount();  
	
  pe_params_car carParams(m_carParams);  
  physicsParams.pCar = &carParams;

  m_pVehicle->GetEntity()->Physicalize(physicsParams);
    
  m_engineMaxRPM = m_carParams.engineMaxRPM;
  m_engineIdleRPM = m_carParams.engineIdleRPM;
  m_engineShiftDownRPM = m_carParams.engineShiftDownRPM;

	IPhysicalEntity *pPhysEnt = GetPhysics();
	if (pPhysEnt && g_pGameCVars->pVehicleQuality->GetIVal()==1 && m_carParams.steerTrackNeutralTurn)
	{
		pe_params_flags pf; pf.flagsOR = wwef_fake_inner_wheels;
		pe_params_foreign_data pfd; pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
		pPhysEnt->SetParams(&pf);
		pPhysEnt->SetParams(&pfd);
	}

	if (m_massCW>0)
	{
		IGeomManager *pGeoman = gEnv->pPhysicalWorld->GetGeomManager();
		primitives::sphere sph; 
		sph.center.zero(); sph.r=0.1f;
		phys_geometry *pGeom = pGeoman->RegisterGeometry(pGeoman->CreatePrimitive(primitives::sphere::type, &sph));
		pGeom->pGeom->Release(); pGeom->nRefCount--;
		pe_geomparams gp;
		gp.pos = m_pos0CW;
		gp.mass = m_massCW;
		gp.flags = gp.flagsCollider = 0;
		pPhysEnt->AddGeometry(pGeom, &gp, m_partidCW);
	}
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::PostPhysicalize()
{
  CVehicleMovementBase::PostPhysicalize();
  
  if (m_maxSpeed == 0.f)
  {
    pe_status_vehicle_abilities ab;    
    if (GetPhysics()->GetStatus(&ab))
      m_maxSpeed = ab.maxVelocity * 0.5f; // fixme! maxVelocity too high
    
#if ENABLE_VEHICLE_DEBUG
    if (g_pGameCVars->v_profileMovement)
      CryLog("%s maxSpeed: %f", m_pVehicle->GetEntity()->GetClass()->GetName(), m_maxSpeed);
#endif
  }
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::InitSurfaceEffects()
{ 
  IPhysicalEntity* pPhysics = GetPhysics();
  pe_status_nparts tmpStatus;
  int numParts = pPhysics->GetStatus(&tmpStatus);
  int numWheels = m_pVehicle->GetWheelCount();

  m_paStats.envStats.emitters.clear();

  // for each wheelgroup, add 1 particleemitter. the position is the wheels' 
  // center in xy-plane and minimum on z-axis
  // direction is upward
  SEnvironmentParticles* envParams = m_pPaParams->GetEnvironmentParticles();
  
  for (int iLayer=0; iLayer < (int)envParams->GetLayerCount(); ++iLayer)
  {
    const SEnvironmentLayer& layer = envParams->GetLayer(iLayer);
    
    m_paStats.envStats.emitters.reserve(m_paStats.envStats.emitters.size() + layer.GetGroupCount());

    for (int i=0; i < (int)layer.GetGroupCount(); ++i)
    { 
      Matrix34 tm;
      tm.SetIdentity();
      
      if (layer.GetHelperCount() == layer.GetGroupCount() && layer.GetHelper(i))
      {
        // use helper pos if specified
				if (IVehicleHelper* pHelper = layer.GetHelper(i))
					pHelper->GetVehicleTM(tm);
				else
					tm.SetIdentity();
      }
      else
      {
        // else use wheels' center
        Vec3 pos(ZERO);
        
        for (int w=0; w < (int)layer.GetWheelCount(i); ++w)
        {       
          int ipart = numParts - numWheels + layer.GetWheelAt(i,w)-1; // wheels are last

          if (ipart < 0 || ipart >= numParts)
          {
            CryLog("%s invalid wheel index: %i, maybe asset/setup mismatch", m_pEntity->GetName(), ipart);
            continue;
          }

          pe_status_pos spos;
          spos.ipart = ipart;
          if (pPhysics->GetStatus(&spos))
          {
            spos.pos.z += spos.BBox[0].z;
            pos = (pos.IsZero()) ? spos.pos : 0.5f*(pos + spos.pos);        
          }
        }
        tm = Matrix34::CreateRotationX(DEG2RAD(90.f));      
        tm.SetTranslation( m_pEntity->GetWorldTM().GetInverted().TransformPoint(pos) );
      }     

      TEnvEmitter emitter;
      emitter.layer = iLayer;        
      emitter.slot = -1;
      emitter.group = i;
      emitter.active = layer.IsGroupActive(i);
      emitter.quatT = QuatT(tm);
      m_paStats.envStats.emitters.push_back(emitter);
      
#if ENABLE_VEHICLE_DEBUG
      if (DebugParticles())
      {
        const Vec3 loc = tm.GetTranslation();
        CryLog("WheelGroup %i local pos: %.1f %.1f %.1f", i, loc.x, loc.y, loc.z);        
      }      
#endif
    }
  }
  
  m_paStats.envStats.initalized = true;  
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::Reset()
{
	CVehicleMovementBase::Reset();

  m_prevAngle = 0.0f;
	m_action.pedal = 0.f;
	m_action.steer = 0.f;
	m_rpmScale = 0.0f;
  m_rpmScalePrev = 0.f;
	m_currentGear = 0;
  m_gearSoundPending = false;
  m_avgLateralSlip = 0.f;  
  m_tireBlownTimer = 0.f; 
  m_wheelContacts = 0;
  
  if (m_blownTires)
    SetEngineRPMMult(1.0f);
  m_blownTires = 0;

  if (m_bForceSleep)
  {
    IPhysicalEntity* pPhysics = GetPhysics();
	if (pPhysics)
	{
		pe_params_car params;
		params.minEnergy = m_carParams.minEnergy;
		pPhysics->SetParams(&params, 1);
	}
  }
  m_bForceSleep = false;
  m_forceSleepTimer = 0.0f;
  m_passengerCount = 0;
  m_initialHandbreak = true;

}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::Release()
{
	CVehicleMovementBase::Release();

	delete this;
}

//------------------------------------------------------------------------
bool CVehicleMovementStdWheeled::StartEngine()
{
  if (!CVehicleMovementBase::StartEngine())
    return false;

  m_brakeTimer = 0.f;
	m_reverseTimer = 0.f;
  m_action.pedal = 0.f;
  m_action.steer = 0.f;
  m_initialHandbreak =false;

  return true;
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::StopEngine()
{
	CVehicleMovementBase::StopEngine();
	m_movementAction.Clear(true);

  UpdateBrakes(0.f);
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params)
{
	CVehicleMovementBase::OnEvent(event, params);

  if (event == eVME_TireBlown || event == eVME_TireRestored)
  {		
    int wheelIndex = params.iValue;
    
    if (m_carParams.steerTrackNeutralTurn == 0.f)
    {
      int blownTiresPrev = m_blownTires;    
      m_blownTires = max(0, min(m_pVehicle->GetWheelCount(), event==eVME_TireBlown ? m_blownTires+1 : m_blownTires-1));
      
      // reduce speed based on number of tyres blown out        
      if (m_blownTires != blownTiresPrev)
      {	
        SetEngineRPMMult(GetWheelCondition());
      }
    }

    // handle particles (sparks etc.)
    SEnvironmentParticles* envParams = m_pPaParams->GetEnvironmentParticles();    
    
    SEnvParticleStatus::TEnvEmitters::iterator emitterIt = m_paStats.envStats.emitters.begin();
    SEnvParticleStatus::TEnvEmitters::iterator emitterItEnd = m_paStats.envStats.emitters.end();
    for (; emitterIt != emitterItEnd; ++emitterIt)
    { 
      // disable this wheel in layer 0, enable in layer 1            
      if (emitterIt->group >= 0)
      {
        const SEnvironmentLayer& layer = envParams->GetLayer(emitterIt->layer);

        for (int i=0; i < (int)layer.GetWheelCount(emitterIt->group); ++i)
        {
          if (layer.GetWheelAt(emitterIt->group, i)-1 == wheelIndex)
          {
            bool bEnable = !strcmp(layer.GetName(), "rims");
						if (event == eVME_TireRestored)
							bEnable=!bEnable;
            EnableEnvEmitter(*emitterIt, bEnable);
            emitterIt->active = bEnable;
          }
        } 
      }
    }     
  }
	else if (m_uprightOnMount && event==eVME_PlayerEnterLeaveVehicle && params.bValue)
	{
		Quat q = m_pVehicle->GetEntity()->GetRotation();
		Vec3 xaxis = q*Vec3(1,0,0), xaxisNew = xaxis;
		xaxisNew.z = 0; xaxisNew.normalize();
		m_pVehicle->GetEntity()->SetRotation(Quat::CreateRotationV0V1(xaxis,xaxisNew)*q);
	}
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{

	CryAutoCriticalSection lk(m_lock);

	CVehicleMovementBase::OnAction(actionId, activationMode, value);

}

//------------------------------------------------------------------------
float CVehicleMovementStdWheeled::GetWheelCondition() const
{
  if (0 == m_blownTires)
    return 1.0f;

  // for a 4-wheel vehicle, want to reduce speed by 20% for each wheel shot out. So I'm assuming that for an 8-wheel
  //	vehicle we'd want to reduce by 10% per wheel.
  float wheelCondition = 1.0f - ((float)m_blownTires / m_pVehicle->GetWheelCount());  
  
  return  1.0f - (0.8f * (1.0f - wheelCondition));
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::SetEngineRPMMult(float mult, int threadSafe)
{
  if (IPhysicalEntity* pPhysics = GetPhysics())
  {
    pe_params_car params;
    params.engineMaxRPM = m_engineMaxRPM * mult;
    params.engineMinRPM = m_carParams.engineMinRPM * mult;
    params.engineShiftDownRPM = m_engineShiftDownRPM * mult;
    params.engineShiftUpRPM = m_carParams.engineShiftUpRPM * mult;
    params.engineIdleRPM = m_engineIdleRPM * mult;
    params.engineStartRPM = m_carParams.engineStartRPM * mult;    
    pPhysics->SetParams(&params, threadSafe);
  }
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	CVehicleMovementBase::OnVehicleEvent(event,params);
}

//------------------------------------------------------------------------
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementStdWheeled::UpdateAxleFriction(float pedal, bool backward, const float deltaTime)
{

  // apply high axleFriction for idle and footbraking, low for normal driving    
  int pedalGearDir = sgn(m_vehicleStatus.iCurGear-1) * sgn(pedal);

  if (m_axleFrictionMax > m_axleFrictionMin)
  {
    float fric;

    if ((!backward || m_vehicleStatus.iCurGear != 0) && pedalGearDir > 0)      
      fric = m_axleFrictionMin; 
    else
      fric = m_axleFrictionMax;

    if (m_axleFriction != fric)
    {
      m_axleFriction = fric;

      pe_params_car carparams;
      carparams.axleFriction = m_axleFriction;
      GetPhysics()->SetParams(&carparams,THREAD_SAFE);
    }
  }

  if (m_brakeImpulse != 0.f && m_wheelContacts && pedal != 0.f && m_PhysDyn.v.len2()>1.f)
  {
	Matrix33 worldTM( m_PhysPos.q );
    Vec3 vel = worldTM.GetInverted()* m_PhysDyn.v;

    if (sgn(vel.y)*sgn(pedal) < 0)
    {
      // add helper impulse for braking
      pe_action_impulse imp;
      imp.impulse = -m_PhysDyn.v.GetNormalized();
      imp.impulse *= m_brakeImpulse * abs(vel.y) * ((float)m_wheelContacts/m_pVehicle->GetWheelCount()) * deltaTime * m_PhysDyn.mass;      
      imp.point = m_PhysDyn.centerOfMass;
      imp.iApplyTime = 0;
      GetPhysics()->Action(&imp, THREAD_SAFE);
/*
      if (IsProfilingMovement())
      {
        IRenderAuxGeom* pGeom = gEnv->pRenderer->GetIRenderAuxGeom();
        float len = 5.f * imp.impulse.len() / deltaTime / m_statusDyn.mass;
        Vec3 dir = imp.impulse.GetNormalized();
        pGeom->DrawCone(imp.point-(dir*len), dir, 1.f, len, ColorB(128,0,0,255));

        pGeom->DrawLine(imp.point+worldTM.TransformVector(Vec3(0,0,1)), ColorB(0,0,255,255), imp.point+worldTM.TransformVector(Vec3(0,0,1))+m_statusDyn.v, ColorB(0,0,255,255));
      }    
*/
    }
  }
}

//------------------------------------------------------------------------
float CVehicleMovementStdWheeled::GetMaxSteer(float speedRel)
{
  // reduce max steering angle with increasing speed
  m_steerMax = m_v0SteerMax - (m_kvSteerMax * speedRel);
//  m_steerMax = 45.0f;
  return DEG2RAD(m_steerMax);
}


//------------------------------------------------------------------------
float CVehicleMovementStdWheeled::GetSteerSpeed(float speedRel)
{
  // reduce steer speed with increasing speed
  float steerDelta = m_steerSpeed - m_steerSpeedMin;
  float steerSpeed = m_steerSpeedMin + steerDelta * speedRel;

  // additionally adjust sensitivity based on speed
  float steerScaleDelta = m_steerSpeedScale - m_steerSpeedScaleMin;
  float sensivity = m_steerSpeedScaleMin + steerScaleDelta * speedRel;
  
  return steerSpeed * sensivity;
}

//----------------------------------------------------------------------------------
void CVehicleMovementStdWheeled::ApplyBoost(float speed, float maxSpeed, float strength, float deltaTime)
{

  // This function need to be MT safe, called StdWeeled and Tank

  const float fullBoostMaxSpeed = 0.75f*m_maxSpeed;


  if (m_action.pedal > 0.01f && m_wheelContacts >= 0.5f*m_pVehicle->GetWheelCount() && speed < maxSpeed)
  {     

    float fraction = 0.0f;
	if ( fabsf( maxSpeed-fullBoostMaxSpeed ) > 0.001f )
		fraction = max(0.f, 1.f - max(0.f, speed-fullBoostMaxSpeed)/(maxSpeed-fullBoostMaxSpeed));
    float amount = fraction * strength * m_action.pedal * m_PhysDyn.mass * deltaTime;
  
    float angle = DEG2RAD(m_carParams.steerTrackNeutralTurn == 0.f ? 30.f : 0.f);
    Vec3 dir(0, cosf(angle), -sinf(angle));

    AABB bounds;
    m_pVehicle->GetEntity()->GetLocalBounds(bounds);

	const Vec3 worldPos =  m_PhysPos.pos;
	const Matrix33 worldMat( m_PhysPos.q);

    pe_action_impulse imp;            
	imp.impulse = worldMat * dir * amount;
    imp.point = worldMat * Vec3(0, bounds.min.y, 0); // approx. at ground
	imp.point +=worldPos;
    imp.iApplyTime = 0;

	GetPhysics()->Action(&imp, THREAD_SAFE);

    //const static float color[] = {1,1,1,1};
    //IRenderAuxText::Draw2dLabel(400, 400, 1.4f, color, false, "fBoost: %.2f", fraction);
  }
}

#if ENABLE_VEHICLE_DEBUG
//----------------------------------------------------------------------------------
void CVehicleMovementStdWheeled::DebugDrawMovement(const float deltaTime)
{
  if (!IsProfilingMovement())
    return;

  if (g_pGameCVars->v_profileMovement==3 || g_pGameCVars->v_profileMovement==1 && m_lastDebugFrame == gEnv->pRenderer->GetFrameID())
    return;
  
  m_lastDebugFrame = gEnv->pRenderer->GetFrameID();
  
  IPhysicalEntity* pPhysics = GetPhysics();

  float color[4] = {1,1,1,1};
  float green[4] = {0,1,0,1};
//  float red[4] = {1,0,0,1};
  ColorB colRed(255,0,0,255);
  float y = 50.f, step1 = 15.f, step2 = 20.f, size=1.3f, sizeL=1.5f;

  float speed = m_vehicleStatus.vel.len();
  float speedRel = min(speed, m_vMaxSteerMax) / m_vMaxSteerMax;
  float steerMax = GetMaxSteer(speedRel);
  float steerSpeed = GetSteerSpeed(speedRel);  
  int percent = (int)(speed / m_maxSpeed * 100.f);
  Vec3 localVel = m_pVehicle->GetEntity()->GetWorldRotation().GetInverted() * m_statusDyn.v;

  pe_params_car carparams;
  pPhysics->GetParams(&carparams);

	IRenderAuxText::Draw2dLabel(5.0f,   y, sizeL, color, false, "Car movement");
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size, color, false, "Speed: %.1f (%.1f km/h) (%i)", speed, speed*3.6f, percent);
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "localVel: %.1f %.1f %.1f", localVel.x, localVel.y, localVel.z);
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "rpm_scale:   %.2f (max rpm: %.0f)", m_rpmScale, carparams.engineMaxRPM); 
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "Gear:  %i", m_vehicleStatus.iCurGear-1);
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "Clutch:  %.2f", m_vehicleStatus.clutch);
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "Torque:  %.1f", m_vehicleStatus.drivingTorque);
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "AxleFric:  %.0f", m_axleFriction);
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "Dampers:  %.2f", m_suspDamping);
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "Stabi:  %.2f", m_stabi);
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "LatSlip:  %.2f", m_avgLateralSlip);  
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "AvgWheelSpeed:  %.2f", m_avgWheelRot);
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "BrakeTime:  %.2f", m_brakeTimer);
	IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "ReverseTime:  %.2f", m_reverseTimer);
    
  if (m_statusDyn.submergedFraction > 0.f)
	  IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "Submerged:  %.2f", m_statusDyn.submergedFraction);

  if (m_damage > 0.f)
	  IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size, color, false, "Damage:  %.2f", m_damage);
    
  if (Boosting())
	  IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, green, false, "Boost:  %.2f", m_boostCounter);

  IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, sizeL, color, false, "Driver input");
  IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size, color, false, "power: %.2f", m_movementAction.power);
  IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "steer: %.2f", m_movementAction.rotateYaw); 
  IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "brake: %i", m_movementAction.brake);

  IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, sizeL, color, false, "Car action");
  IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size, color, false, "pedal: %.2f", m_action.pedal);
  IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "steer: %.2f (max %.2f)", RAD2DEG(m_action.steer), RAD2DEG(steerMax)); 
  IRenderAuxText::Draw2dLabel(5.0f,  y+=step1, size, color, false, "brake: %i", m_action.bHandBrake);

  IRenderAuxText::Draw2dLabel(5.0f,  y+=step2, size, color, false, "steerSpeed: %.2f", steerSpeed);

  const Matrix34& worldTM = m_pVehicle->GetEntity()->GetWorldTM();
  
  IRenderAuxGeom* pGeom = gEnv->pRenderer->GetIRenderAuxGeom();
  ColorB colGreen(0,255,0,255);
  pGeom->DrawSphere(m_statusDyn.centerOfMass, 0.1f, colGreen);
  
  pe_status_wheel ws;
  pe_status_pos wp;
  pe_params_wheel wparams;

  pe_status_nparts tmpStatus;
  int numParts = pPhysics->GetStatus(&tmpStatus);
  
  int count = m_pVehicle->GetWheelCount();
  float tScaleTotal = 0.f;
  
  // wheel-specific
  for (int i=0; i<count; ++i)
  {
    ws.iWheel = i;
    wp.ipart = numParts - count + i;
    wparams.iWheel = i;

    int ok = pPhysics->GetStatus(&ws);
    ok &= pPhysics->GetStatus(&wp);
    ok &= pPhysics->GetParams(&wparams);

    if (!ok)
      continue;

    // slip
    if (g_pGameCVars->v_draw_slip)
    {
      if (ws.bContact)
      { 
        pGeom->DrawSphere(ws.ptContact, 0.05f, colRed);

        float slip = ws.velSlip.len();        
        if (ws.bSlip>0)
        { 
          pGeom->DrawLine(wp.pos, colRed, wp.pos+ws.velSlip, colRed);
        }        
      }
            
      //IRenderAuxText::DrawLabelF(wp.pos, 1.2f, "%.2f", m_wheelStats[i].friction);      

      if (wparams.bDriving || g_pGameCVars->v_draw_slip>1)
      {
        IRenderAuxText::DrawLabelF(wp.pos, 1.2f, "T: %.2f", m_wheelStats[i].torqueScale);
        tScaleTotal += m_wheelStats[i].torqueScale;
      }      
    }    

    // suspension    
    if (g_pGameCVars->v_draw_suspension)
    {
      IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
      ColorB col(255,0,0,255);

      Vec3 lower = m_wheelParts[i]->GetLocalTM(false).GetTranslation();
      lower.x += sgn(lower.x) * 0.5f;
      
      Vec3 upper(lower);
      upper.z += ws.suspLen;
      
      lower = worldTM.TransformPoint(lower);
      pAuxGeom->DrawSphere(lower, 0.1f, col);              
      
      upper = worldTM.TransformPoint(upper);
      pAuxGeom->DrawSphere(upper, 0.1f, col);

      //pAuxGeom->DrawLine(lower, col, upper, col);
    }    
  }

  if (tScaleTotal != 0.f)
  {
    IRenderAuxText::DrawLabelF(worldTM.GetTranslation(),1.3f,"tscale: %.2f",tScaleTotal);
  }

  if (m_pWind[0])
  {
    pe_params_buoyancy buoy;
    pe_status_pos pos;
    if (m_pWind[0]->GetParams(&buoy) && m_pWind[0]->GetStatus(&pos))
    {
      IRenderAuxText::DrawLabelF(pos.pos, 1.3f, "rad: %.1f", buoy.waterFlow.len());
    }
    if (m_pWind[1]->GetParams(&buoy) && m_pWind[1]->GetStatus(&pos))
    {
      IRenderAuxText::DrawLabelF(pos.pos, 1.3f, "lin: %.1f", buoy.waterFlow.len());
    }
  }
}
#endif


//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::Update(const float deltaTime)
{
  CRY_PROFILE_FUNCTION( PROFILE_GAME );

  IPhysicalEntity* pPhysics = GetPhysics();
	if(!pPhysics)
	{
    assert(0 && "[CVehicleMovementStdWheeled::Update]: PhysicalEntity NULL!");
		return;
	}

	pPhysics->GetStatus(&m_statusDyn);
	pPhysics->GetStatus(&m_statusPos);

  const SVehicleStatus& status = m_pVehicle->GetStatus();
  m_passengerCount = status.passengerCount;

  if (!pPhysics->GetStatus(&m_vehicleStatus))
    return;


	CVehicleMovementBase::Update(deltaTime);

	UpdateSuspensionSound(deltaTime);
	UpdateBrakes(deltaTime);

  bool distant = m_pVehicle->IsProbablyDistant();
     
  if (gEnv->IsClient() && !distant)
    UpdateSounds(deltaTime);    

  if (!distant && m_blownTires && m_carParams.steerTrackNeutralTurn == 0.f)
    m_tireBlownTimer += deltaTime;       

#if ENABLE_VEHICLE_DEBUG
	DebugDrawMovement(deltaTime);
#endif

	// update reversing
	if(!distant && IsPowered() && m_actorId)
	{
		Vec3 yAxis = m_statusPos.q.GetColumn1();
		float vel = yAxis.dot(m_statusDyn.v);
		
		if(vel < -0.1f && m_action.pedal < -0.1f)
		{
			if (m_reverseTimer == 0.f)
			{
				SVehicleEventParams params;
				params.bParam = true;
				m_pVehicle->BroadcastVehicleEvent(eVE_Reversing, params);
			}

			m_reverseTimer += deltaTime;
		}
		else
		{
			if(m_reverseTimer > 0.0f)
			{
				SVehicleEventParams params;
				params.bParam = false;
				m_pVehicle->BroadcastVehicleEvent(eVE_Reversing, params);
			}

			m_reverseTimer = 0.f;
		}
	}

	if (m_massCW>0)
	{
		if (IVehicleHelper *pCam = m_pVehicle->GetHelper("driver_ghostview_pos"))
		{
			Matrix34 &mtx = const_cast<Matrix34&>(pCam->GetLocalTM());
			Quat rot = m_pVehicle->GetEntity()->GetRotation();
			Vec3 xworld = rot*Vec3(1,0,0); xworld.z = 0;
			const int sz = sizeof(m_posxCWhist)/sizeof(m_posxCWhist[0]);
			if ((m_dtHist -= gEnv->pTimer->GetFrameRate()) < 0)
			{
				m_dtHist += 0.2f;
				for(int i=sz-1; i>=1; --i)
					m_posxCWhist[i] = m_posxCWhist[i-1];
				m_posxCWhist[0] = m_posxCW;
			}
			float xsum = m_posxCW;
			for(int i=0; i<sz; xsum+=m_posxCWhist[i++]);
			mtx.SetTranslation(Vec3(0,m_pos0CW.y,m_poszCW) + (xworld.normalized()*rot)*max(-m_maxxCWcam,min(m_maxxCWcam,xsum/sz)) + m_offsCWcam);
		}
	}

	const SVehicleDamageParams& damageParams = m_pVehicle->GetDamageParams();
	m_submergedRatioMax = damageParams.submergedRatioMax;
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::UpdateSounds(const float deltaTime)
{ 
  CRY_PROFILE_FUNCTION( PROFILE_GAME );

  // engine load  
  float loadTarget = -1.f;
  
  // update engine sound
  if (m_isEnginePowered && !m_isEngineGoingOff)
  {
    float rpmScale = min(m_vehicleStatus.engineRPM / m_engineMaxRPM, 1.f);
    
    if (rpmScale < GetMinRPMSoundRatio())
    {
      // pitch rpm with pedal, if MinSoundRatio is used and rpm is below that
      rpmScale = min(GetMinRPMSoundRatio(), max(m_action.pedal, rpmScale));
    }

    // scale rpm down when in backward gear
    //if (m_currentGear == 0)
      //rpmScale *= 0.8; 
        
    if (m_vehicleStatus.bHandBrake)
    {
      Interpolate(m_rpmScale, rpmScale, 2.5f, deltaTime);
      m_rpmTarget = 0.f;
      loadTarget = 0.f;
    }
    else if (m_rpmTarget)
    {
      if (m_rpmTarget < m_rpmScale && DoGearSound())
      {
        loadTarget = 0.f;
      }

      Interpolate(m_rpmScale, m_rpmTarget, m_rpmGearShiftSpeed, deltaTime);
      
      float diff = abs(m_rpmScale-m_rpmTarget);
      
      if (m_gearSoundPending && m_currentGear >= 3) // only from 1st gear upward
      {          
        if (diff < 0.5f*abs(m_rpmScalePrev-m_rpmTarget))
        {
					REINST(play gear sound)
          //GetOrPlaySound(eSID_Gear, 0.f, m_enginePos);          
          m_gearSoundPending = false;
        }
      }
      
      if (diff < 0.02)
      {
        m_rpmTarget = 0.f;        
      }
    }
    else
    {
      // don't allow rpm to rev up when in 1st forward/backward gear and clutch is disengaged.
      // a bit hacky, but it prevents the sound glitch
      if (m_vehicleStatus.clutch < 0.9f && (m_vehicleStatus.iCurGear == 0 || m_vehicleStatus.iCurGear == 2))
      {
        rpmScale = min(m_rpmScale, rpmScale);
      }

      float interpspeed = (rpmScale < m_rpmScale) ? m_rpmRelaxSpeed : m_rpmInterpSpeed;
      Interpolate(m_rpmScale, rpmScale, interpspeed, deltaTime);
    }
    
    float rpmSound = max(ms_engineSoundIdleRatio, m_rpmScale);
		SetSoundParam(eSID_Run, "rpm_scale", rpmSound);
    SetSoundParam(eSID_Ambience, "rpm_scale", rpmSound);

    if (m_currentGear != m_vehicleStatus.iCurGear)
    { 
      // when shifting up from 1st upward, set sound target to low rpm to simulate dropdown 
      // during clutch disengagement
      if (m_currentGear >= 2 && m_vehicleStatus.iCurGear>m_currentGear)
      {
        m_rpmTarget = m_engineShiftDownRPM/m_engineMaxRPM;
        m_rpmScalePrev = m_rpmScale;
                
        if (DoGearSound())
        {
          loadTarget = 0.f;
          m_gearSoundPending = true;        
        }
      }

      if (DoGearSound() && !m_rpmTarget && !(m_currentGear<=2 && m_vehicleStatus.iCurGear<=2) && m_vehicleStatus.iCurGear > m_currentGear)
      {
        // do gearshift sound only for gears higher than 1st forward
        // in case rpmTarget has been set, shift is played upon reaching it        
        REINST(play gear sound)
				//GetOrPlaySound(eSID_Gear, 0.f, m_enginePos);        
      }

      m_currentGear = m_vehicleStatus.iCurGear;
    }

    if (loadTarget < 0.f)
    {
      // if not yet set, set load according to pedal
      loadTarget = abs(GetEnginePedal());
    }

    float loadSpeed = 1.f / (loadTarget >= m_load ? LOAD_RAMP_TIME : LOAD_RELAX_TIME);    
    Interpolate(m_load, loadTarget, loadSpeed, deltaTime);    
  }
  else
  {
    m_load = 0.f;
  }

	REINST(Set general parameters)
  //SetSoundParam(eSID_Run, "load", m_load);
  //SetSoundParam(eSID_Run, "surface", m_surfaceSoundStats.surfaceParam);  
  //SetSoundParam(eSID_Run, "scratch", m_surfaceSoundStats.scratching);  // removed there is no "scratch" parameter in the run event [Tomas]

  // tire slip sound
  if (m_maxSoundSlipSpeed > 0.f)
  {
		REINST(tire slip sound)
    //ISound* pSound = GetSound(eSID_Slip);    

    //if (m_surfaceSoundStats.slipRatio > 0.08f)
    //{ 
    //  float slipTimerPrev = m_surfaceSoundStats.slipTimer;
    //  m_surfaceSoundStats.slipTimer += deltaTime;
    //  
    //  const float slipSoundMinTime = 0.12f;
    //  if (!pSound && slipTimerPrev <= slipSoundMinTime && m_surfaceSoundStats.slipTimer > slipSoundMinTime)
    //  {
    //    pSound = PlaySound(eSID_Slip);
    //  }      
    //}
    //else if (m_surfaceSoundStats.slipRatio < 0.03f && m_surfaceSoundStats.slipTimer > 0.f)
    //{
    //  m_surfaceSoundStats.slipTimer -= deltaTime;

    //  if (m_surfaceSoundStats.slipTimer <= 0.f)
    //  {
    //    StopSound(eSID_Slip);
    //    pSound = 0;
    //    m_surfaceSoundStats.slipTimer = 0.f;
    //  }      
    //}
 
    //if (pSound)
    //{
    //  SetSoundParam(eSID_Slip, "slip_speed", m_surfaceSoundStats.slipRatio);
    //  SetSoundParam(eSID_Slip, "surface", m_surfaceSoundStats.surfaceParam);
    //  SetSoundParam(eSID_Slip, "scratch", (float)m_surfaceSoundStats.scratching);
    //  SetSoundParam(eSID_Slip, "in_out", m_soundStats.inout);
    //}   
  }
}


//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::UpdateSuspension(const float deltaTime)
{
  CRY_PROFILE_FUNCTION( PROFILE_GAME );

  float dt = max( deltaTime, 0.005f);

  IPhysicalEntity* pPhysics = GetPhysics();
  bool visible = m_pVehicle->GetGameObject()->IsProbablyVisible();
  bool distant = m_pVehicle->IsProbablyDistant();
  
  // update suspension and friction, if needed      
  float speed = m_PhysDyn.v.len();
  bool bSuspUpdate = false;
        
  pe_status_nparts tmpStatus;
  int numParts = pPhysics->GetStatus(&tmpStatus);

  Matrix34 worldTM( m_PhysPos.q );
  worldTM.AddTranslation( m_PhysPos.pos );

  assert(m_wheelParts.size() == m_pVehicle->GetWheelCount());

  float diffSusp = m_suspDampingMax - m_suspDampingMin;    
  float diffStabi = m_stabiMax - m_stabiMin;
  
  if (diffSusp || diffStabi)
  {
    if (abs(m_speedSuspUpdated-speed) > 0.25f) // only update when speed changes
    {
      float maxSpeed = (m_suspDampingMaxSpeed > 0.f) ? m_suspDampingMaxSpeed : 0.15f*m_maxSpeed;
      float speedNorm = min(1.f, speed/maxSpeed);
      
      if (diffSusp)
      {
        m_suspDamping = m_suspDampingMin + (speedNorm * diffSusp);
        bSuspUpdate = true;
      }           

      if (diffStabi)
      {
        m_stabi = m_stabiMin + (speedNorm * diffStabi);
        
        pe_params_car params;
        params.kStabilizer = m_stabi;
        pPhysics->SetParams(&params, 1);        
      }

      m_speedSuspUpdated = speed;    
    }
  }
  
  bool bBraking = m_movementAction.brake && m_isEnginePowered;
  
  m_compressionMax = 0.f;
  m_avgLateralSlip = 0.f;
  m_avgWheelRot = 0.f;
  int ipart = 0;
  int numSlip = 0;
  int numRot = 0;  
  int numDriving = 0, numDrivingContacts = 0;
  m_wheelContactsLeft = 0; 
  m_wheelContactsRight = 0;
  m_surfaceSoundStats.scratching = 0;
  bool bContactsChanged = false;
  
  int numWheels = m_pVehicle->GetWheelCount();
  for (int i=0; i<numWheels; ++i)
  { 
    pe_params_wheel wheelParams;
    bool bUpdate = bSuspUpdate;
    IVehicleWheel* pWheel = m_wheelParts[i]->GetIWheel();

    pe_status_wheel ws;
    ws.iWheel = i;
    if (!pPhysics->GetStatus(&ws))
      continue;

    const Matrix34& wheelTM = m_wheelParts[i]->GetLocalTM(false);
	//this is littlebit a problem( getting entity position )
    const pe_cargeomparams* pGeomParams = pWheel->GetCarGeomParams();
        
    numDriving += pGeomParams->bDriving;
    
    if (ws.bContact != (int)m_wheelStats[i].bContact)
    {
      m_wheelStats[i].bContact = ws.bContact?true:false;
      
      if (pGeomParams->bDriving)
        bContactsChanged = true;
    }

    if (ws.bContact)
    { 
      m_avgWheelRot += abs(ws.w)*ws.r;
      ++numRot;
      numDrivingContacts += pGeomParams->bDriving;

      if (wheelTM.GetTranslation().x < 0.f)
        ++m_wheelContactsLeft;
      else
        ++m_wheelContactsRight; 
    }
    
    // update friction for handbraking
    if ((!gEnv->IsClient() || !distant) && m_maxBrakingFriction > 0.f && ws.bContact && (bBraking || m_wheelStats[i].handBraking))
    {
      if (bBraking)
      {
        // apply maxBrakingFriction for handbraking
        float friction = m_maxBrakingFriction;

        if (m_carParams.steerTrackNeutralTurn == 0.f)
        {
          // when steering, keep friction high at inner front wheels to achieve better turning
          Vec3 pos = wheelTM.GetTranslation();

          float diff = max(0.f, pGeomParams->maxFriction - m_maxBrakingFriction);
          float steerAmt = abs(m_action.steer)/DEG2RAD(m_steerMax);

          if (pos.y > 0.f && sgn(m_action.steer)*sgn(pos.x)>0)
            friction = m_maxBrakingFriction + diff * steerAmt;
          else
            friction = m_maxBrakingFriction + diff * (1.f-steerAmt); 
        }
                        
        wheelParams.maxFriction = friction;
        wheelParams.minFriction = friction;

        m_wheelStats[i].handBraking = true;        
      }      
      else
      {
        wheelParams.maxFriction = pGeomParams->maxFriction;
        wheelParams.minFriction = pGeomParams->minFriction;
        
        m_wheelStats[i].handBraking = false;        
      }
      bUpdate = true;
    }    

    
    // update slip friction
    if (!m_wheelStats[i].handBraking)
    {
      // update friction with slip vel
      float lateralSlip(0.f);           
      if (ws.bContact && ws.velSlip.GetLengthSquared() > sqr(0.01f))
      {
        if (m_carParams.steerTrackNeutralTurn == 0.f && (ipart = numParts - numWheels + i) >= 0)
        { 
          pe_status_pos wp;
          wp.ipart = ipart;
          if (pPhysics->GetStatus(&wp))
            lateralSlip = abs(wp.q.GetColumn0() * ws.velSlip);         
        }
        else
        {
          lateralSlip = abs(worldTM.GetColumn0() * ws.velSlip);
        }
      }
      
      if (lateralSlip < 0.001f)
        lateralSlip = 0.f;

      if (ws.bContact)
      {
        m_avgLateralSlip += lateralSlip;
        ++numSlip;
      }


      if (lateralSlip != m_wheelStats[i].lateralSlip)
      { 
        if (m_carParams.steerTrackNeutralTurn == 0.f && pWheel->GetSlipFrictionMod(1.f) != 0.f)          
        {
          float slipMod = pWheel->GetSlipFrictionMod(lateralSlip);          
          
          wheelParams.kLatFriction = pGeomParams->kLatFriction + slipMod;
          m_wheelStats[i].friction = wheelParams.kLatFriction;
                      
          bUpdate = true;
        }
                  
        m_wheelStats[i].lateralSlip = lateralSlip;          
      }    
    }

    if (bUpdate)
    {
      if (bSuspUpdate)
        wheelParams.kDamping = m_suspDamping;
      
      wheelParams.iWheel = i;      
      pPhysics->SetParams(&wheelParams, THREAD_SAFE);
    }

    // check for hard bump
    if (visible && !distant && (m_bumpMinSusp + m_bumpMinSpeed > 0.f) && m_lastBump > 1.f && ws.suspLen0 > 0.01f && ws.suspLen < ws.suspLen0)
    { 
      // compression as fraction of relaxed length over time
	  m_wheelStats[i].compression = ((m_wheelStats[i].suspLen-ws.suspLen)/ws.suspLen0) / dt;
      m_compressionMax = max(m_compressionMax, m_wheelStats[i].compression);
    }
    m_wheelStats[i].suspLen = ws.suspLen;
  }  
 
  // compute average lateral slip
  m_wheelContacts = numRot;
  m_avgLateralSlip /= max(1, numSlip);
  m_avgWheelRot /= max(1, numRot); 
  
  if (bContactsChanged && m_carParams.steerTrackNeutralTurn == 0.f)
  {
    // bad, as we might be setting wheelParams twice this frame. optimize when room (move to physics)
    pe_params_wheel pw;    
    float tScale = numDrivingContacts ? float(numDriving)/numDrivingContacts : 1.f;
            
    for (int i=0; i<numWheels; ++i)
    {
      IVehicleWheel* pWheel = m_wheelParts[i]->GetIWheel();
      
      if (!pWheel->GetCarGeomParams()->bDriving)
        continue;
	
			m_wheelStats[i].torqueScale = pWheel->GetTorqueScale() * (numDrivingContacts > 0 ? ((m_wheelStats[i].bContact) ? tScale : 0.f) : 1.f);

      pw.iWheel = i;
      pw.Tscale = m_wheelStats[i].torqueScale;
      pPhysics->SetParams(&pw, THREAD_SAFE);
    }    
  }
  
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::UpdateBrakes(const float deltaTime)
{
  if (m_movementAction.brake || m_pVehicle->GetStatus().health <= 0.f)
    m_action.bHandBrake = 1;
  else
    m_action.bHandBrake = 0;

  if (m_isBreakingOnIdle && m_movementAction.power == 0.0f)
  {
    m_action.bHandBrake = 1;
  }

	// brake lights should come on if... (+ engine is on, and actor is driving)
	//	- handbrake is activated 
	//	- pedal is pressed, and the vehicle is moving in the opposite direction
  if (IsPowered() && m_actorId )
	{
		Vec3 yAxis = m_statusPos.q.GetColumn1();
		float vel = yAxis.dot(m_statusDyn.v);

		if ((abs(m_action.pedal) > 0.1f && abs(vel) > 0.1f && sgn(vel)*sgn(m_action.pedal) < 0)
			|| m_action.bHandBrake == 1)
		{
			if (m_brakeTimer == 0.f)
			{
				SVehicleEventParams params;
				params.bParam = true;
				m_pVehicle->BroadcastVehicleEvent(eVE_Brake, params);
			}

			m_brakeTimer += deltaTime;  
		}
		else
		{
			if (m_brakeTimer > 0.f)
			{
				SVehicleEventParams params;
				params.bParam = false;
				m_pVehicle->BroadcastVehicleEvent(eVE_Brake, params);

				// airbrake sound
				if (m_airbrakeTime > 0.f && IsPowered())
				{ 
					REINST(airbrake sound)
					//if (m_brakeTimer > m_airbrakeTime)
					//{
					//	char name[256];
					//	cry_sprintf(name, "sounds/vehicles:%s:airbrake", m_pVehicle->GetEntity()->GetClass()->GetName());
					//	m_pEntitySoundsProxy->PlaySound(name, Vec3(0), FORWARD_DIRECTION, FLAG_SOUND_DEFAULT_3D, 0, eSoundSemantic_Vehicle);                
					//}          
				}  
			}

			m_brakeTimer = 0.f;  
		}
	}
}

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::UpdateSuspensionSound(const float deltaTime)
{
  CRY_PROFILE_FUNCTION( PROFILE_GAME );

  if (m_pVehicle->GetStatus().health <= 0.f)
    return;

  const bool visible = m_pVehicle->GetGameObject()->IsProbablyVisible();
  const bool distant = m_pVehicle->IsProbablyDistant();

  if ( distant || !visible )
	return;

  IPhysicalEntity* pPhysics = GetPhysics();
  if (!pPhysics)
    return;

  const float speed = m_statusDyn.v.len();
  const int numWheels = m_pVehicle->GetWheelCount();

  int soundMatId = 0;

  m_lostContactTimer += deltaTime;
  for (int i=0; i<numWheels; ++i)
  {
	pe_status_wheel ws;
    ws.iWheel = i;
    if (!pPhysics->GetStatus(&ws))
      continue;

    if (ws.bContact)
    { 
      // sound-related                   
      if (!distant && visible && !m_surfaceSoundStats.scratching && soundMatId==0 && speed > 0.001f)
      {
        if (gEnv->p3DEngine->GetWaterLevel(&ws.ptContact) > ws.ptContact.z+0.02f)
        {        
          soundMatId = gEnv->pPhysicalWorld->GetWaterMat();
          m_lostContactTimer = 0;
        }
        else if (ws.contactSurfaceIdx > 0 /*&& soundMatId != gEnv->pPhysicalWorld->GetWaterMat()*/)
        {   
          if (m_wheelParts[i]->GetState() == IVehiclePart::eVGS_Damaged1)
            m_surfaceSoundStats.scratching = 1;
          
          soundMatId = ws.contactSurfaceIdx;
          m_lostContactTimer = 0;
        }
      }      
    }
  }

  m_lastBump += deltaTime;
  if (visible && !distant && m_pVehicle->GetStatus().speed > m_bumpMinSpeed && m_lastBump > 1.f)
  { 
    if (m_compressionMax > m_bumpMinSusp)
    {
			REINST(play bump sound)
      // do bump sound        
			//if (ISound* pSound = PlaySound(eSID_Bump))
			//{
			//	pSound->SetParam("speed", ms_engineSoundIdleRatio + (1.f-ms_engineSoundIdleRatio)*m_speedRatio, false);
			//	pSound->SetParam("intensity", min(1.f, m_bumpIntensityMult*m_compressionMax/m_bumpMinSusp), false);
			//	m_lastBump = 0;
			//}      
    }            
  }   

  // set surface sound type
  if (visible && !distant && soundMatId != m_surfaceSoundStats.matId)
  { 
    if (m_lostContactTimer == 0.f || m_lostContactTimer > 3.f)
    {
      if (soundMatId > 0)
      {
        //m_surfaceSoundStats.surfaceParam = GetSurfaceSoundParam(soundMatId);
      }    
      else
      {
        //m_surfaceSoundStats.surfaceParam = 0.f;      
      }   
      m_surfaceSoundStats.matId = soundMatId;
    }
  } 

}


//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementStdWheeled::ProcessAI(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	float dt = max( deltaTime,0.005f);

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
		m_movementAction.brake = true;
	}
	else
	{

		Matrix33 entRotMatInvert( m_PhysPos.q );
		entRotMatInvert.Invert();
		float currentAngleSpeed = RAD2DEG(-m_PhysDyn.w.z);

		const float maxSteer = RAD2DEG(gf_PI/4.f); // fix maxsteer, shouldn't change  
		Vec3 vVel = m_PhysDyn.v;
		Vec3 vVelR = entRotMatInvert * vVel;
		float currentSpeed =vVel.GetLength();
		vVelR.NormalizeSafe();
		if ( vVelR.Dot( FORWARD_DIRECTION ) < 0 )
			currentSpeed *= -1.0f;

		// calculate pedal
		const float accScale = 0.5f;
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
		float angle = RAD2DEG( acos_tpl(cosAngle));
		if ( vMoveR.Dot( Vec3( 1.0f,0.0f,0.0f ) )< 0 )
			 angle = -angle;

//		int step =0;
		m_movementAction.rotateYaw = angle * 1.75f/ maxSteer; 

		// implementation 1. if there is enough angle speed, we don't need to steer more
		if ( fabsf(currentAngleSpeed) > fabsf(angle) && angle*currentAngleSpeed > 0.0f )
		{
			m_movementAction.rotateYaw = m_action.steer*0.995f; 
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

		if ( fabsf( angle ) > 20.0f && currentSpeed > 7.0f ) 
		{
			m_movementAction.power *= 0.0f ;
//			step =4;
		}

		// for more tight condition to make curve.
		if ( fabsf( angle ) > 40.0f && currentSpeed > 3.0f ) 
		{
			m_movementAction.power *= 0.0f ;
//			step =5;
		}

//		m_prevAngle =  angle;
//		char buf[1024];
///		cry_sprintf(buf, "steering	%4.2f	%4.2f %4.2f	%4.2f	%4.2f	%4.2f	%d\n", deltaTime,currentSpeed,angle,currentAngleSpeed, m_movementAction.rotateYaw,currentAngleSpeed-m_prevAngle,step);
//		OutputDebugString( buf );
	}

}

//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementStdWheeled::ProcessMovement(const float deltaTime)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );
  
	m_netActionSync.UpdateObject(this);

	CryAutoCriticalSection lk(m_lock);

	CVehicleMovementBase::ProcessMovement(deltaTime);

	IPhysicalEntity* pPhysics = GetPhysics();
	pPhysics->GetStatus(&m_PhysDyn);
	pPhysics->GetStatus(&m_PhysPos);

	NETINPUT_TRACE(m_pVehicle->GetEntityId(), m_action.pedal);

	float speed = m_PhysDyn.v.len();

	bool driverActive = m_actorId && !m_pVehicle->IsDestroyed();
	if (m_massCW>0)
	{
		pe_params_part pp;
		pp.partid = m_partidCW;
		if (pPhysics->GetParams(&pp) && driverActive!=(pp.mass>0))
		{
			new(&pp) pe_params_part;
			pp.partid = m_partidCW;
			pp.mass = driverActive ? m_massCW : 0;
			pPhysics->SetParams(&pp);
		}
	}

	pe_params_car pc; pPhysics->GetParams(&pc);
	if (m_maskWheelDisable)
	{
		int active = speed<m_velWheelDisable && driverActive;
		pe_params_part pp;
		pe_status_nparts snp;
		int nParts = pPhysics->GetStatus(&snp);
		for(int i=0; i<pc.nWheels; i++)
			if (m_maskWheelDisable & 1<<i) 
			{
				pp.ipart = nParts-pc.nWheels+i;
				pp.flagsAND = pp.flagsColliderAND = 0;
				pp.flagsOR = geom_collides & -active;
				pp.flagsColliderOR = (geom_colltype0|geom_colltype_vehicle) & -active;
				pPhysics->SetParams(&pp);
			}
	}

	// speed ratio    
	float speedRel = min(speed, m_vMaxSteerMax) / m_vMaxSteerMax;  
	float steerMax = GetMaxSteer(speedRel);
	float steering = m_movementAction.rotateYaw;    

	if (driverActive && m_massCW>0)
	{
		Vec3 xworld = m_PhysPos.q*Vec3(1,0,0), yworld = m_PhysPos.q*Vec3(0,1,0);
		xworld.z=0; xworld.normalize();
		m_poszCW = m_pos0CW.z+m_kzCW*fabs(m_posxCW);
		Vec3 posw = m_PhysPos.pos+m_PhysPos.q*Vec3(0,m_pos0CW.y,m_poszCW)+xworld*m_posxCW;
		float vcur = (m_PhysDyn.v+(m_PhysDyn.w^posw-m_PhysDyn.centerOfMass))*xworld;
		float xcur = (m_PhysPos.q*Vec3(0,0,m_poszCW))*xworld;
		float xtrg = 0, k = m_ktiltCW[0];
		pe_status_wheel sw;
		Vec3 ptc[2] = { Vec3(0),Vec3(0) };
		for(sw.iWheel=0; sw.iWheel<pc.nWheels; sw.iWheel++)
		{
			pPhysics->GetStatus(&sw);
			Vec3 ptloc = (sw.ptContact-m_PhysPos.pos)*m_PhysPos.q;
			if (sw.bContact)
				ptc[isneg(-ptloc.y)] = ptloc;
		}
		if (speed>m_minVelSteerLean && fabs_tpl(steering)>0.01f && ptc[0].y*ptc[1].y<0)
		{
			float invR = sin(steerMax*ptc[0].y/(ptc[1].y-ptc[0].y))/ptc[0].y;
			pe_simulation_params simp; pPhysics->GetParams(&simp);
			float g = -simp.gravity.z*Vec2(yworld).GetLength();
			float tg = sqr(yworld*m_PhysDyn.v)*invR/g;
			xtrg = m_poszCW*tg/sqrt_tpl(1+tg*tg)*sgnnz(steering);
			float dxFrame = (m_PhysPos.q*Vec3(0,0,1)-m_qPrev*Vec3(0,0,1))*xworld;
			k = m_ktiltCW[1+isneg((xtrg-xcur)*dxFrame)];
		}
		float dx = xtrg-xcur+(xtrg-xcur)*k-m_posxCW;
		float dv = dx*m_stiltCW - (m_velxCW+vcur);
		m_velxCW += dv;
		m_posxCW += m_velxCW*deltaTime;
		if (m_posxCW*m_velxCW > m_maxxCW*fabs_tpl(m_velxCW))
		{
			m_posxCW = max(-m_maxxCW,min(m_maxxCW, m_posxCW));
			m_velxCW = 0;
		}

		pe_params_part pp;
		pp.partid = m_partidCW;
		pp.pos = Vec3(0,m_pos0CW.y,m_poszCW)+(xworld*m_PhysPos.q)*m_posxCW;
		GetPhysics()->SetParams(&pp, THREAD_SAFE);
		pe_action_impulse ai; ai.iApplyTime=0;
		ai.impulse = xworld*(m_massCW*-dv/(m_PhysDyn.mass-m_massCW));
		GetPhysics()->Action(&ai, THREAD_SAFE);
		pPhysics->GetStatus(&m_PhysDyn);
		pe_action_set_velocity asv;
		asv.w = m_PhysDyn.w-yworld*((yworld*m_PhysDyn.w)*m_dampingy*deltaTime);
		pPhysics->Action(&asv, THREAD_SAFE);
		m_qPrev = m_PhysPos.q;
	}

	if (!(m_actorId && m_isEnginePowered) || m_pVehicle->IsDestroyed() )
	{

		const float sleepTime = 3.0f;

		if ( m_passengerCount > 0 || ( m_pVehicle->IsDestroyed() && m_bForceSleep == false ))
		{
			UpdateSuspension(deltaTime);
			UpdateAxleFriction(m_movementAction.power, true, deltaTime);
		}

		m_action.bHandBrake = (m_initialHandbreak || m_movementAction.brake || m_pVehicle->IsDestroyed()) ? 1 : 0;
		m_action.pedal = 0;
		m_action.steer = 0;
		pPhysics->Action(&m_action, THREAD_SAFE);

		bool maybeInTheAir = fabsf(m_PhysDyn.v.z) > 1.0f;
		if ( maybeInTheAir )
		{
			UpdateGravity(-9.81f * 1.4f);
			ApplyAirDamp(DEG2RAD(20.f), DEG2RAD(10.f), deltaTime, THREAD_SAFE);
		}

		if ( m_pVehicle->IsDestroyed() && m_bForceSleep == false )
		{
			int numContact= 0;
			int numWheels = m_pVehicle->GetWheelCount();
			for (int i=0; i<numWheels; ++i)
			{ 
				pe_status_wheel ws;
				ws.iWheel = i;
				if (!pPhysics->GetStatus(&ws))
				  continue;
				if (ws.bContact)
					numContact ++;
			}
			if ( numContact > m_pVehicle->GetWheelCount()/2 || speed<0.2f )
				m_forceSleepTimer += deltaTime;
			else
				m_forceSleepTimer = max(0.0f,m_forceSleepTimer-deltaTime);

			if ( m_forceSleepTimer > sleepTime )
			{
				pe_params_car params;
				params.minEnergy = 0.05f;
				pPhysics->SetParams(&params, 1);
				m_bForceSleep = true;
			}
		}
		return;

	}

	// moved to main thread
	UpdateSuspension(deltaTime);   	
	UpdateAxleFriction(m_movementAction.power, true, deltaTime);
	//UpdateBrakes(deltaTime);

	// calc steer error  	
	float steerError = steering * steerMax - m_action.steer;
	steerError = (fabs(steerError)<0.01) ? 0 : steerError;

	if (fabs(m_movementAction.rotateYaw) > 0.005f)
	{ 
		bool reverse = sgn(m_action.steer)*sgn(steerError) < 0;
		float steerSpeed = GetSteerSpeed(reverse ? 0.f : speedRel);

		// adjust steering based on current error    
		m_action.steer = m_action.steer + sgn(steerError) * DEG2RAD(steerSpeed) * deltaTime;  
		m_action.steer = CLAMP(m_action.steer, -steerMax, steerMax);
	}
	else
	{
    // relax to center
		float d = -m_action.steer;
		float a = min(DEG2RAD(deltaTime * m_steerRelaxation), 1.0f);
		m_action.steer = m_action.steer + d * a;
	}

	// reduce actual pedal with increasing steering and velocity
	float maxPedal = 1 - (speedRel * abs(steering) * m_pedalLimitMax);  
	float damageMul = 1.0f - 0.7f*m_damage;  
	float submergeMul = 1.0f;  
	float totalMul = 1.0f;  
	bool bInWater = m_PhysDyn.submergedFraction > 0.01f;
	if ( GetMovementType()!=IVehicleMovement::eVMT_Amphibious && bInWater )
	{
		submergeMul = max( 0.0f, 0.04f - m_PhysDyn.submergedFraction ) * 10.0f;
		submergeMul *=submergeMul;
		submergeMul = max( 0.2f, submergeMul );
	}

	Vec3 vUp(m_PhysPos.q.GetColumn2());
	Vec3 vUnitUp(0.0f,0.0f,1.0f);

	float slopedot = vUp.Dot( vUnitUp );
	bool bSteep =  fabsf(slopedot) < 0.7f;
	{ //fix for 30911
		if ( bSteep && speed > 7.5f )
		{
			Vec3 vVelNorm = m_PhysDyn.v.GetNormalizedSafe();
			if ( vVelNorm.Dot(vUnitUp)> 0.0f )
			{
				pe_action_impulse imp;
				imp.impulse = -m_PhysDyn.v;
				imp.impulse *= deltaTime * m_PhysDyn.mass*5.0f;      
				imp.point = m_PhysDyn.centerOfMass;
				imp.iApplyTime = 0;
				GetPhysics()->Action(&imp, THREAD_SAFE);
			}
		}
	}

	totalMul = max( 0.3f, damageMul *  submergeMul );
	m_action.pedal = CLAMP(m_movementAction.power, -maxPedal, maxPedal ) * totalMul;

	// make sure cars can't drive under water
	if(GetMovementType()!=IVehicleMovement::eVMT_Amphibious && m_PhysDyn.submergedFraction >= m_submergedRatioMax && m_damage >= 0.99f)
	{
		m_action.pedal = 0.0f;
	}
	
	pPhysics->Action(&m_action, THREAD_SAFE);
  
	if ( !bSteep && Boosting() )
		ApplyBoost(speed, 1.25f*m_maxSpeed*GetWheelCondition()*damageMul, m_boostStrength, deltaTime);  

	if (m_wheelContacts <= 1 && speed > 5.f)
	{
		ApplyAirDamp(DEG2RAD(20.f), DEG2RAD(10.f), deltaTime, THREAD_SAFE);
		if ( !bInWater )
			UpdateGravity(-9.81f * 1.4f);  
	}

	if (m_netActionSync.PublishActions( CNetworkMovementStdWheeled(this) ))
		CHANGED_NETWORK_STATE(m_pVehicle,  eEA_GameClientDynamic );
 
}


//----------------------------------------------------------------------------------
void CVehicleMovementStdWheeled::Boost(bool enable)
{  
  if (enable)
  {
    if (m_action.bHandBrake)
      return;
  }

  CVehicleMovementBase::Boost(enable);
}


//------------------------------------------------------------------------
bool CVehicleMovementStdWheeled::RequestMovement(CMovementRequest& movementRequest)
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

void CVehicleMovementStdWheeled::GetMovementState(SMovementState& movementState)
{
	IPhysicalEntity* pPhysics = GetPhysics();
  if (!pPhysics)
    return;

  if (m_maxSpeed == 0.f)
  {
    pe_status_vehicle_abilities ab;
    if (pPhysics->GetStatus(&ab))
      m_maxSpeed = ab.maxVelocity * 0.5f; // fixme
  }  

	movementState.minSpeed = 0.0f;
	movementState.maxSpeed = m_maxSpeed;
	movementState.normalSpeed = movementState.maxSpeed;
}


//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::Serialize(TSerialize ser, EEntityAspects aspects) 
{
	CVehicleMovementBase::Serialize(ser, aspects);

	if (ser.GetSerializationTarget() == eST_Network)
	{
		if (aspects&CNetworkMovementStdWheeled::CONTROLLED_ASPECT)
			m_netActionSync.Serialize(ser, aspects);
	}
	else 
	{	
		ser.Value("brakeTimer", m_brakeTimer);
		ser.Value("brake", m_movementAction.brake);
		ser.Value("tireBlownTimer", m_tireBlownTimer);
		ser.Value("initialHandbreak", m_initialHandbreak);
	 
		int blownTires = m_blownTires;
		ser.Value("blownTires", m_blownTires);
		ser.Value("bForceSleep", m_bForceSleep);

		if (ser.IsReading() && blownTires != m_blownTires)
		  SetEngineRPMMult(GetWheelCondition());
	    
		ser.Value("m_prevAngle", m_prevAngle);
	}
};

//------------------------------------------------------------------------
void CVehicleMovementStdWheeled::UpdateSurfaceEffects(const float deltaTime)
{ 
  CRY_PROFILE_FUNCTION( PROFILE_GAME );
  
  if (0 == g_pGameCVars->v_pa_surface)
  {
    ResetParticles();
    return;
  }

  const SVehicleStatus& status = m_pVehicle->GetStatus();
  if (status.speed < 0.01f)
    return;

  float distSq = m_pVehicle->GetEntity()->GetWorldPos().GetSquaredDistance(GetISystem()->GetViewCamera().GetPosition());
  if (distSq > sqr(300.f) || (distSq > sqr(50.f) && !m_pVehicle->GetGameObject()->IsProbablyVisible()))
    return;

  IPhysicalEntity* pPhysics = GetPhysics();
  
  // don't render particles for drivers in 1st person (E3 request)
  bool hideForFP = false;
  if (GetMovementType() == eVMT_Land && m_carParams.steerTrackNeutralTurn == 0.f)
  {
    IActor* pActor = m_pVehicle->GetDriver();
		IVehicleSeat* pSeat = pActor ? m_pVehicle->GetSeatForPassenger(pActor->GetEntityId()) : NULL;
		IVehicleView* pView = pSeat ? pSeat->GetCurrentView() : NULL;
    if (pActor && pActor->IsClient() && (pView != NULL) && !pView->IsThirdPerson())
      hideForFP = true;
  }
    
  float soundSlip = 0;

#if ENABLE_VEHICLE_DEBUG
  if (DebugParticles())
  {
    float color[] = {1,1,1,1};
    IRenderAuxText::Draw2dLabel(100, 280, 1.3f, color, false, "%s:", m_pVehicle->GetEntity()->GetName());
  }
#endif
    
  SEnvironmentParticles* envParams = m_pPaParams->GetEnvironmentParticles();
  SEnvParticleStatus::TEnvEmitters::iterator emitterIt = m_paStats.envStats.emitters.begin();
  SEnvParticleStatus::TEnvEmitters::iterator emitterItEnd = m_paStats.envStats.emitters.end();

  for (; emitterIt != emitterItEnd; ++emitterIt)
  { 
    if (emitterIt->layer < 0)
    {
      assert(0);
      continue;
    }

    if (!emitterIt->active)
      continue;

    const SEnvironmentLayer& layer = envParams->GetLayer(emitterIt->layer);

    //if (!layer.active || !layer.IsGroupActive(emitterIt->group))
    
    // scaling for each wheelgroup is based on vehicle speed + avg. slipspeed
    float slipAvg = 0; 
    int cnt = 0;
    bool bContact = false;
    int matId = 0;

		// Calculate the average wheel position and find the water level there - not necessary to find the water
		// level separately for each wheel.
		float fWaterLevel = -FLT_MAX;
    int wheelCount = layer.GetWheelCount(emitterIt->group);
		{
			Vec3 averageWheelPosition(ZERO);
			int foundWheels = 0;
			for (int w=0; w<wheelCount; ++w)
			{
				pe_status_wheel wheelStats;
				wheelStats.iWheel = layer.GetWheelAt(emitterIt->group, w) - 1;

				if (!pPhysics->GetStatus(&wheelStats))
					continue;

				++foundWheels;
				averageWheelPosition += wheelStats.ptContact;
			}
			if(foundWheels)
			{
				averageWheelPosition /= float(foundWheels);
				fWaterLevel = gEnv->p3DEngine->GetWaterLevel(&averageWheelPosition);
			}
		}

    for (int w=0; w<wheelCount; ++w)
    {
      // all wheels in group
      ++cnt;
      pe_status_wheel wheelStats;
      wheelStats.iWheel = layer.GetWheelAt(emitterIt->group, w) - 1;
      
      if (!pPhysics->GetStatus(&wheelStats))
        continue;

      if (wheelStats.bContact)
      {
        bContact = true;
        
        // take care of water
        if (fWaterLevel > wheelStats.ptContact.z+0.02f)
        {
		  if ( fWaterLevel > wheelStats.ptContact.z+2.0f)
		  {
			slipAvg =0.0f;
			bContact = false;
		  }
          matId = gEnv->pPhysicalWorld->GetWaterMat();
        }
        else if (wheelStats.contactSurfaceIdx > matId)
          matId = wheelStats.contactSurfaceIdx;

        if (wheelStats.bSlip)
          slipAvg += wheelStats.velSlip.len();
      }
    }

    if (!bContact && !emitterIt->bContact)
      continue;
    
    emitterIt->bContact = bContact;    
    slipAvg /= cnt;

    bool isSlip = !strcmp(layer.GetName(), "slip");    
    float vel = isSlip ? 0.f : m_statusDyn.v.len(); 
    vel += 1.f*slipAvg;
     
    soundSlip = max(soundSlip, slipAvg);       
    
    float countScale = 1;
    float sizeScale = 1;
		float speedScale = 1;

    if (hideForFP || !bContact || matId == 0)    
      countScale = 0;          
    else
      GetParticleScale(layer, vel, 0.f, countScale, sizeScale, speedScale);
    
    IEntity* pEntity = m_pVehicle->GetEntity();
    SEntitySlotInfo info;
    info.pParticleEmitter = 0;
    pEntity->GetSlotInfo(emitterIt->slot, info);
        
    if (matId != emitterIt->matId)
    {
      // change effect                        
      const char* effect = GetEffectByIndex(matId, layer.GetName());
      IParticleEffect* pEff = 0;   
              
      if (effect && (pEff = gEnv->pParticleManager->FindEffect(effect)))
      {      
#if ENABLE_VEHICLE_DEBUG
        if (DebugParticles())          
          CryLog("<%s> changes sfx to %s (slot %i)", pEntity->GetName(), effect, emitterIt->slot);
#endif

        if (info.pParticleEmitter)
        {   
          // free old emitter and load new one, for old effect to die gracefully           
          info.pParticleEmitter->Activate(false);            
          pEntity->FreeSlot(emitterIt->slot);
        }         
        
        emitterIt->slot = pEntity->LoadParticleEmitter(emitterIt->slot, pEff);
        
        if (emitterIt->slot != -1)
          pEntity->SetSlotLocalTM(emitterIt->slot, Matrix34(emitterIt->quatT));

        info.pParticleEmitter = 0;
        pEntity->GetSlotInfo(emitterIt->slot, info);

        emitterIt->matId = matId;
      }
      else 
      {
#if ENABLE_VEHICLE_DEBUG
        if (DebugParticles())
          CryLog("<%s> found no effect for %i", pEntity->GetName(), matId);
#endif

        // effect not available, disable
        //info.pParticleEmitter->Activate(false);
        countScale = 0.f; 
        emitterIt->matId = 0;
      }        
    }

    if (emitterIt->matId == 0)      
      countScale = 0.f;

    if (info.pParticleEmitter)
    {
      SpawnParams sp;
      sp.fSizeScale = sizeScale;
      sp.fCountScale = countScale;
			sp.fSpeedScale = speedScale;
      info.pParticleEmitter->SetSpawnParams(sp);
    }
   
#if ENABLE_VEHICLE_DEBUG
    if (DebugParticles())
    {
      float color[] = {1,1,1,1};
      IRenderAuxText::Draw2dLabel((float)(100+330*emitterIt->layer), (float)(300+25*emitterIt->group), 1.2f, color, false, "group %i, matId %i: sizeScale %.2f, countScale %.2f, speedScale %.2f (emit: %i)", emitterIt->group, emitterIt->matId, sizeScale, countScale, speedScale, info.pParticleEmitter?1:0);
      gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_pVehicle->GetEntity()->GetSlotWorldTM(emitterIt->slot).GetTranslation(), 0.2f, ColorB(0,0,255,200));
    }     
#endif
  }
  
  if (m_maxSoundSlipSpeed > 0.f)
    m_surfaceSoundStats.slipRatio = min(soundSlip/m_maxSoundSlipSpeed, 1.f);
}

//------------------------------------------------------------------------
bool CVehicleMovementStdWheeled::DoGearSound()
{
  return true;
}

void CVehicleMovementStdWheeled::GetMemoryUsage(ICrySizer * pSizer) const
{
	pSizer->Add(*this);
	GetMemoryUsageInternal(pSizer);
}

void CVehicleMovementStdWheeled::GetMemoryUsageInternal(ICrySizer * pSizer) const
{
	pSizer->AddObject(m_wheelParts);
	pSizer->AddObject(m_wheelStats);
	CVehicleMovementBase::GetMemoryUsageInternal(pSizer);
}

//------------------------------------------------------------------------
CNetworkMovementStdWheeled::CNetworkMovementStdWheeled()
: m_steer(0.0f),
m_pedal(0.0f),
m_brake(false),
m_boost(false)
{
}

//------------------------------------------------------------------------
CNetworkMovementStdWheeled::CNetworkMovementStdWheeled(CVehicleMovementStdWheeled *pMovement)
{
	m_steer = pMovement->m_movementAction.rotateYaw;
	m_pedal = pMovement->m_movementAction.power;
	m_brake = pMovement->m_movementAction.brake;
  m_boost = pMovement->m_boost;
}

//------------------------------------------------------------------------
void CNetworkMovementStdWheeled::UpdateObject(CVehicleMovementStdWheeled *pMovement)
{
	pMovement->m_movementAction.rotateYaw = m_steer;
	pMovement->m_movementAction.power = m_pedal;
	pMovement->m_movementAction.brake = m_brake;
  pMovement->m_boost = m_boost;
}

//------------------------------------------------------------------------
void CNetworkMovementStdWheeled::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget()==eST_Network)
	{
		if (aspects & CONTROLLED_ASPECT)
		{      
			ser.Value("pedal", m_pedal, 'vPed');
			ser.Value("steer", m_steer, 'vStr');      
			ser.Value("brake", m_brake, 'bool');
      ser.Value("boost", m_boost, 'bool');      
		}		
	}	
}
