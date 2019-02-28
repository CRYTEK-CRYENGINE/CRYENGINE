// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RichPresence.h"
#include <CryLobby/ICryLobby.h>
#include <CryLobby/ICryLobbyUI.h>
#include "Network/Lobby/GameLobby.h"
#include "Network/Lobby/GameLobbyData.h"
#include "Network/GameNetworkUtils.h"
#include "Network/Squad/SquadManager.h"
#include "GameRules.h"

CRichPresence::CRichPresence() :
	REGISTER_GAME_MECHANISM(CRichPresence),
	m_currentState(eRPS_None),
	m_pendingState(eRPS_None),
	m_desiredState(eRPS_None),
	m_taskID(CryLobbyInvalidTaskID),
	m_currentSessionID(CrySessionInvalidID),
	m_pendingSessionID(CrySessionInvalidID),
	m_updateTimer(0.f),
	m_refresh(false)
{
	LoadXmlFromFile("Scripts/Network/RichPresence.xml");
}

CRichPresence::~CRichPresence()
{
}

void CRichPresence::Update(float dt)
{
	const float currentTime = m_updateTimer > 0.f ? m_updateTimer - dt : m_updateTimer;
	
	m_updateTimer = currentTime;

	if(m_desiredState != eRPS_None)
	{
		if(currentTime <= 0.f)
		{
			// at present the lobby ui is capable of running only one task at a time, so it is possible for SetRichPresence
			// to fail. To counter this, we store the desired state and try again each frame until we succeed
			if(SetRichPresence(m_desiredState))
			{
				m_desiredState = eRPS_None;
			}
		}
	}
}

void CRichPresence::LoadXmlFromFile(const char* path)
{
	XmlNodeRef node = g_pGame->GetIGameFramework()->GetISystem()->LoadXmlFromFile(path);
	if(node)
	{
		int numElements = node->getChildCount();
		for (int i = 0; i < numElements; ++ i)
		{
			XmlNodeRef childNode = node->getChild(i);
			const char *levelName = NULL;
			int id = -1;

			if (childNode->getAttr("name", &levelName) && childNode->getAttr("id", id))
			{
				m_richPresence[levelName] = id;
			}
		}
	}
}

bool CRichPresence::SetRichPresence(ERichPresenceState state)
{
	// don't set rich presence if we don't have a controller yet
	if(!g_pGame->HasExclusiveControllerIndex())
	{
		CryLog("[Rich Presence] not setting rich presence, no player set");
		return true;
	}

	CrySessionID sessionID = CrySessionInvalidID;

	if((m_currentState == state) && GameNetworkUtils::CompareCrySessionId(sessionID, m_currentSessionID))
	{
		if(state != eRPS_InGame || !gEnv->bMultiplayer)
		{
			CryLog("[Rich Presence] not setting rich presence state %d multiplayer %d", state, gEnv->bMultiplayer);
			return true;
		}
	}

	// we are already setting rich presence, so wait until that task has finished
	if (m_taskID != CryLobbyInvalidTaskID)
	{
		CryLog("  already setting rich presence, setting desired state to %d", state);
		m_desiredState = state;
		return false;
	}

	ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
	ICryLobbyService *pLobbyService = pLobby ? pLobby->GetLobbyService(eCLS_Online) : NULL;
	ICryLobbyUI *pLobbyUI = pLobbyService ? pLobbyService->GetLobbyUI() : NULL;
	ECryLobbyError error = eCLE_Success;

	m_pendingSessionID = sessionID;

	if(pLobbyUI)
	{
		uint32 userIndex = g_pGame->GetExclusiveControllerDeviceIndex();
		
		CryLog("[Rich Presence] SetRichPresence %d userIndex %d", state, userIndex);

		switch(state)
		{
			case eRPS_Idle:
			{
				SCryLobbyUserData data;

				data.m_id = RICHPRESENCE_ID;
				data.m_type = eCLUDT_Int32;
				data.m_int32 = RICHPRESENCE_IDLE;
				
				error = pLobbyUI->SetRichPresence(userIndex, &data, 1, &m_taskID, CRichPresence::SetRichPresenceCallback, this);
				break;
			}

			case eRPS_FrontEnd:
			{
				SCryLobbyUserData data;

				data.m_id = RICHPRESENCE_ID;
				data.m_type = eCLUDT_Int32;
				data.m_int32 = RICHPRESENCE_FRONTEND;

				error = pLobbyUI->SetRichPresence(userIndex, &data, 1, &m_taskID, CRichPresence::SetRichPresenceCallback, this);
				break;
			}

			case eRPS_Lobby:
			{
				SCryLobbyUserData data;

				data.m_id = RICHPRESENCE_ID;
				data.m_type = eCLUDT_Int32;
				data.m_int32 = RICHPRESENCE_LOBBY;

				error = pLobbyUI->SetRichPresence(userIndex, &data, 1, &m_taskID, CRichPresence::SetRichPresenceCallback, this);
				break;
			}

			case eRPS_InGame:
			{
				if(gEnv->bMultiplayer)
				{
					CGameRules *pGameRules = g_pGame->GetGameRules();
					IGameFramework *pGameFramework = g_pGame->GetIGameFramework();
					const char *levelName = pGameFramework ? pGameFramework->GetLevelName() : NULL;
					const char *gameRulesName = pGameRules ? pGameRules->GetEntity()->GetClass()->GetName() : NULL;
					int32 gameMode = 0;
					int32 map = 0;

					if(levelName)
					{
						levelName = PathUtil::GetFileName(levelName);
						TRichPresenceMap::const_iterator iter = m_richPresence.find(levelName);
						map = (iter == m_richPresence.end()) ? 0 : iter->second;
					}

					if(gameRulesName)
					{
						TRichPresenceMap::const_iterator iter = m_richPresence.find(gameRulesName);
						gameMode = (iter == m_richPresence.end()) ? 0 : iter->second;
					}

					SCryLobbyUserData data[eRPT_Max];
					
					data[eRPT_String].m_id = RICHPRESENCE_ID;
					data[eRPT_String].m_type = eCLUDT_Int32;
					data[eRPT_String].m_int32 = RICHPRESENCE_GAMEPLAY;
					
					data[eRPT_Param1].m_id = RICHPRESENCE_GAMEMODES;
					data[eRPT_Param1].m_type = eCLUDT_Int32;
					data[eRPT_Param1].m_int32 = gameMode;
					
					data[eRPT_Param2].m_id = RICHPRESENCE_MAPS;
					data[eRPT_Param2].m_type = eCLUDT_Int32;
					data[eRPT_Param2].m_int32 = map;

					error = pLobbyUI->SetRichPresence(userIndex, data, 3, &m_taskID, CRichPresence::SetRichPresenceCallback, this);
				}
				else
				{
					SCryLobbyUserData data;

					data.m_id = RICHPRESENCE_ID;
					data.m_type = eCLUDT_Int32;
					data.m_int32 = RICHPRESENCE_SINGLEPLAYER;
					
					error = pLobbyUI->SetRichPresence(userIndex, &data, 1, &m_taskID, CRichPresence::SetRichPresenceCallback, this);
				}
				break;
			}

			default:
				CryLog("[RichPresence] SetRichPresence - unknown rich presence %d", state);
				break;
		}

		if(error != eCLE_Success)
		{
			// failed to set rich presence, possibly because of too many lobby tasks, store it and try again later
			m_desiredState = state;
			m_pendingSessionID = CrySessionInvalidID;
			
			CryLog("[Rich Presence] SetRichPresence - Rich presence %s with error code %d", error == eCLE_Success ? "succeeded" : "failed", error);
		}
		else
		{
			m_pendingState = state;
			m_desiredState = eRPS_None;

			CryLog("[Rich Presence] SetRichPresence - Rich presence has been successfully started");
		}
	}
#if !defined(_RELEASE)
	else
	{
		error = eCLE_InternalError;
		CryLog("[Rich Presence] SetRichPresence called but we have no lobby, tried to set state to %d", state);
	}
#endif

	return (error == eCLE_Success);
}

void CRichPresence::OnSetRichPresenceCallback(CryLobbyTaskID taskID, ECryLobbyError error)
{
	bool setCurrent = true;

	m_taskID = CryLobbyInvalidTaskID;

	if(error != eCLE_Success)
	{
		if(error == eCLE_SystemIsBusy)
		{
			// if no new state pending, then try again with the previous state
			if(m_desiredState == eRPS_None)
			{
				CryLog("[Rich Presence] failed to set rich presence and no new state desired, retrying pending %d", m_pendingState);
				m_desiredState = m_pendingState;
			}

			m_updateTimer = g_pGameCVars->g_updateRichPresenceInterval;
			setCurrent = false;
		}
		else
		{
			CryLog("[Rich Presence] cannot handle error, setting as complete for now");
		}
	}
	else
	{
		CryLog("[Rich Presence] successfully set rich presence");
	}
	
	if(setCurrent)
	{
		CryLog("[Rich Presence] setting current rich presence");

		m_currentState = m_pendingState;
		m_currentSessionID = m_pendingSessionID;

		if(m_refresh)
		{
			m_desiredState = (m_desiredState == eRPS_None) ? m_currentState : m_desiredState;
		}
	}

	m_pendingState = eRPS_None;
	m_pendingSessionID = CrySessionInvalidID;
	m_refresh = false;

}

void CRichPresence::SetRichPresenceCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg) // static
{
	CryLog("[Rich Presence] SetRichPresenceCallback - Rich presence %s with error code %d", error == eCLE_Success ? "succeeded" : "failed", error);

	CRichPresence *pRichPresence = (CRichPresence*)pArg;
	pRichPresence->OnSetRichPresenceCallback(taskID, error);
}
