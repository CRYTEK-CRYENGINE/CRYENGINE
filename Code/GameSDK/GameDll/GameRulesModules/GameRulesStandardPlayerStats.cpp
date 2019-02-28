// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Game rules module to handle player scores and stats
	-------------------------------------------------------------------------
	History:
	- 03:09:2009  : Created by Ben Johnson

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesStandardPlayerStats.h"
#include <CrySystem/XML/IXml.h>
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameRules.h"
#include "IGameRulesStateModule.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "Utility/CryWatch.h"
#include "Player.h"
#include "PersistantStats.h"
#include "Network/Lobby/GameLobby.h"
#include "SkillRanking.h"
#include "StatsEntityIdRegistry.h"

#if CRY_WATCH_ENABLED
#define WATCH_LV1				m_dbgWatchLvl<1  ? (NULL) : CryWatch
#define WATCH_LV1_ONLY	m_dbgWatchLvl!=1 ? (NULL) : CryWatch
#define WATCH_LV2				m_dbgWatchLvl<2  ? (NULL) : CryWatch
#else
#define WATCH_LV1
#define WATCH_LV1_ONLY
#define WATCH_LV2
#endif

const float CGameRulesStandardPlayerStats::GR_PLAYER_STATS_PING_UPDATE_INTERVAL = 1000.f;

CRY_FIXME(16,9,2009,"Need a way of displaying points earned on screen!");
#define DISPLAY_POINTS_ON_HUD			0

//-------------------------------------------------------------------------
CGameRulesStandardPlayerStats::CGameRulesStandardPlayerStats()
{
	m_lastUpdatedPings = 0.f;
	m_playerStats.reserve(24);
	m_dbgWatchLvl = 0;
	m_bRecordTimeSurvived = false;
}

//-------------------------------------------------------------------------
CGameRulesStandardPlayerStats::~CGameRulesStandardPlayerStats()
{
	CGameRules  *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->UnRegisterRevivedListener(this);
		pGameRules->UnRegisterRoundsListener(this);
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::Init( XmlNodeRef xml )
{
	int iVal;

	if (xml->getAttr("dbgWatchLvl", iVal))
	{
		m_dbgWatchLvl = iVal;
	}

	if (xml->getAttr("recordTimeSurvived", iVal))
	{
		m_bRecordTimeSurvived = (iVal != 0);
	}

	CGameRules  *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->RegisterRevivedListener(this);
		pGameRules->RegisterRoundsListener(this);
	}

  g_pGame->GetIGameFramework()->GetNetworkSafeClassId(m_classidFrag,"explosivegrenade");
  g_pGame->GetIGameFramework()->GetNetworkSafeClassId(m_classidFlash,"flashbang");
  g_pGame->GetIGameFramework()->GetNetworkSafeClassId(m_classidC4,"c4explosive");
	g_pGame->GetIGameFramework()->GetNetworkSafeClassId(m_classidLTAG, "LTagGrenade");
	g_pGame->GetIGameFramework()->GetNetworkSafeClassId(m_classidJawRocket, "rocket");
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::Reset()
{
	m_lastUpdatedPings = 0.f;
	m_playerStats.clear();
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::Update(float frameTime)
{
#if CRY_WATCH_ENABLED
	if (m_dbgWatchLvl >= 1)
	{
		EntityId localPlayerId = g_pGame->GetIGameFramework()->GetClientActorId();
		SGameRulesPlayerStat *playerStat = GetPlayerStatsInternal(localPlayerId);
		if (playerStat)
		{
			WATCH_LV1("Score=%d; Team=%d", playerStat->points, g_pGame->GetGameRules()->GetTeam(localPlayerId));
		}
		WATCH_LV1_ONLY(" ");
	}

	if (m_dbgWatchLvl >= 2)
	{
		WATCH_LV2("Player stats:");
		for (TPlayerStats::iterator it=m_playerStats.begin(); it!=m_playerStats.end(); ++it)
		{
			if (IEntity* e=gEnv->pEntitySystem->GetEntity(it->playerId))
			{
				WATCH_LV2("%s: deaths %d, deathsThisRound %d, HASSPAWNEDTHISROUND %d, HASHADROUNDRESTART %d", e->GetName(), it->deaths, it->deathsThisRound, ((it->flags&SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND)?1:0), ((it->flags&SGameRulesPlayerStat::PLYSTATFL_HASHADROUNDRESTART)?1:0));
			}
		}
	}
#endif

	if (!gEnv->bServer)
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		float currTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
		if (currTime - m_lastUpdatedPings > CGameRulesStandardPlayerStats::GR_PLAYER_STATS_PING_UPDATE_INTERVAL)
		{
			m_lastUpdatedPings = currTime;

			for (TPlayerStats::iterator it=m_playerStats.begin(); it!=m_playerStats.end(); ++it)
			{
				IActor *actor =  g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(it->playerId);
				if (actor)
				{
					INetChannel *pNetChannel = g_pGame->GetIGameFramework()->GetNetChannel(actor->GetChannelId());
					if (pNetChannel)
						it->ping = (int)floor((pNetChannel->GetPing(true) * 1000.f) + 0.5f);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::OnStartNewRound()
{
	CryLog("[tlh] @ CGameRulesStandardPlayerStats::OnStartNewRound()");
	CRY_ASSERT(gEnv->bServer);

	INetContext*  pNetContext = g_pGame->GetIGameFramework()->GetNetContext();

	for (TPlayerStats::iterator it=m_playerStats.begin(); it!=m_playerStats.end(); ++it)
	{
		it->deathsThisRound = 0;
		it->timeSurvived = 0.f;
		it->flags &= ~(SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND | SGameRulesPlayerStat::PLYSTATFL_DIEDINEXTRATIMETHISROUND | SGameRulesPlayerStat::PLYSTATFL_CANTSPAWNTHISROUND);

		if (pNetContext)
		{
			pNetContext->ChangedAspects(it->playerId, CPlayer::ASPECT_PLAYERSTATS_SERVER);
		}
	}
}

//-------------------------------------------------------------------------
bool CGameRulesStandardPlayerStats::NetSerialize( EntityId playerId, TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (aspect == CPlayer::ASPECT_PLAYERSTATS_SERVER)
	{
		NET_PROFILE_SCOPE("StandardPlayerStats", ser.IsReading());

		SGameRulesPlayerStat *playerStat = GetPlayerStatsInternal(playerId);
		if (playerStat)
		{
			int pointsBefore = playerStat->points;
			uint16 deathsThisRoundBefore = playerStat->deathsThisRound;
			uint8 fragsBefore = playerStat->successfulFrags;
      uint8 flashbangsBefore = playerStat->successfulFlashbangs;
      uint8 c4before = playerStat->successfulC4;
			uint8 ltagBefore = playerStat->successfulLTAG;
			uint8 jawBefore = playerStat->successfulJAW;
      uint8 flagsBefore = playerStat->flags;
			float damageDealtBefore = playerStat->damageDealt;

			playerStat->NetSerialize(ser);

			if (ser.IsReading())
			{
				CRY_ASSERT(!gEnv->bServer);

				if (playerStat->deathsThisRound > deathsThisRoundBefore)
				{
					if (CGameRules* pGameRules=g_pGame->GetGameRules())
					{
						pGameRules->ClPlayerStatsNetSerializeReadDeath_NotifyListeners(playerStat, deathsThisRoundBefore, flagsBefore);  // (for non-server clients only)
					}

					// this is done here so that all stats from this kill have been updated.
					// this should hopefully fix problems with one life modes incorrectly showing 
					// the respawn timer countdown
					if (gEnv->bMultiplayer)
					{
						const EntityId localClientId = gEnv->pGameFramework->GetClientActorId();
						if( localClientId == playerStat->playerId)
						{
							CHUDEventDispatcher::CallEvent(SHUDEvent( eHUDEvent_OnLocalPlayerDeath ));
						}
					}
				}

        int diff = abs(playerStat->successfulFlashbangs - flashbangsBefore) 
										+ abs(playerStat->successfulFrags - fragsBefore) 
										+ abs(playerStat->successfulC4 - c4before)
										+ abs(playerStat->successfulLTAG - ltagBefore)
										+ abs(playerStat->successfulJAW - jawBefore);

				if( diff != 0 && playerStat->playerId == g_pGame->GetIGameFramework()->GetClientActorId())  //!= allows for wrapping back to 0
				{
					float damageDealtThisExplosion = (playerStat->damageDealt - damageDealtBefore);
					if ( damageDealtThisExplosion > 0.0f )
					{
						CPersistantStats::GetInstance()->IncrementClientStats(EFPS_DamageDelt,damageDealtThisExplosion);						
					}

          if ( playerStat->successfulFlashbangs != flashbangsBefore )
          {
            IncrementWeaponHitsStat("FlashBangGrenades");
          }

          if ( playerStat->successfulFrags != fragsBefore )
          {
            IncrementWeaponHitsStat("FragGrenades");
          }

          if ( playerStat->successfulC4 != c4before )
          {
            IncrementWeaponHitsStat("C4");
          }

					if(playerStat->successfulLTAG != ltagBefore)
					{
						IncrementWeaponHitsStat("LTag");
					}

					if(playerStat->successfulJAW != jawBefore)
					{
						IncrementWeaponHitsStat("JAW");
					}

					SendHUDExplosionEvent();
				}
			}

#if 0
			if (pointsBefore != playerStat->points)
			{
				assert (ser.IsReading());

#if DISPLAY_POINTS_ON_HUD
				if (gEnv->IsClient() && (playerId == gEnv->pGameFramework->GetClientActorId()))
				{
					if (CHUD *pHUD = g_pGame->GetUI())
					{
						string message;
						message.Format("%d", (playerStat->points - pointsBefore));
						pHUD->DisplayFunMessage(message.c_str(), NULL);
					}
				}
#endif
			}
#endif
		}
		else
		{
			SGameRulesPlayerStat dummyStat(0);
			dummyStat.NetSerialize(ser);
		}
	}

	return true;
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::CreatePlayerStats(EntityId playerId, uint32 channelId)
{
	if (!GetPlayerStats(playerId))
	{
		CryLog("Add New Player Stats for entity=%d", playerId);
		m_playerStats.push_back(SGameRulesPlayerStat(playerId));
		SGameRulesPlayerStat *pStats = &(*m_playerStats.rbegin());
		pStats->bUseTimeSurvived = m_bRecordTimeSurvived;

		CGameRules *pGameRules = g_pGame->GetGameRules();
		IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
		if (gEnv->bServer)
		{
			// Check if the player can spawn this round
			IGameRulesSpawningModule *pSpawningModule = pGameRules->GetSpawningModule();
			if (pSpawningModule)
			{
				if (!pSpawningModule->SvIsMidRoundJoiningAllowed())
				{
					if (!pSpawningModule->IsInInitialChannelsList(channelId))
					{
						CryLog("CGameRulesStandardPlayerStats::CreatePlayerStats() player id %u (channel %u) as joined too late, adding PLYSTATFL_CANTSPAWNTHISROUND flag", playerId, channelId);
						pStats->flags |= SGameRulesPlayerStat::PLYSTATFL_CANTSPAWNTHISROUND;
						// No need to mark aspect as changed as we're still inside the player init - haven't setup the 
						// network serialisation chunk yet
					}
				}
			}

			if (pStateModule)
			{
				if (pStateModule->GetGameState() == IGameRulesStateModule::EGRS_PreGame ||
						(pStateModule->GetGameState() == IGameRulesStateModule::EGRS_Intro))
				{
					pStats->flags |= SGameRulesPlayerStat::PLYSTATFL_USEINITIALSPAWNS;
				}
			}
		}

		return;
	}
	else
	{
		GameWarning("CreatePlayerStats Stats already exist for player '%d'", playerId);
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::RemovePlayerStats(EntityId playerId)
{
	for (TPlayerStats::iterator it=m_playerStats.begin(); it!=m_playerStats.end(); ++it)
	{
		if (it->playerId == playerId)
		{
			CryLog("Remove Player Stats for entity=%d", playerId);
			m_playerStats.erase(it);
			return;
		}
	}
	
	GameWarning("RemovePlayerStats No stats found for player '%d'", playerId);
}

//-------------------------------------------------------------------------
// only currently called from ENTITY_EVENT_RESET in the editor
void CGameRulesStandardPlayerStats::ClearAllPlayerStats()
{
	for (TPlayerStats::iterator it=m_playerStats.begin(); it!=m_playerStats.end(); ++it)
	{
		// [tlh] this is extremely dodgy, but the player spawning happens before this clear function is called and we need the spawn count to be correct.
		//       also, is this clear function actually needed at all, because between pre-game and ingame the playerstats are recreated anyway...?

		uint8 flagsToSave=(	SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND|
												SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISGAME );

		uint8 preservedFlags = it->flags & (flagsToSave);
		bool bPreservedUseTimeSurvived = it->bUseTimeSurvived;

		it->Clear();

		it->flags |= preservedFlags;
		it->bUseTimeSurvived = bPreservedUseTimeSurvived;

		INetContext* pNetContext = g_pGame->GetIGameFramework()->GetNetContext();
		if(pNetContext)
		{
			pNetContext->ChangedAspects(it->playerId, CPlayer::ASPECT_PLAYERSTATS_SERVER);
		}
	}
}

//-------------------------------------------------------------------------
SGameRulesPlayerStat * CGameRulesStandardPlayerStats::GetPlayerStatsInternal(EntityId playerId)
{
	int num = m_playerStats.size();
	for (int i=0; i < num; ++i)
	{
		if (m_playerStats[i].playerId == playerId)
		{
			return &m_playerStats[i];
		}
	}

	return 0;
}

//-------------------------------------------------------------------------
int CGameRulesStandardPlayerStats::GetNumPlayerStats() const
{
	return m_playerStats.size();
}

//-------------------------------------------------------------------------
const SGameRulesPlayerStat* CGameRulesStandardPlayerStats::GetNthPlayerStats(int n)
{
	CRY_ASSERT(n>=0 && n<(int)m_playerStats.size());
	return &m_playerStats[n];
}

//-------------------------------------------------------------------------
const SGameRulesPlayerStat * CGameRulesStandardPlayerStats::GetPlayerStats(EntityId playerId)
{
	return GetPlayerStatsInternal(playerId);
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::OnPlayerKilled(const HitInfo &info)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (!pGameRules)
	{
		return;
	}

	if (pGameRules->HasGameActuallyStarted() == false)
	{
		return;
	}

	// No scoring at game end
	IGameRulesStateModule *stateModule = pGameRules->GetStateModule();
	if (stateModule != NULL && stateModule->GetGameState() == IGameRulesStateModule::EGRS_PostGame)
	{
		return;
	}

	bool  isSuddenDeath = false; 

	IGameRulesRoundsModule *roundsModule = pGameRules->GetRoundsModule();
	if (roundsModule)
	{
		isSuddenDeath=roundsModule->IsInSuddenDeath();
	}


	IActorSystem* pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
	IActor *targetActor =  pActorSystem->GetActor(info.targetId);
	IActor *shooterActor =  pActorSystem->GetActor(info.shooterId);

	if (targetActor)
	{
		SGameRulesPlayerStat *playerStat = GetPlayerStatsInternal(info.targetId);
		if (playerStat)
		{
			const int  deathsThisRoundBefore = playerStat->deathsThisRound;
			const int  flagsBefore = playerStat->flags;

			playerStat->deaths++;
			playerStat->deathsThisRound++;
			if (isSuddenDeath)
			{
				playerStat->flags |= SGameRulesPlayerStat::PLYSTATFL_DIEDINEXTRATIMETHISROUND;
			}

			if (gEnv->bServer)
			{
				if (gEnv->IsClient())
				{
					pGameRules->ClPlayerStatsNetSerializeReadDeath_NotifyListeners(playerStat, deathsThisRoundBefore, flagsBefore);  // (for non-dedicated server's client only)
				}

				INetContext* pNetContext = g_pGame->GetIGameFramework()->GetNetContext();
				if(pNetContext)
				{
					pNetContext->ChangedAspects(info.targetId, CPlayer::ASPECT_PLAYERSTATS_SERVER);
				}
			}
		}
	}

	if (shooterActor && (shooterActor != targetActor))
	{
		SGameRulesPlayerStat *playerStats = GetPlayerStatsInternal(info.shooterId);
		if (playerStats)
		{
			int targetTeam = pGameRules->GetTeam(info.targetId);
			int shooterTeam = pGameRules->GetTeam(info.shooterId);

			if ((pGameRules->GetTeamCount() > 1) && targetTeam == shooterTeam)	// Team Kill
			{
				playerStats->teamKills++;
			}
			else
			{
				playerStats->kills++;
				
				if (targetActor && targetActor->IsPlayer() && static_cast<CActor*>(targetActor)->IsHeadShot(info))
				{
					playerStats->headshots++;
				}
			}

			if (gEnv->bServer)
			{
				INetContext* pNetContext = g_pGame->GetIGameFramework()->GetNetContext();
				if(pNetContext)
				{
					pNetContext->ChangedAspects(info.shooterId, CPlayer::ASPECT_PLAYERSTATS_SERVER);
				}
			}
		}
	}

	if (gEnv->bMultiplayer && gEnv->bServer)		// always true
	{
		if (gEnv->IsClient())
		{
			// this is done here so that all stats from this kill have been updated.
			// this should hopefully fix problems with one life modes incorrectly showing 
			// the respawn timer countdown
			const EntityId localClientId = gEnv->pGameFramework->GetClientActorId();
			if( localClientId == info.targetId )
			{
				CHUDEventDispatcher::CallEvent(SHUDEvent( eHUDEvent_OnLocalPlayerDeath ));
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::IncreasePoints(EntityId playerId, int amount)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules && !pGameRules->HasGameActuallyStarted())
	{
		return;
	}

	SGameRulesPlayerStat *playerStat = GetPlayerStatsInternal(playerId);
	if (playerStat)
	{
		int newPoints = playerStat->points += amount;
		if (newPoints > 0)
		{
			playerStat->points = newPoints;
		}
		else
		{
			playerStat->points = 0x7FFFFFFF; // max amount of points with signed 32bit integer.
		}

		if (gEnv->bServer)
		{
			INetContext *pNetContext = g_pGame->GetIGameFramework()->GetNetContext();
			if(pNetContext)
			{
				pNetContext->ChangedAspects(playerId, CPlayer::ASPECT_PLAYERSTATS_SERVER);
			}
		}

#if DISPLAY_POINTS_ON_HUD
		if (gEnv->bServer && gEnv->IsClient() && (playerId == gEnv->pGameFramework->GetClientActorId()))
		{
			if (CHUD *pHUD = g_pGame->GetUI())
			{
				string message;
				message.Format("%d", amount);	// TODO: Handle float amounts
				pHUD->DisplayFunMessage(message.c_str(), NULL);
			}
		}
#endif
	}
	else
	{
		CryLog ("CGameRulesStandardPlayerStats::IncreasePoints failed to find SGameRulesPlayerStat for %s", g_pGame->GetGameRules()->GetEntityName(playerId));
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::IncreaseGamemodePoints(EntityId playerId, int amount)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules && !pGameRules->HasGameActuallyStarted())
	{
		return;
	}

	SGameRulesPlayerStat *playerStat = GetPlayerStatsInternal(playerId);
	if (playerStat)
	{
		playerStat->gamemodePoints += amount;

		if (gEnv->bServer)
		{
			INetContext *pNetContext = g_pGame->GetIGameFramework()->GetNetContext();
			if(pNetContext)
			{
				pNetContext->ChangedAspects(playerId, CPlayer::ASPECT_PLAYERSTATS_SERVER);
			}
		}
	}
	else
	{
		CryLog ("CGameRulesStandardPlayerStats::IncreaseGamemodePoints failed to find SGameRulesPlayerStat for %s", g_pGame->GetGameRules()->GetEntityName(playerId));
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::EntityRevived(EntityId entityId)
{
	if (gEnv->bServer)
	{
		if (SGameRulesPlayerStat* s=GetPlayerStatsInternal(entityId))
		{
			s->flags |= SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND;

			if (gEnv->bMultiplayer && gEnv->bServer)
			{
				SHUDEvent event(eHUDEvent_OnPlayerRevive);
				event.AddData((int)entityId);
				CHUDEventDispatcher::CallEvent(event);
			}

#if !defined(_RELEASE)
			if (!(s->flags & SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISGAME))
			{	
				CGameRules  *pGameRules = g_pGame->GetGameRules();
				CryLog("CGameRulesStandardPlayerStats::EntityRevived() player=%s is newly setting PLYSTATFL_HASSPAWNEDTHISGAME", pGameRules->GetEntityName(entityId));
			}
#endif

			if(g_pGame->GetGameRules()->HasGameActuallyStarted())
				s->flags |= SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISGAME;

			INetContext* pNetContext = g_pGame->GetIGameFramework()->GetNetContext();
			if(pNetContext)
			{
				pNetContext->ChangedAspects(entityId, CPlayer::ASPECT_PLAYERSTATS_SERVER);
			}
		}
	}
}

//------------------------------------------------------------------------
// IGameRulesRoundsListener
void CGameRulesStandardPlayerStats::OnRoundEnd()
{
	CryLog("CGameRulesStandardPlayerStats::OnRoundEnd()");

	// the server will sync these PLYSTATFL_HASHADROUNDRESTART flags to everyone anyway, but there's no harm in setting the flag locally for everyone on all the clients anyway, just in case there's a bit of network lag or something

	INetContext*  pNetContext = g_pGame->GetIGameFramework()->GetNetContext();

	const unsigned int  numPlayers = m_playerStats.size();
	for (unsigned int i=0; i<numPlayers; i++)
	{
		SGameRulesPlayerStat*  pStats = &m_playerStats[i];
		pStats->flags |= SGameRulesPlayerStat::PLYSTATFL_HASHADROUNDRESTART;

		if (gEnv->bServer)
		{
			if (pNetContext)
			{
				pNetContext->ChangedAspects(pStats->playerId, CPlayer::ASPECT_PLAYERSTATS_SERVER);
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::ProcessSuccessfulExplosion(EntityId playerId, float damageDealt, uint16 projectileClassId)
{
 	CRY_ASSERT_MESSAGE(gEnv->bServer, "ProcessSuccessfulExplosion should only be called from the server");

	if (SGameRulesPlayerStat* stats = GetPlayerStatsInternal(playerId))
	{
    bool flash = (projectileClassId == m_classidFlash);
    bool frag = (projectileClassId == m_classidFrag);
    bool c4 = (projectileClassId == m_classidC4);
		bool ltag = (projectileClassId == m_classidLTAG);
		bool jaw = (projectileClassId == m_classidJawRocket);

    if (flash)
    {
		  stats->successfulFlashbangs++;
    }
		else if (frag)
    {
      stats->successfulFrags++;
    }
		else if (c4)
    {
      stats->successfulC4++;
    }
		else if(ltag)
		{
			stats->successfulLTAG++;
		}
		else if(jaw)
		{
			stats->successfulJAW++;
		}

		stats->damageDealt += damageDealt;

		if(gEnv->IsClient() && playerId == g_pGame->GetIGameFramework()->GetClientActorId())
		{
      if ( damageDealt > 0.0f )
      {
        CPersistantStats::GetInstance()->IncrementClientStats(EFPS_DamageDelt,damageDealt);						
      }

      if (flash)
      {
				IncrementWeaponHitsStat("FlashBangGrenades");
      }
      else if (frag)
      {
				IncrementWeaponHitsStat("FragGrenades");
      }
			else if (c4)
      {
				IncrementWeaponHitsStat("C4");
      }
			else if (ltag)
			{
				IncrementWeaponHitsStat("LTag");
			}
			else if (jaw)
			{
				IncrementWeaponHitsStat("JAW");
			}

			SendHUDExplosionEvent();
		}
		else
		{
			INetContext* pNetContext = g_pGame->GetIGameFramework()->GetNetContext();
			if(pNetContext)
			{
				pNetContext->ChangedAspects(playerId, CPlayer::ASPECT_PLAYERSTATS_SERVER);
			}
		}
	}
}

void CGameRulesStandardPlayerStats::IncrementAssistKills(EntityId playerId)
{
	CRY_ASSERT_MESSAGE(gEnv->bServer, "IncrementAssistKills should only be called from the server");

	if (SGameRulesPlayerStat* stats = GetPlayerStatsInternal(playerId))
	{
		stats->assists++;

		INetContext* pNetContext = g_pGame->GetIGameFramework()->GetNetContext();
		if(pNetContext)
		{
			pNetContext->ChangedAspects(playerId, CPlayer::ASPECT_PLAYERSTATS_SERVER);
		}
	}
}

void CGameRulesStandardPlayerStats::SendHUDExplosionEvent()
{
	SHUDEventWrapper::HitTarget( EGRTT_Hostile, static_cast<int>(eHUDEventHT_Explosive), 0 );
}


//-------------------------------------------------------------------------
#if ENABLE_PLAYER_KILL_RECORDING
void CGameRulesStandardPlayerStats::IncreaseKillCount( EntityId playerId, EntityId victimId )
{
	SGameRulesPlayerStat *playerStat = GetPlayerStatsInternal(playerId);
	playerStat->IncrementTimesPlayerKilled( victimId );
}
#endif // ENABLE_PLAYER_KILL_RECORDING

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::SetEndOfGameStats( const CGameRules::SPlayerEndGameStatsParams &inStats )
{
	const int numResults = inStats.m_numPlayerStats;
	for (int i = 0; i < numResults; ++ i)
	{
		const CGameRules::SPlayerEndGameStatsParams::SPlayerEndGameStats &inPlayerStats = inStats.m_playerStats[i];

		SGameRulesPlayerStat *pPlayerStats = GetPlayerStatsInternal(inPlayerStats.m_playerId);
		if (pPlayerStats)
		{
			pPlayerStats->points = inPlayerStats.m_points;
			pPlayerStats->kills = inPlayerStats.m_kills;
			pPlayerStats->assists = inPlayerStats.m_assists;
			pPlayerStats->deaths = inPlayerStats.m_deaths;
			pPlayerStats->skillPoints = inPlayerStats.m_skillPoints;
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::CalculateSkillRanking()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	if (pGameRules != NULL && pGameRules->CanCalculateSkillRanking() && pGameLobby != NULL && pGameLobby->IsRankedGame())		// Only update skill ranking if we're in a ranked game
	{
		const float fGameStartTime = pGameRules->GetGameStartTime();
		const float fGameLength = (pGameRules->GetCurrentGameTime() * 1000.f);		// Spawn time is in milliseconds, length is in seconds

		// Calculate ranking changes
		CSkillRanking rankingCalculator;
		const unsigned int numPlayers = m_playerStats.size();
		for (unsigned int i =  0; i < numPlayers; ++ i)
		{
			SGameRulesPlayerStat *pPlayerStats = &m_playerStats[i];

			// TODO: Determine time in game, for now just assume everyone is in for the whole game
			float fFracTimeInGame = 0.f;

			CPlayer *pPlayer = static_cast<CPlayer *>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pPlayerStats->playerId));
			if (pPlayer)
			{
				const float fTimeSpawned = pPlayer->GetTimeFirstSpawned();
				const float fTimeInGame = (fGameLength + fGameStartTime) - fTimeSpawned;
				if (fGameLength > 0.f)
				{
					fFracTimeInGame = (fTimeInGame / fGameLength);
					fFracTimeInGame = CLAMP(fFracTimeInGame, 0.f, 1.f);
				}

				const uint16 skillRanking = pGameLobby->GetSkillRanking(pPlayer->GetChannelId());

				if (fFracTimeInGame > 0.3f)
				{
					const int teamId = pGameRules->GetTeam(pPlayerStats->playerId);

					rankingCalculator.AddPlayer(pPlayerStats->playerId, skillRanking, pPlayerStats->points, teamId, fFracTimeInGame);
				}
				else
				{
					CryLog("CGameRulesStandardPlayerStats::CalculateSkillRanking(), player '%s' has not been in the game long enough (frac=%f)", pPlayer->GetEntity()->GetName(), fFracTimeInGame);
					pPlayerStats->skillPoints = skillRanking;
				}
			}
		}

		if (pGameRules->GetTeamCount() > 1)
		{
			rankingCalculator.TeamGameFinished(pGameRules->GetTeamsScore(1), pGameRules->GetTeamsScore(2));
		}
		else
		{
			rankingCalculator.NonTeamGameFinished();
		}

		for (unsigned int i = 0; i < numPlayers; ++ i)
		{
			SGameRulesPlayerStat *pPlayerStats = &m_playerStats[i];
			uint16 newSkillPoints = 0;
			if (rankingCalculator.GetSkillPoints(pPlayerStats->playerId, newSkillPoints))
			{
				pPlayerStats->skillPoints = newSkillPoints;
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::RecordSurvivalTime( EntityId playerId, float timeSurvived )
{
	SGameRulesPlayerStat *pStats = GetPlayerStatsInternal(playerId);
	if (pStats)
	{
		pStats->timeSurvived = timeSurvived;

		INetContext* pNetContext = g_pGame->GetIGameFramework()->GetNetContext();
		if (pNetContext)
		{
			pNetContext->ChangedAspects(playerId, CPlayer::ASPECT_PLAYERSTATS_SERVER);
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesStandardPlayerStats::IncrementWeaponHitsStat( const char *pWeaponName )
{
	CPersistantStats::GetInstance()->IncrementMapStats(EMPS_WeaponHits, pWeaponName);

}

#undef WATCH_LV1
#undef WATCH_LV1_ONLY
#undef WATCH_LV2
