// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: HUD Tactical override entity
  
 -------------------------------------------------------------------------
  History:
  - 13:12:2012: Created by Dean Claassen

*************************************************************************/

#include "StdAfx.h"
#include "TacticalOverrideEntity.h"

#include "TacticalManager.h"

//------------------------------------------------------------------------
CTacticalOverrideEntity::CTacticalOverrideEntity()
: m_bMappedToParent(false)
{
}

//------------------------------------------------------------------------

CTacticalOverrideEntity::~CTacticalOverrideEntity()
{
}

//------------------------------------------------------------------------
bool CTacticalOverrideEntity::Init(IGameObject *pGameObject)
{
	SetGameObject(pGameObject);
	
	return true;
}

//------------------------------------------------------------------------
void CTacticalOverrideEntity::PostInit(IGameObject *pGameObject)
{
}

//------------------------------------------------------------------------
void CTacticalOverrideEntity::InitClient( int channelId )
{
}

//------------------------------------------------------------------------
void CTacticalOverrideEntity::Release()
{
	CRY_ASSERT(m_bMappedToParent == false); // Done from Detach event / from reset

	delete this;
}

//------------------------------------------------------------------------
void CTacticalOverrideEntity::GetMemoryUsage(ICrySizer *pSizer) const
{
	pSizer->Add(*this);
}

//------------------------------------------------------------------------
bool CTacticalOverrideEntity::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();
	CRY_ASSERT(!"CTacticalOverrideEntity::ReloadExtension not implemented");
	return false;
}

//------------------------------------------------------------------------
bool CTacticalOverrideEntity::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT(!"CTacticalOverrideEntity::GetEntityPoolSignature not implemented");
	return true;
}

//------------------------------------------------------------------------
void CTacticalOverrideEntity::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_ATTACH_THIS:
		{
			const EntityId parentId = (EntityId) event.nParam[0];
			g_pGame->GetTacticalManager()->AddOverrideEntity(parentId, GetEntityId());
			m_bMappedToParent = true;
		}
		break;
	case ENTITY_EVENT_DETACH_THIS:
		{
			if (m_bMappedToParent)
			{
				g_pGame->GetTacticalManager()->RemoveOverrideEntity(GetEntityId());
				m_bMappedToParent = false;
			}
		}
		break;
	case ENTITY_EVENT_RESET:
		{
			if(gEnv->IsEditor())
			{
				if (event.nParam[0]) // Entering game mode
				{
					if (!m_bMappedToParent) // Could already be added if just linked the entity, won't be if loaded the level with linked entity
					{
						IEntity* pEntity = GetEntity();
						if (pEntity)
						{
							IEntity* pParent = pEntity->GetParent();
							if (pParent)
							{
								g_pGame->GetTacticalManager()->AddOverrideEntity(pParent->GetId(), GetEntityId());
								m_bMappedToParent = true;
							}
							else
							{
								GameWarning("CTacticalOverrideEntity::ProcessEvent: EntityId: %u, doesn't have an attached parent, this entity is a waste", GetEntityId());
							}
						}
					}
				}
				else // Exiting game mode
				{
					if (m_bMappedToParent)
					{
						g_pGame->GetTacticalManager()->RemoveOverrideEntity(GetEntityId());
						m_bMappedToParent = false;
					}
				}
			}
		}
		break;
	}
}
