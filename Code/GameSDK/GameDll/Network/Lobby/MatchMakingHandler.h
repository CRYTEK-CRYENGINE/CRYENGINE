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

//////////////////////////////////////////////////////////////////////////
//Header Guard
#ifndef __MATCHMAKINGHANDLER_H__
#define __MATCHMAKINGHANDLER_H__

//////////////////////////////////////////////////////////////////////////
// Important Includes
//For eLDI_Max
#include "GameLobbyData.h"


#if GAMELOBBY_USE_COUNTRY_FILTERING
#define FIND_GAMES_SEARCH_NUM_DATA	13
#else
#define FIND_GAMES_SEARCH_NUM_DATA	12
#endif

class CMatchMakingHandler
{
public:
	CMatchMakingHandler();
	~CMatchMakingHandler();

	IScriptTable*	GetScriptTable() { return m_pScript; }


	//////////////////////////////////////////////////////////////////////////
	// These functions call directly to Lua
	void	OnEnterMatchMaking();
	void	OnLeaveMatchMaking();

	void	Update( float dt );

	void	OnHostMigrationFinished( bool sucess, bool isNewHost );
	void	OnSearchResult( SCrySessionSearchResult* pSession );

	//////////////////////////////////////////////////////////////////////////
	// These functions are useful things for matchmaking
	static float	LegacyC2MatchMakingScore( SCrySessionSearchResult* session, CGameLobby *lobby, bool includeRand );
	static bool		AllowedToCreateGame();
	bool					IsJoiningSession();
	CrySessionID	GetSessionId( int sessionIndex );
	void					MMLog( const char* message, bool isError );

	//////////////////////////////////////////////////////////////////////////
	// These functions handle requests which have come from matchmaking (and usually require a response at a later time)
	void Search( int freeSlotsRequired, int maxResults, SCrySessionSearchData* searchParameters, int numSearchParameters );
	void Join( int sessionIndex );
	void Merge( int sessionIndex );
	void CancelSearch();

	void ClearSessionParameters();
	void NewSessionParameter( ELOBBYIDS paramID, ScriptAnyValue valueVal );

	void RequestSubscribedUpdate( float timeToCall );

	//////////////////////////////////////////////////////////////////////////
	// These functions are callbacks and callback-likes
	static void SearchCallback( CryLobbyTaskID taskID, ECryLobbyError error, SCrySessionSearchResult* session, void* arg );
	
	void GameLobbyJoinFinished( ECryLobbyError error );
	void GameLobbyCreateFinished( ECryLobbyError error );
	void AdjustCreateSessionData( SCrySessionData* pData, uint32 maxDataItems );
			
private:

	//private data types etc.
	enum EMatchMakingHandlerTask
	{
		eMMHT_None,
		eMMHT_EndSearch,
		eMMHT_EndJoin,
		eMMHT_EndMerge,
		eMMHT_EndCreate,
		eMMHT_EndHostMigrate,
	};

	struct SessionDetails
	{
		char									m_name[MAX_SESSION_NAME_LENGTH];
		CrySessionID					m_id;
	};

	typedef std::map< int, SessionDetails >	TSessionIdMap;
	
	union TaskData{ int iDat; bool bDat; };

	struct WaitingTaskEntry
	{
		EMatchMakingHandlerTask		taskType;
		bool											taskSuccess;
		TaskData									taskData;
	};

	//private functions
	bool	LoadScript();
	void	OnInit( CGameLobbyManager* pLobbyManager );

	bool AddWaitingTask( EMatchMakingHandlerTask taskID, bool taskSuccess, TaskData* pData = NULL );

	//Static members
	static int				s_currentMMSearchID;
	static const int	k_maxTasks = 4;

	//Data members
	SCryLobbyUserData	m_sessionParams[ eLDI_Num ];
	TSessionIdMap			m_sessionIdMap;
	WaitingTaskEntry		m_waitingTasksQueue[ k_maxTasks ];

	SmartScriptTable	m_pScript;
	
	float							m_startTime;
	float							m_subscribedUpdateTimer;

	uint32						m_nSessionParams;
	int								m_sessionIdIndex;

	//for the waiting task queue
	uint8							m_currentTask;
	uint8							m_newTask;

	bool							m_bActive;
	bool							m_bIsMerging;

};

#endif	//__MATCHMAKINGHANDLER_H__

