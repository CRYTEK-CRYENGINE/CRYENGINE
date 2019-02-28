// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 17:11:2006   15:38 : Created by Stas Spivakov

*************************************************************************/
#ifndef __GAMESTATS_H__
#define __GAMESTATS_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "IGameplayRecorder.h"
#include "IActorSystem.h"
#include "ILevelSystem.h"

struct IStatsTrack;
struct IServerReport;

class CGameStats :
	public IGameplayListener,
	public ILevelSystemListener,
	public IGameFrameworkListener,
	public IHostMigrationEventListener
{
private:

	struct  SPlayerStats;
	struct  SRoundStats;

	struct  SPlayerInfo
	{
		//these are used to report score/status to server list
		string                name;
		int                   team;
		int                   rank;
		bool                  spectator;
		std::map<string, int> scores;
		//these are 'real' statistics
		int                   id;

		typedef std::vector<_smart_ptr<SPlayerStats>> TStatsVct;
		TStatsVct stats;

		void      GetMemoryUsage(ICrySizer* pSizer) const;

	};
	typedef std::map<int, SPlayerInfo> PlayerStatsMap;

	struct STeamStats
	{
		int    id;
		string name;
		int    score;
	};
	typedef std::map<int, STeamStats> TeamStatsMap;

	typedef std::map<string, int>     TStatsKeyMap;

	struct Listener;
	struct SStatsTrackHelper;

public:
	CGameStats(CCryAction* pGameFramework);
	virtual ~CGameStats();

	int ILINE GetChannelId(IEntity* pEntity)
	{
		IActor* pActor = m_pActorSystem->GetActor(pEntity->GetId());
		if (pActor)
			return pActor->GetChannelId();
		return 0;
	}

	//IGamePlayListener
	virtual void OnGameplayEvent(IEntity* pEntity, const GameplayEvent& event);
	//

	//ILevelSystemListener
	virtual void OnLevelNotFound(const char* levelName);
	virtual void OnLoadingStart(ILevelInfo* pLevel);
	virtual void OnLoadingLevelEntitiesStart(ILevelInfo* pLevel);
	virtual void OnLoadingComplete(ILevelInfo* pLevel);
	virtual void OnLoadingError(ILevelInfo* pLevel, const char* error);
	virtual void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount);
	virtual void OnUnloadComplete(ILevelInfo* pLevel);
	//

	//IGameFrameworkListener
	virtual void OnActionEvent(const SActionEvent& event);
	virtual void OnPostUpdate(float fDeltaTime)    {}
	virtual void OnSaveGame(ISaveGame* pSaveGame)  {}
	virtual void OnLoadGame(ILoadGame* pLoadGame)  {}
	virtual void OnLevelEnd(const char* nextLevel) {}
	//

	// IHostMigrationEventListener
	virtual EHostMigrationReturn OnInitiate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnDisconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnDemoteToClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnPromoteToServer(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnReconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnFinalise(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual void                 OnComplete(SHostMigrationInfo& hostMigrationInfo) {}
	virtual EHostMigrationReturn OnTerminate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnReset(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	// ~IHostMigrationEventListener

	void StartSession();
	void EndSession();
	void OnMapSwitch();
	void StartGame(bool server);
	void EndGame(bool server);
	void SuddenDeath(bool server);
	void EndRound(bool server, int winner);
	void NewPlayer(int plr, int team, bool spectator, bool restored);
	void RemovePlayer(int plr, bool keep);
	void CreateNewLifeStats(int plr);
	void ResetScore(int plr);
	void StartLevel();
	void OnKill(int plr, EntityId* extra);
	void OnDeath(int plr, int shooterId);

	void GameReset();
	void Connected();

	void SetName(int plr, const char* name);
	void SetScore(int plr, const char* score, int value);
	void SetTeam(int plr, int value);
	void SetRank(int plr, int value);
	void SetSpectator(int plr, int value);
	void Update();

	void Dump();
	void SaveStats();
	void ResetStats();

	void GetMemoryStatistics(ICrySizer* s);

	void PromoteToServer();

private:
	void Init();
	//set session-wide parameters
	void ReportSession();
	//set game(map)-wide parameters
	void ReportGame();
	//set dynamic parameters
	void Report();
	//submit stats for player to StatsTrack service
	void SubmitPlayerStats(const SPlayerInfo& plr, bool server, bool client);
	//submit stats for server
	void SubmitServerStats();

	//player stats
	void ProcessPlayerStat(IEntity* pEntity, const GameplayEvent& event);

	bool GetLevelName(string& mapname);

	CCryAction*    m_pGameFramework;
	IActorSystem*  m_pActorSystem;

	bool           m_playing;
	bool           m_stateChanged;
	bool           m_startReportNeeded;
	bool           m_reportStarted;

	PlayerStatsMap m_playerMap;
	TeamStatsMap   m_teamMap;

	IStatsTrack*   m_statsTrack;
	IServerReport* m_serverReport;
	Listener*      m_pListener;
	CTimeValue     m_lastUpdate;
	CTimeValue     m_lastReport; //sync data to network engine

	//some data about game started
	string                       m_gameMode;
	string                       m_mapName;

	CGameStatsConfig*            m_config;

	CTimeValue                   m_lastPosUpdate;
	CTimeValue                   m_roundStart;

	std::unique_ptr<SRoundStats> m_roundStats;
};

#endif /*__GAMESTATS_H__*/
