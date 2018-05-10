// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Objectives for the predator game mode
	-------------------------------------------------------------------------
	History:
	- 09:05:2011  : Created by Colin Gulliver

*************************************************************************/

#include "StdAfx.h"

#include "GameRulesObjective_Predator.h"

#include "GameRules.h"
#include "IGameRulesScoringModule.h"
#include "IGameRulesStateModule.h"
#include "IGameRulesSpawningModule.h"
#include "IGameRulesPlayerStatsModule.h"
#include "IGameRulesSpectatorModule.h"
#include "IGameRulesRoundsModule.h"
#include "IGameRulesVictoryConditionsModule.h"
#include "PlayerPlugin_Interaction.h"
#include "GameActions.h"
#include "Player.h"
#include "Network/Lobby/GameLobby.h"
#include "EquipmentLoadout.h"
#include "ActorManager.h"
#include <CrySystem/XML/IXml.h>
#include "Audio/Announcer.h"
#include "TacticalManager.h"

#include "Effects/GameEffects/HudInterferenceGameEffect.h"

#include "Utility/DesignerWarning.h"

#include "UI/HUD/HUDUtils.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/UIManager.h"
#include "UI/UICVars.h"

#include "Utility/CryWatch.h"

#include "StatsRecordingMgr.h"
#include "PersistantStats.h"

#define SINGLE_ENTITY_RMI_SET_PREDATOR_LOADOUT					0x01
#define SINGLE_ENTITY_RMI_CLIENT_READY_TO_RESPAWN				0x02	//Client player's corpse/death/kill-cam finished and now in spectator mode prior to re-spawning
#define SINGLE_ENTITY_RMI_PREPARE_CHANGE_TEAM_PREDATOR	0x03  //Marine has died and will become a predator in the future

#define LOADOUT_PACKAGE_GROUP_PREDATOR			CEquipmentLoadout::SDK
#define LOADOUT_PACKAGE_GROUP_SOLDIER				CEquipmentLoadout::SDK

#if NUM_ASPECTS > 8
	#define PREDATOR_OBJECTIVE_STATE_ASPECT		eEA_GameServerA
#else
	#define PREDATOR_OBJECTIVE_STATE_ASPECT		eEA_GameServerStatic
#endif

#ifndef _RELEASE
	#define HUNTER_DEBUG_LOG(...) CryLogAlways("[HUNTER MODE]: " __VA_ARGS__)
#else
	#define HUNTER_DEBUG_LOG(...)
#endif

#define INVALID_AMMOGROUP_ID ~0


//-------------------------------------------------------------------------
CGameRulesObjective_Predator::CGameRulesObjective_Predator()
	: m_pGameRules(nullptr)
	, m_moduleRMIIndex(-1)
	, m_numStartPredators(1)
	, m_clientTeam(0)
	, m_numRemainingSoldiers(0)
	, m_SoldierSurvivalScorePeriod(0)
	, m_fMinimalReviveTime(0.0f)
	, m_fSoldierSuicideRespawnTimeMult(0.0f)
	, m_fLateJoiningGracePeriod(15.0f)
	, m_fDelayedAnnouncementTimer(0.0f)
	, m_previousEnemyTimeUntilLockObtained(0.0f)
	, m_pDelayedAnnouncement(nullptr)
	, m_flags(0)
	, m_ClientHasDiedAsMarine(false)
	, m_AMarineHasBeenKilledByRemotePlayerThisRound(false)
	, m_ammoCacheGroupId(INVALID_AMMOGROUP_ID)
{
}

//-------------------------------------------------------------------------
CGameRulesObjective_Predator::~CGameRulesObjective_Predator()
{
	g_pGame->GetUI()->GetCVars()->hud_tagnames_EnemyTimeUntilLockObtained = m_previousEnemyTimeUntilLockObtained;

	m_pGameRules->UnRegisterKillListener(this);
	m_pGameRules->UnRegisterClientConnectionListener(this);
	m_pGameRules->UnRegisterTeamChangedListener(this);
	m_pGameRules->UnRegisterRevivedListener(this);
	m_pGameRules->UnRegisterModuleRMIListener(m_moduleRMIIndex);
	m_pGameRules->UnRegisterRoundsListener(this);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Init( XmlNodeRef xml )
{
	m_pGameRules = g_pGame->GetGameRules();
	IEntityClassRegistry *pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	CGameAudio *pGameAudio = g_pGame->GetGameAudio();

	m_previousEnemyTimeUntilLockObtained = g_pGame->GetUI()->GetCVars()->hud_tagnames_EnemyTimeUntilLockObtained;
	g_pGame->GetUI()->GetCVars()->hud_tagnames_EnemyTimeUntilLockObtained = g_pGameCVars->g_predator_marineRedCrosshairDelay;
	
	const int numChildren = xml->getChildCount();
	for (int i = 0; i < numChildren; ++ i)
	{
		XmlNodeRef xmlChild = xml->getChild(i);
		const char *pTag = xmlChild->getTag();
		if (!stricmp(pTag, "Params"))
		{
			xmlChild->getAttr("numPredators", m_numStartPredators);
			xmlChild->getAttr("SoldierSurvivalScorePeriod", m_SoldierSurvivalScorePeriod);
			xmlChild->getAttr("MinimalReviveTime", m_fMinimalReviveTime);
			xmlChild->getAttr("SoldierSuicideRespawnTimeMult", m_fSoldierSuicideRespawnTimeMult);
			xmlChild->getAttr("LateJoiningGracePeriod", m_fLateJoiningGracePeriod);
			
			XmlNodeRef survivalPointsRoot = xmlChild->findChild("SurvivalPoints");
			if (survivalPointsRoot)
			{
				int numBands = survivalPointsRoot->getChildCount();
				for (int j = 0; j < numBands; ++ j)
				{
					int soldiers;
					int points;
					XmlNodeRef xmlBand = survivalPointsRoot->getChild(j);
					if (xmlBand->getAttr("count", soldiers) && xmlBand->getAttr("points", points))
					{
						DesignerWarning((soldiers & 0xFF) == soldiers, "Predator: Soldiers count (%d) won't fit in a uint8", soldiers);
						DesignerWarning((points & 0xFF) == points, "Predator: Points (%d) won't fit in a uint8", points);

						SSurvivalPointsData band;
						band.m_soldierCount = (uint8) soldiers;
						band.m_score = (uint8) points;
						m_survivalPoints.push_back(band);
					}
				}
			}

			XmlNodeRef ammoOverridesRoot = xmlChild->findChild("AmmoOverrides");
			if (ammoOverridesRoot)
			{
				const char *pClassName;
				int clips;

				int numWeapons = ammoOverridesRoot->getChildCount();
				m_weaponAmmoOverrides.reserve(numWeapons);
				for (int j = 0; j < numWeapons; ++ j)
				{
					XmlNodeRef weaponOverrideNode = ammoOverridesRoot->getChild(j);
					if (!stricmp(weaponOverrideNode->getTag(), "Weapon"))
					{
						if (weaponOverrideNode->getAttr("Class", &pClassName))
						{
							IEntityClass *pClass = pClassRegistry->FindClass(pClassName);
							if (pClass)
							{
								m_weaponAmmoOverrides.push_back(SWeaponAmmoOverride());
								SWeaponAmmoOverride &weapon = m_weaponAmmoOverrides[m_weaponAmmoOverrides.size() - 1];
								int numAmmoTypes = weaponOverrideNode->getChildCount();
								weapon.m_pWeaponClass = pClass;
								weapon.m_bonusAmmo.reserve(numAmmoTypes);
								for (int k = 0; k < numAmmoTypes; ++ k)
								{
									XmlNodeRef ammoNode = weaponOverrideNode->getChild(k);
									if (ammoNode->getAttr("Class", &pClassName) && ammoNode->getAttr("clips", clips))
									{
										pClass = pClassRegistry->FindClass(pClassName);
										if (pClass)
										{
											weapon.m_bonusAmmo.push_back(SAmmoOverride());
											SAmmoOverride &ammo = weapon.m_bonusAmmo[weapon.m_bonusAmmo.size() - 1];
											ammo.m_pAmmoClass = pClass;
											ammo.m_clips = clips;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	m_pGameRules->RegisterKillListener(this);
	m_pGameRules->RegisterClientConnectionListener(this);
	m_pGameRules->RegisterTeamChangedListener(this);
	m_pGameRules->RegisterRevivedListener(this);
	m_pGameRules->RegisterRoundsListener(this);
	m_moduleRMIIndex = m_pGameRules->RegisterModuleRMIListener(this);

	m_lastManSignalIdMarine = pGameAudio->GetSignalID("Hunter_Lastman_Marine");
	m_lastManSignalIdHunter = pGameAudio->GetSignalID("Hunter_Lastman_Hunter");
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Update( float fFrameTime )
{
	if (!m_pGameRules->HasGameActuallyStarted())
	{
		return;
	}

	float currentGameTime = m_pGameRules->GameTimeValid() ? m_pGameRules->GetCurrentGameTime() : 0.f;
	if(!Common_IsFlagSet(ePF_GracePeriodEventSent) && (currentGameTime > m_fLateJoiningGracePeriod))
	{
		SHUDEvent event(eHUDEvent_PredatorGracePeriodFinished);
		CHUDEventDispatcher::CallEvent(event);
		Common_SetFlag(ePF_GracePeriodEventSent);
	}

	if (gEnv->bServer)
	{
		if(m_pGameRules->GetRemainingGameTime() > 0.0f)
		{
			int remainder = (int)currentGameTime % m_SoldierSurvivalScorePeriod;
			int prevRemainder = (int)(currentGameTime - gEnv->pTimer->GetFrameTime()) % m_SoldierSurvivalScorePeriod;
			if(remainder == 0 && prevRemainder > 0)
			{
				Server_GiveSoldierSurvivingPoints(false);
			}
		}
	}

	//If we know our client player will be switching to a predator soon then wait until they are able to revive
	if(Common_IsFlagSet(ePF_ClientSwitchingToPredator))
	{ 
		const EntityId playerId = g_pGame->GetIGameFramework()->GetClientActorId();
		Common_ClearFlag(ePF_ClientSwitchingToPredator);

		if(!gEnv->bServer)
		{
			HUNTER_DEBUG_LOG("ePF_ClientSwitchingToPredator is set, informing server SINGLE_ENTITY_RMI_CLIENT_READY_TO_RESPAWN for client actor");
			CGameRules::SModuleRMIEntityParams params;
			params.m_listenerIndex = m_moduleRMIIndex;
			params.m_entityId = playerId;
			params.m_data = SINGLE_ENTITY_RMI_CLIENT_READY_TO_RESPAWN;
			m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::SvModuleRMISingleEntity(), params, eRMI_ToServer, playerId);
		}
		else
		{
			HUNTER_DEBUG_LOG("ePF_ClientSwitchingToPredator is set, we ARE Server, calling Server_PredatorReadyToRespawn() for client actor");
			Server_PredatorReadyToSpawn(playerId);
		}

		CHUDEventDispatcher::CallEvent(SHUDEvent( eHUDEvent_OnLocalPlayerCanNowRespawn ));
	}

	if (gEnv->IsClient())
	{
		if (m_pGameRules->IsTimeLimited())
		{
			float remainingTime = m_pGameRules->GetRemainingGameTime();
			if (remainingTime < 10.f)
			{
				Client_AnnounceTimeRemaining(ePF_Sent10SecondsRemainingEvent, NULL, "Hunter_10Seconds");
			}
			else if (remainingTime < 30.f)
			{
				Client_AnnounceTimeRemaining(ePF_Sent30SecondsRemainingEvent, "Marine_30Seconds", "Hunter_30Seconds");
			}
			else if (remainingTime < 60.f)
			{
				Client_AnnounceTimeRemaining(ePF_Sent60SecondsRemainingEvent, "Marine_60Seconds", "Hunter_60Seconds");
			}
			else if (remainingTime < 90.f)
			{
				Client_AnnounceTimeRemaining(ePF_Sent90SecondsRemainingEvent, "Marine_90Seconds", "Hunter_90Seconds");
			}
		}

		if (IGameRulesStateModule* pStateModule = m_pGameRules->GetStateModule())
		{
			if (pStateModule->GetGameState() == IGameRulesStateModule::EGRS_InGame)
			{
				if (m_pDelayedAnnouncement && m_pGameRules->GetRoundsModule()->IsInProgress())
				{
					m_fDelayedAnnouncementTimer -= fFrameTime;
					if (m_fDelayedAnnouncementTimer < 0.f)
					{
						CAnnouncer::GetInstance()->Announce(m_pDelayedAnnouncement, CAnnouncer::eAC_inGame);
						m_pDelayedAnnouncement = NULL;
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesObjective_Predator::Client_AnnounceTimeRemaining( EPredatorFlags flag, const char *pMarineEvent, const char *pHunterEvent )
{
	if (!Common_IsFlagSet(flag))
	{
		Common_SetFlag(flag);

		bool bSoldier = (m_pGameRules->GetTeam(gEnv->pGameFramework->GetClientActorId()) == TEAM_SOLDIER);
		if (bSoldier && pMarineEvent)
		{
			CAnnouncer::GetInstance()->Announce(pMarineEvent, CAnnouncer::eAC_inGame);
		}
		else if (!bSoldier && pHunterEvent)
		{
			CAnnouncer::GetInstance()->Announce(pHunterEvent, CAnnouncer::eAC_inGame);
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnStartGame()
{
	m_pGameRules->GetSpawningModule()->SetAutoReviveTimeScaleForTeam(TEAM_PREDATOR, 0.7f);
	
	uint32 numCacheGroups = 0;

	// Construct our ammo cache groups
	const CTacticalManager::TInterestPoints& ammoPoints = g_pGame->GetTacticalManager()->GetTacticalPoints(CTacticalManager::eTacticalEntity_Ammo);
	int numPoints = ammoPoints.size();
	for (int i = 0; i < numPoints; ++ i)
	{
		EntityId crateId = ammoPoints[i].m_entityId;
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(crateId);
		if (pEntity)
		{
			const char *pGroupName;
			if (EntityScripts::GetEntityProperty(pEntity, "GroupId", pGroupName))
			{
				uint32 groupId = CCrc32::ComputeLowercase(pGroupName);
				bool bFound = false;
				for (uint32 j = 0; j < numCacheGroups; ++ j)
				{
					if (m_ammoCacheGroups[j].m_groupId == groupId)
					{
						m_ammoCacheGroups[j].m_entityIds.push_back(crateId);
						bFound = true;
					}
				}
				if (!bFound)
				{
					m_ammoCacheGroups.push_back();
					SAmmoCacheGroup &group = m_ammoCacheGroups[numCacheGroups ++];
					group.m_groupId = groupId;
					group.m_usageCount = 0;
					group.m_entityIds.push_back(crateId);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnStartGamePost()
{

	Common_SetAmmoCacheGroupActive(m_ammoCacheGroupId, true);

	for (uint32 i = 0, numCacheGroups = m_ammoCacheGroups.size(); i < numCacheGroups; ++ i)
	{
		SAmmoCacheGroup &group = m_ammoCacheGroups[i];
		if (group.m_groupId != m_ammoCacheGroupId)
		{
			int numCaches = group.m_entityIds.size();
			for (int j = 0; j < numCaches; ++ j)
			{
				IEntity *pEntity = gEnv->pEntitySystem->GetEntity(group.m_entityIds[j]);
				if (pEntity)
				{
					EntityScripts::CallScriptFunction(pEntity, pEntity->GetScriptTable(), "Enabled", 0);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::PostInitClient( int channelId )
{
}

//-------------------------------------------------------------------------
bool CGameRulesObjective_Predator::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (aspect == PREDATOR_OBJECTIVE_STATE_ASPECT)
	{
		int previousRemainingSoldiers = m_numRemainingSoldiers;
		ser.Value("remainingSoldiers", m_numRemainingSoldiers, 'ui4');
		if (previousRemainingSoldiers != m_numRemainingSoldiers)
		{
			Client_OnSoldiersRemainingCountChanged(previousRemainingSoldiers);
		}
	}
	return true;
}

//-------------------------------------------------------------------------
bool CGameRulesObjective_Predator::HasCompleted( int teamId )
{
	// If time expires, the remaining marines win
	if ((teamId == TEAM_SOLDIER) && m_pGameRules->IsTimeLimited())
	{
		float timeRemaining = m_pGameRules->GetRemainingGameTime();

		if (timeRemaining <= 0.f)
		{
			HUNTER_DEBUG_LOG("CGameRulesObjective_Predator::HasCompleted() - TimeRemaining < 0.0f - marines have survived until end of game");
			Server_GiveSoldierSurvivingPoints(true);

			if( CStatsRecordingMgr* pStats = g_pGame->GetStatsRecorder() )
			{
				if( IStatsTracker* pRoundTracker = pStats->GetRoundTracker() )
				{
					pRoundTracker->Event( eGSE_Hunter_RoundEnd, TEAM_SOLDIER );
				}
			}

			return true;
		}
	}
	else if (teamId == TEAM_PREDATOR)
	{
		if (Common_IsFlagSet(ePF_AllSoldiersDead))
		{
			Common_ClearFlag(ePF_AllSoldiersDead);

			if( CStatsRecordingMgr* pStats = g_pGame->GetStatsRecorder() )
			{
				if( IStatsTracker* pRoundTracker = pStats->GetRoundTracker() )
				{
					pRoundTracker->Event( eGSE_Hunter_RoundEnd, TEAM_PREDATOR );
				}
			}

			//if Hunters won, and no marines were killed by remote players, then local player must have killed all marines themselves
			if( m_AMarineHasBeenKilledByRemotePlayerThisRound == false )
			{
				CPersistantStats::GetInstance()->IncrementClientStats( EIPS_TheHunter );
			}

			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::AssignTeamsFromLobby()
{
	TIntArray initialPredatorChannelIds;
	TIntArray initialSoldierChannelIds;

	// First player has reached team selection, need to make up our minds about initial teams!
	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	if (pGameLobby)
	{
		const SSessionNames &sessionNames = pGameLobby->GetSessionNames();
		int numPlayers = sessionNames.Size();

		int numPredatorsNeeded = Common_GetMaxStartPredators(numPlayers);

#ifndef _RELEASE
		const char *pForceName[] = {	g_pGameCVars->g_predator_forcePredator1->GetString(),
			g_pGameCVars->g_predator_forcePredator2->GetString(),
			g_pGameCVars->g_predator_forcePredator3->GetString(),
			g_pGameCVars->g_predator_forcePredator4->GetString() };
		int numForcePredatorsChosen = 0;
#endif

		for (int i = 0; i < numPlayers; ++ i)
		{
			int uid = (int) sessionNames.m_sessionNames[i].m_conId.m_uid;

#ifndef _RELEASE
			bool bFound = false;
			for (int j = 0; j < 4; ++ j)
			{
				if (stricmp(pForceName[j], sessionNames.m_sessionNames[i].m_name) == 0)
				{
					++ numForcePredatorsChosen;
					initialPredatorChannelIds.push_back(uid);
					bFound = true;
					break;
				}
			}
			if (bFound)
			{
				continue;
			}
#endif

			initialSoldierChannelIds.push_back(uid);
		}

#ifndef _RELEASE
		numPredatorsNeeded -= numForcePredatorsChosen;
		if (numPredatorsNeeded < 0)
		{
			for (int i = numPredatorsNeeded; i < 0; ++ i)
			{
				initialSoldierChannelIds.push_back(initialPredatorChannelIds[initialPredatorChannelIds.size() - 1]);
				initialPredatorChannelIds.pop_back();
			}
		}
#endif
		numPlayers = initialSoldierChannelIds.size();
		for (int i = 0; i < numPredatorsNeeded && numPlayers > 0; ++ i)
		{
			uint32 randomNum = g_pGame->GetRandomNumber();
			uint32 index = randomNum % numPlayers;
			initialPredatorChannelIds.push_back(initialSoldierChannelIds[index]);
			initialSoldierChannelIds.removeAt(index);
			-- numPlayers;
		}

		TIntArray::const_iterator iter = initialPredatorChannelIds.begin();
		TIntArray::const_iterator end = initialPredatorChannelIds.end();
		while(iter != end)
		{
			m_PredatorData[*iter].bHasStartedAsPredator = true;
			m_PredatorData[*iter].bIsPredator = true;
			m_PredatorData[*iter].bStartedThisRoundAsPredator = true;
			++iter;
		}

		iter = initialSoldierChannelIds.begin();
		end = initialSoldierChannelIds.end();
		while(iter != end)
		{
			m_PredatorData[*iter].bHasStartedAsPredator = false;
			m_PredatorData[*iter].bIsPredator = false;
			++iter;
		}
	}
}

//-------------------------------------------------------------------------
int CGameRulesObjective_Predator::GetAutoAssignTeamId( int channelId )
{
	if (m_PredatorData.empty())
	{
		AssignTeamsFromLobby();
	}

#ifndef _RELEASE
	if (g_pGameCVars->g_predator_forceSpawnTeam != 0)
	{
		return g_pGameCVars->g_predator_forceSpawnTeam;
	}
#endif
	
	// Rules:
	// - When the server finishes loading it selects two players to be predators if enough players present, the rest are soldiers
	// - Subsequent joiners will become predators if we still require predators to meet our Common_GetMaxStartPredators() count (I.e. if there were NOT enough players present when the server chose initial teams)
	// - Any player who doesn't get into game before the grace period expires is made into a predators
	// - Anyone not selected as an initial predator who joins before the end of the grace period is a soldier


	// Was this player already selected as an initial predator?
	TPredatorData::const_iterator predIter = m_PredatorData.find(channelId);
	if ((predIter != m_PredatorData.end()) && predIter->second.bHasStartedAsPredator)
	{
		CryLog( "[HUNTER] Spawning as hunter as designated in the lobby (channelId=%d)", channelId);
		return TEAM_PREDATOR;
	}

	// They may have joined after initial selection, Do we *STILL* require more predators?
	int startPredCount = 0, currPredCount = 0, startMarineCount = 0;
	TPredatorData::const_iterator predCountIter			= m_PredatorData.begin();
	TPredatorData::const_iterator predCountEndIter	= m_PredatorData.end();
	while(predCountIter != predCountEndIter)
	{
		if(predCountIter->second.bStartedThisRoundAsPredator)
		{
			++startPredCount;
		}
		else
		{
			++startMarineCount;
		}

		if(predCountIter->second.bIsPredator)
		{
			++currPredCount;
		}
		
		++predCountIter;
	}

	//If we have had enough players start as marines, we can put predators in if the grace period is up
	if(startMarineCount > startPredCount)
	{
		CryLog( "[HUNTER] %d players started as marine, %d started as hunter", startMarineCount, startPredCount);
		float gameTime = ( m_pGameRules->HasGameActuallyStarted() && m_pGameRules->GetRoundsModule()->IsInProgress() && m_pGameRules->GameTimeValid() ) ? m_pGameRules->GetCurrentGameTime() : 0.f;
		if (gameTime > m_fLateJoiningGracePeriod)
		{
			CryLog( "[HUNTER] Forced team to hunter as joining late (channelId=%d)", channelId);
			m_PredatorData[channelId].bIsPredator = true;
			return TEAM_PREDATOR;
		}

		// If we are not up to our quota, this joiner can be a predator.
		int desiredNumPredators = Common_GetMaxStartPredators(m_PredatorData.size());
		if(currPredCount < desiredNumPredators)
		{
			CryLog( "[HUNTER] Forced team to hunter as currently %d hunters out of a desired %d (channelId=%d)", currPredCount, desiredNumPredators, channelId);
			m_PredatorData[channelId].bHasStartedAsPredator = true;
			m_PredatorData[channelId].bIsPredator = true;
			m_PredatorData[channelId].bStartedThisRoundAsPredator = true;
			return TEAM_PREDATOR;
		}
	}

	// Otherwise we're a soldier
	CryLog( "[HUNTER] Spawning as marine (channelId=%d)", channelId);
	m_PredatorData[channelId].bIsPredator = false;
	return TEAM_SOLDIER;
}

//-------------------------------------------------------------------------
bool CGameRulesObjective_Predator::CanPlayerRegenerate( EntityId playerId ) const
{
	if (m_pGameRules->GetTeam(playerId) == TEAM_SOLDIER)
	{
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------
bool CGameRulesObjective_Predator::CanPlayerRevive( EntityId playerId ) const
{
#if USE_PC_PREMATCH
	if(m_pGameRules->GetPrematchState() != CGameRules::ePS_Match)
	{
		return true;
	}
#endif

	//decent chance of having an incorrect time remaining before game starts
	if( m_pGameRules->GameTimeValid() )
	{
		//Don't let people spawn in the last few seconds of the game
		float remainingGameTime = m_pGameRules->IsTimeLimited() ? m_pGameRules->GetRemainingGameTimeNotZeroCapped() : FLT_MAX;
		if(remainingGameTime < GetMinimalReviveTime())
		{
			return false;
		}
	}

	//If we know our client player will be switching to a predator soon then wait until the killcam video has finished
	if(Common_IsFlagSet(ePF_ClientSwitchingToPredator) && playerId == g_pGame->GetIGameFramework()->GetClientActorId())
	{
		return false;
	}

	//Players in this list are waiting to become predators and so should not be allowed to spawn
	if(gEnv->bServer)
	{
		TIntArray::const_iterator iter = m_deadSoldierIds.begin();
		TIntArray::const_iterator end = m_deadSoldierIds.end();
		while(iter != end)
		{
			if(*iter == playerId)
			{
				return false;
			}
			++iter;
		}
	}

	return true;
}

//-------------------------------------------------------------------------
float CGameRulesObjective_Predator::GetPlayerAutoReviveAdditionalTime( IActor* pActor ) const
{
	if(gEnv->bServer)
	{
		TPredatorData::const_iterator result = m_PredatorData.find(pActor->GetChannelId());
		if(result != m_PredatorData.end())
		{
			return result->second.fReviveTimeMult * g_pGameCVars->g_revivetime;
		}
	}
	else
	{
		return Common_IsFlagSet(ePF_ClientSuicide) ? m_fSoldierSuicideRespawnTimeMult * g_pGameCVars->g_revivetime : 0.f;
	}

	return 0.f;
}

//-------------------------------------------------------------------------
bool CGameRulesObjective_Predator::MustShowHealthEffect(EntityId playerId) const
{
	return m_pGameRules->GetTeam(playerId) != TEAM_SOLDIER;
}

//-------------------------------------------------------------------------
bool CGameRulesObjective_Predator::IsWinningKill( const HitInfo &hitInfo ) const
{
	if (m_pGameRules->GetTeam(hitInfo.targetId) == TEAM_SOLDIER)
	{
		return Server_WasFinalKill(hitInfo.targetId);
	}
	return false;
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnSingleEntityRMI( CGameRules::SModuleRMIEntityParams params )
{
	switch (params.m_data)
	{
	case SINGLE_ENTITY_RMI_SET_PREDATOR_LOADOUT:
		{
			CPlayer *pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(params.m_entityId));
			if (pPlayer && pPlayer->IsClient())
			{
				CryLog("PREDATORDEBUG %i Received set loadout message from server", params.m_entityId);
				Client_SetPredatorLoadout();
			}
		}
		break;
	case SINGLE_ENTITY_RMI_CLIENT_READY_TO_RESPAWN:
		{
			Server_PredatorReadyToSpawn(params.m_entityId);
			break;
		}
	case SINGLE_ENTITY_RMI_PREPARE_CHANGE_TEAM_PREDATOR:
		{
			Common_PrepareChangeTeamPredator(params.m_entityId, false);
			break;
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnChangedTeam( EntityId entityId, int oldTeamId, int newTeamId )
{
	if (entityId == gEnv->pGameFramework->GetClientActorId())
	{
		m_clientTeam = newTeamId;
		CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
		CEquipmentLoadout::EEquipmentPackageGroup group = (newTeamId == TEAM_SOLDIER) ? LOADOUT_PACKAGE_GROUP_SOLDIER : LOADOUT_PACKAGE_GROUP_PREDATOR;
		pEquipmentLoadout->SetPackageGroup(group);

		if (newTeamId == TEAM_SOLDIER)
		{
			pEquipmentLoadout->InvalidateLastSentLoadout();
		}

#ifndef _RELEASE
		if (g_pGameCVars->g_predator_predatorHasSuperRadar)
#endif
		{
			if (newTeamId == TEAM_PREDATOR)
			{
				SHUDEvent eventTeamRadar(eHUDEvent_OnTeamRadarChanged);
				eventTeamRadar.AddData(SHUDEventData(true));
				CHUDEventDispatcher::CallEvent(eventTeamRadar);
			}
			else
			{
				SHUDEvent eventTeamRadar(eHUDEvent_OnTeamRadarChanged);
				eventTeamRadar.AddData(SHUDEventData(false));
				CHUDEventDispatcher::CallEvent(eventTeamRadar);
			}
		}

		const bool bIsSoldier = newTeamId == TEAM_SOLDIER;
		const int numCacheGroups = m_ammoCacheGroups.size();
		for (int i = 0; i < numCacheGroups; ++ i)
		{
			const SAmmoCacheGroup& ammoCacheGroup = m_ammoCacheGroups[i];
			if (ammoCacheGroup.m_groupId == m_ammoCacheGroupId)
			{
				int numEntities = ammoCacheGroup.m_entityIds.size();
				for (int j = 0; j < numEntities; ++ j)
				{
					if(bIsSoldier)
					{
						SHUDEventWrapper::OnNewObjective(ammoCacheGroup.m_entityIds[j], EGRMO_Ammo_Cache);
					}
					else
					{
						SHUDEventWrapper::OnRemoveObjective(ammoCacheGroup.m_entityIds[j]);
					}
				}
			}
		}
	}

	if (gEnv->bServer && (oldTeamId == TEAM_SOLDIER))
	{
		IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId);
		if (pActor && pActor->IsPlayer() && !pActor->IsDead())
		{
			Server_OnSoldierKilledOrLeft(entityId);
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::EntityRevived( EntityId entityId )
{
	EntityId clientId = gEnv->pGameFramework->GetClientActorId();
	if (entityId == clientId)
	{
		Common_ClearFlag(ePF_ClientSuicide);

		bool bFirstSpawnThisRound = false;
		if (!Common_IsFlagSet(ePF_HasSpawned))
		{
			bFirstSpawnThisRound = true;
			Common_SetFlag(ePF_HasSpawned);

			int numCacheGroups = m_ammoCacheGroups.size();
			for (int i = 0; i < numCacheGroups; ++ i)
			{
				SAmmoCacheGroup &group = m_ammoCacheGroups[i];
				if (group.m_groupId == m_ammoCacheGroupId)
				{
					int numEntities = group.m_entityIds.size();
					for (int j = 0; j < numEntities; ++ j)
					{
						SHUDEvent hudevent(eHUDEvent_AddEntity);
						hudevent.AddData(SHUDEventData((int)group.m_entityIds[j]));
						CHUDEventDispatcher::CallEvent(hudevent);
					}
				}
			}
		}

		if (m_clientTeam == TEAM_PREDATOR)
		{
			CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
			if (pActor)
			{
				// Stop predators picking up items - they have to stick with the bow :-(
				pActor->EnablePickingUpItems(false);
			}

			if (bFirstSpawnThisRound)
			{
				m_pDelayedAnnouncement = "Hunter_Start";
				Common_SetFlag(ePF_SentHunterSpawn);

				// Server sets this flag *before* attempting to select the bow. Clients run this logic just *after* selecting the bow (too late!) :( 
				if(gEnv->bServer)
				{
					Common_SetFlag(ePF_NextClientBowFirstAsHunter);
				}
			}
			else if (!Common_IsFlagSet(ePF_SentHunterSpawn))
			{
				m_pDelayedAnnouncement = "Hunter_Converted";
				Common_SetFlag(ePF_SentHunterSpawn);
			}
		}
		else
		{
			if (Common_IsFlagSet(ePF_HasStartedAsMarine))
			{
				m_pDelayedAnnouncement = "Marine_RoundStart";
			}
			else
			{
				m_pDelayedAnnouncement = "Marine_First_RoundStart";
				Common_SetFlag(ePF_HasStartedAsMarine);
			}
		}

		if (m_pDelayedAnnouncement)
		{
			m_fDelayedAnnouncementTimer = 3.f;
		}
	}

#if USE_PC_PREMATCH
	if(m_pGameRules->GetPrematchState() != CGameRules::ePS_Match)
	{
		return;
	}
#endif

	if(gEnv->bServer)
	{
		if (m_pGameRules->GetTeam(entityId) == TEAM_SOLDIER)
		{
			Server_UpdateRemainingSoldierCount(1);
		}
		else
		{
			// Reset the server's suicide timer for this guy.
			if(IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId))
			{
				uint16 channelId = pActor->GetChannelId();
				m_PredatorData[channelId].fReviveTimeMult = 0.f;
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Server_GiveSoldierSurvivingPoints(bool survivedToEnd)
{
	IGameRulesScoringModule *pScoringModule = m_pGameRules->GetScoringModule();

	EGRST scoreType;
	TGameRulesScoreInt score = 0;
	if (survivedToEnd)
	{
		score = pScoringModule->GetPlayerPointsByType(EGRST_Predator_SurviveToEnd);
		scoreType = EGRST_Predator_SurviveToEnd;
	}
	else
	{
		int numSurvivingSoldiers = 0;
		EntityId temp = 0;
		Server_GetMarinesStatus(0, temp, numSurvivingSoldiers);

		for (int i = 0, numBands = m_survivalPoints.size(); i < numBands; ++ i)
		{
			if ((int) m_survivalPoints[i].m_soldierCount <= numSurvivingSoldiers)
			{
				score = (TGameRulesScoreInt) m_survivalPoints[i].m_score;
			}
			else
			{
				break;
			}
		}
		scoreType = EGRST_Predator_SurviveTimePeriod;
	}

	Server_LivingSoldiersScore(scoreType, score);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Server_LivingSoldiersScore( EGRST scoreType, TGameRulesScoreInt points )
{
	IGameRulesScoringModule *pScoringModule = m_pGameRules->GetScoringModule();
	CGameRules::TPlayers players;
	m_pGameRules->GetTeamPlayers(TEAM_SOLDIER, players);
	const int numPlayers = players.size();
	for (int i = 0; i < numPlayers; ++ i)
	{
		EntityId playerId = players[i];
		IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId);
		if (pActor && !pActor->IsDead())
		{
			SGameRulesScoreInfo scoreInfo(scoreType, points);
			pScoringModule->OnPlayerScoringEventWithInfo(playerId, &scoreInfo);

			if (scoreType == EGRST_Predator_SurviveToEnd)
			{
				IGameRulesPlayerStatsModule *pStatsModule = m_pGameRules->GetPlayerStatsModule();
				pStatsModule->RecordSurvivalTime(playerId, m_pGameRules->GetCurrentGameTime());
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Server_UpdateRemainingSoldierCount( int deltaCount )
{
	if (deltaCount != 0)
	{
		int previous = m_numRemainingSoldiers;
		m_numRemainingSoldiers += deltaCount;
		CHANGED_NETWORK_STATE(m_pGameRules, PREDATOR_OBJECTIVE_STATE_ASPECT);

		if (gEnv->IsClient())
		{
			Client_OnSoldiersRemainingCountChanged(previous);
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Server_OnSoldierKilledOrLeft( EntityId soldierId )
{
	EntityId lastMarineStanding = 0;
	if (m_pGameRules->HasGameActuallyStarted())
	{
		int survivorCount = 0;
		EntityId lastMarineId = 0;
		Server_GetMarinesStatus(soldierId, lastMarineId, survivorCount);

		switch (survivorCount)
		{
			case 3:
				{
					IGameRulesScoringModule *pScoringModule = m_pGameRules->GetScoringModule();
					Server_LivingSoldiersScore(EGRST_Predator_ThreeMarinesRemaining, pScoringModule->GetPlayerPointsByType(EGRST_Predator_ThreeMarinesRemaining));
				}
				break;
			case 2:
				{
					IGameRulesScoringModule *pScoringModule = m_pGameRules->GetScoringModule();
					Server_LivingSoldiersScore(EGRST_Predator_TwoMarinesRemaining, pScoringModule->GetPlayerPointsByType(EGRST_Predator_TwoMarinesRemaining));
				}
				break;
			case 1:
				{
					IGameRulesScoringModule *pScoringModule = m_pGameRules->GetScoringModule();
					pScoringModule->OnPlayerScoringEvent(lastMarineId, EGRST_Predator_LastMarineStanding);
				}
				break;
			case 0:
				{
					Common_SetFlag(ePF_AllSoldiersDead);
				}
				break;
		}
	}

	Server_UpdateRemainingSoldierCount(-1);
}

//-------------------------------------------------------------------------
int CGameRulesObjective_Predator::Common_GetMaxStartPredators(int numPlayers) const
{
	return max(1, min(m_numStartPredators, numPlayers/2));;
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Common_OnPlayerKilledOrLeft( EntityId targetId, EntityId shooterId )
{
	if (gEnv->bServer)
	{
		const int targetTeamId = m_pGameRules->GetTeam(targetId);
		if (targetTeamId == TEAM_SOLDIER)
		{
			Server_OnSoldierKilledOrLeft(targetId);
		}

		if (shooterId && (shooterId != targetId))
		{
			int shooterTeamId = m_pGameRules->GetTeam(shooterId);
			if (shooterTeamId && targetTeamId && (shooterTeamId != targetTeamId))
			{
				EGRST scoreType = EGRST_Predator_KillAsSoldier;
				if (shooterTeamId != TEAM_SOLDIER)
				{
					bool finalKill = Server_WasFinalKill(targetId);
					scoreType = !finalKill ? EGRST_Predator_KillAsPredator : EGRST_Predator_FinalKillAsPredator;
				}

				IGameRulesScoringModule *pScoringModule = m_pGameRules->GetScoringModule();
				SGameRulesScoreInfo scoreInfo(scoreType, pScoringModule->GetPlayerPointsByType(scoreType));
				scoreInfo.AttachVictim(targetId);
				pScoringModule->OnPlayerScoringEventWithInfo(shooterId, &scoreInfo);
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnClientDisconnect( int channelId, EntityId playerId )
{


	TPredatorData::iterator predData = m_PredatorData.find(channelId);
	bool bWasPredator = predData->second.bIsPredator;
	
	m_PredatorData.erase(channelId);

	if(!EndGameIfOnlyOnePlayerRemains(channelId))
	{
		// If there are only marines left, and no predators, marines win
		if(bWasPredator)
		{
			Server_OnPredatorLeft();
		}
	}
}


//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Server_OnPredatorLeft()
{
	//Iterating over a map is bad, but we currently have no count of the number of predators, and adding it at this
	//	immediately pre-GamesCom stage is asking for trouble, so we'll brute force it for the moment.
	//TODO: Add a nice m_numPredators if we get time.
	for(TPredatorData::const_iterator iter = m_PredatorData.begin(), end = m_PredatorData.end(); iter != end; ++iter)
	{
		if(iter->second.bIsPredator)
		{
			//There's still at least one predator. We're good.
			return;
		}
	}

	//We're all run out 'o predators.
	Server_HandleNoPredatorsLeft();
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Server_HandleNoPredatorsLeft()
{
	if(IGameRulesVictoryConditionsModule * pVictoryConditions = m_pGameRules->GetVictoryConditionsModule())
	{
		pVictoryConditions->SvForceEndGame(TEAM_SOLDIER, EGOR_OpponentsDisconnected);
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnEntityKilled( const HitInfo &hitInfo )
{
#if USE_PC_PREMATCH
	if(m_pGameRules->GetPrematchState() != CGameRules::ePS_Match)
	{
		return;
	}
#endif

	Common_OnPlayerKilledOrLeft(hitInfo.targetId, hitInfo.shooterId);

	if(m_pGameRules->HasGameActuallyStarted() && (m_pGameRules->GetTeam(hitInfo.targetId) == TEAM_SOLDIER) )
	{
		bool isClient = hitInfo.targetId  == g_pGame->GetIGameFramework()->GetClientActorId();
		if(gEnv->bServer || isClient)
		{
			bool wasSuicide = hitInfo.shooterId == 0 || hitInfo.shooterId == hitInfo.targetId;
			if(gEnv->bServer )
			{
				Server_PrepareChangeTeamPredator(hitInfo.targetId, wasSuicide);
			}
			if(isClient && wasSuicide)
			{
				Common_SetFlag(ePF_ClientSuicide);
				SHUDEvent event(eHUDEvent_SuicidePenalty);
				CHUDEventDispatcher::CallEvent(event);
			}
		}

		if( isClient )
		{
			 m_ClientHasDiedAsMarine = true;
		}
	
		if (m_pGameRules->GetTeam(hitInfo.shooterId) == TEAM_PREDATOR)
		{
			if (m_clientTeam == TEAM_SOLDIER)
			{
				SHUDEvent eventTempAddToRadar(eHUDEvent_TemporarilyTrackEntity);
				eventTempAddToRadar.AddData( static_cast<int>(hitInfo.shooterId) );
				eventTempAddToRadar.AddData( 1.5f );			// Time
				eventTempAddToRadar.AddData( true );			// Only show on radar
				eventTempAddToRadar.AddData( true );			// Force to be no updating pos
				CHUDEventDispatcher::CallEvent(eventTempAddToRadar);
			}

			if( hitInfo.shooterId != g_pGame->GetIGameFramework()->GetClientActorId() )
			{
				m_AMarineHasBeenKilledByRemotePlayerThisRound = true;
			}
		}

		if (gEnv->bServer)
		{
			IGameRulesPlayerStatsModule *pStatsModule = m_pGameRules->GetPlayerStatsModule();
			pStatsModule->RecordSurvivalTime(hitInfo.targetId, m_pGameRules->GetCurrentGameTime());
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Server_PrepareChangeTeamPredator( EntityId playerId, bool wasSuicide)	
{
	m_deadSoldierIds.push_back(playerId);

	// If any marines remain alive, the game is not over
	const bool wasFinalMarine = Server_WasFinalKill(playerId);

	IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId);
	if(pActor)
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
		uint16 channelId = pActor->GetChannelId();
		if(wasSuicide)
		{
			m_PredatorData[channelId].fReviveTimeMult = m_fSoldierSuicideRespawnTimeMult;
		}

		g_pGame->GetEquipmentLoadout()->SvRemoveClientEquipmentLoadout(channelId);

	
		CStatsRecordingMgr::TryTrackEvent( pActor, eGSE_Hunter_ChangeToPredator );

	}

	if (!wasFinalMarine)
	{
		if(pActor && !pActor->IsClient())
		{
			//Tell clients
			CGameRules::SModuleRMIEntityParams params;
			params.m_listenerIndex = m_moduleRMIIndex;
			params.m_entityId = playerId;
			params.m_data = SINGLE_ENTITY_RMI_PREPARE_CHANGE_TEAM_PREDATOR;
			m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), params, eRMI_ToRemoteClients, playerId);
		}
		else
		{
			Common_PrepareChangeTeamPredator(playerId, false);
		}
	}
	else
	{
		CryLog("PREDATORDEBUG - Final marine has been killed!");
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Server_PredatorReadyToSpawn( EntityId playerId)
{
	CryLog("PREDATORDEBUG - Received client ready to respawn %i", playerId);

	// Players can't switch team if the round is nearly over
	if (m_pGameRules->IsTimeLimited() && (m_pGameRules->GetRemainingGameTimeNotZeroCapped() < GetMinimalReviveTime()))
	{
		CryLog("PREDATORDEBUG - But round almost over. Denied.");
		return;
	}

	bool found = false;
	for(unsigned int i = 0; i < m_deadSoldierIds.size(); ++i)
	{
		if(m_deadSoldierIds[i] == playerId)
		{
			CryLog("PREDATORDEBUG - Found dead player");
			m_deadSoldierIds.removeAt(i);
			found = true;
			break;
		}
	}

	if(!found)
	{
		CryLog("PREDATORDEBUG - Player is apparently not dead. Client and server are out of synch. Re-spawning is going to go horribly wrong.");
	}

	IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId);
	if(pActor && pActor->IsPlayer())
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);

		//Change team
		m_pGameRules->SetTeam(TEAM_PREDATOR, playerId);
		uint16 channelId = pActor->GetChannelId();
		m_PredatorData[channelId].bIsPredator = true;

		if(!pActor->IsClient())
		{
			//TODO: FIX BANDWIDTH USAGE
			CGameRules::SModuleRMIEntityParams params;
			params.m_listenerIndex = m_moduleRMIIndex;
			params.m_entityId = playerId;
			params.m_data = SINGLE_ENTITY_RMI_SET_PREDATOR_LOADOUT;
			m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), params, eRMI_ToRemoteClients, playerId);
		}
		else
		{
			Client_SetPredatorLoadout();
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Common_PrepareChangeTeamPredator( EntityId playerId, bool wasFinalMarine )
{
	CPlayer *pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId));
	if (pPlayer && pPlayer->IsClient())
	{
		Common_SetFlag(ePF_ClientSwitchingToPredator|ePF_ClientStartedAsPredator);
	}

	const SHUDEvent hudevent_rescanActors(eHUDEvent_RescanActors);
	CHUDEventDispatcher::CallEvent(hudevent_rescanActors);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Client_SetPredatorLoadout()
{
	m_pGameRules->FreezeInput(false);
	if( CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout() )
	{
		pEquipmentLoadout->SetPackageGroup(LOADOUT_PACKAGE_GROUP_PREDATOR);

		pEquipmentLoadout->ForceSelectLastSelectedLoadout();
	}

	const char *pMessage = CHUDUtils::LocalizeString("@ui_msg_hr_status_kill_marines");
	SHUDEventWrapper::OnGameStatusUpdate(eGBNFLP_Neutral, pMessage);

	SHUDEvent eventTeamRadar(eHUDEvent_OnTeamRadarChanged);
	eventTeamRadar.AddData(SHUDEventData(true));
	CHUDEventDispatcher::CallEvent(eventTeamRadar);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Server_OnStartNewRound()
{
	HUNTER_DEBUG_LOG("Server_OnStartNewRound()");

	//Choose predators who have not yet started as predators (if possible)
	TPredatorData::iterator iter = m_PredatorData.begin();
	TPredatorData::iterator end = m_PredatorData.end();

	int numPredatorsToChoose = Common_GetMaxStartPredators(m_PredatorData.size());

	while(iter != end)
	{
		if(IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorByChannelId(iter->first))
		{
			SPredatorData& predData = iter->second;
			EntityId actorId = pActor->GetEntityId();

			if(!predData.bHasStartedAsPredator && numPredatorsToChoose)
			{
				if(!predData.bIsPredator)
				{
					m_pGameRules->SetTeam(TEAM_PREDATOR, actorId);
					predData.bIsPredator = true;
					HUNTER_DEBUG_LOG("		- SETTING actorID[%d][%s] TEAM TO PREDATOR - 2", actorId, pActor->GetEntity()->GetName());
				}
				else
				{
					HUNTER_DEBUG_LOG("		- NO CHANGE - actorID[%d][%s] ALREADY TEAM PREDATOR - 2", actorId, pActor->GetEntity()->GetName());
				}

				predData.bHasStartedAsPredator = true;
				--numPredatorsToChoose;
			}
			else if(predData.bIsPredator)
			{
				m_pGameRules->SetTeam(TEAM_SOLDIER, actorId);
				predData.bIsPredator = false;
				HUNTER_DEBUG_LOG("		- SETTING actorID[%d][%s] TEAM TO MARINE - 1", actorId, pActor->GetEntity()->GetName());
			}
			predData.fReviveTimeMult = 0.f;
		}

		++iter;
	}

	if(numPredatorsToChoose) //Everyone has been predator at least once. Reset and start again
	{
		HUNTER_DEBUG_LOG("Server_OnStartNewRound() - All players have spawned as predator. Re-selecting");
		iter = m_PredatorData.begin();
		while(iter != end)
		{
			if(numPredatorsToChoose)
			{
				if(IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorByChannelId(iter->first))
				{
					SPredatorData& predData = iter->second;
					if(!predData.bIsPredator)
					{
						m_pGameRules->SetTeam(TEAM_PREDATOR, pActor->GetEntityId());
						predData.bIsPredator = true;
						HUNTER_DEBUG_LOG("		- SETTING actorID[%d][%s] TEAM TO PREDATOR - 2", pActor->GetEntityId(), pActor->GetEntity()->GetName());
					}
					else
					{
						HUNTER_DEBUG_LOG("		- NO CHANGE - actorID[%d][%s] ALREADY TEAM PREDATOR - 2", pActor->GetEntityId(), pActor->GetEntity()->GetName());
					}

					predData.bHasStartedAsPredator = true;
					--numPredatorsToChoose;
				}
			}
			else
			{
				iter->second.bHasStartedAsPredator = false;
			}

			++iter;
		}
	}

	Server_UpdateRemainingSoldierCount(-m_numRemainingSoldiers);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnRoundAboutToStart()
{
	Common_ClearFlag(	ePF_ClientSwitchingToPredator |
										ePF_ClientSuicide |
										ePF_ClientStartedAsPredator |
										ePF_Sent90SecondsRemainingEvent |
										ePF_Sent60SecondsRemainingEvent |
										ePF_Sent30SecondsRemainingEvent |
										ePF_Sent10SecondsRemainingEvent |
										ePF_SentHunterSpawn |
										ePF_HasSpawned );

	HUNTER_DEBUG_LOG("On round about to start being called");

	CEquipmentLoadout * pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
	if (pEquipmentLoadout)
	{
		pEquipmentLoadout->InvalidateLastSentLoadout();
	}

	if(gEnv->bServer)
	{
		m_deadSoldierIds.clear();
	
		if (pEquipmentLoadout)
		{
			TPredatorData::const_iterator it = m_PredatorData.begin();
			TPredatorData::const_iterator endit = m_PredatorData.end();
			while (it != endit)
			{
				pEquipmentLoadout->SvRemoveClientEquipmentLoadout(it->first);

				++it;
			}
		}

		Server_OnStartNewRound();
	}
}

//-------------------------------------------------------------------------
bool CGameRulesObjective_Predator::RequestReviveOnLoadoutChange()
{
	return !Common_IsFlagSet(ePF_ClientSwitchingToPredator);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Server_GetMarinesStatus( EntityId excludeId, EntityId &outLastMarineId, int &outCount ) const
{
	outCount = 0;
	outLastMarineId = 0;

	CGameRules::TPlayers soldiers;
	m_pGameRules->GetTeamPlayers(TEAM_SOLDIER, soldiers);
	CGameRules::TPlayers::const_iterator 					liveSoldiersEndIter		= soldiers.end(); 
	CGameRulesObjective_Predator::TIntArray::const_iterator	deadSoldiersBeginIter	= m_deadSoldierIds.begin(); 
	CGameRulesObjective_Predator::TIntArray::const_iterator deadSoldiersEndIter		= m_deadSoldierIds.end();

	for (CGameRules::TPlayers::const_iterator it = soldiers.begin(); it != liveSoldiersEndIter; ++ it)
	{
		EntityId playerId = *it;
		if ((playerId != excludeId) && (std::find(deadSoldiersBeginIter, deadSoldiersEndIter, playerId) == deadSoldiersEndIter))
		{
			++ outCount;
			outLastMarineId = playerId;
		}
	}
}

//-------------------------------------------------------------------------
bool CGameRulesObjective_Predator::Server_WasFinalKill( EntityId playerId ) const
{
	int remainingMarines = 0;
	EntityId temp = 0;

	Server_GetMarinesStatus(playerId, temp, remainingMarines);

	return (remainingMarines == 0);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::UpdateInitialAmmoCounts( const IEntityClass *pWeaponClass, TAmmoVector &ammo, TAmmoVector &bonusAmmo )
{
	int numOverrides = m_weaponAmmoOverrides.size();
	for (int i = 0; i < numOverrides; ++ i)
	{
		SWeaponAmmoOverride &weaponOverride = m_weaponAmmoOverrides[i];
		if (weaponOverride.m_pWeaponClass == pWeaponClass)
		{
			int numAmmoTypes = weaponOverride.m_bonusAmmo.size();
			for (int j = 0; j < numAmmoTypes; ++ j)
			{
				SAmmoOverride &ammoOverride = weaponOverride.m_bonusAmmo[j];

				const SWeaponAmmo *pAmmo = SWeaponAmmoUtils::FindAmmoConst(ammo, ammoOverride.m_pAmmoClass);
				if (pAmmo)
				{
					if(pAmmo->count)
					{
						SWeaponAmmoUtils::SetAmmo(bonusAmmo, ammoOverride.m_pAmmoClass, ammoOverride.m_clips * pAmmo->count);
					}
					else //Grenades/explosives have 0 clip size
					{
						SWeaponAmmoUtils::SetAmmo(bonusAmmo, ammoOverride.m_pAmmoClass, ammoOverride.m_clips);
					}
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Common_SetAmmoCacheGroupActive( uint32 groupId, bool bActive )
{
	int iActive = (bActive ? 1 : 0);
	int numCacheGroups = m_ammoCacheGroups.size();
	for (int i = 0; i < numCacheGroups; ++ i)
	{
		SAmmoCacheGroup &group = m_ammoCacheGroups[i];
		if (group.m_groupId == groupId)
		{
			const bool bIsSoldier = m_pGameRules->GetTeam(gEnv->pGameFramework->GetClientActorId()) == TEAM_SOLDIER;
			int numEntities = group.m_entityIds.size();
			for (int j = 0; j < numEntities; ++ j)
			{
				IEntity *pEntity = gEnv->pEntitySystem->GetEntity(group.m_entityIds[j]);
				if (pEntity)
				{
					EntityScripts::CallScriptFunction(pEntity, pEntity->GetScriptTable(), "Enabled", iActive);

					if(bActive && bIsSoldier)
					{
						SHUDEventWrapper::OnNewObjective(group.m_entityIds[j], EGRMO_Ammo_Cache);
					}
					else
					{
						SHUDEventWrapper::OnRemoveObjective(group.m_entityIds[j]);
					}
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::Client_OnSoldiersRemainingCountChanged(int previousCount)
{
	if(m_numRemainingSoldiers == 0)
	{
		//When the last marine is killed they are not forced to switch teams and they do not respawn. As
		//	the team switch is what drives the update of the 'player heads' display in predator, this means
		//	that the last marine killed never shows up there as a predator. m_numRemainingSoldiers does
		//	get synced, however, so we'll use that to inform the HUD that all the marines are dead. It'll
		//	handle the re-setting itself via other means.
		SHUDEvent event(eHUDEvent_PredatorAllMarinesDead);
		CHUDEventDispatcher::CallEvent(event);
	}

	IActor *pActor = g_pGame->GetIGameFramework()->GetClientActor();
	if (pActor)
	{
		EntityId clientId = pActor->GetEntityId();

		bool bSoldier = m_pGameRules->GetTeam(clientId) == TEAM_SOLDIER;

		if ((m_numRemainingSoldiers == 1) && (previousCount > 1))
		{
			if (bSoldier)
			{
				m_signalPlayer.SetSignal(m_lastManSignalIdMarine);
				CryLog("Play last marine signal (marine)");
			}
			else
			{
				m_signalPlayer.SetSignal(m_lastManSignalIdHunter);
				CryLog("Play last marine signal (hunter)");
			}
		}

		if (bSoldier && pActor->IsDead())
		{
			// Don't announce anything if we're currently switching team
			return;
		}

		// Don't play anything in the first 5 seconds of a round - need the round start messages to play
		if (m_pGameRules->GetRoundsModule()->IsInProgress() && (m_pGameRules->GetCurrentGameTime() > 5.f) && (previousCount > m_numRemainingSoldiers))
		{
			const char *pAnnouncement = NULL;

			switch (m_numRemainingSoldiers)
			{
			case 5:
				pAnnouncement = bSoldier ? "Marine_FiveLeft" : "Hunter_FiveLeft";
				break;
			case 4:
				pAnnouncement = bSoldier ? "Marine_FourLeft" : "Hunter_FourLeft";
				break;
			case 3:
				pAnnouncement = bSoldier ? "Marine_ThreeLeft" : "Hunter_ThreeLeft";
				break;
			case 2:
				pAnnouncement = bSoldier ? "Marine_TwoLeft" : "Hunter_TwoLeft";
				break;
			case 1:
				pAnnouncement = bSoldier ? "Marine_OneLeft" : "Hunter_OneLeft";
				break;
			}

			if (pAnnouncement)
			{
				CAnnouncer::GetInstance()->Announce(pAnnouncement, CAnnouncer::eAC_inGame);
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnNewGroupIdSelected( uint32 id )
{
	
	if(m_ammoCacheGroupId != INVALID_AMMOGROUP_ID)
		Common_SetAmmoCacheGroupActive(m_ammoCacheGroupId, false);

	m_ammoCacheGroupId = id;
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnRoundStart()
{
	Common_ClearFlag(ePF_GracePeriodEventSent);

	m_AMarineHasBeenKilledByRemotePlayerThisRound = false;
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnHostMigration( bool becomeServer )
{
	if (becomeServer)
	{
		CGameLobby *pGameLobby = g_pGame->GetGameLobby();
		if (pGameLobby)
		{
			const SSessionNames &sessionNames = pGameLobby->GetSessionNames();
			int numPlayers = sessionNames.Size();
			IActorSystem *pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
			for (int i = 0; i < numPlayers; ++ i)
			{
				IActor *pActor = pActorSystem->GetActorByChannelId(sessionNames.m_sessionNames[i].m_conId.m_uid);
				if (pActor)
				{
					m_PredatorData[pActor->GetChannelId()].bIsPredator = (m_pGameRules->GetTeam(pActor->GetEntityId()) == TEAM_PREDATOR);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnPrematchStateEnded()
{
	if (gEnv->bServer)
	{
		CGameRules::TPlayers players;
		m_pGameRules->GetTeamPlayers(TEAM_SOLDIER, players);

		IActorSystem *pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
		int numPlayers = players.size();
		int numLivingPlayers = 0;
		for (int i = 0; i < numPlayers; ++ i)
		{
			EntityId playerId = players[i];
			IActor *pActor = pActorSystem->GetActor(playerId);
			if (pActor && !pActor->IsDead())
			{
				++ numLivingPlayers;
			}
		}

		Server_UpdateRemainingSoldierCount(numLivingPlayers - m_numRemainingSoldiers);
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnRoundEnd()
{
	if( IGameRulesRoundsModule* pRoundsModule = m_pGameRules->GetRoundsModule() )
	{
		if( pRoundsModule->GetRoundsRemaining() == 0 )
		{
			//game over
			if( m_ClientHasDiedAsMarine == false && Common_IsFlagSet(ePF_HasStartedAsMarine) )
			{
				//never died as a marine
				CPersistantStats::GetInstance()->IncrementClientStats( EIPS_HunterHideAndSeek );
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_Predator::OnGameReset()
{
	CryLog("CGameRulesObjective_Predator::OnGameReset()");
	Common_SetAmmoCacheGroupActive(m_ammoCacheGroupId, true);
}

bool CGameRulesObjective_Predator::CanPlayIntroSequence() const
{
	return true; 
}

bool CGameRulesObjective_Predator::IsNextClientBowFirstAsHunter() const
{
	if( Common_IsFlagSet(ePF_NextClientBowFirstAsHunter) ||  // This flag Set explicitly if this player has just revived on hunter team for first spawn of this round (set in the entity Revived callback).
	   !Common_IsFlagSet(ePF_HasSpawned) )									 // Due to ordering issues (usually on clients) we sometimes find the item being selected prior to the entity revived callback - we can cope with this.
	{
		return true;
	}
	return false; 
}

bool CGameRulesObjective_Predator::EndGameIfOnlyOnePlayerRemains( int channelId )
{
	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	if (pGameLobby)
	{
		SCryMatchMakingConnectionUID connUID = pGameLobby->GetConnectionUIDFromChannelID(channelId);

		// Don't rely on session names always being up to date at this point (d/c player may not have been removed yet)
		uint8 livePlayerCount = 0;
		int survivorTeamId		= 1;
		const SSessionNames &sessionNames = g_pGame->GetGameLobby()->GetSessionNames();
		const CryFixedArray<SSessionNames::SSessionName, MAX_SESSION_NAMES>& sessionNameArray = sessionNames.m_sessionNames;
		const uint8 sessionNameArraySize = sessionNameArray.size();
		for(int i = 0; i < sessionNameArraySize; ++i)
		{
			if(sessionNameArray[i].m_conId != connUID)
			{
				++livePlayerCount;
				if(livePlayerCount > 1)
				{
					return false;
				}

				survivorTeamId = sessionNameArray[i].m_teamId;
			}
		}

		// You win by default. Congratulations on your achievement! 
		IGameRulesRoundsModule *pRoundsModule									 = m_pGameRules->GetRoundsModule();
		IGameRulesVictoryConditionsModule * pVictoryConditions = m_pGameRules->GetVictoryConditionsModule();
			
		if(pRoundsModule && pVictoryConditions)
		{
			pRoundsModule->SetTreatCurrentRoundAsFinalRound(true);
			pVictoryConditions->SvForceEndGame(survivorTeamId, EGOR_OpponentsDisconnected);
			return true;
		}

	}

	return false;
}

// Play nice with uber file compiling
#undef SINGLE_ENTITY_RMI_PREPARE_CHANGE_TEAM_PREDATOR
#undef SINGLE_ENTITY_RMI_CLIENT_READY_TO_RESPAWN
#undef SINGLE_ENTITY_RMI_SET_PREDATOR_LOADOUT
#undef LOADOUT_PACKAGE_GROUP_PREDATOR
#undef LOADOUT_PACKAGE_GROUP_SOLDIER
