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

#include "StdAfx.h"
//////////////////////////////////////////////////////////////////////////
// This Include
#include "MatchMakingTelemetry.h"

#include "TelemetryCollector.h"
#include "GameLobby.h"
#include "GameLobbyData.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "RecordingBuffer.h"

#include "PlaylistManager.h"

#define MATCHMAKING_BUFFER_SIZE		(4*1024)

//-------------------------------------------------------------------------
//Matchmaking Telemetry main class/event collector section

//-------------------------------------------------------------------------
//Constructor
CMatchmakingTelemetry::CMatchmakingTelemetry()
{
	m_pBuffer = NULL;
	m_currentTranscript = eMMTelTranscript_None;
}

//--------------------------------------------------------------------------
//Destructor
CMatchmakingTelemetry::~CMatchmakingTelemetry()
{
	if( m_pBuffer && m_pBuffer->size() != 0 )
	{
		CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_COMMENT, "Destroying Matchmaking Telemetry object with non-transfered data present" );
	}

	//Need to check if there's any data in transit to telemetry servers here and halt it
	
	SAFE_DELETE( m_pBuffer );
}

//-------------------------------------------------------------------------
CMatchmakingTelemetry::EMMTelRetVal CMatchmakingTelemetry::BeginMatchMakingTranscript( EMMTelTranscriptType type )
{
	CRY_ASSERT( m_currentTranscript == eMMTelTranscript_None );
	m_currentTranscript = type;

	if( m_pBuffer == NULL )
	{
		m_pBuffer = new CRecordingBuffer( MATCHMAKING_BUFFER_SIZE );
		m_pBuffer->SetPacketDiscardCallback( NULL, NULL );
		return eMMTelRV_Success;
	}
	else
	{
		return eMMTelRV_ErrorWrongState;
	}
}

//-------------------------------------------------------------------------
CMatchmakingTelemetry::EMMTelRetVal CMatchmakingTelemetry::EndMatchmakingTranscript( EMMTelTranscriptType transcript, bool logLocally )
{
	EMMTelRetVal retVal = eMMTelRV_Success;

	//TODO: Decide on valid ways to use Begin/End Transcript, and promote comments to warnings if these are invalid
	if( transcript != m_currentTranscript )
	{
		CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_COMMENT, "MMTel: Tried to close a type %d transcript block but current type is %d", transcript, m_currentTranscript );
		retVal = eMMTelRV_ErrorWrongState;
	}
	else if( m_pBuffer == NULL || m_pBuffer->size() == 0 )
	{
		//there is actually no data to send
		CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_COMMENT, "MMTel: When close a type %d transcript block there was no data to send", m_currentTranscript );
		retVal = eMMTelRV_ErrorWrongState;
	}
	else
	{
		//pass our buffer to our telemetry producer, the producer takes ownership of it

		CTelemetryCollector *tc=static_cast<CTelemetryCollector*>(static_cast<CGame*>(g_pGame)->GetITelemetryCollector());
		ITelemetryProducer *pProducer = new CMMTelemetryProducer( m_pBuffer );

		char* nameBase;

		switch( transcript )
		{
		case eMMTelTranscript_QuickMatch:
			nameBase = "matchmakingdata";
			break;
		case eMMTelTranscript_SessionQuality:
			nameBase = "sessionperformance";
			break;
		default:
			nameBase = "unknowndata";
		}

		if( logLocally )
		{
			CryFixedStringT<24> localFilename;
			localFilename.Format( "%s%d.xml", nameBase, gEnv->pSystem->GetApplicationInstance() );

			CTelemetrySaveToFile* pLogger = new CTelemetrySaveToFile( pProducer, localFilename.c_str() );
			pProducer = pLogger;
		}

		CryFixedStringT<24> remoteFilename;
		remoteFilename.Format( "%s.xml", nameBase );

		tc->SubmitTelemetryProducer( pProducer, remoteFilename.c_str() );

		//clear out buffer pointer
		m_pBuffer = NULL;
		m_currentTranscript = eMMTelTranscript_None;
	}

	CryLog( "MMT: End Transcript call finished" );

	return retVal;
}

//-------------------------------------------------------------------------
void CMatchmakingTelemetry::AddEvent( const SMatchMakingEvent& event )
{
	if( m_pBuffer )
	{
		m_pBuffer->AddPacket( event );
	}
}

//-------------------------------------------------------------------------
void CMatchmakingTelemetry::OnOwnClientEnteredGame()
{
	//called on every client before the game starts
#if defined(TRACK_MATCHMAKING)
	CryLog("MMT: End Quick match Telemetry"); 
	EndMatchmakingTranscript( eMMTelTranscript_QuickMatch, true );
#endif
}

//-------------------------------------------------------------------------
void CMatchmakingTelemetry::OnClientEnteredGame( int channelId, bool isReset, EntityId playerId )
{
//called on the server for every player. called after OnOwnClientEnteredGame

	if( m_currentTranscript == eMMTelTranscript_None )
	{
		CryLog("MMT: Start Session Quality Telemetry");
		BeginMatchMakingTranscript( eMMTelTranscript_SessionQuality );
	}

	//tell the Telemetry System this player has joined so we know to record their session performance
	CGameLobby* pGameLobby = g_pGame->GetGameLobby();
	if( pGameLobby != NULL && pGameLobby->GetState() == eLS_Game )
	{
		SCryMatchMakingConnectionUID conUID = pGameLobby->GetConnectionUIDFromChannelID( channelId );
		CryUserID guid = pGameLobby->GetUserIDFromChannelID( channelId );

		CryFixedStringT<CRYLOBBY_USER_NAME_LENGTH> name;

		pGameLobby->GetPlayerNameFromChannelId( channelId, name );

		AddEvent( SMMPlayerJoinedEvent( name, guid, conUID ) );
	}
}

//-------------------------------------------------------------------------
void CMatchmakingTelemetry::OnClientDisconnect( int channelId, EntityId playerId )
{
	CryLog("MMT: OnClienDisconnect");

	//tell the Telemetry System this player has left so we know to not expect anymore session performance records
	CGameLobby* pGameLobby = g_pGame->GetGameLobby();
	if( pGameLobby )
	{
		SCryMatchMakingConnectionUID conUID = pGameLobby->GetConnectionUIDFromChannelID( channelId );
		CryUserID guid = pGameLobby->GetUserIDFromChannelID( channelId );
		AddEvent( SMMPlayerLeftEvent( guid, conUID ) );
	}
}

//-------------------------------------------------------------------------
//Matchmaking Telemetry Producer Section

//-------------------------------------------------------------------------
//Constructor
CMMTelemetryProducer::CMMTelemetryProducer( CRecordingBuffer* pInBuffer )
:	m_pDataBuffer( pInBuffer ),
	m_currentElement( pInBuffer->begin() )
{
}

//-------------------------------------------------------------------------
//Destructor
CMMTelemetryProducer::~CMMTelemetryProducer()
{
	//delete the buffer we took ownership of
	SAFE_DELETE( m_pDataBuffer );
}

//-------------------------------------------------------------------------
ITelemetryProducer::EResult CMMTelemetryProducer::ProduceTelemetry( char *pOutBuffer, int inMinRequired, int inBufferSize, int *pOutWritten )
{
	ITelemetryProducer::EResult retVal = eTS_Available;
	uint32 writtingIndex = 0;
	//change to unsigned for signed/unsigned mis-match protection
	uint32 bufferSize = inBufferSize;

	EventDataString entryString;
	CRecordingBuffer::iterator end = m_pDataBuffer->end();
	bool writtenEnough = false;


	if( m_currentElement == m_pDataBuffer->begin() )
	{
		//open a top level XML tag
		entryString.Format( "<MatchMakingTelemetryXML>\n" );
		if( (writtingIndex + entryString.length()) < bufferSize )
		{
			memcpy( pOutBuffer + writtingIndex, entryString.c_str(), entryString.length() );
			writtingIndex += entryString.length();
		}
	}

	while( !writtenEnough && m_currentElement != end )
	{
		switch( m_currentElement->type )
		{
		case eMMTelE_StartSearch:					
															OutputStartSearchData( *m_currentElement, entryString );
															break;
		case eMMTelE_FoundSession:				
															OutputFoundSessionData( *m_currentElement, entryString );
															break;
		case eMMTelE_ChosenSession:				
															OutputChosenSessionData( *m_currentElement, entryString );
															break;
		case eMMTelE_NoServerSelected:
															OutputNoServerSelectedData( *m_currentElement, entryString );
															break;
		case eMMTelE_SearchTimedOut:
															OutputSearchTimedOutData( *m_currentElement, entryString );
															break;
		case eMMTelE_ServerConnectFailed:	
															OutputServerConnectFailedData( *m_currentElement, entryString );
															break;
		case eMMTelE_MigrateHostLobby:	
															OutputMigrateHostLobbyData( *m_currentElement, entryString );
															break;
		case eMMTelE_BecomeServer:	
															OutputBecomeHostData( *m_currentElement, entryString );
															break;
		case eMMTelE_DemotedToClient:	
															OutputDemotedData( *m_currentElement, entryString );
															break;
		case eMMTelE_MigrateCompleted:
															OutputMigrateCompletedData( *m_currentElement, entryString );
															break;
		case eMMTelE_ServerRequestMerge:
															OutputServerRequestingMergeData( *m_currentElement, entryString );
															break;
		case eMMTelE_MergeRequested:	
															OutputMergeRequestedData( *m_currentElement, entryString );
															break;
		case eMMTelE_LaunchGame:	
															OutputLaunchGameData( *m_currentElement, entryString );			
															break;
		case eMMTelE_LeaveMatchMaking:	
															OutputLeaveMatchMakingData( *m_currentElement, entryString );
															break;
		case eMMTelE_PlayerJoinedMM:				
															OutputPlayerJoinedMMData( *m_currentElement, entryString );
															break;
		case eMMTelE_PlayerLeftMM:				
															OutputPlayerLeftMMData( *m_currentElement, entryString );
															break;
		case eMMTelE_PlayerJoined:				
															OutputPlayerJoinedData( *m_currentElement, entryString );
															break;
		case eMMTelE_PlayerLeft:					
															OutputPlayerLeftData( *m_currentElement, entryString );
															break;
		case eMMTelE_PlayerPing:					
															OutputPlayerPingData( *m_currentElement, entryString );
															break;
		case eMMTelE_PlayerReportLag:
															OutputPlayerReportLagData( *m_currentElement, entryString );
															break;

		case eMMTelE_GenericLog:
															OutputGenericLogData( *m_currentElement, entryString );
															break;

		//Still Need to support these types:
		//eMMTelE_MigrationClientAvailable:
		//eMMTelE_MigrationClientChosen:
		
		default:			//not a type we have data for
									entryString.clear();
		}

		//check we have space then add the data to the buffer
		if( (writtingIndex + entryString.length()) < bufferSize )
		{
			memcpy( pOutBuffer + writtingIndex, entryString.c_str(), entryString.length() );
			writtingIndex += entryString.length();

			//successfully written element, move onto next
			++m_currentElement;
		}
		else
		{
			CRY_ASSERT_MESSAGE( (int)writtingIndex >= inMinRequired, "MatchMaking Telemetry event is too large to send" );
			writtenEnough = true;
		}
	}

	if( m_currentElement == end )
	{
		//close the top level XML tag
		entryString.Format( "</MatchMakingTelemetryXML>" );

		//check we're not going to run out of space
		if( (writtingIndex + entryString.length()) < bufferSize )
		{
			memcpy( pOutBuffer + writtingIndex, entryString.c_str(), entryString.length() );
			writtingIndex += entryString.length();
			retVal = eTS_EndOfStream;
		}
	}

	*pOutWritten = writtingIndex;

	return retVal;
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputStartSearchData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMStartSearchEvent& searchEvent = reinterpret_cast<SMMStartSearchEvent&>( eventPacket );

	entryString.Format( "<searchParams searchID=\"%d\" numFreeSlots=\"%d\" ranked=\"%d\" ", searchEvent.m_searchID, searchEvent.m_numFreeSlots, searchEvent.m_ranked );
	
	EventDataString row;		//needs to be same type and size as dest string
	if( searchEvent.m_version != -1 )
	{
		row.Format( "version=\"%d\" ",  searchEvent.m_version );
		entryString += row;
	}

	if( CPlaylistManager* pPlaylistmanager = g_pGame->GetPlaylistManager() )
	{
		if( searchEvent.m_playlist != -1 )
		{
			row.Format( "playlist=\"%s\" ",   pPlaylistmanager->GetPlaylistNameById(searchEvent.m_playlist) );
			entryString += row;
		}

		if( searchEvent.m_variant != -1 )
		{
			row.Format( "variant=\"%s\" ",  pPlaylistmanager->GetVariantName(searchEvent.m_variant) );
			entryString += row;
		}
	}

	if( searchEvent.m_searchRegion != -1 )
	{
		row.Format( "searchRegion=\"%d\" ",  searchEvent.m_variant );
		entryString += row;
	}
	else
	{
		row.Format( "regionlessSearch=\"1\" " );
		entryString += row;
	}

	row.Format( "timestamp=\"%" PRIi64 "\" />\n", searchEvent.m_timeStamp.GetMilliSecondsAsInt64() );
	entryString += row;

}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputFoundSessionData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMFoundSessionEvent& sessionEvent = reinterpret_cast<SMMFoundSessionEvent&>( eventPacket );

	const char* pStatusName = NULL;

	switch( sessionEvent.m_status )
	{
	case CGameLobby::eAS_Lobby:
											pStatusName = "Lobby";
											break;
	case CGameLobby::eAS_Game:
											pStatusName = "InGame";
											break;
	case CGameLobby::eAS_EndGame:
											pStatusName = "EndGame";
											break;
	case CGameLobby::eAS_StartingGame:
											pStatusName = "StartGame";
											break;
	}

	entryString.Format( "<foundSession serverName=\"%s\" sessionId=\"%s\" ping=\"%d\" filledSlots=\"%d\" status=\"%s\" rankDifference=\"%d\" region=\"%d\" language=\"%d\" badServer=\"%d\" c2mmscore=\"%.3f\" timestamp=\"%" PRIi64 "\" />\n",
			sessionEvent.m_sessionName, sessionEvent.m_sessionID, sessionEvent.m_ping, sessionEvent.m_filledSlots,
			pStatusName, sessionEvent.m_rankDiff, sessionEvent.m_region, sessionEvent.m_language, sessionEvent.m_badServer,
			sessionEvent.m_score, sessionEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputChosenSessionData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMChosenSessionEvent& sessionEvent = reinterpret_cast<SMMChosenSessionEvent&>( eventPacket );

	char* tagName;

	if( sessionEvent.m_created )
	{
		tagName = "createdServer";
	}
	else
	{
		tagName = "chosenServer";
	}
	
	entryString.Format( "<%s searchID=\"%d\" rulesUsed=\"%s\" serverName=\"%s\" sessionId=\"%s\" isPrimary=\"%d\" timestamp=\"%" PRIi64 "\" />\n",
														tagName, sessionEvent.m_searchID, sessionEvent.m_rulesDescription, sessionEvent.m_sessionName, 
														sessionEvent.m_sessionID, sessionEvent.m_primary, sessionEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputServerConnectFailedData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMServerConnectFailedEvent& failedEvent = reinterpret_cast<SMMServerConnectFailedEvent&>( eventPacket );
	entryString.Format( "<failedToConnectToServer sessionId=\"%s\" primarySession=\"%d\" errorCode=\"%d\" timestamp=\"%" PRIi64 "\" />\n",
		failedEvent.m_sessionID, failedEvent.m_wasPrimary, failedEvent.m_errorCode, failedEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputNoServerSelectedData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMNoServerSelectedEvent& noJoinEvent = reinterpret_cast<SMMNoServerSelectedEvent&>( eventPacket );
	entryString.Format( "<noServerSelected searchID=\"%d\" reason=\"%s\" timestamp=\"%" PRIi64 "\" />\n", noJoinEvent.m_searchID, noJoinEvent.m_reason, noJoinEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputSearchTimedOutData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMSearchTimedOutEvent& timedoutEvent = reinterpret_cast<SMMSearchTimedOutEvent&>( eventPacket );
	entryString.Format( "<searchTimedOut searchID=\"%d\" searchingAgain=\"%d\" timestamp=\"%" PRIi64 "\" />\n", timedoutEvent.m_searchID, timedoutEvent.m_searchingAgain, timedoutEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputMigrateHostLobbyData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMMigrateHostLobbyEvent& promoteEvent = reinterpret_cast<SMMMigrateHostLobbyEvent&>( eventPacket );
	entryString.Format( "<startMigrateHostInLobby timestamp=\"%" PRIi64 "\" />\n", promoteEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputBecomeHostData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMBecomeHostEvent& promoteEvent = reinterpret_cast<SMMBecomeHostEvent&>( eventPacket );
	entryString.Format( "<promotedToHost timestamp=\"%" PRIi64 "\" />\n", promoteEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputDemotedData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMBecomeHostEvent& demoteEvent = reinterpret_cast<SMMBecomeHostEvent&>( eventPacket );
	entryString.Format( "<demotedToClient timestamp=\"%" PRIi64 "\" />\n", demoteEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputMigrateCompletedData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMMigrateCompletedEvent& migrateEvent = reinterpret_cast<SMMMigrateCompletedEvent&>( eventPacket );
	entryString.Format( "<migrationCompleted newHostName=\"%s\" newSessionId=\"%s\" timestamp=\"%" PRIi64 "\" />\n", migrateEvent.m_newServer, migrateEvent.m_newSessionID, migrateEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputServerRequestingMergeData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMServerRequestingMerge& mergeEvent = reinterpret_cast<SMMServerRequestingMerge&>( eventPacket );
	entryString.Format( "<requestingMerge currentSessionId=\"%s\" newSessionId=\"%s\" timestamp=\"%" PRIi64 "\" />\n",
		mergeEvent.m_currentSessionID, mergeEvent.m_newSessionID, mergeEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputMergeRequestedData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMMergeRequestedEvent& mergeEvent = reinterpret_cast<SMMMergeRequestedEvent&>( eventPacket );
	entryString.Format( "<mergeRequested newSessionId=\"%s\" timestamp=\"%" PRIi64 "\" />\n", mergeEvent.m_sessionID, mergeEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputLaunchGameData( SRecording_Packet& eventPacket, EventDataString& entryString )		
{
	SMMLaunchGameEvent& launchEvent = reinterpret_cast<SMMLaunchGameEvent&>( eventPacket );
	entryString.Format( "<matchStarted sessionId=\"%s\" timestamp=\"%" PRIi64 "\" />\n", launchEvent.m_sessionID, launchEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputLeaveMatchMakingData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMLeaveMatchMakingEvent& leaveEvent = reinterpret_cast<SMMLeaveMatchMakingEvent&>( eventPacket );
	entryString.Format( "<leftMatchMaking timestamp=\"%" PRIi64 "\" />\n", leaveEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputPlayerJoinedMMData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMPlayerJoinedMMEvent& playerEvent = reinterpret_cast<SMMPlayerJoinedMMEvent&>( eventPacket );
	entryString.Format( "<playerJoined guid=\"%s\" username=\"%s\" sessionId=\"%s\" numCurrentPlayers=\"%d\" isLocal=\"%d\" timestamp = \"%" PRIi64 "\" />\n",
						playerEvent.m_guid, playerEvent.m_userName, playerEvent.m_sessionID, playerEvent.m_nCurrentPlayers,
						playerEvent.m_local, playerEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputPlayerLeftMMData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMPlayerLeftMMEvent& playerEvent = reinterpret_cast<SMMPlayerLeftMMEvent&>( eventPacket );
	entryString.Format( "<playerLeft guid=\"%s\" username=\"%s\" sessionId=\"%s\" numCurrentPlayers=\"%d\" isLocal=\"%d\" timestamp=\"%" PRIi64 "\" />\n",
		playerEvent.m_guid, playerEvent.m_userName, playerEvent.m_sessionID, playerEvent.m_nCurrentPlayers,
		playerEvent.m_local, playerEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputPlayerJoinedData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMPlayerJoinedEvent& playerEvent = reinterpret_cast<SMMPlayerJoinedEvent&>( eventPacket );

	entryString.Format( "<playerJoined GUID=\"%s\" conUID=\"%d\" name=\"%s\" timestamp=\"%" PRIi64 "\" />\n",
		playerEvent.m_guid, playerEvent.m_conUID.m_uid, playerEvent.m_name, playerEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputPlayerLeftData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMPlayerLeftEvent& playerEvent = reinterpret_cast<SMMPlayerLeftEvent&>( eventPacket );
	entryString.Format( "<playerLeft GUID=\"%s\" conUID=\"%d\" timestamp=\"%" PRIi64 "\" />\n",
		playerEvent.m_guid, playerEvent.m_conUID.m_uid, playerEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputPlayerPingData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMPlayerPingEvent& pingEvent = reinterpret_cast<SMMPlayerPingEvent&>( eventPacket );
	entryString.Format( "<playerPing GUID=\"%s\" conUID=\"%d\" ping=\"%d\" timestamp=\"%" PRIi64 "\" />\n",
		pingEvent.m_guid, pingEvent.m_conUID.m_uid, pingEvent.m_ping, pingEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

//-------------------------------------------------------------------------
void CMMTelemetryProducer::OutputPlayerReportLagData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMPlayerReportLagEvent& lagEvent = reinterpret_cast<SMMPlayerReportLagEvent&>( eventPacket );
	entryString.Format( "<playerReportLag GUID=\"%s\" timestamp=\"%" PRIi64 "\" />\n", lagEvent.m_guid, lagEvent.m_timeStamp.GetMilliSecondsAsInt64() );
}

void CMMTelemetryProducer::OutputGenericLogData( SRecording_Packet& eventPacket, EventDataString& entryString )
{
	SMMGenericLogEvent& logEvent = reinterpret_cast<SMMGenericLogEvent&>( eventPacket );

	if( logEvent.m_bError )
	{
		entryString.Format( "<error message=\"%s\" />\n", logEvent.m_message );
	}
	else
	{
		entryString.Format( "<log message=\"%s\" />\n", logEvent.m_message );
	}

}


