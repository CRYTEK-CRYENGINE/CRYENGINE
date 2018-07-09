// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleDamageBehaviorTire.h"

#include "IVehicleSystem.h"
#include "Game.h"
#include "GameRules.h"
#include "IAnimatedCharacter.h"
#include "IActorSystem.h"

#define AI_IMMOBILIZED_TIME 4

CVehicleDamageBehaviorBlowTire::CVehicleDamageBehaviorBlowTire()
	: m_pVehicle(nullptr)
	, m_isActive(false)
{}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorBlowTire::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;
	m_isActive = false;

	return true;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBlowTire::Reset()
{
	Activate(false);
}


//------------------------------------------------------------------------
void CVehicleDamageBehaviorBlowTire::Activate(bool activate)
{
  if (activate == m_isActive)
    return;

  if (activate && m_pVehicle->IsDestroyed())
    return;

  IVehicleComponent* pComponent = m_pVehicle->GetComponent(m_component.c_str());
  if (!pComponent)
    return;

  IVehiclePart* pPart = pComponent->GetPart(0);
  if (!pPart)
    return;

  // if IVehicleWheel available, execute full damage behavior. if null, only apply effects
  IVehicleWheel* pWheel = pPart->GetIWheel();
  
  if (activate)
  {
    IEntity* pEntity = m_pVehicle->GetEntity();
    IPhysicalEntity* pPhysics = pEntity->GetPhysics();
    const Matrix34& wheelTM = pPart->GetLocalTM(false);
    const SVehicleStatus& status = m_pVehicle->GetStatus();

    if (pWheel)
    { 
      const pe_cargeomparams* pParams = pWheel->GetCarGeomParams();  
            
      // handle destroyed wheel
      pe_params_wheel wheelParams;
      wheelParams.iWheel = pWheel->GetWheelIndex();            
      wheelParams.minFriction = wheelParams.maxFriction = 0.5f * pParams->maxFriction;      
      pPhysics->SetParams(&wheelParams); 
      
      if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
      { 
        SVehicleMovementEventParams params;
        params.pComponent = pComponent;
        params.iValue = pWheel->GetWheelIndex();
        pMovement->OnEvent(IVehicleMovement::eVME_TireBlown, params);
      }

      if (status.speed > 0.1f)
      {
        // add angular impulse
        pe_action_impulse angImp;
        float amount = m_pVehicle->GetMass() * status.speed * cry_random(0.25f, 0.45f) * -sgn(wheelTM.GetTranslation().x);
        angImp.angImpulse = pEntity->GetWorldTM().TransformVector(Vec3(0,0,amount));    
        pPhysics->Action(&angImp);
      }
 
			m_pVehicle->SetTimer(-1, AI_IMMOBILIZED_TIME*1000, this);  
    }

    if (!gEnv->pSystem->IsSerializingFile())
    {
      // add linear impulse       
      pe_action_impulse imp;
      imp.point = pPart->GetWorldTM().GetTranslation();

      float amount = m_pVehicle->GetMass() * cry_random(0.1f, 0.15f);

      if (pWheel)
      {
        amount *= max(0.5f, min(10.f, status.speed));

        if (status.speed < 0.1f)
          amount = -0.5f*amount;
      }
      else    
        amount *= 0.5f;

      imp.impulse = pEntity->GetWorldTM().TransformVector(Vec3(0,0,amount));
      pPhysics->Action(&imp);     

			// remove affected decals
			{
				Vec3 pos = pPart->GetWorldTM().GetTranslation();
				AABB aabb = pPart->GetLocalBounds();
				float radius = aabb.GetRadius();
				Vec3 vRadius(radius,radius,radius);
        AABB areaBox(pos-vRadius, pos+vRadius);
        
        IRenderNode * pRenderNode = NULL;				
        if (IEntityRender *pIEntityRender = pEntity->GetRenderInterface())
					pRenderNode = pIEntityRender->GetRenderNode();

        gEnv->p3DEngine->DeleteDecalsInRange(&areaBox, pRenderNode);
			}
    }    
  }
  else
  { 
    if (pWheel)
    {
      // restore wheel properties        
      IPhysicalEntity* pPhysics = m_pVehicle->GetEntity()->GetPhysics();    
      pe_params_wheel wheelParams;

      for (int i=0; i<m_pVehicle->GetWheelCount(); ++i)
      { 
        const pe_cargeomparams* pParams = m_pVehicle->GetWheelPart(i)->GetIWheel()->GetCarGeomParams();

        wheelParams.iWheel = i;
        wheelParams.bBlocked = 0;
        wheelParams.suspLenMax = pParams->lenMax;
        wheelParams.bDriving = pParams->bDriving;      
        wheelParams.minFriction = pParams->minFriction;
        wheelParams.maxFriction = pParams->maxFriction;
        pPhysics->SetParams(&wheelParams);
      }

			if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
			{ 
				SVehicleMovementEventParams params;
				params.pComponent = pComponent;
				params.iValue = pWheel->GetWheelIndex();
				// reset the particle status
				pMovement->OnEvent(IVehicleMovement::eVME_TireRestored, params);
			}
    }  
  }

  m_isActive = activate;      
}


//------------------------------------------------------------------------
void CVehicleDamageBehaviorBlowTire::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
  if (event == eVDBE_Hit && behaviorParams.componentDamageRatio >= 1.0f && behaviorParams.pVehicleComponent)
  {
		m_component = behaviorParams.pVehicleComponent->GetComponentName();
    Activate(true);    
  }
  else if (event == eVDBE_Repair && behaviorParams.componentDamageRatio < 1.f)
  {
    Activate(false);
  }
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBlowTire::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	assert(event == eVE_Timer);

  // notify AI passengers
  IScriptTable* pTable = m_pVehicle->GetEntity()->GetScriptTable();
  HSCRIPTFUNCTION scriptFunction(NULL);
  
  if (pTable && pTable->GetValue("OnVehicleImmobilized", scriptFunction) && scriptFunction)
  {
    Script::Call(gEnv->pScriptSystem, scriptFunction, pTable);
		gEnv->pScriptSystem->ReleaseFunc(scriptFunction);
  }
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBlowTire::Update(const float deltaTime)
{	
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBlowTire::Serialize(TSerialize ser, EEntityAspects aspects)
{
  if (ser.GetSerializationTarget() != eST_Network)
  {
		bool bActive = m_isActive;

    ser.Value("isActive", bActive);
    ser.Value("component", m_component);
   		
		if (ser.IsReading())	
			Activate(bActive);    		
  }
}

void CVehicleDamageBehaviorBlowTire::GetMemoryUsage(ICrySizer * s) const
{
	s->Add(*this);
	s->Add(m_component);
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorBlowTire);
