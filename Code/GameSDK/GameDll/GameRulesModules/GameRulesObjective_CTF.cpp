// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Objectives module for CTF
	-------------------------------------------------------------------------
	History:
	- 23:02:2011  : Created by Colin Gulliver

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesObjective_CTF.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "PlayerPlugin_Interaction.h"
#include "UI/UIManager.h"
#include "UI/UICVars.h"
#include "GameActions.h"
#include "Player.h"
#include "MultiplayerEntities/CarryEntity.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "IGameRulesScoringModule.h"
#include "ActorManager.h"
#include "PersistantStats.h"
#include "Weapon.h"
#include "Audio/Announcer.h"
#include "PlayerVisTable.h"

#ifndef _RELEASE
	#define FATAL_ASSERT(expr) if (!(expr)) { CryFatalError("CGameRulesObjective_CTF - FatalError"); }
#else
	#define FATAL_ASSERT(...) {}
#endif

#define TEAM_SCORE_EVENT(teamId, event) \
	if (IGameRulesScoringModule *pScoringModule = m_pGameRules->GetScoringModule()) \
	{ \
		pScoringModule->OnTeamScoringEvent(teamId, event); \
	}

#define PLAYER_SCORE_EVENT(playerId, event) \
	if (IGameRulesScoringModule *pScoringModule = m_pGameRules->GetScoringModule()) \
	{ \
		pScoringModule->OnPlayerScoringEvent(playerId, event); \
	}

#define PLAY_GLOBAL_SOUND(signalId) \
	if (signalId) \
	{ \
		CAudioSignalPlayer::JustPlay(signalId); \
	}

#define ICON_STRING_FRIENDLY_COLOUR "#9AD5B7"
#define ICON_STRING_HOSTILE_COLOUR  "#AC0000"
#define ICON_STRING_NEUTRAL_COLOUR  "#666666"

#define CTF_FLAG_CLASS_NAME "CTFFlag"

//-------------------------------------------------------------------------
template <typename T>
void GameRulesUtils_CallEntityScriptFunction(EntityId scriptEntId, const char *function, T param)
{
	if (IEntity* pEntity=gEnv->pEntitySystem->GetEntity(scriptEntId))
	{
		IScriptTable*  pScript = pEntity->GetScriptTable();
		if (pScript != NULL && (pScript->GetValueType(function) == svtFunction))
		{
			IScriptSystem*  pScriptSystem = gEnv->pScriptSystem;
			pScriptSystem->BeginCall(pScript, function);
			pScriptSystem->PushFuncParam(pScript);
			pScriptSystem->PushFuncParam(param);
			pScriptSystem->EndCall();
		}
	}
}

//-------------------------------------------------------------------------
template <typename T1, typename T2>
void GameRulesUtils_CallEntityScriptFunction(EntityId scriptEntId, const char *function, T1 param1, T2 param2)
{
	if (IEntity* pEntity=gEnv->pEntitySystem->GetEntity(scriptEntId))
	{
		IScriptTable*  pScript = pEntity->GetScriptTable();
		if (pScript != NULL && (pScript->GetValueType(function) == svtFunction))
		{
			IScriptSystem*  pScriptSystem = gEnv->pScriptSystem;
			pScriptSystem->BeginCall(pScript, function);
			pScriptSystem->PushFuncParam(pScript);
			pScriptSystem->PushFuncParam(param1);
			pScriptSystem->PushFuncParam(param2);
			pScriptSystem->EndCall();
		}
	}
}

//-------------------------------------------------------------------------
CGameRulesObjective_CTF::CGameRulesObjective_CTF()
{
	memset(m_teamInfo, 0, sizeof(m_teamInfo));
	m_teamInfo[0].m_state = eSES_Unknown;
	m_teamInfo[1].m_state = eSES_Unknown;

	m_pGameRules = nullptr;
	m_pFallbackWeaponClass = nullptr;

	m_iconFriendlyFlagCarried = EGRMO_Unknown;
	m_iconHostileFlagCarried = EGRMO_Unknown;
	m_iconFriendlyFlagDropped = EGRMO_Unknown;
	m_iconHostileFlagDropped = EGRMO_Unknown;
	m_iconFriendlyBaseWithFlag = EGRMO_Unknown;
	m_iconHostileBaseWithFlag = EGRMO_Unknown;
	m_iconFriendlyBaseWithNoFlag = EGRMO_Unknown;
	m_iconHostileBaseWithNoFlag = EGRMO_Unknown;

	m_soundFriendlyComplete = INVALID_AUDIOSIGNAL_ID;
	m_soundHostileComplete = INVALID_AUDIOSIGNAL_ID;
	m_soundFriendlyPickup = INVALID_AUDIOSIGNAL_ID;
	m_soundHostilePickup = INVALID_AUDIOSIGNAL_ID;
	m_soundFriendlyReturn = INVALID_AUDIOSIGNAL_ID;
	m_soundHostileReturn = INVALID_AUDIOSIGNAL_ID;
	m_soundFriendlyDropped = INVALID_AUDIOSIGNAL_ID;
	m_soundHostileDropped = INVALID_AUDIOSIGNAL_ID;
	m_soundBaseAlarmOff = INVALID_AUDIOSIGNAL_ID;

	m_moduleRMIIndex = -1;
	m_resetTimerLength = 0.1f;
	m_retrieveFlagDistSqr = 0.f;
	m_pickUpVisCheckHeight = -1.f;
	m_bUseButtonHeld = false;
}

//-------------------------------------------------------------------------
CGameRulesObjective_CTF::~CGameRulesObjective_CTF()
{
	CGameRules *pGameRules = m_pGameRules;

	pGameRules->UnRegisterModuleRMIListener(m_moduleRMIIndex);
	pGameRules->UnRegisterClientConnectionListener(this);
	pGameRules->UnRegisterKillListener(this);
	pGameRules->UnRegisterTeamChangedListener(this);
	pGameRules->UnRegisterRoundsListener(this);
	pGameRules->UnRegisterActorActionListener(this);

	for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
	{
		STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];
		gEnv->pEntitySystem->RemoveEntityEventListener(pTeamInfo->m_baseId, ENTITY_EVENT_ENTERAREA, this);
		gEnv->pEntitySystem->RemoveEntityEventListener(pTeamInfo->m_baseId, ENTITY_EVENT_LEAVEAREA, this);
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Init(XmlNodeRef xml)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	FATAL_ASSERT(pGameRules);
	m_pGameRules = pGameRules;

	const char *pString = NULL;

	const int numChildren = xml->getChildCount();
	for (int i = 0; i < numChildren; ++ i)
	{
		XmlNodeRef xmlChild = xml->getChild(i);

		const char *pTag = xmlChild->getTag();
		if (!stricmp(pTag, "Params"))
		{
			xmlChild->getAttr("droppedResetTimer", m_resetTimerLength);
			m_resetTimerLength = max(m_resetTimerLength, 0.1f);

			xmlChild->getAttr("retrieveFlagDist", m_retrieveFlagDistSqr);
			m_retrieveFlagDistSqr = m_retrieveFlagDistSqr * m_retrieveFlagDistSqr;
			
			xmlChild->getAttr("pickUpVisCheckHeight", m_pickUpVisCheckHeight);
		}
		else if (!stricmp(pTag, "FallbackWeapon"))
		{
			if (xmlChild->getAttr("class", &pString))
			{
				m_pFallbackWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pString);
			}
			FATAL_ASSERT(m_pFallbackWeaponClass);
		}
		else if (!stricmp(pTag, "Strings"))
		{
#define GET_STRING_FROM_XML(name, result) \
			if (xmlChild->getAttr(name, &pString))	\
			{	\
				result.Format("@%s", pString);	\
			}

			GET_STRING_FROM_XML("complete", m_stringComplete);
			GET_STRING_FROM_XML("hasTaken", m_stringHasTaken);
			GET_STRING_FROM_XML("hasCaptured", m_stringHasCaptured);
			GET_STRING_FROM_XML("hasDropped", m_stringHasDropped);
			GET_STRING_FROM_XML("hasReturned", m_stringHasReturned);

			GET_STRING_FROM_XML("iconDefend", m_iconStringDefend);
			GET_STRING_FROM_XML("iconReturn", m_iconStringReturn);
			GET_STRING_FROM_XML("iconEscort", m_iconStringEscort);
			GET_STRING_FROM_XML("iconSeize", m_iconStringSeize);
			GET_STRING_FROM_XML("iconKill", m_iconStringKill);
			GET_STRING_FROM_XML("iconBase", m_iconStringBase);

#undef GET_STRING_FROM_XML
		}
		else if (!stricmp(pTag, "Icons"))
		{
			m_iconFriendlyFlagCarried    = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyFlagCarried"));
			m_iconHostileFlagCarried     = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileFlagCarried"));
			m_iconFriendlyFlagDropped    = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyFlagDropped"));
			m_iconHostileFlagDropped     = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileFlagDropped"));
			m_iconFriendlyBaseWithFlag   = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyBaseWithFlag"));
			m_iconHostileBaseWithFlag    = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileBaseWithFlag"));
			m_iconFriendlyBaseWithNoFlag = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("friendlyBaseWithNoFlag"));
			m_iconHostileBaseWithNoFlag  = SGameRulesMissionObjectiveInfo::GetIconId(xmlChild->getAttr("hostileBaseWithNoFlag"));
		}
		else if (!stricmp(pTag, "Sounds"))
		{
			CGameAudio *pGameAudio = g_pGame->GetGameAudio();
#define GET_SOUND_FROM_XML(name, result) \
			if (xmlChild->getAttr(name, &pString))	\
			{	\
				result = pGameAudio->GetSignalID(pString);	\
			}

			GET_SOUND_FROM_XML("friendlyComplete", m_soundFriendlyComplete);
			GET_SOUND_FROM_XML("hostileComplete", m_soundHostileComplete);
			GET_SOUND_FROM_XML("friendlyPickup", m_soundFriendlyPickup);
			GET_SOUND_FROM_XML("hostilePickup", m_soundHostilePickup);
			GET_SOUND_FROM_XML("friendlyReturn", m_soundFriendlyReturn);
			GET_SOUND_FROM_XML("hostileReturn", m_soundHostileReturn);
			GET_SOUND_FROM_XML("friendlyDropped", m_soundFriendlyDropped);
			GET_SOUND_FROM_XML("hostileDropped", m_soundHostileDropped);

			GET_SOUND_FROM_XML("baseAlarmOff", m_soundBaseAlarmOff);

			TAudioSignalID baseAlarm = INVALID_AUDIOSIGNAL_ID;
			GET_SOUND_FROM_XML("baseAlarm", baseAlarm);
			m_baseAlarmSound[0].SetSignal(baseAlarm);
			m_baseAlarmSound[1].SetSignal(baseAlarm);

#undef GET_SOUND_FROM_XML
		}
	}

	m_moduleRMIIndex = pGameRules->RegisterModuleRMIListener(this);
	pGameRules->RegisterClientConnectionListener(this);
	pGameRules->RegisterKillListener(this);
	pGameRules->RegisterTeamChangedListener(this);
	pGameRules->RegisterRoundsListener(this);
	pGameRules->RegisterActorActionListener(this);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Update(float frameTime)
{
	if (gEnv->IsClient())
	{
		Client_UpdatePickupAndDrop();
	}

	for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
	{
		STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];
		ESpawnEntityState state = pTeamInfo->m_state;
		if (state == ESES_Dropped)
		{
			if(gEnv->bServer)
			{
				IEntity*  pEntity = gEnv->pEntitySystem->GetEntity(pTeamInfo->m_flagId);
				if(pEntity)
				{
					if(g_pGame->GetGameRules()->IsInsideForbiddenArea(pEntity->GetWorldPos(), true, NULL))
					{
						Server_ResetFlag(pTeamInfo, 0); 
						break; 
					}
				}
			}

			pTeamInfo->m_resetTimer += frameTime;
			if (gEnv->bServer && (pTeamInfo->m_resetTimer > m_resetTimerLength))
			{
				Server_ResetFlag(pTeamInfo, 0);
			}
			else
			{
				float timeLeftFraction = pTeamInfo->m_resetTimer / m_resetTimerLength;
				int timeLeftFrame = (int) (100 * timeLeftFraction);
				timeLeftFrame = min(max(timeLeftFrame, 2), 100);

				if (timeLeftFrame != pTeamInfo->m_droppedIconFrame)
				{
					SHUDEvent newUpdateObjectiveClock(eHUDEvent_ObjectiveUpdateClock);
					newUpdateObjectiveClock.AddData(static_cast<int>(pTeamInfo->m_flagId));
					newUpdateObjectiveClock.AddData(timeLeftFrame);
					CHUDEventDispatcher::CallEvent(newUpdateObjectiveClock);

					pTeamInfo->m_droppedIconFrame = timeLeftFrame;
				}
			}
		}
		else if (state == ESES_Carried)
		{
			CPersistantStats::GetInstance()->IncrementStatsForActor(pTeamInfo->m_carrierId, EFPS_FlagCarriedTime, frameTime);
		}
	}

	if (gEnv->bServer)
	{
		Server_CheckForFlagRetrieval();
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::OnStartGame()
{
	if (gEnv->bServer)
	{
		Server_ResetAll();
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::PostInitClient( int channelId )
{
	for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
	{
		STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];
		if (pTeamInfo->m_baseId)
		{
			CGameRules::SModuleRMIEntityParams baseParams(m_moduleRMIIndex, pTeamInfo->m_baseId, eRMI_SetupBase);
			m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), baseParams, eRMI_ToClientChannel|eRMI_NoLocalCalls, pTeamInfo->m_baseId, channelId);

			if (pTeamInfo->m_state == ESES_Dropped)
			{
				CGameRules::SModuleRMIEntityParams flagParams(m_moduleRMIIndex, pTeamInfo->m_flagId, eRMI_SetupDroppedFlag);
				m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), flagParams, eRMI_ToClientChannel|eRMI_NoLocalCalls, pTeamInfo->m_flagId, channelId);
			}
			else
			{
				CGameRules::SModuleRMIEntityParams flagParams(m_moduleRMIIndex, pTeamInfo->m_flagId, eRMI_SetupFlag);
				m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), flagParams, eRMI_ToClientChannel|eRMI_NoLocalCalls, pTeamInfo->m_flagId, channelId);
			}

			CGameRules::SModuleRMITwoEntityParams weaponParams(m_moduleRMIIndex, pTeamInfo->m_fallbackWeaponId, pTeamInfo->m_flagId, eRMI_SetupFallbackWeapon);
			m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), weaponParams, eRMI_ToClientChannel|eRMI_NoLocalCalls, pTeamInfo->m_fallbackWeaponId, channelId);

			if (pTeamInfo->m_state == ESES_Carried)
			{
				CGameRules::SModuleRMIEntityParams params(m_moduleRMIIndex, pTeamInfo->m_carrierId, eRMI_Pickup);
				m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), params, eRMI_ToClientChannel|eRMI_NoLocalCalls, pTeamInfo->m_carrierId, channelId);
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::OnHostMigration( bool becomeServer )
{
	if (becomeServer)
	{
		// Make sure all our carriers are still present
		for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
		{
			STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];
			if ((pTeamInfo->m_state == ESES_Carried) && (pTeamInfo->m_carrierId))
			{
				IEntity *pCarrier = gEnv->pEntitySystem->GetEntity(pTeamInfo->m_carrierId);
				if (!pCarrier)
				{
					CryLog("CGameRulesObjective_CTF::OnHostMigration() found carrier that doesn't exist anymore (%u), dropping", pTeamInfo->m_carrierId);
					Server_Drop(pTeamInfo->m_carrierId, pTeamInfo, pTeamInfo->m_carrierId);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::CheckForInteractionWithEntity(EntityId interactId, EntityId playerId, SInteractionInfo &interactionInfo)
{
	if (m_pGameRules->HasGameActuallyStarted() == false)
	{
		return;
	}

	for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
	{
		if (m_teamInfo[teamIndex].m_flagId == interactId)
		{
			ESpawnEntityState state = m_teamInfo[teamIndex].m_state;
			if ((state == ESES_Carried) && (m_teamInfo[teamIndex].m_carrierId == playerId))
			{
				if(interactionInfo.interactionType != eInteraction_Use && 
					interactionInfo.interactionType != eInteraction_LargeObject) // Still able to kick objects whilst holding relay
				{
					interactionInfo.interactiveEntityId = interactId;
					interactionInfo.interactionType = eInteraction_GameRulesDrop;
					interactionInfo.interactionCustomMsg = "@ui_prompt_interact_drop_flag";
					interactionInfo.displayTime = g_pGame->GetUI()->GetCVars()->hud_inputprompts_dropPromptTime;
				}
			}
			else if ((state == ESES_AtBase) || (state == ESES_Dropped))
			{
				bool ok = (state == ESES_AtBase) || m_pickUpVisCheckHeight<0.f;
				if(!ok)
				{
					CPlayerVisTable::SVisibilityParams params(interactId);
					params.queryParams = eVQP_IgnoreGlass|eVQP_IgnoreSmoke|eVF_CheckAgainstCenter;
					params.heightOffset = m_pickUpVisCheckHeight;
					ok = g_pGame->GetPlayerVisTable()->CanLocalPlayerSee(params, 6);
				}
				if(ok)
				{
					int teamId = m_pGameRules->GetTeam(playerId);
					if ((teamId - 1) == (1 - teamIndex))
					{
						interactionInfo.interactiveEntityId = interactId;
						interactionInfo.interactionType = eInteraction_GameRulesPickup;
						interactionInfo.interactionCustomMsg = "@ui_prompt_interact_pickup_flag";
					}
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
bool CGameRulesObjective_CTF::CheckIsPlayerEntityUsingObjective(EntityId entityId)
{
	return (FindInfoByCarrierId(entityId) != NULL);
}

//-------------------------------------------------------------------------
ESpawnEntityState CGameRulesObjective_CTF::GetObjectiveEntityState(EntityId entityId)
{
	if (STeamInfo *pTeamInfo = FindInfoByFlagId(entityId))
	{
		return pTeamInfo->m_state;
	}
	return eSES_Unknown;
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::OnSingleEntityRMI(CGameRules::SModuleRMIEntityParams params)
{
	CryLog("CGameRulesObjective_CTF::OnSingleEntityRMI() RMI type=%d, entityId=%u", params.m_data, params.m_entityId);
	switch (params.m_data)
	{
		case eRMI_SetupBase:
			Handle_SetupBase(params.m_entityId);
			break;
		case eRMI_Pickup:
			Handle_Pickup(params.m_entityId);
			break;
		case eRMI_Drop:
			Handle_Drop(params.m_entityId);
			break;
		case eRMI_SetupFlag:
			Handle_SetupFlag(params.m_entityId);
			break;
		case eRMI_SetupDroppedFlag:
			Handle_SetupDroppedFlag(params.m_entityId);
			break;
		case eRMI_ResetAll:
			Handle_ResetAll();
			break;
		case eRMI_Scored:
			Handle_Scored(params.m_entityId);
			break;
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::OnDoubleEntityRMI(CGameRules::SModuleRMITwoEntityParams params)
{
	CryLog("CGameRulesObjective_CTF::OnDoubleEntityRMI() RMI type=%d, entityId1=%u, entityId2=%u", params.m_data, params.m_entityId1, params.m_entityId2);
	switch (params.m_data)
	{
		case eRMI_ResetFlag:
			Handle_ResetFlag(params.m_entityId1, params.m_entityId2);
			break;
		case eRMI_SetupFallbackWeapon:
			Handle_SetupFallbackWeapon(params.m_entityId1, params.m_entityId2);
			break;
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::OnSvClientActionRMI( CGameRules::SModuleRMISvClientActionParams params, EntityId fromEid )
{
	CryLog("CGameRulesObjective_CTF::OnSvClientActionRMI() RMI type=%d, fromEid=%u", params.m_action, fromEid);
	switch (params.m_action)
	{
		case CGameRules::SModuleRMISvClientActionParams::eACT_HelperCarry_Pickup:
			Handle_RequestPickup(fromEid);
			break;
		case CGameRules::SModuleRMISvClientActionParams::eACT_HelperCarry_Drop:
			Handle_RequestDrop(fromEid);
			break;
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::OnOwnClientEnteredGame()
{
	Client_SetAllIcons();
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::OnChangedTeam(EntityId entityId, int oldTeamId, int newTeamId)
{
	for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
	{
		STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];
		if (gEnv->bServer && (pTeamInfo->m_state == ESES_Carried) && (pTeamInfo->m_carrierId == entityId))
		{
			Server_Drop(entityId, pTeamInfo, 0);
		}

		if (gEnv->IsClient() && (entityId == g_pGame->GetIGameFramework()->GetClientActorId()))
		{
			Client_SetAllIcons();
		}

		if (pTeamInfo->m_baseId == entityId)
		{
			// Round swap
			if (newTeamId != (teamIndex + 1))
			{
				STeamInfo swapInfo = m_teamInfo[0];
				m_teamInfo[0] = m_teamInfo[1];
				m_teamInfo[1] = swapInfo;

				if (gEnv->bServer)
				{
					m_pGameRules->SetTeam(1, m_teamInfo[0].m_flagId);
					m_pGameRules->SetTeam(2, m_teamInfo[1].m_flagId);
				}
			}

			if (gEnv->IsClient())
			{
				Client_SetAllIcons();
			}
		}

		if (pTeamInfo->m_flagId == entityId)
		{
			SHUDEventWrapper::OnNewGameRulesObjective(pTeamInfo->m_flagId);
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::OnRoundAboutToStart()
{
	if (gEnv->bServer)
	{
		Server_ResetAll();
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::OnAction(const ActionId &action, int activationMode, float value)
{
	bool bIsUseButton    = (action == g_pGame->Actions().use);
	bool bIsSwitchWeapon = (action == g_pGame->Actions().toggle_weapon);

	if (bIsUseButton || bIsSwitchWeapon)
	{
		CPlayer *pClientPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());

		if (pClientPlayer)
		{
			if (pClientPlayer->IsDead() == false)
			{
				const SInteractionInfo& interaction = pClientPlayer->GetCurrentInteractionInfo();

				if (interaction.interactionType == eInteraction_GameRulesPickup)
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

					if (bIsUseButton)
					{
						if ((activationMode == eAAM_OnPress) || (activationMode == eAAM_OnHold))
						{
							m_bUseButtonHeld = true;
						}
						else if (activationMode == eAAM_OnRelease)
						{
							m_bUseButtonHeld = false;
						}
					}
				}
				else if (interaction.interactionType == eInteraction_GameRulesDrop)
				{
					if ((activationMode == eAAM_OnPress) || (activationMode == eAAM_OnHold))
					{
						m_bUseButtonHeld = true;
					}
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::OnEntityEvent( IEntity *pEntity, const SEntityEvent& event )
{
	EntityId entityId = pEntity->GetId();

	if (event.event == ENTITY_EVENT_ENTERAREA)
	{
		EntityId playerId = (EntityId) event.nParam[0];
		CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId));
		if (pActor != NULL && pActor->IsPlayer() && !pActor->IsDead())
		{
			for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
			{
				STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];
				if (entityId == pTeamInfo->m_baseId)
				{
					int otherTeamIndex = (1 - teamIndex);
					STeamInfo *pOtherTeamInfo = &m_teamInfo[otherTeamIndex];

					if (gEnv->IsClient() && pActor->IsClient() && pTeamInfo->m_state != ESES_AtBase && pOtherTeamInfo->m_state == ESES_Carried && (pOtherTeamInfo->m_carrierId == playerId))
					{
						CAnnouncer::GetInstance()->Announce(playerId, "UnableToScore", CAnnouncer::eAC_inGame); 
					}

					if (gEnv->bServer && (pTeamInfo->m_state == ESES_AtBase))
					{
						if ((pOtherTeamInfo->m_state == ESES_Carried) && (pOtherTeamInfo->m_carrierId == playerId))
						{
							// Scored
							Common_Scored(playerId);

							CGameRules::SModuleRMIEntityParams params(m_moduleRMIIndex, playerId, eRMI_Scored);
							m_pGameRules->GetGameObject()->InvokeRMI(CGameRules::ClModuleRMISingleEntity(), params, eRMI_ToRemoteClients);

							TEAM_SCORE_EVENT((teamIndex + 1), EGRST_CarryObjectiveCompleted);
							PLAYER_SCORE_EVENT(playerId, EGRST_CarryObjectiveCompleted);
						}
					}

					if (pActor->IsClient())
					{
						GameRulesUtils_CallEntityScriptFunction(entityId, "SetInProximity", true);
					}
				}
			}
		}
	}
	else if (event.event == ENTITY_EVENT_LEAVEAREA)
	{
		EntityId playerId = (EntityId) event.nParam[0];
		CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId));
		if (pActor && pActor->IsPlayer() && !pActor->IsDead() && pActor->IsClient())
		{
			GameRulesUtils_CallEntityScriptFunction(entityId, "SetInProximity", false);
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::OnPlayerKilledOrLeft(EntityId targetId, EntityId shooterId)
{
	if (gEnv->bServer)
	{
		if (STeamInfo *pTeamInfo = FindInfoByCarrierId(targetId))
		{
			CryLog("CGameRulesObjective_CTF::OnPlayerKilledOrLeft() carrier (%d) has died, dropping flag", targetId);
			Server_Drop(targetId, pTeamInfo, shooterId);
		}
	}
		
	// Always remove this player from all carry entity picked up records. They can pick it up again fresh this life, and receive capture points. 
	SetPlayerHasPickedUp(targetId, false); 
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::AttachFlagToBase(STeamInfo *pTeamInfo)
{
	FATAL_ASSERT((pTeamInfo == &m_teamInfo[0]) || (pTeamInfo == &m_teamInfo[1]));

	IEntity *pBase = gEnv->pEntitySystem->GetEntity(pTeamInfo->m_baseId);
	IEntity *pFlag = gEnv->pEntitySystem->GetEntity(pTeamInfo->m_flagId);

	if (pBase != NULL && pFlag != NULL)
	{
		pFlag->SetPos(pBase->GetPos());
	}

	GameRulesUtils_CallEntityScriptFunction(pTeamInfo->m_baseId, "ObjectiveAttached", ScriptHandle(pTeamInfo->m_flagId), true);
	pTeamInfo->m_state = ESES_AtBase;
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::DetachFlagFromBase(STeamInfo *pTeamInfo)
{
	GameRulesUtils_CallEntityScriptFunction(pTeamInfo->m_baseId, "ObjectiveAttached", ScriptHandle(pTeamInfo->m_flagId), false);
}

//-------------------------------------------------------------------------
CGameRulesObjective_CTF::STeamInfo *CGameRulesObjective_CTF::FindInfoByFlagId(EntityId flagId)
{
	for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
	{
		STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];

		if (pTeamInfo->m_flagId == flagId)
		{
			return pTeamInfo;
		}
	}
	return NULL;
}

//-------------------------------------------------------------------------
int CGameRulesObjective_CTF::FindTeamIndexByCarrierId(EntityId playerId) const
{
	for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
	{
		const STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];

		if ((pTeamInfo->m_state == ESES_Carried) && (pTeamInfo->m_carrierId == playerId))
		{
			return teamIndex;
		}
	}
	return -1;
}

//-------------------------------------------------------------------------
CGameRulesObjective_CTF::STeamInfo *CGameRulesObjective_CTF::FindInfoByCarrierId(EntityId playerId)
{
	int teamIndex = FindTeamIndexByCarrierId(playerId);
	if (teamIndex >= 0)
	{
		return &m_teamInfo[teamIndex];
	}
	return NULL;
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Handle_RequestPickup( EntityId playerId )
{
	int teamId = m_pGameRules->GetTeam(playerId);
	if ((teamId == 1) || (teamId == 2))
	{
		int teamIndex = 2 - teamId;
		STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];
		ESpawnEntityState state = pTeamInfo->m_state;
		if ((state == ESES_AtBase) || (state == ESES_Dropped))
		{
			Server_PhysicalizeFlag(pTeamInfo->m_flagId, false);

			// *prior* to Common_pickup, test if this player has picked up the flag previously this life
			bool bCanAwardObjectiveTaken = false; 
			
			// Only award pick up points the first time a player picks up an objective - test against the entity's record of players who have picked it up. 
			// Players are individually removed from the carry entity's 'picked up record' on death. 
			// All Players are removed from the carry entity's 'picked up record' when their team successfully captures it. 	
			if (!IsPlayerInPickedUpRecord(playerId))
			{
				bCanAwardObjectiveTaken = true; 
			}

			// Pickup
			Common_Pickup(playerId);

			// Tell clients
			CGameRules::SModuleRMIEntityParams params(m_moduleRMIIndex, playerId, eRMI_Pickup);
			m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), params, eRMI_ToRemoteClients, playerId);

			if (state == ESES_AtBase && bCanAwardObjectiveTaken)
			{
				PLAYER_SCORE_EVENT(playerId, EGRST_CarryObjectiveTaken);
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Handle_RequestDrop( EntityId playerId )
{
	if (STeamInfo *pTeamInfo = FindInfoByCarrierId(playerId))
	{
		Server_Drop(playerId, pTeamInfo, 0);
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Handle_ResetFlag( EntityId flagId, EntityId playerId )
{
	Common_ResetFlag(flagId, playerId);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Handle_SetupBase( EntityId baseId )
{
	int teamId = m_pGameRules->GetTeam(baseId);
	FATAL_ASSERT((teamId == 1) || (teamId == 2));

	STeamInfo *pTeamInfo = &m_teamInfo[teamId - 1];
	pTeamInfo->m_baseId = baseId;

	gEnv->pEntitySystem->AddEntityEventListener(baseId, ENTITY_EVENT_ENTERAREA, this);
	gEnv->pEntitySystem->AddEntityEventListener(baseId, ENTITY_EVENT_LEAVEAREA, this);

	Client_SetIconForBase(teamId - 1);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Handle_Pickup( EntityId playerId )
{
	Common_Pickup(playerId);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Handle_Drop( EntityId playerId )
{
	Common_Drop(playerId);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Handle_SetupFlag( EntityId flagId )
{
	int teamId = m_pGameRules->GetTeam(flagId);
	FATAL_ASSERT((teamId == 1) || (teamId == 2));

	int teamIndex = teamId - 1;

	STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];
	pTeamInfo->m_flagId = flagId;
	pTeamInfo->m_state = ESES_AtBase;

	AttachFlagToBase(pTeamInfo);

	SHUDEventWrapper::OnNewGameRulesObjective(flagId);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Handle_SetupDroppedFlag( EntityId flagId )
{
	int teamId = m_pGameRules->GetTeam(flagId);
	FATAL_ASSERT((teamId == 1) || (teamId == 2));

	int teamIndex = teamId - 1;

	STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];
	pTeamInfo->m_flagId = flagId;
	pTeamInfo->m_state = ESES_Dropped;
	pTeamInfo->m_resetTimer = 0.f;

	Client_SetIconForFlag(teamIndex);

	SHUDEventWrapper::OnNewGameRulesObjective(flagId);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Handle_ResetAll()
{
	Common_ResetAll();
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Handle_SetupFallbackWeapon( EntityId weaponId, EntityId flagId )
{
	int teamId = m_pGameRules->GetTeam(flagId);
	FATAL_ASSERT((teamId == 1) || (teamId == 2));

	STeamInfo *pTeamInfo = &m_teamInfo[teamId - 1];
	pTeamInfo->m_fallbackWeaponId = weaponId;

	CCarryEntity *pFlag = static_cast<CCarryEntity*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(flagId, CTF_FLAG_CLASS_NAME));
	if (pFlag)
	{
		pFlag->SetSpawnedWeaponId(weaponId);
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Handle_Scored(EntityId playerId)
{
	Common_Scored(playerId);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Server_ResetAll()
{
	FATAL_ASSERT(gEnv->bServer);
	// Check if we need to find the bases
	if (m_teamInfo[0].m_baseId == 0)
	{
		const IEntityClass *pBaseClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("CTFBase");
		IEntityClass *pFlagClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(CTF_FLAG_CLASS_NAME);

		FATAL_ASSERT(pBaseClass && pFlagClass);

		IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();

		pIt->MoveFirst();
		while (IEntity *pEntity = pIt->Next())
		{
			if (pEntity->GetClass() == pBaseClass)
			{
				EntityId entityId = pEntity->GetId();
				int teamId = m_pGameRules->GetTeam(entityId);

				if ((teamId == 1) || (teamId == 2))
				{
					int teamIndex = teamId - 1;
					STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];
					if ((pTeamInfo->m_baseId == 0) &&
							(pTeamInfo->m_flagId == 0))
					{
						pTeamInfo->m_baseId = entityId;

						gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_ENTERAREA, this);
						gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_LEAVEAREA, this);

						CGameRules::SModuleRMIEntityParams baseParams(m_moduleRMIIndex, entityId, eRMI_SetupBase);
						m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), baseParams, eRMI_ToRemoteClients, entityId);

						TFixedString flagName;
						flagName.Format("CTFFlag_%d", teamId);

						SEntitySpawnParams spawnParams;
						spawnParams.pClass = pFlagClass;
						spawnParams.sName = flagName.c_str();
						spawnParams.vPosition = pEntity->GetWorldPos();
						Ang3 rotation = pEntity->GetWorldAngles();
						spawnParams.nFlags = ENTITY_FLAG_NEVER_NETWORK_STATIC;

						IEntity *pFlagEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams, true);
						FATAL_ASSERT(pFlagEntity);
						if (pFlagEntity)
						{
							EntityId flagId = pFlagEntity->GetId();
							pTeamInfo->m_flagId = flagId;
							m_pGameRules->SetTeam(teamId, flagId);

							CGameRules::SModuleRMIEntityParams flagParams(m_moduleRMIIndex, flagId, eRMI_SetupFlag);
							m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), flagParams, eRMI_ToRemoteClients, flagId);

							SHUDEventWrapper::OnNewGameRulesObjective(flagId);

							if (m_pFallbackWeaponClass)
							{
								TFixedString weaponName;
								weaponName.Format("FallbackWeapon_%d", teamId);

								// Spawn fallback weapon
								SEntitySpawnParams weaponSpawnParams;
								weaponSpawnParams.pClass = m_pFallbackWeaponClass;
								weaponSpawnParams.sName = weaponName.c_str();
								weaponSpawnParams.nFlags = ENTITY_FLAG_NO_PROXIMITY | ENTITY_FLAG_NEVER_NETWORK_STATIC;
								IEntity *pWeapon = gEnv->pEntitySystem->SpawnEntity(weaponSpawnParams, true);
								FATAL_ASSERT(pWeapon);
								if (pWeapon)
								{
									EntityId weaponId = pWeapon->GetId();
									pTeamInfo->m_fallbackWeaponId = weaponId;

									CCarryEntity *pFlag = static_cast<CCarryEntity*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(flagId, CTF_FLAG_CLASS_NAME));
									if (pFlag)
									{
										pFlag->SetSpawnedWeaponId(weaponId);
									}

									CGameRules::SModuleRMITwoEntityParams weaponParams(m_moduleRMIIndex, weaponId, flagId, eRMI_SetupFallbackWeapon);
									m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMIDoubleEntity(), weaponParams, eRMI_ToRemoteClients, weaponId);
								}
							}
						}
					}
					CryLog("CGameRulesObjective_CTF::Server_ResetAll() setup state for base, teamId=%d, base=%d, flag=%d, weapon=%d", teamId, pTeamInfo->m_baseId, pTeamInfo->m_flagId, pTeamInfo->m_fallbackWeaponId);
				}
			}
		}
	}
	else
	{
		CGameRules::SModuleRMIEntityParams resetParams(m_moduleRMIIndex, 0, eRMI_ResetAll);
		m_pGameRules->GetGameObject()->InvokeRMI(CGameRules::ClModuleRMISingleEntity(), resetParams, eRMI_ToRemoteClients);
	}

	Common_ResetAll();

	Server_SwapSides();
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Server_Drop(EntityId playerId, STeamInfo *pTeamInfo, EntityId shooterId)
{
	FATAL_ASSERT(gEnv->bServer);
	Server_PhysicalizeFlag(pTeamInfo->m_flagId, true);

	Common_Drop(playerId);

	CGameRules::SModuleRMIEntityParams params(m_moduleRMIIndex, playerId, eRMI_Drop);
	m_pGameRules->GetGameObject()->InvokeRMI(CGameRules::ClModuleRMISingleEntity(), params, eRMI_ToRemoteClients);

	if (shooterId && (shooterId != playerId))
	{
		PLAYER_SCORE_EVENT(shooterId, EGRST_CarryObjectiveCarrierKilled);
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Server_ResetFlag(STeamInfo *pTeamInfo, EntityId playerId)
{
	FATAL_ASSERT(gEnv->bServer);
	EntityId flagId = pTeamInfo->m_flagId;

	Server_PhysicalizeFlag(flagId, false);
	Common_ResetFlag(flagId, playerId);

	CGameRules::SModuleRMITwoEntityParams params(m_moduleRMIIndex, flagId, playerId, eRMI_ResetFlag);
	m_pGameRules->GetGameObject()->InvokeRMI(CGameRules::ClModuleRMIDoubleEntity(), params, eRMI_ToRemoteClients);
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Server_SwapSides()
{
	FATAL_ASSERT(gEnv->bServer);
	EntityId base1 = m_teamInfo[0].m_baseId;
	EntityId base2 = m_teamInfo[1].m_baseId;

	int base1Team = m_pGameRules->GetTeam(base1);
	if ((base1Team == 1) || (base1Team == 2))
	{
		m_pGameRules->SetTeam(3 - base1Team, base1);
		m_pGameRules->SetTeam(base1Team, base2);
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Server_CheckForFlagRetrieval()
{
	if (CActorManager* pActorManager = CActorManager::GetActorManager())
	{
		const int numActors	= pActorManager->GetNumActorsIncludingLocalPlayer();

		for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
		{
			STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];

			if (pTeamInfo->m_state == ESES_Dropped)
			{
				IEntity *pEntity = gEnv->pEntitySystem->GetEntity(pTeamInfo->m_flagId);

				if (pEntity)
				{
					pActorManager->PrepareForIteration();

					Vec3 entityPos = pEntity->GetWorldPos();

					for (int i = 0; i < numActors; ++ i)
					{
						SActorData actorData;
						pActorManager->GetNthActorData(i, actorData);

						EntityId actorId = actorData.entityId;
						
						if ((actorData.teamId == (teamIndex + 1)) && (actorData.health > 0.f) && (entityPos.GetSquaredDistance(actorData.position) <= m_retrieveFlagDistSqr))
						{
							CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(actorId));
							if (pActor)
							{
								if (pActor->IsPlayer() && (pActor->IsDead() == false) && (pActor->GetSpectatorMode() == CActor::eASM_None))
								{
									Server_ResetFlag(pTeamInfo, actorId);
								}
							}
						}
					}
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Server_PhysicalizeFlag(EntityId flagId, bool bEnable)
{
	CNetworkedPhysicsEntity *pNetPhysEnt = static_cast<CNetworkedPhysicsEntity*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(flagId, CTF_FLAG_CLASS_NAME));
	if (pNetPhysEnt)
	{
		pNetPhysEnt->Physicalize(bEnable ? CNetworkedPhysicsEntity::ePhys_PhysicalizedRigid : CNetworkedPhysicsEntity::ePhys_NotPhysicalized);
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Common_Pickup(EntityId playerId)
{
	int teamId = m_pGameRules->GetTeam(playerId);
	if ((teamId == 1) || (teamId == 2))
	{
		int teamIndex = 2 - teamId;
		STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];

		ESpawnEntityState previousState = pTeamInfo->m_state;
	 	if (previousState	== ESES_AtBase)
		{
			DetachFlagFromBase(pTeamInfo);
			//m_baseAlarmSound[teamIndex].Play(pTeamInfo->m_baseId);
			CryLog("CGameRulesObjective_CTF::Common_Pickup() m_baseAlarmSound[%d] play for entityId=%d", teamIndex, pTeamInfo->m_baseId);
		
			// Add to record on all players to handle host migration etc
			SetPlayerHasPickedUp(playerId, true); 
	
		}

		m_pGameRules->OnPickupEntityAttached(pTeamInfo->m_flagId, playerId, CTF_FLAG_CLASS_NAME);
		pTeamInfo->m_state = ESES_Carried;
		pTeamInfo->m_carrierId = playerId;

		if (gEnv->IsClient())
		{
			Client_SetIconForCarrier(teamIndex, playerId, false);
			if (previousState == ESES_AtBase)
			{
				Client_SetIconForBase(teamIndex);
			}
			else
			{
				Client_SetIconForFlag(teamIndex);
			}

			EntityId clientActorId = g_pGame->GetIGameFramework()->GetClientActorId();
			if (playerId == clientActorId)
			{
				CHUDEventDispatcher::CallEvent( SHUDEvent(eHUDEvent_OnLocalClientStartCarryingFlag) );

				Client_SetIconForBase(1 - teamIndex);
			}

			if (m_pGameRules->GetTeam(clientActorId) == teamId)
			{
				PLAY_GLOBAL_SOUND(m_soundFriendlyPickup);
				SHUDEventWrapper::OnGameStateMessage(playerId, true, m_stringHasTaken.c_str(), eBLIF_Relay);
			}
			else
			{
				PLAY_GLOBAL_SOUND(m_soundHostilePickup);
				SHUDEventWrapper::OnGameStateMessage(playerId, false, m_stringHasTaken.c_str(), eBLIF_Relay);
			}

			CAnnouncer::GetInstance()->Announce(playerId, "FlagTaken", CAnnouncer::eAC_inGame); 
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Common_Drop(EntityId playerId)
{
	int teamId = m_pGameRules->GetTeam(playerId);

	if (STeamInfo *pTeamInfo = FindInfoByCarrierId(playerId))
	{
		m_pGameRules->OnPickupEntityDetached(pTeamInfo->m_flagId, playerId, false, CTF_FLAG_CLASS_NAME);
		pTeamInfo->m_state = ESES_Dropped;
		pTeamInfo->m_carrierId = 0;
		pTeamInfo->m_resetTimer = 0.f;

		if (gEnv->IsClient())
		{
			int teamIndex = (int)(pTeamInfo - m_teamInfo);
			Client_SetIconForCarrier(teamIndex, playerId, true);
			Client_SetIconForFlag(teamIndex);

			EntityId clientActorId = g_pGame->GetIGameFramework()->GetClientActorId();
			if (m_pGameRules->GetTeam(clientActorId) == teamId)
			{
				PLAY_GLOBAL_SOUND(m_soundFriendlyDropped);
				SHUDEventWrapper::OnGameStateMessage(playerId, true, m_stringHasDropped.c_str(), eBLIF_Relay);
			}
			else
			{
				PLAY_GLOBAL_SOUND(m_soundHostileDropped);
				SHUDEventWrapper::OnGameStateMessage(playerId, false, m_stringHasDropped.c_str(), eBLIF_Relay);
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Common_ResetFlag(EntityId flagId, EntityId playerId)
{
	if (STeamInfo *pTeamInfo = FindInfoByFlagId(flagId))
	{
		AttachFlagToBase(pTeamInfo);

		if (gEnv->IsClient())
		{
			int teamIndex = (int)(pTeamInfo - m_teamInfo);
			Client_SetIconForBase(teamIndex);
			Client_SetIconForFlag(teamIndex);

			//m_baseAlarmSound[teamIndex].Stop(pTeamInfo->m_baseId);
			CryLog("CGameRulesObjective_CTF::Common_ResetFlag() m_baseAlarmSound[%d] stop for entityId=%d", teamIndex, pTeamInfo->m_baseId);
			CAudioSignalPlayer::JustPlay(m_soundBaseAlarmOff, pTeamInfo->m_baseId);

			EntityId clientActorId = g_pGame->GetIGameFramework()->GetClientActorId();
			if (m_pGameRules->GetTeam(clientActorId) == (teamIndex + 1))
			{
				PLAY_GLOBAL_SOUND(m_soundFriendlyComplete);
				if (playerId)
				{
					SHUDEventWrapper::OnGameStateMessage(playerId, true, m_stringHasReturned.c_str(), eBLIF_Relay);
				}
			}
			else
			{
				PLAY_GLOBAL_SOUND(m_soundHostileComplete);
				if (playerId)
				{
					SHUDEventWrapper::OnGameStateMessage(playerId, false, m_stringHasReturned.c_str(), eBLIF_Relay);
				}
			}

			CAnnouncer::GetInstance()->Announce(playerId, "FlagReturned", CAnnouncer::eAC_inGame); 
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Common_Scored(EntityId playerId)
{
	int teamIndex = FindTeamIndexByCarrierId(playerId);

	if (teamIndex >= 0)
	{
		STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];

		m_pGameRules->OnPickupEntityDetached(pTeamInfo->m_flagId, pTeamInfo->m_carrierId, false, CTF_FLAG_CLASS_NAME);
		AttachFlagToBase(pTeamInfo);
		
		//m_baseAlarmSound[teamIndex].Stop(pTeamInfo->m_baseId);
		CryLog("CGameRulesObjective_CTF::Common_Scored() m_baseAlarmSound[%d] stop for entityId=%d", teamIndex, pTeamInfo->m_baseId);

		pTeamInfo->m_carrierId = 0;

		// All players can now score when they take this flag again. 
		ClearPlayerPickedUpRecords(); 
		
		if (gEnv->IsClient())
		{
			Client_SetIconForCarrier(teamIndex, playerId, true);
			Client_SetIconForBase(teamIndex);

			EntityId clientActorId = g_pGame->GetIGameFramework()->GetClientActorId();
			if (m_pGameRules->GetTeam(clientActorId) != (teamIndex + 1))
			{
				PLAY_GLOBAL_SOUND(m_soundFriendlyComplete);
				SHUDEventWrapper::OnGameStateMessage(playerId, true, m_stringHasCaptured.c_str(), eBLIF_Relay);
			}
			else
			{
				PLAY_GLOBAL_SOUND(m_soundHostileComplete);
				SHUDEventWrapper::OnGameStateMessage(playerId, false, m_stringHasCaptured.c_str(), eBLIF_Relay);
			}

			CAnnouncer::GetInstance()->Announce(playerId, "FlagScore", CAnnouncer::eAC_inGame); 
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Common_ResetAll()
{
	if (gEnv->IsClient())
	{
		Client_RemoveAllIcons();
	}

	for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
	{
		STeamInfo *pTeamInfo = &m_teamInfo[teamIndex];
		switch (pTeamInfo->m_state)
		{
			case ESES_Carried:
				m_pGameRules->OnPickupEntityDetached(pTeamInfo->m_flagId, pTeamInfo->m_carrierId, false, CTF_FLAG_CLASS_NAME);
				// Deliberate fall-through
			case ESES_Dropped:
			case eSES_Unknown:
				AttachFlagToBase(pTeamInfo);
				//m_baseAlarmSound[teamIndex].Stop(pTeamInfo->m_baseId);
				CryLog("CGameRulesObjective_CTF::Common_ResetAll() m_baseAlarmSound[%d] stop for entityId=%d", teamIndex, pTeamInfo->m_baseId);
				break;
		}
	}

	if (gEnv->IsClient())
	{
		Client_SetAllIcons();
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Client_RemoveAllIcons()
{
	for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
	{
		SHUDEventWrapper::OnRemoveObjectiveWithRadarEntity(m_teamInfo[teamIndex].m_flagId, m_teamInfo[teamIndex].m_flagId, 0);
		SHUDEventWrapper::OnRemoveObjectiveWithRadarEntity(m_teamInfo[teamIndex].m_baseId, m_teamInfo[teamIndex].m_baseId, 0);
		SHUDEventWrapper::OnRemoveObjective(m_teamInfo[teamIndex].m_carrierId, 0);
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Client_SetIconForBase(int teamIndex)
{
	EntityId clientActorId = g_pGame->GetIGameFramework()->GetClientActorId();
	int localTeamId = m_pGameRules->GetTeam(clientActorId);
	bool bFriendly = (localTeamId == (teamIndex + 1));

	const char *pText;
	const char *pColour;

	EGameRulesMissionObjectives icon = EGRMO_Unknown;
	if (m_teamInfo[teamIndex].m_state == ESES_AtBase)
	{
		if (bFriendly)
		{
			icon = m_iconFriendlyBaseWithFlag;
			pText = m_iconStringDefend.c_str();
			pColour = ICON_STRING_FRIENDLY_COLOUR;
		}
		else
		{
			icon = m_iconHostileBaseWithFlag;
			pText = m_iconStringSeize.c_str();
			pColour = ICON_STRING_HOSTILE_COLOUR;
		}
	}
	else
	{
		int otherTeamIndex = 1 - teamIndex;
		if ((m_teamInfo[otherTeamIndex].m_state == ESES_Carried) && (m_teamInfo[otherTeamIndex].m_carrierId == clientActorId))
		{
			pText = m_iconStringBase.c_str();
			pColour = ICON_STRING_NEUTRAL_COLOUR;
			icon = m_iconFriendlyBaseWithNoFlag;
		}
	}

	if (icon != EGRMO_Unknown)
	{
		SHUDEventWrapper::OnNewObjective(m_teamInfo[teamIndex].m_baseId, icon, 0.f, 0, pText, pColour);
	}
	else
	{
		SHUDEventWrapper::OnRemoveObjective(m_teamInfo[teamIndex].m_baseId, 0);
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Client_SetIconForFlag(int teamIndex)
{
	FATAL_ASSERT(gEnv->IsClient());
	EntityId flagId = m_teamInfo[teamIndex].m_flagId;
	if (m_teamInfo[teamIndex].m_state == ESES_Dropped)
	{
		int localTeamId = m_pGameRules->GetTeam(g_pGame->GetIGameFramework()->GetClientActorId());
		bool bFriendly = (localTeamId == (teamIndex + 1));
		if (bFriendly)
		{
			SHUDEventWrapper::OnNewObjectiveWithRadarEntity(flagId, flagId, m_iconFriendlyFlagDropped, 0.f, 0, m_iconStringReturn.c_str(), ICON_STRING_FRIENDLY_COLOUR);
		}
		else
		{
			SHUDEventWrapper::OnNewObjectiveWithRadarEntity(flagId, flagId, m_iconHostileFlagDropped, 0.f, 0, m_iconStringSeize.c_str(), ICON_STRING_HOSTILE_COLOUR);
		}
	}
	else
	{
		SHUDEventWrapper::OnRemoveObjectiveWithRadarEntity(flagId, flagId, 0);
		m_teamInfo[teamIndex].m_droppedIconFrame = 0;
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Client_SetIconForCarrier(int teamIndex, EntityId carrierId, bool bRemoveIcon)
{
	FATAL_ASSERT(gEnv->IsClient());
	if (bRemoveIcon)
	{
		SHUDEventWrapper::OnRemoveObjectiveWithRadarEntity(carrierId, m_teamInfo[teamIndex].m_flagId, 0);
	}
	else
	{
		EGameRulesMissionObjectives icon = EGRMO_Unknown;
		const char *pText = "";
		const char *pColour = "";

		EntityId clientActorId = g_pGame->GetIGameFramework()->GetClientActorId();
		int localTeamId = m_pGameRules->GetTeam(clientActorId);
		bool bFriendly = (localTeamId == (teamIndex + 1));
		if (bFriendly)
		{
			pText = m_iconStringKill;
			pColour = ICON_STRING_HOSTILE_COLOUR;
			icon = m_iconFriendlyFlagCarried;
		}
		else if (carrierId != clientActorId)
		{
			pText = m_iconStringEscort;
			pColour = ICON_STRING_FRIENDLY_COLOUR;
			icon = m_iconHostileFlagCarried;
		}

		if (icon != EGRMO_Unknown)
		{
			SHUDEventWrapper::OnNewObjectiveWithRadarEntity(carrierId, m_teamInfo[teamIndex].m_flagId, icon, 0.f, 0, pText, pColour);
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Client_SetAllIcons()
{
	FATAL_ASSERT(gEnv->IsClient());
	for (int teamIndex = 0; teamIndex < 2; ++ teamIndex)
	{
		Client_SetIconForBase(teamIndex);
		switch (m_teamInfo[teamIndex].m_state)
		{
			case ESES_Carried:
				Client_SetIconForCarrier(teamIndex, m_teamInfo[teamIndex].m_carrierId, false);
				break;
			case ESES_Dropped:
				Client_SetIconForFlag(teamIndex);
				break;
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::Client_UpdatePickupAndDrop()
{
	FATAL_ASSERT(gEnv->IsClient());
	if (m_bUseButtonHeld)
	{
		CPlayer *pClientPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());

		if (pClientPlayer)
		{
			if (pClientPlayer->IsDead() == false)
			{
				const SInteractionInfo& interaction = pClientPlayer->GetCurrentInteractionInfo();

				if (interaction.interactionType == eInteraction_GameRulesPickup)
				{
					CItem *pCurrentItem = static_cast<CItem*>(pClientPlayer->GetCurrentItem());
					if (pCurrentItem != NULL && pCurrentItem->IsRippedOff())
					{
						//--- Drop HMG
						//--- Then reattempt
						if (!pCurrentItem->IsDeselecting())
						{
							pClientPlayer->UseItem(pCurrentItem->GetEntityId());
						}
					}
					else
					{
						// Request pickup
						if (gEnv->bServer)
						{
							Handle_RequestPickup(pClientPlayer->GetEntityId());
						}
						else
						{
							CGameRules::SModuleRMISvClientActionParams params(m_moduleRMIIndex, CGameRules::SModuleRMISvClientActionParams::eACT_HelperCarry_Pickup, NULL);
							m_pGameRules->GetGameObject()->InvokeRMI(CGameRules::SvModuleRMIOnAction(), params, eRMI_ToServer);
						}
						
						m_bUseButtonHeld = false;
					}
				}
				else if (interaction.interactionType == eInteraction_GameRulesDrop)
				{
					// Request drop
					if (gEnv->bServer)
					{
						Handle_RequestDrop(pClientPlayer->GetEntityId());
					}
					else
					{
						CGameRules::SModuleRMISvClientActionParams params(m_moduleRMIIndex, CGameRules::SModuleRMISvClientActionParams::eACT_HelperCarry_Drop, NULL);
						m_pGameRules->GetGameObject()->InvokeRMI(CGameRules::SvModuleRMIOnAction(), params, eRMI_ToServer);
					}

					m_bUseButtonHeld = false;
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
bool CGameRulesObjective_CTF::IsPlayerInPickedUpRecord( EntityId playerId ) const
{
	return std::find(m_playersThatHavePickedUp.begin(), m_playersThatHavePickedUp.end(), playerId) != m_playersThatHavePickedUp.end(); 
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::SetPlayerHasPickedUp( EntityId playerId,  const bool bHasPickedUp )
{
	if(bHasPickedUp)
	{
		stl::push_back_unique(m_playersThatHavePickedUp, playerId);
	}
	else
	{
		for(uint i = 0; i < m_playersThatHavePickedUp.size(); ++i)
		{
			if(m_playersThatHavePickedUp[i] == playerId)
			{
				m_playersThatHavePickedUp.removeAt(i);
				break; 
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesObjective_CTF::ClearPlayerPickedUpRecords()
{
	m_playersThatHavePickedUp.clear(); 
}


// Play nicely with selotaped compiling
#undef FATAL_ASSERT
#undef TEAM_SCORE_EVENT
#undef PLAYER_SCORE_EVENT
#undef PLAY_GLOBAL_SOUND

#undef ICON_STRING_FRIENDLY_COLOUR
#undef ICON_STRING_HOSTILE_COLOUR
#undef ICON_STRING_NEUTRAL_COLOUR

#undef CTF_FLAG_CLASS_NAME
