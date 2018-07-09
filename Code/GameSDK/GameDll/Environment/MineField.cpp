// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Minefield to handle groups of mines

-------------------------------------------------------------------------
History:
- 07:11:2012: Created by Dean Claassen

*************************************************************************/

#include "StdAfx.h"
#include "MineField.h"

#include "../TacticalManager.h"
#include "EntityUtility/EntityScriptCalls.h"
#include <GameObjects/GameObject.h>
#include "UI/HUD/HUDEventDispatcher.h"

namespace MF
{
	void RegisterEvents( IGameObjectExtension& goExt, IGameObject& gameObject )
	{
		const int events[] = {	eGFE_ScriptEvent,

														eMineEventListenerGameObjectEvent_Enabled, 
														eMineEventListenerGameObjectEvent_Disabled, 
														eMineEventListenerGameObjectEvent_Destroyed};

		gameObject.UnRegisterExtForEvents( &goExt, NULL, 0 );
		gameObject.RegisterExtForEvents( &goExt, events, (sizeof(events) / sizeof(int)) );
	}
}

//////////////////////////////////////////////////////////////////////////

CMineField::CMineField()
:	m_currentState(eMineFieldState_NotShowing)
{

}

CMineField::~CMineField()
{
	NotifyAllMinesEvent( eMineGameObjectEvent_UnRegisterListener );
	m_minesData.clear();

	RemoveFromTacticalManager();
}

bool CMineField::Init( IGameObject * pGameObject )
{
	SetGameObject(pGameObject);

	return true;
}

void CMineField::PostInit( IGameObject * pGameObject )
{
	MF::RegisterEvents( *this, *pGameObject );

	Reset( !gEnv->IsEditor() );
}

bool CMineField::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();
	MF::RegisterEvents( *this, *pGameObject );

	CRY_ASSERT_MESSAGE(false, "CMineField::ReloadExtension not implemented");

	return false;
}

bool CMineField::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE(false, "CMineField::GetEntityPoolSignature not implemented");

	return true;
}

void CMineField::Release()
{
	delete this;
}

void CMineField::FullSerialize( TSerialize serializer )
{
	serializer.Value( "CurrentState", (int&)m_currentState );
	uint32 iNumMinesDataData = m_minesData.size();
	serializer.Value( "iNumMinesDataData", iNumMinesDataData );

	if (serializer.IsReading())
	{
		m_minesData.clear();

		for (size_t i = 0; i < iNumMinesDataData; i++)
		{
			serializer.BeginGroup("MineData");
			SMineData mineData;
			serializer.Value("entityId", (int&)mineData.m_entityId);
			serializer.Value("state", mineData.m_state);
			m_minesData.push_back(mineData);
			serializer.EndGroup();
		}

		SetState( m_currentState, true );
	}
	else
	{
		TMinesData::iterator iter = m_minesData.begin();
		const TMinesData::const_iterator iterEnd = m_minesData.end();
		while (iter != iterEnd)
		{
			SMineData& mineData = *iter;
			serializer.BeginGroup("MineData");
			serializer.Value("entityId", (int&)mineData.m_entityId);
			serializer.Value("state", mineData.m_state);
			serializer.EndGroup();

			++iter;
		}
	}
}

void CMineField::Update( SEntityUpdateContext& ctx, int slot )
{
}

void CMineField::HandleEvent( const SGameObjectEvent& gameObjectEvent )
{
	const uint32 eventId = gameObjectEvent.event;
	void* pParam = gameObjectEvent.param;
	if ((eventId == eGFE_ScriptEvent) && (pParam != NULL))
	{
		const char* szEventName = static_cast<const char*>(pParam);
		if (strcmp(szEventName, "OnScanned") == 0)
		{
			// Make all mines scanned and tagged
			TMinesData::iterator iter = m_minesData.begin();
			const TMinesData::const_iterator iterEnd = m_minesData.end();
			while (iter != iterEnd)
			{
				const SMineData& mineData = *iter;
				const EntityId mineEntityId = mineData.m_entityId;
				g_pGame->GetTacticalManager()->SetEntityScanned(mineEntityId);
				SHUDEvent scannedEvent(eHUDEvent_OnEntityScanned);
				scannedEvent.AddData(SHUDEventData(static_cast<int>(mineEntityId)));
				CHUDEventDispatcher::CallEvent(scannedEvent);

				g_pGame->GetTacticalManager()->SetEntityTagged(mineEntityId, true);
				SHUDEvent taggedEvent(eHUDEvent_OnEntityTagged);
				taggedEvent.AddData(SHUDEventData(static_cast<int>(mineEntityId)));
				CHUDEventDispatcher::CallEvent(taggedEvent);

				if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(mineEntityId))
				{
					if(IScriptTable *pScriptTable = pEntity->GetScriptTable())
					{
						Script::CallMethod(pScriptTable, "HasBeenScanned");
					}
				}

				++iter;
			}
		}
		else if (strcmp(szEventName, "OnDestroy") == 0)
		{
			NotifyAllMinesEvent( eMineGameObjectEvent_OnNotifyDestroy );
			UpdateState();
		}
	}
	else if ((eventId == 	eMineEventListenerGameObjectEvent_Enabled) && (pParam != NULL))
	{
		const EntityId mineEntity = *((EntityId*)(pParam));
		CRY_ASSERT(mineEntity != 0);
		if (mineEntity != 0)
		{
				SMineData* pMineData = GetMineData( mineEntity );
				if (pMineData)
				{
					pMineData->m_state |= eMineState_Enabled;
					UpdateState();
				}
				else
				{
					GameWarning( "CMineField::HandleEvent: Failed to find mine entity id: %u", mineEntity );
				}
		}
	}
	else if ((eventId == 	eMineEventListenerGameObjectEvent_Disabled) && (pParam != NULL))
	{
		const EntityId mineEntity = *((EntityId*)(pParam));
		CRY_ASSERT(mineEntity != 0);
		if (mineEntity != 0)
		{
			SMineData* pMineData = GetMineData( mineEntity );
			if (pMineData)
			{
				pMineData->m_state &= ~eMineState_Enabled;
				UpdateState();
			}
			else
			{
				GameWarning( "CMineField::HandleEvent: Failed to find mine entity id: %u", mineEntity );
			}
		}
	}
	else if ((eventId == eMineEventListenerGameObjectEvent_Destroyed) && (pParam != NULL))
	{
		const EntityId mineEntity = *((EntityId*)(pParam));
		CRY_ASSERT(mineEntity != 0);
		if (mineEntity != 0)
		{
			SMineData* pMineData = GetMineData( mineEntity );
			if (pMineData)
			{
				pMineData->m_state |= eMineState_Destroyed;
				UpdateState();
			}
			else
			{
				GameWarning( "CMineField::HandleEvent: Failed to find mine entity id: %u", mineEntity );
			}
		}
	}
}

void CMineField::ProcessEvent( const SEntityEvent& entityEvent )
{
	switch(entityEvent.event)
	{
	case ENTITY_EVENT_RESET:
		{
			const bool bEnteringGameMode = ( entityEvent.nParam[ 0 ] == 1 );
			Reset( bEnteringGameMode );
		}
		break;
	case ENTITY_EVENT_LEVEL_LOADED:
		{
			// For pure game, Only now are all entity links attached and all those entities initialized
			if ( !gEnv->IsEditor() ) // Performed in reset for editor
			{
				NotifyAllMinesEvent( eMineGameObjectEvent_RegisterListener );
				UpdateState();
			}
		}
		break;
	case ENTITY_EVENT_LINK:
		{
			IEntityLink* pLink = (IEntityLink*)entityEvent.nParam[ 0 ];
			CRY_ASSERT(pLink != NULL);
			if (pLink && (stricmp(pLink->name, "Mine") == 0))
			{
				const EntityId mineEntity = pLink->entityId;
				SMineData* pMineData = GetMineData(mineEntity);
				CRY_ASSERT(pMineData == NULL);
				if (!pMineData)
				{
					SMineData newMineData;
					newMineData.m_entityId = mineEntity;
					m_minesData.push_back(newMineData);
				}
			}
		}
		break;
	case ENTITY_EVENT_DELINK:
		{
			IEntityLink* pLink = (IEntityLink*)entityEvent.nParam[ 0 ];
			CRY_ASSERT(pLink != NULL);
			if (pLink && (stricmp(pLink->name, "Mine") == 0))
			{
				const EntityId mineEntity = pLink->entityId;

				TMinesData::iterator iter = m_minesData.begin();
				const TMinesData::const_iterator iterEnd = m_minesData.end();
				while (iter != iterEnd)
				{
					const SMineData& mineData = *iter;
					if (mineData.m_entityId == mineEntity)
					{
						m_minesData.erase(iter);

						NotifyMineEvent( mineEntity, eMineGameObjectEvent_UnRegisterListener );
						UpdateState();

						break;
					}

					++iter;
				}
			}
		}
		break;
	}
}

uint64 CMineField::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEVEL_LOADED) | ENTITY_EVENT_BIT(ENTITY_EVENT_LINK) | ENTITY_EVENT_BIT(ENTITY_EVENT_DELINK);
}

void CMineField::GetMemoryUsage( ICrySizer *pSizer ) const
{

}

void CMineField::SetState( const EMineFieldState state, const bool bForce )
{
	if (state != GetState() || bForce)
	{
		m_currentState = state;

		if (state == eMineFieldState_Showing)
		{
			AddToTacticalManager();
		}
		else
		{
			RemoveFromTacticalManager();
		}
	}
}

void CMineField::NotifyAllMinesEvent( const EMineGameObjectEvent event )
{
	TMinesData::iterator iter = m_minesData.begin();
	const TMinesData::const_iterator iterEnd = m_minesData.end();
	while (iter != iterEnd)
	{
		const SMineData& mineData = *iter;
		NotifyMineEvent( mineData.m_entityId, event );

		++iter;
	}
}

void CMineField::NotifyMineEvent( const EntityId targetEntity, const EMineGameObjectEvent event )
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( targetEntity );
	if (pEntity)
	{
		CGameObject* pGameObject = static_cast<CGameObject*>(pEntity->GetProxy(ENTITY_PROXY_USER));
		if (pGameObject != NULL)
		{
			const EntityId thisEntityId = GetEntityId();
			SGameObjectEvent goEvent( (uint32)event, eGOEF_ToExtensions, IGameObjectSystem::InvalidExtensionID, (void*)(&(thisEntityId)) ); // This entity's id is sent as parameter
			pGameObject->SendEvent( goEvent );
		}
	}
}

void CMineField::AddToTacticalManager()
{
	CTacticalManager* pTacticalManager = g_pGame ? g_pGame->GetTacticalManager() : NULL;
	if ( pTacticalManager != NULL )
	{
		pTacticalManager->AddEntity( GetEntityId(), CTacticalManager::eTacticalEntity_Explosive );
	}
}


void CMineField::RemoveFromTacticalManager()
{
	CTacticalManager* pTacticalManager = g_pGame ? g_pGame->GetTacticalManager() : NULL;
	if ( pTacticalManager != NULL )
	{
		pTacticalManager->RemoveEntity( GetEntityId(), CTacticalManager::eTacticalEntity_Explosive );
	}
}

void CMineField::Reset( const bool bEnteringGameMode )
{
	if (gEnv->IsEditor()) // Need to clear data otherwise won't register mines as not destroyed if were previously destroyed
	{
		TMinesData::iterator iter = m_minesData.begin();
		const TMinesData::const_iterator iterEnd = m_minesData.end();
		while (iter != iterEnd)
		{
			SMineData& mineData = *iter;
			mineData.m_state = eMineState_Enabled;
			++iter;
		}
	}

	if ( bEnteringGameMode )
	{
		NotifyAllMinesEvent( eMineGameObjectEvent_RegisterListener );
	}

	UpdateState();
}

CMineField::SMineData* CMineField::GetMineData( const EntityId entityId )
{
	SMineData* pFoundData = NULL;

	TMinesData::iterator iter = m_minesData.begin();
	const TMinesData::const_iterator iterEnd = m_minesData.end();
	while (iter != iterEnd)
	{
		SMineData& mineData = *iter;
		if (mineData.m_entityId == entityId)
		{
			pFoundData = &mineData;
			break;
		}

		++iter;
	}

	return pFoundData;
}

void CMineField::UpdateState()
{
	if (gEnv->IsEditor() && !gEnv->IsEditorGameMode()) // Always show in editor
	{
		SetState(eMineFieldState_Showing);
	}
	else
	{
		bool bHasVisibleMine = false;

		TMinesData::iterator iter = m_minesData.begin();
		const TMinesData::const_iterator iterEnd = m_minesData.end();
		while (iter != iterEnd)
		{
			SMineData& mineData = *iter;
			if (mineData.m_state & eMineState_Enabled && !(mineData.m_state & eMineState_Destroyed))
			{
				bHasVisibleMine = true;
				break;
			}

			++iter;
		}

		if (bHasVisibleMine)
		{
			SetState(eMineFieldState_Showing);
		}
		else
		{
			SetState(eMineFieldState_NotShowing);
		}
	}
}
