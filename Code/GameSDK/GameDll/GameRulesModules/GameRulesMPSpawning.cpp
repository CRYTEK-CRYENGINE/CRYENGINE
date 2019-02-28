// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 04:09:2009 : Created by James Bamford

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesMPSpawning.h"
#include "GameRules.h"
#include "Game.h"
#include "GameCVars.h"
#include "Actor.h"
#include "GameRulesModules/IGameRulesSpectatorModule.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "GameRulesModules/IGameRulesObjectivesModule.h"
#include "Player.h"
#include "Utility/CryWatch.h"
#include "WeaponSystem.h"
#include "Bullet.h"
#include "Utility/CryDebugLog.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "Network/Lobby/GameLobby.h"
#include "ActorManager.h"
#include "EquipmentLoadout.h"
#include "UI/UIManager.h"
#include "UI/UIMultiPlayer.h"

#define DEBUG_NEW_SPAWNING 0		

#if (0 && DEBUG_SPAWNING_VISTABLE && !defined(_RELEASE))
	#define VERBOSE_DEBUG_SPAWNING_VISTABLE 1
#else
	#define VERBOSE_DEBUG_SPAWNING_VISTABLE 0
#endif

#if !defined(_RELEASE)
#define ADD_ALL_PLAYERS_TO_VISTABLE (1)			// useful for debugging with DummyPlayers
#else
#define ADD_ALL_PLAYERS_TO_VISTABLE (0)			// useful for debugging with DummyPlayers
#endif // #if !defined(_RELEASE)


static AUTOENUM_BUILDNAMEARRAY(s_teamSpawnsTypeStr, ETSTList);

#if VERBOSE_DEBUG_SPAWNING_VISTABLE
#define VerboseDebugLog CryLog
#else
#define VerboseDebugLog(...) (void)(0)
#endif

CGameRulesMPSpawningBase::CGameRulesMPSpawningBase()
	: m_teamGame(false)
	, m_teamSpawnsType(eTST_None)
	, m_currentBestSpawnId(0)
	, m_fTimeLocalActorDead(-1.0f)
	, m_fPrecacheTimer(0.f)
	, m_fInitialAutoSpawnTimer(15.f)
	, m_fTimeTillInitialAutoSpawn(-1.f)
	, m_lastFoundSpawnId(0)
	, m_cachedRandomSpawnId(0)
	, m_fCachedRandomSpawnTimeOut(0.f)
	, m_svKillsSum(0)
	, m_bPrecacheContinuousBestSpawns(false)
	, m_bEquipmentScreenShown(false)
	, m_bNeedToStartAutoReviveTimer(true)
	, m_allowMidRoundJoining(true)
	, m_allowMidRoundJoiningBeforeFirstDeath(false)
{
	m_spawnHistory.reserve(16);
	
	m_bGameHasStarted = false;

	m_autoReviveTimeScaleTeam1 = 1.f;
	m_autoReviveTimeScaleTeam2 = 1.f;

#if MONITOR_BAD_SPAWNS
	m_DBG_spawnData.selectedSpawn = 0;
	m_DBG_spawnData.iSelectedSpawnIndex = 0;
	m_DBG_spawnData.scoringResults.clear();
	m_DBG_spawnData.scoringResults.reserve(100);

	if (gEnv->pInput)
		gEnv->pInput->AddEventListener(this);
	m_fLastSpawnCheckTime = 0.0f;
#endif
}

CGameRulesMPSpawningBase::~CGameRulesMPSpawningBase()
{
	CGameRules  *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->UnRegisterRevivedListener(this);
		pGameRules->UnRegisterRoundsListener(this);
		pGameRules->UnRegisterKillListener(this);
	}

#if MONITOR_BAD_SPAWNS
	if(gEnv->pInput)
		gEnv->pInput->RemoveEventListener(this);
#endif
}

#if MONITOR_BAD_SPAWNS
bool CGameRulesMPSpawningBase::OnInputEvent( const SInputEvent &rInputEvent )
{
	return false;
}
#endif

void CGameRulesMPSpawningBase::Init(XmlNodeRef xml)
{
	inherited::Init(xml);

	m_svKillsSum = 0;  // ReviveAllPlayers() resets this again between rounds

	m_spawnHistory.clear();

	CGameRules* pGameRules = g_pGame->GetGameRules();
	assert(pGameRules);

	CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
	if(pEquipmentLoadout)
	{
		pEquipmentLoadout->SetSelectedPackage(0);
		pGameRules->SetPendingLoadoutChange();
	}

	int  ival;
	const char*  cpval;

	if (!xml->getAttr("teamGame", m_teamGame))
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesMPSpawningBase failed to find valid teamGame param");
	}

	bool  typeOk = false;
	if (xml->getAttr("teamSpawnsType", &cpval))
	{
		if (AutoEnum_GetEnumValFromString(cpval, s_teamSpawnsTypeStr, eTST_NUM, &ival))
		{
			m_teamSpawnsType = (ETeamSpawnsType) ival;
			typeOk = true;
		}
	}

	CRY_ASSERT_MESSAGE(typeOk, "CGameRulesMPSpawningBase failed to find valid teamSpawnsType param");

	if (!xml->getAttr("midRoundJoining", m_allowMidRoundJoining))
	{
		m_allowMidRoundJoining = true;
	}

	xml->getAttr("allowMidRoundJoiningBeforeFirstDeath", m_allowMidRoundJoiningBeforeFirstDeath);
	xml->getAttr("initialAutoSpawnTimer", m_fInitialAutoSpawnTimer);

	CryLog("CGameRulesMPSpawningBase::Init() set params to teamGame=%d; teamSpawnsType=%d; midRoundJoiningBeforeFirstDeath=%d", m_teamGame, m_teamSpawnsType, m_allowMidRoundJoiningBeforeFirstDeath);

	//m_spawningVisTable.Init(m_teamSpawnsType);

	pGameRules->RegisterRevivedListener(this);
	pGameRules->RegisterRoundsListener(this);
	pGameRules->RegisterKillListener(this);
}

void CGameRulesMPSpawningBase::OnInGameBegin()
{
	CryLog("CGameRulesMPSpawningBase::OnInGameBegin() creating list of initial players");
	if (CGameLobby *pGameLobby = g_pGame->GetGameLobby())
	{
		const SSessionNames &sessionNames = pGameLobby->GetSessionNames();
		for (uint32 i=0, numSessionNames = sessionNames.Size(); i<numSessionNames; i++)
		{
			const SSessionNames::SSessionName &player = sessionNames.m_sessionNames[i];
			CryLog("  %s (uid=%u)", player.m_name, player.m_conId.m_uid);
			m_initialChannelIDs.push_back(player.m_conId.m_uid);
		}
	}

	m_spawningVisTable.RepopulatePlayerList();

	if (m_bEquipmentScreenShown && m_bNeedToStartAutoReviveTimer)
	{
		CryLog("CGameRulesMPSpawningBase::OnGameStart() equipment screen already shown, starting auto spawn timer");
		m_fTimeTillInitialAutoSpawn = m_fInitialAutoSpawnTimer;
		m_bNeedToStartAutoReviveTimer = false;
	}
}

void CGameRulesMPSpawningBase::Update(float frameTime)
{
	inherited::Update(frameTime);

	m_fCachedRandomSpawnTimeOut = max(m_fCachedRandomSpawnTimeOut-frameTime, 0.f);

	const IGameRulesObjectivesModule *pObjectivesModule = m_pGameRules->GetObjectivesModule();
	IGameRulesStateModule*  stateModule = m_pGameRules->GetStateModule();

	if (gEnv->bServer)
	{
		if (!stateModule || (stateModule->GetGameState() != IGameRulesStateModule::EGRS_PostGame))
		{
			//The server can force people to spawn if the client hasn't requested a spawn yet

			float gameTime = GetTime();

			const float fTimeDiffToRemoveFromHistory = g_pGameCVars->g_spawn_recentSpawnTimer * 1000.0f;

			//Need to re-load size as we're altering the size of it
			for(uint32 i = 0; i < m_spawnHistory.size(); i++)
			{
				if(gameTime - m_spawnHistory[i].fGameTime > fTimeDiffToRemoveFromHistory)
				{
					m_spawnHistory[i] = m_spawnHistory[m_spawnHistory.size()-1];
					m_spawnHistory.pop_back();
				}
			}

			// Calculate when we can do an auto revive
			float timeTillAutoReviveTeam0 = max(g_pGameCVars->g_forcedReviveTime, GetTimeFromDeathTillAutoReviveForTeam(0) + 2.0f);
			// Impose a minimum of 1 second before autoreviving
			timeTillAutoReviveTeam0 = max(timeTillAutoReviveTeam0, 1.0f) * 1000.f;

			float timeTillAutoReviveTeam1 = max(g_pGameCVars->g_forcedReviveTime, GetTimeFromDeathTillAutoReviveForTeam(1) + 2.0f);
			// Impose a minimum of 1 second before autoreviving
			timeTillAutoReviveTeam1 = max(timeTillAutoReviveTeam1, 1.0f) * 1000.f;
			
			float timeTillAutoReviveTeam2 = max(g_pGameCVars->g_forcedReviveTime, GetTimeFromDeathTillAutoReviveForTeam(2) + 2.0f);
			// Impose a minimum of 1 second before autoreviving
			timeTillAutoReviveTeam2 = max(timeTillAutoReviveTeam2, 1.0f) * 1000.f;


			// Check if any players have been dead long enough
			TPlayerDataMap::iterator it = m_playerValues.begin();
			for (; it != m_playerValues.end(); ++it)
			{
				EntityId playerId = it->first;
				if (!pObjectivesModule || pObjectivesModule->CanPlayerRevive(playerId))
				{
					CActor *pActorImpl = static_cast<CActor *>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId));

					if (pActorImpl)
					{
						if(it->second.lastRevivedTime < it->second.deathTime && !it->second.triedAutoRevive)
						{
							if (pActorImpl->IsDead() && (pActorImpl->GetSpectatorState() == CActor::eASS_Ingame))
							{
								float timeSinceDeath = gameTime - it->second.deathTime;
								const int  actorTeam = static_cast<CPlayer*>(pActorImpl)->GetTeamWhenKilled();
							
								float timeTillAutoRevive=timeTillAutoReviveTeam0;
								if (actorTeam == 1)
								{
									timeTillAutoRevive = timeTillAutoReviveTeam1;
								}
								else if (actorTeam == 2)
								{
									timeTillAutoRevive = timeTillAutoReviveTeam2;
								}

								timeTillAutoRevive += GetPlayerAutoReviveAdditionalTime(pActorImpl);

								if (timeSinceDeath > timeTillAutoRevive)
								{
									CryLog("CGameRulesMPSpawning::Update() %s '%s' has been dead for %.3f seconds - that's over %.3f so auto reviving - FORCED", pActorImpl->GetEntity()->GetClass()->GetName(), pActorImpl->GetEntity()->GetName(), timeSinceDeath / 1000.f, timeTillAutoRevive / 1000.f);
									// don't spawn between rounds etc. But do keep trying
									if (SvRequestRevive(playerId, 0))
									{
										it->second.triedAutoRevive = true;
									}
								}
							}
						}
					}
					else
					{
						CryLog("CGameRulesMPSpawningBase::Update() failed to find an actor for a player in m_playerValues, this should NOT happen");
					}
				}
			}
		}
	}

	//Update the vistable on all clients, but only for the local player!
	CActor * pLocalActorImpl = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	EntityId localActorEntityId = pLocalActorImpl ? pLocalActorImpl->GetEntityId() : 0;

	if(m_bGameHasStarted && pLocalActorImpl != NULL && ((pLocalActorImpl->IsDead() && pLocalActorImpl->GetSpectatorState() == CActor::eASS_Ingame) || (pLocalActorImpl->GetSpectatorState() == CActor::eASS_None) || (m_bPrecacheContinuousBestSpawns)))
	{
		if (!m_teamGame || (m_pGameRules->GetTeam(localActorEntityId) != 0))
		{
			const EntityId currentBestSpawnId = GetSpawnLocation(localActorEntityId);
			if(currentBestSpawnId!=m_currentBestSpawnId && m_bPrecacheContinuousBestSpawns)
			{
				if(IEntity* pSpawnPoint = gEnv->pEntitySystem->GetEntity(currentBestSpawnId))
				{
					const Matrix34& transform = pSpawnPoint->GetWorldTM();
					gEnv->p3DEngine->AddPrecachePoint( transform.GetTranslation(), transform.GetColumn1(), 1.f, 0.65f );
				}
			}
			m_currentBestSpawnId = currentBestSpawnId;
			m_spawningVisTable.Update(currentBestSpawnId);
		}
#ifndef _RELEASE
		else if (m_teamGame)
		{
			CryLogAlways("WOULD HAVE CRASHED HERE - LOOKING FOR A SPAWN POINT WITH NO TEAM SET");
		}
#endif
	}

	const CActor::EActorSpectatorState spectatorState = pLocalActorImpl ? pLocalActorImpl->GetSpectatorState() : CActor::eASS_None;
	CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
	const bool hasSelectedLoadout = pEquipmentLoadout ? pEquipmentLoadout->GetHasPreGameLoadoutSent() : false;
	const bool shouldRequestRevive = hasSelectedLoadout && (spectatorState == CActor::eASS_None || spectatorState == CActor::eASS_ForcedEquipmentChange);
	if(pLocalActorImpl != NULL && pLocalActorImpl->IsDead() && (!stateModule || (stateModule->GetGameState() != IGameRulesStateModule::EGRS_PostGame)) && (spectatorState == CActor::eASS_Ingame || shouldRequestRevive))
	{
		if (!pObjectivesModule || pObjectivesModule->CanPlayerRevive(localActorEntityId))
		{
			const int  actorTeam = m_pGameRules->GetTeam(localActorEntityId);

			bool preGameDone = g_pGame->GetUI()->IsPreGameDone();

			float timeTillAutoRevive = GetTimeFromDeathTillAutoReviveForTeam(static_cast<CPlayer*>(pLocalActorImpl)->GetTeamWhenKilled());
			timeTillAutoRevive += GetPlayerAutoReviveAdditionalTime(pLocalActorImpl);
			
			// If you have no lives left, you can't respawn, etc.
			if(GetNumLives()!=0 && GetRemainingLives(localActorEntityId)==0)
			{
				timeTillAutoRevive = 10000.f;
			}

			// Impose a minimum of 1 second before autoreviving
			timeTillAutoRevive = max(timeTillAutoRevive, 1.0f);

			if(m_fTimeLocalActorDead >= 0.f)
			{
				float fTimeDead = m_fTimeLocalActorDead + frameTime;

				// Start precaching spawns x seconds before respawn.
				if(m_fPrecacheTimer>0.f)
				{
					m_fPrecacheTimer-=frameTime;
					if(m_fPrecacheTimer<=0.f)
					{
						m_bPrecacheContinuousBestSpawns = true;
						if(IEntity* pSpawnPoint = gEnv->pEntitySystem->GetEntity(m_currentBestSpawnId))
						{
							const Matrix34& transform = pSpawnPoint->GetWorldTM();
							gEnv->p3DEngine->AddPrecachePoint( transform.GetTranslation(), transform.GetColumn1(), g_pGameCVars->g_randomSpawnPointCacheTime, 0.65f );
						}
					}
				}
				m_fTimeLocalActorDead = fTimeDead;
				if(fTimeDead >= timeTillAutoRevive)
				{
					ClRequestRevive(localActorEntityId);
					m_fTimeLocalActorDead = timeTillAutoRevive - g_pGameCVars->g_spawn_timeToRetrySpawnRequest;
				}
			}
			else
			{
				// Set up spawnpoint the precache timer.
				m_fPrecacheTimer = (float)__fsel(g_pGameCVars->g_spawnPrecacheTimeBeforeRevive-timeTillAutoRevive, 0.01f, timeTillAutoRevive-g_pGameCVars->g_spawnPrecacheTimeBeforeRevive);
				m_fTimeLocalActorDead = 0.0f;
			}

#if MONITOR_BAD_SPAWNS
			m_fLastSpawnCheckTime = m_pGameRules->GetServerTime();
#endif
		}
	}
	else if (pLocalActorImpl && (m_fTimeTillInitialAutoSpawn >= 0.f))
	{
		m_fTimeTillInitialAutoSpawn -= frameTime;
		if ((m_fTimeTillInitialAutoSpawn < 0.f) && (g_pGame->GetEquipmentLoadout()))
		{
			g_pGame->GetEquipmentLoadout()->ForceSelectLastSelectedLoadout();
		}
	}
	else
	{
		m_fTimeLocalActorDead = -1.0f;
	}
}

void CGameRulesMPSpawningBase::AddSpawnLocation(EntityId location, bool isInitialSpawn, bool doVisTest, const char *pGroupName)
{
	inherited::AddSpawnLocation(location, isInitialSpawn, doVisTest, pGroupName);

	m_spawningVisTable.AddSpawnLocation(location, doVisTest); 
}

void CGameRulesMPSpawningBase::RemoveSpawnLocation(EntityId id, bool isInitialSpawn)
{
	inherited::RemoveSpawnLocation(id, isInitialSpawn);

	m_spawningVisTable.RemoveSpawnLocation(id);
}

void CGameRulesMPSpawningBase::PlayerJoined(EntityId playerId)
{
	inherited::PlayerJoined(playerId);

	m_spawningVisTable.PlayerJoined(playerId);
	IEntity* pPlayer = (IEntity*)gEnv->pEntitySystem->GetEntity(playerId);
	const string sPlayerName = pPlayer ? pPlayer->GetName() : "Unknown";
	CUIMultiPlayer* pUIEvt = UIEvents::Get<CUIMultiPlayer>();
	if (pUIEvt) 
		pUIEvt->PlayerJoined(playerId, sPlayerName);
}

void CGameRulesMPSpawningBase::PlayerLeft(EntityId playerId)
{
	inherited::PlayerLeft(playerId);

	m_spawningVisTable.PlayerLeft(playerId);

	IEntity* pPlayer = (IEntity*)gEnv->pEntitySystem->GetEntity(playerId);
	const string sPlayerName = pPlayer ? pPlayer->GetName() : "Unknown";
	CUIMultiPlayer* pUIEvt = UIEvents::Get<CUIMultiPlayer>();
	if (pUIEvt) 
		pUIEvt->PlayerLeft(playerId, sPlayerName);
}

void CGameRulesMPSpawningBase::AddAvoidPOI(EntityId entityId, float avoidDistance, bool startActive, bool bUseStaticPosn)
{
#if DEBUG_NEW_SPAWNING
	IEntity *poiEntity = gEnv->pEntitySystem->GetEntity(entityId);
	const Vec3& posn = poiEntity->GetWorldPos();
	CryLog("CGameRulesMPSpawningBase::AddAvoidPOI() entity=%s (id=%d); avoidDistance=%f; startActive=%d, posn: (%.2f, %.2f, %.2f)", poiEntity ? poiEntity->GetName() : "NULL", entityId, avoidDistance, startActive, posn.x, posn.y, posn.z);
#endif

	SPointOfInterest newPOI;
	newPOI.SetToAvoidEntity(entityId, avoidDistance*avoidDistance, startActive, bUseStaticPosn);
	m_pointsOfInterest.push_back(newPOI);
}

void CGameRulesMPSpawningBase::RemovePOI(EntityId entityId)
{
#if DEBUG_NEW_SPAWNING
	IEntity *poiEntity = gEnv->pEntitySystem->GetEntity(entityId);
	CryLog("CGameRulesMPSpawningBase::RemovePOI() entity=%s; (id=%d)", poiEntity ? poiEntity->GetName() : "NULL", entityId);
#endif

	for (TPointsOfInterest::iterator it=m_pointsOfInterest.begin(); it!=m_pointsOfInterest.end(); ++it)
	{
		SPointOfInterest &poi = *it;
		if (poi.m_entityId == entityId)
		{
			m_pointsOfInterest.erase(it);
			return;
		}
	}

	CryLog("CGameRulesMPSpawningBase::RemovePOI() failed to find POI for entity");
}

void CGameRulesMPSpawningBase::OnSetTeam(EntityId playerId, int teamId)
{
	m_spawningVisTable.OnSetTeam(playerId, teamId);
}

void CGameRulesMPSpawningBase::EnablePOI(EntityId entityId)
{
#if DEBUG_NEW_SPAWNING
	IEntity *poiEntity = gEnv->pEntitySystem->GetEntity(entityId);
	CryLog("CGameRulesMPSpawningBase::EnablePOI() entity=%s; (id=%d)", poiEntity ? poiEntity->GetName() : "NULL", entityId);
#endif

	for (	TPointsOfInterest::iterator it=m_pointsOfInterest.begin(); it!=m_pointsOfInterest.end(); ++it)
	{
		SPointOfInterest &poi = *it;
		if (poi.m_entityId == entityId)
		{
			poi.Enable();
			return;
		}
	}

	CryLog("CGameRulesMPSpawningBase::EnablePOI() failed to find POI for entity");
}

void CGameRulesMPSpawningBase::DisablePOI(EntityId entityId)
{
#if DEBUG_NEW_SPAWNING
	IEntity *poiEntity = gEnv->pEntitySystem->GetEntity(entityId);
	CryLog("CGameRulesMPSpawningBase::DisablePOI() entity=%s; (id=%d)", poiEntity ? poiEntity->GetName() : "NULL", entityId);
#endif

	for (TPointsOfInterest::iterator it=m_pointsOfInterest.begin(); it!=m_pointsOfInterest.end(); ++it)
	{
		SPointOfInterest &poi = *it;
		if (poi.m_entityId == entityId)
		{
			poi.Disable();
			return;
		}
	}

	CryLog("CGameRulesMPSpawningBase::DisablePOI() failed to find POI for entity");
}

void CGameRulesMPSpawningBase::ClRequestRevive(EntityId playerId)
{
	// no inherited call - not implemented in CGameRulesSpawningBase
	CryLog("CGameRulesMPSpawningBase::ClRequestRevive()");
	INDENT_LOG_DURING_SCOPE();

	m_fTimeTillInitialAutoSpawn = -1.f;
	m_bNeedToStartAutoReviveTimer = false;

	IGameRulesObjectivesModule *pObjectivesModule = m_pGameRules->GetObjectivesModule();
	if (pObjectivesModule && !pObjectivesModule->CanPlayerRevive(playerId))
	{
		return;
	}

	CActor *pActor = static_cast<CActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(playerId));

	if (pActor)
	{
		CryLog("CGameRulesMPSpawningBase::RequestRevive() player (%s)", pActor->GetEntity()->GetName());

		// Check if the player can revive
		if ((pActor->IsDead()) || (pActor->GetSpectatorState()==CActor::eASS_None))
		{
			CryLog("[SPAWN] CGameRulesMPSpawningBase::ClRequestRevive() Requesting revive via RMI ");

			EntityId spawnPointId = GetSpawnLocation(playerId);
			if(playerId==gEnv->pGameFramework->GetClientActorId())
			{
				if(IEntity* pSpawnPoint = gEnv->pEntitySystem->GetEntity(spawnPointId))
				{
					const Matrix34& transform = pSpawnPoint->GetWorldTM();
					gEnv->p3DEngine->ClearAllPrecachePoints();
					gEnv->p3DEngine->OverrideCameraPrecachePoint( transform.GetTranslation() );
					gEnv->p3DEngine->ProposeContentPrecache();
					m_bPrecacheContinuousBestSpawns = false;
				}
			}

			if (gEnv->bServer)
			{
				SvRequestRevive(playerId, spawnPointId);
			}
			else
			{
#if !defined(_RELEASE)
				if(IEntity * pSpawnPoint = gEnv->pEntitySystem->GetEntity(spawnPointId))
				{
					Vec3 posn = pSpawnPoint->GetWorldPos();
					CryLog("[SPAWN]   requesting spawn point '%s' at position (%.2f, %.2f, %.2f)", pSpawnPoint->GetName(), posn.x, posn.y, posn.z);
				}
#endif
				CGameRules::ServerReviveParams params;
				params.entityId = playerId;
				params.index = GetSpawnIndexForEntityId(spawnPointId);
				m_pGameRules->GetGameObject()->InvokeRMI(CGameRules::SvRequestRevive(), params, eRMI_ToServer);
			}
		}
		else
		{
			CryLogAlways("CGameRulesMPSpawningBase::ClRequestRevive() Player %s is not dead", pActor->GetEntity()->GetName());
			CCCPOINT(PlayerState_CannotSpawnBecauseIsAlive);
		}
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesMPSpawningBase::ClRequestRevive() failed to find a playerEntity for player that died");
	}
}

bool CGameRulesMPSpawningBase::SvRequestRevive(EntityId playerId, EntityId preferredSpawnId)
{
	// no inherited call - not implemented in CGameRulesSpawningBase
	IGameRulesStateModule *pStateModule = m_pGameRules->GetStateModule();
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(playerId);

	if (pEntity != NULL && !pEntity->IsGarbage())
	{
		if ((g_pGameCVars->g_gameRules_preGame_StartSpawnedFrozen==0) && (pStateModule != NULL) && (pStateModule->GetGameState() != IGameRulesStateModule::EGRS_InGame))
		{
#ifndef _RELEASE
			CryLog("CGameRulesMPSpawningBase::SvRequestRevive() request received from %s but we're not in InGame state we're in state=%d, refusing", pEntity ? pEntity->GetName() : "<NULL>", pStateModule->GetGameState());
#endif
			return false;
		}

		IGameRulesRoundsModule *pRoundsModule = m_pGameRules->GetRoundsModule();
		if (pRoundsModule != NULL && pRoundsModule->IsRestarting())
		{
			CryLog("CGameRulesMPSpawningBase::SvRequestRevive() request received from %s but we're currently between rounds, refusing", pEntity ? pEntity->GetName() : "<NULL>");
			return false;
		}

		IGameRulesObjectivesModule* pObjectivesModule = m_pGameRules->GetObjectivesModule();
		if(pObjectivesModule && !pObjectivesModule->CanPlayerRevive(playerId))
		{
			CryLog("CGameRulesMPSpawningBase::SvRequestRevive() request received from %s but the game objective refused it", pEntity ? pEntity->GetName() : "<NULL>");
			return false;
		}

		CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId));
		if(pActor)
		{
			CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
			uint16 uActorChannel = pActor->GetChannelId();
			if (pEquipmentLoadout && uActorChannel && pEquipmentLoadout->SvHasClientEquipmentLoadout(uActorChannel) == false)
			{
				CryLog("CGameRulesMPSpawningBase::SvRequestRevive() request received from %s but they have no loadout set, refusing", pEntity ? pEntity->GetName() : "<NULL>");
				return false;
			}

			if (g_pGame->GetHostMigrationState() == CGame::eHMS_NotMigrating)
			{
				TPlayerDataMap::iterator it = m_playerValues.find(playerId);
				if (it != m_playerValues.end())
				{
					int nPlayerTeam = m_pGameRules->GetTeam(playerId); 
					if (m_teamGame==(nPlayerTeam!=0))
					{
						const INetChannel*  pNetChannel = gEnv->pGameFramework->GetNetChannel(uActorChannel);
						const float  netLag = ((pActor->IsClient() || !pNetChannel) ? 0 : ((pNetChannel->GetPing(true) * 1000.f) * 0.5f));  // GetPing() is the round journey, so 0.5 gives us the 1-way trip.
						CRY_ASSERT(netLag >= 0.f);

						const float  autoReviveTime = GetTimeFromDeathTillAutoReviveForTeam(static_cast<CPlayer*>(pActor)->GetTeamWhenKilled()) + GetPlayerAutoReviveAdditionalTime(pActor);
						const float  autoReviveTimeMs = ((autoReviveTime * 1000.f) - netLag);  // 'netLag' tried to roughly take into account how long the revive message will take to get over the network

						if (pNetChannel)
						{
							CryLog("CGameRulesMPSpawningBase::SvRequestRevive() request received from %s ping = %f, lag = %f, g_autoReviveTime = %f, autoReviveTimeMs = %f", pEntity ? pEntity->GetName() : "<NULL>", pNetChannel->GetPing(true), netLag, autoReviveTime, autoReviveTimeMs);
						}
						
						CActor::EActorSpectatorState SpectatorState = pActor->GetSpectatorState();
						if ((SpectatorState==CActor::eASS_None) || (SpectatorState==CActor::eASS_ForcedEquipmentChange) || ((SpectatorState==CActor::eASS_Ingame) && ((GetTime() - it->second.deathTime) > autoReviveTimeMs)))
						{
							PerformRevive(playerId, nPlayerTeam, preferredSpawnId);
							return true;
							CCCPOINT(PlayerState_SvPerformRevive);
						}
						else
						{
							CryLog("CGameRulesMPSpawningBase::SvRequestRevive() abandoning revive. Spectator State %d, CurrentTime: %f, DeathTime: %f, autoReviveTime %f, should revive based on Time: %s", SpectatorState, GetTime(), it->second.deathTime, autoReviveTimeMs, ((GetTime() - it->second.deathTime) > autoReviveTimeMs) ? "true" : "false");
							CCCPOINT(PlayerState_SvDoNotReviveTooSoonAfterDeath);
						}
					}
					else
					{
						CryLog("CGameRulesMPSpawningBase::SvRequestRevive() %s", m_teamGame?"team game trying to revive a player who doesn't have team":"non-team game trying to revive a player who has a team");
					}
				}
			}
		}
#ifndef _RELEASE
		else
		{
			CryLog("CGameRulesMPSpawningBase::SvRequestRevive() request received from %s but we're mid hostmigration, refusing", pEntity ? pEntity->GetName() : "<NULL>");
		}
#endif	
	}

	return false;
}

void CGameRulesMPSpawningBase::PerformRevive(EntityId playerId, int teamId, EntityId preferredSpawnId)
{
	// no inherited call - not implemented in CGameRulesSpawningBase
	CActor *pActor = static_cast<CActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(playerId));

	CRY_ASSERT_MESSAGE(gEnv->bServer, "CGameRulesMPSpawningBase::PerformRevive() being called when NOT a server. This is wrong!");

	if (pActor)
	{
		CryLog("CGameRulesMPSpawningBase::PerformRevive() player (%s) teamId=%d", pActor->GetEntity()->GetName(), teamId);
		INDENT_LOG_DURING_SCOPE();

		// Check if the player can revive
		if (( ( pActor->IsDead() || pActor->GetSpectatorMode() != CActor::eASM_None/*Spectating*/ ) && (!preferredSpawnId || SpawnIsValid(preferredSpawnId, playerId))) || pActor->IsMigrating())
		{
			CRY_ASSERT_MESSAGE(m_playerValues.find(playerId) != m_playerValues.end(), string().Format("CGameRulesMPSpawningBase::PerformRevive() failed to find a playervalue for player %s. This should not happen", pActor->GetEntity()->GetName()));

			if (CheckMidRoundJoining(pActor, playerId))
			{
				if(!preferredSpawnId)
				{
					CryLog("SPAWN-SPEC: No preferred spawn specified, calculating spawn location");
					preferredSpawnId = GetSpawnLocation(pActor->GetEntityId());
				}
				else
				{
					CryLog("SPAWN-SPEC: Spawn specified avoiding calculation");
				}

				bool usePreferredSpawnId = true;
				if(pActor->IsMigrating())
				{
					usePreferredSpawnId = false;
				}
#if (USE_DEDICATED_INPUT)
				else if(g_pGameCVars->g_dummyPlayersRespawnAtDeathPosition == 1 && (strcmp(pActor->GetEntityClassName(), "DummyPlayer") == 0))
				{
					usePreferredSpawnId = false;
				}
#endif
				// If this is a migrating player, ignore the spawn location and use the actor's current location
				const EntityId spawnId = usePreferredSpawnId ? preferredSpawnId : playerId;

				IEntity *pSpawnPoint = gEnv->pEntitySystem->GetEntity(spawnId);
				if (pSpawnPoint)
				{
					SSpawnHistory spawnHistory(spawnId, gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(), teamId);
					m_spawnHistory.push_back(spawnHistory);

#if !defined(_RELEASE)
					const Vec3& posn = pSpawnPoint->GetWorldPos();
					CryLog("CGameRulesMPSpawningBase::PerformRevive, reviving %s '%s' at %s '%s', posn %.2f %.2f %.2f", pActor->GetEntity()->GetClass()->GetName(), pActor->GetEntity()->GetName(), pSpawnPoint->GetClass()->GetName(), pSpawnPoint->GetName(), posn.x, posn.y, posn.z);
#endif

					if (IGameRulesSpectatorModule*  specmod = m_pGameRules->GetSpectatorModule())
					{
						CPlayer *pPlayer = static_cast<CPlayer*>(pActor);
						pPlayer->SetSpectatorState(CActor::eASS_Ingame);
						specmod->ChangeSpectatorMode(pPlayer, CActor::eASM_None, 0, false);
					}

					m_pGameRules->RevivePlayerMP(pActor, pSpawnPoint, teamId, true);

					if(pActor->IsClient())
					{
						m_currentBestSpawnId = 0;
						m_bPrecacheContinuousBestSpawns = false;
					}
				}
				else
				{
					CryLog("CGameRulesMPSpawningBase::PerformRevive() failed to find spawn point");
				}
			}
			else
			{
				CryLog("CGameRulesMPSpawningBase::PerformRevive() failed because mid-round joins are disabled and we're currently mid-round (ingame) and the player to be spawned is not the server");
			}
		}
		else
		{
			CryLog("CGameRulesMPSpawningBase::PerformRevive() Player %s is %s, Spectator State %d, preferred spawn %d, validity: %s, %s migrating",
				pActor->GetEntity()->GetName(), pActor->IsDead() ? "dead" : "not dead", pActor->GetSpectatorMode(), preferredSpawnId, preferredSpawnId ? SpawnIsValid(preferredSpawnId, playerId) ? "VALID" : "INVALID" : "SERVER DETERMINED", pActor->IsMigrating() ? "" : "not");
		}
	}
	else
	{
		CryLog("CGameRulesMPSpawningBase::PerformRevive() failed to find the actor to revive");
	}
}

bool CGameRulesMPSpawningBase::SpawnIsValid(EntityId spawnId, EntityId playerId) const
{
	const IEntitySystem * pEntitySystem = gEnv->pEntitySystem;
	
	IEntity * pDesiredSpawnEntity = pEntitySystem->GetEntity(spawnId);
	const Vec3& vDesiredSpawnEntityPos = pDesiredSpawnEntity->GetPos();
	const float kAcceptableDistanceEnemy		= 20.0f;
	const float kAcceptableDistanceTeamSq		= sqr(0.9f);
	const float kAcceptableDistanceEnemySq	= sqr(kAcceptableDistanceEnemy);

	int iTeamNum = -1;
	if(m_pGameRules->GetTeamCount() > 1)
	{
		iTeamNum = m_pGameRules->GetTeam(playerId);

		const int iSpawnTeam = GetSpawnLocationTeam(spawnId);
		if(iSpawnTeam != 0 && iSpawnTeam != iTeamNum)
		{
			return false;
		}
	}


	//If we're using spawn group locations, ensure that the location the client is requesting is
	//	in the list of the server's spawn groups.
	if(IsPlayerInitialSpawning(playerId) && m_activeInitialSpawnGroupIndex < m_initialSpawnLocations.size())
	{
		const TSpawnLocations& spawnLocations = m_initialSpawnLocations[m_activeInitialSpawnGroupIndex].m_locations;
		bool bFound = false;

		for(TSpawnLocations::const_iterator spawnIter = spawnLocations.begin(), spawnEnd = spawnLocations.end(); spawnIter != spawnEnd && !bFound; ++spawnIter)
		{
			if(*spawnIter == spawnId)
				bFound = true;
		}

		if(!bFound)
		{
			if(g_pGameCVars->g_debugSpawnPointValidity)
				CryFatalError("Client '%s' was requesting a spawn that was not in the initial spawns list", gEnv->pEntitySystem->GetEntity(playerId)->GetName());

			return false;
		}
	}
	
	const int		iNumSpawnRecords				= m_spawnHistory.size();
	for(int i = 0; i < iNumSpawnRecords; i++)
	{
		const SSpawnHistory& rSpawnHistory = m_spawnHistory[i];
		IEntity * pSpawnEnt = pEntitySystem->GetEntity(rSpawnHistory.spawnId);

		//The likelihood of a spawn occurring within a short time of another is significantly low that, on average,
		//	there will not be a saving if we cache the spawn positions when they occur
		const Vec3& vHistoricalSpawnPos = pSpawnEnt->GetPos();
		float fAcceptableDistanceSq;
		
		if(rSpawnHistory.nTeamSpawned != iTeamNum)
		{
			fAcceptableDistanceSq = kAcceptableDistanceEnemySq;
		}
		else
		{
			fAcceptableDistanceSq = kAcceptableDistanceTeamSq;
		}

		const float fDistSq = vDesiredSpawnEntityPos.GetSquaredDistance(vHistoricalSpawnPos);
	
		if(fDistSq < fAcceptableDistanceSq)
		{
			float gameTime = GetTime();
			VerboseDebugLog("ABORTING SPAWN: selected spawn is too close to recent spawn at game time %f\n        Previous spawn from team %d, player %s, at game time %f [%f seconds ago]\n        request from team %d, distance %f\n",
						gameTime, rSpawnHistory.nTeamSpawned, pSpawnEnt->GetName(), rSpawnHistory.fGameTime, gameTime - rSpawnHistory.fGameTime, iTeamNum, sqrt_tpl(fDistSq) );
			return false;
		}
	}


	CActorManager * pActorManager = CActorManager::GetActorManager();

	bool bActorNearby = false;;

	pActorManager->PrepareForIteration();
	const int kNumActors = pActorManager->GetNumActorsIncludingLocalPlayer();
	float fCloseActor = 0.0f;
	Vec3  vClosePosn;

	for(int i = 0; i < kNumActors; i++)
	{
		SActorData actorData;
		pActorManager->GetNthActorData(i, actorData);		

		if ((actorData.spectatorMode == CActor::eASM_None) && (actorData.health > 0.f))
		{
			const float fDistSq = vDesiredSpawnEntityPos.GetSquaredDistance(actorData.position);
			if(fDistSq < kAcceptableDistanceTeamSq)
			{
				bActorNearby = true;
				vClosePosn = actorData.position;
				fCloseActor = fDistSq;
				break;
			}
		}
	}	

	if(bActorNearby)
	{
		VerboseDebugLog("ABORTING SPAWN: selected spawn is too close to CLOSE ACTOR. (distance %f m, posn (%.3f %.3f %.3f))\n", sqrtf(fCloseActor), vClosePosn.x, vClosePosn.y, vClosePosn.z);
	}
	else
	{
		VerboseDebugLog("ALLOWING SPAWN: spawn at (%.2f, %.2f, %.2f) on frame #:%d\n", vDesiredSpawnEntityPos.x, vDesiredSpawnEntityPos.y, vDesiredSpawnEntityPos.z, gEnv->pRenderer->GetFrameID(false));
	}

	return !bActorNearby;
}

EntityId CGameRulesMPSpawningBase::GetSpawnLocation(EntityId playerId)
{
	EntityId id=0;

	IEntity *playerEntity = gEnv->pEntitySystem->GetEntity(playerId);
	if (!playerEntity)
	{
		CryLog("ERROR: GetSpawnLocation() failed to find a playerEntity to work with. Returning.");
		return 0;
	}

	Vec3 deathPos=ZERO;

	TPlayerDataMap::iterator it = m_playerValues.find(playerId);
	if (it != m_playerValues.end())
	{
		deathPos = it->second.deathPos;
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, string().Format("CGameRulesMPSpawningBase::GetSpawnLocation() failed to find a playervalue for player %s. This should not happen", playerEntity->GetName()));
	}

	if (m_teamGame)
	{
		id = GetSpawnLocationTeamGame(playerId, deathPos);
	}
	else
	{
		id = GetSpawnLocationNonTeamGame(playerId, deathPos);
	}
	
	if (id)
	{
		if(m_lastFoundSpawnId != id)
		{
			m_lastFoundSpawnId=id;
			VerboseDebugLog("CGameRulesMPSpawningBase::GetSpawnLocation() player=%s; deathPos=%f, %f, %f; spawnModule params: teamGame=%d; teamSpawnsType=%d;", playerEntity->GetName(), deathPos.x, deathPos.y, deathPos.z, m_teamGame, m_teamSpawnsType);
			VerboseDebugLog("CGameRulesMPSpawningBase::GetSpawnLocation() setting lastSpawnLocationId to %d\n", id);
		}

		return id;
	}
	else
	{
#ifndef _RELEASE
		if(!gEnv->IsEditor())
			CryFatalError("GetSpawnLocation() player=%s; has failed to find a valid spawn. Was using %s spawning", playerEntity->GetName(), m_teamGame ? "team" : "non-team");
#endif //!_RELEASE

		VerboseDebugLog("CGameRulesMPSpawningBase::GetSpawnLocation() player=%s; deathPos=%f, %f, %f; spawnModule params: teamGame=%d; teamSpawnsType=%d;", playerEntity->GetName(), deathPos.x, deathPos.y, deathPos.z, m_teamGame, m_teamSpawnsType);
		VerboseDebugLog("GetSpawnLocation() player=%s; has failed to find a valid spawn itself and within old gamerules GetSpawnLocation(). Something is seriously broken", playerEntity->GetName());

		return 0;
	}
}

int CGameRulesMPSpawningBase::GetSpawnLocationTeam(EntityId spawnLocEid) const
{
	int  team = 0;

	switch (m_teamSpawnsType)
	{
	case eTST_RoundSwap:
		{
			team = m_pGameRules->GetTeam(spawnLocEid);
			if (team > 0)
			{
				IGameRulesRoundsModule*  pRoundsModule = m_pGameRules->GetRoundsModule();
				CRY_ASSERT(pRoundsModule);
				if (pRoundsModule)
				{
					int  prim = pRoundsModule->GetPrimaryTeam();
					LOG_PRIMARY_ROUND("LOG_PRIMARY_ROUND: GetSpawnLocationTeam: eTST_RoundSwap: primary team = %d, orig spawn team = %d", prim, team);
					if (prim != 2)
					{
						CRY_ASSERT(team < 3);
						team = (3 - team);
					}
					LOG_PRIMARY_ROUND("                                                       : returning spawn team as %d", team);
				}
			}
			break;
		}
	case eTST_Initial:
	case eTST_None:
	case eTST_Standard:
	default:
		{
			team = m_pGameRules->GetTeam(spawnLocEid);
			break;
		}
	}

	return team;
}

//------------------------------------------------------------------------
bool CGameRulesMPSpawningBase::TestSpawnLocationWithEnvironment(const IEntity *pSpawn, EntityId playerId, float offset, float height) const
{
	IPhysicalEntity *pPlayerPhysics=0;

	IEntity *pPlayer=gEnv->pEntitySystem->GetEntity(playerId);
	if (pPlayer)
		pPlayerPhysics=pPlayer->GetPhysics();

	const static float r = 0.3f;
	primitives::sphere sphere;
	sphere.center = pSpawn->GetWorldPos();
	sphere.r = r;
	sphere.center.z+=offset+r;

	Vec3 end = sphere.center;
	end.z += height-2.0f*r;

	geom_contact *pContact = 0;
	float dst = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, end-sphere.center, ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid|ent_living,
		&pContact, 0, (geom_colltype_player<<rwi_colltype_bit)|rwi_stop_at_pierceable, 0, 0, 0, &pPlayerPhysics, pPlayerPhysics?1:0);

	if(dst>0.001f)
		return false;
	else
		return true;
}

bool CGameRulesMPSpawningBase::IsSpawnLocationSafeFromExplosives(const IEntity *pSpawn) const
{
	const Vec3& centre = pSpawn->GetPos();
	const Vec3 radiusVec(g_pGameCVars->g_spawn_explosiveSafeDist, g_pGameCVars->g_spawn_explosiveSafeDist, g_pGameCVars->g_spawn_explosiveSafeDist);

	SProjectileQuery projQuery;
	projQuery.box = AABB(centre - radiusVec, centre + radiusVec);
	g_pGame->GetWeaponSystem()->QueryProjectiles(projQuery);

	for (int i=0; i<projQuery.nCount; i++)
	{
		IEntity* pEntity = projQuery.pResults[i];

		if(pEntity->GetClass() != CBullet::EntityClass)
		{
#if DEBUG_NEW_SPAWNING
			CryLog("CGameRulesMPSpawningBase::IsSpawnLocationSafeFromExplosives() has found a non-bullet entity %s within range %f", pEntity->GetName(), g_pGameCVars->g_spawn_explosiveSafeDist);
#endif
			return false;
		}
	}
	
	return true;
}



//------------------------------------------------------------------------
bool CGameRulesMPSpawningBase::IsSpawnLocationSafe(EntityId playerId, const IEntity * pSpawn, float safeDistance, bool ignoreTeam, float zoffset) const
{
	if (safeDistance<=0.01f)
		return true;

	SEntityProximityQuery query;
	Vec3	c(pSpawn->GetWorldPos());
	float l(safeDistance*1.5f);
	float safeDistanceSq=safeDistance*safeDistance;

	query.box = AABB(Vec3(c.x-l,c.y-l,c.z-0.15f), Vec3(c.x+l,c.y+l,c.z+2.0f));
	query.nEntityFlags = 0;
	query.pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Player");
	gEnv->pEntitySystem->QueryProximity(query);

	bool result=true;

	if (zoffset<=0.0001f)
	{
		for (int i=0; i<query.nCount; i++)
		{
			EntityId entityId=query.pEntities[i]->GetId();
			if (playerId==entityId) // ignore self
				continue;

			if(IActor * pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId))
			{
				if(pActor->GetHealth() <= 0.0f || pActor->GetSpectatorMode() != CActor::eASM_None)
					continue;
			}
			else
			{
				continue;
			}

			float fDistSq = query.pEntities[i]->GetWorldPos().GetSquaredDistance(c);
			if (fDistSq <= safeDistanceSq)
			{
#if !defined(_RELEASE)
				CryLog("[SPAWN] Spawn point '%s' at (%.2f, %.2f, %.2f) unsafe due to player entity '%s' at a distance of %f (within %f)", pSpawn->GetName(), c.x, c.y, c.z, query.pEntities[i]->GetName(), sqrtf(fDistSq), safeDistance);
#endif		
				result=false;
				break;
			}
		}
	}
	else
		result=TestSpawnLocationWithEnvironment(pSpawn, playerId, zoffset, 2.0f);

	return result;
}

bool CGameRulesMPSpawningBase::DoesSpawnLocationRespectPOIs(EntityId spawnLocationId) const
{
	IEntity *pSpawn=gEnv->pEntitySystem->GetEntity(spawnLocationId);
	if (!pSpawn)
		return false;

	TPointsOfInterest::const_iterator it;

	Vec3 spawnPos = pSpawn->GetPos();

	for (it=m_pointsOfInterest.begin(); it!=m_pointsOfInterest.end(); ++it)
	{
		const SPointOfInterest &poi = *it;
		
		if (poi.m_flags & EPOIFL_ENABLED)
		{
#if DEBUG_NEW_SPAWNING
			IEntity *pPOI=gEnv->pEntitySystem->GetEntity(poi.m_entityId);
#endif

			const Vec3 * posn = NULL;
			if(poi.m_flags & EPOIFL_USESTATICPOS)
			{
				posn = &poi.m_posn;
			}
			else
			{
#if !DEBUG_NEW_SPAWNING
				IEntity *pPOI=gEnv->pEntitySystem->GetEntity(poi.m_entityId);
#endif
				if (pPOI)
				{
					posn = &pPOI->GetPos();
				}
			}
			
			if(posn)
			{
				Vec3 diff;
				float dist;
				
				diff = *posn - spawnPos;
				dist = diff.GetLengthSquared();

				switch (poi.m_state)
				{
					case EPOIS_AVOID:
						if (dist < poi.m_stateData.avoid.avoidDistance)
						{
#if DEBUG_NEW_SPAWNING
							CryLog("CGameRulesMPSpawningBase::DoesSpawnLocationRespectPOIs() found spawn %s is too close (%f) to avoid POI %s", pSpawn->GetName(), sqrtf(dist), pPOI->GetName());
#endif
							return false;
						}
						break;
					case EPOIS_ATTRACT:
						if (dist > poi.m_stateData.attract.attractDistance)
						{
#if DEBUG_NEW_SPAWNING
							CryLog("CGameRulesMPSpawningBase::DoesSpawnLocationRespectPOIs() found spawn %s is too far away (%f) from attract POI %s", pSpawn->GetName(), sqrtf(dist), pPOI->GetName());
#endif
							return false;
						}
						break;
					default:
#if DEBUG_NEW_SPAWNING
						CRY_ASSERT_TRACE(0, ("CGameRulesSpawningBase::DoesSpawnLocationRespectPOIs() found a POI (entity=%s) with unhandled state %d", pPOI->GetName(), poi.m_state));
#else
						CRY_ASSERT_TRACE(0, ("CGameRulesSpawningBase::DoesSpawnLocationRespectPOIs() found a POI (entity=%d) with unhandled state %d", poi.m_entityId, poi.m_state));
#endif
						break;
				}
			}
		}
	}

	return true;
}


#if !defined(_RELEASE)
void CGameRulesMPSpawningBase::DebugPOIs() const
{
	TPointsOfInterest::const_iterator it;

	for (it=m_pointsOfInterest.begin(); it!=m_pointsOfInterest.end(); ++it)
	{
		const SPointOfInterest &poi = *it;
		IEntity *pPOI=gEnv->pEntitySystem->GetEntity(poi.m_entityId);

		CryWatch("POI: entity=%s (%d); state=%d; enabled=%d", pPOI ? pPOI->GetName() : "NULL", poi.m_entityId, poi.m_state, poi.m_flags & EPOIFL_ENABLED ? 1 : 0);
	}	
}
#endif

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::ReviveAllPlayers(bool isReset, bool bOnlyIfDead)
{
	CryLog("CGameRulesMPSpawningBase::ReviveAllPlayers() isReset=%s bOnlyIfDead=%d", isReset ? "true" : "false", bOnlyIfDead);

	m_svKillsSum = 0;

	if (isReset)
		return;

	CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();

	if(pEquipmentLoadout)
	{
		if (m_teamGame && (m_teamSpawnsType != eTST_None))
		{
			// We're reviving everyone using team spawners so there is no need to check for visibility etc
			for (int teamId = 1; teamId < 3; ++ teamId)
			{
				CGameRules::TPlayers players;
				m_pGameRules->GetTeamPlayers(teamId, players);

				TSpawnLocations spawnPoints;
				GetTeamSpawnLocations(teamId, true, spawnPoints);

				int numPlayers = players.size();
				for (int i = 0; i < numPlayers; ++ i)
				{
					EntityId playerId = players[i];

					IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId);

					if (pActor && (!bOnlyIfDead || pActor->IsDead()))
					{
						if (pEquipmentLoadout->SvHasClientEquipmentLoadout(pActor->GetChannelId()))
						{
							int numPoints = spawnPoints.size();
							if (!numPoints)
							{
								numPoints = GetTeamSpawnLocations(teamId, false, spawnPoints);		// Run out of initial spawns, switch to regular spawns
								if (!numPoints)
								{
									CRY_ASSERT_MESSAGE(false, "Designer ASSERT: No team spawn points");
									return;
								}
							}

							int index = g_pGame->GetRandomNumber() % numPoints;
							EntityId spawnId = spawnPoints[index];

							TSpawnLocations::iterator it = spawnPoints.begin() + index;
							spawnPoints.erase(it);

							ReviveAllPlayers_Internal(pActor, spawnId, teamId);
						}
						else
						{
							CryLog("can't revive player %s (%d) as they haven't set a loadout yet", pActor->GetEntity()->GetName(), playerId);
						}
					}
				}
			}
		}
		else
		{
			CRY_ASSERT_MESSAGE(!m_pGameRules->GetRoundsModule(), "TODO! At the time of writing there are no rounds-based non-team gamemodes. If there ever are any, then this needs implementing... (Shouldn't be hard.)");

			ReviveAllPlayers_NoTeam(isReset, bOnlyIfDead);
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::ReviveAllPlayers_NoTeam(bool isReset, bool bOnlyIfDead)
{
	CGameRules::TPlayers players;
	m_pGameRules->GetPlayers(players);

	int numPlayers = players.size();

	TSpawnLocations eligibleSpawns;
	GetEligibleSpawnListForTeam(m_spawnLocations, eligibleSpawns, 0, false);

	CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
	IActorSystem * pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();

	for (int i = 0; i < numPlayers; ++ i)
	{
		EntityId playerId = players[i];
		IActor *pActor = pActorSystem->GetActor(playerId);
		if (pActor && static_cast<CActor*>(pActor)->GetSpectatorState() != CActor::eASS_SpectatorMode && (!bOnlyIfDead || pActor->IsDead()))
		{
			if (pEquipmentLoadout->SvHasClientEquipmentLoadout(pActor->GetChannelId()))
			{
				EntityId spawnId = 0;
				const int numEligibleSpawnPoints = eligibleSpawns.size();
				if (numEligibleSpawnPoints)
				{
					int index = g_pGame->GetRandomNumber() % numEligibleSpawnPoints;
					spawnId = eligibleSpawns[index];
					eligibleSpawns[index] = eligibleSpawns[numEligibleSpawnPoints-1];
					eligibleSpawns.pop_back();					
					
					VerboseDebugLog("CGameRulesMPSpawningBase::ReviveAllPlayers_NoTeam() picked spawn point eid=%i (%i of %i)", spawnId, index, numElegibleSpawnPoints);
				}
				else
				{
					VerboseDebugLog("CGameRulesMPSpawningBase::ReviveAllPlayers_NoTeam() failed to find elegible spawn point");
					CryFatalError("CGameRulesMPSpawningBase::ReviveAllPlayers_NoTeam() failed to find elegible spawn point");
				}			

				ReviveAllPlayers_Internal(pActor, spawnId, m_pGameRules->GetTeam(playerId));
			}
			else
			{
				CryLog("can't revive player %s (%d) as they haven't set a loadout yet", pActor->GetEntity()->GetName(), playerId);
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::ReviveAllPlayersOnTeam(int teamId)
{
	CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();

	CGameRules::TPlayers players;
	m_pGameRules->GetTeamPlayers(teamId, players);

	TSpawnLocations spawnPoints;
	GetTeamSpawnLocations(teamId, false, spawnPoints);

	int numPlayers = players.size();
	for (int i = 0; i < numPlayers; ++ i)
	{
		EntityId playerId = players[i];

		IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId);

		if (pActor && pActor->IsDead())
		{
			if (pEquipmentLoadout->SvHasClientEquipmentLoadout(pActor->GetChannelId()))
			{
				int numPoints = spawnPoints.size();
				if (!numPoints)
				{
					CryLog("CGameRulesMPSpawningBase::ReviveAllPlayersOnTeam() out of spawn points");
					return;
				}

				int index = g_pGame->GetRandomNumber() % numPoints;
				EntityId spawnId = spawnPoints[index];

				TSpawnLocations::iterator it = spawnPoints.begin() + index;
				spawnPoints.erase(it);

				ReviveAllPlayers_Internal(pActor, spawnId, teamId);
			}
			else
			{
				CryLog("can't revive player %s (%d) as they haven't set a loadout yet", pActor->GetEntity()->GetName(), playerId);
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::ReviveAllPlayers_Internal( IActor * pActor, EntityId spawnId, int nPlayerTeam )
{
	IGameRulesObjectivesModule *pObjectivesModule = m_pGameRules->GetObjectivesModule();
	if(!pObjectivesModule || pObjectivesModule->CanPlayerRevive(pActor->GetEntityId()))
	{
		IEntity *pSpawnPoint = gEnv->pEntitySystem->GetEntity(spawnId);
		if (pSpawnPoint)
		{
			IGameRulesSpectatorModule*  specmod = m_pGameRules->GetSpectatorModule();
			if (specmod)
			{
				CPlayer *pPlayer = static_cast<CPlayer*>(pActor);
				pPlayer->SetSpectatorState(CActor::eASS_Ingame);
				specmod->ChangeSpectatorMode(pPlayer, CActor::eASM_None, 0, false);
			}

			m_pGameRules->RevivePlayerMP(pActor, pSpawnPoint, nPlayerTeam, true);
		}	
	}
	else
	{
		//Player shouldn't be in the match yet
		IGameRulesSpectatorModule*  specmod = m_pGameRules->GetSpectatorModule();
		if (specmod)
		{
			CPlayer *pPlayer = static_cast<CPlayer*>(pActor);
			pPlayer->SetSpectatorState(CActor::eASS_Ingame);
			specmod->ChangeSpectatorMode(pPlayer, CActor::eASM_Fixed, 0, true);
		}
	}
}

//------------------------------------------------------------------------
float CGameRulesMPSpawningBase::GetTimeFromDeathTillAutoReviveForTeam(int teamId) const
{ 
	float autoReviveTime=g_pGameCVars->g_autoReviveTime;
	float autoReviveTimeScale = GetAutoReviveTimeScaleForTeam(teamId);

	autoReviveTime *= autoReviveTimeScale;

	return autoReviveTime;
}

float CGameRulesMPSpawningBase::GetPlayerAutoReviveAdditionalTime(IActor* pActor) const
{
	if(const IGameRulesObjectivesModule *pObjectivesModule = m_pGameRules->GetObjectivesModule())
	{
		return pObjectivesModule->GetPlayerAutoReviveAdditionalTime(pActor);
	}

	return 1.f;
}

//------------------------------------------------------------------------
float CGameRulesMPSpawningBase::GetAutoReviveTimeScaleForTeam(int teamId) const
{
	float autoReviveTimeScale = 1.f;
	if (teamId == 1)
	{
		autoReviveTimeScale = m_autoReviveTimeScaleTeam1;
	}
	else if (teamId == 2)
	{
		autoReviveTimeScale = m_autoReviveTimeScaleTeam2;
	}

	return autoReviveTimeScale;
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::SetAutoReviveTimeScaleForTeam(int teamId, float newScale)
{
	CryLog("CGameRulesMPSpawningBase::SetAutoReviveTimeScaleForTeam() teamId=%d; newScale=%f", teamId, newScale);

	if (teamId == 1)
	{
		m_autoReviveTimeScaleTeam1 = newScale;
	}
	else if (teamId == 2)
	{
		m_autoReviveTimeScaleTeam2 = newScale;
	}
}

//------------------------------------------------------------------------
int CGameRulesMPSpawningBase::GetTeamSpawnLocations( int teamId, bool bInitialSpawns, TSpawnLocations &results )
{
	TSpawnLocations &spawnLocations = (bInitialSpawns && !m_initialSpawnLocations.empty()) ? m_initialSpawnLocations[m_activeInitialSpawnGroupIndex].m_locations : m_spawnLocations;
	int numSpawnLocations = spawnLocations.size();
	results.reserve(numSpawnLocations);
	for (int i = 0; i < numSpawnLocations; ++ i)
	{
		EntityId spawnId = spawnLocations[i];
		int nSpawnLocationTeam = GetSpawnLocationTeam(spawnId);
		if (nSpawnLocationTeam == teamId || (m_teamSpawnsType == eTST_Initial && nSpawnLocationTeam == 0))
		{
			results.push_back(spawnId);
		}
	}

	return results.size();
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::HostMigrationInsertIntoReviveQueue( EntityId playerId, float timeTillRevive )
{
	TPlayerDataMap::iterator it = m_playerValues.begin();
	for (; it != m_playerValues.end(); ++it)
	{
		if (it->first == playerId)
		{
			// Fake death time so that the player is auto revived at the correct time
			float gameTime = GetTime();
			const int  playerTeam = m_pGameRules->GetTeam(playerId);
			it->second.deathTime = (gameTime + timeTillRevive) - GetAutoReviveTimeScaleForTeam(playerTeam);
			break;
		}
	}
}

//------------------------------------------------------------------------
bool CGameRulesMPSpawningBase::CheckMidRoundJoining(CActor * pActor, EntityId playerId)
{
	bool  ok = true;

	if (!m_allowMidRoundJoiningBeforeFirstDeath && !m_allowMidRoundJoining)
	{
		ok = false;

		if (pActor->GetSpectatorState() == CActor::eASS_ForcedEquipmentChange)
		{
			ok = true;
		}

		if (!ok)
		{
			assert(gEnv->bServer);
			ok = true;
			
			if (IGameRulesPlayerStatsModule *pStatsModule = m_pGameRules->GetPlayerStatsModule())
			{
				const SGameRulesPlayerStat *pStats = pStatsModule->GetPlayerStats(playerId);
				if (pStats && pStats->flags & SGameRulesPlayerStat::PLYSTATFL_CANTSPAWNTHISROUND)
				{
					CryLog("CGameRulesMPSpawningBase::CheckMidRoundJoining() player id %u has PLYSTATFL_CANTSPAWNTHISROUND flag set, probably refusing spawn", playerId);
					ok = false;
				}
			}

			if (!ok)
			{
				// If there aren't currently enough players for a game, allow mid-game joining
				if (m_pGameRules->GetTeamCount() > 1)
				{
					int teamId = m_pGameRules->GetTeam(playerId);
					int numTeamMembers = m_pGameRules->GetTeamPlayerCount(teamId, true);
					if (numTeamMembers == 1)
					{
						CryLog("CGameRulesMPSpawningBase::CheckMidRoundJoining() playerId=%d is the only person on their team, allowing spawn", playerId);
						ok = true;
					}
				}
				else
				{
					int numPlayers = m_pGameRules->GetPlayerCount(true);
					if (numPlayers <= 2)
					{
						CryLog("CGameRulesMPSpawningBase::CheckMidRoundJoining() not enough players in game yet, allowing spawn for playerId=%d", playerId);
						ok = true;
					}
				}
			}
		}

	}

	return ok;
}

//------------------------------------------------------------------------
EntityId CGameRulesMPSpawningBase::GetRandomSpawnLocation_Cached( const TSpawnLocations &spawnLocations, EntityId playerId, int playerTeamId, bool isInitalSpawn )
{
	//There is no-one in the game, use a random spawner.
	//Cache it and recycle if it becomes invalid or if it times out.
	if(m_fCachedRandomSpawnTimeOut>0.f)
	{
		if(m_cachedRandomSpawnId==0 || !SpawnIsValid(m_cachedRandomSpawnId, playerId))
		{
			// Force a new random spawn to be cached.
			m_fCachedRandomSpawnTimeOut = 0.f;
		}
	}

	if(m_fCachedRandomSpawnTimeOut<=0.f)
	{
		//CryLogAlways("[SPAWN] Random spawn occurring");

		// Find a new spawn point to cache.
		m_cachedRandomSpawnId = GetRandomSpawnLocation(spawnLocations, playerId, playerTeamId, isInitalSpawn);
		m_fCachedRandomSpawnTimeOut = g_pGameCVars->g_randomSpawnPointCacheTime;
		return m_cachedRandomSpawnId;
	}
	else
	{
		//CryLogAlways("[SPAWN] Using cached spawn point [%d]", m_cachedAloneSpawnId);

		// Use cached spawn.
		return m_cachedRandomSpawnId;
	}
}

//------------------------------------------------------------------------
EntityId CGameRulesMPSpawningBase::GetRandomSpawnLocation( const TSpawnLocations &spawnLocations, EntityId playerId, int playerTeamId, bool isInitalSpawn ) const
{
	//CryLog("CGameRulesMPSpawningBase::GetRandomSpawnLocation() playerTeamId=%d", playerTeamId);

	TSpawnLocations eligibleSpawns;
	GetEligibleSpawnListForTeam(spawnLocations, eligibleSpawns, playerTeamId, isInitalSpawn);

	EntityId spawnId = 0;
	const int numElegibleSpawnPoints = eligibleSpawns.size();
	if (numElegibleSpawnPoints)
	{
		int loops = 20;
		bool bValid = false;
		do 
		{
			int index = g_pGame->GetRandomNumber() % numElegibleSpawnPoints;
			spawnId = eligibleSpawns[index];
			bValid = SpawnIsValid(spawnId, playerId);
			loops--;
		} while (!bValid && loops>0);
		
		VerboseDebugLog("CGameRulesMPSpawningBase::GetRandomSpawnLocation() picked spawn point eid=%i (%i of %i)", spawnId, index, numElegibleSpawnPoints);
	}
	else
	{
		VerboseDebugLog("CGameRulesMPSpawningBase::GetRandomSpawnLocation() failed to find elegible spawn point");
		CryFatalError("CGameRulesMPSpawningBase::GetRandomSpawnLocation() failed to find elegible spawn point");
	}

	return spawnId;
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::GetEligibleSpawnListForTeam(const TSpawnLocations& rAllLocations, TSpawnLocations& rEligibleLocations, int nTeam, bool bInitialSpawn) const
{
	const int numSpawnPoints = rAllLocations.size();
	rEligibleLocations.reserve(numSpawnPoints);

	for (int i = 0; i < numSpawnPoints; ++ i)
	{
		EntityId spawnId = rAllLocations[i];

		if (nTeam > 0)
		{
			int spawnTeam = GetSpawnLocationTeam(spawnId);
			if ((spawnTeam != 0) && (spawnTeam != nTeam))		// Allow spawns of the correct team or of no team
			{
				continue;
			}
		}

		if (bInitialSpawn || DoesSpawnLocationRespectPOIs(spawnId))
		{
			rEligibleLocations.push_back(spawnId);
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::EntityRevived(EntityId entityId)
{
	TPlayerDataMap::iterator it = m_playerValues.find(entityId);
	if (it != m_playerValues.end())
	{
		it->second.lastRevivedTime = GetTime();
		it->second.triedAutoRevive = false;
	}
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::OnInitialEquipmentScreenShown()
{
	m_bEquipmentScreenShown = true;
	m_bPrecacheContinuousBestSpawns = true;
	IGameRulesStateModule *pStateModule = g_pGame->GetGameRules()->GetStateModule();
	if (pStateModule != NULL && (pStateModule->GetGameState() == IGameRulesStateModule::EGRS_InGame))
	{
		CryLog("CGameRulesMPSpawningBase::OnInitialEquipmentScreenShown() game in progress, starting auto spawn timer");
		m_fTimeTillInitialAutoSpawn = m_fInitialAutoSpawnTimer;
		m_bNeedToStartAutoReviveTimer = false;
	}
}

void CGameRulesMPSpawningBase::OnNewRoundEquipmentScreenShown()
{
	m_bPrecacheContinuousBestSpawns = true;
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::OnRoundStart()
{
	if (m_bEquipmentScreenShown)
	{
		CryLog("CGameRulesMPSpawningBase::OnRoundStart() equipment shown, starting auto spawn timer");
		CActor *pPlayer = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
		if (pPlayer != NULL && pPlayer->GetSpectatorState() == CActor::eASS_ForcedEquipmentChange)
		{
			m_fTimeTillInitialAutoSpawn = m_fInitialAutoSpawnTimer;
			m_bNeedToStartAutoReviveTimer = false;
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::OnEntityKilled(const HitInfo &hitInfo)
{
	if (gEnv->bServer)
	{
		EntityId  entityId = hitInfo.targetId;
#if !defined(_RELEASE)
		IEntity*  e = gEnv->pEntitySystem->GetEntity(entityId);
		CryLog("[tlh] @ CGameRulesMPSpawningBase::OnEntityKilled(), entity=%s", (e?e->GetName():"!"));
#endif
		IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId);
		if (pActor != NULL && pActor->IsPlayer())
		{
			CryLog("      > got OnEntityKilled broadcast, frame = %d", gEnv->nMainFrameID);
			if (IGameRulesStateModule* pStateModule=m_pGameRules->GetStateModule())
			{
				if (pStateModule->GetGameState() == IGameRulesStateModule::EGRS_InGame)
				{
					m_svKillsSum++;
				}
			}
		}
	}
}

//------------------------------------------------------------------------
float CGameRulesMPSpawningBase::GetRemainingInitialAutoSpawnTimer()
{
	return m_fTimeTillInitialAutoSpawn;
}

//------------------------------------------------------------------------
bool CGameRulesMPSpawningBase::SvIsMidRoundJoiningAllowed() const 
{ 
	bool allowed=true;

	CRY_ASSERT(gEnv->bServer);

	if ( m_allowMidRoundJoiningBeforeFirstDeath )
	{
		if (m_svKillsSum > 0 )
		{
			allowed=false;
		}
	}
	else
	{
		allowed=m_allowMidRoundJoining; 
	}

	return allowed;
}

//------------------------------------------------------------------------
bool CGameRulesMPSpawningBase::IsInInitialChannelsList( uint32 channelId ) const
{
	if (m_initialChannelIDs.size() != 0)
	{
		for (uint32 i=0; i<m_initialChannelIDs.size(); i++)
		{
			if (m_initialChannelIDs[i] == channelId)
			{
				return true;
			}
		}
	}
	else
	{
		// If we get here it's probably because m_initialChannelIDs hasn't been initialised yet
		// that means everybody should be in the list
		return true;
	}
	return false;
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::HostMigrationStopAddingPlayers()
{
	m_spawningVisTable.HostMigrationStopAddingPlayers();
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::HostMigrationResumeAddingPlayers()
{
	m_spawningVisTable.HostMigrationResumeAddingPlayers();
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::OnRoundEnd()
{
	if (gEnv->bServer)
	{
		SelectNewInitialSpawnGroup();
	}
}

//------------------------------------------------------------------------
void CGameRulesMPSpawningBase::OnStartGame()
{
	m_bGameHasStarted = true;
	if (gEnv->bServer)
	{
		SelectNewInitialSpawnGroup();
	}
}
