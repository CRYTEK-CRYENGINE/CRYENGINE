// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Objective handler for extraction gamemode
	-------------------------------------------------------------------------
	History:
	- 10/05/2010  : Created by Jim Bamford

*************************************************************************/


#include "StdAfx.h"
#include "GameRulesObjective_Extraction.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "Utility/CryWatch.h"
#include "Item.h"
#include "Player.h"
#include "Utility/CryDebugLog.h"
#include "UI/UIManager.h"
#include "UI/UICVars.h"
#include "UI/HUD/HUDUtils.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "Utility/DesignerWarning.h"
#include "IGameRulesRoundsModule.h"
#include "IGameRulesScoringModule.h"
#include "IGameRulesVictoryConditionsModule.h"
#include "GameActions.h"
#include "PersistantStats.h"
#include "Network/Lobby/GameLobby.h"
#include "MultiplayerEntities/CarryEntity.h"
#include "PlayerPlugin_Interaction.h"
#include "Audio/Announcer.h"
#include "Weapon.h"
#include "PlayerVisTable.h"

//#include "CryFixedString.h"

#define DbgLog(...) CRY_DEBUG_LOG(GAMEMODE_EXTRACTION, __VA_ARGS__)
#define DbgLogAlways(...) CRY_DEBUG_LOG_ALWAYS(GAMEMODE_EXTRACTION, __VA_ARGS__)

static AUTOENUM_BUILDNAMEARRAY(s_extractionSuitModeListStr, EExtractionSuitModeList);

const int k_defaultExtractionDebugging=0;	

#if NUM_ASPECTS > 8
	#define EXTRACTION_OBJECTIVE_STATE_ASPECT		eEA_GameServerA
#else
	#define EXTRACTION_OBJECTIVE_STATE_ASPECT		eEA_GameServerStatic
#endif

// TODO - use this to set gamestate messages in score element
//const char *localisedMessage = CHUDUtils::LocalizeString(m_gameStateFriendlyString.c_str());
//SHUDEventWrapper::OnGameStatusUpdate(eGBNFLP_Good, localisedMessage);

void CGameRulesObjective_Extraction::SPickup::SetState(EPickupState inState)
{
	DbgLog("[state] SPickup::SetState() state=%d -> %d; suitMode=%s; this=%p", m_state, inState, s_extractionSuitModeListStr[m_suitModeEffected], this);
	m_state = inState;

	SetAudioState(inState);
}


void CGameRulesObjective_Extraction::SPickup::SetAudioState(EPickupState inState)
{
	switch(inState)
	{
	case ePickupState_AtBase:
		{
			/*if(!m_signalPlayer.IsPlaying())
			{
				m_signalPlayer.Play(m_spawnedPickupEntityId, "carried", 0.0f);
			}*/
		}
		break;
	case ePickupState_Carried:
		{
			/*if(!m_signalPlayer.IsPlaying())
			{
				m_signalPlayer.Play(m_spawnedPickupEntityId);
			}*/
			m_signalPlayer.SetParam(m_spawnedPickupEntityId, "carried", 1.0f);
		}
		break;
	case ePickupState_Dropped:
		{
			m_signalPlayer.SetParam(m_spawnedPickupEntityId, "carried", 0.0f);
		}
		break;
	case ePickupState_Extracted:
		{
			//m_signalPlayer.Stop(m_spawnedPickupEntityId);
		}
		break;
	}
}

CGameRulesObjective_Extraction::CGameRulesObjective_Extraction()
{
	m_spawnAtEntityClass = nullptr;
	m_extractAtEntityClass = nullptr;
	m_fallbackGunEntityClass = nullptr;

	m_moduleRMIIndex = 0;
	m_droppedRemoveTime = 0;
	m_defensiveKillRadiusSqr = 0.0f;
	m_defendingDroppedRadiusSqr = 0.0f;
	m_defendingDroppedTimeToScore = 0.0f;
	m_pickUpVisCheckHeight = -1.0f;

	m_physicsType=ePhysType_None;

	m_extractionDebuggingCVar=REGISTER_INT("g_extraction_debugging", k_defaultExtractionDebugging, 0, "enable to turn on extraction gamemode debugging");

	m_haveStartedGame=false;

	m_timeLimit=0.0f;
	m_previousTimeTaken = 0.0f;

	m_armourFriendlyIconDropped = EGRMO_Unknown;
	m_armourHostileIconDropped  = EGRMO_Unknown;
	m_stealthFriendlyIconDropped = EGRMO_Unknown;
	m_stealthHostileIconDropped  = EGRMO_Unknown;
	m_armourFriendlyIconCarried = EGRMO_Unknown;
	m_armourHostileIconCarried  = EGRMO_Unknown;
	m_stealthFriendlyIconCarried = EGRMO_Unknown;
	m_stealthHostileIconCarried  = EGRMO_Unknown;
	m_armourFriendlyIconAtBase  = EGRMO_Unknown;
	m_armourHostileIconAtBase   = EGRMO_Unknown;
	m_stealthFriendlyIconAtBase  = EGRMO_Unknown;
	m_stealthHostileIconAtBase   = EGRMO_Unknown;
	m_friendlyBaseIcon		= EGRMO_Unknown;
	m_hostileBaseIcon			= EGRMO_Unknown;

	m_numberToExtract = 0;

	m_useButtonHeld = false;
	m_attemptPickup = false;

	m_spawnOffset.zero();

	m_localPlayerEnergyBoostActive=false;
	m_hasFirstFrameInited=false;
}

CGameRulesObjective_Extraction::~CGameRulesObjective_Extraction()
{
	IConsole		*ic=gEnv->pConsole;
	if (ic)
	{
		ic->UnregisterVariable(m_extractionDebuggingCVar->GetName());
	}
	
	if (gEnv->pEntitySystem)
	{
		for (size_t i = 0; i < m_pickups.size(); i++)
		{
			SPickup *pickup=&m_pickups[i];
			if (pickup->m_spawnedPickupEntityId)
			{
				gEnv->pEntitySystem->RemoveEntityEventListener(pickup->m_spawnedPickupEntityId, ENTITY_EVENT_DONE, this);
			}
		}
	}
	
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->UnRegisterModuleRMIListener(m_moduleRMIIndex);
		pGameRules->UnRegisterClientConnectionListener(this);
		pGameRules->UnRegisterKillListener(this);
		pGameRules->UnRegisterActorActionListener(this);
		pGameRules->UnRegisterTeamChangedListener(this);
		pGameRules->UnRegisterRoundsListener(this);
	}
}

void CGameRulesObjective_Extraction::InitPickups(XmlNodeRef xml)
{
	DbgLog("CGameRulesObjective_Extraction::InitPickups()");

	int numChildren = xml->getChildCount();
	for (int i = 0; i < numChildren; ++ i)
	{
		XmlNodeRef xmlChild = xml->getChild(i);
		if (!stricmp(xmlChild->getTag(), "Pickup"))
		{
			DesignerWarning(m_pickups.size() < m_pickups.max_size(), "Extraction - Invalid XML - too many pickups defined within pickups. Only %d allowed", m_pickups.max_size());

			if (m_pickups.size() < m_pickups.max_size())
			{
				SPickup pickup;
					
				const char *pEntityName = 0;
				if (xmlChild->getAttr("entity", &pEntityName))
				{
					DbgLog("CGameRulesObjectiveHelper_Spawn::Init, 'spawn at' entity type=%s", pEntityName);
					pickup.m_entityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pEntityName);
				}
				DesignerWarning(pickup.m_entityClass, "Extraction - Invalid XML - unable to find entity class for pickup's entity=%s", pEntityName);

				const char *pSuitModeName=NULL;
				if (xmlChild->getAttr("suitMode", &pSuitModeName))
				{
					CryFixedStringT<128> suitModeStr;
					suitModeStr.Format("eExtractionSuitMode_%s", pSuitModeName);
					
					DbgLog("trying to find suitMode enum from string=%s; stitched onto enum prefix giving %s", pSuitModeName, suitModeStr.c_str());

					int iVal;
					if (AutoEnum_GetEnumValFromString(pSuitModeName /*suitModeStr.c_str()*/, s_extractionSuitModeListStr, eExtractionSuitMode_Num, &iVal))
					{
						DbgLog("successfully found enum value of %d from string %s", iVal, suitModeStr.c_str());
						pickup.m_suitModeEffected = (EExtractionSuitMode)iVal;
					}
					else
					{
						DesignerWarning(0, "Extraction - Invalid XML - failed to find an enum for suitMode=%s",pSuitModeName);
					}
				}

				const char *pSignalName = 0;
				if (xmlChild->getAttr("sound", &pSignalName))
				{
					pickup.m_signalPlayer.SetSignal(pSignalName);
				}
				
				m_pickups.push_back(pickup);
			}
		}
		else
		{
			DesignerWarning(0, "Extraction - Invalid XML - unhandled xml node of tag %s found within pickups", xmlChild->getTag());
		}
	}
}

void CGameRulesObjective_Extraction::Init(XmlNodeRef xml)
{
	DbgLog("CGameRulesObjective_Extraction::Init()");

	const char *pEntityName = 0;
	if (xml->getAttr("spawnAt", &pEntityName))
	{
		DbgLog("CGameRulesObjective_Extraction::Init, 'spawn at' entity type=%s", pEntityName);
		m_spawnAtEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pEntityName);
	}
	DesignerWarning(m_spawnAtEntityClass, "Extraction - Invalid XML - unable to find entity class for spawnAt=%s", pEntityName);

	pEntityName = 0;
	if (xml->getAttr("extractAt", &pEntityName))
	{
		DbgLog("CGameRulesObjective_Extraction::Init, 'extract at' entity type=%s", pEntityName);
		m_extractAtEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pEntityName);
	}
	DesignerWarning(m_spawnAtEntityClass, "Extraction - Invalid XML - unable to find entity class for extractAt=%s", pEntityName);

	if (xml->getAttr("fallbackGun", &pEntityName))
	{
		DbgLog("CGameRulesObjective_Extraction::Init() 'fallbackGun' entity className=%s", pEntityName);
		m_fallbackGunEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pEntityName);
	}
	DesignerWarning(m_fallbackGunEntityClass, "Extraction - Invalid XML - unable to find entity class for m_fallbackGunEntityClass=%s", pEntityName);

	xml->getAttr("pickUpVisCheckHeight", m_pickUpVisCheckHeight);

	if (xml->getAttr("droppedTimerLength", m_droppedRemoveTime))
	{
		m_droppedRemoveTime*=1000;
		DbgLog("CGameRulesObjective_Extraction::Init() droppedTimerLength=%d", m_droppedRemoveTime);
	}
	else
	{
		DesignerWarning(0, "Extraction - Invalid XML - unable to find a droppedTimeLength attribute");
	}

	if (xml->getAttr("defensiveKillRadius", m_defensiveKillRadiusSqr))
	{
		m_defensiveKillRadiusSqr = m_defensiveKillRadiusSqr * m_defensiveKillRadiusSqr;
		DbgLog("CGameRulesObjective_Extraction::Init() m_defensiveKillRadiusSqr=%f", m_defensiveKillRadiusSqr);
	}
	else
	{
		DesignerWarning(0, "Extraction - Invalid XML - unable to find a defensiveKillRadius attribute");
	}

	if (xml->getAttr("defendingDroppedRadius", m_defendingDroppedRadiusSqr))
	{
		m_defendingDroppedRadiusSqr = m_defendingDroppedRadiusSqr * m_defendingDroppedRadiusSqr;
		DbgLog("CGameRulesObjective_Extraction::Init() m_defendingDroppedRadiusSqr=%f", m_defendingDroppedRadiusSqr);
	}
	else
	{
		DesignerWarning(0, "Extraction - Invalid XML - unable to find a defendingDroppedRadius attribute");
	}

	if (xml->getAttr("defendingDroppedTimeToScore", m_defendingDroppedTimeToScore))
	{
		DbgLog("CGameRulesObjective_Extraction::Init() m_defendingDroppedTimeToScore=%f", m_defendingDroppedTimeToScore);
	}
	else
	{
		DesignerWarning(0, "Extraction - Invalid XML - unable to find a defendingDroppedRadius attribute");
	}
	
	m_timeLimit = g_pGameCVars->g_timelimit;

	int numChildren = xml->getChildCount();
	for (int i = 0; i < numChildren; ++ i)
	{
		XmlNodeRef xmlChild = xml->getChild(i);

		if (!stricmp(xmlChild->getTag(), "Pickups"))
		{
			InitPickups(xmlChild);
		}
		else if (!stricmp(xmlChild->getTag(), "DroppedIcons"))
		{
			m_stealthFriendlyIconDropped = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyStealth"));
			m_stealthHostileIconDropped  = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileStealth"));
			m_armourFriendlyIconDropped = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyArmour"));
			m_armourHostileIconDropped  = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileArmour"));
		}
		else if (!stricmp(xmlChild->getTag(), "CarriedIcons"))
		{
			m_stealthFriendlyIconCarried = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyStealth"));
			m_stealthHostileIconCarried  = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileStealth"));
			m_armourFriendlyIconCarried = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyArmour"));
			m_armourHostileIconCarried  = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileArmour"));
		}
		else if (!stricmp(xmlChild->getTag(), "AtBaseIcons"))
		{
			m_stealthFriendlyIconAtBase = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyStealth"));
			m_stealthHostileIconAtBase  = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileStealth"));
			m_armourFriendlyIconAtBase = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyArmour"));
			m_armourHostileIconAtBase  = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileArmour"));
		}
		else if (!stricmp(xmlChild->getTag(), "BaseIcons"))
		{
			m_friendlyBaseIcon = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendly"));
			m_hostileBaseIcon = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostile"));

			//xmlChild->getAttr("priority", m_iconPriority);
		}
		else if (!stricmp(xmlChild->getTag(), "Offset"))
		{
			xmlChild->getAttr("x", m_spawnOffset.x);
			xmlChild->getAttr("y", m_spawnOffset.y);
			xmlChild->getAttr("z", m_spawnOffset.z);
		}
		else if (!stricmp(xmlChild->getTag(), "Strings"))
		{
			const char *pString = 0;
			if (xmlChild->getAttr("friendlyPickUp", &pString))
			{
				m_textMessagePickUpFriendly.Format("@%s", pString);
			}
			if (xmlChild->getAttr("hostilePickUp", &pString))
			{
				m_textMessagePickUpHostile.Format("@%s", pString);
			}
			if (xmlChild->getAttr("friendlyDropped", &pString))
			{
				m_textMessageDroppedFriendly.Format("@%s", pString);
			}
			if (xmlChild->getAttr("hostileDropped", &pString))
			{
				m_textMessageDroppedHostile.Format("@%s", pString);
			}
			if (xmlChild->getAttr("defenderReturned", &pString))
			{
				m_textMessageReturnedDefender.Format("@%s", pString);
			}
			if (xmlChild->getAttr("attackerReturned", &pString))
			{
				m_textMessageReturnedAttacker.Format("@%s", pString);
			}
			if (xmlChild->getAttr("friendlyExtracted", &pString))
			{
				m_textMessageExtractedFriendly.Format("@%s", pString);
			}
			if (xmlChild->getAttr("hostileExtracted", &pString))
			{
				m_textMessageExtractedHostile.Format("@%s", pString);
			}
			if (xmlChild->getAttr("friendlyExtractedEffect", &pString))
			{
				m_textMessageExtractedFriendlyEffect.Format("@%s", pString);
			}
			if (xmlChild->getAttr("hostileExtractedEffect", &pString))
			{
				m_textMessageExtractedHostileEffect.Format("@%s", pString);
			}
			if (xmlChild->getAttr("powerCell", &pString))
			{
				m_textPowerCell.Format("@%s", pString);
			}
			if (xmlChild->getAttr("statusAttackersTicksSafe", &pString))
			{
				m_statusAttackersTicksSafe.Format("@%s", pString);
			}
 			if (xmlChild->getAttr("statusDefendersTicksSafe", &pString))
			{
				m_statusDefendersTicksSafe.Format("@%s", pString);
			}         
			if (xmlChild->getAttr("statusAttackerCarryingTick", &pString))
			{
				m_statusAttackerCarryingTick.Format("@%s", pString);
			}
			if (xmlChild->getAttr("statusDefendersTickCarriedDropped", &pString))
			{
				m_statusDefendersTickCarriedDropped.Format("@%s", pString);
			}
			if (xmlChild->getAttr("iconTextDefend", &pString))
			{
				m_iconTextDefend.Format("@%s", pString);
			}
			if (xmlChild->getAttr("iconTextEscort", &pString))
			{
				m_iconTextEscort.Format("@%s", pString);
			}
			if (xmlChild->getAttr("iconTextSeize", &pString))
			{
				m_iconTextSeize.Format("@%s", pString);
			}
			if (xmlChild->getAttr("iconTextKill", &pString))
			{
				m_iconTextKill.Format("@%s", pString);
			}
			if (xmlChild->getAttr("iconTextExtraction", &pString))
			{
				m_iconTextExtraction.Format("@%s", pString);
			}
			if (xmlChild->getAttr("hasTaken", &pString))
			{
				m_hasTaken.Format("@%s", pString);
			}
			if (xmlChild->getAttr("hasExtracted", &pString))
			{
				m_hasExtracted.Format("@%s", pString);
			}
			if (xmlChild->getAttr("hasDropped", &pString))
			{
				m_hasDropped.Format("@%s", pString);
			}
		}
		else if(!stricmp(xmlChild->getTag(), "Sounds"))
		{
			const char *pString = NULL;
			if(xmlChild->getAttr("pickUpFriendly", &pString))
			{
				m_pickupFriendlySignal = g_pGame->GetGameAudio()->GetSignalID(pString);
			}
			if(xmlChild->getAttr("pickUpHostile", &pString))
			{
				m_pickupHostileSignal = g_pGame->GetGameAudio()->GetSignalID(pString);
			}
			if(xmlChild->getAttr("extractedFriendly", &pString))
			{
				m_extractedFriendlySignal = g_pGame->GetGameAudio()->GetSignalID(pString);
			}
			if(xmlChild->getAttr("extractedHostile", &pString))
			{
				m_extractedHostileSignal = g_pGame->GetGameAudio()->GetSignalID(pString);
			}
			if(xmlChild->getAttr("returnedDefender", &pString))
			{
				m_returnedDefenderSignal = g_pGame->GetGameAudio()->GetSignalID(pString);
			}
			if(xmlChild->getAttr("returnedAttacker", &pString))
			{
				m_returnedAttackerSignal = g_pGame->GetGameAudio()->GetSignalID(pString);
			}
			if(xmlChild->getAttr("droppedFriendly", &pString))
			{
				m_droppedFriendlySignal = g_pGame->GetGameAudio()->GetSignalID(pString);
			}
			if(xmlChild->getAttr("droppedHostile", &pString))
			{
				m_droppedHostileSignal = g_pGame->GetGameAudio()->GetSignalID(pString);
			}
		}
		else if (!stricmp(xmlChild->getTag(), "Physics"))
		{
			const char *pString = NULL;
			if (xmlChild->getAttr("type", &pString))
			{
				if (!stricmp(pString, "Networked"))
				{
					m_physicsType = ePhysType_Networked;
				}
			}
		}
	}

	CGameRules *pGameRules = g_pGame->GetGameRules();
	pGameRules->RegisterClientConnectionListener(this);
	pGameRules->RegisterKillListener(this);
	pGameRules->RegisterActorActionListener(this);
	pGameRules->RegisterTeamChangedListener(this);
	pGameRules->RegisterRoundsListener(this);
	m_moduleRMIIndex = pGameRules->RegisterModuleRMIListener(this);
}

void CGameRulesObjective_Extraction::InitialiseAllHUD()
{
	DbgLog("CGameRulesObjective_Extraction::InitialiseAllHUD()");
	UpdateGameStateText(eGameStateUpdate_Initialise, NULL);

	DbgLog("[icons] CGameRulesObjective_Extraction::InitialiseAllHUD: calling SetIconForAllExtractionPoints()");
	SetIconForAllExtractionPoints();
	DbgLog("[icons] CGameRulesObjective_Extraction::InitialiseAllHUD: calling SetIconForAllPickups()");
	SetIconForAllPickups();
}

#if DEBUG_EXTRACTION

void CGameRulesObjective_Extraction::DebugMe()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();

	IGameRulesRoundsModule*  pRoundsModule = g_pGame->GetGameRules()->GetRoundsModule();
	CRY_ASSERT(pRoundsModule);
	if (pRoundsModule)
	{
		int  primaryTeam = pRoundsModule->GetPrimaryTeam();	// attacking
		int secondaryTeam = (primaryTeam == 1) ? 2 : 1;			// defending
		CryWatch("primaryTeam Attacking=%d; secondaryTeam Defending=%d", primaryTeam, secondaryTeam);

		int localTeamId = pGameRules->GetTeam(localActorId);
		
		CryWatch("local player: team=%d; %s", localTeamId, localTeamId == primaryTeam ? "attacking" : "defending" );
	}

	bool anyDropped=false;

	int numPickups=m_pickups.size();
	for (int i=0; i<numPickups; i++)
	{
		SPickup &pickup = m_pickups[i];

		IEntity *pCarrier = gEnv->pEntitySystem->GetEntity(pickup.m_carrierId);
		IEntity *pickupEntity = gEnv->pEntitySystem->GetEntity(pickup.m_spawnedPickupEntityId);

		CryWatch("Pickup: entityClass=%s; pickupEntity=%s; state=%d carrier=%s; suitMode=%s; timeSinceDropped=%.2f", pickup.m_entityClass ? pickup.m_entityClass->GetName() : "NULL", pickupEntity ? pickupEntity->GetName() : "NULL", pickup.m_state, pCarrier ? pCarrier->GetName() : "NULL", s_extractionSuitModeListStr[pickup.m_suitModeEffected], pickup.m_timeSinceDropped/1000.f);
	
		int numCarriers=pickup.m_carriers.size();
		for (int j=0; j<numCarriers; j++)
		{
			CryWatch("	carriers[%d]=%d", j, pickup.m_carriers[j]);
		}

		if (pickup.m_state == ePickupState_Dropped)
		{
			anyDropped=true;

			for (unsigned int j=0; j<pickup.m_droppedDefenders.size(); j++)
			{
				CryWatch("  defenders[%d]=%s (%d) for %.2fs", j, pGameRules->GetEntityName(pickup.m_droppedDefenders[j].m_playerId), pickup.m_droppedDefenders[j].m_playerId, pickup.m_droppedDefenders[j].m_timeInRange);
			}
		}
	}

	CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(localActorId));
	EntityId pickupId = 0;
	if (pPlayer)
	{
		const SInteractionInfo& interaction = pPlayer->GetCurrentInteractionInfo() ;
		pickupId = interaction.interactionType == eInteraction_GameRulesPickup ? interaction.interactiveEntityId : INVALID_ENTITYID;
	}

	IEntity *pickupableEntity = gEnv->pEntitySystem->GetEntity(pickupId);

	const char* useButton = m_useButtonHeld ? "true" : "false";
	const char* attemptPickup = m_attemptPickup ? "true" : "false";

	CryWatch("m_entPickupableByButton=%d (%s); m_useButtonHeld=%s; m_attemptPickup=%s",	pickupId, pickupableEntity ? pickupableEntity->GetName() : "NULL", useButton, attemptPickup);

	if (anyDropped)
	{
		float serverTime = pGameRules->GetServerTime();
		CryWatch("serverTime=%.2f", serverTime/1000.f);
	}
}

#endif // DEBUG_EXTRACTION

void CGameRulesObjective_Extraction::UpdateButtonPresses()
{
	assert(gEnv->IsClient());


	CGameRules*  pGameRules = g_pGame->GetGameRules();
	IGameRulesSpectatorModule*  specmod = pGameRules->GetSpectatorModule();
	bool abortButtonHolds=false;

	CPlayer *pClientPlayer = static_cast<CPlayer *>(g_pGame->GetIGameFramework()->GetClientActor());
	CRY_ASSERT(pClientPlayer);
	EntityId clientEid = pClientPlayer ? pClientPlayer->GetEntityId() : 0;
	CRY_ASSERT(clientEid);
	if (pClientPlayer)
	{
		if (pClientPlayer->IsDead())
		{
#ifndef _RELEASE
			if (m_useButtonHeld)
			{
				DbgLog("CGameRulesObjective_Extraction::UpdateButtonPresses() player is dead - aborting any button holds");
			}
#endif // _RELEASE
			abortButtonHolds=true;
		}
		else if (specmod && pClientPlayer->GetSpectatorMode() > 0)
		{
#ifndef _RELEASE
			if (m_useButtonHeld)
			{
				DbgLog("CGameRulesObjective_Extraction::UpdateButtonPresses() is in a spectator mode - aborting any button holds");
			}
#endif // _RELEASE
			abortButtonHolds=true;
		}
	}

	if (abortButtonHolds)
	{
		m_useButtonHeld = false;
		m_attemptPickup = false;
	}

	if (pClientPlayer && m_useButtonHeld && m_attemptPickup)
	{
		bool  keepPressing = false;
		const SInteractionInfo& interaction = pClientPlayer->GetCurrentInteractionInfo();

		if(interaction.interactionType == eInteraction_GameRulesPickup)
		{
			if (CWeapon* pWeapon = pClientPlayer->GetWeapon(pClientPlayer->GetCurrentItemId()))
			{
				if(pWeapon->IsRippedOff())
				{
					pClientPlayer->UseItem(pWeapon->GetEntityId());
				}

				if(pWeapon->IsHeavyWeapon())
				{
					pClientPlayer->DropItem(pWeapon->GetEntityId());
				}
			}

			if (pClientPlayer->GetPickAndThrowEntity())
			{
				pClientPlayer->ExitPickAndThrow(true);
			}

			SPickup* pickup = GetPickupForPickupEntityId(interaction.interactiveEntityId);
			CRY_ASSERT_MESSAGE(pickup, "UpdateButtonPresses() has failed to find a pickup for the ent pickupableByButton. this shouldn't happen!");

			if (pickup)
			{
				CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(clientEid));
				CItem *pCurrentItem = static_cast<CItem*>(pActor->GetCurrentItem());
				if (pCurrentItem && pCurrentItem->IsRippedOff())
				{
					//--- Drop HMG
					//--- Then reattempt
					if (!pCurrentItem->IsDeselecting())
					{
						pActor->UseItem(pCurrentItem->GetEntityId());
					}
				}
				else
				{
					m_attemptPickup = false;

					if (gEnv->bServer)
					{
						PlayerCollectsPickup(clientEid, pickup);
					}
					else
					{
						CGameRules::SModuleRMISvClientActionParams::UActionData  data;
						data.helperCarryPickup.pickupEid = interaction.interactiveEntityId;
						CGameRules::SModuleRMISvClientActionParams  params (m_moduleRMIIndex, CGameRules::SModuleRMISvClientActionParams::eACT_HelperCarry_Pickup, &data);
						g_pGame->GetGameRules()->GetGameObject()->InvokeRMI(CGameRules::SvModuleRMIOnAction(), params, eRMI_ToServer);
					}
				}
			}
		}
	}
}

void CGameRulesObjective_Extraction::HandleDefendersForDroppedPickup(SPickup *pickup, float frameTime)
{
	CRY_ASSERT(pickup);
	CRY_ASSERT(pickup->m_state == ePickupState_Dropped);

	IEntity *pickupEntity = gEnv->pEntitySystem->GetEntity(pickup->m_spawnedPickupEntityId);
	
	CGameRules *pGameRules = g_pGame->GetGameRules();
	int pickupTeamId = pGameRules->GetTeam(pickup->m_spawnedPickupEntityId);
	CRY_ASSERT(pickupTeamId);

	CGameRules::TPlayers players;
	pGameRules->GetTeamPlayers(pickupTeamId, players);	// only want to consider defending players on the same team as the dropped pickup

	int numPlayers = players.size();
	for (int i = 0; i < numPlayers; ++ i)
	{
		EntityId playerId = players[i];
		IEntity *playerEntity = gEnv->pEntitySystem->GetEntity(playerId);
		CRY_ASSERT(playerEntity);
		Vec3 playerWorldPos = playerEntity->GetWorldPos();
		const float sqrDist = playerWorldPos.GetSquaredDistance(pickupEntity->GetWorldPos());

		if (sqrDist < m_defendingDroppedRadiusSqr)
		{
			// in range
			int numPlayersInRange = pickup->m_droppedDefenders.size();
			int j;
			for (j=0; j<numPlayersInRange; j++)
			{
				SDefender &defender = pickup->m_droppedDefenders[j];
				if (defender.m_playerId == playerId)
				{
					// found inrange player is already a defender
					defender.m_timeInRange += frameTime;
					DbgLog("CGameRulesObjective_Extraction::HandleDefendersForDroppedPickup() is increasing time spent defending this pickup for player %s", playerEntity->GetName());
					break;
				}
			}

			if (j == numPlayersInRange)
			{
				// inrange player is not currently a defender - adding it
				SDefender newDefender(playerId);
				pickup->m_droppedDefenders.push_back(newDefender);

				DbgLog("CGameRulesObjective_Extraction::HandleDefendersForDroppedPickup() is adding a new defender to this pickup for player %s", playerEntity->GetName());
			}
		}
		else
		{
			// out of range
			int numPlayersInRange = pickup->m_droppedDefenders.size();
			int j;
			for (j=0; j<numPlayersInRange; j++)
			{
				SDefender &defender = pickup->m_droppedDefenders[j];
				if (defender.m_playerId == playerId)
				{
					// current defender is now out of range - removing him
					pickup->m_droppedDefenders.removeAt(j);
					DbgLog("CGameRulesObjective_Extraction::HandleDefendersForDroppedPickup() is removing defending player %s for this pickup as he's out of range", playerEntity->GetName());
				}
			}
		}
	}
}

void CGameRulesObjective_Extraction::Update(float frameTime)
{
#if DEBUG_EXTRACTION
	if (m_extractionDebuggingCVar->GetIVal())
	{
		DebugMe();
	}
#endif

	CGameRules *pGameRules = g_pGame->GetGameRules();
	float serverTime = pGameRules->GetServerTime();
	int numLeft = 0;
	// update the timer on any dropped pickups, on clients and servers!
	int numPickups = m_pickups.size();
	for (int i=0; i<numPickups; i++)
	{
		SPickup *pickup = &m_pickups[i];
		if (pickup->m_state != ePickupState_Extracted )
		{
			numLeft++;
		}
		
		if (pickup->m_state == ePickupState_Dropped)
		{
			if(pickup->m_timeSinceDropped >= pickup->m_timeToCheckForbiddenArea)
			{
				IEntity*  pEntity = gEnv->pEntitySystem->GetEntity(pickup->m_spawnedPickupEntityId);
				if(pEntity)
				{
					if(pGameRules->IsInsideForbiddenArea(pEntity->GetWorldPos(), true, NULL))
					{
						pickup->m_timeSinceDropped = (float) m_droppedRemoveTime;
						CryLog("Resetting due to being inside the ForbiddenArea");
					}
				}

				//check every second
				pickup->m_timeToCheckForbiddenArea = pickup->m_timeSinceDropped + 1000.0f;
			}

			pickup->m_timeSinceDropped += (frameTime * 1000.f);

			// TODO - add only sending the update when the fraction has changed sufficiently
			// probably change it from fraction to frame so we can watch for it changing
			float timeLeftFraction = pickup->m_timeSinceDropped / m_droppedRemoveTime;
			int timeLeftFrame = min(100, max(2, static_cast<int>(100*timeLeftFraction)));

			if (timeLeftFrame != pickup->m_currentObjectiveCountDown)
			{
				SHUDEvent newUpdateObjectiveClock(eHUDEvent_ObjectiveUpdateClock);
				newUpdateObjectiveClock.AddData(static_cast<int>(pickup->m_currentObjectiveFollowEntity));
				newUpdateObjectiveClock.AddData(timeLeftFrame);
				CHUDEventDispatcher::CallEvent(newUpdateObjectiveClock);

				pickup->m_currentObjectiveCountDown=timeLeftFrame;
			}

			HandleDefendersForDroppedPickup(pickup, frameTime);
		}		
	}

	const int numExtractAts=m_extractAtElements.size();
	for (int i=0; i<numExtractAts; i++)
	{
		SExtractAt &extractAt = m_extractAtElements[i];
		IEntity *extractAtEntity = gEnv->pEntitySystem->GetEntity(extractAt.m_entityId);
		const Vec3 extractAtPos = extractAtEntity->GetWorldPos();
		bool hasPickupInRange = false;
		for (int p=0; p<numPickups; p++)
		{
			SPickup *pickup = &m_pickups[p];

			if (pickup->m_state != ePickupState_Extracted)
			{
				IEntity *pickupEntity = gEnv->pEntitySystem->GetEntity(pickup->m_spawnedPickupEntityId);
				if (pickupEntity && (extractAtPos.GetSquaredDistance(pickupEntity->GetWorldPos()) < extractAt.m_sqrPickupRange))
				{
					hasPickupInRange = true;
					break;
				}
			}
		}

		if (hasPickupInRange != extractAt.m_hasPickupInRange)
		{
			extractAt.m_hasPickupInRange = hasPickupInRange;
			CallEntityScriptFunction(extractAt.m_entityId, "SetOpen", hasPickupInRange);
		}
	}

	if( m_numberToExtract != numLeft )
	{
		SHUDEvent extractionNumberLeft(eHUDEvent_OnUpdateObjectivesLeft);
		extractionNumberLeft.AddData(numLeft);
		CHUDEventDispatcher::CallEvent(extractionNumberLeft);

		m_numberToExtract = numLeft;
	}

	if (gEnv->IsClient())
	{
		if (!m_hasFirstFrameInited)
		{
			m_hasFirstFrameInited=true;
		
			DbgLog("[icons] CGameRulesObjective_Extraction::Update() first frame init calling InitialiseAllHUD()");
			InitialiseAllHUD();
		}

		UpdateButtonPresses();
	}

	if (!gEnv->bServer)
	{
		return;
	}

	int primaryTeam = -1;
	int secondaryTeam = -1;

	IGameRulesRoundsModule*  pRoundsModule = pGameRules->GetRoundsModule();
	CRY_ASSERT(pRoundsModule);
	if (pRoundsModule)
	{
		primaryTeam = pRoundsModule->GetPrimaryTeam();	// attacking
		secondaryTeam = 3 - primaryTeam; 								// defending
	}

	for (int i=0; i<numPickups; i++)
	{
		SPickup *pickup = &m_pickups[i];
		if (pickup->m_state == ePickupState_Dropped)
		{
			if (pickup->m_timeSinceDropped > m_droppedRemoveTime)
			{
				IEntity *spawnedAtEntity = gEnv->pEntitySystem->GetEntity(pickup->m_spawnAt);
				IEntity *pickupEntity = gEnv->pEntitySystem->GetEntity(pickup->m_spawnedPickupEntityId);
				
				if (IGameRulesScoringModule* pScoringModule=pGameRules->GetScoringModule())
				{
					int numDefenders = pickup->m_droppedDefenders.size();
					for (int j=0; j<numDefenders; j++)
					{
						SDefender &defender = pickup->m_droppedDefenders[j];
						if (defender.m_timeInRange > m_defendingDroppedTimeToScore)
						{
							pScoringModule->OnPlayerScoringEvent(defender.m_playerId, EGRST_Extraction_ObjectiveReturnDefend);
						}
					}
				}

				PickupReturnsCommon(pickup, pickupEntity, spawnedAtEntity, false);

				CGameRules::SModuleRMITwoEntityParams params;
				params.m_listenerIndex = m_moduleRMIIndex;
				params.m_entityId1 = pickup->m_spawnedPickupEntityId;
				params.m_entityId2 = pickup->m_spawnAt;
				params.m_data = eRMITypeDoubleEntity_pickup_returned_to_base;

				// RMI will patch entityIDs between clients
				// this RMI will depend on the pickup entity existing on the clients before being sent
				pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), params, eRMI_ToRemoteClients, pickup->m_spawnedPickupEntityId);
			}
		}
	}
}

void CGameRulesObjective_Extraction::RemoveAllPickups()
{
	DbgLog("CGameRulesObjective_Extraction::RemoveAllPickups()");

	int numPickups=m_pickups.size();
	for (int i=0; i<numPickups; i++)
	{
		SPickup *pickup = &m_pickups[i];
		if (pickup->m_spawnedPickupEntityId)
		{
			if(pickup->m_carrierId)
			{
				g_pGame->GetGameRules()->OnPickupEntityDetached(pickup->m_spawnedPickupEntityId, pickup->m_carrierId, true, pickup->m_entityClass->GetName());
			}

			gEnv->pEntitySystem->RemoveEntity(pickup->m_spawnedPickupEntityId);
			pickup->m_spawnedPickupEntityId=0;
		}

		if (pickup->m_spawnedFallbackGunId)
		{
			gEnv->pEntitySystem->RemoveEntity(pickup->m_spawnedFallbackGunId);
			pickup->m_spawnedFallbackGunId=0;
		}

		pickup->m_carrierId=0;
		pickup->m_extractedBy=0;
		DbgLog("[state] CGameRulesObjective_Extraction::RemoveAllPickups: setting pickup '%d's (*%p) state to ePickupState_None", i, pickup);
		pickup->SetState(ePickupState_None);
	}
}

CGameRulesObjective_Extraction::ERMITypes 
	CGameRulesObjective_Extraction::GetSpawnedPickupRMITypeFromSuitMode(EExtractionSuitMode suitMode)
{
	ERMITypes rmiType=eRMIType_None;
	switch(suitMode)
	{
		case eExtractionSuitMode_Power:
			rmiType=eRMITypeDoubleEntity_power_pickup_spawned;
			break;
		case eExtractionSuitMode_Armour:
			rmiType=eRMITypeDoubleEntity_armour_pickup_spawned;
			break;
		case eExtractionSuitMode_Stealth:
			rmiType=eRMITypeDoubleEntity_stealth_pickup_spawned;
			break;
		default:
			CRY_ASSERT_MESSAGE(0, string().Format("GetSpawnedPickupRMITypeFromSuitMode() has encountered and unhandled suitmode=%d", suitMode));
			break;
	}

	return rmiType;
}

void CGameRulesObjective_Extraction::NewFallbackGunForPickup(EntityId pickupEntityId, EntityId fallbackGunEntityId)
{	
	DbgLog("CGameRulesObjective_Extraction::NewFallbackGunForPickup() pickupEntityId=%d; fallbackGunEntityId=%d", pickupEntityId, fallbackGunEntityId);

	SPickup *pickup = GetPickupForPickupEntityId(pickupEntityId);
	CRY_ASSERT_MESSAGE(pickup, "failed to find pickup for entity id");
	if (pickup)
	{
		pickup->m_spawnedFallbackGunId = fallbackGunEntityId;

		// tell lua about their new hammer to play with
		IEntity *pickupEntity = gEnv->pEntitySystem->GetEntity(pickupEntityId);
		CRY_ASSERT_MESSAGE(pickupEntity, "NewFallbackGunForPickup() failed to get the pickup entity");

		if (pickupEntity)
		{
			IEntity *spawnedFallbackGun = gEnv->pEntitySystem->GetEntity(fallbackGunEntityId);
			CRY_ASSERT_MESSAGE(spawnedFallbackGun, "NewFallbackGunForPickup() failed to get the hammer entity");

			if (spawnedFallbackGun)
			{
				spawnedFallbackGun->Hide(true);

				CCarryEntity *pTick = static_cast<CCarryEntity*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(pickup->m_spawnedPickupEntityId, pickup->m_entityClass->GetName()));
				if (pTick)
				{
					pTick->SetSpawnedWeaponId(spawnedFallbackGun->GetId());
				}
			}
		}
	}
}

void CGameRulesObjective_Extraction::SpawnAllPickups()
{
	DbgLog("CGameRulesObjective_Extraction::SpawnAllPickups()");
	
	IGameRulesRoundsModule*  pRoundsModule = g_pGame->GetGameRules()->GetRoundsModule();
	CRY_ASSERT(pRoundsModule);
	if (pRoundsModule)
	{
		int  primaryTeam = pRoundsModule->GetPrimaryTeam();	// attacking
		int secondaryTeam = (primaryTeam == 1) ? 2 : 1;			// defending

		m_availableSpawnAtElements.clear();
		int numSpawnAts=m_spawnAtElements.size();
		for (int i=0; i<numSpawnAts; i++)
		{
			m_availableSpawnAtElements.push_back(&m_spawnAtElements[i]);
		}

		// setup the spawns
		int numPickups=m_pickups.size();
		for (int i=0; i<numPickups; i++)
		{
			SPickup *pickup = &m_pickups[i];

			if (pickup->m_entityClass)
			{
				EntityId spawnAtEntityId=FindNewEntityIdToSpawnAt();
				if (spawnAtEntityId)
				{
					pickup->m_spawnAt=spawnAtEntityId;
					const char *enumStr=s_extractionSuitModeListStr[pickup->m_suitModeEffected];
					const char *enumName=strchr(enumStr, '_');
					if (enumName)
					{
						enumName++;
					}
					else
					{
						enumName=enumStr;
					}
					
					IEntity *entity=SpawnEntity(spawnAtEntityId, pickup->m_entityClass, secondaryTeam, enumName);
					pickup->m_spawnedPickupEntityId=entity->GetId();
					pickup->SetState(ePickupState_AtBase);
	
					ERMITypes rmiType=GetSpawnedPickupRMITypeFromSuitMode(pickup->m_suitModeEffected);
					if (rmiType != eRMIType_None)
					{
						// tell clients about new pickup (and who spawned it)
						CGameRules::SModuleRMITwoEntityParams params;
						params.m_listenerIndex = m_moduleRMIIndex;
						params.m_entityId1 = pickup->m_spawnedPickupEntityId;
						params.m_entityId2 = pickup->m_spawnAt;
						params.m_data = rmiType;

						g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), params, eRMI_ToRemoteClients, pickup->m_spawnedPickupEntityId);
					}

					gEnv->pEntitySystem->AddEntityEventListener(pickup->m_spawnedPickupEntityId, ENTITY_EVENT_DONE, this);

					Vec3 zeroVec(ZERO);
					Ang3 zeroAng(ZERO);
					SEntitySpawnParams params;
					params.pClass = m_fallbackGunEntityClass;
					params.sName = "MagicHammer"; 
					params.vPosition = zeroVec;
					params.qRotation = Quat(zeroAng);
					params.nFlags = ENTITY_FLAG_NO_PROXIMITY|ENTITY_FLAG_NEVER_NETWORK_STATIC;	
					IEntity *spawnedMagicHammer = gEnv->pEntitySystem->SpawnEntity(params, true);
					CRY_ASSERT_MESSAGE(spawnedMagicHammer, "failed to spawn magic hammer. This shouldn't happen");
					if (spawnedMagicHammer)
					{
						NewFallbackGunForPickup(pickup->m_spawnedPickupEntityId, spawnedMagicHammer->GetId());

						DbgLog("CGameRulesObjective_Extraction::SpawnAllPickups() sending a spawned fallbackGun RMI to clients");
						CGameRules::SModuleRMITwoEntityParams entparams;
						entparams.m_listenerIndex = m_moduleRMIIndex;
						entparams.m_entityId1 = pickup->m_spawnedPickupEntityId;
						entparams.m_entityId2 = pickup->m_spawnedFallbackGunId;
						entparams.m_data = eRMITypeDoubleEntity_fallback_gun_spawned;

						// spawns are unordered over the network but the RMIs are ordered. So depending on the hammer here, the spawned tick above will have arrived as it will block the RMI above
						g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), entparams, eRMI_ToRemoteClients, pickup->m_spawnedFallbackGunId);
					}


					// this is too early the local player doesn't have his team setup yet
					//SetIconForPickup(pickup);
				}
				else
				{
					CRY_ASSERT_MESSAGE(0, "Failed to find and entityId to spawn at. Unable to spawn a pickup!!!");
				}
			}
			else
			{
				CRY_ASSERT_MESSAGE(0, "failed to try and spawn pickup. entityClass is invalid");
			}
		}

		CRY_ASSERT_MESSAGE(gEnv->bServer, "we're about to change aspects after spawning pickups, we better be on the server, and we aint!!");
		CHANGED_NETWORK_STATE(g_pGame->GetGameRules(), EXTRACTION_OBJECTIVE_STATE_ASPECT);
	}
}

// On first round this is happening BEFORE the rounds module calls OnStartGame() 
// so no teams are setup at this point. Hence Using OnStartGamePost()
// On the second round however this is happening AFTER teams have been setup so we're ok to use it
// On the second round this is ONLY called on the server though, NOT on clients :(
void CGameRulesObjective_Extraction::OnStartGame()
{
	if (gEnv->bServer)
	{
		const float timeLimit = GetTimeLimit();
		DbgLog("CGameRulesObjective_Extraction::OnStartGame() setting timeLimit to %f", timeLimit);
		g_pGameCVars->g_timelimit = timeLimit;
		
		if (m_haveStartedGame)
		{
			DbgLog("CGameRulesObjective_Extraction::OnStartGame() and haveStartedGame so this is round 2");

			// all the teams have swapped but the original spawnAts, extractAts are all valid so just reuse them

			// tear down any existing setup from before
			RemoveAllPickups();

			SpawnAllPickups();

			int numExtractAts=m_extractAtElements.size();
			for (int i=0; i<numExtractAts; i++)
			{
				SExtractAt &extractAt = m_extractAtElements[i];

				DbgLog("[icons] CGameRulesObjective_Extraction::OnStartGame: calling SetIconForExtractionPoint(entityId=%d) [extractAt idx %d]", extractAt.m_entityId, i);
				SetIconForExtractionPoint(extractAt.m_entityId);
				// tell clients about new extraction point
				CGameRules::SModuleRMIEntityParams params(m_moduleRMIIndex, extractAt.m_entityId, eRMITypeSingleEntity_extraction_point_added);
				g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), params, eRMI_ToRemoteClients, extractAt.m_entityId);
			}

			DbgLog("[icons] CGameRulesObjective_Extraction::OnStartGame: calling SetIconForAllPickups()");
			SetIconForAllPickups();
		}
		else
		{
			DbgLog("CGameRulesObjective_Extraction::OnStartGame() and have NOT Started Game so this is the first round. Doing nothing");
		}
	}
}

// done after rounds has had chance to setup the teams
// this is only called for first round!
void CGameRulesObjective_Extraction::OnStartGamePost()
{
	DbgLog("CGameRulesObjective_Extraction::OnStartGamePost()");
	
	// TODO - be careful this is likely to be called for the 2nd round and perhaps wont want to do everything it does on the initial run
	// perhaps add hasStarted bool to let us distinguish between intial round and round 2
	if (m_spawnAtEntityClass)
	{
		IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();
		IEntity *pEntity = 0;

		IGameRulesRoundsModule*  pRoundsModule = g_pGame->GetGameRules()->GetRoundsModule();
		CRY_ASSERT(pRoundsModule);
		if (pRoundsModule)
		{
			int  primaryTeam = pRoundsModule->GetPrimaryTeam();	// attacking
			int secondaryTeam = (primaryTeam == 1) ? 2 : 1;			// defending
			
			pIt->MoveFirst();
			while(pEntity = pIt->Next())
			{
				const EntityId entId = pEntity->GetId();
				int teamId = g_pGame->GetGameRules()->GetTeam(entId);

				// rounds module will be setting up the teams on the random number of spawnAt points
				// so we probably don't need to do any randomisation in here, just consider all the spawnAts on the defenders team
				if (pEntity->GetClass() == m_spawnAtEntityClass)
				{
					if (teamId && teamId != primaryTeam)
					{
						SSpawnAt spawnAt;
						spawnAt.m_entityId = entId;
						m_spawnAtElements.push_back(spawnAt);

						gEnv->pEntitySystem->AddEntityEventListener(entId, ENTITY_EVENT_ENTERAREA, this);
						gEnv->pEntitySystem->AddEntityEventListener(entId, ENTITY_EVENT_LEAVEAREA, this);

						DbgLog("found 'spawn at' entity, idx=%i, id=%i, name=%s, team=%d", (m_spawnAtElements.size() - 1), entId, pEntity->GetName(), g_pGame->GetGameRules()->GetTeam(entId));
					}
					else
					{
						DbgLog("ignoring 'spawn at' entity, id=%i, name=%s, team=%d", entId, pEntity->GetName(), g_pGame->GetGameRules()->GetTeam(entId));
					}
				}

				// gather extraction points
				if (pEntity->GetClass() == m_extractAtEntityClass)
				{
					if (teamId && teamId == primaryTeam)
					{
						SExtractAt extractAt;
						extractAt.m_entityId = entId;
						IScriptTable*  pScript = pEntity->GetScriptTable();
						if (pScript->GetValueType("Properties") == svtObject)
						{
							SmartScriptTable pPropertiesScript;
							if (pScript->GetValue("Properties", pPropertiesScript))
							{
								if (pPropertiesScript->GetValue("OpenRange", extractAt.m_sqrPickupRange))
								{
									extractAt.m_sqrPickupRange *= extractAt.m_sqrPickupRange;
								}
								else
								{
									extractAt.m_sqrPickupRange = 6.0f * 6.0f;
								}
							}
						}
						m_extractAtElements.push_back(extractAt);

						gEnv->pEntitySystem->AddEntityEventListener(entId, ENTITY_EVENT_ENTERAREA, this);

						DbgLog("found 'extract at' entity, idx=%i, id=%i, name=%s, team=%d", (m_extractAtElements.size() - 1), entId, pEntity->GetName(), g_pGame->GetGameRules()->GetTeam(entId));
					}
					else
					{
						DbgLog("ignoring 'extract at' entity, id=%i, name=%s, team=%d", entId, pEntity->GetName(), g_pGame->GetGameRules()->GetTeam(entId));
					}
				}
			}

			if (gEnv->bServer)
			{
				SpawnAllPickups();
			}
		}
	}

	m_haveStartedGame=true;
}

bool CGameRulesObjective_Extraction::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	return true;
}

EntityId CGameRulesObjective_Extraction::FindNewEntityIdToSpawnAt()
{
	EntityId ret=0;

	if (m_availableSpawnAtElements.size() > 0)
	{
		int index=g_pGame->GetRandomNumber() % m_availableSpawnAtElements.size();
		ret=m_availableSpawnAtElements[index]->m_entityId;
		m_availableSpawnAtElements.removeAt(index);
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "Failed to find an entityId to spawn at. No available spawnAts left!");
	}

	return ret;
}

void AttachToDevice(IEntity *deviceEnt, EntityId tickEnt, bool attach)
{
	ICharacterInstance *pCharInst = deviceEnt ? deviceEnt->GetCharacter(0) : NULL;
	if (pCharInst)
	{
		IAttachment *pAttachment = pCharInst->GetIAttachmentManager()->GetInterfaceByName("handle");
		if (pAttachment)
		{
			if (attach)
			{
				CEntityAttachment *pEntityAttachment = new CEntityAttachment();
				pEntityAttachment->SetEntityId(tickEnt);
				pAttachment->AddBinding(pEntityAttachment);
				//pEntityAttachment->ProcessAttachment(pAttachment); // ensure the newly attached tick updates its position if the device isn't currently being rendered - doesn't work for initial spawn
			}
			else
			{
				IAttachmentObject *entObj = pAttachment->GetIAttachmentObject();
				EntityId curEntID = entObj != NULL && entObj->GetAttachmentType() == (IAttachmentObject::eAttachment_Entity) ? ((CEntityAttachment*)entObj)->GetEntityId() : 0;

				//--- Validate the tick is attached, the player may be recollecting it from the floor
				if (curEntID == tickEnt)
				{
					pAttachment->ClearBinding();
				}
			}
		}
	}
}

//------------------------------------------------------------------------
IEntity *CGameRulesObjective_Extraction::SpawnEntity( EntityId spawnAt, IEntityClass *spawnEntityClass, int teamId, const char *name )
{
	DbgLog("CGameRulesObjective_Extraction::SpawnEntity() spawnAt=%d; spawnEntityClass=%p; teamId=%d", spawnAt, spawnEntityClass, teamId);

	IEntity *pSpawnedEntity = NULL;

	IEntity *pSpawnAtEntity = gEnv->pEntitySystem->GetEntity(spawnAt);
	if (pSpawnAtEntity)
	{
		// Spawn entity

		//CryFixedStringT<32>  sName;
		//if (m_prefixSpawnAtName)
		//	sName.Format("%s__spawnedItem_%i", pSpawnAtEntity->GetName(), teamId);
		//else
		//	sName.Format("spawnedItem_%i", teamId);

		SEntitySpawnParams params;
		params.pClass = spawnEntityClass; 
		params.sName = name; //sName.c_str();
		params.vPosition = pSpawnAtEntity->GetWorldPos();
		params.vPosition += m_spawnOffset;

		Ang3 rotation = pSpawnAtEntity->GetWorldAngles();
		//rotation.x += m_spawnRotation.x;
		//rotation.y += m_spawnRotation.y;
		//rotation.z += m_spawnRotation.z;
		params.qRotation = Quat(rotation);
		params.nFlags = ENTITY_FLAG_NEVER_NETWORK_STATIC;

		pSpawnedEntity = gEnv->pEntitySystem->SpawnEntity(params, true);
		if (pSpawnedEntity)
		{
			IGameObject *pGameObject = g_pGame->GetIGameFramework()->GetGameObject(pSpawnedEntity->GetId());
			if (pGameObject != NULL && spawnAt)
			{
				pGameObject->SetNetworkParent(spawnAt);
			}
			
			g_pGame->GetGameRules()->SetTeam(teamId, pSpawnedEntity->GetId());
			DbgLog("CGameRulesObjectiveHelper_Spawn::Update(), spawned entity '%s' id=%i for team %i", pSpawnedEntity->GetName(), pSpawnedEntity->GetId(), teamId);

			AttachToDevice(pSpawnAtEntity, pSpawnedEntity->GetId(), true);

			/*
			int itemTeamId = GetItemTeamId(teamId);
			int itemTeamIndex = itemTeamId - 1;
			g_pGame->GetGameRules()->SetTeam(itemTeamId, pSpawnedEntity->GetId());
			
			if (pDetails->m_spawnedEntityId[itemTeamIndex])
			{
				// If we already have an entity for this slot, remove it
				gEnv->pEntitySystem->RemoveEntity(pDetails->m_spawnedEntityId[itemTeamIndex]);
			}

			pDetails->m_spawnedEntityId[itemTeamIndex] = pSpawnedEntity->GetId();
			pDetails->m_carrierEntityId[itemTeamIndex] = 0;
			pDetails->m_state[itemTeamIndex] = ESES_AtBase;

			gEnv->pEntitySystem->AddEntityEventListener(pSpawnedEntity->GetId(), ENTITY_EVENT_DONE, this);
			if (gEnv->IsClient())
			{
				AddEntity(pSpawnedEntity->GetId(), pDetails->m_positionEntityId, pDetails->m_state[itemTeamIndex]);
			}
			CGameRules::SModuleRMITwoEntityParams params(m_moduleRMIIndex, pSpawnedEntity->GetId(), pDetails->m_positionEntityId, int(pDetails->m_state[itemTeamIndex]));
			g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), params, eRMI_ToRemoteClients, params.m_entityId1);
			*/
		}
		else
		{
			CRY_ASSERT_MESSAGE(0, "failed to spawn entity");
		}
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "failed to find entity to spawnAt");
	}

	return pSpawnedEntity;
}

void CGameRulesObjective_Extraction::OnHostMigration(bool becomeServer)
{
	CryLog("CGameRulesObjective_Extraction::OnHostMigration becomeServer %d", becomeServer);
	
	if(becomeServer)
	{
		int numPickups = m_pickups.size();
		for (int i=0; i<numPickups; i++)
		{
			SPickup *pPickup = &m_pickups[i];
			if (pPickup->m_state == ePickupState_Carried && pPickup->m_carrierId)
			{
				IEntity *pCarrier = gEnv->pEntitySystem->GetEntity(pPickup->m_carrierId);
				if(!pCarrier)
				{
					CryLog("entity %d is carried by entity %d but entity does not exist, dropping...", pPickup->m_spawnedPickupEntityId, pPickup->m_carrierId);
					CheckForAndDropItem(pPickup->m_carrierId);
				}	
			}
		}
	}
}

// will only return a valid pickup* if inEntityId>0
CGameRulesObjective_Extraction::SPickup *CGameRulesObjective_Extraction::GetPickupForPickupEntityId(EntityId inEntityId)
{
	SPickup *ret=NULL;
	int numPickups=m_pickups.size();
	CRY_ASSERT_MESSAGE(inEntityId, "GetPickupForPickupEntityId() doesn't really want to ever pass inEntityId=0");
	if (inEntityId)
	{
		for (int i=0; i<numPickups; i++)
		{
			SPickup *pickup = &m_pickups[i];	
			if (inEntityId == pickup->m_spawnedPickupEntityId)
			{
				ret = pickup;
				break;
			}
		}
	}

	return ret;
}

// will only return a valid pickup* if inEntityId>0
CGameRulesObjective_Extraction::SPickup *CGameRulesObjective_Extraction::GetPickupForCarrierEntityId(EntityId inEntityId)
{
	SPickup *ret=NULL;
	int numPickups=m_pickups.size();
	CRY_ASSERT_MESSAGE(inEntityId, "GetPickupForCarrierEntityId() doesn't really want to ever pass inEntityId=0");
	if (inEntityId)
	{
		for (int i=0; i<numPickups; i++)
		{
			SPickup *pickup = &m_pickups[i];	
			if (inEntityId == pickup->m_carrierId)
			{
				ret = pickup;
				break;
			}
		}
	}

	return ret;
}

CGameRulesObjective_Extraction::SPickup *CGameRulesObjective_Extraction::GetPickupForSuitMode(EExtractionSuitMode suitMode)
{
	SPickup *ret=NULL;
	int numPickups=m_pickups.size();
	for (int i=0; i<numPickups; i++)
	{
		SPickup *pickup = &m_pickups[i];	
		if (suitMode == pickup->m_suitModeEffected)
		{
			ret = pickup;
			break;
		}
	}

	return ret;
}

CGameRulesObjective_Extraction::SExtractAt *CGameRulesObjective_Extraction::GetExtractAtForEntityId(EntityId inEntityId)
{
	SExtractAt *outExtractAt=NULL;
	const int numExtractAts = m_extractAtElements.size();

	for (int i = 0; i < numExtractAts; i ++)
	{
		SExtractAt &extractAt = m_extractAtElements[i];
		if (extractAt.m_entityId == inEntityId)
		{
			outExtractAt = &extractAt;
			break;
		}
	}

	CRY_ASSERT_MESSAGE(outExtractAt, "failed to find extractAt for entityId");
	return outExtractAt;
}


int CGameRulesObjective_Extraction::FindCarrierInPickupCarriers(SPickup *pickup, EntityId carrierId)
{
	DbgLog("CGameRulesObjective_Extraction::FindCarrierInPickupCarriers()");	
	int foundAt=-1;

	int numCarriers=pickup->m_carriers.size();
	for (int i=0; i<numCarriers; i++)
	{
		if (pickup->m_carriers[i] == carrierId)
		{
			DbgLog("have found that carrier is already present in list of carriers at index=%d", i);
			foundAt=i;
			break;
		}
	}

	return foundAt;
}

bool CGameRulesObjective_Extraction::CarrierAlreadyInPickupCarriers(SPickup *pickup, EntityId carrierId)
{
	return (FindCarrierInPickupCarriers(pickup, carrierId) >= 0); 
}


void CGameRulesObjective_Extraction::CallEntityScriptFunction(EntityId entityId, const char *function, bool active)
{
	if (IEntity* pEntity=gEnv->pEntitySystem->GetEntity(entityId))
	{
		IScriptTable*  pScript = pEntity->GetScriptTable();
		if (pScript != NULL && (pScript->GetValueType(function) == svtFunction))
		{
			IScriptSystem*  pScriptSystem = gEnv->pScriptSystem;
			pScriptSystem->BeginCall(pScript, function);
			pScriptSystem->PushFuncParam(pScript);
			pScriptSystem->PushFuncParam(active);
			pScriptSystem->EndCall();
		}
	}
}

bool CGameRulesObjective_Extraction::AddCarrierToPickup(SPickup *pickup, EntityId carrierId)
{
	bool addedCarrierToArray=false;

	DbgLog("CGameRulesObjective_Extraction::AddCarrierToPickup()");

	pickup->m_carrierId = carrierId;

	// do for server and client now so our carriers are valid after a host migration
	{
		bool alreadyInCarriers=CarrierAlreadyInPickupCarriers(pickup, carrierId);

		if (!alreadyInCarriers)
		{
			pickup->m_carriers.push_back(carrierId);
			addedCarrierToArray=true;
		}
	}

	return addedCarrierToArray;
}

void CGameRulesObjective_Extraction::PlayerCollectsPickupCommon(EntityId playerId, SPickup *pickup, bool newPickup)
{
	DbgLog("CGameRulesObjective_Extraction::PlayerCollectsPickupCommon() playerId=%d", playerId);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	
	bool added=AddCarrierToPickup(pickup, playerId);
	if (!added && gEnv->bServer )
	{
		DbgLog("CGameRulesObjective_Extraction::PlayerCollectsPickupCommon() not adding playerId=%d to pickup carriers, already in list", playerId);
	}

	IEntity *pSpawnAtEntity = gEnv->pEntitySystem->GetEntity(pickup->m_spawnAt);
	AttachToDevice(pSpawnAtEntity, pickup->m_spawnedPickupEntityId, false);
	CallEntityScriptFunction(pickup->m_spawnAt, "HasTick", false);

	pGameRules->OnPickupEntityAttached(pickup->m_spawnedPickupEntityId, playerId, pickup->m_entityClass->GetName());
	pickup->SetState(ePickupState_Carried);

	DbgLog("[icons] CGameRulesObjective_Extraction::PlayerCollectsPickupCommon: calling SetIconForAllExtractionPoints()");
	SetIconForAllExtractionPoints();
	DbgLog("[icons] CGameRulesObjective_Extraction::PlayerCollectsPickupCommon: calling SetIconForAllPickups()");
	SetIconForAllPickups();
	//SetIconForPickup(pickup);
	
	IEntity *playerEntity = gEnv->pEntitySystem->GetEntity(playerId);

	PhysicalizeEntityForPickup(pickup, false);

	if (newPickup)
	{
		EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
		int localTeamId = pGameRules->GetTeam(localActorId);
		int playerTeamId = pGameRules->GetTeam(playerId);
		bool localPlayerPlayerSameTeam = (localTeamId == playerTeamId);
		const char *pickupString = GetPickupString(pickup);
		string pickupStringCache = pickupString;
		
		CryFixedStringT<DISPLAY_NAME_LENGTH> displayName("");
		if( CGameLobby* pLobby = g_pGame->GetGameLobby() )
		{
			pLobby->GetPlayerDisplayNameFromEntity( playerId, displayName );
		}
		else
		{
			displayName = playerEntity ? playerEntity->GetName() : "NULL ENTITY";	
		}

		if (localPlayerPlayerSameTeam)
		{
			CAudioSignalPlayer::JustPlay(m_pickupFriendlySignal);	 
		}
		else 
		{
			CAudioSignalPlayer::JustPlay(m_pickupHostileSignal);
		}

		SHUDEventWrapper::OnGameStateMessage(playerId, localPlayerPlayerSameTeam, m_hasTaken.c_str(), eBLIF_Tick);
	}

	CAnnouncer::GetInstance()->Announce(playerId, "PickUpCell", CAnnouncer::eAC_inGame); 
	UpdateGameStateText(eGameStateUpdate_TickCollected, pickup);
}

void CGameRulesObjective_Extraction::PlayerCollectsPickup(EntityId playerId, SPickup *pickup)
{
	DbgLog("CGameRulesObjective_Extraction::PlayerCollectsPickup() playerId=%d", playerId);

	if (CarrierAlreadyInPickupCarriers(pickup, playerId))
	{
		DbgLog("CGameRulesObjective_Extraction::PlayerCollectsPickup() - has found player is already in carriers which means that we've already scored collecting this pickup this life. Not scoring again");
	}
	else
	{
		DbgLog("CGameRulesObjective_Extraction::PlayerCollectsPickup() - has found player NOT already in carriers. Scoring for collecting this pickup");
		if (IGameRulesScoringModule* pScoringModule=g_pGame->GetGameRules()->GetScoringModule())
		{
			pScoringModule->OnPlayerScoringEvent(playerId, EGRST_CarryObjectiveTaken);
		}
	}

	PlayerCollectsPickupCommon(playerId, pickup, true);

	// tell clients about the pickup
	CGameRules::SModuleRMITwoEntityParams params;
	params.m_listenerIndex = m_moduleRMIIndex;
	params.m_entityId1 = playerId;
	params.m_entityId2 = pickup->m_spawnedPickupEntityId;
	params.m_data = eRMITypeDoubleEntity_pickup_collected;

	// RMI will patch entityIDs between clients
	// this RMI will depend on the pickup entity existing on the clients before being sent
	g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), params, eRMI_ToRemoteClients, pickup->m_spawnedPickupEntityId);
}

void CGameRulesObjective_Extraction::PlayerExtractsPickupCommon(EntityId playerId, SPickup *pickup)
{
	DbgLog("CGameRulesObjective_Extraction::PlayerExtractsPickupCommon() playerId=%d", playerId);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	const EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
	const bool isLocalActor = (gEnv->IsClient() && (playerId == localActorId));
	const int localTeamId = pGameRules->GetTeam(localActorId);
	const int playerTeamId = pGameRules->GetTeam(playerId);
	const bool localPlayerPlayerSameTeam = (localTeamId == playerTeamId);
	IEntity *playerEntity = gEnv->pEntitySystem->GetEntity(playerId);

	pGameRules->OnPickupEntityDetached(pickup->m_spawnedPickupEntityId, playerId, true, pickup->m_entityClass->GetName());
	
	pickup->SetState(ePickupState_Extracted);
	pickup->m_extractedBy = pickup->m_carrierId;
	pickup->m_carrierId=0;
	
	DbgLog("[icons] CGameRulesObjective_Extraction::PlayerExtractsPickupCommon: calling SetIconForAllPickups()");
	SetIconForAllPickups();
	DbgLog("[icons] CGameRulesObjective_Extraction::PlayerExtractsPickupCommon: calling SetIconForAllExtractionPoints()");
	SetIconForAllExtractionPoints();
	//SetIconForPickup(pickup);

	PhysicalizeEntityForPickup(pickup, false);

	CryFixedStringT<DISPLAY_NAME_LENGTH> displayName("");
	if( CGameLobby* pLobby = g_pGame->GetGameLobby() )
	{
		pLobby->GetPlayerDisplayNameFromEntity( playerId, displayName );
	}
	else
	{
		displayName = playerEntity ? playerEntity->GetName() : "NULL ENTITY";	
	}

	const char *pickupString = GetPickupString(pickup);	
	string pickupStringCache = pickupString;

	if (isLocalActor)
	{
		SHUDEventWrapper::ClearInteractionRequest(true); // remove drop bug interaction prompt
	}

	if (localPlayerPlayerSameTeam)
	{
		CAudioSignalPlayer::JustPlay(m_extractedFriendlySignal);
	}
	else
	{
		CAudioSignalPlayer::JustPlay(m_extractedHostileSignal);
	}

	SHUDEventWrapper::OnGameStateMessage(playerId, localPlayerPlayerSameTeam, m_hasExtracted.c_str(), eBLIF_Tick);

	PlayerExtractsPickupCommonAnnouncement(playerId);

	UpdateGameStateText(eGameStateUpdate_TickExtracted, pickup);

	EntityId pickupEntityId = pickup->m_spawnedPickupEntityId;
	IEntity *pickupEntity = gEnv->pEntitySystem->GetEntity(pickupEntityId);
	IEntity *spawnedAtEntity = gEnv->pEntitySystem->GetEntity(pickup->m_spawnAt); // should be synced over to clients via RMIs

	CRY_ASSERT(pickupEntity);
	CRY_ASSERT(spawnedAtEntity);

	PickupReturnsCommon(pickup, pickupEntity, spawnedAtEntity, true);
}

void CGameRulesObjective_Extraction::PlayerExtractsPickupCommonAnnouncement(const EntityId playerId) const
{
	CAnnouncer::GetInstance()->Announce(playerId, "ExtractedCell", CAnnouncer::eAC_inGame);
}

void CGameRulesObjective_Extraction::PlayerExtractsPickup(EntityId playerId, SPickup *pickup, IEntity *extractionPointEntity)
{
	// technically playerId is redundant as pickup already has carrierId
	DbgLog("CGameRulesObjective_Extraction::PlayerExtractsPickup() playerId=%d", playerId);

	EntityId pickupEntityId = pickup->m_spawnedPickupEntityId;

	PlayerExtractsPickupCommon(playerId, pickup);

	//g_pGame->GetGameRules()->OnPickupEntityAttached(pickup->m_spawnedPickupEntityId, playerId);

	CGameRules *pGameRules = g_pGame->GetGameRules();

	// change player suit effects
	int teamId = pGameRules->GetTeam(playerId);
	
	if (IGameRulesScoringModule* pScoringModule=pGameRules->GetScoringModule())
	{
		pScoringModule->OnTeamScoringEvent(teamId, EGRST_CarryObjectiveRetrieved);
		pScoringModule->OnPlayerScoringEvent(pickup->m_extractedBy, EGRST_CarryObjectiveRetrieved);
	}

	//Score per round is extracted ticks
	int newTeamRoundScore = pGameRules->GetTeamRoundScore(teamId);
	pGameRules->SetTeamRoundScore(teamId, ++newTeamRoundScore);

	if (IGameRulesVictoryConditionsModule *pVictoryModule=pGameRules->GetVictoryConditionsModule())
	{
		pVictoryModule->TeamCompletedAnObjective(teamId);
	}

	// RMI sent to depend on the carrier as the pickupEntity has just been deleted
	// may want to change this to a single entity RMI
	CGameRules::SModuleRMITwoEntityParams params;
	params.m_listenerIndex = m_moduleRMIIndex;
	params.m_entityId1 = pickup->m_extractedBy;
	params.m_entityId2 = pickupEntityId;
	params.m_data = eRMITypeDoubleEntity_pickup_extracted;
	// RMI will patch entityIDs between clients
	// this RMI will depend on the pickup entity existing on the clients before being sent
	pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), params, eRMI_ToRemoteClients, pickup->m_carrierId);

		// bonus score if retrieve carry objective whilst carrying another
//		int  numEntities = m_entities.size();
//		for (int i=0; i<numEntities; i++)
//		{
//			if (m_entities[i].m_carrierId == insideId)
//			{
//				pScoringModule->OnPlayerScoringEvent(insideId, EGRST_CarryObjectiveRetrieved);
//				break;
//			}
//		}
//	}
}

void CGameRulesObjective_Extraction::PlayerDropsPickupCommon(EntityId playerId, SPickup *pickup, bool newDrop)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	DbgLog("CGameRulesObjective_Extraction::PlayerDropsPickupCommon() playerId=%d", playerId);

	CRY_ASSERT_MESSAGE(playerId == pickup->m_carrierId, "PlayerDropsPickupCommon() requires the player doing the dropping to already be the carrier, and it isn't!!");
	if (pickup->m_state == ePickupState_Carried)	// new joining clients want to have their state updated but have no attachment to actually drop
	{
		g_pGame->GetGameRules()->OnPickupEntityDetached(pickup->m_spawnedPickupEntityId, pickup->m_carrierId, false, pickup->m_entityClass->GetName());
	}
	const EntityId carrierIdWas = pickup->m_carrierId;
	pickup->SetState(ePickupState_Dropped);
	pickup->m_droppedBy = pickup->m_carrierId;
	pickup->m_carrierId = 0;
	pickup->m_timeSinceDropped = 0.f;
	pickup->m_timeToCheckForbiddenArea = 0.f;
	
	DbgLog("[icons] CGameRulesObjective_Extraction::PlayerDropsPickupCommon: calling SetIconForAllPickups()");
	SetIconForAllPickups();
	DbgLog("[icons] CGameRulesObjective_Extraction::PlayerDropsPickupCommon: calling SetIconForAllExtractionPoints()");
	SetIconForAllExtractionPoints();
	//SetIconForPickup(pickup);
	
#if 0
	if (gEnv->IsClient())
	{
		// TODO - disable this snapping to floor code if physics is set and we get physicalisation of the ticks working

		// place tick onto the floor
		IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
		
		// TODO - move into class
		// copied from tunneling grenade
		static uint32 PROBE_TYPES = ent_terrain|ent_static|ent_sleeping_rigid|ent_rigid;
		static uint32 PROBE_FLAGS = ((geom_colltype_ray/*|geom_colltype13*/)<<rwi_colltype_bit)|rwi_colltype_any|rwi_force_pierceable_noncoll|rwi_ignore_solid_back_faces|8;
		static const float testDistance = 2.0f;

		IEntity *tickEntity = gEnv->pEntitySystem->GetEntity(pickup->m_spawnedPickupEntityId);
		const Vec3 &pos = tickEntity->GetPos();

#if 0
		// linetest
		Vec3 dir;
		dir.Set(0.f, 0.f, -testDistance);

		ray_hit hit;

		if ( pWorld->RayWorldIntersection(pos, dir, PROBE_TYPES, PROBE_FLAGS, &hit, 1) )
		{
			tickEntity->SetPos(hit.pt);
		}
#else
		// swept sphere
		static const float r = 0.3f;
		primitives::sphere sphere;
		sphere.center.Set(pos.x, pos.y, pos.z + 0.5f);
		sphere.r = r;

		Vec3 end = sphere.center;
		end.z -= testDistance;

		geom_contact *pContact = 0;
		float dst = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, end-sphere.center, PROBE_TYPES,
			&pContact, 0, PROBE_FLAGS, 0, 0, 0);

		if(dst>0.001f)
		{
			Vec3 newPos = pContact->center;
			AABB bounds;
			tickEntity->GetLocalBounds(bounds);
				
			Vec3 boundsSize = bounds.GetSize();

			newPos.z += boundsSize.z * 0.25f; // move tick out of the floor
			tickEntity->SetPos(newPos);
		}
#endif

	}
#endif
	PhysicalizeEntityForPickup(pickup, true);

	if (newDrop)
	{
		EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
		int localTeamId = pGameRules->GetTeam(localActorId);
		int playerTeamId = pGameRules->GetTeam(carrierIdWas);
		bool localPlayerPlayerSameTeam = (localTeamId == playerTeamId);
		IEntity *playerEntity = gEnv->pEntitySystem->GetEntity(playerId);

		CryFixedStringT<DISPLAY_NAME_LENGTH> displayName("");
		if( CGameLobby* pLobby = g_pGame->GetGameLobby() )
		{
			pLobby->GetPlayerDisplayNameFromEntity( playerId, displayName );
		}
		else
		{
			displayName = playerEntity ? playerEntity->GetName() : "NULL ENTITY";	
		}

		const bool  isLocalActor = (gEnv->IsClient() && (playerId == g_pGame->GetIGameFramework()->GetClientActorId()));

		const char *pickupString = GetPickupString(pickup);	
		string pickupStringCache = pickupString;

		if (localPlayerPlayerSameTeam)
		{
			CAudioSignalPlayer::JustPlay(m_droppedFriendlySignal);
		}
		else
		{
			CAudioSignalPlayer::JustPlay(m_droppedHostileSignal);
		}

		SHUDEventWrapper::OnGameStateMessage(playerId, localPlayerPlayerSameTeam, m_hasDropped.c_str(), eBLIF_Tick);
	}

	UpdateGameStateText(eGameStateUpdate_TickDropped, pickup);
}

void CGameRulesObjective_Extraction::PlayerDropsPickup(EntityId playerId, SPickup *pickup)
{
	DbgLog("CGameRulesObjective_Extraction::PlayerDropsPickup() playerId=%d", playerId);

	PlayerDropsPickupCommon(playerId, pickup, true);

	CGameRules::SModuleRMITwoEntityParams params;
	params.m_listenerIndex = m_moduleRMIIndex;
	params.m_entityId1 = playerId;
	params.m_entityId2 = pickup->m_spawnedPickupEntityId;
	params.m_data = eRMITypeDoubleEntity_pickup_dropped;


	// RMI will patch entityIDs between clients
	// this RMI will depend on the pickup entity existing on the clients before being sent
	g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), params, eRMI_ToRemoteClients, pickup->m_spawnedPickupEntityId);
}

void CGameRulesObjective_Extraction::PickupReturnsCommon(SPickup *pickup, IEntity *pickupEntity, IEntity *spawnedAtEntity, bool wasExtracted)
{
	//IEntity *spawnedAtEntity = gEnv->pEntitySystem->GetEntity(pickup->m_spawnAt);
	//IEntity *pickupEntity = gEnv->pEntitySystem->GetEntity(pickup->m_spawnedPickupEntityId);

	CRY_ASSERT(pickup);
	CRY_ASSERT(pickupEntity);
	CRY_ASSERT(spawnedAtEntity);

	const Vec3 &spawnAtPos = spawnedAtEntity->GetPos();
	Vec3 returnPos = spawnAtPos + m_spawnOffset;

	// offsets if required - will need them on actual original spawning position as well
	DbgLog("CGameRulesObjective_Extraction::PickupReturnsCommon() has found a dropped pickup %d (%s) that has been dropped for longer than droppedRemoveTime=%d; so returning it to the base it was originally spawned at %d (%s) at position=%f, %f, %f", pickup->m_spawnedPickupEntityId, pickupEntity ? pickupEntity->GetName() : "NULL", m_droppedRemoveTime/1000, pickup->m_spawnAt, spawnedAtEntity ? spawnedAtEntity->GetName() : "NULL", returnPos.x, returnPos.y, returnPos.z);
	pickupEntity->SetPos(returnPos); // this shouldn't be needed but the attachment system doesn't do well with the thing being attached to not currently being rendered. For now, use this to snap it to within reasonable location, as any player actually approaches and the device properly renders the pickup will then get correctly attached
	pickup->SetState(ePickupState_AtBase);

	pickup->m_droppedDefenders.clear();

	// may not need to do this for all when pickup is returning.. but for consistency do do
	DbgLog("[icons] CGameRulesObjective_Extraction::PickupReturnsCommon: calling SetIconForAllPickups()");
	SetIconForAllPickups();
	DbgLog("[icons] CGameRulesObjective_Extraction::PickupReturnsCommon: calling SetIconForAllExtractionPoints()");
	SetIconForAllExtractionPoints();
	//SetIconForPickup(pickup);

	IEntity *pSpawnAtEntity = gEnv->pEntitySystem->GetEntity(pickup->m_spawnAt);
	AttachToDevice(pSpawnAtEntity, pickup->m_spawnedPickupEntityId, true);

	CallEntityScriptFunction(pickup->m_spawnAt, "HasTick", true);

	// This will only be able to be tested when we're able to drop the pickup without dying!
	DbgLog("CGameRulesObjective_Extraction::Update() has found a dropped pickup so clearing out its carriers so everyone can pick it up");
	pickup->m_carriers.clear();

	PhysicalizeEntityForPickup(pickup, false);

	if (!wasExtracted)
	{
		EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
		CGameRules *pGameRules = g_pGame->GetGameRules();
		int localTeamId = pGameRules->GetTeam(localActorId);
		int pickupTeamId = pGameRules->GetTeam(pickup->m_spawnedPickupEntityId); // pickup->m_teamAffected will be the defending team
		bool localPlayerPickupSameTeam = (localTeamId == pickupTeamId);

		const char *pickupString = GetPickupString(pickup);
		string pickupStringCache = pickupString;

		IGameRulesRoundsModule*  pRoundsModule = pGameRules->GetRoundsModule();
		int primaryTeamId = pRoundsModule->GetPrimaryTeam(); 
		if (pickupTeamId == primaryTeamId && !m_textMessageReturnedAttacker.empty()) // attacking team
		{
			SHUDEventWrapper::GameStateNotify(m_textMessageReturnedAttacker.c_str(), m_returnedAttackerSignal, pickupStringCache.c_str());
		}
		else if (pickupTeamId != primaryTeamId && !m_textMessageReturnedDefender.empty())
		{
			SHUDEventWrapper::GameStateNotify(m_textMessageReturnedDefender.c_str(), m_returnedDefenderSignal, pickupStringCache.c_str());
		}
	}

	UpdateGameStateText(eGameStateUpdate_TickReturned, pickup);
}

void CGameRulesObjective_Extraction::OnEntityEvent( IEntity *pEntity, const SEntityEvent& event )
{
	if (gEnv->IsClient() && event.event == ENTITY_EVENT_DONE)
	{
		SPickup *pickup = GetPickupForPickupEntityId(pEntity->GetId());

		DbgLog("OnEntityEvent() pEntity=%s; event.event=%d; pickup=%p;", pEntity->GetName(), event.event, pickup);

		if (pickup)
		{
			DbgLog("CGameRulesObjective_Extraction::OnEntityEvent() received ENTITY_EVENT_DONE for a pickup entity - removing the entity from pickup and resetting icon");
			CRY_ASSERT_MESSAGE(pickup->m_spawnedPickupEntityId == pEntity->GetId(), "OnEntityEvent() EVENT_DONE trying to remove the hud icons for a removed pickup entity but the pickup's entityId and pEntity aren't the same");
			
			if(pickup->m_spawnedPickupEntityId && pickup->m_carrierId)
			{
				g_pGame->GetGameRules()->OnPickupEntityDetached(pickup->m_spawnedPickupEntityId, pickup->m_carrierId, true, pickup->m_entityClass->GetName());
			}

			pickup->m_spawnedPickupEntityId=0;

			const EntityId entityId = pEntity->GetId();
			gEnv->pEntitySystem->RemoveEntityEventListener(entityId, ENTITY_EVENT_DONE, this);
		}
	}

	if (gEnv->bServer && (pEntity->GetClass() == m_extractAtEntityClass) && (event.event == ENTITY_EVENT_ENTERAREA))
	{
		// original proximity pickup collecting
		EntityId entityEnteredId = (EntityId) event.nParam[0];
		IEntity *entityEntered = gEnv->pEntitySystem->GetEntity(entityEnteredId);

		DbgLog("OnEntityEvent() Server - ENTERAREA entity=%p (%s) has entered pEntity=%p (%s);", entityEntered, entityEntered ? entityEntered->GetName() : "NULL", pEntity, pEntity ? pEntity->GetName() : "NULL");

		if (pEntity->GetClass() == m_extractAtEntityClass)
		{
			DbgLog("OnEntityEvent() ENTERAREA entity is an extraction site");
			CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityEnteredId));
			if (pActor && pActor->IsPlayer() && !pActor->IsDead() && pActor->GetSpectatorState() == CActor::eASS_Ingame && pActor->GetSpectatorMode() == CActor::eASM_None)
			{
				DbgLog("OnEntityEvent() ENTERAREA entity entered is a valid player");
				// testing for the pickup means we dont need to test on teams, player wont be carrying unless he's on the right team
				SPickup *pickup = GetPickupForCarrierEntityId(entityEnteredId);
				if (pickup)
				{
					DbgLog("OnEntityEvent() ENTERAREA entity entered is a carrying player");
					PlayerExtractsPickup(entityEnteredId, pickup, pEntity);
				}
			}
		}
	}
	else if ((pEntity->GetClass() == m_spawnAtEntityClass)
		&& ((event.event == ENTITY_EVENT_ENTERAREA) || (event.event == ENTITY_EVENT_LEAVEAREA)))								
	{
		EntityId entityEnteredId = (EntityId) event.nParam[0];
		CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityEnteredId));
		if (pActor && pActor->IsPlayer() && !pActor->IsDead() && pActor->GetSpectatorState() == CActor::eASS_Ingame && pActor->GetSpectatorMode() == CActor::eASM_None)
		{
			DbgLog("OnEntityEvent() ENTERAREA entity entered is a valid player");

			CGameRules *pGameRules = g_pGame->GetGameRules();
			IGameRulesRoundsModule*  pRoundsModule = pGameRules->GetRoundsModule();
			CRY_ASSERT(pRoundsModule);
			if (pRoundsModule)
			{
				if (pRoundsModule->GetPrimaryTeam() == pGameRules->GetTeam(entityEnteredId))
				{
					for (TSpawnAtElements::iterator iter = m_spawnAtElements.begin(); iter != m_spawnAtElements.end(); ++iter)
					{
						if (iter->m_entityId == pEntity->GetId())
						{
							if ((event.event == ENTITY_EVENT_ENTERAREA))
							{
								iter->m_numInProximity++;
								if (iter->m_numInProximity == 1)
								{
									CallEntityScriptFunction(iter->m_entityId, "InProximity", true);
								}
							}
							else 	
							{
								iter->m_numInProximity--;
								if (iter->m_numInProximity == 0)
								{
									CallEntityScriptFunction(iter->m_entityId, "InProximity", false);
								}
							}
							break;
						}
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
bool CGameRulesObjective_Extraction::CheckForAndDropItem( EntityId playerId )
{
	bool dropped = false;

	DbgLog("CGameRulesObjective_Extraction::CheckForAndDropItem() playerId=%d", playerId);

	SPickup *pickup = GetPickupForCarrierEntityId(playerId);
	if (pickup)
	{
		DbgLog("found that player is carrying a pickup - drop the pickup");
		PlayerDropsPickup(playerId, pickup);
		dropped=true;
	}

	return dropped;
}

void CGameRulesObjective_Extraction::OnEntityKilledEarly(const HitInfo &hitInfo)
{
	DbgLog("CGameRulesObjective_Extraction::OnEntityKilledEarly()");

	if (gEnv->bServer)
	{
		SPickup *pickup = GetPickupForCarrierEntityId(hitInfo.targetId);
		if (pickup)
		{
			DbgLog("CGameRulesObjective_Extraction::OnEntityKilledEarly() has found a carrier has been killed.");
			
			if (hitInfo.shooterId && hitInfo.shooterId != hitInfo.targetId)
			{
				DbgLog("CGameRulesObjective_Extraction::OnEntityKilledEarly() has found a carrier has been killed, and that there was a killer entity. Awarding killer with bonus points");

				if (IGameRulesScoringModule* pScoringModule=g_pGame->GetGameRules()->GetScoringModule())
				{
					pScoringModule->OnPlayerScoringEvent(hitInfo.shooterId, EGRST_CarryObjectiveCarrierKilled);
				}
			}
		}
	}

	CheckForAndDropItem(hitInfo.targetId);
}



void CGameRulesObjective_Extraction::OnEntityKilled( const HitInfo &hitInfo )
{
	DbgLog("CGameRulesObjective_Extraction::OnEntityKilled()");

	if (gEnv->bServer)
	{
		SPickup *pickup = GetPickupForCarrierEntityId(hitInfo.targetId);
		if (pickup)
		{
			DbgLog("CGameRulesObjective_Extraction::OnEntityKilled() has found a carrier has been killed.");
			
			if (hitInfo.shooterId && hitInfo.shooterId != hitInfo.targetId)
			{
				DbgLog("CGameRulesObjective_Extraction::OnEntityKilled() has found a carrier has been killed, and that there was a killer entity. Awarding killer with bonus points");

				if (IGameRulesScoringModule* pScoringModule=g_pGame->GetGameRules()->GetScoringModule())
				{
					pScoringModule->OnPlayerScoringEvent(hitInfo.shooterId, EGRST_CarryObjectiveCarrierKilled);
				}
			}
		}

		// this is too late to drop the item. It's already been dropped by the inventory system at this point
		// which is bad if you were using the magic hammer
		// CheckForAndDropItem(hitInfo.targetId);

		const size_t numPickups = m_pickups.size();
		for (size_t i = 0; i < numPickups; i++)
		{
			SPickup* pickupElement = &m_pickups[i];

			int index=FindCarrierInPickupCarriers(pickupElement, hitInfo.targetId);
			if (index >= 0)
			{
				DbgLog("CGameRulesObjective_Extraction::OnEntityKilled() has found the victim in a pickup's list of carriers. Removing now he's died");
				pickupElement->m_carriers.removeAt(index);
			}
		}

		CGameRules *pGameRules = g_pGame->GetGameRules();
		IGameRulesRoundsModule*  pRoundsModule = pGameRules->GetRoundsModule();
		CRY_ASSERT(pRoundsModule);
		if (pRoundsModule)
		{
			int  primaryTeam = pRoundsModule->GetPrimaryTeam();	// attacking
			int secondaryTeam = 3 - primaryTeam; 								// defending
			int shooterTeamId = pGameRules->GetTeam(hitInfo.shooterId);
			int targetTeamId = pGameRules->GetTeam(hitInfo.targetId);

			IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(hitInfo.targetId);

			if(pActor && shooterTeamId != targetTeamId)
			{
				if (IGameRulesScoringModule* pScoringModule = pGameRules->GetScoringModule())
				{
					if (shooterTeamId == primaryTeam)
					{
						DbgLog("CGameRulesObjective_Extraction::OnEntityKilled() shooter was an attacker, adding attacking kill");
						const int killPoints = pScoringModule->GetPlayerPointsByType(EGRST_Extraction_AttackingKill);
						SGameRulesScoreInfo  scoreInfo (EGRST_Extraction_AttackingKill, killPoints);
						scoreInfo.AttachVictim(hitInfo.targetId);
						pScoringModule->OnPlayerScoringEventWithInfo(hitInfo.shooterId, &scoreInfo);
					}
					else if (shooterTeamId == secondaryTeam)
					{
						DbgLog("CGameRulesObjective_Extraction::OnEntityKilled() shooter was a defender, adding defending kill");
						const int killPoints = pScoringModule->GetPlayerPointsByType(EGRST_Extraction_DefendingKill);
						SGameRulesScoreInfo  scoreInfo (EGRST_Extraction_DefendingKill, killPoints);
						scoreInfo.AttachVictim(hitInfo.targetId);
						pScoringModule->OnPlayerScoringEventWithInfo(hitInfo.shooterId, &scoreInfo);
					}
					else
					{
						DbgLog("CGameRulesObjective_Extraction::OnEntityKilled() but the shooter team=%d isn't the attacking=%d or defending team=%d", shooterTeamId, primaryTeam, secondaryTeam);
						CRY_ASSERT_MESSAGE(0, string().Format("OnEntityKilled() but the shooter team=%d isn't the attacking=%d or defending team=%d", shooterTeamId, primaryTeam, secondaryTeam));
					}
				}
			}
		}
	}
}

void CGameRulesObjective_Extraction::OnClientDisconnect(int channelId, EntityId playerId)
{
	if (gEnv->bServer)
	{
		CheckForAndDropItem(playerId);
	}
}

void CGameRulesObjective_Extraction::OnClientEnteredGame(int channelId, bool isReset, EntityId playerId)
{
	DbgLog("CGameRulesObjective_Extraction::OnClientEnteredGame()");
	EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
	if (gEnv->bServer)
	{
		int  plyrTeam = g_pGame->GetGameRules()->GetTeam(playerId);

		int numPickups=m_pickups.size();
		int numExtractAts=m_extractAtElements.size();

		if (playerId == localActorId)
		{
			DbgLog("OnClientEnteredGame() with the local client");

			DbgLog("[icons] CGameRulesObjective_Extraction::OnClientEnteredGame: calling SetIconForAllPickups()");
			SetIconForAllPickups();
			DbgLog("[icons] CGameRulesObjective_Extraction::OnClientEnteredGame: calling SetIconForAllExtractionPoints()");
			SetIconForAllExtractionPoints();
		}
		else
		{
			DbgLog("OnClientEnteredGame() with a remote client");

			for (int i=0; i<numExtractAts; i++)
			{
				SExtractAt &extractAt = m_extractAtElements[i];

				// tell clients about new extraction point
				CGameRules::SModuleRMIEntityParams params(m_moduleRMIIndex, extractAt.m_entityId, eRMITypeSingleEntity_extraction_point_added);
				g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), params, eRMI_ToClientChannel|eRMI_NoLocalCalls, extractAt.m_entityId, channelId);		
			}

			for (int i=0; i<numPickups; i++)
			{
				SPickup *pickup = &m_pickups[i];

				//if (pickup->m_teamAffected == plyrTeam)
				//{
				//	ApplyPickupEffectToPlayer(pickup, playerId);
				//}

				if (pickup->m_state == ePickupState_Extracted)
				{
					DbgLog("OnClientEnteredGame() found a pickup thats extracted, not telling clients about this pickup, its entity doesn't exist anymore");
				}
				else
				{
					DbgLog("OnClientEnteredGame() found a pickup thats NOT extracted, telling clients about this pickup");

					ERMITypes rmiType=GetSpawnedPickupRMITypeFromSuitMode(pickup->m_suitModeEffected);
					if (rmiType != eRMIType_None)
					{
						// tell clients about new pickup (and who spawned it)
						CGameRules::SModuleRMITwoEntityParams params;
						params.m_listenerIndex = m_moduleRMIIndex;
						params.m_entityId1 = pickup->m_spawnedPickupEntityId;
						params.m_entityId2 = pickup->m_spawnAt;
						params.m_data = rmiType;

						g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), params, eRMI_ToClientChannel|eRMI_NoLocalCalls, pickup->m_spawnedPickupEntityId, channelId);

						DbgLog("OnClientEnteredGame() telling new client about magic hammer fallbackGun pickupEntityId=%d; magicHammerId=%d", pickup->m_spawnedPickupEntityId, pickup->m_spawnedFallbackGunId);
						// tell the client about this magic hammer - RMIs are ordered so this will get sent before any carried RMIs are below
						CGameRules::SModuleRMITwoEntityParams params2;
						params2.m_listenerIndex = m_moduleRMIIndex;
						params2.m_entityId1 = pickup->m_spawnedPickupEntityId;
						params2.m_entityId2 = pickup->m_spawnedFallbackGunId;
						params2.m_data = eRMITypeDoubleEntity_fallback_gun_spawned;

						// spawns are unordered over the network but the RMIs are ordered. So depending on the hammer here, the spawned tick above will have arrived as it will block the RMI above
						g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), params2, eRMI_ToClientChannel|eRMI_NoLocalCalls, pickup->m_spawnedFallbackGunId, channelId);
					}
				}
			}

			for (int i=0; i<numPickups; i++)
			{
				SPickup *pickup = &m_pickups[i];

				if (pickup->m_state == ePickupState_Dropped)
				{
					DbgLog("CGameRulesObjective_Extraction::OnClientEnteredGame() pickup being dropped sending RMI to connecting client");

					// tell incoming client about the pickup being dropped
					CGameRules::SModuleRMITwoEntityParams params;
					params.m_listenerIndex = m_moduleRMIIndex;
					params.m_entityId1 = pickup->m_droppedBy;
					params.m_entityId2 = pickup->m_spawnedPickupEntityId;
					params.m_data = eRMITypeDoubleEntity_pickup_already_dropped;
					// RMI will patch entityIDs between clients
					// this RMI will depend on the pickup entity existing on the clients before being sent
					g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), params, eRMI_ToClientChannel|eRMI_NoLocalCalls, pickup->m_spawnedPickupEntityId, channelId);
				}
				else if (pickup->m_state == ePickupState_Carried)
				{
					DbgLog("CGameRulesObjective_Extraction::OnClientEnteredGame() pickup being carried sending RMI to connecting client");

					// tell incoming client about the pickup being carried
					CGameRules::SModuleRMITwoEntityParams params;
					params.m_listenerIndex = m_moduleRMIIndex;
					params.m_entityId1 = pickup->m_carrierId;
					params.m_entityId2 = pickup->m_spawnedPickupEntityId;
					params.m_data = eRMITypeDoubleEntity_pickup_already_collected;
					// RMI will patch entityIDs between clients
					// this RMI will depend on the pickup entity existing on the clients before being sent
					g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), params, eRMI_ToClientChannel|eRMI_NoLocalCalls, pickup->m_spawnedPickupEntityId, channelId);
				}
			}
		}
	}
}

// For clients their localPlayer's team isn't set by the time this is called!!!!
// rely on OnChangedTeam() instead
void CGameRulesObjective_Extraction::OnOwnClientEnteredGame()
{
	DbgLog("CGameRulesObjective_Extraction::OnOwnClientEnteredGame()");
}

// from servers to clients
void CGameRulesObjective_Extraction::OnSingleEntityRMI(CGameRules::SModuleRMIEntityParams params)
{
	DbgLog("CGameRulesObjective_Extraction::OnSingleEntityRMI() rmiType=%d", params.m_data);
	switch(params.m_data)
	{
		case eRMITypeSingleEntity_extraction_point_added:
		{
			DbgLog("CGameRulesObjective_Extraction::OnSingleEntityRMI() - extraction point added");
			SExtractAt extractAt;
			extractAt.m_entityId = params.m_entityId;
			stl::push_back_unique(m_extractAtElements, extractAt);
			DbgLog("[icons] CGameRulesObjective_Extraction::OnSingleEntityRMI: calling SetIconForExtractionPoint(entityId=%d)", params.m_entityId);
			SetIconForExtractionPoint(params.m_entityId);
			break;
		}
		default:
			CRY_ASSERT_MESSAGE(0, string().Format("OnSingleEntityRMI() unknown rmitype=%d", params.m_data));
	}
}

void CGameRulesObjective_Extraction::OnPickupSpawned(EExtractionSuitMode suitMode, EntityId pickupEnt, EntityId spawnAtEnt)
{
	SPickup *pickup = GetPickupForSuitMode(suitMode);
	CRY_ASSERT_TRACE(pickup, ("OnDoubleEntityRMI() failed to find pickup for suitmode %d", suitMode));
	if (pickup)
	{
		if (pickup->m_spawnedPickupEntityId)
		{
			gEnv->pEntitySystem->RemoveEntityEventListener(pickup->m_spawnedPickupEntityId, ENTITY_EVENT_DONE, this);
		}

		DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() - pickup spawned type: %d", suitMode);
		pickup->m_spawnedPickupEntityId = pickupEnt;
		pickup->m_spawnAt = spawnAtEnt;
		pickup->SetState(ePickupState_AtBase);
		DbgLog("[icons] CGameRulesObjective_Extraction::OnPickupSpawned: calling SetIconForPickup(%p) [entityId=%d]", pickup, pickupEnt);
		SetIconForPickup(pickup);		// shouldn't need to do a set icon for all here.. some will not be setup early on anyway

		IGameObject *pGameObject = g_pGame->GetIGameFramework()->GetGameObject(pickupEnt);
		if (pGameObject != NULL && spawnAtEnt)
		{
			pGameObject->SetNetworkParent(spawnAtEnt);
		}

		IEntity *pSpawnAtEntity = gEnv->pEntitySystem->GetEntity(spawnAtEnt);
		AttachToDevice(pSpawnAtEntity, pickupEnt, true);

		gEnv->pEntitySystem->AddEntityEventListener(pickupEnt, ENTITY_EVENT_DONE, this);
	}
}

// from servers to clients
void CGameRulesObjective_Extraction::OnDoubleEntityRMI(CGameRules::SModuleRMITwoEntityParams params)
{
	// TODO - as more things use this add enum for what the message actually is
	DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() - rmiType=%d", params.m_data);
	switch(params.m_data)
	{
		case eRMITypeDoubleEntity_power_pickup_spawned:
		{
			OnPickupSpawned(eExtractionSuitMode_Power, params.m_entityId1, params.m_entityId2);
			break;
		}
		case eRMITypeDoubleEntity_armour_pickup_spawned:
		{
			OnPickupSpawned(eExtractionSuitMode_Armour, params.m_entityId1, params.m_entityId2);
			break;
		}
		case eRMITypeDoubleEntity_stealth_pickup_spawned:
		{
			OnPickupSpawned(eExtractionSuitMode_Stealth, params.m_entityId1, params.m_entityId2);
			break;
		}

		case eRMITypeDoubleEntity_fallback_gun_spawned:
		{
			DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() received RMI eRMITypeDoubleEntity_fallback_gun_spawned");
			NewFallbackGunForPickup(params.m_entityId1, params.m_entityId2);
			break;
		}

		case eRMITypeDoubleEntity_pickup_already_collected:
		{
			DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() eRMITypeDoubleEntity_pickup_already_collected");

			SPickup *pickup = GetPickupForPickupEntityId(params.m_entityId2);
			if (pickup)
			{
				DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() is a pickup collected RMI - picking up entity");
				PlayerCollectsPickupCommon(params.m_entityId1, pickup, false);
			}
			else
			{
				CryLog("OnDoubleEntityRMI() - pickup collected - failed to find pickup for collected entity - relying on additional RMIs sent from Server in OnClientEnteredGame() to happen later to sync us up");
			}
			break;
		}
		case eRMITypeDoubleEntity_pickup_collected:
		{
			DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() eRMITypeDoubleEntity_pickup_collected");

			SPickup *pickup = GetPickupForPickupEntityId(params.m_entityId2);
			if (pickup)
			{
				DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() is a pickup collected RMI - picking up entity");
				PlayerCollectsPickupCommon(params.m_entityId1, pickup, true);
			}
			else
			{
				CryLog("OnDoubleEntityRMI() - pickup collected - failed to find pickup for collected entity - relying on additional RMIs sent from Server in OnClientEnteredGame() to happen later to sync us up");
			}
			break;
		}
		case eRMITypeDoubleEntity_pickup_already_dropped:
		{
			DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() eRMITypeDoubleEntity_pickup_already_dropped");

			SPickup *pickup = GetPickupForPickupEntityId(params.m_entityId2);
			if (pickup)
			{
				DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() is a pickup being dropped - dropping it");

				PlayerDropsPickupCommon(params.m_entityId1, pickup, false);
			}
			else
			{
				CryLog("OnDoubleEntityRMI() - pickup dropped - failed to find pickup for dropped entity - relying on additional RMIs sent from Server in OnClientEnteredGame() to happen later to sync us up");
			}

			break;
		}
		case eRMITypeDoubleEntity_pickup_dropped:
		{
			DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() eRMITypeDoubleEntity_pickup_dropped");

			SPickup *pickup = GetPickupForPickupEntityId(params.m_entityId2);
			if (pickup)
			{
				DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() is a pickup being dropped - dropping it");

				PlayerDropsPickupCommon(params.m_entityId1, pickup, true);
			}
			else
			{
				CryLog("OnDoubleEntityRMI() - pickup dropped - failed to find pickup for dropped entity - relying on additional RMIs sent from Server in OnClientEnteredGame() to happen later to sync us up");
			}

			break;
		}
		case eRMITypeDoubleEntity_pickup_returned_to_base:
		{
			DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() eRMITypeDoubleEntity_pickup_returned_to_base");
			SPickup *pickup = GetPickupForPickupEntityId(params.m_entityId1);
			if (pickup)
			{
				DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() is a pickup being returned to base - returning it");
				IEntity *pickupEntity = gEnv->pEntitySystem->GetEntity(params.m_entityId1);
				IEntity *spawnedAtEntity = gEnv->pEntitySystem->GetEntity(params.m_entityId2);

				PickupReturnsCommon(pickup, pickupEntity, spawnedAtEntity, false);
			}
			else
			{
				CryLog("OnDoubleEntityRMI() - pickup dropped - failed to find pickup for returned to base entity - relying on additional RMIs sent from Server in OnClientEnteredGame() to happen later to sync us up");
			}
			break;
		}
		case eRMITypeDoubleEntity_pickup_extracted:
		{
			DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() eRMITypeDoubleEntity_pickup_extracted");

			SPickup *pickup = GetPickupForCarrierEntityId(params.m_entityId1);
			if (pickup)
			{
				DbgLog("CGameRulesObjective_Extraction::OnDoubleEntityRMI() eRMITypeDoubleEntity_pickup_extracted - updating state, unattaching pickup if possible");

				// this detach is likely to not succeed if its already been deleted
				PlayerExtractsPickupCommon(params.m_entityId1, pickup);
			}
			else
			{
				CryLog("OnDoubleEntityRMI() - pickup extracted - failed to find pickup for carrier entity - relying on additional RMIs sent from Server in OnClientEnteredGame() to happen later to sync us up");
			}
			break;
		}
		default:
			CRY_ASSERT_MESSAGE(0, string().Format("OnDoubleEntityRMI() unknown rmitype=%d", params.m_data));
	}
}

// from clients to servers
void CGameRulesObjective_Extraction::OnSvClientActionRMI(CGameRules::SModuleRMISvClientActionParams params, EntityId fromEid)
{
	CRY_ASSERT(gEnv->bServer);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules && (pGameRules->HasGameActuallyStarted() == false))
	{
		return;
	}

	DbgLog("CGameRulesObjective_Extraction::OnSvClientActionRMI() - action=%d", params.m_action);

	switch (params.m_action)
	{
		case CGameRules::SModuleRMISvClientActionParams::eACT_HelperCarry_Drop:
		{
			SPickup *pickup = GetPickupForCarrierEntityId(fromEid);
			DbgLog("CGameRulesObjective_Extraction::OnSvClientActionRMI() helperCarry_drop action - dropping pickup");
			CRY_ASSERT_MESSAGE(pickup, "OnSvClientActionRMI() dropping a pickup but no pickup found for carrier!!");
			if (pickup)
			{
				CheckForAndDropItem(fromEid);
			}
			break;
		}
		case CGameRules::SModuleRMISvClientActionParams::eACT_HelperCarry_Pickup:
		{
			DbgLog("CGameRulesObjective_Extraction::OnSvClientActionRMI() helperCarry_pickup action - picking up pickup");

			CRY_ASSERT(params.m_datau.helperCarryPickup.pickupEid);
			CPlayer* pFromPlayer=static_cast< CPlayer* >( gEnv->pGameFramework->GetIActorSystem()->GetActor(fromEid) );
			CRY_ASSERT(pFromPlayer);

			if (pFromPlayer->IsDead())
			{
				DbgLog("CGameRulesObjective_Extraction::OnSvClientActionRMI() has found that the player claiming to be picking up a pickup is actually dead. Denying this player from picking the pickup up");
			}
			else
			{
				DbgLog("CGameRulesObjective_Extraction::OnSvClientActionRMI() has found that the player picking up a pickup is alive");

				SPickup *pickup = GetPickupForPickupEntityId(params.m_datau.helperCarryPickup.pickupEid);
				CRY_ASSERT_MESSAGE(pickup, "OnSvClientActionRMI() collecting a pickup but failed to find a pickup for the item to be collected");
				if (pickup)
				{
					CRY_ASSERT_MESSAGE(pickup->m_state != ePickupState_Carried, "OnSvClientActionRMI() trying to collect a pickup but the pickup is already in state carried.. can't pick it up again!!!");
					if (pickup->m_state == ePickupState_Carried)
					{
						DbgLog("CGameRulesObjective_Extraction::OnSvClientActionRMI() trying to collect a pickup but the pickup is already in state carried.. can't pick it up again!!!");
					}
					else
					{
						CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(fromEid));

						if(pActor)
						{
							IItem* pItem = pActor->GetCurrentItem(false);
							CWeapon* pWeapon = pItem ? static_cast<CWeapon*>(pItem->GetIWeapon()) : NULL;

							if(!pWeapon || pWeapon->AllowInteraction(params.m_datau.helperCarryPickup.pickupEid, eInteraction_GameRulesPickup))
							{
								PlayerCollectsPickup(fromEid, pickup);
							}
						}
					}
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void CGameRulesObjective_Extraction::OnAction(const ActionId& action, int activationMode, float value)
{
	EntityId  clientEid = g_pGame->GetIGameFramework()->GetClientActorId();
	CRY_ASSERT(clientEid);

	if (CPlayer* pClientPlayer=static_cast< CPlayer* >( gEnv->pGameFramework->GetIActorSystem()->GetActor(clientEid) ))
	{
		CGameRules*  pGameRules = g_pGame->GetGameRules();
		IGameRulesSpectatorModule*  specmod = pGameRules->GetSpectatorModule();

		if (!specmod || (pClientPlayer->GetSpectatorMode() <= 0))
		{
			// Not in spectator mode

			if (pClientPlayer->IsDead() == false)
			{
				// Alive
				bool dropItem = false;

				if (action == g_pGame->Actions().use)
				{
					const float  curTime = gEnv->pTimer->GetAsyncCurTime();

					switch (activationMode)
					{
						case eAAM_OnPress:
						case eAAM_OnHold:
						{
							if(!m_useButtonHeld)
							{
								m_useButtonHeld = true;
								m_attemptPickup = false;

								const SInteractionInfo& interaction = pClientPlayer->GetCurrentInteractionInfo();
								SPickup *pickup = GetPickupForCarrierEntityId(clientEid);
								if (pickup && interaction.interactionType == eInteraction_GameRulesDrop)
								{
									dropItem = true;
									DbgLog("CGameRulesObjective_Extraction::OnAction() reload button pressed when we already have a pickup being carried.. dropping");
								}
								else
								{
									m_attemptPickup = true;
								}
							}
							break;
						}
						case eAAM_OnRelease:
						{
							m_useButtonHeld = false;
							m_attemptPickup = false;
							break;
						}
					}
				}
				else if ((action == g_pGame->Actions().toggle_weapon) && (activationMode == eAAM_OnPress))
				{
					dropItem = (GetPickupForCarrierEntityId(clientEid) != NULL);
				}

				if (dropItem)
				{
					if (gEnv->bServer)
					{
						CheckForAndDropItem(clientEid);
					}
					else
					{
						CGameRules::SModuleRMISvClientActionParams  params (m_moduleRMIIndex, CGameRules::SModuleRMISvClientActionParams::eACT_HelperCarry_Drop, NULL);
						g_pGame->GetGameRules()->GetGameObject()->InvokeRMI(CGameRules::SvModuleRMIOnAction(), params, eRMI_ToServer);
					}
				}
			}
		}
	}
}

void CGameRulesObjective_Extraction::SetIconForExtractionPoint(EntityId extractionPointEntityId)
{
	IGameRulesRoundsModule*  pRoundsModule = g_pGame->GetGameRules()->GetRoundsModule();
	SExtractAt *extractAt = GetExtractAtForEntityId(extractionPointEntityId);
	int  primaryTeam = pRoundsModule->GetPrimaryTeam();	// attacking
	int secondaryTeam = (primaryTeam == 1) ? 2 : 1;			// defending
	EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
	CGameRules *pGameRules=g_pGame->GetGameRules();
	int localTeamId = pGameRules->GetTeam(localActorId);
	int extractionPointTeamId = pGameRules->GetTeam(extractionPointEntityId);
	bool sameTeam=(localTeamId == extractionPointTeamId);
	const char *newIconName=m_iconTextExtraction;
	bool showIcon=false;
	bool localPlayerIsAnAttacker = (localTeamId == primaryTeam);
	bool localPlayerCarryingAPickup=false;
	bool anyPlayerCarryingAPickup=false;


	const char *localisedIconName = CHUDUtils::LocalizeString(newIconName);		
	CryFixedStringT<64> localisedIconNameCache = localisedIconName;

	int numPickups = m_pickups.size();
	for (int i=0; i<numPickups; i++)
	{
		SPickup *pickup = &m_pickups[i];
		if (pickup->m_state == ePickupState_Carried)
		{
			if (pickup->m_carrierId == localActorId)
			{
				localPlayerCarryingAPickup=true;
			}
			if (pickup->m_state == ePickupState_Carried)
			{
				anyPlayerCarryingAPickup=true;
			}
		}
	}

	if (localPlayerIsAnAttacker && localPlayerCarryingAPickup)
	{
		showIcon=true;
	}
	else if (!localPlayerIsAnAttacker && anyPlayerCarryingAPickup)
	{
		showIcon=true;
	}
	

	DbgLog("CGameRulesObjective_Extraction::SetIconForExtractionPoint()");
	
	EGameRulesMissionObjectives iconToUse=EGRMO_Unknown;
	if (showIcon)
	{
		if (sameTeam && (m_friendlyBaseIcon != EGRMO_Unknown))
		{
			DbgLog("CGameRulesObjective_Extraction::SetIconForExtractionPoint() - using friendly base icon");
			iconToUse = m_friendlyBaseIcon;
		}
		else if (!sameTeam && (m_hostileBaseIcon != EGRMO_Unknown))
		{
			DbgLog("CGameRulesObjective_Extraction::SetIconForExtractionPoint() - using hostile base icon");
			iconToUse = m_hostileBaseIcon;
		}
	}

	if (extractAt != NULL && iconToUse !=  extractAt->m_currentObjectiveIcon)
	{
		if (extractAt->m_currentObjectiveIcon != EGRMO_Unknown)
		{
			DbgLog("CGameRulesObjective_Extraction::SetIconForExtractionPoint() - is setting a new icon and removing an old one");
			SHUDEventWrapper::OnRemoveObjective(extractAt->m_entityId, 0);
		}

		if (iconToUse != EGRMO_Unknown)
		{
			SHUDEventWrapper::OnNewObjective(extractionPointEntityId, iconToUse, 0.f, 0, localisedIconNameCache.c_str(), "#666666");
		}

		extractAt->m_currentObjectiveIcon = iconToUse;
	}
}

void CGameRulesObjective_Extraction::SetIconForPickup(SPickup *pickup)
{
	EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
	CGameRules *pGameRules=g_pGame->GetGameRules();
	int localTeamId = pGameRules->GetTeam(localActorId);
	int pickupTeamId = pGameRules->GetTeam(pickup->m_spawnedPickupEntityId);
	bool localPlayerPickupSameTeam = (localTeamId == pickupTeamId);
	const char *newIconName=NULL;
	const char *newIconColour=NULL;

	const char *friendlyColour="#9AD5B7";
	const char *enemyColour="#AC0000";

	bool localPlayerCarryingAPickup=false;

	int numPickups = m_pickups.size();
	for (int i=0; i<numPickups; i++)
	{
		SPickup *p = &m_pickups[i];
		if (p->m_state == ePickupState_Carried)
		{
			if (p->m_carrierId == localActorId)
			{
				localPlayerCarryingAPickup=true;
				break;
			}
		}
	}

	EGameRulesMissionObjectives newObjectiveIcon=EGRMO_Unknown;
	EntityId newObjectiveFollowEntity=0;
	int newPriority=0;

	// when carrying a pickup you only want to see the extraction point
	if (!localPlayerCarryingAPickup)
	{
		switch(pickup->m_state)
		{
			case ePickupState_AtBase:
				CRY_ASSERT_TRACE(localTeamId && pickupTeamId, ("SetIconForPickup() requires player and pickup to both have teams set"));
				if (localPlayerPickupSameTeam)
				{
					if (pickup->m_suitModeEffected == eExtractionSuitMode_Armour)
					{
						newObjectiveIcon=m_armourFriendlyIconAtBase;
					}
					else if (pickup->m_suitModeEffected == eExtractionSuitMode_Stealth)
					{
						newObjectiveIcon=m_stealthFriendlyIconAtBase;
					}
					else
					{
						CRY_ASSERT_MESSAGE(0, "Extraction: unhandled pickup suitmode effected");
					}

					newIconName=m_iconTextDefend;
					newIconColour = friendlyColour;
				}
				else
				{
					if (pickup->m_suitModeEffected == eExtractionSuitMode_Armour)
					{
						newObjectiveIcon=m_armourHostileIconAtBase;
					}
					else if (pickup->m_suitModeEffected == eExtractionSuitMode_Stealth)
					{
						newObjectiveIcon=m_stealthHostileIconAtBase;
					}
					else
					{
						CRY_ASSERT_MESSAGE(0, "Extraction: unhandled pickup suitmode effected");
					}
					newIconName=m_iconTextSeize;
					newIconColour=enemyColour;
				}
				newObjectiveFollowEntity=pickup->m_spawnedPickupEntityId;
				break;
			case ePickupState_Carried:
			{
				IEntity *pickupEntity = gEnv->pEntitySystem->GetEntity(pickup->m_spawnedPickupEntityId);
				int carrierTeamId=pGameRules->GetTeam(pickup->m_carrierId);
				bool localPlayerPickupCarrierSameTeam = (localTeamId == carrierTeamId);
				CRY_ASSERT_TRACE(localTeamId && carrierTeamId, ("SetIconForPickup() requires player and carrier to both have teams set"));
				// TODO - on clients perhaps detect a pickup gone into state extraction by virtue of the pickup entity not resolving anymore (its deleted when extracted)
				// TODO - FIXME - this is not right test the team of the carrier with the local player to pick color
				if (pickupEntity)
				{
					if (pickup->m_carrierId == localActorId)
					{
						DbgLog("CGameRulesObjective_Extraction::SetIconForPickup() - state carried but local client is the carrier, NOT showing an icon for ourselves");
					}
					else
					{
						if (localPlayerPickupCarrierSameTeam)
						{
							if (pickup->m_suitModeEffected == eExtractionSuitMode_Armour)
							{
								newObjectiveIcon=m_armourFriendlyIconCarried;
							}
							else if (pickup->m_suitModeEffected == eExtractionSuitMode_Stealth)
							{
								newObjectiveIcon=m_stealthFriendlyIconCarried;
							}
							else
							{
								CRY_ASSERT_MESSAGE(0, "Extraction: unhandled pickup suitmode effected");
							}
							newIconName=m_iconTextEscort;
							newIconColour=friendlyColour;
						}
						else
						{
							if (pickup->m_suitModeEffected == eExtractionSuitMode_Armour)
							{
								newObjectiveIcon=m_armourHostileIconCarried;
							}
							else if (pickup->m_suitModeEffected == eExtractionSuitMode_Stealth)
							{
								newObjectiveIcon=m_stealthHostileIconCarried;
							}
							else
							{
								CRY_ASSERT_MESSAGE(0, "Extraction: unhandled pickup suitmode effected");
							}
							newIconName=m_iconTextKill;
							newIconColour=enemyColour;
						}
						newObjectiveFollowEntity=pickup->m_carrierId; // cannot follow item when its being carried, doesn't update properly
					}
				}
				else
				{
					if (!gEnv->bServer)
					{
						DbgLog("CGameRulesObjective_Extraction::SetIconForPickup() in state Carried and have detected that the pickupEntity doesn't resolve anymore. Its likely been deleted and we now need to be in state extracted");
						// don't have it actually change now that the RMI should properly take care of this
						//pickup->SetState(ePickupState_Extracted);
					}
				}
				break;
			}
			case ePickupState_Dropped:
				CRY_ASSERT_TRACE(localTeamId && pickupTeamId, ("SetIconForPickup() requires player and pickup to both have teams set"));
				if (localPlayerPickupSameTeam)
				{
					if (pickup->m_suitModeEffected == eExtractionSuitMode_Armour)
					{
						newObjectiveIcon=m_armourFriendlyIconDropped;
					}
					else if (pickup->m_suitModeEffected == eExtractionSuitMode_Stealth)
					{
						newObjectiveIcon=m_stealthFriendlyIconDropped;
					}
					else
					{
						CRY_ASSERT_MESSAGE(0, "Extraction: unhandled pickup suitmode effected");
					}
					newIconName=m_iconTextDefend;
					newIconColour=friendlyColour;
				}
				else 
				{
					if (pickup->m_suitModeEffected == eExtractionSuitMode_Armour)
					{
						newObjectiveIcon=m_armourHostileIconDropped;
					}
					else if (pickup->m_suitModeEffected == eExtractionSuitMode_Stealth)
					{
						newObjectiveIcon=m_stealthHostileIconDropped;
					}
					else
					{
						CRY_ASSERT_MESSAGE(0, "Extraction: unhandled pickup suitmode effected");
					}
					newIconName=m_iconTextSeize;
					newIconColour=enemyColour;
				}
				newObjectiveFollowEntity=pickup->m_spawnedPickupEntityId;
				break;
			case ePickupState_Extracted:
				// only currently known on the server so leave for now
				// we dont want any hud icons for this state
				newObjectiveIcon=EGRMO_Unknown;
				break;
			default:
				CRY_ASSERT_TRACE(0, ("SetIconForPickup() encountered unknown pickupstate=%d", pickup->m_state));
				break;
		}
	}

	if (newObjectiveIcon != pickup->m_currentObjectiveIcon ||
		  newObjectiveFollowEntity != pickup->m_currentObjectiveFollowEntity ||
			newPriority != pickup->m_currentObjectivePriority ||
			newIconName != pickup->m_currentIconName)
	{
		DbgLog("CGameRulesObjective_Extraction::SetIconForPickup() - is setting a new icon. localTeamId=%d; pickupTeamId=%d", localTeamId, pickupTeamId);

		if (pickup->m_currentObjectiveIcon != EGRMO_Unknown)
		{
			DbgLog("CGameRulesObjective_Extraction::SetIconForPickup() - is setting a new icon and removing an old one");
			SHUDEventWrapper::OnRemoveObjectiveWithRadarEntity(pickup->m_currentObjectiveFollowEntity, pickup->m_spawnedPickupEntityId, pickup->m_currentObjectivePriority);
		}

		if (newObjectiveIcon != EGRMO_Unknown)
		{
			const char *localisedIconName = CHUDUtils::LocalizeString(newIconName);
			CryFixedStringT<64>localisedIconNameCache = localisedIconName;
			assert(newIconColour);
			SHUDEventWrapper::OnNewObjectiveWithRadarEntity(newObjectiveFollowEntity, pickup->m_spawnedPickupEntityId, newObjectiveIcon, 0.f, newPriority, localisedIconNameCache.c_str(), newIconColour ? newIconColour : friendlyColour);
		}

		pickup->m_currentObjectiveIcon=newObjectiveIcon;
		pickup->m_currentObjectiveFollowEntity=newObjectiveFollowEntity;
		pickup->m_currentObjectivePriority=newPriority;
		pickup->m_currentIconName=newIconName;
	}
}

// needed now that pickup changes need to affect all icons
void CGameRulesObjective_Extraction::SetIconForAllExtractionPoints()
{
	const int numExtractAts = m_extractAtElements.size();

	for (int i = 0; i < numExtractAts; i ++)
	{
		SExtractAt &extractAt = m_extractAtElements[i];
		DbgLog("[icons] CGameRulesObjective_Extraction::SetIconForAllExtractionPoints: calling SetIconForExtractionPoint(entityId=%d) [extractAt idx %d]", extractAt.m_entityId, i);
		SetIconForExtractionPoint(extractAt.m_entityId);
	}
}

void CGameRulesObjective_Extraction::SetIconForAllPickups()
{
	const int numPickups = m_pickups.size();

	for (int i = 0; i < numPickups; i ++)
	{
		DbgLog("[icons] CGameRulesObjective_Extraction::SetIconForAllPickups: calling SetIconForPickup(pickup=%p) [entityId=%d]", &m_pickups[i], m_pickups[i].m_spawnedPickupEntityId);
		SetIconForPickup(&m_pickups[i]);
	}
}

void CGameRulesObjective_Extraction::UpdateGameStateText(EGameStateUpdate inUpdate, SPickup *inPickup)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	IGameRulesRoundsModule*  pRoundsModule = g_pGame->GetGameRules()->GetRoundsModule();

#ifndef _RELEASE
	const char *stringForPickup=inPickup ? GetPickupString(inPickup) : "NULL";
	if (!stringForPickup)
	{
		stringForPickup="Invalid pickup";
	}
	DbgLog("CGameRulesObjective_Extraction::UpdateGameStateText() inUpdate=%d; inPickup=%s", inUpdate, stringForPickup);
#endif

	CRY_ASSERT(pRoundsModule);
	CRY_ASSERT(pGameRules);
	if (!pRoundsModule)
	{
		DbgLog("CGameRulesObjective_Extraction::UpdateGameStateText() early exiting. No Rounds module");
		return;
	}

	int  primaryTeam = pRoundsModule->GetPrimaryTeam();	// attacking
	int secondaryTeam = (primaryTeam == 1) ? 2 : 1;			// defending
	EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
	CRY_ASSERT(localActorId);
	int localTeamId = pGameRules->GetTeam(localActorId);
	CRY_ASSERT(localTeamId);

	const char *localisedMessage=NULL;

	switch(inUpdate)
	{
		case eGameStateUpdate_Initialise:

			DbgLog("CGameRulesObjective_Extraction::UpdateGameStateText() inUpdate is eGameStateUpdate_Initialise - localTeamId=%d; primaryTeam=%d", localTeamId, primaryTeam);
			if (localTeamId == primaryTeam)
			{
				localisedMessage = CHUDUtils::LocalizeString(m_statusAttackersTicksSafe);		
			}
			else
			{
				bool pickupNeedsSecuring=false;
				int numPickups = m_pickups.size();
				for (int i=0; i<numPickups; i++)
				{
					SPickup *pickup = &m_pickups[i];
					if (pickup->m_state == ePickupState_Carried || pickup->m_state == ePickupState_Dropped)
					{
						pickupNeedsSecuring=true;
					}
				}

				DbgLog("CGameRulesObjective_Extraction::UpdateGameStateText() inUpdate is eGameStateUpdate_Initialise - am a defender - pickup needs securing=%d", pickupNeedsSecuring);
				if (pickupNeedsSecuring)
				{
					localisedMessage=CHUDUtils::LocalizeString(m_statusDefendersTickCarriedDropped);
				}
				else
				{
					localisedMessage=CHUDUtils::LocalizeString(m_statusDefendersTicksSafe);
				}
			}
			break;
		case eGameStateUpdate_TickCollected:
			CRY_ASSERT_MESSAGE(inPickup, "UpdateGameStateText() tick collected update needs a valid inPickup");
			if (localTeamId == primaryTeam)
			{
				if (inPickup && inPickup->m_carrierId == localActorId)
				{
					localisedMessage = CHUDUtils::LocalizeString(m_statusAttackerCarryingTick);		
				}
			}
			else
			{
				localisedMessage = CHUDUtils::LocalizeString(m_statusDefendersTickCarriedDropped);		
			}
			break;
		case eGameStateUpdate_TickDropped:
			CRY_ASSERT_MESSAGE(inPickup, "UpdateGameStateText() tick dropped update needs a valid inPickup");
			if (localTeamId == primaryTeam)
			{
				if (inPickup && inPickup->m_droppedBy == localActorId)
				{
					localisedMessage = CHUDUtils::LocalizeString(m_statusAttackersTicksSafe);		
				}
			}
			break;
		case eGameStateUpdate_TickExtracted:
			CRY_ASSERT_MESSAGE(inPickup, "UpdateGameStateText() tick extracted update needs a valid inPickup");
			if (localTeamId == primaryTeam)
			{
				if (inPickup && inPickup->m_extractedBy == localActorId)
				{
					localisedMessage = CHUDUtils::LocalizeString(m_statusAttackersTicksSafe);		
				}
			}
			// no break intentionally
		case eGameStateUpdate_TickReturned:
			if (localTeamId == secondaryTeam)
			{
				bool pickupNeedsSecuring=false;

				int numPickups = m_pickups.size();
				for (int i=0; i<numPickups; i++)
				{
					SPickup *pickup = &m_pickups[i];
					if (pickup->m_state == ePickupState_Carried || pickup->m_state == ePickupState_Dropped)
					{
						pickupNeedsSecuring=true;
					}
				}

				if (!pickupNeedsSecuring)
				{
					localisedMessage=CHUDUtils::LocalizeString(m_statusDefendersTicksSafe);
				}
			}
			break;

		default:
			DbgLog("CGameRulesObjective_Extraction::UpdateGameStateText() unhandled inUpdate=%d", inUpdate);
			break;
	}
	
	DbgLog("CGameRulesObjective_Extraction::UpdateGameStateText() inUpdate=%d; inPickup=%p; localTeamId=%d; primaryTeam=%d; secondaryTeam=%d; localisedMessage=%s", inUpdate, inPickup, localTeamId, primaryTeam, secondaryTeam, localisedMessage ? localisedMessage : "[NO MESSAGE]");

	if (localisedMessage)
	{
		SHUDEventWrapper::OnGameStatusUpdate(eGBNFLP_Neutral, localisedMessage);
	}
}

//------------------------------------------------------------------------
void CGameRulesObjective_Extraction::PhysicalizeEntityForPickup( SPickup *pickup, bool bEnable )
{
	if ((m_physicsType == ePhysType_Networked) && pickup->m_entityClass)
	{
		CCarryEntity *pNetPhysEnt = static_cast<CCarryEntity*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(pickup->m_spawnedPickupEntityId, pickup->m_entityClass->GetName()));
		if (pNetPhysEnt)
		{
			pNetPhysEnt->Physicalize(bEnable ? CNetworkedPhysicsEntity::ePhys_PhysicalizedRigid : CNetworkedPhysicsEntity::ePhys_NotPhysicalized);
		}
	}
}

void CGameRulesObjective_Extraction::CheckForInteractionWithEntity(EntityId interactId, EntityId playerId, SInteractionInfo& interactionInfo) 
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules && (pGameRules->HasGameActuallyStarted() == false))
	{
		return;
	}

	if (SPickup* pPickup = GetPickupForPickupEntityId(interactId))
	{
		if(pPickup->m_carrierId && pPickup->m_carrierId == playerId)
		{
			if(interactionInfo.interactionType != eInteraction_Use && 
				interactionInfo.interactionType != eInteraction_LargeObject)
			{
				interactionInfo.interactiveEntityId = interactId;
				interactionInfo.interactionType = eInteraction_GameRulesDrop;
				interactionInfo.interactionCustomMsg = "@ui_prompt_interact_drop_extraction_tick";
				interactionInfo.displayTime = g_pGame->GetUI()->GetCVars()->hud_inputprompts_dropPromptTime;
			}
		}
		else if (pGameRules && (pPickup->m_state == ePickupState_AtBase || pPickup->m_state == ePickupState_Dropped))
		{
			int  entTeam = pGameRules->GetTeam(interactId);
			int  plyrTeam = pGameRules->GetTeam(playerId);
			if(entTeam != plyrTeam)
			{
				bool ok = m_pickUpVisCheckHeight<0.f;
				if(!ok)
				{
					CPlayerVisTable::SVisibilityParams params(interactId);
					params.queryParams = eVQP_IgnoreGlass|eVQP_IgnoreSmoke|eVF_CheckAgainstCenter;
					params.heightOffset = m_pickUpVisCheckHeight;
					ok = g_pGame->GetPlayerVisTable()->CanLocalPlayerSee(params, 6);
				}
				if(ok)
				{
					SPickup *pCarrierPickup = GetPickupForCarrierEntityId(playerId);
					if (!pCarrierPickup)			// ensure we're not already carrying another tick
					{
						interactionInfo.interactiveEntityId = interactId;
						interactionInfo.interactionType = eInteraction_GameRulesPickup;
						interactionInfo.interactionCustomMsg = "@ui_prompt_interact_pickup_extraction_tick";
					}
				}
			}
		}
	}
}

bool CGameRulesObjective_Extraction::CheckIsPlayerEntityUsingObjective(EntityId playerId)
{
	int toBeExtracted = 0;
	const size_t numPickups = m_pickups.size();
	for (size_t i = 0; i < numPickups; i++)
	{
		const SPickup* pickupElement = &m_pickups[i];

		if((pickupElement->m_carrierId == playerId) && (pickupElement->m_state != ePickupState_Extracted))
		{
			return true;
		}
	}

	return false;
}

void CGameRulesObjective_Extraction::OnChangedTeam( EntityId entityId, int oldTeamId, int newTeamId )
{
	if (gEnv->IsClient())
	{
		EntityId clientActorId = g_pGame->GetIGameFramework()->GetClientActorId();
		if (clientActorId == entityId)
		{
			DbgLog("CGameRulesObjective_Extraction::OnChangedTeam");

			if (gEnv->bServer || m_hasFirstFrameInited)  // for clients only do the following InitialiseAllHUD() call if they've already done their first-frame initialisation in the Update, because if they haven't then it's likely that the pickups won't have been fully created yet and therefore won't have correct states set, etc., so their correct HUD icons won't be determinable
			{
				DbgLog("[icons] CGameRulesObjective_Extraction::OnChangedTeam() calling InitialiseAllHUD()");
				InitialiseAllHUD();
			}
		}
	}
}

void CGameRulesObjective_Extraction::OnRoundStart()
{
	DbgLog("CGameRulesObjective_Extraction::OnRoundStart()");
	
	DbgLog("[icons] CGameRulesObjective_Extraction::OnRoundStart() calling InitialiseAllHUD()");
	InitialiseAllHUD();
}

void CGameRulesObjective_Extraction::OnRoundEnd()
{
	DbgLog("CGameRulesObjective_Extraction::OnRoundEnd()");
}

float CGameRulesObjective_Extraction::GetTimeLimit()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	IGameRulesRoundsModule*  pRoundsModule = pGameRules->GetRoundsModule();
	CRY_ASSERT(pRoundsModule);
	if (pRoundsModule)
	{
		if(pRoundsModule->GetRoundNumber() & 0x1)	//Is Odd
		{
			// TODO - probably don't want to cut this short by the completion time now
			const int *previousRoundsTeamScores = pRoundsModule->GetPreviousRoundTeamScores();
			if(previousRoundsTeamScores)
			{
				const int primaryTeamIndex = 0;	//for previous even round
				if(previousRoundsTeamScores[primaryTeamIndex] == m_pickups.size())
				{
					if (IGameRulesVictoryConditionsModule *pVictoryModule = pGameRules->GetVictoryConditionsModule())
					{
						const SDrawResolutionData* timeLimitData = pVictoryModule->GetDrawResolutionData(ESVC_DrawResolution_level_1);
						CRY_ASSERT(timeLimitData);
						if(timeLimitData)
						{
							CRY_ASSERT(timeLimitData->m_dataType == SDrawResolutionData::eDataType_float);
							const float previousTimeLimit = m_previousTimeTaken;
							float timeLimitIncludingPrevious = (timeLimitData->m_floatDataForTeams[primaryTeamIndex]/60.0f);
							m_previousTimeTaken = timeLimitIncludingPrevious;
							return std::min(m_timeLimit, timeLimitIncludingPrevious - previousTimeLimit); // To prevent sudden-death in a previous round from causing a longer starting time.
						}
					}
				}
			}
		}
	}

	return m_timeLimit;
}

bool CGameRulesObjective_Extraction::AreSuddenDeathConditionsValid() const
{
	return false;  // no sudden death anymore
}

void CGameRulesObjective_Extraction::OnRoundAboutToStart()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		IGameRulesRoundsModule *pRoundsModule = pGameRules->GetRoundsModule();
		if (pRoundsModule)
		{
			// Round is about to restart but hasn't yet, flip the teams before deciding on a message
			const int attackingTeam = 3 - pRoundsModule->GetPrimaryTeam();
			EntityId clientActorId = g_pGame->GetIGameFramework()->GetClientActorId();
			const int clientTeamId = pGameRules->GetTeam(clientActorId);

			const char *pLocalisedMessage = NULL;

			if (clientTeamId == attackingTeam)
			{
				pLocalisedMessage = CHUDUtils::LocalizeString(m_statusAttackersTicksSafe);		
			}
			else
			{
				pLocalisedMessage = CHUDUtils::LocalizeString(m_statusDefendersTicksSafe);		
			}

			SHUDEventWrapper::OnGameStatusUpdate(eGBNFLP_Neutral, pLocalisedMessage);
		}
	}
}

// play nice with selotaped compiling
#undef DbgLog
#undef DbgLogAlways 
