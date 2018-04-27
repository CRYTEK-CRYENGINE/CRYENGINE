// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Game side definitions for lobby user packets

-------------------------------------------------------------------------
History:
- 12:03:2010 : Created By Ben Parbury

*************************************************************************/

#include <CryLobby/ICryLobby.h>
#include <CryLobby/CryLobbyPacket.h>

#pragma once

enum GameUserPacketDefinitions
{
	eGUPD_LobbyStartCountdownTimer = CRYLOBBY_USER_PACKET_START,
	eGUPD_LobbyGameHasStarted,
	eGUPD_LobbyEndGame,
	eGUPD_LobbyEndGameResponse,
	eGUPD_LobbyUpdatedSessionInfo,
	eGUPD_LobbyMoveSession,
	eGUPD_SquadJoin,
	eGUPD_SquadJoinGame,
	eGUPD_SquadNotInGame,
	eGUPD_SetTeamAndRank,
	eGUPD_SendChatMessage,								// Clients request to send a message to other players
	eGUPD_ChatMessage,										// Server sent message to all appropriate other players.
	eGUPD_ReservationRequest,							// Sent by squad leader client after joined game to identify self as leader and to tell the game server to reserve slots for its members
	eGUPD_ReservationClientIdentify,			// Sent by clients after joined game to identify self to game server so it can check if client passes reserve checks (if any)
	eGUPD_ReservationsMade,								// Sent to a squad leader by a server when requested reservations have been successfully made upon receipt of a eGUPD_ReservationRequest packet 
	eGUPD_ReservationFailedSessionFull,		// Can be sent to clients when there's no room for them in a game session. Generally causes them to "kick" themselves by deleting their own session
	eGUPD_SyncPlaylistContents,						// Sync entire playlist
	eGUPD_SetGameVariant,
	eGUPD_SyncPlaylistRotation,
	eGUPD_SquadLeaveGame,									// Squad: Tell all members in the squad to leave (game host will leave last)
	eGUPD_SquadNotSupported,							// Squads not suported in current gamemode
	eGUPD_UpdateCountdownTimer,
	eGUPD_RequestAdvancePlaylist,
	eGUPD_SyncExtendedServerDetails,
	eGUPD_DetailedServerInfoRequest,
	eGUPD_DetailedServerInfoResponse,
	eGUPD_SquadDifferentVersion,					// Response to SquadJoin packet sent when the client is on a different patch
	eGUPD_SquadKick,
	eGUPD_SquadMerge,
	eGUPD_SetupDedicatedServer,
	eGUPD_SetupJoinCommand,
	eGUPD_SetAutoStartingGame,
	eGUPD_SetupArbitratedLobby,
	eGUPD_TeamBalancingSetup,
	eGUPD_TeamBalancingAddPlayer,
	eGUPD_TeamBalancingRemovePlayer,
	eGUPD_TeamBalancingUpdatePlayer,

	eGUPD_End
};

