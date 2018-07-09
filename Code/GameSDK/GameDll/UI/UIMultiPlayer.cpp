// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIMultiPlayer.cpp
//  Version:     v1.00
//  Created:     26/8/2011 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "UIMultiPlayer.h"

#include <CryGame/IGameFramework.h>
#include "Game.h"
#include "Actor.h"
#include "GameRules.h"
#include "Network/Lobby/GameLobby.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"

////////////////////////////////////////////////////////////////////////////
CUIMultiPlayer::CUIMultiPlayer()
	: m_pUIEvents(NULL)
	, m_pUIFunctions(NULL)
	, m_LocalPlayerName("Dude")
{
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::InitEventSystem()
{
	if (!gEnv->pFlashUI)
		return;

	ICVar* pServerVar = gEnv->pConsole->GetCVar("cl_serveraddr");
	m_ServerName = pServerVar ? pServerVar->GetString() : "";
	if (m_ServerName == "")
		m_ServerName = "localhost";

	// events to send from this class to UI flowgraphs
	m_pUIFunctions = gEnv->pFlashUI->CreateEventSystem("MP", IUIEventSystem::eEST_SYSTEM_TO_UI);
	m_eventSender.Init(m_pUIFunctions);

	{
		SUIEventDesc evtDesc("EnteredGame", "Triggered once the local player enters the game");
		m_eventSender.RegisterEvent<eUIE_EnteredGame>(evtDesc);
	}

	{
		SUIEventDesc evtDesc("ChatMessageReceived", "Triggered when chatmessages is received");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Player", "Name of the player");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Message", "Message");
		m_eventSender.RegisterEvent<eUIE_ChatMsgReceived>(evtDesc);
	}

	{
		SUIEventDesc evtDesc("PlayerJoined", "Triggered if a player joins the game");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Int>("ID", "ID of player");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Name", "Name of the player");
		m_eventSender.RegisterEvent<eUIE_PlayerJoined>(evtDesc);
	}

	{
		SUIEventDesc evtDesc("ScoreBoardItemWasUpdated", "Triggered to update an item on the scoreboard");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Int>("ID", "ID of player");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Name", "Name of the player");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Int>("Kills", "amount of kills this player has");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Int>("Deaths", "amount of deaths this player had");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Team", "Team of the player");
		m_eventSender.RegisterEvent<eUIE_ScoreBoardItemWasUpdated>(evtDesc);
	}

	{
		SUIEventDesc evtDesc("PlayerLeft", "Triggered if a player left the game");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Int>("ID", "ID of player");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Name", "Name of the player");
		m_eventSender.RegisterEvent<eUIE_PlayerLeft>(evtDesc);
	}

	{
		SUIEventDesc evtDesc("PlayerKilled", "Triggered if a player gets killed");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Int>("ID", "ID of player");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Name", "Name of the player");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Int>("ShooterID", "ID of the shooter");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("ShooterName", "Name of the shooter");
		m_eventSender.RegisterEvent<eUIE_PlayerKilled>(evtDesc);
	}

	{
		SUIEventDesc evtDesc("PlayerRenamed", "Triggered if a player was renamed");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Int>("ID", "ID of player");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("NewName", "New name of the player");
		m_eventSender.RegisterEvent<eUIE_PlayerRenamed>(evtDesc);
	}

	{
		SUIEventDesc evtDesc("OnGetName", "Triggers once the \"GetPlayerName\" node was called");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Name", "Name of the local player");
		m_eventSender.RegisterEvent<eUIE_SendName>(evtDesc);
	}

	{
		SUIEventDesc evtDesc("OnGetServerName", "Triggers once the \"GetLastServer\" node was called");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Address", "Last server address");
		m_eventSender.RegisterEvent<eUIE_SendServer>(evtDesc);
	}


	// events that can be sent from UI flowgraphs to this class
	m_pUIEvents = gEnv->pFlashUI->CreateEventSystem("MP", IUIEventSystem::eEST_UI_TO_SYSTEM);
	m_eventDispatcher.Init(m_pUIEvents, this, "CUIMultiPlayer");

	{
		SUIEventDesc evtDesc("GetPlayers", "Request all players (will trigger the \"PlayerJoined\" node for each player)");
		m_eventDispatcher.RegisterEvent(evtDesc, &CUIMultiPlayer::RequestPlayers);
	}

	{
		SUIEventDesc evtDesc("GetPlayerName", "Get the name of the local player in mp (will trigger the \"OnGetName\" node)");
		m_eventDispatcher.RegisterEvent(evtDesc, &CUIMultiPlayer::GetPlayerName);
	}

	{
		SUIEventDesc evtDesc("RequestUpdatedScores", "Enables the scoreboard to be updated in MP");
		m_eventDispatcher.RegisterEvent(evtDesc, &CUIMultiPlayer::RequestUpdatedScores);
	}

	{
		SUIEventDesc evtDesc("SetPlayerName", "Set the name of the local player in mp");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Name", "Local player name");
		m_eventDispatcher.RegisterEvent(evtDesc, &CUIMultiPlayer::SetPlayerName);
	}

	{
		SUIEventDesc evtDesc("ConnectToServer", "Connect to a server");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Address", "server address");
		m_eventDispatcher.RegisterEvent(evtDesc, &CUIMultiPlayer::ConnectToServer);
	}

	{
		SUIEventDesc evtDesc("GetLastServer", "Get the server name that was last used");
		m_eventDispatcher.RegisterEvent(evtDesc, &CUIMultiPlayer::GetServerName);
	}

	{
		SUIEventDesc evtDesc("SendChatMessage", "");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Message", "");
		m_eventDispatcher.RegisterEvent(evtDesc, &CUIMultiPlayer::OnSendChatMessage);
	}

	gEnv->pFlashUI->RegisterModule(this, "CUIMultiPlayer");
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::UnloadEventSystem()
{
	if (gEnv->pFlashUI)
		gEnv->pFlashUI->UnregisterModule(this);
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::Reset()
{
	m_Players.clear();
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
// ui functions
void CUIMultiPlayer::EnteredGame()
{
	m_eventSender.SendEvent<eUIE_EnteredGame>();
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::PlayerJoined(EntityId playerid, const string& name)
{
	CryLogAlways("[CUIMultiPlayer] PlayerJoined %i %s", playerid, name.c_str() );

	m_Players[playerid].name = name;

	if (gEnv->pGameFramework->GetClientActorId() == playerid)
	{
		SubmitNewName();
		return;
	}

	m_eventSender.SendEvent<eUIE_PlayerJoined>(playerid, name);
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::UpdateScoreBoardItem(EntityId playerid, const string& name, int kills, int deaths, const string& team)
{
	m_eventSender.SendEvent<eUIE_ScoreBoardItemWasUpdated>(playerid, name, kills, deaths, team);
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::PlayerLeft(EntityId playerid, const string& name)
{
	CryLogAlways("[CUIMultiPlayer] PlayerLeft %i %s", playerid, name.c_str() );

	if (gEnv->pGameFramework->GetClientActorId() == playerid)
		return;

	// fix up player id in case that the entity was already removed and the Network was not able to resolve entity id
	if (playerid == 0)
	{
		for (TPlayers::const_iterator it = m_Players.begin(); it != m_Players.end(); ++it)
		{
			if (it->second.name == name)
			{
				playerid = it->first;
			}
		}
	}

	const bool ok = stl::member_find_and_erase(m_Players, playerid);
 	assert( ok );

	m_eventSender.SendEvent<eUIE_PlayerLeft>(playerid, name);
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::PlayerKilled(EntityId playerid, EntityId shooterid)
{
	m_eventSender.SendEvent<eUIE_PlayerKilled>(playerid, GetPlayerNameById(playerid), shooterid, GetPlayerNameById(shooterid));
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::PlayerRenamed(EntityId playerid, const string& newName)
{
	CryLogAlways("[CUIMultiPlayer] PlayerRenamed %i %s", playerid, newName.c_str() );

	m_Players[playerid].name = newName;

	m_eventSender.SendEvent<eUIE_PlayerRenamed>(playerid, newName);
}

void CUIMultiPlayer::OnChatReceived(EntityId senderId, int teamFaction, const char* message)
{
	m_eventSender.SendEvent<eUIE_ChatMsgReceived>(GetPlayerNameById(senderId), message);
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::RequestPlayers()
{
	IActorIteratorPtr actors = gEnv->pGameFramework->GetIActorSystem()->CreateActorIterator();
	while (IActor* pActor = actors->Next())
	{
		if (pActor->IsPlayer() && m_Players.find(pActor->GetEntityId()) == m_Players.end())
		{
			PlayerJoined(pActor->GetEntityId(), pActor->GetEntity()->GetName());
		}
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::GetPlayerName()
{
	m_eventSender.SendEvent<eUIE_SendName>(m_LocalPlayerName);
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::RequestUpdatedScores()
{
	CGameRules* pGameRules = g_pGame->GetGameRules();
	if (!pGameRules)
		return;
	IGameRulesPlayerStatsModule* pPlayerStats = pGameRules->GetPlayerStatsModule();
	int statCount = pPlayerStats->GetNumPlayerStats();
	
	for (int i = 0; i < statCount; i++)
	{
		const SGameRulesPlayerStat* pPlayerStat = pPlayerStats->GetNthPlayerStats(i);
		UpdateScoreBoardItem(pPlayerStat->playerId, GetPlayerNameById(pPlayerStat->playerId), pPlayerStat->kills, pPlayerStat->deaths, GetPlayerTeamById(pPlayerStat->playerId));
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::SetPlayerName( const string& newname )
{
	m_LocalPlayerName = newname;
	SubmitNewName();
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::ConnectToServer( const string& server )
{
	if (gEnv->IsEditor()) return;

	m_ServerName = server;
	g_pGame->GetIGameFramework()->ExecuteCommandNextFrame(string().Format("connect %s", server.c_str()));
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::GetServerName()
{
	m_eventSender.SendEvent<eUIE_SendServer>(m_ServerName);
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::OnSendChatMessage( const string& message )
{
	//chat to all
#if ENABLE_CHAT_MESSAGES
	CGameLobby* lobby = g_pGame->GetGameLobby();
	if(lobby)
	{
		lobby->SendChatMessage(false, message.c_str());
	}
#endif
}


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::SubmitNewName()
{
	if (m_LocalPlayerName.empty())
		return;

	INetChannel* pNetChannel = g_pGame->GetIGameFramework()->GetClientChannel();
	if (pNetChannel && pNetChannel->GetNickname() && strcmp(pNetChannel->GetNickname(), m_LocalPlayerName.c_str()) == 0 )
		return;

	CActor* pLocalPlayer = (CActor*) g_pGame->GetIGameFramework()->GetClientActor();
	if (pLocalPlayer && g_pGame->GetGameRules())
		g_pGame->GetGameRules()->RenamePlayer( pLocalPlayer, m_LocalPlayerName.c_str() );
};

////////////////////////////////////////////////////////////////////////////
string CUIMultiPlayer::GetPlayerNameById( EntityId playerid )
{
	TPlayers::const_iterator it = m_Players.find(playerid);
	if (it != m_Players.end() )
	{
		return it->second.name;
	}

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(playerid);
	return pEntity ? pEntity->GetName() : "<UNDEFINED>";
}

////////////////////////////////////////////////////////////////////////////
string CUIMultiPlayer::GetPlayerTeamById( EntityId playerid )
{
	IGameFramework * const pGameFramework = gEnv->pGameFramework;
	IActorSystem * const pActorSystem = pGameFramework ? pGameFramework->GetIActorSystem() : NULL;
	IActor * const pActor = pActorSystem ? pActorSystem->GetActor(playerid) : NULL;
	if (pActor)
	{
		const int teamId = pActor->GetTeamId();
		if (teamId > 0)
		{
			IGameRulesSystem * const pGameRulesSystem = pGameFramework ? pGameFramework->GetIGameRulesSystem() : NULL;
			IGameRules * const pGameRules = pGameRulesSystem ? pGameRulesSystem->GetCurrentGameRules() : NULL;
			const char * const pTeamName = pGameRules ? pGameRules->GetTeamName(teamId) : NULL;
			if (pTeamName)
			{
				return pTeamName;
			}
		}
	}

	return string();
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::LoadProfile(IPlayerProfile* pProfile)
{
	pProfile->GetAttribute( "mp_username", m_LocalPlayerName);
	pProfile->GetAttribute( "mp_server",  m_ServerName);

	// override if setup in system cfg
	if (ICVar* pServerVar = gEnv->pConsole->GetCVar("cl_serveraddr"))
	{
		const string serverName = pServerVar->GetString();
		if (!serverName.empty())
			m_ServerName = serverName;
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::SaveProfile(IPlayerProfile* pProfile) const
{
	pProfile->SetAttribute( "mp_username", m_LocalPlayerName);
	pProfile->SetAttribute( "mp_server",  m_ServerName);
}

////////////////////////////////////////////////////////////////////////////
void CUIMultiPlayer::OnEntityKilled( const HitInfo &hitInfo )
{
	PlayerKilled(hitInfo.targetId, hitInfo.shooterId);
}

////////////////////////////////////////////////////////////////////////////
REGISTER_UI_EVENTSYSTEM( CUIMultiPlayer );
