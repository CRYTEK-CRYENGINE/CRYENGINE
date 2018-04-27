// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Vehicle part class that spawns an entity and attaches it to the vehicle.

   -------------------------------------------------------------------------
   History:
   - 01:09:2010: Created by Paul Slinger

*************************************************************************/

#include "StdAfx.h"

#include <IViewSystem.h>
#include <IItemSystem.h>
#include <IVehicleSystem.h>
#include <CryPhysics/IPhysics.h>
#include <CryAnimation/ICryAnimation.h>
#include <IActorSystem.h>
#include <CryNetwork/ISerialize.h>
#include <CryAISystem/IAgent.h>

#include "CryAction.h"
#include "Vehicle.h"
#include "VehiclePartEntity.h"
#include "VehicleUtils.h"

//------------------------------------------------------------------------
CVehiclePartEntity::CVehiclePartEntity()
	: m_entityId(0)
	, m_pHelper(nullptr)
	, m_pLink(nullptr)
	, m_hidden(false)
	, m_destroyed(false)
	, m_entityAttached(false)
	, m_constraintId(0)
	, m_CollideWithParent(true)
	, m_createBuddyConstraint(false)
	, m_entityNetId(0)
	, m_shouldDetachInsteadOfDestroy(false)
{
}

//------------------------------------------------------------------------
CVehiclePartEntity::~CVehiclePartEntity()
{
	SafeRemoveBuddyConstraint();
	DestroyEntity();

	m_pVehicle->UnregisterVehicleEventListener(this);
}

//------------------------------------------------------------------------
bool CVehiclePartEntity::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo, eVPT_Entity))
	{
		return false;
	}

	if (CVehicleParams params = table.findChild("Entity"))
	{
		//prefer to use index, if not available try id
		bool found = params.getAttr("index", m_index);
		if (!found)
		{
			params.getAttr("id", m_index);
		}

		m_entityName = params.getAttr("name");
		m_entityArchetype = params.getAttr("archetype");
		m_helperName = params.getAttr("helper");

		//getAttr is confused by bitfield bool
		bool temp = true;
		params.getAttr("collideWithParent", temp);
		m_CollideWithParent = temp;

		temp = false;
		params.getAttr("detachInsteadOfDestroy", temp);
		m_shouldDetachInsteadOfDestroy = temp;
	}

	m_pVehicle->RegisterVehicleEventListener(this, "VehiclePartEntity");

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartEntity::PostInit()
{
	if (!m_helperName.empty())
	{
		m_pHelper = m_pVehicle->GetHelper(m_helperName.c_str());
	}

	Reset();
}

//------------------------------------------------------------------------
void CVehiclePartEntity::Reset()
{
	if (m_entityId && !gEnv->pEntitySystem->GetEntity(m_entityId))
	{
		m_entityId = 0;
	}

	if (!m_entityId)
	{
		SEntitySpawnParams entitySpawnParams;

		entitySpawnParams.pArchetype = gEnv->pEntitySystem->LoadEntityArchetype(m_entityArchetype.c_str());

		if (entitySpawnParams.pArchetype)
		{
			entitySpawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(m_entityName.c_str());

			if (entitySpawnParams.pClass)
			{
				entitySpawnParams.vPosition = Vec3(0.0f, 0.0f, 0.0f);
				entitySpawnParams.sName = m_entityName.c_str();
				entitySpawnParams.nFlags |= (m_pVehicle->GetEntity()->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY)) | ENTITY_FLAG_NEVER_NETWORK_STATIC;

				//if the entity will be created by the server, don't create it on the clients
				//we will receive an entity spawn from the server and join it on later
				if (gEnv->bServer || (entitySpawnParams.nFlags & ENTITY_FLAG_CLIENT_ONLY))
				{
					if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(entitySpawnParams))
					{
						m_entityId = pEntity->GetId();

						//Create an entity link so the entity can find the Vehicle if it needs to
						m_pLink = pEntity->AddEntityLink("VehiclePartLink", m_pVehicle->GetEntityId(),m_pVehicle->GetEntity()->GetGuid());

						m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

						pEntity->Hide(m_hidden);

						if (m_CollideWithParent == false)
						{
							m_createBuddyConstraint = true;
						}

						INetContext* pNetContext = gEnv->pGameFramework->GetNetContext();
						if (pNetContext)
						{
							SNetObjectID netId = pNetContext->GetNetID(m_entityId, false);
							m_entityNetId = netId.id;
						}

						UpdateEntity();
					}
				}
			}
		}
	}
	else
	{
		//this is required to handle resetting world in editor, should not be hit while running game
		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId))
		{
			if (!m_entityAttached)
			{
				//Create an entity link so the entity can find the Vehicle if it needs to
				m_pLink = pEntity->AddEntityLink("VehiclePartLink", m_pVehicle->GetEntityId(),m_pVehicle->GetEntity()->GetGuid());

				m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

				pEntity->Hide(m_hidden);

				if (m_CollideWithParent == false)
				{
					m_createBuddyConstraint = true;
				}

				INetContext* pNetContext = gEnv->pGameFramework->GetNetContext();
				if (pNetContext)
				{
					SNetObjectID netId = pNetContext->GetNetID(m_entityId, false);
					m_entityNetId = netId.id;
				}

				UpdateEntity();
			}
			else
			{
				m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

				if (m_constraintId)
				{
					if (m_CollideWithParent == false)
					{
						m_createBuddyConstraint = true;
					}
				}
			}
		}
	}

	m_entityAttached = !!m_entityId;
}

//------------------------------------------------------------------------
void CVehiclePartEntity::Update(const float frameTime)
{
	CVehiclePartBase::Update(frameTime);

	if (m_createBuddyConstraint && GetPartEntity()) //Wait for part entity to be valid
	{
		//stop collision between vehicle and part
		if (IPhysicalEntity* pVehiclePE = m_pVehicle->GetEntity()->GetPhysics())
		{
			IPhysicalEntity* pPartPE = GetPartEntity()->GetPhysics();
			pe_action_add_constraint ic;
			ic.flags = constraint_inactive | constraint_ignore_buddy;
			ic.pBuddy = pPartPE;
			ic.pt[0].Set(0, 0, 0);
			m_constraintId = pVehiclePE->Action(&ic);
		}

		m_createBuddyConstraint = false;
	}

	if (m_entityAttached)
	{
		UpdateEntity();
	}
}

//------------------------------------------------------------------------
void CVehiclePartEntity::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	switch (event)
	{
	case eVE_Destroyed:
		{
			if (m_shouldDetachInsteadOfDestroy)
			{
				TryDetachPart(params);
			}
			else
			{
				DestroyEntity();
			}

			break;
		}

	case eVE_TryDetachPartEntity:
		{
			if (params.iParam == m_index)
			{
				TryDetachPart(params);
			}

			break;
		}

	case eVE_OnDetachPartEntity:
		{
			if (m_entityAttached && params.entityId == m_entityId)
			{
				m_entityAttached = false;

				SafeRemoveBuddyConstraint();
			}

			break;
		}

	case eVE_TryAttachPartEntityNetID:
		{
			if (params.iParam == m_entityNetId && params.entityId != 0)
			{
				//Can only attach an entity if we currently don't have one
				if (m_entityId == 0)
				{
					if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(params.entityId))
					{
						m_entityId = params.entityId;
						//Create an entity link so the entity can find the Vehicle if it needs to
						m_pLink = pEntity->AddEntityLink("VehiclePartLink", m_pVehicle->GetEntityId(),m_pVehicle->GetEntity()->GetGuid());

						m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

						pEntity->Hide(m_hidden);

						if (m_CollideWithParent == false)
						{
							m_createBuddyConstraint = true;
						}

						m_entityAttached = true;

						CryLog("VehiclePartEntity: Successfully attached part %d to entity %u", m_index, m_entityId);
					}
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CVehiclePartEntity::OnVehicleEvent trying to attach an entity but part already has one");
				}
			}

			break;
		}

	case eVE_Hide:
		{
			m_hidden = !!params.iParam;

			if (m_entityAttached)
			{
				if (IEntity* pEntity = GetPartEntity())
				{
					pEntity->Hide(m_hidden);
				}
			}

			break;
		}
	}
}

//------------------------------------------------------------------------
void CVehiclePartEntity::Serialize(TSerialize serializer, EEntityAspects aspects)
{
	CVehiclePartBase::Serialize(serializer, aspects);

	bool saveGame = serializer.GetSerializationTarget() != eST_Network;

	if (saveGame)
	{
		uint32 flags = 0;

		if (serializer.IsWriting())
		{
			if (m_hidden)
			{
				flags |= Flags::Hidden;
			}

			if (m_entityAttached)
			{
				flags |= Flags::EntityAttached;
			}

			if (m_destroyed)
			{
				flags |= Flags::Destroyed;
			}
		}

		serializer.Value("entityId", m_entityId);
		serializer.Value("flags", flags);

		if (serializer.IsReading())
		{
			m_hidden = !!(flags & Flags::Hidden);
			m_entityAttached = !!(flags & Flags::EntityAttached);
			m_destroyed = !!(flags & Flags::Destroyed);
		}
	}
	else
	{
		serializer.Value("netId", m_entityNetId, 'ui16');
	}

}

//------------------------------------------------------------------------
void CVehiclePartEntity::PostSerialize()
{
	CVehiclePartBase::PostSerialize();

	if (!m_destroyed)
	{
		Reset();
	}
}

//------------------------------------------------------------------------
void CVehiclePartEntity::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);

	CVehiclePartBase::GetMemoryUsageInternal(pSizer);
}

//------------------------------------------------------------------------
IEntity* CVehiclePartEntity::GetPartEntity() const
{
	return m_entityId ? gEnv->pEntitySystem->GetEntity(m_entityId) : NULL;
}

//------------------------------------------------------------------------
void CVehiclePartEntity::UpdateEntity()
{
	if (IEntity* pEntity = GetPartEntity())
	{
		Matrix34 worldTM;

		if (m_pHelper)
		{
			m_pHelper->GetWorldTM(worldTM);
		}
		else if (IVehiclePart* pParent = GetParent())
		{
			worldTM = pParent->GetWorldTM();
		}
		else
		{
			worldTM = GetEntity()->GetWorldTM();
		}

		pEntity->SetWorldTM(worldTM);
	}
}

//------------------------------------------------------------------------
void CVehiclePartEntity::DestroyEntity()
{
	m_destroyed = true;

	if (m_entityId)
	{
		gEnv->pEntitySystem->RemoveEntity(m_entityId);

		m_entityId = 0;
	}

	m_entityAttached = false;
	SafeRemoveBuddyConstraint();

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
}

void CVehiclePartEntity::SafeRemoveBuddyConstraint()
{
	if (IEntity* pPartEntity = GetPartEntity())
	{
		if (m_constraintId)
		{
			if (IPhysicalEntity* pPartPE = pPartEntity->GetPhysics())
			{
				pe_action_update_constraint up;
				up.bRemove = true;
				up.idConstraint = m_constraintId;
				m_constraintId = 0;
				pPartPE->Action(&up);
			}
		}

		if (m_pLink)
		{
			pPartEntity->RemoveEntityLink(m_pLink);
			m_pLink = NULL;
		}
	}
}

void CVehiclePartEntity::TryDetachPart(const SVehicleEventParams& params)
{
	SVehicleEventParams vehicleEventParams = params;

	vehicleEventParams.entityId = m_entityId;
	vehicleEventParams.iParam = m_pVehicle->GetEntityId();

	m_pVehicle->BroadcastVehicleEvent(eVE_OnDetachPartEntity, vehicleEventParams);

	m_entityAttached = false;

	SafeRemoveBuddyConstraint();
}

//------------------------------------------------------------------------
DEFINE_VEHICLEOBJECT(CVehiclePartEntity)
