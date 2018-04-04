// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   TriggerProxy.h
//  Version:     v1.00
//  Created:     5/12/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TriggerProxy.h"
#include <CryNetwork/ISerialize.h>
#include "ProximityTriggerSystem.h"

CRYREGISTER_CLASS(CEntityComponentTriggerBounds)

//////////////////////////////////////////////////////////////////////////
CEntityComponentTriggerBounds::CEntityComponentTriggerBounds()
	: m_forwardingEntity(0)
	, m_pProximityTrigger(NULL)
	, m_aabb(AABB::RESET)
{
	m_componentFlags.Add(EEntityComponentFlags::NoSave);
}

//////////////////////////////////////////////////////////////////////////
CEntityComponentTriggerBounds::~CEntityComponentTriggerBounds()
{
	GetTriggerSystem()->RemoveTrigger(m_pProximityTrigger);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentTriggerBounds::Initialize()
{
	m_pProximityTrigger = GetTriggerSystem()->CreateTrigger();
	m_pProximityTrigger->id = m_pEntity->GetId();

	Reset();
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentTriggerBounds::Reset()
{
	auto* pCEntity = static_cast<CEntity*>(m_pEntity);

	pCEntity->m_bTrigger = true;
	m_forwardingEntity = 0;

	// Release existing proximity entity if present, triggers should not trigger themself.
	if (pCEntity->m_pProximityEntity)
	{
		GetTriggerSystem()->RemoveEntity(pCEntity->m_pProximityEntity);
		pCEntity->m_pProximityEntity = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentTriggerBounds::ProcessEvent(SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_XFORM:
		OnMove();
		break;
	case ENTITY_EVENT_ENTERAREA:
	case ENTITY_EVENT_LEAVEAREA:
		if (m_forwardingEntity)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_forwardingEntity);
			if (pEntity && (pEntity != this->GetEntity()))
			{
				pEntity->SendEvent(event);
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityComponentTriggerBounds::NeedGameSerialize()
{
	return true;
};

//////////////////////////////////////////////////////////////////////////
void CEntityComponentTriggerBounds::GameSerialize(TSerialize ser)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		if (ser.BeginOptionalGroup("TriggerProxy", true))
		{
			ser.Value("BoxMin", m_aabb.min);
			ser.Value("BoxMax", m_aabb.max);
			ser.EndGroup();
		}

		if (ser.IsReading())
			OnMove();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentTriggerBounds::OnMove(bool invalidateAABB)
{
	Vec3 pos = m_pEntity->GetWorldPos();
	AABB box = m_aabb;
	box.min += pos;
	box.max += pos;
	GetTriggerSystem()->MoveTrigger(m_pProximityTrigger, box, invalidateAABB);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentTriggerBounds::InvalidateTrigger()
{
	OnMove(true);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentTriggerBounds::SetAABB(const AABB& aabb)
{
	m_aabb = aabb;
	OnMove();
}
