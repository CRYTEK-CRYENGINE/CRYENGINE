// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HUDEventWrapper.h"
#include "HUDEventDispatcher.h"

//////////////////////////////////////////////////////////////////////////

#include "HUDEventDispatcher.h"
#include "HUDUtils.h"
#include "Audio/GameAudio.h"

//////////////////////////////////////////////////////////////////////////

const SHUDEventWrapper::SMsgAudio  SHUDEventWrapper::kMsgAudioNULL;
CAudioSignalPlayer SHUDEventWrapper::s_unlockSound;
CAudioSignalPlayer SHUDEventWrapper::s_unlockSoundRare;

//////////////////////////////////////////////////////////////////////////

namespace SHUDEventWrapperUtils
{
	void InternalGenericMessageNotify(const EHUDEventType type, const char* msg, const bool loop, const SHUDEventWrapper::SMsgAudio& audio/*=kMsgAudioNULL*/)
	{
		if (!gEnv->IsDedicated())
		{
			SHUDEvent newRoundMessage(type);
			newRoundMessage.AddData(msg);
			newRoundMessage.AddData(loop);
			newRoundMessage.AddData(&audio);
			CHUDEventDispatcher::CallEvent(newRoundMessage);
		}
	}
}

void SHUDEventWrapper::RoundMessageNotify(const char* msg, const SHUDEventWrapper::SMsgAudio& audio/*=kMsgAudioNULL*/)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEventWrapperUtils::InternalGenericMessageNotify( eHUDEvent_OnRoundMessage, msg, false, audio);
	}
}

// Do not localise your message before passing into this
// Double localisation can cause corruption
void SHUDEventWrapper::GameStateNotify( const char* msg, const SHUDEventWrapper::SMsgAudio& audio, const char* p1 /*=NULL*/, const char* p2 /*=NULL*/, const char* p3 /*=NULL*/, const char* p4 /*=NULL*/ )
{
	if (!gEnv->IsDedicated())
	{
		string localisedString = CHUDUtils::LocalizeString(msg, p1, p2, p3, p4);	// cache the string to ensure no subsequent LocalizeString() calls could mess with our result
		SHUDEventWrapperUtils::InternalGenericMessageNotify( eHUDEvent_OnGameStateNotifyMessage, localisedString.c_str(), false, audio);
	}
}
void SHUDEventWrapper::GameStateNotify( const char* msg, const char* p1 /*=NULL*/, const char* p2 /*=NULL*/, const char* p3 /*=NULL*/, const char* p4 /*=NULL*/ )
{
	if (!gEnv->IsDedicated())
	{
    SHUDEventWrapper::GameStateNotify( msg, INVALID_AUDIOSIGNAL_ID, p1, p2, p3, p4);
	}
}

void SHUDEventWrapper::DisplayLateJoinMessage()
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent displayLateJoinMessage(eHUDEvent_DisplayLateJoinMessage);
		CHUDEventDispatcher::CallEvent(displayLateJoinMessage);
	}
}

void SHUDEventWrapper::TeamMessage( const char *msg, const int team, const SHUDEventWrapper::SMsgAudio& audio, const bool bShowTeamName, const bool bShowTeamIcon, const char * pCustomHeader, const float timeToHoldPauseState)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent newTeamMessage(eHUDEvent_OnTeamMessage);
		newTeamMessage.AddData(msg);
		newTeamMessage.AddData(team);
		newTeamMessage.AddData(&audio);
		newTeamMessage.AddData(bShowTeamName);
		newTeamMessage.AddData(bShowTeamIcon);
		newTeamMessage.AddData(pCustomHeader);
		newTeamMessage.AddData(timeToHoldPauseState);
		CHUDEventDispatcher::CallEvent(newTeamMessage);
	}
}

void SHUDEventWrapper::SimpleBannerMessage( const char *msg, const SHUDEventWrapper::SMsgAudio& audio, const float timeToHoldPauseState)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent newBannerMessage(eHUDEvent_OnSimpleBannerMessage);
		newBannerMessage.AddData(msg);
		newBannerMessage.AddData(&audio);
		newBannerMessage.AddData(timeToHoldPauseState);
		CHUDEventDispatcher::CallEvent(newBannerMessage);
	}
}

void SHUDEventWrapper::OnChatMessage( const EntityId entity, const int teamId, const char* message)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent chatMessageEvent(eHUDEvent_OnChatMessage);
		chatMessageEvent.AddData( static_cast<int>(entity) );
		chatMessageEvent.AddData( teamId );
		chatMessageEvent.AddData( message );
		CHUDEventDispatcher::CallEvent(chatMessageEvent);
	}
}

void SHUDEventWrapper::OnAssessmentCompleteMessage( const char* groupName, const char* assesmentName, const char* description, const char* descriptionParam, const int xp )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent xpHudEvent(eHUDEvent_OnAssessmentComplete);
		xpHudEvent.AddData((const void*) groupName);		//Group name, e.g 'Scar Kills'
		xpHudEvent.AddData((const void*) assesmentName);	//Assessment name e.g 'Recruit'
		xpHudEvent.AddData((const void*) description);
		xpHudEvent.AddData((const void*) descriptionParam);
		xpHudEvent.AddData(xp);		//XP given
		CHUDEventDispatcher::CallEvent(xpHudEvent);
	}
}

void SHUDEventWrapper::OnNewMedalMessage( const char* name, const char* description, const char* descriptionParam )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent newMedalEvent(eHUDEvent_OnNewMedalAward);
		newMedalEvent.AddData((const void*) name);		     // title
		newMedalEvent.AddData((const void*) description); // description
		newMedalEvent.AddData((const void*) descriptionParam);
		CHUDEventDispatcher::CallEvent(newMedalEvent);
	}
}

void SHUDEventWrapper::OnPromotionMessage( const char* rankName, const int rank, const int xpRequired )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent newPromotionEvent(eHUDEvent_OnPromotionMessage);
		newPromotionEvent.AddData((const void*) rankName);		     // rank name, will be localised with @pp_promoted.
		newPromotionEvent.AddData(rank);                           // rank id for icon.
		newPromotionEvent.AddData(xpRequired);
		CHUDEventDispatcher::CallEvent(newPromotionEvent);
	}
}

void SHUDEventWrapper::OnSupportBonusXPMessage( const int xpType /*EPPType*/, const int xpGained )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent xpHudEvent(eHUDEvent_OnNewSkillKill);
		xpHudEvent.AddData(xpType);   // type info EPPType
		xpHudEvent.AddData(xpGained); // XP Points
		xpHudEvent.AddData(true);
		CHUDEventDispatcher::CallEvent(xpHudEvent);
	}
}

void SHUDEventWrapper::OnSkillKillMessage( const int xpType /*EPPType*/, const int xpGained )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent skillKillEvent(eHUDEvent_OnNewSkillKill);
		skillKillEvent.AddData(xpType);   // type info EPPType
		skillKillEvent.AddData(xpGained); // XP Points
		skillKillEvent.AddData(false);
		CHUDEventDispatcher::CallEvent(skillKillEvent);
	}
}

// Set Message to NULL to clear.
void SHUDEventWrapper::OnGameStatusUpdate( const EGoodBadNeutralForLocalPlayer good, const char* message )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent gameSatusMessageEvent(eHUDEvent_OnSetGameStateMessage);
		gameSatusMessageEvent.AddData((int)good);         // for colours/anims
		gameSatusMessageEvent.AddData((void*)message);    // XP Points
		CHUDEventDispatcher::CallEvent(gameSatusMessageEvent);
	}
}

void SHUDEventWrapper::OnRoundEnd( const int winner, const bool clientScoreIsTop, const char* victoryDescMessage, const char* victoryMessage, const EAnnouncementID announcement )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent endRoundMessage(eHUDEvent_OnRoundEnd);
		endRoundMessage.AddData( SHUDEventData(winner) );
		endRoundMessage.AddData( SHUDEventData(clientScoreIsTop) );
		endRoundMessage.AddData( SHUDEventData(victoryDescMessage) );
		endRoundMessage.AddData( SHUDEventData(victoryMessage) );
		endRoundMessage.AddData( SHUDEventData((int) announcement) );
		CHUDEventDispatcher::CallEvent(endRoundMessage);
	}
}

void SHUDEventWrapper::OnSuddenDeath()
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent suddenDeathMessage(eHUDEvent_OnSuddenDeath);
		CHUDEventDispatcher::CallEvent(suddenDeathMessage);
	}
}

void SHUDEventWrapper::OnGameEnd(const int teamOrPlayerId, const bool clientScoreIsTop, EGameOverReason reason, const char* message, ESVC_DrawResolution drawResolution, const EAnnouncementID announcement)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent endGameMessage(eHUDEvent_GameEnded);
		endGameMessage.AddData(SHUDEventData(teamOrPlayerId));
		endGameMessage.AddData(SHUDEventData(clientScoreIsTop));
		endGameMessage.AddData(SHUDEventData((int)reason));
		endGameMessage.AddData(SHUDEventData(message));
		endGameMessage.AddData(SHUDEventData((int) drawResolution));
		endGameMessage.AddData(SHUDEventData((int) announcement));
		CHUDEventDispatcher::CallEvent(endGameMessage);
	}
}

void SHUDEventWrapper::UpdateGameStartCountdown(const EPreGameCountdownType countdownType, const float timeTillStartInSeconds)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent prematchEvent;
		prematchEvent.eventType = eHUDEvent_OnUpdateGameStartMessage;
		prematchEvent.AddData(timeTillStartInSeconds);
		prematchEvent.AddData((int)countdownType);
		CHUDEventDispatcher::CallEvent(prematchEvent);
	}
}

void SHUDEventWrapper::CantFire( void )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent cantfire( eHUDEvent_CantFire );
		CHUDEventDispatcher::CallEvent(cantfire);
	}
}

void SHUDEventWrapper::FireModeChanged( IWeapon* pWeapon, const int currentFireMode, bool bForceFireModeUpdate /* = false */ )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent event(eHUDEvent_OnFireModeChanged);
		event.AddData(SHUDEventData((void*)pWeapon));
		event.AddData(SHUDEventData(currentFireMode));
		event.AddData(SHUDEventData(bForceFireModeUpdate));

		CHUDEventDispatcher::CallEvent(event);
	}
}

void SHUDEventWrapper::ForceCrosshairType( IWeapon* pWeapon, const ECrosshairTypes desiredCrosshairType )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent event(eHUDEvent_ForceCrosshairType);
		event.AddData(SHUDEventData((void*)pWeapon));
		event.AddData(SHUDEventData(desiredCrosshairType));

		CHUDEventDispatcher::CallEvent(event);
	}
}

void SHUDEventWrapper::OnInteractionUseHoldTrack(const bool bTrackingUse)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent interactionEvent(eHUDEvent_OnInteractionUseHoldTrack);
		interactionEvent.AddData(SHUDEventData(bTrackingUse));
		CHUDEventDispatcher::CallEvent(interactionEvent);
	}
}

void SHUDEventWrapper::OnInteractionUseHoldActivated(const bool bSuccessful)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent interactionEvent(eHUDEvent_OnInteractionUseHoldActivated);
		interactionEvent.AddData(SHUDEventData(bSuccessful));
		CHUDEventDispatcher::CallEvent(interactionEvent);
	}
}

void SHUDEventWrapper::InteractionRequest( const bool activate, const char* msg, const char* action, const char* actionmap, const float duration, const bool bGameRulesRequest, const bool bIsFlowNodeReq, const bool bShouldSerialize )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent interactionEvent(eHUDEvent_OnInteractionRequest);
		interactionEvent.AddData(SHUDEventData(activate));
		interactionEvent.AddData(SHUDEventData(msg));
		interactionEvent.AddData(SHUDEventData(action));
		interactionEvent.AddData(SHUDEventData(actionmap));
		interactionEvent.AddData(SHUDEventData(duration));
		interactionEvent.AddData(SHUDEventData(bGameRulesRequest));
		interactionEvent.AddData(SHUDEventData(bIsFlowNodeReq));
		interactionEvent.AddData(SHUDEventData(bShouldSerialize));
		CHUDEventDispatcher::CallEvent(interactionEvent);
	}
}

void SHUDEventWrapper::ClearInteractionRequest( const bool bGameRulesRequest ) // TODO: Remove this and all uses of it, should use ClearInteractionMsg
{
	if (!gEnv->IsDedicated())
	{
		SHUDEventWrapper::InteractionRequest( false, NULL, NULL, NULL, -1.0f, bGameRulesRequest );
	}
}

void SHUDEventWrapper::SetInteractionMsg(const EHUDInteractionMsgType interactionType, 
																										const char* message, 
																										const float duration, 
																										const bool bShouldSerialize, 
																										const char* interactionAction/*= NULL*/,
																										const char* interactionActionmap/* = NULL*/,
																										const EntityId usableEntityId/* = 0*/,
																										const EntityId usableSwapEntityId/* = 0*/,
																										const EInteractionType usableInteractionType/* = eInteraction_None*/,
																										const char* szExtraParam1/* = NULL*/)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent event(eHUDEvent_OnSetInteractionMsg);
		event.AddData(SHUDEventData((const int)interactionType));
		event.AddData(SHUDEventData(message));
		event.AddData(SHUDEventData(duration));
		event.AddData(SHUDEventData(bShouldSerialize));
		event.AddData(SHUDEventData(interactionAction));
		event.AddData(SHUDEventData(interactionActionmap));
		event.AddData(SHUDEventData((const int)usableEntityId));
		event.AddData(SHUDEventData((const int)usableSwapEntityId));
		event.AddData(SHUDEventData((const int)usableInteractionType));
		event.AddData(SHUDEventData(szExtraParam1));
	
		CHUDEventDispatcher::CallEvent(event);	
	}
}

void SHUDEventWrapper::ClearInteractionMsg(EHUDInteractionMsgType interactionType, const char* onlyRemoveMsg ,const float fadeOutTime /*= -1.0f*/)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent event(eHUDEvent_OnClearInteractionMsg);
		event.AddData(SHUDEventData((int)interactionType));
		event.AddData(SHUDEventData(onlyRemoveMsg));
		event.AddData(SHUDEventData(fadeOutTime));
		CHUDEventDispatcher::CallEvent(event);	
	}
}

void SHUDEventWrapper::DisplayInfo( const int system, const float time, const char* msg, const char* param1/*=NULL*/, const char* param2/*=NULL*/, const char* param3/*=NULL*/, const char* param4/*=NULL*/, const EInfoSystemPriority priority/*=eInfoPriority_Low*/)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent event(eHUDEvent_InfoSystemsEvent);
		event.AddData(SHUDEventData(system));
		event.AddData(SHUDEventData(time));
		event.AddData(SHUDEventData(msg));
		event.AddData(SHUDEventData(param1));
		event.AddData(SHUDEventData(param2));
		event.AddData(SHUDEventData(param3));
		event.AddData(SHUDEventData(param4));
		event.AddData(SHUDEventData((int)priority));
		CHUDEventDispatcher::CallEvent(event);
	}
}

void SHUDEventWrapper::ClearDisplayInfo( const int system, const EInfoSystemPriority priority/*=eInfoPriority_Low*/)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEventWrapper::DisplayInfo( system, 0.0f, NULL, NULL, NULL, NULL, NULL, priority);
	}
}

void SHUDEventWrapper::GenericBattleLogMessage(const EntityId actorId, const char *message, const char* p1 /*=NULL*/, const char* p2 /*=NULL*/, const char* p3 /*=NULL*/, const char* p4 /*=NULL*/ )
{
	if (!gEnv->IsDedicated())
	{
		string localisedString = CHUDUtils::LocalizeString(message, p1, p2, p3, p4);	// cache the string to ensure no subsequent LocalizeString() calls could mess with our result
	
		SHUDEvent genericMessageEvent(eHUDEvent_OnGenericBattleLogMessage);
		genericMessageEvent.AddData(static_cast<int>(actorId));
		genericMessageEvent.AddData(localisedString.c_str());
		CHUDEventDispatcher::CallEvent(genericMessageEvent);
	}
}

void SHUDEventWrapper::HitTarget( EGameRulesTargetType targetType, int bulletType, EntityId targetId )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent hudEvent(eHUDEvent_OnHitTarget);
		hudEvent.AddData( static_cast<int>(targetType) );
		hudEvent.AddData( bulletType );
		hudEvent.AddData( static_cast<int>(targetId) );
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

void SHUDEventWrapper::OnGameStateMessage( const EntityId actorId, const bool bIsFriendly, const char* pMessage, EBL_IconFrames icon)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent hudEvent(eHUDEvent_OnNewGameStateMessage);
		hudEvent.AddData(static_cast<int>(actorId));
		hudEvent.AddData(bIsFriendly);
		hudEvent.AddData(static_cast<const void*>(pMessage));
		hudEvent.AddData(static_cast<int>(icon));
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

void SHUDEventWrapper::OnNewObjective( const EntityId entityId, const EGameRulesMissionObjectives iconType )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent hudEvent(eHUDEvent_OnNewObjective);
		hudEvent.AddData( static_cast<int>(entityId) );
		hudEvent.AddData( static_cast<int>(iconType) );
		CRY_ASSERT(hudEvent.GetDataSize() != 7);	//The HUD_Radar will need updating if this changes
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

void SHUDEventWrapper::OnNewObjective( const EntityId entityId, const EGameRulesMissionObjectives iconType, const float progress, const int priority )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent hudEvent(eHUDEvent_OnNewObjective);
		hudEvent.AddData( static_cast<int>(entityId) );
		hudEvent.AddData( static_cast<int>(iconType) );
		hudEvent.AddData( progress ); 
		hudEvent.AddData( priority );
		CRY_ASSERT(hudEvent.GetDataSize() != 7);	//The HUD_Radar will need updating if this changes
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

void SHUDEventWrapper::OnNewObjective( const EntityId entityId, const EGameRulesMissionObjectives iconType, const float progress, const int priority, const char *pNameOverride, const char *pColourStr )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent hudEvent(eHUDEvent_OnNewObjective);
		hudEvent.AddData( static_cast<int>(entityId) );
		hudEvent.AddData( static_cast<int>(iconType) );
		hudEvent.AddData( progress ); 
		hudEvent.AddData( priority );
		hudEvent.AddData( pNameOverride );
		hudEvent.AddData( pColourStr );
		CRY_ASSERT(hudEvent.GetDataSize() != 7);	//The HUD_Radar will need updating if this changes
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

void SHUDEventWrapper::OnNewObjectiveWithRadarEntity( const EntityId entityId, const EntityId radarEntityId, const EGameRulesMissionObjectives iconType, const float progress, const int priority, const char *pNameOverride, const char *pColourStr )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent hudEvent(eHUDEvent_OnNewObjective);
		hudEvent.AddData( static_cast<int>(entityId) );
		hudEvent.AddData( static_cast<int>(iconType) );
		hudEvent.AddData( progress ); 
		hudEvent.AddData( priority );
		hudEvent.AddData( pNameOverride );
		hudEvent.AddData( pColourStr );
		hudEvent.AddData( static_cast<int>(radarEntityId) );
		CRY_ASSERT(hudEvent.GetDataSize() == 7);	//The HUD_Radar will need updating if this changes
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

void SHUDEventWrapper::OnRemoveObjective( const EntityId entityId )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent hudEvent(eHUDEvent_OnRemoveObjective);
		hudEvent.AddData( static_cast<int>(entityId) );
		CRY_ASSERT(hudEvent.GetDataSize() != 3);	//The HUD_Radar will need updating if this changes
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

void SHUDEventWrapper::OnRemoveObjective( const EntityId entityId, const int priority )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent hudEvent(eHUDEvent_OnRemoveObjective);
		hudEvent.AddData( static_cast<int>(entityId) );
		hudEvent.AddData( priority );
		CRY_ASSERT(hudEvent.GetDataSize() != 3);	//The HUD_Radar will need updating if this changes
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

void SHUDEventWrapper::OnRemoveObjectiveWithRadarEntity( const EntityId entityId, const EntityId radarEntityId, const int priority )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent hudEvent(eHUDEvent_OnRemoveObjective);
		hudEvent.AddData( static_cast<int>(entityId) );
		hudEvent.AddData( priority );
		hudEvent.AddData( static_cast<int>(radarEntityId) );
		CRY_ASSERT(hudEvent.GetDataSize() == 3);	//The HUD_Radar will need updating if this changes
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

void SHUDEventWrapper::OnNewGameRulesObjective( const EntityId entityId )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent hudEvent(eHUDEvent_OnNewGameRulesObjective);
		hudEvent.AddData( static_cast<int>(entityId) );
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

void SHUDEventWrapper::OnBootAction(const uint32 bootAction, const char* elementName)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent hudEvent(eHUDEvent_HUDBoot);
		hudEvent.AddData(static_cast<int>(bootAction));
		if (elementName && elementName[0])
		{
			hudEvent.AddData((void*)elementName);
		}
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

void SHUDEventWrapper::ActivateOverlay(const int type)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent event(eHUDEvent_ActivateOverlay);
		event.AddData(SHUDEventData(type));
		CHUDEventDispatcher::CallEvent(event);
	}
}

void SHUDEventWrapper::DeactivateOverlay(const int type)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent event(eHUDEvent_DeactivateOverlay);
		event.AddData(SHUDEventData(type));
		CHUDEventDispatcher::CallEvent(event);
	}
}

void SHUDEventWrapper::OnPrepareItemSelected(const EntityId itemId, const EItemCategoryType category, const EWeaponSwitchSpecialParam specialParam )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent eventPrepareItemSelected(eHUDEvent_OnPrepareItemSelected);
		eventPrepareItemSelected.AddData(SHUDEventData((int)itemId));
		eventPrepareItemSelected.AddData(SHUDEventData((int)category));
		eventPrepareItemSelected.AddData(SHUDEventData((int)specialParam));
		CHUDEventDispatcher::CallEvent(eventPrepareItemSelected);
	}
}

void SHUDEventWrapper::PlayerRename(const EntityId playerId, const bool isReplay /*= false*/)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent renameEvent(eHUDEvent_RenamePlayer);
		renameEvent.AddData(SHUDEventData((int)playerId));
		renameEvent.AddData(SHUDEventData(isReplay));
		CHUDEventDispatcher::CallEvent(renameEvent);	
	}
}

void SHUDEventWrapper::OnPlayerLinkedToVehicle( const EntityId playerId, const EntityId vehicleId, const bool alwaysShowHealth )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent linkEvent(eHUDEvent_PlayerLinkedToVehicle);
		linkEvent.AddData(SHUDEventData((int)playerId));
		linkEvent.AddData(SHUDEventData((int)vehicleId));
		linkEvent.AddData(SHUDEventData(alwaysShowHealth));
		CHUDEventDispatcher::CallEvent(linkEvent);		
	}
}

void SHUDEventWrapper::OnPlayerUnlinkedFromVehicle( const EntityId playerId, const EntityId vehicleId, const bool keepHealthBar )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent linkEvent(eHUDEvent_PlayerUnlinkedFromVehicle);
		linkEvent.AddData(SHUDEventData((int)playerId));
		linkEvent.AddData(SHUDEventData((int)vehicleId));
		linkEvent.AddData(SHUDEventData(keepHealthBar));
		CHUDEventDispatcher::CallEvent(linkEvent);
	}
}

void SHUDEventWrapper::OnStartTrackFakePlayerTagname( const EntityId tagEntityId, const int sessionNameIdxToTrack )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent linkEvent(eHUDEvent_OnStartTrackFakePlayerTagname);
		linkEvent.AddData(SHUDEventData((int)tagEntityId));
		linkEvent.AddData(SHUDEventData((int)sessionNameIdxToTrack));
		CHUDEventDispatcher::CallEvent(linkEvent);
	}
}

void SHUDEventWrapper::OnStopTrackFakePlayerTagname( const EntityId tagEntityId )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent linkEvent(eHUDEvent_OnEndTrackFakePlayerTagname);
		linkEvent.AddData(SHUDEventData((int)tagEntityId));
		CHUDEventDispatcher::CallEvent(linkEvent);
	}
}

void SHUDEventWrapper::PowerStruggleNodeStateChange(const int activeNodeIdentityId, const int nodeHUDState)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent nodeStateChange(eHUDEvent_OnPowerStruggle_NodeStateChange);
		nodeStateChange.AddData(SHUDEventData(activeNodeIdentityId));
		nodeStateChange.AddData(SHUDEventData(nodeHUDState));
		CHUDEventDispatcher::CallEvent(nodeStateChange);
	}
}

void SHUDEventWrapper::PowerStruggleManageCaptureBar(EHUDPowerStruggleCaptureBarType inType, float inChargeAmount, bool inContention, const char *inBarString)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent manageCaptureBar(eHUDEvent_OnPowerStruggle_ManageCaptureBar);
		manageCaptureBar.AddData(SHUDEventData(static_cast<int>(inType)));
		manageCaptureBar.AddData(SHUDEventData(inChargeAmount));
		manageCaptureBar.AddData(SHUDEventData(inContention));
		manageCaptureBar.AddData(SHUDEventData(inBarString));
		CHUDEventDispatcher::CallEvent(manageCaptureBar);
	}
}

void SHUDEventWrapper::OnBigMessage(const char *inSubTitle, const char *inTitle)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent bigMessage(eHUDEvent_OnBigMessage);
		bigMessage.AddData(inSubTitle);
		bigMessage.AddData(inTitle);
		CHUDEventDispatcher::CallEvent(bigMessage);
	}
}

void SHUDEventWrapper::OnBigWarningMessage(const char *line1, const char *line2, const float duration)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent bigWarningMessage(eHUDEvent_OnBigWarningMessage);
		bigWarningMessage.AddData(line1);
		bigWarningMessage.AddData(line2);
		bigWarningMessage.AddData(duration);
		CHUDEventDispatcher::CallEvent(bigWarningMessage);
	}
}

void SHUDEventWrapper::OnBigWarningMessageUnlocalized(const char *line1, const char *line2, const float duration)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent bigWarningMessage(eHUDEvent_OnBigWarningMessageUnlocalized);
		bigWarningMessage.AddData(line1);
		bigWarningMessage.AddData(line2);
		bigWarningMessage.AddData(duration);
		CHUDEventDispatcher::CallEvent(bigWarningMessage);
	}
}

void SHUDEventWrapper::UpdatedDirectionIndicator(const EntityId &id)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent updatedDirectionIndicator(eHUDEvent_OnShowHitIndicatorBothUpdated);
		updatedDirectionIndicator.AddData((int)id);
		CHUDEventDispatcher::CallEvent(updatedDirectionIndicator);
	}
}

void SHUDEventWrapper::SetStaticTimeLimit(bool active, int time)
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent staticTimeLimitMessage(eHUDEvent_OnSetStaticTimeLimit);
		staticTimeLimitMessage.AddData(active);
		staticTimeLimitMessage.AddData(time);
		CHUDEventDispatcher::CallEvent(staticTimeLimitMessage);
	}
}

void SHUDEventWrapper::PlayUnlockSound()
{
	if (!gEnv->IsDedicated())
		{
		if(!s_unlockSound.HasValidSignal())
		{
			s_unlockSound.SetSignal("HUD_CollectiblePickUp");
		}
		REINST("needs verification!");
		//s_unlockSound.Play();
	}
}

void SHUDEventWrapper::PlayUnlockSoundRare()
{
	if (!gEnv->IsDedicated())
	{
		if(!s_unlockSoundRare.HasValidSignal())
		{
			s_unlockSoundRare.SetSignal("HUD_CollectiblePickUpRare");
		}
		REINST("needs verification!");
		//s_unlockSoundRare.Play();
	}
}

void SHUDEventWrapper::DisplayWeaponUnlockMsg(const char* szCollectibleId)
{
	SHUDEventWrapper::PlayUnlockSound();
}

void SHUDEventWrapper::RadarSweepActivated( int targetTeam, float activateDuration )
{
	if (!gEnv->IsDedicated())
	{
		SHUDEvent radarSweepMessage(eHUDEvent_OnRadarSweep);
		radarSweepMessage.AddData(targetTeam);
		radarSweepMessage.AddData(activateDuration);
		CHUDEventDispatcher::CallEvent(radarSweepMessage);
	}
}
