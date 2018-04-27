// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description:
Network-syncs the actor entity ID being targeted by a player
**************************************************************************/

#include "StdAfx.h"
#include "PlayerPlugin_CurrentlyTargetting.h"
#include "Player.h"
#include "GameCVars.h"
#include "Utility/CryWatch.h"
#include "IWorldQuery.h"
#include "GameRules.h"
#include "Battlechatter.h"
#include "UI/UIManager.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "PlayerPluginEventDistributor.h"


CPlayerPlugin_CurrentlyTargetting::CPlayerPlugin_CurrentlyTargetting()
{
	m_currentTarget = 0;
	m_currentTargetTime = 0.0f;
	m_bTargetingLocalPlayer = false;
	m_targetedSignal.SetSignal("SniperCountermeasuresTargetted");
}

void CPlayerPlugin_CurrentlyTargetting::Enter( CPlayerPluginEventDistributor* pEventDist )
{
	CPlayerPlugin::Enter(pEventDist);

	pEventDist->RegisterEvent(this, EPE_Reset);
}

void CPlayerPlugin_CurrentlyTargetting::Leave()
{
	m_currentTarget = 0;
	m_currentTargetTime = 0.0f;

	if(m_bTargetingLocalPlayer)
	{
		SHUDEvent event (eHUDEvent_LocalPlayerTargeted);
		event.AddData(false);
		CHUDEventDispatcher::CallEvent(event);

		REINST("needs verification!");
		/*EntityId clientId = gEnv->pGameFramework->GetClientActorId();
		if(clientId && m_targetedSignal.IsPlaying(clientId))
		{
			m_targetedSignal.Stop(clientId);
		}*/
	}

	CPlayerPlugin::Leave();
}

void CPlayerPlugin_CurrentlyTargetting::Update(float dt)
{
	m_currentTargetTime += dt;	//updated locally for all players (so doesn't have to be synced)

	assert (IsEntered());
	if (m_ownerPlayer->IsClient())
	{
		EntityId newTargetId = !m_ownerPlayer->IsDead() ? m_ownerPlayer->GetGameObject()->GetWorldQuery()->GetLookAtEntityId() : 0;

		if (newTargetId)
		{
			IActor * targettedActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(newTargetId);
			if (targettedActor == NULL || targettedActor->IsDead())
			{
				newTargetId = 0;
			}
		}

		if (m_currentTarget != newTargetId)
		{
			m_currentTarget = newTargetId;

#if 0 //PLAYER_PLUGIN_DEBUGGING
			// Quite spammy, feel free to re-enable locally
			if (gEnv->bMultiplayer)
			{
				IEntity * entity = gEnv->pEntitySystem->GetEntity(m_currentTarget);
				CryLog("[TARGETING] Local %s %s is now targeting e%05d (%s %s)", m_ownerPlayer->GetEntity()->GetClass()->GetName(), m_ownerPlayer->GetEntity()->GetName(), m_currentTarget, entity ? entity->GetName() : "NULL", entity ? entity->GetClass()->GetName() : "entity");
			}
#endif

			CCCPOINT_IF(m_currentTarget, PlayerState_LocalPlayerNowTargettingSomebody);
			CCCPOINT_IF(!m_currentTarget, PlayerState_LocalPlayerNowTargettingNobody);

			m_currentTargetTime = 0.0f;
			CHANGED_NETWORK_STATE(m_ownerPlayer, CPlayer::ASPECT_CURRENTLYTARGETTING_CLIENT);
		}
	}

#if PLAYER_PLUGIN_DEBUGGING
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_currentTarget);
	PlayerPluginWatch ("Target e%05d (%s %s) - %.2f", m_currentTarget, pEntity ? pEntity->GetName() : "NULL", pEntity ? pEntity->GetClass()->GetName() : "entity", m_currentTargetTime);
#endif
}

void CPlayerPlugin_CurrentlyTargetting::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if(aspect == CPlayer::ASPECT_CURRENTLYTARGETTING_CLIENT)
	{
		NET_PROFILE_SCOPE("CurrentlyTargeting", ser.IsReading());

		EntityId previousTarget = m_currentTarget;
		ser.Value("curTargetId", m_currentTarget, 'eid');

		if(ser.IsReading())
		{
			if(m_currentTarget != previousTarget)
			{
#if 0 // PLAYER_PLUGIN_DEBUGGING
			// Quite spammy, feel free to re-enable locally
				if (gEnv->bMultiplayer)
				{
					IEntity * entity = gEnv->pEntitySystem->GetEntity(m_currentTarget);
					CryLog("[TARGETING] Remote %s %s is now targeting e%05d (%s %s)", m_ownerPlayer->GetEntity()->GetClass()->GetName(), m_ownerPlayer->GetEntity()->GetName(), m_currentTarget, entity ? entity->GetName() : "NULL", entity ? entity->GetClass()->GetName() : "entity");
				}
#endif

				CCCPOINT_IF(m_currentTarget, PlayerState_RemotePlayerNowTargettingSomebody);
				CCCPOINT_IF(!m_currentTarget, PlayerState_RemotePlayerNowTargettingNobody);

				m_currentTargetTime = 0.0f;

				CGameRules *pGameRules = g_pGame->GetGameRules();

				const EntityId clientActorId = gEnv->pGameFramework->GetClientActorId();
				if (m_bTargetingLocalPlayer)
				{
					SHUDEvent event (eHUDEvent_LocalPlayerTargeted);
					event.AddData(false);
					CHUDEventDispatcher::CallEvent(event);

					m_bTargetingLocalPlayer = false;
					//m_targetedSignal.Stop(clientActorId);
				}
			}
		}
	}
}

void CPlayerPlugin_CurrentlyTargetting::HandleEvent(EPlayerPlugInEvent theEvent, void * data)
{
	CPlayerPlugin::HandleEvent(theEvent, data);

	switch (theEvent)
	{
		case EPE_Reset:
		if (m_ownerPlayer->IsClient())
		{
			assert (m_currentTarget == 0);
		}
		break;
	}
}


