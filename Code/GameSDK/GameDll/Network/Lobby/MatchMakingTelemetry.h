// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Class for recording and sending matchmaking data.
Aim is to record the data we used to rate the available sessions and the session we chose
(so we can later find out if we are making good decisions)
and also data on the actual performance of the session
(so we can see if our decisions are based on good data).
-------------------------------------------------------------------------
History:
- 02:06:2011 : Created By Andrew Blackwell

*************************************************************************/

//////////////////////////////////////////////////////////////////////////
//Header Guard
#ifndef __MATCHMAKINGTELEMETRY_H__
#define __MATCHMAKINGTELEMETRY_H__

//////////////////////////////////////////////////////////////////////////
//Base Class includes
#include "GameRulesModules/IGameRulesClientConnectionListener.h"
#include "ITelemetryCollector.h"


//////////////////////////////////////////////////////////////////////////
// Events Include
#include "MatchMakingEvents.h"

enum EMMTelEventPacket
{
	eMMTelE_StartSearch = eRBPT_Custom,
	eMMTelE_FoundSession,
	eMMTelE_ChosenSession,
	eMMTelE_NoServerSelected,		//for when we look for servers, but none are better than our own session
	eMMTelE_SearchTimedOut,
	eMMTelE_ServerConnectFailed,
	eMMTelE_MigrateHostLobby,
	eMMTelE_BecomeServer,
	eMMTelE_DemotedToClient,
	eMMTelE_MigrateCompleted,
	eMMTelE_ServerRequestMerge,
	eMMTelE_MergeRequested,
	eMMTelE_LaunchGame,
	eMMTelE_LeaveMatchMaking,
	eMMTelE_PlayerJoinedMM,
	eMMTelE_PlayerLeftMM,
	eMMTelE_PlayerJoined,	
	eMMTelE_PlayerLeft,
	eMMTelE_PlayerPing,
	eMMTelE_PlayerReportLag,
	eMMTelE_GenericLog,
	eMMTelE_MigrationClientAvailable,
	eMMTelE_MigrationClientChosen,
};

class CMatchmakingTelemetry : public IGameRulesClientConnectionListener
{
public:
	CMatchmakingTelemetry();
	virtual ~CMatchmakingTelemetry();

	enum EMMTelRetVal
	{
		eMMTelRV_Success,
		eMMTelRV_ErrorDispatchBlocked,
		eMMTelRV_ErrorWrongState,
		eMMTelRV_ErrorOutOfMem,
		eMMTelRV_WarnDataIncomplete
	};

	enum EMMTelState
	{
		eMMTelState_Clear,						
		eMMTelState_Writing,
		eMMTelState_WaitingForDispatch,
	};

	enum EMMTelTranscriptType
	{
		eMMTelTranscript_None,
		eMMTelTranscript_QuickMatch,
		eMMTelTranscript_MidGameMigration,
		eMMTelTranscript_PostGameMigration,
		eMMTelTranscript_SessionQuality
	};

	//typical flow for the transaction types:

	//QuickMatch
	//add any existing players (include if they're in a squad)
	//repeat
		//Add the requested search parameters
		//add any sessions we find
		//record selected session
		//record players who we meet or who join or leave the session
	//record when we start a match or abandon matchmaking

	//In game Migration
	//Record the host who disappeared (if known)
	//record the available clients
	//record new selected host

	//Post match Migration (host only)
	//record the available clients
	//record the selected host (could be self)

	//Session Quality
	//record all the players in the session as they are created/join
	//regularly record their pings
	//record players when they leave


	EMMTelRetVal					BeginMatchMakingTranscript( EMMTelTranscriptType type );
	EMMTelRetVal					EndMatchmakingTranscript( EMMTelTranscriptType transaction, bool logLocally );

	void									AddEvent( const SMatchMakingEvent& event );

	EMMTelTranscriptType	GetCurrentTranscriptType() const { return m_currentTranscript; }

	// IGameRulesClientConnectionListener
	virtual void OnClientConnect(int channelId, bool isReset, EntityId playerId) { CryLog("MMT: OnClientConnect"); }
	virtual void OnClientDisconnect(int channelId, EntityId playerId);
	virtual void OnClientEnteredGame(int channelId, bool isReset, EntityId playerId);
	virtual void OnOwnClientEnteredGame();
	// ~IGameRulesClientConnectionListener

private:
	void					CollectPings();

	CRecordingBuffer		*m_pBuffer;

	EMMTelTranscriptType m_currentTranscript;

};

class CMMTelemetryProducer : public ITelemetryProducer
{
public:
	CMMTelemetryProducer( CRecordingBuffer* pInBuffer );
	virtual ~CMMTelemetryProducer();

	virtual EResult	ProduceTelemetry( char *pOutBuffer, int inMinRequired, int inBufferSize, int *pOutWritten );

private:

	typedef CryFixedStringT<256> EventDataString;

	void OutputStartSearchData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputFoundSessionData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputChosenSessionData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputServerConnectFailedData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputNoServerSelectedData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputSearchTimedOutData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputMigrateHostLobbyData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputBecomeHostData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputDemotedData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputMigrateCompletedData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputServerRequestingMergeData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputMergeRequestedData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputLaunchGameData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputLeaveMatchMakingData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputPlayerJoinedMMData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputPlayerLeftMMData( SRecording_Packet& eventPacket, EventDataString& entryString );
	
	void OutputPlayerPingData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputPlayerLeftData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputPlayerJoinedData( SRecording_Packet& eventPacket, EventDataString& entryString );
	void OutputPlayerReportLagData( SRecording_Packet& eventPacket, EventDataString& entryString );

	void OutputGenericLogData( SRecording_Packet& eventPacket, EventDataString& entryString );

	CRecordingBuffer*						m_pDataBuffer;
	CRecordingBuffer::iterator	m_currentElement;
};

#endif	//__MATCHMAKINGTELEMETRY_H__

