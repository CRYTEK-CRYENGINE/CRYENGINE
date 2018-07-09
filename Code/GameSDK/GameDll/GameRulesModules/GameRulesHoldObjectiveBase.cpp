// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Base implementation for a take and hold objective
			- Handles keeping track of who is inside the objective area

	-------------------------------------------------------------------------
	History:
	- 10:02:2010  : Created by Colin Gulliver

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesHoldObjectiveBase.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "Player.h"
#include "GameRulesModules/IGameRulesScoringModule.h"
#include "Utility/CryWatch.h"
#include <CryString/StringUtils.h>
#include "Utility/CryDebugLog.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "Audio/Announcer.h"
#include "PersistantStats.h"

#if NUM_ASPECTS > 8
	#define HOLD_OBJECTIVE_STATE_ASPECT		eEA_GameServerA
#else
	#define HOLD_OBJECTIVE_STATE_ASPECT		eEA_GameServerStatic
#endif

enum EHoldObjectiveBaseDebugView
{
	eHOB_Debug_Draw_Sphere = 1,
	eHOB_Debug_Fade_Ring_With_Pulse = 2
};

#define HOLD_OBJECTIVE_TRACKVIEW_DEFERRED_TIME 1.f

//------------------------------------------------------------------------
CGameRulesHoldObjectiveBase::CGameRulesHoldObjectiveBase()
{
	m_spawnPOIType = eSPT_None;
	m_spawnPOIDistance = 0.f;
	m_currentPulseType = eRPT_Neutral;
	m_shouldPlayIncomingAudio = false;
	m_bHasNetSerialized = false;
	m_bExpectingMovieStart = false;
	m_bAddedMovieListener = false;
	m_pStartingAnimSequence = NULL;
	m_deferredTrackViewTime = 0.f;

	g_pGame->GetGameRules()->RegisterClientConnectionListener(this);
	g_pGame->GetGameRules()->RegisterKillListener(this);
	g_pGame->GetGameRules()->RegisterTeamChangedListener(this);

	g_pGame->GetIGameFramework()->RegisterListener(this, "holdObjective", FRAMEWORKLISTENERPRIORITY_GAME);
}

//------------------------------------------------------------------------
CGameRulesHoldObjectiveBase::~CGameRulesHoldObjectiveBase()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->UnRegisterClientConnectionListener(this);
		pGameRules->UnRegisterKillListener(this);
		pGameRules->UnRegisterTeamChangedListener(this);
	}

	g_pGame->GetIGameFramework()->UnregisterListener(this);

	if (gEnv->pMovieSystem && m_bAddedMovieListener)
	{
		CryLog("CGameRulesHoldObjectiveBase::CGameRulesHoldObjectiveBase() removing movie listener");
		gEnv->pMovieSystem->RemoveMovieListener(NULL, this);
	}

	SAFE_RELEASE(m_effectData.pParticleGeomMaterial);
	SAFE_RELEASE(m_effectData.pParticleEffect);
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::Init( XmlNodeRef xml )
{
	const int numChildren = xml->getChildCount();
	for (int childIdx = 0; childIdx < numChildren; ++ childIdx)
	{
		XmlNodeRef xmlChild = xml->getChild(childIdx);
		if (!stricmp(xmlChild->getTag(), "SpawnParams"))
		{
			const char *pType = 0;
			if (xmlChild->getAttr("type", &pType))
			{
				if (!stricmp(pType, "avoid"))
				{
					m_spawnPOIType = eSPT_Avoid;
				}
				else
				{
					CryLog("CGameRulesHoldObjectiveBase::Init: ERROR: Unknown spawn point of interest type ('%s')", pType);
				}
				xmlChild->getAttr("distance", m_spawnPOIDistance);
			}
		}
		else if (!stricmp(xmlChild->getTag(), "EffectData"))
		{
			InitEffectData(xmlChild);
		}
	}

	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		m_entities[i].Reset();
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::InitEffectData(XmlNodeRef xmlEffectNode)
{
	const int numChildren = xmlEffectNode->getChildCount();
	for(int childIdx=0; childIdx<numChildren; childIdx++)
	{
		XmlNodeRef xmlChild = xmlEffectNode->getChild(childIdx);
		if(!stricmp(xmlChild->getTag(), "ParticleEffect"))
		{
			const char* pParticleEffectName = NULL;
			if (xmlChild->getAttr("name", &pParticleEffectName))
			{
				m_effectData.pParticleEffect = gEnv->pParticleManager->FindEffect(pParticleEffectName, "CGameRulesHoldObjectiveBase");
				if(m_effectData.pParticleEffect)
				{
					m_effectData.pParticleEffect->AddRef();
				}
			}
			const char* pParticleGeomMaterialName = NULL;
			if (xmlChild->getAttr("particleGeomMaterial", &pParticleGeomMaterialName))
			{
				m_effectData.pParticleGeomMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(pParticleGeomMaterialName);
				if(m_effectData.pParticleGeomMaterial)
				{
					m_effectData.pParticleGeomMaterial->AddRef();
				}
			}
		}
		else if(!stricmp(xmlChild->getTag(), "RingEntity"))
		{
			const char* pRingEntityGeomName = NULL;
			if (xmlChild->getAttr("geom", &pRingEntityGeomName))
			{
				// Create ring effect entity
				SEntitySpawnParams ringSpawnParams;
				ringSpawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
				ringSpawnParams.sName = "crashSiteRing";
				ringSpawnParams.nFlags = ENTITY_FLAG_NO_PROXIMITY | ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_NO_SAVE;
				IEntity* pRingEntity = gEnv->pEntitySystem->SpawnEntity(ringSpawnParams);
				if(pRingEntity)
				{
					pRingEntity->LoadGeometry(0,pRingEntityGeomName);
					pRingEntity->Hide(true);
					m_effectData.ringEntityID = pRingEntity->GetId();
				}
			}
		}
		else if(!stricmp(xmlChild->getTag(), "Params"))
		{
			float param = 0.0f;

			xmlChild->getAttr("alphaLerpSpeed", param);
			m_effectData.alphaLerp.SetLerpSpeed(param);

			xmlChild->getAttr("materialColorLerpSpeed", param);
			m_effectData.materialColorLerp.SetLerpSpeed(param);

			xmlChild->getAttr("particleColorLerpSpeed", param);
			m_effectData.particleColorLerp.SetLerpSpeed(param);

			xmlChild->getAttr("alphaFadeInDelayDuration", m_effectData.alphaFadeInDelayDuration);
			xmlChild->getAttr("particleEffectScale", m_effectData.particleEffectScale);
			xmlChild->getAttr("ringGeomScale", m_effectData.ringGeomScale);

			xmlChild->getAttr("friendlyColor", m_effectData.friendlyColor);
			xmlChild->getAttr("enemyColor", m_effectData.enemyColor);
			xmlChild->getAttr("neutralColor", m_effectData.neutralColor);
		}
	}

	SetNewEffectColor(eRPT_Neutral);
}

#ifndef _RELEASE
//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::DebugDrawCylinder(SHoldEntityDetails *pDetails)
{
	// Draw debug cylinder
	if(g_pGameCVars->g_holdObjectiveDebug == eHOB_Debug_Draw_Sphere)
	{
		IEntity *pHoldEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
		if (pHoldEntity)
		{

			IRenderAuxGeom* pAuxRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
			SAuxGeomRenderFlags renderFlags = pAuxRenderer->GetRenderFlags();
			renderFlags.SetAlphaBlendMode(e_AlphaBlended);
			pAuxRenderer->SetRenderFlags(renderFlags);

			pAuxRenderer->DrawCylinder(	pHoldEntity->GetPos()+Vec3(0.f,0.f,pDetails->m_controlOffsetZ+(pDetails->m_controlHeight*0.5f)),
																	Vec3(0.0f,0.0f,1.0f),
																	pDetails->m_controlRadius,
																	pDetails->m_controlHeight,
																	ColorB(255,255,0,128));
		}
	}
}
#endif

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::AddEntityId(int type, EntityId entityId, int index, bool isNewEntity, const CTimeValue &timeAdded)
{
	if(index >= HOLD_OBJECTIVE_MAX_ENTITIES)
	{
		CryFatalError("Too Many Hold Objective Entities Active!");
	}
	CryLog("CGameRulesHoldObjectiveBase::AddEntityId() received objective, eid=%i, index=%i, isNewEntity=%s, fTimeAdded=%" PRIi64 ", adding to pending queue", entityId, index, isNewEntity ? "true" : "false", timeAdded.GetMilliSecondsAsInt64());

	SHoldEntityDetails *pDetails = &m_entities[index];
	if (pDetails->m_id)
	{
		// We're overwriting another entity, disable the old one
		CleanUpEntity(pDetails);
	}
	// Can't actually activate the entity here since the entity may not be fully initialised yet (horrible ordering issue on game restart),
	// instead set it as pending and do the actual add in the ::Update(...) method
	pDetails->m_pendingId = entityId;
	pDetails->m_isNewEntity = isNewEntity;
	m_pendingTimeAdded = timeAdded;
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::DoAddEntityId(int type, EntityId entityId, int index, bool isNewEntity)
{
	CRY_ASSERT(index < HOLD_OBJECTIVE_MAX_ENTITIES);
	CryLog("CGameRulesHoldObjectiveBase::DoAddEntityId() received objective, eid=%i, index=%i", entityId, index);

	SHoldEntityDetails *pDetails = &m_entities[index];

	pDetails->m_id = entityId;

	if (m_spawnPOIType == eSPT_Avoid)
	{
		IGameRulesSpawningModule *pSpawningModule = g_pGame->GetGameRules()->GetSpawningModule();
		if (pSpawningModule)
		{
			pSpawningModule->AddAvoidPOI(entityId, m_spawnPOIDistance, true, AreObjectivesStatic());
		}
	}

	OnNewHoldEntity(pDetails, index);
	CCCPOINT(HoldObjective_AddEntity);

	gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_ENTERAREA, this);
	gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_LEAVEAREA, this);

	if (gEnv->bServer)
	{
		CHANGED_NETWORK_STATE(g_pGame->GetGameRules(), HOLD_OBJECTIVE_STATE_ASPECT);
	}

	if(isNewEntity)
	{
		//Not playing for the first time because it clashes with game start
		Announce("Incoming", k_announceType_CS_Incoming, m_shouldPlayIncomingAudio);
		m_shouldPlayIncomingAudio = true;
	}

	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	if (pEntity)
	{
		IScriptTable *pScript = pEntity->GetScriptTable();
		CRY_TODO(11, 02, 2009, "function name from xml?");
		if (pScript)
		{
			if (pScript->GetValueType("ActivateCapturePoint") == svtFunction)
			{
				if (isNewEntity)
				{
					// Set flag to say we're expecting a trackview to start - so we can set the start position in the callback
					m_bExpectingMovieStart = true;

					if (!m_bAddedMovieListener)
					{
						if (gEnv->pMovieSystem)
						{
							CryLog("CGameRulesHoldObjectiveBase::CGameRulesHoldObjectiveBase() adding movie listener");
							gEnv->pMovieSystem->AddMovieListener(NULL, this);
							m_bAddedMovieListener = true;
						}
					}
				}

				IScriptSystem *pScriptSystem = gEnv->pScriptSystem;
				pScriptSystem->BeginCall(pScript, "ActivateCapturePoint");
				pScriptSystem->PushFuncParam(pScript);
				pScriptSystem->PushFuncParam(isNewEntity);
				pScriptSystem->EndCall();
			}
			SmartScriptTable propertiesTable;
			if (pScript->GetValue("Properties", propertiesTable))
			{
				pDetails->m_controlRadius = 5.f;
				propertiesTable->GetValue("ControlRadius", pDetails->m_controlRadius);
				pDetails->m_controlRadiusSqr = (pDetails->m_controlRadius * pDetails->m_controlRadius);
				pDetails->m_controlHeight = 5.f;
				propertiesTable->GetValue("ControlHeight", pDetails->m_controlHeight);
				pDetails->m_controlOffsetZ = 0.f;
				propertiesTable->GetValue("ControlOffsetZ", pDetails->m_controlOffsetZ);
			}
		}

		const IActor *pLocalPlayer = g_pGame->GetIGameFramework()->GetClientActor();
		if (pLocalPlayer != NULL && IsActorEligible(pLocalPlayer))
		{
			CheckLocalPlayerInside(pDetails, pEntity, pLocalPlayer->GetEntity());
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::RemoveEntityId( int type, EntityId entityId )
{
	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		SHoldEntityDetails *pDetails = &m_entities[i];
		if (pDetails->m_id == entityId)
		{
			CleanUpEntity(pDetails);
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::ClearEntities(int type)
{
	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		SHoldEntityDetails *pDetails = &m_entities[i];
		if (pDetails->m_id)
		{
			CleanUpEntity(pDetails);
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::CleanUpEntity(SHoldEntityDetails *pDetails)
{
	CCCPOINT(HoldObjective_CleanUpActiveCaptureEntity);

	if (pDetails->m_localPlayerIsWithinRange)
	{		
		CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnSiteAboutToExplode));
	}

	OnRemoveHoldEntity(pDetails);

	gEnv->pEntitySystem->RemoveEntityEventListener(pDetails->m_id, ENTITY_EVENT_ENTERAREA, this);
	gEnv->pEntitySystem->RemoveEntityEventListener(pDetails->m_id, ENTITY_EVENT_LEAVEAREA, this);

	if (m_spawnPOIType == eSPT_Avoid)
	{
		IGameRulesSpawningModule *pSpawningModule = g_pGame->GetGameRules()->GetSpawningModule();
		if (pSpawningModule)
		{
			pSpawningModule->RemovePOI(pDetails->m_id);
		}
	}

	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
	if (pEntity)
	{
		IScriptTable *pScript = pEntity->GetScriptTable();
		if (pScript != NULL && pScript->GetValueType("DeactivateCapturePoint") == svtFunction)
		{
			IScriptSystem *pScriptSystem = gEnv->pScriptSystem;
			pScriptSystem->BeginCall(pScript, "DeactivateCapturePoint");
			pScriptSystem->PushFuncParam(pScript);
			pScriptSystem->PushFuncParam(true);
			pScriptSystem->EndCall();
		}

		CGameRules *pGameRules = g_pGame->GetGameRules();
		EGameMode gamemode = pGameRules->GetGameMode();
		if (gamemode == eGM_CrashSite)
		{
			Announce("Destruct", k_announceType_CS_Destruct);
		}
	}

	pDetails->Reset();

	// Fade out effect
	m_effectData.alphaLerp.Set(1.0f,0.0f,0.0f);
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::OnEntityEvent( IEntity *pEntity, const SEntityEvent& event )
{
	EntityId insideId = (EntityId) event.nParam[0];
	int teamId = g_pGame->GetGameRules()->GetTeam(insideId);

	IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(insideId);
	if (pActor != NULL && pActor->IsPlayer() && teamId)
	{
		SHoldEntityDetails *pDetails = NULL;

		for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
		{
			if (m_entities[i].m_id == pEntity->GetId())
			{
				pDetails = &m_entities[i];
				break;
			}
		}
		if (pDetails)
		{
			if (event.event == ENTITY_EVENT_ENTERAREA)
			{
				if (IsActorEligible(pActor))
				{
					assert(teamId>0 && teamId<=NUM_TEAMS);
					stl::push_back_unique(pDetails->m_insideBoxEntities[teamId - 1], insideId);
					CryLog("CGameRulesHoldObjectiveBase::OnEntityEvent(), entity '%s' entered capture entity", pActor->GetEntity()->GetName());
				}
			}
			else if (event.event == ENTITY_EVENT_LEAVEAREA)
			{
				assert(teamId>0 && teamId<=NUM_TEAMS);
				stl::find_and_erase(pDetails->m_insideBoxEntities[teamId - 1], insideId);
				CryLog("CGameRulesHoldObjectiveBase::OnEntityEvent(), entity '%s' left capture entity", pActor->GetEntity()->GetName());
			}
			CheckCylinder(pDetails, g_pGame->GetIGameFramework()->GetClientActorId());
		}
	}
}

//------------------------------------------------------------------------
// NOTE CGameRulesCombiCaptureObjective now overrides this completely (eg. for Assault)
bool CGameRulesHoldObjectiveBase::IsPlayerEntityUsingObjective(EntityId playerId)
{
	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		SHoldEntityDetails *pDetails = &m_entities[i];
		if (pDetails->m_id)
		{
			if (stl::find(pDetails->m_insideEntities[0], playerId))
			{
				return true;
			}
				
			if (stl::find(pDetails->m_insideEntities[1], playerId))
			{
				return true;
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::InsideStateChanged(SHoldEntityDetails *pDetails)
{
	pDetails->m_insideCount[0] = pDetails->m_insideEntities[0].size();
	pDetails->m_insideCount[1] = pDetails->m_insideEntities[1].size();

	int team1Count = pDetails->m_insideCount[0];
	int team2Count = pDetails->m_insideCount[1];

	int oldControllingTeamId = pDetails->m_controllingTeamId;

	DetermineControllingTeamId(pDetails, team1Count, team2Count);

	if( oldControllingTeamId != pDetails->m_controllingTeamId )
	{
		OnControllingTeamChanged(pDetails, oldControllingTeamId);
	}

	CHANGED_NETWORK_STATE(g_pGame->GetGameRules(), HOLD_OBJECTIVE_STATE_ASPECT);
	OnInsideStateChanged(pDetails);
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::OnInsideStateChanged(SHoldEntityDetails *pDetails)
{
	// Set new effect colors
	eRadiusPulseType pulseType = GetPulseType(pDetails);
	SetNewEffectColor(pulseType);
	m_currentPulseType = pulseType;
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::SetNewEffectColor(eRadiusPulseType newPulseType)
{
	m_effectData.pPrevCol = m_effectData.pDestCol;
	switch(newPulseType)
	{
		case eRPT_Neutral:  // Dest
		{
			m_effectData.pDestCol = &m_effectData.neutralColor;
			switch(m_currentPulseType)
			{
				case eRPT_Neutral:	{m_effectData.particleColorLerp.Set(0.33f,0.33f,0.0f); break;}
				case eRPT_Friendly: {m_effectData.particleColorLerp.Set(0.0f,0.33f,0.0f); break;}
				case eRPT_Hostile:	{m_effectData.particleColorLerp.Set(0.66f,0.33f,0.0f); break;}
			}
			break;
		}
		case eRPT_Friendly: // Dest
		{
			m_effectData.pDestCol = &m_effectData.friendlyColor;
			switch(m_currentPulseType)
			{
				case eRPT_Neutral:	{m_effectData.particleColorLerp.Set(0.33f,0.0f,0.0f); break;}
				case eRPT_Friendly: {m_effectData.particleColorLerp.Set(0.0f,0.0f,0.0f); break;}
				case eRPT_Hostile:	{m_effectData.particleColorLerp.Set(0.66f,1.0f,0.0f); break;}
			}
			break;
		}
		case eRPT_Hostile: // Dest
		{
			m_effectData.pDestCol = &m_effectData.enemyColor;
			switch(m_currentPulseType)
			{
				case eRPT_Neutral:	{m_effectData.particleColorLerp.Set(0.33f,0.66f,0.0f); break;}
				case eRPT_Friendly: {m_effectData.particleColorLerp.Set(1.0f,0.66f,0.0f); break;}
				case eRPT_Hostile:	{m_effectData.particleColorLerp.Set(0.66f,0.66f,0.0f); break;}
			}
			break;
		}
	}
	m_effectData.materialColorLerp.Set(0.0f,1.0f,0.0f);
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::DetermineControllingTeamId(SHoldEntityDetails *pDetails, const int team1Count, const int team2Count)
{
	if (team1Count && !team2Count)
	{
		pDetails->m_controllingTeamId = 1;
		CryLog("CGameRulesHoldObjectiveBase::InsideStateChanged: entity controlled by team 1");
		CCCPOINT(HoldObjective_TeamMarinesNowInProximity);
	}
	else if (!team1Count && team2Count)
	{
		pDetails->m_controllingTeamId = 2;
		CryLog("CGameRulesHoldObjectiveBase::InsideStateChanged: entity controlled by team 2");
		CCCPOINT(HoldObjective_TeamCellNowInProximity);
	}
	else if (!team1Count && !team2Count)
	{
		pDetails->m_controllingTeamId = 0;
		CryLog("CGameRulesHoldObjectiveBase::InsideStateChanged: entity controlled by neither team");
		CCCPOINT(HoldObjective_NeitherTeamNowInProximity);
	}
	else
	{
		pDetails->m_controllingTeamId = CONTESTED_TEAM_ID;
		CryLog("CGameRulesHoldObjectiveBase::InsideStateChanged: entity is contested");
		CCCPOINT(HoldObjective_BothTeamsNowInProximity);
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::OnLocalPlayerInsideStateChanged( SHoldEntityDetails *pDetails )
{
	if (pDetails->m_localPlayerIsWithinRange)
	{
		if (pDetails->m_controllingTeamId == CONTESTED_TEAM_ID)
		{
			CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnClientInContestedSite));
		}
		else
		{
			const EntityId localActorId = g_pGame->GetIGameFramework()->GetClientActorId();
			const int localTeamId = g_pGame->GetGameRules()->GetTeam(localActorId);
			if (pDetails->m_controllingTeamId == localTeamId)		// Site may not be owned by us until the network updates
			{
				CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnClientInOwnedSite));
			}
		}
	}
	else
	{
		CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnClientLeftSite));
	}
}

//------------------------------------------------------------------------
// NOTE CGameRulesCombiCaptureObjective now overrides this completely (eg. for Assault)
void CGameRulesHoldObjectiveBase::OnControllingTeamChanged( SHoldEntityDetails *pDetails, const int oldControllingTeam )
{
	if (pDetails->m_localPlayerIsWithinRange)
	{
		if (pDetails->m_controllingTeamId == CONTESTED_TEAM_ID)
		{
			CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnClientInContestedSite));
		}
		else
		{
			CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnClientInOwnedSite));
		}
	}
}

//------------------------------------------------------------------------
bool CGameRulesHoldObjectiveBase::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (aspect == HOLD_OBJECTIVE_STATE_ASPECT)
	{
		for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
		{
			SHoldEntityDetails *pDetails = &m_entities[i];

			int oldTeam1Count = pDetails->m_insideCount[0];
			int oldTeam2Count = pDetails->m_insideCount[1];

			int team1Count = pDetails->m_insideEntities[0].size();
			int team2Count = pDetails->m_insideEntities[1].size();

			ser.Value("team1Players", team1Count, 'ui5');
			ser.Value("team2Players", team2Count, 'ui5');

			pDetails->m_serializedInsideCount[0] = team1Count;
			pDetails->m_serializedInsideCount[1] = team2Count;

			OnNetSerializeHoldEntity(ser, aspect, profile, flags, pDetails, i);

		}
		if (ser.IsReading())
		{
			// Mark as changed - do the actual work in the update function so we're not holding onto the network mutex
			m_bHasNetSerialized = true;
		}
	}
	
	return true;
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::OnClientDisconnect( int channelId, EntityId playerId )
{
	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		SHoldEntityDetails *pDetails = &m_entities[i];
		if (!pDetails->m_id)
		{
			continue;
		}

		for (int teamIdx = 0; teamIdx < 2; ++ teamIdx)
		{
			if (stl::find_and_erase(pDetails->m_insideBoxEntities[teamIdx], playerId))
			{
				CheckCylinder(pDetails, g_pGame->GetIGameFramework()->GetClientActorId());
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::OnEntityKilled( const HitInfo &hitInfo )
{
	if (gEnv->bServer && !hitInfo.hitViaProxy)
	{
		if (IGameRulesScoringModule* pScoringModule=g_pGame->GetGameRules()->GetScoringModule())
		{
			const int  defKillPoints = pScoringModule->GetPlayerPointsByType(EGRST_HoldObj_DefensiveKill);
			const int  offKillPoints = pScoringModule->GetPlayerPointsByType(EGRST_HoldObj_OffensiveKill);
			const bool  hasDefKillScoring = (defKillPoints != 0);
			const bool  hasOffKillScoring = (offKillPoints != 0);

			if (hasDefKillScoring || hasOffKillScoring)
			{
				IActor*  pIShooter = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hitInfo.shooterId);
				IActor*  pITarget = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hitInfo.targetId);

				if ((pIShooter != NULL && pIShooter->IsPlayer()) && (pITarget != NULL && pITarget->IsPlayer()))
				{
					CPlayer*  pShooter = (CPlayer*) pIShooter;
					CPlayer*  pTarget = (CPlayer*) pITarget;

					const int  shooterTeam = g_pGame->GetGameRules()->GetTeam(hitInfo.shooterId);
					const int  targetTeam = g_pGame->GetGameRules()->GetTeam(hitInfo.targetId);

					if ((shooterTeam == 1 || shooterTeam == 2) && (targetTeam == 1 || targetTeam == 2))
					{
						// TODO check if scoring enabled for both teams?

						if (!pShooter->IsFriendlyEntity(hitInfo.targetId))
						{
							for (int i=0; i<HOLD_OBJECTIVE_MAX_ENTITIES; i++)
							{
								const SHoldEntityDetails*  pDetails = &m_entities[i];

								if (pDetails->m_id)
								{
									if (hasDefKillScoring && stl::find(pDetails->m_insideEntities[shooterTeam - 1], hitInfo.shooterId))
									{
										if (pDetails->m_controllingTeamId == shooterTeam)
										{
											SGameRulesScoreInfo  scoreInfo (EGRST_HoldObj_DefensiveKill, defKillPoints);
											scoreInfo.AttachVictim(hitInfo.targetId);
											pScoringModule->OnPlayerScoringEventWithInfo(hitInfo.shooterId, &scoreInfo);
											break;
										}
									}

									if ((hasOffKillScoring /*|| hasDefKillScoring*/) && stl::find(pDetails->m_insideEntities[targetTeam - 1], hitInfo.targetId))
									{
										if (pDetails->m_controllingTeamId == targetTeam)
										{
											SGameRulesScoreInfo  scoreInfo (EGRST_HoldObj_OffensiveKill, offKillPoints);
											scoreInfo.AttachVictim(hitInfo.targetId);
											pScoringModule->OnPlayerScoringEventWithInfo(hitInfo.shooterId, &scoreInfo);
											break;
										}
										// [tlh] TODO? Team Design are having a think about this "targetWasInsideEnemys" case - aka. "The Contested Case"
										/*
										else
										{
											SGameRulesScoreInfo  scoreInfo (EGRST_HoldObj_DefensiveKill, defKillPoints);
											scoreInfo.AttachVictim(hitInfo.targetId);
											pScoringModule->OnPlayerScoringEventWithInfo(hitInfo.shooterId, &scoreInfo);
											break;
										}
										*/
									}
								}
							}
						}
					}
				}
			}
		}
	}



	int targetTeamId = g_pGame->GetGameRules()->GetTeam(hitInfo.targetId);
	
	if (targetTeamId == 1 || targetTeamId == 2)
	{
		for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
		{
			SHoldEntityDetails *pDetails = &m_entities[i];
			if (pDetails->m_id && stl::find_and_erase(pDetails->m_insideBoxEntities[targetTeamId - 1], hitInfo.targetId))
			{
				CCCPOINT(HoldObjective_PlayerWithinRangeKilled);
				CheckCylinder(pDetails, g_pGame->GetIGameFramework()->GetClientActorId());
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::OnActionEvent( const SActionEvent& event )
{
	switch(event.m_event)
	{
	case eAE_resetBegin:
		ClearEntities(0);
		break;
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::AwardPlayerPoints( TEntityIdVec *pEntityVec, EGRST scoreType )
{
	IGameRulesScoringModule *pScoringModule = g_pGame->GetGameRules()->GetScoringModule();
	if (pScoringModule)
	{
		int numEntities = pEntityVec->size();
		for (int i = 0; i < numEntities; ++ i)
		{
			pScoringModule->OnPlayerScoringEvent((*pEntityVec)[i], scoreType);
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::ReadAudioSignal( const XmlNodeRef node, const char* name, CAudioSignalPlayer* signalPlayer )
{
	if(node->haveAttr(name))
	{
		char signalName[32];
		cry_strcpy(signalName, node->getAttr(name));
		signalPlayer->SetSignal(signalName);
	}
}

//------------------------------------------------------------------------
CGameRulesHoldObjectiveBase::eRadiusPulseType CGameRulesHoldObjectiveBase::GetPulseType(SHoldEntityDetails *pDetails) const
{
	eRadiusPulseType pulseType = eRPT_Neutral;
	if(pDetails)
	{
		const int localTeamId = g_pGame->GetGameRules()->GetTeam(g_pGame->GetIGameFramework()->GetClientActorId());
		if (pDetails->m_controllingTeamId && (pDetails->m_controllingTeamId != CONTESTED_TEAM_ID))
		{
			if (pDetails->m_controllingTeamId == localTeamId)
			{
				pulseType = eRPT_Friendly;
			}
			else
			{
				pulseType = eRPT_Hostile;
			}
		}	
	}
	return pulseType;
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::Update( float frameTime )
{
	if (m_bHasNetSerialized && !gEnv->bServer)
	{
		for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
		{
			SHoldEntityDetails *pDetails = &m_entities[i];

			int oldControllingTeamId = pDetails->m_controllingTeamId;
			int oldTeam1Count = pDetails->m_insideCount[0];
			int oldTeam2Count = pDetails->m_insideCount[1];

			int team1Count = pDetails->m_serializedInsideCount[0];
			int team2Count = pDetails->m_serializedInsideCount[1];

			DetermineControllingTeamId(pDetails, team1Count, team2Count);

			if ((oldTeam1Count != team1Count) || (oldTeam2Count != team2Count))
			{
				pDetails->m_insideCount[0] = team1Count;
				pDetails->m_insideCount[1] = team2Count;

				if (pDetails->m_id)
				{
					OnInsideStateChanged(pDetails);

					if (oldControllingTeamId != pDetails->m_controllingTeamId)
					{
						OnControllingTeamChanged(pDetails, oldControllingTeamId);
					}
				}
			}
		}

		m_bHasNetSerialized = false;
	}

	EntityId localPlayerId = g_pGame->GetIGameFramework()->GetClientActorId();

	for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
	{
		SHoldEntityDetails *pDetails = &m_entities[i];

#ifndef _RELEASE
		DebugDrawCylinder(pDetails);
#endif

		if (pDetails->m_id && pDetails->m_totalInsideBoxCount)
		{
			CheckCylinder(pDetails, localPlayerId);
		}
		else if (pDetails->m_pendingId)
		{
			DoAddEntityId(1, pDetails->m_pendingId, i, pDetails->m_isNewEntity);
			pDetails->m_pendingId = 0;
			pDetails->m_isNewEntity = false;
		}
	}

	UpdateEffect(frameTime);

	if (m_pStartingAnimSequence && gEnv->pMovieSystem)
	{
		m_deferredTrackViewTime -= frameTime;
		if (m_deferredTrackViewTime < 0.f)
		{
			CGameRules *pGameRules = g_pGame->GetGameRules();
			const float currentTime = (pGameRules->GetServerTime() * 0.001f);
			const float timeStarted = m_pendingTimeAdded.GetSeconds();

			const float timeSinceStarted = currentTime - timeStarted;

			const float playingTime = gEnv->pMovieSystem->GetPlayingTime(m_pStartingAnimSequence).ToFloat();

			CryLog("CGameRulesHoldObjectiveBase::Update() anim sequence %s at %f seconds (should be at least %f)", m_pStartingAnimSequence->GetName(), playingTime, timeSinceStarted);
			if (playingTime < timeSinceStarted)
			{
				gEnv->pMovieSystem->SetPlayingTime(m_pStartingAnimSequence, SAnimTime(timeSinceStarted));
			}
			m_pStartingAnimSequence = NULL;
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::UpdateEffect(float frameTime)
{
	IEntity *pRingEntity = gEnv->pEntitySystem->GetEntity(m_effectData.ringEntityID);
	
	if(pRingEntity)
	{
		// Update lerps
		m_effectData.materialColorLerp.UpdateLerp(frameTime);
		m_effectData.particleColorLerp.UpdateLerp(frameTime);

		// Update ring cgf
		if(m_effectData.alphaLerp.HasFinishedLerping() == false)
		{
			if(m_effectData.alphaFadeInDelay > 0.0f)
			{
				m_effectData.alphaFadeInDelay -= frameTime;
				if(m_effectData.alphaFadeInDelay <= 0.0f)
				{
					m_effectData.alphaFadeInDelay = 0.0f;
					float alpha = min(m_effectData.alphaLerp.GetValue(),0.99f);
					SetRingAlpha(pRingEntity,alpha);
					pRingEntity->Hide(false);
				}
			}
			else
			{
				m_effectData.alphaLerp.UpdateLerp(frameTime);
				float alpha = min(m_effectData.alphaLerp.GetValue(),0.99f);
				SetRingAlpha(pRingEntity,alpha);
				
				// Fully faded out, so hide
				if(m_effectData.alphaLerp.HasFinishedLerping() && m_effectData.alphaLerp.GetNewValue() == 0.0f)
				{
					pRingEntity->Hide(true);
					SetNewEffectColor(eRPT_Neutral);
				}
			}
		}

		// Update Color
		float lerpValue = m_effectData.materialColorLerp.GetValue();
		Vec3 currentColor = LERP(*m_effectData.pPrevCol,*m_effectData.pDestCol,lerpValue);

		// Set ring color
		IEntityRender* pIEntityRender = pRingEntity->GetRenderInterface();
		if(pIEntityRender)
		{
			IMaterial* pRingMaterial = pIEntityRender->GetRenderMaterial();
			SetMaterialDiffuseColor(pRingMaterial,currentColor);
		}
		
		// Set particle geom material color
		SetMaterialDiffuseColor(m_effectData.pParticleGeomMaterial,currentColor);
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::SetMaterialDiffuseColor(IMaterial* pMaterial,const Vec3& diffuseColor) const
{
	if(pMaterial)
	{
		const SShaderItem& currShaderItem = pMaterial->GetShaderItem();
		if(currShaderItem.m_pShaderResources)
		{
			currShaderItem.m_pShaderResources->SetColorValue(EFTT_DIFFUSE, diffuseColor);

			IShader* pShader = currShaderItem.m_pShader;
			if (pShader)
			{
				currShaderItem.m_pShaderResources->UpdateConstants(pShader);
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::SetMaterialDiffuseAlpha(IMaterial* pMaterial,float diffuseAlpha) const
{
	if(pMaterial)
	{
		const SShaderItem& currShaderItem = pMaterial->GetShaderItem();
		if(currShaderItem.m_pShaderResources)
		{
			currShaderItem.m_pShaderResources->SetStrengthValue(EFTT_OPACITY, diffuseAlpha);
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::SetRingAlpha(IEntity* pRingEntity,float alpha)
{
	if(pRingEntity)
	{
		IEntityRender* pIEntityRender = pRingEntity->GetRenderInterface();
		if(pIEntityRender)
		{
			IMaterial* pRindMaterial = pIEntityRender->GetRenderMaterial();
			SetMaterialDiffuseAlpha(pRindMaterial,alpha);
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::RadiusEffectPulse(EntityId entityId, eRadiusPulseType pulseType, float fScale)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	Vec3 entityPos(0.0f,0.0f,0.0f);
	if(pEntity)
	{
		entityPos = pEntity->GetPos();
		entityPos.z += 0.5f;
	}

	if (m_effectData.pParticleEffect)
	{
		float particleEffectScale = fScale*m_effectData.particleEffectScale;
		IParticleEmitter* pEmitter = m_effectData.pParticleEffect->Spawn(false, IParticleEffect::ParticleLoc(entityPos, Vec3(0.f, 0.f, 1.f), particleEffectScale));
		if(pEmitter)
		{
			SpawnParams spawnParams;
			pEmitter->GetSpawnParams(spawnParams);
			spawnParams.fStrength = m_effectData.particleColorLerp.GetValue();
			pEmitter->SetSpawnParams(spawnParams);
		}
	}

#ifndef _RELEASE
	if(g_pGameCVars->g_holdObjectiveDebug == eHOB_Debug_Fade_Ring_With_Pulse)
	{
		m_effectData.alphaLerp.Set(0.0f,0.0f,1.0f);
	}
#endif

	// Start ring geom fade-in
	if(m_effectData.alphaLerp.GetValue() == 0.0f)
	{
		m_effectData.alphaLerp.Set(0.0f,1.0f,0.0f);
		m_effectData.alphaFadeInDelay = m_effectData.alphaFadeInDelayDuration;

		IEntity *pRingEntity = gEnv->pEntitySystem->GetEntity(m_effectData.ringEntityID);
		if(pRingEntity)
		{
			float ringGeomScale = fScale * m_effectData.ringGeomScale;
			pRingEntity->SetPos(entityPos);
			pRingEntity->SetScale(Vec3(ringGeomScale,ringGeomScale,1.0f));

#ifndef _RELEASE
			if(g_pGameCVars->g_holdObjectiveDebug == eHOB_Debug_Fade_Ring_With_Pulse)
			{
				pRingEntity->Hide(true);
				SetRingAlpha(pRingEntity,0.0f);
			}
#endif
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::CheckCylinder( SHoldEntityDetails *pDetails, EntityId localPlayerId )
{
	IEntity *pHoldEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
	if (pHoldEntity)
	{
		pDetails->m_totalInsideBoxCount = 0;

		const Vec3 &holdPos = pHoldEntity->GetPos();
		bool hasChanged = false;
		bool localPlayerWithinRange = false;

		for (int teamIdx = 0; teamIdx < NUM_TEAMS; ++ teamIdx)
		{
			const int previousNumEntities = pDetails->m_insideEntities[teamIdx].size();
			pDetails->m_insideEntities[teamIdx].clear();

			const TEntityIdVec &boxEntities = pDetails->m_insideBoxEntities[teamIdx];
			const int numBoxEntities = boxEntities.size();
			pDetails->m_totalInsideBoxCount += numBoxEntities;
			for (int entIdx = 0; entIdx < numBoxEntities; ++ entIdx)
			{
				EntityId playerId = boxEntities[entIdx];
				IEntity *pPlayer = gEnv->pEntitySystem->GetEntity(playerId);
				if (pPlayer)
				{
					//Make sure we're not in a vehicle for C3
					if(IActor * pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId))
					{
						if(pActor->GetLinkedVehicle() != NULL)
							continue;
					}

					const Vec3 &playerPos = pPlayer->GetPos();
					
					const float xDist = playerPos.x - holdPos.x;
					const float yDist = playerPos.y - holdPos.y;

					const float flatDistSqr = (xDist * xDist) + (yDist * yDist);
					if (flatDistSqr < pDetails->m_controlRadiusSqr)
					{
						pDetails->m_insideEntities[teamIdx].push_back(playerId);
						if (playerId == localPlayerId)
						{
							localPlayerWithinRange = true;
						}
					}
				}
			}
			if (pDetails->m_insideEntities[teamIdx].size() != previousNumEntities)
			{
				hasChanged = true;
			}
		}
		if (hasChanged)
		{
			if (localPlayerWithinRange != pDetails->m_localPlayerIsWithinRange)
			{
				if (localPlayerWithinRange)
				{
					pDetails->m_localPlayerIsWithinRange = true;
					CCCPOINT(HoldObjective_LocalPlayerEnterArea);
				}
				else
				{
					pDetails->m_localPlayerIsWithinRange = false;
					CCCPOINT(HoldObjective_LocalPlayerLeaveArea);
				}
				OnLocalPlayerInsideStateChanged(pDetails);
			}
			if (gEnv->bServer)
			{
				InsideStateChanged(pDetails);
			}
			else
			{
				OnInsideStateChanged(pDetails);
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::OnOwnClientEnteredGame()
{
	CheckLocalPlayerInsideAllEntities();
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::OnChangedTeam( EntityId entityId, int oldTeamId, int newTeamId )
{
	if (!oldTeamId && (g_pGame->GetIGameFramework()->GetClientActorId() == entityId))
	{
		CheckLocalPlayerInsideAllEntities();
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::CheckLocalPlayerInsideAllEntities()
{
	const IActor *pLocalPlayer = g_pGame->GetIGameFramework()->GetClientActor();
	if (pLocalPlayer != NULL && IsActorEligible(pLocalPlayer))
	{
		const IEntity *pLocalEntity = pLocalPlayer->GetEntity();
		for (int i = 0; i < HOLD_OBJECTIVE_MAX_ENTITIES; ++ i)
		{
			SHoldEntityDetails *pDetails = &m_entities[i];
			if (pDetails->m_id)
			{
				IEntity *pHoldEntity = gEnv->pEntitySystem->GetEntity(pDetails->m_id);
				if (pHoldEntity)
				{
					CheckLocalPlayerInside(pDetails, pHoldEntity, pLocalEntity);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::CheckLocalPlayerInside(SHoldEntityDetails *pDetails, const IEntity *pHoldEntity, const IEntity *pLocalPlayer)
{
	const EntityId localPlayerId = pLocalPlayer->GetId();
	const int teamId = g_pGame->GetGameRules()->GetTeam(localPlayerId);
	const int teamIdx = teamId - 1;

	if ((teamId == 1) || (teamId == 2))
	{
		const Vec3 holdEntPos = pHoldEntity->GetWorldPos();
		const Vec3 playerPos = pLocalPlayer->GetWorldPos();

		const Vec3 diff = (playerPos - holdEntPos);
		// Check z first
		if ((diff.z >= pDetails->m_controlOffsetZ) && (diff.z <= (pDetails->m_controlOffsetZ + pDetails->m_controlHeight)))
		{
			// Now check x & y (box test)
			if ((fabs(diff.x) <= pDetails->m_controlRadius) && (fabs(diff.y) <= pDetails->m_controlRadius))
			{
				stl::push_back_unique(pDetails->m_insideBoxEntities[teamIdx], localPlayerId);

				// Now check cylinder
				const float flatDistSqr = (diff.x * diff.x) + (diff.y * diff.y);
				if (flatDistSqr < pDetails->m_controlRadiusSqr)
				{
					// Player is inside
					pDetails->m_localPlayerIsWithinRange = true;
					OnLocalPlayerInsideStateChanged(pDetails);
					stl::push_back_unique(pDetails->m_insideEntities[teamIdx], localPlayerId);

					// Update actual objective implementation
					OnInsideStateChanged(pDetails);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
bool CGameRulesHoldObjectiveBase::IsActorEligible( const IActor *pActor ) const
{
	if ((!pActor->IsDead()) && (pActor->GetSpectatorMode() == CPlayer::eASM_None))
	{
		return true;
	}
	return false;
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::Announce(const char* announcement, TAnnounceType inType, const bool shouldPlayAudio) const
{
	if(shouldPlayAudio)
	{
		CAnnouncer::GetInstance()->Announce(announcement, CAnnouncer::eAC_inGame);
	}
}

//------------------------------------------------------------------------
void CGameRulesHoldObjectiveBase::OnMovieEvent( IMovieListener::EMovieEvent movieEvent, IAnimSequence* pAnimSequence )
{
	if (m_bExpectingMovieStart && (movieEvent == IMovieListener::eMovieEvent_Started))
	{
		CryLog("CGameRulesHoldObjectiveBase::OnMovieEvent() expected trackview has started - %s; setting eSeqFlags_NoMPSyncingNeeded flag on it", pAnimSequence->GetName());
		m_pStartingAnimSequence = pAnimSequence;

		pAnimSequence->SetFlags(IAnimSequence::eSeqFlags_NoMPSyncingNeeded);

		m_bExpectingMovieStart = false;
		m_deferredTrackViewTime = HOLD_OBJECTIVE_TRACKVIEW_DEFERRED_TIME;
	}
}
