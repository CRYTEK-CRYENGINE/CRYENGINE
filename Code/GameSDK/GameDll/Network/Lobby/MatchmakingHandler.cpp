// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Class for interface between C++ and Lua for MatchMaking
-------------------------------------------------------------------------
History:
- 01:08:2011 : Created By Andrew Blackwell

*************************************************************************/

#include "StdAfx.h"
//////////////////////////////////////////////////////////////////////////
// This Include
#include "MatchMakingHandler.h"

#include "GameLobby.h"
#include "GameLobbyManager.h"
#include "GameLobbyCVars.h"
#include "MatchMakingTelemetry.h"
#include "MatchMakingEvents.h"
#include "GameBrowser.h"
#include "ScriptBind_MatchMaking.h"

#include "GameCVars.h"


int CMatchMakingHandler::s_currentMMSearchID = 0;

//------------------------------------------------------------------------
//Constructor
CMatchMakingHandler::CMatchMakingHandler()
:	m_pScript( gEnv->pScriptSystem, true ),
	m_sessionIdIndex( 0 ),
	m_bActive( false ),
	m_nSessionParams( 0 ),
	m_bIsMerging( false ),
	m_startTime( 0.0f ),
	m_subscribedUpdateTimer( -1.0f )
{
	ClearSessionParameters();
}


//------------------------------------------------------------------------
//Destructor
CMatchMakingHandler::~CMatchMakingHandler()
{
}

//------------------------------------------------------------------------
bool CMatchMakingHandler::LoadScript()
{
	bool retval = true;
	if( gEnv->pScriptSystem )
	{
		// Load matchmaking script.
		retval = gEnv->pScriptSystem->ExecuteFile( "scripts/Matchmaking/matchmaking.lua", true, true );

		if( retval )
		{
			retval = gEnv->pScriptSystem->GetGlobalValue( "MatchMaking", m_pScript );
		}
	}

	return retval;
}

//------------------------------------------------------------------------
void CMatchMakingHandler::OnInit( CGameLobbyManager* pLobbyManager )
{
	g_pGame->GetMatchMakingScriptBind()->AttachTo( this, pLobbyManager );

	for( int i = 0; i < k_maxTasks; i++ )
	{
		m_waitingTasksQueue[i].taskType = eMMHT_None;
		m_waitingTasksQueue[i].taskSuccess = false;
	}

	m_currentTask = m_newTask = 0;

	m_bIsMerging = false;
	m_nSessionParams = 0;

	HSCRIPTFUNCTION scriptFunction;
	if( m_pScript->GetValue( "OnInit", scriptFunction ) )  
	{
		Script::Call( gEnv->pScriptSystem, scriptFunction, m_pScript );
		gEnv->pScriptSystem->ReleaseFunc( scriptFunction );
	}
}

//------------------------------------------------------------------------
void CMatchMakingHandler::OnEnterMatchMaking()
{
	if( m_bActive )
		return;

	LoadScript();
	OnInit( g_pGame->GetGameLobbyManager() );

	HSCRIPTFUNCTION scriptFunction;
	if( m_pScript->GetValue( "OnEnterMatchMaking", scriptFunction ) )  
	{
		Script::Call( gEnv->pScriptSystem, scriptFunction, m_pScript );
		gEnv->pScriptSystem->ReleaseFunc( scriptFunction );
	}

	m_startTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	m_bActive = true;
	m_subscribedUpdateTimer = -1.0f;
}

//------------------------------------------------------------------------
void CMatchMakingHandler::OnLeaveMatchMaking()
{
	CryLog( "MMLua: Leaving matchmaking" );

	if( !m_bActive )
		return;

	HSCRIPTFUNCTION scriptFunction;
	if( m_pScript->GetValue( "OnLeaveMatchMaking", scriptFunction ) )  
	{
		Script::Call( gEnv->pScriptSystem, scriptFunction, m_pScript );
		gEnv->pScriptSystem->ReleaseFunc( scriptFunction );
	}

	m_sessionIdMap.clear();

	//remove our reference to the script table so it can be released
	m_pScript = NULL;
	m_bActive = false;
}

//------------------------------------------------------------------------
void CMatchMakingHandler::Search( int freeSlots, int maxResults, SCrySessionSearchData* searchParameters, int numSearchParameters )
{
	//might still want equivalents of these
	//m_findGameTimeout = GetFindGameTimeout();
	//m_findGameResults.clear();

	SCrySessionSearchParam param;

	param.m_type = FIND_GAME_SESSION_QUERY;

	param.m_data = searchParameters;
	param.m_numFreeSlots = freeSlots;
	CRY_ASSERT(param.m_numFreeSlots > 0);
	param.m_maxNumReturn = maxResults;
	param.m_ranked = false;

	int curData = 0;

	CRY_ASSERT_MESSAGE( numSearchParameters < FIND_GAMES_SEARCH_NUM_DATA, "Session search data buffer overrun" );
	searchParameters[ numSearchParameters ].m_operator = eCSSO_Equal;
	searchParameters[ numSearchParameters ].m_data.m_id = LID_MATCHDATA_VERSION;
	searchParameters[ numSearchParameters ].m_data.m_type = eCLUDT_Int32;
	searchParameters[ numSearchParameters ].m_data.m_int32 = GameLobbyData::GetVersion();
	numSearchParameters++;

	param.m_numData = numSearchParameters;

	++s_currentMMSearchID;
#if defined(TRACK_MATCHMAKING)
	if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
	{
		pMMTel->AddEvent( SMMStartSearchEvent( param, s_currentMMSearchID ) );
	}
#endif

	ECryLobbyError result = g_pGame->GetGameBrowser()->StartSearchingForServers(&param, CMatchMakingHandler::SearchCallback, this, false);
	if (result == eCLE_Success)
	{
		CryLog("MatchMakingHandler::Search() search successfully started, ");//setting s_bShouldBeSearching to FALSE to prevent another one starting");
	}
	else
	{
		CryLog("MatchMakingHandler::Search() search failed to start (error=%i)", result);// setting s_bShouldBeSearching to TRUE so we start another one when the timeout occurs", result);
	}
	
}

//------------------------------------------------------------------------
void CMatchMakingHandler::CancelSearch()
{
	if( CGameBrowser *pGameBrowser = g_pGame->GetGameBrowser() )
	{
		pGameBrowser->CancelSearching();
	}
}

//------------------------------------------------------------------------
void CMatchMakingHandler::SearchCallback( CryLobbyTaskID taskID, ECryLobbyError error, SCrySessionSearchResult* session, void* arg )
{
	CMatchMakingHandler* pThis = (CMatchMakingHandler*)(arg);
	
	if( session )
	{
		pThis->OnSearchResult( session );
	}

	if( (error != eCLE_SuccessContinue) && (error != eCLE_SuccessUnreachable) )
	{
#if defined(TRACK_MATCHMAKING)
		if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
		{
			pMMTel->AddEvent( SMMSearchTimedOutEvent( true, s_currentMMSearchID ) );
		}
#endif		

		//search done, inform the Lua next update
		pThis->AddWaitingTask( eMMHT_EndSearch, true );
	}
}

//------------------------------------------------------------------------
void CMatchMakingHandler::OnSearchResult( SCrySessionSearchResult* pSession )
{
	//session will expire at the end of the callback
	//so copy it into a results structure (we need lots of details to pass to the matchmaking part
	//store it in a map (indexed via sessionID?)
	//pass the index to the lua


	CGameLobbyManager* pLobbyManager = g_pGame->GetGameLobbyManager();
	if( pLobbyManager )
	{
		CGameLobby* pLobby = pLobbyManager->GetGameLobby();
		
		//first check if this result refers to a lobby we're in/hosting
		if( pLobby->IsCurrentSessionId( pSession->m_id ) )
		{
			//this is the session for the lobby we are in, early out
			return;
		}
		//also check secondary lobby
		if( CGameLobby* pSecondaryLobby = pLobbyManager->GetNextGameLobby() )	
		{
			if( ( pSecondaryLobby->IsCurrentSessionId( pSession->m_id ) ) )
			{
				//this is the session for the lobby we are going to, early out
				return;
			}
		}
		
		const CGameLobby::EActiveStatus activeStatus = (CGameLobby::EActiveStatus) GameLobbyData::GetSearchResultsData( pSession, LID_MATCHDATA_ACTIVE );
		bool bIsBadServer = pLobby->IsBadServer( pSession->m_id );
		const int skillRank = pLobby->CalculateAverageSkill();
		const int sessionSkillRank = GameLobbyData::GetSearchResultsData( pSession, LID_MATCHDATA_SKILL );
		const int sessionLanguageId = GameLobbyData::GetSearchResultsData( pSession, LID_MATCHDATA_LANGUAGE );

		float sessionScore = LegacyC2MatchMakingScore( pSession, pLobby, false );

		int32 region = 0;
#if GAMELOBBY_USE_COUNTRY_FILTERING
		region = GameLobbyData::GetSearchResultsData( pSession, LID_MATCHDATA_COUNTRY );
#endif //GAMELOBBY_USE_COUNTRY_FILTERING

#if defined(TRACK_MATCHMAKING)
		if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
		{
			pMMTel->AddEvent( SMMFoundSessionEvent( pSession, activeStatus, skillRank - sessionSkillRank, region, sessionLanguageId, bIsBadServer, sessionScore ) );
		}
#endif //defined(TRACK_MATCHMAKING)

		HSCRIPTFUNCTION scriptFunction;

		if( m_pScript->GetValue( "OnSearchResult", scriptFunction ) )  
		{
			//Make a table to hold the search result
			SmartScriptTable result( gEnv->pScriptSystem );

			SessionDetails newSession;
			newSession.m_id = pSession->m_id;
			cry_strcpy( newSession.m_name, pSession->m_data.m_name );

			//capture the session ID in a map
			std::pair< TSessionIdMap::iterator, bool > insertResult = m_sessionIdMap.insert( std::make_pair( m_sessionIdIndex++, newSession ) );

			if( insertResult.second )
			{
				CryLog( "MMLua: Adding a server with parameters ping %d, status %d, avSkill %d", pSession->m_ping, activeStatus, sessionSkillRank );

				result->SetValue( "SessionId", insertResult.first->first );
				result->SetValue( "SearchId", s_currentMMSearchID );

				float timeSinceStarted = gEnv->pTimer->GetFrameStartTime().GetSeconds() - m_startTime;
				result->SetValue( "TimeFound", timeSinceStarted );
				
				//make a sub table to hold all the known session parameters	
				SmartScriptTable parameters( gEnv->pScriptSystem );
				result->SetValue( "Parameters", parameters );

				parameters->SetValue( "ActiveStatus", activeStatus );
				parameters->SetValue( "ServerAvSkill", sessionSkillRank );
				parameters->SetValue( "Region", region );
				parameters->SetValue( "Language", sessionLanguageId );
				parameters->SetValue( "BadServer", bIsBadServer );
				parameters->SetValue( "Ping", pSession->m_ping );
				parameters->SetValue( "FilledSlots", pSession->m_numFilledSlots );

				Script::Call( gEnv->pScriptSystem, scriptFunction, m_pScript, result );
				gEnv->pScriptSystem->ReleaseFunc( scriptFunction );
			}
		}
	}
}


//------------------------------------------------------------------------
CrySessionID CMatchMakingHandler::GetSessionId( int sessionIndex )
{
	TSessionIdMap::const_iterator it = m_sessionIdMap.find( sessionIndex );
	
	if( it == m_sessionIdMap.end() )
	{
		return CrySessionInvalidID;
	}
	else
	{ 
		return it->second.m_id;
	}
}

//------------------------------------------------------------------------
void CMatchMakingHandler::Join( int sessionIndex )
{
	bool success = false;

	if( CGameLobby* pLobby = g_pGame->GetGameLobby() )
	{
		TSessionIdMap::const_iterator itSession = m_sessionIdMap.find( sessionIndex );

		if( itSession != m_sessionIdMap.end() )
		{
#if defined(TRACK_MATCHMAKING)
			if( CGameLobbyManager* pLobbyMan = g_pGame->GetGameLobbyManager() )
			{
				if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
				{
					pMMTel->AddEvent( SMMChosenSessionEvent( itSession->second.m_name, itSession->second.m_id, "Lua Matchmaking", false, s_currentMMSearchID, pLobbyMan->IsPrimarySession( pLobby ) ) );
				}
			}
#endif

			m_bIsMerging = false;

			success = pLobby->JoinServer( itSession->second.m_id, itSession->second.m_name, CryMatchMakingInvalidConnectionUID, false); 
		}
		else
		{
			MMLog( "Unable to find session ID in session ID map", true );
		}
	}

	if( success == false )
	{
		AddWaitingTask( eMMHT_EndJoin, false );
	}

}

//------------------------------------------------------------------------
void CMatchMakingHandler::Merge( int sessionIndex )
{
	if( CGameLobby* pLobby = g_pGame->GetGameLobby() )
	{
		TSessionIdMap::const_iterator itSession = m_sessionIdMap.find( sessionIndex );

		if( itSession != m_sessionIdMap.end() )
		{
#if defined(TRACK_MATCHMAKING)
			if( CGameLobbyManager* pLobbyMan = g_pGame->GetGameLobbyManager() )
			{
				if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
				{
					pMMTel->AddEvent( SMMChosenSessionEvent( itSession->second.m_name, itSession->second.m_id, "Lua Matchmaking", false, s_currentMMSearchID, pLobbyMan->IsPrimarySession( pLobby ) ) );
				}
			}
#endif

			m_bIsMerging = true;

			bool success = pLobby->MergeToServer( itSession->second.m_id );

			if( success == false )
			{
				AddWaitingTask( eMMHT_EndMerge, false );
			}
		}
	}
}

//------------------------------------------------------------------------
void CMatchMakingHandler::GameLobbyJoinFinished( ECryLobbyError error )
{
	bool success = (error == eCLE_Success);

	if( m_bIsMerging )
	{
		AddWaitingTask( eMMHT_EndMerge, success );
	}
	else
	{
		AddWaitingTask( eMMHT_EndJoin, success );
	}
}


//------------------------------------------------------------------------
void CMatchMakingHandler::GameLobbyCreateFinished( ECryLobbyError error )
{
	bool success = (error == eCLE_Success);

	AddWaitingTask( eMMHT_EndCreate, success );
}


//------------------------------------------------------------------------
void CMatchMakingHandler::Update( float dt )
{
	if( !m_bActive )
		return;

	//peek queue
	EMatchMakingHandlerTask temp = m_waitingTasksQueue[ m_currentTask ].taskType;
	if( temp != eMMHT_None )
	{
		bool taskSuccess = m_waitingTasksQueue[ m_currentTask ].taskSuccess;

		switch( temp )
		{
		case eMMHT_EndSearch:
			{
				HSCRIPTFUNCTION scriptFunction;
				if( m_pScript->GetValue( "OnSearchFinished", scriptFunction ) )  
				{
					Script::Call( gEnv->pScriptSystem, scriptFunction, m_pScript );
					gEnv->pScriptSystem->ReleaseFunc( scriptFunction );
				}
				else
				{
					CryLog("Important lua Function missing");
				}
				break;
			}
		case eMMHT_EndJoin:
			{
				HSCRIPTFUNCTION scriptFunction;
				CryLog( "MMHandler: OnJoinedFinished" );
				if( m_pScript->GetValue( "OnJoinFinished", scriptFunction ) )  
				{
					Script::Call( gEnv->pScriptSystem, scriptFunction, m_pScript, taskSuccess );
					gEnv->pScriptSystem->ReleaseFunc( scriptFunction );
				}
				else
				{
					CryLog("Important lua Function missing");
				}
				break;
			}
		case eMMHT_EndMerge:
			{
				HSCRIPTFUNCTION scriptFunction;
				if( m_pScript->GetValue( "OnMergeFinished", scriptFunction ) )  
				{
					Script::Call( gEnv->pScriptSystem, scriptFunction, m_pScript, taskSuccess );
					gEnv->pScriptSystem->ReleaseFunc( scriptFunction );
				}
				else
				{
					CryLog("Important lua Function missing");
				}
				break;
			}
		case eMMHT_EndCreate:
			{
				HSCRIPTFUNCTION scriptFunction;
				if( m_pScript->GetValue( "OnCreateFinished", scriptFunction ) )  
				{
					Script::Call( gEnv->pScriptSystem, scriptFunction, m_pScript, taskSuccess );
					gEnv->pScriptSystem->ReleaseFunc( scriptFunction );
				}
				else
				{
					CryLog("Important lua Function missing");
				}
				break;
			}
		case eMMHT_EndHostMigrate:
			{
				CryLog( "MMHandler: OnHostMigrate" );
				HSCRIPTFUNCTION scriptFunction;
				if( m_pScript->GetValue( "OnHostMigrationFinished", scriptFunction ) )  
				{
					Script::Call( gEnv->pScriptSystem, scriptFunction, m_pScript, taskSuccess, m_waitingTasksQueue[ m_currentTask ].taskData.bDat );
					gEnv->pScriptSystem->ReleaseFunc( scriptFunction );
				}
				break;
			}
		default:
			CRY_ASSERT_MESSAGE( false, "MMHandler: Invalid task ID in waiting tasks" );
		}

		//pop and loop queue
		m_waitingTasksQueue[ m_currentTask ].taskType = eMMHT_None;
		
		m_currentTask++;
		if( m_currentTask >= k_maxTasks )
		{
			m_currentTask = 0;
		}
	}

	if( m_subscribedUpdateTimer > 0.0f )
	{
		m_subscribedUpdateTimer -= dt;

		if( m_subscribedUpdateTimer <= 0.0f )
		{
			m_subscribedUpdateTimer = -1.0f;

			CryLog( "MMLua: Calling to subscribed Update" );
			HSCRIPTFUNCTION scriptFunction;
			if( m_pScript->GetValue( "Update", scriptFunction ) )  
			{
				Script::Call( gEnv->pScriptSystem, scriptFunction, m_pScript );
				gEnv->pScriptSystem->ReleaseFunc( scriptFunction );
			}
			else
			{
				CryLog("Important lua Function missing");
			}
		}
	}

}

//------------------------------------------------------------------------
void CMatchMakingHandler::OnHostMigrationFinished( bool success, bool isNewHost )
{
	TaskData data;
	data.bDat = isNewHost;

	AddWaitingTask(eMMHT_EndHostMigrate, success, &data );
}

//------------------------------------------------------------------------
float CMatchMakingHandler::LegacyC2MatchMakingScore( SCrySessionSearchResult* session, CGameLobby *lobby, bool includeRand )
{
	//Creates sub metrics (between 0-1 (1 being best))

	CGameLobbyCVars *pGameLobbyCVars = CGameLobbyCVars::Get();
	const CGameLobby::EActiveStatus activeStatus = (CGameLobby::EActiveStatus) GameLobbyData::GetSearchResultsData( session, LID_MATCHDATA_ACTIVE );

	const float pingScale = pGameLobbyCVars ? pGameLobbyCVars->gl_findGamePingScale : 1.f;
	const float idealPlayerCount = pGameLobbyCVars ? pGameLobbyCVars->gl_findGameIdealPlayerCount : 1.f;

	float pingSubMetric = 1.0f - clamp_tpl((session->m_ping / pingScale), 0.0f, 1.0f);					//300ms or above gives you a 0 rating
	float playerSubMetric = clamp_tpl((float)session->m_numFilledSlots / idealPlayerCount, 0.0f, 1.0f);					//more players the better
	float lobbySubMetric = (activeStatus != CGameLobby::eAS_Lobby) ? 0.0f : 1.f;		// Prefer games that haven't started yet

	float skillSubMetric = 0.f;
	const int skillRank = lobby->CalculateAverageSkill();
	const int sessionSkillRank = GameLobbyData::GetSearchResultsData(session, LID_MATCHDATA_SKILL);
	if (skillRank)
	{										
		float diff = (float) abs(skillRank - sessionSkillRank);
		float fracDiff = diff / (float) skillRank;
		skillSubMetric = 1.f - std::min(fracDiff, 1.f);
		skillSubMetric = (skillSubMetric * skillSubMetric);
	}

	int32 languageId = lobby->GetCurrentLanguageId();

	float languageSubMetric = 0.f;
	if (languageId == GameLobbyData::GetSearchResultsData(session, LID_MATCHDATA_LANGUAGE))
	{
		languageSubMetric = 1.f;
	}

	float randomSubMetric = ((float) (g_pGame->GetRandomNumber() & 0xffff)) / ((float) 0xffff);

	if(pGameLobbyCVars)
	{
		pingSubMetric *= pGameLobbyCVars->gl_findGamePingMultiplier;
		playerSubMetric *= pGameLobbyCVars->gl_findGamePlayerMultiplier;
		lobbySubMetric *= pGameLobbyCVars->gl_findGameLobbyMultiplier;
		skillSubMetric *= pGameLobbyCVars->gl_findGameSkillMultiplier;
		languageSubMetric *= pGameLobbyCVars->gl_findGameLanguageMultiplier;
		randomSubMetric *= pGameLobbyCVars->gl_findGameRandomMultiplier;
	}

	float score = pingSubMetric + playerSubMetric + lobbySubMetric + skillSubMetric + languageSubMetric;
	
	if( includeRand )
	{
		score += randomSubMetric;
	}

	CryLog("MMLua: Final Score %.2f", score );

	return score;
}

//------------------------------------------------------------------------
bool CMatchMakingHandler::AllowedToCreateGame()
{
	return (gEnv->IsDedicated()) || (g_pGameCVars->g_EnableDevMenuOptions != 0);	// only allow create game on PC if dev options are enabled
}

bool CMatchMakingHandler::AddWaitingTask( EMatchMakingHandlerTask taskID, bool taskSuccess, TaskData* pData /*= NULL*/ )
{
	if( !m_bActive )
		return false;

	bool ok = ( m_waitingTasksQueue[ m_newTask ].taskType == eMMHT_None );
	CRY_ASSERT( ok );
	if( ok )
	{
		m_waitingTasksQueue[ m_newTask ].taskType = taskID;
		m_waitingTasksQueue[ m_newTask ].taskSuccess = taskSuccess;

		if( pData )
		{
			m_waitingTasksQueue[ m_newTask ].taskData = *pData;
		}

		m_newTask++;
		if( m_newTask >= k_maxTasks )
		{
			m_newTask = 0;
		}

	}

	return ok;
}

void CMatchMakingHandler::MMLog( const char* message, bool isError )
{
	if( isError )
	{
		CryLog( "MMHandlerError: %s", message );
	}
	else
	{
		CryLog( "MMHandlerLog: %s", message );
	} 

	if( CMatchmakingTelemetry* pMMTel = g_pGame->GetMatchMakingTelemetry() )
	{
		pMMTel->AddEvent( SMMGenericLogEvent( message, isError ) );
	}

}

void CMatchMakingHandler::ClearSessionParameters()
{
	for (unsigned int i = 0; i < m_nSessionParams; i++)
	{
		new (&m_sessionParams[i]) SCryLobbyUserData();
	}
	m_nSessionParams = 0;
}

void CMatchMakingHandler::NewSessionParameter( ELOBBYIDS paramID, ScriptAnyValue valueVal )
{
	if( m_nSessionParams < eLDI_Num )
	{
		m_sessionParams[ m_nSessionParams ].m_id = paramID;

		switch (valueVal.GetType())
		{
		case EScriptAnyType::Number:
			m_sessionParams[ m_nSessionParams ].m_type = eCLUDT_Int32;
			m_sessionParams[ m_nSessionParams ].m_int32 = static_cast<int32>(valueVal.GetNumber());
			break;

		case EScriptAnyType::Boolean:
			m_sessionParams[ m_nSessionParams ].m_type = eCLUDT_Int32;
			m_sessionParams[ m_nSessionParams ].m_int32 = valueVal.GetBool() ? 1 : 0;
			break;

		case EScriptAnyType::Handle:
			m_sessionParams[ m_nSessionParams ].m_type = eCLUDT_Int32;
			m_sessionParams[ m_nSessionParams ].m_int32 = static_cast<int32>(valueVal.GetScriptHandle().n);
			break;

		default:
			MMLog( "MMLua: Unsupported type in session data", true );
		}

		CryLog( "MMLua: Create Session Parameter, id %s, value %d", m_sessionParams[ m_nSessionParams ].m_id.c_str(), m_sessionParams[ m_nSessionParams ].m_int32 );
		m_nSessionParams++;
	}
	else
	{
		MMLog( "MMLua: Too many session search parameters set from Lua", true );
	}
}

void CMatchMakingHandler::AdjustCreateSessionData( SCrySessionData* pData, uint32 maxDataItems )
{
	if( !m_bActive )
		return;
		
	//for every parameter in our data
	for( uint32 iParam = 0; iParam < m_nSessionParams; iParam++ )
	{
		CryLobbyUserDataID paramID = m_sessionParams[ iParam ].m_id;
		bool found = false;
		//check if it is in the source data
		for( uint32 iData = 0; iData < pData->m_numData; iData++ )
		{
			if( pData->m_data[ iData ].m_id == paramID )
			{
				//if it is, change it
				CRY_ASSERT( pData->m_data[ iData ].m_type == m_sessionParams[ iParam ].m_type );
				pData->m_data[ iData ].m_int32 = m_sessionParams[ iParam ].m_int32;
				found = true;
				break;
			}
		}

		if( ! found )
		{
			//if not, check we have space for another parameter
			if( pData->m_numData < maxDataItems )
			{
				//and add it to the source data
				pData->m_data[ pData->m_numData ] = m_sessionParams[ iParam ];
				pData->m_numData++;
			}		
		}		
	}
}

void CMatchMakingHandler::RequestSubscribedUpdate( float timeToCall )
{
	CryLog( "MMLua: Requesting an update call in %.2f secs", timeToCall );
	if( m_subscribedUpdateTimer > 0.0f )
	{
		CryLog( "MMLua: Asking for subscribed update but already have one set, this is bad" );
	}

	m_subscribedUpdateTimer = timeToCall;
}

