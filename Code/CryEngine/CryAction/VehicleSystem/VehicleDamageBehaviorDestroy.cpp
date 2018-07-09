// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleDamageBehaviorDestroy.h"

#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleComponent.h"
#include "VehicleSeat.h"

CVehicleDamageBehaviorDestroy::CVehicleDamageBehaviorDestroy()
	: m_pVehicle(nullptr)
{}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorDestroy::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = (CVehicle*) pVehicle;

	if (table.haveAttr("effect"))
		m_effectName = table.getAttr("effect");

	return true;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorDestroy::Reset()
{
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorDestroy::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
	if (event == eVDBE_Repair)
		return;

	if (behaviorParams.componentDamageRatio >= 1.0f)
		SetDestroyed(true, behaviorParams.shooterId);
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorDestroy::SetDestroyed(bool isDestroyed, EntityId shooterId)
{
	IEntity* pVehicleEntity = m_pVehicle->GetEntity();

	bool destroyed = m_pVehicle->IsDestroyed();
	m_pVehicle->SetDestroyedStatus(isDestroyed);

	// destroy the vehicle
	if (!destroyed && isDestroyed)
	{
		CryLog("%s being driven by %s has been destroyed!", m_pVehicle->GetEntity()->GetEntityTextDescription().c_str(), m_pVehicle->GetDriver() ? m_pVehicle->GetDriver()->GetEntity()->GetEntityTextDescription().c_str() : "nobody");
		INDENT_LOG_DURING_SCOPE();

		// call OnVehicleDestroyed function on lua side
		HSCRIPTFUNCTION scriptFunction(NULL);
		if (pVehicleEntity->GetScriptTable()->GetValue("OnVehicleDestroyed", scriptFunction) && scriptFunction)
		{
			IScriptSystem* pIScriptSystem = gEnv->pScriptSystem;
			Script::Call(pIScriptSystem, scriptFunction, pVehicleEntity->GetScriptTable());
			pIScriptSystem->ReleaseFunc(scriptFunction);
		}

		// notify the other components
		TVehicleComponentVector& components = m_pVehicle->GetComponents();
		for (TVehicleComponentVector::iterator ite = components.begin(); ite != components.end(); ++ite)
		{
			CVehicleComponent* pComponent = *ite;
			pComponent->OnVehicleDestroyed();
		}

		// notify vehicle parts

		TVehiclePartVector parts = m_pVehicle->GetParts();
		IVehiclePart::SVehiclePartEvent partEvent;
		partEvent.type = IVehiclePart::eVPE_Damaged;
		partEvent.fparam = 1.0f;

		for (TVehiclePartVector::iterator ite = parts.begin(); ite != parts.end(); ++ite)
		{
			IVehiclePart* pPart = ite->second;
			pPart->OnEvent(partEvent);
		}

		// notify active movement type
		if (IVehicleMovement* pVehicleMovement = m_pVehicle->GetMovement())
		{
			SVehicleMovementEventParams movementParams;
			movementParams.fValue = 1.0f;
			movementParams.bValue = true;
			pVehicleMovement->OnEvent(IVehicleMovement::eVME_Damage, movementParams);

			movementParams.fValue = 1.0f;
			movementParams.bValue = false;
			pVehicleMovement->OnEvent(IVehicleMovement::eVME_VehicleDestroyed, movementParams);
		}

		// broadcast event through all vehicle notifiers (eg. seats..)
		SVehicleEventParams eventParams;
		eventParams.entityId = shooterId;
		m_pVehicle->BroadcastVehicleEvent(eVE_Destroyed, eventParams);

		// send event to Entity and AI system

		const char pEventName[] = "Destroy";
		const bool val = true;

		SEntityEvent entityEvent;
		entityEvent.event = ENTITY_EVENT_SCRIPT_EVENT;
		entityEvent.nParam[0] = (INT_PTR) pEventName;
		entityEvent.nParam[1] = IEntityClass::EVT_BOOL;
		entityEvent.nParam[2] = (INT_PTR)&val;
		m_pVehicle->GetEntity()->SendEvent(entityEvent);

		IAISystem* pAISystem = gEnv->pAISystem;
		if (pAISystem && pAISystem->IsEnabled() && pAISystem->GetSmartObjectManager())
		{
			gEnv->pAISystem->GetSmartObjectManager()->SetSmartObjectState(m_pVehicle->GetEntity(), "Dead");
		}

		m_pVehicle->OnDestroyed();
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorDestroy::Serialize(TSerialize ser, EEntityAspects aspects)
{
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorDestroy);
