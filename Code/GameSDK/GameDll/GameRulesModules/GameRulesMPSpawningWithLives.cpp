// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 04:11:2009 : Created by Thomas Houghton

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesMPSpawningWithLives.h"

#include "GameRules.h"
#include "IGameRulesStateModule.h"
#include "IGameRulesPlayerStatsModule.h"
#include "IGameRulesRoundsModule.h"
#include "Player.h"
#include "Utility/CryWatch.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "UI/HUD/HUDEventDispatcher.h"

#if CRY_WATCH_ENABLED
#define WATCH_LV1				m_dbgWatchLvl<1  ? (NULL) : CryWatch
#define WATCH_LV1_ONLY	m_dbgWatchLvl!=1 ? (NULL) : CryWatch
#define WATCH_LV2				m_dbgWatchLvl<2  ? (NULL) : CryWatch
#else
#define WATCH_LV1
#define WATCH_LV1_ONLY
#define WATCH_LV2
#endif

CGameRulesMPSpawningWithLives::CGameRulesMPSpawningWithLives()
	: m_freeElimMarker(nullptr)
	, m_numLives(0)
	, m_dbgWatchLvl(0)
	, m_elimMarkerDuration(0.0f)
	, m_bLivesDirty(false)
{
	memset(m_elimMarkers, 0, sizeof(m_elimMarkers));
}

CGameRulesMPSpawningWithLives::~CGameRulesMPSpawningWithLives()
{
	CGameRules  *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->UnRegisterPlayerStatsListener(this);
	}

	RemoveAllElimMarkers();
}

void CGameRulesMPSpawningWithLives::Init(XmlNodeRef xml)
{
	inherited::Init(xml);

	m_dbgWatchLvl = 0;
	m_elimMarkerDuration = 0.f;
	memset(m_elimMarkers, 0, sizeof(m_elimMarkers));
	m_freeElimMarker = &m_elimMarkers[0];

	if (xml->getAttr("numLives", m_numLives))
	{
		// Only assault mode should have this set, otherwise we go by the variants or frontends setting
		CRY_ASSERT_MESSAGE(g_pGame->GetGameRules()->GetGameMode() == eGM_Assault, string().Format("SpawningWithLives expects only the assault gamemode to override numLives within the xml. Yet gamemode=%d is trying to", g_pGame->GetGameRules()->GetGameMode()));
		CryLog("CGameRulesMPSpawningWithLives::Init() is setting numLives from the xml to %d", m_numLives);
	}
	else
	{
		m_numLives = g_pGameCVars->g_numLives;
		CryLog("CGameRulesMPSpawningWithLives::Init() has no numLives set in the xml. Using the current cvar value=%d", m_numLives);

		if (m_numLives <= 0)
		{
			m_numLives = 0;		// Make sure this isn't negative, horrible things would happen! (0 = infinite)
			return;
		}
	}

	xml->getAttr("dbgWatchLvl", m_dbgWatchLvl);
	xml->getAttr("eliminatedMarkerDuration", m_elimMarkerDuration);

	CRY_TODO(23,11,09, "Need to use localisable strings from string tables for these.");
	m_tmpPlayerEliminatedMsg.Format("%s", xml->getAttr("tmpPlayerEliminatedMsg"));
	m_tmpPlayerEliminatedMarkerTxt.Format("%s", xml->getAttr("tmpPlayerEliminatedMarkerTxt"));

	CGameRules  *pGameRules = g_pGame->GetGameRules();
	pGameRules->RegisterPlayerStatsListener(this);
}

void CGameRulesMPSpawningWithLives::Update(float frameTime)
{
	inherited::Update(frameTime);

	if (!m_numLives)
	{
		return;
	}

	bool  isSuddenDeath = IsInSuddenDeath(); 

	if (gEnv->IsClient() && (m_elimMarkerDuration > 0.f))
	{
		UpdateElimMarkers(frameTime);
	}

#if CRY_WATCH_ENABLED
	if (m_dbgWatchLvl >= 1)
	{
		if (gEnv->IsClient())
		{
			if (IGameRulesPlayerStatsModule* pPlayStatsMod=m_pGameRules->GetPlayerStatsModule())
			{
				{
					if (const SGameRulesPlayerStat* s=pPlayStatsMod->GetPlayerStats(g_pGame->GetIGameFramework()->GetClientActorId()))
					{
						int  livesRem = (m_numLives - s->deathsThisRound);
						if (isSuddenDeath && (livesRem > 0))
							livesRem = ((s->flags & SGameRulesPlayerStat::PLYSTATFL_DIEDINEXTRATIMETHISROUND) ? 0 : 1);
						WATCH_LV1("LIVES: %d", livesRem);
					}
					WATCH_LV1_ONLY(" ");
				}

				{
					WATCH_LV1("REMAINING:");
					EntityId  top4[4];
					int  count = 0;
					int  numStats = pPlayStatsMod->GetNumPlayerStats();
					for (int i=0; i<numStats; i++)
					{
						const SGameRulesPlayerStat*  s = pPlayStatsMod->GetNthPlayerStats(i);
						if ((s->flags & SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND) && (s->deathsThisRound < m_numLives))
						{
							if (!isSuddenDeath || !(s->flags & SGameRulesPlayerStat::PLYSTATFL_DIEDINEXTRATIMETHISROUND))
							{
								if (count < 4)
									top4[count] = s->playerId;
								count++;
							}
						}
					}
					if ((count > 4) || (count == 0))
					{
						WATCH_LV1("%d", count);
					}
					else
					{
						for (int i=0; i<count; i++)
						{
							IEntity*  e = gEnv->pEntitySystem->GetEntity(top4[i]);
							WATCH_LV1("%s", (e?e->GetName():"!"));
						}
					}
					WATCH_LV1_ONLY(" ");
				}
			}
		}
	}

	if (m_dbgWatchLvl >= 2)
	{
		WATCH_LV2("[CGameRulesMPSpawningWithLives]");

		if (gEnv->bServer)
		{
			WATCH_LV2("Kills sum = %d", m_svKillsSum);
		}

		WATCH_LV2("Player lives:");
		if (IGameRulesPlayerStatsModule* pPlayStatsMod=m_pGameRules->GetPlayerStatsModule())
		{
			int  numStats = pPlayStatsMod->GetNumPlayerStats();
			for (int i=0; i<numStats; i++)
			{
				const SGameRulesPlayerStat*  s = pPlayStatsMod->GetNthPlayerStats(i);
				EntityId  eid = s->playerId;
				IEntity*  e = gEnv->pEntitySystem->GetEntity(eid);
				int  livesRem = (m_numLives - s->deathsThisRound);
				WATCH_LV2("%d '%s': %d lives", eid, (e?e->GetName():"!"), livesRem);
			}
		}
	}
#endif

	if (m_bLivesDirty)
	{
		SvNotifySurvivorCount();
		m_bLivesDirty = false;
	}
}

void CGameRulesMPSpawningWithLives::PerformRevive(EntityId playerId, int teamId, EntityId preferredSpawnId)
{
	CRY_ASSERT(gEnv->bServer);

	bool  isSuddenDeath = IsInSuddenDeath();
	
	bool bGameHasStarted = m_pGameRules->HasGameActuallyStarted();

	if ((!m_numLives && !isSuddenDeath) || !bGameHasStarted)
	{
		inherited::PerformRevive(playerId, teamId, preferredSpawnId);
		return;
	}

	CPlayer*  pPlayer = (CPlayer*) gEnv->pGameFramework->GetIActorSystem()->GetActor(playerId);
	IEntity*  pEntity = gEnv->pEntitySystem->GetEntity( playerId );
	CryLog("[tlh] @ CGameRulesMPSpawningWithLives::PerformRevive(playerId=%d, teamId=%d), player='%s'", playerId, teamId, (pEntity?pEntity->GetName():"!"));

	if( pPlayer != NULL && (pPlayer->IsDead() || pPlayer->GetSpectatorMode() != CActor::eASM_None/*Spectating*/) )
	{
		bool  perform = true;

		if (IGameRulesStateModule* pStateModule=m_pGameRules->GetStateModule())
		{
			CryLog("      > game state = %d", pStateModule->GetGameState());

			switch (pStateModule->GetGameState())
			{
			case IGameRulesStateModule::EGRS_Reset:
			case IGameRulesStateModule::EGRS_InGame:
			case IGameRulesStateModule::EGRS_PostGame:
				{
					if (IGameRulesPlayerStatsModule* pPlayStatsMod=m_pGameRules->GetPlayerStatsModule())
					{
						if (const SGameRulesPlayerStat* s=pPlayStatsMod->GetPlayerStats(playerId))
						{
							if (!isSuddenDeath || !(s->flags & SGameRulesPlayerStat::PLYSTATFL_DIEDINEXTRATIMETHISROUND))
							{
								// may need to allow spawning for the first few seconds of each new round so that late joiners the previous round (who haven't spawned this game) can join
								// somehow first round fresh spawners get in, and most of the time subsequent round spawners get in. Adding a window where spawns are always allowed and 
								// changing the check to spawned this game should hopefully tidy up any out of order problems we're currently having

								// All or nothing will set m_allowMidRoundJoiningBeforeFirstDeath
								// this also stops CGameRulesMPSpawningBase from stopping mid-game joins by restricting it to only the players present in the lobby at game start

								
								// this if just allows through any spawn when midRoundJoiningBeforeFirstDeath==false
								// presumably as in this case logic elsewhere deals with whether its a midround join before trying to perform revive?
								if ( !m_allowMidRoundJoiningBeforeFirstDeath || 
										(m_svKillsSum == 0) || 
										(s->flags & SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISGAME) || 
										(pPlayer->GetSpectatorState() == CActor::eASS_ForcedEquipmentChange) ||
										(IsInInitialChannelsList(pPlayer->GetChannelId())) ) // mid-game joining is only allowed whilst no deaths have happened this round
								{
									if (s->deathsThisRound < m_numLives)
									{
										CryLog("      > performing life-limited revive, frame = %d", gEnv->pRenderer->GetFrameID());
										CCCPOINT(MPSpawningWithLives_SvPerformReviveAllowed);
									}
									else
									{
										perform = false;
										CryLog("      > not reviving player %s - no lives remaining", pPlayer->GetEntity()->GetName());
										CCCPOINT(MPSpawningWithLives_SvPerformReviveDenied_NoLives);
									}
								}
								else
								{
									perform = false;
									CryLog("      > not reviving player %s - others died already", pPlayer->GetEntity()->GetName());
									CCCPOINT(MPSpawningWithLives_SvPerformReviveDenied_OthersDiedAlready);
								}
							}
							else
							{
								perform = false;
								CCCPOINT(MPSpawningWithLives_SvPerformReviveDenied_SuddenDeath);
								CryLog("      > not reviving player %s - sudden death", pPlayer->GetEntity()->GetName());
							}
						}
						else
						{
							CRY_ASSERT_MESSAGE(0, string().Format("CGameRulesMPSpawningWithLives::PerformRevive() failed to find playerstats for player"));
						}
					}
					break;
				}
			}
		}

		if (perform)
		{
			inherited::PerformRevive(playerId, teamId, preferredSpawnId);
		}
	}
	else
	{
		CryLog("      > player is still/already alive");
	}
}

void CGameRulesMPSpawningWithLives::EntityRevived(EntityId entityId)
{
	inherited::EntityRevived(entityId);
	if (gEnv->bServer)
	{
		CCCPOINT(MPSpawningWithLives_SvListenedEntityRevived);
		if (IGameRulesPlayerStatsModule* pPlayStatsMod=m_pGameRules->GetPlayerStatsModule())
		{
			if (const SGameRulesPlayerStat* s=pPlayStatsMod->GetPlayerStats(entityId))
			{
				CRY_ASSERT_MESSAGE((s->flags & SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND), "The PlayerStats module needs to come before the Spawning module in the XML for this game mode.");
				if (s->deathsThisRound == 0)
				{
					CCCPOINT(MPSpawningWithLives_SvListenedEntityRevived_SurvivorChangeNotified);
					m_bLivesDirty = true;
				}
			}
		}
	}
}

// IGameRulesPlayerStatsListener
void CGameRulesMPSpawningWithLives::ClPlayerStatsNetSerializeReadDeath(const SGameRulesPlayerStat* s, uint16 prevDeathsThisRound, uint8 prevFlags)
{
	CryLog("[tlh] @ CGameRulesMPSpawningWithLives::ClPlayerStatsNetSerializeReadDeath(s=%p, prevDeathsThisRound=%u, prevFlags=%u)", s, prevDeathsThisRound, prevFlags);
	CRY_ASSERT(gEnv->IsClient());
	if (!gEnv->bServer)
	{
		if ((s->deathsThisRound >= m_numLives) || (s->flags & SGameRulesPlayerStat::PLYSTATFL_DIEDINEXTRATIMETHISROUND))
		{
			OnPlayerElimination(s->playerId);
		}
	}
}

// IGameRulesRoundsListener
void CGameRulesMPSpawningWithLives::ClRoundsNetSerializeReadState(int newState, int curState)
{
	CRY_ASSERT(!gEnv->bServer);
	CRY_ASSERT(gEnv->IsClient());
	IGameRulesRoundsModule*  pRoundsModule = m_pGameRules->GetRoundsModule();
	CRY_ASSERT(pRoundsModule);
	if (pRoundsModule->IsRestartingRound(curState))
	{
		RemoveAllElimMarkers();
	}
}

void CGameRulesMPSpawningWithLives::OnPlayerElimination(EntityId playerId)
{
	if (!m_tmpPlayerEliminatedMsg.empty())
	{
		/*
		string  msg;
		IEntity*  e = gEnv->pEntitySystem->GetEntity(playerId);
		msg.Format("%s%s", (e?e->GetName():"?"), m_tmpPlayerEliminatedMsg.c_str());

		const char* sz_msg = msg.c_str();
		*/

		CRY_TODO( 22, 07, 2010, "HUD: Notify player of player elimination in All or Nothing.")
	}

	if (gEnv->IsClient() && (m_elimMarkerDuration > 0.f))
	{
		if (playerId != g_pGame->GetIGameFramework()->GetClientActorId())
		{
			CreateElimMarker(playerId);
		}
	}

}

// this gets called between rounds
void CGameRulesMPSpawningWithLives::ReviveAllPlayers(bool isReset, bool bOnlyIfDead)
{
	CRY_ASSERT(gEnv->bServer);

	if (!m_numLives)
	{
		inherited::ReviveAllPlayers(isReset, bOnlyIfDead);
		return;
	}

	RemoveAllElimMarkers();

	inherited::ReviveAllPlayers(isReset, bOnlyIfDead);

	if (!isReset)
	{
		CCCPOINT(MPSpawningWithLives_SvReviveAllPlayers_SurvivorChangeNotified);
		m_bLivesDirty = true;
	}
}

void CGameRulesMPSpawningWithLives::PlayerJoined(EntityId playerId)
{
	if (m_numLives)
	{
		CRY_ASSERT(!m_pGameRules->GetPlayerStatsModule() || m_pGameRules->GetPlayerStatsModule()->GetPlayerStats(playerId));  // it's important that the playerstats for this player are created before this function is called
		if (gEnv->bServer)
		{
			CCCPOINT(MPSpawningWithLives_SvPlayerJoined_SurvivorChangeNotified);
			m_bLivesDirty = true;
		}
	}
	inherited::PlayerJoined(playerId);
}

void CGameRulesMPSpawningWithLives::PlayerLeft(EntityId playerId)
{
	if (m_numLives)
	{
		CRY_ASSERT(!m_pGameRules->GetPlayerStatsModule() || !m_pGameRules->GetPlayerStatsModule()->GetPlayerStats(playerId));  // it's important that the playerstats for the player are removed before this function is called
		if (gEnv->bServer)
		{
			CCCPOINT(MPSpawningWithLives_SvPlayerLeft_SurvivorChangeNotified);
			m_bLivesDirty = true;
		}
	}
	inherited::PlayerLeft(playerId);
}

// IGameRulesKillListener
void CGameRulesMPSpawningWithLives::OnEntityKilled(const HitInfo &hitInfo)
{
	inherited::OnEntityKilled(hitInfo);

	if (gEnv->bServer)
	{
		EntityId  entityId = hitInfo.targetId;
		IEntity*  e = gEnv->pEntitySystem->GetEntity(entityId);
		CryLog("[tlh] @ CGameRulesMPSpawningWithLives::OnEntityKilled(), entity=%s", (e?e->GetName():"!"));
		CCCPOINT(MPSpawningWithLives_ListenedEntityKilled);
		if (IGameRulesPlayerStatsModule* pPlayStatsMod=m_pGameRules->GetPlayerStatsModule())
		{
			IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId);
			if (pActor != NULL && pActor->IsPlayer())
			{
				if (const SGameRulesPlayerStat* s=pPlayStatsMod->GetPlayerStats(entityId))  // NOTE that the members of 's' won't necessarily have been synced on clients yet
				{
					CryLog("      > got OnEntityKilled broadcast, frame = %d", gEnv->pRenderer->GetFrameID());
					bool  isSuddenDeath = IsInSuddenDeath(); 
					if (IGameRulesStateModule* pStateModule=m_pGameRules->GetStateModule())
					{
						if (pStateModule->GetGameState() == IGameRulesStateModule::EGRS_InGame)
						{
							if (isSuddenDeath || (s->deathsThisRound >= m_numLives))
							{
								OnPlayerElimination(s->playerId);

								CCCPOINT(MPSpawningWithLives_SvListenedEntityKilled_SurvivorChangeNotified);
								m_bLivesDirty = true;
							}
						}
					}
				}
				else
				{
					CRY_ASSERT_MESSAGE(0, string().Format("CGameRulesMPSpawningWithLives::OnEntityKilled() failed to find a playerstats for player"));
				}
			}
		}
	}
}

void CGameRulesMPSpawningWithLives::SvNotifySurvivorCount()
{
	CryLog("[tlh] @ CGameRulesMPSpawningWithLives::SvNotifySurvivorCount()");
	CRY_ASSERT(gEnv->bServer);
	EntityId  survivors[MAX_PLAYER_LIMIT];
	int  count = 0;
	bool  isSuddenDeath = IsInSuddenDeath(); 
	if (IGameRulesPlayerStatsModule* pPlayStatsMod=m_pGameRules->GetPlayerStatsModule())
	{
		int  numStats = pPlayStatsMod->GetNumPlayerStats();
		for (int i=0; i<numStats; i++)
		{
			const SGameRulesPlayerStat*  s = pPlayStatsMod->GetNthPlayerStats(i);
			if (s->flags & SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND)
			{
				if (s->deathsThisRound < m_numLives)
				{
					if (!isSuddenDeath || !(s->flags & SGameRulesPlayerStat::PLYSTATFL_DIEDINEXTRATIMETHISROUND))
					{
						if (count < MAX_PLAYER_LIMIT)
						{
							survivors[count] = s->playerId;
							count++;
						}
						else
						{
							CRY_ASSERT(0);
						}
						{
							IEntity*  e = gEnv->pEntitySystem->GetEntity(s->playerId);
							CryLog("      > added player '%s' to survivor array at idx %d", (e?e->GetName():"!"), (count - 1));
						}
					}
				}
			}
		}
	}
	g_pGame->GetGameRules()->SvSurvivorCountRefresh_NotifyListeners(count, survivors, m_svKillsSum);
}

int CGameRulesMPSpawningWithLives::GetRemainingLives(EntityId playerId)
{
	if (IGameRulesStateModule* pStateModule = m_pGameRules->GetStateModule())
	{
		if (pStateModule->GetGameState() != IGameRulesStateModule::EGRS_PreGame)
		{
			if (IGameRulesPlayerStatsModule* pPlayStatsMod=m_pGameRules->GetPlayerStatsModule())
			{
				if (const SGameRulesPlayerStat* s=pPlayStatsMod->GetPlayerStats(playerId))
				{

					bool  isSuddenDeath = IsInSuddenDeath();

					int  rem = (m_numLives - s->deathsThisRound);
					
					//CryLog("CGameRulesMPSpawningWithLives::GetRemainingLives() found stats for player - m_numLives=%d; deathsThisRound=%d; rem=%d; isSuddenDeath=%d; g_pGameCVars->g_timelimitextratime=%f; m_pGameRules->GetRemainingGameTimeNotZeroCapped()=%f", m_numLives, s->deathsThisRound, rem, isSuddenDeath, g_pGameCVars->g_timelimitextratime, m_pGameRules->GetRemainingGameTimeNotZeroCapped());
					if (isSuddenDeath && (rem > 0 || m_numLives==0))
					{
						return ((s->flags & SGameRulesPlayerStat::PLYSTATFL_DIEDINEXTRATIMETHISROUND) ? 0 : 1);
					}
					else
					{
						return rem;
					}
				}
			}
		}
		else
		{
			return m_numLives; // Pre-game stick to max
		}
	}

	return 0;
}

void CGameRulesMPSpawningWithLives::CreateElimMarker(EntityId playerId)
{
	CRY_ASSERT(m_elimMarkerDuration > 0.f);
	if (IEntity* e=gEnv->pEntitySystem->GetEntity(playerId))
	{
		CRY_ASSERT_MESSAGE(m_freeElimMarker, "Ran out of space in ElimMarkers array.");
		if (m_freeElimMarker)
		{
			SEntitySpawnParams  params;
			params.sName = string().Format(" %s%s ", e->GetName(), m_tmpPlayerEliminatedMarkerTxt.c_str());
			CRY_ASSERT(gEnv->pGameFramework->GetIActorSystem()->GetActor(gEnv->pEntitySystem->FindEntityByName(e->GetName())->GetId())->IsPlayer());
			params.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
			params.nFlags = (ENTITY_FLAG_SPAWNED | ENTITY_FLAG_CLIENT_ONLY);
			params.vPosition = e->GetWorldPos();
			params.qRotation = e->GetWorldRotation();
			IEntity*  pMarkerEnt = gEnv->pEntitySystem->SpawnEntity(params);
			CRY_ASSERT_MESSAGE(pMarkerEnt, "Couldn't create local client entity for ElimMarker.");
			if (pMarkerEnt)
			{
				SElimMarker*  pMarker = m_freeElimMarker;

				SElimMarker*  m;
				for (m=(m_freeElimMarker+1); m!=m_freeElimMarker; m++)
				{
					if (m >= (m_elimMarkers + m_kMaxElimMarkers))
						m = &m_elimMarkers[0];
					if (m->markerEid == 0)
					{
						break;
					}
				}
				if (m != m_freeElimMarker)
					m_freeElimMarker = m;
				else
					m_freeElimMarker = NULL;
				CRY_ASSERT_MESSAGE(!m_freeElimMarker || ((m_freeElimMarker >= &m_elimMarkers[0]) && (m_freeElimMarker <= &m_elimMarkers[m_kMaxElimMarkers - 1])), "Stomping memory!!");

				pMarker->playerEid = playerId;
				pMarker->markerEid = pMarkerEnt->GetId();
				pMarker->time = 0.f;
			}
		}
	}
}

void CGameRulesMPSpawningWithLives::UpdateElimMarkers(float frameTime)
{
	for (int i=0; i<m_kMaxElimMarkers; i++)
	{
		SElimMarker*  pMarker = &m_elimMarkers[i];
		if (pMarker->markerEid)
		{
			CRY_ASSERT(gEnv->pEntitySystem->GetEntity(pMarker->markerEid));
			pMarker->time += frameTime;
			if (pMarker->time <= m_elimMarkerDuration)
			{
				//...
			}
			else
			{
				gEnv->pEntitySystem->RemoveEntity(pMarker->markerEid, false);
				pMarker->markerEid = 0;
				if (!m_freeElimMarker)
				{
					m_freeElimMarker = pMarker;
				}
			}
		}
	}
}

void CGameRulesMPSpawningWithLives::RemoveAllElimMarkers()
{
	for (int i=0; i<m_kMaxElimMarkers; i++)
	{
		SElimMarker*  pMarker = &m_elimMarkers[i];
		if (pMarker->markerEid)
		{
			CRY_ASSERT(gEnv->pEntitySystem->GetEntity(pMarker->markerEid));
			gEnv->pEntitySystem->RemoveEntity(pMarker->markerEid, false);
			pMarker->markerEid = 0;
		}
	}
	m_freeElimMarker = &m_elimMarkers[0];
}

bool CGameRulesMPSpawningWithLives::IsInSuddenDeath()
{
	bool isSuddenDeath=false;

	IGameRulesRoundsModule *roundsModule = m_pGameRules->GetRoundsModule();
	if (roundsModule)
	{
		isSuddenDeath=roundsModule->IsInSuddenDeath();
	}

	return isSuddenDeath;
}

#undef WATCH_LV1
#undef WATCH_LV1_ONLY
#undef WATCH_LV2
