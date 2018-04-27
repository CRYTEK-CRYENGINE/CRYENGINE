// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a vehicle action for Entity Attachment

-------------------------------------------------------------------------
History:
- 07:12:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "VehicleActionEntityAttachment.h"
#include "Game.h"
#include "GameRules.h"

//------------------------------------------------------------------------
CVehicleActionEntityAttachment::CVehicleActionEntityAttachment()
	: m_pVehicle(nullptr)
	, m_pHelper(nullptr)
	, m_entityId(0)
	, m_isAttached(false)
	, m_timer(0.0f)
{
}

//------------------------------------------------------------------------
CVehicleActionEntityAttachment::~CVehicleActionEntityAttachment()
{
	if (m_entityId)
	{
		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		assert(pEntitySystem);

		pEntitySystem->RemoveEntity(m_entityId);
	}
}

//------------------------------------------------------------------------
bool CVehicleActionEntityAttachment::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;

	CVehicleParams entityAttachmentTable = table.findChild("EntityAttachment");
	if (!entityAttachmentTable)
		return false;

	if (entityAttachmentTable.haveAttr("helper"))
		m_pHelper = m_pVehicle->GetHelper(entityAttachmentTable.getAttr("helper"));

	if (entityAttachmentTable.haveAttr("class"))
	{
		IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
		assert(pClassRegistry);

		m_entityClassName = entityAttachmentTable.getAttr("class");

		if (pClassRegistry->FindClass(m_entityClassName.c_str()))
		{
			SpawnEntity();
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
void CVehicleActionEntityAttachment::Reset()
{
	if (m_entityId)
	{
		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		assert(pEntitySystem);

		pEntitySystem->RemoveEntity(m_entityId);
	}

	SpawnEntity();

	if (m_timer > 0.0f)
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);

	m_timer = 0.0f;
}

//------------------------------------------------------------------------
int CVehicleActionEntityAttachment::OnEvent(int eventType, SVehicleEventParams& eventParams)
{
	if (eventType == eVE_Hit || eventType == eVE_Destroyed)
	{
		DetachEntity();
	}

	return 0;
}

float g_parachuteForce = 1.0f;
float g_parachuteTimeMax = 3.0f;

//------------------------------------------------------------------------
void CVehicleActionEntityAttachment::Update(const float deltaTime)
{
	if (m_isAttached)
		return;

	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	assert(pEntitySystem);

	IEntity* pEntity = pEntitySystem->GetEntity(m_entityId);
	if (!pEntity)
		return;

	IPhysicalEntity* pPhysEntity = pEntity->GetPhysics();
	if (!pPhysEntity)
		return;

	pe_simulation_params paramsSim;
	float gravity;
	
	if (pPhysEntity->GetParams(&paramsSim))
		gravity = abs(paramsSim.gravity.z);
	else
		gravity = 9.82f;

	pe_status_dynamics dyn;
	if (pPhysEntity->GetStatus(&dyn))
	{
		pe_action_impulse impulse;
		impulse.impulse  = Matrix33(pEntity->GetWorldTM()) * Vec3(0.0f, 0.0f, 1.0f) * g_parachuteForce * gravity;
		impulse.impulse = impulse.impulse - dyn.v;
		impulse.impulse *= dyn.mass * deltaTime;
		impulse.iSource = 3;

		pPhysEntity->Action(&impulse);
	}

	m_timer -= deltaTime;
	if (m_timer <= 0.0f || dyn.v.z >= 0.0f)
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
}

//------------------------------------------------------------------------
void CVehicleActionEntityAttachment::SpawnEntity()
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	assert(pEntitySystem);

	IEntityClassRegistry* pClassRegistry = pEntitySystem->GetClassRegistry();
	assert(pClassRegistry);

	IEntityClass* pEntityClass = pClassRegistry->FindClass(m_entityClassName.c_str());
	if (!pEntityClass)
		return;

	char pEntityName[256];
	cry_sprintf(pEntityName, "%s_%s", m_pVehicle->GetEntity()->GetName(), m_entityClassName.c_str());

	SEntitySpawnParams params;
	params.sName = pEntityName;
	params.nFlags = ENTITY_FLAG_CLIENT_ONLY;
	params.pClass = pEntityClass;

	IEntity* pEntity = pEntitySystem->SpawnEntity(params, true);
	if (!pEntity)
	{
		m_entityId = 0;
		return;
	}

	m_entityId = pEntity->GetId();

	m_pVehicle->GetEntity()->AttachChild(pEntity);
	Matrix34 tm;
	m_pHelper->GetVehicleTM(tm);
	pEntity->SetLocalTM(tm);

	m_isAttached = true;
}

//------------------------------------------------------------------------
bool CVehicleActionEntityAttachment::DetachEntity()
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	assert(pEntitySystem);

	if (IEntity* pEntity = pEntitySystem->GetEntity(m_entityId))
	{
		IVehicleSystem* pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();
		assert(pVehicleSystem);

		if (IVehicle* pVehicle = pVehicleSystem->GetVehicle(m_entityId))
		{
			int hitType = g_pGame->GetGameRules()->GetHitTypeId("disableCollisions");
			HitInfo hitInfo;
			hitInfo.targetId = m_pVehicle->GetEntityId();
			hitInfo.shooterId= hitInfo.targetId;
			hitInfo.damage   = 10.f;
			hitInfo.type     = hitType;
			pVehicle->OnHit(hitInfo);
		}

		pEntity->DetachThis();
		m_isAttached = false;

		m_timer = g_parachuteTimeMax;
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
		return true;
	}

	return false;
}

DEFINE_VEHICLEOBJECT(CVehicleActionEntityAttachment);
