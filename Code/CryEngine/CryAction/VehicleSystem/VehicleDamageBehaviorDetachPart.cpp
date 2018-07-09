// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleDamageBehaviorDetachPart.h"

#include <CryAnimation/ICryAnimation.h>
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimatedJoint.h"

//------------------------------------------------------------------------
CVehicleDamageBehaviorDetachPart::CVehicleDamageBehaviorDetachPart()
	: m_pVehicle(nullptr)
	, m_detachedEntityId(0)
	, m_pEffect(nullptr)
	, m_pickableDebris(false)
	, m_notifyMovement(false)
{}

//------------------------------------------------------------------------
CVehicleDamageBehaviorDetachPart::~CVehicleDamageBehaviorDetachPart()
{
	if (m_detachedEntityId != 0)
	{
		gEnv->pEntitySystem->RemoveEntity(m_detachedEntityId);
		m_detachedEntityId = 0;
	}
}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorDetachPart::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = static_cast<CVehicle*>(pVehicle);

	m_detachedEntityId = 0;

	//<DetachPartParams geometry="door2" direction="1.0,0.0,0.0" />

	if (CVehicleParams detachPartParams = table.findChild("DetachPart"))
	{
		m_partName = detachPartParams.getAttr("part");
		detachPartParams.getAttr("notifyMovement", m_notifyMovement);

		// Get optional custom particle effect
		if (detachPartParams.haveAttr("effect"))
			m_pEffect = gEnv->pParticleManager->FindEffect(detachPartParams.getAttr("effect"), "CVehicleDamageBehaviorDetachPart()");

		detachPartParams.getAttr("pickable", m_pickableDebris);

		return true;
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorDetachPart::Init(IVehicle* pVehicle, const string& partName, const string& effectName)
{
	m_pVehicle = static_cast<CVehicle*>(pVehicle);

	m_detachedEntityId = 0;

	m_partName = partName;

	// Get optional custom particle effect
	if (!effectName.empty())
		m_pEffect = gEnv->pParticleManager->FindEffect(effectName.c_str(), "CVehicleDamageBehaviorDetachPart()");

	return true;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorDetachPart::Reset()
{
	if (m_detachedEntityId)
	{
		for (TDetachedStatObjs::iterator ite = m_detachedStatObjs.begin(), end = m_detachedStatObjs.end(); ite != end; ++ite)
		{
			CVehiclePartBase* pPartBase = ite->first;

			if (IStatObj* pStatObj = ite->second)
			{
				if (pPartBase)
				{
					pPartBase->SetStatObj(pStatObj);
				}

				pStatObj->Release();
			}
		}

		m_detachedStatObjs.clear();

		if (GetISystem()->IsSerializingFile() != 1)
		{
			IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
			pEntitySystem->RemoveEntity(m_detachedEntityId, true);
		}

		m_detachedEntityId = 0;
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorDetachPart::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		EntityId detachedId = m_detachedEntityId;
		ser.Value("m_detachedEntityId", detachedId);

		if (ser.IsReading() && m_detachedEntityId != detachedId)
		{
			if (detachedId)
			{
				m_detachedEntityId = detachedId;

				if (IEntity* pDetachedEntity = gEnv->pEntitySystem->GetEntity(m_detachedEntityId))
				{
					if (CVehiclePartBase* pPart = (CVehiclePartBase*)m_pVehicle->GetPart(m_partName.c_str()))
					{
						MovePartToTheNewEntity(pDetachedEntity, pPart);
					}
				}
			}
			else
			{
				Reset();
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorDetachPart::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
	if (event == eVDBE_Repair)
		return;

	if (!m_detachedEntityId && behaviorParams.componentDamageRatio >= 1.0f)
	{
		CVehiclePartBase* pPart = (CVehiclePartBase*)m_pVehicle->GetPart(m_partName.c_str());
		if (!pPart || !pPart->GetStatObj())
			return;

		if (max(1.f - behaviorParams.randomness, pPart->GetDetachProbability()) < cry_random(0.0f, 1.0f))
			return;

		IEntity* pDetachedEntity = SpawnDetachedEntity();
		if (!pDetachedEntity)
			return;

		m_detachedEntityId = pDetachedEntity->GetId();

		const Matrix34& partWorldTM = pPart->GetWorldTM();
		pDetachedEntity->SetWorldTM(partWorldTM);

		MovePartToTheNewEntity(pDetachedEntity, pPart);

		SEntityPhysicalizeParams physicsParams;
		physicsParams.mass = pPart->GetMass();
		physicsParams.type = PE_RIGID;
		physicsParams.nSlot = 0;
		pDetachedEntity->Physicalize(physicsParams);

		IPhysicalEntity* pPhysics = pDetachedEntity->GetPhysics();

		if (pPhysics)
		{
			pe_params_part params;
			params.flagsOR = geom_collides | geom_floats;
			params.flagsColliderAND = ~geom_colltype3;
			params.flagsColliderOR = geom_colltype0;
			pPhysics->SetParams(&params);

			pe_action_add_constraint ac;
			ac.flags = constraint_inactive | constraint_ignore_buddy;
			ac.pBuddy = m_pVehicle->GetEntity()->GetPhysics();
			ac.pt[0].Set(0, 0, 0);
			pPhysics->Action(&ac);

			// after 1s, remove the constraint again
			m_pVehicle->SetTimer(-1, 1000, this);

			// set the impulse
			const Vec3& velocity = m_pVehicle->GetStatus().vel;
			Vec3 baseForce = m_pVehicle->GetEntity()->GetWorldTM().TransformVector(pPart->GetDetachBaseForce());
			baseForce *= cry_random(6.0f, 10.0f);

			pe_action_impulse actionImpulse;
			actionImpulse.impulse = physicsParams.mass * (velocity + baseForce);
			actionImpulse.angImpulse = physicsParams.mass * Vec3(cry_random(-1.0f, 1.0f), cry_random(-1.0f, 1.0f), cry_random(-1.0f, 1.0f));
			actionImpulse.iApplyTime = 1;

			pPhysics->Action(&actionImpulse);
		}

		// copy vehicle's material to new entity (fixes detaching parts from vehicles with different paints),
		// or specify the destroyed material if it exists

		IStatObj* pExternalStatObj = pPart->GetExternalGeometry(false); // Get undamaged external geometry (if any)
		IMaterial* pMaterial = pExternalStatObj ? pExternalStatObj->GetMaterial() : m_pVehicle->GetEntity()->GetMaterial();

		if (event == eVDBE_VehicleDestroyed || event == eVDBE_Hit)
		{
			if (pExternalStatObj)
			{
				if (IStatObj* pStatObj = pPart->GetExternalGeometry(true))     // Try to get the destroyed, external geometry material
					pMaterial = pStatObj->GetMaterial();
			}
			else if (m_pVehicle->GetDestroyedMaterial())  // If there is no external geometry, try the vehicle's destroyed material
			{
				pMaterial = m_pVehicle->GetDestroyedMaterial();
			}
		}

		pDetachedEntity->SetMaterial(pMaterial);

		AttachParticleEffect(pDetachedEntity, m_pEffect);

		if (m_notifyMovement)
		{
			SVehicleMovementEventParams params;
			params.iValue = pPart->GetIndex();
			m_pVehicle->GetMovement()->OnEvent(IVehicleMovement::eVME_PartDetached, params);
		}
	}
}

//------------------------------------------------------------------------
IEntity* CVehicleDamageBehaviorDetachPart::SpawnDetachedEntity()
{
	IEntity* pVehicleEntity = m_pVehicle->GetEntity();
	CRY_ASSERT(pVehicleEntity);

	// spawn the detached entity
	char pPartName[128];
	cry_sprintf(pPartName, "%s_DetachedPart_%s", pVehicleEntity->GetName(), m_partName.c_str());

	SEntitySpawnParams spawnParams;
	spawnParams.sName = pPartName;
	spawnParams.nFlags = ENTITY_FLAG_CLIENT_ONLY;
	if (!m_pickableDebris)
		spawnParams.nFlags |= ENTITY_FLAG_NO_PROXIMITY;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("VehiclePartDetached");
	if (!spawnParams.pClass)
	{
		CRY_ASSERT(0);
		return NULL;
	}

	return gEnv->pEntitySystem->SpawnEntity(spawnParams, true);
}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorDetachPart::MovePartToTheNewEntity(IEntity* pTargetEntity, CVehiclePartBase* pPartBase)
{
	if (!pPartBase)
		return false;

	IEntity* pVehicleEntity = m_pVehicle->GetEntity();
	CRY_ASSERT(pVehicleEntity);

	assert(m_detachedEntityId == pTargetEntity->GetId());

	IStatObj* pStatObj = pPartBase->GetStatObj();
	if (pStatObj)
	{
		pStatObj->AddRef();
	}
	m_detachedStatObjs.push_back(TPartObjectPair(pPartBase, pStatObj));

	// place the geometry on the new entity
	int slot = pTargetEntity->SetStatObj(pStatObj, -1, true, pPartBase->GetMass());

	const Matrix34& partTM = pPartBase->GetWorldTM();
	Matrix34 localTM = pTargetEntity->GetWorldTM().GetInverted() * partTM;
	pTargetEntity->SetSlotLocalTM(slot, localTM);

	pPartBase->SetStatObj(NULL);

	TVehiclePartVector& parts = m_pVehicle->GetParts();

	const CVehiclePartBase::TVehicleChildParts& children = pPartBase->GetChildParts();
	for (CVehiclePartBase::TVehicleChildParts::const_iterator ite = children.begin(), end = children.end(); ite != end; ++ite)
	{
		MovePartToTheNewEntity(pTargetEntity, (*ite));
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorDetachPart::AttachParticleEffect(IEntity* pDetachedEntity, IParticleEffect* pEffect)
{
	if (pEffect)
	{
		int slot = pDetachedEntity->LoadParticleEmitter(-1, pEffect, NULL, false, true);

		if (IParticleEmitter* pParticleEmitter = pDetachedEntity->GetParticleEmitter(slot))
		{
			SpawnParams spawnParams;
			spawnParams.fSizeScale = cry_random(0.5f, 1.0f);

			pParticleEmitter->SetSpawnParams(spawnParams);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorDetachPart::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	assert(event == eVE_Timer);
	assert(m_detachedEntityId);

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_detachedEntityId);
	if (pEntity)
	{
		pe_action_update_constraint ac;
		ac.bRemove = true;
		pEntity->GetPhysics()->Action(&ac);
	}
}

void CVehicleDamageBehaviorDetachPart::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
	s->AddObject(m_partName);
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorDetachPart);
