// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Smart proximity mine

-------------------------------------------------------------------------
History:
- 20:03:2012: Created by Benito G.R.

*************************************************************************/

#include "StdAfx.h"
#include "SmartMine.h"

#include <GameObjects/GameObject.h>
#include "AutoAimManager.h"
#include "TacticalManager.h"
#include "Game.h"

#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IFactionMap.h>
#include <IItemSystem.h>

DEFINE_STATE_MACHINE( CSmartMine, Behavior );

namespace SM
{
	void RegisterEvents( IGameObjectExtension& goExt, IGameObject& gameObject )
	{
		const int events[] = {	eGFE_ScriptEvent,

														eMineGameObjectEvent_RegisterListener, 
														eMineGameObjectEvent_UnRegisterListener, 
														eMineGameObjectEvent_OnNotifyDestroy};

		gameObject.UnRegisterExtForEvents( &goExt, NULL, 0 );
		gameObject.RegisterExtForEvents( &goExt, events, (sizeof(events) / sizeof(int)) );
	}
}


CSmartMine::CSmartMine()
	: m_inTacticalManager( false )
	, m_enabled( true )
	, m_factionId(IFactionMap::InvalidFactionID)
{

}

CSmartMine::~CSmartMine()
{
	StateMachineReleaseBehavior();

	RemoveFromTacticalManager();
}

bool CSmartMine::Init( IGameObject * pGameObject )
{
	SetGameObject(pGameObject);

	m_effectsController.InitWithEntity( GetEntity() );

	return true;
}

void CSmartMine::PostInit( IGameObject * pGameObject )
{
	SM::RegisterEvents( *this, *pGameObject );
	StateMachineInitBehavior();
}

bool CSmartMine::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();
	SM::RegisterEvents( *this, *pGameObject );

	CRY_ASSERT_MESSAGE(false, "CSmartMine::ReloadExtension not implemented");

	return false;
}

bool CSmartMine::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE(false, "CSmartMine::GetEntityPoolSignature not implemented");

	return true;
}

void CSmartMine::Release()
{
	delete this;
}

void CSmartMine::FullSerialize( TSerialize ser )
{
	uint32 targetCount = m_trackedEntities.size();

	ser.Value( "MineEnabled", m_enabled );
	ser.Value( "MineFaction", m_factionId );
	ser.Value( "MineTargetCount", targetCount );

	CryFixedStringT<16> targetName;
	if (ser.IsReading())
	{
		m_trackedEntities.clear();
		for(uint32 i = 0; i < targetCount; ++i)
		{
			m_trackedEntities.push_back();
			targetName.Format( "MineTarget_%d", i );
			ser.Value( targetName.c_str(), m_trackedEntities[i] );
		}
	}
	else
	{
		for(uint32 i = 0; i < targetCount; ++i)
		{
			targetName.Format( "MineTarget_%d", i );
			ser.Value( targetName.c_str(), m_trackedEntities[i] );
		}
	}

	StateMachineSerializeBehavior( SStateEventSerialize( ser ) );
}

void CSmartMine::Update( SEntityUpdateContext& ctx, int slot )
{
	CRY_ASSERT( slot == SMART_MINE_MAIN_UPDATE_SLOT );

	StateMachineHandleEventBehavior( SSmartMineEvent_Update(ctx.fFrameTime) );
}

void CSmartMine::HandleEvent( const SGameObjectEvent& gameObjectEvent )
{
	const uint32 eventId = gameObjectEvent.event;
	void* pParam = gameObjectEvent.param;
	if ((eventId == eGFE_ScriptEvent) && (pParam != NULL))
	{
		const char* eventName = static_cast<const char*>(pParam);
		if (strcmp(eventName, "enable") == 0)
		{
			OnEnabled();
		}
		else if (strcmp(eventName, "disable") == 0)
		{
			OnDisabled();
		}
		else if (strcmp(eventName, "detonate") == 0)
		{
			StateMachineHandleEventBehavior( SStateEvent(STATE_EVENT_SMARTMINE_TRIGGER_DETONATE) );
		}
	}
	else if ((eventId == eMineGameObjectEvent_RegisterListener) && (pParam != NULL))
	{
		const EntityId listenerEntity = *((EntityId*)(pParam));
		CRY_ASSERT(listenerEntity != 0);
		if (listenerEntity != 0)
		{
			stl::push_back_unique( m_mineEventListeners, listenerEntity );
			UpdateTacticalIcon();
		}
	}
	else if ((eventId == eMineGameObjectEvent_UnRegisterListener) && (pParam != NULL))
	{
		const EntityId listenerEntity = *((EntityId*)(pParam));
		CRY_ASSERT(listenerEntity != 0);
		if (listenerEntity != 0)
		{
			stl::find_and_erase( m_mineEventListeners, listenerEntity );
		}
	}
	else if (eventId == eMineGameObjectEvent_OnNotifyDestroy)
	{
		StateMachineHandleEventBehavior( SStateEvent(STATE_EVENT_SMARTMINE_TRIGGER_DETONATE) );
	}
}

void CSmartMine::ProcessEvent( const SEntityEvent& entityEvent )
{
	switch(entityEvent.event)
	{

	case ENTITY_EVENT_RESET:
		{
			if (gEnv->IsEditor())
			{
				if (entityEvent.nParam[0] == 0) // If leaving game mode in editor...
				{
					Reset();
				}
				else // Want to force icon update
				{
					UpdateTacticalIcon();
				}
			}
		}
		break;

	case ENTITY_EVENT_ENTERAREA:
	case ENTITY_EVENT_LEAVEAREA:
		{
			const bool allowAreaEvents = (gEnv->IsEditing() == false) && (gEnv->pSystem->IsSerializingFile() == 0);
			if (allowAreaEvents)
			{
				const ESmartMineBehaviorEvent eventType = (entityEvent.event == ENTITY_EVENT_ENTERAREA) ? STATE_EVENT_SMARTMINE_ENTITY_ENTERED_AREA : STATE_EVENT_SMARTMINE_ENTITY_LEFT_AREA;
				StateMachineHandleEventBehavior( SSmartMineEvent_TriggerEntity( eventType , static_cast<EntityId>(entityEvent.nParam[0]) ) );
			}
		}
		break;

	case ENTITY_EVENT_HIDE:
	case ENTITY_EVENT_UNHIDE:
		{
			const ESmartMineBehaviorEvent eventType = (entityEvent.event == ENTITY_EVENT_HIDE) ? STATE_EVENT_SMARTMINE_HIDE : STATE_EVENT_SMARTMINE_UNHIDE;
			StateMachineHandleEventBehavior( SStateEvent( eventType ) );
		}
		break;

	}
}

uint64 CSmartMine::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVEAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_HIDE) | ENTITY_EVENT_BIT(ENTITY_EVENT_UNHIDE);
}

void CSmartMine::GetMemoryUsage( ICrySizer *pSizer ) const
{

}

void CSmartMine::StartTrackingEntity( const EntityId entityId )
{
	if ( ShouldStartTrackingEntity( entityId ) )
	{
		if (m_trackedEntities.size() < m_trackedEntities.max_size())
		{
			stl::push_back_unique( m_trackedEntities, entityId );
		}
	}
}

void CSmartMine::StopTrackingEntity( const EntityId entityId )
{
	size_t removeIdx = 0;
	const size_t trackedEntityCount = m_trackedEntities.size();

	while( (removeIdx < trackedEntityCount) && (m_trackedEntities[removeIdx] != entityId) )
	{
		removeIdx++;
	}

	if (removeIdx < trackedEntityCount)
	{
		m_trackedEntities.removeAt(removeIdx);
	}
}

bool CSmartMine::IsHostileEntity( const EntityId entityId ) const
{
	if (gEnv->pAISystem != NULL)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( entityId );
		if (pEntity != NULL)
		{
			const uint8 targetFactionId = g_pGame->GetAutoAimManager().GetTargetFaction( *pEntity );
			if(targetFactionId != IFactionMap::InvalidFactionID)
			{
				return (gEnv->pAISystem->GetFactionMap().GetReaction( targetFactionId, m_factionId ) == IFactionMap::Hostile);
			}
			else
			{
				return true; // In this case is an kickable/thrown object which entered the area
			}
		}
	}

	return false;
}

bool CSmartMine::ContinueTrackingEntity( const EntityId entityId ) const
{
	// Always track player
	if (entityId == g_pGame->GetIGameFramework()->GetClientActorId())
	{
		return true;
	}

	// ... or any AI while alive
	const SAutoaimTarget* pTargetInfo = g_pGame->GetAutoAimManager().GetTargetInfo( entityId );
	if(pTargetInfo != NULL)
	{
		return (pTargetInfo->pActorWeak.lock() != NULL);
	}

	// Anything else must be an object we allowed to be tracked in first place, so continue tracking it if there
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( entityId );
	return (pEntity != NULL);
}

bool CSmartMine::ShouldStartTrackingEntity( const EntityId entityId ) const
{
	// Always track player...
	if (entityId == g_pGame->GetIGameFramework()->GetClientActorId())
	{
		return true;
	}

	// ... or any AI
	const SAutoaimTarget* pTargetInfo = g_pGame->GetAutoAimManager().GetTargetInfo( entityId );
	if(pTargetInfo != NULL)
	{
		return (pTargetInfo->pActorWeak.lock() != NULL);
	}

	// Also track kickable and pickable objects
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( entityId );
	IScriptTable* pScriptTable = (pEntity != NULL) ? pEntity->GetScriptTable() : NULL;
	if(pScriptTable != NULL)
	{
		SmartScriptTable propertiesTable;
		if(pScriptTable->GetValue("Properties", propertiesTable))
		{
			int pickable = 0, kickable = 0;
			propertiesTable->GetValue("bPickable", pickable);
			propertiesTable->GetValue("bInteractLargeObject", kickable);

			if(pickable)
			{
				// Filter out items/weapons
				pickable = (g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(entityId) == NULL);
			}

			if (pickable || kickable)
			{
				//Check if object is moving
				IPhysicalEntity* pEntityPhysics = pEntity->GetPhysics();
				if(pEntityPhysics != NULL)
				{
					pe_status_dynamics entityDynamics;
					if(pEntityPhysics->GetStatus(&entityDynamics))
					{
						return (entityDynamics.v.len2() > 0.1f);
					}
				}
			}
		}
	}

	return false;
}

void CSmartMine::AddToTacticalManager()
{
	if (m_inTacticalManager)
		return;

	CTacticalManager* pTacticalManager = g_pGame ? g_pGame->GetTacticalManager() : NULL;
	if ( pTacticalManager != NULL )
	{
		pTacticalManager->AddEntity( GetEntityId(), CTacticalManager::eTacticalEntity_Explosive );
		UpdateTacticalIcon();
		m_inTacticalManager = true;;
	}
}


void CSmartMine::RemoveFromTacticalManager()
{
	if (!m_inTacticalManager)
		return;

	CTacticalManager* pTacticalManager = g_pGame ? g_pGame->GetTacticalManager() : NULL;
	if ( pTacticalManager != NULL )
	{
		pTacticalManager->RemoveEntity( GetEntityId(), CTacticalManager::eTacticalEntity_Explosive );
		m_inTacticalManager = false;
	}
}

void CSmartMine::UpdateTacticalIcon()
{
	CTacticalManager* pTacticalManager = g_pGame ? g_pGame->GetTacticalManager() : NULL;
	if ( pTacticalManager != NULL )
	{
		pTacticalManager->SetEntityOverrideIcon(GetEntityId(), eIconType_NumIcons);
	}
}

void CSmartMine::Reset()
{
	m_enabled = true;
	m_trackedEntities.clear();

	RemoveFromTacticalManager();
	StateMachineResetBehavior();
}

void CSmartMine::NotifyMineListenersEvent( const EMineEventListenerGameObjectEvent event )
{
	const EntityId thisEntityId = GetEntityId();
	TMineEventListeners::iterator iter = m_mineEventListeners.begin();
	const TMineEventListeners::const_iterator iterEnd = m_mineEventListeners.end();
	while (iter != iterEnd)
	{
		const EntityId entityId = *iter;
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( entityId );
		if (pEntity)
		{
			CGameObject* pGameObject = static_cast<CGameObject*>(pEntity->GetProxy(ENTITY_PROXY_USER));
			if (pGameObject != NULL)
			{
				SGameObjectEvent goEvent( (uint32)event, eGOEF_ToExtensions, IGameObjectSystem::InvalidExtensionID, (void*)(&(thisEntityId)) ); // This entity's id is sent as parameter
				pGameObject->SendEvent( goEvent );
			}
		}

		++iter;
	}
}

void CSmartMine::OnEnabled()
{
	if (m_enabled)
		return;
	Reset();
	m_enabled = true;
	GetEntity()->Hide( false );
}


void CSmartMine::OnDisabled()
{
	if (!m_enabled)
		return;
	m_enabled = false;
	RemoveFromTacticalManager();
	if (GetGameObject()->GetUpdateSlotEnables( this, SMART_MINE_MAIN_UPDATE_SLOT ) > 0)
	{
		GetGameObject()->DisableUpdateSlot( this, SMART_MINE_MAIN_UPDATE_SLOT );
	}
	GetEntity()->Hide( true );
}

