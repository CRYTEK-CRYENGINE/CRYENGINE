// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Class for returning a fake session list to session searches.
Aim is to use this to test match making performance.
The fake session lists are loaded from XML files.

-------------------------------------------------------------------------
History:
- 20:07:2011 : Created By Andrew Blackwell

*************************************************************************/

#include "StdAfx.h"

//////////////////////////////////////////////////////////////////////////
// This Include
#include "SessionSearchSimulator.h"

#include "GameLobbyData.h"
#include "GameLobby.h"
#include "PlayerProgression.h"

//-------------------------------------------------------------------------
//Constructor
CSessionSearchSimulator::CSessionSearchSimulator()
:	m_currentNode( 0 )
{
}

//--------------------------------------------------------------------------
//Destructor
CSessionSearchSimulator::~CSessionSearchSimulator()
{
}

//--------------------------------------------------------------------------
bool CSessionSearchSimulator::OpenSessionListXML( const char* filepath )
{
	m_sessionListXML = GetISystem()->LoadXmlFromFile( filepath );

	if( m_sessionListXML )
	{
		m_currentFilepath.Format( filepath );
		if( strcmpi( m_sessionListXML->getTag(), "MatchMakingTelemetryXML" ) == 0 )
		{
			//XML appears to be in the format we expect
			if( m_sessionListXML->getChildCount() > 0 )
			{
				m_currentNode = 0;
				return true;
			}
		}
	}

	return false;
}

//--------------------------------------------------------------------------
bool CSessionSearchSimulator::OutputSessionListBlock( CryLobbyTaskID& taskID, CryMatchmakingSessionSearchCallback cb, void* cbArg )
{
	bool noMoreSessions = false;
	bool retval = false;
	XmlNodeRef currentNode;

	if( m_sessionListXML && m_currentNode < m_sessionListXML->getChildCount() )
	{
		//skip any nodes that aren't found session nodes
		currentNode = m_sessionListXML->getChild( m_currentNode );

		while( strcmpi( currentNode->getTag(), "foundSession" ) != 0)
		{
			++m_currentNode;
			if( m_currentNode >= m_sessionListXML->getChildCount() )
			{
				noMoreSessions = true;
				break;
			}

			currentNode = m_sessionListXML->getChild( m_currentNode );
		}
	}
	else
	{
		noMoreSessions = true;
	}

	//if we didn't find any session nodes
	if( noMoreSessions )
	{
		//end call, return false
		(cb)( taskID, eCLE_Success, NULL, cbArg );
		retval = false;
	}	
	else
	{
		//for each node until current node isn't a session node
		while( strcmpi( currentNode->getTag(), "foundSession" ) == 0)
		{
			//get the node's data
			SCrySessionSearchResult sessionData;
			SCryLobbyUserData userData[16];

			sessionData.m_data.m_data = userData;

			uint32 ping;
			if( currentNode->getAttr( "ping", ping ) )
			{	
				sessionData.m_ping = ping;
			}

			uint32 filledslots;
			if( currentNode->getAttr( "filledSlots", filledslots ) )
			{
				sessionData.m_numFilledSlots = filledslots;
			}

			const char* pSessionStr;
			if( currentNode->getAttr( "sessionId", &pSessionStr ) )
			{
				SCryFakeSessionID* pSessionID = new SCryFakeSessionID();
				cry_strcpy( pSessionID->m_idStr, pSessionStr );
			
				sessionData.m_id = pSessionID;
			}

			const char* pSessionName;
			if( currentNode->getAttr( "servername", &pSessionName ) )
			{
				cry_strcpy( sessionData.m_data.m_name, pSessionName );
			}

			//generic data elements
			uint32 iData = 0;

			//all sessions returned should be the same version, playlist and variant as we are currently running/searching for
			sessionData.m_data.m_data[ iData ].m_id = LID_MATCHDATA_VERSION;
			sessionData.m_data.m_data[ iData ].m_type = eCLUDT_Int32;
			sessionData.m_data.m_data[ iData ].m_int32 = GameLobbyData::GetVersion();
			iData++;

			sessionData.m_data.m_data[ iData ].m_id = LID_MATCHDATA_PLAYLIST;
			sessionData.m_data.m_data[ iData ].m_type = eCLUDT_Int32;
			sessionData.m_data.m_data[ iData ].m_int32 = GameLobbyData::GetPlaylistId();
			iData++;

			sessionData.m_data.m_data[ iData ].m_id = LID_MATCHDATA_VARIANT;
			sessionData.m_data.m_data[ iData ].m_type = eCLUDT_Int32;
			sessionData.m_data.m_data[ iData ].m_int32 = GameLobbyData::GetVariantId();
			iData++;

			sessionData.m_data.m_data[ iData ].m_id = LID_MATCHDATA_REQUIRED_DLCS;
			sessionData.m_data.m_data[ iData ].m_type = eCLUDT_Int32;
			sessionData.m_data.m_data[ iData ].m_int32 = 0;
			iData++;

			uint32 skillDiff;
			if( currentNode->getAttr( "rankDifference", skillDiff ) )
			{
				sessionData.m_data.m_data[ iData ].m_id = LID_MATCHDATA_SKILL;
				sessionData.m_data.m_data[ iData ].m_type = eCLUDT_Int32;
				sessionData.m_data.m_data[ iData ].m_int32 = CPlayerProgression::GetInstance()->GetData(EPP_SkillRank) - skillDiff;
				iData++;
			}

			uint32 languageID;
			if( currentNode->getAttr( "language", languageID ) )
			{
				sessionData.m_data.m_data[ iData ].m_id = LID_MATCHDATA_LANGUAGE;
				sessionData.m_data.m_data[ iData ].m_type = eCLUDT_Int32;
				sessionData.m_data.m_data[ iData ].m_int32 = languageID;
				iData++;
			}

			uint32 regionID;
			if( currentNode->getAttr( "region", regionID ) )
			{
				//match the parameter used by the different platforms for region data
#if GAMELOBBY_USE_COUNTRY_FILTERING
				sessionData.m_data.m_data[ iData ].m_id = LID_MATCHDATA_COUNTRY;
				sessionData.m_data.m_data[ iData ].m_type = eCLUDT_Int32;
				sessionData.m_data.m_data[ iData ].m_int32 = regionID;
				iData++;
#endif
			}

			uint32 activeStatus = CGameLobby::eAS_Lobby;
			const char* statusStr;
			if( currentNode->getAttr( "status", &statusStr ) )
			{
				if( strcmpi( statusStr, "Lobby" ) == 0)
				{
					activeStatus = CGameLobby::eAS_Lobby;
				}
				else if( strcmpi( statusStr, "InGame" ) == 0)
				{
					activeStatus = CGameLobby::eAS_Game;
				}
				else if( strcmpi( statusStr, "EndGame" ) == 0)
				{
					activeStatus = CGameLobby::eAS_EndGame;
				}
				else if( strcmpi( statusStr, "StartGame" ) == 0)
				{
					activeStatus = CGameLobby::eAS_StartingGame;
				}
			}

			sessionData.m_data.m_data[ iData ].m_id = LID_MATCHDATA_ACTIVE;
			sessionData.m_data.m_data[ iData ].m_type = eCLUDT_Int32;
			sessionData.m_data.m_data[ iData ].m_int32 = activeStatus;
			iData++;

			sessionData.m_data.m_numData = iData;
								
			//pass the data to the callback
			(cb)( taskID, eCLE_SuccessContinue, &sessionData, cbArg );

			//check we're not out of nodes
			++m_currentNode;
			if( m_currentNode >= m_sessionListXML->getChildCount() )
			{
				break;
			}
			currentNode = m_sessionListXML->getChild( m_currentNode );
		}

		//end call normally, return true
		(cb)( taskID, eCLE_Success, NULL, cbArg );
		retval = true;
	}

	return retval;
}


