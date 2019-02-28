// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Flash animated door panel

-------------------------------------------------------------------------
History:
- 03:04:2012: Created by Dean Claassen

*************************************************************************/

#include "StdAfx.h"
#include "DoorPanel.h"

#include "../AutoAimManager.h"
#include "../TacticalManager.h"
#include "EntityUtility/EntityScriptCalls.h"
#include "UI/HUD/HUDUtils.h"
#include <GameObjects/GameObject.h>

namespace DP
{
	void RegisterEvents( IGameObjectExtension& goExt, IGameObject& gameObject )
	{
		gameObject.UnRegisterExtForEvents( &goExt, NULL, 0 );
		const int iScriptEventID = eGFE_ScriptEvent;
		gameObject.RegisterExtForEvents( &goExt, &iScriptEventID, 1 );
		const int iStartSharingScreenEventID = eDoorPanelGameObjectEvent_StartShareScreen;
		gameObject.RegisterExtForEvents( &goExt, &iStartSharingScreenEventID, 1 );
		const int iStopSharingScreenEventID = eDoorPanelGameObjectEvent_StopShareScreen;
		gameObject.RegisterExtForEvents( &goExt, &iStopSharingScreenEventID, 1 );
	}
}


namespace DoorPanelBehaviorStateNames
{
	const char* g_doorPanelStateNames[ eDoorPanelBehaviorState_Count ] =
	{
		"Idle",
		"Scanning",
		"ScanSuccess",
		"ScanComplete",
		"ScanFailure",
		"Destroyed",
	};

	const char** GetNames()
	{
		return &g_doorPanelStateNames[ 0 ];
	}

	EDoorPanelBehaviorState FindId( const char* const szName )
	{
		for ( size_t i = 0; i < eDoorPanelBehaviorState_Count; ++i )
		{
			const char* const stateName = g_doorPanelStateNames[ i ];
			if ( strcmp( stateName, szName ) == 0 )
			{
				return static_cast< EDoorPanelBehaviorState >( i );
			}
		}

		GameWarning("DoorPanelBehaviorStateNames:FindId: Failed to find door panel name: %s", szName);
		return eDoorPanelBehaviorState_Invalid;
	}

	const char* GetNameFromId( const EDoorPanelBehaviorState stateId )
	{
		if (stateId >= 0 && stateId < eDoorPanelBehaviorState_Count)
		{
			return g_doorPanelStateNames[stateId];
		}

		GameWarning("DoorPanelBehaviorStateNames:GetNameFromId: Failed to get name for id: %d", stateId);
		return NULL;
	}
}

//////////////////////////////////////////////////////////////////////////

DEFINE_STATE_MACHINE( CDoorPanel, Behavior );

CDoorPanel::CDoorPanel()
:	m_fLastVisibleDistanceCheckTime(0.0f)
, m_fVisibleDistanceSq(0.0f)
, m_fDelayedStateEventTimeDelay(-1.0f)
, m_currentState(eDoorPanelBehaviorState_Invalid)
, m_delayedState(eDoorPanelBehaviorState_Invalid)
, m_sharingMaterialEntity(0)
, m_bHasDelayedStateEvent(false)
, m_bInTacticalManager(false)
, m_bFlashVisible(false)
{

}

CDoorPanel::~CDoorPanel()
{
	StateMachineReleaseBehavior();
}

bool CDoorPanel::Init( IGameObject * pGameObject )
{
	SetGameObject(pGameObject);

	return true;
}

void CDoorPanel::PostInit( IGameObject * pGameObject )
{
	DP::RegisterEvents( *this, *pGameObject );

	StateMachineInitBehavior();

	GetGameObject()->EnableUpdateSlot( this, DOOR_PANEL_MODEL_NORMAL_SLOT );

	Reset( !gEnv->IsEditor() );

	SetStateById( GetInitialBehaviorStateId() );
}

bool CDoorPanel::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();
	DP::RegisterEvents( *this, *pGameObject );

	CRY_ASSERT_MESSAGE(false, "CDoorPanel::ReloadExtension not implemented");

	return false;
}

bool CDoorPanel::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE(false, "CDoorPanel::GetEntityPoolSignature not implemented");

	return true;
}

void CDoorPanel::Release()
{
	delete this;
}

void CDoorPanel::FullSerialize( TSerialize serializer )
{
	if (serializer.IsReading())
	{
		m_fLastVisibleDistanceCheckTime = 0.0f;
		int iCurrentState = (int)eDoorPanelBehaviorState_Idle;
		serializer.Value( "CurrentState", iCurrentState );
		const EDoorPanelBehaviorState stateId = (EDoorPanelBehaviorState)iCurrentState;
		if (stateId != eDoorPanelBehaviorState_Invalid)
		{
			m_currentState = stateId;
		}
	}
	else
	{
		int iCurrentState = (int)m_currentState;
		serializer.Value( "CurrentState", iCurrentState );
	}
	
	StateMachineSerializeBehavior( SStateEventSerialize(serializer) );
}

void CDoorPanel::Update( SEntityUpdateContext& ctx, int slot )
{
	if (m_bHasDelayedStateEvent)
	{
		if (m_fDelayedStateEventTimeDelay > FLT_EPSILON)
		{
			m_fDelayedStateEventTimeDelay -= ctx.fFrameTime;
		}
		else
		{
			m_fDelayedStateEventTimeDelay = -1.0f;
			m_bHasDelayedStateEvent = false;
			const EDoorPanelBehaviorState delayedState = m_delayedState;
			m_delayedState = eDoorPanelBehaviorState_Invalid;
			SetStateById(delayedState);
		}		
	}

	// Check visible distance
	if (m_fVisibleDistanceSq > 0.0f)
	{
		const float fCurTime = gEnv->pTimer->GetCurrTime();
		if ((fCurTime - m_fLastVisibleDistanceCheckTime) >= GetGameConstCVar(g_flashdoorpanel_distancecheckinterval))
		{
			m_fLastVisibleDistanceCheckTime = fCurTime;
			const Vec3& clientPos = CHUDUtils::GetClientPos();
			const float fDistSqFromClientToObject = clientPos.GetSquaredDistance(GetEntity()->GetWorldPos());
			const bool bVisible = fDistSqFromClientToObject <= m_fVisibleDistanceSq;
			if (bVisible != m_bFlashVisible)
			{
				m_bFlashVisible = bVisible;
				SDoorPanelVisibleEvent visibleEvent( bVisible );
				StateMachineHandleEventBehavior( visibleEvent );
			}
		}
	}
}

void CDoorPanel::HandleEvent( const SGameObjectEvent& gameObjectEvent )
{
	const uint32 eventId = gameObjectEvent.event;
	void* pParam = gameObjectEvent.param;
	if ((eventId == eGFE_ScriptEvent) && (pParam != NULL))
	{
		const char* szEventName = static_cast<const char*>(pParam);

		const EDoorPanelBehaviorState stateId = DoorPanelBehaviorStateNames::FindId( szEventName );
		if ( stateId != eDoorPanelBehaviorState_Invalid )
		{
			SetStateById( stateId );
		}
		else
		{
			GameWarning("CDoorPanel::HandleEvent: Failed to handle event: %s", szEventName);
		}
	}
	else if ((eventId == 	eDoorPanelGameObjectEvent_StartShareScreen) && (pParam != NULL))
	{
		const EntityId entityToShareScreen = *((EntityId*)(pParam));
		CRY_ASSERT(entityToShareScreen != 0);
		CRY_ASSERT(m_sharingMaterialEntity == 0);
		if (m_sharingMaterialEntity == 0 && entityToShareScreen != 0)
		{
			IEntity* pEntity = GetEntity();
			if (pEntity && !pEntity->IsHidden())
			{
				// Force flash deinit and init (Much cleaner to do this instead of delaying till after entity links are set)
				StateMachineHandleEventBehavior( SDoorPanelVisibleEvent(false) );
				m_sharingMaterialEntity = entityToShareScreen; // Can't set till after releasing to ensure it doesnt think its already shared
				m_bFlashVisible = true;
				StateMachineHandleEventBehavior( SDoorPanelVisibleEvent(true) );
			}
			else
			{
				m_sharingMaterialEntity = entityToShareScreen;
			}
		}
		else
		{
			GameWarning("CDoorPanel::ProcessEvent: Not valid to have multiple ShareScreen entity links or invalid entity id");
		}
	}
	else if ((eventId == 	eDoorPanelGameObjectEvent_StopShareScreen) && (pParam != NULL))
	{
		if (m_sharingMaterialEntity != 0)
		{
			const EntityId entityToShareScreen = *((EntityId*)(pParam));
			CRY_ASSERT(entityToShareScreen == m_sharingMaterialEntity);
			if (entityToShareScreen == m_sharingMaterialEntity)
			{
				StateMachineHandleEventBehavior( SDoorPanelVisibleEvent(false) );
				m_sharingMaterialEntity = 0;
				m_bFlashVisible = false;
			}
		}
	}
}

void CDoorPanel::ProcessEvent( const SEntityEvent& entityEvent )
{
	switch(entityEvent.event)
	{
	case ENTITY_EVENT_RESET:
		{
			const bool bEnteringGameMode = ( entityEvent.nParam[ 0 ] == 1 );
			Reset( bEnteringGameMode );
		}
		break;
	case ENTITY_EVENT_UNHIDE:
		{
			GetGameObject()->EnableUpdateSlot( this, DOOR_PANEL_MODEL_NORMAL_SLOT );
			if (m_fVisibleDistanceSq < 0.0f)
			{
				m_bFlashVisible = true;
				SDoorPanelVisibleEvent visibleEvent( true );
				StateMachineHandleEventBehavior( visibleEvent );
			}
		}
		break;
	case ENTITY_EVENT_HIDE:
		{
			GetGameObject()->DisableUpdateSlot( this, DOOR_PANEL_MODEL_NORMAL_SLOT );
			m_bFlashVisible = false;
			SDoorPanelVisibleEvent visibleEvent( false );
			StateMachineHandleEventBehavior( visibleEvent );
		}
		break;
	case ENTITY_EVENT_LINK:
		{
			IEntityLink* pLink = (IEntityLink*)entityEvent.nParam[ 0 ];
			CRY_ASSERT(pLink != NULL);
			if (pLink && (stricmp(pLink->name, "ShareScreen") == 0))
			{
				const EntityId entityToShareTo = pLink->entityId;
				stl::push_back_unique( m_screenSharingEntities, entityToShareTo );
			}
		}
		break;
	case ENTITY_EVENT_DELINK:
		{
			IEntityLink* pLink = (IEntityLink*)entityEvent.nParam[ 0 ];
			CRY_ASSERT(pLink != NULL);
			if (pLink && (stricmp(pLink->name, "ShareScreen") == 0))
			{
				const EntityId entityToShareTo = pLink->entityId;
				stl::find_and_erase(m_screenSharingEntities, entityToShareTo);
			}
		}
		break;
	}
}

uint64 CDoorPanel::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_UNHIDE) | ENTITY_EVENT_BIT(ENTITY_EVENT_HIDE) | ENTITY_EVENT_BIT(ENTITY_EVENT_LINK) | ENTITY_EVENT_BIT(ENTITY_EVENT_DELINK);
}

void CDoorPanel::GetMemoryUsage( ICrySizer *pSizer ) const
{

}

void CDoorPanel::HandleFSCommand(const char* pCommand, const char* pArgs, void* pUserData)
{
	/* Nolonger using fs commands to handle state change due to needing to use pDynTexSrc->EnablePerFrameRendering(true) to prevent state change not being processed when not being rendered
		 using m_fDelayedStateEventTimeDelay instead
	if( strcmp(pCommand, "ScanSuccessDone") == 0)
	{
		SetDelayedStateChange(eDoorPanelBehaviorState_ScanComplete);
	}
	else if( strcmp(pCommand, "ScanFailureDone") == 0)
	{
		SetDelayedStateChange(eDoorPanelBehaviorState_Idle);
	}
	*/
}

EDoorPanelBehaviorState CDoorPanel::GetInitialBehaviorStateId() const
{
	EDoorPanelBehaviorState doorPanelState = eDoorPanelBehaviorState_Invalid;

	IEntity* pEntity = GetEntity();

	char* szDoorPanelStateName = NULL;
	bool bResult = EntityScripts::GetEntityProperty(GetEntity(), "esDoorPanelState", szDoorPanelStateName);
	if (bResult)
	{
		doorPanelState = DoorPanelBehaviorStateNames::FindId( szDoorPanelStateName );
	}

	if ( doorPanelState != eDoorPanelBehaviorState_Invalid )
	{
		return doorPanelState;
	}

	return eDoorPanelBehaviorState_Idle;
}

void CDoorPanel::SetStateById( const EDoorPanelBehaviorState stateId )
{
	SDoorPanelStateEventForceState stateEventForceState( stateId );
	StateMachineHandleEventBehavior( stateEventForceState );
}

void CDoorPanel::NotifyBehaviorStateEnter( const EDoorPanelBehaviorState stateId )
{
	m_currentState = stateId;

	if ( stateId != eDoorPanelBehaviorState_Invalid ) // Notify state change to script
	{
		IEntity* pEntity = GetEntity();
		if (pEntity)
		{
			AssignAsFSCommandHandler(); // Need this on state change to ensure if a door panel which has a shared screen handles it has command focus

			const char* szDoorPanelStateName = DoorPanelBehaviorStateNames::GetNameFromId( stateId );
			if (szDoorPanelStateName != NULL && szDoorPanelStateName[0] != '\0')
			{
				EntityScripts::CallScriptFunction(pEntity, pEntity->GetScriptTable(), "OnStateChange", szDoorPanelStateName);
			}
		}
	}
}

void CDoorPanel::SetDelayedStateChange(const EDoorPanelBehaviorState stateId, const float fTimeDelay)
{
	CRY_ASSERT(m_bHasDelayedStateEvent == false);
	if (!m_bHasDelayedStateEvent)
	{
		m_bHasDelayedStateEvent = true;
		m_fDelayedStateEventTimeDelay = fTimeDelay;
		m_delayedState = stateId;
	}
	else
	{
		GameWarning("CDoorPanel::AddDelayedStateEvent: Already have a delayed state event, this shouldn't happen");
	}
}

void CDoorPanel::AssignAsFSCommandHandler()
{
	IEntity* pEntity = GetEntity();
	if (pEntity)
	{
		IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
		
		{
			_smart_ptr<IMaterial> pMaterial = pIEntityRender->GetRenderMaterial(DOOR_PANEL_MODEL_NORMAL_SLOT);
			IFlashPlayer* pFlashPlayer = CHUDUtils::GetFlashPlayerFromMaterialIncludingSubMaterials(pMaterial, true);
			if (pFlashPlayer) // Valid to not have a flash player, since will update when flash setup
			{
				pFlashPlayer->SetFSCommandHandler(this);
			}
		}
	}
}

void CDoorPanel::NotifyScreenSharingEvent(const EDoorPanelGameObjectEvent event)
{
	if (m_screenSharingEntities.size() > 0)
	{
		const EntityId thisEntityId = GetEntityId();
		for (size_t i = 0; i < m_screenSharingEntities.size(); i++)
		{
			const EntityId entityId = m_screenSharingEntities[i];
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity( entityId );
			if (pEntity)
			{
				CGameObject* pGameObject = static_cast<CGameObject*>(pEntity->GetProxy(ENTITY_PROXY_USER));
				if (pGameObject != NULL)
				{
					SGameObjectEvent goEvent( (uint32)event, eGOEF_ToExtensions, IGameObjectSystem::InvalidExtensionID, (void*)(&(thisEntityId)) );
					pGameObject->SendEvent( goEvent );
				}
			}
		}
	}
}

void CDoorPanel::AddToTacticalManager()
{
	if (m_bInTacticalManager)
		return;

	CTacticalManager* pTacticalManager = g_pGame ? g_pGame->GetTacticalManager() : NULL;
	if ( pTacticalManager != NULL )
	{
		pTacticalManager->AddEntity( GetEntityId(), CTacticalManager::eTacticalEntity_Item );
		m_bInTacticalManager = true;;
	}
}


void CDoorPanel::RemoveFromTacticalManager()
{
	if (!m_bInTacticalManager)
		return;

	CTacticalManager* pTacticalManager = g_pGame ? g_pGame->GetTacticalManager() : NULL;
	if ( pTacticalManager != NULL )
	{
		pTacticalManager->RemoveEntity( GetEntityId(), CTacticalManager::eTacticalEntity_Item );
		m_bInTacticalManager = false;
	}
}

void CDoorPanel::Reset( const bool bEnteringGameMode )
{
	m_fLastVisibleDistanceCheckTime = 0.0f;
	m_bHasDelayedStateEvent = false;

	SDoorPanelResetEvent resetEvent( bEnteringGameMode );
	StateMachineHandleEventBehavior( resetEvent );

	SetStateById( GetInitialBehaviorStateId() );
}
